---
name: kain-command-platform
description: Use when adding, renaming, routing, documenting, validating, or debugging Kain CLI commands across crates/kain-commands and crates/cli, including kain/kn/blade command pack manifests, typed and dynamic Clap routers, registry metadata, runtime [[commands]] contributions, and command introspection.
---

# Kain Command Platform

## Start Here

Read the project root `ARCHITECTURE.md` and `MEMORY.md`, then inspect only the command files needed for the task:

- `crates/kain-commands/commands/index.toml`
- the relevant top-level pack under `crates/kain-commands/commands/*.toml`
- `crates/kain-commands/src/kain.rs`
- `crates/kain-commands/src/blade.rs`
- `crates/kain-commands/src/dynamic_clap.rs`
- `crates/kain-commands/src/registry.rs`
- `crates/kain-commands/src/runtime.rs`
- `crates/cli/src/main.rs`
- `crates/cli/src/bin/blade.rs`

## Ownership

`crates/kain-commands` owns command shape and command metadata: the built-in command pack index, flat top-level pack manifests, typed Clap routers, the dynamic Clap help/resolution builder, shared argument structs, aliases, bin exposure, registry serialization, conflict detection, launcher helpers, and runtime command contribution resolution.

`crates/cli` owns execution: invoking handlers, printing results, setting exit codes, and calling domain crates. Do not add a dependency from `kain-commands` back to `cli`.

Domain crates own behavior: `kain-driver`, `kain-build`, `blade`, `kain-check`, `kain-test`, `kain-repair`, `kain-repl`, `kain-omni`, `kain-codebase`, and similar crates should keep the real work.

Keep operator-facing binary provenance aligned with the managed sync lane. `kain doctor` now combines compile-time build tracking (`managed` vs `unmanaged`) with runtime sync-stamp drift data from `~/.kain/state/kain_sync_stamp.json`; do not invent parallel build-number logic in another command host.

`crates/cli/src/main.rs` now treats non-terminal stdout as machine-facing output and suppresses the human CLI banner there. Preserve that rule when touching CLI startup or command preambles; MCP, JSON, and other pipe-driven consumers must see protocol/data bytes first.

## Adding A Built-In Command

1. Pick or create the matching top-level command pack under `crates/kain-commands/commands/`.
2. Register new pack files in `crates/kain-commands/commands/index.toml`; keep pack files flat, not nested.
3. Add or update command metadata in the selected pack TOML.
4. Preserve the pack boundary: `import.toml` owns import commands, `blade.toml` owns Kain blade plus standalone `blade.exe` commands, `run.toml` owns run/watch commands, `runtime.toml` owns native runtime build/validate operator commands, and `unreal.toml` owns UE5-facing command entries.
5. Add or update the typed Clap shape in `crates/kain-commands/src/kain.rs` or `src/blade.rs` when the command is executable today.
6. Put shared argument structs in a focused `kain-commands` module only when more than one router uses them.
7. Wire execution in `crates/cli/src/main.rs` or `crates/cli/src/bin/blade.rs`, usually by calling an existing domain crate or a thin `crates/cli/src/*.rs` host module.
8. Update `docs/reference/command-matrix.md` and `docs/cli/cli-overview.md` when the public surface changes.

## Dynamic Clap And Registry Views

`crates/kain-commands/src/dynamic_clap.rs` builds a Clap tree from `CommandRegistry`. Use it for registry-backed help, previews, docs, completion experiments, and future dynamic dispatch. Typed Clap routers still own built-in execution today, so do not expose a visible built-in command path in TOML unless the host can execute it or the path is intentionally metadata-only and documented that way.

Useful smoke commands:

```powershell
target\codex-kain-command-packs-cli\debug\kain.exe commands packs
target\codex-kain-command-packs-cli\debug\kain.exe commands packs --json
target\codex-kain-command-packs-cli\debug\kain.exe commands list --bin kain
target\codex-kain-command-packs-cli\debug\kain.exe commands help --bin kain
target\codex-kain-command-packs-cli\debug\kain.exe runtime build --help
target\codex-kain-command-packs-cli\debug\kain.exe runtime validate --help
target\codex-kain-command-packs-cli\debug\kain.exe commands help --bin blade
```

## Runtime Command Contributions

Runtime bolt-ons use `[[commands]]` records in KAIN-style manifests. The CLI discovers manifests through the blade workspace resolver, then `kain_commands::runtime` merges them with the built-in registry.

Rules:

- Do not scan random folders for command manifests; use the blade resolver.
- Built-in commands win path conflicts.
- Duplicate runtime paths are errors.
- Runtime records may include `tags` and `args` for dynamic help/documentation.
- Runtime command matching is available as a fallback, but dynamic handler execution is v1-recognized and fails clearly until a real handler bridge is added.

Example:

```toml
[[commands]]
id = "my_blade.sharpen"
bins = ["kain"]
path = ["sharpen"]
about = "Sharpen this blade"
handler = "blade:my_blade:sharpen"
```

## Validation

Run the narrow command-platform tests first:

```powershell
cargo test -p kain-commands --target-dir target\codex-kain-commands -- --nocapture
cargo check -p cli --target-dir target\codex-kain-commands-cli
```

For executable proof, build the host CLI and smoke the help/registry paths:

```powershell
$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY = "1"
cargo build -p cli --target-dir target\codex-kain-commands-cli
target\codex-kain-commands-cli\debug\kain.exe --help
target\codex-kain-commands-cli\debug\kn.exe --help
target\codex-kain-commands-cli\debug\blade.exe --help
target\codex-kain-commands-cli\debug\kain.exe commands packs
target\codex-kain-commands-cli\debug\kain.exe commands list --bin kain
target\codex-kain-commands-cli\debug\kain.exe commands help --bin kain
target\codex-kain-commands-cli\debug\kain.exe runtime build --help
target\codex-kain-commands-cli\debug\kain.exe runtime validate --skip-cli-build --skip-runtime-build --skip-fixtures --skip-conformance
target\codex-kain-commands-cli\debug\kain.exe commands export --bin blade
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\windows\sync-kain-source-of-truth.ps1 -ManagedSync
kain doctor
```

When changing blade build commands, also run `python labs\blades_workspace_smoke\scripts\run_blades_smoke.py` with `KAIN_BIN` and `BLADE_BIN` pointed at the freshly built binaries.
