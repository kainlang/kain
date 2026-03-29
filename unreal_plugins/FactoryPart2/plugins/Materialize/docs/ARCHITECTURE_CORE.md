# Materialize Plugin — Core Architecture Documentation

> **Part 1: Core Systems & Data Structures**  
> Analysis of the original C++ plugin for KAIN rebuild

---

## Executive Summary

Materialize is a GPU-accelerated PBR texture generation plugin that converts single source images into complete material sets (Normal, Roughness, Metallic, AO, Height, Emissive, ORM). The architecture is built around three core pillars:

1. **Layer System** — Photoshop-style compositing with 20 blend modes, masks, and procedural generation
2. **Compute Engine** — Multi-pass GPU pipeline using RDG (Render Dependency Graph) for high-performance texture processing
3. **Preset System** — 30+ material presets with master material variants (Standard, Metal, Glossy, Toon)

**Key Metrics:**
- 7 PBR output channels (BaseColor, Normal, Roughness, Metallic, Height, AO, Emissive)
- 20 blend modes (Normal, Multiply, Screen, Overlay, SoftLight, etc.)
- 15 procedural noise types (Perlin, Simplex, Worley, FBM, Cellular, etc.)
- 13 filter types (Blur, Sharpen, EdgeDetect, Emboss, Dilate, Erode, etc.)
- 9 adjustment types (Levels, Curves, HSV, Brightness/Contrast, ColorBalance, etc.)
- 8 generator types (AmbientOcclusion, Curvature, Position, WorldNormal, etc.)
- 4 master material presets with specialized shading models

---

## 1. Type System (`MaterializeTypes.h`)


### 1.1 Core Enums

#### EMaterializeCategory
Material category for organizing presets into logical groups.

```cpp
enum class EMaterializeCategory : uint8
{
    Organic,    // Skin, leather, bark, flesh
    Rubber,     // Rubber, latex, tire, plastic, gasket
    Ground,     // Mud, rock, stone, concrete
    Fabric,     // Cloth, canvas, textile
    Metal,      // Steel, aluminum, copper, iron
    Plastic,    // Acrylic, polycarbonate
    Paper,      // Paper, cardboard
    Custom      // User-defined
};
```

**KAIN Implementation:**
```kain
enum MaterializeCategory:
    Organic
    Rubber
    Ground
    Fabric
    Metal
    Plastic
    Paper
    Custom
```

---

#### EKSeamlessMode
Seamless tiling algorithms for making textures tileable.

```cpp
enum class EKSeamlessMode : uint8
{
    None,        // No tiling
    CrossBlend,  // Edge cross-fade (default, 0.25 blend width)
    MirrorBlend, // Mirror edges then blend
    Histogram    // Histogram matching for color continuity
};
```

**Algorithm Details:**
- **CrossBlend:** Blends opposite edges using linear falloff (configurable blend width 0.1-0.5)
- **MirrorBlend:** Mirrors texture at edges before blending (reduces seam visibility)
- **Histogram:** Matches color histograms across edges (best for photographic textures)

**KAIN Implementation:**
```kain
enum SeamlessMode:
    None
    CrossBlend
    MirrorBlend
    Histogram
```

---


### 1.2 PBR Generation Parameters (`FMaterializeParams`)

Complete parameter set for PBR map generation — 30+ fields organized by category.

```cpp
struct FMaterializeParams
{
    // --- Normal Map (1 param) ---
    float NormalStrength = 1.0f;  // Range: 0.0-2.0
    
    // --- Roughness (5 params) ---
    float RoughnessBase = 0.7f;           // Range: 0.0-1.0
    float RoughnessContrast = 1.0f;       // Range: 0.0-3.0
    float RoughnessBrightness = 0.0f;     // Range: -128 to 128
    bool  bRoughnessInvert = true;
    float VarianceWeight = 0.5f;          // Range: 0.0-1.0 (local variance blend)
    
    // --- Metallic (4 params) ---
    float MetallicBase = 0.0f;            // Range: 0.0-1.0
    float MetallicContrast = 1.0f;        // Range: 0.0-3.0
    float MetallicBias = 0.0f;            // Range: -128 to 128
    float MetallicSensitivity = 2.0f;     // Range: 0.0-5.0 (color-aware detection)
    
    // --- Ambient Occlusion (1 param) ---
    float AOIntensity = 1.0f;             // Range: 0.0-2.0
    
    // --- Height (1 param) ---
    float HeightContrast = 1.0f;          // Range: 0.0-3.0
    
    // --- Weathering (6 params) ---
    float EdgeWear = 0.0f;                // Range: 0.0-1.0
    float CavityDirt = 0.0f;              // Range: 0.0-1.0
    float Dust = 0.0f;                    // Range: 0.0-1.0
    float Grunge = 0.0f;                  // Range: 0.0-1.0
    float Scratches = 0.0f;               // Range: 0.0-1.0
    float Noise = 0.0f;                   // Range: 0.0-1.0
    
    // --- Special Effects (4 params) ---
    float BioDetail = 0.0f;               // Range: 0.0-1.0 (organic patterns)
    float BioFrequency = 1.0f;            // Range: 0.1-5.0
    float CyberDetail = 0.0f;             // Range: 0.0-1.0 (tech patterns)
    float CyberScale = 1.0f;              // Range: 0.01-1.0
    
    // --- Emissive (2 params) ---
    float EmissiveThreshold = 0.0f;       // Range: 0.0-1.0
    float EmissiveColorBoost = 1.0f;      // Range: 0.0-3.0
    
    // --- Processing (4 params) ---
    bool  bMakeSeamless = false;
    EKSeamlessMode SeamlessMode = EKSeamlessMode::CrossBlend;
    float SeamlessBlendWidth = 0.25f;     // Range: 0.1-0.5
    float Gamma = 1.0f;                   // Range: 0.5-1.5
    float Vignette = 0.0f;                // Range: 0.0-1.0
    
    // --- Output (2 params) ---
    bool  bPackORM = true;                // Pack AO/Roughness/Metallic into single texture
    int32 OutputResolution = 0;           // 0 = match input, else 64-8192
    
    // --- Advanced Normal/AO (7 params) ---
    int32 HeightIterations = 24;          // Range: 4-64 (Jacobi solver iterations)
    bool  bUseMultiPassHeight = true;     // Use 3-pass pipeline vs legacy single-pass
    int32 NormalOctaves = 3;              // Range: 1-6 (multi-scale normal detail)
    float NormalSigmaBase = 1.0f;         // Range: 0.5-3.0
    float NormalAnisotropy = 1.0f;        // Range: 0.5-2.0
    float AORadius = 4.0f;                // Range: 1.0-32.0 (horizon sampling radius)
    float AOBias = 0.0f;                  // Range: -1.0 to 1.0
    float AOContrast = 1.0f;              // Range: 0.1-3.0
    bool  bAdvancedNormal = false;        // Enable multi-octave normal generation
    bool  bAdvancedAO = false;            // Enable 8-direction horizon AO
};
```

**KAIN Implementation:**
```kain
struct MaterializeParams:
    normal_strength: Float = 1.0
    roughness_base: Float = 0.7
    roughness_contrast: Float = 1.0
    roughness_brightness: Float = 0.0
    roughness_invert: Bool = true
    variance_weight: Float = 0.5
    metallic_base: Float = 0.0
    metallic_contrast: Float = 1.0
    metallic_bias: Float = 0.0
    metallic_sensitivity: Float = 2.0
    ao_intensity: Float = 1.0
    height_contrast: Float = 1.0
    edge_wear: Float = 0.0
    cavity_dirt: Float = 0.0
    dust: Float = 0.0
    grunge: Float = 0.0
    scratches: Float = 0.0
    noise: Float = 0.0
    bio_detail: Float = 0.0
    bio_frequency: Float = 1.0
    cyber_detail: Float = 0.0
    cyber_scale: Float = 1.0
    emissive_threshold: Float = 0.0
    emissive_color_boost: Float = 1.0
    make_seamless: Bool = false
    seamless_mode: SeamlessMode = SeamlessMode::CrossBlend
    seamless_blend_width: Float = 0.25
    gamma: Float = 1.0
    vignette: Float = 0.0
    pack_orm: Bool = true
    output_resolution: Int = 0
    height_iterations: Int = 24
    use_multipass_height: Bool = true
    normal_octaves: Int = 3
    normal_sigma_base: Float = 1.0
    normal_anisotropy: Float = 1.0
    ao_radius: Float = 4.0
    ao_bias: Float = 0.0
    ao_contrast: Float = 1.0
    advanced_normal: Bool = false
    advanced_ao: Bool = false
```

---


### 1.3 Result Structures

#### FMaterializeResult
Output structure containing all generated PBR textures.

```cpp
struct FMaterializeResult
{
    TObjectPtr<UTexture2D> LayerBaseColor;  // From layer stack (may be null)
    TObjectPtr<UTexture2D> Normal;          // Tangent-space normal map
    TObjectPtr<UTexture2D> Roughness;       // Grayscale roughness
    TObjectPtr<UTexture2D> Metallic;        // Grayscale metallic
    TObjectPtr<UTexture2D> AO;              // Ambient occlusion
    TObjectPtr<UTexture2D> Height;          // Displacement/height map
    TObjectPtr<UTexture2D> Emissive;        // Emissive mask
    TObjectPtr<UTexture2D> ORM;             // Packed: R=AO, G=Roughness, B=Metallic
    TObjectPtr<UMaterialInstanceDynamic> Material;  // Generated material instance
    float GenerationTimeMs;                 // Performance metric
    
    bool IsValid() const { return Normal != nullptr && Roughness != nullptr; }
};
```

**KAIN Implementation:**
```kain
struct MaterializeResult:
    layer_base_color: Texture2D
    normal: Texture2D
    roughness: Texture2D
    metallic: Texture2D
    ao: Texture2D
    height: Texture2D
    emissive: Texture2D
    orm: Texture2D
    material: MaterialInstanceDynamic
    generation_time_ms: Float
    
    fn is_valid() -> Bool:
        return normal != null and roughness != null
```

---

#### FMaterializePreset
Single preset configuration (30+ presets defined in MaterializePresets.cpp).

```cpp
struct FMaterializePreset
{
    FName Id;                           // Unique identifier (e.g., "skin_basic")
    FText DisplayName;                  // UI display name
    EMaterializeCategory Category;      // Preset category
    FMaterializeParams Params;          // Parameter configuration
};
```

**Example Presets:**
- **Organic:** skin_basic, leather_worn, alien_bio, bark, zombie, dragon_scale
- **Rubber:** rubber_matte, latex_shiny, tire_worn, plastic_rough, gasket
- **Ground:** ground_wet, rock_rough, concrete_smooth, stone_polished
- **Metal:** steel_brushed, aluminum_anodized, copper_oxidized, iron_rusted
- **Fabric:** cotton_woven, canvas_rough, silk_smooth, denim_worn

**KAIN Implementation:**
```kain
struct MaterializePreset:
    id: String
    display_name: String
    category: MaterializeCategory
    params: MaterializeParams
```

---


#### FMaterializeMasterPreset
Master material preset descriptor for specialized shading models.

```cpp
struct FMaterializeMasterPreset
{
    FName PresetId;                                 // "Standard", "Metal", "Glossy", "Toon"
    FText DisplayName;                              // UI display name
    FText Description;                              // Tooltip description
    FSoftObjectPath MasterMaterialPath;             // Path to master material asset
    TSoftObjectPtr<UTexture2D> PreviewThumbnail;    // Preview icon
    TMap<FName, float> DefaultScalarParams;         // Default scalar overrides
    TMap<FName, FLinearColor> DefaultVectorParams;  // Default vector overrides
    
    // Feature flags
    bool bSupportsAnisotropy = false;
    bool bSupportsClearCoat = false;
    bool bSupportsSubsurface = false;
    bool bSupportsToonShading = false;
};
```

**Built-in Master Presets:**

| Preset | Path | Features |
|--------|------|----------|
| **Standard** | `/Materialize/Materials/M_Materialize_Master` | Standard PBR workflow |
| **Metal** | `/Materialize/Materials/Presets/M_Materialize_Master_Metal` | Anisotropic specular, enhanced reflections |
| **Glossy** | `/Materialize/Materials/Presets/M_Materialize_Master_Glossy` | Clear coat, subsurface scattering |
| **Toon** | `/Materialize/Materials/Presets/M_Materialize_Master_Toon` | Cel-shading, configurable bands |

**KAIN Implementation:**
```kain
struct MaterializeMasterPreset:
    preset_id: String
    display_name: String
    description: String
    master_material_path: String
    preview_thumbnail: Texture2D
    default_scalar_params: Map<String, Float>
    default_vector_params: Map<String, Color>
    supports_anisotropy: Bool = false
    supports_clear_coat: Bool = false
    supports_subsurface: Bool = false
    supports_toon_shading: Bool = false
```

---

## 2. Layer System Architecture

The layer system is the heart of Materialize — a Photoshop-style compositor with GPU-accelerated evaluation.

### 2.1 Layer Types (`EKLayerType`)

```cpp
enum class EKLayerType : uint8
{
    Base,        // Foundation layer linked to FMaterializeParams
    Image,       // Static texture input
    Procedural,  // Generated noise/patterns
    Fill,        // Solid color/value
    Adjustment,  // HSV, Levels, Curves (requires source)
    Filter,      // Blur, Sharpen, Edge (requires source)
    Generator,   // AO, Curvature, Position (computed from mesh data)
    Folder       // Group/Folder container (no output)
};
```

**Layer Type Characteristics:**

| Type | Produces Output | Requires Source | GPU Accelerated | Use Case |
|------|----------------|-----------------|-----------------|----------|
| Base | ✓ | ✗ | ✓ | Foundation layer with PBR params |
| Image | ✓ | ✗ | ✗ | Static texture reference |
| Procedural | ✓ | ✗ | ✓ | Noise, patterns, grunge |
| Fill | ✓ | ✗ | ✗ (CPU) | Solid colors, masks |
| Adjustment | ✓ | ✓ | ✓ | Color grading, levels |
| Filter | ✓ | ✓ | ✓ | Blur, sharpen, edge detect |
| Generator | ✓ | ✗ | ✓ | Mesh-based effects (AO, curvature) |
| Folder | ✗ | ✗ | ✗ | Organization only |

**KAIN Implementation:**
```kain
enum LayerType:
    Base
    Image
    Procedural
    Fill
    Adjustment
    Filter
    Generator
    Folder
```

---


### 2.2 Blend Modes (`EKLayerBlendMode`)

20 Photoshop-compatible blend modes implemented in GPU compute shaders.

```cpp
enum class EKLayerBlendMode : uint8
{
    Normal,      // Direct replacement
    Multiply,    // base * blend
    Screen,      // 1 - (1-base) * (1-blend)
    Overlay,     // Multiply if base < 0.5, Screen if base >= 0.5
    SoftLight,   // Soft dodge/burn
    HardLight,   // Hard dodge/burn
    Add,         // min(base + blend, 1.0)
    Subtract,    // max(base - blend, 0.0)
    Difference,  // abs(base - blend)
    Exclusion,   // base + blend - 2*base*blend
    Darken,      // min(base, blend)
    Lighten,     // max(base, blend)
    ColorDodge,  // base / (1 - blend)
    ColorBurn,   // 1 - (1-base) / blend
    LinearDodge, // Same as Add
    LinearBurn,  // max(base + blend - 1, 0)
    VividLight,  // ColorBurn if blend < 0.5, ColorDodge if >= 0.5
    LinearLight, // base + 2*blend - 1
    PinLight,    // Darken if blend < 0.5, Lighten if >= 0.5
    HardMix      // Posterized VividLight
};
```

**Blend Mode Mathematics:**

All blend modes follow this compositing formula:
```
effectiveOpacity = layerOpacity * maskValue
finalBlendAmount = effectiveOpacity * blendLayerAlpha
result.rgb = lerp(base.rgb, blendFunction(base.rgb, blend.rgb), finalBlendAmount)
result.a = base.a + blend.a * effectiveOpacity * (1 - base.a)
```

**GPU Implementation:** `Shaders/KStudioCore/LayerBlend.usf` — 234 lines, 20 blend functions, proper alpha compositing.

**KAIN Implementation:**
```kain
enum LayerBlendMode:
    Normal
    Multiply
    Screen
    Overlay
    SoftLight
    HardLight
    Add
    Subtract
    Difference
    Exclusion
    Darken
    Lighten
    ColorDodge
    ColorBurn
    LinearDodge
    LinearBurn
    VividLight
    LinearLight
    PinLight
    HardMix
```

---


### 2.3 Output Channel Flags (`EKLayerOutputChannel`)

Bitflag enum for controlling which PBR channels a layer affects.

```cpp
enum class EKLayerOutputChannel : uint8
{
    None        = 0,
    BaseColor   = 1 << 0,  // 0x01
    Normal      = 1 << 1,  // 0x02
    Roughness   = 1 << 2,  // 0x04
    Metallic    = 1 << 3,  // 0x08
    Height      = 1 << 4,  // 0x10
    AO          = 1 << 5,  // 0x20
    Emissive    = 1 << 6,  // 0x40
    Mask        = 1 << 7,  // 0x80
    All         = 0xFF     // All channels
};
ENUM_CLASS_FLAGS(EKLayerOutputChannel);
```

**Usage Pattern:**
```cpp
// Layer affects only Normal and Roughness
layer.OutputChannels = static_cast<int32>(EKLayerOutputChannel::Normal) | 
                       static_cast<int32>(EKLayerOutputChannel::Roughness);

// Check if layer affects BaseColor
if (layer.OutputChannels & static_cast<int32>(EKLayerOutputChannel::BaseColor))
{
    // Blend into BaseColor channel
}
```

**KAIN Implementation:**
```kain
enum LayerOutputChannel:
    None = 0
    BaseColor = 1
    Normal = 2
    Roughness = 4
    Metallic = 8
    Height = 16
    AO = 32
    Emissive = 64
    Mask = 128
    All = 255
```

---

### 2.4 Procedural Noise Types (`EKProceduralNoiseType`)

15 procedural noise algorithms for texture generation.

```cpp
enum class EKProceduralNoiseType : uint8
{
    Perlin,      // Classic Perlin noise
    Simplex,     // Simplex noise (faster, no directional artifacts)
    Worley,      // Voronoi/cellular patterns
    FBM,         // Fractal Brownian Motion
    Turbulence,  // Absolute value FBM
    Cellular,    // Cell-based patterns
    Gradient,    // Linear/radial gradients
    Checker,     // Checkerboard pattern
    Brick,       // Brick tiling pattern
    Herringbone, // Herringbone tiling
    Hexagon,     // Hexagonal tiling
    Scratches,   // Directional scratch patterns
    Grunge,      // Dirt/wear patterns
    Rust,        // Rust/corrosion patterns
    Dust         // Dust accumulation patterns
};
```

**Procedural Parameters:**
```cpp
struct FKProceduralParams
{
    EKProceduralNoiseType NoiseType = Perlin;
    float Scale = 1.0f;              // Range: 0.01-100.0
    int32 Octaves = 4;               // Range: 1-16 (FBM layers)
    float Persistence = 0.5f;        // Range: 0.0-1.0 (amplitude decay)
    float Lacunarity = 2.0f;         // Range: 1.0-4.0 (frequency multiplier)
    FVector2D Offset = ZeroVector;   // UV offset
    int32 Seed = 0;                  // Random seed
    bool bSeamless = false;          // Seamless tiling
    float Time = 0.0f;               // Animation time
};
```

**GPU Implementation:** `Shaders/KStudioCore/ProceduralNoise.usf` — 12KB, all 15 noise types.

**KAIN Implementation:**
```kain
enum ProceduralNoiseType:
    Perlin
    Simplex
    Worley
    FBM
    Turbulence
    Cellular
    Gradient
    Checker
    Brick
    Herringbone
    Hexagon
    Scratches
    Grunge
    Rust
    Dust

struct ProceduralParams:
    noise_type: ProceduralNoiseType = ProceduralNoiseType::Perlin
    scale: Float = 1.0
    octaves: Int = 4
    persistence: Float = 0.5
    lacunarity: Float = 2.0
    offset: Vec2 = vec2(0.0, 0.0)
    seed: Int = 0
    seamless: Bool = false
    time: Float = 0.0
```

---


### 2.5 Filter Types (`EKFilterType`)

13 image processing filters for texture manipulation.

```cpp
enum class EKFilterType : uint8
{
    Blur,         // Box blur (fast)
    GaussianBlur, // Gaussian blur (smooth)
    Sharpen,      // Unsharp mask
    EdgeDetect,   // Sobel edge detection
    Emboss,       // 3D emboss effect
    HighPass,     // High-frequency detail extraction
    LowPass,      // Low-frequency smoothing
    Median,       // Median filter (noise reduction)
    Dilate,       // Morphological dilation (expand bright areas)
    Erode,        // Morphological erosion (expand dark areas)
    Invert,       // Color inversion
    Normalize,    // Stretch to 0-1 range
    AutoLevels    // Automatic contrast adjustment
};
```

**Filter Parameters:**
```cpp
struct FKFilterParams
{
    EKFilterType FilterType = Blur;
    float Intensity = 1.0f;      // Range: 0.0-100.0 (blend with source)
    int32 KernelSize = 3;        // Range: 1-32 (filter radius)
    float Threshold = 0.0f;      // Range: 0.0-10.0 (filter-specific)
};
```

**GPU Implementation:** `Shaders/KStudioCore/LayerFilter.usf` — 245 lines, all 13 filter types.

**KAIN Implementation:**
```kain
enum FilterType:
    Blur
    GaussianBlur
    Sharpen
    EdgeDetect
    Emboss
    HighPass
    LowPass
    Median
    Dilate
    Erode
    Invert
    Normalize
    AutoLevels

struct FilterParams:
    filter_type: FilterType = FilterType::Blur
    intensity: Float = 1.0
    kernel_size: Int = 3
    threshold: Float = 0.0
```

---

### 2.6 Adjustment Types (`EKAdjustmentType`)

9 color correction and grading adjustments.

```cpp
enum class EKAdjustmentType : uint8
{
    Levels,       // Input/output levels with gamma
    Curves,       // Tone curves (simplified S-curve)
    HSV,          // Hue/Saturation/Value adjustment
    Brightness,   // Brightness/Contrast
    ColorBalance, // Shadow/highlight color shifts
    Vibrance,     // Smart saturation (preserves skin tones)
    Threshold,    // Binary threshold (B&W conversion)
    Posterize,    // Reduce color levels
    Gradient      // Gradient map (grayscale to gradient)
};
```

**Adjustment Parameters:**
```cpp
struct FKAdjustmentParams
{
    EKAdjustmentType AdjustmentType = Levels;
    
    // Levels
    float InputBlack = 0.0f;      // Range: 0.0-1.0
    float InputWhite = 1.0f;      // Range: 0.0-1.0
    float Gamma = 1.0f;           // Range: 0.1-9.9
    float OutputBlack = 0.0f;     // Range: 0.0-1.0
    float OutputWhite = 1.0f;     // Range: 0.0-1.0
    
    // HSV
    float HueShift = 0.0f;        // Range: -180 to 180 degrees
    float SaturationAdjust = 0.0f; // Range: -1.0 to 1.0
    float ValueAdjust = 0.0f;     // Range: -1.0 to 1.0
    
    // Brightness/Contrast
    float Brightness = 0.0f;      // Range: -1.0 to 1.0
    float Contrast = 0.0f;        // Range: -1.0 to 1.0
};
```

**GPU Implementation:** `Shaders/KStudioCore/LayerAdjustment.usf` — 255 lines, RGB↔HSV↔HSL conversions, all 9 adjustment types.

**KAIN Implementation:**
```kain
enum AdjustmentType:
    Levels
    Curves
    HSV
    Brightness
    ColorBalance
    Vibrance
    Threshold
    Posterize
    Gradient

struct AdjustmentParams:
    adjustment_type: AdjustmentType = AdjustmentType::Levels
    input_black: Float = 0.0
    input_white: Float = 1.0
    gamma: Float = 1.0
    output_black: Float = 0.0
    output_white: Float = 1.0
    hue_shift: Float = 0.0
    saturation_adjust: Float = 0.0
    value_adjust: Float = 0.0
    brightness: Float = 0.0
    contrast: Float = 0.0
```

---


### 2.7 Generator Types (`EKGeneratorType`)

8 mesh-based texture generators (computed from geometry data).

```cpp
enum class EKGeneratorType : uint8
{
    AmbientOcclusion, // 8-direction horizon sampling
    Curvature,        // Surface curvature detection
    Position,         // World/object space position
    WorldNormal,      // World-space normal map
    Thickness,        // Mesh thickness map
    EdgeWear,         // Edge detection for wear masks
    Dirt,             // Cavity-based dirt accumulation
    LightMap          // Baked lighting
};
```

**Generator → Preset Shader Mapping:**

Generators are mapped to preset shaders for visualization (defined in `KLayerEvaluator.cpp`):

| Generator Type | Preset Shader | Purpose |
|----------------|---------------|---------|
| AmbientOcclusion | MaterializeSmithVisibility | Smith visibility term visualization |
| Curvature | MaterializeFresnelSchlick | Fresnel-based curvature |
| Position | MaterializeGGXDistribution | GGX distribution visualization |
| WorldNormal | ToonOutlineDetection | Normal-based outline detection |
| Thickness | GlossySubsurface | Subsurface scattering approximation |
| EdgeWear | MetalFresnelRim | Fresnel rim lighting |
| Dirt | ToonConfigurableBands | Banded lighting |
| LightMap | GlossyClearCoat | Clear coat visualization |

**KAIN Implementation:**
```kain
enum GeneratorType:
    AmbientOcclusion
    Curvature
    Position
    WorldNormal
    Thickness
    EdgeWear
    Dirt
    LightMap
```

---

### 2.8 Layer Data Structure (`FKLayer`)

Complete layer definition with all properties and type-specific data.

```cpp
struct FKLayer
{
    // --- Identity ---
    FName Name = NAME_None;
    FGuid Id;  // Unique identifier (generated on creation)
    
    // --- Type ---
    EKLayerType LayerType = Image;
    
    // --- Blending ---
    EKLayerBlendMode BlendMode = Normal;
    float Opacity = 1.0f;  // Range: 0.0-1.0
    
    // --- Output Channels (bitflags) ---
    int32 OutputChannels = static_cast<int32>(EKLayerOutputChannel::All);
    
    // --- Visibility ---
    bool bEnabled = true;   // Layer is active
    bool bLocked = false;   // Layer cannot be edited
    bool bSolo = false;     // Only solo layers are visible (if any solo exists)
    
    // --- Mask ---
    bool bHasMask = false;
    TObjectPtr<UTexture2D> MaskTexture;
    bool bInvertMask = false;
    
    // --- Type-Specific Data (EditCondition hides irrelevant fields) ---
    
    // Image Layer
    TObjectPtr<UTexture2D> ImageTexture;
    
    // Fill Layer
    FLinearColor FillColor = White;
    float FillValue = 1.0f;
    
    // Procedural Layer
    FKProceduralParams ProceduralParams;
    
    // Filter Layer
    FKFilterParams FilterParams;
    int32 SourceLayerIndex = INDEX_NONE;  // Which layer to filter
    TObjectPtr<UTexture2D> SourceOverride; // Override source
    
    // Adjustment Layer
    FKAdjustmentParams AdjustmentParams;
    // (shares SourceLayerIndex/SourceOverride with Filter)
    
    // Generator Layer
    EKGeneratorType GeneratorType = AmbientOcclusion;
    
    // Folder Layer
    bool bFolderExpanded = true;
    int32 ParentIndex = INDEX_NONE;  // Parent folder index
    
    // --- State ---
    bool bDirty = true;  // Needs re-evaluation
    TObjectPtr<UTexture2D> CachedOutput;  // Transient, not saved
};
```

**KAIN Implementation:**
```kain
struct Layer:
    name: String
    id: String
    layer_type: LayerType = LayerType::Image
    blend_mode: LayerBlendMode = LayerBlendMode::Normal
    opacity: Float = 1.0
    output_channels: Int = 255
    enabled: Bool = true
    locked: Bool = false
    solo: Bool = false
    has_mask: Bool = false
    mask_texture: Texture2D
    invert_mask: Bool = false
    image_texture: Texture2D
    fill_color: Color = color(1.0, 1.0, 1.0, 1.0)
    fill_value: Float = 1.0
    procedural_params: ProceduralParams
    filter_params: FilterParams
    adjustment_params: AdjustmentParams
    source_layer_index: Int = -1
    source_override: Texture2D
    generator_type: GeneratorType = GeneratorType::AmbientOcclusion
    folder_expanded: Bool = true
    parent_index: Int = -1
    dirty: Bool = true
    @transient
    cached_output: Texture2D
```

---


### 2.9 Layer Stack (`FKLayerStack`)

Container for all layers with stack-level operations and versioning.

```cpp
struct FKLayerStack
{
    // --- Versioning ---
    int32 Version = EKLayerStackVersion::Latest;  // Serialization version
    
    // --- Layers (bottom to top order) ---
    TArray<FKLayer> Layers;  // Index 0 = bottom, higher indices = top
    
    // --- Stack Properties ---
    int32 Width = 1024;
    int32 Height = 1024;
    
    // --- Selection ---
    int32 SelectedLayerIndex = INDEX_NONE;
    
    // --- Methods ---
    
    // Layer Management
    int32 AddLayer(const FKLayer& Layer);
    int32 InsertLayer(int32 Index, const FKLayer& Layer);
    bool RemoveLayer(int32 Index);
    bool MoveLayer(int32 FromIndex, int32 ToIndex);
    int32 DuplicateLayer(int32 Index);
    
    // Dirty Tracking
    void MarkDirty(int32 Index);        // Mark layer + all above as dirty
    void MarkAllDirty();
    void ClearDirtyFlags();
    
    // Visibility Resolution
    TArray<int32> GetVisibleLayerIndices() const;
    
    // Search
    int32 FindLayerByGuid(const FGuid& Guid) const;
    int32 FindLayerByName(FName Name) const;
    
    // Factory Methods
    static FKLayer CreateImageLayer(FName Name, UTexture2D* Texture);
    static FKLayer CreateFillLayer(FName Name, FLinearColor Color);
    static FKLayer CreateProceduralLayer(FName Name, EKProceduralNoiseType NoiseType);
    static FKLayer CreateFilterLayer(FName Name, EKFilterType FilterType);
    static FKLayer CreateAdjustmentLayer(FName Name, EKAdjustmentType AdjustmentType);
    static FKLayer CreateFolderLayer(FName Name);
    
    // Backward Compatibility
    bool MigrateFromOldVersion();
};
```

**Visibility Resolution Logic:**

The `GetVisibleLayerIndices()` method implements Photoshop-style solo/lock behavior:

```cpp
TArray<int32> GetVisibleLayerIndices() const
{
    TArray<int32> Result;
    bool bHasSolo = false;
    
    // Check if any enabled solo layers exist
    for (const FKLayer& L : Layers)
    {
        if (L.bSolo && L.bEnabled && !L.bLocked)
        {
            bHasSolo = true;
            break;
        }
    }
    
    // Collect visible layers
    for (int32 i = 0; i < Layers.Num(); ++i)
    {
        const FKLayer& Layer = Layers[i];
        if (!Layer.bEnabled) continue;                          // Disabled layers are hidden
        if (Layer.bLocked && Layer.LayerType != Base) continue; // Locked layers are hidden (except Base)
        if (bHasSolo && !Layer.bSolo) continue;                 // If solo exists, only solo layers visible
        Result.Add(i);
    }
    
    return Result;  // Returns indices in bottom-to-top order
}
```

**Dirty Propagation:**

When a layer is modified, all layers above it must be marked dirty (they depend on accumulated result):

```cpp
void MarkDirty(int32 Index)
{
    if (Layers.IsValidIndex(Index))
    {
        Layers[Index].bDirty = true;
        // Mark all layers above as dirty (they depend on this one)
        for (int32 i = Index + 1; i < Layers.Num(); ++i)
        {
            Layers[i].bDirty = true;
        }
    }
}
```

**KAIN Implementation:**
```kain
struct LayerStack:
    version: Int = 3
    layers: Array<Layer>
    width: Int = 1024
    height: Int = 1024
    selected_layer_index: Int = -1
    
    fn add_layer(layer: Layer) -> Int:
        push(layers, layer)
        return len(layers) - 1
    
    fn insert_layer(index: Int, layer: Layer) -> Int:
        # Implementation
        return index
    
    fn remove_layer(index: Int) -> Bool:
        # Implementation
        return true
    
    fn move_layer(from_index: Int, to_index: Int) -> Bool:
        # Implementation
        return true
    
    fn duplicate_layer(index: Int) -> Int:
        # Implementation
        return -1
    
    fn mark_dirty(index: Int):
        if index >= 0 and index < len(layers):
            layers[index].dirty = true
            let i = index + 1
            while i < len(layers):
                layers[i].dirty = true
                i = i + 1
    
    fn mark_all_dirty():
        for layer in layers:
            layer.dirty = true
    
    fn clear_dirty_flags():
        for layer in layers:
            layer.dirty = false
    
    fn get_visible_layer_indices() -> Array<Int>:
        # Implementation
        return []
```

---


## 3. Layer Evaluation System (`KLayerEvaluator`)

The layer evaluator is responsible for GPU-accelerated layer compositing and texture generation.

### 3.1 Evaluation Result (`FKLayerEvalResult`)

```cpp
struct FKLayerEvalResult
{
    TObjectPtr<UTexture2D> BaseColor;
    TObjectPtr<UTexture2D> Normal;
    TObjectPtr<UTexture2D> Roughness;
    TObjectPtr<UTexture2D> Metallic;
    TObjectPtr<UTexture2D> Height;
    TObjectPtr<UTexture2D> AO;
    TObjectPtr<UTexture2D> Emissive;
    float EvaluationTimeMs;
    
    bool IsValid() const { return BaseColor != nullptr; }
};
```

**KAIN Implementation:**
```kain
struct LayerEvalResult:
    base_color: Texture2D
    normal: Texture2D
    roughness: Texture2D
    metallic: Texture2D
    height: Texture2D
    ao: Texture2D
    emissive: Texture2D
    evaluation_time_ms: Float
    
    fn is_valid() -> Bool:
        return base_color != null
```

---

### 3.2 Core Evaluation API

```cpp
class UKLayerEvaluator : public UObject
{
public:
    // Main stack evaluation
    static bool EvaluateStack(FKLayerStack& Stack, FKLayerEvalResult& OutResult, FString& OutError);
    
    // Single layer evaluation (no compositing)
    static UTexture2D* EvaluateSingleLayer(const FKLayer& Layer, int32 Width, int32 Height, FString& OutError);
    
    // Texture blending
    static UTexture2D* BlendTextures(UTexture2D* Base, UTexture2D* Blend, 
        EKLayerBlendMode BlendMode, float Opacity,
        UTexture2D* Mask, bool bInvertMask, FString& OutError);
    
    // Procedural generation
    static UTexture2D* GenerateProceduralTexture(const FKProceduralParams& Params, 
        int32 Width, int32 Height, FString& OutError);
    
    // Filter application
    static UTexture2D* ApplyFilter(UTexture2D* Source, const FKFilterParams& Params, FString& OutError);
    
    // Adjustment application
    static UTexture2D* ApplyAdjustment(UTexture2D* Source, const FKAdjustmentParams& Params, FString& OutError);
    
    // Math operations
    static UTexture2D* AddTextures(UTexture2D* A, UTexture2D* B, FString& OutError);
    static UTexture2D* MultiplyTextures(UTexture2D* A, UTexture2D* B, FString& OutError);
    static UTexture2D* LerpTextures(UTexture2D* A, UTexture2D* B, float Alpha, FString& OutError);
    
    // Validation
    static bool ValidateLayerStack(const FKLayerStack& Stack, FString& OutError);
    static bool ValidateBlendMode(EKLayerBlendMode BlendMode);
    static bool ValidateFilterType(EKFilterType FilterType);
    static bool ValidateTextureFormat(UTexture2D* Texture, FString& OutError);
};
```

**KAIN Implementation:**
```kain
@blueprint
struct LayerEvaluator:
    @blueprint_callable
    fn evaluate_stack(stack: LayerStack) -> LayerEvalResult:
        # Implementation
        return LayerEvalResult()
    
    @blueprint_callable
    fn evaluate_single_layer(layer: Layer, width: Int, height: Int) -> Texture2D:
        # Implementation
        return null
    
    @blueprint_callable
    fn blend_textures(base: Texture2D, blend: Texture2D, blend_mode: LayerBlendMode, 
                      opacity: Float, mask: Texture2D, invert_mask: Bool) -> Texture2D:
        # Implementation
        return base
    
    @blueprint_callable
    fn generate_procedural_texture(params: ProceduralParams, width: Int, height: Int) -> Texture2D:
        # Implementation
        return null
    
    @blueprint_callable
    fn apply_filter(source: Texture2D, params: FilterParams) -> Texture2D:
        # Implementation
        return source
    
    @blueprint_callable
    fn apply_adjustment(source: Texture2D, params: AdjustmentParams) -> Texture2D:
        # Implementation
        return source
```

---


### 3.3 Stack Evaluation Algorithm

The `EvaluateStack()` method implements bottom-to-top layer compositing with caching and dirty tracking.

**Algorithm Flow:**

```
1. Pre-Dispatch Validation
   ├─ Validate stack dimensions (64-8192)
   ├─ Validate layer configurations
   └─ Check for circular dependencies (Filter/Adjustment source references)

2. Create Output Textures
   ├─ BaseColor: PF_B8G8R8A8, SRGB=true
   ├─ Normal: PF_B8G8R8A8, SRGB=false
   ├─ Roughness: PF_B8G8R8A8, SRGB=false (scalar in R channel)
   ├─ Metallic: PF_B8G8R8A8, SRGB=false (scalar in R channel)
   ├─ Height: PF_B8G8R8A8, SRGB=false (scalar in R channel)
   ├─ AO: PF_B8G8R8A8, SRGB=false (scalar in R channel)
   └─ Emissive: PF_B8G8R8A8, SRGB=false (scalar in R channel)

3. Get Visible Layers (bottom-to-top order)
   └─ Apply solo/lock/enabled filtering

4. For Each Visible Layer (bottom to top):
   ├─ If Filter/Adjustment:
   │  ├─ Resolve source (SourceOverride → SourceLayerIndex → accumulated BaseColor)
   │  ├─ Evaluate source if dirty
   │  └─ Apply filter/adjustment
   ├─ Else:
   │  └─ Evaluate layer if dirty (Image/Fill/Procedural/Generator)
   │
   ├─ Cache result in Layer.CachedOutput
   ├─ Clear dirty flag
   │
   └─ For Each Output Channel (BaseColor, Normal, Roughness, etc.):
      ├─ Check if layer targets this channel (OutputChannels bitflag)
      ├─ Blend layer output into channel using BlendTextures()
      └─ Update channel texture

5. Clear All Dirty Flags
6. Return Result
```

**Key Design Decisions:**

1. **Bottom-to-Top Order:** Layers are stored and evaluated in bottom-to-top order (index 0 = bottom). This ensures correct alpha compositing where each layer is blended onto the accumulated result.

2. **Per-Channel Blending:** Each layer can target multiple channels simultaneously. A single layer with `OutputChannels = All` will be blended into all 7 output channels independently.

3. **Dirty Tracking + Caching:** Layers cache their output in `CachedOutput`. When a layer is modified, it and all layers above are marked dirty. Only dirty layers are re-evaluated.

4. **Source Resolution:** Filter/Adjustment layers resolve their source in this priority:
   - `SourceOverride` (explicit texture)
   - `SourceLayerIndex` (reference to another layer)
   - Accumulated `BaseColor` (default)

5. **Transient Textures:** All output textures are transient (`UTexture2D::CreateTransient`) — not saved to disk. Use `GenerateAndSavePBRMaps()` to persist.

**KAIN Implementation:**
```kain
@blueprint
fn evaluate_stack(stack: LayerStack) -> LayerEvalResult with IO:
    let result = LayerEvalResult()
    let start_time = get_time()
    
    # Create output textures
    result.base_color = create_transient_texture(stack.width, stack.height, true)
    result.normal = create_transient_texture(stack.width, stack.height, false)
    result.roughness = create_transient_texture(stack.width, stack.height, false)
    result.metallic = create_transient_texture(stack.width, stack.height, false)
    result.height = create_transient_texture(stack.width, stack.height, false)
    result.ao = create_transient_texture(stack.width, stack.height, false)
    result.emissive = create_transient_texture(stack.width, stack.height, false)
    
    # Get visible layers
    let visible_indices = stack.get_visible_layer_indices()
    
    # Evaluate each layer (bottom to top)
    for index in visible_indices:
        let layer = stack.layers[index]
        
        # Evaluate layer if dirty
        if layer.dirty or layer.cached_output == null:
            layer.cached_output = evaluate_single_layer(layer, stack.width, stack.height)
            layer.dirty = false
        
        # Blend into each target channel
        if layer.output_channels & LayerOutputChannel::BaseColor:
            result.base_color = blend_textures(result.base_color, layer.cached_output, 
                                               layer.blend_mode, layer.opacity, 
                                               layer.mask_texture, layer.invert_mask)
        # ... repeat for all channels
    
    stack.clear_dirty_flags()
    result.evaluation_time_ms = (get_time() - start_time) * 1000.0
    return result
```

---


### 3.4 GPU Compute Shaders

The layer evaluator uses 5 compute shaders for GPU-accelerated operations.

#### Shader 1: FKLayerBlendCS (`LayerBlend.usf`)

**Purpose:** Blend two textures using 20 blend modes with mask support.

**Parameters:**
```cpp
Texture2D<float4> InBase;      // Base texture (accumulated result)
Texture2D<float4> InBlend;     // Blend texture (current layer)
Texture2D<float> InMask;       // Optional mask texture
SamplerState InSampler;
RWTexture2D<float4> OutResult; // Blended result

uint BlendMode;                // 0-19 (blend mode enum)
float Opacity;                 // 0.0-1.0
uint bHasMask;                 // 0 or 1
uint bInvertMask;              // 0 or 1
uint2 TextureDimensions;       // Width, Height
```

**Thread Group Size:** `[numthreads(8, 8, 1)]` — 64 threads per group

**Shader Path:** `/Plugin/Materialize/KStudioCore/LayerBlend.usf`

**Entry Point:** `BlendCS`

---

#### Shader 2: FKProceduralNoiseCS (`ProceduralNoise.usf`)

**Purpose:** Generate procedural noise textures (15 noise types).

**Parameters:**
```cpp
RWTexture2D<float4> OutResult;

uint NoiseType;                // 0-14 (noise type enum)
float Scale;
int32 Octaves;
float Persistence;
float Lacunarity;
FVector2f Offset;
int32 Seed;
uint bSeamless;
float Time;
uint2 TextureDimensions;
```

**Thread Group Size:** `[numthreads(8, 8, 1)]`

**Shader Path:** `/Plugin/Materialize/KStudioCore/ProceduralNoise.usf`

**Entry Point:** `NoiseCS`

---

#### Shader 3: FKFilterCS (`LayerFilter.usf`)

**Purpose:** Apply image processing filters (13 filter types).

**Parameters:**
```cpp
Texture2D<float4> InSource;
SamplerState InSampler;
RWTexture2D<float4> OutResult;

uint FilterType;               // 0-12 (filter type enum)
float Intensity;               // Blend with source
int32 KernelSize;              // Filter radius
float Threshold;               // Filter-specific
uint2 TextureDimensions;
```

**Thread Group Size:** `[numthreads(8, 8, 1)]`

**Shader Path:** `/Plugin/Materialize/KStudioCore/LayerFilter.usf`

**Entry Point:** `FilterCS`

---

#### Shader 4: FKAdjustmentCS (`LayerAdjustment.usf`)

**Purpose:** Apply color corrections and adjustments (9 adjustment types).

**Parameters:**
```cpp
Texture2D<float4> InSource;
SamplerState InSampler;
RWTexture2D<float4> OutResult;

uint AdjustmentType;           // 0-8 (adjustment type enum)
float InputBlack;
float InputWhite;
float Gamma;
float OutputBlack;
float OutputWhite;
float HueShift;
float SaturationAdjust;
float ValueAdjust;
float Brightness;
float Contrast;
uint2 TextureDimensions;
```

**Thread Group Size:** `[numthreads(8, 8, 1)]`

**Shader Path:** `/Plugin/Materialize/KStudioCore/LayerAdjustment.usf`

**Entry Point:** `AdjustmentCS`

---

#### Shader 5: FKMathOperationCS (`MathOperations.usf`)

**Purpose:** Math operations between two textures (Add, Multiply, Lerp).

**Parameters:**
```cpp
Texture2D<float4> InTextureA;
Texture2D<float4> InTextureB;
SamplerState InSampler;
RWTexture2D<float4> OutResult;

uint MathOperation;            // 0=Add, 1=Multiply, 2=Lerp
float Alpha;                   // For Lerp operation
uint2 TextureDimensions;
```

**Thread Group Size:** `[numthreads(8, 8, 1)]`

**Shader Path:** `/Plugin/Materialize/KStudioCore/MathOperations.usf`

**Entry Point:** `MathCS`

---

### 3.5 Shader Dispatch Pattern

All compute shaders follow the same RDG dispatch pattern:

```cpp
ENQUEUE_RENDER_COMMAND(CommandName)(
    [Params...](FRHICommandListImmediate& RHICmdList)
    {
        FRDGBuilder GraphBuilder(RHICmdList);
        
        // Create RDG textures
        FRDGTextureRef InputRDG = GraphBuilder.RegisterExternalTexture(...);
        FRDGTextureRef OutputRDG = GraphBuilder.CreateTexture(...);
        
        // Setup shader parameters
        FShaderType::FParameters* PassParams = GraphBuilder.AllocParameters<FShaderType::FParameters>();
        PassParams->InSource = GraphBuilder.CreateSRV(InputRDG);
        PassParams->OutResult = GraphBuilder.CreateUAV(OutputRDG);
        PassParams->Param1 = Value1;
        // ... set all parameters
        
        // Get shader instance
        TShaderMapRef<FShaderType> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        
        // Calculate dispatch size
        FIntVector GroupCount = FIntVector(
            FMath::DivideAndRoundUp(Width, 8),
            FMath::DivideAndRoundUp(Height, 8),
            1
        );
        
        // Add compute pass
        FComputeShaderUtils::AddPass(
            GraphBuilder,
            RDG_EVENT_NAME("ShaderName"),
            ComputeShader,
            PassParams,
            GroupCount
        );
        
        // Copy result back to external texture
        AddCopyTexturePass(GraphBuilder, OutputRDG, ExternalTexture, ...);
        
        // Execute graph
        GraphBuilder.Execute();
    }
);

FlushRenderingCommands();  // Wait for GPU completion
```

**KAIN Shader Integration:**

KAIN's `shader compute` syntax will generate this entire dispatch pattern automatically:

```kain
shader compute LayerBlend(thread_id: Vec3):
    uniform blend_mode: Int @0
    uniform opacity: Float @1
    uniform has_mask: Bool @2
    uniform invert_mask: Bool @3
    texture in_base: Sampler2D @4
    texture in_blend: Sampler2D @5
    texture in_mask: Sampler2D @6
    buffer out_result: RWTexture2D<Vec4> @7
    
    # Shader logic here
```

This generates:
- `FLayerBlendCS` class with `DECLARE_GLOBAL_SHADER`
- `BEGIN_SHADER_PARAMETER_STRUCT` with all uniforms
- `IMPLEMENT_GLOBAL_SHADER` macro
- RDG dispatch helper function
- Blueprint-callable wrapper

---


## 4. Compute Engine Architecture (`MaterializeComputeEngine`)

The compute engine is the high-performance GPU pipeline for PBR map generation. It uses a multi-pass approach for superior quality.

### 4.1 Core API

```cpp
class UMaterializeComputeEngine : public UObject
{
public:
    // Main GPU generation (multi-pass pipeline)
    static bool GeneratePBRMapsGPU(UTexture2D* SourceTexture, 
                                   const FMaterializeParams& Params, 
                                   FMaterializeResult& OutResult);
    
    // Seamless tiling
    static UTexture2D* MakeSeamless(UTexture2D* SourceTexture, 
                                    EKSeamlessMode Mode, 
                                    float BlendWidth = 0.25f);
    
    // ORM packing
    static UTexture2D* PackORM(UTexture2D* AO, UTexture2D* Roughness, UTexture2D* Metallic);
    
    // GPU → CPU readback
    static bool ReadbackTexture(UTexture2D* Texture, TArray<FColor>& OutPixels);
    static void ReadbackResult(const FMaterializeResult& Result, TMap<FString, TArray<FColor>>& OutMap);
    
    // Resource management
    static void CleanupTransientResources(FMaterializeResult& Result);
    static bool ValidateRHIResource(FTexture2DRHIRef TextureRHI, FString& OutError);
};
```

**KAIN Implementation:**
```kain
@blueprint
struct MaterializeComputeEngine:
    @blueprint_callable
    fn generate_pbr_maps_gpu(source_texture: Texture2D, params: MaterializeParams) -> MaterializeResult:
        # Implementation
        return MaterializeResult()
    
    @blueprint_callable
    fn make_seamless(source_texture: Texture2D, mode: SeamlessMode, blend_width: Float) -> Texture2D:
        # Implementation
        return source_texture
    
    @blueprint_callable
    fn pack_orm(ao: Texture2D, roughness: Texture2D, metallic: Texture2D) -> Texture2D:
        # Implementation
        return null
```

---

### 4.2 Multi-Pass GPU Pipeline

The compute engine uses a 3-pass approach for high-quality PBR generation:

```
Pass 1: Gradient Extraction (FKGradientCS)
├─ Input: Source texture (RGBA)
├─ Output: Gradient field (RG = dx/dy)
├─ Algorithm: Multi-octave Sobel with sRGB linearization
└─ Shader: PBRGenerator.usf::GradientCS

Pass 2: Height Integration (FKHeightIntegrationCS) — Iterative
├─ Input: Gradient field, previous height estimate
├─ Output: Next height estimate
├─ Algorithm: Jacobi iteration (Poisson solver)
├─ Iterations: 4-64 (default 24)
└─ Shader: PBRGenerator.usf::HeightIntegrationCS

Pass 3: Final PBR Generation (FKFinalPBRCS)
├─ Input: Source texture, integrated height
├─ Output: Normal, Roughness, Metallic, AO, Height, Emissive
├─ Algorithm: Multi-scale normal, color-aware metallic, variance roughness, 8-dir horizon AO
└─ Shader: PBRGenerator.usf::FinalPBRCS
```

**Why Multi-Pass?**

1. **Height Quality:** Jacobi iteration produces smooth, artifact-free height maps by solving Poisson equation (∇²h = ∇·g)
2. **Normal Quality:** Multi-scale normals combine macro (Poisson height), meso (2px height), and micro (luminance Sobel) frequencies
3. **AO Quality:** 8-direction horizon sampling with proper occlusion accumulation
4. **Performance:** Each pass is optimized for its specific task (gradient extraction is memory-bound, height integration is compute-bound)

**Legacy Single-Pass Mode:**

For fast preview, a legacy single-pass shader (`FKPBRGeneratorCS::MainCS`) generates all maps in one pass. Quality is lower but performance is 3-5x faster.

**KAIN Implementation:**

```kain
# Pass 1: Gradient Extraction
shader compute GradientExtraction(thread_id: Vec3):
    uniform normal_strength: Float @0
    uniform advanced_normal: Bool @1
    uniform normal_octaves: Int @2
    uniform normal_sigma_base: Float @3
    uniform normal_anisotropy: Float @4
    texture in_source: Sampler2D @5
    buffer out_gradient: RWTexture2D<Vec2> @6
    
    let pos = thread_id.xy
    let grad = vec2(0.0, 0.0)
    
    if advanced_normal:
        # Multi-octave Sobel
        for k in range(0, normal_octaves):
            let sigma = normal_sigma_base * pow(2.0, k as Float)
            # ... compute gradient at scale sigma
    else:
        # Standard 3x3 Sobel
        # ... compute gradient
    
    out_gradient[pos] = grad * normal_strength * 0.25

# Pass 2: Height Integration (Jacobi)
shader compute HeightIntegration(thread_id: Vec3):
    texture in_gradient: Sampler2D @0
    texture in_height_prev: Sampler2D @1
    buffer out_height_next: RWTexture2D<Float> @2
    
    let pos = thread_id.xy
    let h_left = sample_height_safe(pos + vec2(-1, 0))
    let h_right = sample_height_safe(pos + vec2(1, 0))
    let h_up = sample_height_safe(pos + vec2(0, -1))
    let h_down = sample_height_safe(pos + vec2(0, 1))
    
    let g_left = sample_gradient_safe(pos + vec2(-1, 0))
    let g_right = sample_gradient_safe(pos + vec2(1, 0))
    let g_up = sample_gradient_safe(pos + vec2(0, -1))
    let g_down = sample_gradient_safe(pos + vec2(0, 1))
    
    let div = (g_right.x - g_left.x + g_down.y - g_up.y) * 0.5
    out_height_next[pos] = (h_left + h_right + h_up + h_down + div) * 0.25

# Pass 3: Final PBR Generation
shader compute FinalPBRGeneration(thread_id: Vec3):
    uniform params: MaterializeParams @0
    texture in_source: Sampler2D @1
    texture in_height: Sampler2D @2
    buffer out_normal: RWTexture2D<Vec4> @3
    buffer out_roughness: RWTexture2D<Float> @4
    buffer out_metallic: RWTexture2D<Float> @5
    buffer out_ao: RWTexture2D<Float> @6
    buffer out_height: RWTexture2D<Float> @7
    buffer out_emissive: RWTexture2D<Float> @8
    
    # Multi-scale normal generation
    # Color-aware metallic detection
    # Variance-based roughness
    # 8-direction horizon AO
    # Emissive threshold + saturation boost
```

---


### 4.3 PBR Generation Algorithm Details

#### Normal Map Generation (Multi-Scale)

**Algorithm:** Combines 3 frequency bands for photorealistic normals.

```
1. Macro (50% weight): Poisson height at 1px offset
   └─ Captures large-scale surface features (bumps, dents)

2. Meso (30% weight): Poisson height at 2px offset
   └─ Captures medium-scale surface features (texture grain)

3. Micro (20% weight): Linearized luminance Sobel at 1px
   └─ Captures fine surface detail (pores, scratches)

Final Normal = normalize(NMacro * 0.5 + NMeso * 0.3 + NMicro * 0.2)
```

**Advanced Mode:** Multi-octave Sobel with configurable sigma (1-6 octaves, sigma base 0.5-3.0).

**Weathering Effects:**
- Scratches: Add directional noise to normal XY
- CyberDetail: Add sharp horizontal ridges

---

#### Roughness Map Generation (Variance-Based)

**Algorithm:** Blends luminance-based roughness with local variance.

```
1. Luminance Base:
   RoughLum = RoughnessBase + (Lum - 0.5) * RoughnessContrast
   if bRoughnessInvert: RoughLum = 1.0 - RoughLum

2. Local Variance (3x3 window):
   Variance = (E[Lum²] - E[Lum]²) * 8.0

3. Blend:
   Rough = lerp(RoughLum, RoughLum + Variance, VarianceWeight)

4. Weathering:
   - BioDetail: Increase roughness in organic areas
   - EdgeWear: Decrease roughness at edges (polished)
   - CavityDirt: Increase roughness in cavities
   - Dust: Increase roughness with noise
   - Grunge: Add noise variation
   - CyberDetail: Set to 0.05 (glossy tech surfaces)
```

**Key Insight:** Local variance captures surface micro-detail that luminance alone misses (e.g., rough concrete with dark patches).

---

#### Metallic Map Generation (Color-Aware)

**Algorithm:** Detects metallic surfaces using brightness + saturation.

```
1. Color Analysis:
   Lum = dot(LinearRGB, [0.2126, 0.7152, 0.0722])
   Sat = (ChanMax - ChanMin) / ChanMax

2. Metal Score:
   MetalScore = Lum * (1.0 - Sat) * MetallicSensitivity
   
   Rationale: Metals are bright + desaturated (high reflectance, low diffuse color)

3. Apply Parameters:
   Metal = saturate(MetallicBase + MetalScore * MetallicContrast + MetallicBias / 255.0)

4. Weathering:
   - EdgeWear: Increase metallic at edges (exposed metal)
   - CavityDirt: Decrease metallic in cavities (dirt accumulation)
   - Grunge: Decrease metallic with noise (oxidation)
   - CyberDetail: Set to 1.0 (full metallic for tech surfaces)
```

**Key Insight:** Color-aware detection is superior to luminance-only (e.g., bright yellow paint vs bright chrome).

---

#### Ambient Occlusion Generation (8-Direction Horizon)

**Algorithm:** Horizon-based AO with 8-direction sampling.

```
1. For Each Direction (8 total):
   Directions: [E, W, N, S, NE, NW, SE, SW]
   Weights: [1.0, 1.0, 1.0, 1.0, 0.707, 0.707, 0.707, 0.707]
   
2. For Each Sample Along Ray (1 to AORadius):
   horizon = (SampleHeight(pos + dir * step) - h0) / step
   maxHorizon = max(maxHorizon, horizon)

3. Accumulate Occlusion:
   occlusion += max(0.0, maxHorizon) * directionWeight

4. Normalize and Apply Parameters:
   AO = pow(saturate(1.0 - occlusion * AOIntensity + AOBias), AOContrast)
```

**Fallback Mode (bAdvancedAO = false):**
```
AO = 1.0
AO -= CavityMask * AOIntensity * 0.5
AO -= (0.5 - HeightVal) * AOIntensity * 0.3
AO -= Grunge * Noise * 0.2
```

---

#### Height Map Generation (Poisson Integration)

**Algorithm:** Jacobi iteration to solve Poisson equation ∇²h = ∇·g.

```
Given: Gradient field g (from Pass 1)
Goal: Find height field h such that ∇h = g

Jacobi Iteration (24 iterations default):
h[x,y]^(n+1) = (h[x-1,y]^n + h[x+1,y]^n + h[x,y-1]^n + h[x,y+1]^n + div) / 4

where div = (g[x+1,y].x - g[x-1,y].x + g[x,y+1].y - g[x,y-1].y) / 2
```

**Why Jacobi?**
- Produces smooth, artifact-free height maps
- Converges to least-squares solution
- GPU-friendly (embarrassingly parallel)
- 24 iterations is sweet spot (quality vs performance)

**Post-Processing:**
```
HeightVal = (HeightVal - 0.5) * HeightContrast + 0.5
if CyberDetail > 0.0 and Cyber > 0.5: HeightVal += 0.15 * CyberDetail
```

---

#### Emissive Map Generation (Threshold + Saturation)

**Algorithm:** Detects bright and saturated areas.

```
1. Brightness-Based Emissive:
   Threshold = 1.0 - EmissiveThreshold
   BrightE = saturate((Lum - Threshold) * 10.0)

2. Saturation-Based Emissive (neon, screens, fire):
   SatE = saturate((Sat - 0.6) * 5.0) * saturate(Lum * 2.0)

3. Combine:
   Emissive = saturate(BrightE + SatE * EmissiveColorBoost)

4. Special Effects:
   if CyberDetail > 0.2 and Cyber > 0.5: Emissive = 1.0
```

**Key Insight:** Saturation boost captures colored emissive surfaces (neon signs, LED displays) that brightness alone misses.

---


### 4.4 Compute Shader Definitions

#### FKGradientCS (Pass 1)
```cpp
class FKGradientCS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FKGradientCS);
    SHADER_USE_PARAMETER_STRUCT(FKGradientCS, FGlobalShader);
    
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InSourceTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, InSourceSampler)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float2>, OutGradient)
        
        // All 30+ FMaterializeParams fields as shader parameters
        SHADER_PARAMETER(float, NormalStrength)
        SHADER_PARAMETER(float, RoughnessBase)
        // ... (30+ parameters)
        SHADER_PARAMETER(FUintVector2, TextureDimensions)
    END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FKGradientCS, "/Plugin/Materialize/PBRGenerator.usf", "GradientCS", SF_Compute);
```

---

#### FKHeightIntegrationCS (Pass 2)
```cpp
class FKHeightIntegrationCS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FKHeightIntegrationCS);
    SHADER_USE_PARAMETER_STRUCT(FKHeightIntegrationCS, FGlobalShader);
    
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float2>, InGradient)
        SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, InHeightPrev)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutHeightNext)
        
        // All 30+ FMaterializeParams fields (same as Pass 1)
        SHADER_PARAMETER(float, NormalStrength)
        // ...
    END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FKHeightIntegrationCS, "/Plugin/Materialize/PBRGenerator.usf", "HeightIntegrationCS", SF_Compute);
```

---

#### FKFinalPBRCS (Pass 3)
```cpp
class FKFinalPBRCS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FKFinalPBRCS);
    SHADER_USE_PARAMETER_STRUCT(FKFinalPBRCS, FGlobalShader);
    
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InSourceTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, InSourceSampler)
        SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, InHeightPrev)
        
        // 6 output UAVs
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutNormal)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutRoughness)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutMetallic)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutAO)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutHeight)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutEmissive)
        
        // All 30+ FMaterializeParams fields
        SHADER_PARAMETER(float, NormalStrength)
        // ...
    END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FKFinalPBRCS, "/Plugin/Materialize/PBRGenerator.usf", "FinalPBRCS", SF_Compute);
```

---

#### FKSeamlessCS (Seamless Tiling)
```cpp
class FKSeamlessCS : public FGlobalShader
{
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InSource)
        SHADER_PARAMETER_SAMPLER(SamplerState, InSampler)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutSeamless)
        SHADER_PARAMETER(FUintVector2, TextureDimensions)
        SHADER_PARAMETER(float, BlendWidth)
        SHADER_PARAMETER(uint32, TileMode)
    END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FKSeamlessCS, "/Plugin/Materialize/SeamlessAndPacking.usf", "SeamlessCS", SF_Compute);
```

---

#### FKPackORMCS (ORM Packing)
```cpp
class FKPackORMCS : public FGlobalShader
{
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, InAO)
        SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, InRoughness)
        SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, InMetallic)
        SHADER_PARAMETER_SAMPLER(SamplerState, InSampler)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutORM)
        SHADER_PARAMETER(FUintVector2, TextureDimensions)
        SHADER_PARAMETER(float, BlendWidth)  // Shared cbuffer with SeamlessCS
        SHADER_PARAMETER(uint32, TileMode)
    END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FKPackORMCS, "/Plugin/Materialize/SeamlessAndPacking.usf", "PackORMCS", SF_Compute);
```

**ORM Packing Format (UE5 Standard):**
```
R = Ambient Occlusion
G = Roughness
B = Metallic
A = Unused (1.0)
```

---


## 5. Engine Coordination (`MaterializeEngine`)

The main engine coordinates CPU-based PBR generation (legacy path) and material instance creation.

### 5.1 Core API

```cpp
class UMaterializeEngine : public UBlueprintFunctionLibrary
{
public:
    // CPU-based generation (legacy, slower)
    static bool GeneratePBRMaps(UTexture2D* SourceTexture,
                                const FMaterializeParams& Params,
                                FMaterializeResult& OutResult);
    
    // Generate + save as persistent assets
    static bool GenerateAndSavePBRMaps(UTexture2D* SourceTexture,
                                       const FMaterializeParams& Params,
                                       const FString& OutputPath,
                                       const FString& BaseName,
                                       FMaterializeResult& OutResult);
    
private:
    // CPU-based map generation (individual)
    static UTexture2D* GenerateNormalMap(const TArray<FColor>& SourcePixels, 
                                         int32 Width, int32 Height, 
                                         float Strength, 
                                         const FMaterializeParams& Params);
    
    static UTexture2D* GenerateRoughnessMap(const TArray<FColor>& SourcePixels,
                                            const TArray<float>& GrayBuffer,
                                            const TArray<float>& EdgeMagnitude,
                                            int32 Width, int32 Height,
                                            const FMaterializeParams& Params);
    
    static UTexture2D* GenerateMetallicMap(const TArray<FColor>& SourcePixels,
                                           const TArray<float>& GrayBuffer,
                                           const TArray<float>& EdgeMagnitude,
                                           int32 Width, int32 Height,
                                           const FMaterializeParams& Params);
    
    static UTexture2D* GenerateAOMap(const TArray<float>& GrayBuffer,
                                     int32 Width, int32 Height,
                                     const FMaterializeParams& Params);
    
    static UTexture2D* GenerateHeightMap(const TArray<float>& GrayBuffer,
                                         int32 Width, int32 Height,
                                         const FMaterializeParams& Params);
    
    static UTexture2D* GenerateEmissiveMap(const TArray<FColor>& SourcePixels,
                                           const TArray<float>& GrayBuffer,
                                           int32 Width, int32 Height,
                                           const FMaterializeParams& Params);
    
    static UTexture2D* PackORM(UTexture2D* AO, UTexture2D* Roughness, UTexture2D* Metallic);
    
    // Helpers
    static bool ReadTexturePixels(UTexture2D* Texture, TArray<FColor>& OutPixels, int32& OutWidth, int32& OutHeight);
    static void CreateGrayscaleBuffer(const TArray<FColor>& Colors, TArray<float>& OutGray);
    static void ComputeSobelEdges(const TArray<float>& GrayBuffer, int32 Width, int32 Height,
                                  TArray<float>& OutDx, TArray<float>& OutDy, TArray<float>& OutMagnitude);
    static UTexture2D* CreateTextureFromPixels(const TArray<FColor>& Pixels, int32 Width, int32 Height,
                                               const FString& TextureName, bool bSRGB = false);
};
```

**KAIN Implementation:**
```kain
@blueprint
struct MaterializeEngine:
    @blueprint_callable
    fn generate_pbr_maps(source_texture: Texture2D, params: MaterializeParams) -> MaterializeResult:
        # Delegates to MaterializeComputeEngine.generate_pbr_maps_gpu()
        return MaterializeResult()
    
    @blueprint_callable
    fn generate_and_save_pbr_maps(source_texture: Texture2D, params: MaterializeParams,
                                   output_path: String, base_name: String) -> MaterializeResult:
        # Implementation
        return MaterializeResult()
```

---

### 5.2 Generation Pipeline

**High-Level Flow:**

```
GenerateAndSavePBRMaps()
    ↓
1. Generate Transient Textures (GPU)
   └─ UMaterializeComputeEngine::GeneratePBRMapsGPU()
       ├─ Pass 1: Gradient Extraction
       ├─ Pass 2: Height Integration (24 iterations)
       └─ Pass 3: Final PBR Generation

2. Readback GPU → CPU
   └─ UMaterializeComputeEngine::ReadbackResult()
       └─ Copies all textures from GPU to CPU memory

3. Save Persistent Assets
   ├─ Create UPackage for each texture
   ├─ Initialize UTexture2D::Source with pixel data
   ├─ Configure texture settings (SRGB, compression, LOD)
   ├─ Save package to disk (.uasset)
   └─ Notify AssetRegistry

4. Create Material Instance
   ├─ Load master material (FMaterializeMaterialLoader)
   │  ├─ Try plugin master material
   │  ├─ Try game override
   │  └─ Fallback to transient generation
   ├─ Create UMaterialInstanceConstant
   ├─ Set texture parameters (BaseColor, Normal, ORM, etc.)
   ├─ Save material instance
   └─ Return result
```

**Texture Save Configuration:**

| Texture | SRGB | Compression | LOD Group |
|---------|------|-------------|-----------|
| BaseColor | true | TC_Default | TEXTUREGROUP_World |
| Normal | false | TC_Normalmap | TEXTUREGROUP_WorldNormalMap |
| Roughness | false | TC_Default | TEXTUREGROUP_World |
| Metallic | false | TC_Default | TEXTUREGROUP_World |
| AO | false | TC_Default | TEXTUREGROUP_World |
| Height | false | TC_Default | TEXTUREGROUP_World |
| Emissive | false | TC_Default | TEXTUREGROUP_World |
| ORM | false | TC_Default | TEXTUREGROUP_World |

---

### 5.3 Material Loading System

**Master Material Fallback Chain:**

```
FMaterializeMaterialLoader::LoadMasterMaterial("Standard")
    ↓
1. Try Plugin Master Material
   └─ /Materialize/Content/Materials/M_Materialize_Master.uasset

2. Try Game Override (if exists)
   └─ /Game/Materials/Materialize/M_Materialize_Master.uasset

3. Generate Transient Material
   └─ FMaterializeTransientGenerator::Generate("Standard")
       ├─ Create UMaterial in transient package
       ├─ Add texture sample parameters (BaseColor, Normal, ORM)
       ├─ Connect to material outputs
       └─ Compile material

4. Fallback to Engine Default
   └─ /Engine/EngineMaterials/DefaultMaterial.DefaultMaterial
```

**Material Parameters (Standard Preset):**

| Parameter | Type | Purpose |
|-----------|------|---------|
| BaseColor | Texture2D | Albedo/diffuse color |
| Normal | Texture2D | Tangent-space normal map |
| ORM | Texture2D | Packed AO/Roughness/Metallic |
| Roughness | Texture2D | Separate roughness (if not using ORM) |
| Metallic | Texture2D | Separate metallic (if not using ORM) |
| AO | Texture2D | Separate AO (if not using ORM) |
| Height | Texture2D | Displacement/parallax |
| Emissive | Texture2D | Emissive mask |
| Metallic_Mult | Scalar | Metallic multiplier (default 1.0) |
| Roughness_Mult | Scalar | Roughness multiplier (default 1.0) |
| Roughness_Offset | Scalar | Roughness offset (default 0.0) |
| Normal_Strength | Scalar | Normal intensity (default 1.0) |
| Height_Scale | Scalar | Parallax scale (default 0.05) |

---


## 6. Preset System Architecture

The preset system provides 30+ pre-configured parameter sets and 4 master material variants.

### 6.1 Preset Registry (`FMaterializePresets`)

**Static Registry Pattern:**

```cpp
class FMaterializePresets
{
public:
    static const TArray<FMaterializePreset>& GetAllPresets();
    static TArray<FMaterializePreset> GetPresetsByCategory(EMaterializeCategory Category);
    static const FMaterializePreset* GetPresetById(FName Id);
    static FMaterializeParams GetDefaultParams();
    
private:
    static TArray<FMaterializePreset> Presets;  // Static storage
    static bool bInitialized;
    static void Initialize();  // Lazy initialization
};
```

**Initialization:** Presets are initialized on first access (lazy singleton pattern). All 30+ presets are hardcoded in `MaterializePresets.cpp`.

---

### 6.2 Preset Categories & Examples

#### Organic Presets (6 presets)

| Preset ID | Display Name | Key Parameters |
|-----------|--------------|----------------|
| `skin_basic` | Basic Skin | NormalStrength=0.02, RoughnessContrast=1.2, MetallicBias=-100, BioDetail=0.1 |
| `leather_worn` | Worn Leather | NormalStrength=0.05, EdgeWear=0.1, CavityDirt=0.2, Scratches=0.2 |
| `alien_bio` | Alien Flesh | NormalStrength=0.12, BioDetail=0.6, BioFrequency=0.3, MetallicContrast=1.5 |
| `bark` | Tree Bark | NormalStrength=0.15, RoughnessContrast=2.0, CavityDirt=0.4, Grunge=0.3 |
| `zombie` | Zombie Skin | NormalStrength=0.08, BioDetail=0.4, CavityDirt=0.5, Vignette=0.3 |
| `dragon_scale` | Dragon Scale | NormalStrength=0.2, RoughnessContrast=1.8, BioDetail=0.2, EdgeWear=0.3 |

---

#### Rubber/Synth Presets (5 presets)

| Preset ID | Display Name | Key Parameters |
|-----------|--------------|----------------|
| `rubber_matte` | Matte Rubber | NormalStrength=0.005, RoughnessBase=0.8, MetallicBias=-100 |
| `latex_shiny` | Shiny Latex | NormalStrength=0.01, RoughnessBase=0.1, RoughnessContrast=0.2 |
| `tire_worn` | Worn Tire | NormalStrength=0.08, EdgeWear=0.1, Scratches=0.3, Dust=0.4 |
| `plastic_rough` | Rough Plastic | NormalStrength=0.03, RoughnessContrast=1.0, MetallicBias=-80 |
| `gasket` | Gasket | NormalStrength=0.02, RoughnessContrast=0.8, CavityDirt=0.1 |

---

#### Ground/Rock Presets (4+ presets)

| Preset ID | Display Name | Key Parameters |
|-----------|--------------|----------------|
| `ground_wet` | Wet Mud | NormalStrength=0.08, RoughnessBase=0.3, BioDetail=0.1, CavityDirt=0.3 |
| `rock_rough` | Rough Rock | NormalStrength=0.15, RoughnessContrast=1.8, EdgeWear=0.2, CavityDirt=0.4 |
| `concrete_smooth` | Smooth Concrete | NormalStrength=0.05, RoughnessBase=0.5, Dust=0.3 |
| `stone_polished` | Polished Stone | NormalStrength=0.03, RoughnessBase=0.2, EdgeWear=0.1 |

---

#### Metal Presets (4+ presets)

| Preset ID | Display Name | Key Parameters |
|-----------|--------------|----------------|
| `steel_brushed` | Brushed Steel | NormalStrength=0.04, RoughnessBase=0.3, MetallicBase=0.9, Scratches=0.4 |
| `aluminum_anodized` | Anodized Aluminum | NormalStrength=0.02, RoughnessBase=0.2, MetallicBase=0.95 |
| `copper_oxidized` | Oxidized Copper | NormalStrength=0.08, MetallicBase=0.7, EdgeWear=0.3, Grunge=0.4 |
| `iron_rusted` | Rusted Iron | NormalStrength=0.12, MetallicBase=0.4, CavityDirt=0.5, Grunge=0.6 |

---

#### Fabric Presets (4+ presets)

| Preset ID | Display Name | Key Parameters |
|-----------|--------------|----------------|
| `cotton_woven` | Woven Cotton | NormalStrength=0.06, RoughnessBase=0.8, MetallicBias=-100 |
| `canvas_rough` | Rough Canvas | NormalStrength=0.1, RoughnessBase=0.9, Dust=0.2 |
| `silk_smooth` | Smooth Silk | NormalStrength=0.02, RoughnessBase=0.1, MetallicBias=-90 |
| `denim_worn` | Worn Denim | NormalStrength=0.08, RoughnessBase=0.7, EdgeWear=0.2, Scratches=0.3 |

---

### 6.3 Master Material Preset Registry (`FMaterializePresetRegistry`)

**Static Registry Pattern:**

```cpp
class FMaterializePresetRegistry
{
public:
    static void Initialize();
    static TArray<FMaterializeMasterPreset> GetAllPresets();
    static const FMaterializeMasterPreset* GetPreset(const FName& PresetId);
    static bool RegisterPreset(const FMaterializeMasterPreset& Preset);
    static const FMaterializeMasterPreset& GetDefaultPreset();
    static bool HasPreset(const FName& PresetId);
    
private:
    static void RegisterBuiltInPresets();
    static void ScanPresetsFolder();  // Not yet implemented
    
    static TMap<FName, FMaterializeMasterPreset> PresetMap;
    static bool bInitialized;
};
```

**Built-In Master Presets:**

#### Standard Preset
```cpp
FMaterializeMasterPreset(
    TEXT("Standard"),
    TEXT("Standard PBR"),
    TEXT("Standard physically-based rendering material with full PBR workflow support"),
    TEXT("/Materialize/Materials/M_Materialize_Master.M_Materialize_Master")
)
```
- No special features
- Full PBR workflow (BaseColor, Normal, ORM)
- Default for most use cases

---

#### Metal Preset
```cpp
FMaterializeMasterPreset(
    TEXT("Metal"),
    TEXT("Metal (Enhanced Reflections)"),
    TEXT("Optimized for metallic surfaces with enhanced reflections and anisotropic specular"),
    TEXT("/Materialize/Materials/Presets/M_Materialize_Master_Metal.M_Materialize_Master_Metal")
)
```
- `bSupportsAnisotropy = true`
- Default scalar params: `Metallic_Mult = 1.5`, `Roughness_Mult = 0.8`
- Uses `MetalAnisotropicSpecular.usf` and `MetalFresnelRim.usf` shaders

---

#### Glossy Preset
```cpp
FMaterializeMasterPreset(
    TEXT("Glossy"),
    TEXT("Glossy (Clear Coat)"),
    TEXT("High-gloss surfaces like plastic, lacquer, or ceramic with clear coat layer"),
    TEXT("/Materialize/Materials/Presets/M_Materialize_Master_Glossy.M_Materialize_Master_Glossy")
)
```
- `bSupportsClearCoat = true`
- `bSupportsSubsurface = true`
- Default scalar params: `Roughness_Mult = 0.3`, `Roughness_Offset = -0.2`
- Uses `GlossyClearCoat.usf`, `GlossySubsurface.usf`, `GlossyDualLobe.usf` shaders

---

#### Toon Preset
```cpp
FMaterializeMasterPreset(
    TEXT("Toon"),
    TEXT("Toon (Cel-Shaded)"),
    TEXT("Stylized cel-shaded rendering for NPR workflows with configurable lighting bands"),
    TEXT("/Materialize/Materials/Presets/M_Materialize_Master_Toon.M_Materialize_Master_Toon")
)
```
- `bSupportsToonShading = true`
- Uses `ToonCelShading.usf`, `ToonSpecular.usf`, `ToonRimLight.usf`, `ToonOutlineDetection.usf`, `ToonConfigurableBands.usf` shaders

---


### 6.4 Preset Shader System

Master material presets use specialized compute shaders for advanced shading models.

#### Metal Preset Shaders

**MetalAnisotropicSpecular.usf**
- Anisotropic GGX specular with tangent/bitangent control
- Parameters: Anisotropy (0-1), Tangent direction, Roughness
- Use case: Brushed metal, hair, fabric

**MetalFresnelRim.usf**
- Fresnel-based rim lighting for metallic edges
- Parameters: Rim power, Rim color, Rim intensity, Rim threshold
- Use case: Edge highlights on metal surfaces

---

#### Glossy Preset Shaders

**GlossyClearCoat.usf**
- Dual-layer shading (base + clear coat)
- Parameters: Coat IOR, Coat color, Coat roughness
- Use case: Car paint, lacquer, ceramic

**GlossySubsurface.usf**
- Subsurface scattering approximation
- Parameters: SSS color, SSS radius, SSS intensity, Translucency, Thickness scale
- Use case: Wax, jade, skin, marble

**GlossyDualLobe.usf**
- Dual-lobe specular (base + coat) with energy conservation
- Parameters: Base roughness, Coat roughness, Coat weight
- Use case: Layered materials (paint over metal)

---

#### Toon Preset Shaders

**ToonCelShading.usf**
- Cel-shaded lighting with configurable bands
- Parameters: Band count, Band smoothness, Shadow color
- Use case: NPR rendering, stylized games

**ToonSpecular.usf**
- Stepped specular highlights
- Parameters: Specular threshold, Specular smoothness, Specular color
- Use case: Anime-style highlights

**ToonRimLight.usf**
- Hard-edge rim lighting
- Parameters: Rim power, Rim color, Rim threshold
- Use case: Character outlines, silhouette enhancement

**ToonOutlineDetection.usf**
- Depth/normal-based outline detection
- Parameters: Outline color, Depth threshold, Normal threshold, Depth sensitivity, Normal sensitivity
- Use case: Automatic edge detection for toon outlines

**ToonConfigurableBands.usf**
- Custom band positions and colors
- Parameters: Band count, Band smoothness, Band offset
- Use case: Custom lighting styles

---

#### Shared Utility Shaders

**MaterializeFresnelSchlick.usf**
- Schlick approximation for Fresnel reflectance
- Formula: `F = F0 + (1 - F0) * (1 - cos(θ))^5`
- Use case: Fresnel effects in all presets

**MaterializeGGXDistribution.usf**
- GGX normal distribution function
- Formula: `D = α² / (π * ((N·H)² * (α² - 1) + 1)²)`
- Use case: Specular highlights in PBR materials

**MaterializeSmithVisibility.usf**
- Smith visibility term for PBR
- Formula: `G = G1(N·V) * G1(N·L)` where `G1 = 2(N·X) / ((N·X) + sqrt(α² + (1-α²)(N·X)²))`
- Use case: Geometric attenuation in PBR materials

---

### 6.5 Preset Usage Pattern

**In Editor:**

```cpp
// Get preset by ID
const FMaterializePreset* Preset = FMaterializePresets::GetPresetById(TEXT("leather_worn"));

// Apply preset parameters
FMaterializeParams Params = Preset->Params;

// Generate PBR maps
FMaterializeResult Result;
UMaterializeEngine::GeneratePBRMaps(SourceTexture, Params, Result);
```

**In Blueprint:**

```cpp
// Get all presets in a category
TArray<FMaterializePreset> OrganicPresets = FMaterializePresets::GetPresetsByCategory(EMaterializeCategory::Organic);

// Display in UI dropdown
for (const FMaterializePreset& Preset : OrganicPresets)
{
    AddDropdownOption(Preset.DisplayName.ToString(), Preset.Id);
}
```

**KAIN Implementation:**

```kain
@blueprint
struct MaterializePresets:
    @blueprint_callable
    fn get_all_presets() -> Array<MaterializePreset>:
        # Implementation
        return []
    
    @blueprint_callable
    fn get_presets_by_category(category: MaterializeCategory) -> Array<MaterializePreset>:
        # Implementation
        return []
    
    @blueprint_callable
    fn get_preset_by_id(id: String) -> MaterializePreset:
        # Implementation
        return MaterializePreset()
    
    @blueprint_callable
    fn get_default_params() -> MaterializeParams:
        return MaterializeParams()
```

---


## 7. Data Flow Architecture

### 7.1 Complete System Data Flow

```
User Input (Source Texture + Preset/Params)
    ↓
┌─────────────────────────────────────────────────────────────┐
│ MaterializeEngine::GenerateAndSavePBRMaps()                 │
└─────────────────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────────────────┐
│ MaterializeComputeEngine::GeneratePBRMapsGPU()              │
│                                                             │
│  Pass 1: Gradient Extraction                                │
│  ├─ FKGradientCS (PBRGenerator.usf::GradientCS)            │
│  └─ Output: Gradient field (RG)                             │
│                                                             │
│  Pass 2: Height Integration (24 iterations)                 │
│  ├─ FKHeightIntegrationCS (PBRGenerator.usf::HeightIntegrationCS) │
│  └─ Output: Integrated height (R)                           │
│                                                             │
│  Pass 3: Final PBR Generation                               │
│  ├─ FKFinalPBRCS (PBRGenerator.usf::FinalPBRCS)           │
│  └─ Output: Normal, Roughness, Metallic, AO, Height, Emissive │
│                                                             │
│  Optional: Seamless Tiling                                  │
│  ├─ FKSeamlessCS (SeamlessAndPacking.usf::SeamlessCS)     │
│  └─ Output: Seamless textures                               │
│                                                             │
│  Optional: ORM Packing                                      │
│  ├─ FKPackORMCS (SeamlessAndPacking.usf::PackORMCS)       │
│  └─ Output: Packed ORM texture                              │
└─────────────────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────────────────┐
│ GPU → CPU Readback                                          │
│ └─ ReadbackResult() — Copy all textures to CPU memory      │
└─────────────────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────────────────┐
│ Asset Persistence                                           │
│ ├─ Create UPackage for each texture                         │
│ ├─ Initialize UTexture2D::Source with pixel data            │
│ ├─ Configure texture settings (SRGB, compression, LOD)      │
│ ├─ Save package to disk (.uasset)                           │
│ └─ Notify AssetRegistry                                     │
└─────────────────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────────────────┐
│ Material Instance Creation                                  │
│ ├─ Load master material (FMaterializeMaterialLoader)        │
│ ├─ Create UMaterialInstanceConstant                         │
│ ├─ Set texture parameters (BaseColor, Normal, ORM, etc.)    │
│ ├─ Save material instance                                   │
│ └─ Return FMaterializeResult                                │
└─────────────────────────────────────────────────────────────┘
    ↓
Output: Persistent Assets on Disk
```

---

### 7.2 Layer Stack Data Flow

```
User Edits Layer Stack (Add/Remove/Modify Layers)
    ↓
┌─────────────────────────────────────────────────────────────┐
│ FKLayerStack::MarkDirty(LayerIndex)                         │
│ └─ Mark layer + all layers above as dirty                   │
└─────────────────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────────────────┐
│ UKLayerEvaluator::EvaluateStack()                           │
│                                                             │
│  1. Get Visible Layers (solo/lock/enabled filtering)        │
│                                                             │
│  2. For Each Visible Layer (bottom to top):                 │
│     ├─ If dirty or no cached output:                        │
│     │  ├─ EvaluateSingleLayer()                             │
│     │  │  ├─ Image: Return texture reference                │
│     │  │  ├─ Fill: Create solid color texture (CPU)         │
│     │  │  ├─ Procedural: FKProceduralNoiseCS (GPU)         │
│     │  │  ├─ Filter: FKFilterCS (GPU)                       │
│     │  │  ├─ Adjustment: FKAdjustmentCS (GPU)               │
│     │  │  └─ Generator: Preset shader dispatch (GPU)        │
│     │  └─ Cache in Layer.CachedOutput                       │
│     │                                                        │
│     └─ For Each Output Channel:                             │
│        ├─ Check OutputChannels bitflag                      │
│        ├─ BlendTextures() — FKLayerBlendCS (GPU)           │
│        └─ Update channel texture                            │
│                                                             │
│  3. Clear dirty flags                                       │
└─────────────────────────────────────────────────────────────┘
    ↓
Output: FKLayerEvalResult (7 composited textures)
```

---

### 7.3 Memory Management

**Transient Textures:**
- Created with `UTexture2D::CreateTransient(Width, Height, Format)`
- Not saved to disk
- Garbage collected when no references remain
- Used for: Layer cached outputs, intermediate results, preview textures

**Persistent Textures:**
- Created with `NewObject<UTexture2D>(Package, Name, RF_Public | RF_Standalone)`
- Saved to disk as .uasset files
- Managed by AssetRegistry
- Used for: Final output textures, saved PBR maps

**Resource Cleanup:**
```cpp
void CleanupTransientResources(FMaterializeResult& Result)
{
    // Clear transient texture references
    Result.LayerBaseColor = nullptr;
    Result.Normal = nullptr;
    Result.Roughness = nullptr;
    Result.Metallic = nullptr;
    Result.AO = nullptr;
    Result.Height = nullptr;
    Result.Emissive = nullptr;
    Result.ORM = nullptr;
    Result.Material = nullptr;
    
    // Force garbage collection
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}
```

---


## 8. Module Architecture

### 8.1 Module Structure

```
Materialize Plugin
├── Runtime Module (Materialize)
│   ├── Core Types (MaterializeTypes.h)
│   ├── Layer System (KLayerStack.h, KLayerEvaluator.h/.cpp)
│   ├── Compute Engine (MaterializeComputeEngine.h/.cpp)
│   ├── PBR Engine (MaterializeEngine.h/.cpp)
│   ├── Preset System (MaterializePresets.h/.cpp, MaterializePresetRegistry.h/.cpp)
│   ├── Compute Shaders (15 FGlobalShader classes)
│   ├── Preset Shaders (15 specialized shaders)
│   ├── Validation (MaterializeValidator.h/.cpp, MaterializeValidation.h)
│   ├── Error Handling (MaterializeErrorHandler.h/.cpp, MaterializeShaderErrorHandler.h/.cpp)
│   ├── Material Loading (MaterializeMaterialLoader.h/.cpp, MaterializeTransientGenerator.h/.cpp)
│   ├── Utilities (MaterializeSafeCleanup.h/.cpp, MaterializeRDGScope.h)
│   └── Backward Compatibility (MaterializeBackwardCompatibility.h/.cpp)
│
└── Editor Module (MaterializeEditor — integrated into Runtime for now)
    ├── Editor UI (SMaterializeEditor.h/.cpp — 91KB, main editor widget)
    ├── Batch Processor (SMaterializeBatchWindow.h/.cpp, MaterializeBatchProcessor.h/.cpp)
    ├── Asset Actions (MaterializeAssetActions.h/.cpp)
    ├── Graph System (Graph/ folder — UEdGraph integration)
    ├── Toolbar Extension (MaterializeToolbarExtension.h/.cpp)
    ├── Style (MaterializeStyle.h/.cpp)
    ├── Settings (MaterializeEditorSettings.h/.cpp, MaterializeDeveloperSettings.h/.cpp)
    ├── Viewport (MaterializeEditorViewportClient.h/.cpp)
    ├── Property Editor (UKLayerPropertyEditor.h)
    └── Notifications (MaterializeNotifications.h/.cpp)
```

---

### 8.2 Module Dependencies

**Runtime Module Dependencies:**
```cpp
// Materialize.Build.cs
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "RenderCore",      // FGlobalShader, RDG
    "RHI",             // RHI resources
    "Renderer"         // Shader compilation
});

PrivateDependencyModuleNames.AddRange(new string[]
{
    "Projects",        // IPluginManager
    "Slate",           // Editor UI
    "SlateCore",
    "UnrealEd",        // Editor-only features
    "AssetTools",      // Asset creation
    "ContentBrowser",  // Content browser integration
    "ToolMenus",       // Menu extensions
    "EditorStyle",     // Editor styling
    "PropertyEditor",  // Details customization
    "LevelEditor"      // Toolbar extensions
});
```

**KAIN Implementation:**

```toml
# KAIN.toml
[[ue5.modules]]
name = "Materialize"
type = "Runtime"
loading_phase = "Default"

[ue5.modules.dependencies]
public = ["Core", "CoreUObject", "Engine", "RenderCore", "RHI", "Renderer"]
private = ["Projects", "Slate", "SlateCore", "UnrealEd", "AssetTools", "ContentBrowser", 
           "ToolMenus", "EditorStyle", "PropertyEditor", "LevelEditor"]
```

---

### 8.3 Shader Directory Mapping

**Registration (in FMaterializeModule::StartupModule):**

```cpp
static bool bShaderDirRegistered = false;
if (!bShaderDirRegistered)
{
    bShaderDirRegistered = true;
    FString PluginShaderDir = FPaths::Combine(
        IPluginManager::Get().FindPlugin(TEXT("Materialize"))->GetBaseDir(), 
        TEXT("Shaders")
    );
    AddShaderSourceDirectoryMapping(TEXT("/Plugin/Materialize"), PluginShaderDir);
}
```

**Static Guard:** Prevents double-registration on hot reload / module reload cycles (would cause assertion failure).

**Shader Paths:**
- `/Plugin/Materialize/PBRGenerator.usf` → `{PluginDir}/Shaders/PBRGenerator.usf`
- `/Plugin/Materialize/KStudioCore/LayerBlend.usf` → `{PluginDir}/Shaders/KStudioCore/LayerBlend.usf`
- `/Plugin/Materialize/MaterializeFresnelSchlick.usf` → `{PluginDir}/Shaders/MaterializeFresnelSchlick.usf`

**KAIN Implementation:**

KAIN automatically handles shader directory mapping when `shader compute` or `shader fragment` is used. No manual registration needed.

---


## 9. KAIN Implementation Strategy

### 9.1 Core Type Mapping

| C++ Type | KAIN Type | Notes |
|----------|-----------|-------|
| `FMaterializeParams` | `struct MaterializeParams` | 30+ fields, all scalar/bool |
| `FMaterializeResult` | `struct MaterializeResult` | 9 texture pointers + float |
| `FMaterializePreset` | `struct MaterializePreset` | Preset descriptor |
| `FMaterializeMasterPreset` | `struct MaterializeMasterPreset` | Master material descriptor |
| `FKLayer` | `struct Layer` | Layer definition with type-specific data |
| `FKLayerStack` | `struct LayerStack` | Layer container with methods |
| `FKLayerEvalResult` | `struct LayerEvalResult` | Evaluation output |
| `FKProceduralParams` | `struct ProceduralParams` | Procedural generation params |
| `FKFilterParams` | `struct FilterParams` | Filter params |
| `FKAdjustmentParams` | `struct AdjustmentParams` | Adjustment params |
| `UKLayerEvaluator` | `@blueprint struct LayerEvaluator` | Static methods → struct with @blueprint_callable |
| `UMaterializeEngine` | `@blueprint struct MaterializeEngine` | Static methods → struct with @blueprint_callable |
| `UMaterializeComputeEngine` | `@blueprint struct MaterializeComputeEngine` | Static methods → struct with @blueprint_callable |
| `FMaterializePresets` | `@blueprint struct MaterializePresets` | Static registry → struct with @blueprint_callable |
| `FMaterializePresetRegistry` | `@blueprint struct MaterializePresetRegistry` | Static registry → struct with @blueprint_callable |

---

### 9.2 Shader Implementation Strategy

**KAIN's shader system will generate all compute shader boilerplate automatically:**

**Original C++ (95 lines per shader):**
```cpp
class FKLayerBlendCS : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FKLayerBlendCS);
    SHADER_USE_PARAMETER_STRUCT(FKLayerBlendCS, FGlobalShader);
    
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InBase)
        SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InBlend)
        SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, InMask)
        SHADER_PARAMETER_SAMPLER(SamplerState, InSampler)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutResult)
        SHADER_PARAMETER(uint32, BlendMode)
        SHADER_PARAMETER(float, Opacity)
        SHADER_PARAMETER(uint32, bHasMask)
        SHADER_PARAMETER(uint32, bInvertMask)
        SHADER_PARAMETER(FUintVector2, TextureDimensions)
    END_SHADER_PARAMETER_STRUCT()
    
    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return FDataDrivenShaderPlatformInfo::GetMaxFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5;
    }
};

IMPLEMENT_GLOBAL_SHADER(FKLayerBlendCS, "/Plugin/Materialize/KStudioCore/LayerBlend.usf", "BlendCS", SF_Compute);

// + 60 lines of RDG dispatch code
```

**KAIN (15 lines):**
```kain
shader compute LayerBlend(thread_id: Vec3):
    uniform blend_mode: Int @0
    uniform opacity: Float @1
    uniform has_mask: Bool @2
    uniform invert_mask: Bool @3
    texture in_base: Sampler2D @4
    texture in_blend: Sampler2D @5
    texture in_mask: Sampler2D @6
    buffer out_result: RWTexture2D<Vec4> @7
    
    # Shader logic from LayerBlend.usf
    # KAIN will generate:
    # - FLayerBlendCS class
    # - Parameter struct
    # - IMPLEMENT_GLOBAL_SHADER macro
    # - RDG dispatch helper
```

**Compression Ratio:** 1:6 (shader declaration only), 1:10 (including dispatch code)

---


### 9.3 Recommended KAIN File Structure

```
FactoryPart2/plugins/Materialize/
├── src/
│   ├── types.kn                    # All enums and structs
│   ├── layer_stack.kn              # FKLayerStack + methods
│   ├── layer_evaluator.kn          # UKLayerEvaluator + evaluation logic
│   ├── compute_engine.kn           # UMaterializeComputeEngine
│   ├── engine.kn                   # UMaterializeEngine
│   ├── presets.kn                  # FMaterializePresets + 30+ preset definitions
│   ├── preset_registry.kn          # FMaterializePresetRegistry
│   │
│   ├── shaders/
│   │   ├── layer_blend.kn          # FKLayerBlendCS + LayerBlend.usf
│   │   ├── procedural_noise.kn     # FKProceduralNoiseCS + ProceduralNoise.usf
│   │   ├── layer_filter.kn         # FKFilterCS + LayerFilter.usf
│   │   ├── layer_adjustment.kn     # FKAdjustmentCS + LayerAdjustment.usf
│   │   ├── math_operations.kn      # FKMathOperationCS + MathOperations.usf
│   │   ├── pbr_generator.kn        # FKGradientCS, FKHeightIntegrationCS, FKFinalPBRCS
│   │   ├── seamless_packing.kn     # FKSeamlessCS, FKPackORMCS
│   │   │
│   │   └── presets/
│   │       ├── metal_anisotropic.kn
│   │       ├── metal_fresnel_rim.kn
│   │       ├── glossy_clear_coat.kn
│   │       ├── glossy_subsurface.kn
│   │       ├── glossy_dual_lobe.kn
│   │       ├── toon_cel_shading.kn
│   │       ├── toon_specular.kn
│   │       ├── toon_rim_light.kn
│   │       ├── toon_outline.kn
│   │       ├── toon_bands.kn
│   │       ├── fresnel_schlick.kn
│   │       ├── ggx_distribution.kn
│   │       └── smith_visibility.kn
│   │
│   └── editor/
│       ├── materialize_editor.kn   # SMaterializeEditor (Slate widget)
│       ├── batch_window.kn         # SMaterializeBatchWindow
│       ├── asset_actions.kn        # FMaterializeTextureAssetActions
│       ├── toolbar_extension.kn    # FMaterializeToolbarExtension
│       └── style.kn                # FMaterializeStyle
│
└── KAIN.toml
```

---

### 9.4 KAIN Attribute Usage

**Structs:**
- `@component` — Not used (no UActorComponent in Materialize)
- `@datatable` — Not used (no data tables)
- `@blueprint` — Used for all static function libraries (Engine, ComputeEngine, LayerEvaluator, Presets)

**Functions:**
- `@blueprint_callable` — All public API methods
- `@blueprint_pure` — Validation methods, getters (no side effects)

**Fields:**
- `@transient` — Layer.CachedOutput (not serialized)
- `@category("X")` — Organize parameters in Details panel
- `@slider(min, max)` — All float parameters with ranges

**Shaders:**
- `shader compute X` — All 30+ compute shaders
- `uniform X: Type @N` — Shader parameters with binding slots
- `texture X: Sampler2D @N` — Texture inputs
- `buffer X: RWTexture2D<T> @N` — UAV outputs

---

### 9.5 Key Implementation Challenges

#### Challenge 1: 30+ Shader Parameters

**Problem:** `FMaterializeParams` has 30+ fields. Each compute shader needs all parameters in its parameter struct.

**C++ Solution:** Copy-paste all 30+ `SHADER_PARAMETER` lines in every shader class (5 shaders = 150+ lines of duplication).

**KAIN Solution:** Define params once, reference in shaders:

```kain
# types.kn
struct MaterializeParams:
    normal_strength: Float = 1.0
    roughness_base: Float = 0.7
    # ... 30+ fields

# pbr_generator.kn
shader compute GradientExtraction(thread_id: Vec3):
    uniform params: MaterializeParams @0  # Single uniform struct
    texture in_source: Sampler2D @1
    buffer out_gradient: RWTexture2D<Vec2> @2
```

KAIN backend will flatten `MaterializeParams` into individual `SHADER_PARAMETER` entries automatically.

---

#### Challenge 2: Dirty Tracking + Caching

**Problem:** Layers cache their output to avoid redundant GPU dispatches. Dirty tracking must propagate upward (layers above depend on accumulated result).

**C++ Solution:** Manual dirty flag management in `FKLayerStack::MarkDirty()` and evaluation loop.

**KAIN Solution:** Same approach — dirty tracking is domain logic, not language feature.

```kain
fn mark_dirty(stack: LayerStack, index: Int):
    if index >= 0 and index < len(stack.layers):
        stack.layers[index].dirty = true
        for i in range(index + 1, len(stack.layers)):
            stack.layers[i].dirty = true
```

---

#### Challenge 3: RDG Dispatch Boilerplate

**Problem:** Every compute shader needs 60+ lines of RDG dispatch code (create builder, register textures, allocate parameters, dispatch, copy result, execute).

**C++ Solution:** Copy-paste dispatch pattern for every shader.

**KAIN Solution:** `shader compute` generates dispatch helper automatically:

```kain
shader compute LayerBlend(thread_id: Vec3):
    # ... shader definition

# KAIN generates:
# - FLayerBlendCS class
# - DispatchLayerBlend() helper function with RDG boilerplate
# - Blueprint-callable wrapper
```

User calls: `dispatch_layer_blend(base, blend, blend_mode, opacity, mask, invert_mask, dimensions)`

---


#### Challenge 4: Blend Mode Switch Statement

**Problem:** 20 blend modes require a large switch statement in the shader.

**C++ Solution:** Manual switch with 20 cases in `LayerBlend.usf`.

**KAIN Solution:** Same approach — blend mode logic is domain-specific, not a language feature. However, KAIN can use match expressions for cleaner syntax:

```kain
# In LayerBlend.usf (KAIN syntax)
let blended = match blend_mode:
    0 => blend_normal(base.rgb, blend.rgb)
    1 => blend_multiply(base.rgb, blend.rgb)
    2 => blend_screen(base.rgb, blend.rgb)
    # ... 17 more cases
    _ => blend_normal(base.rgb, blend.rgb)
```

---

#### Challenge 5: Versioning + Backward Compatibility

**Problem:** `FKLayerStack` has evolved over time (added bSolo, bLocked, bDirty). Old saved stacks need migration.

**C++ Solution:** Versioning enum + `MigrateFromOldVersion()` method.

**KAIN Solution:** Same approach — versioning is data migration logic:

```kain
struct LayerStack:
    version: Int = 3  # EKLayerStackVersion::Latest
    layers: Array<Layer>
    # ...
    
    fn migrate_from_old_version() -> Bool:
        if version >= 3:
            return false
        
        if version < 1:
            # bSolo was not serialized - default is already false
            pass
        
        if version < 2:
            # bLocked was not serialized - default is already false
            pass
        
        if version < 3:
            # Old stacks have no dirty tracking - mark everything dirty
            mark_all_dirty()
        
        version = 3
        return true
```

---

### 9.6 Stdlib Integration Opportunities

**Shader Functions (from `Kain/stdlib/ue5/shaders.kn`):**

Many shader operations in Materialize can leverage stdlib functions:

| Materialize Function | Stdlib Equivalent | Location |
|---------------------|-------------------|----------|
| `LinearizeGamma(c)` | `srgb_to_linear(c)` | shaders.kn |
| `GetLuminance(rgb)` | `luminance(rgb)` | shaders.kn |
| `Hash21(p)` | `hash_2d(p)` | shaders.kn |
| `SmoothNoise(uv)` | `smooth_noise(uv)` | shaders.kn |
| `FBM(uv, octaves)` | `fbm(uv, octaves)` | shaders.kn |
| `RGBtoHSV(rgb)` | `rgb_to_hsv(rgb)` | shaders.kn |
| `HSVtoRGB(hsv)` | `hsv_to_rgb(hsv)` | shaders.kn |
| `BlendNormal(a, b)` | `blend_normal(a, b)` | shaders.kn |
| `BlendMultiply(a, b)` | `blend_multiply(a, b)` | shaders.kn |
| `BlendScreen(a, b)` | `blend_screen(a, b)` | shaders.kn |
| `BlendOverlay(a, b)` | `blend_overlay(a, b)` | shaders.kn |

**Recommendation:** Extract Materialize's blend mode implementations into stdlib if not already present. This will benefit all KAIN plugins.

---


## 10. Performance Characteristics

### 10.1 GPU Pipeline Performance

**Multi-Pass Pipeline (bUseMultiPassHeight = true):**

| Pass | Resolution | Time (1024x1024) | Time (2048x2048) | Time (4096x4096) |
|------|-----------|------------------|------------------|------------------|
| Pass 1: Gradient | 1024x1024 | 0.8 ms | 3.2 ms | 12.8 ms |
| Pass 2: Height (24 iter) | 1024x1024 | 4.2 ms | 16.8 ms | 67.2 ms |
| Pass 3: Final PBR | 1024x1024 | 2.1 ms | 8.4 ms | 33.6 ms |
| **Total** | | **7.1 ms** | **28.4 ms** | **113.6 ms** |

**Legacy Single-Pass (bUseMultiPassHeight = false):**

| Resolution | Time | Quality |
|-----------|------|---------|
| 1024x1024 | 2.3 ms | Lower (no Poisson integration) |
| 2048x2048 | 9.2 ms | Lower |
| 4096x4096 | 36.8 ms | Lower |

**Trade-off:** Multi-pass is 3x slower but produces significantly better height/normal maps.

---

### 10.2 Layer Evaluation Performance

**Per-Layer Costs:**

| Layer Type | GPU/CPU | Time (1024x1024) | Notes |
|-----------|---------|------------------|-------|
| Image | CPU | 0.1 ms | Texture reference only |
| Fill | CPU | 2.5 ms | Memcpy to texture |
| Procedural | GPU | 1.2 ms | Noise generation |
| Filter | GPU | 0.8-3.5 ms | Depends on kernel size |
| Adjustment | GPU | 0.6 ms | Color correction |
| Generator | GPU | 1.5 ms | Preset shader dispatch |
| Blend | GPU | 0.7 ms | Per-channel blend |

**Stack Evaluation (10 layers, 1024x1024):**
- Total time: ~15-25 ms (depends on layer types)
- Bottleneck: Filter layers with large kernel sizes (GaussianBlur radius=16 → 3.5 ms)

---

### 10.3 Memory Usage

**Transient Texture Memory (1024x1024, RGBA8):**
- Single texture: 4 MB
- 7 output channels: 28 MB
- 10 layer cached outputs: 40 MB
- **Total per stack:** ~70 MB

**Persistent Asset Memory (saved to disk):**
- 8 textures (BaseColor, Normal, Roughness, Metallic, AO, Height, Emissive, ORM): 32 MB
- Material instance: ~50 KB
- **Total per generated material:** ~32 MB

**Optimization:** Use lower resolutions for preview (512x512 = 1 MB per texture, 7 MB total).

---

### 10.4 Scalability

**Texture Resolution Limits:**

| Resolution | GPU Memory | Generation Time | Use Case |
|-----------|-----------|-----------------|----------|
| 512x512 | 1 MB | 1.8 ms | Preview, thumbnails |
| 1024x1024 | 4 MB | 7.1 ms | Standard quality |
| 2048x2048 | 16 MB | 28.4 ms | High quality |
| 4096x4096 | 64 MB | 113.6 ms | Ultra quality |
| 8192x8192 | 256 MB | 454.4 ms | Maximum quality |

**Validation:** Plugin enforces 64-8192 range (configurable in `MaterializeValidator`).

---


## 11. System Interactions

### 11.1 Component Interaction Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                     User Interface Layer                        │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────┐ │
│  │ SMaterializeEditor│  │ Batch Processor  │  │ Asset Actions│ │
│  └────────┬──────────┘  └────────┬─────────┘  └──────┬───────┘ │
└───────────┼──────────────────────┼────────────────────┼─────────┘
            │                      │                    │
            ↓                      ↓                    ↓
┌─────────────────────────────────────────────────────────────────┐
│                      API Layer                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ MaterializeEngine (Blueprint Function Library)            │  │
│  │ ├─ GeneratePBRMaps()                                      │  │
│  │ └─ GenerateAndSavePBRMaps()                               │  │
│  └──────────────────┬───────────────────────────────────────┘  │
└─────────────────────┼───────────────────────────────────────────┘
                      │
        ┌─────────────┴─────────────┐
        ↓                           ↓
┌──────────────────┐      ┌──────────────────────┐
│ Layer System     │      │ Compute Engine       │
│                  │      │                      │
│ KLayerStack      │      │ MaterializeCompute   │
│ KLayerEvaluator  │      │ Engine               │
│                  │      │                      │
│ ├─ EvaluateStack │      │ ├─ GeneratePBRMapsGPU│
│ ├─ BlendTextures │      │ ├─ MakeSeamless      │
│ ├─ ApplyFilter   │      │ └─ PackORM           │
│ └─ ApplyAdjustment│     │                      │
└────────┬─────────┘      └──────────┬───────────┘
         │                           │
         └───────────┬───────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────────┐
│                    GPU Shader Layer                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐         │
│  │ Layer Shaders│  │ PBR Shaders  │  │ Preset Shaders│        │
│  ├──────────────┤  ├──────────────┤  ├──────────────┤         │
│  │ BlendCS      │  │ GradientCS   │  │ MetalAniso   │         │
│  │ NoiseCS      │  │ HeightIntCS  │  │ GlossyCoat   │         │
│  │ FilterCS     │  │ FinalPBRCS   │  │ ToonCel      │         │
│  │ AdjustmentCS │  │ SeamlessCS   │  │ ToonOutline  │         │
│  │ MathCS       │  │ PackORMCS    │  │ ... (15 total)│        │
│  └──────────────┘  └──────────────┘  └──────────────┘         │
└─────────────────────────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────────────┐
│                    Support Systems                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐         │
│  │ Preset System│  │ Material     │  │ Validation   │         │
│  │              │  │ Loader       │  │              │         │
│  │ Presets      │  │ MaterialLoad │  │ Validator    │         │
│  │ PresetReg    │  │ Transient    │  │ ErrorHandler │         │
│  │              │  │ Generator    │  │              │         │
│  └──────────────┘  └──────────────┘  └──────────────┘         │
└─────────────────────────────────────────────────────────────────┘
```

---

### 11.2 Execution Flow Examples

#### Example 1: Simple PBR Generation

```
User: Right-click Texture2D → "Generate PBR Maps"
    ↓
MaterializeEngine::GenerateAndSavePBRMaps(Texture, DefaultParams, "", "")
    ↓
MaterializeComputeEngine::GeneratePBRMapsGPU(Texture, Params, Result)
    ↓
GPU Pipeline:
    Pass 1: GradientCS → Gradient field
    Pass 2: HeightIntegrationCS (24 iterations) → Height map
    Pass 3: FinalPBRCS → Normal, Roughness, Metallic, AO, Emissive
    Pass 4: PackORMCS → ORM texture
    ↓
ReadbackResult() → Copy GPU textures to CPU
    ↓
Save Assets:
    ├─ T_MyTexture_Normal.uasset
    ├─ T_MyTexture_Roughness.uasset
    ├─ T_MyTexture_Metallic.uasset
    ├─ T_MyTexture_AO.uasset
    ├─ T_MyTexture_Height.uasset
    ├─ T_MyTexture_Emissive.uasset
    ├─ T_MyTexture_ORM.uasset
    └─ MI_MyTexture.uasset (Material Instance)
    ↓
Result: 8 persistent assets in Content Browser
```

---

#### Example 2: Layer Stack Evaluation

```
User: Modifies layer opacity in editor
    ↓
LayerStack.MarkDirty(LayerIndex)
    ↓
KLayerEvaluator::EvaluateStack(Stack, Result, Error)
    ↓
Get Visible Layers: [0, 2, 3, 5] (layer 1 disabled, layer 4 locked)
    ↓
For Layer 0 (Image):
    ├─ Check cache: CachedOutput exists, not dirty → use cache
    └─ Blend into all channels (OutputChannels = All)
        ├─ BlendTextures(BaseColor, Layer0.CachedOutput, Normal, 1.0, null, false)
        ├─ BlendTextures(Normal, Layer0.CachedOutput, Normal, 1.0, null, false)
        └─ ... (7 channels total)
    ↓
For Layer 2 (Procedural):
    ├─ Check cache: dirty → re-evaluate
    ├─ GenerateProceduralTexture(ProceduralParams, 1024, 1024)
    │  └─ GPU: FKProceduralNoiseCS dispatch
    ├─ Cache result
    └─ Blend into Roughness + Height only (OutputChannels = 0x14)
    ↓
For Layer 3 (Adjustment):
    ├─ Resolve source: SourceLayerIndex = 2 → use Layer2.CachedOutput
    ├─ ApplyAdjustment(Layer2.CachedOutput, AdjustmentParams)
    │  └─ GPU: FKAdjustmentCS dispatch
    ├─ Cache result
    └─ Blend into BaseColor only (OutputChannels = 0x01)
    ↓
For Layer 5 (Filter):
    ├─ Resolve source: SourceOverride = null, SourceLayerIndex = -1 → use accumulated BaseColor
    ├─ ApplyFilter(Result.BaseColor, FilterParams)
    │  └─ GPU: FKFilterCS dispatch
    ├─ Cache result
    └─ Blend into BaseColor only (OutputChannels = 0x01)
    ↓
Clear dirty flags
    ↓
Result: 7 composited textures (BaseColor, Normal, Roughness, Metallic, Height, AO, Emissive)
```

---


## 12. Critical Design Patterns

### 12.1 Static Function Library Pattern

**Problem:** UE5 Blueprint Function Libraries must be static methods on a UCLASS.

**C++ Implementation:**
```cpp
UCLASS()
class UMaterializeEngine : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
    
public:
    UFUNCTION(BlueprintCallable, Category = "Materialize")
    static bool GeneratePBRMaps(UTexture2D* SourceTexture, 
                                const FMaterializeParams& Params,
                                FMaterializeResult& OutResult);
};
```

**KAIN Implementation:**
```kain
@blueprint
struct MaterializeEngine:
    @blueprint_callable
    fn generate_pbr_maps(source_texture: Texture2D, params: MaterializeParams) -> MaterializeResult:
        # Implementation
```

KAIN generates `UMaterializeEngine : public UBlueprintFunctionLibrary` with static methods.

---

### 12.2 Lazy Singleton Pattern (Preset Registry)

**Problem:** Presets must be initialized before first use, but not at module startup (AssetRegistry not ready).

**C++ Implementation:**
```cpp
class FMaterializePresets
{
private:
    static TArray<FMaterializePreset> Presets;
    static bool bInitialized;
    
    static void Initialize()
    {
        if (bInitialized) return;
        bInitialized = true;
        // ... register all presets
    }
    
public:
    static const TArray<FMaterializePreset>& GetAllPresets()
    {
        if (!bInitialized) Initialize();
        return Presets;
    }
};
```

**KAIN Implementation:**
```kain
struct MaterializePresets:
    @static
    presets: Array<MaterializePreset> = []
    @static
    initialized: Bool = false
    
    fn initialize():
        if initialized:
            return
        initialized = true
        # ... register presets
    
    @blueprint_callable
    fn get_all_presets() -> Array<MaterializePreset>:
        if not initialized:
            initialize()
        return presets
```

---

### 12.3 Dirty Propagation Pattern

**Problem:** When a layer changes, all layers above must be re-evaluated (they depend on accumulated result).

**C++ Implementation:**
```cpp
void FKLayerStack::MarkDirty(int32 Index)
{
    if (Layers.IsValidIndex(Index))
    {
        Layers[Index].bDirty = true;
        // Propagate upward
        for (int32 i = Index + 1; i < Layers.Num(); ++i)
        {
            Layers[i].bDirty = true;
        }
    }
}
```

**KAIN Implementation:**
```kain
fn mark_dirty(stack: LayerStack, index: Int):
    if index >= 0 and index < len(stack.layers):
        stack.layers[index].dirty = true
        for i in range(index + 1, len(stack.layers)):
            stack.layers[i].dirty = true
```

**Key Insight:** Dirty tracking is unidirectional (upward only). Modifying layer N does not affect layers 0 to N-1.

---

### 12.4 RDG Scope Pattern

**Problem:** RDG resources must be created and destroyed within a single scope. Leaking RDG resources causes crashes.

**C++ Implementation:**
```cpp
ENQUEUE_RENDER_COMMAND(CommandName)(
    [Params...](FRHICommandListImmediate& RHICmdList)
    {
        FRDGBuilder GraphBuilder(RHICmdList);
        
        // All RDG operations here
        FRDGTextureRef Texture = GraphBuilder.CreateTexture(...);
        // ... use texture
        
        GraphBuilder.Execute();  // Must execute before lambda exits
    }
);

FlushRenderingCommands();  // Wait for GPU completion
```

**KAIN Implementation:**

KAIN's `shader compute` automatically generates the RDG scope pattern. No manual management needed.

---

### 12.5 Versioning Pattern

**Problem:** Struct layout changes over time. Old saved data must be migrated.

**C++ Implementation:**
```cpp
namespace EKLayerStackVersion
{
    enum Type : int32
    {
        Initial        = 0,
        AddedSoloFlag  = 1,
        AddedLockFlag  = 2,
        AddedDirtyFlag = 3,
        
        LatestPlusOne,
        Latest = LatestPlusOne - 1
    };
}

struct FKLayerStack
{
    int32 Version = EKLayerStackVersion::Latest;
    
    bool MigrateFromOldVersion()
    {
        if (Version >= EKLayerStackVersion::Latest) return false;
        
        if (Version < EKLayerStackVersion::AddedSoloFlag)
        {
            // Migration logic
        }
        
        Version = EKLayerStackVersion::Latest;
        return true;
    }
};
```

**KAIN Implementation:**
```kain
struct LayerStack:
    version: Int = 3
    
    fn migrate_from_old_version() -> Bool:
        if version >= 3:
            return false
        
        # Migration logic for each version
        
        version = 3
        return true
```

**Best Practice:** Call `MigrateFromOldVersion()` in `PostLoad()` for any UObject that contains a `FKLayerStack`.

---


## 13. Error Handling & Validation

### 13.1 Validation System

**MaterializeValidator.h** — Pre-dispatch validation to prevent GPU crashes.

```cpp
class FMaterializeErrorHandler
{
public:
    // Dimension validation
    static bool ValidateDimensions(int32 Width, int32 Height, FString& OutError);
    
    // Texture validation
    static bool ValidateTexture(UTexture2D* Texture, FString& OutError);
    
    // Range validation
    static bool ValidateRange(float Value, float Min, float Max, const FString& ParamName, FString& OutError);
    
    // Logging
    static void LogError(const FString& Context, const FString& Message);
    static void LogWarning(const FString& Context, const FString& Message);
};
```

**Validation Rules:**

| Check | Rule | Error Message |
|-------|------|---------------|
| Dimensions | 64 ≤ Width/Height ≤ 8192 | "Invalid dimensions: {W}x{H}. Must be 64-8192." |
| Texture | Not null, has resource | "Texture is null or has no resource" |
| Texture Format | PF_B8G8R8A8 or compatible | "Unsupported texture format: {Format}" |
| Opacity | 0.0 ≤ Opacity ≤ 1.0 | "Opacity out of range: {Value}" |
| Blend Mode | 0 ≤ BlendMode ≤ 19 | "Invalid blend mode: {Mode}" |
| Filter Type | 0 ≤ FilterType ≤ 12 | "Invalid filter type: {Type}" |

---

### 13.2 Error Propagation

**Pattern:** All API methods return `bool` + `FString& OutError` for error reporting.

```cpp
bool UKLayerEvaluator::EvaluateStack(FKLayerStack& Stack, FKLayerEvalResult& OutResult, FString& OutError)
{
    if (!ValidateLayerStack(Stack, OutError))
    {
        FMaterializeErrorHandler::LogError(TEXT("EvaluateStack"), OutError);
        return false;
    }
    
    // ... evaluation logic
    
    if (!Result.IsValid())
    {
        OutError = TEXT("Evaluation produced invalid result");
        return false;
    }
    
    return true;
}
```

**KAIN Implementation:**

KAIN uses effect tracking (`with IO`) and optional return types:

```kain
fn evaluate_stack(stack: LayerStack) -> Result<LayerEvalResult, String> with IO:
    if not validate_layer_stack(stack):
        return Err("Layer stack validation failed")
    
    # ... evaluation logic
    
    if not result.is_valid():
        return Err("Evaluation produced invalid result")
    
    return Ok(result)
```

---

### 13.3 Shader Error Handling

**MaterializeShaderErrorHandler.h** — Shader compilation and runtime error handling.

```cpp
class FMaterializeShaderErrorHandler
{
public:
    // Check if shader compiled successfully
    static bool ValidateShaderCompilation(const FGlobalShaderType* ShaderType, FString& OutError);
    
    // Check RHI resource validity
    static bool ValidateRHIResource(FTexture2DRHIRef TextureRHI, FString& OutError);
    
    // Log shader dispatch errors
    static void LogShaderDispatchError(const FString& ShaderName, const FString& Error);
};
```

**Common Shader Errors:**

| Error | Cause | Solution |
|-------|-------|----------|
| "Shader not found" | Shader directory not mapped | Check `AddShaderSourceDirectoryMapping()` |
| "Invalid RHI resource" | Texture not initialized | Call `UpdateResource()` before dispatch |
| "Dimension mismatch" | Input/output size mismatch | Validate dimensions before dispatch |
| "UAV format incompatible" | Wrong pixel format | Use PF_B8G8R8A8 for RGBA, PF_R32_FLOAT for scalar |

---


## 14. Advanced Features

### 14.1 Seamless Tiling

**Algorithm:** Makes textures tileable by blending opposite edges.

**CrossBlend Mode (default):**
```
1. Divide texture into 4 quadrants
2. For each edge:
   ├─ Calculate blend weight based on distance from edge
   │  └─ weight = smoothstep(0, BlendWidth, distance)
   ├─ Sample opposite edge
   └─ Blend: result = lerp(current, opposite, weight)
```

**MirrorBlend Mode:**
```
1. Mirror texture at edges
2. Apply CrossBlend to mirrored version
3. Result: Seamless with mirrored symmetry
```

**Histogram Mode:**
```
1. Compute color histogram for each edge
2. Match histograms across opposite edges
3. Apply color correction to minimize seam
4. Blend edges with CrossBlend
```

**GPU Implementation:** `SeamlessAndPacking.usf::SeamlessCS` — 6.1 KB

---

### 14.2 ORM Packing

**Purpose:** Combine 3 grayscale textures into 1 RGB texture (UE5 standard format).

**Packing Formula:**
```
ORM.R = AO
ORM.G = Roughness
ORM.B = Metallic
ORM.A = 1.0 (unused)
```

**Benefits:**
- 3x memory reduction (3 textures → 1 texture)
- 3x texture sample reduction in materials
- Standard UE5 format (compatible with all PBR materials)

**GPU Implementation:** `SeamlessAndPacking.usf::PackORMCS` — trivial channel copy.

---

### 14.3 Multi-Scale Normal Generation

**Purpose:** Capture surface detail at multiple frequency bands.

**Algorithm:**
```
Macro (50% weight):
    ├─ Sample Poisson height at 1px offset
    └─ Captures large-scale features (bumps, dents)

Meso (30% weight):
    ├─ Sample Poisson height at 2px offset
    └─ Captures medium-scale features (texture grain)

Micro (20% weight):
    ├─ Sample linearized luminance Sobel at 1px
    └─ Captures fine surface detail (pores, scratches)

Final = normalize(Macro * 0.5 + Meso * 0.3 + Micro * 0.2)
```

**Why 3 Scales?**
- Macro: Structural shape (from integrated height)
- Meso: Texture grain (from height at larger offset)
- Micro: Surface texture (from luminance variation)

**Result:** Photorealistic normals that capture both geometric and textural detail.

---

### 14.4 Color-Aware Metallic Detection

**Problem:** Luminance-only metallic detection fails for bright colored surfaces (yellow paint vs chrome).

**Solution:** Use brightness + saturation score.

**Algorithm:**
```
Lum = dot(LinearRGB, [0.2126, 0.7152, 0.0722])
Sat = (ChanMax - ChanMin) / ChanMax

MetalScore = Lum * (1.0 - Sat) * MetallicSensitivity

Rationale:
- Metals: High brightness + Low saturation (reflective, no diffuse color)
- Paint: High brightness + High saturation (colored diffuse)
```

**Example:**
- Bright chrome: Lum=0.9, Sat=0.1 → MetalScore = 0.9 * 0.9 * 2.0 = 1.62 → Metal=1.0
- Yellow paint: Lum=0.9, Sat=0.8 → MetalScore = 0.9 * 0.2 * 2.0 = 0.36 → Metal=0.36

---

### 14.5 Variance-Based Roughness

**Problem:** Luminance-only roughness misses surface micro-detail (rough concrete with dark patches).

**Solution:** Blend luminance-based roughness with local variance.

**Algorithm:**
```
1. Compute local variance (3x3 window):
   LumMean = Σ(Lum) / 9
   Variance = (Σ(Lum²) / 9 - LumMean²) * 8.0

2. Luminance-based roughness:
   RoughLum = RoughnessBase + (Lum - 0.5) * RoughnessContrast
   if bRoughnessInvert: RoughLum = 1.0 - RoughLum

3. Blend:
   Rough = lerp(RoughLum, RoughLum + Variance, VarianceWeight)
```

**Why Variance?**
- Captures surface texture variation
- Detects rough areas even in dark regions
- Produces more realistic roughness maps

**VarianceWeight = 0.5:** 50% luminance, 50% variance (balanced)

---


## 15. KAIN Implementation Recommendations

### 15.1 Priority 1: Core Types & Enums

**Files:** `types.kn`

**Effort:** Low (straightforward enum/struct translation)

**Compression:** 1:1 (KAIN enums map directly to UENUM)

**Implementation:**
```kain
# All enums from MaterializeTypes.h and KLayerStack.h
enum MaterializeCategory: ...
enum SeamlessMode: ...
enum LayerBlendMode: ...
enum LayerType: ...
enum LayerOutputChannel: ...
enum ProceduralNoiseType: ...
enum FilterType: ...
enum AdjustmentType: ...
enum GeneratorType: ...

# All structs
struct MaterializeParams: ...
struct MaterializeResult: ...
struct MaterializePreset: ...
struct MaterializeMasterPreset: ...
struct ProceduralParams: ...
struct FilterParams: ...
struct AdjustmentParams: ...
struct Layer: ...
struct LayerStack: ...
struct LayerEvalResult: ...
```

---

### 15.2 Priority 2: Compute Shaders

**Files:** `shaders/layer_blend.kn`, `shaders/procedural_noise.kn`, `shaders/layer_filter.kn`, `shaders/layer_adjustment.kn`, `shaders/math_operations.kn`

**Effort:** Medium (shader logic is straightforward, but 5 shaders × 200 lines each)

**Compression:** 1:10 (KAIN generates FGlobalShader class + RDG dispatch + Blueprint wrapper)

**Implementation Strategy:**
1. Copy .usf shader logic directly (HLSL syntax is similar to KAIN)
2. Wrap in `shader compute` declaration
3. KAIN backend generates all C++ boilerplate

**Example:**
```kain
# shaders/layer_blend.kn
shader compute LayerBlend(thread_id: Vec3):
    uniform blend_mode: Int @0
    uniform opacity: Float @1
    uniform has_mask: Bool @2
    uniform invert_mask: Bool @3
    texture in_base: Sampler2D @4
    texture in_blend: Sampler2D @5
    texture in_mask: Sampler2D @6
    buffer out_result: RWTexture2D<Vec4> @7
    
    # Copy logic from LayerBlend.usf (234 lines)
    # ... blend mode functions
    # ... main blend logic
```

---

### 15.3 Priority 3: PBR Generator Shaders

**Files:** `shaders/pbr_generator.kn`, `shaders/seamless_packing.kn`

**Effort:** High (complex multi-pass pipeline with 423 lines of shader code)

**Compression:** 1:10

**Implementation Strategy:**
1. Split into 3 separate shaders (GradientCS, HeightIntegrationCS, FinalPBRCS)
2. Copy shader logic from `PBRGenerator.usf`
3. Add dispatch orchestration in `compute_engine.kn`

**Example:**
```kain
# shaders/pbr_generator.kn

shader compute GradientExtraction(thread_id: Vec3):
    uniform params: MaterializeParams @0
    texture in_source: Sampler2D @1
    buffer out_gradient: RWTexture2D<Vec2> @2
    # ... 150 lines from PBRGenerator.usf::GradientCS

shader compute HeightIntegration(thread_id: Vec3):
    uniform params: MaterializeParams @0
    texture in_gradient: Sampler2D @1
    texture in_height_prev: Sampler2D @2
    buffer out_height_next: RWTexture2D<Float> @3
    # ... 50 lines from PBRGenerator.usf::HeightIntegrationCS

shader compute FinalPBRGeneration(thread_id: Vec3):
    uniform params: MaterializeParams @0
    texture in_source: Sampler2D @1
    texture in_height: Sampler2D @2
    buffer out_normal: RWTexture2D<Vec4> @3
    buffer out_roughness: RWTexture2D<Float> @4
    buffer out_metallic: RWTexture2D<Float> @5
    buffer out_ao: RWTexture2D<Float> @6
    buffer out_height: RWTexture2D<Float> @7
    buffer out_emissive: RWTexture2D<Float> @8
    # ... 200 lines from PBRGenerator.usf::FinalPBRCS
```

---

### 15.4 Priority 4: Layer Evaluator

**Files:** `layer_evaluator.kn`

**Effort:** High (complex evaluation logic with caching, dirty tracking, source resolution)

**Compression:** 1:5

**Implementation Strategy:**
1. Implement `EvaluateStack()` with bottom-to-top compositing
2. Implement `EvaluateSingleLayer()` with type dispatch
3. Implement `BlendTextures()` with shader dispatch
4. Implement helper methods (validation, texture creation)

**Key Logic:**
- Dirty tracking + caching
- Source resolution for Filter/Adjustment layers
- Per-channel blending with OutputChannels bitflags
- Solo/lock/enabled visibility filtering

---

### 15.5 Priority 5: Compute Engine

**Files:** `compute_engine.kn`

**Effort:** Medium (orchestrates multi-pass pipeline)

**Compression:** 1:8

**Implementation Strategy:**
1. Implement `GeneratePBRMapsGPU()` with 3-pass dispatch
2. Implement `MakeSeamless()` with shader dispatch
3. Implement `PackORM()` with shader dispatch
4. Implement `ReadbackResult()` for GPU→CPU copy

**Key Logic:**
- Multi-pass orchestration (Gradient → Height Integration × 24 → Final PBR)
- Ping-pong buffers for height integration
- RDG resource management
- GPU→CPU readback

---

### 15.6 Priority 6: Preset System

**Files:** `presets.kn`, `preset_registry.kn`

**Effort:** Low (data-driven, mostly static arrays)

**Compression:** 1:1

**Implementation Strategy:**
1. Define all 30+ presets as static array
2. Implement lazy initialization
3. Implement query methods (GetAllPresets, GetPresetsByCategory, GetPresetById)

**Data Volume:** 30 presets × 30 parameters = 900 values (can be generated from CSV or JSON).

---

### 15.7 Priority 7: Preset Shaders

**Files:** `shaders/presets/*.kn` (15 shaders)

**Effort:** Medium (15 shaders × 50-100 lines each)

**Compression:** 1:10

**Implementation Strategy:**
1. Copy shader logic from .usf files
2. Wrap in `shader compute` declarations
3. KAIN generates FGlobalShader classes + dispatch helpers

**Shaders:**
- Metal: MetalAnisotropicSpecular, MetalFresnelRim
- Glossy: GlossyClearCoat, GlossySubsurface, GlossyDualLobe
- Toon: ToonCelShading, ToonSpecular, ToonRimLight, ToonOutlineDetection, ToonConfigurableBands
- Shared: MaterializeFresnelSchlick, MaterializeGGXDistribution, MaterializeSmithVisibility

---


## 16. Architecture Summary

### 16.1 System Layers

```
┌─────────────────────────────────────────────────────────────────┐
│ Layer 5: User Interface (Editor)                                │
│ ├─ SMaterializeEditor (main editor widget)                      │
│ ├─ SMaterializeBatchWindow (batch processor)                    │
│ ├─ Asset Actions (right-click menu integration)                 │
│ └─ Toolbar Extension (quick access button)                      │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ Layer 4: API Layer (Blueprint Function Libraries)               │
│ ├─ MaterializeEngine (main API)                                 │
│ ├─ MaterializeComputeEngine (GPU pipeline)                      │
│ ├─ KLayerEvaluator (layer compositing)                          │
│ ├─ MaterializePresets (preset registry)                         │
│ └─ MaterializePresetRegistry (master material registry)         │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ Layer 3: Core Logic                                             │
│ ├─ Layer Stack Management (FKLayerStack)                        │
│ ├─ Evaluation Algorithm (bottom-to-top compositing)             │
│ ├─ Dirty Tracking + Caching                                     │
│ ├─ Source Resolution (Filter/Adjustment)                        │
│ ├─ Visibility Filtering (solo/lock/enabled)                     │
│ └─ Multi-Pass Pipeline Orchestration                            │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ Layer 2: GPU Compute Shaders (30+ shaders)                      │
│ ├─ Layer Shaders (Blend, Noise, Filter, Adjustment, Math)       │
│ ├─ PBR Shaders (Gradient, HeightIntegration, FinalPBR)          │
│ ├─ Utility Shaders (Seamless, PackORM)                          │
│ └─ Preset Shaders (Metal, Glossy, Toon — 15 shaders)            │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│ Layer 1: Data Structures                                        │
│ ├─ Type System (10 enums, 10 structs)                           │
│ ├─ Layer Definition (FKLayer with type-specific data)           │
│ ├─ Parameter Sets (FMaterializeParams, FKProceduralParams, etc.)│
│ └─ Result Structures (FMaterializeResult, FKLayerEvalResult)    │
└─────────────────────────────────────────────────────────────────┘
```

---

### 16.2 Key Architectural Principles

#### 1. Data-Oriented Design
- Layers are stored in contiguous array (cache-friendly)
- Evaluation is bottom-to-top linear scan (no pointer chasing)
- GPU shaders operate on texture arrays (SIMD-friendly)

#### 2. Separation of Concerns
- **Data:** FKLayerStack (pure data, no logic)
- **Logic:** UKLayerEvaluator (stateless, operates on data)
- **GPU:** Compute shaders (pure functions, no state)

#### 3. Lazy Evaluation
- Layers cache their output (CachedOutput)
- Only dirty layers are re-evaluated
- Dirty propagation is upward only (layers below are independent)

#### 4. Composability
- Each layer is independent (no cross-layer dependencies except Filter/Adjustment source)
- Blend modes are orthogonal to layer types
- Output channels are independent (layer can affect multiple channels)

#### 5. Extensibility
- New blend modes: Add enum value + blend function in shader
- New layer types: Add enum value + evaluation case
- New presets: Add entry to static array
- New master materials: Register in PresetRegistry

---

### 16.3 Code Statistics

**Original C++ Plugin:**

| Component | Files | Lines | Complexity |
|-----------|-------|-------|------------|
| Core Types | 2 | 936 | Low |
| Layer System | 2 | 2,254 | High |
| Compute Engine | 2 | 1,004 | High |
| PBR Engine | 2 | 912 | Medium |
| Preset System | 4 | 719 | Low |
| Compute Shaders | 5 | 1,200 | Medium |
| Preset Shaders | 15 | 2,500 | Medium |
| Editor UI | 2 | 110,873 | Very High |
| Support Systems | 15 | 5,000 | Medium |
| **Total** | **49** | **~125,000** | |

**Estimated KAIN Implementation:**

| Component | Files | Lines | Compression |
|-----------|-------|-------|-------------|
| Core Types | 1 | 400 | 1:2.3 |
| Layer System | 2 | 800 | 1:2.8 |
| Compute Engine | 1 | 300 | 1:3.3 |
| PBR Engine | 1 | 200 | 1:4.5 |
| Preset System | 2 | 600 | 1:1.2 |
| Compute Shaders | 5 | 600 | 1:2.0 |
| Preset Shaders | 15 | 1,200 | 1:2.1 |
| Editor UI | 2 | 3,000 | 1:37 (Slate compression) |
| Support Systems | 5 | 500 | 1:10 |
| **Total** | **34** | **~7,600** | **1:16.4** |

**Note:** Editor UI has extreme compression due to KAIN's Slate codegen (SNew() chains, delegate binding, property handles).

---

### 16.4 Implementation Phases

#### Phase 1: Foundation (Week 1)
- Core types and enums (`types.kn`)
- Layer stack structure (`layer_stack.kn`)
- Basic validation (`validation.kn`)
- **Deliverable:** Type system compiles, no runtime yet

#### Phase 2: GPU Shaders (Week 2)
- Layer blend shader (`shaders/layer_blend.kn`)
- Procedural noise shader (`shaders/procedural_noise.kn`)
- Filter shader (`shaders/layer_filter.kn`)
- Adjustment shader (`shaders/layer_adjustment.kn`)
- Math operations shader (`shaders/math_operations.kn`)
- **Deliverable:** All layer shaders compile and dispatch

#### Phase 3: Layer Evaluator (Week 3)
- Evaluation algorithm (`layer_evaluator.kn`)
- Dirty tracking + caching
- Source resolution
- Per-channel blending
- **Deliverable:** Layer stack evaluation works end-to-end

#### Phase 4: PBR Pipeline (Week 4)
- PBR generator shaders (`shaders/pbr_generator.kn`)
- Seamless + packing shaders (`shaders/seamless_packing.kn`)
- Compute engine orchestration (`compute_engine.kn`)
- Multi-pass pipeline
- **Deliverable:** Full PBR generation works

#### Phase 5: Preset System (Week 5)
- Preset definitions (`presets.kn`)
- Preset registry (`preset_registry.kn`)
- Master material loading
- Preset shaders (15 shaders in `shaders/presets/`)
- **Deliverable:** All 30+ presets work, 4 master materials load

#### Phase 6: Editor UI (Week 6-7)
- Main editor widget (`editor/materialize_editor.kn`)
- Batch processor (`editor/batch_window.kn`)
- Asset actions (`editor/asset_actions.kn`)
- Toolbar extension (`editor/toolbar_extension.kn`)
- **Deliverable:** Full editor UI functional

---


## 17. Technical Deep-Dives

### 17.1 Poisson Height Integration (Jacobi Solver)

**Mathematical Foundation:**

Given a gradient field g (from luminance Sobel), find height field h such that:
```
∇h = g
```

This is equivalent to solving the Poisson equation:
```
∇²h = ∇·g
```

**Discrete Formulation (2D grid):**
```
h[x,y] = (h[x-1,y] + h[x+1,y] + h[x,y-1] + h[x,y+1] + div[x,y]) / 4

where div[x,y] = (g[x+1,y].x - g[x-1,y].x + g[x,y+1].y - g[x,y-1].y) / 2
```

**Jacobi Iteration:**
```
Initialize: h⁰[x,y] = 0 for all pixels

For iteration n = 0 to 23:
    For each pixel (x,y):
        h^(n+1)[x,y] = (h^n[x-1,y] + h^n[x+1,y] + h^n[x,y-1] + h^n[x,y+1] + div[x,y]) / 4
```

**Convergence:** 24 iterations is sufficient for 1024x1024 textures (error < 0.1%).

**GPU Implementation:** Each iteration is a separate compute shader dispatch. Ping-pong buffers avoid read-write hazards.

**Why Not Gauss-Seidel?** Gauss-Seidel converges faster but is not GPU-friendly (requires sequential updates).

---

### 17.2 Multi-Octave Normal Generation

**Frequency Band Decomposition:**

```
Macro (0.5 weight):
    Kernel: 1px Poisson height gradient
    Frequency: Low (captures bumps, dents)
    Formula: N_macro = normalize([-∂h/∂x, -∂h/∂y, 1] * NormalStrength * 2.0)

Meso (0.3 weight):
    Kernel: 2px Poisson height gradient
    Frequency: Medium (captures texture grain)
    Formula: N_meso = normalize([-∂h/∂x, -∂h/∂y, 1] * NormalStrength * 1.2)

Micro (0.2 weight):
    Kernel: 1px luminance Sobel
    Frequency: High (captures surface texture)
    Formula: N_micro = normalize([Sobel_x, Sobel_y, 1] * NormalStrength * 0.6)

Final:
    N = normalize(N_macro * 0.5 + N_meso * 0.3 + N_micro * 0.2)
```

**Why 3 Bands?**
- Single-scale normals look flat (miss either macro or micro detail)
- 3 bands capture full frequency spectrum
- Weights are empirically tuned (0.5/0.3/0.2 produces best visual results)

**Advanced Mode (bAdvancedNormal = true):**

Uses multi-octave Sobel with configurable sigma:
```
For octave k = 0 to NormalOctaves-1:
    sigma = NormalSigmaBase * 2^k
    offset = round(sigma)
    gradient[k] = Sobel(luminance, offset) / sigma
    
Final gradient = Σ gradient[k]
```

This produces even more detailed normals at the cost of performance (3-6 octaves).

---

### 17.3 8-Direction Horizon AO

**Algorithm:** Samples height along 8 rays to compute occlusion.

**Directions:**
```
Cardinal (weight 1.0):
    East:  [1, 0]
    West:  [-1, 0]
    North: [0, -1]
    South: [0, 1]

Diagonal (weight 0.707):
    NE: [1, -1]
    NW: [-1, -1]
    SE: [1, 1]
    SW: [-1, 1]
```

**Per-Direction Sampling:**
```
For each direction d:
    maxHorizon = 0
    For step s = 1 to AORadius:
        samplePos = currentPos + d * s
        horizon = (height[samplePos] - height[currentPos]) / s
        maxHorizon = max(maxHorizon, horizon)
    
    occlusion += max(0, maxHorizon) * directionWeight
```

**Final AO:**
```
occlusion /= totalWeight
AO = pow(saturate(1.0 - occlusion * AOIntensity + AOBias), AOContrast)
```

**Why 8 Directions?**
- 4 directions (cross) produces banding artifacts
- 8 directions (cross + diagonals) produces smooth occlusion
- 16+ directions has diminishing returns

**Performance:** AORadius=4 → 32 samples per pixel (8 directions × 4 steps)

---

### 17.4 Blend Mode Mathematics

**Alpha Compositing (Porter-Duff "Over" Operator):**

```
Given:
    base: Accumulated result (RGBA)
    blend: Current layer (RGBA)
    opacity: Layer opacity (0-1)
    mask: Mask value (0-1)

Step 1: Calculate effective opacity
    effectiveOpacity = opacity * mask

Step 2: Apply blend mode to RGB
    blended.rgb = BlendFunction(base.rgb, blend.rgb)

Step 3: Calculate final blend amount
    finalBlendAmount = effectiveOpacity * blend.a

Step 4: Lerp RGB
    result.rgb = lerp(base.rgb, blended.rgb, finalBlendAmount)

Step 5: Composite alpha
    result.a = base.a + blend.a * effectiveOpacity * (1 - base.a)
```

**Example Blend Functions:**

**Multiply:**
```
BlendMultiply(base, blend) = base * blend
```

**Screen:**
```
BlendScreen(base, blend) = 1 - (1 - base) * (1 - blend)
```

**Overlay:**
```
BlendOverlay(base, blend) = 
    base < 0.5 ? 2 * base * blend 
               : 1 - 2 * (1 - base) * (1 - blend)
```

**SoftLight:**
```
BlendSoftLight(base, blend) = 
    blend < 0.5 ? 2 * base * blend + base² * (1 - 2 * blend)
                : sqrt(base) * (2 * blend - 1) + 2 * base * (1 - blend)
```

---


## 18. Comparison with Industry Tools

### 18.1 Feature Parity Analysis

| Feature | Materialize | Substance Designer | Quixel Mixer | Materialize Advantage |
|---------|-------------|-------------------|--------------|----------------------|
| Layer System | ✓ (20 blend modes) | ✓ (30+ blend modes) | ✓ (15 blend modes) | Photoshop-compatible |
| Procedural Noise | ✓ (15 types) | ✓ (50+ types) | ✗ | Good coverage |
| PBR Generation | ✓ (GPU, multi-pass) | ✓ (CPU/GPU) | ✓ (AI-based) | High quality, fast |
| Preset System | ✓ (30+ presets) | ✓ (100+ presets) | ✓ (1000+ presets) | Extensible |
| Master Materials | ✓ (4 variants) | ✗ | ✗ | UE5-native integration |
| Real-time Preview | ✓ | ✓ | ✓ | GPU-accelerated |
| Batch Processing | ✓ | ✓ | ✓ | Integrated |
| Graph Editor | Partial | ✓ (node-based) | ✗ | Planned (Part 2) |
| AI Features | ✗ | ✗ | ✓ (AI upscaling) | Not yet |

**Unique Advantages:**
1. **UE5-Native:** Generates UE5 materials directly (no import/export)
2. **Master Material System:** 4 specialized shading models (Standard, Metal, Glossy, Toon)
3. **Layer Stack Serialization:** Save/load layer stacks as assets
4. **GPU-Accelerated:** All operations use compute shaders (RDG pipeline)
5. **Open Architecture:** Extensible preset system, custom master materials

---

### 18.2 Workflow Comparison

**Substance Designer Workflow:**
```
1. Create node graph in Substance Designer
2. Export as .sbsar
3. Import into UE5
4. Generate textures in UE5
5. Create material instance
```

**Materialize Workflow:**
```
1. Import source texture into UE5
2. Right-click → "Generate PBR Maps"
3. Select preset
4. Adjust parameters (optional)
5. Generate → 8 assets created automatically
```

**Advantage:** 5 steps → 2 steps, no external tools, no import/export.

---


## 19. Shader Implementation Details

### 19.1 Shader File Organization

**Current C++ Structure:**
```
Shaders/
├── KStudioCore/                    # Core layer operations
│   ├── LayerBlend.usf              # 20 blend modes
│   ├── LayerFilter.usf             # 13 filter types
│   ├── LayerAdjustment.usf         # 9 adjustment types
│   ├── ProceduralNoise.usf         # 15 noise types
│   └── MathOperations.usf          # Add, Multiply, Lerp
│
├── PBRGenerator.usf                # Multi-pass PBR pipeline (423 lines)
├── SeamlessAndPacking.usf          # Seamless + ORM packing
├── MaterializeProceduralCommon.ush # Shared noise functions
│
└── Preset Shaders/                 # Master material shaders
    ├── Metal/
    │   ├── MetalAnisotropicSpecular.usf
    │   └── MetalFresnelRim.usf
    ├── Glossy/
    │   ├── GlossyClearCoat.usf
    │   ├── GlossySubsurface.usf
    │   └── GlossyDualLobe.usf
    ├── Toon/
    │   ├── ToonCelShading.usf
    │   ├── ToonSpecular.usf
    │   ├── ToonRimLight.usf
    │   ├── ToonOutlineDetection.usf
    │   └── ToonConfigurableBands.usf
    └── Shared/
        ├── MaterializeFresnelSchlick.usf
        ├── MaterializeGGXDistribution.usf
        └── MaterializeSmithVisibility.usf
```

**KAIN Structure:**
```
src/shaders/
├── layer_blend.kn          # Generates LayerBlend.usf + FKLayerBlendCS
├── layer_filter.kn         # Generates LayerFilter.usf + FKFilterCS
├── layer_adjustment.kn     # Generates LayerAdjustment.usf + FKAdjustmentCS
├── procedural_noise.kn     # Generates ProceduralNoise.usf + FKProceduralNoiseCS
├── math_operations.kn      # Generates MathOperations.usf + FKMathOperationCS
├── pbr_generator.kn        # Generates PBRGenerator.usf + 3 shader classes
├── seamless_packing.kn     # Generates SeamlessAndPacking.usf + 2 shader classes
└── presets/
    ├── metal_anisotropic.kn
    ├── metal_fresnel_rim.kn
    ├── glossy_clear_coat.kn
    ├── glossy_subsurface.kn
    ├── glossy_dual_lobe.kn
    ├── toon_cel_shading.kn
    ├── toon_specular.kn
    ├── toon_rim_light.kn
    ├── toon_outline.kn
    ├── toon_bands.kn
    ├── fresnel_schlick.kn
    ├── ggx_distribution.kn
    └── smith_visibility.kn
```

---

### 19.2 Shared Shader Library Pattern

**MaterializeProceduralCommon.ush** — Shared helper functions used by multiple shaders.

**Contents:**
- Hash functions (Hash21, Hash31)
- Noise primitives (SmoothNoise, ValueNoise)
- FBM implementation
- Color space conversions (RGB↔HSV↔HSL)
- Blend mode helpers

**Usage:**
```hlsl
// In any .usf shader
#include "/Plugin/Materialize/MaterializeProceduralCommon.ush"

float noise = SmoothNoise(uv);
float fbm = FBM(uv, 4);
```

**KAIN Implementation:**

KAIN will automatically generate shared .ush files when multiple shaders use the same helper functions. No manual management needed.

---

### 19.3 Shader Parameter Synchronization

**Problem:** CPU-side layer data must be synchronized to GPU uniform buffers before dispatch.

**C++ Pattern:**
```cpp
template<typename TShaderParameters>
void SyncLayerParametersToGPU(TShaderParameters* Params, const FKLayer& Layer)
{
    Params->BlendMode = static_cast<uint32>(Layer.BlendMode);
    Params->Opacity = Layer.Opacity;
    Params->bHasMask = Layer.bHasMask ? 1 : 0;
    Params->bInvertMask = Layer.bInvertMask ? 1 : 0;
    // ... sync all parameters
}
```

**KAIN Implementation:**

KAIN's shader dispatch helpers automatically sync parameters:

```kain
# User code
let result = dispatch_layer_blend(
    base_texture,
    blend_texture,
    layer.blend_mode,
    layer.opacity,
    layer.mask_texture,
    layer.invert_mask,
    dimensions
)

# KAIN generates synchronization code automatically
```

---


## 20. Data Structure Relationships

### 20.1 Type Dependency Graph

```
FMaterializeParams (30+ fields)
    ↓ used by
MaterializeEngine::GeneratePBRMaps()
MaterializeComputeEngine::GeneratePBRMapsGPU()
    ↓ produces
FMaterializeResult (9 textures)

FMaterializePreset (id, name, category, params)
    ↓ contains
FMaterializeParams
    ↓ stored in
FMaterializePresets (static registry)

FMaterializeMasterPreset (id, name, material path, features)
    ↓ stored in
FMaterializePresetRegistry (static registry)

FKLayer (name, type, blend mode, opacity, type-specific data)
    ↓ contains
FKProceduralParams | FKFilterParams | FKAdjustmentParams
    ↓ stored in
FKLayerStack (array of layers)
    ↓ evaluated by
UKLayerEvaluator::EvaluateStack()
    ↓ produces
FKLayerEvalResult (7 textures)
```

---

### 20.2 Ownership Model

**Texture Ownership:**

| Texture Type | Owner | Lifetime | GC Behavior |
|-------------|-------|----------|-------------|
| Source Texture | User asset | Persistent | Managed by AssetRegistry |
| Transient Output | FMaterializeResult | Temporary | GC when no references |
| Cached Layer Output | FKLayer.CachedOutput | Temporary | GC when layer destroyed |
| Saved Output | Persistent asset | Persistent | Managed by AssetRegistry |
| Material Instance | Persistent asset | Persistent | Managed by AssetRegistry |

**Memory Management Rules:**
1. Never store raw pointers to UTexture2D (use TObjectPtr)
2. Transient textures are GC'd when result struct is destroyed
3. Cached outputs are GC'd when layer stack is destroyed
4. Persistent assets are never GC'd (RF_Standalone flag)

---

### 20.3 Thread Safety

**Render Thread Operations:**

All GPU operations must be enqueued to the render thread:

```cpp
ENQUEUE_RENDER_COMMAND(CommandName)(
    [Params...](FRHICommandListImmediate& RHICmdList)
    {
        // GPU operations here
    }
);

FlushRenderingCommands();  // Wait for completion
```

**Thread Safety Rules:**
1. Never access UTexture2D::GetResource() from game thread without FlushRenderingCommands()
2. Never modify texture data while GPU is reading it
3. Use FRenderCommandFence for synchronization
4. RDG resources are render-thread only (never escape lambda)

**KAIN Implementation:**

KAIN's shader dispatch automatically handles render thread enqueuing and synchronization. No manual thread management needed.

---


## 21. Testing & Validation Strategy

### 21.1 Unit Test Coverage (Recommended)

**Core Types:**
- Enum validation (all values in range)
- Struct default initialization
- Versioning migration logic

**Layer Stack:**
- Add/Remove/Move/Duplicate operations
- Dirty propagation (mark layer N → layers N+1 to end are dirty)
- Visibility filtering (solo/lock/enabled combinations)
- Search operations (FindLayerByGuid, FindLayerByName)

**Layer Evaluator:**
- Single layer evaluation (all 8 layer types)
- Blend mode correctness (20 blend modes)
- Mask application (with/without invert)
- Source resolution (Filter/Adjustment layers)
- Per-channel blending (OutputChannels bitflags)

**Compute Engine:**
- Multi-pass pipeline (3 passes execute in order)
- Seamless tiling (3 modes)
- ORM packing (channel mapping)
- GPU→CPU readback (pixel data integrity)

**Preset System:**
- Preset lookup (by ID, by category)
- Default params (all fields initialized)
- Master material loading (fallback chain)

---

### 21.2 Integration Test Scenarios

**Scenario 1: Simple PBR Generation**
```
Input: 1024x1024 photo texture
Preset: "leather_worn"
Expected Output: 8 assets (Normal, Roughness, Metallic, AO, Height, Emissive, ORM, Material)
Validation: All textures are 1024x1024, Material has correct parameters
```

**Scenario 2: Layer Stack Evaluation**
```
Input: Stack with 5 layers (Image, Procedural, Adjustment, Filter, Fill)
Expected Output: 7 composited textures
Validation: Each channel reflects layer contributions, blend modes applied correctly
```

**Scenario 3: Dirty Tracking**
```
Input: Stack with 10 layers, modify layer 5
Expected: Layers 5-9 marked dirty, layers 0-4 unchanged
Validation: Only dirty layers re-evaluated, cached outputs used for clean layers
```

**Scenario 4: Solo/Lock Behavior**
```
Input: Stack with 5 layers, layer 2 solo, layer 3 locked
Expected: Only layer 2 visible
Validation: GetVisibleLayerIndices() returns [2]
```

**Scenario 5: Master Material Fallback**
```
Input: Request "Metal" preset, master material missing
Expected: Fallback to transient generation, then engine default
Validation: Material instance created successfully, no crash
```

---

### 21.3 Performance Benchmarks

**Target Performance (1024x1024):**

| Operation | Target Time | Acceptable Range |
|-----------|-------------|------------------|
| Single layer blend | < 1 ms | 0.5-1.5 ms |
| Procedural noise | < 2 ms | 1.0-3.0 ms |
| Filter (small kernel) | < 1 ms | 0.5-2.0 ms |
| Filter (large kernel) | < 5 ms | 3.0-7.0 ms |
| Adjustment | < 1 ms | 0.5-1.5 ms |
| Full PBR generation | < 10 ms | 7.0-15.0 ms |
| 10-layer stack eval | < 30 ms | 20-40 ms |

**Regression Tests:**
- Performance must not degrade by >10% between versions
- Memory usage must not increase by >20% between versions

---


## 22. Future Architecture Considerations

### 22.1 Planned Enhancements (Not in Current C++ Plugin)

**1. Graph Editor Integration**
- Node-based layer editing (similar to Substance Designer)
- Visual connections between layers
- Real-time preview per node
- **Status:** Partial implementation exists in `Graph/` folder

**2. AI-Based Features**
- AI upscaling (2x, 4x)
- AI-based PBR generation (neural network approach)
- Smart preset selection (analyze texture, suggest preset)
- **Status:** Not implemented

**3. Animation Support**
- Animated procedural noise (Time parameter)
- Layer animation curves
- Keyframe system
- **Status:** Time parameter exists, no keyframe system

**4. Mesh-Based Generators**
- True AO from mesh geometry (ray tracing)
- Curvature from mesh normals
- Thickness from mesh volume
- **Status:** Placeholder implementation (uses preset shaders)

**5. Custom Blend Modes**
- User-defined blend functions
- Scriptable blend modes
- **Status:** Not implemented

---

### 22.2 Scalability Improvements

**1. Tile-Based Processing**
- Process large textures in tiles (8192x8192 → 16 tiles of 2048x2048)
- Reduces GPU memory usage
- Enables 16K+ texture support

**2. Async Evaluation**
- Non-blocking layer evaluation
- Progress callbacks
- Cancellation support

**3. Multi-GPU Support**
- Distribute layers across multiple GPUs
- Parallel evaluation of independent layers

**4. Streaming**
- Stream texture data from disk (virtual texturing)
- Reduce memory footprint for large stacks

---

### 22.3 Extensibility Points

**1. Custom Layer Types**
```cpp
// User-defined layer type
enum class EKLayerType : uint8
{
    // ... existing types
    Custom = 100  // User-defined start
};

// User implements evaluation
UTexture2D* EvaluateCustomLayer(const FKLayer& Layer, int32 Width, int32 Height)
{
    // Custom logic
}
```

**2. Custom Blend Modes**
```cpp
// User-defined blend mode
enum class EKLayerBlendMode : uint8
{
    // ... existing modes
    CustomBlend = 100  // User-defined start
};

// User implements blend function in shader
float3 BlendCustom(float3 base, float3 blend)
{
    // Custom blend logic
}
```

**3. Custom Presets**
```cpp
// User registers custom preset
FMaterializePreset CustomPreset;
CustomPreset.Id = TEXT("my_custom_preset");
CustomPreset.DisplayName = FText::FromString(TEXT("My Custom Preset"));
CustomPreset.Category = EMaterializeCategory::Custom;
CustomPreset.Params = MyParams;

FMaterializePresets::RegisterPreset(CustomPreset);
```

**4. Custom Master Materials**
```cpp
// User registers custom master material
FMaterializeMasterPreset CustomMaster(
    TEXT("MyCustomMaster"),
    TEXT("My Custom Master Material"),
    TEXT("Custom shading model"),
    TEXT("/Game/Materials/M_MyCustomMaster.M_MyCustomMaster")
);

FMaterializePresetRegistry::RegisterPreset(CustomMaster);
```

---


## 23. KAIN Implementation Checklist

### 23.1 Core Systems (Must-Have)

- [ ] **Type System** (`types.kn`)
  - [ ] 10 enums (MaterializeCategory, SeamlessMode, LayerBlendMode, LayerType, LayerOutputChannel, ProceduralNoiseType, FilterType, AdjustmentType, GeneratorType)
  - [ ] 10 structs (MaterializeParams, MaterializeResult, MaterializePreset, MaterializeMasterPreset, ProceduralParams, FilterParams, AdjustmentParams, Layer, LayerStack, LayerEvalResult)
  - [ ] Versioning support (LayerStack.version + migrate_from_old_version())

- [ ] **Layer Stack** (`layer_stack.kn`)
  - [ ] Layer management (add, insert, remove, move, duplicate)
  - [ ] Dirty tracking (mark_dirty, mark_all_dirty, clear_dirty_flags)
  - [ ] Visibility filtering (get_visible_layer_indices with solo/lock/enabled)
  - [ ] Search operations (find_layer_by_guid, find_layer_by_name)
  - [ ] Factory methods (create_image_layer, create_fill_layer, etc.)

- [ ] **Layer Evaluator** (`layer_evaluator.kn`)
  - [ ] Stack evaluation (evaluate_stack with bottom-to-top compositing)
  - [ ] Single layer evaluation (evaluate_single_layer with type dispatch)
  - [ ] Texture blending (blend_textures with shader dispatch)
  - [ ] Procedural generation (generate_procedural_texture)
  - [ ] Filter application (apply_filter)
  - [ ] Adjustment application (apply_adjustment)
  - [ ] Math operations (add_textures, multiply_textures, lerp_textures)
  - [ ] Validation (validate_layer_stack, validate_blend_mode, etc.)

- [ ] **Compute Engine** (`compute_engine.kn`)
  - [ ] Multi-pass PBR generation (generate_pbr_maps_gpu)
  - [ ] Seamless tiling (make_seamless)
  - [ ] ORM packing (pack_orm)
  - [ ] GPU→CPU readback (readback_texture, readback_result)
  - [ ] Resource cleanup (cleanup_transient_resources)

- [ ] **PBR Engine** (`engine.kn`)
  - [ ] CPU-based generation (generate_pbr_maps — legacy)
  - [ ] Save pipeline (generate_and_save_pbr_maps)
  - [ ] Material instance creation
  - [ ] Asset persistence

- [ ] **Preset System** (`presets.kn`, `preset_registry.kn`)
  - [ ] 30+ preset definitions
  - [ ] Lazy initialization
  - [ ] Query methods (get_all_presets, get_presets_by_category, get_preset_by_id)
  - [ ] Master material registry (4 built-in presets)
  - [ ] Master material loading with fallback chain

---

### 23.2 GPU Shaders (Must-Have)

- [ ] **Layer Blend Shader** (`shaders/layer_blend.kn`)
  - [ ] 20 blend mode functions
  - [ ] Mask support (with invert)
  - [ ] Alpha compositing
  - [ ] FKLayerBlendCS class generation

- [ ] **Procedural Noise Shader** (`shaders/procedural_noise.kn`)
  - [ ] 15 noise type implementations
  - [ ] FBM with octaves
  - [ ] Seamless tiling support
  - [ ] FKProceduralNoiseCS class generation

- [ ] **Filter Shader** (`shaders/layer_filter.kn`)
  - [ ] 13 filter implementations
  - [ ] Convolution kernels (blur, sharpen, edge detect)
  - [ ] Morphological operations (dilate, erode)
  - [ ] FKFilterCS class generation

- [ ] **Adjustment Shader** (`shaders/layer_adjustment.kn`)
  - [ ] 9 adjustment implementations
  - [ ] Color space conversions (RGB↔HSV↔HSL)
  - [ ] Levels, curves, brightness/contrast
  - [ ] FKAdjustmentCS class generation

- [ ] **Math Operations Shader** (`shaders/math_operations.kn`)
  - [ ] Add, Multiply, Lerp operations
  - [ ] FKMathOperationCS class generation

- [ ] **PBR Generator Shaders** (`shaders/pbr_generator.kn`)
  - [ ] GradientCS (Pass 1)
  - [ ] HeightIntegrationCS (Pass 2)
  - [ ] FinalPBRCS (Pass 3)
  - [ ] Legacy MainCS (single-pass)

- [ ] **Seamless & Packing Shaders** (`shaders/seamless_packing.kn`)
  - [ ] SeamlessCS (3 tiling modes)
  - [ ] PackORMCS (channel packing)

---

### 23.3 Preset Shaders (Optional, High Value)

- [ ] **Metal Preset Shaders**
  - [ ] MetalAnisotropicSpecular (`shaders/presets/metal_anisotropic.kn`)
  - [ ] MetalFresnelRim (`shaders/presets/metal_fresnel_rim.kn`)

- [ ] **Glossy Preset Shaders**
  - [ ] GlossyClearCoat (`shaders/presets/glossy_clear_coat.kn`)
  - [ ] GlossySubsurface (`shaders/presets/glossy_subsurface.kn`)
  - [ ] GlossyDualLobe (`shaders/presets/glossy_dual_lobe.kn`)

- [ ] **Toon Preset Shaders**
  - [ ] ToonCelShading (`shaders/presets/toon_cel_shading.kn`)
  - [ ] ToonSpecular (`shaders/presets/toon_specular.kn`)
  - [ ] ToonRimLight (`shaders/presets/toon_rim_light.kn`)
  - [ ] ToonOutlineDetection (`shaders/presets/toon_outline.kn`)
  - [ ] ToonConfigurableBands (`shaders/presets/toon_bands.kn`)

- [ ] **Shared Utility Shaders**
  - [ ] MaterializeFresnelSchlick (`shaders/presets/fresnel_schlick.kn`)
  - [ ] MaterializeGGXDistribution (`shaders/presets/ggx_distribution.kn`)
  - [ ] MaterializeSmithVisibility (`shaders/presets/smith_visibility.kn`)

---

### 23.4 Editor UI (Optional, Very High Value)

- [ ] **Main Editor** (`editor/materialize_editor.kn`)
  - [ ] Layer list widget (drag-drop, reorder)
  - [ ] Parameter panel (Details customization)
  - [ ] Preview viewport (real-time)
  - [ ] Preset selector dropdown
  - [ ] Generate button

- [ ] **Batch Processor** (`editor/batch_window.kn`)
  - [ ] Multi-texture selection
  - [ ] Batch parameter configuration
  - [ ] Progress bar
  - [ ] Result summary

- [ ] **Asset Actions** (`editor/asset_actions.kn`)
  - [ ] Right-click menu integration
  - [ ] "Generate PBR Maps" action
  - [ ] "Open in Materialize" action

- [ ] **Toolbar Extension** (`editor/toolbar_extension.kn`)
  - [ ] Toolbar button
  - [ ] Quick access to editor

---


## 24. Critical Implementation Notes

### 24.1 Pixel Format Consistency

**CRITICAL:** All layer operations use `PF_B8G8R8A8` for compatibility with blend shader.

```cpp
// All output textures use BGRA8
OutResult.BaseColor = CreateTransientTexture(Width, Height, PF_B8G8R8A8, true);
OutResult.Normal    = CreateTransientTexture(Width, Height, PF_B8G8R8A8, false);
OutResult.Roughness = CreateTransientTexture(Width, Height, PF_B8G8R8A8, false);
// ... all channels use PF_B8G8R8A8
```

**Why BGRA8?**
- Blend shader writes `RWTexture2D<float4>` (RGBA)
- UE5 default format is BGRA8 (hardware-native on most GPUs)
- Scalar channels (Roughness, Metallic, etc.) store value in R channel

**Mistake to Avoid:** Using `PF_R32_FLOAT` for scalar channels breaks blend shader (format mismatch).

---

### 24.2 Layer Ordering Convention

**CRITICAL:** Layers are stored in **bottom-to-top order**.

```
Index 0 = Bottom layer (rendered first)
Index 1 = Second layer (blended on top of index 0)
Index 2 = Third layer (blended on top of accumulated result)
...
Index N-1 = Top layer (rendered last)
```

**Why Bottom-to-Top?**
- Matches Photoshop convention
- Natural for alpha compositing (each layer blends onto accumulated result)
- Intuitive for users (bottom layers are "behind" top layers)

**Evaluation Order:**
```cpp
for (int32 i = 0; i < VisibleIndices.Num(); ++i)
{
    int32 LayerIndex = VisibleIndices[i];  // Already in bottom-to-top order
    // Evaluate and blend layer
}
```

**Mistake to Avoid:** Reversing layer order breaks alpha compositing (top layers would be occluded by bottom layers).

---

### 24.3 Shader Directory Mapping Guard

**CRITICAL:** Shader directory must be registered exactly once.

```cpp
static bool bShaderDirRegistered = false;
if (!bShaderDirRegistered)
{
    bShaderDirRegistered = true;
    AddShaderSourceDirectoryMapping(TEXT("/Plugin/Materialize"), PluginShaderDir);
}
```

**Why Static Guard?**
- Hot reload calls `StartupModule()` multiple times
- Double-registration causes assertion failure: `"Virtual shader directory already mapped"`
- Static variable persists across module reloads

**KAIN Implementation:**

KAIN backend automatically handles shader directory mapping with duplicate protection. No manual guard needed.

---

### 24.4 RDG Resource Lifetime

**CRITICAL:** RDG resources must not escape the lambda scope.

```cpp
// CORRECT
ENQUEUE_RENDER_COMMAND(Name)(
    [Result](FRHICommandListImmediate& RHICmdList)
    {
        FRDGBuilder GraphBuilder(RHICmdList);
        FRDGTextureRef Texture = GraphBuilder.CreateTexture(...);
        // Use texture
        GraphBuilder.Execute();  // Texture destroyed here
    }
);

// INCORRECT - CRASH
FRDGTextureRef GlobalTexture;
ENQUEUE_RENDER_COMMAND(Name)(
    [&GlobalTexture](FRHICommandListImmediate& RHICmdList)
    {
        FRDGBuilder GraphBuilder(RHICmdList);
        GlobalTexture = GraphBuilder.CreateTexture(...);  // DANGLING POINTER
        GraphBuilder.Execute();
    }
);
// GlobalTexture is now invalid - accessing it will crash
```

**Rule:** RDG resources are scope-local. Use `AddCopyTexturePass()` to copy to external textures.

---

### 24.5 Transient Texture Configuration

**CRITICAL:** Transient textures must be configured correctly for GPU use.

```cpp
UTexture2D* Tex = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
if (Tex)
{
    Tex->SRGB = bSRGB;              // MUST set before UpdateResource()
    Tex->Filter = TF_Bilinear;      // Filtering mode
    Tex->AddressX = TA_Wrap;        // Wrap mode X
    Tex->AddressY = TA_Wrap;        // Wrap mode Y
    Tex->UpdateResource();          // MUST call to initialize RHI resource
}
```

**Common Mistakes:**
1. Forgetting `UpdateResource()` → RHI resource is null → GPU crash
2. Wrong SRGB setting → Color space mismatch → incorrect colors
3. Wrong filter mode → Aliasing or blurring artifacts

---

