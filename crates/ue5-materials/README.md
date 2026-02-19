# UE5 Materials Crate

Automatic UE5 material generation from KAIN material graphs. Generates C++ factory code that creates materials at Editor startup.

## Features

- **Material Graph IR** - Intermediate representation for material node graphs
- **C++ Factory Generation** - Produces UE5 C++ code that creates materials at runtime
- **16+ Node Types** - Texture samples, parameters, math ops, constants, and more
- **Material Properties** - Blend modes, shading models, two-sided, etc.
- **Auto-wiring** - Connections between nodes handled automatically
- **Content Folder Management** - Creates `Content/Materials/` directory structure

## Architecture

```
KAIN Source → Parser → MaterialGraph IR → C++ Factory Generator → UE5 C++
```

### MaterialGraph IR

The intermediate representation that describes a material:

```rust
pub struct MaterialGraph {
    pub name: String,
    pub inputs: Vec<MaterialInput>,
    pub nodes: Vec<MaterialNode>,
    pub outputs: MaterialOutputs,
    pub properties: MaterialProperties,
}
```

### Supported Node Types

- **Parameters**: ScalarParameter, VectorParameter, ColorParameter
- **Texture**: TextureSample, TextureCoordinate
- **Math**: Multiply, Add, Subtract, Divide, Power, Clamp
- **Interpolation**: Lerp, Fresnel
- **Vector**: Dot, ComponentMask, Append
- **Constants**: ConstantFloat, ConstantVec3, ConstantVec4

### Material Properties

```rust
pub struct MaterialProperties {
    pub domain: MaterialDomain,        // Surface, DeferredDecal, etc.
    pub blend_mode: BlendMode,         // Opaque, Translucent, etc.
    pub shading_model: ShadingModel,   // DefaultLit, Unlit, etc.
    pub two_sided: bool,
}
```

## Usage Example

### Building a Material Graph Programmatically

```rust
use ue5_materials::*;

let mut builder = MaterialNodeBuilder::new();

// Create texture parameter
let albedo_tex = builder.texture_sample(None, None, -400, 0);

// Create scalar parameters
let roughness = builder.scalar_param("Roughness", 0.5, -400, 100);
let metallic = builder.scalar_param("Metallic", 0.0, -400, 200);

// Create tint color parameter
let tint = builder.vector_param("TintColor", [1.0, 1.0, 1.0], -400, 300);

// Multiply albedo by tint
let tinted = builder.multiply(&albedo_tex, &tint, -200, 0);

// Build the graph
let mut graph = MaterialGraph::new("MyMaterial".to_string());
graph.nodes = builder.build();
graph.outputs.base_color = Some(tinted);
graph.outputs.roughness = Some(roughness);
graph.outputs.metallic = Some(metallic);
graph.properties.blend_mode = BlendMode::Opaque;
graph.properties.shading_model = ShadingModel::DefaultLit;
```

### Generating C++ Factory Code

```rust
use ue5_materials::MaterialFactoryGenerator;

let generator = MaterialFactoryGenerator::new("MyPlugin".to_string());

// Generate header
let header = generator.generate_factory_header(&[graph.clone()]);
std::fs::write("MaterialFactories.h", header)?;

// Generate implementation
let cpp = generator.generate_factory_cpp(&[graph]);
std::fs::write("MaterialFactories.cpp", cpp)?;
```

### Generated C++ Output

The factory generator produces code like:

```cpp
void FMyPluginMaterialFactory::Generate_MyMaterial()
{
    FString PackageName = TEXT("/MyPlugin/Materials/M_MyMaterial");
    UPackage* Package = CreatePackage(*PackageName);
    Package->FullyLoad();
    
    UMaterial* Material = NewObject<UMaterial>(Package, TEXT("M_MyMaterial"), RF_Public | RF_Standalone);
    
    // Create nodes
    UMaterialExpressionTextureSample* node_0 = NewObject<UMaterialExpressionTextureSample>(Material);
    node_0->MaterialExpressionEditorX = -400;
    node_0->MaterialExpressionEditorY = 0;
    Material->GetExpressionCollection().AddExpression(node_0);
    
    UMaterialExpressionScalarParameter* node_1 = NewObject<UMaterialExpressionScalarParameter>(Material);
    node_1->ParameterName = TEXT("Roughness");
    node_1->DefaultValue = 0.5f;
    // ... more nodes
    
    // Wire connections
    node_3->A.Expression = node_0;
    node_3->B.Expression = node_2;
    
    // Connect to material outputs
    Material->GetEditorOnlyData()->BaseColor.Expression = node_3;
    Material->GetEditorOnlyData()->Roughness.Expression = node_1;
    
    // Set material properties
    Material->BlendMode = BLEND_Opaque;
    Material->ShadingModel = MSM_DefaultLit;
    
    // Compile and save
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    UPackage::SavePackage(Package, Material, *PackageFileName, SaveArgs);
    
    FAssetRegistryModule::AssetCreated(Material);
}
```

### Module Integration

The factory is called from the plugin's module startup:

```cpp
void FMyPluginModule::StartupModule()
{
    if (GIsEditor && !IsRunningCommandlet())
    {
        FMyPluginMaterialFactory::GenerateMaterials();
    }
}
```

## Integration with KAIN Pipeline

The `cli` crate's packager calls this crate during `kain build --ue5`:

```rust
use ue5_materials::{MaterialGraph, MaterialFactoryGenerator};

// In packager.rs
let material_graphs = extract_material_graphs_from_ast(&typed_ast);
material_gen::generate_material_factories(plugin_name, &material_graphs, output_dir)?;
```

## Future Enhancements

### Phase 2: Material Instances (unreal_asset)
- Generate `.uasset` files directly at compile time
- No Editor restart needed for instances
- Instant parameter overrides

### Phase 3: KAIN Syntax
```kn
@material_graph
material HologramMaterial:
    input glow_intensity: Float = 1.0
    input glow_color: Vec3 = vec3(0, 1, 1)
    
    let sample = sample(albedo, uv)
    let glow = sample.rgb * glow_color * glow_intensity
    
    output base_color = glow
    output emissive = glow * 2.0
```

### Phase 4: Advanced Nodes
- Custom HLSL nodes
- Material functions
- Dynamic material instances
- Blueprint material parameter control

## Testing

```bash
cargo test --package ue5-materials --lib
```

All tests validate:
- Factory header generation
- Node code generation
- Connection wiring
- Material property settings

## Dependencies

- `kain-core` - AST types
- `serde` - Serialization
- `indoc` - Clean multiline strings

## License

MIT
