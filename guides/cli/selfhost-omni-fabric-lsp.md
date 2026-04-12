# Selfhost, Omni, Fabric, And LSP

These commands cover the orchestration and language-service lanes.

## `selfhost`

`kain selfhost phase1`
`kain selfhost phase2`

Shared flags:

- `--inventory-dir`
- `--output-dir`
- `--profile-path`
- `--emit-bundles`
- `--all-crates`
- `--force`

Phase 2 also supports:

- `--emit-roundtrip-rust`
- `--assemble-stage2`
- `--build-stage2`

`--all-crates` discovers the live workspace crate set instead of relying only on
the profile slice.

## `omni`

`kain omni init`
`kain omni build`

Flags:

- `--manifest`

Omni is the mixed-language orchestration lane.

## `fabric`

`kain fabric init`
`kain fabric validate`
`kain fabric run`

Flags:

- `--template local|polyglot`
- `--manifest`

Fabric is the local-first polyglot manifest lane.

## `lsp`

`kain lsp` starts the language server.

## Practical Rule

Use `selfhost` when you want the workspace mirrored into Kain, `omni` when you
want mixed-language orchestration, and `fabric` when you want a local-first
execution manifest.
