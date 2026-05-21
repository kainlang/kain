---
name: kain-run-pipeline
description: Use when adding, changing, debugging, validating, or reviewing Kain's unified run pipeline, including crates/kain-run, kain run, kain run dev, kain run plan, kain watch, kain blades run, blade run, KAIN.toml [run] metadata, run reports, run caches, and C/Cargo/Fabric/Node/Bun/Kain execution adapters.
---

# Kain Run Pipeline

## Start Here

Read the project root `AGENTS.md`, `ARCHITECTURE.md`, and `MEMORY.md` first. The run pipeline is a crate-owned subsystem, not a CLI-only branch.

Use these files as the main map:

- `crates/kain-run/src/lib.rs`: `RunRequest`, `RunPlan`, `RunUnit`, adapters, execution, watcher loop, reports.
- `crates/kain-run/Cargo.toml`: dependency boundary.
- `crates/kain-blades/src/lib.rs`: `KainRunSection` and blade/workspace manifest parsing.
- `crates/kain-commands/src/kain.rs`: typed Clap shape for `run`, `run dev`, `run plan`, `watch`.
- `crates/kain-commands/src/blade.rs`: typed Clap shape for `kain blades run` and `blade run`.
- `crates/kain-commands/commands/kain.toml` and `commands/blade.toml`: command registry metadata.
- `crates/cli/src/run.rs`: CLI print/exit wrapper.
- `crates/cli/src/main.rs`, `crates/cli/src/blades.rs`, `crates/cli/src/bin/blade.rs`: host dispatch.

## Ownership Rules

- Keep target inference, manifest `[run]` handling, adapter selection, report paths, run caches, and watch inputs in `crates/kain-run`.
- Keep CLI files thin: parse through `kain-commands`, build a `RunRequest`, print a plan/report, set the exit code.
- Keep workspace/blade discovery in `crates/kain-blades`; do not rescan `apps/*`, `blades/*`, or `crates/*` in the CLI.
- Use `kain-fs` for run IO, paths, hashing, reports, and polling watchers.
- Use `kain-process::ProcessSpec` shapes for process-backed report metadata.
- Keep build artifacts under `.kain/build`, `.kain/cache/build`, and `.kain/reports/build`; run artifacts belong under `.kain/cache/run` and `.kain/reports/run`.

## Manifest Contract

`KAIN.toml` can include:

```toml
[run]
entry = "src/main.kn"
target = "auto"
args = ["--demo"]
cwd = "."
watch = ["src", "assets"]

[run.env]
KAIN_MODE = "dev"

[[platform.packages]]
name = "vulkan"
provider = "system"
```

Supported `target` values are `auto`, `kain`, `c`, `cargo`, `fabric`, `node`, and `bun`. CLI runtime args should override manifest args; manifest `env`, `cwd`, and `watch` should still apply to the resolved `RunUnit`.
`build.kn` / `platform.kn` platform package declarations are authoritative when present; equivalent `KAIN.toml` `[[platform.packages]]` entries should produce the same graph, and mismatches must show explicit provenance in run/build reports.

## Adapter Notes

- Kain source runs through `kain_driver::compile(..., CompileTarget::Interpret)`.
- Kain interpreter units must honor the resolved `RunUnit.cwd` and `RunUnit.env`
  before compiling/interpreting. Absolute `kain.exe run D:\...` launches need to
  work from arbitrary current directories, not only from the repo root.
- C files compile with Clang into `.kain/cache/run/c/<stem>-<hash>.exe` or platform equivalent, then execute without requiring the user to run a separate compile command.
- Cargo runs use `cargo run --manifest-path ...` with `CARGO_TARGET_DIR` under `.kain/cache/run/cargo`.
- Fabric manifests run through the existing host Fabric executor.
- Node and Bun adapters are process-backed and receive runtime args after `--`.
- Platform package locks are planned/imported by `crates/kain-run` before execution. Watch inputs should include `KAIN.toml`, `kain.toml`, `build.kn`, `platform.kn`, lockfiles, generated platform modules, binding reports, and inherited blade C/FFI inputs. Process-backed units receive `KAIN_PLATFORM_LOCKS` and `KAIN_PLATFORM_GENERATED_ROOTS` when generated packages are present.
- Repo-local MCP configs for this checkout must stay repo-relative:
  `command = "kain"`, `args = ["run", "blades/kain-mcp"]`, `cwd = "."`.
  Do not use `${KAIN_REPO_ROOT}` in repo `codex.config.toml` or `mcp.json`;
  Codex passes that placeholder literally in this lane.

## Validation

After changing this pipeline, run focused validation first:

```powershell
$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'
cargo fmt -p kain-run -p blade -p kain-commands -p cli
cargo test -p kain-run -p blade -p kain-commands --target-dir target\codex-kain-run -- --nocapture
cargo test -p kain-core type_env_registers_stdlib_registry_bridge_globals --target-dir target\codex-kain-run -- --nocapture
cargo check -p kain-run -p kain-commands -p cli --target-dir target\codex-kain-run
cargo build -p cli --target-dir target\codex-kain-run
```

Then smoke the actual binaries:

```powershell
target\codex-kain-run\debug\kain.exe run plan docs\examples\00_hello_and_cli.kn --json
target\codex-kain-run\debug\kain.exe run docs\examples\00_hello_and_cli.kn
target\codex-kain-run\debug\kain.exe watch docs\examples\00_hello_and_cli.kn --dry-run
target\codex-kain-run\debug\blade.exe run --help
```

For the C lane, use a tiny temporary `.c` file under `target/` and run:

```powershell
target\codex-kain-run\debug\kain.exe run <temp-file.c> --target c -- smoke-arg
```

If C execution fails, check `KAIN_CLANG_PATH`, `toolchain/llvm/bin/clang.exe`, and the host `clang` on `PATH`.

## Docs And Memory

For significant changes, update:

- `docs/cli/build-run-init.md`
- `docs/cli/cli-overview.md`
- `docs/reference/command-matrix.md`
- `ARCHITECTURE.md`
- `MEMORY.md`

Also update this skill when the run contract, adapters, cache/report layout, or validation flow changes.
