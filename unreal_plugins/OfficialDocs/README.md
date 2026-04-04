# Kain UE5 Official Docs

This folder is the product-facing documentation set for authoring Unreal Engine 5 plugins with Kain.

Kain should be understood here as a UE5-focused DSL plus code generation pipeline:

- you author `.kn` source
- Kain parses, type-checks, validates, and lowers it
- the UE5 backend and packager generate C++, shaders, assets, module files, and plugin scaffolding

These docs are grounded in the current implementation across:

- `crates/ue5`
- `crates/ue5-editor`
- `crates/ue5-shaders`
- `crates/ue5-materials`
- `crates/ue5-blueprints`
- `crates/ue5-graphs`
- `crates/ue5-gas`
- `crates/ue5-config`
- `crates/cli`
- `unreal_plugins/*`

## What Kain Can Do For UE5 Today

At a high level, the current UE5 pipeline supports:

- gameplay actors, components, subsystems, structs, enums, and delegates
- replication and RPC-oriented code generation
- Blueprint-callable and Blueprint-event surfaces
- editor tooling: Slate, Details panels, toolbars, editor modules, and viewports
- compute, fragment, vertex, and surface shader authoring for UE5 shader output
- material graph generation
- graph editor and graph runtime generation
- developer settings and config generation
- non-destructive injection into existing UE5 plugins
- optional migration helpers through `import-rust`, `import-ts`, and `import-c`

Important current nuance:

- the main UE5 runtime backend is production-strength
- some adjacent lanes still have partial or staged integration
- these docs call out those limits directly instead of flattening everything into "fully supported"

## Recommended Reading Order

1. `01-Getting-Started.md`
2. `02-KAIN-TOML-And-Project-Layout.md`
3. `03-Language-To-UE5-Mapping.md`
4. `04-Editor-UI-And-Tools.md`
5. `05-Shaders-Materials-And-Graphs.md`
6. `06-Blueprints-GAS-And-Config.md`
7. `07-Imports-Injection-And-Migration.md`
8. `08-Examples-Feature-Matrix-And-Limits.md`

## Core Commands

These are the commands most relevant to the UE5 workflow:

```powershell
kain build --ue5
kain inject <FILES...> --ue5
kain build <file.kn> --target ue5
kain build <file.kn> --target ue5editor
kain doctor
kain import-rust <path>
kain import-ts <path>
kain import-c <path>
```

## How The UE5 Pipeline Is Split

The current ownership model matters when you are debugging or extending the system:

- `crates/kain-core`
  Owns parsing, typing, common language semantics, and frontend truth.
- `crates/ue5`
  Owns primary UE5 runtime C++ code generation.
- `crates/ue5-editor`
  Owns editor-only authoring surfaces such as Slate and Details.
- `crates/ue5-shaders`
  Owns `.usf` shader generation and shader-side C++ helpers.
- `crates/ue5-materials`
  Owns material graph IR and binary material asset output.
- `crates/ue5-graphs`
  Owns graph editor and graph runtime systems.
- `crates/ue5-config`
  Owns developer settings and config-oriented code generation.
- `crates/ue5-gas`
  Owns Gameplay Ability System code generation, with mixed maturity by phase.
- `crates/cli`
  Owns the UE5 packaging pipeline, config loading, modular output routing, and `kain inject`.

## Proof Surface

The strongest repo-local proof sources live under `unreal_plugins/`.

Useful starting examples:

- `Example_Comprehensive`
- `Example_Blueprint`
- `Example_Material`
- `Example_Shader`
- `Example_Slate`
- `Example_Graph`
- `Example_GAS`
- `FluidFlow`
- `VoxelForgePro`
- `Cinema4DMograph`
- `TemporalBlueprint`
- `MetaFitter`

## Scope Of These Docs

These docs are for:

- engineers learning how to write UE5 plugins in Kain
- teams evaluating Kain as a UE5 codegen workflow
- future maintainers documenting the current UE5 lane honestly

These docs are not trying to explain the entire Kain ecosystem outside the UE5 slice unless it directly affects the UE5 workflow.
