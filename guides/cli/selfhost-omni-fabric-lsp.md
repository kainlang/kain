# Selfhost, Omni, Fabric, And LSP

These commands cover the orchestration and language-service lanes. The command
pages are operational docs; the conceptual pipeline pages live under
`guides/pipelines/`.

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

`phase1` emits the initial mirror and bundle graph for `kain-core` and
`kain-import`. `phase2` adds round-trip Rust emission plus stage2 assembly and
build options, with `cli` treated as the executable-parity gate before backend
expansion.

## `omni`

`kain omni init`
`kain omni build`

Flags:

- `--manifest`

Omni is the mixed-language orchestration lane.
The conceptual pipeline page is
[guides/pipelines/omni.md](/home/ephemara/Dev/Kain/guides/pipelines/omni.md).
Read [guides/pipelines/omni.md](/home/ephemara/Dev/Kain/guides/pipelines/omni.md)
for the manifest and staged-import model.

## `fabric`

`kain fabric init`
`kain fabric validate`
`kain fabric run`

Flags:

- `--template local|polyglot`
- `--manifest`

Fabric is the local-first polyglot manifest lane.
The conceptual pipeline page is
[guides/pipelines/fabric.md](/home/ephemara/Dev/Kain/guides/pipelines/fabric.md).
Read [guides/pipelines/fabric.md](/home/ephemara/Dev/Kain/guides/pipelines/fabric.md)
for the runtime kinds, contracts, and report model.

## `lsp`

`kain lsp` starts the language server.

## Practical Rule

Use `selfhost` when you want the workspace mirrored into Kain, `omni` when you
want mixed-language orchestration, and `fabric` when you want a local-first
execution manifest.
