# kain-import

`kain-import` is the source-ingestion crate for KAIN. Right now its production path is the C importer.

For the actual workflow and current readiness level, read:

- [C_IMPORT_PIPELINE.md](./C_IMPORT_PIPELINE.md)
- [IMPLEMENTATION_STATUS.md](./IMPLEMENTATION_STATUS.md)

## Current Scope

- C import is implemented and wired into the CLI.
- Directory import, per-file module wrapping, flat merge mode, and failure-report JSON are supported.
- Rust/C++/Python importers are still planned, not active.

## Fast Start

```bash
# Single file -> KAIN
kain.exe import-c .\physics.c --output .\physics.kn

# Single file -> KAIN -> TypeScript
kain.exe import-c .\physics.c --output .\physics.kn --target ts

# Directory import with partial-failure reporting
kain.exe import-c .\src --output .\game.kn --report-json .\game.import_report.json
```

## Library API

```rust,no_run
use std::path::Path;

let program = kain_import::import_c(Path::new("runtime.c"))?;
# Ok::<(), Box<dyn std::error::Error>>(())
```

## Notes

- The CLI is the main supported workflow for generated `.kn` artifacts.
- The crate can successfully import and emit large C subsets, but it is not yet a full C build-system replacement.
- The importer is improving toward self-hosting and large decompilation targets, but memory-model fidelity is still the main frontier.
