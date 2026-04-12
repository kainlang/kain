# Kain Crates Folder Guide

This is the human-friendly index for `crates/`.
Use `crates/repomap.md` when you need the full tree view.

## What Lives Here

- `browser` and `cli` are the end-user tooling and packaging lanes.
- `kain-core` and `kain-driver` are the core compiler and orchestration layers.
- `kain-host`, `kain-host-derive`, `kain-reflect`, and `kain-sdk` are the embeddable Rust host stack.
- `kain-c-ffi`, `kain-crate-ffi`, `kain-interop`, and `kain-gpu-runtime` cover runtime bridge and payload execution lanes.
- `kain-python`, `kain-node`, `kain-ui`, `kain-ui-native`, and `kain-3D` cover mixed-runtime and application materialization paths.
- `kain-import`, `kain-asm`, `kain-build`, `kain-selfhost`, and `kain-omni` cover importer, bootstrap, and orchestration workflows.
- `kain-sys-codegen` is the codegen scaffolding and backend support lane.
- `gpu`, `web`, `ue5`, and `unreal` hold target-specific codegen and asset/tooling surfaces.

## Current Doc Anchors

- [`repomap.md`](./repomap.md) for the crate tree.
- [`kain-gpu-runtime/README.md`](./kain-gpu-runtime/README.md) for the runtime executor crate.
- [`../guides/README.md`](../guides/README.md) for the canonical long-form guide tree.
- [`../guides/crates/index.md`](../guides/crates/index.md) for the crate family guide.
- [`../README.md`](../README.md) for the repo-level operating brief.
- [`../repomap.md`](../repomap.md) for the workspace overview.
- [`../runtime/native/C_RUNTIME_CONTRACT_PIPELINE.md`](../runtime/native/C_RUNTIME_CONTRACT_PIPELINE.md) for the native runtime contract lane.

## Notes

- `kain-gpu-runtime` is a runtime-side Vulkan executor, not a compiler backend.
- Keep this folder guide current when crates are added, renamed, or retired.
- Avoid reviving the old audit dump pattern; use the guide and map instead.
