# ue5-blueprints Features Audit

> **Crate:** `Kain/crates/ue5-blueprints`
> **Status:** Phase 2 complete for simple Blueprints, complex Kismet bytecode falls back to C++ factory
> **Last Updated:** 2026-03-02

---

## Overview

The ue5-blueprints crate generates UE5 Blueprint assets from KAIN Blueprint constructs. It produces:
- Binary `.uasset` Blueprint files
- C++ `UK2Node` custom node subclasses
- Kismet bytecode for event graphs
- Blueprint function libraries

**Total Size:** ~82KB across 5 core files

---

## Feature Categories

### 1. Blueprint Function Libraries

**Status:** ✅ Full Support

**KAIN Syntax:**
```kain
@blueprint
fn calculate_damage(base: Float, multiplier: Float, armor: Float) -> Float:
    let raw = base * multiplier
    return max(raw * (1.0 - armor / 100.0), 0.0)
```

**Generated Output:**
```cpp
UCLASS()
class UMyPluginBlueprintLibrary : public UBlueprintFunctionLibrary {
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="MyPlugin")
    static float CalculateDamage(float Base, float Multiplier, float Armor);
};
```

**Factory Part 1 Examples:**
- **VRAMSniper**: `CalculateTextureVRAM`, `DetectTextureIssues`, `IsPowerOfTwo`, `GetCompressionFormatName`, `GetIssueDescription`, `FormatVRAMSize`, `GetOptimalCompressionFormat`
- **UltimateVFX**: `get_quality_sample_count`, `get_time_of_day_sun_angle`, `get_weather_fog_density`, `lerp_vec3`, `calculate_sun_color`, `get_atmosphere_preset_colors`
- **TickOptimizer**: Blueprint utility functions for tick optimization

**Key Features:**
- Static methods in `UBlueprintFunctionLibrary`
- `UFUNCTION(BlueprintCallable)` macro
- Category organization
- Type marshalling (KAIN → UE5 types)

---

### 2. Blueprint Callable Methods

**Status:** ✅ Full Support

**KAIN Syntax:**
```kain
@subsystem
struct TextureAnalyzer:
    total_textures: Int
    total_vram_mb: Float
    
    @blueprint_callable
    fn GetTotalTextures() -> Int:
        return total_textures
    
    @blueprint_callable
    fn GetTotalVRAM() -> Float:
        return total_vram_mb
```

**Generated Output:**
```cpp
UFUNCTION(BlueprintCallable, Category="TextureAnalyzer")
int32 GetTotalTextures() const;

UFUNCTION(BlueprintCallable, Category="TextureAnalyzer")
float GetTotalVRAM() const;
```

**Factory Part 1 Examples:**
- **VRAMSniper/TextureAnalyzerSubsystem**: `GetTotalTextures`, `GetTotalVRAM`, `GetTexturesWithIssues`, `GetScanProgress`, `GetOptimizationProgress`, `GetVRAMSaved`, `GetTexturesOptimized`
- **TickOptimizer/TickOptimizerSubsystem**: `GetActorsOptimized`, `GetActorsWhitelisted`, `GetTotalActorsTracked`, `GetCPUTimeSaved`
- **UltimateVFX/AtmosphereController**: `SetAtmospherePreset`

**Key Features:**
- Instance methods on actors/components/subsystems
- `UFUNCTION(BlueprintCallable)` macro
- Category organization
- Const correctness

---

### 3. Blueprint Pure Functions

**Status:** ✅ Full Support

**KAIN Syntax:**
```kain
@blueprint_pure
fn IsPowerOfTwo(value: Int) -> Bool:
    if value <= 0:
        return false
    return (value & (value - 1)) == 0
```

**Generated Output:**
```cpp
UFUNCTION(BlueprintPure, Category="Math")
static bool IsPowerOfTwo(int32 Value);
```

**Factory Part 1 Examples:**
- **VRAMSniper**: `IsPowerOfTwo`
- **TickOptimizer**: `IsEnabled`, `IsProfileModeEnabled`

**Key Features:**
- `UFUNCTION(BlueprintPure)` macro
- Adds `const` to method signature
- No execution pins in Blueprint (pure data flow)
- Can be called multiple times without side effects

---

### 4. Blueprint Events

**Status:** ✅ Full Support

**KAIN Syntax:**
```kain
actor GameMode:
    @blueprint_event
    fn on_player_joined(player: Actor):
        println("Player joined!")
```

**Generated Output:**
```cpp
UFUNCTION(BlueprintNativeEvent, Category="GameMode")
void OnPlayerJoined(AActor* Player);

// Implementation method
virtual void OnPlayerJoined_Implementation(AActor* Player);
```

**Factory Part 1 Examples:**
- Limited direct usage in Factory Part 1 (most use blueprint_callable)
- Pattern used in game mode and actor lifecycle events

**Key Features:**
- `UFUNCTION(BlueprintNativeEvent)` macro
- Auto-generates `_Implementation()` method
- Blueprint can override native implementation
- Event-driven architecture

---

### 5. Blueprint Implementable Events

**Status:** ✅ Full Support

**KAIN Syntax:**
```kain
actor CustomActor:
    @blueprint_implementable_event
    fn on_custom_event(data: Int):
        pass  # Blueprint must implement
```

**Generated Output:**
```cpp
UFUNCTION(BlueprintImplementableEvent, Category="CustomActor")
void OnCustomEvent(int32 Data);
```

**Factory Part 1 Examples:**
- Used in actor lifecycle hooks
- Custom event systems

**Key Features:**
- `UFUNCTION(BlueprintImplementableEvent)` macro
- No C++ implementation (Blueprint-only)
- Pure virtual in Blueprint context

---

### 6. Custom Blueprint Nodes (UK2Node)

**Status:** ✅ Full Support (15.4KB factory.rs)

**KAIN Syntax:**
```kain
@blueprint_node
fn async_load_texture(path: String) -> Texture2D:
    # Async loading logic
    pass
```

**Generated Output:**
```cpp
UCLASS()
class UAsyncLoadTextureNode : public UK2Node {
    GENERATED_BODY()
public:
    virtual void AllocateDefaultPins() override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetMenuCategory() const override;
    virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
};
```

**Factory Part 1 Examples:**
- Limited direct usage (most plugins use standard blueprint functions)
- Pattern available for custom node creation

**Key Features:**
- Full `UK2Node` subclass generation
- `AllocateDefaultPins()` for pin creation
- `ExpandNode()` for node expansion at compile time
- Custom node title and category
- Async node support via `UK2Node_AsyncAction`

---

### 7. Async Blueprint Nodes

**Status:** ✅ Full Support

**KAIN Syntax:**
```kain
@async
@blueprint
fn async_download_file(url: String) -> String:
    # Async download logic
    pass
```

**Generated Output:**
```cpp
UCLASS()
class UAsyncDownloadFileNode : public UK2Node_AsyncAction {
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable)
    FOnDownloadComplete OnComplete;
    
    UPROPERTY(BlueprintAssignable)
    FOnDownloadFailed OnFailed;
    
    UFUNCTION(BlueprintCallable, Category="Network")
    static UAsyncDownloadFileNode* AsyncDownloadFile(const FString& URL);
};
```

**Factory Part 1 Examples:**
- Pattern available but not extensively used in Factory Part 1
- Suitable for async operations (network, file I/O, long computations)

**Key Features:**
- `UK2Node_AsyncAction` base class
- Multiple output execution pins (OnComplete, OnFailed, etc.)
- Delegate-based completion callbacks
- Latent action support

---

### 8. Blueprint Binary Writer

**Status:** ✅ Phase 2 Complete (35.5KB writer.rs)

**Purpose:** Direct binary `.uasset` Blueprint serialization

#### 8.1 Phase 1 — Simple Blueprints (Complete)

**Supported:**
- `UDataAsset` subclasses
- Blueprint-callable functions
- Simple property sets

**Property Types (14):**
1. Bool
2. Int
3. Float
4. String
5. Name
6. Text
7. Enum
8. Object
9. Struct
10. SoftObject
11. SoftClass
12. Array
13. Map
14. Set

**Factory Part 1 Examples:**
- All blueprint function libraries generate binary assets
- Simple data assets for configuration

---

#### 8.2 Phase 2 — Event Graphs (Complete for Simple Graphs)

**Supported:**
- `BeginPlay` / `EndPlay` event nodes
- Function call nodes with direct output wiring
- Variable get/set nodes
- Branch (if/else) nodes

**Limitations:**
- Complex event graphs (arbitrary branching, loops) fall back to C++ factory
- No async node UI (progress pins)

**Factory Part 1 Examples:**
- Simple event graphs in actor blueprints
- Basic function call chains

---

### 9. Kismet Bytecode Generation

**Status:** ✅ Full Support for Simple Graphs (13.6KB kismet.rs)

**Purpose:** Generate Kismet VM bytecode for event graphs

**Supported Instructions:**

| Instruction | KAIN Origin | Purpose |
|-------------|-------------|---------|
| `EX_CallMath` | Math function call | Call math function |
| `EX_LocalVariable` | Variable read | Read local variable |
| `EX_InstanceVariable` | Field access | Read instance field |
| `EX_LocalOutVariable` | Output from function | Write to output parameter |
| `EX_True` / `EX_False` | Boolean constants | Boolean literals |
| `EX_IntConst` | Integer literal | Integer constant |
| `EX_FloatConst` | Float literal | Float constant |
| `EX_StringConst` | String literal | String constant |
| `EX_Jump` | Unconditional branch | Jump to label |
| `EX_JumpIfNot` | Conditional branch | Jump if false |
| `EX_Return` | Function return | Return from function |
| `EX_EndOfScript` | Block terminator | End of script block |

**Factory Part 1 Examples:**
- Simple event graphs with function calls
- Variable access patterns
- Basic control flow

**Limitation:** Complex event graphs (arbitrary branching, loops, async nodes) still fall back to C++ factory generation rather than Kismet bytecode.

---

### 10. Blueprint Node IR

**Status:** ✅ Full Support (7.4KB ir.rs)

**Purpose:** Intermediate representation for Blueprint nodes

**IR Structures:**
- `BlueprintNode` - Node representation
- `BlueprintPin` - Pin representation (input/output)
- `BlueprintConnection` - Connection between pins
- `BlueprintGraph` - Complete graph structure

**Pin Types:**
- Exec (execution flow)
- Bool
- Int
- Float
- String
- Object
- Struct
- Enum
- Wildcard
- Array

**Factory Part 1 Examples:**
- All blueprint generation uses IR internally

---

### 11. Blueprint Conversion

**Status:** ✅ Full Support (10.8KB conversion.rs)

**Purpose:** KAIN Blueprint node IR → UE5 Blueprint pin/connection conversion

**Key Features:**
- Pin type mapping (KAIN → UE5)
- Connection validation
- Node expansion
- Type checking

---

## Feature Coverage Summary

| Feature | Status | Factory Part 1 Usage |
|---------|--------|---------------------|
| Blueprint Function Libraries | ✅ Full | 20+ functions across 5 plugins |
| Blueprint Callable Methods | ✅ Full | 30+ methods across 5 plugins |
| Blueprint Pure Functions | ✅ Full | 5+ functions across 3 plugins |
| Blueprint Events | ✅ Full | Limited direct usage |
| Blueprint Implementable Events | ✅ Full | Limited direct usage |
| Custom Blueprint Nodes (UK2Node) | ✅ Full | Pattern available |
| Async Blueprint Nodes | ✅ Full | Pattern available |
| Blueprint Binary Writer | ✅ Phase 2 | All blueprint assets |
| Kismet Bytecode | ✅ Simple Graphs | Simple event graphs |
| Blueprint Node IR | ✅ Full | All blueprint generation |
| Blueprint Conversion | ✅ Full | All blueprint generation |

---

## Known Limitations

1. **Complex event graph Kismet codegen** - Arbitrary Blueprint logic falls back to C++ factory
2. **No async Blueprint Task UI** - `UK2Node_AsyncAction` generated but no progress pin
3. **No Blueprint interface codegen** - `UInterface` assets not generated
4. **Limited Blueprint macro support** - Blueprint macros not yet supported
5. **No Blueprint animation graph** - Animation Blueprint nodes not yet supported

---

## Test Coverage

**21 tests passing** covering:
- Blueprint function library generation
- UK2Node generation
- Kismet bytecode generation
- Async node generation
- Blueprint binary writer
- Property type serialization
- Pin type mapping
- Connection validation

---

## Factory Part 1 Plugin Examples

### VRAMSniper (10+ blueprint functions)
- `CalculateTextureVRAM` - Calculate VRAM usage for texture
- `DetectTextureIssues` - Detect texture optimization issues
- `IsPowerOfTwo` - Check if value is power of two
- `GetCompressionFormatName` - Get compression format name
- `GetIssueDescription` - Get issue description
- `FormatVRAMSize` - Format VRAM size for display
- `GetOptimalCompressionFormat` - Get optimal compression format
- `GetTotalTextures` - Get total texture count (callable)
- `GetTotalVRAM` - Get total VRAM usage (callable)
- `GetVRAMSaved` - Get VRAM saved by optimization (callable)

### UltimateVFX (8+ blueprint functions)
- `get_quality_sample_count` - Get sample count for quality level
- `get_time_of_day_sun_angle` - Get sun angle for time of day
- `get_weather_fog_density` - Get fog density for weather type
- `lerp_vec3` - Lerp between two Vec3 values
- `calculate_sun_color` - Calculate sun color for time of day
- `get_atmosphere_preset_colors` - Get atmosphere preset colors
- `SetAtmospherePreset` - Set atmosphere preset (callable)

### TickOptimizer (8+ blueprint methods)
- `GetActorsOptimized` - Get count of optimized actors (callable)
- `GetActorsWhitelisted` - Get count of whitelisted actors (callable)
- `GetTotalActorsTracked` - Get total tracked actors (callable)
- `GetCPUTimeSaved` - Get CPU time saved (callable)
- `IsEnabled` - Check if optimizer is enabled (pure)
- `IsProfileModeEnabled` - Check if profile mode is enabled (pure)

---

## Crate Files

| File | Size | Purpose |
|------|------|---------|
| `writer.rs` | 35.5KB | Blueprint binary writer |
| `factory.rs` | 15.4KB | UK2Node C++ codegen |
| `kismet.rs` | 13.6KB | Kismet bytecode emitter |
| `conversion.rs` | 10.8KB | KAIN → UE5 conversion |
| `ir.rs` | 7.4KB | Blueprint node IR |

**Total:** ~82KB

---

## Future Enhancements

1. **Complex Kismet bytecode** - Support arbitrary branching, loops, async nodes
2. **Blueprint interfaces** - Generate `UInterface` assets
3. **Blueprint macros** - Support Blueprint macro libraries
4. **Animation Blueprints** - Generate animation graph nodes
5. **Blueprint debugging** - Add debugging metadata to generated blueprints
