# Material System - NUCLEAR POWER MODE 🚀⚡

> Making the material system so powerful that LLMs can generate AAA-quality materials in seconds

---

## Current Status: Phase 2 Complete ✅

**What works:**
- ✅ Parser parses `@material_graph` syntax
- ✅ Basic node types (math, parameters, constants)
- ✅ C++ factory generator
- ✅ Auto-generation at Editor startup
- ✅ 8 example materials

**What's missing:**
- ❌ Expression → Node conversion (expressions are debug strings)
- ❌ Custom HLSL nodes
- ❌ Texture sampling
- ❌ Time-based animation
- ❌ UV manipulation
- ❌ Material functions
- ❌ Dynamic material instances
- ❌ Shader integration

---

## 🔥 PHASE 3: CRITICAL FIXES (5-8 hours)

### 3.1: Fix Expression → Node Conversion ⚡ CRITICAL
**Time:** 2-3 hours  
**Priority:** 🔴 HIGHEST

**Problem:** Expressions like `base_color * 2.0` are stored as strings, not actual nodes.

**Solution:**
```rust
// In ue5_pipeline.rs
use ue5_materials::MaterialGraphConverter;

let mut converter = MaterialGraphConverter::new();
let material_graph = converter.convert(&ast_material_def)?;
// Now material_graph.nodes is ACTUALLY POPULATED!
```

**What this unlocks:**
- ✅ Complex expressions become real node graphs
- ✅ Let bindings create intermediate nodes
- ✅ Automatic node wiring
- ✅ Proper node ID generation

**Test:**
```kn
@material_graph
material ComplexMath:
    input a: Float = 1.0
    input b: Float = 2.0
    
    let sum = a + b
    let product = sum * 2.0
    let final = product / 3.0
    
    output emissive = vec3(final, final, final)
```
Should generate: 5 nodes (2 params, add, multiply, divide) properly wired.

---

### 3.2: Custom HLSL Nodes ⚡ GAME CHANGER
**Time:** 1-2 hours  
**Priority:** 🔴 HIGHEST

**What you get:** Write ANY HLSL code directly in materials.

**KAIN Syntax:**
```kn
@material_graph
material CustomShader:
    input base_color: Vec3 = vec3(1, 1, 1)
    input time: Float = 0.0
    
    let effect = custom_hlsl("""
        float3 result = BaseColor;
        float wave = sin(Time * 3.14159);
        result *= (wave * 0.5 + 0.5);
        return result;
    """, 
    inputs: [base_color, time],
    output_type: "float3",
    description: "Sine wave effect")
    
    output emissive = effect
```

**Implementation:**
1. Add `CustomHLSL` node type to `MaterialNodeType`
2. Add parser support for `custom_hlsl()` function
3. Generate `UMaterialExpressionCustom` in factory

**Generated C++:**
```cpp
UMaterialExpressionCustom* CustomNode = NewObject<UMaterialExpressionCustom>(Material);
CustomNode->Code = TEXT("float3 result = BaseColor;\nfloat wave = sin(Time * 3.14159);\nresult *= (wave * 0.5 + 0.5);\nreturn result;");
CustomNode->OutputType = CMOT_Float3;
CustomNode->Description = TEXT("Sine wave effect");

// Add inputs
FCustomInput Input0;
Input0.InputName = TEXT("BaseColor");
CustomNode->Inputs.Add(Input0);

FCustomInput Input1;
Input1.InputName = TEXT("Time");
CustomNode->Inputs.Add(Input1);
```

**What this unlocks:**
- ✅ ANY shader effect possible
- ✅ No node graph limitations
- ✅ Direct HLSL control
- ✅ Unblocks ALL custom effects

---

### 3.3: Shader Integration ⚡ SYNERGY
**Time:** 2-3 hours  
**Priority:** 🟡 HIGH

**What you get:** Call your existing KAIN shaders from materials.

**KAIN Syntax:**
```kn
// Your existing shader
shader fragment WaveEffect(uv: Vec2) -> Vec4:
    uniform intensity: Float @0
    uniform frequency: Float @1
    
    let wave = sin(uv.y * frequency)
    let color = vec3(wave * intensity)
    return vec4(color, 1.0)

// Use it in a material
@material_graph
material ShaderMaterial:
    input intensity: Float = 1.0
    input frequency: Float = 10.0
    
    let effect = call_shader(WaveEffect, 
        intensity: intensity,
        frequency: frequency)
    
    output emissive = effect.rgb
```

**Implementation:**
1. Detect shader references in material graphs
2. Extract shader HLSL code
3. Generate Custom HLSL node with shader code
4. Wire shader parameters to material inputs

**What this unlocks:**
- ✅ Reuse shader code in materials
- ✅ Unified shader/material workflow
- ✅ No code duplication
- ✅ LLM can generate both at once

---

## 🎨 PHASE 4: TEXTURE SUPPORT (3-4 hours)

### 4.1: Texture Sampling ⚡ ESSENTIAL
**Time:** 2-3 hours  
**Priority:** 🟡 HIGH

**KAIN Syntax:**
```kn
@material_graph
material TexturedPBR:
    input albedo_map: Texture2D
    input normal_map: Texture2D
    input roughness_map: Texture2D
    input tiling: Vec2 = vec2(1, 1)
    
    let uv = texture_coordinate(0, tiling)
    let albedo = sample(albedo_map, uv)
    let normal = sample(normal_map, uv)
    let roughness = sample(roughness_map, uv)
    
    output base_color = albedo.rgb
    output normal = normal.rgb
    output roughness = roughness.r
```

**Implementation:**
1. Add `Texture2D` input type
2. Add `TextureSample` node type
3. Add `TextureCoordinate` node type
4. Generate `UMaterialExpressionTextureSample`
5. Generate `UMaterialExpressionTextureCoordinate`
6. Wire UV connections

**What this unlocks:**
- ✅ Realistic materials with textures
- ✅ Normal mapping
- ✅ PBR workflows
- ✅ Marketplace-ready materials

---

### 4.2: UV Manipulation ⚡ ADVANCED
**Time:** 1 hour  
**Priority:** 🟢 MEDIUM

**KAIN Syntax:**
```kn
@material_graph
material ScrollingTexture:
    input albedo_map: Texture2D
    input scroll_speed: Vec2 = vec2(0.1, 0.0)
    input tiling: Vec2 = vec2(1, 1)
    
    let base_uv = texture_coordinate(0, tiling)
    let time_offset = time() * scroll_speed
    let scrolled_uv = base_uv + time_offset
    let albedo = sample(albedo_map, scrolled_uv)
    
    output base_color = albedo.rgb
```

**Implementation:**
1. Add `time()` function
2. Add UV math operations (add, multiply, etc.)
3. Generate `UMaterialExpressionTime`
4. Wire UV transformations

**What this unlocks:**
- ✅ Scrolling textures (water, lava)
- ✅ Animated UVs
- ✅ Texture rotation
- ✅ Dynamic effects

---

## ⚡ PHASE 5: ANIMATION & TIME (2-3 hours)

### 5.1: Time-Based Effects ⚡ DYNAMIC
**Time:** 2-3 hours  
**Priority:** 🟡 HIGH

**KAIN Syntax:**
```kn
@material_graph
material PulsingGlow:
    input glow_color: Vec3 = vec3(0, 1, 1)
    input pulse_speed: Float = 2.0
    input pulse_min: Float = 0.2
    input pulse_max: Float = 1.0
    
    let time_value = time()
    let sine_wave = sin(time_value * pulse_speed)
    let normalized = (sine_wave + 1.0) * 0.5  // 0.0 to 1.0
    let pulse = lerp(pulse_min, pulse_max, normalized)
    let glow = glow_color * pulse
    
    output emissive = glow
```

**Implementation:**
1. Add `time()` function → `UMaterialExpressionTime`
2. Add `sin()`, `cos()`, `tan()` functions
3. Add `abs()`, `frac()`, `floor()`, `ceil()`
4. Wire time-based nodes

**What this unlocks:**
- ✅ Pulsing effects
- ✅ Animated materials
- ✅ Scrolling textures
- ✅ Dynamic shaders

---

## 🎯 PHASE 6: MATERIAL FUNCTIONS (4-5 hours)

### 6.1: Reusable Material Functions ⚡ POWER
**Time:** 4-5 hours  
**Priority:** 🟢 MEDIUM

**KAIN Syntax:**
```kn
@material_function
fn color_grading(color: Vec3, saturation: Float, brightness: Float) -> Vec3:
    let gray = dot(color, vec3(0.299, 0.587, 0.114))
    let saturated = lerp(vec3(gray), color, saturation)
    let final = saturated * brightness
    return final

@material_function
fn fresnel_rim(rim_color: Vec3, rim_power: Float, rim_intensity: Float) -> Vec3:
    let fresnel_value = fresnel(rim_power, 0.0)
    let rim = rim_color * fresnel_value * rim_intensity
    return rim

@material_graph
material AdvancedMaterial:
    input base_color: Vec3 = vec3(0.5, 0.5, 0.5)
    input saturation: Float = 1.0
    input brightness: Float = 1.0
    input rim_color: Vec3 = vec3(0, 0.5, 1)
    input rim_power: Float = 3.0
    input rim_intensity: Float = 2.0
    
    let graded = color_grading(base_color, saturation, brightness)
    let rim = fresnel_rim(rim_color, rim_power, rim_intensity)
    
    output base_color = graded
    output emissive = rim
```

**Implementation:**
1. Add `@material_function` attribute
2. Parse function definitions
3. Generate `UMaterialFunction` assets
4. Wire function calls in materials
5. Handle function parameters

**What this unlocks:**
- ✅ Reusable shader logic
- ✅ Library of effects
- ✅ Cleaner material graphs
- ✅ Easier maintenance

---

## 🔧 PHASE 7: DYNAMIC MATERIALS (2-3 hours)

### 7.1: Runtime Material Control ⚡ INTERACTIVE
**Time:** 2-3 hours  
**Priority:** 🟡 HIGH

**KAIN Syntax:**
```kn
actor MaterialController:
    state mesh: StaticMeshComponent = StaticMeshComponent()
    state material: GlowMaterial = GlowMaterial()
    state dynamic_mat: DynamicMaterialInstance = null
    
    on BeginPlay():
        mesh.SetMaterial(0, material)
        dynamic_mat = mesh.CreateDynamicMaterialInstance(0, material)
        dynamic_mat.SetScalarParameterValue("Intensity", 2.0)
        dynamic_mat.SetVectorParameterValue("GlowColor", vec3(1, 0, 0))
    
    on Tick(delta: Float):
        let time = GetGameTime()
        let pulse = sin(time * 2.0) * 0.5 + 0.5
        dynamic_mat.SetScalarParameterValue("Intensity", pulse * 3.0)
```

**Implementation:**
1. Add `DynamicMaterialInstance` type
2. Add `CreateDynamicMaterialInstance()` method
3. Add `SetScalarParameterValue()` method
4. Add `SetVectorParameterValue()` method
5. Generate UE5 dynamic material code

**What this unlocks:**
- ✅ Runtime material changes
- ✅ Interactive effects
- ✅ Animated parameters
- ✅ Gameplay-driven visuals

---

## 🌟 PHASE 8: ADVANCED FEATURES (8-10 hours)

### 8.1: Material Layers ⚡ COMPLEX
**Time:** 5-6 hours  
**Priority:** 🟢 LOW

**KAIN Syntax:**
```kn
@material_graph
material LayeredMaterial:
    input base_layer: Material
    input detail_layer: Material
    input blend_mask: Texture2D
    input blend_strength: Float = 1.0
    
    let uv = texture_coordinate(0, vec2(1, 1))
    let mask = sample(blend_mask, uv).r
    let final_mask = mask * blend_strength
    
    let blended = lerp(base_layer, detail_layer, final_mask)
    
    output base_color = blended.base_color
    output roughness = blended.roughness
    output metallic = blended.metallic
```

**What this unlocks:**
- ✅ Complex material blending
- ✅ Layered materials
- ✅ Terrain materials
- ✅ Advanced effects

---

### 8.2: World-Space Operations ⚡ SPATIAL
**Time:** 2-3 hours  
**Priority:** 🟢 LOW

**KAIN Syntax:**
```kn
@material_graph
material WorldSpaceMaterial:
    input tiling: Float = 1.0
    input blend_sharpness: Float = 5.0
    
    let world_pos = world_position()
    let world_normal = world_normal()
    
    // Triplanar mapping
    let uv_x = world_pos.yz * tiling
    let uv_y = world_pos.xz * tiling
    let uv_z = world_pos.xy * tiling
    
    let blend = abs(world_normal)
    let blend_normalized = blend / (blend.x + blend.y + blend.z)
    
    output base_color = vec3(blend_normalized)
```

**What this unlocks:**
- ✅ Triplanar mapping
- ✅ World-space textures
- ✅ Position-based effects
- ✅ Procedural materials

---

### 8.3: Vertex Shader Support ⚡ DEFORMATION
**Time:** 1-2 hours  
**Priority:** 🟢 LOW

**KAIN Syntax:**
```kn
@material_graph
material WavingGrass:
    input wave_speed: Float = 1.0
    input wave_strength: Float = 10.0
    input wind_direction: Vec3 = vec3(1, 0, 0)
    
    let world_pos = world_position()
    let time_value = time()
    
    let wave = sin(world_pos.x * 0.1 + time_value * wave_speed)
    let offset = wind_direction * wave * wave_strength
    
    output world_position_offset = offset
    output base_color = vec3(0.2, 0.6, 0.1)
```

**What this unlocks:**
- ✅ Vertex animation
- ✅ Waving grass/foliage
- ✅ Water waves
- ✅ Cloth simulation

---

## 📊 PRIORITY MATRIX

### 🔴 DO FIRST (Nuclear Core - 5-8 hours)
1. **Expression → Node Conversion** (2-3 hours) - Makes system actually work
2. **Custom HLSL Nodes** (1-2 hours) - Unblocks everything
3. **Shader Integration** (2-3 hours) - Synergy with existing shaders

### 🟡 DO NEXT (Power Boost - 7-10 hours)
4. **Texture Sampling** (2-3 hours) - Essential for realistic materials
5. **UV Manipulation** (1 hour) - Animated textures
6. **Time-Based Effects** (2-3 hours) - Dynamic materials
7. **Dynamic Material Instances** (2-3 hours) - Runtime control

### 🟢 DO LATER (Advanced - 12-16 hours)
8. **Material Functions** (4-5 hours) - Reusable logic
9. **Material Layers** (5-6 hours) - Complex blending
10. **World-Space Operations** (2-3 hours) - Spatial effects
11. **Vertex Shader Support** (1-2 hours) - Deformation

---

## 🎯 RECOMMENDED PATH

### Path 1: NUCLEAR MINIMUM (5-8 hours)
Get the system fully operational with custom HLSL:
1. Fix expression conversion (2-3 hours)
2. Add custom HLSL nodes (1-2 hours)
3. Add shader integration (2-3 hours)

**Result:** LLMs can generate ANY material effect using custom HLSL.

### Path 2: PRODUCTION READY (12-18 hours)
Add textures and animation:
1. Nuclear Minimum (5-8 hours)
2. Texture sampling (2-3 hours)
3. UV manipulation (1 hour)
4. Time-based effects (2-3 hours)
5. Dynamic materials (2-3 hours)

**Result:** Full-featured material system, marketplace-ready.

### Path 3: ULTIMATE POWER (24-34 hours)
Everything:
1. Production Ready (12-18 hours)
2. Material functions (4-5 hours)
3. Material layers (5-6 hours)
4. World-space ops (2-3 hours)
5. Vertex shaders (1-2 hours)

**Result:** AAA-quality material system, industry-leading.

---

## 💡 IMMEDIATE NEXT STEPS

### Option A: Quick Win (1-2 hours)
**Add custom HLSL nodes RIGHT NOW.**

This unblocks you immediately and lets you write ANY shader effect:
```kn
@material_graph
material AnyEffect:
    let effect = custom_hlsl("""
        // ANY HLSL CODE HERE
        return float3(1, 0, 0);
    """, output_type: "float3")
    
    output emissive = effect
```

### Option B: Full Nuclear (5-8 hours)
**Do all of Phase 3 in one session.**

Get expression conversion, custom HLSL, and shader integration all at once. This makes the system production-ready for custom effects.

### Option C: Texture Focus (3-4 hours)
**Skip custom HLSL, go straight to textures.**

If you need realistic materials more than custom effects, implement texture sampling first.

---

## 🚀 WHAT I RECOMMEND

**DO THIS NOW (1-2 hours):**
Add custom HLSL nodes. This is the BIGGEST bang for your buck:
- Unblocks ALL custom effects
- Works with existing system
- No complex refactoring needed
- LLMs can generate any shader

**THEN DO THIS (2-3 hours):**
Fix expression conversion. This makes the node graph system actually work properly.

**TOTAL: 3-5 hours to nuclear power mode.** ⚡

---

## 📝 IMPLEMENTATION CHECKLIST

### Phase 3.1: Expression Conversion
- [ ] Integrate MaterialGraphConverter in packager
- [ ] Test binary operations (add, multiply, etc.)
- [ ] Test function calls (lerp, dot, etc.)
- [ ] Test field access (color.rgb, etc.)
- [ ] Test let bindings
- [ ] Verify node IDs are unique
- [ ] Verify connections are wired correctly

### Phase 3.2: Custom HLSL
- [ ] Add `CustomHLSL` node type to IR
- [ ] Add `custom_hlsl()` parser support
- [ ] Generate `UMaterialExpressionCustom` in factory
- [ ] Handle input wiring
- [ ] Handle output types (float, float2, float3, float4)
- [ ] Test with simple HLSL code
- [ ] Test with complex HLSL code
- [ ] Test with multiple inputs

### Phase 3.3: Shader Integration
- [ ] Detect shader references in materials
- [ ] Extract shader HLSL code
- [ ] Convert shader to Custom HLSL node
- [ ] Wire shader parameters
- [ ] Test with fragment shaders
- [ ] Test with compute shaders
- [ ] Handle shader uniforms

---

## 🎉 EXPECTED RESULTS

After Phase 3 (Nuclear Core):
```kn
@material_graph
material UltimateShader:
    input intensity: Float = 1.0
    input color: Vec3 = vec3(1, 0, 0)
    
    // Option 1: Use custom HLSL
    let effect1 = custom_hlsl("""
        float wave = sin(_Time.y * 3.14159);
        return BaseColor * wave * Intensity;
    """, inputs: [color, intensity], output_type: "float3")
    
    // Option 2: Use existing shader
    let effect2 = call_shader(MyWaveShader, intensity: intensity)
    
    // Option 3: Use node graph
    let effect3 = color * intensity
    
    output emissive = effect1  // Or effect2, or effect3!
```

**LLMs can now generate materials using:**
- ✅ Custom HLSL (unlimited power)
- ✅ Existing shaders (code reuse)
- ✅ Node graphs (visual clarity)

**ALL THREE APPROACHES WORK TOGETHER.** 🔥

---

**Ready to go nuclear? Say the word and I'll start with custom HLSL nodes!** ⚡🚀
