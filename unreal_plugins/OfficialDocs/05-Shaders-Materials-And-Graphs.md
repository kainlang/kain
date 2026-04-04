# 05 Shaders Materials And Graphs

Kain's UE5 lane includes three related but distinct authoring systems:

- shader authoring
- material graph authoring
- graph editor and graph runtime authoring

Treat them as separate systems that can still work together inside one plugin.

## Shader Authoring

The `ue5-shaders` crate owns UE5 shader output.

Current shader families supported by the codegen layer:

- compute shaders
- fragment shaders
- vertex shaders
- surface shaders

Example:

```kain
shader compute NoiseGenerator(thread_id: Vec3):
    uniform grid_size: Int @0
    uniform noise_scale: Float @1
    buffer output: RWBuffer<Float> @2
```

The UE5 shader pipeline can generate:

- `.usf` source
- C++ shader wrapper classes
- dispatch helpers
- POD mirror structs for CPU-to-GPU data transfer
- shared `.ush` helper code for multi-shader plugins

## Shader Validation

The shader lane validates several failure modes before output:

- invalid thread group sizes
- duplicate binding slots
- invalid resource binding combinations
- type compatibility issues
- POD-invalid parameter shapes

## Current Shader Gotcha

Large local proof plugins document a current operational seam:

- complex projects may still require explicit shader registration or manifest maintenance
- do not assume every authored shader block is auto-harvested in every workflow

If your plugin is shader-heavy, verify its manifest and generated shader outputs carefully.

## Material Graph Authoring

The `ue5-materials` crate owns material graph lowering and asset generation.

The current implementation supports:

- parameter nodes
- texture sample nodes
- texture coordinate generation
- binary math operations
- larger material graph conversion from Kain AST to material IR
- binary `.uasset` generation for materials

## Graph Systems

The `ue5-graphs` crate covers two major systems:

- graph editor authoring
- graph runtime authoring

### Graph Editor

Graph editor authoring uses constructs such as:

- `@graph_editor`
- graph properties
- node type definitions
- pin definitions
- schema rules
- context actions
- validation rules

### Graph Runtime

Graph runtime generation adds execution-focused runtime classes and node data handling.

That makes Kain interesting for:

- dialogue tools
- quest tools
- material-style node systems
- gameplay or tool execution graphs

## Best Example Sources

Use these example folders when learning the lane:

- `unreal_plugins/Example_Shader`
- `unreal_plugins/Example_Material`
- `unreal_plugins/Example_Graph`
- `unreal_plugins/FluidFlow`
- `unreal_plugins/VoxelForgePro`

`FluidFlow/FluidDynamics.kn` is especially valuable because it shows one extreme of what this system can drive:

- many authored components
- multiple actors
- Blueprint-facing surfaces
- heavy compute shader volume
- large domain-specific plugin structure
