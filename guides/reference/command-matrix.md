# Command Matrix

Snapshot: April 12, 2026.

This page is the canonical command inventory for `crates/cli/src/main.rs` and
its subcommand modules.

## Launcher Behavior

- `kain` is the explicit compiler/driver entrypoint.
- `kn` is the run-first launcher.
- `kn` shows the quick-start menu when launched with no command and no input.
- `kn` treats a bare `--target wasm` request with no output as `run`.
- The CLI prints the supported target aliases from the live target registry, not
  from a hardcoded doc list.

## Global Flags

These flags live at the top level of `kain` / `kn`:

| Flag | Meaning |
| --- | --- |
| `input` | legacy positional source file |
| `-c`, `--code` | inline Kain source |
| `-o`, `--output` | output file path |
| `-t`, `--target` | compilation target, default `wasm` |
| `--run` | run after compilation when the lane supports it |
| `--watch` | recompile on file changes |
| `--emit-ast` | print the AST |
| `--emit-typed` | print the typed AST |
| `--verbose` | verbose CLI output |
| `--plugin` | UE5 plugin name for shader copy and packaging lanes |
| `--plugins-dir` | base UE5 plugin directory |
| `--dry-run` | print planned actions only |
| `--strict` | treat supported warnings as errors |
| `--analyze` | shader complexity analysis for USF-related paths |

## Top-Level Commands

| Command | What it does | Primary guide |
| --- | --- | --- |
| `init` | create a new KAIN project with `KAIN.toml`, `src/main.kn`, and `.gitignore` | `cli/build-run-init.md` |
| `lsp` | start the language server | `cli/cli-overview.md` |
| `doctor` | print diagnostics and expose the repair lane | `cli/doctor-and-repair.md` |
| `selfhost` | run the self-host bootstrap pipeline | `cli/selfhost-omni-fabric-lsp.md` |
| `omni` | build mixed-language omni manifests | `cli/selfhost-omni-fabric-lsp.md` |
| `fabric` | init, validate, and run Fabric manifests | `cli/selfhost-omni-fabric-lsp.md` |
| `build` | build a file or a `KAIN.toml` project | `cli/build-run-init.md` |
| `run` | explicit interpret-mode execution | `cli/build-run-init.md` |
| `gpu-artifacts` | emit SPIR-V, Rust host wrappers, and reflection JSON | `cli/native-ui-and-packaging.md` |
| `inject` | inject Kain output into an existing plugin | `cli/native-ui-and-packaging.md` |
| `import-asm` | import assembly source | `cli/importers.md` |
| `import-c` | import C source | `cli/importers.md` |
| `import-rust` | import Rust source | `cli/importers.md` |
| `import-crate` | import a Rust crate through the crate FFI layer | `cli/importers.md` |
| `import-ts` | import TypeScript source | `cli/importers.md` |

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

### Fabric

| Command | Flags |
| --- | --- |
| `fabric init [path]` | `--template local|polyglot` |
| `fabric validate` | `--manifest <path>` |
| `fabric run` | `--manifest <path>` |

## Native UI And Packaging

- `kain build native-ui` drives the native UI materialization pipeline.
- `kain inject` stages `.kn` files into an existing plugin.
- `kain gpu-artifacts` emits shader-side artifact bundles for GPU workflows.
- `kain build -t ue5`, `kain build -t ue5editor`, and `kain inject --ue5`
  are the UE5-oriented packaging lanes.
