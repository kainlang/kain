# Stream IMPORT: Full Import System (use / include / import)

**Stream ID:** IMPORT
**Role:** Make the self-host compiler resolve `use` imports across files and stdlib, wire the `include` C header pipeline with libclang-powered type extraction, and implement the `import` Python host-object bridge. This stream makes kainc a multi-file compiler that can import stdlib modules, C headers, and Python libraries — the foundation for compiling real Kain programs beyond single-file demos.
**Effort:** 3-4 weeks
**Depends On:** Stream RED (typechecker must produce real TypedItems with field maps and resolved types for imported items), Stream GREEN (compiler pipeline must handle multi-file source aggregation and workspace discovery)
**Requirements Covered:** FR-RUNTIME.1–17 (LLVM-C FFI, @extern ABI, runtime table, KainType↔CType, C header pipeline), FR-CLI.14–20 (Resolve phase, workspace discovery, multi-file compilation), FR-PARSE.12 (use parsing), FR-PARSE.19–21 (include/import parsing), EC-20 (missing C header), EC-23 (missing workspace anchor), EC-24 (self-reference via imports), ERR-24 (module file not found)
**Design Reference:** design.md §Module Resolution, §Runtime Contract & FFI, §CLI Driver (DriverSession pipeline Resolve phase); research/05-runtime-contract-ffi.md §§3.1–3.6 (C header import pipeline), research/GAP_ANALYSIS_SRC_VS_CRATES.md §14 (Module Resolution gap)

---

## Context

The self-host compiler currently parses `use`, `include`, and `import` statements correctly — `parse_use()` at parser.kn:2511, `parse_include()` at parser.kn:2996, `parse_import()` at parser.kn:3040, and `parse_from_import()` at parser.kn:3070 all produce valid AST nodes (`AST_ITEM_USE` = 6, `AST_ITEM_IMPORT` = 24). However, EVERYTHING downstream of parsing is a stub:

1. **Typechecker** (`types.kn`): `check_item()` only handles functions, structs, enums, and type aliases. It silently skips `use`, `import`, const, trait, impl, and all L1-L7 items. Imported symbols are never registered in `TypeEnv`, so any reference to `std::actor` or `std::fs` causes an "undefined symbol" error (or more precisely, resolves to `rt_i64()` because the typechecker doesn't actually reject anything yet).

2. **Codegen** (`codegen.kn`): `codegen_textual()` only processes `ITEM_FUNCTION`, `ITEM_STRUCT`, `ITEM_ENUM`, `ITEM_CONST`. `use` and `import` items are silently dropped. The codegen has no concept of multi-file output or external symbol linkage.

3. **Compiler Driver** (`compiler.kn`): `discover_workspace()` returns `""` (hardcoded empty string). There is no source aggregation, no `use` resolution, no stdlib path lookup, no `KAIN.toml` reading. The driver compiles exactly one file and stops.

4. **Module Resolution**: The Rust bootstrap has `module_resolution.rs` (431 lines) with `resolve_filesystem_module_file_with_context()` and `resolve_stdlib_module_file()`. Kain has zero lines of module resolution code.

This stream closes all of these gaps. It is organized in three tiers by priority:

- **Tier 1 (IMPORT-1 through IMPORT-4): `use` Resolution** — Highest priority. The ouroboros compiler's own 23 source files must be able to `use` each other and `use std::*` modules. This is the minimum viable import system. ~2 weeks.

- **Tier 2 (IMPORT-5 through IMPORT-7): `include` C Header Pipeline** — Medium priority. Enables `include <llvm-c/Core.h> as llvm` for the LLVM-C API codegen path (Path B) and `include <stdio.h> as libc` for C FFI. Requires libclang integration. ~1 week.

- **Tier 3 (IMPORT-8): `import` Python Bridge** — Lowest priority. Enables `import json as py_json` and `from torch import tensor` for Python interop. Can be stubbed indefinitely since the compiler itself does not use Python imports. ~0.5 week.

---

## Files You Own

### Files to Modify

| File | Region/Function | Change Description |
|------|-----------------|--------------------|
| `X:/blades/kain/src/compiler.kn` | `discover_workspace` (line ~340) | Make real: directory ascent for KAIN.toml/build.kn/.git |
| `X:/blades/kain/src/compiler.kn` | NEW: `resolve_imports` | Core import resolution: walk AST for use/include/import items → locate files → read source → aggregate |
| `X:/blades/kain/src/compiler.kn` | NEW: `resolve_use_path` | Resolve `use std::path::to::Symbol` to source files on disk |
| `X:/blades/kain/src/compiler.kn` | NEW: `resolve_stdlib_module` | Find stdlib modules by name in KAIN_HOME/stdlib/ |
| `X:/blades/kain/src/compiler.kn` | `driver_session_compile` | Add Phase 0 (Resolve) that calls `resolve_imports` before Phase 1 (Lex) |
| `X:/blades/kain/src/compiler.kn` | NEW: `SourceAggregate` struct | Aggregated source buffer with per-file origin tracking |
| `X:/blades/kain/src/types.kn` | `type_env_new` | Register stdlib primitive types and builtin names so imported types resolve |
| `X:/blades/kain/src/types.kn` | `check_item` | Add dispatch for `AST_ITEM_USE` and `AST_ITEM_IMPORT` — register imported symbols in env |
| `X:/blades/kain/src/types.kn` | `type_env_register` | Support registration of types from external modules (by path prefix) |
| `X:/blades/kain/src/codegen.kn` | `codegen_textual` | Add handling for AST_ITEM_USE (emit extern declare for C symbols, skip for Kain symbols already in module) |
| `X:/blades/kain/src/codegen.kn` | NEW: `emit_extern_declares` | Emit `declare` statements for `@extern` functions from `include` C headers |
| `X:/blades/kain/src/KAIN.toml` | `[source_order]` section | This file exists and lists the compilation order. Used by resolve phase for file discovery. |
| `X:/blades/kain/src/KAIN.toml` | NEW: `[stdlib]` section | Stdlib root path, enabled modules list |

### New Files to Create

| File | Purpose | Approximate Lines |
|------|---------|-------------------|
| `X:/blades/kain/src/resolve.kn` | Module resolution subsystem: file discovery, path resolution, source aggregation, import graph construction | ~500 |
| `X:/blades/kain/src/include_ffi.kn` | C header import pipeline: `include <header.h> as alias` → type extraction via libclang → FFI binding generation | ~400 |
| `X:/blades/kain/src/import_py.kn` | Python import bridge: `import module as alias` → Python host-object binding with kwargs lowering | ~200 |

### Files You Must NOT Touch

| File | Reason |
|------|--------|
| `X:/blades/kain/src/parser.kn` | Parser is done — `parse_use`, `parse_include`, `parse_import` already exist and are correct |
| `X:/blades/kain/src/lexer.kn` | Lexer is done |
| `X:/blades/kain/src/ast.kn` | AST constants are defined and correct |
| `X:/blades/kain/src/monomorphize.kn` | Owned by Stream RED |
| `X:/blades/kain/src/orchestrator.kn` | Owned by Stream GREEN |
| `X:/blades/kain/src/llvm_ffi.kn` | Owned by Stream GREEN (conditional LLVM-C includes) |

---

## Implementation Tasks

### Phase 1: Module Resolution Subsystem (~4 days)

**Objective:** Create the module resolution engine — file discovery, path resolution, and source aggregation. This is the foundation that `use`, `include`, and `import` all build on.

**Current state:** `discover_workspace()` returns `""`. No file discovery, no stdlib path resolution, no source aggregation.

**Implementation Steps:**

1. **Create `X:/blades/kain/src/resolve.kn`** — New file for the module resolution subsystem.

2. **Implement `SourceAggregate` struct:**
   ```
   struct SourceAggregate:
       sources:      Array<SourceFile>     // all aggregated source files
       combined:     String                // concatenated source text with file markers
       name_table:   HashMap<String, Int>  // file name → index
       errors:       Array<Diagnostic>     // resolution errors

   struct SourceFile:
       path:         String      // absolute file path
       relative:     String      // path relative to workspace root
       content:      String      // raw source text
       byte_offset:  Int         // where this file starts in combined source
       resolved:     Bool        // true if all imports were resolved
       imports:      Array<ImportRef>  // use/include/import references

   struct ImportRef:
       kind:         Int         // 0=use, 1=include, 2=import
       path:         String      // import path string (e.g. "std::fs", "stdio.h", "json")
       alias:        String      // optional alias
       resolved_to:  String      // resolved file path (or "" if unresolved)
       line:         Int         // source line for error reporting
   ```

3. **Implement `discover_workspace(start_path: String) -> String`:**
   - Ascend directory tree from `start_path`
   - Stop at first directory containing: `KAIN.toml`, `kain.toml`, `build.kn`, `platform.kn`, or `.git`
   - Return absolute path of workspace root (or `""` if none found)
   - Algorithm: while current != filesystem root: if any anchor exists → return current; else current = parent(current)

4. **Implement `discover_source_files(workspace_root: String, source_root: String) -> Array<String>`:**
   - Read `source_root` from KAIN.toml `[source_order]` section (or default to `"src/"`)
   - Walk `workspace_root / source_root` recursively
   - Collect all `.kn` files sorted by filename (matches the source_order convention)
   - Return array of absolute paths

5. **Implement `resolve_use_path(segments: Array<String>, workspace_root: String, source_files: Array<String>) -> String`:**
   - For `use std::module::Symbol`:
     - Look up `module` in stdlib path (`KAIN_HOME/stdlib/` or `$KAIN_HOME/lib/stdlib/`)
     - Map module name to file: `stdlib/module.kn` or `stdlib/module/mod.kn`
   - For `use local::module::Symbol` (relative to current file):
     - Map path segments to filesystem: replace `::` with `/`
     - Look for `module.kn` or `module/mod.kn`
   - For `use crate::module::Symbol` (absolute from workspace root):
     - Start from workspace root source directory
     - Map path segments to filesystem
   - Return absolute file path (or `""` if not found)

6. **Implement `aggregate_sources(workspace_root: String) -> SourceAggregate`:**
   - Discover all source files
   - Build import graph: for each file, scan AST for `use`/`include`/`import` items
   - Walk transitive imports: for each import, resolve the path, add to source list
   - Read all source files
   - Concatenate into `combined` string with comment markers:
     ```
     // ── FILE: src/lexer.kn ──
     <content of lexer.kn>
     // ── FILE: src/parser.kn ──
     <content of parser.kn>
     ```
   - Populate `SourceFile.byte_offset` for each file's position in combined source
   - Return `SourceAggregate`

7. **Implement `read_stdlib_module(module_name: String) -> String`:**
   - Look up `$KAIN_HOME/stdlib/module_name.kn` or `$KAIN_HOME/lib/stdlib/module_name.kn`
   - Read and return source text (or `""` if not found)
   - Cache read modules in a `HashMap<String, String>` for repeated access

8. **Wir into `compiler.kn`:**
   - Update `driver_session_compile`: add Phase 0 (Resolve) before Phase 1 (Lex)
   - Phase 0 calls `aggregate_sources` → produces `SourceAggregate`
   - SourceAggregate.combined feeds into Phase 1 (Lex) instead of the raw input file
   - On error in Phase 0 (unresolved imports), bail with diagnostics

**Acceptance Criteria:**
- [ ] `kainc check src/` discovers all 23 `.kn` files in the workspace
- [ ] `use std::fmt` resolves to `stdlib/fmt.kn` (or `lib/stdlib/fmt.kn`)
- [ ] `use token` in parser.kn resolves to `src/token.kn` (relative import)
- [ ] Source aggregation concatenates all files with file markers
- [ ] Unresolved imports produce diagnostics (file path + line number)
- [ ] Workspace discovery finds `KAIN.toml` in `src/` parent

---

### Phase 2: `use` Statement Resolution — Multi-File Compilation (~5 days)

**Objective:** Wire `use` resolution into the compilation pipeline. After this phase, `kainc check src/lexer.kn` can resolve `use token` and `use error` imports, loading those modules' types into the type environment.

**Current state:** Typechecker skips `AST_ITEM_USE` items completely. No cross-file symbol resolution. All external symbols resolve to `rt_i64()`.

**Implementation Steps:**

1. **Update `type_env_new()` in `types.kn`:**
   - After registering primitive types (I8-I128, U8-U128, Bool, Float, String, etc.), pre-register stdlib module namespaces as empty.
   - Add a `modules: HashMap<String, TypeEnv>` field to `TypeEnv` for per-module type tables.
   - Or simpler approach: use a flat namespace with module-qualified names (e.g., `std::fs::read_to_string` stored as `"std::fs::read_to_string"`).

2. **Add dispatch for `AST_ITEM_USE` in `check_item()`:**
   ```
   if kind == AST_ITEM_USE:
       return check_use_item(env, item_node, aggregate)
   ```
   - `check_use_item()`:
     - Extract path segments from AST node data
     - Look up the target file in `SourceAggregate.sources` by resolved path
     - If the target file is already typechecked (in `TypedProgram`): import its public symbols into current env
     - If not yet typechecked: queue it for typechecking (add to Pass 1 predeclare list)
     - Register imported symbols with their module-qualified names
     - Support `use std::module::Symbol` → single symbol import
     - Support `use std::module` → wildcard import (import all pub symbols)
     - Support `use std::module::*` → glob import

3. **Symbol visibility tracking:**
   - In `TypedItem`, add a `visibility: Int` field (0=private, 1=public)
   - Parser already captures `pub` keyword. Store it in AST data or extract from `vis_val` parameter.
   - In `check_use_item()`, filter imported symbols: only `pub` items are visible across modules.

4. **Handle circular imports:**
   - Maintain a `importing: Array<String>` stack of files currently being imported
   - If a `use` references a file already in the importing stack: circular import detected
   - Strategy: predeclare names from the circular target (Pass 1 only), defer full typecheck to Pass 3 (forward reference re-register)
   - Emit warning (not error) for circular imports

5. **Wire into the 4-pass typecheck pipeline:**
   - **Pass 1 (predeclare):** For each `use` item, add the imported module's type names to the env as empty shells. For stdlib modules, load and predeclare their types.
   - **Pass 2 (register):** For each `use` item, register the imported module's field/variant/method types into the env.
   - **Pass 3 (re-register):** Retry any `use` items that failed Pass 2 (forward references or circular imports).
   - **Pass 4 (check):** Now all imported symbols are available. Check function bodies against the fully-populated env.

6. **Stdlib module preloading:**
   - At startup (in `driver_session_compile`), preload the minimum stdlib surface:
     - `std::fmt` (print, println, format, str, Int→String conversion)
     - `std::fs` (file I/O for reading source files during bootstrap)
     - `std::process` (exit, args for CLI)
     - `std::markscript` (markscript VM embedding for orchestration)
   - For each preloaded module, read the source, typecheck the public surface (not the bodies), and cache the resulting `TypeEnv`.

7. **Update `codegen_textual()` for multi-file compilation:**
   - When compiling a program that includes imports, the codegen receives the aggregated AST from all files.
   - `use` items themselves don't produce LLVM IR — they're resolved at the AST/type level.
   - The codegen already compiles all functions in the `MonomorphizedProgram`. No change needed for the emitting phase.
   - For C ABI `use` items (from `include`), emit `declare` statements (see Phase 3).

**Acceptance Criteria:**
- [ ] `use token` in parser.kn resolves types from token.kn (Token, TokenKind, Span)
- [ ] `use error` resolves diagnostic types from error.kn
- [ ] `use std::fmt` resolves println, format, str from stdlib
- [ ] `pub fn` items are visible across modules; non-pub items are not
- [ ] Circular imports between two modules produce a warning, not a crash
- [ ] Imported symbols are available in the typechecker at Pass 4 (expression checking)
- [ ] `kainc check src/parser.kn` no longer fails with "symbol not found" for `token`, `error`, `span`, `ast`, `lexer` imports

---

### Phase 3: Stdlib Surface Typechecking (~3 days)

**Objective:** Make the most-critical stdlib modules typecheckable so the compiler's own `use std::*` imports resolve. The compiler uses a small set of stdlib symbols — we don't need the entire 67-module stdlib, just the surface needed for self-compilation.

**Current state:** Stdlib modules are not loaded. `use std::fmt` and `use std::process` are no-ops. The compiler works around this by having local stubs and not importing stdlib for critical paths.

**Implementation Steps:**

1. **Identify the minimum stdlib surface needed by the compiler's own source:**
   - Audit every `use std::*` in the 23 source files
   - Current imports (from `COMPILER_IMPL_AUDIT.md`):
     - `main.kn`: `use std::process`
     - `codegen.kn`: `use std::fmt`
     - `orchestrator.kn`: `use std::markscript`, `use std::fmt`
     - `build.kn`: `use std::markscript`
     - `runtime.kn`: (no stdlib imports, self-contained)
     - Most files: no stdlib imports (self-contained)

2. **Create stdlib surface stubs (`X:/blades/kain/src/stdlib_stubs.kn`):**
   - Define the minimal types and functions:
     ```
     // std::process
     pub fn exit(code: Int) with IO: ...
     pub fn args() -> Array<String> with IO: ...

     // std::fmt
     pub fn println(msg: String) with IO: ...
     pub fn format(template: String, args: ...) -> String with Pure: ...
     pub fn str(value: Int) -> String with Pure: ...
     pub fn str_f64(value: Float) -> String with Pure: ...

     // std::markscript
     pub fn mks_new_vm() -> Int with IO: ...
     pub fn mks_register(vm: Int, intent: String, handler_id: Int) -> Bool with IO: ...
     pub fn mks_table_get_string(vm: Int, table_handle: Int, row: Int, col: Int, default: String) -> String with IO: ...
     pub fn mks_table_get_int(vm: Int, table_handle: Int, row: Int, col: Int, default: Int) -> Int with IO: ...
     // ... (the ~20 markscript functions used by orchestrator.kn)
     ```
   - These stubs don't need real implementations — the codegen will emit `declare` statements and link against the real stdlib at runtime.

3. **Wire stdlib stubs into module resolution:**
   - When `resolve_use_path` encounters `use std::module`, check for:
     1. Real stdlib file at `$KAIN_HOME/stdlib/module.kn` (preferred)
     2. Stub in `stdlib_stubs.kn` (fallback)
   - For the bootstrap phase, use stubs. When real stdlib is available, upgrade.

4. **Register `@extern` symbols from stdlib stubs:**
   - Each stub function with `@extern` annotation gets registered in the RuntimeTable
   - Codegen emits `declare` statements for these symbols
   - At link time, symbols resolve against the real `kain_runtime.lib`

**Acceptance Criteria:**
- [ ] `use std::fmt` in codegen.kn resolves println, format, str
- [ ] `use std::process` in main.kn resolves exit, args
- [ ] `use std::markscript` in orchestrator.kn resolves all 20 markscript API functions
- [ ] Stdlib stub types appear in TypeEnv and are available for typechecking
- [ ] Stub functions with `@extern` annotations produce `declare` statements in LLVM IR

---

### Phase 4: `use` End-to-End Integration (~2 days)

**Objective:** Wire everything together. Make the full 23-file compiler source compile with all `use` imports resolved.

**Implementation Steps:**

1. **Update `driver_session_compile` full pipeline:**
   ```
   driver_session_compile(session, source, source_path, target):
       // Phase 0: Resolve
       emit_progress("Resolve")
       let workspace = discover_workspace(source_path)
       let aggregate = aggregate_sources(workspace)
       if aggregate.errors.len() > 0: bail

       // Phase 1: Lex (on combined source)
       emit_progress("Lex")
       session.tokens = indent_process(lexer_tokenize_all(aggregate.combined, source_path))

       // Phase 2: Parse (single parse over combined source)
       emit_progress("Parse")
       session.ast = parse(parser_new(session.tokens, source_path))

       // Phase 3: Typecheck (4-pass, with use resolution)
       emit_progress("Typecheck")
       session.typed = typecheck(type_env_new(), session.ast, aggregate)

       // Phase 4: Monomorphize
       // Phase 5: Codegen
       // Phase 6: Link
   ```

2. **Update `driver_session_check`** for single-file use case:
   - When checking a single file (not workspace), resolve `use` imports relative to that file's directory
   - If `use std::*` is encountered, preload stdlib stubs
   - If `use local::*` is encountered, find and read the local file, aggregate it

3. **Handle `use` at top level vs in `mod` blocks:**
   - `use` at top level of a file: import symbols into the file's module scope
   - `use` inside `mod Name:` block: import symbols into the submodule scope
   - Kain's `use` is file-scoped, not block-scoped (like Rust)

4. **Symbol conflict detection:**
   - If `use module_a::Symbol` and `use module_b::Symbol` both import the same name: emit ambiguity error
   - If an imported symbol conflicts with a locally-defined symbol: emit shadowing warning (local wins)
   - Provide `use module::Symbol as Alias` to disambiguate (already parsed, just wire it)

5. **Performance optimization — incremental resolution cache:**
   - Cache resolved import paths in a `HashMap<String, String>` keyed by import path string
   - Avoid re-reading and re-typechecking stdlib modules on every compilation
   - In the bootstrap phase, this matters less. Important for interactive `kain check` loops.

**Acceptance Criteria:**
- [ ] `kainc check src/` typechecks all 23 files with imports resolved
- [ ] `kainc build src/ --target llvm` produces LLVM IR for all 23 files aggregated
- [ ] Symbol conflicts produce clear diagnostics
- [ ] Single-file `kainc check src/lexer.kn` resolves imports relative to `src/`
- [ ] Ouroboros Phase 1 (source concatenation via KAIN.toml source_order) integrates with resolve phase
- [ ] The combined source from `aggregate_sources` is byte-identical to the existing `kainc_bootstrap.kn` (when source_order is used instead of auto-discovery)

---

### Phase 5: `include` C Header Pipeline (~4 days)

**Objective:** Make `include <header.h> as alias` work. This enables the compiler to bind against LLVM-C headers for the LLVM-C API codegen path (Path B) and against system C libraries.

**Current state:** `parse_include()` produces valid AST nodes. Everything downstream is a stub. The Rust bootstrap has a 3-tier extraction pipeline (libclang → lang-c AST → regex fallback) across ~6,500 lines in `crates/c-ffi/`.

**Architecture decision:** For the self-host compiler, we use a SIMPLIFIED single-tier approach:
1. **Primary path:** `include <header.h> as alias` → shell out to a companion Python script (`scripts/kain_include_extract.py`) that uses libclang's Python bindings to extract type information → parse the JSON output → generate type-safe FFI bindings.
2. **Fallback path:** Treat all C functions as `fn name(...) -> ptr<Byte> with Unsafe` and all C types as `ptr<Byte>`. This is what the current stubs do — it's lossy but allows compilation to proceed.
3. **Why not embed libclang in Kain?** The Rust bootstrap uses the `clang-sys` crate (Rust bindings to libclang's C API). Kain has no equivalent. Embedding the full libclang C API surface (~1500 functions) via `include <clang-c/Index.h>` would itself require a working C header import pipeline — a chicken-and-egg problem for the bootstrap phase.

**Implementation Steps:**

1. **Create `X:/blades/kain/src/include_ffi.kn`** with:
   - `struct CFunction`: name, return_type, param_names, param_types, calling_conv, attributes
   - `struct CStruct`: name, fields (name + type pairs)
   - `struct CEnum`: name, variants (name + value pairs)
   - `struct CTypedef`: name, underlying_type
   - `struct CBindings`: functions, structs, enums, typedefs

2. **Implement `extract_c_header(header_path: String, alias: String) -> CBindings`:**
   - Shell out: `python scripts/kain_include_extract.py --header <header_path> --json-out <temp.json>`
   - Parse JSON output into CBindings struct
   - For system headers (`<stdio.h>`, `<windows.h>`), use cached pre-extracted bindings
   - If the Python script fails or is unavailable: fall back to stub bindings

3. **Implement `generate_ffi_bindings(bindings: CBindings, alias: String) -> Array<AstNode>`:**
   - For each C function in bindings:
     - Map C types to Kain types using the KainType↔CType mapping table (from design.md)
     - Generate: `pub fn alias_funcName(params) -> ReturnType with Unsafe, IO:`
     - Mark as `@extern @link_name("real_c_symbol")`
   - For each C struct:
     - Generate: `pub struct alias_StructName: field1: Type1, field2: Type2, ...`
   - For each C enum:
     - Generate: `pub enum alias_EnumName: Variant1 = value1, Variant2 = value2, ...`
   - For each C typedef:
     - Generate: `pub type alias_TypeName = MappedKainType`

4. **Implement C type → Kain type mapping:**
   - `int` → `Int(I32)`, `long` → `Int(platform_dependent)`, `long long` → `Int(I64)`
   - `float` → `Float(F32)`, `double` → `Float(F64)`
   - `char*` → `String`, `const char*` → `String`
   - `void*` → `ptr<Byte>`, `int*` → `ptr<Int>`
   - `size_t` → `UInt(U64)` (or `Usize`)
   - Struct types → generated Kain struct names
   - `HANDLE`, `HWND`, etc. (opaque Windows types) → `ptr<Byte>`
   - Function pointers → `ptr<Byte>` (simplified)

5. **Wire into the Resolve phase:**
   - During `aggregate_sources`, when `AST_ITEM_IMPORT` with `include` kind is encountered:
     - Call `extract_c_header` to get bindings
     - Call `generate_ffi_bindings` to produce Kain AST nodes
     - Inject the generated AST nodes into the program (before the main source)
   - The typechecker then checks the injected bindings as regular functions/structs/enums

6. **For the bootstrap phase — provide pre-extracted bindings:**
   - The compiler needs `include <llvm-c/Core.h> as llvm` for Path B codegen
   - Pre-extract LLVM-C bindings into `src/generated/llvm_c_bindings.kn` using the Rust bootstrap's libclang pipeline
   - Commit the generated bindings to the repo
   - During bootstrap, `include <llvm-c/Core.h> as llvm` resolves to the pre-generated bindings file
   - Post-bootstrap, the self-host compiler can extract headers dynamically

**Acceptance Criteria:**
- [ ] `include <stdio.h> as libc` produces bindings for printf, puts, etc.
- [ ] C struct types map correctly to Kain structs with matching field layout
- [ ] `@extern @link_name` annotations are emitted for C functions
- [ ] LLVM-C API bindings are available (pre-generated for bootstrap)
- [ ] Missing C headers produce a diagnostic (not a crash)
- [ ] Fallback stub path works: all C types treated as `ptr<Byte>`, all functions as `fn(...) -> ptr<Byte>`

---

### Phase 6: `include` — Runtime Declare Integration (~2 days)

**Objective:** Wire C header imports into codegen. When a C function is imported via `include`, the codegen must emit the corresponding `declare` statement in the LLVM IR output.

**Implementation Steps:**

1. **Update `RuntimeTable` in `runtime.kn`:**
   - Add functions from `include`-generated bindings to the runtime table
   - Each C function gets an entry with:
     - LLVM symbol name (from `@link_name` or Kain name)
     - LLVM return type (mapped from C type)
     - LLVM parameter types (mapped from C types)
     - C calling convention (`ccc` on Linux, `win64cc` on Windows)
     - Attributes: `nounwind` (default), `readonly` (for pure functions), etc.

2. **Update `codegen_textual()`:**
   - In the module header section, after emitting struct definitions:
     - Walk the `MonomorphizedProgram` for all `@extern` functions
     - Emit `declare` statements for each:
       ```
       declare i32 @printf(i8* nocapture readonly, ...) nounwind
       declare i64 @LLVMContextCreate() nounwind
       ```

3. **Handle C ABI specifics in codegen:**
   - **Untagging:** Before calling a C function, untag integer arguments: `%raw = ashr i64 %tagged, 3`
   - **Tagging:** After a C function returns an integer, tag the result: `%tagged = shl i64 %raw, 3; %tagged = or i64 %tagged, 1`
   - **String passing:** Extract `{i8*, i64}` fat pointer → pass only the `i8*` data pointer to C
   - **String return:** Call `strlen` on returned `i8*` → construct `{i8*, i64}` fat pointer

4. **Update `compile_call_textual()` in codegen.kn:**
   - Detect if the callee is an `@extern` C function (from `TypedItem` metadata)
   - If yes: apply untagging to integer arguments, tagging to return value
   - Apply correct calling convention

**Acceptance Criteria:**
- [ ] `include <stdio.h> as libc` → `declare i32 @printf(i8*, ...)` in LLVM IR
- [ ] Integer arguments to C functions are untagged before the call
- [ ] Integer return values from C functions are tagged after the call
- [ ] Windows calling convention (`win64cc`) is applied on Windows targets
- [ ] `kainc build --target llvm` on a file using `include <stdio.h>` produces valid IR with declares

---

### Phase 7: `include` Type Mapping Verification (~1 day)

**Objective:** Verify that C types map correctly to Kain types and that the ABI is sound.

**Implementation Steps:**

1. **Create a test matrix** of C types → Kain types → LLVM types:
   ```
   C Type         | Kain Type      | LLVM Type       | Size (LP64) | Size (LLP64)
   int            | Int(I32)       | i32             | 4           | 4
   long           | Int(I64/I32)   | i64 / i32       | 8           | 4
   long long      | Int(I64)       | i64             | 8           | 8
   float          | Float(F32)     | float           | 4           | 4
   double         | Float(F64)     | double          | 8           | 8
   void*          | ptr<Byte>      | ptr             | 8           | 8
   char*          | String         | {i8*, i64}      | 16          | 16
   size_t         | UInt(U64)      | i64             | 8           | 8
   ```

2. **Platform detection for C ABI policy:**
   - On Windows: use LLP64 (`long` = 32-bit)
   - On Linux/macOS: use LP64 (`long` = 64-bit)
   - Detect from target triple in BuildConfig

3. **Test with a minimal C header:**
   ```
   include "test/simple.h" as test
   // simple.h: int add(int a, int b) { return a + b; }
   fn main() -> Int with IO:
       return test.add(3, 4)  // expects 7
   ```

**Acceptance Criteria:**
- [ ] C `int` → Kain `Int(I32)` → LLVM `i32` mapping verified
- [ ] C `long` size matches platform ABI (4 bytes on Windows, 8 bytes on Linux)
- [ ] C struct layout matches Kain struct layout field-for-field
- [ ] Test: call a simple C function via `include` and get correct result

---

### Phase 8: `import` Python Bridge (~2 days)

**Objective:** Make `import module as alias` and `from module import name` work for Python interop.

**Current state:** `parse_import()` and `parse_from_import()` parse correctly. Everything downstream is stubbed. The Rust bootstrap has `crates/import/src/` with Python host-object binding logic.

**Architecture decision:** For the self-host compiler, Python imports are a thin codegen layer:
1. At parse/typecheck time: Python imports are treated as opaque — imported symbols have type `ptr<Byte>`.
2. At codegen time: Calls to Python-imported functions emit calls to the Python bridge runtime (`abi_python_call`, `abi_python_import`, etc.).
3. The actual Python host-object binding lives in the C runtime (`runtime/native/src/core/python_system.c`), not in the compiler.

**Implementation Steps:**

1. **Create `X:/blades/kain/src/import_py.kn`** with:
   - `struct PythonImport`: module_name, alias, imported_names (for `from X import a, b`)
   - `fn resolve_python_import(module_name: String) -> PythonImport`
   - Simply records the import; all Python symbols default to `ptr<Byte>` type with `Unsafe, IO` effects

2. **Update `check_item()` for `AST_ITEM_IMPORT` with Python kind:**
   - Register imported module as a namespace
   - All symbols from the module resolve to: `fn name(...) -> ptr<Byte> with Unsafe, IO`
   - Named Kain arguments are preserved for kwargs lowering (the runtime handles this)

3. **Update `compile_call_textual()` for Python calls:**
   - Detect calls to Python-imported functions (from `TypedItem` metadata)
   - Emit call to `abi_python_call(module_name, func_name, args_json, kwargs_json)`
   - Serialize arguments and keyword arguments into JSON strings
   - Deserialize return value from JSON string

4. **Python kwargs lowering:**
   - Kain's named arguments naturally map to Python kwargs:
     ```
     import json as py_json
     let result = py_json.dumps(data, separators = [",", ":"])
     // Lowers to: abi_python_call("json", "dumps", [data], {separators: [",", ":"]})
     ```
   - The codegen distinguishes positional args (unnamed) from keyword args (named) in the call.

5. **Provide runtime bridge functions in the RuntimeTable:**
   - `abi_python_import(module_name: ptr<Byte>) -> ptr<Byte>` — import a Python module
   - `abi_python_call(module: ptr<Byte>, func: ptr<Byte>, args_json: ptr<Byte>, kwargs_json: ptr<Byte>) -> ptr<Byte>` — call a Python function
   - `abi_python_from_import(module_name: ptr<Byte>, name: ptr<Byte>) -> ptr<Byte>` — from-import a single name
   - These functions are declared as `@extern` and emit `declare` statements in LLVM IR

6. **For the bootstrap phase — stub Python imports:**
   - The compiler itself does not use Python imports
   - All Python import calls compile to `declare` + `call` with correct signatures
   - At link time, if `kain_runtime.lib` has the Python bridge, it resolves. Otherwise, linker error.
   - For testing: provide a small Python bridge library (`runtime/native/src/core/python_system.c` already exists)

**Acceptance Criteria:**
- [ ] `import json as py_json` registers py_json as a namespace in the type env
- [ ] `py_json.dumps(value, indent = 2)` compiles to `call @abi_python_call(...)`
- [ ] Named arguments are preserved as kwargs
- [ ] `from torch import tensor` imports a single name
- [ ] Missing Python runtime produces a clear linker error (not a compiler crash)
- [ ] Test: a Kain file using `import json` compiles to valid LLVM IR with Python bridge calls

---

## Verification Checklist

### Module Resolution
- [ ] `discover_workspace("src/")` finds workspace root via KAIN.toml
- [ ] `discover_source_files()` finds all 23 `.kn` files in `src/`
- [ ] `resolve_use_path(["std", "fmt"])` resolves to stdlib/stubs
- [ ] `aggregate_sources()` concatenates all files with byte offset tracking
- [ ] Unresolved imports produce diagnostics with file path and line

### `use` Resolution
- [ ] `use token` in parser.kn resolves to token.kn types
- [ ] `use std::fmt` resolves to stdlib format functions
- [ ] Cross-module symbol visibility respects `pub`
- [ ] Circular imports produce warning, not crash
- [ ] `kainc check src/` typechecks all 23 files with imports resolved

### `include` C Headers
- [ ] `include <stdio.h> as libc` produces C function bindings
- [ ] C struct types map correctly to Kain structs
- [ ] LLVM-C API bindings available (pre-generated)
- [ ] Codegen emits `declare` statements for C functions
- [ ] Integer untagging/tagging at C ABI boundary
- [ ] Platform ABI (LP64 vs LLP64) correctly applied

### `import` Python
- [ ] `import json as py_json` registers as namespace
- [ ] Python calls emit `abi_python_call` bridge calls
- [ ] Kwargs lowering preserves named arguments
- [ ] `from X import Y` imports single names

### End-to-End
- [ ] `kainc build src/ --target llvm` produces LLVM IR for all 23 files
- [ ] Ouroboros source concatenation is byte-identical to existing `kainc_bootstrap.kn`
- [ ] No files outside the owned set were modified

---

## Completion Report

When done, report:
- Files modified: <list with changes summary>
- New files created: <count and names>
- `use` imports resolved: <count of resolved use items across 23 files>
- `include` headers supported: <list of pre-extracted headers>
- `import` modules: <list of stubbed Python imports>
- Module resolution time: <ms for 23-file workspace>
- Any unimplemented import features: <list or "none">
- Any issues encountered: <list or "none">
- What ouroboros Phase 2 needs to know: <notes on import resolution for self-compilation>
- Test results: `kainc check src/` output summary with import resolution diagnostics
