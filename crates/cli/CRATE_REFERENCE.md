# cli — KAIN Compiler CLI Reference

> **Last Updated:** 2026-03-01
> **Status:** Production — full pipeline for all 15 targets, LSP, UE5 packager, C importer, Rust importer, ASM importer.

---

## Purpose

The KAIN compiler entry point. Orchestrates all compilation pipelines, the UE5 packager, the language server, watch mode, project init, and all import commands.

---

## Source Files

| File | Size | Purpose |
|---|---|---|
| `packager/ue5_pipeline.rs` | 124KB (2658L) | UE5 plugin build orchestrator — the largest file in the CLI |
| `import_c.rs` | 57KB | C import command handler |
| `main.rs` | 40KB | CLI arg parsing, command dispatch, compile pipeline |
| `lsp.rs` | 22KB | Language Server Protocol implementation |
| `lib.rs` | 17KB | Public CLI library API |
| `packager/codegen.rs` | 76KB | Per-module code generation coordinator |
| `packager/dependencies.rs` | 16KB | Build dependency resolution |
| `packager/inject.rs` | 17KB | `kain inject` — non-destructive plugin injection |
| `packager/registry_writer.rs` | 16KB | `AssetRegistry.bin` generation |
| `packager/build_cs_gen.rs` | 14KB | `Build.cs` generation |
| `packager/plugin_layout.rs` | 11KB | Plugin directory structure |
| `packager/config.rs` | 8KB | `KAIN.toml` + `Ue5Config` schema |
| `packager/cpp_validator.rs` | 10KB | Generated C++ validation |
| `packager/post_process.rs` | 7KB | 5 post-processing fix passes |
| `packager/uplugin_gen.rs` | 4KB | `.uplugin` JSON generation |

---

## Commands

### `kain build` — Compile a file or project

```
kain build [FILE] [OPTIONS]

Options:
  --ue5                   Build UE5 plugin from KAIN.toml
  --target / -t <TARGET>  Compilation target
  --targets <LIST>        Multiple targets (comma-separated)
  --output / -o <PATH>    Output file or directory
  --verbose               Verbose output
  --emit-ast              Dump parsed AST
  --emit-typed            Dump type-annotated AST
  --dry-run               Preview actions without writing
  --watch / -w            Watch mode — auto-recompile on change
  --strict                Treat warnings as errors
  --analyze               Analyze shader complexity (USF only)
  --plugin <NAME>         Target plugin name
  --plugins-dir <DIR>     Base plugins directory
  --embed                 Embed KAIN markers in generated output
```

### `kain run` — Execute immediately (interpreter)

```
kain run <FILE> [--verbose]
```

### `kain init` — Initialize new project

```
kain init [PATH] [--name <PROJECT_NAME>]
```

Generates: `KAIN.toml`, `src/main.kn`, `stdlib/`, `.gitignore`.

### `kain inject` — Non-destructive plugin injection

```
kain inject <FILES...> [OPTIONS]

Options:
  --ue5                   Use UE5 mode
  --plugin <NAME>         Target plugin name (auto-detected if omitted)
  --plugin-dir <DIR>      Explicit plugin directory
  --dry-run               Preview changes without writing
  --force                 Overwrite existing generated files
```

Inject preserves all existing plugin code — only adds new generated files to `Source/Private/Generated/`.

### `kain import-c` — Import C source

```
kain import-c <INPUT> [OPTIONS]

Options:
  --output / -o <FILE>    Output .kn file
  --target / -t <TARGET>  Compile target for ABI policy selection
  --validate              Parse/transform only, no output
  --fail-fast             Stop on first error (for directories)
  --report-json <FILE>    Write import failure/report JSON
```

Processes a single `.c` file or a directory of `.c` files.

### `kain import-asm` — Import assembly

```
kain import-asm <INPUT> [OPTIONS]

Options:
  --format <FORMAT>       Dialect: gameboy, 6502-furby, z80, etc.
  --out / -o <FILE>       Output .kn file
  --validate-only         Parse/validate without writing
```

### LSP Mode

```
kain lsp
```

Invoked by IDEs. Implements LSP over stdin/stdout with:
- Diagnostics on file change
- Hover (type info for identifiers)
- Go-to-definition
- Completion (stdlib functions, KAIN keywords)

---

## UE5 Plugin Build Pipeline (`ue5_pipeline.rs`, 124KB)

### `build_ue5_plugin_with_options()` — Core Orchestrator (1431 lines)

11-stage pipeline:

| Stage | Action |
|---|---|
| 1 | Read `KAIN.toml` or auto-detect config (`create_default_config`) |
| 2 | Resolve source files per module via `source_globs` |
| 3 | Load stdlib + user source, parse, type-check |
| 4 | Run The Oracle (`validate_program_with_custom_rules`) |
| 5 | Generate C++ per-item (`codegen.rs`) |
| 6 | Generate GAS code (tags, attributes; abilities/effects if CLI-wired) |
| 7 | Generate shaders via `ue5-shaders` |
| 8 | Serialize material graphs → binary `.uasset` via `ue5-materials` |
| 9 | Post-process C++ (5 fix passes) |
| 10 | Write output files (`Source/`, `Shaders/`, `Content/`) |
| 11 | Generate `.uplugin`, per-module `Build.cs`, `AssetRegistry.bin` |

### `load_and_parse_sources()` (556 lines)

Multi-source loader:
- Reads stdlib files in profile order
- Reads user `.kn` files per module glob
- Merges into single source string
- Calls `Parser::new().parse()`
- Calls `types::check()`
- Returns `TypedProgram` + `shader_names` + `material_graphs` + `graph_editors` + `graph_runtimes`

### Material Pipeline (`convert_material_graph`, `emit_expr`)

`convert_material_graph()` maps AST `MaterialGraphDef` to `ue5_materials::MaterialGraph`:
- Recursively calls `emit_expr()` — 252-line recursive material node emitter
- Handles `call(shader_name)` nodes via `surface_shaders` map (pre-emitted HLSL for surface shaders)
- Assigns `(x, y)` layout positions automatically for graph display

### Engine Version Parsing

`parse_engine_version(s)` accepts:
- `"5.4"` / `"5.7"`
- `"UE5_4"` / `"UE5_7"`
- `"VER_UE5_4"` / `"VER_UE5_7"`

Maps to `unreal_asset_base::engine_version::EngineVersion` — native true version, not capped.

---

## KAIN.toml Schema (`packager/config.rs`)

```toml
[package]
name = "MyPlugin"
version = "1.0.0"
authors = ["Your Name"]

[ue5]
plugin_name = "MyPlugin"
engine_version = "5.4"          # "5.0" - "5.7"
category = "Gameplay"
description = "My plugin"

[[ue5.modules]]
name = "MyPlugin"
type = "Runtime"               # Runtime | Editor | Developer | UncookedOnly
loading_phase = "Default"
source_globs = ["src/runtime/**/*.kn"]

[[ue5.modules]]
name = "MyPluginEditor"
type = "Editor"
depends_on = ["MyPlugin"]
source_globs = ["src/editor/**/*.kn"]

[build]
targets = ["wasm", "js"]
output_dir = "dist"
```

**Module validation:** duplicate module names, unknown dependency names, circular dependency detection — all at parse time before any codegen.

---

## Compile Pipeline (non-UE5 targets, `run_compile`)

```
load stdlib (load_stdlib_for_target)
    ↓
Lexer::new().tokenize()
    ↓
Parser::new().parse()
    ↓
comptime::eval_program()     [if comptime annotations present]
    ↓
types::check()
    ↓
monomorphize::monomorphize()
    ↓
lower_typed_program_memory_for_target()   [low-level semantics]
    ↓
backend::generate()         [web / sys / gpu / ue5 crate]
    ↓
write output
```

### Watch Mode

`watch_mode()` uses `notify` crate to monitor file changes, debounces 100ms, re-runs full `run_compile()` on modification.

---

## `kain doctor` Output

`print_doctor()` reports:
- Compiler version + build timestamp
- Detected UE5 installations (path + version)
- Stdlib search roots (resolved paths)
- Active language capabilities
- Available dialect formats (ASM importer)

---

## LSP (`lsp.rs`, 22KB)

Full LSP server over stdin/stdout:
- `textDocument/didOpen`, `textDocument/didChange` → parse + diagnostics push
- `textDocument/hover` → identifier type lookup
- `textDocument/definition` → go-to-definition (function/struct/enum)
- `textDocument/completion` → stdlib functions + KAIN keywords
- Message framing: `Content-Length: N\r\n\r\n{json}` (standard LSP wire format)

---

## Dependencies

| Crate | Role |
|---|---|
| `clap` | CLI argument parsing |
| `kain-core` | Lexer, Parser, TypeChecker, Monomorphize, Stdlib |
| `kain-import` | C + Rust importers |
| `kain-asm` | Assembly importer |
| `web` | WASM / JS / TS / KS / Hybrid backends |
| `sys` | LLVM / Rust / C++ backends |
| `gpu` | SPIR-V / HLSL backends |
| `ue5` | UE5 runtime codegen |
| `ue5-shaders` | USF codegen |
| `ue5-materials` | Material `.uasset` codegen |
| `ue5-blueprints` | Blueprint codegen |
| `ue5-editor` | Editor UI codegen |
| `ue5-gas` | GAS codegen |
| `ue5-graphs` | Graph editor + runtime codegen |
| `ue5-asset-utils` | Binary asset primitives |
| `notify` | File system watching (watch mode) |
| `serde` / `serde_json` | KAIN.toml + LSP JSON |
| `unreal_asset_base` | UE5 engine version enum |
