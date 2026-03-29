# Materialize Shader Analysis

**Analysis Date:** February 2026  
**Source:** `Research/UEProj/Project_5.4/Plugins/Materialize/Shaders/`  
**Total Shaders:** 22 files (21 .usf, 1 .ush, 1 .hlsl)

---

## Executive Summary

Materialize contains a sophisticated GPU compute pipeline with 22 shader files spanning:
- **PBR Generation** — Multi-pass gradient extraction, Poisson height integration, physically-based material synthesis
- **Layer System** — 20 Photoshop-style blend modes, 13 convolution filters, mask support
- **Procedural Generation** — 16 noise types (Perlin, Voronoi, Worley, FBM, Turbulence, Ridged)
- **Specialized Shading** — Glossy materials (dual-lobe, clear coat, subsurface), toon rendering (cel, rim, outline)
- **Utility** — Seamless tiling (3 modes), ORM packing, HSL adjustment, edge detection

**Dispatch Pattern:** All shaders use `FGlobalShader` + `SHADER_USE_PARAMETER_STRUCT` with RDG (Render Dependency Graph) via `FComputeShaderUtils::AddPass()`. Thread group size: 8x8x1.

---

## Shader Inventory

### 1. Core PBR Generation

#### PBRGenerator.usf (16.5KB, 423 lines)
**Purpose:** Multi-pass physically-based material synthesis from albedo texture

**Kernels:**
- `GradientCS` — Sobel gradient extraction with multi-scale support (1-8 octaves, anisotropic)
- `HeightIntegrationCS` — Poisson equation solver via Jacobi iteration (4-64 iterations)
- `FinalPBRCS` — Generates Normal, Roughness, Metallic, AO, Height, Emissive from integrated height
- `MainCS` — Legacy single-pass (fast preview, backward compat)

**Inputs:**
- `InSourceTexture` (Texture2D<float4>) — Albedo/diffuse input
- `InGradient` (Texture2D<float2>) — Gradient field from Pass 1
- `InHeightPrev` (Texture2D<float>) — Height from previous Jacobi iteration

**Outputs:**
- `OutGradient` (RWTexture2D<float2>) — Gradient field
- `OutHeightNext` (RWTexture2D<float>) — Integrated height
- `OutNormal` (RWTexture2D<float4>) — Tangent-space normal map
- `OutRoughness` (RWTexture2D<float>) — Surface roughness
- `OutMetallic` (RWTexture2D<float>) — Metallic mask
- `OutAO` (RWTexture2D<float>) — Ambient occlusion
- `OutHeight` (RWTexture2D<float>) — Displacement/height map
- `OutEmissive` (RWTexture2D<float>) — Emissive mask

**Uniforms (33 parameters):**
```hlsl
float NormalStrength, RoughnessBase, RoughnessContrast;
uint bRoughnessInvert;
float MetallicBase, MetallicContrast, MetallicBias, MetallicSensitivity;
float AOIntensity, HeightContrast;
float BioDetail, BioFrequency, CyberDetail, CyberScale;
float EdgeWear, CavityDirt, Dust, Grunge, Scratches, Noise;
float EmissiveThreshold, EmissiveColorBoost, VarianceWeight;
uint2 TextureDimensions;
uint bAdvancedNormal, bAdvancedAO;
int NormalOctaves;
float NormalSigmaBase, NormalAnisotropy, AORadius, AOBias, AOContrast;
```

**Algorithm Highlights:**
- **sRGB Linearization** — `pow(c, 2.2)` before all math operations
- **Multi-Scale Normal** — Macro (Poisson 1px, 50%) + Meso (Poisson 2px, 30%) + Micro (luminance Sobel, 20%)
- **Color-Aware Metallic** — `Lum * (1 - Sat) * Sensitivity` (metals = high brightness + low saturation)
- **Variance-Based Roughness** — Local 3x3 variance blended with luminance base
- **8-Direction Horizon AO** — Cardinal (weight 1.0) + diagonal (weight 0.707) horizon sampling
- **Emissive Detection** — Brightness threshold + saturation boost for neon/screens

---

### 2. Layer Blending System

#### MaterializeBlend.usf (5.6KB, 184 lines)
**Purpose:** Photoshop-compatible blend modes for texture compositing

**Kernel:** `BlendTexturesCS`

**Inputs:**
- `BaseTexture` (Texture2D) — Bottom layer
- `BlendTexture` (Texture2D) — Top layer
- `OutputTexture` (RWTexture2D<float4>) — Blended result

**Uniforms:**
- `BlendMode` (uint) — 0-15 (Normal, Add, Subtract, Multiply, Screen, Overlay, Soft Light, Hard Light, Darken, Lighten, Difference, Exclusion, Color Dodge, Color Burn, Linear Light, Vivid Light)
- `Opacity` (float) — Layer opacity 0-1
- `TextureSize` (uint2) — Texture dimensions

**Blend Modes (16 total):**
```
0=Normal, 1=Add, 2=Subtract, 3=Multiply, 4=Screen, 5=Overlay, 
6=Soft Light, 7=Hard Light, 8=Darken, 9=Lighten, 10=Difference, 
11=Exclusion, 12=Color Dodge, 13=Color Burn, 14=Linear Light, 15=Vivid Light
```

---

#### KStudioCore/LayerBlend.usf (8.4KB, 234 lines)
**Purpose:** Extended blend system with mask support and alpha compositing

**Kernel:** `BlendCS`

**Inputs:**
- `InBase`, `InBlend` (Texture2D<float4>)
- `InMask` (Texture2D<float>) — Optional mask
- `OutResult` (RWTexture2D<float4>)

**Uniforms:**
- `BlendMode` (uint) — 0-19 (adds Pin Light, Hard Mix, Linear Dodge, Linear Burn)
- `Opacity` (float)
- `bHasMask`, `bInvertMask` (uint)
- `TextureDimensions` (uint2)

**Alpha Compositing:**
```hlsl
float effectiveOpacity = Opacity * maskValue;
float finalBlendAmount = effectiveOpacity * blendAlpha;
result.rgb = lerp(base.rgb, blended, finalBlendAmount);
result.a = base.a + blendAlpha * effectiveOpacity * (1.0 - base.a);
```

---

### 3. Image Filters

#### MaterializeFilters.usf (9.7KB, 277 lines)
**Purpose:** GPU-accelerated image processing operations

**Kernels (6 total):**
- `BlurHorizontalCS` / `BlurVerticalCS` — Separable 9-tap Gaussian blur
- `SharpenCS` — Unsharp mask (5-tap kernel)
- `EdgeDetectCS` — Sobel edge detection
- `LevelsCS` — Input/output level adjustment
- `HSLAdjustCS` — Hue/Saturation/Lightness color adjustment
- `InvertCS` — Color inversion
- `GrayscaleCS` — Rec. 709 luminance conversion

**Inputs:**
- `InputTexture` (Texture2D)
- `OutputTexture` (RWTexture2D<float4>)

**Uniforms:**
- `TextureSize` (uint2)
- `FilterParams` (float4) — x=Strength, y=Radius, z=Threshold
- `FilterParams2` (float4) — For levels: InBlack, InWhite, OutBlack, OutWhite

**Gaussian Weights (9-tap):**
```hlsl
static const float GaussianWeights[9] = {
    0.0162, 0.0540, 0.1216, 0.1933, 0.2258, 0.1933, 0.1216, 0.0540, 0.0162
};
```

---

#### KStudioCore/LayerFilter.usf (6.5KB, 245 lines)
**Purpose:** Extended filter library with morphological operations

**Kernel:** `FilterCS`

**Filter Types (13 total):**
```
0=BoxBlur, 1=GaussianBlur, 2=Sharpen, 3=EdgeDetect, 4=Emboss, 
5=HighPass, 6=LowPass, 7=Median, 8=Dilate, 9=Erode, 
10=Invert, 11=Normalize, 12=AutoLevels
```

**Uniforms:**
- `FilterType` (uint)
- `Intensity` (float)
- `KernelSize` (int)
- `Threshold` (float)
- `TextureDimensions` (uint2)

**Notable Implementations:**
- **Median Filter** — 3x3 bubble sort on luminance (noise reduction)
- **Dilate/Erode** — Max/min filters for morphological operations
- **High Pass** — `center - blur` for detail extraction

---

### 4. Procedural Noise Generation

#### MaterializeNoiseGenerator.usf (6.2KB, 173 lines)
**Purpose:** Procedural texture generation with UV transformations

**Kernels (5 total):**
- `GenerateNoiseCS` — Main noise generator (5 types)
- `GenerateRadialCS` — Radial falloff gradient
- `GenerateCircleCS` — Hard circle with soft edge
- `GenerateBricksCS` — Brick pattern with mortar
- `GenerateDotsCS` — Polka dot pattern

**Noise Types:**
```
0=Perlin, 1=Voronoi, 2=Worley, 3=Ridged, 4=Turbulence
```

**Uniforms:**
- `NoiseParams` (float4) — Scale, Octaves, Persistence, Lacunarity
- `NoiseParams2` (float4) — Seed, Contrast, Brightness, Invert
- `TransformParams` (float4) — Rotation, ScaleX, ScaleY, OffsetX
- `TransformParams2` (float4) — OffsetY, TilingMode
- `NoiseType` (uint)
- `TextureSize` (uint2)

**UV Transformations:**
- Rotation (radians)
- Non-uniform scale
- Offset
- Tiling modes: 0=Clamp, 1=Repeat, 2=Mirror

---

#### KStudioCore/ProceduralNoise.usf (12KB, 436 lines)
**Purpose:** Extended noise library with 16 noise types

**Kernel:** `NoiseCS`

**Noise Types (16 total):**
```
0=Perlin, 1=Simplex, 2=Worley, 3=FBM, 4=Turbulence, 5=Cellular,
6=Gradient, 7=Checker, 8=Brick, 9=Herringbone, 10=Hexagon,
11=Scratches, 12=Grunge, 13=Rust, 14=Dust, 15=Voronoise
```

**Uniforms:**
- `NoiseType` (uint)
- `Scale` (float)
- `Octaves` (int)
- `Persistence`, `Lacunarity` (float)
- `Offset` (float2)
- `Seed` (int)
- `bSeamless` (uint)
- `TextureDimensions` (uint2)
- `Time` (float) — For animated noise

**Advanced Features:**
- **Voronoise4D** — 4D Voronoi noise for seamless tiling (ported from TextureGraph)
- **Scratches** — 8-direction line patterns with random offsets
- **Grunge** — Multi-layer FBM + Worley composite
- **Dust** — 50 random circular spots
- **Rust** — FBM base + turbulence detail + Worley edges

---

#### MaterializeProceduralCommon.ush (8.5KB, 305 lines)
**Purpose:** Shared procedural generation library (header-only)

**Functions:**
- **Hash Functions:** `hash11`, `hash21`, `hash22`, `hash33`
- **Perlin Noise:** `grad2`, `fbm`, `perlin_noise`
- **Voronoi:** `voronoi`, `worley_noise`, `voronoi_edges`
- **Geometric Patterns:** `radial_falloff`, `circle`, `square`, `diamond`
- **Tiling Patterns:** `bricks`, `dots`, `hexagon`
- **Special Effects:** `turbulence`, `ridged_noise`
- **UV Transformations:** `transform_uv`, `apply_tiling`

**Key Algorithms:**
- **Quintic Interpolation** — `f * f * f * (f * (f * 6.0 - 15.0) + 10.0)` for smooth Perlin
- **FBM** — Multi-octave noise with lacunarity/persistence control
- **Voronoi F1/F2** — Closest and second-closest cell distances

---

### 5. Seamless Tiling & Packing

#### SeamlessAndPacking.usf (6.1KB, 175 lines)
**Purpose:** Seamless texture tiling and channel packing

**Kernels:**
- `SeamlessCS` — 3 tiling modes
- `PackORMCS` — UE5 ORM channel packing

**Seamless Modes:**
- **Mode 0 (CrossBlend)** — Offset by 50%, blend center seam with diamond mask
- **Mode 1 (MirrorBlend)** — Mirror edges, bilinear blend of 4 quadrants
- **Mode 2 (HistogramMatch)** — Cross-blend + local contrast adjustment

**ORM Packing:**
```hlsl
OutORM = float4(AO, Roughness, Metallic, 1.0);  // UE5 standard
```

**Uniforms:**
- `TextureDimensions` (uint2)
- `BlendWidth` (float) — Edge blend width 0-0.5
- `TileMode` (uint)

---

### 6. Specialized Shading Models

#### GlossyDualLobe.usf (2KB, 56 lines)
**Purpose:** Dual-lobe specular for car paint, lacquer

**Kernel:** `GlossyDualLobePS` (fragment shader)

**Inputs (via interpolators):**
- `TexCoord0` — normal (xyz), base_roughness (w)
- `TexCoord1` — view_dir (xyz), coat_roughness (w)
- `TexCoord2` — light_dir (xyz), coat_amount (w)

**Uniforms:**
- `base_color`, `coat_color` (float3)
- `energy_conservation` (float)

**Algorithm:**
- GGX distribution for base + coat layers
- Fresnel-Schlick (f0=0.04)
- Energy conservation: `base_energy = 1.0 - coat_amount * conservation`

---

#### GlossyClearCoat.usf (1.7KB)
**Purpose:** Clear coat layer (automotive, varnish)

**Similar to DualLobe but with fixed coat parameters**

---

#### GlossySubsurface.usf (1.4KB)
**Purpose:** Subsurface scattering approximation

**Uses wrap lighting + translucency term**

---

### 7. Toon Rendering

#### ToonCelShading.usf (1.7KB, 58 lines)
**Purpose:** Cel-shaded lighting with configurable bands

**Kernel:** `ToonCelShadingPS`

**Inputs:**
- `TexCoord0` — normal
- `TexCoord1` — light_dir
- `TexCoord2` — view_dir
- `TexCoord3` — albedo

**Uniforms:**
- `shadow_color`, `highlight_color`, `midtone_color` (float3)
- `band_count` (float) — Number of discrete shading bands
- `band_smoothness` (float) — Band edge softness
- `wrap_amount` (float) — Wrap lighting for softer shadows

**Algorithm:**
```hlsl
wrapped_ndl = (ndl + wrap) / (1 + wrap);
band_value = floor(ndl * bands) / bands;
smooth_ndl = smoothstep(band_value - smoothness, band_value + smoothness, ndl);
// 3-band color ramp: shadow (0-0.33), midtone (0.33-0.67), highlight (0.67-1.0)
```

---

#### ToonOutlineDetection.usf (1.3KB)
**Purpose:** Edge detection for toon outlines

**Sobel + depth discontinuity detection**

---

#### ToonRimLight.usf (1.1KB)
**Purpose:** Fresnel-based rim lighting for toon

---

#### ToonSpecular.usf (1.3KB)
**Purpose:** Stylized specular highlights

---

#### ToonConfigurableBands.usf (1.8KB)
**Purpose:** Advanced cel shading with custom band positions

---

### 8. PBR Microfacet Components

#### MaterializeFresnelSchlick.usf (789B)
**Purpose:** Fresnel-Schlick approximation

```hlsl
F = F0 + (1 - F0) * pow(1 - cos_theta, 5)
```

---

#### MaterializeGGXDistribution.usf (905B)
**Purpose:** GGX normal distribution function

```hlsl
D = alpha^2 / (PI * (NdH^2 * (alpha^2 - 1) + 1)^2)
```

---

#### MaterializeSmithVisibility.usf (1.1KB)
**Purpose:** Smith geometric shadowing term

---

#### MetalAnisotropicSpecular.usf (2.4KB)
**Purpose:** Anisotropic specular for brushed metal

---

#### MetalFresnelRim.usf (1.1KB)
**Purpose:** Metallic Fresnel rim lighting

---

### 9. Adjustment Filters

#### KStudioCore/LayerAdjustment.usf (7.5KB)
**Purpose:** Color/tone adjustments (not read in detail, but likely contains)

**Expected filters:** Brightness, Contrast, Saturation, Hue Shift, Levels, Curves

---

### 10. Math Operations

#### KStudioCore/MathOperations.usf (1.4KB)
**Purpose:** Texture math operations (Add, Multiply, Lerp)

---

### 11. Legacy Vertex Shader

#### SimpleVertex.usf (29B) + SimpleVertex.hlsl (590B)
**Purpose:** Passthrough vertex shader for fullscreen quad

---

## Compute Dispatch Flow

### Pattern 1: Multi-Pass PBR Generation (MaterializeComputeEngine.cpp)

```cpp
ENQUEUE_RENDER_COMMAND(KSampleGenGPU_MultiPass)(
    [captures...](FRHICommandListImmediate& RHICmdList) {
        FRDGBuilder GraphBuilder(RHICmdList);
        
        // 1. Register external input
        FRDGTextureRef InputRDG = GraphBuilder.RegisterExternalTexture(...);
        
        // 2. Create intermediate buffers (pure RDG)
        FRDGTextureRef GradientRDG = GraphBuilder.CreateTexture(...);
        FRDGTextureRef HeightPingRDG = GraphBuilder.CreateTexture(...);
        FRDGTextureRef HeightPongRDG = GraphBuilder.CreateTexture(...);
        
        // 3. Pass 1: Gradient Extraction
        TShaderMapRef<FKGradientCS> GradShader(GetGlobalShaderMap(...));
        FKGradientCS::FParameters* GradParams = GraphBuilder.AllocParameters<...>();
        GradParams->InSourceTexture = InputSRV;
        GradParams->OutGradient = GraphBuilder.CreateUAV(GradientRDG);
        // ... set 33 parameters
        FComputeShaderUtils::AddPass(GraphBuilder, ..., GradShader, GradParams, GroupCount);
        
        // 4. Pass 2: Height Integration (Jacobi iterations)
        for (int i = 0; i < HeightIterations; i++) {
            TShaderMapRef<FKHeightIntegrationCS> HeightShader(...);
            FKHeightIntegrationCS::FParameters* HeightParams = ...;
            HeightParams->InGradient = GraphBuilder.CreateSRV(GradientRDG);
            HeightParams->InHeightPrev = GraphBuilder.CreateSRV((i%2==0) ? HeightPingRDG : HeightPongRDG);
            HeightParams->OutHeightNext = GraphBuilder.CreateUAV((i%2==0) ? HeightPongRDG : HeightPingRDG);
            FComputeShaderUtils::AddPass(...);
        }
        
        // 5. Pass 3: Final PBR
        TShaderMapRef<FKFinalPBRCS> FinalShader(...);
        FKFinalPBRCS::FParameters* FinalParams = ...;
        FinalParams->InHeightPrev = GraphBuilder.CreateSRV((HeightIterations%2==0) ? HeightPingRDG : HeightPongRDG);
        FinalParams->OutNormal = GraphBuilder.CreateUAV(NormalRDG);
        FinalParams->OutRoughness = GraphBuilder.CreateUAV(RoughRDG);
        // ... 6 outputs
        FComputeShaderUtils::AddPass(...);
        
        // 6. Optional: Seamless tiling (6 passes, one per map)
        if (bMakeSeamless) {
            FinalNormal = MakeSeamlessPass(NormalRDG);
            // ... repeat for Rough, Metal, AO, Height, Emissive
        }
        
        // 7. Optional: ORM Packing
        if (bPackORM) {
            TShaderMapRef<FKPackORMCS> PackShader(...);
            // ... pack AO+Rough+Metal into single RGBA
        }
        
        // 8. Queue extraction to external textures
        GraphBuilder.QueueTextureExtraction(FinalNormal, &ExtNormal);
        // ... repeat for all outputs
        
        // 9. Execute graph
        GraphBuilder.Execute();
        
        // 10. Copy to external UTexture2D RHI resources
        RHICmdList.CopyTexture(ExtNormal->GetRHI(), NormalRHI, ...);
        // ... repeat for all outputs
    }
);
FlushRenderingCommands();
```

**Key Patterns:**
- **Ping-Pong Buffers** — Height integration alternates between two buffers
- **RDG Extraction** — `QueueTextureExtraction()` before `Execute()`
- **Safe Copy** — Validate RHI resources before `CopyTexture()`
- **Transition Management** — `RHICmdList.Transition()` for resource state changes

---

### Pattern 2: Single-Pass Operations (Filters, Blending)

```cpp
ENQUEUE_RENDER_COMMAND(KLayerBlend)(
    [captures...](FRHICommandListImmediate& RHICmdList) {
        FMaterializeRDGScope RDGScope(RHICmdList);  // RAII wrapper
        FRDGBuilder& GraphBuilder = RDGScope.GetGraphBuilder();
        
        // Register inputs
        FRDGTextureRef BaseRDG = GraphBuilder.RegisterExternalTexture(...);
        FRDGTextureRef BlendRDG = GraphBuilder.RegisterExternalTexture(...);
        
        // Create output
        FRDGTextureRef OutputRDG = GraphBuilder.CreateTexture(...);
        
        // Dispatch shader
        TShaderMapRef<FKLayerBlendCS> Shader(...);
        FKLayerBlendCS::FParameters* Params = GraphBuilder.AllocParameters<...>();
        Params->InBase = GraphBuilder.CreateSRV(BaseRDG);
        Params->InBlend = GraphBuilder.CreateSRV(BlendRDG);
        Params->OutResult = GraphBuilder.CreateUAV(OutputRDG);
        FComputeShaderUtils::AddPass(...);
        
        // Copy to external
        AddCopyTexturePass(GraphBuilder, OutputRDG, ExternalDest, ...);
        
        // RDGScope destructor calls GraphBuilder.Execute()
    }
);
```

**RAII Pattern:** `FMaterializeRDGScope` auto-executes graph on destruction

---

## Shader Parameter Binding

### C++ Shader Declaration Pattern

```cpp
class FKGradientCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FKGradientCS);
    SHADER_USE_PARAMETER_STRUCT(FKGradientCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InSourceTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, InSourceSampler)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float2>, OutGradient)
        SHADER_PARAMETER(float, NormalStrength)
        SHADER_PARAMETER(FUintVector2, TextureDimensions)
        // ... 30 more parameters
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Params) {
        return FDataDrivenShaderPlatformInfo::GetMaxFeatureLevel(Params.Platform) >= ERHIFeatureLevel::SM5;
    }
};

IMPLEMENT_GLOBAL_SHADER(FKGradientCS, "/Plugin/Materialize/PBRGenerator.usf", "GradientCS", SF_Compute);
```

### USF Shader Declaration Pattern

```hlsl
#include "/Engine/Public/Platform.ush"

Texture2D<float4> InSourceTexture;
SamplerState InSourceSampler;
RWTexture2D<float2> OutGradient;

float NormalStrength;
uint2 TextureDimensions;
// ... 30 more uniforms

[numthreads(8, 8, 1)]
void GradientCS(uint3 ThreadId : SV_DispatchThreadID)
{
    if (ThreadId.x >= TextureDimensions.x || ThreadId.y >= TextureDimensions.y) return;
    // ... shader logic
}
```

**Critical:** C++ `SHADER_PARAMETER` names must match USF uniform names exactly (case-sensitive)

---

## KAIN Shader Consolidation Plan

### Phase 1: Core PBR Shaders (Priority 1)

**Target:** Consolidate PBRGenerator.usf into KAIN

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
    
    let pos = thread_id.xy
    if pos.x >= texture_dimensions.x or pos.y >= texture_dimensions.y:
        return
    
    var grad = vec2(0.0, 0.0)
    
    if advanced_normal:
        for k in 0..normal_octaves:
            let sigma = normal_sigma_base * pow(2.0, k as Float)
            let offset = max(1, round(sigma) as Int)
            // Multi-scale gradient extraction
            grad = grad + compute_gradient_at_scale(pos, offset, sigma)
    else:
        // Standard Sobel
        grad = sobel_gradient(pos)
    
    grad.x = grad.x * normal_anisotropy
    out_gradient[pos] = grad * normal_strength * 0.25
```

**Benefits:**
- Single source for gradient extraction
- Eliminates 33-parameter C++ boilerplate
- Type-safe uniform binding
- Auto-generates `FGlobalShader` + `SHADER_PARAMETER_STRUCT`

---

### Phase 2: Blend & Filter Library (Priority 2)

**Target:** Consolidate MaterializeBlend.usf + LayerBlend.usf

```kain
shader compute LayerBlend(thread_id: Vec3):
    uniform base_texture: Sampler2D @0
    uniform blend_texture: Sampler2D @1
    uniform mask_texture: Sampler2D @2
    uniform blend_mode: Int @3
    uniform opacity: Float @4
    uniform has_mask: Bool @5
    uniform invert_mask: Bool @6
    uniform texture_dimensions: Vec2 @7
    buffer out_result: RWBuffer<Vec4> @8
    
    let pos = thread_id.xy
    if pos.x >= texture_dimensions.x or pos.y >= texture_dimensions.y:
        return
    
    let base = sample(base_texture, pos)
    let blend = sample(blend_texture, pos)
    
    var mask_value = 1.0
    if has_mask:
        mask_value = sample(mask_texture, pos).r
        if invert_mask:
            mask_value = 1.0 - mask_value
    
    let blended = apply_blend_mode(base.rgb, blend.rgb, blend_mode)
    let effective_opacity = opacity * mask_value
    let final_blend = effective_opacity * blend.a
    
    let result_rgb = lerp(base.rgb, blended, final_blend)
    let result_a = base.a + blend.a * effective_opacity * (1.0 - base.a)
    
    out_result[pos] = vec4(result_rgb, result_a)

fn apply_blend_mode(base: Vec3, blend: Vec3, mode: Int) -> Vec3:
    match mode:
        0 => blend  // Normal
        1 => base * blend  // Multiply
        2 => 1.0 - (1.0 - base) * (1.0 - blend)  // Screen
        3 => overlay_blend(base, blend)
        // ... 16 more modes
```

**Benefits:**
- Eliminates duplicate blend implementations (MaterializeBlend vs LayerBlend)
- Pattern matching for blend mode dispatch
- Stdlib integration for common blend functions

---

### Phase 3: Procedural Noise Library (Priority 3)

**Target:** Consolidate MaterializeNoiseGenerator.usf + ProceduralNoise.usf + MaterializeProceduralCommon.ush

```kain
// Shared library (auto-included)
fn perlin_noise(uv: Vec2, scale: Float, octaves: Int, seed: Float) -> Float:
    let p = uv * scale + seed
    return fbm(p, octaves, 2.0, 0.5)

fn fbm(p: Vec2, octaves: Int, lacunarity: Float, persistence: Float) -> Float:
    var value = 0.0
    var amplitude = 0.5
    var frequency = 1.0
    var max_value = 0.0
    
    for i in 0..octaves:
        value = value + amplitude * (grad2(p * frequency) * 2.0 - 1.0)
        max_value = max_value + amplitude
        amplitude = amplitude * persistence
        frequency = frequency * lacunarity
    
    return (value / max_value) * 0.5 + 0.5

shader compute NoiseGenerator(thread_id: Vec3):
    uniform noise_type: Int @0
    uniform scale: Float @1
    uniform octaves: Int @2
    uniform seed: Float @3
    uniform texture_size: Vec2 @4
    buffer out_texture: RWBuffer<Vec4> @5
    
    let uv = (thread_id.xy + 0.5) / texture_size
    
    let value = match noise_type:
        0 => perlin_noise(uv, scale, octaves, seed)
        1 => simplex_noise(uv, scale, seed)
        2 => worley_noise(uv, scale, seed)
        3 => ridged_noise(uv, scale, octaves, seed)
        4 => turbulence(uv, scale, octaves, seed)
        _ => 0.0
    
    out_texture[thread_id.xy] = vec4(value, value, value, 1.0)
```

**Benefits:**
- Eliminates 3 files (NoiseGenerator, ProceduralNoise, ProceduralCommon)
- Noise functions in stdlib (globally available)
- Pattern matching for noise type dispatch

---

### Phase 4: Filter Consolidation (Priority 4)

**Target:** Merge MaterializeFilters.usf + LayerFilter.usf

**Current Duplication:**
- Both implement: Blur, Sharpen, Edge Detect, Invert
- LayerFilter adds: Median, Dilate, Erode, High/Low Pass
- MaterializeFilters adds: HSL Adjust, Levels, Grayscale

**KAIN Approach:**
```kain
shader compute ImageFilter(thread_id: Vec3):
    uniform input_texture: Sampler2D @0
    uniform filter_type: Int @1
    uniform intensity: Float @2
    uniform kernel_size: Int @3
    uniform texture_size: Vec2 @4
    buffer out_texture: RWBuffer<Vec4> @5
    
    let pos = thread_id.xy
    let source = sample(input_texture, pos)
    
    let result = match filter_type:
        0 => box_blur(pos, kernel_size / 2)
        1 => gaussian_blur(pos, kernel_size / 2)
        2 => sharpen(pos, intensity)
        3 => edge_detect(pos)
        4 => emboss(pos)
        5 => high_pass(pos, kernel_size / 2)
        6 => median_filter(pos)
        7 => dilate(pos, kernel_size / 2)
        8 => erode(pos, kernel_size / 2)
        9 => invert(source)
        _ => source
    
    // Blend with source based on intensity
    let final = if filter_type != 9:
        lerp(source, result, clamp(intensity, 0.0, 1.0))
    else:
        result
    
    out_texture[pos] = final
```

---

### Phase 5: Specialized Shaders (Priority 5)

**Toon Shaders** — Keep separate (4 files, domain-specific)
**Glossy Shaders** — Keep separate (3 files, PBR microfacet components)
**PBR Components** — Consolidate into stdlib:
  - `fresnel_schlick()` → `Kain/stdlib/ue5/shaders.kn`
  - `ggx_distribution()` → stdlib
  - `smith_visibility()` → stdlib

---

## Consolidation Benefits

### Before (Current State)
- **22 shader files** (21 .usf, 1 .ush)
- **~100KB total shader code**
- **Duplicate implementations** (blend modes in 2 files, filters in 2 files, noise in 3 files)
- **Manual C++ binding** (33-parameter structs, 7 shader classes)

### After (KAIN)
- **~8 KAIN shader files**
  1. `pbr_generator.kn` — Gradient, Height Integration, Final PBR (3 kernels)
  2. `layer_blend.kn` — 20 blend modes
  3. `image_filters.kn` — 13 filter types
  4. `noise_generator.kn` — 16 noise types
  5. `seamless_packing.kn` — Seamless + ORM packing
  6. `toon_shading.kn` — 4 toon kernels
  7. `glossy_shading.kn` — 3 glossy kernels
  8. `pbr_components.kn` — Fresnel, GGX, Smith (or move to stdlib)

- **~40KB KAIN code** (60% reduction)
- **Zero duplication** — Single source of truth per algorithm
- **Auto C++ binding** — No manual `SHADER_PARAMETER_STRUCT`
- **Stdlib integration** — Common functions globally available

---

## Recommended Consolidation Order

### Sprint 1: PBR Core (Week 1)
1. Port `PBRGenerator.usf` → `pbr_generator.kn`
2. Test gradient extraction, height integration, final PBR passes
3. Validate against existing C++ output (pixel-perfect match)

### Sprint 2: Layer System (Week 2)
4. Port blend modes → `layer_blend.kn`
5. Port filters → `image_filters.kn`
6. Test layer stack evaluation

### Sprint 3: Procedural (Week 3)
7. Extract noise functions to stdlib
8. Port noise generators → `noise_generator.kn`
9. Test all 16 noise types

### Sprint 4: Utilities (Week 4)
10. Port seamless + packing → `seamless_packing.kn`
11. Keep specialized shaders as-is (toon, glossy)
12. Full regression test against 20 Factory plugins

---

## Technical Debt Identified

### Duplication
- **Blend modes** — 2 implementations (MaterializeBlend.usf vs LayerBlend.usf)
- **Filters** — 2 implementations (MaterializeFilters.usf vs LayerFilter.usf)
- **Noise** — 3 files (NoiseGenerator, ProceduralNoise, ProceduralCommon)
- **Hash functions** — Defined in 3 places

### Inconsistencies
- **Blend mode enums** — MaterializeBlend uses 0-15, LayerBlend uses 0-19
- **Parameter packing** — Some use float4 vectors, others use individual floats
- **Naming** — MaterializeFilters uses `FilterParams`, LayerFilter uses individual params

### Missing Features
- **Shader permutations** — No `CFG_*` / `ENABLE_*` compile-time branches
- **Shared libraries** — MaterializeProceduralCommon.ush not used by all shaders
- **Documentation** — No inline comments explaining algorithm choices

---

## KAIN Migration Strategy

### Automatic Conversions

| USF Pattern | KAIN Equivalent |
|-------------|-----------------|
| `Texture2D<float4> In` | `uniform in: Sampler2D @N` |
| `RWTexture2D<float4> Out` | `buffer out: RWBuffer<Vec4> @N` |
| `float Param` | `uniform param: Float @N` |
| `uint2 Dims` | `uniform dims: Vec2 @N` |
| `[numthreads(8,8,1)]` | Auto-generated from buffer size |
| `ThreadId.xy` | `thread_id.xy` |
| `if (x >= w \|\| y >= h) return` | Auto-generated bounds check |
| `switch (mode) { case 0: ... }` | `match mode: 0 => ...` |

### Manual Conversions

| USF Pattern | KAIN Approach |
|-------------|---------------|
| `SampleSafe()` helper | Extract to stdlib as `sample_clamped()` |
| `Hash21()` | Extract to stdlib as `hash21()` |
| `LinearizeGamma()` | Extract to stdlib as `linearize_srgb()` |
| Ping-pong logic | Use KAIN array indexing with modulo |
| Static const arrays | Use KAIN array literals |

---

## Validation Checklist

### Per-Shader Validation
- [ ] Pixel-perfect output match (compare against USF)
- [ ] Parameter binding correctness (all 33 params for PBR)
- [ ] Thread group size validation (8x8x1)
- [ ] Bounds checking (early return on out-of-bounds)
- [ ] RDG resource transitions (SRV/UAV states)

### Integration Validation
- [ ] Multi-pass pipeline (Gradient → Height → Final)
- [ ] Ping-pong buffer logic (even/odd frame detection)
- [ ] Seamless tiling (3 modes)
- [ ] ORM packing (R=AO, G=Rough, B=Metal)
- [ ] Layer stack evaluation (blend + filter + mask)

### Performance Validation
- [ ] GPU profiling (RenderDoc capture)
- [ ] Memory usage (transient texture reuse)
- [ ] Dispatch overhead (batch multiple passes)
- [ ] Compilation time (KAIN vs USF)

---

## Appendix: Complete Shader File List

### Core Shaders (6 files)
1. `PBRGenerator.usf` — 16.5KB, 4 kernels (Gradient, HeightIntegration, FinalPBR, MainCS)
2. `MaterializeBlend.usf` — 5.6KB, 1 kernel, 16 blend modes
3. `MaterializeFilters.usf` — 9.7KB, 6 kernels (Blur H/V, Sharpen, Edge, Levels, HSL, Invert, Grayscale)
4. `MaterializeNoiseGenerator.usf` — 6.2KB, 5 kernels (Noise, Radial, Circle, Bricks, Dots)
5. `SeamlessAndPacking.usf` — 6.1KB, 2 kernels (Seamless, PackORM)
6. `MaterializeProceduralCommon.ush` — 8.5KB, header-only library

### KStudioCore Shaders (5 files)
7. `KStudioCore/LayerBlend.usf` — 8.4KB, 1 kernel, 20 blend modes
8. `KStudioCore/LayerFilter.usf` — 6.5KB, 1 kernel, 13 filter types
9. `KStudioCore/ProceduralNoise.usf` — 12KB, 1 kernel, 16 noise types
10. `KStudioCore/LayerAdjustment.usf` — 7.5KB (not analyzed)
11. `KStudioCore/MathOperations.usf` — 1.4KB (not analyzed)

### Glossy Shaders (3 files)
12. `GlossyDualLobe.usf` — 2KB, dual-lobe specular
13. `GlossyClearCoat.usf` — 1.7KB, clear coat layer
14. `GlossySubsurface.usf` — 1.4KB, SSS approximation

### Toon Shaders (5 files)
15. `ToonCelShading.usf` — 1.7KB, cel shading with bands
16. `ToonOutlineDetection.usf` — 1.3KB, edge detection
17. `ToonRimLight.usf` — 1.1KB, Fresnel rim
18. `ToonSpecular.usf` — 1.3KB, stylized specular
19. `ToonConfigurableBands.usf` — 1.8KB, custom band positions

### PBR Components (3 files)
20. `MaterializeFresnelSchlick.usf` — 789B
21. `MaterializeGGXDistribution.usf` — 905B
22. `MaterializeSmithVisibility.usf` — 1.1KB

### Vertex Shaders (2 files)
23. `SimpleVertex.usf` — 29B (includes SimpleVertex.hlsl)
24. `SimpleVertex.hlsl` — 590B

---

## Next Steps

1. **Create KAIN shader stubs** — Start with `pbr_generator.kn` (highest complexity)
2. **Implement stdlib helpers** — Extract common functions (hash, linearize, blend modes)
3. **Test single-pass first** — Validate `MainCS` before multi-pass
4. **Incremental migration** — One shader at a time, validate against USF output
5. **Update KAIN.toml** — Add shader module configuration
6. **Regression suite** — Test against all 20 Factory plugins

---

**End of Analysis**
