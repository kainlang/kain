# Shader Library Extraction Analysis
# Task 3.1: Analyze kn_library/shaders/ for stdlib extraction

## Executive Summary

Analyzed 8 key shader files containing 100+ functions across 9 categories:
- **PBR & Lighting**: 15 functions
- **Noise & Procedural**: 20+ functions  
- **Color Grading & Post-Processing**: 25+ functions
- **UV Manipulation**: 8 functions
- **Volumetric Effects**: 12 functions
- **Subsurface Scattering**: 6 functions
- **Ray Marching**: 8 functions
- **Procedural Generators**: 21 functions
- **Image Filters**: 15 functions

## Files Analyzed

1. **AlphaGen_FULL.kn** - 21 procedural generators + 15 filters
2. **KainCosmosGod.kn** - 12 GPU kernels for planetary simulation
3. **KainFlowGod.kn** - 6 Navier-Stokes fluid simulation kernels
4. **volumetric_clouds.kn** - Cloud raymarching with noise functions
5. **volumetric_fog.kn** - Height-based fog with scattering
6. **pbr_material.kn** - Complete PBR BRDF implementation
7. **subsurface_scattering.kn** - Burley SSS with transmittance
8. **UltimateVisualEffectsSuite.kn** - 16 post-processing effects
9. **post_processing.kn** - TAA, bloom, color grading
10. **02_shaders_generators.kn** - Shape generators
11. **03_shaders_filters.kn** - Image processing filters

---

## Category 1: PBR & Lighting Functions

### From: pbr_material.kn

**High Priority Extractions:**


1. **distribution_ggx(n: Vec3, h: Vec3, roughness: Float) -> Float**
   - GGX/Trowbridge-Reitz normal distribution function
   - Core PBR specular calculation
   - Used in Cook-Torrance BRDF

2. **geometry_schlick_ggx(n_dot_v: Float, roughness: Float) -> Float**
   - Schlick-GGX geometry function
   - Single direction geometry term
   - Part of Smith's geometry function

3. **geometry_smith(n: Vec3, v: Vec3, l: Vec3, roughness: Float) -> Float**
   - Smith's geometry function combining view and light
   - Accounts for microfacet shadowing/masking
   - Essential for physically accurate specular

4. **fresnel_schlick(cos_theta: Float, f0: Vec3) -> Vec3**
   - Fresnel-Schlick approximation
   - Calculates surface reflectance at angle
   - Used for both direct and IBL lighting

5. **fresnel_schlick_roughness(cos_theta: Float, f0: Vec3, roughness: Float) -> Vec3**
   - Fresnel with roughness for IBL
   - Accounts for surface roughness in reflections
   - Used in image-based lighting calculations

6. **cook_torrance_brdf(material, light, normal, view_dir, frag_pos) -> Vec3**
   - Complete Cook-Torrance BRDF implementation
   - Combines NDF, geometry, and Fresnel terms
   - Returns outgoing radiance for a single light

**Supporting Functions:**

7. **luminance(color: Vec3) -> Float** (from post_processing.kn)
   - Standard luminance calculation (Rec. 709)
   - Used in tone mapping and color grading

---

## Category 2: Noise & Procedural Functions

### From: volumetric_clouds.kn

**High Priority Extractions:**


1. **hash(p: Vec3) -> Float**
   - 3D hash function for procedural noise
   - Foundation for all noise generation
   - Fast, deterministic pseudo-random

2. **noise_3d(p: Vec3) -> Float**
   - 3D Perlin-style noise with trilinear interpolation
   - Samples 8 corners of cube
   - Smooth, continuous noise field

3. **fbm(p: Vec3, octaves: Int) -> Float**
   - Fractal Brownian Motion
   - Multi-octave noise with frequency/amplitude scaling
   - Creates natural-looking detail at multiple scales

4. **cloud_density(pos: Vec3, time: Float) -> Float**
   - Height-based density with animated noise
   - Combines base shape + detail layers
   - Threshold-based cloud formation

**Additional Noise Functions (from KainCosmosGod.kn):**

5. **Simple hash noise** - Used in K1_Tectonics, K5_Resources
   - `fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453)`
   - 2D variant for texture generation

---

## Category 3: Color Grading & Post-Processing

### From: post_processing.kn

**High Priority Extractions:**

1. **rgb_to_hsv(rgb: Vec3) -> Vec3**
   - RGB to HSV color space conversion
   - Enables hue/saturation manipulation
   - Handles edge cases (delta near zero)

2. **hsv_to_rgb(hsv: Vec3) -> Vec3**
   - HSV to RGB color space conversion
   - 6-region piecewise conversion
   - Complements rgb_to_hsv

3. **random(uv: Vec2) -> Float**
   - 2D pseudo-random for film grain/dithering
   - Screen-space noise generation
   - Temporal variation support

4. **ACES Tonemap Formula** (from TonemapACES shader)
   - Industry-standard filmic tone mapping
   - Parameters: a=2.51, b=0.03, c=2.43, d=0.59, e=0.14
   - Formula: `(x*(a*x+b))/(x*(c*x+d)+e)`

**Color Grading Operations:**

5. **Temperature/Tint Adjustment**
   - White balance simulation
   - temp_shift: `vec3(temp*0.1, 0.0, -temp*0.1)`
   - tint_shift: `vec3(0.0, -tint*0.05, tint*0.05)`

6. **Lift/Gamma/Gain (ASC-CDL)**
   - Industry-standard color correction
   - Formula: `pow(color * gain + lift, 1.0 / gamma)`

7. **Three-Way Color Correction**
   - Separate shadows/midtones/highlights control
   - Luminance-based weight calculation
   - Smooth transitions between regions

---

## Category 4: UV Manipulation

### From: AlphaGen_FULL.kn and UltimateVisualEffectsSuite.kn

**High Priority Extractions:**


1. **Spherize Distortion** (from FilterSpherize)
   - Radial distortion effect
   - Formula: `new_dist = dist * (1.0 - amount * (1.0 - dist * 2.0))`
   - Creates lens/sphere warping

2. **Spiral/Twist Distortion** (from FilterSpiral)
   - Polar coordinate rotation based on distance
   - `new_angle = angle + twist * (1.0 - dist * 2.0)`
   - Creates swirl effects

3. **Domain Warp** (from FilterDomainWarp)
   - Noise-based UV displacement
   - Two-channel noise for x/y displacement
   - Creates organic distortion

4. **Polar Coordinates**
   - Cartesian to polar: `(length(uv), atan2(uv.y, uv.x))`
   - Polar to cartesian: `(cos(angle)*radius, sin(angle)*radius)`
   - Used in radial effects

5. **UV Tiling/Offset**
   - Grid-based repetition
   - Cell ID: `floor(uv * scale)`
   - Local UV: `fract(uv * scale)`

6. **Rotated UV**
   - 2D rotation matrix application
   - Used in crosshatch, fibers patterns

---

## Category 5: Volumetric Effects

### From: volumetric_clouds.kn, volumetric_fog.kn, KainFlowGod.kn

**High Priority Extractions:**

1. **henyey_greenstein(cos_theta: Float, g: Float) -> Float**
   - Phase function for anisotropic scattering
   - Models light scattering in participating media
   - Formula: `(1-g²) / (4π * (1+g²-2g*cosθ)^1.5)`

2. **beer_lambert(density: Float, distance: Float) -> Float**
   - Transmittance calculation
   - Formula: `exp(-density * distance)`
   - Models light absorption through medium

3. **powder_effect(density: Float, cos_theta: Float) -> Float**
   - Multi-scattering approximation (Schneider 2015)
   - Enhances cloud realism
   - Formula: `mix(1.0, 1.0 - exp(-density*2.0), smoothstep(0.5, -0.5, cos_theta))`

4. **light_march(pos: Vec3, sun_dir: Vec3, time: Float) -> Float**
   - Shadow ray marching toward light source
   - Accumulates density along ray
   - Returns light occlusion factor

5. **Exponential Height Fog**
   - Height-based density falloff
   - Formula: `exp(-max(pos.y - fog_height, 0.0) * falloff)`
   - Creates realistic atmospheric fog

**Fluid Simulation Functions (from KainFlowGod.kn):**

6. **Curl/Vorticity Calculation**
   - 3D curl operator on velocity field
   - Central differences: `(vT.z - vB.z) - (vU.y - vD.y)` etc.
   - Used in vorticity confinement

7. **Divergence Calculation**
   - Velocity field flux
   - Formula: `0.5 * (vR - vL + vT - vB + vU - vD)`
   - Should be near zero for incompressible flow

8. **Jacobi Iteration (Pressure Solver)**
   - Iterative pressure field solver
   - Formula: `(div + pL + pR + pB + pT + pD + pU) / 6.0`
   - Enforces incompressibility

---

## Category 6: Subsurface Scattering

### From: subsurface_scattering.kn

**High Priority Extractions:**


1. **Burley Diffusion Profile**
   - Separable screen-space SSS kernel
   - Per-channel falloff: `exp(-dist * s) / dist + exp(-dist * s / 3.0) / (3.0 * dist)`
   - Three channels (R/G/B) with different scatter distances

2. **Wrap Lighting**
   - Softer diffuse for translucent materials
   - Formula: `max((NdotL + wrap) / (1.0 + wrap), 0.0)`
   - Simulates light wrapping around edges

3. **Beer-Lambert Tissue Absorption**
   - Multi-layer absorption (epidermis/dermis)
   - Per-channel: `exp(-thickness / layer_thickness * factor)`
   - Models light penetration through skin layers

4. **Back-Face Transmittance**
   - Light transmission through thin surfaces
   - Factor: `clamp(-NdotL, 0.0, 1.0) * translucency`
   - Creates backlit glow effect

5. **Depth-Aware Kernel Sampling**
   - Screen-space sampling with depth rejection
   - Distance: `sqrt(screen_dist² + depth_diff²)`
   - Prevents bleeding across depth discontinuities

6. **Fresnel Rim Lighting**
   - Edge highlighting for organic materials
   - Formula: `pow(1.0 - max(dot(N, V), 0.0), 4.0)`
   - Enhances silhouette definition

---

## Category 7: Ray Marching Utilities

### From: volumetric_clouds.kn, volumetric_fog.kn

**High Priority Extractions:**

1. **Ray-Box Intersection**
   - Calculate entry/exit points for volume
   - `t_bottom = (cloud_base - camera_pos.y) / ray_dir.y`
   - `t_near = max(min(t_bottom, t_top), 0.0)`

2. **Temporal Jitter**
   - Reduces banding artifacts
   - `fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453 + time)`
   - Distributes samples over frames

3. **Transmittance Accumulation**
   - Beer-Lambert integration along ray
   - `transmittance *= exp(-density * step_size * absorption)`
   - Tracks light attenuation

4. **Light Energy Accumulation**
   - In-scattering integration
   - `light_energy += transmittance * scatter * integration * density`
   - Builds up scattered light contribution

5. **Early Ray Termination**
   - Optimization: exit when opaque
   - `if transmittance < 0.01: break`
   - Saves computation on occluded rays

---

## Category 8: Procedural Generators (21 total)

### From: AlphaGen_FULL.kn, 02_shaders_generators.kn

**Shape Generators:**

1. **Radial Falloff** - `pow(1.0 - saturate(dist), falloff)`
2. **Circle** - `1.0 - smoothstep(1.0 - softness, 1.0, dist)`
3. **Square** - `max(abs(centered.x), abs(centered.y))`
4. **Diamond** - `abs(centered.x) + abs(centered.y)`
5. **Checkerboard** - `fract((floor(u*scale) + floor(v*scale)) * 0.5) * 2.0`

**Noise-Based Generators:**

6. **Perlin Noise** - Multi-octave noise with seed
7. **Voronoi** - Cell-based patterns with edge detection
8. **Seamless Noise** - Tileable noise generation
9. **Cells** - Organic cell patterns
10. **Grunge** - Multi-scale detail noise

**Pattern Generators:**

11. **Bricks** - Grid with mortar gaps
12. **Dots** - Regular dot pattern with spacing
13. **Crosshatch** - Intersecting line patterns
14. **Waves** - Sinusoidal wave patterns
15. **Hexagon** - Hexagonal tiling

**Organic Generators:**

16. **Tears** - Drip/streak patterns
17. **Scratches** - Linear damage patterns
18. **Splatter** - Irregular blob patterns
19. **Cracks** - Fracture line patterns
20. **Fibers** - Directional fiber patterns
21. **Caustics** - Animated water caustics

---

## Category 9: Image Filters (15 total)

### From: AlphaGen_FULL.kn, 03_shaders_filters.kn

**Basic Filters:**


1. **Invert** - `1.0 - color`
2. **Threshold** - `step(cutoff, value)`
3. **Posterize** - `floor(color * steps) / (steps - 1.0)`
4. **Pixelate** - Block-based downsampling

**Blur/Sharpen:**

5. **Gaussian Blur** - 9-tap separable kernel with weights
6. **Box Blur** - Simple averaging filter
7. **Sharpen** - Center-weighted enhancement

**Morphological:**

8. **Dilate** - Expand bright regions
9. **Erode** - Shrink bright regions

**Edge Detection:**

10. **Sobel Edge Detect** - Gradient-based edge finding
    - Gx: `-tl + tr - bl + br`
    - Gy: `-tl - tr + bl + br`
    - Edge: `sqrt(gx² + gy²)`

**Color Adjustment:**

11. **Levels** - Black/white point + gamma
    - `pow(saturate((value - black) / (white - black)), gamma)`

12. **Contrast** - Midpoint-based contrast
    - `saturate((value - 0.5) * contrast + 0.5)`

**Distortion Filters:**

13. **Spherize** - Radial lens distortion
14. **Spiral** - Rotational twist
15. **Domain Warp** - Noise-based displacement

---

## Additional Utility Functions

### Mathematical Helpers

1. **smoothstep** - Hermite interpolation (built-in but worth documenting)
2. **saturate** - `clamp(x, 0.0, 1.0)` (built-in)
3. **mix/lerp** - Linear interpolation (built-in)
4. **fract** - Fractional part (built-in)
5. **mod** - Modulo operation (built-in)

### Vector Operations

1. **normalize** - Vector normalization (built-in)
2. **length** - Vector magnitude (built-in)
3. **dot** - Dot product (built-in)
4. **cross** - Cross product (built-in)
5. **reflect** - Vector reflection (built-in)

### Color Space Conversions

1. **sRGB to Linear** - `pow(color, vec3(2.2))`
2. **Linear to sRGB** - `pow(color, vec3(1.0/2.2))`
3. **RGB to Luminance** - `dot(color, vec3(0.2126, 0.7152, 0.0722))`

---

## Extraction Priority Matrix

### Tier 1: Essential (Extract First)
- PBR functions (distribution_ggx, geometry_smith, fresnel_schlick)
- Noise functions (hash, noise_3d, fbm)
- Color grading (rgb_to_hsv, hsv_to_rgb, ACES tonemap)
- Volumetric (henyey_greenstein, beer_lambert)

### Tier 2: High Value (Extract Second)
- UV manipulation (spherize, spiral, domain_warp)
- SSS functions (Burley profile, wrap lighting)
- Ray marching utilities
- Basic filters (blur, sharpen, edge detect)

### Tier 3: Specialized (Extract Third)
- Procedural generators (21 shape/pattern functions)
- Advanced filters (morphological, distortion)
- Fluid simulation (curl, divergence, Jacobi)
- Atmospheric scattering

---

## Implementation Notes

### Function Signatures

All extracted functions should follow stdlib conventions:
- Pure functions (no side effects)
- Explicit parameter types
- Return type annotations
- Documentation comments

### Example Extraction Format:

```kain
# PBR: GGX Normal Distribution Function
# Trowbridge-Reitz distribution for microfacet specular
fn distribution_ggx(n: Vec3, h: Vec3, roughness: Float) -> Float with Pure:
    let a = roughness * roughness
    let a2 = a * a
    let n_dot_h = max(dot(n, h), 0.0)
    let n_dot_h_2 = n_dot_h * n_dot_h
    
    let nom = a2
    let denom_inner = n_dot_h_2 * (a2 - 1.0) + 1.0
    let denom = 3.14159265 * denom_inner * denom_inner
    
    return nom / max(denom, 0.0001)
```

### Testing Strategy

Each extracted function should have:
1. Unit test with known inputs/outputs
2. Visual test shader demonstrating usage
3. Performance benchmark
4. Documentation with references

---

## File Organization Recommendations


### Proposed stdlib Structure:

```
kn_library/stdlib/
├── math/
│   ├── noise.kn          # hash, noise_3d, fbm, voronoi
│   ├── interpolation.kn  # smoothstep variants, easing
│   └── geometry.kn       # distance functions, intersections
│
├── graphics/
│   ├── pbr.kn           # BRDF functions, lighting models
│   ├── color.kn         # Color space conversions, grading
│   ├── uv.kn            # UV manipulation, distortions
│   └── filters.kn       # Image processing filters
│
├── volumetric/
│   ├── scattering.kn    # Phase functions, transmittance
│   ├── raymarching.kn   # Ray utilities, accumulation
│   └── sss.kn           # Subsurface scattering
│
└── procedural/
    ├── shapes.kn        # Basic shape generators
    ├── patterns.kn      # Tiling patterns, textures
    └── organic.kn       # Natural patterns (cracks, fibers)
```

---

## Cross-References

### Functions Used Together:

**PBR Rendering Chain:**
1. distribution_ggx
2. geometry_smith
3. fresnel_schlick
4. cook_torrance_brdf
→ Complete physically-based lighting

**Volumetric Rendering Chain:**
1. noise_3d / fbm (density)
2. henyey_greenstein (phase)
3. beer_lambert (transmittance)
4. light_march (shadows)
→ Complete volumetric clouds/fog

**Color Grading Chain:**
1. rgb_to_hsv
2. (manipulate HSV)
3. hsv_to_rgb
4. ACES tonemap
→ Complete color pipeline

**SSS Chain:**
1. Burley diffusion profile
2. wrap_lighting
3. beer_lambert (absorption)
4. fresnel_rim
→ Complete subsurface scattering

---

## Dependencies & Prerequisites

### Required Language Features:
- ✅ Pure functions with `with Pure`
- ✅ Vec2/Vec3/Vec4 types
- ✅ Float/Int types
- ✅ Math built-ins (sin, cos, pow, exp, etc.)
- ✅ Array types (for kernel weights)
- ⚠️ Loop support (for some filters - may need unrolling)

### Optional Enhancements:
- Generic functions (for Vec2/Vec3/Vec4 variants)
- Const expressions (for compile-time constants)
- Inline hints (for performance-critical functions)

---

## Estimated Extraction Effort

### By Category:

| Category | Functions | Complexity | Effort (hours) |
|----------|-----------|------------|----------------|
| PBR & Lighting | 15 | Medium | 8-10 |
| Noise & Procedural | 20+ | Medium | 10-12 |
| Color Grading | 25+ | Low-Medium | 6-8 |
| UV Manipulation | 8 | Low | 3-4 |
| Volumetric | 12 | High | 8-10 |
| SSS | 6 | High | 6-8 |
| Ray Marching | 8 | Medium | 4-6 |
| Generators | 21 | Low-Medium | 8-10 |
| Filters | 15 | Low-Medium | 6-8 |

**Total Estimated Effort:** 60-76 hours

### Parallelization Opportunities:
- Categories are independent
- Can extract 3-4 categories simultaneously
- Testing can overlap with extraction

---

## Quality Checklist

For each extracted function:

- [ ] Function signature matches stdlib conventions
- [ ] Pure function annotation (`with Pure`)
- [ ] Documentation comment with description
- [ ] Parameter descriptions
- [ ] Return value description
- [ ] Usage example in comment
- [ ] Reference to source algorithm/paper (if applicable)
- [ ] Unit test with known values
- [ ] Visual test shader
- [ ] Performance notes (if relevant)
- [ ] Edge case handling documented

---

## References & Citations

### PBR:
- Cook-Torrance BRDF (1982)
- GGX/Trowbridge-Reitz NDF (2007)
- Schlick's Fresnel approximation (1994)
- Smith's geometry function (1967)

### Volumetric:
- Henyey-Greenstein phase function (1941)
- Beer-Lambert law (1852)
- Schneider powder effect (2015)

### SSS:
- Burley diffusion profile (2015, Disney)
- Separable screen-space SSS (Jimenez 2015)

### Noise:
- Perlin noise (1985)
- Voronoi diagrams (1908)
- Fractal Brownian Motion

### Color:
- ACES tone mapping (Academy Color Encoding System)
- ASC-CDL (American Society of Cinematographers)
- Rec. 709 luminance coefficients

---

## Next Steps (Tasks 3.3-3.11)

1. **Task 3.3: Extract PBR functions** → `stdlib/graphics/pbr.kn`
2. **Task 3.4: Extract noise functions** → `stdlib/math/noise.kn`
3. **Task 3.5: Extract color grading** → `stdlib/graphics/color.kn`
4. **Task 3.6: Extract UV manipulation** → `stdlib/graphics/uv.kn`
5. **Task 3.7: Extract volumetric** → `stdlib/volumetric/scattering.kn`
6. **Task 3.8: Extract SSS** → `stdlib/volumetric/sss.kn`
7. **Task 3.9: Extract ray marching** → `stdlib/volumetric/raymarching.kn`
8. **Task 3.10: Extract procedural** → `stdlib/procedural/`
9. **Task 3.11: Extract filters** → `stdlib/graphics/filters.kn`

---

## Summary Statistics

- **Total Functions Identified:** 120+
- **Source Files Analyzed:** 11
- **Categories:** 9
- **Tier 1 (Essential):** ~25 functions
- **Tier 2 (High Value):** ~35 functions
- **Tier 3 (Specialized):** ~60 functions

**Analysis Complete: Ready for extraction phase.**
