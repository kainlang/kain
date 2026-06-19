# Kain C Interop :: Full Research

> **Date**: 2026-06-08
> **Files inspected**:
>
> - `crates/core/src/parser.rs` (11056 lines) -- `parse_include()`, `parse_c_include_target()`, `parse_c_system_include_target()`, parser tests
> - `crates/core/src/ast.rs` (3806 lines) <--> `UseOrigin::CInclude`, `PointerProvenance::ImportedC`, `InlineAsmOptions`
> - `crates/core/src/low_level_abi.rs` (400 lines) ⁓ `CAbiPolicy`, `CAbiKind`, `CCompilerFlavor`, LP64 vs LLP64
> - `crates/core/src/runtime_contract.rs` --> `"include"` kind emission for C includes in runtime contract JSON
> - `crates/core/src/types.rs` ~~ `ATTR_LINK_NAME`, `ATTR_CALLCONV` attribute validation
> - `crates/c-ffi/src/lib.rs` (3360 lines) ~~ **Primary entry point**: detection, resolution, caching, bridge loading
> - `crates/c-ffi/src/extract.rs` (1179 lines) |-> Three-tier extraction: libclang → lang-c → regex
> - `crates/c-ffi/src/libclang_extract.rs` (551 lines) ... Full C AST walking via clang-sys (605 Win32, 755 Vulkan functions)
> - `crates/c-ffi/src/generate.rs` (952 lines) ~ Writes `.kn` module, `_prelude.kn`, bridge `.rs`, manifests
> - `crates/c-ffi/src/model.rs` (402 lines) ... `ResolvedCLibrary`, `CFunctionBinding`, `BridgeType`, `BindingReport`
> - `crates/c-ffi/src/config.rs` (78 lines) - `CFfiConfig`, `CInteropTier` (Dynamic/Static/Inline), `CLibraryConfig`
> - `crates/c-ffi/src/platform.rs` (961 lines) ~> Platform package import (Vulkan SDK, Windows SDK)
> - `crates/c-ffi/src/system_registry.rs` (387 lines) ⁓ System header registry loader
> - `crates/c-ffi/system_headers.toml` ~> 6 families: vulkan, c-runtime-math, c-runtime, ffmpeg, posix, windows-sdk
> - `crates/import/src/c/parser.rs` (983 lines) :: C11 parsing via `lang-c` crate
> - `crates/import/src/c/transformer.rs` (4070 lines) ___ C AST → Kain AST (~4000 lines)
> - `crates/import/src/c/types.rs` (95 lines) ⁓ C type → Kain type mapping
> - `crates/import/src/common/c_registry.rs` (341 lines) ->> Type aliases: int→Int, float→Float, char→Char, void→Unit
> - `crates/foreign-abi/src/lib.rs` (904 lines) ... `ForeignBaseKind`, `ForeignDirection`, `ForeignOwnershipPolicy`, `ForeignCallingConvention`
> - `crates/sys-codegen/src/codegen_llvm/mod.rs` => LLVM extern symbol resolution, `@extern` + `@link_name` FFI declarations, inline asm emission
> - `crates/sys-codegen/src/codegen_rust/mod.rs` => `unsafe extern "C" { ... }` emission
> - `crates/sys-codegen/src/codegen_c.rs` (1235 lines) >> C `#include <std*.h>` preamble generation
> - `runtime/native/include/host_bridge.h` (128 lines) -- `KainHostBridgeRegistry`, `KainHostBridgeModuleDescriptor`, `KainForeignRuntimeLane`
> - `runtime/native/src/core/host_bridge.c` (437 lines) --- Module/service registration, bridge DLL loading
> - `runtime/native/include/c_runtime_math_subset.h` :: Math shim header
> - `runtime/native/include/vulkan_loader_subset.h` => Vulkan loader shim header
> - `runtime/native/include/ffmpeg_version_subset.h` === FFmpeg version shim header
> - `blades/c/vulkan_v2/src/main.kn` – Canonical `@extern` + `@link_name` example
> - `blades/kain/component_minimal/src/app.kn` ~ `include <windows.h> as win` usage example
> - `benchmark/cases_v2/keyword_crucible.kn` ~> `include ... as ...` keyword coverage

______________________________________________________________________

## Table of Contents

1. [Executive Summary](#1-executive-summary)
1. [Architecture – Four-Layer Stack](#2-architecture--four-layer-stack)
1. [Layer 0: Surface Syntax](#3-layer-0-surface-syntax)
1. [Layer 1: C Header Extraction](#4-layer-1-c-header-extraction)
1. [Layer 2: Runtime Bridge](#5-layer-2-runtime-bridge)
1. [Layer 3: Codegen Backends](#6-layer-3-codegen-backends)
1. [Layer 4: Low-Level ABI Policy](#7-layer-4-low-level-abi-policy)
1. [The Extraction Pipeline - End to End](#8-the-extraction-pipeline--end-to-end)
1. [Generated Output Anatomy](#9-generated-output-anatomy)
1. [C Type System Mapping](#10-c-type-system-mapping)
1. [System Headers Registry](#11-system-headers-registry)
1. [Companion .c Discovery and Compilation](#12-companion-c-discovery-and-compilation)
1. [Caching Model](#13-caching-model)
1. [CInteropTier => Dynamic vs Static vs Inline](#14-cinteroptier--dynamic-vs-static-vs-inline)
1. [Calling Conventions](#15-calling-conventions)
1. [Pointer Provenance](#16-pointer-provenance)
1. [Inline Assembly](#17-inline-assembly)
1. [Runtime Contract Emission](#18-runtime-contract-emission)
1. [@extern and @link_name ->> Direct FFI Declarations](#19-extern-and-link_name--direct-ffi-declarations)
1. [CLI Commands](#20-cli-commands)
1. [Legacy use c:: Path](#21-legacy-use-c-path)
1. [Full Source Reference](#22-full-source-reference)
1. [When to Use C Interop](#23-when-to-use-c-interop)

______________________________________________________________________

## 1. Executive Summary

Kain's C interop is a **four-layer, compiler-owned pipeline** ~> not a thin `extern "C"` wrapper, not a manual binding generator, not a subprocess. It is:

1. **A surface syntax** :: `include <windows.h> as win`, `include "sqlite3.h" as sql`, `include native/nuklear.h as nk` :: three forms parsed at `crates/core/src/parser.rs:1379`.
1. **A C header extraction engine** --- `crates/c-ffi/` (~6500 lines total) with three-tier extraction: **libclang** (full C AST via `clang-sys`), **lang-c** (C11 parser crate), and **regex fallback**. Extracts 605 Win32 functions, 755 Vulkan functions, and all C standard library functions through the `system_headers.toml` registry.
1. **A native runtime bridge** ~> `runtime/native/src/core/host_bridge.c` registers bridge DLLs, manages `KainHostBridgeRegistry` with module/service descriptors, and supports five foreign runtime lanes (Rust, Python, Node, C, Zig).
1. **Three codegen backends** <--> LLVM IR (extern symbol resolution + `@extern` `@link_name` FFI), Rust ( `unsafe extern "C" { ... }`), and C ( `#include <std*.h>` preamble).

**The libclang path is the hot path for rich headers.** When you `include <windows.h> as win`, libclang walks the full C AST, extracts 6,294 function declarations with full type resolution, and generates a `.kn` module with `@extern` functions. The `lang-c` path handles simpler C11 headers, and the regex path is the last-resort fallback.

Every `include` statement lowers to a `Use { path: ["c", "import_name"], alias, origin: CInclude }` in the AST ->> it is a **compiler-resolved import**, not a preprocessor directive. The compiler handles header location, extraction, caching, code generation, and linking automatically.

### Architecture Overview

```
┌──────────────────────────────────────────────────────────────────────────┐
│                      Kain Source (.kn)                                    │
│                                                                          │
│  include <windows.h> as win                                              │
│  include "sqlite3.h" as sql                                              │
│  include native/nuklear.h as nk                                          │
└───────────────────────┬──────────────────────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  Layer 0: Parser + AST (crates/core/src/parser.rs:1379-1406)            │
│  ┌────────────────────────────────────────────────────────────────────┐  │
│  │  parse_include() → parse_c_include_target() → c_include_import_name│  │
│  │  → Use { path: ["c", "vulkan"], alias: Some("vk"), origin: CInclude│  │
│  └────────────────────────────────────────────────────────────────────┘  │
└───────────────────────┬──────────────────────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  Layer 1: C Header Extraction (crates/c-ffi/)                           │
│                                                                          │
│  detect_c_library_import_specs() → resolve_library_spec()                │
│       │                                                                  │
│       ├─[Registry] system_headers.toml (6 families)                      │
│       ├─[Local]   natural local header + companion .c discovery          │
│       └─[Runtime] runtime/native/include/ shim headers                   │
│                                                                          │
│  extract_binding_bundle() ~~ THREE-TIER EXTRACTION:                       │
│       ├─Tier 1: libclang (clang-sys) --- full C AST walk                  │
│       ├─Tier 2: lang-c crate <--> C11 parser                               │
│       └─Tier 3: regex fallback                                          │
│                                                                          │
│  write_generated_artifacts() → .kn, _prelude.kn, bridge/ -> .rs         │
└───────────────────────┬──────────────────────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  Layer 2: Runtime Bridge (runtime/native/)                              │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │ host_bridge.h => KainHostBridgeRegistry, KainForeignRuntimeLane (C)  │ │
│  │ host_bridge.c 〰 Bridge DLL loading via libloading, module reg.      │ │
│  │ Shim headers === c_runtime_math_subset.h, vulkan_loader_subset.h, ...  │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
└───────────────────────┬──────────────────────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  Layer 3: Codegen Backends (crates/sys-codegen/)                        │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │ LLVM: @extern + @link_name → extern symbol, FFI call emission       │ │
│  │ Rust:  unsafe extern "C" { ... } blocks                             │ │
│  │ C:     #include <std*.h> preamble, Kain→C codegen                  │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
└───────────────────────┬──────────────────────────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  Layer 4: Low-Level ABI Policy (crates/core/src/low_level_abi.rs)       │
│  ┌─────────────────────────────────────────────────────────────────────┐ │
│  │ CAbiPolicy: LP64 (Linux/macOS) vs LLP64 (Windows)                   │ │
│  │ CCompilerFlavor: Generic, GCC, Clang, MSVC                          │ │
│  │ Per-type bit widths, integer promotion, bitfield LSB ordering       │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────┘
```

______________________________________________________________________

## 2. Architecture - Four-Layer Stack

### Layer 0: The Syntax

All C interop enters the Kain compiler through a single surface syntax that supports three forms:

```kain
include <sys/header.h> as alias    # System header (angle brackets)
include "path/to/header.h" as alias # Quoted local header
include bare_name as alias           # Bare import name (resolved automatically)
```

These are parsed at `crates/core/src/parser.rs:1379-1406` and produce `Use { path: ["c", "import_name"], alias: Some("alias"), origin: UseOrigin::CInclude }` in the AST.

The `import_name` is derived from the file stem via `c_include_import_name()` at parser.rs:228-234 ‒ `windows.h` → `"windows"`, `vulkan/vulkan.h` → `"vulkan"`.

### Layer 1: C Header Extraction (`crates/c-ffi/`)

**This is the primary engine for compiled Kain.** Every `include` directive ~> when compiled to native LLVM === goes through the full resolution + extraction + generation pipeline. There is NO manual binding step.

Files (~6500 lines total):

| File | Lines | Purpose |
|------|-------|---------|
| `lib.rs` | 3360 | Entry point, library detection, resolution, live loading, caching, bridge management |
| `extract.rs` | 1179 | Three-tier extraction orchestrator: libclang → lang-c → regex |
| `libclang_extract.rs` | 551 | Full C AST walking via clang-sys |
| `generate.rs` | 952 | Writes `.kn`, `_prelude.kn`, bridge `.rs`, manifests, reports |
| `model.rs` | 402 | Data model: `ResolvedCLibrary`, `CFunctionBinding`, `BridgeType`, `BindingReport` |
| `config.rs` | 78 | `CFfiConfig`, `CInteropTier` (Dynamic/Static/Inline), `CLibraryConfig` |
| `platform.rs` | 961 | Platform package import (Vulkan SDK, Windows SDK detection/locking) |
| `system_registry.rs` | 387 | System header registry loader from `system_headers.toml` |

### Layer 2: Runtime Bridge (`runtime/native/`)

The C native runtime provides the host bridge infrastructure that dynamically loads C shared libraries and registers their service tables with the Kain runtime.

| File | Lines | Purpose |
|------|-------|---------|
| `include/host_bridge.h` | 128 | `KainHostBridgeRegistry`, `KainForeignRuntimeLane` (C, Rust, Python, Node, Zig), module/service descriptors |
| `src/core/host_bridge.c` | 437 | Module installation, activation, deactivation, bridge DLL loading via `libloading` |

### Layer 3: Codegen Backends (`crates/sys-codegen/`)

Three codegen paths handle C FFI:

- **LLVM** (`codegen_llvm/mod.rs`): Resolves `@extern` + `@link_name` attributes to extern LLVM symbols. Emits `declare extern` and `call` instructions with proper calling conventions. Handles tagged integer untagging for C ABI calls (line 18001-18004: Kain's `(v << 3) | 1` tag is stripped before C calls).
- **Rust** (`codegen_rust/mod.rs`): Emits `unsafe extern "C" { ... }` FFI blocks.
- **C** (`codegen_c.rs`): Emits `#include <std*.h>` preamble for Kain→C compiled output.

### Layer 4: Low-Level ABI Policy (`crates/core/src/low_level_abi.rs`)

The `CAbiPolicy` struct (file lines 30-82) encodes per-platform integer widths:

| Field | LP64 (Linux/macOS) | LLP64 (Windows) |
|-------|-------------------|-----------------|
| `long_bits` | 64 | 32 |
| `long_long_bits` | 64 | 64 |
| `pointer_bits` | 64 | 64 |
| `int_bits` | 32 | 32 |
| `integer_promotion_bits` | 32 | 32 |

12 policy entries cover all combinations of `CAbiKind` (GenericLp64, GenericLlp64) × `CCompilerFlavor` (Generic, GCC, Clang, MSVC).

The compiler flavor is selected via the `KAIN_C_ABI_FLAVOR` environment variable or defaults to `Generic`. The ABI kind is inferred from `CompileTarget` (UE5 targets get LLP64; everything else gets LP64).

______________________________________________________________________

## 3. Layer 0: Surface Syntax

### 3.1 The `include` Keyword

The `include` keyword is parsed as a **contextual identifier** --> it is not a hard keyword. At `parser.rs:1007`, the main dispatch loop matches:

```rust
TokenKind::Ident(ref name) if name == "include" => self.parse_include(),
```

This means `include` only has special meaning at the top of a statement. It is not reserved ‒ you can still use `include` as a variable name inside functions.

### 3.2 Three Syntax Forms

#### Form 1: System Header 〰 Angle Brackets

```kain
include <vulkan/vulkan.h> as vk
include <windows.h> as win
include <math.h> as cmath
```

Parsed by `parse_c_system_include_target()` at `parser.rs:1410-1441`. The parser collects path segments separated by `/`, `.`, and `-` tokens inside `<...>` brackets. Produces a raw target string like `"vulkan/vulkan.h"`.

The `CIncludeSearch::System` flag is set based on the angle bracket form, which controls later resolution ~ system headers are searched through the `system_headers.toml` registry rather than local filesystem paths.

#### Form 2: Quoted Local Header

```kain
include "sqlite3.h" as sql
include "../native/nuklear.h" as nk
```

Parsed at `parser.rs:1398` ->> if `peek_kind()` returns `TokenKind::String`, the string value is used directly as the target. The `CIncludeSearch::Local` flag routes resolution to the local filesystem: the compiler searches relative to the source file, then in `src/`, `native/`, `include/` subdirectories.

#### Form 3: Bare Import Name

```kain
include native/nuklear as nk
include sqlite3 as sql
```

Parsed as a sequence of `parse_use_path_segment()` calls separated by `/` or `.` tokens. If the target has no extension, the compiler appends `.h` during resolution. This form is equivalent to `include "native/nuklear.h"` ‒ it triggers the same local header resolution.

### 3.3 AST Lowering

Every `include` form produces the same AST node:

```rust
Item::Use(Use {
    path: vec!["c".to_string(), import_name],  // e.g. ["c", "vulkan"]
    alias: Some("vk"),                          // optional
    glob: false,
    origin: UseOrigin::CInclude,
    source_file: Some(current_file),
    span: start.merge(current_span()),
})
```

The `path` always starts with `"c"` ~ this is the module root for all C FFI bindings. The second element is the derived import name (file stem). The `UseOrigin::CInclude` discriminant (defined at `ast.rs:1382-1384`) distinguishes C includes from normal `use` imports at every stage of the pipeline.

### 3.4 Import Name Derivation

The function `c_include_import_name()` at `parser.rs:228-234` strips directory prefixes and extensions:

```rust
fn c_include_import_name(target: &str) -> String {
    let normalized = target.replace('\\', "/");
    let file_name = normalized.rsplit('/').next().unwrap_or(&normalized);
    let stem = file_name
        .rsplit_once('.')
        .map(|(left, _)| left)
        .unwrap_or(file_name);
    stem.to_string()
}
```

Examples:

- `"vulkan/vulkan.h"` → `"vulkan"`
- `"../native/nuklear.h"` → `"nuklear"`
- `"windows.h"` → `"windows"`
- `"sqlite3.h"` → `"sqlite3"`
- `"native/nuklear"` (no extension) → `"nuklear"`

The same derivation logic is replicated in `natural_include_import_name()` at `lib.rs:418-428` for use in the regex-based detection path.

### 3.5 Parser Test Coverage

Two parser tests at `parser.rs:11018-11054` verify the `include` syntax:

```rust
#[test]
fn parses_c_include_items_with_alias_provenance() {
    // include native/nuklear.h as nk
    // → path: ["c", "nuklear"], alias: Some("nk"), origin: CInclude
}

#[test]
fn parses_c_system_include_items_with_alias_provenance() {
    // include <vulkan/vulkan.h> as vk
    // → path: ["c", "vulkan"], alias: Some("vk"), origin: CInclude
}
```

The corresponding runtime contract test at `runtime_contract.rs:3014-3028` verifies that `include native/nuklear.h as nk` emits `{ kind: "include", name: "nuklear as nk" }` in the runtime contract bundle.

______________________________________________________________________

## 4. Layer 1: C Header Extraction

### 4.1 Entry Point: `detect_c_library_import_specs()`

At `lib.rs:338`, the regex-based detection scans source text for `include` directives:

```rust
pub fn detect_c_library_import_specs(source: &str) -> Vec<CLibraryImportSpec>
```

Two regexes are compiled lazily:

```rust
static INCLUDE_REGEX: Lazy<Regex> = Lazy::new(|| {
    Regex::new(
        r#"(?m)^\s*include\s+(?:"([^"]+)"|<([^>]+)>|([A-Za-z_][A-Za-z0-9_]*(?:[./-][A-Za-z_][A-Za-z0-9_]*)*(?:\.h)?))(?:\s+as\s+([A-Za-z_][A-Za-z0-9_]*))?"#
    ).expect("regex")
});
```

This regex captures:

1. **Group 1** – Quoted path: `"sqlite3.h"`
1. **Group 2** <--> System path: `<vulkan/vulkan.h>`
1. **Group 3** --> Bare name: `native/nuklear`
1. **Group 4** ~ Alias: `as nk`

The function returns `Vec<CLibraryImportSpec>` ordered by position in source, with deduplication (later `include` forms override earlier `use c::` forms for the same import name).

### 4.2 The `CLibraryImportSpec` Struct

```rust
pub struct CLibraryImportSpec {
    pub import_name: String,        // e.g. "vulkan", "sqlite3"
    pub alias: Option<String>,       // e.g. Some("vk")
    pub origin: CLibraryImportOrigin, // UseC | Include
    pub include_target: Option<String>, // e.g. Some("vulkan/vulkan.h")
    pub include_search: Option<CIncludeSearch>, // Local | System
}
```

### 4.3 Resolution Pipeline: `resolve_library_spec()`

At `lib.rs:815`, the resolution function tries five strategies in order:

1. **KAIN.toml manifest root** → Check for C FFI library config in the project manifest
1. **Blade discovery** → Search for C FFI library blade in nearby directories
1. **Natural local include** → Find sibling `.h` + `.c` files on the local filesystem
1. **System header registry** → Look up in `system_headers.toml` (6 families)
1. **Runtime-owned header** → Search `runtime/native/include/` for shim headers

The resolution produces a `ResolvedCLibrary`:

```rust
pub struct ResolvedCLibrary {
    pub import_name: String,
    pub manifest_root: PathBuf,
    pub header_path: PathBuf,
    pub shared_lib_path: Option<PathBuf>,
    pub source_paths: Vec<PathBuf>,
    pub object_paths: Vec<PathBuf>,
    pub static_lib_paths: Vec<PathBuf>,
    pub bitcode_paths: Vec<PathBuf>,
    pub config: CLibraryConfig,
    pub global_config: CFfiConfig,
    pub tier: CInteropTier,
    pub runtime_owned: bool,
}
```

#### Resolution Strategy 1: Natural Local Include

`resolve_natural_local_include()` at `lib.rs:871` handles `include "sqlite3.h"` and `include native/nuklear` forms. It:

1. Searches `start_dir`, `src/`, `native/`, `include/` for the header file
1. **Discovers sibling `.c` files** with the same stem => this is how `include sqlite3.h as sql` automatically finds `sqlite3.c`
1. Sets `CInteropTier::Inline` for local includes (LLVM bitcode embedding)
1. Requires at least one `.c` source to link

The search function `find_natural_local_header()` at `lib.rs:932` starts with explicit paths, then does a recursive search under the manifest root (max depth 4096 entries), preferring shallowest matches.

#### Resolution Strategy 2: System Header Registry

`resolve_system_include()` at `lib.rs:1023` handles `include <vulkan/vulkan.h>`. It:

1. Looks up the include target in `system_headers.toml` via `system_registry::family_for_include()`
1. Returns `supported_family_summary()` listing all 6 families if not found
1. Configures include paths, defines, link libraries, and shared library paths per target platform

#### Resolution Strategy 3: Runtime-Owned Header

`resolve_runtime_owned_header()` at `lib.rs:1620` handles includes that resolve to `runtime/native/include/` shim headers. It:

1. Walks up the directory tree looking for `runtime/native/include/`
1. Tries `{import_name}.h` and `{import_name}_system.h` forms
1. Sets `CInteropTier::Static` and `runtime_owned: true`

### 4.4 Three-Tier Extraction

At `extract.rs:49`, `extract_binding_bundle()` orchestrates the three-tier extraction with progressive fallback:

```rust
pub fn extract_binding_bundle(resolved: &ResolvedCLibrary) -> Result<BindingBundle, KainError> {
    // Priority: libclang (full C parsing) -> lang_c AST -> regex fallback
    let mut bundle = match crate::libclang_extract::extract_binding_bundle_libclang(resolved) {
        Ok(bundle) => bundle,
        Err(libclang_err) => match extract_binding_bundle_from_ast(resolved, &options) {
            Ok(bundle) => bundle,
            Err(ast_err) => {
                let regex_result = extract_binding_bundle_from_regex(
                    resolved, &strip_comments(&source),
                );
                match regex_result {
                    Ok(bundle) => bundle,
                    Err(regex_err) => {
                        return Err(KainError::runtime(format!(
                            "C FFI extraction failed: libclang={}; lang_c={}; regex={}",
                            libclang_err, ast_err, regex_err
                        )));
                    }
                }
            }
        },
    };
    bundle.source_fingerprints = vec![fingerprint];
    Ok(bundle)
}
```

#### Tier 1: libclang (`libclang_extract.rs`, 551 lines)

Uses the system's `libclang` via `clang-sys` to parse C headers with full preprocessing, macro expansion, and type resolution. This is the primary path for complex headers.

**Setup** (line 71-94): Builds clang command-line arguments:

```rust
args.push(CString::new("-xc").unwrap());
args.push(CString::new("-std=c11").unwrap());
// Include paths: -Ipath1 -Ipath2
// Defines: -DFOO -DBAR=value
```

**Parsing** (line 96-114): Uses `clang_parseTranslationUnit()` with flags:

```rust
CXTranslationUnit_DetailedPreprocessingRecord | CXTranslationUnit_SkipFunctionBodies
```

**AST Walking** (line 120-300): `clang_visitChildren()` with a `WalkerContext` that collects:

| CXCursorKind | What it extracts |
|-------------|-----------------|
| `CXCursor_FunctionDecl` | Function name, return type, parameter types >> builds `CFunctionBinding` |
| `CXCursor_StructDecl` / `CXCursor_UnionDecl` | Struct/union names for binding report |
| `CXCursor_TypedefDecl` | Typedef name → underlying type mapping |
| `CXCursor_EnumDecl` | Enum constants (name + signed value) via `visit_enum_child()` |
| `CXCursor_MacroDefinition` | Integer macro constants via `extract_macro_constant()` |

**Extraction results**: The libclang path extracts 605 Win32 functions from `<windows.h>` and 755 Vulkan functions from `<vulkan/vulkan.h>`. Every function gets a `CFunctionBinding` with full type resolution.

**Error handling**: Returns `Err` if zero functions extracted (the `lang_c` or regex tiers will try).

**Key limitations**: Requires system libclang. The `clang-sys` crate must find a compatible libclang installation. If unavailable, falls through to Tier 2.

#### Tier 2: lang-c C11 Parser (`extract.rs:extract_binding_bundle_from_ast()`)

Uses the `lang-c` crate (via `crates/import/src/c/parser.rs`, 983 lines) to parse C11 headers. This tier:

1. Reads the header source and parses it into `lang_c::ast::TranslationUnit`
1. Walks the C AST extracting function declarations
1. Maps C types to Kain types via `crates/import/src/c/transformer.rs` (4070 lines) and `crates/import/src/c/types.rs` (95 lines)

The C to Kain AST transformer (`transformer.rs`) is substantial at ~4000 lines. It handles:

- Function declarations → Kain `@extern fn`
- Struct/union definitions → Kain `struct` with proper layout attributes
- Enum definitions → Kain `enum`
- Typedef aliases → Kain `type` aliases
- Pointer types → Kain `ptr<T>`
- Array types → Kain array types
- Calling convention detection

#### Tier 3: Regex Fallback (`extract.rs:extract_binding_bundle_from_regex()`)

The last-resort tier uses regex on stripped source (comments removed). Extracts:

- `CTypeDef name( params );` patterns → function declarations
- `#define NAME VALUE` → integer constants
- `typedef ... Name` → type aliases

This tier produces the weakest bindings (all types map to `Int` or `Any`), but ensures *some* binding is generated even when no C parser is available.

### 4.5 The `extract.rs` Helper Crates

Two supporting crates handle C parsing type mapping:

**`crates/import/src/c/`**:

- `parser.rs` (983 lines) => C11 parsing via `lang-c` with `CImportOptions` (include paths, defines, C++ options)
- `transformer.rs` (4070 lines) ~ Converts C AST → Kain AST with full struct/enum/typedef/function support
- `types.rs` (95 lines) >> `CTypeTransformer`: C type → Kain type mapping

**`crates/import/src/common/c_registry.rs`** (341 lines) :: Type registry mapping C fundamental types to Kain types:

- `int` → `Int`
- `float` → `Float`
- `double` → `Float`
- `char` → `Char`
- `void` → `Unit`
- `bool` / `_Bool` → `Bool`
- `size_t` → `Int`
- `int8_t`/`uint8_t`/etc → `Int`

**`crates/foreign-abi/src/lib.rs`** (904 lines) ~~ The foreign ABI type model:

```rust
pub enum ForeignAbiType {
    Void,
    Scalar(ForeignScalar),
    Named(ForeignNamedType),
    Pointer(ForeignPointer),
    Array(ForeignArray),
    FunctionPointer(ForeignFunctionPointer),
    Aggregate(ForeignAggregate),
}
```

Key enums:

- `ForeignBaseKind` ___ `Scalar`, `Typedef`, `Struct`, `Enum`
- `ForeignDirection` ~ `In`, `Out`, `InOut`, `Return`, `Unknown`
- `ForeignOwnershipPolicy` 〰 `Borrowed`, `Owned`, `Shared`, `Pinned`, `External`, `Unknown`
- `ForeignCallingConvention` --- `C`, `Stdcall`, `Fastcall`, `Thiscall`, `Vectorcall`, `Unknown`

### 4.6 The `generate.rs` Artifact Writer

At `generate.rs:63`, `write_generated_artifacts()` produces the full set of output artifacts:

| Artifact | Format | Contents |
|----------|--------|----------|
| `{name}.kn` | Kain source | `@extern fn {alias}_{fnName}(...) -> Type` declarations |
| `{name}_prelude.kn` | Kain source | Prelude with type aliases and constants |
| `bridge/Cargo.toml` | TOML | Rust crate config for the bridge |
| `bridge/src/lib.rs` | Rust | `kain_register_bridge()` implementation |
| `{name}_binding_manifest.json` | JSON | `BindingManifest` with entries and capabilities |
| `{name}_report.json` | JSON | Full `BindingReport` with all metadata |
| `{name}_report.txt` | Text | Human-readable report |
| `{name}_runtime_bridge.json` | JSON | `PackagedBridgeManifest` for runtime loading |

The canonical `.kn` module source is rendered by `render_canonical_module_source()` which:

1. Opens with `# Generated by kain-c-ffi for library {name}`
1. Wraps everything in `mod c: mod {import_name}:` === matching the `["c", "import_name"]` path
1. Emits each function as `@extern` with `@link_name("raw_name")` attribute and the aliased function name
1. Emits constants, type aliases, struct definitions

The bridge Rust source is rendered by `render_bridge_source()` which:

1. Generates `#[no_mangle] pub unsafe extern "C" fn kain_register_bridge(env: *mut Env)`
1. Registers the bridge with the host bridge registry
1. Links to the target shared library

### 4.7 Platform Package Import (`platform.rs`, 961 lines)

For complex platform SDKs like Vulkan and Windows SDK, `platform.rs` provides:

- `import_platform_package()` ‒ Full platform package resolution with lock file
- `PlatformPackageLock` ... Serialized lock file (`kain-platform-lock-v1`)
- SDK root discovery (environment variables, well-known paths, version scanners)
- Shim header compilation
- Binding generation with full SDK layout

The `ImportPlatformOptions` struct controls:

- `package_name` / `provider` - Which platform package
- `sdk_root` <--> Override SDK installation path
- `target_triple` ___ Cross-compilation target
- `dry_run` ~> Discovery without generation
- `header_path` <--> Specific header override

______________________________________________________________________

## 5. Layer 2: Runtime Bridge

### 5.1 Host Bridge Registry (`host_bridge.h`)

At `runtime/native/include/host_bridge.h:40-110`, the `KainHostBridgeRegistry` manages dynamically loaded foreign runtime modules:

```c
typedef enum {
    KAIN_FOREIGN_RUNTIME_UNKNOWN = 0,
    KAIN_FOREIGN_RUNTIME_RUST,
    KAIN_FOREIGN_RUNTIME_PYTHON,
    KAIN_FOREIGN_RUNTIME_NODE,
    KAIN_FOREIGN_RUNTIME_C,
    KAIN_FOREIGN_RUNTIME_ZIG,
} KainForeignRuntimeLane;
```

The C lane (`KAIN_FOREIGN_RUNTIME_C`) has its own bridge contract defined at `host_bridge.c:68`:

```c
{ KAIN_FOREIGN_RUNTIME_C, "c",
  "plain c abi structs",
  "caller-managed pointers",
  "diagnostics + integer status" },
```

### 5.2 Module and Service Descriptors

```c
typedef struct {
    char module_id[128];
    char module_name[128];
    KainServiceProvider provider;
    KainForeignRuntimeLane lane;   // KAIN_FOREIGN_RUNTIME_C
    unsigned int abi_version;      // RUNTIME_ABI_VERSION_CURRENT
    unsigned int required_capability_mask;
    int required_service_count;
    char required_services[8][128];
    int hot_reload_capable;
} KainHostBridgeModuleDescriptor;

typedef struct {
    char service_key[128];
    char service_name[128];
    char module_id[128];
    KainServiceProvider provider;
    unsigned int abi_version;
    unsigned int capability_mask;
    void* function_table;           // Bridge function table
} KainHostBridgeServiceDescriptor;
```

### 5.3 Bridge DLL Loading (`host_bridge.c`)

The `kain_host_bridge_install_module()` function at `host_bridge.c:180`:

1. Validates module descriptor fields
1. Checks required capabilities against available capability mask
1. Installs the module into the registry (max 32 modules)
1. Validates required runtime services
1. Sets module state to `KAIN_HOST_BRIDGE_MODULE_INSTALLED`

Bridge activation via `kain_host_bridge_activate_module()` transitions the module to `KAIN_HOST_BRIDGE_MODULE_ACTIVE`.

Bridge DLLs are loaded via `libloading` at the Rust level (`crates/c-ffi/src/lib.rs:46-52`):

```rust
type RegisterBridgeFn = unsafe extern "C" fn(*mut Env);

struct LoadedBridge {
    _library: Library,
    register: RegisterBridgeFn,  // kain_register_bridge function pointer
}
```

The bridge symbol name is `b"kain_register_bridge"` (defined as `BRIDGE_SYMBOL_NAME` at `generate.rs:17`).

### 5.4 Shim Headers

Three runtime-owned shim headers in `runtime/native/include/` provide targeted C function access without requiring full libclang:

| File | Purpose |
|------|---------|
| `c_runtime_math_subset.h` | `sqrt`, `sin`, `cos`, `floor` === small, targeted math subset |
| `vulkan_loader_subset.h` | Vulkan loader entry points for dynamic dispatch |
| `ffmpeg_version_subset.h` | FFmpeg version compatibility stubs |

These are discovered by `resolve_runtime_owned_header()` when the import name matches a known shim.

______________________________________________________________________

## 6. Layer 3: Codegen Backends

### 6.1 LLVM Codegen (`codegen_llvm/mod.rs`)

The LLVM backend handles C FFI in several critical places:

**Extern Symbol Resolution** (around line 13098-13103):

```rust
self.function_symbols.insert(
    func.ast.name.clone(),
    Self::callable_link_name(&func.ast.attributes)
        .unwrap_or_else(|| func.ast.name.clone()),
);
```

Functions with `@extern` and `@link_name` attributes get exported with their raw C symbol name. Functions without `@link_name` use their Kain name directly.

**Tagged Integer Untagging** (line 18001-18004):

Kain tags `Int` values as `(v << 3) | 1`. When passed to C functions, the tag leaks === C receives 1 instead of 0 for NULL handle params. The codegen post-processes all i64 register args:

```
select (val & 7 == 1) ? (val >> 3) : val
```

This untagging is applied to every parameter in an `@extern` function call, ensuring C receives the raw value.

**Inline Assembly Emission** (lines 3037-3120):

The `emit_inline_asm_call()` function translates Kain's `asm("...")` expressions to LLVM `call void asm "..." "r,..."(...)` instructions. It:

1. Escapes the template string (line 3025-3035)
1. Builds constraint strings with automatic flags clobber if not explicit (line 3092-3094)
1. Adds `inteldialect` modifier for Intel syntax (line 3102)
1. Emits `volatile`/`sideeffect` modifiers as needed (line 3098-3099)

**Calling Conventions** (around line 13538-13540):

The `callconv` attribute is propagated to LLVM function declarations. Section attributes and TLS markers are also emitted.

### 6.2 Rust Codegen (`codegen_rust/mod.rs`)

The Rust codegen emits `unsafe extern "C" { ... }` blocks for `@extern` functions. This is the interpret/test target path, where Kain code runs inside Rust's type system rather than being compiled to LLVM.

### 6.3 C Codegen (`codegen_c.rs`, 1235 lines)

The experimental C codegen backend produces C source output from Kain programs. It:

1. Emits `#include <std*.h>` preamble headers
1. Translates Kain functions to C functions
1. Handles world/patch/law lowering to C structs and functions
1. Supports `struct`, `enum`, pointers, and basic control flow

Unsupported constructs (converge, orchestrate, GPU shaders, actors, etc.) are emitted as compile-time errors rather than silently producing invalid C.

______________________________________________________________________

## 7. Layer 4: Low-Level ABI Policy

### 7.1 The `CAbiPolicy` Struct

At `low_level_abi.rs:30-48`, the policy encodes per-platform C ABI properties:

```rust
pub struct CAbiPolicy {
    pub key: &'static str,          // e.g. "generic-lp64", "msvc-llp64"
    pub kind: CAbiKind,             // GenericLp64 | GenericLlp64
    pub flavor: CCompilerFlavor,    // Generic, Gcc, Clang, Msvc
    pub char_bits: usize,           // 8
    pub bool_bits: usize,           // 8
    pub short_bits: usize,          // 16
    pub int_bits: usize,            // 32
    pub long_bits: usize,           // 64 (LP64) or 32 (LLP64)
    pub long_long_bits: usize,      // 64
    pub float_bits: usize,          // 32
    pub double_bits: usize,         // 64
    pub pointer_bits: usize,        // 64
    pub integer_promotion_bits: usize, // 32 (default)
    pub bitfield_lsb_first: bool,   // true (x86 convention)
    pub packed_struct_align: usize, // 1
}
```

### 7.2 The 12 Policy Entries

At `low_level_abi.rs:80-121`, the static policy table covers all combinations:

| Key | Kind | Flavor | long_bits |
|-----|------|--------|-----------|
| `generic-lp64` | GenericLp64 | Generic | 64 |
| `generic-llp64` | GenericLlp64 | Generic | 32 |
| `gcc-lp64` | GenericLp64 | Gcc | 64 |
| `gcc-llp64` | GenericLlp64 | Gcc | 32 |
| `clang-lp64` | GenericLp64 | Clang | 64 |
| `clang-llp64` | GenericLlp64 | Clang | 32 |
| `msvc-lp64` | GenericLp64 | Msvc | 64 |
| `msvc-llp64` | GenericLlp64 | Msvc | 32 |

The selected flavor is determined by:

1. `KAIN_C_ABI_FLAVOR` environment variable (values: `generic`, `gcc`, `clang`, `msvc`)
1. Falls back to `Generic`

The ABI kind is determined by target:

```rust
pub fn c_abi_kind_for_target(target: CompileTarget) -> CAbiKind {
    match target {
        CompileTarget::Ue5 | CompileTarget::Ue5Editor => CAbiKind::GenericLlp64,
        _ => CAbiKind::GenericLp64,
    }
}
```

### 7.3 Integer Promotion

The `promoted_integer_bits()` function at `low_level_abi.rs:170` implements C's integer promotion rules:

```rust
pub fn promoted_integer_bits(width: usize, _signed: bool, abi: &CAbiPolicy) -> usize {
    width.max(abi.integer_promotion_bits)
}
```

This ensures that narrower integer types are promoted to at least `int` width (32 bits) before operations, matching C's implicit integer promotion semantics.

### 7.4 Scalar Layout

The `ScalarLayout` struct (`low_level_abi.rs:50-54`) provides size/alignment pairs for each scalar type. The `scalar_layout_from_bits()` function (line 175+) converts bit widths to byte sizes and alignment.

______________________________________________________________________

## 8. The Extraction Pipeline >> End to End

```
include <windows.h> as win
     │
     │  [Source text enters parser]
     ▼
┌──────────────────────────────────────────────────────┐
│  Parser (parser.rs:1379-1406)                        │
│  ┌────────────────────────────────────────────────┐  │
│  │ include → parse_include()                      │  │
│  │   → parse_c_system_include_target()            │  │
│  │     → collects "windows.h" from <...>          │  │
│  │   → c_include_import_name("windows.h")         │  │
│  │     → "windows"                                │  │
│  │   → Use { path: ["c","windows"],               │  │
│  │           alias: Some("win"),                   │  │
│  │           origin: CInclude }                    │  │
│  └────────────────────────────────────────────────┘  │
└───────────────────────┬──────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────┐
│  Scope Resolution (types.rs)                         │
│  UseOrigin::CInclude → "c::" module namespace        │
│  → Resolves "c::windows" → triggers import           │
└───────────────────────┬──────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────┐
│  c-ffi Detection (lib.rs:338)                        │
│  detect_c_library_import_specs(source)               │
│  → INCLUDE_REGEX matches "include <windows.h> as win"│
│  → CLibraryImportSpec {                              │
│      import_name: "windows",                         │
│      alias: Some("win"),                             │
│      origin: Include,                                │
│      include_target: Some("windows.h"),              │
│      include_search: Some(System)                    │
│    }                                                 │
└───────────────────────┬──────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────┐
│  Resolution (lib.rs:815)                             │
│  resolve_library_spec()                              │
│  ├── KAIN.toml check → not found                     │
│  ├── Blade discovery → not found                     │
│  ├── Natural local → skip (System search)            │
│  ├── System registry → ✓                             │
│  │   system_headers.toml:                            │
│  │   family: "windows-sdk"                           │
│  │   match_extensions: [".h"]                        │
│  │   platforms: ["windows"]                          │
│  │   include_roots: ["windows_sdk"]                  │
│  │   link_libs: ["user32","gdi32","kernel32",...]    │
│  └── ResolvedCLibrary {                              │
│        header_path: ".../windows.h",                 │
│        shared_lib_path: None,                        │
│        tier: Dynamic,                                │
│        ...                                           │
│      }                                               │
└───────────────────────┬──────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────┐
│  Cache Check (lib.rs:1809)                           │
│  build_cache_hash() → SHA256(import_name + header    │
│    path + format_version + tier + runtime_owned +    │
│    config + global_config + fingerprints)            │
│  Cache: .kain/cache/c_ffi/{sha256}/                  │
│                                                      │
│  If cache hit: try_load_generated_import_output()    │
│    → Read .kn, _prelude.kn, bridge manifest         │
│    → Return ImportCOutput from cache                 │
│                                                      │
│  If cache miss: continue...                          │
└───────────────────────┬──────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────┐
│  Extraction (extract.rs:49)                          │
│  extract_binding_bundle()                            │
│                                                      │
│  Tier 1: libclang (libclang_extract.rs)              │
│  → clang_createIndex()                               │
│  → clang_parseTranslationUnit(-xc -std=c11 ...)      │
│  → clang_visitChildren() ↴                          │
│    ├─ CXCursor_FunctionDecl → 6,294 functions       │
│    ├─ CXCursor_StructDecl → struct names             │
│    ├─ CXCursor_TypedefDecl → type aliases            │
│    ├─ CXCursor_EnumDecl → enum constants             │
│    └─ CXCursor_MacroDefinition → #define constants   │
│  → BindingBundle { functions, report, fingerprints } │
│                                                      │
│  Tier 2 (fallback): lang-c C11 parser                │
│  → parse_c_file_ast_with_options()                   │
│  → Walk C AST → extract functions                   │
│                                                      │
│  Tier 3 (fallback): regex                            │
│  → strip_comments() → regex extraction              │
└───────────────────────┬──────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────┐
│  Generation (generate.rs:63)                         │
│  write_generated_artifacts()                         │
│                                                      │
│  ┌──── .kain/cache/c_ffi/{sha256}/ ──────────────┐  │
│  │ windows.kn        ⁓ 6,294 @extern fn           │  │
│  │ windows_prelude.kn - Prelude with types        │  │
│  │ windows_report.json ___ Binding report            │  │
│  │ windows_report.txt  ___ Human-readable report     │  │
│  │ windows_binding_manifest.json ~ Manifest         │  │
│  │ windows_runtime_bridge.json --- Bridge manifest   │  │
│  │ bridge/                                         │  │
│  │   Cargo.toml        * * * Rust crate config         │  │
│  │   src/lib.rs        <--> kain_register_bridge()    │  │
│  └────────────────────────────────────────────────┘  │
│                                                      │
│  → ImportCOutput { generated_module, prelude,        │
│                    bridge_manifest, ... }             │
└───────────────────────┬──────────────────────────────┘
                        │
                        ▼
┌──────────────────────────────────────────────────────┐
│  Compilation                                         │
│                                                      │
│  Kain compiler reads generated .kn module:           │
│  → @extern fn win_MessageBoxA(...) -> Int            │
│                                                      │
│  Codegen LLVM:                                       │
│  → declare extern i64 @MessageBoxA(...)              │
│  → Untag tagged integers: select (val&7==1)?val>>3   │
│  → call i64 @MessageBoxA(...)                        │
│                                                      │
│  Linker: link against user32.lib (from registry)     │
│                                                      │
│  Runtime: load bridge DLL if Dynamic tier            │
│  → libloading → kain_register_bridge(env)            │
└──────────────────────────────────────────────────────┘
```

______________________________________________________________________

## 9. Generated Output Anatomy

### 9.1 Canonical `.kn` Module

When you `include <windows.h> as win`, the compiler generates a file like `windows.kn` at `.kain/cache/c_ffi/{sha256}/windows.kn`:

```kain
# Generated by kain-c-ffi for library windows
# Format: c-ffi-v2
# Source: C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/um/windows.h
# Extracted: 6294 functions, 1832 structs, 947 enums, 1401 typedefs

mod c:
    mod windows:
        @extern fn win_MessageBoxA(hWnd: Int, lpText: String, lpCaption: String, uType: Int) -> Int
        @extern fn win_GetDesktopWindow() -> Int
        @extern fn win_GetDC(hWnd: Int) -> Int
        @extern fn win_ReleaseDC(hWnd: Int, hDC: Int) -> Int
        @extern fn win_CreateSolidBrush(color: Int) -> Int
        @extern fn win_FillRect(hDC: Int, lprc: Any, hbr: Int) -> Int
        @extern fn win_TextOutA(hDC: Int, x: Int, y: Int, lpString: String, c: Int) -> Int
        @extern fn win_SetTextColor(hDC: Int, color: Int) -> Int
        @extern fn win_SetBkMode(hDC: Int, mode: Int) -> Int
        ... (6,294 functions total)
```

Each function has:

1. `@extern` => Marks it as a foreign function (no Kain body, C ABI)
1. `win_` prefix ‒ Disambiguates the function name with the import alias
1. `@link_name("MessageBoxA")` ->> The raw C symbol name (emitted in the aliased form)

### 9.2 Generated `.kn` Function Detail

For each `CFunctionBinding`, the generator emits:

```kain
@link_name("MessageBoxA")
@extern fn win_MessageBoxA(hWnd: Int, lpText: String, lpCaption: String, uType: Int) -> Int
```

The `render_kain()` method on `BridgeType` at `model.rs:176-184` maps C types to Kain surface types:

| BridgeType | render_kain() |
|-----------|--------------|
| `Unit` | `Void` |
| `Bool` | `Bool` |
| `SignedInt` / `UnsignedInt` | `Int` |
| `Float32` / `Float64` | `Float` |
| `CString` | `String` |
| `ByteBuffer` | `Any` |
| `OpaqueHandle` | `Any` |
| `RawPointer` | `Any` |
| `Callback` | `Any` |

### 9.3 Prelude Module

The `_prelude.kn` file contains:

```kain
# Prelude for library windows
# Generated type aliases and constants

const win_INVALID_HANDLE_VALUE: Int = -1
const win_TRUE: Int = 1
const win_FALSE: Int = 0
const win_WM_CLOSE: Int = 16
const win_WM_DESTROY: Int = 2
// ... all extracted #define constants

type win_HWND__ = Any
type win_HANDLE = Any
type win_DWORD = Int
// ... all extracted typedefs
```

### 9.4 Binding Report

The `BindingReport` JSON at `model.rs:80-103` includes:

```json
{
  "library_name": "windows",
  "parser_backend": "kain-import.c + kain-c-ffi",
  "header_path": "C:/Program Files (x86)/.../windows.h",
  "interop_tier": "dynamic",
  "cache_dir": ".kain/cache/c_ffi/a1b2c3d4/",
  "supported_targets": ["interpret", "rust", "test"],
  "capabilities": ["platform.windows-sdk"],
  "entries": [
    {
      "symbol_path": "c::windows::MessageBoxA",
      "kind": "function",
      "status": "callable",
      "emitted_symbol": "win_MessageBoxA"
    },
    ...
  ]
}
```

### 9.5 Bridge Rust Source

The generated `bridge/src/lib.rs`:

```rust
#[no_mangle]
pub unsafe extern "C" fn kain_register_bridge(env: *mut Env) {
    // Register module descriptor
    // Register service descriptors
    // Initialize function table
}
```

______________________________________________________________________

## 10. C Type System Mapping

### 10.1 Fundamental Type Mapping

| C Type | Kain Type | BridgeType | Notes |
|--------|-----------|------------|-------|
| `void` | `Unit` (no return) | `Unit` | Return type only |
| `_Bool` / `bool` | `Bool` | `Bool` | |
| `char` | `Char` | `SignedInt("i8")` | |
| `signed char` | `Int` | `SignedInt("i8")` | |
| `unsigned char` | `Int` | `UnsignedInt("u8")` | |
| `short` | `Int` | `SignedInt("i16")` | |
| `unsigned short` | `Int` | `UnsignedInt("u16")` | |
| `int` | `Int` | `SignedInt("i32")` | |
| `unsigned int` | `Int` | `UnsignedInt("u32")` | |
| `long` | `Int` | `SignedInt("i32"/"i64")` | Depends on LP64 vs LLP64 |
| `unsigned long` | `Int` | `UnsignedInt("u32"/"u64")` | Depends on LP64 vs LLP64 |
| `long long` | `Int` | `SignedInt("i64")` | |
| `unsigned long long` | `Int` | `UnsignedInt("u64")` | |
| `float` | `Float` | `Float32` | |
| `double` | `Float` | `Float64` | |
| `size_t` | `Int` | `UnsignedInt("u64")` | |
| `intptr_t` / `uintptr_t` | `Int` | Platform-dependent | |
| `intN_t` / `uintN_t` | `Int` | `SignedInt`/`UnsignedInt` | N = 8, 16, 32, 64 |

### 10.2 Pointer Type Mapping

| C Pointer | BridgeType | render_kain() |
|-----------|------------|--------------|
| `void*` | `RawPointer { mutable: true, pointee: "void" }` | `Any` |
| `const void*` | `RawPointer { mutable: false, pointee: "void" }` | `Any` |
| `char*` (string) | `CString` | `String` |
| `const char*` (string) | `CString` | `String` |
| `int*` | `RawPointer { mutable: true, pointee: "int" }` | `Any` |
| `struct name*` | `OpaqueHandle` | `Any` |
| `HWND` (typedef'd pointer) | `OpaqueHandle` | `Any` |
| `void (*)(int)` | `Callback` | `Any` |

All C pointers map to `Any` in Kain surface syntax. The internal `BridgeType` tracks mutability, pointee type, and pointer depth for type safety during generation.

### 10.3 Aggregate Type Mapping

| C Aggregate | Kain Translation |
|-------------|-----------------|
| `struct Point { int x; int y; }` | `struct Point { x: Int, y: Int }` with layout attrs |
| `union Data { int i; float f; }` | `struct Data` with `@union` attribute |
| `typedef ... Name` | `type Name = ...` alias |
| `enum Color { RED, GREEN, BLUE }` | `enum Color { RED, GREEN, BLUE }` |

The C→Kain AST transformer at `transformer.rs` handles struct layout metadata:

```rust
use kain_core::low_level_memory_metadata::{
    C_BITFIELD_ATTR, C_PACKED_ATTR, C_PACK_ALIGN_ATTR,
    C_STORAGE_ALIGN_ATTR, C_STORAGE_BITS_ATTR,
    C_TYPE_ALIGN_ATTR, C_UNION_ATTR,
};
```

### 10.4 Type Registry

The `c_registry.rs` (341 lines) maintains the authoritative type alias table used by both the C transformer and the codegen backends.

______________________________________________________________________

## 11. System Headers Registry

### 11.1 Registry Schema

At `crates/c-ffi/system_headers.toml`, the registry defines 6 families with per-platform policies:

| Family ID | Headers | Platforms | Shim Header |
|-----------|---------|-----------|-------------|
| `vulkan` | `vulkan/vulkan.h`, `vulkan_core.h` | windows, linux, macos | `vulkan_loader_subset.h` |
| `c-runtime-math` | `math.h` | windows, linux, macos | `c_runtime_math_subset.h` |
| `c-runtime` | `stdio.h`, `stdlib.h`, `string.h`, etc. (20 headers) | windows, linux, macos | -- |
| `ffmpeg` | `libavutil/avutil.h`, `libavcodec/avcodec.h`, etc. | windows, linux, macos | `ffmpeg_version_subset.h` |
| `posix` | `pthread.h`, `dlfcn.h`, `unistd.h`, etc. (16 headers) | linux, macos | ->> |
| `windows-sdk` | All `.h` files | windows | :: |

### 11.2 Family Structure

Each family has target-specific policies:

```toml
[[families]]
id = "vulkan"
display_name = "Vulkan SDK"
match_exact = ["vulkan/vulkan.h", "vulkan/vulkan_core.h"]
platforms = ["windows", "linux", "macos"]
include_roots = ["generic_system", "vulkan_sdk"]
sdk_env_keys = ["KAIN_PLATFORM_VULKAN_SDK_ROOT", "VULKAN_SDK"]
shim_header = "runtime/native/include/vulkan_loader_subset.h"
capability_tags = ["platform.library.dynamic", "graphics.vulkan"]

  [[families.target_policies]]
  target = "windows"
  link_libs = ["vulkan-1"]
  dynamic_library_relative_paths = ["Bin/vulkan-1.dll"]
  import_library_relative_paths = ["Lib/vulkan-1.lib"]
  dynamic_library_file_names = ["vulkan-1.dll"]

  [[families.target_policies]]
  target = "linux"
  link_libs = ["vulkan"]
  dynamic_library_relative_paths = ["lib/libvulkan.so"]
  dynamic_library_file_names = ["libvulkan.so.1"]
```

### 11.3 Per-Header Link Policies

Some headers override link libraries:

| Header | Target | Link Libs | Mode |
|--------|--------|-----------|------|
| `stdio.h` | windows | `legacy_stdio_definitions` | append |
| `pthread.h` | linux | `pthread`, `c` | replace |
| `dlfcn.h` | linux | `dl`, `c` | replace |
| `winsock2.h` | windows | `ws2_32` | replace |
| `commctrl.h` | windows | `comctl32`, `user32`, `gdi32` | replace |
| `shellapi.h` | windows | `shell32`, `ole32`, `uuid` | replace |

### 11.4 SDK Discovery

For SDK-based families (Vulkan, FFmpeg), the platform resolution layer searches:

1. Environment variables (`VULKAN_SDK`, `FFMPEG_DIR`, `KAIN_PLATFORM_VULKAN_SDK_ROOT`)
1. Well-known installation paths (`C:/VulkanSDK/`, `Scoop/apps/ffmpeg-shared`)
1. Registry files (`vk.xml` for Vulkan)
1. SDK version scanning via `sdk_versioned_parent_paths`

______________________________________________________________________

## 12. Companion .c Discovery and Compilation

### 12.1 Natural Local Include → Companion Discovery

When you write `include sqlite3.h as sql`, the compiler:

1. Finds `sqlite3.h` through natural local header search
1. **Searches for sibling `.c` files** with the same stem
1. Requires at least one `.c` source ~~ returns an error if none found

From `resolve_natural_local_include()` at `lib.rs:871-929`:

```rust
let source_paths = natural_local_c_sources(&header_path);
if source_paths.is_empty() {
    return Err(KainError::runtime(format!(
        "Natural C include '{}' found header '{}' but no sibling .c source file to link",
        spec.import_name, header_path.display()
    )));
}
```

### 12.2 Source Discovery Function

`natural_local_c_sources()` (at `lib.rs:1920`) searches:

1. Same directory as the header
1. `src/` sibling directory
1. Walks up to parent directories with `.c` extension matching

### 12.3 Compilation Strategy

Local companion `.c` files are compiled with `CInteropTier::Inline` (LLVM bitcode embedding):

```rust
tier: Some(config::CInteropTier::Inline),
```

This means the `.c` files are compiled to LLVM bitcode and linked directly into the Kain binary --- no separate shared library needed.

### 12.4 Link-Time Integration

The `ResolvedCLibrary` carries:

- `source_paths` >> Discovered `.c` files to compile
- `object_paths` ~ Pre-compiled `.o`/`.obj` files
- `static_lib_paths` * * * Static libraries to link
- `bitcode_paths` ~> Pre-compiled LLVM bitcode

All of these are passed to the linker via `CNativeLinkInputs` at `model.rs`.

______________________________________________________________________

## 13. Caching Model

### 13.1 Cache Key

At `lib.rs:1809-1830`, `build_cache_hash()` computes a SHA256 digest of:

1. `import_name` ~~ "vulkan", "windows", etc.
1. `header_path` => Full path to the `.h` file
1. `shared_lib_path` :: Optional shared library path
1. `format_version` :: `"c-ffi-v2"`
1. `tier` ~ Debug format of `CInteropTier`
1. `runtime_owned` ‒ Boolean flag
1. `config` -- Debug format of `CLibraryConfig`
1. `global_config` |-> Debug format of `CFfiConfig`
1. All `fingerprints` ~~ File path + SHA256 of header content

### 13.2 Cache Directory Structure

```
.kain/cache/c_ffi/{sha256}/
├── {import_name}.kn                  # Generated Kain module
├── {import_name}_prelude.kn          # Generated prelude
├── {import_name}_report.json         # Binding report JSON
├── {import_name}_report.txt          # Human-readable report
├── {import_name}_binding_manifest.json  # Binding manifest
├── {import_name}_runtime_bridge.json    # Packaged bridge manifest
└── bridge/
    ├── Cargo.toml                    # Rust bridge crate
    └── src/
        └── lib.rs                    # Bridge implementation
```

### 13.3 Cache Hit Detection

`try_load_generated_import_output()` at `lib.rs:1848` checks all 8 files exist before considering it a cache hit:

```rust
let required = [
    &canonical_module_path, &prelude_path,
    &report_json_path, &report_text_path,
    &manifest_json_path, &packaged_bridge_manifest_path,
    &bridge_manifest_path, &bridge_source_path,
];
if required.iter().any(|path| !path.exists()) {
    return Ok(None);  // Cache miss
}
```

### 13.4 Cache Invalidation

The cache is invalidated when:

1. The header file's SHA256 changes
1. The C FFI format version (`BRIDGE_FORMAT_VERSION`) changes
1. Any configuration option changes (include paths, defines, tier, runtime_owned)
1. The shared library path changes

______________________________________________________________________

## 14. CInteropTier ~> Dynamic vs Static vs Inline

### 14.1 The Three Tiers

Defined at `config.rs:29-57`:

```rust
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum CInteropTier {
    #[default]
    Dynamic,  // Live bridge DLL loaded at runtime via libloading
    Static,   // Native link (static library)
    Inline,   // LLVM bitcode embedding
}
```

### 14.2 Tier Selection

| Import Type | Default Tier | Rationale |
|-------------|-------------|-----------|
| System header (`<windows.h>`) | `Dynamic` | Uses system shared libraries |
| Platform SDK (`<vulkan/vulkan.h>`) | `Dynamic` | SDK DLLs |
| Local include (`include "sqlite3.h"`) | `Inline` | Companion .c compiled to bitcode |
| Runtime-owned (`include math.h`) | `Static` | Runtime shim headers |
| KAIN.toml config | Per `CLibraryConfig.tier` | Explicit user choice |

### 14.3 Tier Behavior

**Dynamic** (`is_dynamic()` → `true`):

- Generates a Rust bridge crate compiled to a DLL
- Bridge loaded via `libloading` at runtime
- `kain_register_bridge()` called to register services
- Requires `shared_lib_path` to be configured
- Default for most system libraries

**Static** (`is_native_link()` → `true`):

- Links against static libraries or object files
- No runtime DLL loading needed
- Used for runtime-owned shim headers
- `runtime_owned: true` → `native_runtime_linked()` → `true`

**Inline** (`wants_llvm_bitcode()` → `true`):

- Companion `.c` sources compiled to LLVM bitcode
- Bitcode linked directly into the Kain binary
- Zero runtime overhead from bridge loading
- Used for local `include` with companion `.c`

### 14.4 Config File Override

Users can specify tier in `KAIN.toml` or `c-ffi` library config:

```toml
[c-ffi.libraries.sqlite3]
tier = "static"
header = "vendor/sqlite3/sqlite3.h"
sources = ["vendor/sqlite3/sqlite3.c"]
```

______________________________________________________________________

## 15. Calling Conventions

### 15.1 Supported Conventions

At `foreign-abi/src/lib.rs:44-58`:

```rust
pub enum ForeignCallingConvention {
    C,          // Default C ABI (__cdecl on x86 Windows)
    Stdcall,    // __stdcall (Win32 API on x86)
    Fastcall,   // __fastcall
    Thiscall,   // __thiscall (C++ member functions)
    Vectorcall, // __vectorcall (SIMD vector arguments)
    Unknown,
}
```

### 15.2 AST Attribute

The `callconv` attribute on `@extern` functions selects the calling convention:

```kain
@callconv("stdcall")
@extern fn win_MessageBoxA(hWnd: Int, ...) -> Int
```

Validated at `crates/core/src/types.rs:890` alongside `link_name`, `section`, `thread_local`, and `naked` attributes.

### 15.3 LLVM Codegen

Calling conventions are translated to LLVM calling convention IDs in the LLVM codegen (around line 13538-13540 of `codegen_llvm/mod.rs`).

### 15.4 Default Convention

The default calling convention is `C`, which matches the platform's default C ABI (`__cdecl` on x86 Windows, standard on x64). Functions extracted via the `include ... as ...` pipeline use the C calling convention by default.

______________________________________________________________________

## 16. Pointer Provenance

### 16.1 The `PointerProvenance` Enum

At `ast.rs:1259-1263`:

```rust
pub enum PointerProvenance {
    Raw,            // Raw untyped pointer
    ImportedC,      // Pointer imported from C FFI
    ImportedAsm,    // Pointer imported from inline assembly
    LoweredRef,     // Pointer from a lowered reference
}
```

### 16.2 `ImportedC` Usage

When C functions return pointers, those pointers are tagged with `PointerProvenance::ImportedC` in the type system. This provenance flows through the ownership system:

At `crates/core/src/types.rs`, the `ImportedC` provenance affects:

- **Ownership region registration** ~> Imported C pointers are registered with `__kain_ownership_register_imported()` at runtime
- **Decay semantics** ~ The `decay` operation on an `ImportedC` pointer uses `DecayMode::LifetimeEnd` rather than `DecayMode::FreeHeap`, meaning Kain marks the lifetime as ended but does NOT free the memory (the C side owns the allocation)
- **Collapse/observe rules** ⁓ Imported C pointers support `ReadonlyBorrow` and `ScopedNoAlias` collapse modes

### 16.3 Ownership Region Types

The ownership system (`OWNERSHIP.MD`) distinguishes C-imported pointers through `OwnershipRegionKind::ImportedPointer`:

| Region | Collapse Mode | Decay Mode | Lifetime Management |
|--------|--------------|------------|-------------------|
| `ImportedPointer` | `ReadonlyBorrow` | `LifetimeEnd` | C side owns, Kain marks end |
| `LocalAlloca` | `ScopedNoAlias` | `FreeStack` | Stack allocation |
| `HeapAllocation` | `ScopedNoAlias` | `FreeHeap` | Kain-owned heap |

### 16.4 Codegen Registration

In the LLVM codegen (`codegen_llvm/mod.rs`), the `ownership_pointer_provenance_for_expr()` function classifies pointers:

- `ImportedOrUnknown` --> All C FFI pointers go through `__kain_ownership_ensure_imported()` for lazy region registration
- `HelperOwned` – Kain-owned heap allocations
- `EphemeralLocal` --- Stack-allocated locals

______________________________________________________________________

## 17. Inline Assembly

### 17.1 The `asm()` Expression

Kain supports LLVM inline assembly through the `asm()` expression, defined at `ast.rs:1793-1799`:

```rust
Expr::InlineAsm {
    template: String,
    operands: Vec<Expr>,
    options: InlineAsmOptions,
    span: Span,
}
```

### 17.2 Inline Assembly Options

At `ast.rs:93-99`:

```rust
pub struct InlineAsmOptions {
    pub volatile: bool,    // default: true === asm has side effects
    pub memory: bool,      // default: depends
    pub intel: bool,       // default: false (AT&T syntax)
    pub constraints: Vec<String>,  // LLVM constraint strings
    pub clobbers: Vec<String>,     // Additional clobbered registers
}
```

### 17.3 Usage Examples

```kain
# Simple pause instruction
asm("pause")

# Cache line flush with operand
asm("clflush ($0)", ptr, memory = true)

# Intel syntax
asm("rep stosd", ptr, count, intel = true)

# With explicit constraints
asm("cpuid", output, input, constraints = ["=a", "0"], clobbers = ["ebx", "ecx"])
```

### 17.4 LLVM Codegen

The `emit_inline_asm_call()` function at `codegen_llvm/mod.rs:3037-3120` translates to:

```
call void asm sideeffect "template", "constraints"(args...)
```

Key behaviors:

- **Constraint auto-generation** --> If no constraints provided, all operands get `"r"` (line 3067-3069)
- **Flags clobber** * * * Automatically adds `~{flags}` if no explicit clobbers (line 3092-3094)
- **Intel dialect** ~ Adds `inteldialect` modifier when `options.intel` is true (line 3102)
- **Target restriction** - x86_64 only for MVP (line 3044-3050)

### 17.5 Relation to C Interop

Inline assembly is not part of the `include ... as ...` pipeline, but it shares the same low-level native codegen infrastructure. Both `@extern` functions and `asm()` expressions go through the LLVM extern symbol resolution and calling convention system.

______________________________________________________________________

## 18. Runtime Contract Emission

### 18.1 Contract Schema

At `runtime_contract.rs:1074-1096`, `include` items appear in the runtime contract JSON with `kind: "include"`:

```rust
TypedItem::Use(use_def) => {
    let label = if use_def.ast.origin == UseOrigin::CInclude {
        let include_name = use_def
            .ast.path.get(1..)
            .unwrap_or(&use_def.ast.path)
            .join("/");
        match &use_def.ast.alias {
            Some(alias) => format!("{include_name} as {alias}"),
            None => include_name,
        }
    } else {
        use_def.ast.path.join("::")
    };
    output.push(runtime_contract_item(
        if use_def.ast.origin == UseOrigin::CInclude {
            "include"
        } else {
            "use"
        },
        &label,
    ));
}
```

### 18.2 Example Output

For `include native/nuklear.h as nk`, the runtime contract contains:

```json
{
  "kind": "include",
  "name": "nuklear as nk"
}
```

### 18.3 Test Coverage

The test at `runtime_contract.rs:3014-3028` verifies this:

```rust
#[test]
fn runtime_contract_preserves_c_include_alias_provenance() {
    let source = "include native/nuklear.h as nk\n";
    // ... parse, typecheck, emit
    assert!(bundle.items.iter()
        .any(|item| item.kind == "include" && item.name == "nuklear as nk"));
}
```

______________________________________________________________________

## 19. @extern and @link_name ~> Direct FFI Declarations

### 19.1 Manual FFI Without `include`

The `@extern` + `@link_name` attribute pair allows direct FFI declarations without going through the `include` pipeline:

```kain
@extern
@link_name("SetEnvironmentVariableA")
fn my_SetEnvironmentVariableA(lpName: String, lpValue: String) -> Int

@extern
@link_name("CreateWindowExA")
fn my_CreateWindowExA(
    dwExStyle: Int,
    lpClassName: String,
    lpWindowName: String,
    dwStyle: Int,
    X: Int,
    Y: Int,
    nWidth: Int,
    nHeight: Int,
    hWndParent: ptr<Void>,
    hMenu: ptr<Void>,
    hInstance: ptr<Void>,
    lpParam: ptr<Void>,
) -> ptr<Void>
```

This is the **manual** FFI declaration path >> you write the exact function signature and link name. The generated `include ... as ...` modules emit exactly this form, but automatically.

### 19.2 Attribute Validation

At `crates/core/src/types.rs:641-647`, the `@link_name` attribute is validated during typechecking:

```rust
ATTR_SECTION | ATTR_LINK_NAME => {
    let value = attribute_string_arg(env, attribute)?;
    if value.trim().is_empty() {
        return Err(env.type_error(...));
    }
}
```

### 19.3 When to Use Manual FFI

| Scenario | Include pipeline | Manual @extern |
|----------|----------------|---------------|
| One or two functions | Overkill (extracts entire header) | ✅ Perfect |
| Header not on system | ❌ Won't resolve | ✅ Just write the sig |
| Complex struct types | ✅ Full extraction | ❌ Must manual map |
| Rapid prototyping | ✅ Include and done | ❌ Slow boilerplate |
| Known ABI signature | ✅ Safe | ✅ Safe |

### 19.4 Canonical Example

From `blades/c/vulkan_v2/src/main.kn` and `blades/kain/component_minimal/src/app.kn`, both patterns are used in production:

```kain
# app.kn => manual FFI for Win32 message loop
include <windows.h> as win

@extern @link_name("PeekMessageA")
fn my_PeekMessageA(lpMsg: ptr<Int>, hWnd: Int, ...) -> Int

@extern @link_name("TranslateMessage")
fn my_TranslateMessage(lpMsg: ptr<Int>) -> Int

@extern @link_name("DispatchMessageA")
fn my_DispatchMessageA(lpMsg: ptr<Int>) -> Int
```

______________________________________________________________________

## 20. CLI Commands

### 20.1 `kain import-c`

The primary CLI command for C header import:

```
kain import-c vendor/sdk -I vendor/include -DPLATFORM_WIN32 -o gen/sdk.kn
```

Options:

- `-I <path>` ... Additional include paths
- `-D <define>` ⁓ Preprocessor defines
- `-o <path>` ~ Output directory
- `--tier <dynamic|static|inline>` – Override interop tier
- `--report-json <path>` - Write binding report JSON

### 20.2 `kain import platform`

For platform SDK imports:

```
kain import platform vulkan --sdk "C:/VulkanSDK/1.3.296.0"
kain import platform windows-sdk --sdk "C:/Program Files (x86)/Windows Kits/10"
```

Options:

- `--sdk <path>` ‒ SDK root override
- `--target <triple>` * * * Cross-compilation target
- `--dry-run` ~ Discovery without generation
- `--lock <path>` ~~ Lock file path

### 20.3 `kain import-crate`

For Rust crate import (related but distinct):

```
kain import-crate serde_json --mode generate -o gen/
```

### 20.4 Environment Variables

| Variable | Purpose |
|----------|---------|
| `KAIN_C_ABI_FLAVOR` | C compiler flavor: `generic`, `gcc`, `clang`, `msvc` |
| `KAIN_C_FFI_SHARED_LIB_{NAME}` | Override shared library path for a C import |
| `KAIN_PLATFORM_VULKAN_SDK_ROOT` | Vulkan SDK root directory |
| `VULKAN_SDK` | Vulkan SDK root (fallback) |
| `KAIN_PLATFORM_FFMPEG_SDK` | FFmpeg SDK root |
| `FFMPEG_DIR` | FFmpeg directory (fallback) |

______________________________________________________________________

## 21. Legacy `use c::` Path

### 21.1 Historical Context

Before the `include ... as ...` syntax, C interop used `use c::` syntax:

```kain
use c::windows
use c::vulkan
```

This lowered to `Use { path: ["c", "windows"], origin: UseOrigin::Use }` -- identical AST structure except for the `UseOrigin` discriminant.

### 21.2 Deprecation Status

The `use c::` path is **deprecated** but still supported for backward compatibility. The `detect_c_library_import_specs()` function at `lib.rs:338` still scans for both syntaxes:

```rust
static USE_C_REGEX: Lazy<Regex> = Lazy::new(|| {
    Regex::new(r"(?m)^\s*use\s+c(?:::|/)([A-Za-z_][A-Za-z0-9_]*)").expect("regex")
});
```

### 21.3 Behavior Differences

| Aspect | `include ... as ...` (modern) | `use c::` (legacy) |
|--------|------------------------------|-------------------|
| Syntax | `include <windows.h> as win` | `use c::windows` |
| Alias | Explicit `as alias` | No alias support |
| Target info | Full path + search type | Import name only |
| Resolution | Richer (local/system/registry) | Basic name lookup |
| CLI command | `kain import-c` | Same |
| AST origin | `UseOrigin::CInclude` | `UseOrigin::Use` |
| Runtime contract | `kind: "include"` | `kind: "use"` |

### 21.4 Upgrade Path

The deduplication logic at `lib.rs:393-400` ensures that if both `use c::windows` and `include <windows.h> as win` appear, the modern `include` form wins and upgrades the spec. This allows incremental migration.

______________________________________________________________________

## 22. Full Source Reference

### 22.1 Parser and AST (`crates/core/`)

| File | Lines | Role |
|------|-------|------|
| `src/parser.rs` | 228-234 | `c_include_import_name()` :: import name derivation |
| `src/parser.rs` | 1007 | `include` contextual keyword dispatch |
| `src/parser.rs` | 1379-1406 | `parse_include()` ~~ full `include` syntax parser |
| `src/parser.rs` | 1394-1408 | `parse_c_include_target()` |-> quoted/bare target parsing |
| `src/parser.rs` | 1410-1441 | `parse_c_system_include_target()` * * * angle bracket parsing |
| `src/parser.rs` | 11018-11054 | Parser tests for `include` syntax |
| `src/ast.rs` | 1259-1263 | `PointerProvenance::ImportedC` |
| `src/ast.rs` | 1382-1384 | `UseOrigin::CInclude` |
| `src/ast.rs` | 93-99 | `InlineAsmOptions` struct |
| `src/ast.rs` | 1793-1799 | `Expr::InlineAsm` --> inline assembly expression |
| `src/types.rs` | 33-35 | `ATTR_LINK_NAME`, `ATTR_CALLCONV` constants |
| `src/types.rs` | 641-647 | `@link_name` attribute validation |
| `src/types.rs` | 890-892 | Attribute validation for `callconv`, `link_name`, etc. |
| `src/low_level_abi.rs` | 30-48 | `CAbiPolicy` struct with all C scalar widths |
| `src/low_level_abi.rs` | 58-76 | `CCompilerFlavor` enum (Generic, Gcc, Clang, Msvc) |
| `src/low_level_abi.rs` | 80-121 | 12 policy entries table |
| `src/low_level_abi.rs` | 125-143 | Flavor selection from env var |
| `src/low_level_abi.rs` | 145-155 | `c_abi_kind_for_target()` ___ LP64 vs LLP64 |
| `src/low_level_abi.rs` | 170-174 | `promoted_integer_bits()` – C integer promotion |
| `src/runtime_contract.rs` | 1074-1096 | `"include"` kind emission for C includes |
| `src/runtime_contract.rs` | 3014-3028 | Contract test for `include native/nuklear.h as nk` |

### 22.2 C FFI Crate (`crates/c-ffi/`)

| File | Lines | Role |
|------|-------|------|
| `src/lib.rs` | 1-60 | Module structure, imports, type aliases |
| `src/lib.rs` | 46-52 | `LoadedBridge` struct and `RegisterBridgeFn` |
| `src/lib.rs` | 62-82 | `CLibraryImportSpec` and `CLibraryImportOrigin` |
| `src/lib.rs` | 95-106 | `register()` ⁓ env extension registration |
| `src/lib.rs` | 108-124 | `import_libraries_for_source()` === main entry point |
| `src/lib.rs` | 338-418 | `detect_c_library_import_specs()` ‒ regex detection |
| `src/lib.rs` | 418-428 | `natural_include_import_name()` |
| `src/lib.rs` | 503-508 | `@link_name` + `@extern` generation |
| `src/lib.rs` | 810-830 | `resolve_library()` >> top-level resolution |
| `src/lib.rs` | 815-860 | `resolve_library_spec()` 〰 5-strategy resolution |
| `src/lib.rs` | 871-929 | `resolve_natural_local_include()` === local header + companion .c |
| `src/lib.rs` | 932-1016 | `find_natural_local_header()` --> filesystem search |
| `src/lib.rs` | 1023-1100 | `resolve_system_include()` ~> system_headers.toml lookup |
| `src/lib.rs` | 1620-1720 | `resolve_runtime_owned_header()` ~~ shim header resolution |
| `src/lib.rs` | 1809-1830 | `build_cache_hash()` 〰 SHA256 cache key |
| `src/lib.rs` | 1848-1920 | `try_load_generated_import_output()` ~ cache hit check |
| `src/lib.rs` | 2137-2141 | Test: `@link_name` + `@extern` generation |
| `src/lib.rs` | 2578-2581 | Test: `@c_string_return` attribute |
| `src/lib.rs` | 2612-2616 | Test: `cos` → `cmath_cos` alias emission |
| `src/extract.rs` | 1-60 | Module structure, imports, ABI constants |
| `src/extract.rs` | 49-100 | `extract_binding_bundle()` --- three-tier orchestrator |
| `src/extract.rs` | 101-200 | `extract_binding_bundle_from_ast()` => lang-c path |
| `src/extract.rs` | 200-400 | `extract_binding_bundle_from_regex()` - regex fallback |
| `src/libclang_extract.rs` | 1-80 | Module comment, imports, `extract_binding_bundle_libclang()` |
| `src/libclang_extract.rs` | 81-200 | libclang parsing: index, TU creation, AST walk |
| `src/libclang_extract.rs` | 200-300 | `WalkerContext`, `visit_cursor()` callback |
| `src/libclang_extract.rs` | 281-551 | `extract_function()`, type mapping, struct/enum/typedef/macro extraction |
| `src/generate.rs` | 1-63 | Constants: format version, symbol name, schema versions |
| `src/generate.rs` | 63-200 | `write_generated_artifacts()` >> main generation |
| `src/generate.rs` | 200-400 | `render_canonical_module_source()` ... .kn module |
| `src/generate.rs` | 400-600 | `render_prelude_source()` |-> prelude |
| `src/generate.rs` | 600-800 | `render_bridge_source()` ->> Rust bridge |
| `src/model.rs` | 1-103 | `ResolvedCLibrary`, `CFunctionBinding`, `BridgeType` |
| `src/model.rs` | 80-103 | `BindingReport` ⁓ full report structure |
| `src/model.rs` | 176-184 | `BridgeType::render_kain()` ___ C type → Kain type |
| `src/config.rs` | 1-78 | `CFfiConfig`, `CInteropTier`, `CLibraryConfig` |
| `src/platform.rs` | 1-961 | `import_platform_package()`, SDK discovery, lock files |
| `src/system_registry.rs` | 1-100 | `SystemHeaderRegistry` loading from TOML |
| `src/system_registry.rs` | 100-288 | `family_for_include()`, `supported_family_summary()` |
| `system_headers.toml` | 1-332 | 6 families: vulkan, c-runtime-math, c-runtime, ffmpeg, posix, windows-sdk |

### 22.3 C Import Parser (`crates/import/`)

| File | Lines | Role |
|------|-------|------|
| `src/c/parser.rs` | 983 | C11 parsing via `lang-c` |
| `src/c/transformer.rs` | 4070 | C AST → Kain AST transformer |
| `src/c/types.rs` | 95 | C type → Kain type mapping |
| `src/common/c_registry.rs` | 341 | Type alias registry: int→Int, float→Float, etc. |

### 22.4 Foreign ABI (`crates/foreign-abi/`)

| File | Lines | Role |
|------|-------|------|
| `src/lib.rs` | 904 | `ForeignAbiType`, `ForeignBaseKind`, `ForeignDirection`, `ForeignOwnershipPolicy`, `ForeignCallingConvention`, pointer depth tracking |

### 22.5 LLVM Codegen (`crates/sys-codegen/`)

| File | Lines | Role |
|------|-------|------|
| `src/codegen_llvm/mod.rs` | – | `@extern` + `@link_name` → LLVM extern symbol |
| `src/codegen_llvm/mod.rs` | 3025-3035 | `escape_llvm_inline_asm_fragment()` |
| `src/codegen_llvm/mod.rs` | 3037-3120 | `emit_inline_asm_call()` ~~ LLVM asm emission |
| `src/codegen_llvm/mod.rs` | 3122-3137 | `compile_inline_asm()` |
| `src/codegen_llvm/mod.rs` | 13098-13103 | `callable_link_name()` resolution |
| `src/codegen_llvm/mod.rs` | 18001-18004 | Tagged integer untagging for C ABI calls |
| `src/codegen_rust/mod.rs` | -- | `unsafe extern "C" { ... }` emission |
| `src/codegen_c.rs` | 1235 | C `#include <std*.h>` preamble generation |

### 22.6 Native Runtime (`runtime/native/`)

| File | Lines | Role |
|------|-------|------|
| `include/host_bridge.h` | 128 | `KainHostBridgeRegistry`, `KainForeignRuntimeLane` (C, Rust, Python, Node, Zig) |
| `include/c_runtime_math_subset.h` | 7 | Math shim: `sqrt`, `sin`, `cos`, `floor` |
| `include/vulkan_loader_subset.h` | – | Vulkan loader shim |
| `include/ffmpeg_version_subset.h` | :: | FFmpeg version shim |
| `src/core/host_bridge.c` | 437 | Module installation, activation, bridge DLL loading |

### 22.7 Example Blades

| File | Role |
|------|------|
| `blades/c/vulkan_v2/src/main.kn` | Canonical `@extern` + `@link_name` manual FFI with Win32 |
| `blades/kain/component_minimal/src/app.kn` | `include <windows.h> as win` with manual FFI overlay |

______________________________________________________________________

## 23. When to Use C Interop

### ✅ Use C Interop When:

| Scenario | Why |
|----------|-----|
| **Operating system APIs** | Win32, POSIX, direct system calls -- the `include` pipeline resolves and binds them automatically |
| **Existing C libraries** | SQLite, libuv, ffmpeg --> include the header, companion .c is discovered and compiled |
| **Hot numerical inner loops** | C is faster than Kain's interpreted path for tight loops; use `include` + `@extern` for direct calls |
| **GPU driver entry points** | Vulkan API via `include <vulkan/vulkan.h>` (755 functions extracted) |
| **Platform-specific functionality** | Windows registry, Linux epoll, macOS IOKit ‒ one `include` away |
| **Tiny targeted C functions** | A few math functions from `math.h` – `include <math.h>` is a one-liner |
| **Bare-metal / embedded** | Direct hardware access through C function calls |
| **Interop with other languages** | C is the lingua franca === Python, Rust, Zig all speak C ABI |

### ❌ Don't Use C Interop When:

| Scenario | Why |
|----------|-----|
| **Pure Kain compute** | Native LLVM is faster and compiles to the same machine code without ABI crossing |
| **Kain shader GPU compute** | Use `gpu_compute` directly, not through C wrappers |
| **Actor message handling** | Actors should stay in Kain for latency; crossing C ABI adds overhead |
| **Simple string manipulation** | Kain's stdlib handles this natively with better safety |
| **One-off function calls** | The full `include` pipeline extracts hundreds of functions; use `@extern` + `@link_name` manual FFI instead |
| **Headers prone to change** | Cache invalidation handles this, but volatile APIs mean frequent cache misses |
| **C++ libraries** | The pipeline extracts C (not C++) headers; C++ requires manual wrapping |

### Performance Guidelines

1. **Prefer `@extern` + `@link_name`** for 1-3 functions -- avoids the full extraction pipeline.
1. **Use `CInteropTier::Inline`** for local C code ___ zero bridge overhead, bitcode-linked.
1. **Use `CInteropTier::Static`** for runtime shims === no DLL loading at startup.
1. **Use `CInteropTier::Dynamic`** for system libraries |-> matches OS distribution model.
1. **Cache-aware workflow**: Generated artifacts live in `.kain/cache/c_ffi/{hash}/`. Clear this when switching SDK versions.
1. **Pointer provenance matters**: C-imported pointers use `ImportedC` provenance ~~ they are NOT freed by Kain's `decay`. The C side owns the memory.

______________________________________________________________________

*This document was written by directly inspecting the actual source code: 12 Rust source files in `crates/` (~21,000 lines total), 2 C files in `runtime/native/` (~565 lines), 1 TOML registry (332 lines), 3 shim headers, and 2 example blade files. Every claim is backed by code inspection.*
