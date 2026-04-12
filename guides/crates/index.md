# Crates Index

This page is the human-friendly map of the workspace crates.

## How To Use This Page

- use it to find the right family before opening a crate-specific README
- use `crates/repomap.md` for the full machine-style tree
- use the family guides linked below for the high-signal explanation of each
  area

## Crate Families

| Family | Crates |
| --- | --- |
| Compiler core | `cli`, `kain-core`, `kain-driver`, `kain-import`, `kain-build`, `kain-asm`, `kain-repair`, `kain-selfhost`, `kain-omni`, `browser` |
| Embedding / host | `kain-host`, `kain-host-derive`, `kain-reflect`, `kain-sdk`, `kain-interop`, `kain-c-ffi`, `kain-crate-ffi`, `kain-python`, `kain-node`, `kain-sys-codegen`, `kain-fast3d-runtime`, `kain-gpu-runtime` |
| UI / 3D | `kain-ui`, `kain-ui-native`, `kain-3D`, `gpu`, `web` |
| UE5 / Unreal | `ue5`, `ue5-asset-utils`, `ue5-blueprints`, `ue5-config`, `ue5-editor`, `ue5-gas`, `ue5-graphs`, `ue5-materials`, `ue5-shaders`, `unreal` |

## Family Guides

- `compiler-core.md`
- `runtime-and-host.md`
- `ui-gpu-3d.md`
- `ue5-and-targets.md`

## Practical Rule

If you are unsure where a behavior lives, start at `kain-core` for language
meaning, `kain-driver` for orchestration/materialization, and `runtime/native`
for the native ABI floor.
