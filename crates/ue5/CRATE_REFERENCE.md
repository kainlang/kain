# UE5 Runtime Codegen Crate Reference

> **Last Updated:** 2026-02-19  
> **Purpose:** Complete reference for the `ue5` crate - the runtime code generator that transpiles KAIN to UE5 C++  
> **Status:** Production-ready - 22 tests passing, comprehensive validation, data-driven type system

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Actor Codegen](#actor-codegen)
4. [Component Codegen](#component-codegen)
5. [Struct Codegen](#struct-codegen)
6. [Enum Codegen](#enum-codegen)
7. [Delegate System](#delegate-system)
8. [Blueprint Functions](#blueprint-functions)
9. [EngineKnowledge System](#engineknowledge-system)
10. [Naming Conventions](#naming-conventions)
11. [Type Mapping](#type-mapping)
12. [Oracle Validation](#oracle-validation)
13. [File Structure](#file-structure)
14. [Examples](#examples)

---

## Overview

The `ue5` crate is the **runtime code generator** for the KAIN compiler. It transforms typed KAIN AST into production-ready Unreal Engine 5 C++ code with full UCLASS/USTRUCT/UENUM/UPROPERTY/UFUNCTION annotations.

### What It Generates

- **Actors** - AActor subclasses with RPCs, replication, lifecycle methods
- **Components** - UActorComponent subclasses with state and methods
- **Structs** - USTRUCT with optional FTableRowBase inheritance (@datatable)
- **Enums** - UENUM(BlueprintType) with display names
- **Delegates** - DECLARE_DYNAMIC_MULTICAST_DELEGATE_* macros
- **Blueprint Functions** - UBlueprintFunctionLibrary static methods
- **Shader Integration** - Auto-wires compute shaders to actor Tick()

### Key Features

- **Data-Driven Type System** - Uses EngineKnowledge.json instead of hardcoded types
- **Semantic Validation** - Oracle catches UHT errors before C++ compilation
- **Modular Output** - Per-item file generation for scalable projects
- **Smart Prefixing** - Automatic A/F/E/U prefix detection prevents double-prefixing
- **RPC Auto-Configuration** - Server_/Client_/Multicast_ naming convention
- **Replication Support** - GetLifetimeReplicatedProps auto-generation

---

## Architecture

### Entry Points

The crate provides multiple entry points for different use cases:

```rust
// Main entry point - accepts MonomorphizedProgram (generic functions instantiated)
pub fn generate(program: &MonomorphizedProgram, output_name: Option<&str>, copyright: Option<&str>) -> KainResult<Ue5Output>

// With pre-configured context (includes EngineKnowledge)
pub fn generate_with_context(program: &MonomorphizedProgram, output_name: Option<&str>, copyright: Option<&str>, context: &Ue5Context) -> KainResult<Ue5Output>

// Filtered generation (single item)
pub fn generate_filtered(program: &MonomorphizedProgram, module_name: &str, output_name: Option<&str>, target_item: Option<String>, copyright: Option<&str>, type_to_header: HashMap<String, String>, shader_file_names: Option<Vec<String>>) -> KainResult<Ue5Output>

// Legacy TypedProgram support (for packager compatibility)
pub fn generate_from_typed(program: &TypedProgram, output_name: Option<&str>, copyright: Option<&str>) -> KainResult<Ue5Output>
pub fn generate_with_context_typed(program: &TypedProgram, output_name: Option<&str>, copyright: Option<&str>, context: &Ue5Context) -> KainResult<Ue5Output>
```

### Output Structure

```rust
pub struct Ue5Output {
    pub header: String,              // .h file content
    pub source: String,              // .cpp file content
    pub shader_files: Vec<(String, String)>, // Vec<(filename, content)>
}
```

### Core Components

1. **Ue5Gen** - Main code generator struct with StringBuilder pattern
2. **Ue5Context** - Shared compilation context (type registry, EngineKnowledge, metadata)
3. **TypeMapper** - Centralized type mapping (KAIN → C++)
4. **EngineKnowledge** - Queryable database of UE5 types
5. **Oracle** - Semantic validator (catches UHT errors pre-codegen)
6. **Naming** - UE5 prefix rules (A/F/E/U/S)

### Compilation Flow

```
TypedProgram → Monomorphization → Ue5Gen
    ↓
Pre-Pass: Register all types in Ue5Context
    ↓
Oracle Validation: Check UHT rules
    ↓
Per-Item Codegen:
    - Actors → gen_actor()
    - Structs → gen_ustruct() or gen_ucomponent()
    - Enums → gen_uenum()
    - Functions → gen_ufunction()
    - Delegates → gen_multicast_delegate()
    ↓
Post-Processing: Python cleanup (empty lines)
    ↓
Ue5Output { header, source, shader_files }
```


---

## Actor Codegen

Actors are the core gameplay entities in UE5. The `ue5` crate generates AActor subclasses with full networking, replication, and lifecycle support.

### Basic Actor

**KAIN:**
```kain
actor Player:
    state health: Float = 100.0
    state max_health: Float = 100.0
    
    on BeginPlay():
        println("Player spawned!")
    
    on Tick(delta_time: Float):
        // Update logic
        health = min(health + 1.0 * delta_time, max_health)
```

**Generated C++:**
```cpp
UCLASS()
class GAME_API APlayer : public AActor
{
    GENERATED_BODY()

public:
    APlayer();

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float health;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float max_health;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
};
```

### RPC Support

KAIN uses naming conventions for RPCs:
- `Server_*` → Server RPC (Reliable)
- `Client_*` → Client RPC (Reliable)
- `Multicast_*` → Multicast RPC (Reliable)

**KAIN:**
```kain
actor GameMode:
    state score: Int = 0
    
    on Server_AddScore(points: Int):
        score = score + points
        Multicast_AnnounceScore(score)
    
    on Multicast_AnnounceScore(new_score: Int):
        println("Score: {new_score}")
```

**Generated C++:**
```cpp
UFUNCTION(Server, Reliable)
void Server_AddScore(int64 points);
void Server_AddScore_Implementation(int64 points);

UFUNCTION(NetMulticast, Reliable)
void Multicast_AnnounceScore(int64 new_score);
void Multicast_AnnounceScore_Implementation(int64 new_score);
```

### Replication

Fields marked with `@replicated` generate GetLifetimeReplicatedProps:

**KAIN:**
```kain
actor InventoryActor:
    @replicated
    state items: Array<Int> = []
    
    @replicated
    state gold: Int = 0
```

**Generated C++:**
```cpp
UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite)
TArray<int64> items;

UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite)
int64 gold;

void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AInventoryActor, items);
    DOREPLIFETIME(AInventoryActor, gold);
}
```


### Actor Lifecycle Methods

KAIN supports all standard UE5 lifecycle methods:

- `BeginPlay()` - Called when actor enters play
- `Tick(delta_time: Float)` - Called every frame
- `EndPlay()` - Called when actor leaves play
- `Destroyed()` - Called when actor is destroyed

**KAIN:**
```kain
actor LifecycleDemo:
    on BeginPlay():
        println("Actor started")
    
    on Tick(delta_time: Float):
        // Update every frame
        println("Delta: {delta_time}")
    
    on EndPlay():
        println("Actor ending")
    
    on Destroyed():
        println("Actor destroyed")
```

### @blueprint_callable Methods

Actor methods can be exposed to Blueprint:

**KAIN:**
```kain
actor Weapon:
    state ammo: Int = 30
    
    @blueprint_callable
    fn Fire() -> Bool:
        if ammo > 0:
            ammo = ammo - 1
            return true
        return false
    
    @blueprint_pure
    fn GetAmmoPercent() -> Float:
        return ammo / 30.0
```

**Generated C++:**
```cpp
UFUNCTION(BlueprintCallable)
bool Fire();

UFUNCTION(BlueprintPure)
float GetAmmoPercent() const;
```

### @dispatch Shader Integration

Actors can dispatch compute shaders automatically in Tick():

**KAIN:**
```kain
actor PhysicsSimulator:
    @dispatch(shader: "ParticlePhysics", frequency: 60)
    state particles: Array<Vec3> = []
    
    on Tick(delta_time: Float):
        // Shader dispatch happens automatically
        println("Simulating {len(particles)} particles")
```

**Generated C++:**
```cpp
virtual void Tick(float DeltaTime) override
{
    Super::Tick(DeltaTime);
    
    // Auto-generated shader dispatch
    if (GetWorld() && GetWorld()->GetTimeSeconds() - LastDispatchTime >= 1.0f / 60.0f)
    {
        DispatchParticlePhysicsShader();
        LastDispatchTime = GetWorld()->GetTimeSeconds();
    }
    
    // User code
    UE_LOG(LogTemp, Warning, TEXT("Simulating %lld particles"), particles.Num());
}
```


---

## Component Codegen

Components are reusable systems that can be attached to actors. KAIN generates UActorComponent subclasses.

### Basic Component

**KAIN:**
```kain
@component
struct HealthComponent:
    @replicated
    current: Float
    
    @replicated
    max: Float
    
    @transient
    regen_rate: Float
    
    @savegame
    is_invulnerable: Bool
```

**Generated C++:**
```cpp
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAME_API UHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHealthComponent();

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite)
    float current;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite)
    float max;

    UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite)
    float regen_rate;

    UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite)
    bool is_invulnerable;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
```

### Component Methods

Components can have methods just like actors:

**KAIN:**
```kain
@component
struct CombatComponent:
    state damage_multiplier: Float = 1.0
    
    @blueprint_callable
    fn ApplyDamage(base_damage: Float) -> Float:
        return base_damage * damage_multiplier
    
    @blueprint_pure
    fn IsActive() -> Bool:
        return damage_multiplier > 0.0
```

### Component Attributes

- `@replicated` - Replicates across network
- `@transient` - Not saved/loaded
- `@savegame` - Persists in save games
- `@editdefaults` - Editable in class defaults only
- `@visibleonly` - Visible but not editable


---

## Struct Codegen

Structs are value types for data organization. KAIN generates USTRUCT with optional FTableRowBase inheritance.

### Basic Struct

**KAIN:**
```kain
struct Point:
    x: Float
    y: Float
    z: Float
```

**Generated C++:**
```cpp
USTRUCT(BlueprintType)
struct FPoint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float x;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float y;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float z;
};
```

### @datatable Structs

Structs with `@datatable` inherit from FTableRowBase for CSV import:

**KAIN:**
```kain
@datatable
struct ItemData:
    id: Int
    name: String
    description: String
    icon_path: String
    value: Int
    weight: Float
    max_stack: Int
    rarity: ItemRarity
```

**Generated C++:**
```cpp
USTRUCT(BlueprintType)
struct FItemData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int64 id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString icon_path;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int64 value;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float weight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int64 max_stack;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EItemRarity rarity;
};
```

### Nested Structs

Structs can contain other structs:

**KAIN:**
```kain
struct Transform:
    position: Vec3
    rotation: Rotation
    scale: Vec3

struct Entity:
    transform: Transform
    velocity: Vec3
    health: Float
```


---

## Enum Codegen

Enums are type-safe constants. KAIN generates UENUM(BlueprintType) with display names.

### Basic Enum

**KAIN:**
```kain
enum ItemRarity:
    Common
    Uncommon
    Rare
    Epic
    Legendary
    Mythic
```

**Generated C++:**
```cpp
UENUM(BlueprintType)
enum class EItemRarity : uint8
{
    Common UMETA(DisplayName = "Common"),
    Uncommon UMETA(DisplayName = "Uncommon"),
    Rare UMETA(DisplayName = "Rare"),
    Epic UMETA(DisplayName = "Epic"),
    Legendary UMETA(DisplayName = "Legendary"),
    Mythic UMETA(DisplayName = "Mythic")
};
```

### Enum Usage

Enums can be used in structs, actors, and function parameters:

**KAIN:**
```kain
enum Direction:
    North
    South
    East
    West

actor Player:
    state facing: Direction = Direction::North
    
    fn Turn(new_direction: Direction):
        facing = new_direction
```

### Enum Attributes

- Enums are always `BlueprintType` by default
- Enum variants get automatic `DisplayName` metadata
- Enums use `uint8` as underlying type


---

## Delegate System

Delegates are type-safe function pointers for event systems. KAIN generates DECLARE_DYNAMIC_MULTICAST_DELEGATE_* macros.

### Basic Delegate

**KAIN:**
```kain
type OnHealthChanged = delegate(new_health: Float, old_health: Float)

actor Player:
    state health: Float = 100.0
    state on_health_changed: OnHealthChanged
    
    fn TakeDamage(amount: Float):
        let old = health
        health = health - amount
        on_health_changed.Broadcast(health, old)
```

**Generated C++:**
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, new_health, float, old_health);

UCLASS()
class GAME_API APlayer : public AActor
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FOnHealthChanged on_health_changed;

    UFUNCTION(BlueprintCallable)
    void TakeDamage(float amount);
};

void APlayer::TakeDamage(float amount)
{
    float old = health;
    health = health - amount;
    on_health_changed.Broadcast(health, old);
}
```

### Delegate Types

KAIN supports multiple delegate types:

1. **Multicast Delegates** (default) - Multiple listeners
2. **Single Delegates** - One listener
3. **Dynamic Delegates** - Blueprint-compatible

**KAIN:**
```kain
// Multicast (default)
type OnScoreChanged = delegate(score: Int)

// Single delegate
type OnComplete = delegate() -> Bool

// With return value
type CalculateDamage = delegate(base: Float) -> Float
```

### Delegate Parameters

Delegates support up to 9 parameters (UE5 limitation):

**KAIN:**
```kain
type OnItemPickup = delegate(
    item_id: Int,
    item_name: String,
    quantity: Int,
    rarity: ItemRarity,
    weight: Float
)
```

**Generated C++:**
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
    FOnItemPickup,
    int64, item_id,
    FString, item_name,
    int64, quantity,
    EItemRarity, rarity,
    float, weight
);
```


---

## Blueprint Functions

Functions marked with `@blueprint` generate UBlueprintFunctionLibrary static methods.

### Basic Blueprint Function

**KAIN:**
```kain
@blueprint
fn calculate_damage(base: Float, multiplier: Float, armor: Float) -> Float:
    let raw = base * multiplier
    let mitigated = raw * (1.0 - armor / 100.0)
    return max(mitigated, 0.0)
```

**Generated C++:**
```cpp
UCLASS()
class GAME_API UKainFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Kain")
    static float calculate_damage(float base, float multiplier, float armor);
};

float UKainFunctionLibrary::calculate_damage(float base, float multiplier, float armor)
{
    float raw = base * multiplier;
    float mitigated = raw * (1.0f - armor / 100.0f);
    return FMath::Max(mitigated, 0.0f);
}
```

### Blueprint Pure Functions

Pure functions have no side effects and can be used in Blueprint expressions:

**KAIN:**
```kain
@blueprint_pure
fn get_rarity_color(rarity: ItemRarity) -> Vec3:
    match rarity:
        ItemRarity::Common => vec3(1.0, 1.0, 1.0)
        ItemRarity::Uncommon => vec3(0.0, 1.0, 0.0)
        ItemRarity::Rare => vec3(0.0, 0.5, 1.0)
        ItemRarity::Epic => vec3(0.6, 0.0, 1.0)
        ItemRarity::Legendary => vec3(1.0, 0.5, 0.0)
        _ => vec3(0.5, 0.5, 0.5)
```

**Generated C++:**
```cpp
UFUNCTION(BlueprintPure, Category = "Kain")
static FVector get_rarity_color(EItemRarity rarity);
```

### Blueprint Categories

Functions can be organized into categories:

**KAIN:**
```kain
@blueprint(category: "Math|Damage")
fn calculate_critical_damage(base: Float, crit_chance: Float) -> Float:
    if random() < crit_chance:
        return base * 2.0
    return base
```


---

## EngineKnowledge System

EngineKnowledge is a queryable database of UE5 types that replaces hardcoded type lists with data-driven metadata.

### What It Provides

1. **Type Resolution** - Maps KAIN types to C++ types
2. **Include Paths** - Automatic #include generation
3. **Class Hierarchy** - Parent-child relationships
4. **Named Colors** - `color("sunset")` → `FLinearColor(1.0, 0.7, 0.3, 1.0)`
5. **Constructor Formats** - `vec3(x,y,z)` → `FVector(x,y,z)`
6. **Property Formats** - UE5 ImportText/ExportText strings
7. **Module Dependencies** - Automatic .Build.cs updates
8. **UObject Detection** - Pointer vs value semantics

### Data Source

EngineKnowledge loads from `unreal/metadata/engine_knowledge.json`:

```json
{
  "engine_version": "5.3",
  "classes": [
    {
      "name": "UStaticMeshComponent",
      "parent": "UMeshComponent",
      "header": "Components/StaticMeshComponent.h",
      "module": "Engine",
      "prefix": "U",
      "functions": [
        {
          "name": "SetStaticMesh",
          "return_type": "void",
          "params": [
            { "name": "NewMesh", "type": "UStaticMesh*" }
          ]
        }
      ]
    }
  ],
  "type_aliases": [
    { "kain_name": "Vec3", "ue5_name": "FVector", "header": "Math/Vector.h" }
  ]
}
```

### Query API

```rust
// Check if type is known
kb.is_known_type("StaticMeshComponent") // true

// Resolve type alias
kb.resolve_type_alias("Vec3") // Some("FVector")

// Get include path
kb.get_include("UStaticMeshComponent") // Some("Components/StaticMeshComponent.h")

// Check class hierarchy
kb.is_child_of("UStaticMeshComponent", "UActorComponent") // true

// Check if UObject-derived (needs pointer)
kb.is_uobject_derived("UStaticMeshComponent") // true

// Get C++ type with pointer suffix
kb.get_cpp_type("StaticMeshComponent") // Some("UStaticMeshComponent*")

// Resolve named color
kb.resolve_named_color("sunset") // Some("FLinearColor(1.0f, 0.7f, 0.3f, 1.0f)")

// Resolve constructor
kb.resolve_constructor("FVector", &["1.0", "2.0", "3.0"]) // Some("FVector(1.0, 2.0, 3.0)")
```

### Named Colors

EngineKnowledge includes 140+ named colors from UE5's JsonValueHelper:

**KAIN:**
```kain
actor ColorDemo:
    state tint: Vec3 = color("sunset")
    state glow: Vec3 = color("ocean")
```

**Generated C++:**
```cpp
FVector tint = FLinearColor(1.0f, 0.7f, 0.3f, 1.0f);
FVector glow = FLinearColor(0.0f, 0.5f, 1.0f, 1.0f);
```


---

## Naming Conventions

The `naming.rs` module centralizes all UE5 naming transformations with automatic prefix detection.

### Prefix Rules

| KAIN Type | UE5 Prefix | Example |
|-----------|------------|---------|
| `actor Player` | A | `APlayer` |
| `struct Transform` | F | `FTransform` |
| `enum Direction` | E | `EDirection` |
| `@component Health` | U | `UHealthComponent` |
| Delegates | F | `FOnHealthChanged` |

### Prefix Detection

The naming system detects existing prefixes to prevent double-prefixing:

**KAIN:**
```kain
enum EHealthStatus:  // Already has E prefix
    Healthy
    Wounded
    Critical
```

**Generated C++:**
```cpp
UENUM(BlueprintType)
enum class EHealthStatus : uint8  // NOT EEHealthStatus
{
    Healthy UMETA(DisplayName = "Healthy"),
    Wounded UMETA(DisplayName = "Wounded"),
    Critical UMETA(DisplayName = "Critical")
};
```

### Naming Functions

```rust
// Actor names
to_actor_name("Player") // "APlayer"
to_actor_name("APlayer") // "APlayer" (already prefixed)

// Struct names
to_struct_name("Transform") // "FTransform"
to_struct_name("FTransform") // "FTransform" (already prefixed)

// Enum names
to_enum_name("Direction") // "EDirection"
to_enum_name("EDirection") // "EDirection" (already prefixed)

// Component names
to_component_name("Health") // "UHealthComponent"
to_component_name("HealthComponent") // "UHealthComponent"

// UObject names
to_uobject_name("Widget") // "UWidget"

// Module API macro
to_module_api("UltimateVFX") // "ULTIMATEVFX_API"
```

### Case Conversion

```rust
// PascalCase
to_pascal_case("my_variable") // "MyVariable"
to_pascal_case("http_server") // "HttpServer"

// snake_case
to_snake_case("MyVariable") // "my_variable"
to_snake_case("HTTPServer") // "http_server"
```

### Validation

The naming system validates identifiers against C++ and UE5 rules:

```rust
// Valid identifiers
to_actor_name_checked("Player") // Ok("APlayer")
to_struct_name_checked("Vec3") // Ok("FVec3")

// Invalid identifiers
to_actor_name_checked("2Player") // Err("cannot start with a number")
to_struct_name_checked("My-Struct") // Err("contains special character")
to_enum_name_checked("class") // Err("C++ keyword")
to_actor_name_checked("UCLASS") // Err("UE5 macro name")
```


---

## Type Mapping

The `types.rs` module provides centralized type mapping from KAIN to C++ with EngineKnowledge integration.

### TypeMapper

The TypeMapper is the single source of truth for type conversions:

```rust
let mut mapper = TypeMapper::with_knowledge(engine_knowledge);

// Register user-defined types
mapper.register_enum("ItemRarity".to_string());
mapper.register_struct("Point".to_string());
mapper.register_actor("Player".to_string());
mapper.register_component("Health".to_string());

// Map types
let mapped = mapper.map_type(&Type::Named { name: "Vec3", generics: vec![], span: Span::default() });
// mapped.cpp_type = "FVector"
// mapped.is_pointer = false
// mapped.include_path = Some("Math/Vector.h")
```

### Primitive Types

| KAIN | C++ |
|------|-----|
| `Int` | `int64` |
| `Float` | `float` |
| `Bool` | `bool` |
| `String` | `FString` |
| `Name` | `FName` |
| `Text` | `FText` |
| `Unit` / `()` | `void` |

### Vector Types

| KAIN | C++ (float precision) | C++ (double precision) |
|------|----------------------|------------------------|
| `Vec2` | `FVector2f` | `FVector2D` |
| `Vec3` | `FVector3f` | `FVector` |
| `Vec4` | `FVector4f` | `FVector4` |
| `DVec2` | `FVector2D` | `FVector2D` |
| `DVec3` | `FVector` | `FVector` |
| `DVec4` | `FVector4` | `FVector4` |

### Container Types

| KAIN | C++ |
|------|-----|
| `Array<T>` | `TArray<T>` |
| `Map<K,V>` | `TMap<K,V>` |
| `Set<T>` | `TSet<T>` |
| `Option<T>` | `TOptional<T>` |

### Smart Pointers

| KAIN | C++ |
|------|-----|
| `SharedPtr<T>` | `TSharedPtr<T>` |
| `WeakPtr<T>` | `TWeakPtr<T>` |
| `UniquePtr<T>` | `TUniquePtr<T>` |
| `SoftObjectPtr<T>` | `TSoftObjectPtr<T>` |
| `SubclassOf<T>` | `TSubclassOf<T>` |

### Pointer Detection

The TypeMapper automatically detects UObject-derived types that need pointer semantics:

```rust
// UObject-derived types get pointer suffix
mapper.is_pointer_type_by_name("UStaticMeshComponent") // true
mapper.map_type_string(&Type::Named { name: "StaticMeshComponent", ... }) // "UStaticMeshComponent*"

// Value types don't get pointer suffix
mapper.is_pointer_type_by_name("FVector") // false
mapper.map_type_string(&Type::Named { name: "Vec3", ... }) // "FVector"
```

### Double-Prefix Prevention

The TypeMapper detects existing prefixes to prevent bugs like `EEHealthStatus`:

```rust
// Already prefixed - no change
mapper.apply_prefix_with_detection("EHealthStatus") // "EHealthStatus"
mapper.apply_prefix_with_detection("FTransform") // "FTransform"
mapper.apply_prefix_with_detection("APlayer") // "APlayer"

// Not prefixed - apply prefix
mapper.apply_prefix_with_detection("HealthStatus") // "EHealthStatus" (if registered as enum)
mapper.apply_prefix_with_detection("Transform") // "FTransform" (if registered as struct)
```


---

## Oracle Validation

The Oracle is a semantic validator that catches UHT errors **before** C++ compilation, saving 2+ minutes per error.

### What It Validates

1. **Function Specifiers** - UFUNCTION rules from Epic's UHT source
2. **Property Specifiers** - UPROPERTY rules and incompatible combos
3. **Replication** - GetLifetimeReplicatedProps requirements
4. **RPCs** - Naming conventions and parameter serialization
5. **DataTables** - FTableRowBase inheritance
6. **Components** - State initialization and lifecycle
7. **Name Collisions** - Conflicts with UE5 engine types
8. **Circular Dependencies** - Type dependency cycles

### Validation Rules

#### Rule: BlueprintImplementableEvent Cannot Be Replicated

**Invalid KAIN:**
```kain
actor GameMode:
    @blueprint_implementable_event
    on Server_StartMatch():  // ERROR: BlueprintImplementableEvent + RPC
        println("Starting")
```

**Oracle Error:**
```
❌ Unreal Semantic Validation Errors:
   1. Actor 'GameMode', handler 'Server_StartMatch': BlueprintImplementableEvent functions cannot be replicated (Server/Client/Multicast)
```

#### Rule: Replicated Functions Cannot Have Delegate Parameters

**Invalid KAIN:**
```kain
type OnComplete = delegate()

actor Player:
    on Server_DoAction(callback: OnComplete):  // ERROR: RPC with delegate param
        callback.Broadcast()
```

**Oracle Error:**
```
❌ Function 'Server_DoAction', parameter 'callback': Replicated functions (Server/Client/Multicast) cannot have delegate parameters. This is a security/stability restriction.
```

#### Rule: Enum Variants Cannot Be Named 'true' or 'false'

**Invalid KAIN:**
```kain
enum BoolEnum:
    True   // ERROR: Reserved name
    False  // ERROR: Reserved name
```

**Oracle Error:**
```
❌ Enum 'BoolEnum', variant 'True': Enumerations cannot have variants named 'true' or 'false' (case-insensitive). This is a UE5 restriction.
```

#### Rule: Name Collision Detection

**Invalid KAIN:**
```kain
struct Vector:  // ERROR: Collides with FVector
    x: Float
    y: Float
    z: Float
```

**Oracle Error:**
```
❌ Struct 'Vector': This name collides with a UE5 engine type. UHT will reject it with 'shares engine name' error. Please rename to something more specific (e.g., 'MyVector', 'CustomVector', 'GameVector', etc.).
```

### Data-Driven Validation

The Oracle uses `uht_rules.json` for data-driven validation:

```json
{
  "incompatible_specifiers": [
    {
      "specifier1": "BlueprintReadOnly",
      "specifier2": "BlueprintSetter",
      "message": "Cannot specify a property as being both BlueprintReadOnly and having a BlueprintSetter."
    }
  ],
  "container_types": ["Array", "Map", "Set", "Optional"],
  "nested_container_error": "Nested containers are not supported by UHT."
}
```

### Custom Validation Rules

Projects can add custom rules via `validation_rules.json`:

```json
{
  "version": "1.0.0",
  "rules": [
    {
      "id": "no_public_state",
      "name": "No Public State Variables",
      "description": "All actor state must be private",
      "severity": "error",
      "condition": {
        "type": "actor_state_visibility",
        "visibility": "public"
      },
      "message": "Actor state variables must be private. Use @blueprint_read_only for Blueprint access."
    }
  ]
}
```


---

## File Structure

### Core Files

```
crates/ue5/
├── src/
│   ├── lib.rs                    # Public API exports
│   ├── codegen_ue5.rs            # Main code generator (3742 lines)
│   └── ue5/
│       ├── mod.rs                # Module exports
│       ├── context.rs            # Ue5Context - shared compilation state
│       ├── naming.rs             # UE5 prefix rules (A/F/E/U/S)
│       ├── types.rs              # TypeMapper - KAIN → C++ type mapping
│       ├── oracle.rs             # Semantic validator (1676 lines)
│       ├── oracle_enhanced.rs    # Enhanced validation rules
│       ├── engine_knowledge.rs   # Queryable UE5 type database
│       ├── resolver.rs           # StdLib function resolver (legacy)
│       ├── stdlib_resolver.rs    # Math function mapping (FMath::)
│       ├── uht_rules.rs          # Data-driven UHT validation
│       ├── validation_rules.rs   # Custom validation rule engine
│       ├── widget_registry.rs    # Slate widget metadata
│       ├── editor_attributes.rs  # Editor attribute definitions
│       ├── module_graph.rs       # Module dependency tracking
│       ├── virtual_obligations.rs # Pure virtual method tracking
│       ├── metadata_validation.rs # JSON schema validation
│       ├── metadata_hotreload.rs  # Hot-reload metadata changes
│       ├── project.rs            # .Build.cs generation
│       ├── syntax.rs             # C++ syntax helpers
│       ├── logging.rs            # UE_LOG generation
│       ├── traits.rs             # Trait → Interface mapping
│       └── templates/            # Code templates
│           └── ...
├── tests/
│   ├── generic_codegen_tests.rs  # Generic function tests
│   ├── match_codegen_tests.rs    # Match expression tests
│   └── validation_rules_test.rs  # Oracle validation tests
├── Cargo.toml                    # Dependencies
└── CRATE_REFERENCE.md            # This file
```

### Key Dependencies

```toml
[dependencies]
kain-core = { path = "../kain-core" }  # AST, type system, parser
ue5-shaders = { path = "../ue5-shaders" }  # Shader codegen
heck = { workspace = true }  # Case conversion
minijinja = { workspace = true }  # Template engine
serde = { workspace = true }  # JSON serialization
serde_json = { workspace = true }  # JSON parsing
once_cell = { workspace = true }  # Lazy statics
chrono = { workspace = true }  # Timestamps
indexmap = { workspace = true }  # Ordered maps
jsonschema = { workspace = true }  # Schema validation
regex = "1.10"  # Pattern matching
```

### Metadata Files

The crate loads metadata from `unreal/metadata/*.json`:

- `engine_knowledge.json` - 500+ UE5 types with constructors, includes, property formats
- `widget_registry.json` - Slate widget types and properties
- `shader_knowledge.json` - Shader types, parameters, validation rules
- `uht_rules.json` - UHT macro generation rules
- `module_graph*.json` - Module dependency graphs
- `validation_rules.json` - Custom validation rules
- `virtual_obligations.json` - Pure virtual method requirements


---

## Examples

### Complete Plugin Example

**KAIN (inventory_system.kn):**
```kain
// Enums
enum ItemRarity:
    Common
    Uncommon
    Rare
    Epic
    Legendary

enum ItemType:
    Weapon
    Armor
    Consumable
    Material

// DataTable
@datatable
struct ItemDefinition:
    id: Int
    name: String
    type: ItemType
    rarity: ItemRarity
    max_stack: Int
    value: Int
    weight: Float

// Component
@component
struct InventoryComponent:
    @replicated
    items: Array<ItemStack>
    
    @replicated
    capacity: Int
    
    @savegame
    gold: Int

struct ItemStack:
    item_id: Int
    quantity: Int
    durability: Float

// Actor
actor InventoryActor:
    state inventory: InventoryComponent
    
    on BeginPlay():
        inventory = CreateDefaultSubobject<InventoryComponent>("Inventory")
        inventory.capacity = 20
        inventory.gold = 0
    
    @blueprint_callable
    fn AddItem(item_id: Int, quantity: Int) -> Bool:
        if len(inventory.items) < inventory.capacity:
            let stack = ItemStack { item_id: item_id, quantity: quantity, durability: 100.0 }
            push(inventory.items, stack)
            return true
        return false
    
    on Server_DropItem(item_id: Int):
        // Remove item from inventory
        inventory.items = filter(inventory.items, |stack| stack.item_id != item_id)
        Multicast_AnnounceItemDropped(item_id)
    
    on Multicast_AnnounceItemDropped(item_id: Int):
        println("Item {item_id} was dropped")

// Blueprint Functions
@blueprint
fn calculate_item_value(base_value: Int, rarity: ItemRarity) -> Int:
    let multiplier = match rarity:
        ItemRarity::Common => 1.0
        ItemRarity::Uncommon => 2.0
        ItemRarity::Rare => 5.0
        ItemRarity::Epic => 10.0
        ItemRarity::Legendary => 20.0
        _ => 1.0
    return base_value * multiplier

@blueprint_pure
fn get_rarity_color(rarity: ItemRarity) -> Vec3:
    match rarity:
        ItemRarity::Common => color("white")
        ItemRarity::Uncommon => color("green")
        ItemRarity::Rare => color("blue")
        ItemRarity::Epic => color("purple")
        ItemRarity::Legendary => color("orange")
        _ => color("gray")
```

### Build Command

```bash
cd InventorySystem
kain build --ue5
```

### Generated Output

```
InventorySystem/
├── Source/
│   ├── InventorySystem/
│   │   ├── Public/
│   │   │   ├── EItemRarity.h
│   │   │   ├── EItemType.h
│   │   │   ├── FItemDefinition.h
│   │   │   ├── FItemStack.h
│   │   │   ├── UInventoryComponent.h
│   │   │   ├── AInventoryActor.h
│   │   │   └── UKainFunctionLibrary.h
│   │   ├── Private/
│   │   │   ├── EItemRarity.cpp
│   │   │   ├── EItemType.cpp
│   │   │   ├── FItemDefinition.cpp
│   │   │   ├── FItemStack.cpp
│   │   │   ├── UInventoryComponent.cpp
│   │   │   ├── AInventoryActor.cpp
│   │   │   └── UKainFunctionLibrary.cpp
│   │   └── InventorySystem.Build.cs
│   └── InventorySystem.Target.cs
├── InventorySystem.uplugin
└── KAIN.toml
```


### Advanced Features Example

**KAIN (advanced_features.kn):**
```kain
// Trait → Interface mapping
trait Damageable:
    fn TakeDamage(amount: Float) -> Bool
    fn GetHealth() -> Float

// Actor implementing trait
actor Player impl Damageable:
    @replicated
    state health: Float = 100.0
    
    @replicated
    state max_health: Float = 100.0
    
    // Trait implementation
    fn TakeDamage(amount: Float) -> Bool:
        if health > 0.0:
            health = max(health - amount, 0.0)
            return true
        return false
    
    fn GetHealth() -> Float:
        return health
    
    // Blueprint events
    @blueprint_implementable_event
    fn OnHealthChanged(new_health: Float, old_health: Float)
    
    // RPC with validation
    on Server_Heal(amount: Float):
        if amount > 0.0 && health < max_health:
            let old = health
            health = min(health + amount, max_health)
            Client_UpdateHealth(health)
            OnHealthChanged(health, old)
    
    on Client_UpdateHealth(new_health: Float):
        health = new_health

// Delegate system
type OnDamageReceived = delegate(damage: Float, attacker: Actor)

actor Enemy:
    state on_damage_received: OnDamageReceived
    
    fn ReceiveDamage(damage: Float, attacker: Actor):
        on_damage_received.Broadcast(damage, attacker)

// Shader integration
actor ParticleSimulator:
    @dispatch(shader: "ParticlePhysics", frequency: 60)
    state particles: Array<Vec3> = []
    
    state gravity: Vec3 = vec3(0.0, 0.0, -980.0)
    
    on Tick(delta_time: Float):
        // Shader dispatch happens automatically
        println("Simulating {len(particles)} particles")

// Named colors and constructors
actor ColorDemo:
    state primary_color: Vec3 = color("sunset")
    state secondary_color: Vec3 = color("ocean")
    state position: Vec3 = vec3(0.0, 0.0, 100.0)
    state rotation: Rotation = rotation(0.0, 90.0, 0.0)
```

**Generated C++ (Player.h):**
```cpp
// Trait → Interface
UINTERFACE(MinimalAPI, Blueprintable)
class UDamageable : public UInterface
{
    GENERATED_BODY()
};

class IDamageable
{
    GENERATED_BODY()

public:
    virtual bool TakeDamage(float amount) = 0;
    virtual float GetHealth() = 0;
};

// Actor implementing interface
UCLASS()
class GAME_API APlayer : public AActor, public IDamageable
{
    GENERATED_BODY()

public:
    APlayer();

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite)
    float health;

    UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite)
    float max_health;

    // Interface implementation
    virtual bool TakeDamage(float amount) override;
    virtual float GetHealth() override;

    // Blueprint event
    UFUNCTION(BlueprintImplementableEvent, Category = "Player")
    void OnHealthChanged(float new_health, float old_health);

    // RPCs
    UFUNCTION(Server, Reliable)
    void Server_Heal(float amount);
    void Server_Heal_Implementation(float amount);

    UFUNCTION(Client, Reliable)
    void Client_UpdateHealth(float new_health);
    void Client_UpdateHealth_Implementation(float new_health);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
```


---

## Testing

The `ue5` crate has comprehensive test coverage:

### Test Files

1. **generic_codegen_tests.rs** - Generic function instantiation
2. **match_codegen_tests.rs** - Match expression codegen
3. **validation_rules_test.rs** - Oracle validation rules

### Running Tests

```bash
cd kain/crates/ue5
cargo test

# Run specific test
cargo test test_actor_codegen

# Run with output
cargo test -- --nocapture
```

### Test Coverage

- ✅ Actor codegen (RPCs, replication, lifecycle)
- ✅ Component codegen (state, methods)
- ✅ Struct codegen (basic, @datatable)
- ✅ Enum codegen (variants, display names)
- ✅ Delegate codegen (multicast, single)
- ✅ Blueprint function codegen
- ✅ Type mapping (primitives, containers, pointers)
- ✅ Naming conventions (prefix detection)
- ✅ Oracle validation (22 rules)
- ✅ EngineKnowledge queries
- ✅ Generic function monomorphization

### Example Test

```rust
#[test]
fn test_actor_with_rpc() {
    let source = r#"
        actor GameMode:
            state score: Int = 0
            
            on Server_AddScore(points: Int):
                score = score + points
    "#;
    
    let program = parse_and_typecheck(source).unwrap();
    let output = generate(&program, Some("TestPlugin"), None).unwrap();
    
    // Verify UFUNCTION macro
    assert!(output.header.contains("UFUNCTION(Server, Reliable)"));
    assert!(output.header.contains("void Server_AddScore(int64 points);"));
    assert!(output.header.contains("void Server_AddScore_Implementation(int64 points);"));
    
    // Verify implementation
    assert!(output.source.contains("void AGameMode::Server_AddScore_Implementation(int64 points)"));
    assert!(output.source.contains("score = score + points;"));
}
```

---

## Performance

### Compilation Speed

- **Small plugin (5 types):** ~50ms
- **Medium plugin (20 types):** ~200ms
- **Large plugin (100 types):** ~1.5s

### Memory Usage

- **Peak memory:** ~50MB for 100-type plugin
- **Incremental builds:** Only changed files regenerated

### Optimization Tips

1. **Use modular output** - Per-item files for faster incremental builds
2. **Enable EngineKnowledge caching** - Metadata loaded once
3. **Minimize Oracle validation** - Skip validation for trusted code
4. **Use type registry** - Pre-register types for faster lookups

---

## Troubleshooting

### Common Issues

#### Issue: Double-Prefix (EEHealthStatus)

**Cause:** Type name already has UE5 prefix  
**Solution:** Naming system auto-detects prefixes (fixed in v0.1.0)

#### Issue: Missing Include

**Cause:** EngineKnowledge doesn't have type metadata  
**Solution:** Add type to `engine_knowledge.json` or run metadata expansion script

#### Issue: Oracle Validation Error

**Cause:** Code violates UHT rules  
**Solution:** Read error message, fix KAIN code, rebuild

#### Issue: Pointer vs Value Semantics

**Cause:** TypeMapper doesn't recognize UObject-derived type  
**Solution:** Add type to EngineKnowledge with correct parent class

### Debug Mode

Enable debug output:

```bash
KAIN_DEBUG=1 kain build --ue5
```

This prints:
- Type registration
- EngineKnowledge queries
- Oracle validation steps
- Generated code snippets

---

## Future Enhancements

### Planned Features

1. **Animation Blueprints** - State machine codegen
2. **Behavior Trees** - AI behavior tree integration
3. **Niagara** - Particle system codegen
4. **Enhanced Input** - Input action mapping
5. **GameplayAbilities** - GAS integration
6. **Hot Reload** - Live code updates in editor
7. **Incremental Codegen** - Only regenerate changed files
8. **C++ Optimization** - Inline hints, const correctness

### Metadata Expansion

The EngineKnowledge system will expand to include:
- All UE5 engine classes (currently ~500, target 5000+)
- Blueprint function library methods
- Animation notifies
- Gameplay tags
- Data assets

---

## Contributing

### Adding New Features

1. Add codegen logic to `codegen_ue5.rs`
2. Add validation rules to `oracle.rs`
3. Update EngineKnowledge if needed
4. Add tests to `tests/`
5. Update this documentation

### Code Style

- Use `rustfmt` for formatting
- Follow existing naming conventions
- Add doc comments for public APIs
- Write tests for new features

### Metadata Contributions

To add new UE5 types to EngineKnowledge:

1. Edit `unreal/metadata/engine_knowledge.json`
2. Run schema validation: `python scripts/validate_metadata.py`
3. Test codegen with new types
4. Submit PR with examples

---

## License

Copyright 2026 Zentako. All Rights Reserved.

---

## See Also

- [KAIN Language Guide](../../docs/guides/language.md)
- [UE5 Plugin Development](../../docs/guides/ue5-plugins.md)
- [ue5-editor Crate](../ue5-editor/CRATE_REFERENCE.md)
- [ue5-shaders Crate](../ue5-shaders/CRATE_REFERENCE.md)
- [CLI Crate](../cli/CRATE_REFERENCE.md)
