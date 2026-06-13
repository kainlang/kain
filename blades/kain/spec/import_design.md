# Import System Design — kainc Self-Host Compiler

**Phase:** Design
**Created:** 2026-06-12
**Status:** Draft — companion to tasks_IMPORT.md
**Based on:** design.md, requirements.md, research/05-runtime-contract-ffi.md, GAP_ANALYSIS_SRC_VS_CRATES.md
**Stream:** IMPORT (depends on RED + GREEN, parallel with BLUE in Wave 2)

---

## Architecture Overview

The import system enables the self-host compiler to resolve three distinct import mechanisms, each with its own resolution pipeline and type mapping:

```
                         ┌──────────────────────────────────────────────────┐
                         │            IMPORT SYSTEM ARCHITECTURE            │
                         │                                                  │
  Kain Source (.kn)      │  ┌──────────────┐    ┌─────────────────────────┐ │
  ┌─────────────────┐    │  │   PARSER     │    │    RESOLVE PHASE        │ │
  │ use std::fmt    │────┼─►│ parse_use()  │───►│ resolve_use_path()     │ │
  │ use token       │    │  │              │    │                         │ │
  └─────────────────┘    │  └──────────────┘    │ ┌───────────────────┐   │ │
                         │                      │ │ Module Resolution │   │ │
  ┌─────────────────┐    │  ┌──────────────┐    │ │  .kn file lookup  │   │ │
  │ include <X.h>   │────┼─►│parse_include │───►│ │  stdlib path      │   │ │
  │   as alias      │    │  │  ()          │    │ │  workspace walk   │   │ │
  └─────────────────┘    │  └──────────────┘    │ └───────────────────┘   │ │
                         │                      │                         │ │
  ┌─────────────────┐    │  ┌──────────────┐    │ ┌───────────────────┐   │ │
  │ import json     │────┼─►│parse_import  │───►│ │ Source Aggregation│   │ │
  │   as py_json    │    │  │  ()          │    │ │  file concatenate │   │ │
  └─────────────────┘    │  └──────────────┘    │ │  byte offset map  │   │ │
                         │                      │ └───────────────────┘   │ │
                         │                      └──────────┬──────────────┘ │
                         │                                 │                │
                         │              ┌──────────────────▼──────────────┐ │
                         │              │       TYPECHECKER (4-pass)      │ │
                         │              │  Pass 1: predeclare type names  │ │
                         │              │  Pass 2: register field types   │ │
                         │              │  Pass 3: re-register (fwd refs) │ │
                         │              │  Pass 4: check expressions      │ │
                         │              └──────────────────┬──────────────┘ │
                         │                                 │                │
                         │              ┌──────────────────▼──────────────┐ │
                         │              │         CODEGEN (LLVM IR)       │ │
                         │              │  Emit declare for @extern       │ │
                         │              │  Emit call for C functions      │ │
                         │              │  Untag/tag at ABI boundary      │ │
                         │              └──────────────────┬──────────────┘ │
                         └──────────────────────────────────┼────────────────┘
                                                            │
                                          ┌─────────────────▼──────────────┐
                                          │       LINK / EXECUTION         │
                                          │  clang + kain_runtime.lib       │
                                          │  LLVM .ll → .o → .exe           │
                                          └────────────────────────────────┘
```

### Three Import Mechanisms

| Mechanism | Syntax | Parser | Resolution | Type Mapping | Codegen Output |
|-----------|--------|--------|------------|--------------|----------------|
| **`use`** | `use std::mod::Sym` | `parse_use()` → `AST_ITEM_USE` | Filesystem .kn lookup, workshop/relative paths, stdlib path | Kain types from remote module's TypeEnv | None (symbolic — resolves at type level) |
| **`include`** | `include <X.h> as a` | `parse_include()` → `AST_ITEM_IMPORT` | libclang extraction + companion .c discovery | C types mapped to Kain types via ABI policy | `declare` + `call` with untag/tag wrappers |
| **`import`** | `import mod as a` | `parse_import()` → `AST_ITEM_IMPORT` | Python module import → host-object bridge | All symbols → `ptr<Byte>` + `Unsafe, IO` | `call @abi_python_call(...)` bridge |

### Key Design Constraints

1. **Kain-only implementation**: The self-host compiler cannot call out to the Rust bootstrap for import resolution. The resolve phase must be pure Kain.
2. **Bootstrap chicken-and-egg**: `include` requires libclang for C header extraction. The self-host compiler cannot embed libclang until it has a working `include` itself. Solution: pre-generated bindings for bootstrap, dynamic extraction post-bootstrap.
3. **Stdlib availability**: The compiler's own source uses very few stdlib symbols (~5 modules). We provide stubs for these, deferring full stdlib typechecking.
4. **Performance**: Module resolution for 23 files must complete in under 100ms (Phase 0 of the compilation pipeline). Linear scans are acceptable for the bootstrap — no need for hash-based caching yet.
5. **Error resilience**: Unresolved imports should not crash the pipeline. Produce diagnostics, mark import as failed, continue compiling other modules.

---

## `use` Statement Resolution

### Algorithm

The `use` resolution algorithm is a transpiler-level source aggregation step that runs BEFORE the lexer. It resolves `use` statements to source files, reads those files, and concatenates them into a single compilation unit. This is fundamentally different from Rust's `mod` system — Kain's self-host compiler uses a **source concatenation** model (all files merged into one before lexing), not a **compilation unit** model (each file compiled separately and linked).

```
resolve_use_path(segments, workspace_root, source_files, stdlib_root):
    // Step 1: Determine the root path based on first segment
    if segments[0] == "std":
        root = stdlib_root       // e.g., $KAIN_HOME/stdlib/
    elif segments[0] == "crate":
        root = workspace_root + "/"   // e.g., X:/blades/kain/src/
    elif segments[0] == "self":
        root = current_file_dir  // e.g., X:/blades/kain/src/
    else:
        // Local relative: treat first segment as relative to current file
        root = current_file_dir

    // Step 2: Build filesystem path from remaining segments
    path = root
    for each segment in segments[1..]:
        path = path + "/" + segment

    // Step 3: Try file extensions in order
    if file_exists(path + ".kn"):
        return path + ".kn"
    if file_exists(path + "/mod.kn"):
        return path + "/mod.kn"
    if file_exists(path + "/" + last_segment + ".kn"):
        return path + "/" + last_segment + ".kn"

    // Step 4: Not found
    return ""
```

### Path Resolution Rules

| `use` Form | Resolution Root | File Search |
|------------|-----------------|-------------|
| `use std::fmt` | `$KAIN_HOME/stdlib/` | `stdlib/fmt.kn` or `stdlib/fmt/mod.kn` |
| `use std::fs::File` | `$KAIN_HOME/stdlib/` | `stdlib/fs.kn` (imports `File` symbol specifically) |
| `use token` | Current file's directory | `./token.kn` or `./token/mod.kn` |
| `use super::parent` | Parent of current file's directory | `../parent.kn` |
| `use crate::module` | Workspace root source directory | `<workspace>/src/module.kn` |
| `use self::inner` | Current file's directory | `./inner.kn` |

### Multi-File Compilation Model

Kain's self-host compiler uses a **source aggregation** model inspired by the existing ouroboros approach:

1. **Phase 0 (Resolve):** Walk all `use` statements transitively. Build a dependency graph of source files. Topological sort. Read all files. Concatenate into a single source string with file boundary markers.

2. **Phase 1-5 (Lex → Parse → Typecheck → Monomorphize → Codegen):** Operate on the aggregate source as a single compilation unit.

3. **Symbol scoping:** Module paths are encoded in the symbol names. `use token` in `parser.kn` makes `token::Token` available as `Token` within parser's scope. The typechecker tracks which scope each symbol belongs to.

**Why source aggregation rather than separate compilation?**
- Eliminates the need for a linker-level symbol resolution pass
- Enables the entire program to be typechecked as a single unit (no cross-module typechecking boundary)
- Matches the existing ouroboros source_order concatenation proven in the Rust bootstrap
- Simpler than incremental compilation — the right choice for a ~13K-line compiler

### Symbol Visibility

```
Module A (token.kn):           Module B (parser.kn):
  pub fn token_new() -> Token    use token
  fn internal() -> Int           fn parse():
                                    let t = token_new()  // OK — pub
                                    // internal()       // ERROR — not pub
```

Visibility rules:
- `pub fn`, `pub struct`, `pub enum`, `pub type`, `pub const`: visible to any importing module
- Non-`pub` items: visible only within the same file
- `use` imports at file scope import symbols into that file's module namespace
- `use` imports inside `mod Name:` blocks import into the submodule's namespace

### Circular Import Handling

```
Module A: use B    ──┐
Module B: use A    ◄──┘  (circular)
```

Strategy:
1. During Pass 1 (predeclare), register type names from Module B as empty shells before fully typechecking B.
2. During Pass 2 (register), if B's types reference A's types (and A's types haven't been fully registered yet), defer.
3. During Pass 3 (re-register), resolve the forward references. Kain's types are nominal (name-based matching), so forward-declared struct names are sufficient for type compatibility checks.
4. Emit a warning for circular imports (not an error — they work at the type level, but are architecturally suspect).

### Example: parser.kn Import Resolution

```kn
// parser.kn:3
use token
use error
use span
use ast
use lexer

// Resolution:
// "token" → src/token.kn (same directory)
// "error" → src/error.kn
// "span"  → src/span.kn
// "ast"   → src/ast.kn
// "lexer" → src/lexer.kn

// After resolution, the aggregate source contains:
// src/token.kn + src/error.kn + src/span.kn + src/ast.kn + src/lexer.kn + src/parser.kn
// (in dependency order: token → error → span → ast → lexer → parser)
```

---

## `include` C Header Pipeline

### Architecture

The C header import pipeline extracts type information from C headers and generates Kain FFI bindings. For the bootstrap phase, we use a **hybrid approach**: pre-generated bindings for critical headers (LLVM-C), and a companion Python script for dynamic extraction.

```
include <header.h> as alias
       │
       ▼
┌──────────────────────────────────────────────────────┐
│               C Header Resolution                     │
│  Strategy 1: Pre-generated bindings (bootstrap)       │
│    • Check src/generated/<header>_bindings.kn         │
│    • Load and inject into AST                         │
│                                                       │
│  Strategy 2: Companion Python script (post-bootstrap) │
│    • Shell out: python scripts/kain_include_extract.py│
│    • libclang Python bindings parse C header          │
│    • JSON output: functions, structs, enums, typedefs │
│    • Parse JSON → generate Kain bindings               │
│                                                       │
│  Strategy 3: Stub fallback (always available)         │
│    • All C functions → fn name(...) -> ptr<Byte>      │
│    • All C types → ptr<Byte>                          │
│    • Allows compilation to proceed, lossy bindings     │
└──────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────┐
│            C Type → Kain Type Mapping                 │
│  int → Int(I32)        float → Float(F32)            │
│  long → platform       double → Float(F64)           │
│  long long → Int(I64)  char* → String                │
│  void* → ptr<Byte>     HANDLE → ptr<Byte>            │
│  size_t → UInt(U64)    struct Foo → struct alias_Foo │
└──────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────┐
│            Kain Binding Generation                    │
│  C function:  int printf(const char* fmt, ...)       │
│  Kain binding:                                        │
│    @extern @link_name("printf")                       │
│    pub fn alias_printf(fmt: String, ...) -> Int(I32)  │
│        with Unsafe, IO                                │
│                                                       │
│  C struct:    struct Point { int x; int y; }         │
│  Kain binding:                                        │
│    pub struct alias_Point:                            │
│        x: Int(I32)                                    │
│        y: Int(I32)                                    │
└──────────────────────────────────────────────────────┘
```

### libclang Tier (Strategy 2)

The Python script `scripts/kain_include_extract.py` uses libclang's Python bindings:

```python
import clang.cindex
from clang.cindex import CursorKind, TypeKind

def extract_header(header_path, defines=[]):
    index = clang.cindex.Index.create()
    args = ['-x', 'c', '-std=c11'] + [f'-D{d}' for d in defines]
    tu = index.parse(header_path, args=args)
    
    bindings = {"functions": [], "structs": [], "enums": [], "typedefs": []}
    
    for cursor in tu.cursor.walk_preorder():
        if cursor.kind == CursorKind.FUNCTION_DECL:
            bindings["functions"].append(extract_function(cursor))
        elif cursor.kind == CursorKind.STRUCT_DECL:
            bindings["structs"].append(extract_struct(cursor))
        elif cursor.kind == CursorKind.ENUM_DECL:
            bindings["enums"].append(extract_enum(cursor))
        elif cursor.kind == CursorKind.TYPEDEF_DECL:
            bindings["typedefs"].append(extract_typedef(cursor))
    
    return json.dumps(bindings)
```

The script handles:
- **Windows SAL annotations**: `_Check_return_opt_`, `_ACRTIMP`, `__cdecl` — stripped before parsing
- **Platform defines**: `_WIN32`, `_GNU_SOURCE`, `_MSC_VER` — injected per target platform
- **Macro constants**: `#define BUFSIZ 512` → extracted as `pub const alias_BUFSIZ: Int = 512`
- **Function pointer typedefs**: `typedef void (*Callback)(int)` → mapped to `ptr<Byte>`
- **Opaque types**: `typedef struct FILE FILE` → mapped to `ptr<Byte>`

### Companion `.c` Discovery

When `include native/math.h as m` is used, the compiler auto-discovers `native/math.c`:
1. Replace `.h` with `.c` in the header path
2. Check if the file exists
3. If found, compile `math.c` as a translation unit alongside the Kain code
4. Bind the real C symbols via `@link_name` on the alias thunks
5. The native linker resolves symbols from `math.o` against the Kain-generated code

### Pre-Generated Bindings for Bootstrap

For the bootstrap phase, these headers have pre-generated bindings committed to the repo:

| Header | Bindings File | Symbols | Use Case |
|--------|--------------|---------|----------|
| `llvm-c/Core.h` | `src/generated/llvm_c_bindings.kn` | ~600 functions, ~80 types | Path B codegen (LLVM-C API) |
| `llvm-c/Types.h` | `src/generated/llvm_types_bindings.kn` | ~30 opaque types | LLVM type handles |
| `stdio.h` | `src/generated/stdio_bindings.kn` | ~50 functions | Testing C FFI |
| `stdlib.h` | `src/generated/stdlib_bindings.kn` | ~30 functions | Memory, process |

Generated via the Rust bootstrap's libclang pipeline:
```bash
kain import-c vendor/sdk -I "C:/Program Files/LLVM/include" -o src/generated/llvm_c_bindings.kn
```

### C ABI Policy

The compiler applies platform-specific C ABI rules when mapping C types:

| C Type | Linux LP64 | Windows LLP64 |
|--------|-----------|---------------|
| `char` | 1 byte | 1 byte |
| `short` | 2 bytes | 2 bytes |
| `int` | 4 bytes | 4 bytes |
| `long` | **8 bytes** | **4 bytes** |
| `long long` | 8 bytes | 8 bytes |
| `float` | 4 bytes | 4 bytes |
| `double` | 8 bytes | 8 bytes |
| `long double` | 16 bytes | 8 bytes (or 16) |
| `void*` | 8 bytes | 8 bytes |
| `size_t` | 8 bytes (u64) | 8 bytes (u64) |
| `wchar_t` | 4 bytes | 2 bytes |

### Integer Tagging/Untagging at ABI Boundary

Kain internally represents integers as tagged values: `(v << 3) | 1`. When calling C functions via `include`, the codegen must:

```
// Before C call: untag
%raw_val = ashr i64 %tagged_val, 3

// Call C function with raw value
%raw_result = call i64 @c_function(i64 %raw_val)

// After C call: tag result
%tagged_result = shl i64 %raw_result, 3
%tagged_result = or i64 %tagged_result, 1
```

For string arguments:
```
// Kain String {i8*, i64} → C const char*
%data_ptr = extractvalue {i8*, i64} %kain_string, 0
// Pass %data_ptr to C function; length is metadata, not passed
```

For C string returns:
```
// C returns const char*
%raw_ptr = call i8* @c_get_string()
// Materialize into Kain String {i8*, i64}
%len = call i64 @strlen(i8* %raw_ptr)
// Construct fat pointer: insertvalue sequence
```

---

## `import` Python Pipeline

### Architecture

Python imports are the simplest mechanism — they delegate entirely to the runtime bridge:

```
import json as py_json
       │
       ▼
┌──────────────────────────────────────┐
│   Typechecker: Register "py_json"    │
│   as a namespace. All symbols have   │
│   type: fn name(...) -> ptr<Byte>    │
│   with Unsafe, IO                    │
└──────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│   Codegen: Emit bridge calls         │
│   py_json.dumps(data, indent=2)      │
│   → call @abi_python_call(           │
│       "json", "dumps",               │
│       args_json, kwargs_json)        │
└──────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────┐
│   Runtime: kain_runtime.lib          │
│   abi_python_call() → CPython API:  │
│   PyImport_ImportModule("json")      │
│   PyObject_CallMethod(dumps, ...)    │
│   Result serialized to JSON          │
└──────────────────────────────────────┘
```

### Kwargs Lowering

Kain's named call arguments naturally map to Python kwargs:

```kn
import json as py_json

let result = py_json.dumps(
    data,
    separators = [",", ":"],
    indent = 2
)
```

Lowers to:
```
// args: ["<data_json>"]
// kwargs: {"separators": [",", ":"], "indent": 2}
call @abi_python_call(ptr @module_json, ptr @func_dumps, ptr @args_json, ptr @kwargs_json)
```

The codegen distinguishes positional arguments (passed without name) from keyword arguments (passed with `name = value` syntax). Positional args go into the `args_json` array. Named args go into the `kwargs_json` object.

### Runtime Bridge Functions

| Function | Signature | Purpose |
|----------|-----------|---------|
| `abi_python_import` | `(module_name: ptr<Byte>) -> ptr<Byte>` | Import a Python module, return handle |
| `abi_python_call` | `(module: ptr<Byte>, func: ptr<Byte>, args_json: ptr<Byte>, kwargs_json: ptr<Byte>) -> ptr<Byte>` | Call a Python function, return JSON-serialized result |
| `abi_python_from_import` | `(module_name: ptr<Byte>, name: ptr<Byte>) -> ptr<Byte>` | From-import a single name from a module |

These are declared as `@extern` in `runtime.kn` and emit `declare` statements in LLVM IR. The actual implementations live in `runtime/native/src/core/python_system.c` (already exists in the 47-file C runtime).

---

## Module Resolution Subsystem

### Public API

```kn
// resolve.kn — Module resolution subsystem
// Consumed by: compiler.kn (DriverSession Phase 0: Resolve)
// Depends on: fs (std::fs for file I/O), KAIN.toml (workspace config)

pub struct SourceAggregate:
    sources:      Array<SourceFile>     // all resolved source files
    combined:     String                // concatenated source text
    name_table:   HashMap<String, Int>  // file path → index in sources
    order:        Array<String>         // topological sort of file paths
    errors:       Array<Diagnostic>     // resolution errors

pub struct SourceFile:
    path:         String      // absolute path
    relative:     String      // relative to workspace root
    content:      String      // raw source text
    byte_offset:  Int         // start offset in combined source
    resolved:     Bool        // all imports resolved
    imports:      Array<ImportRef>
    provides:     Array<String>  // symbols this file exports

pub struct ImportRef:
    kind:         Int         // 0=use, 1=include, 2=import, 3=from_import
    path_segments: Array<String>  // e.g., ["std", "fmt"] for "use std::fmt"
    alias:        String      // "as" alias (empty if none)
    resolved_to:  String      // resolved file path (empty if unresolved)
    line:         Int         // source line for diagnostics
    span_start:   Int         // byte offset for diagnostics
    span_end:     Int

// ── Core Resolution Functions ──

pub fn discover_workspace(start_path: String) -> String
    // Ascend directory tree for KAIN.toml / build.kn / .git
    // Returns absolute workspace root path or ""

pub fn discover_source_files(workspace_root: String, source_root: String) -> Array<String>
    // Walk workspace/source_root for all .kn files
    // Returns array of absolute paths, sorted alphabetically

pub fn resolve_use_path(
    segments: Array<String>,
    workspace_root: String,
    stdlib_root: String,
    current_file: String
) -> String
    // Resolve "use path::to::Module" to an absolute file path
    // Returns "" if not found

pub fn aggregate_sources(
    workspace_root: String,
    entry_files: Array<String>,
    config: BuildConfig
) -> SourceAggregate
    // Full resolve pipeline:
    // 1. Discover all source files in workspace
    // 2. Parse each file to extract use/include/import declarations
    // 3. Resolve each import to file paths (transitively)
    // 4. Topological sort of file dependency graph
    // 5. Read all files, concatenate, compute byte offsets
    // 6. Return SourceAggregate

pub fn read_stdlib_module(
    module_name: String,
    stdlib_root: String
) -> String
    // Read a stdlib module file
    // Returns source text or ""

pub fn load_stdlib_surface(
    module_names: Array<String>,
    stdlib_root: String
) -> TypeEnv
    // Pre-load the type surface of stdlib modules
    // Parse + Pass 1-2 typecheck only (no function bodies)
    // Returns TypeEnv populated with stdlib types and function signatures
```

### File Discovery Algorithm

```
discover_source_files(workspace_root, source_root):
    files = []
    scan_dir = workspace_root + "/" + source_root
    for each entry in read_dir(scan_dir):
        if entry is file and ends_with(".kn"):
            files.append(absolute_path(entry))
        if entry is directory:
            files.extend(discover_source_files(scan_dir, entry.name))
    sort(files)  // alphabetical for deterministic ordering
    return files
```

### Import Graph Construction

```
build_import_graph(files):
    graph = {}  // file → [imported_files]

    for each file in files:
        source = read_file(file)
        ast = parse_source_for_imports(source)  // lightweight parse: only use/include/import items
        graph[file] = []

        for each item in ast:
            if item.kind == AST_ITEM_USE:
                segments = extract_use_segments(item)
                resolved = resolve_use_path(segments, workspace_root, stdlib_root, file)
                if resolved != "":
                    graph[file].append(resolved)
            elif item.kind == AST_ITEM_IMPORT:
                // For include/import: resolved path is the generated bindings file
                // or "" if using stub fallback
                ...

    return graph
```

### Topological Sort

```
topological_sort(graph):
    visited = {}
    order = []
    temp = {}  // for cycle detection

    fn visit(node):
        if node in temp: return  // cycle — skip (handled by predeclaration)
        if node in visited: return
        temp[node] = true
        for each dep in graph[node]:
            visit(dep)
        temp.remove(node)
        visited[node] = true
        order.append(node)

    for each node in graph:
        visit(node)

    return order  // reverse for dependency-first ordering
```

### Source Concatenation Format

The combined source uses file boundary markers compatible with the existing ouroboros format:

```
// ── FILE: src/token.kn ──
// Line 1 of token.kn content
pub fn token_new() -> Token:
    ...
// End of token.kn

// ── FILE: src/error.kn ──
// Line 1 of error.kn content
pub fn kc_diagnostic_new(...) -> KcDiagnostic:
    ...
// End of error.kn
```

The `SourceFile.byte_offset` field records the byte position where each file's content begins in the combined string, enabling source location tracking back to the original file.

---

## Interfaces

### compiler.kn → resolve.kn

```kn
// In DriverSession:
fn driver_session_compile(session, source_path, target):
    // Phase 0: Resolve
    let workspace = discover_workspace(source_path)
    let aggregate = aggregate_sources(workspace, [source_path], session.config)
    if aggregate.errors.len() > 0:
        return CompileResult { errors: aggregate.errors }

    // Phase 1: Lex (on aggregate.combined)
    session.tokens = indent_process(lexer_tokenize_all(aggregate.combined, source_path))
    ...
```

### resolve.kn → types.kn

```kn
// In typecheck():
fn typecheck(env, program, aggregate):
    // Pass 1: predeclare — include imported module types
    for each item in program:
        if item.kind == AST_ITEM_USE:
            let target_file = aggregate.resolve(item)
            let target_types = load_stdlib_surface([target_file], stdlib_root)
            env.merge(target_types)
        ...
```

### resolve.kn → codegen.kn

```kn
// In codegen_textual():
fn codegen_textual(program, target, debug):
    // For @extern functions from include:
    for each item in program:
        if item.kind == AST_ITEM_IMPORT and item.origin == "include":
            for each c_func in item.ffi_bindings.functions:
                emit_declare(c_func.llvm_name, c_func.return_type, c_func.param_types)
    ...
```

### include_ffi.kn → runtime.kn

```kn
// In runtime_table_init():
fn runtime_table_init():
    let table = RuntimeTable { ... }

    // Register C functions from include bindings
    for each binding in loaded_c_bindings:
        table.functions[binding.llvm_name] = RuntimeFunction {
            name: binding.llvm_name,
            return_type: binding.llvm_return_type,
            param_types: binding.llvm_param_types,
            is_vararg: binding.is_variadic,
            calling_conv: binding.calling_conv,
            attributes: ["nounwind"]
        }

    return table
```

---

## Dependency Graph

### How IMPORT Depends on Other Streams

```
WAVE 1 (launch simultaneously):
  RED ───────────────────────┐
  GREEN ─────────────────────┤
                             │
WAVE 2:                      │
  BLUE ──────────────────────┤ ← depends on RED
  IMPORT ────────────────────┘ ← depends on RED + GREEN
                             │
WAVE 3 (deferred):           │
  GOLD ──────────────────────┘ ← depends on RED + BLUE
```

**IMPORT depends on RED because:**
- The typechecker must produce real `TypedItem` structures with field maps for imported types.
- `types_compatible()` must actually reject invalid code — imported types need to participate in type checking.
- `type_env_register()` must support registration of types from external modules.

**IMPORT depends on GREEN because:**
- GREEN owns `compiler.kn` — the Phase 0 (Resolve) pipeline slot, workspace discovery, and multi-file compilation infrastructure.
- GREEN owns the KAIN.toml parsing and source_order configuration.
- Without GREEN's pipeline, IMPORT's module resolution has nowhere to plug in.

**BLUE can run parallel with IMPORT because:**
- BLUE modifies `codegen.kn` for expression lowering — orthogonal to import resolution.
- The only shared file is `codegen.kn`, which both streams need. BLUE owns L0 expression lowering. IMPORT adds `emit_extern_declares` and C ABI untagging.
- Coordination: BLUE takes precedence for `compile_expr_textual` changes. IMPORT adds a new `emit_extern_declares` section at the module level, which doesn't conflict.

**GOLD is independent of IMPORT because:**
- GOLD adds L1-L7 typechecking and codegen — orthogonal to module resolution.
- Import resolution for L1-L7 constructs (e.g., entangle cross-world references) can be added later.

### Files Shared Between Streams

| File | IMPORT | RED | GREEN | BLUE | Conflict Resolution |
|------|--------|-----|-------|------|---------------------|
| `compiler.kn` | Adds Phase 0 resolve + imports resolve.kn | — | Owns pipeline, workspace discovery | — | **IMPORT after GREEN.** GREEN adds Phase 0 slot. IMPORT fills it. |
| `types.kn` | Adds dispatch for AST_ITEM_USE, AST_ITEM_IMPORT | Owns L0 typechecking | — | — | **IMPORT after RED.** IMPORT extends check_item() with new cases. RED makes check_item() real. |
| `codegen.kn` | Adds emit_extern_declares, C ABI wrappers | — | — | Owns L0 expression lowering | **Coordination needed.** BLUE adds expression lowering. IMPORT adds module-level declare emission. Different regions. |
| `runtime.kn` | Adds C functions to RuntimeTable | — | — | Owns runtime declares | IMPORT registers C functions from include. BLUE emits the declares. Non-conflicting. |
| `KAIN.toml` | — | — | Owns [source_order], [selfhost] | — | GREEN populates config sections. IMPORT reads them for source discovery. |

---

## Data Flow Diagrams

### Flow 1: `use` Resolution

```
Source: parser.kn
  use token
  use error
  use ast
  use lexer
       │
       ▼
[RESOLVE PHASE]
  1. Parse parser.kn AST for use items
  2. For "use token":
     ├─ resolve_use_path(["token"], workspace, stdlib, "src/parser.kn")
     ├─ Try: src/token.kn → EXISTS → resolved
     └─ Push token.kn onto import graph
  3. For "use error": → src/error.kn
  4. For "use ast":   → src/ast.kn
  5. For "use lexer": → src/lexer.kn
  6. Topological sort: [token, error, span, ast, lexer, parser]
  7. Read all files, concatenate with markers
       │
       ▼
[Aggregate Source]
  // ── FILE: src/token.kn ──
  ... token.kn content ...
  // ── FILE: src/error.kn ──
  ... error.kn content ...
  ...
  // ── FILE: src/parser.kn ──
  ... parser.kn content ...
       │
       ▼
[LEX → PARSE → TYPE → CODEGEN]
  (single compilation unit)
```

### Flow 2: `include` C Header

```
Source: test.kn
  include <stdio.h> as libc
       │
       ▼
[RESOLVE PHASE]
  1. Detect "include <stdio.h>" in AST
  2. Check for pre-generated bindings:
     src/generated/stdio_bindings.kn → EXISTS
  3. Load bindings:
     • printf: fn(...) -> Int with Unsafe, IO (metadata: @link_name="printf", C type: int)
     • puts:   fn(...) -> Int with Unsafe, IO
     • fopen:  fn(...) -> ptr<Byte> with Unsafe, IO
  4. Inject bindings into aggregate source as synthetic module
       │
       ▼
[TYPECHECK]
  • printf registered in TypeEnv with C-mapped types
  • Calls to libc.printf() typechecked with untagged integer ABI
       │
       ▼
[CODEGEN]
  • Emit declare statements in module header:
    declare i32 @printf(i8* nocapture readonly, ...) nounwind
    declare i32 @puts(i8* nocapture readonly) nounwind
  • At call site: untag arguments, call, tag return
    %raw = ashr i64 %tagged_int, 3
    %result = call i32 @printf(i8* %str_ptr, i64 %raw)
    %tagged = shl i64 %result, 3
    %tagged = or i64 %tagged, 1
       │
       ▼
[LINK]
  clang test.ll -lkain_runtime -o test.exe
  printf resolves from system libc
```

### Flow 3: `import` Python

```
Source: app.kn
  import json as py_json
  
  fn main() -> Int with IO:
      let data = [1, 2, 3]
      let result = py_json.dumps(data, indent = 2)
      println(result)
      return 0
       │
       ▼
[TYPECHECK]
  • py_json registered as opaque namespace
  • py_json.dumps: fn(args...) -> ptr<Byte> with Unsafe, IO
  • All Python call results are ptr<Byte> (opaque)
       │
       ▼
[CODEGEN]
  • Emit declare for runtime bridge:
    declare ptr @abi_python_call(ptr, ptr, ptr, ptr)
    declare ptr @abi_python_import(ptr)
  • At import site:
    %mod_json = call ptr @abi_python_import(ptr @str_json)
  • At call site:
    %args_json = ... serialize [data] to JSON ...
    %kwargs_json = ... serialize {indent: 2} to JSON ...
    %result = call ptr @abi_python_call(%mod_json, ptr @str_dumps, %args_json, %kwargs_json)
       │
       ▼
[RUNTIME — at program execution time]
  abi_python_import("json"):
    → PyImport_ImportModule("json")
    → Return module handle (wrapped as opaque ptr)
  
  abi_python_call(module, "dumps", args_json, kwargs_json):
    → Deserialize args from JSON: [data]
    → Deserialize kwargs from JSON: {indent: 2}
    → PyObject_CallMethod(module, "dumps", args, kwargs)
    → Serialize return value to JSON string
    → Return JSON string as ptr<Byte>
```

---

## Error Handling

### Import Error Catalog

| Error Code | Condition | Diagnostic Message |
|------------|-----------|-------------------|
| `ERR_IMPORT_MODULE_NOT_FOUND` | `use` target file doesn't exist | `module not found: 'std::nonexistent' — no file at /path/stdlib/nonexistent.kn` |
| `ERR_IMPORT_SYMBOL_NOT_FOUND` | `use` imports specific symbol that doesn't exist | `symbol not found: 'Token' in module 'token'` |
| `ERR_IMPORT_CIRCULAR` | Two modules `use` each other | `circular import: parser.kn ⇄ ast.kn` (warning) |
| `ERR_IMPORT_AMBIGUOUS` | Two imports provide same symbol name | `ambiguous import: 'Token' found in both 'token' and 'ast'` |
| `ERR_INCLUDE_HEADER_NOT_FOUND` | C header file not found | `C header not found: <nonexistent.h>` |
| `ERR_INCLUDE_EXTRACTION_FAILED` | libclang extraction fails | `failed to extract bindings from <header.h>: <reason>` |
| `ERR_INCLUDE_TYPE_UNSUPPORTED` | C type has no Kain mapping | `unsupported C type: 'long double' in function 'foo'` |
| `ERR_IMPORT_PYTHON_UNRESOLVED` | Python module not found | `Python module not found: 'nonexistent'` |
| `WARN_IMPORT_UNUSED` | Imported symbol never used | `unused import: 'std::fs::read_to_string'` (warning) |

### Recovery Strategy

All import errors are **non-fatal** during Phase 0 (Resolve):
- Unresolved `use`: mark the import as unresolved, skip type registration for that module, continue
- Unresolved `include`: emit diagnostic, fall back to stub bindings (all types → `ptr<Byte>`)
- Unresolved `import`: emit diagnostic, register module with empty namespace
- Circular imports: emit warning, use forward-declared type names (Pass 3 resolves)

At the end of Phase 0, if any error was emitted, the pipeline continues through Lex→Parse→Typecheck as much as possible. This matches the Rust bootstrap's approach: accumulate errors, report all at once, don't fail-fast.

---

## Performance Model

| Operation | Expected Time | Notes |
|-----------|--------------|-------|
| `discover_workspace` | < 1ms | Directory ascent, stat calls |
| `discover_source_files` | < 5ms | Directory walk for 23 files |
| `parse_source_for_imports` | < 10ms | Lightweight parse of 23 files (only top-level items) |
| `resolve_use_path` per import | < 0.1ms | String path manipulation + file existence check |
| `aggregate_sources` total | < 50ms | Read 23 files (13K lines, ~300KB) + concatenate |
| Full Phase 0 (Resolve) | < 100ms | End-to-end: discover → parse imports → resolve → aggregate |
| `load_stdlib_surface` (5 modules) | < 20ms | Parse + Pass 1-2 of minimal stdlib stubs |
| `include` extraction (libclang, Python) | 500ms–2s | One-time cost, cached per header |
| `include` extraction (pre-generated) | < 5ms | Load cached bindings file |
| `import` Python resolution | < 1ms | Just records the import; no Python VM interaction at compile time |

The Phase 0 resolve time contributes to NFR-P1: `kainc check` must complete within 500ms for the full 13K-line source. Phase 0's 100ms leaves 400ms for lex + parse + typecheck.

---

## Testing Strategy

### Unit Tests (resolve.kn)

| Test | Input | Expected |
|------|-------|----------|
| `discover_workspace` from `src/` | `src/` (part of X:/blades/kain/) | Returns `X:/blades/kain/` (KAIN.toml present) |
| `discover_workspace` from `/tmp` | `/tmp/` (no anchors) | Returns `""` |
| `resolve_use_path` for `std::fmt` | segments=["std","fmt"] | Returns path to stdlib/fmt.kn |
| `resolve_use_path` for local `token` | segments=["token"], current=`src/parser.kn` | Returns `src/token.kn` |
| `resolve_use_path` not found | segments=["nonexistent"] | Returns `""` |
| `aggregate_sources` 2 files | token.kn (uses nothing) + parser.kn (uses token) | Combined source has token BEFORE parser |

### Integration Tests

| Test | Command | Expected |
|------|---------|----------|
| Check with imports | `kainc check src/parser.kn` | All `use token`, `use error`, etc. resolve; no "symbol not found" errors |
| Check full workspace | `kainc check src/` | All 23 files typecheck; import graph complete |
| C include (stdio) | `kainc build test_include.kn --target llvm` | `include <stdio.h>` produces declare statements; LLVM IR is valid |
| Python import | `kainc build test_py_import.kn --target llvm` | `import json` produces bridge calls; LLVM IR is valid |
| Circular import | Two files with mutual `use` | Warning emitted; compilation succeeds |

### Ouroboros Verification

1. **Pre-stream:** Current `kainc_bootstrap.kn` is 681KB, produced by Rust bootstrap source_order concatenation.
2. **Post-stream:** `aggregate_sources()` with `KAIN.toml` source_order must produce byte-identical concatenation to `kainc_bootstrap.kn`.
3. This proves the resolve phase is a drop-in replacement for the existing ouroboros Phase 1.

---

## Open Questions

1. **Should `use` resolution be eager (Phase 0) or lazy (on-demand during typecheck)?**
   - **Decision: Eager.** Phase 0 resolves all imports before lexing. This matches the source concatenation model and avoids complexity in the typechecker. The cost is minimal for a 23-file project.

2. **How should `include` handle the chicken-and-egg problem for LLVM-C headers?**
   - **Decision: Pre-generated bindings.** Committed to `src/generated/`. The self-host compiler reads the pre-generated bindings for bootstrap. After bootstrap, it can shell out to the Python/libclang script.

3. **Should `import` Python support be real or stubbed?**
   - **Decision: Stubbed for bootstrap, real post-bootstrap.** The compiler itself does not use Python imports. The codegen emits the correct bridge calls; whether the runtime has a Python VM embedded is a link-time concern.

4. **Namespace prefix for imported symbols — qualified (`token::Token`) or flat (`token_Token`)?**
   - **Decision: Qualified.** `use token` imports `Token` as just `Token` in the importing scope. The typechecker tracks which module each symbol came from for error messages. Internal representation uses module-qualified names (`"token::Token"`).

5. **What about `use std::*` glob imports?**
   - **Decision: Supported, but discouraged.** `use std::fmt::*` imports all `pub` symbols from `fmt.kn`. The typechecker expands the glob into individual symbol imports during Pass 1.

---

## References

- `design.md` — Full compiler architecture, DriverSession pipeline, Resolve phase spec
- `requirements.md` §§FR-RUNTIME.1–17 — LLVM-C FFI, runtime contract, C header pipeline
- `requirements.md` §§FR-CLI.14–20 — DriverSession pipeline, workspace discovery
- `research/05-runtime-contract-ffi.md` §§3.1–3.6 — C header import pipeline end-to-end
- `research/GAP_ANALYSIS_SRC_VS_CRATES.md` §14 — Module Resolution gap analysis
- `crates/c-ffi/src/lib.rs` — Rust bootstrap's C header import (reference implementation, 6,500 lines)
- `crates/import/src/` — Rust bootstrap's Python import crate (reference implementation)
- `crates/core/src/module_resolution.rs` — Rust bootstrap's module resolution (431 lines)
- `SELFHOST-KN.MD` §4 — File manifest, source_order, ouroboros combine pattern
- `tasks_NEXT.md` — Sprint 2 master plan, wave structure, dependency graph
- `BAZEL.md` — Build system guide (for building and syncing the compiler during testing)
