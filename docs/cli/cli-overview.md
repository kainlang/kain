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
- `build`
- `run`
- `gpu-artifacts`
- `inject`
- `import-asm`
- `import-c`
- `import-rust`
- `import-crate`
- `import-ts`
- `help`

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

## Source Of Truth

The CLI surface is defined in:

- `crates/cli/src/main.rs`
- `crates/cli/src/lib.rs`
- `crates/kain-check/src/lib.rs`
- `crates/kain-test/src/lib.rs`
- `crates/cli/src/packager/`
- `crates/cli/src/import_*.rs`
- `crates/cli/src/selfhost.rs`
