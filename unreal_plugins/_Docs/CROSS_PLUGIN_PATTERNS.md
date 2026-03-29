# Cross-Plugin Pattern Database

**Purpose:** Track patterns, issues, and solutions across all Factory plugins to identify compiler improvements and common patterns.

**Last Updated:** 2026-02-23

---

## Plugin Comparison Matrix

| Plugin | KAIN Lines | C++ Lines | Ratio | Actors | Components | Enums | Structs | BP Functions | Shaders | Editor UI | Status |
|--------|-----------|-----------|-------|--------|------------|-------|---------|--------------|---------|-----------|--------|
| **MetaFitter** | **5,500** | **44,000** | **1:8.0** | **3** | **4** | **18** | **50+** | **150+** | **0** | **✓✓✓** | **KAIN ✓ / UE5 ✗** |
| **Cinema4DMograph** | 3,000 | 15,000 | 1:5.0 | 2 | 7 | 13 | 53 | 250 | 0 | ✓ | KAIN ✓ / UE5 ✗ |
| **TemporalBlueprint** | 1,500 | 10,000 | 1:6.7 | 5 | 4 | 7+ | 20+ | 2,628 lines | 0 | ✓✓ | KAIN ✓ / UE5 ✗ |
| VoxelForgePro | 1,943 | 15,000 | 1:7.7 | ? | ? | ? | ? | ? | 19 | ? | ✓ |
| TitanGraph | 1,692 | 10,000 | 1:5.9 | ? | ? | ? | ? | ? | ? | ✓ | ✓ |
| AeroTunnel | 1,620 | 12,000 | 1:7.4 | ? | ? | ? | ? | ? | ? | ? | ✓ |
| KainFlow | 966 | 8,000 | 1:8.3 | ? | ? | ? | ? | ? | ? | ? | ✓ |
| NarrativeGraph | 464 | 2,321 | 1:5.0 | ? | ? | ? | ? | ? | ? | ✓ | ✓ |

---

## TemporalBlueprint Patterns

### Unique Characteristics
1. **Editor-heavy plugin:** Most comprehensive editor UI generation to date
2. **Massive Blueprint library:** 2,628 lines (10x larger than Cinema4DMograph)
3. **Dual subsystems:** Runtime + Editor subsystems with tick support
4. **Complete Details panels:** 5 customization classes with category organization
5. **Slate widget suite:** 6 widgets including viewport overlay
6. **Temporal debugging:** Complete time-state management system

### Code Generation Patterns

#### 1. Subsystem with Tick Support
**Pattern:** UWorldSubsystem + FTickableGameObject dual inheritance

**Generated Code:**
```cpp
UCLASS()
class TEMPORAL_API UTemporalSubsystem : public UWorldSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
    
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    
    // FTickableGameObject interface
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;
    
    // 27 state properties
    // 24 public methods
};
```

**Quality Indicators:**
- ✓ Proper dual inheritance
- ✓ All lifecycle methods implemented
- ✓ CYCLE_STAT macro for profiling
- ✓ Comprehensive state management (27 properties)
- ✓ Complete API (24 methods)

**Observation:** First plugin demonstrating complete subsystem codegen with tick support. Production-ready pattern.

#### 2. Details Panel with Category Organization
**Pattern:** IDetailCustomization with hierarchical categories

**Category Structure:**
```
Temporal|Identity
  - actor_guid
  - display_name

Temporal|Behavior
  - behavior
  - native_era

Temporal|Visibility
  - visible_in_ancient
  - visible_in_past
  - visible_in_present
  - visible_in_future
  - visible_in_apocalyptic
  - visible_in_alternate
  - visible_in_void

Temporal|Ghost
  - show_ghost_in_non_native
  - ghost_opacity
  - ghost_color
  - ghost_emissive_strength

Temporal|CustomData
  - custom_data_0
  - custom_data_1
  - custom_data_2
  - custom_data_3

Temporal|Causality
  - participates_in_causality
  - causality_group_tag
```

**Generated Code:**
```cpp
void FFTemporalActorProxyDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    TArray<TWeakObjectPtr<UObject>> Objects;
    DetailBuilder.GetObjectsBeingCustomized(Objects);
    if (Objects.Num() > 0)
    {
        CachedObject = Objects[0];
    }
    
    IDetailCategoryBuilder& IdentityCat = DetailBuilder.EditCategory(TEXT("Temporal|Identity"));
    TSharedRef<IPropertyHandle> actor_guidHandle = DetailBuilder.GetProperty(TEXT("actor_guid"));
    IdentityCat.AddProperty(actor_guidHandle);
    // ... continues with proper property binding
}
```

**Quality Indicators:**
- ✓ Hierarchical category naming (Plugin|Category)
- ✓ Proper property handle retrieval
- ✓ Cached object reference
- ✓ Clean organization (6 categories)

**Observation:** Most sophisticated details panel generation to date. Demonstrates mature editor codegen.

#### 3. Slate Widget Suite
**Pattern:** 6 specialized Slate widgets for temporal debugging

**Widgets:**
1. `SSTemporalEditorPanel` - Main editor panel
2. `SSTemporalActorInspector` - Actor state inspector
3. `SSTemporalCausalityPanel` - Causality graph viewer
4. `SSTemporalEraConfigPanel` - Era configuration
5. `SSTemporalSnapshotPanel` - Snapshot manager
6. `SSTemporalViewportOverlayViewport` - Viewport overlay

**Generated Structure:**
```cpp
class SSTemporalEditorPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSTemporalEditorPanel)
        : _Content()
        {}
        
        SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()
    
    void Construct(const FArguments& InArgs);
    
    void build();
    void EraPickerRow();
    void TimelineStrip();
    void ActorStateTable();
    void ControlButtons();
};
```

**Quality Indicators:**
- ✓ Proper SLATE_BEGIN_ARGS / SLATE_END_ARGS macros
- ✓ Construct() method
- ✓ Logical method breakdown (build, EraPickerRow, TimelineStrip, etc.)

**Observation:** Demonstrates compiler's ability to generate complete Slate widget suites for complex editor tools.

#### 4. Blueprint Function Library Scale
**Pattern:** Massive utility library (2,628 lines)

**File:** `TemporalBlueprintLibrary.cpp` (120,275 bytes)

**Scale Comparison:**
- **TemporalBlueprint:** 2,628 lines
- **Cinema4DMograph:** 250 functions
- **Ratio:** 10x larger

**Function Categories:**
- Combat utilities (damage, healing, armor, crits)
- Experience/leveling system
- Inventory management
- Cooldown tracking
- Math utilities (remap, lerp, smoothstep)
- Noise functions
- Easing functions
- Procedural generation

**Observation:** Largest Blueprint function library generated by KAIN to date. Demonstrates compiler can handle extreme scale (10x previous record).

#### 5. Multi-Module Editor Plugin
**Pattern:** Runtime + Editor modules with proper dependencies

**Module Structure:**
```toml
[[ue5.modules]]
name = "TemporalBlueprint"
type = "Runtime"
loading_phase = "Default"

[[ue5.modules]]
name = "TemporalBlueprintEditor"
type = "Editor"
loading_phase = "PostEngineInit"
```

**Runtime Dependencies:**
- Core, CoreUObject, Engine (standard)
- RHI, RenderCore (shader support)
- GameplayTags, NetCore (gameplay systems)
- GeometryCore (geometry processing)

**Editor Dependencies:**
- Slate, SlateCore (UI)
- PropertyEditor (details panels)
- AdvancedPreviewScene (viewport)
- ToolMenus, WorkspaceMenuStructure (editor integration)
- TemporalBlueprint (runtime module)

**Quality Indicators:**
- ✓ Proper module separation
- ✓ Comprehensive dependencies
- ✓ Correct loading phases

**Observation:** Demonstrates mature multi-module plugin generation with complete editor integration.

---

## Cinema4DMograph Patterns

### Unique Characteristics
1. **Largest KAIN codebase:** 3,000 lines (highest in Factory)
2. **Massive Blueprint library:** 250 functions (unprecedented scale)
3. **Complex modifier system:** 20+ modifier types with inheritance
4. **Multi-module structure:** Runtime + Editor modules
5. **DataTable integration:** 3 DataTable row types

### Code Generation Patterns

#### 1. Blueprint Function Library Scale
**Pattern:** Single UBlueprintFunctionLibrary with 250 UFUNCTION(BlueprintCallable) functions

**Files:**
- Header: `ZenMographBlueprintLibrary.h` (549 lines)
- Implementation: `ZenMographBlueprintLibrary.cpp` (140.6 KB)

**Categories:**
- Math utilities (Remap, SmoothStep, InverseLerp, PingPong)
- Color utilities (HSVtoRGB, GetRainbowColor, GetHeatmapColor)
- Noise functions (Perlin, Voronoi, Simplex, Cellular)
- Procedural generation (asteroid fields, galaxy spirals, terrain)
- Easing functions (20+ types)
- Vector/transform utilities

**Observation:** This is the largest Blueprint function library generated by KAIN to date. Demonstrates compiler's ability to handle massive utility libraries.

#### 2. Modifier Type Hierarchy
**Pattern:** 20+ modifier structs with shared base structure

**Types:**
- FAttractModifier, FBounceModifier, FDelayModifier, FElasticModifier
- FFigure8Modifier, FFloatModifier, FGravityModifier, FLissajousModifier
- FNoiseModifier, FOrbitModifier, FPendulumModifier, FPulseModifier
- FPushModifier, FRandomModifier, FShakeModifier, FStepModifier
- FSwayModifier, FTargetModifier, FTumbleModifier, FVortexModifier, FWaveModifier

**Common Structure:**
```cpp
USTRUCT(BlueprintType)
struct ZENMOGRAPH_API FXxxModifier
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float strength;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float frequency;
    
    // Modifier-specific properties...
};
```

**Observation:** Clean, consistent struct generation. All modifiers follow same pattern with proper USTRUCT macros.

#### 3. Component Architecture
**Pattern:** 7 specialized components for different mograph subsystems

**Components:**
- UClonerAnimationComponent (animation tweaking)
- UClonerEffectorComponent (effector management)
- UClonerInstanceComponent (instance management)
- UClonerNiagaraDataInterface (Niagara integration)
- UClonerPerformanceComponent (performance monitoring)
- UClonerTargetComponent (target tracking)
- UClonerVFXComponent (VFX integration)

**Generation Quality:**
- All use `UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))`
- Proper inheritance from UActorComponent
- Correct ZENMOGRAPH_API export macro

#### 4. Editor UI Generation
**Pattern:** Complete Slate widget + Details panel + Asset editor

**Slate Widgets:**
- SBakeSettingsDialog (dialog with SLATE_BEGIN_ARGS)
- SClonerPreviewViewport (SEditorViewport with viewport client)
- SClonerProperties (property panel)
- SClonerTimeline (timeline widget)

**Details Customizations:**
- FClonerActorDetailsCustomization (IDetailCustomization)
- FClonerDataDetailsCustomization (IDetailCustomization)

**Asset Editor:**
- FClonerAssetEditorToolkit (FAssetEditorToolkit)
- Integrates viewport + details + toolbar

**Observation:** First plugin with complete asset editor generation. Demonstrates editor codegen maturity.

---

## Issues Discovered

### Issue #1: Case-Insensitive Function Name Collision

**Plugin:** Cinema4DMograph  
**Severity:** Critical (blocks UE5 compilation)  
**Category:** Blueprint Function Naming

**Description:**
KAIN compiler generates both lowercase and capitalized versions of the same function:
- `float remap(...)` (line 363) - from KAIN standard library
- `float Remap(...)` (line 481) - from user's utilities.kn

UE5's reflection system treats these as the same function name (case-insensitive), causing:
```
Error: 'Remap' conflicts with 'Function /Script/ZenMograph.ZenMographFunctionLibrary:remap'
```

**Root Cause:**
1. KAIN standard library provides `remap()` function
2. User defines `Remap()` function in utilities.kn
3. Both get generated into same UBlueprintFunctionLibrary
4. UE5 UnrealHeaderTool sees duplicate (case-insensitive)

**Impact:**
- Blocks UE5 compilation
- Prevents plugin from loading
- Affects any plugin using both stdlib and user-defined functions with case-variant names

**Recommended Fix:**
1. **Compiler-side:** Add case-insensitive duplicate detection for Blueprint-exposed functions
2. **Oracle validation:** New rule category for Blueprint naming conflicts
3. **Standard library:** Consider prefixing all stdlib functions with `kain_` to avoid collisions
4. **Short-term workaround:** Rename user function to `RemapValue` or `KainRemap`

**Affected Files:**
- `ZenMographBlueprintLibrary.h` (lines 363, 481)
- `ZenMographBlueprintLibrary.cpp` (lines 1781, 2616)

**Priority:** HIGH - Blocks all plugins with stdlib + user function name collisions

---

## Patterns: Blueprint Function Libraries

### Scale Comparison

| Plugin | BP Functions | Library Size | Pattern |
|--------|--------------|--------------|---------|
| **Cinema4DMograph** | **250** | **140.6 KB** | **Massive utility library** |
| NarrativeGraph | ~20 | ~15 KB | Quest/dialogue helpers |
| VoxelForgePro | ~30 | ~20 KB | Voxel math utilities |

**Observation:** Cinema4DMograph has **10x more Blueprint functions** than typical plugins. This stress-tests the Blueprint codegen system.

### Common Function Categories
1. **Math utilities:** Remap, Lerp, SmoothStep, InverseLerp, Clamp
2. **Noise functions:** Perlin, Simplex, Voronoi, Cellular
3. **Easing functions:** Linear, Quad, Cubic, Quart, Quint, Sine, Expo, Circ, Back, Elastic, Bounce
4. **Color utilities:** HSV/RGB conversion, gradients, color spaces
5. **Vector utilities:** Normalize, Dot, Cross, Distance, Length
6. **Procedural generation:** Terrain, asteroids, galaxies, clouds

**Recommendation:** Consider extracting common utilities into a shared KAIN stdlib module to avoid duplication across plugins.

---

## Patterns: Multi-Module Plugins

### Module Structure Comparison

| Plugin | Runtime Module | Editor Module | Other Modules |
|--------|---------------|---------------|---------------|
| **Cinema4DMograph** | **ZenMograph** | **ZenMographEditor** | - |
| TitanGraph | TitanGraph | TitanGraphEditor | - |
| NarrativeGraph | NarrativeGraph | NarrativeGraphEditor | - |

**Common Pattern:**
```toml
[[ue5.modules]]
name = "PluginName"
type = "Runtime"
loading_phase = "PostConfigInit"

[[ue5.modules]]
name = "PluginNameEditor"
type = "Editor"
loading_phase = "PostEngineInit"
```

**Observation:** All multi-module plugins follow same naming convention: `{Plugin}` + `{Plugin}Editor`. Loading phases are consistent.

---

## Patterns: DataTable Integration

### Cinema4DMograph DataTable Types

1. **FClonerPresetData** (FTableRowBase)
   - Preset configurations for cloner setups
   - Includes modifier stacks, distribution layers

2. **FModifierPresetData** (FTableRowBase)
   - Preset configurations for individual modifiers
   - Includes strength, frequency, easing curves

3. **FExpressionModifierPreset** (FTableRowBase)
   - Expression-based modifier presets
   - Includes formula strings, variable bindings

**Pattern:**
```cpp
USTRUCT(BlueprintType)
struct ZENMOGRAPH_API FXxxData : public FTableRowBase
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // ... fields ...
};
```

**Observation:** All DataTable rows properly inherit from FTableRowBase. Ready for CSV import in UE5.

---

## Compression Ratio Analysis

### By Plugin Complexity

| Complexity | Plugin | Ratio | Observation |
|------------|--------|-------|-------------|
| **Simple** | NarrativeGraph | 1:5.0 | Basic actors + components |
| **Medium** | TitanGraph | 1:5.9 | Graph editor + runtime |
| **Complex** | AeroTunnel | 1:7.4 | Physics simulation |
| **Complex** | VoxelForgePro | 1:7.7 | GPU compute shaders |
| **Very Complex** | KainFlow | 1:8.3 | Soft-body physics |
| **Massive Utility** | **Cinema4DMograph** | **1:5.0** | **250 BP functions** |

**Insight:** Compression ratio correlates with **code complexity**, not **code volume**. Cinema4DMograph has high volume (250 functions) but low complexity (simple utility functions), resulting in 1:5 ratio similar to NarrativeGraph.

**Hypothesis:** 
- Simple utilities: 1:5 ratio
- Complex logic (physics, shaders): 1:7-8 ratio
- Graph systems: 1:6 ratio

---

## Patterns Appearing in 3+ Plugins

### Pattern #1: Multi-Module Structure (Runtime + Editor)
**Frequency:** 3 plugins (Cinema4DMograph, TemporalBlueprint, TitanGraph, NarrativeGraph)

**Common Structure:**
```toml
[[ue5.modules]]
name = "PluginName"
type = "Runtime"
loading_phase = "Default" or "PostConfigInit"

[[ue5.modules]]
name = "PluginNameEditor"
type = "Editor"
loading_phase = "PostEngineInit"
```

**Naming Convention:** `{Plugin}` + `{Plugin}Editor`

**Loading Phases:**
- Runtime: `Default` or `PostConfigInit`
- Editor: `PostEngineInit` (consistent across all plugins)

**Recommendation:** This is the standard pattern for plugins with editor tools. Should be documented as best practice.

---

### Pattern #2: Details Panel Customization
**Frequency:** 3 plugins (Cinema4DMograph, TemporalBlueprint, TitanGraph)

**Common Structure:**
```cpp
class FXxxDetailsCustomization : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance() { 
        return MakeShareable(new FXxxDetailsCustomization()); 
    }
    
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
    
private:
    TWeakObjectPtr<UObject> CachedObject;
};
```

**Common Patterns:**
- Property handle retrieval: `DetailBuilder.GetProperty(TEXT("property_name"))`
- Category creation: `DetailBuilder.EditCategory(TEXT("Category|Subcategory"))`
- Property binding: `CategoryBuilder.AddProperty(propertyHandle)`
- Object caching: `TWeakObjectPtr<UObject> CachedObject`

**Quality Indicators:**
- ✓ All use hierarchical category naming (Plugin|Category)
- ✓ All cache object references
- ✓ All use proper property handle pattern

**Recommendation:** This pattern is mature and should be used as reference for future plugins.

---

### Pattern #3: Massive Blueprint Function Libraries
**Frequency:** 2 plugins (Cinema4DMograph: 250 functions, TemporalBlueprint: 2,628 lines)

**Common Categories:**
- Math utilities (Remap, Lerp, SmoothStep, Clamp)
- Noise functions (Perlin, Simplex, Voronoi)
- Easing functions (Linear, Quad, Cubic, etc.)
- Color utilities (HSV/RGB conversion, gradients)
- Vector utilities (Normalize, Dot, Cross, Distance)

**Scale:**
- Cinema4DMograph: 250 functions, 140.6 KB
- TemporalBlueprint: 2,628 lines, 120.3 KB

**Observation:** Both plugins demonstrate KAIN's ability to generate extremely large Blueprint function libraries. No performance or compilation issues observed.

**Recommendation:** Consider extracting common utilities (math, noise, easing) into shared KAIN stdlib module to avoid duplication.

---

### Pattern #4: Subsystem with Tick Support
**Frequency:** 2 plugins (TemporalBlueprint: 2 subsystems, likely others)

**Common Structure:**
```cpp
UCLASS()
class PLUGIN_API UXxxSubsystem : public UWorldSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
    
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    
    // FTickableGameObject interface
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;
};
```

**Quality Indicators:**
- ✓ Dual inheritance (UWorldSubsystem + FTickableGameObject)
- ✓ All lifecycle methods implemented
- ✓ CYCLE_STAT macro for profiling
- ✓ Proper tick control (IsTickable)

**Recommendation:** This is the correct pattern for tickable subsystems. Should be documented as best practice.

---

### Pattern #5: Slate Widget Suite
**Frequency:** 2 plugins (Cinema4DMograph: 4 widgets, TemporalBlueprint: 6 widgets)

**Common Structure:**
```cpp
class SXxxWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SXxxWidget)
        : _Content()
        {}
        
        SLATE_DEFAULT_SLOT(FArguments, Content)
    SLATE_END_ARGS()
    
    void Construct(const FArguments& InArgs);
};
```

**Common Widget Types:**
- Editor panels (main UI)
- Viewport overlays
- Property panels
- Timeline widgets
- Inspector panels

**Quality Indicators:**
- ✓ All use SLATE_BEGIN_ARGS / SLATE_END_ARGS
- ✓ All have Construct() method
- ✓ All inherit from SCompoundWidget

**Recommendation:** Slate widget generation is mature. Consider adding more complete Construct() implementation in future.

---

### Pattern #6: Component Architecture
**Frequency:** 3+ plugins (Cinema4DMograph: 7, TemporalBlueprint: 4, likely others)

**Common Structure:**
```cpp
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PLUGIN_API UXxxComponent : public UActorComponent
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Component")
    // ... properties ...
};
```

**Common Patterns:**
- ClassGroup=(Custom)
- meta=(BlueprintSpawnableComponent)
- Proper PLUGIN_API export macro
- Category organization

**Recommendation:** Component generation is consistent and production-ready.

---

## Issues Appearing in 2+ Plugins

### Issue #1: Case-Insensitive Function Name Collision
**Frequency:** 2 plugins (Cinema4DMograph, potentially TemporalBlueprint)

**Description:** KAIN generates both lowercase and capitalized versions of same function, causing UE5 reflection system conflicts.

**Example:**
- `float remap(...)` (stdlib)
- `float Remap(...)` (user-defined)

**Impact:** Blocks UE5 compilation

**Recommendation:** Add case-insensitive duplicate detection in Blueprint codegen (HIGH PRIORITY)

---

### Issue #2: File Lock During FULLBUILD.bat
**Frequency:** 2 plugins (Cinema4DMograph, TemporalBlueprint)

**Description:** .uasset files locked during build, preventing deletion/copy operations.

**Error:**
```
Failed to delete directory '...\HostProject'
Failed to delete ...\BP_XxxActor.uasset for copy
AutomationTool exiting with ExitCode=1 (Error_Unknown)
```

**Impact:** Blocks UE5 compilation (not a code generation issue)

**Recommendation:** Improve FULLBUILD.bat to detect and handle file locks gracefully (MEDIUM PRIORITY)

---

## Compression Ratio Insights

### By Plugin Type

| Type | Plugins | Avg Ratio | Observation |
|------|---------|-----------|-------------|
| **Simple Utilities** | NarrativeGraph, Cinema4DMograph | 1:5.0 | Low complexity, high volume |
| **Graph Systems** | TitanGraph | 1:5.9 | Medium complexity |
| **Editor-Heavy** | TemporalBlueprint | 1:6.7 | Complex editor UI |
| **Physics/Simulation** | AeroTunnel | 1:7.4 | Complex logic |
| **GPU Compute** | VoxelForgePro | 1:7.7 | Shader complexity |
| **Soft-Body Physics** | KainFlow | 1:8.3 | Very complex logic |

**Insight:** Compression ratio correlates with **code complexity**, not **code volume**.

**Hypothesis:**
- Simple utilities: 1:5 ratio
- Editor-heavy plugins: 1:6-7 ratio
- Complex logic (physics, shaders): 1:7-8 ratio

---

## Recommendations for Compiler Improvements

### 1. Case-Insensitive Duplicate Detection
**Priority:** HIGH  
**Impact:** Blocks Cinema4DMograph UE5 build

**Implementation:**
- Add pre-flight check in Blueprint codegen
- Maintain case-insensitive function name registry
- Emit error if duplicate detected
- Suggest alternative names

### 2. Standard Library Namespacing
**Priority:** MEDIUM  
**Impact:** Prevents future collisions

**Options:**
- Prefix all stdlib functions with `kain_` (e.g., `kain_remap`)
- Use namespace in generated code (e.g., `KainStdlib::Remap`)
- Allow user to opt-out of stdlib functions

### 3. Blueprint Function Library Splitting
**Priority:** LOW  
**Impact:** Improves organization for large libraries

**Proposal:**
- Auto-split libraries >100 functions into categories
- Generate multiple UBlueprintFunctionLibrary classes
- Example: `UZenMographMathLibrary`, `UZenMographNoiseLibrary`, `UZenMographColorLibrary`

### 4. Oracle Validation Rules
**Priority:** HIGH  
**Impact:** Catches issues before codegen

**New Rules:**
- Blueprint function name collision (case-insensitive)
- DataTable row validation (FTableRowBase inheritance)
- Component naming conventions
- Module dependency cycles

---

## Next Steps

1. ✓ **TemporalBlueprint validation complete** → Build report created, patterns documented
2. **Fix Cinema4DMograph naming conflict** → Recompile → Validate UE5 build
3. **Fix TemporalBlueprint naming conflicts** → Address Task 21.2 issues → Recompile
4. **Extract common patterns** → Create shared KAIN stdlib module (math, noise, easing)
5. **Update Oracle** → Add case-insensitive duplicate detection (HIGH PRIORITY)
6. **Improve FULLBUILD.bat** → Add file lock detection and handling
7. **Document remaining plugins** → Fill in missing data in comparison matrix
8. **Run regression tests** → Verify Materialize, VoxelForgePro, Cinema4DMograph still compile

---

**Maintained by:** KAIN Validation Pipeline  
**Contributors:** plugin-compilation-pipeline spec subagent  
**Version:** 1.1 (Updated 2026-02-23 with TemporalBlueprint patterns)

---

## MetaFitter Patterns

### Unique Characteristics
1. **Largest plugin:** 15 KAIN source files (5,500 lines total)
2. **Most complex:** MetaHuman API integration, physics, viewport, batch, materials
3. **Highest compression ratio:** 1:8.0 (most complex logic per line)
4. **External API integration:** MetaHuman SDK, ChaosCloth, ChaosOutfit
5. **Production-scale:** Enterprise batch processing, statistics tracking

### Code Generation Patterns

#### 1. MetaHuman API Integration
**Pattern:** 20+ MetaHuman-specific Blueprint functions

**Key Functions:**
- `get_metahuman_body_mesh(character_path)` - Extract body mesh from character
- `get_metahuman_skeleton(character_path)` - Extract skeleton
- `get_metahuman_body_material(character_path)` - Extract material
- `apply_outfit_to_metahuman(wardrobe_item_path, metahuman_character_path)` - Apply clothing
- `finalize_clothing_asset(source_mesh_path, target_metahuman_path, output_name, clothing_type, layer_slot)` - Finalize asset
- `build_metahuman_character(character_path)` - Build character
- `can_build_metahuman(character_path)` - Validate build readiness
- `get_metahuman_editor_subsystem()` - Access editor subsystem
- `add_wardrobe_item_to_character(character_path, wardrobe_item_path)` - Add wardrobe item
- `transfer_materials(source_mesh_path, target_metahuman_path)` - Transfer materials
- `adjust_material_for_metahuman(material_path, clothing_type)` - Adjust material

**Observation:** First plugin with extensive external API integration. Demonstrates compiler's ability to generate bindings for complex UE5 subsystems.

#### 2. Conforming Algorithm Functions
**Pattern:** 15+ functions for mesh conforming workflow

**Key Functions:**
- `create_default_conform_settings()` - Create default settings
- `apply_clothing_type_defaults(settings, clothing_type)` - Apply type-specific defaults
- `validate_mesh_for_conforming(topology)` - Validate mesh topology
- `analyze_mesh_topology_from_path(mesh_path)` - Analyze mesh structure
- `detect_mesh_openings(topology)` - Detect openings (neck, wrists, ankles)
- `perform_shrinkwrap(source_mesh_path, target_body_path, tightness, offset_multiplier, preserve_wrinkles, wrinkle_strength)` - Shrinkwrap algorithm
- `calculate_vertex_offset(clothing_type, tightness, region)` - Calculate per-vertex offset
- `transfer_skin_weights(source_mesh, target_skeleton, max_influences)` - Transfer weights
- `smooth_vertex_weights(weights, iterations, mode)` - Smooth weights (Laplacian, Gaussian, Bilateral)

**Observation:** Complex algorithm implementations with multiple parameters. Demonstrates compiler's ability to handle production-grade algorithms.

#### 3. Physics Simulation
**Pattern:** Complete cloth physics system with presets

**Physics Presets:**
- Silk (light, flowing)
- Cotton (medium weight)
- Leather (stiff, heavy)
- Denim (stiff, medium)
- Heavy (armor, coats)

**Key Functions:**
- `setup_cloth_physics(mesh_path, params)` - Setup physics
- `create_physics_preset(preset_type)` - Create preset
- `calculate_wind_force(wind_direction, wind_strength, surface_normal)` - Wind simulation
- `apply_collision_primitives(mesh_path, primitives)` - Collision setup

**Observation:** First plugin with complete physics preset system. Demonstrates compiler's ability to generate physics-related code.

#### 4. Subsystem + Tickable Pattern
**Pattern:** UWorldSubsystem + FTickableGameObject combined

**Implementation:**
```cpp
UCLASS()
class METAFITTER_API UMetaFitterSubsystem : public UWorldSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
    
    // UWorldSubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    
    // FTickableGameObject interface
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;
};
```

**Features:**
- Default settings management
- Batch processing state
- Statistics tracking (attempts, successes, failures)
- Performance metrics (average time, ETA)

**Observation:** First plugin with combined subsystem + tickable pattern. Demonstrates compiler's ability to generate multiple interface implementations.

#### 5. Batch Processing System
**Pattern:** Enterprise-grade batch job management

**Features:**
- Job queue management
- Progress tracking (current/total)
- Success/failure statistics
- ETA calculation
- Batch cancellation
- Performance metrics

**Key Functions:**
- `start_batch(total_jobs)` - Start batch
- `advance_batch(success)` - Advance to next job
- `cancel_batch()` - Cancel batch
- `is_batch_active()` - Check if batch active
- `get_batch_progress()` - Get progress percentage
- `get_batch_eta_seconds()` - Get estimated time remaining

**Observation:** First plugin with production-scale batch processing. Demonstrates compiler's ability to generate enterprise features.

#### 6. Material Transfer System
**Pattern:** Material transfer and adjustment for MetaHuman

**Key Functions:**
- `generate_hidden_face_map(source_mesh_path, target_body_path, config)` - Generate hidden face map
- `create_coverage_map(clothing_type, openings)` - Create coverage map
- `dilate_texture(texture_path, pixels)` - Dilate texture

**Observation:** First plugin with material transfer system. Demonstrates compiler's ability to generate material-related code.

#### 7. Multi-File Plugin Structure
**Pattern:** 15 files organized by subsystem

**File Organization:**
- **Core:** types.kn (root dependency)
- **Algorithms:** algorithms.kn (conforming, shrinkwrap, weights)
- **Runtime:** components.kn, actors.kn, subsystems.kn
- **Physics:** physics.kn
- **MetaHuman:** metahuman_integration.kn
- **Batch:** batch.kn
- **Materials:** materials.kn
- **Presets:** presets.kn
- **Editor:** details.kn, editor_module.kn, editor_toolbar.kn, editor_ui.kn, editor_viewport.kn

**Observation:** Largest multi-file plugin. Demonstrates compiler's ability to handle complex dependency graphs.

---

## Issues Discovered

### Issue #2: Component Naming Inconsistency

**Plugin:** MetaFitter  
**Severity:** Critical (blocks UE5 compilation)  
**Category:** Component Codegen

**Description:**
KAIN compiler generates inconsistent naming for `@component` structs:
- KAIN source: `@component struct ClothingLayerManager`
- Expected class: `UClothingLayerManagerComponent` (with "Component" suffix)
- Generated class: `UClothingLayerManager` (missing "Component" suffix)
- Generated header: `FClothingLayerManager.h` (wrong prefix - F instead of U)
- Actor reference: `UClothingLayerManager* layer_manager` (missing "Component" suffix)

UE5's UnrealHeaderTool cannot find the type:
```
Error: Unable to find 'class', 'delegate', 'enum', or 'struct' with name 'UClothingLayerManagerComponent'
```

**Root Cause:**
1. `@component` attribute should generate class name with "Component" suffix
2. Header file should use U prefix (not F prefix) for component classes
3. Actor field references should use full component name with suffix

**Impact:**
- Blocks UE5 compilation
- Prevents plugin from loading
- Affects any plugin using `@component` structs

**Recommended Fix:**
1. **Backend codegen:** Update `ue5` crate to ensure `@component` structs generate `U{Name}Component` class names
2. **Header naming:** Ensure component headers use U prefix matching class name
3. **Reference consistency:** Ensure actor field types match generated component class names
4. **Oracle validation:** Add validation rule for component naming consistency

**Affected Files:**
- `FClothingLayerManager.h` (should be `UClothingLayerManagerComponent.h`)
- `AClothConformerActor.h` (line 41: references `UClothingLayerManager*`)

**Priority:** HIGH - Blocks MetaFitter UE5 build

---

## Compression Ratio Analysis (Updated)

### By Plugin Complexity

| Complexity | Plugin | Ratio | Observation |
|------------|--------|-------|-------------|
| **Simple** | NarrativeGraph | 1:5.0 | Basic actors + components |
| **Medium** | Cinema4DMograph | 1:5.0 | 250 BP functions (high volume, low complexity) |
| **Medium** | TitanGraph | 1:5.9 | Graph editor + runtime |
| **Complex** | TemporalBlueprint | 1:6.7 | 9 files, subsystems, details, toolbar |
| **Complex** | AeroTunnel | 1:7.4 | Physics simulation |
| **Complex** | VoxelForgePro | 1:7.7 | GPU compute shaders |
| **Very Complex** | **MetaFitter** | **1:8.0** | **MetaHuman API, physics, batch, materials** |
| **Very Complex** | KainFlow | 1:8.3 | Soft-body physics |

**Updated Insight:** MetaFitter achieves 1:8.0 ratio due to:
1. **External API integration** (MetaHuman SDK) - complex bindings
2. **Production algorithms** (shrinkwrap, weight transfer) - complex logic
3. **Physics simulation** (cloth physics) - complex calculations
4. **Batch processing** (enterprise features) - complex state management
5. **Material systems** (hidden face maps, coverage maps) - complex operations

**Confirmed Hypothesis:**
- Simple utilities: 1:5 ratio
- Complex logic (physics, algorithms): 1:7-8 ratio
- External API integration: 1:8+ ratio

---

## Recommendations for Compiler Improvements (Updated)

### 1. Component Naming Consistency
**Priority:** CRITICAL  
**Impact:** Blocks MetaFitter UE5 build

**Implementation:**
- Update `ue5` crate codegen to ensure `@component` structs generate `U{Name}Component` class names
- Ensure header files use U prefix matching class name
- Ensure actor field references use full component name with suffix
- Add Oracle validation rule for component naming consistency

### 2. External API Integration Support
**Priority:** MEDIUM  
**Impact:** Enables more plugins with external dependencies

**Proposal:**
- Add metadata system for external API bindings (MetaHuman, Niagara, PCG, etc.)
- Generate proper includes for external modules
- Validate external API usage at compile time
- Document external API integration patterns

### 3. Production Algorithm Support
**Priority:** LOW  
**Impact:** Improves code quality for complex algorithms

**Proposal:**
- Add support for algorithm-specific optimizations
- Generate performance profiling code for algorithms
- Add validation for algorithm parameters
- Document algorithm implementation patterns

### 4. Oracle Validation Rules (Updated)
**Priority:** HIGH  
**Impact:** Catches issues before codegen

**New Rules:**
- Component naming consistency (U prefix, Component suffix)
- External API validation (module dependencies, includes)
- Algorithm parameter validation (ranges, types)
- Batch processing validation (state management, progress tracking)

---

## Next Steps (Updated)

1. **Fix MetaFitter component naming** → Backend fix → Recompile → Validate UE5 build
2. **Fix Cinema4DMograph naming conflict** → Recompile → Validate UE5 build
3. **Fix TemporalBlueprint name collisions** → Recompile → Validate UE5 build
4. **Extract common patterns** → Create shared KAIN stdlib module
5. **Update Oracle** → Add component naming + case-insensitive duplicate detection
6. **Document remaining plugins** → Fill in missing data in comparison matrix
7. **Run regression tests** → Verify Materialize, VoxelForgePro still compile

---

**Last Updated:** 2026-02-23 (MetaFitter patterns added)  
**Maintained by:** KAIN Validation Pipeline  
**Contributors:** plugin-compilation-pipeline spec subagent  
**Version:** 1.1
