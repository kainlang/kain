# Command Matrix

Snapshot: May 12, 2026.

This page is the canonical command inventory for the built-in command
manifests in `crates/kain-commands/commands/*.toml` and the typed Clap routers
in `crates/kain-commands/src/*.rs`. `crates/cli` is the host binary and
execution shell.

## Launcher Behavior

- `kain` is the explicit compiler/driver entrypoint.
- `kn` is the run-first launcher.
- `kn` shows the quick-start menu when launched with no command and no input.
- `kn` treats a bare `--target wasm` request with no output as `run`.
- The CLI prints the supported target aliases from the live target registry, not
  from a hardcoded doc list.
- Unknown `kain` / `kn` commands pass through the runtime command resolver
  before returning an unsupported-command error.

## Global Flags

These flags live at the top level of `kain` / `kn`:

| Flag | Meaning |
| --- | --- |
| `input` | legacy positional source file |
| `-c`, `--code` | inline Kain source |
| `-o`, `--output` | output file path |
| `-t`, `--target` | compilation target, default `wasm` |
| `-r`, `--run` | run after compilation when the lane supports it |
| `-w`, `--watch` | recompile on file changes |
| `--emit-ast` | print the AST |
| `--emit-typed` | print the typed AST |
| `-v`, `--verbose` | verbose CLI output |
| `--plugin` | UE5 plugin name for shader copy and packaging lanes |
| `--plugins-dir` | base UE5 plugin directory |
| `--dry-run` | print planned actions only |
| `--strict` | treat supported warnings as errors |
| `--analyze` | shader complexity analysis for USF-related paths |
| `-h`, `--help` | show command help |
| `-V`, `--version` | show binary version |

## Top-Level Commands

| Command | What it does | Primary guide |
| --- | --- | --- |
| `init` | create a new KAIN project with `KAIN.toml`, `src/main.kn`, and `.gitignore` | `cli/build-run-init.md` |
| `lsp` | start the language server | `cli/cli-overview.md` |
| `doctor` | print diagnostics and expose the repair lane | `cli/doctor-and-repair.md` |
| `check` | typecheck `.kn` / `.ks` source without emitting artifacts | `cli/check-and-test.md` |
| `test` | run compiletest-style Kain source suites and `test` items | `cli/check-and-test.md` |
| `selfhost` | run the self-host bootstrap pipeline | `cli/selfhost-omni-fabric-lsp.md` |
| `omni` | build mixed-language omni manifests | `cli/selfhost-omni-fabric-lsp.md` |
| `fabric` | init, validate, and run Fabric manifests | `cli/selfhost-omni-fabric-lsp.md` |
| `commands` | list/export registry metadata, list manifest packs, or render dynamic registry help | `cli/cli-overview.md` |
| `build` | build a file or a `KAIN.toml` project | `cli/build-run-init.md` |
| `run` | resolve and execute a Kain source, C file, blade, manifest, Cargo crate, Node/Bun entry, or workspace | `cli/build-run-init.md` |
| `watch` | run the unified run plan in watcher mode | `cli/build-run-init.md` |
| `gpu-artifacts` | emit SPIR-V, Rust host wrappers, and reflection JSON | `cli/native-ui-and-packaging.md` |
| `inject` | inject Kain output into an existing plugin | `cli/native-ui-and-packaging.md` |
| `import-asm` | import assembly source | `cli/importers.md` |
| `import-c` | import C source | `cli/importers.md` |
| `import-rust` | import Rust source | `cli/importers.md` |
| `import-crate` | import a Rust crate through the crate FFI layer | `cli/importers.md` |
| `import-ts` | import TypeScript source | `cli/importers.md` |
| `help` | print help for the root command or a subcommand | `cli/cli-overview.md` |

## Command Registry

- Built-in command metadata is indexed by
  `crates/kain-commands/commands/index.toml`, with each top-level
  `crates/kain-commands/commands/*.toml` file acting as a command pack.
- `unreal.toml` intentionally preserves the UE5-facing command surface as a
  visible pack. Current executable entries are `gpu-artifacts` and `inject`,
  while `build` keeps UE5 build targeting through its existing flags and tags.
- Typed command parsers live in `crates/kain-commands/src/kain.rs` and
  `crates/kain-commands/src/blade.rs`; shared argument structs live beside them.
- `kain commands list --bin kain|kn|blade` prints the registered command view.
- `kain commands export --bin kain|kn|blade` emits the same registry as JSON.
- `kain commands packs` prints the built-in command packs.
- `kain commands help --bin kain|kn|blade` renders the dynamic Clap tree built
  from the registry.
- `--runtime` includes `[[commands]]` contributions loaded from the current
  blade workspace manifests. Built-ins win conflicts in this first runtime pass.

## Build Command

`build` supports both file mode and manifest mode.

### Common Flags

| Flag | Meaning |
| --- | --- |
| `--targets` | comma-separated target override list |
| `--ue5` | build a UE5 plugin from the manifest |
| `--rust` | build Rust artifacts from the manifest |
| `--embed` | embed original KAIN source in generated C++ |

### `build native-ui`

| Flag | Meaning |
| --- | --- |
| `input` | Kain source file for the native UI app |
| `--root` | root component override |
| `--app-name` | app / Cargo package name |
| `--window-title` | native window title |
| `-o`, `--out` | materialized project output directory |
| `--artifact-dir` | artifact directory inside the materialized project |
| `--bundle-only` | stop after materializing the app bundle |
| `--release` | build the generated executable in release mode |
| `--runtime-crate` | native UI runtime crate name, default `kain-ui-native` |
| `--runtime-path` | explicit path dependency for the runtime crate |
| `--runtime-version` | published version dependency for the runtime crate |

## Run And Watch

`run` is the unified immediate-execution pipeline owned by `crates/kain-run`.
The CLI host only parses, prints, and sets exit codes.

| Command | Meaning |
| --- | --- |
| `kain run [input]` | resolve and execute once |
| `kain run dev [input]` | execute and keep re-running when planned inputs change |
| `kain run plan [input]` | print the resolved plan without executing |
| `kain watch [input]` | alias-style top-level watcher for the same dev mode |
| `kain blades run [blade]` | run a selected blade through the same pipeline |
| `blade run [blade]` | standalone blade launcher for the same pipeline |

Shared flags:

| Flag | Meaning |
| --- | --- |
| `--target auto|kain|c|cargo|fabric|node|bun` | target override |
| `--json` | emit JSON plan or report |
| `--trace` | request trace-oriented report detail |
| `--keep-artifacts` | keep cached/generated run artifacts |
| `--dry-run` | plan without executing; on watch/dev this avoids entering the watcher loop |
| `-- <ARGS>...` | pass runtime args to process-backed adapters |

The `[run]` manifest section can provide `entry`, `blade`, `target`, `args`,
`env`, `cwd`, and `watch`. Cached run executables and Cargo target dirs live
under `.kain/cache/run`; JSON reports and JSONL event streams live under
`.kain/reports/run`.

## Check And Test

### `check`

| Flag | Meaning |
| --- | --- |
| `input` | Kain source file, directory, or `-` for stdin |
| `-t`, `--target` | target profile to validate against, default `run` |
| `--fail-fast` | stop after the first failed file |
| `--json` | write a structured check report |

### `test`

| Flag | Meaning |
| --- | --- |
| `input` | Kain source file or directory |
| `--mode` | override source directives with `check-pass`, `check-fail`, `run-pass`, `run-fail`, or `kain-test` |
| `-t`, `--target` | default target profile for check modes, default `run` |
| `--fail-fast` | stop after the first failed case |
| `--ignored` | run cases marked with `//@ ignore` instead of skipping them |
| `--json` | write a structured test report |

## Importer Flags

### `import-asm`

| Flag | Meaning |
| --- | --- |
| `input` | assembly source file |
| `--format` | input dialect, default `6502-furby` |
| `--out` | generated `.kn` output file |
| `--validate-only` | parse and report without writing |

### `import-c`

| Flag | Meaning |
| --- | --- |
| `input` | C source file or directory |
| `-o`, `--output` | generated `.kn` output |
| `-t`, `--target` | compile directly without writing `.kn` |
| `-I`, `--include-paths` | C preprocessor include paths |
| `-D`, `--defines` | C preprocessor defines |
| `--flat` | flatten imported symbols into one global scope |
| `--include` | include filters by path fragment |
| `--exclude` | exclude filters by path fragment |
| `--fail-fast` | stop on first failed file import |
| `--report-json` | write an import report |

### `import-rust`

| Flag | Meaning |
| --- | --- |
| `input` | Rust source file or directory |
| `-o`, `--output` | generated `.kn` output |
| `-t`, `--target` | compile directly without writing `.kn` |
| `--flat` | flatten imported symbols into one global scope |
| `--include` | include filters by path fragment |
| `--exclude` | exclude filters by path fragment |
| `--fail-fast` | stop on first failed file import |
| `--report-json` | write an import report |

### `import-crate`

| Flag | Meaning |
| --- | --- |
| `crate_name` | crate name used by `use rust::<crate_name>` |
| `--manifest-path` | Cargo manifest for workspace resolution |
| `--crate-path` | explicit local crate folder or `Cargo.toml` |
| `--mode` | `live`, `generate`, or `both` |
| `-o`, `--output` | generated artifacts directory |
| `--report-json` | override the report path |
| `--features` | comma-separated Cargo features |
| `--all-features` | enable all crate features |
| `--no-default-features` | disable default crate features |

### `import-ts`

| Flag | Meaning |
| --- | --- |
| `input` | TypeScript source file or directory |
| `-o`, `--output` | generated `.kn` output |
| `-t`, `--target` | compile directly without writing `.kn` |
| `--flat` | flatten imported symbols into one global scope |
| `--include` | include filters by path fragment |
| `--exclude` | exclude filters by path fragment |
| `--fail-fast` | stop on first failed file import |
| `--report-json` | write an import report |

## Doctor And Repair

`DoctorRepairArgs` supports:

- `--repair FILE`
- `--repair-tree DIR`
- `--profile safe|aggressive`
- `--suggest`
- `--dry-run`
- `--write`

Selection precedence is:

1. `--suggest`
2. `--dry-run`
3. `--profile aggressive`
4. `--profile safe`

`--repair-tree` and `--repair` are mutually exclusive.

## Selfhost

| Command | Flags |
| --- | --- |
| `selfhost phase1` | `--inventory-dir`, `--output-dir`, `--profile-path`, `--emit-bundles`, `--all-crates`, `--force` |
| `selfhost phase2` | `--inventory-dir`, `--output-dir`, `--profile-path`, `--emit-bundles`, `--emit-roundtrip-rust`, `--assemble-stage2`, `--build-stage2`, `--all-crates`, `--force` |

`phase2` is the repair-oriented lane. It can emit split roundtrip Rust trees
and assemble a stage2 workspace from them.

## Omni And Fabric

### Omni

| Command | Flags |
| --- | --- |
| `omni init [path]` | creates `KAIN.omni.toml` |
| `omni build` | `--manifest <path>` |

See [guides/pipelines/omni.md](/home/ephemara/Dev/Kain/guides/pipelines/omni.md)
for the staged-import and target fan-out model.

### Fabric

| Command | Flags |
| --- | --- |
| `fabric init [path]` | `--template local|polyglot` |
| `fabric validate` | `--manifest <path>` |
| `fabric run` | `--manifest <path>` |

See [guides/pipelines/fabric.md](/home/ephemara/Dev/Kain/guides/pipelines/fabric.md)
for the manifest, runtime kind, and report model.

## Native UI And Packaging

- `kain build native-ui` drives the native UI materialization pipeline.
- `kain inject` stages `.kn` files into an existing plugin.
- `kain gpu-artifacts` emits shader-side artifact bundles for GPU workflows.
- `kain build -t ue5`, `kain build -t ue5editor`, and `kain inject --ue5`
  are the UE5-oriented packaging lanes.
