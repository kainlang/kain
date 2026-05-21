---
name: kain-engineer
description: Use when an agent is authoring, importing, debugging, compiling, or reviewing Kain (.kn) code and adjacent Kain workflows. Covers the canonical CLI, build targets, module/import resolution, stdlib loading, KainScript (.ks), TypeScript/C/Rust import pipelines including workspace crate import, and advanced self-hosting commands in the M:\Code monorepo.
---

# Kain Engineer

Use this skill for any task that touches:

##critical -- this skill is a little bit outdated and doesnt reflect the recent work on the repo. kain is a much different lang now so treat this with a grain of salt. the preferable workflow is simple for writing kain-- view /benchmark/cases and go through some high value examples and furthermore 

- `.kn` source files
- `kain build`, `kain run`, or `kain import-*` workflows
- backend target selection or codegen behavior
- KainScript `.ks`
- TypeScript, TSX, Rust, C, or assembly import into Kain
- compiler or self-hosting work in `M:\Code\Kain`

## Core Rules

- Treat live CLI help and current source as the truth. `M:\Code\README.md` and `M:\Code\Kain\README.md` are useful, but may lag behind the code.
- Prefer the modern subcommand CLI: `kain build`, `kain run`, `kain import crates`, `kain import-c`, `kain import-rust`, `kain import-ts`, `kain import-asm`, `kain doctor`, `kain lsp`, `kain selfhost`, `kain inject`, `kain gpu-artifacts`.
- Distinguish three layers before making claims:
  1. Kain language or frontend feature
  2. importer lowering behavior
  3. backend codegen support
- When docs and code disagree, say so explicitly and follow the code.
- Default new Kain source to `.kn`. Treat `.god` as legacy compatibility, not new authoring format.
- Be target-aware. Low-level memory, C ABI, or pointer-heavy code is not equally portable across `ks`, `ts`, `js`, `wasm`, `rust`, `cpp`, `llvm`, and UE5 targets.
- Prefer data-driven additions for mappings, registries, capabilities, or codegen tables instead of scattering new match arms and string checks.

## First Pass

Inside `D:\GreebleFS`, prefer the dev MCP Kain router when available:

```json
{ "command": "overview" }
```

Use that with `gfs_kain`, then use `guide`, `search`, `examples`, `doctor`, `cli`, `run`, or `validate_examples` as needed. This is the fastest way for agents to read the current `src-kain/guides/**` tree, inspect `src-kain/ffi/examples/**`, and run the active local `C:\Users\Admin\.cargo\bin\kain.exe` binary without relying on stale model memory.

From `M:\Code\Kain`:

```powershell
kain doctor
.\target\debug\kain.exe --help
.\target\debug\kain.exe build --help
.\target\debug\kain.exe import-ts --help
.\target\debug\kain.exe import-c --help
.\target\debug\kain.exe import crates --help
```

If the debug binary is missing:

```powershell
cargo build -p cli
cargo run -q -p cli -- --help
```

If you want a quick live dump of the current command surface, run:

```powershell
.\scripts\show-kain-cli.ps1
```

## Task Routing

### Writing or editing `.kn`

Open:

- `references/language-and-authoring.md`
- `references/imports-modules-and-stdlib.md`

### Choosing a build command, target, or packaging path

Open:

- `references/cli-build-and-targets.md`

### Working on Kain actors

Use the `kain-actor-system` skill for actor-system work. The current split is: `crates/kain-core` owns actor syntax/typechecking/interpreter execution, while `crates/kain-actor` owns reusable actor IDs, message contracts, mailbox/lifecycle/supervision/scheduler/behavior/registry/system/native ABI models, validation, and typed actor contracts.

### Working on the Kain REPL

Use `crates/kain-repl` as the ownership point for interactive REPL work. Keep `crates/cli/src/main.rs` as a thin command host for `kain repl`, `kn repl`, and the `kn` no-args terminal path.

Current split:

- `crates/kain-repl/src/lib.rs` is only the public index.
- `command.rs` owns dot directives such as `.run`, `.clear`, `.exit`, `.quit`, and `.help`.
- `session.rs` owns multiline buffering, prompt state, blank-line evaluation, and EOF behavior.
- `evaluation.rs` owns diagnostics-formatted evaluation through `kain-driver::DriverSession` and `CompileTarget::Interpret`.
- `terminal.rs` owns process IO and generic testable `BufRead` / `Write` entrypoints.
- `source.rs` owns BOM/shebang normalization shared by CLI source reads.
- `metadata.rs` owns the REPL banner/build metadata shape.

Validation for REPL work:

```powershell
cargo fmt -p kain-repl -p cli
cargo test -p kain-repl --target-dir target\codex-kain-repl -- --nocapture
cargo build -p cli --target-dir target\codex-kain-repl-cli
$inputText = "fn main() -> Int:`r`n    return 42`r`n`r`n.exit`r`n"; $inputText | target\codex-kain-repl-cli\debug\kain.exe repl
```

Do not recreate a second CLI-local interactive loop. Future persistent bindings, history/editing adapters, structured evaluation events, or UI/API hosts should land in `kain-repl` first.

### Working on native LLVM/C stdlib or runtime

Use this current contract before changing target stdlib, native runtime manifests, or actor/entangle lowering:

- The root `stdlib/` folder is the shared canonical stdlib profile for `llvm` and direct `c` builds. `stdlib/c` is loaded after it only for direct C, and legacy `std::native::*` imports are compatibility aliases over the same root modules rather than a second on-disk stdlib tree.
- Normal file builds should prefer `runtime/native_core_runtime.toml`; the broader `runtime/native_runtime.toml` is for app/UI/vendor runtime work.
- The stdlib-facing C ABI facade is `runtime/native/include/kain_runtime_native_stdlib.h` plus `runtime/native/src/core/kain_runtime_native_stdlib.c`. It wraps runtime init/shutdown, actors, entangle, diagnostics/status, scheduler, and time helpers for generated native code.
- Direct C backend rules live in `crates/kain-sys-codegen/src/codegen_c.rs`: `@extern` emits declarations only, actor `spawn`/`send` lower to the facade, `main` must emit C `int`, and entangle metadata registers through a generated `__kain_register_entanglements()` call from `main`.
- The current all-in-one proof fixture is `runtime/fixtures/native_world_actor_intent/main.kn`. Validate both native targets with `kain build ... -t llvm` and `kain build ... -t c`, then run both produced executables.
- The facade conformance smoke is `runtime/conformance/native_stdlib_bridge/test_native_stdlib_bridge.c`; build it with bundled `toolchain\llvm\bin\clang.exe` and the core native runtime sources.

### Working on `use`, `mod`, stdlib, or file layout

Open:

- `references/imports-modules-and-stdlib.md`

### Working on web output, scripts, TS, or TSX

Open:

- `references/kainscript-and-typescript.md`

For TypeScript importer ambient globals:

- Treat `crates/kain-import/src/typescript/data/typescript_ambient_manifest.json` as generated data, not a hand-edited source file.
- Regenerate it with `python tools\typescript_import\extract_ambient_manifest.py` after changing `reference/TypeScript-main/src/lib` inputs or `tools/typescript_import/typescript_ambient_overrides.json`.
- Put Kain-specific constructor aliases, lowering helpers, ecosystem globals, suppressed built-in type names, and utility-type fallback names in `tools/typescript_import/typescript_ambient_overrides.json`; do not reintroduce hardcoded prelude arrays in `crates/cli/src/import_typescript.rs`.
- Validate with `cargo test -p kain-import ambient --target-dir target\codex-ts-import-manifest`, `cargo build -p cli --target-dir target\codex-ts-import-manifest`, and at least one `kain import-ts ... --target ts` smoke.
- When large TS imports fail on generated unknown identifiers, inspect destructured params and callback lowering first. The importer should bind destructured function/constructor params from their synthetic source params and should emit fallback `Any` bindings for high-arity callback params instead of adding project-specific globals.

### Importing or validating C-derived code

Open:

- `references/c-import-and-low-level.md`

### Importing or mirroring Rust workspaces

- Use `kain import crates [workspace_root]` for workspace-scale Rust imports.
- The command auto-detects `crates/`, `rust/`, or `src/rust/` under the chosen
  root unless `--source-root` is provided.
- Default mode writes one bundle at `<source-root>.kn`.
- `kain import crates --blades` mirrors each discovered crate/file into a
  blades-style `.kn` tree under `<workspace-root>/blades` or an explicit
  `--output`.
- Good smoke corpus: `reference/cuda`, which exercises many crates and nested
  source/test/example folders without requiring selfhost-specific assumptions.

### Changing compiler, importer, or self-hosting internals

Open:

- `references/source-of-truth-and-repo-map.md`
- `references/selfhost-and-ouroboros.md`

## Output Expectations

When you apply this skill:

- Name the intended target or targets if behavior is target-sensitive.
- Call out importer and backend limits instead of implying universal support.
- Prefer small validation loops: `kain doctor`, `kain build`, `cargo test -p <crate>`, importer smoke tests, or emitted artifact inspection.
- If you modify CLI, importer, parser, runtime, or backend behavior, add or update tests in the owning crate when practical.
- Keep notes grounded in current repo paths and source files, not only the top-level README.
