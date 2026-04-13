# UE5 And Target Adapter Crates

These crates cover the Unreal Engine 5 pipeline and target-adapter surfaces.

## UE5 Family

- `ue5`
- `ue5-asset-utils`
- `ue5-blueprints`
- `ue5-config`
- `ue5-editor`
- `ue5-gas`
- `ue5-graphs`
- `ue5-materials`
- `ue5-shaders`

## Unreal Integration

- `unreal`

## What This Family Covers

- plugin generation and module layout
- module graph inference and Build.cs dependency shaping
- Oracle validation and UE5-specific semantic checks
- materials, shaders, and graphs
- gameplay ability system artifacts
- editor-facing integration and config

For the conceptual UE5 authoring model, see
[guides/ue5/overview.md](/home/ephemara/Dev/Kain/guides/ue5/overview.md).

## Rule

UE5 support is a target-adapter surface. The language meaning still lives in
`kain-core`. Use [guides/ue5/overview.md](/home/ephemara/Dev/Kain/guides/ue5/overview.md)
for the conceptual UE5 lane and [guides/cli/native-ui-and-packaging.md](/home/ephemara/Dev/Kain/guides/cli/native-ui-and-packaging.md)
for the command-line packaging path.
