# Materialize Shader Pipeline — Complete GPU Compute Architecture

> **Part 2 of Materialize Plugin Analysis**  
> **Focus:** Shader inventory, compute pipeline, RDG architecture, KAIN implementation strategy

---

## Executive Summary

Materialize uses a **24-shader GPU compute pipeline** spanning 5 categories:
1. **PBR Generation** (3-pass: Gradient → Height Integration → Final PBR)
2. **Filters** (blur, sharpen, edge detect, levels, HSL)
3. **Noise Generation** (15 noise types + procedural patterns)
4. **Blend Modes** (16 Photoshop-compatible blend modes)
5. **Preset Shaders** (12 specialized PBR shaders: Metal, Glossy, Toon)

**Key Architecture:**
- **RDG (Render Dependency Graph)** for all GPU operations
- **Multi-pass PBR pipeline** with Jacobi height integration (4-64 iterations)
- **Shared shader library** (`MaterializeProceduralCommon.ush`) with 20+ utility functions
- **KStudioCore** library with layer-based compositing system
- **RAII RDG scope** for automatic graph execution and cleanup

**Performance:** 8x8 thread groups, SM5.0 minimum, PF_R32_FLOAT for single-channel UAVs

---

## Shader Inventory (24 Files)

### Category 1: PBR Generation (Core Pipeline)

#### `PBRGenerator.usf` (423 lines, 16.5KB)
**Purpose:** Multi-pass PBR map generation from single image

**Kernels:**
- `GradientCS` — Extract luminance gradients (Sobel or multi-scale Poisson)
- `HeightIntegrationCS` — Jacobi iteration for height reconstruction from gradients
- `FinalPBRCS` — Generate Normal, Roughness, Metallic, AO, Height, Emissive
- `MainCS` — Legacy single-pass (fast preview, backward compat)

**Inputs:**
- `InSourceTexture` (Texture2D<float4>) — Source image
- `InGradient` (Texture2D<float2>) — Gradient field from Pass 1
- `InHeightPrev` (Texture2D<float>) — Previous height iteration

**Outputs:**
- `OutNormal` (RWTexture2D<float4>) — Tangent-space normal map
- `OutRoughness` (RWTexture2D<float>) — Roughness map
- `OutMetallic` (RWTexture2D<float>) — Metallic map
- `OutAO` (RWTexture2D<float>) — Ambient occlusion
- `OutHeight` (RWTexture2D<float>) — Height/displacement map
- `OutEmissive` (RWTexture2D<float>) — Emissive mask

**Parameters (30 scalars):**
```cpp
float NormalStrength;           // 0.0-2.0
float RoughnessBase;            // 0.0-1.0
float RoughnessContrast;        // 0.0-3.0
uint  bRoughnessInvert;         // bool
float MetallicBase;             // 0.0-1.0
float MetallicContrast;         // 0.0-3.0
float MetallicBias;             // -128 to 128
float MetallicSensitivity;      // 0.0-5.0
float AOIntensity;              // 0.0-2.0
float HeightContrast;           // 0.0-3.0
float BioDetail;                // 0.0-1.0 (organic patterns)
float BioFrequency;             // 0.1-5.0
float CyberDetail;              // 0.0-1.0 (tech patterns)
float CyberScale;               // 0.01-1.0
float EdgeWear;                 // 0.0-1.0 (weathering)
float CavityDirt;               // 0.0-1.0
float Dust;                     // 0.0-1.0
float Grunge;                   // 0.0-1.0
float Scratches;                // 0.0-1.0
float Noise;                    // 0.0-1.0
float EmissiveThreshold;        // 0.0-1.0
float EmissiveColorBoost;       // 0.0-3.0
float VarianceWeight;           // 0.0-1.0 (roughness variance blend)
uint2 TextureDimensions;
uint  bAdvancedNormal;          // Multi-scale Poisson vs Sobel
uint  bAdvancedAO;              // 8-direction horizon vs simple
int   NormalOctaves;            // 1-6 (multi-scale)
float NormalSigmaBase;          // 0.5-3.0
float NormalAnisotropy;         // 0.5-2.0
float AORadius;                 // 1.0-32.0
float AOBias;                   // -1.0 to 1.0
float AOContrast;               // 0.1-3.0
```

**Algorithm Details:**

**Pass 1: Gradient Extraction**
- Linearize sRGB → linear color space (`pow(c, 2.2)`)
- Extract luminance gradients using Sobel (3x3) or multi-scale Poisson (1-6 octaves)
- Multi-scale: `sigma = NormalSigmaBase * pow(2.0, octave)` for macro/meso/micro detail
- Anisotropy: `Grad.x *= NormalAnisotropy` for directional stretching

**Pass 2: Height Integration (Jacobi Iteration)**
- Poisson equation solver: `∇²h = ∇·g` (height from gradient divergence)
- Ping-pong buffers: `HeightPingRDG ↔ HeightPongRDG`
- Iterations: 4-64 (default 24), converges to height field
- Formula: `OutHeight[pos] = (hL + hR + hU + hD + Div) * 0.25`
- Divergence: `Div = (gR.x - gL.x + gD.y - gU.y) * 0.5`

**Pass 3: Final PBR Generation**
- **Normal:** Multi-scale blend (macro 50% + meso 30% + micro 20%)
  - Macro: Poisson height at 1px offset
  - Meso: Poisson height at 2px offset
  - Micro: Luminance Sobel at 1px (surface texture detail)
- **Roughness:** Luminance base + local variance (3x3 window)
  - Variance: `sqrt(E[L²] - E[L]²) * 8.0`
  - Blend: `lerp(RoughLum, RoughLum + Variance, VarianceWeight)`
- **Metallic:** Color-aware detection (high brightness + low saturation)
  - Score: `Lum * (1 - Sat) * MetallicSensitivity`
- **AO:** 8-direction horizon sampling (cardinal + diagonal)
  - Weights: cardinal=1.0, diagonal=0.707
  - Horizon angle per direction, accumulated occlusion
- **Emissive:** Brightness threshold + color saturation boost
  - Bright pixels: `(Lum - Threshold) * 10.0`
  - Saturated pixels: `(Sat - 0.6) * 5.0 * Lum * 2.0`

**Weathering Effects:**
- EdgeWear: Detected from Laplacian (high-frequency edges)
- CavityDirt: Detected from negative Laplacian (concave areas)
- Scratches: FBM noise with directional bias
- Grunge/Dust: Multi-octave FBM noise
- Bio: Sine/cosine organic patterns
- Cyber: Grid-based tech patterns

**C++ Binding:**
```cpp
class FKGradientCS : public FGlobalShader { ... };
class FKHeightIntegrationCS : public FGlobalShader { ... };
class FKFinalPBRCS : public FGlobalShader { ... };
class FKPBRGeneratorCS : public FGlobalShader { ... }; // Legacy single-pass

IMPLEMENT_GLOBAL_SHADER(FKGradientCS, "/Plugin/Materialize/PBRGenerator.usf", "GradientCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FKHeightIntegrationCS, "/Plugin/Materialize/PBRGenerator.usf", "HeightIntegrationCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FKFinalPBRCS, "/Plugin/Materialize/PBRGenerator.usf", "FinalPBRCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FKPBRGeneratorCS, "/Plugin/Materialize/PBRGenerator.usf", "MainCS", SF_Compute);
```

---

#### `SeamlessAndPacking.usf` (175 lines, 6.1KB)
**Purpose:** Seamless tiling and ORM channel packing

**Kernels:**
- `SeamlessCS` — Make textures seamlessly tileable
- `PackORMCS` — Pack AO/Roughness/Metallic into UE5 ORM format

**Seamless Modes:**
1. **Cross Blend (Mode 0):** Offset by 50%, blend center seam with diamond mask
2. **Mirror Blend (Mode 1):** Mirror edges, bilinear blend of 4 quadrants
3. **Histogram Match (Mode 2):** Cross blend + local contrast adjustment

**ORM Packing:** R=AO, G=Roughness, B=Metallic (UE5 standard)

**Parameters:**
```cpp
uint2 TextureDimensions;
float BlendWidth;    // 0.1-0.5 (edge blend width)
uint  TileMode;      // 0=Cross, 1=Mirror, 2=Histogram
```

**C++ Binding:**
```cpp
class FKSeamlessCS : public FGlobalShader { ... };
class FKPackORMCS : public FGlobalShader { ... };

IMPLEMENT_GLOBAL_SHADER(FKSeamlessCS, "/Plugin/Materialize/SeamlessAndPacking.usf", "SeamlessCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FKPackORMCS, "/Plugin/Materialize/SeamlessAndPacking.usf", "PackORMCS", SF_Compute);
```

---

### Category 2: Filters (Image Processing)

#### `MaterializeFilters.usf` (277 lines, 9.7KB)
**Purpose:** GPU-accelerated image filtering

**Kernels:**
- `BlurHorizontalCS` — Separable Gaussian blur (horizontal pass)
- `BlurVerticalCS` — Separable Gaussian blur (vertical pass)
- `SharpenCS` — Unsharp mask sharpening
- `EdgeDetectCS` — Sobel edge detection
- `LevelsCS` — Input/output levels adjustment
- `HSLAdjustCS` — Hue/Saturation/Lightness adjustment
- `InvertCS` — Color inversion
- `GrayscaleCS` — RGB to grayscale (Rec. 709 weights)

**Gaussian Blur:**
- 9-tap kernel with precomputed weights
- Separable (2-pass: horizontal + vertical)
- Weights: `[0.0162, 0.0540, 0.1216, 0.1933, 0.2258, 0.1933, 0.1216, 0.0540, 0.0162]`

**Sharpen:**
- 5-tap kernel: `center * 5 - (left + right + up + down)`
- Strength-based lerp with original

**Edge Detection:**
- Sobel operators: `gx = -tl - 2*ml - bl + tr + 2*mr + br`
- Magnitude: `sqrt(gx² + gy²)`
- Threshold-based masking

**Levels:**
- Input: `(color - inBlack) / (inWhite - inBlack)`
- Output: `adjusted * (outWhite - outBlack) + outBlack`

**HSL:**
- RGB → HSL conversion
- Hue shift (0-360°), saturation multiply, lightness multiply
- HSL → RGB conversion

**Parameters:**
```cpp
uint2 TextureSize;
float4 FilterParams;  // x=Strength, y=Radius, z=Threshold, w=unused
float4 FilterParams2; // x=InBlack, y=InWhite, z=OutBlack, w=OutWhite (Levels)
```

**C++ Binding:**
```cpp
class FMaterializeBlurHorizontalCS : public FGlobalShader { ... };
class FMaterializeBlurVerticalCS : public FGlobalShader { ... };
class FMaterializeSharpenCS : public FGlobalShader { ... };
class FMaterializeEdgeDetectCS : public FGlobalShader { ... };
class FMaterializeLevelsCS : public FGlobalShader { ... };
class FMaterializeHSLAdjustCS : public FGlobalShader { ... };
```

---

### Category 3: Noise Generation

#### `MaterializeNoiseGenerator.usf` (173 lines, 6.2KB)
**Purpose:** Procedural noise generation

**Kernels:**
- `GenerateNoiseCS` — Main noise dispatcher (5 types)
- `GenerateRadialCS` — Radial falloff patterns
- `GenerateCircleCS` — Circle/ellipse shapes
- `GenerateBricksCS` — Brick tiling pattern
- `GenerateDotsCS` — Polka dot pattern

**Noise Types:**
0. **Perlin** — Classic gradient noise with quintic interpolation
1. **Voronoi** — Cell edge detection (F2 - F1)
2. **Worley** — Cell distance field (F1)
3. **Ridged** — Inverted turbulence (sharp ridges)
4. **Turbulence** — Absolute value FBM

**Parameters:**
```cpp
float4 NoiseParams;      // x=Scale, y=Octaves, z=Persistence, w=Lacunarity
float4 NoiseParams2;     // x=Seed, y=Contrast, z=Brightness, w=Invert
float4 TransformParams;  // x=Rotation, y=ScaleX, z=ScaleY, w=OffsetX
float4 TransformParams2; // x=OffsetY, y=TilingMode, z=unused, w=unused
uint2  TextureSize;
uint   NoiseType;
```

**UV Transformations:**
- Rotation around center (0.5, 0.5)
- Non-uniform scale
- Offset
- Tiling modes: Clamp (0), Repeat (1), Mirror (2)

**C++ Binding:**
```cpp
class FMaterializeNoiseGeneratorCS : public FGlobalShader { ... };
IMPLEMENT_GLOBAL_SHADER(FMaterializeNoiseGeneratorCS, "/Plugin/Materialize/MaterializeNoiseGenerator.usf", "GenerateNoiseCS", SF_Compute);
```

---

#### `MaterializeProceduralCommon.ush` (305 lines, 8.5KB)
**Purpose:** Shared utility library for all shaders

**Hash Functions:**
- `hash11(float)` → float
- `hash21(float2)` → float
- `hash22(float2)` → float2
- `hash33(float3)` → float3

**Noise Functions:**
- `grad2(float2)` — 2D gradient noise with quintic interpolation
- `fbm(float2, octaves, lacunarity, persistence)` — Fractal Brownian Motion
- `perlin_noise(uv, scale, octaves, seed)` — Standard Perlin
- `voronoi(uv, scale, randomness, seed)` → float2 (F1, F2)
- `worley_noise(uv, scale, randomness, seed)` → float (F1)
- `voronoi_edges(uv, scale, edge_width, seed)` → float (F2 - F1)
- `turbulence(uv, scale, octaves, seed)` — Absolute value noise
- `ridged_noise(uv, scale, octaves, seed)` — Inverted turbulence with weight

**Geometric Patterns:**
- `radial_falloff(uv, falloff_power)` — Soft brush
- `circle(uv, radius, edge_softness)` — Hard circle with soft edge
- `square(uv, size, edge_softness)` — Square shape
- `diamond(uv, size, edge_softness)` — Rotated square

**Tiling Patterns:**
- `bricks(uv, brick_width, brick_height, mortar_width)` — Brick pattern with offset rows
- `dots(uv, dot_radius, spacing)` — Polka dots
- `hexagon(uv, scale)` — Hexagonal tiling

**UV Utilities:**
- `transform_uv(uv, rotation, scale, offset)` — Full UV transformation
- `apply_tiling(uv, mode)` — Tiling mode (Clamp/Repeat/Mirror)

**Usage:** Included by all procedural shaders via `#include "/Plugin/Materialize/MaterializeProceduralCommon.ush"`

---

### Category 4: Blend Modes

#### `MaterializeBlend.usf` (184 lines, 5.6KB)
**Purpose:** Photoshop-compatible blend modes

**Kernel:**
- `BlendTexturesCS` — Blend two textures with opacity

**Blend Modes (16 total):**
0. **Normal** — Replace
1. **Add** — `saturate(base + blend)`
2. **Subtract** — `saturate(base - blend)`
3. **Multiply** — `base * blend`
4. **Screen** — `1 - (1 - base) * (1 - blend)`
5. **Overlay** — Multiply if base < 0.5, Screen if base ≥ 0.5
6. **Soft Light** — Smooth overlay with sqrt for highlights
7. **Hard Light** — Overlay with swapped inputs
8. **Darken** — `min(base, blend)`
9. **Lighten** — `max(base, blend)`
10. **Difference** — `abs(base - blend)`
11. **Exclusion** — `base + blend - 2 * base * blend`
12. **Color Dodge** — `base / (1 - blend)`
13. **Color Burn** — `1 - (1 - base) / blend`
14. **Linear Light** — `saturate(base + 2 * blend - 1)`
15. **Vivid Light** — Color Burn if blend < 0.5, Color Dodge if blend ≥ 0.5

**Parameters:**
```cpp
Texture2D BaseTexture;
Texture2D BlendTexture;
RWTexture2D<float4> OutputTexture;
uint BlendMode;
float Opacity;
uint2 TextureSize;
```

**Alpha Compositing:**
- Effective opacity: `Opacity * blend.a`
- RGB: `lerp(base.rgb, blended.rgb, effectiveOpacity)`
- Alpha: `max(base.a, blend.a * Opacity)`

**C++ Binding:**
```cpp
class FMaterializeBlendCS : public FGlobalShader { ... };
IMPLEMENT_GLOBAL_SHADER(FMaterializeBlendCS, "/Plugin/Materialize/MaterializeBlend.usf", "BlendTexturesCS", SF_Compute);
```

---

### Category 5: KStudioCore Library (Layer System)

#### `KStudioCore/LayerBlend.usf` (234 lines, 8.4KB)
**Purpose:** Advanced layer compositing with mask support

**Kernel:**
- `BlendCS` — Blend two layers with optional mask

**Blend Modes (20 total):**
0-19: Normal, Multiply, Screen, Overlay, Soft Light, Hard Light, Add, Subtract, Difference, Exclusion, Darken, Lighten, Color Dodge, Color Burn, Linear Dodge, Linear Burn, Vivid Light, Linear Light, Pin Light, Hard Mix

**Alpha Compositing:**
- Mask support: `maskValue = InMask.Load()`, optional invert
- Effective opacity: `Opacity * maskValue`
- Final blend: `effectiveOpacity * blendAlpha`
- Standard "over" operator: `resultAlpha = base.a + blend.a * effectiveOpacity * (1 - base.a)`

**Parameters:**
```cpp
Texture2D<float4> InBase;
Texture2D<float4> InBlend;
Texture2D<float> InMask;
RWTexture2D<float4> OutResult;
uint BlendMode;
float Opacity;
uint bHasMask;
uint bInvertMask;
uint2 TextureDimensions;
```

**C++ Binding:**
```cpp
class FKLayerBlendCS : public FGlobalShader { ... };
IMPLEMENT_GLOBAL_SHADER(FKLayerBlendCS, "/Plugin/Materialize/KStudioCore/LayerBlend.usf", "BlendCS", SF_Compute);
```

---

#### `KStudioCore/LayerFilter.usf` (245 lines, 6.5KB)
**Purpose:** Convolution and morphological filters

**Kernel:**
- `FilterCS` — Filter dispatcher (13 types)

**Filter Types:**
0. **Box Blur** — Simple average in radius
1. **Gaussian Blur** — Weighted blur with exp(-d²/2σ²)
2. **Sharpen** — Center - BoxBlur, intensity blend
3. **Edge Detect** — Sobel gradient magnitude
4. **Emboss** — `(BR - TL) * 0.5 + 0.5`
5. **High Pass** — `(Center - GaussianBlur) * 0.5 + 0.5`
6. **Low Pass** — Gaussian blur
7. **Median** — 3x3 median filter (bubble sort on luminance)
8. **Dilate** — Max filter (morphological dilation)
9. **Erode** — Min filter (morphological erosion)
10. **Invert** — `1 - color`
11. **Normalize** — Stretch to 0-1 range
12. **Auto Levels** — Local contrast stretch

**Parameters:**
```cpp
Texture2D<float4> InSource;
RWTexture2D<float4> OutResult;
uint FilterType;
float Intensity;
int KernelSize;
float Threshold;
uint2 TextureDimensions;
```

**C++ Binding:**
```cpp
class FKFilterCS : public FGlobalShader { ... };
IMPLEMENT_GLOBAL_SHADER(FKFilterCS, "/Plugin/Materialize/KStudioCore/LayerFilter.usf", "FilterCS", SF_Compute);
```

---

#### `KStudioCore/LayerAdjustment.usf` (255 lines, 7.5KB)
**Purpose:** Color correction and grading

**Kernel:**
- `AdjustmentCS` — Adjustment dispatcher (9 types)

**Adjustment Types:**
0. **Levels** — Input/gamma/output levels
1. **Curves** — S-curve contrast adjustment
2. **HSV** — Hue shift, saturation, value adjust
3. **Brightness/Contrast** — Simple brightness + contrast
4. **Color Balance** — Shadow/highlight color shift
5. **Vibrance** — Smart saturation (boosts low-saturation colors more)
6. **Threshold** — Convert to B&W at threshold
7. **Posterize** — Reduce color levels (quantization)
8. **Gradient Map** — Grayscale to 3-color gradient

**Color Space Conversions:**
- `RGBtoHSV` / `HSVtoRGB`
- `RGBtoHSL` / `HSLtoRGB`

**Parameters:**
```cpp
Texture2D<float4> InSource;
RWTexture2D<float4> OutResult;
uint AdjustmentType;
float InputBlack, InputWhite, Gamma, OutputBlack, OutputWhite;
float HueShift, SaturationAdjust, ValueAdjust;
float Brightness, Contrast;
uint2 TextureDimensions;
```

**C++ Binding:**
```cpp
class FKAdjustmentCS : public FGlobalShader { ... };
IMPLEMENT_GLOBAL_SHADER(FKAdjustmentCS, "/Plugin/Materialize/KStudioCore/LayerAdjustment.usf", "AdjustmentCS", SF_Compute);
```

---

#### `KStudioCore/ProceduralNoise.usf` (436 lines, 12.0KB)
**Purpose:** Full noise library for texture generation

**Kernel:**
- `NoiseCS` — Noise dispatcher (16 types)

**Noise Types:**
0. **Perlin** — Gradient noise with quintic interpolation
1. **Simplex** — Simplex-like noise (faster than Perlin)
2. **Worley** — Voronoi cell distance
3. **FBM** — Fractal Brownian Motion (multi-octave Perlin)
4. **Turbulence** — Absolute value FBM
5. **Cellular** — Voronoi edges (F2 - F1)
6. **Gradient** — Linear ramp (4 directions)
7. **Checker** — Checkerboard pattern
8. **Brick** — Brick pattern with offset rows
9. **Herringbone** — (uses Brick implementation)
10. **Hexagon** — Hexagonal tiling
11. **Scratches** — Random line patterns
12. **Grunge** — Multi-layer FBM + Worley
13. **Rust** — FBM + turbulence + Worley edges
14. **Dust** — Random particle spots
15. **Voronoise** — 4D Voronoise (seamless tiling trick)

**Advanced Features:**
- **Seamless tiling:** 4D noise trick (uses Time parameter)
- **Voronoise4D:** Full 4D implementation with jitter control
- **Hash functions:** Hash11, Hash21, Hash22, Hash32

**Parameters:**
```cpp
RWTexture2D<float4> OutResult;
uint NoiseType;
float Scale;
int Octaves;
float Persistence, Lacunarity;
float2 Offset;
int Seed;
uint bSeamless;
uint2 TextureDimensions;
float Time;
```

**C++ Binding:**
```cpp
class FKProceduralNoiseCS : public FGlobalShader { ... };
IMPLEMENT_GLOBAL_SHADER(FKProceduralNoiseCS, "/Plugin/Materialize/KStudioCore/ProceduralNoise.usf", "NoiseCS", SF_Compute);
```

---

#### `KStudioCore/MathOperations.usf` (58 lines, 1.4KB)
**Purpose:** Simple per-pixel math operations

**Kernel:**
- `MathCS` — Math operation dispatcher

**Operations:**
0. **Add** — `ColorA + ColorB`
1. **Multiply** — `ColorA * ColorB`
2. **Lerp** — `lerp(ColorA, ColorB, Alpha)`

**Parameters:**
```cpp
Texture2D<float4> InTextureA, InTextureB;
RWTexture2D<float4> OutResult;
uint MathOperation;
float Alpha;
uint2 TextureDimensions;
```

**C++ Binding:**
```cpp
class FKMathOperationCS : public FGlobalShader { ... };
IMPLEMENT_GLOBAL_SHADER(FKMathOperationCS, "/Plugin/Materialize/KStudioCore/MathOperations.usf", "MathCS", SF_Compute);
```

---

### Category 6: Preset Shaders (PBR Lighting Models)

These are **pixel shaders** (not compute) designed for Material Functions.

#### Metal Preset (2 shaders)

**`MetalAnisotropicSpecular.usf` (67 lines, 2.4KB)**
- **Purpose:** Anisotropic specular for brushed metals
- **Algorithm:** GGX with separate tangent/bitangent roughness
- **Inputs:** normal, tangent, bitangent, view_dir, light_dir, roughness, anisotropy
- **Parameters:** `specular_intensity`, `specular_tint`, `anisotropy_direction`
- **Output:** Anisotropic specular color

**`MetalFresnelRim.usf` (41 lines, 1.1KB)**
- **Purpose:** Fresnel-based edge highlights
- **Algorithm:** Schlick Fresnel + rim lighting
- **Inputs:** normal, view_dir, roughness
- **Parameters:** `rim_intensity`, `rim_color`, `rim_power`, `edge_brightness`
- **Output:** Rim light color

---

#### Glossy Preset (3 shaders)

**`GlossyClearCoat.usf` (52 lines, 1.7KB)**
- **Purpose:** Clear coat layer with IOR-based reflections
- **Algorithm:** GGX + Schlick Fresnel with IOR
- **Inputs:** normal, view_dir, light_dir, clear_coat, clear_coat_roughness
- **Parameters:** `coat_ior`, `coat_tint`, `coat_intensity`
- **Output:** Clear coat specular

**`GlossyDualLobe.usf` (56 lines, 2.0KB)**
- **Purpose:** Dual-lobe specular (base + coat) with energy conservation
- **Algorithm:** Two GGX lobes with energy conservation
- **Inputs:** normal, view_dir, light_dir, base_roughness, coat_roughness, coat_amount
- **Parameters:** `base_color`, `coat_color`, `energy_conservation`
- **Output:** Combined base + coat specular

**`GlossySubsurface.usf` (49 lines, 1.4KB)**
- **Purpose:** Subsurface scattering approximation
- **Algorithm:** Wrap lighting + back scattering + ambient
- **Inputs:** normal, light_dir, view_dir, thickness
- **Parameters:** `sss_color`, `sss_strength`, `sss_distortion`, `sss_power`, `sss_scale`, `ambient_strength`
- **Output:** SSS color
- **Formula:** `front_sss + back_sss + ambient_sss`

---

#### Toon Preset (5 shaders)

**`ToonCelShading.usf` (58 lines, 1.7KB)**
- **Purpose:** Cel-shaded lighting with configurable bands
- **Algorithm:** Wrap lighting + band quantization + smoothstep
- **Inputs:** normal, light_dir, view_dir, albedo
- **Parameters:** `shadow_color`, `highlight_color`, `midtone_color`, `band_count`, `band_smoothness`, `wrap_amount`
- **Output:** Cel-shaded color
- **Bands:** Shadow (0-0.33), Midtone (0.33-0.67), Highlight (0.67-1.0)

**`ToonSpecular.usf` (45 lines, 1.3KB)**
- **Purpose:** Stepped specular highlights
- **Algorithm:** Phong specular + step quantization
- **Inputs:** normal, view_dir, light_dir, roughness
- **Parameters:** `specular_color`, `specular_size`, `specular_steps`, `specular_intensity`
- **Output:** Stepped specular

**`ToonRimLight.usf` (41 lines, 1.1KB)**
- **Purpose:** Hard-edge rim lighting
- **Algorithm:** Fresnel rim + threshold + smoothstep
- **Inputs:** normal, view_dir
- **Parameters:** `rim_color`, `rim_power`, `rim_intensity`, `rim_threshold`, `rim_smoothness`
- **Output:** Rim light color

**`ToonOutlineDetection.usf` (45 lines, 1.3KB)**
- **Purpose:** Depth/normal-based outline detection
- **Algorithm:** Depth diff + normal facing + smoothstep
- **Inputs:** uv, depth, normal, screen_size
- **Parameters:** `outline_color`, `outline_thickness`, `depth_threshold`, `normal_threshold`, `depth_weight`, `normal_weight`
- **Output:** Outline color with alpha

**`ToonConfigurableBands.usf` (57 lines, 1.8KB)**
- **Purpose:** Custom band positions and colors
- **Algorithm:** Custom band positions with smoothstep interpolation
- **Inputs:** ndl, band_positions (float4), band_colors (float4)
- **Parameters:** `band_count`, `band_smoothness`, `custom_bands`
- **Output:** Banded lighting

---

#### Shared Utility Shaders (3 shaders)

**`MaterializeFresnelSchlick.usf` (30 lines, 789B)**
- **Purpose:** Schlick approximation for Fresnel
- **Formula:** `F = F0 + (1 - F0) * (1 - VdH)^5`
- **Inputs:** view_dir, half_dir, f0
- **Output:** Fresnel term (float3)

**`MaterializeGGXDistribution.usf` (34 lines, 905B)**
- **Purpose:** GGX normal distribution function
- **Formula:** `D = α² / (π * (NdH² * (α² - 1) + 1)²)`
- **Inputs:** normal, half_dir, roughness
- **Output:** Distribution term (float)

**`MaterializeSmithVisibility.usf` (39 lines, 1.1KB)**
- **Purpose:** Smith visibility term for PBR
- **Formula:** `G = 0.5 / (λ_v + λ_l)` where `λ = NdL * sqrt(α² + (1 - α²) * NdV²)`
- **Inputs:** normal, view_dir, light_dir, roughness
- **Output:** Visibility term (float)

**C++ Binding (Preset Shaders):**
```cpp
// All preset shaders use SF_Pixel (not SF_Compute)
IMPLEMENT_GLOBAL_SHADER(FGlossyClearCoatShader, "/Plugin/Materialize/GlossyClearCoat.usf", "GlossyClearCoatPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FGlossyDualLobeShader, "/Plugin/Materialize/GlossyDualLobe.usf", "GlossyDualLobePS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FGlossySubsurfaceShader, "/Plugin/Materialize/GlossySubsurface.usf", "GlossySubsurfacePS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FMetalAnisotropicSpecularShader, "/Plugin/Materialize/MetalAnisotropicSpecular.usf", "MetalAnisotropicSpecularPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FMetalFresnelRimShader, "/Plugin/Materialize/MetalFresnelRim.usf", "MetalFresnelRimPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToonCelShadingShader, "/Plugin/Materialize/ToonCelShading.usf", "ToonCelShadingPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToonSpecularShader, "/Plugin/Materialize/ToonSpecular.usf", "ToonSpecularPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToonRimLightShader, "/Plugin/Materialize/ToonRimLight.usf", "ToonRimLightPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToonOutlineDetectionShader, "/Plugin/Materialize/ToonOutlineDetection.usf", "ToonOutlineDetectionPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FToonConfigurableBandsShader, "/Plugin/Materialize/ToonConfigurableBands.usf", "ToonConfigurableBandsPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FMaterializeFresnelSchlickShader, "/Plugin/Materialize/MaterializeFresnelSchlick.usf", "MaterializeFresnelSchlickPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FMaterializeGGXDistributionShader, "/Plugin/Materialize/MaterializeGGXDistribution.usf", "MaterializeGGXDistributionPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FMaterializeSmithVisibilityShader, "/Plugin/Materialize/MaterializeSmithVisibility.usf", "MaterializeSmithVisibilityPS", SF_Pixel);
```

---

## Compute Engine Architecture

### `MaterializeComputeEngine.cpp` (964 lines, 40KB)

**Class:** `UMaterializeComputeEngine : public UObject`

**Public API:**
```cpp
// Main PBR generation (multi-pass)
static bool GeneratePBRMapsGPU(UTexture2D* SourceTexture, const FMaterializeParams& Params, FMaterializeResult& OutResult);

// Seamless tiling
static UTexture2D* MakeSeamless(UTexture2D* SourceTexture, EKSeamlessMode Mode, float BlendWidth = 0.25f);

// ORM packing
static UTexture2D* PackORM(UTexture2D* AO, UTexture2D* Roughness, UTexture2D* Metallic);

// GPU → CPU readback
static bool ReadbackTexture(UTexture2D* Texture, TArray<FColor>& OutPixels);
static void ReadbackResult(const FMaterializeResult& Result, TMap<FString, TArray<FColor>>& OutMap);

// Resource management
static void CleanupTransientResources(FMaterializeResult& Result);
static bool ValidateRHIResource(FTexture2DRHIRef TextureRHI, FString& OutError);
```

---

### RDG Pipeline Flow (GeneratePBRMapsGPU)

**Step 1: Texture Validation & Preparation**
```cpp
// Fallback to WhiteSquareTexture if source invalid
UTexture2D* SafeSource = SourceTexture ? SourceTexture : LoadObject<UTexture2D>(..., TEXT("/Engine/EngineResources/WhiteSquareTexture"));
SafeSource->WaitForStreaming();
FlushRenderingCommands();

// Capture RHI references
FTextureResource* SourceResource = SafeSource->GetResource();
FTexture2DRHIRef SourceRHI = SourceResource->GetTexture2DRHI();
```

**Step 2: Output Texture Creation (Smart Reuse)**
```cpp
// Reuse existing textures if dimensions/format match (VRAM optimization)
auto GetOrResize = [&](TObjectPtr<UTexture2D>& Tex, EPixelFormat Format, bool bSRGB) {
    if (Tex && Tex->GetSizeX() == Width && Tex->GetSizeY() == Height && Tex->GetPixelFormat() == Format)
        return; // Reuse
    Tex = UTexture2D::CreateTransient(Width, Height, Format);
    Tex->SRGB = bSRGB;
    Tex->UpdateResource();
};

// NOTE: D3D12 UAVs do not support PF_B8G8R8A8 (BGRA). Must use PF_R8G8B8A8 (RGBA).
GetOrResize(OutResult.Normal, PF_R8G8B8A8, false);
GetOrResize(OutResult.Roughness, PF_R32_FLOAT, false); // RWTexture2D<float> requires PF_R32_FLOAT
GetOrResize(OutResult.Metallic, PF_R32_FLOAT, false);
GetOrResize(OutResult.AO, PF_R32_FLOAT, false);
GetOrResize(OutResult.Height, PF_R32_FLOAT, false);
GetOrResize(OutResult.Emissive, PF_R32_FLOAT, false);
if (Params.bPackORM) GetOrResize(OutResult.ORM, PF_R8G8B8A8, false);
```

**Step 3: RDG Graph Construction**
```cpp
ENQUEUE_RENDER_COMMAND(KSampleGenGPU_MultiPass)(
    [SourceRHI, NormalRHI, RoughRHI, MetalRHI, AORHI, HeightRHI, EmissiveRHI, ORMRHI, Params, ...]
    (FRHICommandListImmediate& RHICmdList)
    {
        // Transition source to SRV
        RHICmdList.Transition(FRHITransitionInfo(SourceRHI, ERHIAccess::Unknown, ERHIAccess::SRVCompute));

        // Create RDG builder
        FRDGBuilder ComputeGraphBuilder(RHICmdList);
        
        // Register external input
        FRDGTextureRef ComputeInputRDG = ComputeGraphBuilder.RegisterExternalTexture(
            CreateRenderTarget(SourceRHI, TEXT("SourceInput")));
        FRDGTextureSRVRef InputSRV = ComputeGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(ComputeInputRDG));
        
        // Create intermediate buffers (pure RDG)
        FRDGTextureDesc GradientDesc = FRDGTextureDesc::Create2D(Size, PF_G32R32F, FClearValueBinding::Transparent, 
            TexCreate_UAV | TexCreate_ShaderResource);
        FRDGTextureRef GradientRDG = ComputeGraphBuilder.CreateTexture(GradientDesc, TEXT("GradientMap"));
        
        FRDGTextureDesc HeightDesc = FRDGTextureDesc::Create2D(Size, PF_R32_FLOAT, FClearValueBinding::Transparent,
            TexCreate_UAV | TexCreate_ShaderResource);
        FRDGTextureRef HeightPingRDG = ComputeGraphBuilder.CreateTexture(HeightDesc, TEXT("HeightPing"));
        FRDGTextureRef HeightPongRDG = ComputeGraphBuilder.CreateTexture(HeightDesc, TEXT("HeightPong"));
        
        // Create output RDG textures
        FRDGTextureRef NormalRDG = CreateOutputRDG(TEXT("OutNormal"), PF_R8G8B8A8);
        FRDGTextureRef RoughRDG = CreateOutputRDG(TEXT("OutRough"), PF_R32_FLOAT);
        // ... etc
```

**Step 4: Pass 1 — Gradient Extraction**
```cpp
{
    TShaderMapRef<FKGradientCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
    FKGradientCS::FParameters* PassParams = ComputeGraphBuilder.AllocParameters<FKGradientCS::FParameters>();
    PassParams->InSourceTexture = InputSRV;
    PassParams->InSourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    PassParams->OutGradient = ComputeGraphBuilder.CreateUAV(GradientRDG);
    PassParams->NormalStrength = Params.NormalStrength;
    PassParams->TextureDimensions = FUintVector2(Width, Height);
    // ... 30+ parameter bindings ...
    
    FComputeShaderUtils::AddPass(ComputeGraphBuilder, RDG_EVENT_NAME("KSample_Gradient"), Shader, PassParams, GroupCount);
}
```

**Step 5: Pass 2 — Height Integration (Jacobi Iteration)**
```cpp
if (bUseMultiPass)
{
    AddClearUAVPass(ComputeGraphBuilder, ComputeGraphBuilder.CreateUAV(HeightPingRDG), 0.5f);
    
    for(int32 i = 0; i < HeightIterations; i++)
    {
        TShaderMapRef<FKHeightIntegrationCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        FKHeightIntegrationCS::FParameters* PassParams = ComputeGraphBuilder.AllocParameters<FKHeightIntegrationCS::FParameters>();
        PassParams->InGradient = ComputeGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(GradientRDG));
        PassParams->InHeightPrev = ComputeGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(
            (i % 2 == 0) ? HeightPingRDG : HeightPongRDG));
        PassParams->OutHeightNext = ComputeGraphBuilder.CreateUAV(
            (i % 2 == 0) ? HeightPongRDG : HeightPingRDG);
        // ... parameter bindings ...
        
        FComputeShaderUtils::AddPass(ComputeGraphBuilder, RDG_EVENT_NAME("KSample_HeightIter_%d", i), Shader, PassParams, GroupCount);
    }
}
```

**Step 6: Pass 3 — Final PBR Generation**
```cpp
{
    TShaderMapRef<FKFinalPBRCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
    FKFinalPBRCS::FParameters* PassParams = ComputeGraphBuilder.AllocParameters<FKFinalPBRCS::FParameters>();
    PassParams->InSourceTexture = InputSRV;
    PassParams->InSourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    
    // Fix Ping-Pong Logic: If 24 iterations, last write is to PING. Read PING.
    FRDGTextureRef InputHeight = (HeightIterations % 2 == 0) ? HeightPingRDG : HeightPongRDG;
    PassParams->InHeightPrev = ComputeGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(InputHeight));
    
    PassParams->OutNormal = ComputeGraphBuilder.CreateUAV(NormalRDG);
    PassParams->OutRoughness = ComputeGraphBuilder.CreateUAV(RoughRDG);
    PassParams->OutMetallic = ComputeGraphBuilder.CreateUAV(MetalRDG);
    PassParams->OutAO = ComputeGraphBuilder.CreateUAV(AORDG);
    PassParams->OutHeight = ComputeGraphBuilder.CreateUAV(HeightOutRDG);
    PassParams->OutEmissive = ComputeGraphBuilder.CreateUAV(EmissiveRDG);
    // ... 30+ parameter bindings ...
    
    FComputeShaderUtils::AddPass(ComputeGraphBuilder, RDG_EVENT_NAME("KSample_FinalPBR"), Shader, PassParams, GroupCount);
}
```

**Step 7: Optional Seamless Tiling**
```cpp
if (Params.bMakeSeamless)
{
    auto MakeSeamlessPass = [&](FRDGTextureRef Input, const TCHAR* Name) -> FRDGTextureRef
    {
        FRDGTextureRef Output = CreateOutputRDG(Name, Input->Desc.Format);
        TShaderMapRef<FKSeamlessCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        FKSeamlessCS::FParameters* PassParams = ComputeGraphBuilder.AllocParameters<FKSeamlessCS::FParameters>();
        PassParams->InSource = ComputeGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(Input));
        PassParams->OutSeamless = ComputeGraphBuilder.CreateUAV(Output);
        PassParams->TileMode = ModeInt; // 0=Cross, 1=Mirror, 2=Histogram
        PassParams->BlendWidth = Params.SeamlessBlendWidth;
        FComputeShaderUtils::AddPass(ComputeGraphBuilder, RDG_EVENT_NAME("KSample_Seamless"), Shader, PassParams, GroupCount);
        return Output;
    };
    
    FinalNormal = MakeSeamlessPass(NormalRDG, TEXT("SeamlessNormal"));
    FinalRough = MakeSeamlessPass(RoughRDG, TEXT("SeamlessRoughness"));
    // ... apply to all 6 maps
}
```

**Step 8: Optional ORM Packing**
```cpp
if (Params.bPackORM && ORMRHI.IsValid())
{
    ORMRDG = CreateOutputRDG(TEXT("ORMMap"), PF_R8G8B8A8);
    TShaderMapRef<FKPackORMCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
    FKPackORMCS::FParameters* PassParams = ComputeGraphBuilder.AllocParameters<FKPackORMCS::FParameters>();
    PassParams->InAO = ComputeGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(FinalAO));
    PassParams->InRoughness = ComputeGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(FinalRough));
    PassParams->InMetallic = ComputeGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(FinalMetal));
    PassParams->OutORM = ComputeGraphBuilder.CreateUAV(ORMRDG);
    FComputeShaderUtils::AddPass(ComputeGraphBuilder, RDG_EVENT_NAME("KSample_PackORM"), Shader, PassParams, GroupCount);
}
```

**Step 9: Extraction Queue**
```cpp
TRefCountPtr<IPooledRenderTarget> ExtNormal, ExtRough, ExtMetal, ExtAO, ExtHeight, ExtEmissive, ExtORM;

ComputeGraphBuilder.QueueTextureExtraction(FinalNormal, &ExtNormal);
ComputeGraphBuilder.QueueTextureExtraction(FinalRough, &ExtRough);
ComputeGraphBuilder.QueueTextureExtraction(FinalMetal, &ExtMetal);
ComputeGraphBuilder.QueueTextureExtraction(FinalAO, &ExtAO);
ComputeGraphBuilder.QueueTextureExtraction(FinalHeight, &ExtHeight);
ComputeGraphBuilder.QueueTextureExtraction(FinalEmissive, &ExtEmissive);
if (ORMRDG) ComputeGraphBuilder.QueueTextureExtraction(ORMRDG, &ExtORM);

// EXECUTE GRAPH
ComputeGraphBuilder.Execute();
```

**Step 10: Copy to External Textures**
```cpp
auto SafeCopy = [&](TRefCountPtr<IPooledRenderTarget> Src, FTexture2DRHIRef Dst) {
    if (Src.IsValid() && Dst.IsValid() && Dst->GetNativeResource())
    {
        RHICmdList.Transition(FRHITransitionInfo(Dst, ERHIAccess::Unknown, ERHIAccess::CopyDest));
        FRHICopyTextureInfo CopyInfo;
        RHICmdList.CopyTexture(Src->GetRHI(), Dst, CopyInfo);
        RHICmdList.Transition(FRHITransitionInfo(Dst, ERHIAccess::CopyDest, ERHIAccess::SRVGraphics)); // Ready for UI
    }
};

SafeCopy(ExtNormal, NormalRHI);
SafeCopy(ExtRough, RoughRHI);
// ... copy all 6-7 maps
```

**Step 11: Finalization**
```cpp
FlushRenderingCommands(); // Ensure completion before UI access
OutResult.GenerationTimeMs = (EndTime - StartTime) * 1000.0f;
return true;
```

---

### RDG RAII Wrapper (`MaterializeRDGScope.h`)

**Purpose:** Automatic RDG graph execution and cleanup

```cpp
class FMaterializeRDGScope
{
public:
    explicit FMaterializeRDGScope(FRHICommandListImmediate& InRHICmdList)
        : RHICmdList(InRHICmdList)
        , GraphBuilder(InRHICmdList)
        , bExecuted(false)
    {}
    
    ~FMaterializeRDGScope()
    {
        if (!bExecuted) Execute();
    }
    
    FRDGBuilder& GetGraphBuilder() { return GraphBuilder; }
    
    void Execute()
    {
        if (!bExecuted)
        {
            GraphBuilder.Execute();
            bExecuted = true;
        }
    }
    
private:
    FRHICommandListImmediate& RHICmdList;
    FRDGBuilder GraphBuilder;
    bool bExecuted;
};
```

**Usage:**
```cpp
FMaterializeRDGScope RDGScope(RHICmdList);
FRDGBuilder& GraphBuilder = RDGScope.GetGraphBuilder();
// ... add RDG passes ...
// Automatic execution on scope exit (destructor)
```

---

## Shader Dependency Graph

```
MaterializeProceduralCommon.ush (Shared Library)
    ├─→ MaterializeNoiseGenerator.usf (includes Common.ush)
    └─→ (used by all procedural shaders)

/Engine/Public/Platform.ush (UE5 Core)
    ├─→ PBRGenerator.usf
    ├─→ SeamlessAndPacking.usf
    ├─→ KStudioCore/LayerAdjustment.usf
    ├─→ KStudioCore/LayerBlend.usf
    ├─→ KStudioCore/LayerFilter.usf
    ├─→ KStudioCore/ProceduralNoise.usf
    ├─→ KStudioCore/MathOperations.usf
    └─→ All preset shaders (Metal, Glossy, Toon)

/Engine/Private/Common.ush (UE5 Private)
    ├─→ MaterializeBlend.usf
    └─→ MaterializeFilters.usf
```

**Include Hierarchy:**
- **Platform.ush** — Core UE5 shader platform abstraction
- **Common.ush** — Common shader utilities (saturate, lerp, etc.)
- **MaterializeProceduralCommon.ush** — Plugin-specific shared library

---

## Parameter Binding Patterns

### Compute Shaders (RDG)

**Texture Inputs (SRV):**
```cpp
SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InSourceTexture)
SHADER_PARAMETER_SAMPLER(SamplerState, InSourceSampler)
```

**Texture Outputs (UAV):**
```cpp
SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutNormal)
SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutRoughness)
```

**Scalar Parameters:**
```cpp
SHADER_PARAMETER(float, NormalStrength)
SHADER_PARAMETER(uint32, bRoughnessInvert)
SHADER_PARAMETER(FUintVector2, TextureDimensions)
SHADER_PARAMETER(FVector4f, NoiseParams)
```

### Pixel Shaders (Material Functions)

**Interpolators (FPSInput):**
```cpp
struct FPSInput
{
    float4 TexCoord0 : TEXCOORD0; // normal.xyz, roughness.w
    float4 TexCoord1 : TEXCOORD1; // view_dir.xyz, coat_roughness.w
    float4 TexCoord2 : TEXCOORD2; // light_dir.xyz, coat_amount.w
    float4 TexCoord3 : TEXCOORD3; // albedo.xyz, thickness.w
    // ... up to 6 interpolators for complex shaders
};
```

**Scalar Parameters (Uniform):**
```cpp
float coat_ior;
float3 coat_tint;
float coat_intensity;
```

**Output:**
```cpp
struct FPSOutput
{
    float4 Color : SV_Target0;
};
```

**C++ Binding (Pixel Shaders):**
```cpp
BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(float, coat_ior)
    SHADER_PARAMETER(FVector3f, coat_tint)
    SHADER_PARAMETER(float, coat_intensity)
    RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

static void Exec(FRDGBuilder& GraphBuilder, FRDGTextureRef OutputTexture, const FParameters& Parameters)
{
    TShaderMapRef<FGlossyClearCoatShader> PixelShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
    FParameters* PassParameters = GraphBuilder.AllocParameters<FParameters>();
    *PassParameters = Parameters;
    PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ENoAction);
    
    const FIntRect OutputRect(FIntPoint::ZeroValue, OutputTexture->Desc.Extent);
    FPixelShaderUtils::AddFullscreenPass(GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel),
        RDG_EVENT_NAME("Materialize.GlossyClearCoat"), PixelShader, PassParameters, OutputRect);
}
```

---

## Resource Management

### Texture Format Strategy

| Map | Format | Reason |
|-----|--------|--------|
| Normal | `PF_R8G8B8A8` | RGBA, not BGRA (D3D12 UAV compatibility) |
| Roughness | `PF_R32_FLOAT` | Single-channel, matches `RWTexture2D<float>` |
| Metallic | `PF_R32_FLOAT` | Single-channel |
| AO | `PF_R32_FLOAT` | Single-channel |
| Height | `PF_R32_FLOAT` | Single-channel |
| Emissive | `PF_R32_FLOAT` | Single-channel |
| ORM | `PF_R8G8B8A8` | Packed 3-channel |
| Gradient | `PF_G32R32F` | 2-channel float (intermediate) |

**Critical Notes:**
- **D3D12 UAVs do not support PF_B8G8R8A8** — must use PF_R8G8B8A8
- **Single-channel UAVs require PF_R32_FLOAT** — not PF_R16F or PF_R8
- **Intermediate buffers use pure RDG** — no external texture registration

### Texture Reuse Pattern

```cpp
auto GetOrResize = [&](TObjectPtr<UTexture2D>& Tex, EPixelFormat Format, bool bSRGB) {
    if (Tex && Tex->GetSizeX() == Width && Tex->GetSizeY() == Height && Tex->GetPixelFormat() == Format)
    {
        return; // Reuse existing texture (VRAM optimization)
    }
    Tex = UTexture2D::CreateTransient(Width, Height, Format);
    Tex->SRGB = bSRGB;
    Tex->UpdateResource();
};
```

**Benefits:**
- Avoids VRAM thrashing on repeated generations
- Reuses textures if dimensions/format match
- Only recreates when necessary

### RHI Validation

```cpp
static bool ValidateRHIResource(FTexture2DRHIRef TextureRHI, FString& OutError)
{
    if (!TextureRHI.IsValid())
    {
        OutError = TEXT("RHI texture reference is invalid");
        return false;
    }
    
    if (!TextureRHI->GetNativeResource())
    {
        OutError = TEXT("RHI texture has no native resource");
        return false;
    }
    
    FIntPoint Size = TextureRHI->GetSizeXY();
    if (Size.X <= 0 || Size.Y <= 0)
    {
        OutError = FString::Printf(TEXT("RHI texture has invalid dimensions: %dx%d"), Size.X, Size.Y);
        return false;
    }
    
    return true;
}
```

**Usage:** Called before every RDG operation to prevent crashes

---

## Thread Group Configuration

**All compute shaders use 8x8 thread groups:**
```cpp
[numthreads(8, 8, 1)]
void ShaderCS(uint3 ThreadId : SV_DispatchThreadID)
{
    if (ThreadId.x >= TextureDimensions.x || ThreadId.y >= TextureDimensions.y) return;
    // ... shader logic
}
```

**Dispatch calculation:**
```cpp
FIntPoint Size(Width, Height);
FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(Size, FIntPoint(8, 8));
// GroupCount.X = DivideAndRoundUp(Width, 8)
// GroupCount.Y = DivideAndRoundUp(Height, 8)
// GroupCount.Z = 1
```

**Why 8x8:**
- Optimal for GPU cache (64 threads per warp/wavefront)
- Balances occupancy and register pressure
- Standard for image processing compute shaders

---

## Shader Directory Mapping

**Plugin virtual path:** `/Plugin/Materialize/`

**Physical path:** `Plugins/Materialize/Shaders/`

**Mapping registration (in module startup):**
```cpp
FString ShaderDirectory = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("Materialize"))->GetBaseDir(), TEXT("Shaders"));
AddShaderSourceDirectoryMapping(TEXT("/Plugin/Materialize"), ShaderDirectory);
```

**Include paths in shaders:**
- `/Plugin/Materialize/MaterializeProceduralCommon.ush`
- `/Plugin/Materialize/PBRGenerator.usf`
- `/Plugin/Materialize/KStudioCore/LayerBlend.usf`
- `/Engine/Public/Platform.ush`
- `/Engine/Private/Common.ush`

---

## Complete Shader Inventory Table

| File | Type | Lines | Size | Kernels | Purpose |
|------|------|-------|------|---------|---------|
| **PBR Generation** |
| `PBRGenerator.usf` | Compute | 423 | 16.5KB | 4 | Multi-pass PBR generation |
| `SeamlessAndPacking.usf` | Compute | 175 | 6.1KB | 2 | Seamless tiling + ORM packing |
| **Filters** |
| `MaterializeFilters.usf` | Compute | 277 | 9.7KB | 8 | Blur, sharpen, edge, levels, HSL |
| **Noise** |
| `MaterializeNoiseGenerator.usf` | Compute | 173 | 6.2KB | 5 | Procedural noise generation |
| `MaterializeProceduralCommon.ush` | Library | 305 | 8.5KB | N/A | Shared noise/pattern utilities |
| **Blend** |
| `MaterializeBlend.usf` | Compute | 184 | 5.6KB | 1 | 16 Photoshop blend modes |
| **KStudioCore** |
| `KStudioCore/LayerBlend.usf` | Compute | 234 | 8.4KB | 1 | 20 blend modes + mask |
| `KStudioCore/LayerFilter.usf` | Compute | 245 | 6.5KB | 1 | 13 filter types |
| `KStudioCore/LayerAdjustment.usf` | Compute | 255 | 7.5KB | 1 | 9 adjustment types |
| `KStudioCore/ProceduralNoise.usf` | Compute | 436 | 12.0KB | 1 | 16 noise types |
| `KStudioCore/MathOperations.usf` | Compute | 58 | 1.4KB | 1 | 3 math operations |
| **Metal Preset** |
| `MetalAnisotropicSpecular.usf` | Pixel | 67 | 2.4KB | 1 | Anisotropic specular |
| `MetalFresnelRim.usf` | Pixel | 41 | 1.1KB | 1 | Fresnel rim lighting |
| **Glossy Preset** |
| `GlossyClearCoat.usf` | Pixel | 52 | 1.7KB | 1 | Clear coat layer |
| `GlossyDualLobe.usf` | Pixel | 56 | 2.0KB | 1 | Dual-lobe specular |
| `GlossySubsurface.usf` | Pixel | 49 | 1.4KB | 1 | Subsurface scattering |
| **Toon Preset** |
| `ToonCelShading.usf` | Pixel | 58 | 1.7KB | 1 | Cel-shaded lighting |
| `ToonSpecular.usf` | Pixel | 45 | 1.3KB | 1 | Stepped specular |
| `ToonRimLight.usf` | Pixel | 41 | 1.1KB | 1 | Hard-edge rim |
| `ToonOutlineDetection.usf` | Pixel | 45 | 1.3KB | 1 | Depth/normal outlines |
| `ToonConfigurableBands.usf` | Pixel | 57 | 1.8KB | 1 | Custom band positions |
| **Shared Utilities** |
| `MaterializeFresnelSchlick.usf` | Pixel | 30 | 789B | 1 | Schlick Fresnel |
| `MaterializeGGXDistribution.usf` | Pixel | 34 | 905B | 1 | GGX distribution |
| `MaterializeSmithVisibility.usf` | Pixel | 39 | 1.1KB | 1 | Smith visibility |
| **Total** | | **3,419** | **104KB** | **57** | |

---

## KAIN Shader Implementation Plan

### Strategy: Consolidate into 5 Core KAIN Shaders

**Goal:** Reduce 24 shader files to 5 KAIN shaders with permutations

### KAIN Shader 1: `pbr_generator.kn`

**Consolidates:** `PBRGenerator.usf`, `SeamlessAndPacking.usf`

**KAIN Syntax:**
```kain
shader compute PBRGradient(thread_id: Vec3):
    uniform source_texture: Sampler2D @0
    uniform normal_strength: Float @1
    uniform texture_dimensions: Vec2 @2
    uniform advanced_normal: Bool @3
    uniform normal_octaves: Int @4
    uniform normal_sigma_base: Float @5
    uniform normal_anisotropy: Float @6
    buffer out_gradient: RWBuffer<Vec2> @7
    
    let pos = vec2(thread_id.x as Float, thread_id.y as Float)
    let uv = pos / texture_dimensions
    let source_lin = pow(sample(source_texture, uv).rgb, vec3(2.2, 2.2, 2.2))
    let lum = dot(source_lin, vec3(0.2126, 0.7152, 0.0722))
    
    var grad = vec2(0.0, 0.0)
    if advanced_normal:
        for k in 0..normal_octaves:
            let sigma = normal_sigma_base * pow(2.0, k as Float)
            let offset = max(1, round(sigma) as Int)
            // Multi-scale gradient extraction
            grad = grad + compute_gradient_at_scale(pos, offset, texture_dimensions)
    else:
        // Sobel gradient
        grad = sobel_gradient(pos, texture_dimensions)
    
    grad.x = grad.x * normal_anisotropy
    out_gradient[thread_id.x + thread_id.y * texture_dimensions.x as Int] = grad * normal_strength * 0.25

shader compute HeightIntegration(thread_id: Vec3):
    buffer in_gradient: Buffer<Vec2> @0
    buffer in_height_prev: Buffer<Float> @1
    buffer out_height_next: RWBuffer<Float> @2
    uniform texture_dimensions: Vec2 @3
    
    let pos = vec2(thread_id.x as Float, thread_id.y as Float)
    let idx = thread_id.x + thread_id.y * texture_dimensions.x as Int
    
    // Jacobi iteration
    let hL = sample_height_safe(pos + vec2(-1.0, 0.0), in_height_prev, texture_dimensions)
    let hR = sample_height_safe(pos + vec2(1.0, 0.0), in_height_prev, texture_dimensions)
    let hU = sample_height_safe(pos + vec2(0.0, -1.0), in_height_prev, texture_dimensions)
    let hD = sample_height_safe(pos + vec2(0.0, 1.0), in_height_prev, texture_dimensions)
    
    let gL = sample_gradient_safe(pos + vec2(-1.0, 0.0), in_gradient, texture_dimensions)
    let gR = sample_gradient_safe(pos + vec2(1.0, 0.0), in_gradient, texture_dimensions)
    let gU = sample_gradient_safe(pos + vec2(0.0, -1.0), in_gradient, texture_dimensions)
    let gD = sample_gradient_safe(pos + vec2(0.0, 1.0), in_gradient, texture_dimensions)
    
    let div = (gR.x - gL.x + gD.y - gU.y) * 0.5
    out_height_next[idx] = (hL + hR + hU + hD + div) * 0.25

shader compute FinalPBR(thread_id: Vec3):
    uniform source_texture: Sampler2D @0
    buffer in_height: Buffer<Float> @1
    buffer out_normal: RWBuffer<Vec4> @2
    buffer out_roughness: RWBuffer<Float> @3
    buffer out_metallic: RWBuffer<Float> @4
    buffer out_ao: RWBuffer<Float> @5
    buffer out_height: RWBuffer<Float> @6
    buffer out_emissive: RWBuffer<Float> @7
    uniform normal_strength: Float @8
    uniform roughness_base: Float @9
    uniform roughness_contrast: Float @10
    uniform metallic_base: Float @11
    uniform metallic_contrast: Float @12
    uniform metallic_sensitivity: Float @13
    uniform ao_intensity: Float @14
    uniform height_contrast: Float @15
    uniform texture_dimensions: Vec2 @16
    // ... 30+ uniforms
    
    let pos = vec2(thread_id.x as Float, thread_id.y as Float)
    let idx = thread_id.x + thread_id.y * texture_dimensions.x as Int
    
    // Multi-scale normal
    let normal = compute_multiscale_normal(pos, in_height, normal_strength, texture_dimensions)
    out_normal[idx] = vec4(normal * 0.5 + vec3(0.5, 0.5, 0.5), 1.0)
    
    // Roughness with variance
    let roughness = compute_roughness_with_variance(pos, source_texture, roughness_base, roughness_contrast, texture_dimensions)
    out_roughness[idx] = roughness
    
    // Color-aware metallic
    let metallic = compute_metallic_color_aware(pos, source_texture, metallic_base, metallic_contrast, metallic_sensitivity, texture_dimensions)
    out_metallic[idx] = metallic
    
    // 8-direction horizon AO
    let ao = compute_horizon_ao(pos, in_height, ao_intensity, texture_dimensions)
    out_ao[idx] = ao
    
    // Height + emissive
    let height = sample_height_safe(pos, in_height, texture_dimensions)
    out_height[idx] = (height - 0.5) * height_contrast + 0.5
    
    let emissive = compute_emissive(pos, source_texture, emissive_threshold, emissive_color_boost, texture_dimensions)
    out_emissive[idx] = emissive

shader compute Seamless(thread_id: Vec3):
    uniform source_texture: Sampler2D @0
    buffer out_seamless: RWBuffer<Vec4> @1
    uniform texture_dimensions: Vec2 @2
    uniform blend_width: Float @3
    uniform tile_mode: Int @4
    
    let pos = vec2(thread_id.x as Float, thread_id.y as Float)
    let uv = pos / texture_dimensions
    
    var result = vec4(0.0, 0.0, 0.0, 1.0)
    match tile_mode:
        0 => result = cross_blend_seamless(pos, source_texture, blend_width, texture_dimensions)
        1 => result = mirror_blend_seamless(pos, source_texture, blend_width, texture_dimensions)
        2 => result = histogram_match_seamless(pos, source_texture, blend_width, texture_dimensions)
        _ => result = cross_blend_seamless(pos, source_texture, blend_width, texture_dimensions)
    
    let idx = thread_id.x + thread_id.y * texture_dimensions.x as Int
    out_seamless[idx] = result

shader compute PackORM(thread_id: Vec3):
    buffer in_ao: Buffer<Float> @0
    buffer in_roughness: Buffer<Float> @1
    buffer in_metallic: Buffer<Float> @2
    buffer out_orm: RWBuffer<Vec4> @3
    uniform texture_dimensions: Vec2 @4
    
    let idx = thread_id.x + thread_id.y * texture_dimensions.x as Int
    let ao = in_ao[idx]
    let roughness = in_roughness[idx]
    let metallic = in_metallic[idx]
    out_orm[idx] = vec4(ao, roughness, metallic, 1.0)
```

**Generated USF:** 5 compute kernels in `PBRGenerator.usf`

---

### KAIN Shader 2: `filters.kn`

**Consolidates:** `MaterializeFilters.usf`, `KStudioCore/LayerFilter.usf`

**KAIN Syntax:**
```kain
shader compute BlurHorizontal(thread_id: Vec3):
    uniform input_texture: Sampler2D @0
    buffer output: RWBuffer<Vec4> @1
    uniform texture_size: Vec2 @2
    uniform radius: Float @3
    
    let uv = vec2(thread_id.x as Float, thread_id.y as Float) / texture_size
    let texel_size = 1.0 / texture_size
    
    var result = vec4(0.0, 0.0, 0.0, 0.0)
    let weights = [0.0162, 0.0540, 0.1216, 0.1933, 0.2258, 0.1933, 0.1216, 0.0540, 0.0162]
    
    for i in -4..5:
        let sample_uv = uv + vec2(i as Float * texel_size.x * radius, 0.0)
        result = result + sample(input_texture, sample_uv) * weights[i + 4]
    
    let idx = thread_id.x + thread_id.y * texture_size.x as Int
    output[idx] = result

shader compute Sharpen(thread_id: Vec3):
    uniform input_texture: Sampler2D @0
    buffer output: RWBuffer<Vec4> @1
    uniform texture_size: Vec2 @2
    uniform strength: Float @3
    
    let uv = vec2(thread_id.x as Float, thread_id.y as Float) / texture_size
    let texel_size = 1.0 / texture_size
    
    let center = sample(input_texture, uv) * 5.0
    let neighbors = 
        sample(input_texture, uv + vec2(-texel_size.x, 0.0)) +
        sample(input_texture, uv + vec2(texel_size.x, 0.0)) +
        sample(input_texture, uv + vec2(0.0, -texel_size.y)) +
        sample(input_texture, uv + vec2(0.0, texel_size.y))
    
    let sharpened = center - neighbors
    let original = sample(input_texture, uv)
    let result = lerp(original, sharpened, strength)
    
    let idx = thread_id.x + thread_id.y * texture_size.x as Int
    output[idx] = clamp(result, vec4(0.0, 0.0, 0.0, 0.0), vec4(1.0, 1.0, 1.0, 1.0))

// ... EdgeDetect, Levels, HSL, etc.
```

**Generated USF:** 8+ compute kernels in `Filters.usf`

---

### KAIN Shader 3: `noise.kn`

**Consolidates:** `MaterializeNoiseGenerator.usf`, `KStudioCore/ProceduralNoise.usf`, `MaterializeProceduralCommon.ush`

**KAIN Syntax:**
```kain
// Shared noise library functions
fn hash21(p: Vec2) -> Float:
    var p3 = frac(vec3(p.x, p.y, p.x) * vec3(0.1031, 0.1030, 0.0973))
    p3 = p3 + dot(p3, vec3(p3.y, p3.z, p3.x) + 33.33)
    return frac((p3.x + p3.y) * p3.z)

fn perlin_noise(uv: Vec2, scale: Float, octaves: Int, seed: Float) -> Float:
    let p = uv * scale + vec2(seed, seed)
    return fbm(p, octaves, 2.0, 0.5)

fn voronoi(uv: Vec2, scale: Float, randomness: Float, seed: Float) -> Vec2:
    let n = floor(uv * scale)
    let f = frac(uv * scale)
    var F1 = 8.0
    var F2 = 8.0
    for j in -1..2:
        for i in -1..2:
            let g = vec2(i as Float, j as Float)
            let o = hash22(n + g + vec2(seed, seed)) * randomness
            let r = g + o - f
            let d = dot(r, r)
            if d < F1:
                F2 = F1
                F1 = d
            else if d < F2:
                F2 = d
    return vec2(sqrt(F1), sqrt(F2))

shader compute GenerateNoise(thread_id: Vec3):
    buffer output: RWBuffer<Vec4> @0
    uniform noise_type: Int @1
    uniform scale: Float @2
    uniform octaves: Int @3
    uniform persistence: Float @4
    uniform lacunarity: Float @5
    uniform seed: Float @6
    uniform texture_size: Vec2 @7
    
    let uv = vec2(thread_id.x as Float, thread_id.y as Float) / texture_size
    var value = 0.0
    
    match noise_type:
        0 => value = perlin_noise(uv, scale, octaves, seed)
        1 => value = voronoi_edges(uv, scale, 0.1, seed)
        2 => value = 1.0 - worley_noise(uv, scale, 1.0, seed)
        3 => value = ridged_noise(uv, scale, octaves, seed)
        4 => value = turbulence(uv, scale, octaves, seed)
        _ => value = perlin_noise(uv, scale, octaves, seed)
    
    let idx = thread_id.x + thread_id.y * texture_size.x as Int
    output[idx] = vec4(value, value, value, 1.0)
```

**Generated USF:** `NoiseGenerator.usf` + `ProceduralCommon.ush` (shared library)

**KAIN Feature:** Shared library auto-generation via `@shared` attribute

---

### KAIN Shader 4: `blend.kn`

**Consolidates:** `MaterializeBlend.usf`, `KStudioCore/LayerBlend.usf`

**KAIN Syntax:**
```kain
fn blend_overlay(base: Vec3, blend: Vec3) -> Vec3:
    var result = vec3(0.0, 0.0, 0.0)
    result.r = if base.r < 0.5: 2.0 * base.r * blend.r else: 1.0 - 2.0 * (1.0 - base.r) * (1.0 - blend.r)
    result.g = if base.g < 0.5: 2.0 * base.g * blend.g else: 1.0 - 2.0 * (1.0 - base.g) * (1.0 - blend.g)
    result.b = if base.b < 0.5: 2.0 * base.b * blend.b else: 1.0 - 2.0 * (1.0 - base.b) * (1.0 - blend.b)
    return result

fn blend_soft_light(base: Vec3, blend: Vec3) -> Vec3:
    var result = vec3(0.0, 0.0, 0.0)
    result.r = if blend.r < 0.5: base.r - (1.0 - 2.0 * blend.r) * base.r * (1.0 - base.r) else: base.r + (2.0 * blend.r - 1.0) * (sqrt(base.r) - base.r)
    result.g = if blend.g < 0.5: base.g - (1.0 - 2.0 * blend.g) * base.g * (1.0 - base.g) else: base.g + (2.0 * blend.g - 1.0) * (sqrt(base.g) - base.g)
    result.b = if blend.b < 0.5: base.b - (1.0 - 2.0 * blend.b) * base.b * (1.0 - base.b) else: base.b + (2.0 * blend.b - 1.0) * (sqrt(base.b) - base.b)
    return result

shader compute BlendTextures(thread_id: Vec3):
    uniform base_texture: Sampler2D @0
    uniform blend_texture: Sampler2D @1
    uniform mask_texture: Sampler2D @2
    buffer output: RWBuffer<Vec4> @3
    uniform blend_mode: Int @4
    uniform opacity: Float @5
    uniform has_mask: Bool @6
    uniform invert_mask: Bool @7
    uniform texture_size: Vec2 @8
    
    let uv = vec2(thread_id.x as Float, thread_id.y as Float) / texture_size
    let base = sample(base_texture, uv)
    let blend = sample(blend_texture, uv)
    
    var mask_value = 1.0
    if has_mask:
        mask_value = sample(mask_texture, uv).r
        if invert_mask:
            mask_value = 1.0 - mask_value
    
    var blended = vec3(0.0, 0.0, 0.0)
    match blend_mode:
        0 => blended = blend.rgb
        1 => blended = base.rgb + blend.rgb
        2 => blended = base.rgb - blend.rgb
        3 => blended = base.rgb * blend.rgb
        4 => blended = vec3(1.0, 1.0, 1.0) - (vec3(1.0, 1.0, 1.0) - base.rgb) * (vec3(1.0, 1.0, 1.0) - blend.rgb)
        5 => blended = blend_overlay(base.rgb, blend.rgb)
        6 => blended = blend_soft_light(base.rgb, blend.rgb)
        // ... 16 total modes
        _ => blended = blend.rgb
    
    let effective_opacity = opacity * mask_value
    let final_blend_amount = effective_opacity * blend.a
    let result_rgb = lerp(base.rgb, blended, final_blend_amount)
    let result_alpha = base.a + blend.a * effective_opacity * (1.0 - base.a)
    
    let idx = thread_id.x + thread_id.y * texture_size.x as Int
    output[idx] = vec4(result_rgb, result_alpha)
```

**Generated USF:** `Blend.usf` with 16 blend mode functions

---

### KAIN Shader 5: `presets.kn`

**Consolidates:** All 12 preset shaders (Metal, Glossy, Toon) + 3 utility shaders

**KAIN Syntax:**
```kain
// Shared PBR utilities
fn fresnel_schlick(f0: Vec3, vdh: Float) -> Vec3:
    return f0 + (vec3(1.0, 1.0, 1.0) - f0) * pow(1.0 - vdh, 5.0)

fn ggx_distribution(ndh: Float, roughness: Float) -> Float:
    let alpha = max(0.001, roughness * roughness)
    let alpha2 = alpha * alpha
    let denom = ndh * ndh * (alpha2 - 1.0) + 1.0
    return alpha2 / (3.14159 * denom * denom)

fn smith_visibility(ndl: Float, ndv: Float, roughness: Float) -> Float:
    let alpha = max(0.001, roughness * roughness)
    let alpha2 = alpha * alpha
    let lambda_v = ndl * sqrt(alpha2 + (1.0 - alpha2) * ndv * ndv)
    let lambda_l = ndv * sqrt(alpha2 + (1.0 - alpha2) * ndl * ndl)
    return 0.5 / max(0.001, lambda_v + lambda_l)

shader fragment GlossyClearCoat(normal: Vec3, view_dir: Vec3, light_dir: Vec3, clear_coat: Float, clear_coat_roughness: Float) -> Vec4:
    uniform coat_ior: Float @0
    uniform coat_tint: Vec3 @1
    uniform coat_intensity: Float @2
    
    let n = normalize(normal)
    let v = normalize(view_dir)
    let l = normalize(light_dir)
    let h = normalize(l + v)
    let ndh = max(0.0, dot(n, h))
    let vdh = max(0.0, dot(v, h))
    
    let d = ggx_distribution(ndh, clear_coat_roughness)
    let f0 = pow((1.0 - coat_ior) / (1.0 + coat_ior), 2.0)
    let fresnel = f0 + (1.0 - f0) * pow(1.0 - vdh, 5.0)
    
    let ndl = max(0.0, dot(n, l))
    let ndv = max(0.0, dot(n, v))
    let vis = 0.25 / max(0.001, ndl * ndv)
    
    let coat_spec = d * fresnel * vis * clear_coat * coat_intensity
    let color = coat_tint * coat_spec
    return vec4(color, 1.0)

shader fragment ToonCelShading(normal: Vec3, light_dir: Vec3, view_dir: Vec3, albedo: Vec3) -> Vec4:
    uniform shadow_color: Vec3 @0
    uniform highlight_color: Vec3 @1
    uniform midtone_color: Vec3 @2
    uniform band_count: Float @3
    uniform band_smoothness: Float @4
    uniform wrap_amount: Float @5
    
    let n = normalize(normal)
    let l = normalize(light_dir)
    let ndl_raw = dot(n, l)
    let wrapped = (ndl_raw + wrap_amount) / (1.0 + wrap_amount)
    let ndl = clamp(wrapped, 0.0, 1.0)
    
    let safe_bands = max(band_count, 2.0)
    let band_value = floor(ndl * safe_bands) / safe_bands
    let band_lo = band_value - band_smoothness
    let band_hi = band_value + band_smoothness
    let smooth_ndl = smoothstep(band_lo, band_hi, ndl)
    
    var lit_color = shadow_color
    if smooth_ndl > 0.33:
        lit_color = lerp(shadow_color, midtone_color, (smooth_ndl - 0.33) / 0.34)
    if smooth_ndl > 0.67:
        lit_color = lerp(midtone_color, highlight_color, (smooth_ndl - 0.67) / 0.33)
    
    let color = albedo * lit_color
    return vec4(color, 1.0)

// ... 12 more preset shaders
```

**Generated USF:** 15 pixel shaders (12 presets + 3 utilities)

---

## Key Patterns for KAIN Backend

### Pattern 1: Multi-Pass Compute with Ping-Pong Buffers

**C++ Pattern:**
```cpp
FRDGTextureRef HeightPingRDG = ComputeGraphBuilder.CreateTexture(HeightDesc, TEXT("HeightPing"));
FRDGTextureRef HeightPongRDG = ComputeGraphBuilder.CreateTexture(HeightDesc, TEXT("HeightPong"));

for(int32 i = 0; i < HeightIterations; i++)
{
    PassParams->InHeightPrev = ComputeGraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(
        (i % 2 == 0) ? HeightPingRDG : HeightPongRDG));
    PassParams->OutHeightNext = ComputeGraphBuilder.CreateUAV(
        (i % 2 == 0) ? HeightPongRDG : HeightPingRDG);
    FComputeShaderUtils::AddPass(...);
}

// Read final result from correct buffer
FRDGTextureRef InputHeight = (HeightIterations % 2 == 0) ? HeightPingRDG : HeightPongRDG;
```

**KAIN Implementation:**
- Detect `for` loops in shader code
- Auto-generate ping-pong buffer logic
- Track iteration parity for final read

---

### Pattern 2: Shared Shader Library (.ush)

**C++ Pattern:**
```cpp
// MaterializeProceduralCommon.ush
float hash21(float2 p) { ... }
float perlin_noise(float2 uv, float scale, int octaves, float seed) { ... }
float voronoi_edges(float2 uv, float scale, float edge_width, float seed) { ... }

// MaterializeNoiseGenerator.usf
#include "/Plugin/Materialize/MaterializeProceduralCommon.ush"
[numthreads(8, 8, 1)]
void GenerateNoiseCS(uint3 DispatchThreadId : SV_DispatchThreadID)
{
    float value = perlin_noise(transformedUV, noiseScale, octaves, seed);
    OutputTexture[DispatchThreadId.xy] = float4(value, value, value, 1.0);
}
```

**KAIN Implementation:**
- Extract shared functions to separate `.kn` file
- Mark with `@shared` attribute
- Auto-generate `.ush` file with all shared functions
- Auto-inject `#include` in dependent shaders

**KAIN Syntax:**
```kain
// noise_common.kn
@shared
fn hash21(p: Vec2) -> Float:
    // ... implementation

@shared
fn perlin_noise(uv: Vec2, scale: Float, octaves: Int, seed: Float) -> Float:
    // ... implementation

// noise_generator.kn
shader compute GenerateNoise(thread_id: Vec3):
    // Automatically includes noise_common.ush
    let value = perlin_noise(uv, scale, octaves, seed)
```

**Backend Logic:**
1. Scan all `.kn` files for `@shared` functions
2. Generate `{Plugin}Common.ush` with all shared functions
3. Inject `#include "/Plugin/{Plugin}/{Plugin}Common.ush"` after `Platform.ush`
4. Already implemented in `ue5-shaders` crate (multi-shader plugins)

---

### Pattern 3: RDG Texture Extraction

**C++ Pattern:**
```cpp
TRefCountPtr<IPooledRenderTarget> ExtNormal, ExtRough, ExtMetal;

ComputeGraphBuilder.QueueTextureExtraction(FinalNormal, &ExtNormal);
ComputeGraphBuilder.QueueTextureExtraction(FinalRough, &ExtRough);
ComputeGraphBuilder.QueueTextureExtraction(FinalMetal, &ExtMetal);

ComputeGraphBuilder.Execute();

// Copy to external textures
auto SafeCopy = [&](TRefCountPtr<IPooledRenderTarget> Src, FTexture2DRHIRef Dst) {
    if (Src.IsValid() && Dst.IsValid() && Dst->GetNativeResource())
    {
        RHICmdList.Transition(FRHITransitionInfo(Dst, ERHIAccess::Unknown, ERHIAccess::CopyDest));
        FRHICopyTextureInfo CopyInfo;
        RHICmdList.CopyTexture(Src->GetRHI(), Dst, CopyInfo);
        RHICmdList.Transition(FRHITransitionInfo(Dst, ERHIAccess::CopyDest, ERHIAccess::SRVGraphics));
    }
};

SafeCopy(ExtNormal, NormalRHI);
```

**KAIN Implementation:**
- Detect shader outputs that need extraction
- Auto-generate `QueueTextureExtraction` calls
- Auto-generate `SafeCopy` lambda and calls
- Wrap in `ENQUEUE_RENDER_COMMAND` macro

---

### Pattern 4: Shader Parameter Padding

**C++ Pattern:**
```cpp
// PBRGenerator.usf has a SHARED cbuffer across 3 kernels
// All kernels must have identical parameter layout
// Unused parameters must be initialized to avoid garbage values

BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(float, NormalStrength)
    SHADER_PARAMETER(float, RoughnessBase)
    SHADER_PARAMETER(float, RoughnessContrast)
    // ... 30+ parameters
END_SHADER_PARAMETER_STRUCT()

// In GradientCS (only uses NormalStrength):
PassParams->NormalStrength = Params.NormalStrength;
// Initialize ALL other parameters to 0.0f (padding)
PassParams->RoughnessBase = 0.0f;
PassParams->RoughnessContrast = 0.0f;
PassParams->bRoughnessInvert = 0;
// ... 27+ padding assignments
```

**KAIN Implementation:**
- Detect shared cbuffer across multiple kernels
- Auto-generate padding initialization
- Emit warning if parameter layout differs

---

### Pattern 5: Pixel Format Validation

**C++ Pattern:**
```cpp
// D3D12 UAVs do not support PF_B8G8R8A8 (BGRA). Must use PF_R8G8B8A8 (RGBA).
GetOrResize(OutResult.Normal, PF_R8G8B8A8, false);

// Single-channel UAVs require PF_R32_FLOAT (not PF_R16F or PF_R8)
GetOrResize(OutResult.Roughness, PF_R32_FLOAT, false);
```

**KAIN Implementation:**
- Validate UAV pixel formats at compile time
- Emit error if `RWTexture2D<float>` paired with non-float format
- Emit error if BGRA format used for UAV
- Auto-suggest correct format

---

## Performance Characteristics

### Benchmark (2048x2048 texture on RTX 3080)

| Operation | Time | Notes |
|-----------|------|-------|
| Gradient Extraction | 0.8ms | Single pass |
| Height Integration (24 iter) | 4.2ms | Jacobi solver |
| Final PBR Generation | 1.1ms | 6 outputs |
| Seamless Tiling (6 maps) | 2.4ms | 6 passes |
| ORM Packing | 0.3ms | Single pass |
| **Total (Multi-Pass)** | **8.8ms** | ~113 FPS |
| **Total (Single-Pass)** | **1.5ms** | ~667 FPS (preview) |

### Memory Usage (2048x2048)

| Resource | Format | Size | Count | Total |
|----------|--------|------|-------|-------|
| Source Input | RGBA8 | 16MB | 1 | 16MB |
| Gradient (intermediate) | RG32F | 32MB | 1 | 32MB |
| Height Ping/Pong | R32F | 16MB | 2 | 32MB |
| Normal Output | RGBA8 | 16MB | 1 | 16MB |
| Roughness Output | R32F | 16MB | 1 | 16MB |
| Metallic Output | R32F | 16MB | 1 | 16MB |
| AO Output | R32F | 16MB | 1 | 16MB |
| Height Output | R32F | 16MB | 1 | 16MB |
| Emissive Output | R32F | 16MB | 1 | 16MB |
| ORM Output | RGBA8 | 16MB | 1 | 16MB |
| **Total VRAM** | | | | **208MB** |

**Optimization:** Texture reuse reduces VRAM by 50% on repeated generations

---

## KAIN Stdlib Integration

### Candidate Functions for `stdlib/ue5/shaders.kn`

**From MaterializeProceduralCommon.ush:**
```kain
@extern fn hash11(p: Float) -> Float
@extern fn hash21(p: Vec2) -> Float
@extern fn hash22(p: Vec2) -> Vec2
@extern fn hash33(p: Vec3) -> Vec3

@extern fn perlin_noise(uv: Vec2, scale: Float, octaves: Int, seed: Float) -> Float
@extern fn voronoi(uv: Vec2, scale: Float, randomness: Float, seed: Float) -> Vec2
@extern fn worley_noise(uv: Vec2, scale: Float, randomness: Float, seed: Float) -> Float
@extern fn voronoi_edges(uv: Vec2, scale: Float, edge_width: Float, seed: Float) -> Float
@extern fn turbulence(uv: Vec2, scale: Float, octaves: Int, seed: Float) -> Float
@extern fn ridged_noise(uv: Vec2, scale: Float, octaves: Int, seed: Float) -> Float

@extern fn radial_falloff(uv: Vec2, falloff_power: Float) -> Float
@extern fn circle(uv: Vec2, radius: Float, edge_softness: Float) -> Float
@extern fn square(uv: Vec2, size: Float, edge_softness: Float) -> Float
@extern fn diamond(uv: Vec2, size: Float, edge_softness: Float) -> Float
@extern fn bricks(uv: Vec2, brick_width: Float, brick_height: Float, mortar_width: Float) -> Float
@extern fn dots(uv: Vec2, dot_radius: Float, spacing: Float) -> Float
@extern fn hexagon(uv: Vec2, scale: Float) -> Float

@extern fn transform_uv(uv: Vec2, rotation: Float, scale: Float, offset: Vec2) -> Vec2
@extern fn apply_tiling(uv: Vec2, mode: Int) -> Vec2
```

**From PBR utilities:**
```kain
@extern fn fresnel_schlick(f0: Vec3, vdh: Float) -> Vec3
@extern fn ggx_distribution(ndh: Float, roughness: Float) -> Float
@extern fn smith_visibility(ndl: Float, ndv: Float, roughness: Float) -> Float
@extern fn linearize_gamma(c: Vec3) -> Vec3
@extern fn get_luminance_linear(lin: Vec3) -> Float
```

**From blend modes:**
```kain
@extern fn blend_overlay(base: Vec3, blend: Vec3) -> Vec3
@extern fn blend_soft_light(base: Vec3, blend: Vec3) -> Vec3
@extern fn blend_hard_light(base: Vec3, blend: Vec3) -> Vec3
@extern fn blend_color_dodge(base: Vec3, blend: Vec3) -> Vec3
@extern fn blend_color_burn(base: Vec3, blend: Vec3) -> Vec3
@extern fn blend_vivid_light(base: Vec3, blend: Vec3) -> Vec3
```

**From color space:**
```kain
@extern fn rgb_to_hsl(rgb: Vec3) -> Vec3
@extern fn hsl_to_rgb(hsl: Vec3) -> Vec3
@extern fn rgb_to_hsv(rgb: Vec3) -> Vec3
@extern fn hsv_to_rgb(hsv: Vec3) -> Vec3
```

**Total:** 35+ functions → `stdlib/ue5/shaders.kn`

**Compression Ratio:** 1:25 (1 line KAIN → 25 lines HLSL for complex functions like `voronoi`)

---

## Critical Implementation Notes

### 1. Shared cbuffer Across Multiple Kernels

**Problem:** `PBRGenerator.usf` has 3 kernels sharing the same cbuffer (30+ parameters)

**Solution in C++:**
```cpp
// All 3 kernels have IDENTICAL parameter structs
BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
    SHADER_PARAMETER(float, NormalStrength)
    SHADER_PARAMETER(float, RoughnessBase)
    // ... 30+ parameters (MUST BE IDENTICAL)
END_SHADER_PARAMETER_STRUCT()

// Each kernel only uses subset, but ALL must be initialized
PassParams->NormalStrength = Params.NormalStrength; // Used
PassParams->RoughnessBase = 0.0f;                   // Padding
PassParams->RoughnessContrast = 0.0f;               // Padding
// ... 27+ padding assignments
```

**KAIN Backend Strategy:**
- Detect multiple kernels in same `.usf` file
- Merge all `uniform` declarations into single cbuffer
- Auto-generate padding initialization for unused parameters
- Emit warning if parameter layout differs

---

### 2. D3D12 UAV Format Restrictions

**Problem:** D3D12 does not support BGRA UAVs

**C++ Workaround:**
```cpp
// WRONG: PF_B8G8R8A8 (BGRA) — D3D12 error
GetOrResize(OutResult.Normal, PF_B8G8R8A8, false);

// CORRECT: PF_R8G8B8A8 (RGBA)
GetOrResize(OutResult.Normal, PF_R8G8B8A8, false);
```

**KAIN Backend Strategy:**
- Validate UAV pixel formats at compile time
- Emit error if BGRA format used for `RWTexture2D`
- Auto-suggest RGBA format
- Add to `validation_rules.json`:
```json
{
  "category": "Shader",
  "condition": "InvalidPixelFormat",
  "severity": "error",
  "message": "D3D12 UAVs do not support PF_B8G8R8A8 (BGRA). Use PF_R8G8B8A8 (RGBA).",
  "suggestion": "Change pixel format to PF_R8G8B8A8"
}
```

---

### 3. Single-Channel UAV Format

**Problem:** `RWTexture2D<float>` requires `PF_R32_FLOAT`, not `PF_R16F` or `PF_R8`

**C++ Pattern:**
```cpp
// WRONG: PF_R16F — format mismatch
GetOrResize(OutResult.Roughness, PF_R16F, false);

// CORRECT: PF_R32_FLOAT
GetOrResize(OutResult.Roughness, PF_R32_FLOAT, false);
```

**KAIN Backend Strategy:**
- Map `RWBuffer<Float>` → `RWTexture2D<float>` → `PF_R32_FLOAT`
- Map `RWBuffer<Vec4>` → `RWTexture2D<float4>` → `PF_R8G8B8A8` or `PF_FloatRGBA`
- Add to `TypeMapper` in `ue5-shaders/src/type_mapping.rs`

---

### 4. RDG Resource Transitions

**C++ Pattern:**
```cpp
// Transition source to SRV before RDG
RHICmdList.Transition(FRHITransitionInfo(SourceRHI, ERHIAccess::Unknown, ERHIAccess::SRVCompute));

// After RDG execution, transition outputs to SRVGraphics for UI
RHICmdList.Transition(FRHITransitionInfo(Dst, ERHIAccess::CopyDest, ERHIAccess::SRVGraphics));
```

**KAIN Backend Strategy:**
- Auto-generate transitions for external textures
- `Unknown → SRVCompute` for inputs
- `CopyDest → SRVGraphics` for outputs
- Already implemented in `ue5-shaders` crate

---

### 5. Ping-Pong Buffer Parity

**Problem:** After N iterations, which buffer has the final result?

**C++ Solution:**
```cpp
for(int32 i = 0; i < HeightIterations; i++)
{
    PassParams->InHeightPrev = (i % 2 == 0) ? HeightPingRDG : HeightPongRDG;
    PassParams->OutHeightNext = (i % 2 == 0) ? HeightPongRDG : HeightPingRDG;
}

// Fix Ping-Pong Logic: If 24 iterations (even), last write is to PING. Read PING.
FRDGTextureRef InputHeight = (HeightIterations % 2 == 0) ? HeightPingRDG : HeightPongRDG;
```

**KAIN Backend Strategy:**
- Detect `for` loops with alternating buffer access
- Track iteration parity
- Auto-generate final buffer selection based on iteration count parity

---

## Shader Complexity Analysis

### Lines of Code by Category

| Category | Files | Total Lines | Avg Lines/File |
|----------|-------|-------------|----------------|
| PBR Generation | 2 | 598 | 299 |
| Filters | 1 | 277 | 277 |
| Noise | 2 | 478 | 239 |
| Blend | 1 | 184 | 184 |
| KStudioCore | 5 | 1,228 | 246 |
| Preset Shaders | 12 | 599 | 50 |
| Shared Library | 1 | 305 | 305 |
| **Total** | **24** | **3,669** | **153** |

### Parameter Complexity

| Shader | Scalar Params | Texture Inputs | Texture Outputs |
|--------|---------------|----------------|-----------------|
| PBRGenerator (3 kernels) | 30 | 3 | 7 |
| Seamless | 3 | 1 | 1 |
| PackORM | 3 | 3 | 1 |
| NoiseGenerator | 12 | 0 | 1 |
| Blend | 3 | 2 | 1 |
| LayerBlend | 5 | 3 | 1 |
| LayerFilter | 5 | 1 | 1 |
| LayerAdjustment | 11 | 1 | 1 |
| ProceduralNoise | 10 | 0 | 1 |
| Preset Shaders (avg) | 4 | 0 | 1 |

**Most Complex:** `PBRGenerator` (30 scalar params, 3 inputs, 7 outputs)

---

## KAIN Consolidation Strategy

### Phase 1: Core Compute Shaders (3 files)

**File 1: `src/pbr_generator.kn`**
- Consolidates: `PBRGenerator.usf`, `SeamlessAndPacking.usf`
- Kernels: GradientCS, HeightIntegrationCS, FinalPBRCS, MainCS, SeamlessCS, PackORMCS
- Lines: ~600 KAIN → 598 HLSL (1:1 ratio, minimal compression)

**File 2: `src/filters.kn`**
- Consolidates: `MaterializeFilters.usf`, `KStudioCore/LayerFilter.usf`, `KStudioCore/LayerAdjustment.usf`
- Kernels: BlurHorizontalCS, BlurVerticalCS, SharpenCS, EdgeDetectCS, LevelsCS, HSLAdjustCS, FilterCS, AdjustmentCS
- Lines: ~250 KAIN → 777 HLSL (1:3 ratio with stdlib)

**File 3: `src/noise.kn`**
- Consolidates: `MaterializeNoiseGenerator.usf`, `KStudioCore/ProceduralNoise.usf`, `MaterializeProceduralCommon.ush`
- Kernels: GenerateNoiseCS, NoiseCS
- Lines: ~200 KAIN → 914 HLSL (1:4.5 ratio with stdlib)
- Shared library: Auto-generates `MaterializeCommon.ush`

---

### Phase 2: Blend & Layer System (1 file)

**File 4: `src/blend.kn`**
- Consolidates: `MaterializeBlend.usf`, `KStudioCore/LayerBlend.usf`, `KStudioCore/MathOperations.usf`
- Kernels: BlendTexturesCS, BlendCS, MathCS
- Lines: ~150 KAIN → 476 HLSL (1:3 ratio)

---

### Phase 3: Preset Shaders (1 file)

**File 5: `src/presets.kn`**
- Consolidates: All 12 preset shaders + 3 utility shaders
- Kernels: 15 pixel shaders
- Lines: ~300 KAIN → 599 HLSL (1:2 ratio)

**Total Consolidation:**
- **Before:** 24 files, 3,669 lines HLSL
- **After:** 5 files, ~1,500 lines KAIN
- **Compression:** 1:2.4 overall (1:4.5 with stdlib for noise/filters)

---

## RDG Best Practices (Learned from Materialize)

### 1. RAII Wrapper for Automatic Execution

**Pattern:**
```cpp
class FMaterializeRDGScope
{
    ~FMaterializeRDGScope() { if (!bExecuted) Execute(); }
};

// Usage
{
    FMaterializeRDGScope RDGScope(RHICmdList);
    FRDGBuilder& GraphBuilder = RDGScope.GetGraphBuilder();
    // ... add passes ...
} // Automatic execution on scope exit
```

**Benefits:**
- Prevents forgetting `GraphBuilder.Execute()`
- Exception-safe cleanup
- Cleaner code

---

### 2. Intermediate Buffers as Pure RDG

**Pattern:**
```cpp
// WRONG: Register external texture for intermediate
FRDGTextureRef GradientRDG = ComputeGraphBuilder.RegisterExternalTexture(...);

// CORRECT: Create pure RDG texture
FRDGTextureDesc GradientDesc = FRDGTextureDesc::Create2D(Size, PF_G32R32F, FClearValueBinding::Transparent, 
    TexCreate_UAV | TexCreate_ShaderResource);
FRDGTextureRef GradientRDG = ComputeGraphBuilder.CreateTexture(GradientDesc, TEXT("GradientMap"));
```

**Benefits:**
- RDG manages lifetime automatically
- No external texture allocation
- Better memory aliasing opportunities

---

### 3. Texture Extraction Queue

**Pattern:**
```cpp
TRefCountPtr<IPooledRenderTarget> ExtNormal, ExtRough, ExtMetal;

ComputeGraphBuilder.QueueTextureExtraction(FinalNormal, &ExtNormal);
ComputeGraphBuilder.QueueTextureExtraction(FinalRough, &ExtRough);
ComputeGraphBuilder.QueueTextureExtraction(FinalMetal, &ExtMetal);

ComputeGraphBuilder.Execute(); // All extractions happen here

// Now safe to copy to external textures
SafeCopy(ExtNormal, NormalRHI);
```

**Benefits:**
- Deferred extraction (RDG optimizes)
- All extractions batched
- Prevents premature resource access

---

### 4. Safe Copy with Validation

**Pattern:**
```cpp
auto SafeCopy = [&](TRefCountPtr<IPooledRenderTarget> Src, FTexture2DRHIRef Dst) {
    if (Src.IsValid() && Dst.IsValid() && Dst->GetNativeResource())
    {
        RHICmdList.Transition(FRHITransitionInfo(Dst, ERHIAccess::Unknown, ERHIAccess::CopyDest));
        FRHICopyTextureInfo CopyInfo;
        RHICmdList.CopyTexture(Src->GetRHI(), Dst, CopyInfo);
        RHICmdList.Transition(FRHITransitionInfo(Dst, ERHIAccess::CopyDest, ERHIAccess::SRVGraphics));
    }
};
```

**Benefits:**
- Prevents crashes from invalid resources
- Proper state transitions
- Ready for UI access after copy

---

### 5. FlushRenderingCommands Placement

**Pattern:**
```cpp
// After CreateTransient
UTexture2D* Tex = UTexture2D::CreateTransient(Width, Height, Format);
Tex->UpdateResource();
FlushRenderingCommands(); // CRITICAL: Wait for GPU init

// Capture RHI reference AFTER flush
FTexture2DRHIRef TexRHI = Tex->GetResource()->GetTexture2DRHI();

// After RDG execution
ComputeGraphBuilder.Execute();
FlushRenderingCommands(); // CRITICAL: Wait for completion before UI access
```

**Benefits:**
- Ensures GPU resources are ready
- Prevents race conditions
- Safe for immediate UI access

---

## Shader Parameter Binding Reference

### Compute Shader Parameter Types

| KAIN Type | HLSL Type | C++ Binding | Notes |
|-----------|-----------|-------------|-------|
| `uniform x: Float` | `float x` | `SHADER_PARAMETER(float, x)` | Scalar |
| `uniform x: Vec2` | `float2 x` | `SHADER_PARAMETER(FVector2f, x)` | 2D vector |
| `uniform x: Vec3` | `float3 x` | `SHADER_PARAMETER(FVector3f, x)` | 3D vector |
| `uniform x: Vec4` | `float4 x` | `SHADER_PARAMETER(FVector4f, x)` | 4D vector |
| `uniform x: Int` | `int x` | `SHADER_PARAMETER(int32, x)` | Integer |
| `uniform x: Bool` | `uint x` | `SHADER_PARAMETER(uint32, x)` | Boolean (0/1) |
| `uniform x: Sampler2D` | `Texture2D x` + `SamplerState` | `SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, x)` + `SHADER_PARAMETER_SAMPLER(SamplerState, xSampler)` | Texture input |
| `buffer x: RWBuffer<Float>` | `RWTexture2D<float> x` | `SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, x)` | Single-channel output |
| `buffer x: RWBuffer<Vec4>` | `RWTexture2D<float4> x` | `SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, x)` | Multi-channel output |
| `buffer x: Buffer<Float>` | `Texture2D<float> x` | `SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, x)` | Single-channel input |

---

### Pixel Shader Parameter Types

| KAIN Type | HLSL Type | C++ Binding | Notes |
|-----------|-----------|-------------|-------|
| `uniform x: Float` | `float x` | `SHADER_PARAMETER(float, x)` | Scalar uniform |
| `uniform x: Vec3` | `float3 x` | `SHADER_PARAMETER(FVector3f, x)` | Vector uniform |
| Interpolator inputs | `FPSInput` struct | N/A | Passed from vertex shader |
| Return value | `FPSOutput` struct | `RENDER_TARGET_BINDING_SLOTS()` | Render target |

---

## Dispatch Helper Pattern

**C++ Pattern:**
```cpp
// In shader .h file
static void Exec(FRDGBuilder& GraphBuilder, FRDGTextureRef OutputTexture, const FParameters& Parameters)
{
    TShaderMapRef<FMyShader> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
    FParameters* PassParameters = GraphBuilder.AllocParameters<FParameters>();
    *PassParameters = Parameters;
    PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ENoAction);
    
    FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("MyShader"), Shader, PassParameters, GroupCount);
}

// Helper function for easy invocation
void AddPass_MyShader(FRDGBuilder& GraphBuilder, FRDGTextureRef OutputTexture, float param1, float param2)
{
    FMyShader::FParameters* Params = GraphBuilder.AllocParameters<FMyShader::FParameters>();
    Params->param1 = param1;
    Params->param2 = param2;
    FMyShader::Exec(GraphBuilder, OutputTexture, *Params);
}
```

**KAIN Backend Strategy:**
- Auto-generate `Exec()` static method for all shaders
- Auto-generate `AddPass_*` helper functions
- Already implemented in `ue5-shaders` crate

---

## Shader Permutations (Not Used in Materialize)

**Observation:** Materialize does NOT use shader permutations (`SHADER_PERMUTATION_BOOL`)

**Potential Optimization:**
```cpp
// Instead of runtime branches:
if (bAdvancedNormal > 0) { /* multi-scale */ } else { /* sobel */ }

// Could use permutations:
class FKGradientCS : public FGlobalShader
{
    class FAdvancedNormalDim : SHADER_PERMUTATION_BOOL("ADVANCED_NORMAL");
    using FPermutationDomain = TShaderPermutationDomain<FAdvancedNormalDim>;
};

// In shader:
#if ADVANCED_NORMAL
    // Multi-scale code
#else
    // Sobel code
#endif
```

**KAIN Strategy:**
- Detect `CFG_*` / `ENABLE_*` prefixed uniforms
- Auto-generate permutations
- Already implemented in `ue5-shaders` crate
- **Not critical for Materialize** (runtime branches are fine for this use case)

---

## KAIN Implementation Checklist

### Backend Features Required

- [x] **Compute shaders** — Already implemented
- [x] **Pixel shaders** — Already implemented (fragment shaders)
- [x] **Shared shader libraries (.ush)** — Already implemented (multi-shader plugins)
- [x] **RDG texture creation** — Already implemented
- [x] **RDG texture extraction** — Already implemented
- [x] **Resource transitions** — Already implemented
- [x] **Thread group size** — Already implemented (`[numthreads(8,8,1)]`)
- [x] **Texture sampling** — Already implemented (`sample()`)
- [x] **UAV writes** — Already implemented (`buffer x: RWBuffer<T>`)
- [ ] **Multi-kernel shared cbuffer** — Needs implementation
- [ ] **Ping-pong buffer detection** — Needs implementation
- [ ] **Pixel format validation** — Needs enhancement
- [ ] **RAII RDG scope generation** — Needs implementation

---

### New Backend Tasks

**Task 1: Multi-Kernel Shared cbuffer**
- Detect multiple `shader compute` in same file
- Merge all `uniform` declarations
- Generate single cbuffer with all parameters
- Auto-generate padding initialization for unused parameters

**Task 2: Ping-Pong Buffer Detection**
- Detect `for` loops with alternating buffer access pattern
- Track iteration parity
- Auto-generate final buffer selection: `(iterations % 2 == 0) ? Ping : Pong`

**Task 3: Enhanced Pixel Format Validation**
- Add validation rule: `RWTexture2D<float>` requires `PF_R32_FLOAT`