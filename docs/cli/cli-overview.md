# CLI Overview

Kain ships a modern subcommand CLI plus the older launcher-style entrypoint.
The command surface is live and source-driven; do not rely on stale README
snippets when the binary help output disagrees.

## Launchers

- `kain` is the explicit compiler-oriented launcher.
- `kn` is the run-first launcher. It defaults to interpret mode and shows a
  quick-start menu when invoked without input.
- `kn` also accepts the same subcommands as `kain`; the difference is launch
  bias, not language semantics.

## Top-Level Commands

- `init`
- `lsp`
- `doctor`
- `check`
- `test`
- `selfhost`
- `omni`
- `fabric`
- `commands`
- `build`
- `run`
- `watch`
- `gpu-artifacts`
- `inject`
- `import-asm`
- `import-c`
- `import-rust`
- `import-crate`
- `import-ts`
- `help`

## Command Registry

Use `kain commands list --bin kain|kn|blade` to inspect the command registry and
`kain commands export --bin kain|kn|blade` for JSON. Add `--runtime` to include
`[[commands]]` contributions discovered from the current blade workspace.

## Global Flags

The root command supports:

- source input as a positional file
- inline source via `-c/--code`
- `-o/--output`
- `-t/--target`
- `-r/--run`
- `-w/--watch`
- `--emit-ast`
- `--emit-typed`
- `-v/--verbose`
- `--plugin`
- `--plugins-dir`
- `--dry-run`
- `--strict`
- `--analyze`
- `-h/--help`
- `-V/--version`

## Command Selection Rule

If you want a one-file compile or run, use the root launcher. If you want a
packaging, import, orchestration, or repair workflow, use the corresponding
subcommand. Use `reference/command-matrix.md` when you need the exact flag
table and `reference/target-matrix.md` when you need the target aliases.

Use `kain run`, `kain run dev`, `kain run plan`, or `kain watch` for the
unified runtime loop. That path is owned by `crates/kain-run` and supports Kain
source, C files, Cargo crates, Fabric manifests, Node, Bun, blades, and
workspace `[run]` metadata.

## Source Of Truth

The CLI surface is defined in:

- `crates/kain-commands/commands/kain.toml`
- `crates/kain-commands/commands/blade.toml`
- `crates/kain-commands/src/kain.rs`
- `crates/kain-commands/src/blade.rs`
- `crates/kain-commands/src/registry.rs`
- `crates/kain-commands/src/runtime.rs`
- `crates/kain-run/src/lib.rs`
- `crates/cli/src/main.rs` for host dispatch and handler execution
- `crates/cli/src/run.rs` for CLI printing and exit-code handling around
  `kain-run`
- `crates/kain-check/src/lib.rs`
- `crates/kain-test/src/lib.rs`
- `crates/cli/src/packager/`
- `crates/cli/src/import_*.rs`
