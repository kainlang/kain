# KAIN Material Graph Syntax Guide

## Overview

KAIN's material graph system allows you to define UE5 materials using a node-based syntax that compiles to C++ code. Materials are generated at runtime and saved as UE5 assets in your plugin's Content folder.

## Basic Syntax

```kn
@material_graph(
    blend_mode: Opaque,
    shading_model: DefaultLit,
    two_sided: false
)
material MaterialName:
    input param_name: Type = default_value
    
    let variable = node_expression
    
    output output_name = node_reference
```

## Material Properties

### Blend Modes
- `Opaque` - Solid, non-transparent (default)
- `Masked` - Binary transparency (alpha test)
- `Translucent` - Smooth transparency
- `Additive` - Additive blending (glow effects)
- `Modulate` - Multiplicative blending

### Shading Models
- `DefaultLit` - Standard PBR lighting (default)
- `Unlit` - No lighting calculations
- `Subsurface` - Subsurface scattering
- `PreintegratedSkin` - Optimized skin rendering
- `ClearCoat` - Car paint, lacquer
- `SubsurfaceProfile` - Profile-based SSS
- `TwoSidedFoliage` - Foliage with translucency
- `Hair` - Hair/fur rendering
- `Cloth` - Fabric rendering
- `Eye` - Eye rendering

### Material Domain
- `Surface` - Standard surface material (default)
- `DeferredDecal` - Decal material
- `LightFunction` - Light function material
- `PostProcess` - Post-process material
- `UI` - UI material

## Input Types

### Scalar Parameters
```kn
input roughness: Float = 0.5
input metallic: Float = 0.0
input intensity: Float = 1.0
```

### Vector Parameters
```kn
input tint: Vec3 = vec3(1.0, 1.0, 1.0)
input offset: Vec3 = vec3(0.0, 0.0, 0.0)
```

### Color Parameters
```kn
input base_color: Vec4 = vec4(1.0, 1.0, 1.0, 1.0)
input glow_color: Vec4 = vec4(0.0, 1.0, 1.0, 1.0)
```

### Texture Parameters (Future)
```kn
input albedo_map: Texture2D
input normal_map: Texture2D
```

## Material Outputs

### PBR Outputs
- `base_color` - Base color (albedo) - Vec3 or Vec4
- `metallic` - Metallic value - Float (0.0 = dielectric, 1.0 = metal)
- `specular` - Specular intensity - Float (default 0.5)
- `roughness` - Roughness value - Float (0.0 = smooth, 1.0 = rough)
- `emissive` - Emissive color - Vec3 or Vec4
- `opacity` - Opacity value - Float (0.0 = transparent, 1.0 = opaque)
- `normal` - Normal map - Vec3
- `world_position_offset` - Vertex displacement - Vec3

## Node Types

### Parameter Nodes
```kn
// Scalar parameter
let roughness_param = scalar_parameter("Roughness", 0.5)

// Vector parameter
let tint_param = vector_parameter("Tint", vec3(1.0, 1.0, 1.0))

// Color parameter
let color_param = color_parameter("Color", vec4(1.0, 0.5, 0.0, 1.0))
```

### Math Nodes
```kn
// Basic operations
let sum = add(a, b)
let difference = subtract(a, b)
let product = multiply(a, b)
let quotient = divide(a, b)

// Advanced operations
let interpolated = lerp(a, b, alpha)
let dot_result = dot(a, b)
let powered = power(base, exponent)
let clamped = clamp(input, min, max)
```

### Utility Nodes
```kn
// Component mask (extract channels)
let red_channel = component_mask(color, r: true, g: false, b: false, a: false)
let rgb_only = component_mask(color, r: true, g: true, b: true, a: false)

// Append vectors
let vec4_result = append(vec3_value, float_value)

// Fresnel effect
let fresnel_result = fresnel(exponent, base_reflect_fraction)
```

### Constant Nodes
```kn
// Constant values
let one = constant_float(1.0)
let white = constant_vec3(vec3(1.0, 1.0, 1.0))
let opaque_white = constant_vec4(vec4(1.0, 1.0, 1.0, 1.0))
```

### Texture Nodes (Future)
```kn
// Texture sampling
let uv = texture_coordinate(index: 0, tiling: vec2(1.0, 1.0))
let sampled = texture_sample(albedo_map, uv)
```

## Complete Examples

### Example 1: Simple PBR Material
```kn
@material_graph
material SimplePBR:
    input roughness: Float = 0.5
    input metallic: Float = 0.0
    input tint: Vec3 = vec3(1.0, 1.0, 1.0)
    
    output base_color = tint
    output roughness = roughness
    output metallic = metallic
```

**Generated:** A basic PBR material with adjustable roughness, metallic, and tint parameters.

### Example 2: Emissive Glow Material
```kn
@material_graph(blend_mode: Additive)
material GlowMaterial:
    input glow_color: Vec3 = vec3(0.0, 1.0, 1.0)
    input glow_intensity: Float = 2.0
    
    let glow = multiply(glow_color, glow_intensity)
    
    output emissive = glow
```

**Generated:** An additive material perfect for glowing effects, neon signs, energy shields.

### Example 3: Tinted Metal Material
```kn
@material_graph
material TintedMetal:
    input base_color: Vec3 = vec3(0.8, 0.8, 0.8)
    input tint_color: Vec3 = vec3(1.0, 0.5, 0.0)
    input tint_strength: Float = 0.5
    input roughness: Float = 0.3
    
    let tinted = multiply(multiply(base_color, tint_color), tint_strength)
    let final_color = add(base_color, tinted)
    
    output base_color = final_color
    output metallic = constant_float(1.0)
    output roughness = roughness
```

**Generated:** A metallic material with adjustable color tinting, useful for colored metals.

### Example 4: Fresnel Rim Light
```kn
@material_graph
material FresnelRim:
    input base_color: Vec3 = vec3(0.1, 0.1, 0.1)
    input rim_color: Vec3 = vec3(0.0, 0.5, 1.0)
    input rim_power: Float = 3.0
    input rim_intensity: Float = 2.0
    
    let fresnel_value = fresnel(rim_power, constant_float(0.0))
    let rim_effect = multiply(multiply(rim_color, fresnel_value), rim_intensity)
    
    output base_color = base_color
    output emissive = rim_effect
    output roughness = constant_float(0.5)
```

**Generated:** A material with rim lighting effect, great for force fields, shields, holograms.

### Example 5: Pulsing Emissive
```kn
@material_graph
material PulsingEmissive:
    input base_color: Vec3 = vec3(0.2, 0.2, 0.2)
    input pulse_color: Vec3 = vec3(1.0, 0.0, 0.0)
    input pulse_speed: Float = 1.0
    input pulse_min: Float = 0.2
    input pulse_max: Float = 1.0
    
    // Note: Time node would be needed for actual pulsing
    // This example shows the structure
    let pulse_strength = constant_float(0.5)  // Would be time-based
    let clamped_pulse = clamp(pulse_strength, pulse_min, pulse_max)
    let pulse_effect = multiply(pulse_color, clamped_pulse)
    
    output base_color = base_color
    output emissive = pulse_effect
```

**Generated:** Foundation for a pulsing emissive material (time node support coming soon).

### Example 6: Two-Sided Foliage
```kn
@material_graph(
    shading_model: TwoSidedFoliage,
    two_sided: true
)
material Foliage:
    input leaf_color: Vec3 = vec3(0.2, 0.6, 0.1)
    input translucency: Float = 0.3
    input roughness: Float = 0.8
    
    output base_color = leaf_color
    output roughness = roughness
    output opacity = translucency
```

**Generated:** A two-sided foliage material with subsurface scattering.

### Example 7: Hologram Effect
```kn
@material_graph(blend_mode: Translucent)
material Hologram:
    input holo_color: Vec3 = vec3(0.0, 1.0, 1.0)
    input scan_line_intensity: Float = 0.3
    input base_opacity: Float = 0.5
    input fresnel_power: Float = 2.0
    
    let fresnel_value = fresnel(fresnel_power, constant_float(0.1))
    let fresnel_glow = multiply(holo_color, fresnel_value)
    
    output base_color = holo_color
    output emissive = fresnel_glow
    output opacity = base_opacity
```

**Generated:** A translucent hologram material with fresnel rim lighting.

### Example 8: Metallic Paint
```kn
@material_graph
material MetallicPaint:
    input paint_color: Vec3 = vec3(0.8, 0.1, 0.1)
    input metallic_flakes: Float = 0.3
    input clear_coat_roughness: Float = 0.1
    input base_roughness: Float = 0.6
    
    let mixed_roughness = lerp(base_roughness, clear_coat_roughness, metallic_flakes)
    
    output base_color = paint_color
    output metallic = metallic_flakes
    output roughness = mixed_roughness
```

**Generated:** A car paint-style material with metallic flakes.

## Node Positioning

Nodes are automatically positioned in the material editor. The system uses a left-to-right flow:
- Parameters on the left (x: 0-200)
- Operations in the middle (x: 300-600)
- Outputs on the right (x: 800+)

Vertical spacing is automatic based on node count.

## Best Practices

### 1. Use Descriptive Names
```kn
// Good
input rim_light_intensity: Float = 2.0

// Bad
input val1: Float = 2.0
```

### 2. Group Related Parameters
```kn
// Organize by function
input base_color: Vec3 = vec3(1.0, 1.0, 1.0)
input base_roughness: Float = 0.5
input base_metallic: Float = 0.0

input rim_color: Vec3 = vec3(0.0, 0.5, 1.0)
input rim_intensity: Float = 2.0
input rim_power: Float = 3.0
```

### 3. Use Intermediate Variables
```kn
// Good - readable
let tinted = multiply(base_color, tint_color)
let scaled = multiply(tinted, tint_strength)
output base_color = scaled

// Bad - hard to follow
output base_color = multiply(multiply(base_color, tint_color), tint_strength)
```

### 4. Clamp Values When Needed
```kn
// Ensure values stay in valid range
let clamped_roughness = clamp(roughness, constant_float(0.0), constant_float(1.0))
output roughness = clamped_roughness
```

### 5. Use Appropriate Blend Modes
- `Opaque` - Solid objects (default, best performance)
- `Masked` - Foliage, fences (binary transparency)
- `Translucent` - Glass, water (expensive)
- `Additive` - Glows, particles (no depth writes)

## Limitations (Current Phase)

### Not Yet Supported
- Texture sampling (coming in Phase 3)
- Time-based animations (coming in Phase 3)
- Custom material functions
- Material instances (use parameters instead)
- World-space operations
- Vertex shader modifications (except world_position_offset)

### Workarounds
- **Textures:** Use solid colors for now, add texture support later
- **Animation:** Use Blueprint to modify parameters at runtime
- **Complex logic:** Break into multiple materials or use custom shaders

## Compilation

### Build Command
```bash
cd YourPlugin
kain build --ue5
```

### Generated Files
```
YourPlugin/
├── Source/
│   └── Generated/
│       ├── MaterialFactories.h
│       └── MaterialFactories.cpp
└── Content/
    └── Materials/
        ├── M_SimplePBR.uasset
        ├── M_GlowMaterial.uasset
        └── M_TintedMetal.uasset
```

### Using Generated Materials
1. Build your plugin with `kain build --ue5`
2. Open your UE5 project
3. Materials are in `Plugins/YourPlugin/Content/Materials/`
4. Drag materials onto meshes in your scene
5. Adjust parameters in the Details panel

## Troubleshooting

### Material Not Appearing
- Check build output for errors
- Verify `@material_graph` attribute is present
- Ensure plugin is enabled in UE5

### Parameters Not Showing
- Verify input declarations have correct types
- Check parameter names don't conflict with UE5 reserved words
- Rebuild plugin after changes

### Incorrect Colors
- Remember Vec3 is RGB (0.0-1.0 range)
- Vec4 is RGBA (alpha channel for opacity)
- Use `constant_float(1.0)` for full intensity

### Performance Issues
- Use `Opaque` blend mode when possible
- Minimize `lerp` and `power` operations
- Avoid deep node chains (>10 operations)

## Future Enhancements

### Phase 3 (Texture Support)
```kn
@material_graph
material TexturedPBR:
    input albedo_map: Texture2D
    input normal_map: Texture2D
    input roughness_map: Texture2D
    
    let uv = texture_coordinate(0, vec2(1.0, 1.0))
    let albedo = texture_sample(albedo_map, uv)
    let normal = texture_sample(normal_map, uv)
    let roughness = texture_sample(roughness_map, uv)
    
    output base_color = albedo
    output normal = normal
    output roughness = component_mask(roughness, r: true, g: false, b: false, a: false)
```

### Phase 4 (Animation Support)
```kn
@material_graph
material AnimatedGlow:
    input glow_color: Vec3 = vec3(0.0, 1.0, 1.0)
    input pulse_speed: Float = 1.0
    
    let time_value = time()
    let sine_wave = sine(multiply(time_value, pulse_speed))
    let pulse = add(multiply(sine_wave, constant_float(0.5)), constant_float(0.5))
    let glow = multiply(glow_color, pulse)
    
    output emissive = glow
```

## Reference

### All Node Types
- `scalar_parameter(name, default)` - Float parameter
- `vector_parameter(name, default)` - Vec3 parameter
- `color_parameter(name, default)` - Vec4 parameter
- `add(a, b)` - Addition
- `subtract(a, b)` - Subtraction
- `multiply(a, b)` - Multiplication
- `divide(a, b)` - Division
- `lerp(a, b, alpha)` - Linear interpolation
- `dot(a, b)` - Dot product
- `power(base, exponent)` - Power operation
- `clamp(input, min, max)` - Clamp to range
- `fresnel(exponent, base_reflect_fraction)` - Fresnel effect
- `component_mask(input, r, g, b, a)` - Extract channels
- `append(a, b)` - Append vectors
- `constant_float(value)` - Float constant
- `constant_vec3(value)` - Vec3 constant
- `constant_vec4(value)` - Vec4 constant
- `texture_coordinate(index, tiling)` - UV coordinates (future)
- `texture_sample(texture, uv)` - Sample texture (future)

### All Output Pins
- `base_color` - Base color (Vec3/Vec4)
- `metallic` - Metallic (Float)
- `specular` - Specular (Float)
- `roughness` - Roughness (Float)
- `emissive` - Emissive (Vec3/Vec4)
- `opacity` - Opacity (Float)
- `normal` - Normal (Vec3)
- `world_position_offset` - Vertex displacement (Vec3)

---

**Status:** Phase 2 Complete - Basic material graph system operational  
**Next:** Phase 3 - Texture sampling and UV manipulation  
**Version:** KAIN 0.2.0
