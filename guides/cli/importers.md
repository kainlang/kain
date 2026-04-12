# Importers

Kain's importers translate foreign source into Kain source or direct target
artifacts.

## `import-asm`

`kain import-asm <input> [--format NAME] [--out FILE] [--validate-only]`

Use this for legacy assembly transliteration. The default format is
`6502-furby`.

## `import-c`

`kain import-c <input> [options]`

Flags:

- `-o/--output`
- `-t/--target`
- `-I/--include-paths`
- `-D/--defines`
- `--flat`
- `--include`
- `--exclude`
- `--fail-fast`
- `--report-json`

It can emit `.kn`, compile directly, or do both.

## `import-rust`

`kain import-rust <input> [options]`

Flags:

- `-o/--output`
- `-t/--target`
- `--flat`
- `--include`
- `--exclude`
- `--fail-fast`
- `--report-json`

The Rust importer preserves directory structure unless flat mode is enabled.

## `import-crate`

`kain import-crate <crate_name> [options]`

Flags:

- `--manifest-path`
- `--crate-path`
- `--mode live|generate|both`
- `-o/--output`
- `--report-json`
- `--features`
- `--all-features`
- `--no-default-features`

This path goes through the crate FFI layer and can also create a live bridge.

## `import-ts`

`kain import-ts <input> [options]`

Flags mirror the source importers:

- `-o/--output`
- `-t/--target`
- `--flat`
- `--include`
- `--exclude`
- `--fail-fast`
- `--report-json`

It walks `ts`, `tsx`, `mts`, and `cts` files and skips common build output
directories.

## Importer Rule

Importers are not just file converters. They are one way Kain grows from foreign
source into canonical Kain source or target artifacts.
