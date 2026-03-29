# Materialize Core Architecture

> **Complete type system, layer compositor, and preset system documentation**

## Overview

Materialize is a GPU-accelerated PBR map generation system with a Photoshop-style layer compositor. It transforms single source textures into complete PBR material sets (Normal, Roughness, Metallic, AO, Height, Emissive, ORM).

**Core Components:**
- **Type System** — 4 enums, 8 structs defining parameters, layers, and results
- **Layer System** — 8 layer types, 20 blend modes, GPU-accelerated compositor
- **Preset System** — 23+ material presets across 5 categories
- **Engine** — GPU-accelerated PBR generation with CPU fallback

---

## Type System

### Enums

#### EMaterializeCategory
Material category for organizing presets.

```cpp
enum class EMaterializeCategory : uint8
{
    Organic,    // Skin, leather, bark, flesh
    Rubber,     // Rubber, latex, tire, plastic, gasket
    Ground,     // Mud, rock, concrete, snow, asphalt
    Fabric,     // Denim, silk, wool, canvas, velvet
    Metal,      // Iron, gold, aluminum, copper, sci-fi panels
    Plastic,    // Glossy, matte, bakelite, PVC
    Paper,      // Cardboard, clean paper, parchment
    Custom      // User-defined
};
```

**KAIN Mapping:**
```kain
enum MaterialCategory:
    Organic
    Rubber
    Ground
    Fabric
    Metal
    Plastic
    Paper
    Custom
```


#### EKSeamlessMode
Seamless tiling algorithm selection.

```cpp
enum class EKSeamlessMode : uint8
{
    None,        // No tiling
    CrossBlend,  // Cross-fade edges (default)
    MirrorBlend, // Mirror and blend
    Histogram    // Histogram matching for color continuity
};
```

**KAIN Mapping:**
```kain
enum SeamlessMode:
    None
    CrossBlend
    MirrorBlend
    Histogram
```

---

#### EKLayerBlendMode
Photoshop-style blend modes for layer compositing (20 modes).

```cpp
enum class EKLayerBlendMode : uint8
{
    Normal, Multiply, Screen, Overlay, SoftLight, HardLight,
    Add, Subtract, Difference, Exclusion,
    Darken, Lighten, ColorDodge, ColorBurn,
    LinearDodge, LinearBurn, VividLight, LinearLight,
    PinLight, HardMix
};
```

**KAIN Mapping:**
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


#### EKLayerType
Layer type classification (8 types).

```cpp
enum class EKLayerType : uint8
{
    Base,        // Foundation layer linked to FMaterializeParams
    Image,       // Static texture input
    Procedural,  // Generated noise/patterns (15 types)
    Fill,        // Solid color/value
    Adjustment,  // HSV, Levels, Curves, Brightness/Contrast
    Filter,      // Blur, Sharpen, Edge Detect, Emboss
    Generator,   // AO, Curvature, Position, WorldNormal (mesh-derived)
    Folder       // Group/Folder container
};
```

**KAIN Mapping:**
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

#### EKLayerOutputChannel (Bitflags)
Output channel targeting for layers.

```cpp
enum class EKLayerOutputChannel : uint8
{
    None        = 0,
    BaseColor   = 1 << 0,
    Normal      = 1 << 1,
    Roughness   = 1 << 2,
    Metallic    = 1 << 3,
    Height      = 1 << 4,
    AO          = 1 << 5,
    Emissive    = 1 << 6,
    Mask        = 1 << 7,
    All         = 0xFF
};
ENUM_CLASS_FLAGS(EKLayerOutputChannel);
```

**KAIN Mapping:**
```kain
@bitflags
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


#### EKProceduralNoiseType
Procedural noise/pattern generators (15 types).

```cpp
enum class EKProceduralNoiseType : uint8
{
    Perlin, Simplex, Worley, FBM, Turbulence, Cellular,
    Gradient, Checker, Brick, Herringbone, Hexagon,
    Scratches, Grunge, Rust, Dust
};
```

**KAIN Mapping:**
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
```

---

#### EKFilterType
Image filter operations (13 types).

```cpp
enum class EKFilterType : uint8
{
    Blur, GaussianBlur, Sharpen, EdgeDetect, Emboss,
    HighPass, LowPass, Median, Dilate, Erode,
    Invert, Normalize, AutoLevels
};
```

**KAIN Mapping:**
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
```


#### EKAdjustmentType
Color/tone adjustment operations (9 types).

```cpp
enum class EKAdjustmentType : uint8
{
    Levels, Curves, HSV, Brightness, ColorBalance,
    Vibrance, Threshold, Posterize, Gradient
};
```

**KAIN Mapping:**
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
```

---

#### EKGeneratorType
Mesh-derived texture generators (8 types).

```cpp
enum class EKGeneratorType : uint8
{
    AmbientOcclusion, Curvature, Position, WorldNormal,
    Thickness, EdgeWear, Dirt, LightMap
};
```

**KAIN Mapping:**
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


### Structs

#### FMaterializeParams
PBR generation parameters (40+ fields across 9 categories).

```cpp
struct FMaterializeParams
{
    // Normal Map (1 field)
    float NormalStrength = 1.0f;  // [0.0, 2.0]
    
    // Roughness (5 fields)
    float RoughnessBase = 0.7f;           // [0.0, 1.0]
    float RoughnessContrast = 1.0f;       // [0.0, 3.0]
    float RoughnessBrightness = 0.0f;     // [-128, 128]
    bool bRoughnessInvert = true;
    float VarianceWeight = 0.5f;          // [0.0, 1.0]
    
    // Metallic (4 fields)
    float MetallicBase = 0.0f;            // [0.0, 1.0]
    float MetallicContrast = 1.0f;        // [0.0, 3.0]
    float MetallicBias = 0.0f;            // [-128, 128]
    float MetallicSensitivity = 2.0f;     // [0.0, 5.0]
    
    // Ambient Occlusion (1 field)
    float AOIntensity = 1.0f;             // [0.0, 2.0]
    
    // Height (1 field)
    float HeightContrast = 1.0f;          // [0.0, 3.0]
    
    // Weathering (6 fields)
    float EdgeWear = 0.0f;                // [0.0, 1.0]
    float CavityDirt = 0.0f;              // [0.0, 1.0]
    float Dust = 0.0f;                    // [0.0, 1.0]
    float Grunge = 0.0f;                  // [0.0, 1.0]
    float Scratches = 0.0f;               // [0.0, 1.0]
    float Noise = 0.0f;                   // [0.0, 1.0]
    
    // Special Effects (4 fields)
    float BioDetail = 0.0f;               // [0.0, 1.0] - Organic detail
    float BioFrequency = 1.0f;            // [0.1, 5.0]
    float CyberDetail = 0.0f;             // [0.0, 1.0] - Tech patterns
    float CyberScale = 1.0f;              // [0.01, 1.0]
    
    // Emissive (2 fields)
    float EmissiveThreshold = 0.0f;       // [0.0, 1.0]
    float EmissiveColorBoost = 1.0f;      // [0.0, 3.0]
    
    // Processing (4 fields)
    bool bMakeSeamless = false;
    EKSeamlessMode SeamlessMode = EKSeamlessMode::CrossBlend;
    float SeamlessBlendWidth = 0.25f;     // [0.1, 0.5]
    float Gamma = 1.0f;                   // [0.5, 1.5]
    float Vignette = 0.0f;                // [0.0, 1.0]
    
    // Output (2 fields)
    bool bPackORM = true;
    int32 OutputResolution = 0;           // 0 = match input, [64, 8192]
    
    // Advanced (7 fields)
    int32 HeightIterations = 24;          // [4, 64]
    bool bUseMultiPassHeight = true;
    int32 NormalOctaves = 3;              // [1, 6]
    float NormalSigmaBase = 1.0f;         // [0.5, 3.0]
    float NormalAnisotropy = 1.0f;        // [0.5, 2.0]
    float AORadius = 4.0f;                // [1.0, 32.0]
    float AOBias = 0.0f;                  // [-1.0, 1.0]
    float AOContrast = 1.0f;              // [0.1, 3.0]
    bool bAdvancedNormal = false;
    bool bAdvancedAO = false;
};
```


**KAIN Mapping:**
```kain
struct MaterializeParams:
    # Normal
    normal_strength: Float = 1.0
    
    # Roughness
    roughness_base: Float = 0.7
    roughness_contrast: Float = 1.0
    roughness_brightness: Float = 0.0
    roughness_invert: Bool = true
    variance_weight: Float = 0.5
    
    # Metallic
    metallic_base: Float = 0.0
    metallic_contrast: Float = 1.0
    metallic_bias: Float = 0.0
    metallic_sensitivity: Float = 2.0
    
    # AO
    ao_intensity: Float = 1.0
    
    # Height
    height_contrast: Float = 1.0
    
    # Weathering
    edge_wear: Float = 0.0
    cavity_dirt: Float = 0.0
    dust: Float = 0.0
    grunge: Float = 0.0
    scratches: Float = 0.0
    noise: Float = 0.0
    
    # Special Effects
    bio_detail: Float = 0.0
    bio_frequency: Float = 1.0
    cyber_detail: Float = 0.0
    cyber_scale: Float = 1.0
    
    # Emissive
    emissive_threshold: Float = 0.0
    emissive_color_boost: Float = 1.0
    
    # Processing
    make_seamless: Bool = false
    seamless_mode: SeamlessMode = SeamlessMode.CrossBlend
    seamless_blend_width: Float = 0.25
    gamma: Float = 1.0
    vignette: Float = 0.0
    
    # Output
    pack_orm: Bool = true
    output_resolution: Int = 0
    
    # Advanced
    height_iterations: Int = 24
    use_multi_pass_height: Bool = true
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

#### FMaterializePreset
A single material preset with ID, name, category, and parameters.

```cpp
struct FMaterializePreset
{
    FName Id;                          // Unique identifier (e.g., "skin_basic")
    FText DisplayName;                 // UI display name (e.g., "Basic Skin")
    EMaterializeCategory Category;     // Preset category
    FMaterializeParams Params;         // Generation parameters
};
```

**KAIN Mapping:**
```kain
struct MaterializePreset:
    id: String
    display_name: String
    category: MaterialCategory
    params: MaterializeParams
```

---

#### FMaterializeResult
Result of PBR generation (8 textures + material + timing).

```cpp
struct FMaterializeResult
{
    TObjectPtr<UTexture2D> LayerBaseColor;  // From layer stack (may be null)
    TObjectPtr<UTexture2D> Normal;
    TObjectPtr<UTexture2D> Roughness;
    TObjectPtr<UTexture2D> Metallic;
    TObjectPtr<UTexture2D> AO;
    TObjectPtr<UTexture2D> Height;
    TObjectPtr<UTexture2D> Emissive;
    TObjectPtr<UTexture2D> ORM;             // Packed: R=AO, G=Roughness, B=Metallic
    TObjectPtr<UMaterialInstanceDynamic> Material;
    float GenerationTimeMs;
    
    bool IsValid() const { return Normal != nullptr && Roughness != nullptr; }
};
```

**KAIN Mapping:**
```kain
struct MaterializeResult:
    layer_base_color: Texture2D?
    normal: Texture2D?
    roughness: Texture2D?
    metallic: Texture2D?
    ao: Texture2D?
    height: Texture2D?
    emissive: Texture2D?
    orm: Texture2D?
    material: MaterialInstanceDynamic?
    generation_time_ms: Float
    
    fn is_valid() -> Bool:
        return normal != null and roughness != null
```


---

#### FMaterializeMasterPreset
Master material preset descriptor with feature flags and default parameters.

```cpp
struct FMaterializeMasterPreset
{
    FName PresetId;                                    // Unique identifier
    FText DisplayName;                                 // UI display name
    FText Description;                                 // Tooltip description
    FSoftObjectPath MasterMaterialPath;                // Path to master material asset
    TSoftObjectPtr<UTexture2D> PreviewThumbnail;       // Preview thumbnail
    TMap<FName, float> DefaultScalarParams;            // Default scalar parameters
    TMap<FName, FLinearColor> DefaultVectorParams;     // Default vector parameters
    
    // Feature flags
    bool bSupportsAnisotropy = false;
    bool bSupportsClearCoat = false;
    bool bSupportsSubsurface = false;
    bool bSupportsToonShading = false;
};
```

**KAIN Mapping:**
```kain
struct MasterPreset:
    preset_id: String
    display_name: String
    description: String
    master_material_path: String
    preview_thumbnail: Texture2D?
    default_scalar_params: Map<String, Float>
    default_vector_params: Map<String, Vec4>
    supports_anisotropy: Bool = false
    supports_clear_coat: Bool = false
    supports_subsurface: Bool = false
    supports_toon_shading: Bool = false
```

---


## Layer System

### FKLayer
Core layer structure with type-specific parameters.

```cpp
struct FKLayer
{
    // Identity
    FName Name;
    FGuid Id;
    
    // Type
    EKLayerType LayerType;
    
    // Blending
    EKLayerBlendMode BlendMode = EKLayerBlendMode::Normal;
    float Opacity = 1.0f;  // [0.0, 1.0]
    
    // Output Channels (bitflags)
    int32 OutputChannels = static_cast<int32>(EKLayerOutputChannel::All);
    
    // Visibility
    bool bEnabled = true;
    bool bLocked = false;
    bool bSolo = false;
    
    // Mask
    bool bHasMask = false;
    TObjectPtr<UTexture2D> MaskTexture;
    bool bInvertMask = false;
    
    // Type-Specific Data (conditional on LayerType)
    TObjectPtr<UTexture2D> ImageTexture;           // Image layer
    FLinearColor FillColor;                        // Fill layer (color)
    float FillValue = 1.0f;                        // Fill layer (grayscale)
    FKProceduralParams ProceduralParams;           // Procedural layer
    FKFilterParams FilterParams;                   // Filter layer
    FKAdjustmentParams AdjustmentParams;           // Adjustment layer
    int32 SourceLayerIndex = INDEX_NONE;           // Filter/Adjustment source
    TObjectPtr<UTexture2D> SourceOverride;         // Filter/Adjustment override
    EKGeneratorType GeneratorType;                 // Generator layer
    bool bFolderExpanded = true;                   // Folder layer
    int32 ParentIndex = INDEX_NONE;                // Folder hierarchy
    
    // State
    bool bDirty = true;
    TObjectPtr<UTexture2D> CachedOutput;           // Transient cache
};
```

**KAIN Mapping:**
```kain
struct Layer:
    # Identity
    name: String
    id: String
    
    # Type
    layer_type: LayerType
    
    # Blending
    blend_mode: LayerBlendMode = LayerBlendMode.Normal
    opacity: Float = 1.0
    
    # Output Channels
    output_channels: Int = 255  # All channels
    
    # Visibility
    enabled: Bool = true
    locked: Bool = false
    solo: Bool = false
    
    # Mask
    has_mask: Bool = false
    mask_texture: Texture2D?
    invert_mask: Bool = false
    
    # Type-Specific Data
    image_texture: Texture2D?
    fill_color: Vec4
    fill_value: Float = 1.0
    procedural_params: ProceduralParams
    filter_params: FilterParams
    adjustment_params: AdjustmentParams
    source_layer_index: Int = -1
    source_override: Texture2D?
    generator_type: GeneratorType
    folder_expanded: Bool = true
    parent_index: Int = -1
    
    # State
    dirty: Bool = true
    @transient
    cached_output: Texture2D?
```


---

### FKProceduralParams
Procedural noise generation parameters.

```cpp
struct FKProceduralParams
{
    EKProceduralNoiseType NoiseType = EKProceduralNoiseType::Perlin;
    float Scale = 1.0f;           // [0.01, 100.0]
    int32 Octaves = 4;            // [1, 16]
    float Persistence = 0.5f;     // [0.0, 1.0]
    float Lacunarity = 2.0f;      // [1.0, 4.0]
    FVector2D Offset = FVector2D::ZeroVector;
    int32 Seed = 0;
    bool bSeamless = false;
    float Time = 0.0f;
};
```

**KAIN Mapping:**
```kain
struct ProceduralParams:
    noise_type: ProceduralNoiseType = ProceduralNoiseType.Perlin
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

### FKFilterParams
Image filter parameters.

```cpp
struct FKFilterParams
{
    EKFilterType FilterType = EKFilterType::Blur;
    float Intensity = 1.0f;       // [0.0, 100.0]
    int32 KernelSize = 3;         // [1, 32]
    float Threshold = 0.0f;       // [0.0, 10.0]
};
```

**KAIN Mapping:**
```kain
struct FilterParams:
    filter_type: FilterType = FilterType.Blur
    intensity: Float = 1.0
    kernel_size: Int = 3
    threshold: Float = 0.0
```


---

### FKAdjustmentParams
Color/tone adjustment parameters.

```cpp
struct FKAdjustmentParams
{
    EKAdjustmentType AdjustmentType = EKAdjustmentType::Levels;
    
    // Levels
    float InputBlack = 0.0f;      // [0.0, 1.0]
    float InputWhite = 1.0f;      // [0.0, 1.0]
    float Gamma = 1.0f;           // [0.1, 9.9]
    float OutputBlack = 0.0f;     // [0.0, 1.0]
    float OutputWhite = 1.0f;     // [0.0, 1.0]
    
    // HSV
    float HueShift = 0.0f;        // [-180.0, 180.0]
    float SaturationAdjust = 0.0f;// [-1.0, 1.0]
    float ValueAdjust = 0.0f;     // [-1.0, 1.0]
    
    // Brightness/Contrast
    float Brightness = 0.0f;      // [-1.0, 1.0]
    float Contrast = 0.0f;        // [-1.0, 1.0]
};
```

**KAIN Mapping:**
```kain
struct AdjustmentParams:
    adjustment_type: AdjustmentType = AdjustmentType.Levels
    
    # Levels
    input_black: Float = 0.0
    input_white: Float = 1.0
    gamma: Float = 1.0
    output_black: Float = 0.0
    output_white: Float = 1.0
    
    # HSV
    hue_shift: Float = 0.0
    saturation_adjust: Float = 0.0
    value_adjust: Float = 0.0
    
    # Brightness/Contrast
    brightness: Float = 0.0
    contrast: Float = 0.0
```


---

### FKLayerStack
Complete layer stack with versioning and management methods.

```cpp
struct FKLayerStack
{
    int32 Version = EKLayerStackVersion::Latest;  // Serialization version
    TArray<FKLayer> Layers;                        // Bottom to top
    int32 Width = 1024;
    int32 Height = 1024;
    int32 SelectedLayerIndex = INDEX_NONE;
    
    // Methods
    bool MigrateFromOldVersion();
    int32 AddLayer(const FKLayer& Layer);
    int32 InsertLayer(int32 Index, const FKLayer& Layer);
    bool RemoveLayer(int32 Index);
    bool MoveLayer(int32 FromIndex, int32 ToIndex);
    int32 DuplicateLayer(int32 Index);
    void MarkDirty(int32 Index);
    void MarkAllDirty();
    void ClearDirtyFlags();
    TArray<int32> GetVisibleLayerIndices() const;
    int32 FindLayerByGuid(const FGuid& Guid) const;
    int32 FindLayerByName(FName Name) const;
    
    // Factory methods
    static FKLayer CreateImageLayer(FName Name, UTexture2D* Texture);
    static FKLayer CreateFillLayer(FName Name, FLinearColor Color);
    static FKLayer CreateProceduralLayer(FName Name, EKProceduralNoiseType NoiseType);
    static FKLayer CreateFilterLayer(FName Name, EKFilterType FilterType);
    static FKLayer CreateAdjustmentLayer(FName Name, EKAdjustmentType AdjustmentType);
    static FKLayer CreateFolderLayer(FName Name);
};
```

**Versioning System:**
```cpp
namespace EKLayerStackVersion
{
    enum Type : int32
    {
        Initial        = 0,  // Original layout
        AddedSoloFlag  = 1,  // bSolo added
        AddedLockFlag  = 2,  // bLocked added
        AddedDirtyFlag = 3,  // bDirty / CachedOutput added
        Latest = 3
    };
}
```

**KAIN Mapping:**
```kain
struct LayerStack:
    version: Int = 3
    layers: Array<Layer>
    width: Int = 1024
    height: Int = 1024
    selected_layer_index: Int = -1
    
    fn migrate_from_old_version() -> Bool
    fn add_layer(layer: Layer) -> Int
    fn insert_layer(index: Int, layer: Layer) -> Int
    fn remove_layer(index: Int) -> Bool
    fn move_layer(from_index: Int, to_index: Int) -> Bool
    fn duplicate_layer(index: Int) -> Int
    fn mark_dirty(index: Int)
    fn mark_all_dirty()
    fn clear_dirty_flags()
    fn get_visible_layer_indices() -> Array<Int>
    fn find_layer_by_guid(guid: String) -> Int
    fn find_layer_by_name(name: String) -> Int
```


---

### FKLayerEvalResult
Layer stack evaluation result (7 textures + timing).

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
    float EvaluationTimeMs = 0.0f;
    
    bool IsValid() const { return BaseColor != nullptr; }
};
```

**KAIN Mapping:**
```kain
struct LayerEvalResult:
    base_color: Texture2D?
    normal: Texture2D?
    roughness: Texture2D?
    metallic: Texture2D?
    height: Texture2D?
    ao: Texture2D?
    emissive: Texture2D?
    evaluation_time_ms: Float = 0.0
    
    fn is_valid() -> Bool:
        return base_color != null
```

---

## Layer System Design

### Architecture

The layer system is a **Photoshop-style compositor** with GPU acceleration:

1. **Bottom-to-Top Evaluation** — Layers are stored bottom-to-top in the array
2. **Dirty Tracking** — Each layer has a `bDirty` flag; when a layer changes, all layers above are marked dirty
3. **Cached Output** — Each layer caches its output texture to avoid re-evaluation
4. **Multi-Channel** — Each layer can target specific output channels (BaseColor, Normal, Roughness, etc.)
5. **Visibility System** — Layers can be enabled/disabled, locked, or solo'd
6. **Masking** — Each layer can have an optional mask texture with invert support


### Layer Types

| Type | Purpose | Data Fields | Example Use Case |
|------|---------|-------------|------------------|
| **Base** | Foundation layer linked to FMaterializeParams | None (uses stack params) | Initial PBR generation from source |
| **Image** | Static texture input | `ImageTexture` | Photo overlay, decal |
| **Procedural** | Generated noise/patterns | `ProceduralParams` (15 noise types) | Grunge, scratches, rust |
| **Fill** | Solid color/value | `FillColor`, `FillValue` | Solid color overlay, mask |
| **Adjustment** | Color/tone adjustment | `AdjustmentParams` (9 types) | HSV shift, levels, curves |
| **Filter** | Image processing | `FilterParams` (13 types) | Blur, sharpen, edge detect |
| **Generator** | Mesh-derived textures | `GeneratorType` (8 types) | AO, curvature, position |
| **Folder** | Group/container | `bFolderExpanded`, `ParentIndex` | Layer organization |

### Blend Modes

**Photoshop-equivalent blend modes** (20 total):

| Category | Modes |
|----------|-------|
| **Basic** | Normal, Multiply, Screen, Overlay |
| **Light** | SoftLight, HardLight, VividLight, LinearLight, PinLight |
| **Math** | Add, Subtract, Difference, Exclusion |
| **Darken** | Darken, ColorBurn, LinearBurn |
| **Lighten** | Lighten, ColorDodge, LinearDodge |
| **Special** | HardMix |

### Visibility System

**Layer visibility is determined by:**
1. `bEnabled` — Layer is active
2. `bLocked` — Layer is locked (Base layers can still render when locked)
3. `bSolo` — Solo mode (only solo layers render)

**Solo Logic:**
- If ANY layer has `bSolo = true` AND `bEnabled = true`, ONLY solo layers render
- Disabled layers never render, even if solo'd
- Locked layers don't render (except Base type)

**Implementation:**
```cpp
TArray<int32> GetVisibleLayerIndices() const
{
    TArray<int32> Result;
    bool bHasSolo = false;
    
    // Check for any enabled solo layers
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
        if (!Layer.bEnabled) continue;
        if (Layer.bLocked && Layer.LayerType != EKLayerType::Base) continue;
        if (bHasSolo && !Layer.bSolo) continue;
        Result.Add(i);
    }
    
    return Result;
}
```


### Dirty Tracking & Caching

**Dirty tracking optimizes re-evaluation:**

1. When a layer changes, it's marked dirty
2. All layers ABOVE it are also marked dirty (they depend on layers below)
3. Dirty layers re-evaluate on next stack evaluation
4. Clean layers use cached output

**Implementation:**
```cpp
void MarkDirty(int32 Index)
{
    if (Layers.IsValidIndex(Index))
    {
        Layers[Index].bDirty = true;
        // Mark all layers above as dirty too
        for (int32 i = Index + 1; i < Layers.Num(); ++i)
        {
            Layers[i].bDirty = true;
        }
    }
}
```

**Cache invalidation triggers:**
- Layer parameter change
- Layer reordering
- Layer enable/disable
- Mask change
- Source texture change

---

## Layer Evaluator (UKLayerEvaluator)

GPU-accelerated layer compositor with CPU fallback.

### Core API

```cpp
class UKLayerEvaluator : public UObject
{
    // Evaluate entire stack
    static bool EvaluateStack(FKLayerStack& Stack, FKLayerEvalResult& OutResult, FString& OutError);
    
    // Evaluate single layer
    static UTexture2D* EvaluateSingleLayer(const FKLayer& Layer, int32 Width, int32 Height, FString& OutError);
    
    // Blend two textures
    static UTexture2D* BlendTextures(UTexture2D* Base, UTexture2D* Blend, 
        EKLayerBlendMode BlendMode, float Opacity,
        UTexture2D* Mask, bool bInvertMask, FString& OutError);
    
    // Generate procedural texture
    static UTexture2D* GenerateProceduralTexture(const FKProceduralParams& Params, 
        int32 Width, int32 Height, FString& OutError);
    
    // Apply filter
    static UTexture2D* ApplyFilter(UTexture2D* Source, const FKFilterParams& Params, FString& OutError);
    
    // Apply adjustment
    static UTexture2D* ApplyAdjustment(UTexture2D* Source, const FKAdjustmentParams& Params, FString& OutError);
    
    // Texture math operations
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


### Evaluation Pipeline

**Stack evaluation flow:**

```
1. ValidateLayerStack() — Check for errors
2. GetVisibleLayerIndices() — Filter by enabled/solo/locked
3. For each visible layer (bottom to top):
   a. Check if dirty
   b. If dirty:
      - EvaluateSingleLayer() → texture
      - Cache result in layer.CachedOutput
      - Clear dirty flag
   c. If clean:
      - Use layer.CachedOutput
   d. BlendTextures() with previous result
4. Split channels into separate textures
5. Return FKLayerEvalResult
```

**GPU Acceleration:**
- Blend modes implemented as compute shaders
- Procedural noise runs on GPU
- Filters use GPU convolution
- CPU fallback for unsupported operations

**Parameter Synchronization:**
```cpp
template<typename TShaderParameters>
static void SyncLayerParametersToGPU(TShaderParameters* Params, const FKLayer& Layer);

template<typename TShaderParameters>
static bool ValidateGPUParameters(const TShaderParameters* Params, const FKLayer& Layer, FString& OutError);
```

---

## Preset System

### FMaterializePresets (Static Registry)

**23 Built-in Presets** across 5 categories:

#### Organic (6 presets)
1. **skin_basic** — Basic Skin
2. **leather_worn** — Worn Leather
3. **alien_bio** — Alien Flesh
4. **bark** — Tree Bark
5. **zombie** — Zombie Skin
6. **dragon_scale** — Dragon Scale

#### Rubber/Synth (5 presets)
7. **rubber_matte** — Matte Rubber
8. **latex_shiny** — Shiny Latex
9. **tire_worn** — Worn Tire
10. **plastic_rough** — Rough Plastic
11. **gasket** — Gasket

#### Ground/Rock (5 presets)
12. **ground_wet** — Wet Mud
13. **rock_rough** — Rough Rock
14. **concrete** — Concrete
15. **snow** — Snow
16. **asphalt** — Asphalt

#### Fabric (5 presets)
17. **denim** — Denim
18. **silk** — Silk
19. **wool** — Wool
20. **canvas** — Canvas
21. **velvet** — Velvet

#### Metal (5 presets)
22. **iron_rusty** — Rusted Iron
23. **gold_dirty** — Aged Gold
24. **aluminum_brushed** — Brushed Aluminum
25. **scifi_panel** — Sci-Fi Panel
26. **copper** — Copper

#### Plastic (4 presets)
27. **plastic_glossy** — Glossy Plastic
28. **plastic_matte** — Matte Plastic
29. **bakelite** — Bakelite
30. **pvc** — PVC Pipe

#### Paper/Card (3 presets)
31. **cardboard** — Cardboard
32. **paper_clean** — Clean Paper
33. **parchment** — Old Parchment


### Preset API

```cpp
class FMaterializePresets
{
    static const TArray<FMaterializePreset>& GetAllPresets();
    static TArray<FMaterializePreset> GetPresetsByCategory(EMaterializeCategory Category);
    static const FMaterializePreset* GetPresetById(FName Id);
    static FMaterializeParams GetDefaultParams();
};
```

**KAIN Mapping:**
```kain
@blueprint
fn get_all_presets() -> Array<MaterializePreset>

@blueprint
fn get_presets_by_category(category: MaterialCategory) -> Array<MaterializePreset>

@blueprint
fn get_preset_by_id(id: String) -> MaterializePreset?

@blueprint
fn get_default_params() -> MaterializeParams
```

### Master Material Preset Registry

**FMaterializePresetRegistry** manages master material presets (4 built-in):

1. **Standard** — Standard PBR (default)
2. **Metal** — Enhanced reflections + anisotropy
3. **Glossy** — Clear coat + subsurface
4. **Toon** — Cel-shaded NPR

```cpp
class FMaterializePresetRegistry
{
    static void Initialize();
    static TArray<FMaterializeMasterPreset> GetAllPresets();
    static const FMaterializeMasterPreset* GetPreset(const FName& PresetId);
    static bool RegisterPreset(const FMaterializeMasterPreset& Preset);
    static const FMaterializeMasterPreset& GetDefaultPreset();
    static bool HasPreset(const FName& PresetId);
};
```

**Extensibility:**
- Built-in presets registered in `RegisterBuiltInPresets()`
- User presets scanned from `/Materialize/Content/Materials/Presets/` (not yet implemented)
- Runtime registration via `RegisterPreset()`


---

## Engine (UMaterializeEngine)

GPU-accelerated PBR map generation from single source texture.

### Core API

```cpp
class UMaterializeEngine : public UBlueprintFunctionLibrary
{
    // Generate all PBR maps
    static bool GeneratePBRMaps(
        UTexture2D* SourceTexture,
        const FMaterializeParams& Params,
        FMaterializeResult& OutResult
    );
    
    // Generate and save as assets
    static bool GenerateAndSavePBRMaps(
        UTexture2D* SourceTexture,
        const FMaterializeParams& Params,
        const FString& OutputPath,
        const FString& BaseName,
        FMaterializeResult& OutResult
    );
    
    // Individual map generators
    static UTexture2D* GenerateNormalMap(const TArray<FColor>& SourcePixels, int32 Width, int32 Height, float Strength, const FMaterializeParams& Params);
    static UTexture2D* GenerateRoughnessMap(const TArray<FColor>& SourcePixels, const TArray<float>& GrayBuffer, const TArray<float>& EdgeMagnitude, int32 Width, int32 Height, const FMaterializeParams& Params);
    static UTexture2D* GenerateMetallicMap(const TArray<FColor>& SourcePixels, const TArray<float>& GrayBuffer, const TArray<float>& EdgeMagnitude, int32 Width, int32 Height, const FMaterializeParams& Params);
    static UTexture2D* GenerateAOMap(const TArray<float>& GrayBuffer, int32 Width, int32 Height, const FMaterializeParams& Params);
    static UTexture2D* GenerateHeightMap(const TArray<float>& GrayBuffer, int32 Width, int32 Height, const FMaterializeParams& Params);
    static UTexture2D* GenerateEmissiveMap(const TArray<FColor>& SourcePixels, const TArray<float>& GrayBuffer, int32 Width, int32 Height, const FMaterializeParams& Params);
    static UTexture2D* PackORM(UTexture2D* AO, UTexture2D* Roughness, UTexture2D* Metallic);
};
```

### Generation Pipeline

```
Source Texture
    ↓
ReadTexturePixels() → TArray<FColor>
    ↓
CreateGrayscaleBuffer() → TArray<float>
    ↓
ComputeSobelEdges() → EdgeMagnitude, Dx, Dy
    ↓
Parallel Generation:
    ├─→ GenerateNormalMap() (Sobel filter)
    ├─→ GenerateRoughnessMap() (luminance + variance + edge)
    ├─→ GenerateMetallicMap() (luminance + edge + sensitivity)
    ├─→ GenerateAOMap() (multi-scale blur)
    ├─→ GenerateHeightMap() (iterative refinement)
    └─→ GenerateEmissiveMap() (threshold + color boost)
    ↓
PackORM() → R=AO, G=Roughness, B=Metallic
    ↓
CreateMaterialInstance() → UMaterialInstanceDynamic
    ↓
FMaterializeResult
```


### Algorithm Details

#### Normal Map Generation (Sobel Filter)
```
1. Compute Sobel gradients (Dx, Dy) from grayscale
2. Calculate normal vectors: N = normalize((-Dx, -Dy, 1.0 / Strength))
3. Optional: Multi-octave refinement (bAdvancedNormal)
4. Pack to RGB: (N.x * 0.5 + 0.5, N.y * 0.5 + 0.5, N.z)
```

#### Roughness Map Generation
```
1. Base roughness from luminance
2. Add variance weight (local standard deviation)
3. Add edge magnitude contribution
4. Apply contrast, brightness, invert
5. Clamp to [0, 1]
```

#### Metallic Map Generation
```
1. Detect high-luminance areas (potential metal)
2. Weight by edge sharpness (metals have sharp specular)
3. Apply sensitivity threshold
4. Apply contrast and bias
5. Clamp to [0, 1]
```

#### AO Map Generation
```
1. Multi-scale Gaussian blur (simulates light occlusion)
2. Invert (dark areas = occluded)
3. Apply radius, bias, contrast
4. Optional: Advanced AO with multiple passes
```

#### Height Map Generation
```
1. Start with grayscale as initial height
2. Iterative refinement (HeightIterations):
   - Compute gradients
   - Adjust heights to match gradients
   - Smooth
3. Optional: Multi-pass for better quality
4. Apply contrast
5. Normalize to [0, 1]
```

#### Emissive Map Generation
```
1. Threshold luminance (EmissiveThreshold)
2. Boost color (EmissiveColorBoost)
3. Preserve original color hue
4. Output RGB emissive
```

---

## KAIN Type Mapping Plan

### Core Types

| C++ Type | KAIN Type | Notes |
|----------|-----------|-------|
| `EMaterializeCategory` | `MaterialCategory` | Direct enum mapping |
| `EKSeamlessMode` | `SeamlessMode` | Direct enum mapping |
| `EKLayerBlendMode` | `LayerBlendMode` | Direct enum mapping |
| `EKLayerType` | `LayerType` | Direct enum mapping |
| `EKLayerOutputChannel` | `LayerOutputChannel` | Bitflags enum |
| `EKProceduralNoiseType` | `ProceduralNoiseType` | Direct enum mapping |
| `EKFilterType` | `FilterType` | Direct enum mapping |
| `EKAdjustmentType` | `AdjustmentType` | Direct enum mapping |
| `EKGeneratorType` | `GeneratorType` | Direct enum mapping |
| `FMaterializeParams` | `MaterializeParams` | Struct with 40+ fields |
| `FMaterializePreset` | `MaterializePreset` | Struct with 4 fields |
| `FMaterializeResult` | `MaterializeResult` | Struct with 10 fields |
| `FMaterializeMasterPreset` | `MasterPreset` | Struct with 11 fields |
| `FKLayer` | `Layer` | Struct with 25+ fields |
| `FKLayerStack` | `LayerStack` | Struct with methods |
| `FKLayerEvalResult` | `LayerEvalResult` | Struct with 8 fields |
| `FKProceduralParams` | `ProceduralParams` | Struct with 9 fields |
| `FKFilterParams` | `FilterParams` | Struct with 4 fields |
| `FKAdjustmentParams` | `AdjustmentParams` | Struct with 11 fields |


### Blueprint Function Library Mapping

**UMaterializeEngine → KAIN:**

```kain
@blueprint
fn generate_pbr_maps(source: Texture2D, params: MaterializeParams) -> MaterializeResult?

@blueprint
fn generate_and_save_pbr_maps(
    source: Texture2D,
    params: MaterializeParams,
    output_path: String,
    base_name: String
) -> MaterializeResult?
```

**UKLayerEvaluator → KAIN:**

```kain
@blueprint
fn evaluate_stack(stack: LayerStack) -> LayerEvalResult?

@blueprint
fn evaluate_single_layer(layer: Layer, width: Int, height: Int) -> Texture2D?

@blueprint
fn blend_textures(
    base: Texture2D,
    blend: Texture2D,
    blend_mode: LayerBlendMode,
    opacity: Float,
    mask: Texture2D?,
    invert_mask: Bool
) -> Texture2D?

@blueprint
fn generate_procedural_texture(params: ProceduralParams, width: Int, height: Int) -> Texture2D?

@blueprint
fn apply_filter(source: Texture2D, params: FilterParams) -> Texture2D?

@blueprint
fn apply_adjustment(source: Texture2D, params: AdjustmentParams) -> Texture2D?

@blueprint
fn add_textures(a: Texture2D, b: Texture2D) -> Texture2D?

@blueprint
fn multiply_textures(a: Texture2D, b: Texture2D) -> Texture2D?

@blueprint
fn lerp_textures(a: Texture2D, b: Texture2D, alpha: Float) -> Texture2D?
```

**FMaterializePresets → KAIN:**

```kain
@blueprint
fn get_all_materialize_presets() -> Array<MaterializePreset>

@blueprint
fn get_materialize_presets_by_category(category: MaterialCategory) -> Array<MaterializePreset>

@blueprint
fn get_materialize_preset_by_id(id: String) -> MaterializePreset?

@blueprint
fn get_default_materialize_params() -> MaterializeParams
```


---

## Preset Parameter Examples

### Organic Materials

**Basic Skin:**
```kain
let skin_params = MaterializeParams:
    normal_strength: 0.02
    roughness_contrast: 1.2
    roughness_brightness: 20.0
    roughness_invert: true
    metallic_contrast: 0.0
    metallic_bias: -100.0
    ao_intensity: 0.8
    bio_detail: 0.1
```

**Alien Flesh:**
```kain
let alien_params = MaterializeParams:
    normal_strength: 0.12
    roughness_contrast: 1.0
    roughness_brightness: -20.0
    metallic_contrast: 1.5
    metallic_bias: 20.0
    ao_intensity: 1.2
    bio_detail: 0.6
    bio_frequency: 0.3
    cavity_dirt: 0.3
    gamma: 1.1
```

### Metal Materials

**Rusted Iron:**
```kain
let iron_params = MaterializeParams:
    normal_strength: 0.1
    roughness_contrast: 2.0
    roughness_brightness: 40.0
    metallic_base: 0.8
    metallic_contrast: 1.0
    ao_intensity: 1.6
    cavity_dirt: 0.3
    scratches: 0.4
    gamma: 0.9
    vignette: 0.2
```

**Sci-Fi Panel:**
```kain
let scifi_params = MaterializeParams:
    normal_strength: 0.05
    roughness_base: 0.3
    roughness_contrast: 1.2
    roughness_brightness: -10.0
    metallic_base: 0.9
    metallic_contrast: 2.0
    metallic_bias: 40.0
    ao_intensity: 1.2
    edge_wear: 0.3
    cavity_dirt: 0.2
    cyber_detail: 0.35
    cyber_scale: 0.15
    scratches: 0.2
    vignette: 0.1
```


### Fabric Materials

**Denim:**
```kain
let denim_params = MaterializeParams:
    normal_strength: 0.06
    roughness_contrast: 1.5
    roughness_brightness: 40.0
    metallic_contrast: 0.0
    metallic_bias: -100.0
    ao_intensity: 1.2
    edge_wear: 0.1
    vignette: 0.1
```

**Silk:**
```kain
let silk_params = MaterializeParams:
    normal_strength: 0.02
    roughness_base: 0.2
    roughness_contrast: 0.5
    roughness_brightness: -20.0
    metallic_contrast: 1.0
    metallic_bias: -20.0
    ao_intensity: 0.6
```

### Ground Materials

**Wet Mud:**
```kain
let mud_params = MaterializeParams:
    normal_strength: 0.08
    roughness_base: 0.3
    roughness_contrast: 2.5
    roughness_brightness: -30.0
    metallic_contrast: 0.8
    metallic_bias: -60.0
    ao_intensity: 1.5
    bio_detail: 0.1
    bio_frequency: 0.5
    cavity_dirt: 0.3
    dust: 0.1
    gamma: 0.9
```

**Concrete:**
```kain
let concrete_params = MaterializeParams:
    normal_strength: 0.05
    roughness_contrast: 1.2
    roughness_brightness: 30.0
    metallic_contrast: 0.0
    metallic_bias: -100.0
    ao_intensity: 1.0
    edge_wear: 0.1
    cavity_dirt: 0.1
    dust: 0.1
    grunge: 0.1
    vignette: 0.1
```


---

## KAIN Implementation Strategy

### Module Structure

```kain
# FactoryPart2/plugins/Materialize/src/types.kn
enum MaterialCategory: ...
enum SeamlessMode: ...
enum LayerBlendMode: ...
enum LayerType: ...
enum LayerOutputChannel: ...
enum ProceduralNoiseType: ...
enum FilterType: ...
enum AdjustmentType: ...
enum GeneratorType: ...

struct MaterializeParams: ...
struct MaterializePreset: ...
struct MaterializeResult: ...
struct MasterPreset: ...
struct ProceduralParams: ...
struct FilterParams: ...
struct AdjustmentParams: ...
struct Layer: ...
struct LayerStack: ...
struct LayerEvalResult: ...
```

```kain
# FactoryPart2/plugins/Materialize/src/engine.kn
@blueprint
fn generate_pbr_maps(source: Texture2D, params: MaterializeParams) -> MaterializeResult?

@blueprint
fn generate_and_save_pbr_maps(
    source: Texture2D,
    params: MaterializeParams,
    output_path: String,
    base_name: String
) -> MaterializeResult?
```

```kain
# FactoryPart2/plugins/Materialize/src/layer_evaluator.kn
@blueprint
fn evaluate_stack(stack: LayerStack) -> LayerEvalResult?

@blueprint
fn evaluate_single_layer(layer: Layer, width: Int, height: Int) -> Texture2D?

@blueprint
fn blend_textures(
    base: Texture2D,
    blend: Texture2D,
    blend_mode: LayerBlendMode,
    opacity: Float,
    mask: Texture2D?,
    invert_mask: Bool
) -> Texture2D?
```

```kain
# FactoryPart2/plugins/Materialize/src/presets.kn
@blueprint
fn get_all_materialize_presets() -> Array<MaterializePreset>

@blueprint
fn get_materialize_presets_by_category(category: MaterialCategory) -> Array<MaterializePreset>

@blueprint
fn get_materialize_preset_by_id(id: String) -> MaterializePreset?
```


### Naming Conventions

**C++ → KAIN naming transformations:**

| C++ Pattern | KAIN Pattern | Example |
|-------------|--------------|---------|
| `EMaterializeCategory` | `MaterialCategory` | Remove E prefix |
| `EKSeamlessMode` | `SeamlessMode` | Remove EK prefix |
| `EKLayerBlendMode` | `LayerBlendMode` | Remove EK prefix |
| `FMaterializeParams` | `MaterializeParams` | Remove F prefix |
| `FKLayer` | `Layer` | Remove FK prefix |
| `FKLayerStack` | `LayerStack` | Remove FK prefix |
| `UMaterializeEngine` | Blueprint functions | Static methods → top-level functions |
| `UKLayerEvaluator` | Blueprint functions | Static methods → top-level functions |
| `TObjectPtr<UTexture2D>` | `Texture2D?` | Nullable texture reference |
| `TArray<T>` | `Array<T>` | Direct mapping |
| `TMap<K, V>` | `Map<K, V>` | Direct mapping |
| `FVector2D` | `Vec2` | Direct mapping |
| `FLinearColor` | `Vec4` | Direct mapping |
| `FGuid` | `String` | GUID as string |
| `FName` | `String` | Name as string |
| `FText` | `String` | Text as string |

### Field Naming Conventions

**C++ PascalCase → KAIN snake_case:**

| C++ Field | KAIN Field |
|-----------|------------|
| `NormalStrength` | `normal_strength` |
| `RoughnessBase` | `roughness_base` |
| `bRoughnessInvert` | `roughness_invert` |
| `MetallicSensitivity` | `metallic_sensitivity` |
| `AOIntensity` | `ao_intensity` |
| `EdgeWear` | `edge_wear` |
| `CavityDirt` | `cavity_dirt` |
| `BioDetail` | `bio_detail` |
| `BioFrequency` | `bio_frequency` |
| `CyberDetail` | `cyber_detail` |
| `CyberScale` | `cyber_scale` |
| `EmissiveThreshold` | `emissive_threshold` |
| `EmissiveColorBoost` | `emissive_color_boost` |
| `bMakeSeamless` | `make_seamless` |
| `SeamlessMode` | `seamless_mode` |
| `SeamlessBlendWidth` | `seamless_blend_width` |
| `bPackORM` | `pack_orm` |
| `OutputResolution` | `output_resolution` |
| `HeightIterations` | `height_iterations` |
| `bUseMultiPassHeight` | `use_multi_pass_height` |
| `NormalOctaves` | `normal_octaves` |
| `NormalSigmaBase` | `normal_sigma_base` |
| `NormalAnisotropy` | `normal_anisotropy` |
| `AORadius` | `ao_radius` |
| `AOBias` | `ao_bias` |
| `AOContrast` | `ao_contrast` |
| `bAdvancedNormal` | `advanced_normal` |
| `bAdvancedAO` | `advanced_ao` |


---

## Key Design Patterns

### 1. Type-Specific Union Pattern

`FKLayer` uses a **discriminated union pattern** where fields are conditionally used based on `LayerType`:

```cpp
// Only used when LayerType == Image
TObjectPtr<UTexture2D> ImageTexture;

// Only used when LayerType == Fill
FLinearColor FillColor;
float FillValue;

// Only used when LayerType == Procedural
FKProceduralParams ProceduralParams;

// Only used when LayerType == Filter
FKFilterParams FilterParams;

// Only used when LayerType == Adjustment
FKAdjustmentParams AdjustmentParams;

// Only used when LayerType == Generator
EKGeneratorType GeneratorType;

// Only used when LayerType == Folder
bool bFolderExpanded;
int32 ParentIndex;
```

**KAIN Implementation:**
Use optional fields with runtime checks:
```kain
struct Layer:
    layer_type: LayerType
    
    # Conditional fields (check layer_type before access)
    image_texture: Texture2D?
    fill_color: Vec4
    fill_value: Float
    procedural_params: ProceduralParams
    filter_params: FilterParams
    adjustment_params: AdjustmentParams
    generator_type: GeneratorType
    folder_expanded: Bool
    parent_index: Int
```

### 2. Bitflags for Multi-Channel Targeting

`EKLayerOutputChannel` uses bitflags to allow layers to target multiple channels:

```cpp
// Layer affects only Normal and Roughness
layer.OutputChannels = static_cast<int32>(EKLayerOutputChannel::Normal | EKLayerOutputChannel::Roughness);

// Layer affects all channels
layer.OutputChannels = static_cast<int32>(EKLayerOutputChannel::All);
```

**KAIN Implementation:**
```kain
@bitflags
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

# Usage
let layer = Layer:
    output_channels: LayerOutputChannel.Normal | LayerOutputChannel.Roughness
```


### 3. Versioned Serialization

`FKLayerStack` uses a version field for backward compatibility:

```cpp
namespace EKLayerStackVersion
{
    enum Type : int32
    {
        Initial        = 0,
        AddedSoloFlag  = 1,
        AddedLockFlag  = 2,
        AddedDirtyFlag = 3,
        Latest = 3
    };
}

bool FKLayerStack::MigrateFromOldVersion()
{
    if (Version >= EKLayerStackVersion::Latest) return false;
    
    if (Version < EKLayerStackVersion::AddedDirtyFlag)
    {
        MarkAllDirty();  // Old stacks need full re-evaluation
    }
    
    Version = EKLayerStackVersion::Latest;
    return true;
}
```

**KAIN Implementation:**
```kain
struct LayerStack:
    version: Int = 3
    
    fn migrate_from_old_version() -> Bool:
        if version >= 3:
            return false
        
        if version < 3:
            mark_all_dirty()
        
        version = 3
        return true
```

### 4. Factory Methods for Layer Creation

`FKLayerStack` provides static factory methods for common layer types:

```cpp
static FKLayer CreateImageLayer(FName Name, UTexture2D* Texture);
static FKLayer CreateFillLayer(FName Name, FLinearColor Color);
static FKLayer CreateProceduralLayer(FName Name, EKProceduralNoiseType NoiseType);
static FKLayer CreateFilterLayer(FName Name, EKFilterType FilterType);
static FKLayer CreateAdjustmentLayer(FName Name, EKAdjustmentType AdjustmentType);
static FKLayer CreateFolderLayer(FName Name);
```

**KAIN Implementation:**
```kain
struct LayerStack:
    @blueprint
    fn create_image_layer(name: String, texture: Texture2D) -> Layer
    
    @blueprint
    fn create_fill_layer(name: String, color: Vec4) -> Layer
    
    @blueprint
    fn create_procedural_layer(name: String, noise_type: ProceduralNoiseType) -> Layer
    
    @blueprint
    fn create_filter_layer(name: String, filter_type: FilterType) -> Layer
    
    @blueprint
    fn create_adjustment_layer(name: String, adjustment_type: AdjustmentType) -> Layer
    
    @blueprint
    fn create_folder_layer(name: String) -> Layer
```


### 5. Error Handling Pattern

All operations return success/failure with descriptive error messages:

```cpp
static bool EvaluateStack(FKLayerStack& Stack, FKLayerEvalResult& OutResult, FString& OutError);
static UTexture2D* BlendTextures(..., FString& OutError);
static bool ValidateLayerStack(const FKLayerStack& Stack, FString& OutError);
```

**KAIN Implementation:**
Use Result types or nullable returns:
```kain
fn evaluate_stack(stack: LayerStack) -> LayerEvalResult?
fn blend_textures(...) -> Texture2D?
fn validate_layer_stack(stack: LayerStack) -> Bool
```

### 6. Transient Cache Pattern

Layers cache their output to avoid redundant computation:

```cpp
UPROPERTY(Transient)
TObjectPtr<UTexture2D> CachedOutput;

bool bDirty = true;
```

**Evaluation logic:**
```cpp
if (Layer.bDirty)
{
    Layer.CachedOutput = EvaluateSingleLayer(Layer, Width, Height, Error);
    Layer.bDirty = false;
}
return Layer.CachedOutput;
```

**KAIN Implementation:**
```kain
struct Layer:
    dirty: Bool = true
    @transient
    cached_output: Texture2D?

fn evaluate_layer(layer: Layer, width: Int, height: Int) -> Texture2D?:
    if layer.dirty:
        layer.cached_output = compute_layer_output(layer, width, height)
        layer.dirty = false
    return layer.cached_output
```


---

## Data Flow Diagrams

### PBR Generation Flow

```
User Input
    ↓
Source Texture + FMaterializeParams
    ↓
UMaterializeEngine::GeneratePBRMaps()
    ↓
    ├─→ ReadTexturePixels() → TArray<FColor>
    ├─→ CreateGrayscaleBuffer() → TArray<float>
    └─→ ComputeSobelEdges() → EdgeMagnitude
    ↓
Parallel Map Generation:
    ├─→ GenerateNormalMap() → Normal.png
    ├─→ GenerateRoughnessMap() → Roughness.png
    ├─→ GenerateMetallicMap() → Metallic.png
    ├─→ GenerateAOMap() → AO.png
    ├─→ GenerateHeightMap() → Height.png
    └─→ GenerateEmissiveMap() → Emissive.png
    ↓
PackORM() → ORM.png (R=AO, G=Roughness, B=Metallic)
    ↓
CreateMaterialInstance() → UMaterialInstanceDynamic
    ↓
FMaterializeResult
```

### Layer Stack Evaluation Flow

```
FKLayerStack
    ↓
ValidateLayerStack()
    ↓
GetVisibleLayerIndices() → [0, 2, 5, 7]
    ↓
For each channel (BaseColor, Normal, Roughness, Metallic, Height, AO, Emissive):
    ↓
    Initialize accumulator texture
    ↓
    For each visible layer (bottom to top):
        ↓
        Check if layer targets this channel (OutputChannels bitflag)
        ↓
        If dirty:
            EvaluateSingleLayer() → texture
            Cache in layer.CachedOutput
            Clear dirty flag
        Else:
            Use layer.CachedOutput
        ↓
        BlendTextures(accumulator, layer_output, BlendMode, Opacity, Mask)
        ↓
        Update accumulator
    ↓
    Store final channel texture
    ↓
FKLayerEvalResult
```


### Single Layer Evaluation Flow

```
FKLayer
    ↓
Switch on LayerType:
    ↓
    Base → Use FMaterializeParams from stack
    ↓
    Image → Return ImageTexture
    ↓
    Procedural → GenerateProceduralTexture(ProceduralParams)
        ↓
        GPU Compute Shader:
            - Perlin/Simplex/Worley/FBM noise
            - Checker/Brick/Herringbone patterns
            - Scratches/Grunge/Rust/Dust generators
    ↓
    Fill → Create solid color/value texture
    ↓
    Filter → ApplyFilter(SourceTexture, FilterParams)
        ↓
        GPU Compute Shader:
            - Convolution kernels (Blur, Sharpen, Edge)
            - Morphological ops (Dilate, Erode)
            - Color ops (Invert, Normalize, AutoLevels)
    ↓
    Adjustment → ApplyAdjustment(SourceTexture, AdjustmentParams)
        ↓
        GPU Compute Shader:
            - Levels (input/output black/white, gamma)
            - HSV (hue shift, saturation, value)
            - Brightness/Contrast
            - Color Balance, Vibrance, Threshold
    ↓
    Generator → Generate from mesh data
        ↓
        GPU Compute Shader:
            - AO (ray marching)
            - Curvature (second derivative)
            - Position/Normal (vertex data)
            - EdgeWear/Dirt (geometric analysis)
    ↓
    Folder → Skip (container only)
    ↓
UTexture2D* Output
```

---

## Implementation Priorities

### Phase 1: Core Types (High Priority)
- All 9 enums
- FMaterializeParams (40+ fields)
- FMaterializePreset
- FMaterializeResult
- FMaterializeMasterPreset

### Phase 2: Layer System (High Priority)
- FKLayer (25+ fields)
- FKLayerStack (with methods)
- FKLayerEvalResult
- FKProceduralParams
- FKFilterParams
- FKAdjustmentParams

### Phase 3: Engine API (Medium Priority)
- UMaterializeEngine blueprint functions
- UKLayerEvaluator blueprint functions
- FMaterializePresets static methods
- FMaterializePresetRegistry static methods

### Phase 4: Advanced Features (Low Priority)
- GPU shader implementations
- Preset folder scanning
- Master material instance creation
- Asset saving pipeline


---

## Critical Implementation Notes

### 1. Layer Stack Ordering
**CRITICAL:** Layers are stored **bottom-to-top** in the array. Index 0 is the bottom layer, higher indices are on top. This is essential for correct alpha compositing.

```cpp
// CORRECT: Bottom to top
Layers[0] = Base Layer (bottom)
Layers[1] = Grunge Layer
Layers[2] = Scratches Layer
Layers[3] = Color Adjustment (top)

// Evaluation order: 0 → 1 → 2 → 3
```

### 2. Dirty Propagation
When a layer changes, **all layers above it must be marked dirty** because they may depend on the changed layer's output.

```cpp
void MarkDirty(int32 Index)
{
    Layers[Index].bDirty = true;
    // Propagate upward
    for (int32 i = Index + 1; i < Layers.Num(); ++i)
    {
        Layers[i].bDirty = true;
    }
}
```

### 3. Source Layer References
Filter and Adjustment layers can reference other layers as input:

```cpp
// Option 1: Reference by index
layer.SourceLayerIndex = 2;  // Use output of layer 2

// Option 2: Override with explicit texture
layer.SourceOverride = MyTexture;

// Evaluation priority: SourceOverride > SourceLayerIndex > previous layer
```

### 4. Multi-Channel Output
Each layer can target specific channels via bitflags:

```cpp
// Layer only affects Normal and Roughness
layer.OutputChannels = (1 << 1) | (1 << 2);  // Normal | Roughness

// During evaluation, check if layer affects current channel:
if ((layer.OutputChannels & (1 << CurrentChannel)) != 0)
{
    // Blend this layer into current channel
}
```

### 5. Mask Application
Masks control layer opacity spatially:

```cpp
// Mask value determines blend amount per pixel
float MaskValue = SampleMask(MaskTexture, UV);
if (bInvertMask) MaskValue = 1.0f - MaskValue;
float EffectiveOpacity = Opacity * MaskValue;

// Blend with effective opacity
Result = Blend(Base, Layer, BlendMode, EffectiveOpacity);
```


---

## Preset Parameter Patterns

### Organic Materials Pattern
```
- Low normal strength (0.02-0.15)
- High roughness (0.7-1.0 base)
- Low/zero metallic (-100 to -50 bias)
- Moderate to high AO (0.8-1.8)
- Bio detail for organic variation
- Cavity dirt for pores/crevices
```

### Metal Materials Pattern
```
- Low to moderate normal strength (0.02-0.1)
- Variable roughness (0.3-0.8 base)
- High metallic (0.8-1.0 base, 0-90 bias)
- Moderate AO (0.8-1.6)
- Edge wear for aging
- Scratches for wear
- Cyber detail for sci-fi
```

### Fabric Materials Pattern
```
- Low to moderate normal strength (0.02-0.1)
- High roughness (0.2-0.7 base)
- Zero metallic (-100 bias)
- Moderate AO (0.6-1.5)
- Minimal weathering
- Vignette for texture edges
```

### Ground Materials Pattern
```
- Moderate normal strength (0.04-0.15)
- High roughness (0.3-0.7 base, high contrast)
- Low metallic (-100 to -50 bias)
- High AO (1.0-1.8)
- Heavy weathering (dirt, dust, grunge)
- Bio detail for moss/algae
```

### Plastic Materials Pattern
```
- Very low normal strength (0.01-0.02)
- Variable roughness (0.05-0.6 base)
- Low metallic (-90 to -70 bias)
- Low AO (0.5-0.7)
- Minimal weathering
- Cavity dirt for texture
```

### Paper Materials Pattern
```
- Low to moderate normal strength (0.02-0.06)
- High roughness (0.8-1.5 contrast)
- Zero metallic (-100 bias)
- Moderate AO (0.5-1.2)
- Grunge for aging
- Vignette for edges
```


---

## KAIN Codegen Requirements

### Struct Generation

**FMaterializeParams → KAIN:**
- 40+ fields with default values
- All fields need `@editanywhere` + `@category`
- Slider ranges via `@slider(min, max)` or `@meta("ClampMin=..., ClampMax=...")`
- Conditional visibility via `@meta("EditCondition=...")`

**Example:**
```kain
struct MaterializeParams:
    @editanywhere
    @category("Normal")
    @slider(0.0, 2.0)
    normal_strength: Float = 1.0
    
    @editanywhere
    @category("Roughness")
    @slider(0.0, 1.0)
    roughness_base: Float = 0.7
    
    @editanywhere
    @category("Processing")
    make_seamless: Bool = false
    
    @editanywhere
    @category("Processing")
    @meta("EditCondition=make_seamless")
    seamless_mode: SeamlessMode = SeamlessMode.CrossBlend
```

### Enum Generation

**All enums need:**
- `UENUM(BlueprintType)`
- `UMETA(DisplayName=...)` for each value
- Proper E prefix (EMaterializeCategory, EKLayerBlendMode, etc.)

**KAIN compiler should:**
- Auto-add E prefix for enums
- Generate UMETA DisplayName from enum value name
- Support bitflags enums with `@bitflags` attribute

### Blueprint Function Library Generation

**Static methods → Blueprint functions:**

```cpp
// C++
static bool GeneratePBRMaps(UTexture2D* Source, const FMaterializeParams& Params, FMaterializeResult& OutResult);

// KAIN
@blueprint
fn generate_pbr_maps(source: Texture2D, params: MaterializeParams) -> MaterializeResult?
```

**KAIN compiler should:**
- Generate `UBlueprintFunctionLibrary` subclass
- Add `UFUNCTION(BlueprintCallable, Category="Materialize")`
- Handle out parameters (return nullable or tuple)
- Convert error strings to return values


### Method Generation

**FKLayerStack methods → KAIN:**

```kain
struct LayerStack:
    layers: Array<Layer>
    width: Int = 1024
    height: Int = 1024
    selected_layer_index: Int = -1
    
    fn add_layer(layer: Layer) -> Int:
        push(layers, layer)
        return len(layers) - 1
    
    fn insert_layer(index: Int, layer: Layer) -> Int:
        if index < 0 or index > len(layers):
            index = len(layers)
        # Array insert at index
        return index
    
    fn remove_layer(index: Int) -> Bool:
        if index < 0 or index >= len(layers):
            return false
        # Array remove at index
        return true
    
    fn move_layer(from_index: Int, to_index: Int) -> Bool:
        if from_index < 0 or from_index >= len(layers):
            return false
        if to_index < 0 or to_index >= len(layers):
            return false
        let layer = layers[from_index]
        # Remove from old position
        # Insert at new position
        return true
    
    fn duplicate_layer(index: Int) -> Int:
        if index < 0 or index >= len(layers):
            return -1
        let new_layer = layers[index]
        new_layer.id = generate_guid()
        new_layer.name = "{layers[index].name}_Copy"
        return add_layer(new_layer)
    
    fn mark_dirty(index: Int):
        if index >= 0 and index < len(layers):
            layers[index].dirty = true
            # Mark all layers above as dirty
            for i in range(index + 1, len(layers)):
                layers[i].dirty = true
    
    fn mark_all_dirty():
        for layer in layers:
            layer.dirty = true
    
    fn clear_dirty_flags():
        for layer in layers:
            layer.dirty = false
    
    fn get_visible_layer_indices() -> Array<Int>:
        let result: Array<Int> = []
        let has_solo = false
        
        # Check for solo layers
        for i in range(0, len(layers)):
            let layer = layers[i]
            if layer.solo and layer.enabled and not layer.locked:
                has_solo = true
                break
        
        # Collect visible layers
        for i in range(0, len(layers)):
            let layer = layers[i]
            if not layer.enabled:
                continue
            if layer.locked and layer.layer_type != LayerType.Base:
                continue
            if has_solo and not layer.solo:
                continue
            push(result, i)
        
        return result
    
    fn find_layer_by_name(name: String) -> Int:
        for i in range(0, len(layers)):
            if layers[i].name == name:
                return i
        return -1
```


---

## Validation Requirements

### Layer Stack Validation

**Pre-evaluation checks:**
1. Stack has at least one layer
2. Stack width/height are valid (> 0, power of 2 recommended)
3. All visible layers have valid data:
   - Image layers have non-null ImageTexture
   - Filter/Adjustment layers have valid SourceLayerIndex or SourceOverride
   - Procedural layers have valid parameters
4. No circular dependencies in source references
5. Blend modes are valid enum values
6. Output channels are valid bitflags

**KAIN Implementation:**
```kain
fn validate_layer_stack(stack: LayerStack) -> Bool:
    if len(stack.layers) == 0:
        return false
    
    if stack.width <= 0 or stack.height <= 0:
        return false
    
    let visible = stack.get_visible_layer_indices()
    for index in visible:
        let layer = stack.layers[index]
        
        match layer.layer_type:
            LayerType.Image:
                if layer.image_texture == null:
                    return false
            LayerType.Filter:
                if layer.source_layer_index == -1 and layer.source_override == null:
                    return false
            LayerType.Adjustment:
                if layer.source_layer_index == -1 and layer.source_override == null:
                    return false
    
    return true
```

### Texture Format Validation

**Required texture properties:**
- Format: PF_B8G8R8A8 or PF_FloatRGBA
- Dimensions: Power of 2 (recommended, not required)
- Mip maps: Optional
- sRGB: True for BaseColor/Emissive, False for Normal/Roughness/Metallic/AO/Height

**KAIN Implementation:**
```kain
fn validate_texture_format(texture: Texture2D) -> Bool:
    if texture == null:
        return false
    
    # Check format (implementation-specific)
    # Check dimensions > 0
    
    return true
```


---

## Performance Considerations

### 1. Dirty Tracking Optimization
Only re-evaluate dirty layers. Clean layers use cached output.

**Savings:**
- Changing layer 0 → re-evaluate all layers above
- Changing layer 5 (top) → re-evaluate only layer 5
- No changes → zero re-evaluation

### 2. GPU Acceleration
All blend modes, filters, and procedural generators run on GPU via compute shaders.

**Performance targets:**
- 1024x1024 blend: < 1ms
- 2048x2048 procedural: < 5ms
- Full stack evaluation (10 layers): < 50ms

### 3. Texture Pooling
Reuse render targets to avoid allocation overhead.

**Pattern:**
```cpp
static TArray<URenderTarget2D*> RenderTargetPool;

URenderTarget2D* AcquireRenderTarget(int32 Width, int32 Height)
{
    // Find unused RT in pool
    // Or create new RT
}

void ReleaseRenderTarget(URenderTarget2D* RT)
{
    // Return to pool
}
```

### 4. Parallel Channel Evaluation
Evaluate each output channel independently (BaseColor, Normal, Roughness, etc.) in parallel.

**Potential speedup:** 7x (7 channels evaluated concurrently)

### 5. Incremental Updates
When a single layer changes, only re-evaluate affected channels:

```cpp
// Layer only affects Normal and Roughness
layer.OutputChannels = Normal | Roughness;

// On change: only re-evaluate Normal and Roughness channels
// BaseColor, Metallic, Height, AO, Emissive remain cached
```

---

## Memory Layout

### FMaterializeParams
**Size:** ~200 bytes (40 floats + 10 bools + 2 ints + 1 enum)

### FKLayer
**Size:** ~300 bytes (base) + texture pointers
- Identity: 16 bytes (FName) + 16 bytes (FGuid)
- Blending: 8 bytes (enum + float + int32)
- Visibility: 3 bytes (3 bools)
- Mask: 16 bytes (bool + pointer + bool)
- Type-specific data: ~100 bytes (largest is FKProceduralParams)
- Cache: 8 bytes (pointer)

### FKLayerStack
**Size:** ~50 bytes + (N layers × 300 bytes)
- Version: 4 bytes
- Layers array: 16 bytes (TArray header) + N × 300 bytes
- Dimensions: 8 bytes (2 ints)
- Selection: 4 bytes

**Example:** 10-layer stack = 50 + (10 × 300) = 3,050 bytes (~3KB)


---

## Usage Examples

### Example 1: Generate PBR Maps from Photo

```kain
# Load source texture
let source = load_texture("T_WoodPlank_Albedo")

# Configure parameters
let params = MaterializeParams:
    normal_strength: 0.8
    roughness_contrast: 1.5
    metallic_base: 0.0
    ao_intensity: 1.2
    make_seamless: true
    seamless_mode: SeamlessMode.CrossBlend
    pack_orm: true

# Generate maps
let result = generate_pbr_maps(source, params)

if result != null:
    println("Generated in {result.generation_time_ms}ms")
    # Use result.normal, result.roughness, result.orm, etc.
```

### Example 2: Apply Preset

```kain
# Get preset
let preset = get_materialize_preset_by_id("iron_rusty")

if preset != null:
    let source = load_texture("T_Metal_Albedo")
    let result = generate_pbr_maps(source, preset.params)
```

### Example 3: Build Layer Stack

```kain
# Create stack
let stack = LayerStack:
    width: 2048
    height: 2048

# Add base layer (PBR generation)
let base_layer = Layer:
    name: "Base"
    layer_type: LayerType.Base
    blend_mode: LayerBlendMode.Normal
    opacity: 1.0

stack.add_layer(base_layer)

# Add grunge overlay
let grunge_layer = Layer:
    name: "Grunge"
    layer_type: LayerType.Procedural
    blend_mode: LayerBlendMode.Multiply
    opacity: 0.5
    output_channels: LayerOutputChannel.Roughness | LayerOutputChannel.AO
    procedural_params: ProceduralParams:
        noise_type: ProceduralNoiseType.Grunge
        scale: 2.0
        octaves: 4

stack.add_layer(grunge_layer)

# Add scratches
let scratch_layer = Layer:
    name: "Scratches"
    layer_type: LayerType.Procedural
    blend_mode: LayerBlendMode.Add
    opacity: 0.3
    output_channels: LayerOutputChannel.Normal | LayerOutputChannel.Roughness
    procedural_params: ProceduralParams:
        noise_type: ProceduralNoiseType.Scratches
        scale: 5.0

stack.add_layer(scratch_layer)

# Evaluate stack
let result = evaluate_stack(stack)

if result != null:
    println("Evaluated in {result.evaluation_time_ms}ms")
```


### Example 4: Layer Masking

```kain
# Create base layer
let base = Layer:
    name: "Base"
    layer_type: LayerType.Image
    image_texture: load_texture("T_Metal_Albedo")

# Create rust overlay with mask
let rust = Layer:
    name: "Rust"
    layer_type: LayerType.Procedural
    blend_mode: LayerBlendMode.Overlay
    opacity: 0.7
    has_mask: true
    mask_texture: load_texture("T_RustMask")
    invert_mask: false
    procedural_params: ProceduralParams:
        noise_type: ProceduralNoiseType.Rust
        scale: 3.0

let stack = LayerStack:
    width: 1024
    height: 1024

stack.add_layer(base)
stack.add_layer(rust)

let result = evaluate_stack(stack)
```

### Example 5: Filter and Adjustment Layers

```kain
# Create stack with image
let stack = LayerStack:
    width: 1024
    height: 1024

let base = Layer:
    name: "Photo"
    layer_type: LayerType.Image
    image_texture: load_texture("T_Concrete_Photo")

stack.add_layer(base)

# Add blur filter
let blur = Layer:
    name: "Blur"
    layer_type: LayerType.Filter
    blend_mode: LayerBlendMode.Normal
    opacity: 1.0
    source_layer_index: 0  # Reference base layer
    filter_params: FilterParams:
        filter_type: FilterType.GaussianBlur
        intensity: 2.0
        kernel_size: 5

stack.add_layer(blur)

# Add levels adjustment
let levels = Layer:
    name: "Levels"
    layer_type: LayerType.Adjustment
    blend_mode: LayerBlendMode.Normal
    opacity: 1.0
    source_layer_index: 1  # Reference blur layer
    adjustment_params: AdjustmentParams:
        adjustment_type: AdjustmentType.Levels
        input_black: 0.1
        input_white: 0.9
        gamma: 1.2

stack.add_layer(levels)

let result = evaluate_stack(stack)
```


### Example 6: Solo and Lock Layers

```kain
let stack = LayerStack:
    width: 1024
    height: 1024

# Add 3 layers
stack.add_layer(Layer:
    name: "Base"
    layer_type: LayerType.Image
    image_texture: load_texture("T_Base")
)

stack.add_layer(Layer:
    name: "Grunge"
    layer_type: LayerType.Procedural
    solo: true  # Solo this layer
    procedural_params: ProceduralParams:
        noise_type: ProceduralNoiseType.Grunge
)

stack.add_layer(Layer:
    name: "Scratches"
    layer_type: LayerType.Procedural
    locked: true  # Lock this layer (won't render)
    procedural_params: ProceduralParams:
        noise_type: ProceduralNoiseType.Scratches
)

# Evaluation will only render "Grunge" layer (solo mode)
let result = evaluate_stack(stack)
```

---

## Architecture Summary

### Type System
- **9 enums** — 8 standard + 1 bitflags (LayerOutputChannel)
- **10 structs** — 3 params, 3 results, 3 layer-related, 1 preset
- **Total fields:** ~150 across all structs

### Layer System
- **8 layer types** — Base, Image, Procedural, Fill, Adjustment, Filter, Generator, Folder
- **20 blend modes** — Photoshop-equivalent compositing
- **15 procedural generators** — Noise, patterns, weathering
- **13 filters** — Blur, sharpen, edge detect, morphological ops
- **9 adjustments** — Levels, curves, HSV, brightness/contrast
- **8 mesh generators** — AO, curvature, position, normal, thickness, wear, dirt, lightmap

### Preset System
- **33 presets** — 23 PBR generation + 4 master materials + 6 categories
- **5 categories** — Organic, Rubber, Ground, Fabric, Metal, Plastic, Paper
- **Extensible** — Runtime registration + folder scanning

### Engine
- **GPU-accelerated** — Compute shaders for all operations
- **CPU fallback** — For unsupported operations
- **Parallel generation** — All maps generated concurrently
- **Asset pipeline** — Save generated textures as .uasset files


---

## KAIN Compiler Requirements

### 1. Bitflags Enum Support

**Required:** `@bitflags` attribute for `EKLayerOutputChannel`

```kain
@bitflags
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

**Generates:**
```cpp
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ELayerOutputChannel : uint8
{
    None        = 0         UMETA(Hidden),
    BaseColor   = 1 << 0    UMETA(DisplayName = "Base Color"),
    Normal      = 1 << 1    UMETA(DisplayName = "Normal"),
    // ...
};
ENUM_CLASS_FLAGS(ELayerOutputChannel);
```

### 2. Conditional Property Visibility

**Required:** `@meta("EditCondition=...")` support

```kain
struct MaterializeParams:
    @editanywhere
    make_seamless: Bool = false
    
    @editanywhere
    @meta("EditCondition=make_seamless")
    seamless_mode: SeamlessMode = SeamlessMode.CrossBlend
```

**Generates:**
```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing")
bool bMakeSeamless = false;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing", meta = (EditCondition = "bMakeSeamless"))
EKSeamlessMode SeamlessMode = EKSeamlessMode::CrossBlend;
```

### 3. Slider Range Metadata

**Required:** `@slider(min, max)` attribute

```kain
@slider(0.0, 2.0)
normal_strength: Float = 1.0
```

**Generates:**
```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Normal", meta = (ClampMin = "0.0", ClampMax = "2.0"))
float NormalStrength = 1.0f;
```

### 4. Nullable Texture References

**Required:** `Texture2D?` → `TObjectPtr<UTexture2D>`

```kain
struct Layer:
    image_texture: Texture2D?
    mask_texture: Texture2D?
    cached_output: Texture2D?
```

**Generates:**
```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite)
TObjectPtr<UTexture2D> ImageTexture;

UPROPERTY(EditAnywhere, BlueprintReadWrite)
TObjectPtr<UTexture2D> MaskTexture;

UPROPERTY(Transient)
TObjectPtr<UTexture2D> CachedOutput;
```

### 5. Static Method → Blueprint Function

**Required:** `@blueprint` on top-level functions generates `UBlueprintFunctionLibrary`

```kain
@blueprint
fn generate_pbr_maps(source: Texture2D, params: MaterializeParams) -> MaterializeResult?
```

**Generates:**
```cpp
UCLASS()
class UMaterializeBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
    
    UFUNCTION(BlueprintCallable, Category = "Materialize")
    static bool GeneratePBRMaps(UTexture2D* Source, const FMaterializeParams& Params, FMaterializeResult& OutResult);
};
```


### 6. Struct Methods

**Required:** Struct methods generate member functions

```kain
struct LayerStack:
    layers: Array<Layer>
    
    fn add_layer(layer: Layer) -> Int:
        push(layers, layer)
        return len(layers) - 1
    
    fn mark_dirty(index: Int):
        if index >= 0 and index < len(layers):
            layers[index].dirty = true
```

**Generates:**
```cpp
USTRUCT(BlueprintType)
struct FLayerStack
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FLayer> Layers;
    
    int32 AddLayer(const FLayer& Layer)
    {
        Layers.Add(Layer);
        return Layers.Num() - 1;
    }
    
    void MarkDirty(int32 Index)
    {
        if (Layers.IsValidIndex(Index))
        {
            Layers[Index].bDirty = true;
        }
    }
};
```

### 7. Default Struct Constructors

**Required:** Structs with default values need default constructors

```kain
struct MaterializeParams:
    normal_strength: Float = 1.0
    roughness_base: Float = 0.7
    # ... 40+ fields
```

**Generates:**
```cpp
USTRUCT(BlueprintType)
struct FMaterializeParams
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NormalStrength = 1.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RoughnessBase = 0.7f;
    
    // ... 40+ fields
    
    FMaterializeParams() = default;
};
```


---

## Testing Strategy

### Unit Tests

**Type validation:**
- Enum value ranges
- Struct default values
- Bitflags operations

**Layer stack operations:**
- Add/Insert/Remove/Move/Duplicate layers
- Dirty tracking propagation
- Visibility filtering (enabled/solo/locked)
- Layer finding (by GUID, by name)

**Preset system:**
- All 33 presets load correctly
- Category filtering works
- Preset lookup by ID
- Default params are valid

### Integration Tests

**PBR generation:**
- Generate from 1024x1024 source
- All 7 maps produced
- ORM packing correct (R=AO, G=Roughness, B=Metallic)
- Generation time < 500ms

**Layer evaluation:**
- 10-layer stack evaluates correctly
- Blend modes produce expected results
- Masking works correctly
- Dirty tracking avoids redundant work

**Preset application:**
- Apply each preset to test texture
- Verify parameter values match preset
- Verify output quality

### Performance Tests

**Benchmarks:**
- 1024x1024 single layer: < 5ms
- 2048x2048 single layer: < 20ms
- 10-layer stack (1024x1024): < 50ms
- Full PBR generation (2048x2048): < 500ms

**Memory:**
- 10-layer stack: < 5KB
- Cached textures: Width × Height × 4 bytes per layer
- Total memory: < 100MB for typical workflow


---

## Migration Path (C++ → KAIN)

### Step 1: Type Definitions
Create `types.kn` with all 9 enums and 10 structs. This establishes the data model.

### Step 2: Preset Data
Create `presets.kn` with all 33 preset definitions. This is pure data, no logic.

### Step 3: Engine API
Create `engine.kn` with blueprint functions for PBR generation. This wraps the C++ implementation.

### Step 4: Layer Evaluator API
Create `layer_evaluator.kn` with blueprint functions for layer operations. This wraps the C++ compositor.

### Step 5: Validation
Add validation functions to ensure data integrity before GPU operations.

### Step 6: Testing
Write unit tests for all types, presets, and operations.

---

## Dependencies

### UE5 Modules Required
- **Core** — FName, FGuid, TArray, TMap
- **CoreUObject** — UObject, USTRUCT, UENUM
- **Engine** — UTexture2D, UMaterialInstanceDynamic
- **RenderCore** — GPU operations
- **RHI** — Render target management

### External Dependencies
- None (self-contained)

### KAIN Stdlib Functions Used
- `push()` — Array append
- `len()` — Array length
- `range()` — Iteration
- `println()` — Debug output
- `generate_guid()` — GUID generation (if added to stdlib)

---

## File Structure

```
FactoryPart2/plugins/Materialize/
├── src/
│   ├── types.kn              # All enums and structs (9 enums, 10 structs)
│   ├── presets.kn            # 33 preset definitions
│   ├── engine.kn             # PBR generation API (2 functions)
│   ├── layer_evaluator.kn    # Layer compositor API (9 functions)
│   └── validation.kn         # Validation helpers (4 functions)
├── docs/
│   ├── CORE_ARCHITECTURE.md  # This file
│   ├── ARCHITECTURE_CORE.md  # High-level overview
│   ├── SHADER_PIPELINE.md    # GPU shader details
│   └── IMPLEMENTATION_PLAN.md # Implementation roadmap
├── KAIN.toml                 # Plugin configuration
└── README.md                 # Plugin overview
```


---

## Quick Reference

### All Enums (9)
1. `EMaterializeCategory` (8 values) — Organic, Rubber, Ground, Fabric, Metal, Plastic, Paper, Custom
2. `EKSeamlessMode` (4 values) — None, CrossBlend, MirrorBlend, Histogram
3. `EKLayerBlendMode` (20 values) — Normal, Multiply, Screen, Overlay, SoftLight, HardLight, Add, Subtract, Difference, Exclusion, Darken, Lighten, ColorDodge, ColorBurn, LinearDodge, LinearBurn, VividLight, LinearLight, PinLight, HardMix
4. `EKLayerType` (8 values) — Base, Image, Procedural, Fill, Adjustment, Filter, Generator, Folder
5. `EKLayerOutputChannel` (9 values, bitflags) — None, BaseColor, Normal, Roughness, Metallic, Height, AO, Emissive, Mask, All
6. `EKProceduralNoiseType` (15 values) — Perlin, Simplex, Worley, FBM, Turbulence, Cellular, Gradient, Checker, Brick, Herringbone, Hexagon, Scratches, Grunge, Rust, Dust
7. `EKFilterType` (13 values) — Blur, GaussianBlur, Sharpen, EdgeDetect, Emboss, HighPass, LowPass, Median, Dilate, Erode, Invert, Normalize, AutoLevels
8. `EKAdjustmentType` (9 values) — Levels, Curves, HSV, Brightness, ColorBalance, Vibrance, Threshold, Posterize, Gradient
9. `EKGeneratorType` (8 values) — AmbientOcclusion, Curvature, Position, WorldNormal, Thickness, EdgeWear, Dirt, LightMap

### All Structs (10)
1. `FMaterializeParams` (40+ fields) — PBR generation parameters
2. `FMaterializePreset` (4 fields) — Preset descriptor
3. `FMaterializeResult` (10 fields) — PBR generation result
4. `FMaterializeMasterPreset` (11 fields) — Master material preset
5. `FKLayer` (25+ fields) — Single layer definition
6. `FKLayerStack` (5 fields + methods) — Layer stack container
7. `FKLayerEvalResult` (8 fields) — Layer evaluation result
8. `FKProceduralParams` (9 fields) — Procedural generation parameters
9. `FKFilterParams` (4 fields) — Filter parameters
10. `FKAdjustmentParams` (11 fields) — Adjustment parameters

### All Blueprint Functions (15)
1. `GeneratePBRMaps()` — Generate all PBR maps
2. `GenerateAndSavePBRMaps()` — Generate and save as assets
3. `EvaluateStack()` — Evaluate entire layer stack
4. `EvaluateSingleLayer()` — Evaluate one layer
5. `BlendTextures()` — Blend two textures
6. `GenerateProceduralTexture()` — Generate procedural texture
7. `ApplyFilter()` — Apply filter to texture
8. `ApplyAdjustment()` — Apply adjustment to texture
9. `AddTextures()` — Add two textures
10. `MultiplyTextures()` — Multiply two textures
11. `LerpTextures()` — Lerp two textures
12. `GetAllPresets()` — Get all presets
13. `GetPresetsByCategory()` — Get presets by category
14. `GetPresetById()` — Get preset by ID
15. `GetDefaultParams()` — Get default parameters

### All Presets (33)

**Organic (6):** skin_basic, leather_worn, alien_bio, bark, zombie, dragon_scale

**Rubber/Synth (5):** rubber_matte, latex_shiny, tire_worn, plastic_rough, gasket

**Ground/Rock (5):** ground_wet, rock_rough, concrete, snow, asphalt

**Fabric (5):** denim, silk, wool, canvas, velvet

**Metal (5):** iron_rusty, gold_dirty, aluminum_brushed, scifi_panel, copper

**Plastic (4):** plastic_glossy, plastic_matte, bakelite, pvc

**Paper/Card (3):** cardboard, paper_clean, parchment

**Master Materials (4):** Standard, Metal, Glossy, Toon

---

## Conclusion

Materialize core architecture is a **well-structured, GPU-accelerated PBR generation system** with:

- **Comprehensive type system** — 9 enums, 10 structs, 150+ fields
- **Photoshop-style layer compositor** — 8 layer types, 20 blend modes, dirty tracking, caching
- **Rich preset library** — 33 presets covering 7 material categories
- **Blueprint-friendly API** — 15 blueprint functions for all operations
- **Performance-optimized** — GPU acceleration, dirty tracking, texture pooling

**KAIN implementation is straightforward** — direct 1:1 mapping of types, enums, and functions with standard naming conventions (PascalCase → snake_case, remove prefixes).

**Key challenges:**
1. Bitflags enum support (`@bitflags`)
2. Conditional property visibility (`@meta("EditCondition=...")`)
3. Struct methods (already supported)
4. Nullable texture references (already supported)
5. Blueprint function generation (already supported)

**Estimated KAIN LOC:** ~800 lines (200 type definitions + 400 preset data + 200 function wrappers)

**Estimated C++ LOC:** ~4,000 lines (5:1 compression ratio)
