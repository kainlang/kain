# Material Pipeline Roadmap - What's Left

## Current Status: Phase 2 Complete ✅

**What works:**
- Parser parses `@material_graph` syntax
- AST types represent materials
- MaterialGraph IR exists
- C++ factory generator produces code
- Files are generated and integrated

**What's broken:**
- Expressions aren't converted to nodes (just debug strings)
- No actual node graph construction
- Custom HLSL nodes not supported yet

---

## Phase 3: Make It Actually Work (CRITICAL)

### 3.1: Fix Expression → Node Conversion (2-3 hours)

**Problem:** Right now expressions like `base_tint * 2.0` are just stored as debug strings, not actual material nodes.

**Solution:** Integrate the AST converter properly:

```rust
// In packager ue5_pipeline.rs
use ue5_materials::MaterialGraphConverter;

let mut converter = MaterialGraphConverter::new();
let material_graph = converter.convert(&ast_material_def)?;
// Now material_graph.nodes is populated with actual nodes!
```

**What this fixes:**
- ✅ Expressions become real material nodes
- ✅ Let bindings create intermediate nodes
- ✅ Node IDs are generated correctly
- ✅ Connections are wired automatically

**Estimated time:** 2-3 hours (mostly testing)

---

### 3.2: Add Custom HLSL Node Support (1-2 hours)

**What you need:** Ability to write raw HLSL in materials.

**KAIN Syntax:**
```kn
@material_graph
material CustomShaderMaterial:
    input base_color: Vec3 = vec3(1, 1, 1)
    input time: Float = 0.0
    
    let custom_effect = custom_hlsl("""
        float3 result = BaseColor;
        float wave = sin(Time * 3.14159);
        result *= wave;
        return result;
    """, inputs: [base_color, time], output_type: Vec3)
    
    output base_color = custom_effect
```

**Implementation:**

1. Add `CustomHLSL` node type to MaterialNodeType:
```rust
pub enum MaterialNodeType {
    // ... existing types
    CustomHLSL {
        code: String,
        inputs: Vec<String>,  // node IDs
        output_type: String,  // "float", "float3", etc.
        description: String,
    },
}
```

2. Add parser support for `custom_hlsl()` function

3. Generate `UMaterialExpressionCustom` in factory:
```cpp
UMaterialExpressionCustom* CustomNode = NewObject<UMaterialExpressionCustom>(Material);
CustomNode->Code = TEXT("your HLSL code here");
CustomNode->OutputType = CMOT_Float3;
CustomNode->Description = TEXT("Custom Effect");

// Add inputs
FCustomInput Input;
Input.InputName = TEXT("BaseColor");
CustomNode->Inputs.Add(Input);
```

**Estimated time:** 1-2 hours

---

### 3.3: Shader Integration (2-3 hours)

**What you need:** Call your existing KAIN shaders from materials.

**KAIN Syntax:**
```kn
// Your existing shader
shader fragment MyEffect(uv: Vec2) -> Vec4:
    uniform intensity: Float @0
    let wave = sin(uv.y * 10.0)
    return vec4(wave, wave, wave, 1.0)

// Use it in a material
@material_graph
material ShaderMaterial:
    input intensity: Float = 1.0
    
    let effect = call_shader(MyEffect, intensity: intensity)
    
    output emissive = effect
```

**Implementation:**

1. Detect shader references in material graphs
2. Generate Custom HLSL node that includes shader code
3. Wire shader parameters to material inputs

**Estimated time:** 2-3 hours

---

## Phase 4: Texture Support (3-4 hours)

**What you need:** Sample textures in materials.

**KAIN Syntax:**
```kn
@material_graph
material TexturedMaterial:
    input albedo_map: Texture2D
    input normal_map: Texture2D
    input tiling: Vec2 = vec2(1, 1)
    
    let uv = texture_coordinate(0, tiling)
    let albedo = sample(albedo_map, uv)
    let normal = sample(normal_map, uv)
    
    output base_color = albedo.rgb
    output normal = normal.rgb
```

**Implementation:**

1. Add `TextureSample` node generation
2. Add `TextureCoordinate` node generation
3. Handle texture parameters in factory
4. Wire UV connections

**Estimated time:** 3-4 hours

---

## Phase 5: Advanced Features (Optional)

### 5.1: Material Functions (4-5 hours)
Reusable material node graphs:

```kn
@material_function
fn color_grading(color: Vec3, saturation: Float) -> Vec3:
    let gray = dot(color, vec3(0.299, 0.587, 0.114))
    return lerp(vec3(gray), color, saturation)

@material_graph
material GradedMaterial:
    input base_color: Vec3 = vec3(1, 1, 1)
    input saturation: Float = 1.0
    
    let graded = color_grading(base_color, saturation)
    output base_color = graded
```

### 5.2: Dynamic Material Instances (2-3 hours)
Runtime parameter control:

```kn
actor MyActor:
    state mesh: StaticMeshComponent = StaticMeshComponent()
    state material: MyMaterial = MyMaterial()
    
    on BeginPlay():
        mesh.SetMaterial(0, material)
        material.set_intensity(2.0)  // Runtime control
```

### 5.3: Material Layers (5-6 hours)
Blend multiple materials:

```kn
@material_graph
material LayeredMaterial:
    input base_layer: Material
    input detail_layer: Material
    input blend_mask: Texture2D
    
    let blended = lerp(base_layer, detail_layer, blend_mask)
    output base_color = blended
```

---

## Total Time Estimates

### To Get Custom Nodes Working (Your Immediate Need):
- **Phase 3.1:** Fix expression conversion (2-3 hours)
- **Phase 3.2:** Add custom HLSL nodes (1-2 hours)
- **Phase 3.3:** Shader integration (2-3 hours)
- **Total:** 5-8 hours

### To Get Full Material System:
- **Phase 3:** Core functionality (5-8 hours)
- **Phase 4:** Texture support (3-4 hours)
- **Phase 5:** Advanced features (11-14 hours, optional)
- **Total:** 8-26 hours depending on features needed

---

## What You Can Do RIGHT NOW

### Option 1: Use Custom HLSL Directly (Workaround)

You can manually create materials with Custom nodes in UE5 and reference them from KAIN actors:

```kn
actor MyActor:
    state mesh: StaticMeshComponent = StaticMeshComponent()
    
    on BeginPlay():
        // Load material created in UE5
        let mat = LoadObject("/Game/Materials/M_CustomShader")
        mesh.SetMaterial(0, mat)
        
        // Set parameters
        let dyn_mat = mesh.CreateDynamicMaterialInstance(0, mat)
        dyn_mat.SetScalarParameterValue("Intensity", 2.0)
```

**Pros:** Works immediately  
**Cons:** Still requires manual UE5 work

### Option 2: Wait for Phase 3.2 (1-2 hours)

I can add custom HLSL node support right now. It's actually pretty straightforward:

1. Add `custom_hlsl()` function to parser
2. Add `CustomHLSL` node type to IR
3. Generate `UMaterialExpressionCustom` in factory

**This would let you write:**
```kn
@material_graph
material MyShader:
    input intensity: Float = 1.0
    
    let effect = custom_hlsl("""
        return BaseColor * Intensity;
    """, inputs: [base_color, intensity])
    
    output emissive = effect
```

---

## Recommendation

**For your immediate plugin needs:**

1. **Quick win (1-2 hours):** Add custom HLSL node support
   - This unblocks you immediately
   - You can write any HLSL you need
   - Works with existing material system

2. **Follow-up (2-3 hours):** Fix expression conversion
   - Makes the system more robust
   - Enables complex material graphs
   - Better for LLM generation

3. **Later (3-4 hours):** Add texture support
   - When you need texture sampling
   - Not critical for custom shaders

**Total to unblock you:** 1-2 hours for custom HLSL nodes

---

## Want Me To Add Custom HLSL Nodes Now?

I can add support for `custom_hlsl()` in 1-2 hours:

```kn
@material_graph
material CustomEffect:
    input base_color: Vec3 = vec3(1, 1, 1)
    input time: Float = 0.0
    
    let effect = custom_hlsl("""
        float wave = sin(Time * 3.14159);
        return BaseColor * wave;
    """, 
    inputs: [base_color, time],
    output_type: "float3")
    
    output emissive = effect
```

This would generate a `UMaterialExpressionCustom` node with your HLSL code, fully wired up and ready to use.

**Say the word and I'll build it!** 🔥
