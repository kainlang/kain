# Troubleshooting

Snapshot: April 12, 2026.

This page collects the recurring failures that show up in the current codebase
and points to the subsystem that owns the fix.

## `Unknown target`

Likely cause:

- the alias is not in the live driver registry
- the backend feature for that target was compiled out

Fix:

- check `guides/reference/target-matrix.md`
- run `kain doctor` and confirm the supported target list
- remember that `kn` rewrites a bare `wasm` request to `run` when no output is
  specified

Relevant code:

- `crates/kain-driver/src/lib.rs`
- `crates/cli/src/main.rs`

## `No KAIN.toml found`

Likely cause:

- you are inside a file build lane but not inside a KAIN project root

Fix:

- run `kain init`
- or point `kain build`, `kain omni`, or `kain fabric` at the project root

Relevant code:

- `crates/cli/src/packager/mod.rs`
- `crates/cli/src/omni.rs`
- `crates/cli/src/fabric.rs`

## Native UI Build Fails With No Component

Likely cause:

- the input source does not contain a component root that can become the app
  entrypoint

Fix:

- add at least one `component`
- or pass `--root` when the root component name is known

Relevant code:

- `crates/cli/src/native_ui_build.rs`

## LLVM Native Builds Feel Heavy

Likely cause:

- the native lane compiles the full runtime bundle, not a tiny ad hoc helper

Fix:

- treat `kain build -t llvm` as a full native link step
- keep the runtime bundle in sync and avoid assuming this path is incremental

Relevant code:

- `crates/cli/src/main.rs`
- `runtime/native_runtime.toml`

## GPU Artifact Generation Needs More Than One Feature

Likely cause:

- `kain build` or `kain gpu-artifacts` was built without both `gpu` and `sys`
  features

Fix:

- rebuild the CLI with the appropriate feature set
- use `kain doctor` to confirm enabled features

Relevant code:

- `crates/kain-driver/src/lib.rs`
- `crates/cli/src/main.rs`

## Doctor Repair Behavior Looks Too Aggressive

Likely cause:

- `--suggest`, `--dry-run`, and `--profile` were combined in a surprising way

Fix:

- remember the selection order:
  1. `--suggest`
  2. `--dry-run`
  3. `--profile aggressive`
  4. `--profile safe`
- use `--repair-tree` only when you want every `.kn` file under a directory

Relevant code:

- `crates/cli/src/repair.rs`

## Selfhost Emits Partial Output

Likely cause:

- earlier crates in the phase failed and later output was stopped or skipped

Fix:

- use `--force` when you want the phase to continue emitting later artifacts
- use `--all-crates` when you want the workspace-wide crate sweep

Relevant code:

- `crates/cli/src/selfhost.rs`
- `crates/cli/src/selfhost_report.rs`

## Fabric Or Omni Commands Do Not Match The Manifest

Likely cause:

- the manifest file is missing or malformed
- the selected template does not match the output you expected

Fix:

- `fabric init` writes `KAIN.fabric.toml`
- `omni init` writes `KAIN.omni.toml`
- validate the manifest before trying to run it

Relevant code:

- `crates/cli/src/fabric.rs`
- `crates/cli/src/omni.rs`

## Importers Produce More Or Less Output Than Expected

Likely cause:

- the include/exclude filters were too broad
- `--flat` changed module layout
- `--report-json` was omitted when you wanted a failure report

Fix:

- inspect the importer-specific flags in `guides/reference/command-matrix.md`
- prefer `--fail-fast` only when you want the first hard stop

Relevant code:

- `crates/cli/src/import_c.rs`
- `crates/cli/src/import_rust.rs`
- `crates/cli/src/import_typescript.rs`
- `crates/cli/src/import_usf.rs`
- `crates/cli/src/import_crate.rs`

## Environment Variables To Check

| Variable | Subsystem |
| --- | --- |
| `KAIN_STDLIB_PATH` | stdlib loader |
| `KAIN_STDLIB_PROFILE` | stdlib loader |
| `KAIN_REALTIME_APP_BUNDLE` | graphics/realtime runtime |
| `KAIN_COMPUTE_RESIDENCY` | compute residency runtime |
| `KAIN_GPU_RUNTIME_LIBRARY` | GPU runtime selection |
| `KAIN_UI_COMPILED_BUNDLE_ENV` | UI bundle loader |
| `KAIN_UI_NATIVE_QT_ARTIFACT_DIR` | native UI artifact capture |
| `KAIN_UI_NATIVE_QT_SCREENSHOT_PATH` | native UI screenshot capture |

## When In Doubt

- Trust the code over stale prose.
- Use `kain doctor` first.
- Use the reference pages in `guides/reference/` before assuming the CLI or
  target behavior is missing.
