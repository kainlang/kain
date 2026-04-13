# Importers

Snapshot: April 12, 2026.

Kain's importers translate foreign source into Kain source or direct target
artifacts. They are first-class workflows, not utility scripts.

## Shared Rules

Across the current importers:

- directory input is recursive by default
- `--flat` collapses nested module structure into a flatter output tree
- `--include` and `--exclude` are path-fragment filters
- `--report-json` writes a machine-readable import report
- `-t/--target` switches from source emission to direct target compilation
- `-o/--output` controls where generated `.kn` or target artifacts go

If you give a directory and no `--output`, the importer writes next to the
source tree by default. If you give a single file and no `--output`, the importer
usually keeps the result in memory unless you also ask for a target write.

## `import-asm`

`kain import-asm <input> [--format NAME] [--out FILE] [--validate-only]`

This lane exists for legacy assembly transliteration and recovery reporting.
Supported dialect IDs include:

- `6502-furby`
- `lr35902-gameboy`
- `z80`

Useful aliases exist for the same dialect families, but the docs should always
name the canonical IDs first.

Outputs can include:

- the canonicalized ASM path
- the generated Kain path
- a map JSON sidecar
- a recovery report JSON

`--validate-only` suppresses the canonical ASM and Kain writes, but it still
produces the report JSON so you can inspect the parse and recovery result.

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

This importer consumes a C file or directory plus preprocessor include paths and
defines. It emits a Kain `Program`, can optionally write a `.kn` file, can
optionally compile directly to a target, and produces a report JSON.

The CLI wrapper intentionally does not expose arbitrary custom C++ preprocessing
knobs here. Docs should not promise more preprocessing control than the command
actually exposes.

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

The Rust importer preserves directory structure unless `--flat` is enabled. It
also surfaces lossy-lowering diagnostics and repair hints, because not every
Rust construct maps cleanly to Kain.

The report JSON includes:

- `lossy_diagnostics`
- `diagnostics_by_class`
- file-level `repair_hint` entries

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

This path goes through the crate FFI layer and is Cargo-aware. It can generate:

- a canonical Kain module
- a prelude `.kn`
- report JSON and human-readable reports
- a bridge `Cargo.toml`
- a bridge `src/lib.rs`
- an optional live bridge library

`Both` is the default mode. The parser also accepts `gen` as a code-only alias
for generate mode.

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

This importer walks `ts`, `tsx`, `mts`, and `cts` files and skips common build
output directories such as `_out`, `_single_out`, `_batch_out`, `dist`,
`build`, and `node_modules`. It also skips generated files with `.generated.`
in the path.

The importer produces a Kain `Program`, can write `.kn`, can compile directly to
the requested target, and reports counts for functions, structs, enums, impls,
and type aliases.

## Lower-Level USF Import

There is also a lower-level USF import pipeline in the source tree. Treat it as
research-oriented unless the live CLI explicitly promotes it to a first-class
command. Do not confuse it with the canonical `usf` compile target.

## Importer Rule

Importers are not just file converters. They are one way Kain grows from foreign
source into canonical Kain source or target artifacts.
