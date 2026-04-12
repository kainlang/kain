# CLI Overview

Kain ships a modern subcommand CLI plus the older launcher-style entrypoint.

## Launchers

- `kain` is the explicit compiler-oriented launcher.
- `kn` is the run-first launcher. It defaults to interpret mode and shows a
  quick-start menu when invoked without input.

## Top-Level Commands

- `init`
- `lsp`
- `doctor`
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

## Global Flags

The root command supports:

- source input as a positional file
- inline source via `-c/--code`
- `-o/--output`
- `-t/--target`
- `--run`
- `--watch`
- `--emit-ast`
- `--emit-typed`
- `--verbose`
- `--plugin`
- `--plugins-dir`
- `--dry-run`
- `--strict`
- `--analyze`

## Command Selection Rule

If you want a one-file compile or run, use the root launcher. If you want a
packaging, import, selfhost, or repair workflow, use the corresponding
subcommand.

## Source Of Truth

The CLI surface is defined in:

- `crates/cli/src/main.rs`
- `crates/cli/src/lib.rs`
- `crates/cli/src/packager/`
- `crates/cli/src/import_*.rs`
- `crates/cli/src/selfhost.rs`
