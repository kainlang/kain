# KAIN-PRO UE5 GODMODE - LLM Agent Guide

## MISSION
Dominate Fab Marketplace. Ship 10x faster than competitors. Generate production UE5 code from KAIN. Zero boilerplate. Zero typos. Maximum velocity.

## IDENTITY
**KAIN-PRO** = Dual GODMODE compiler:
1. **USF GODMODE**: Shaders → .usf + type-safe C++ headers
2. **UE5 GODMODE**: Game code → .h/.cpp with full Blueprint integration

**Binary:** `kain` (globally installed)  
**Syntax:** Python-like with types  
**File ext:** `.kn`

## CORE POWER

### What KAIN-PRO Does
- **Input:** 200 lines of `.kn` code
- **Output:** 12KB of production UE5 C++ + shaders
- **Time:** < 1 second
- **Quality:** Compiler-verified, zero typos
- **Speed:** 10-30x faster than manual

### What Gets Generated
✅ AActor classes with networking (Server/Client/Multicast RPCs)  
✅ UActorComponent classes with replication  
✅ DataTable structs (FTableRowBase, CSV import ready)  
✅ Blueprint function libraries (UBlueprintFunctionLibrary)  
✅ Enums with UMETA display names  
✅ Production shaders with C++ bindings  
✅ All UPROPERTY/UFUNCTION/GENERATED_BODY() macros  
✅ Automatic F/A/E/U prefixing

## CLI COMMANDS

### UE5 Game Code
```bash
kain MyGame.kn -t ue5 -o MyGame
# Outputs: MyGame.h + MyGame.cpp
```

### UE5 Shaders
```bash
kain MyShader.kn -t usf -o MyShader
# Outputs: MyShader.usf + MyShader.h
```

### Watch Mode (Hot Reload)
```bash
kain MyFile.kn -t ue5 -o Output -w
# Auto-recompiles on save
```

### Other Targets
```bash
kain file.kn -t wasm    # WebAssembly
kain file.kn -t js      # JavaScript
kain file.kn -t rust    # Rust
kain file.kn -t cpp     # C++17
kain file.kn -t llvm    # Native binary
```

## LANGUAGE SYNTAX (Ultra-Compact)

### Variables & Functions
```kn
let x: Int = 42          # Immutable
var y = 3.14             # Mutable, type inferred

fn add(a: Int, b: Int) -> Int:
    return a + b
```

### Structs & Enums
```kn
struct Point:
    x: Float
    y: Float

enum Rarity:
    Common
    Rare
    Epic
```

### Pattern Matching
```kn
match value:
    0 => println("zero")
    1 | 2 => println("one or two")
    n if n > 10 => println("big")
    _ => println("other")
```

### Arrays
```kn
let arr = [1, 2, 3, 4]
push(arr, 5)
```

## UE5 ATTRIBUTES

### @datatable - CSV Import Ready
```kn
@datatable
struct ItemData:
    id: Int
    name: String
    value: Int
```
**Generates:** `FItemData : public FTableRowBase`

### @component - Reusable Components
```kn
@component
struct HealthComponent:
    @replicated
    current: Float
    max: Float
```
**Generates:** `UHealthComponent : public UActorComponent`

### @blueprint - Blueprint Functions
```kn
@blueprint
fn calculate_damage(base: Float, mult: Float) -> Float:
    return base * mult
```
**Generates:** `UKainFunctionLibrary::calculate_damage()` (static, Blueprint-callable)

### Property Attributes
```kn
@replicated      # UPROPERTY(Replicated)
@savegame        # UPROPERTY(SaveGame)
@transient       # UPROPERTY(Transient)
@editdefaults    # UPROPERTY(EditDefaultsOnly)
@visibleonly     # UPROPERTY(VisibleAnywhere)
```

## UE5 ACTORS & NETWORKING

### Actor with RPCs
```kn
actor GameMode:
    on Server_StartMatch():
        println("Server: Starting match")
    
    on Client_UpdateScore(score: Int):
        println("Client: Score updated")
    
    on Multicast_BroadcastEvent(msg: String):
        println("All clients: Event broadcast")
```

**Generates:**
- `AGameMode : public AActor`
- `Server_StartMatch()` → `UFUNCTION(Server, Reliable, ...)`
- `Client_UpdateScore()` → `UFUNCTION(Client, Reliable, ...)`
- `Multicast_BroadcastEvent()` → `UFUNCTION(NetMulticast, Reliable, ...)`

### RPC Naming Convention
- `Server_*` → Server RPC (Reliable)
- `Client_*` → Client RPC (Reliable)
- `Multicast_*` → Multicast RPC (Reliable)

## UE5 SHADERS

### Fragment Shader
```kn
shader fragment ColorTint(uv: Vec2) -> Vec4:
    uniform color: Vec3 @0
    uniform intensity: Float @1
    return vec4(color * intensity, 1.0)
```

### Shader with Permutations
```kn
shader fragment Optimized(uv: Vec2) -> Vec4:
    uniform CFG_HIGH_QUALITY: Float @0
    uniform ENABLE_SHADOWS: Float @1
    uniform color: Vec3 @2
    
    var result = color
    
    if CFG_HIGH_QUALITY:
        result = expensive_calc(result)
    else:
        result = cheap_calc(result)
    
    if ENABLE_SHADOWS:
        result = result * shadow_factor()
    
    return vec4(result, 1.0)
```

**Generates:** 4 shader variants (2^2 permutations), zero runtime cost

### Surface Shader (UE5 Material)
```kn
shader surface PBR(uv: Vec2) -> SurfaceOutput:
    uniform roughness: Float @0
    uniform metallic: Float @1
    uniform albedo: Sampler2D @2
    
    var out: SurfaceOutput
    out.base_color = sample(albedo, uv).rgb
    out.roughness = roughness
    out.metallic = metallic
    out.normal = vec3(0, 0, 1)
    return out
```

## TYPE MAPPINGS

| KAIN | UE5 C++ |
|------|---------|
| `Int` | `int64` |
| `Float` | `float` |
| `Bool` | `bool` |
| `String` | `FString` |
| `Vec2` | `FVector2D` |
| `Vec3` | `FVector` |
| `Vec4` | `FVector4` |
| `Array<T>` | `TArray<T>` |
| `Map<K,V>` | `TMap<K,V>` |
| `Option<T>` | `TOptional<T>` |

## NAMING CONVENTIONS (Automatic)

| KAIN | UE5 | Rule |
|------|-----|------|
| `struct Point` | `FPoint` | Structs get `F` |
| `actor Player` | `APlayer` | Actors get `A` |
| `enum State` | `EState` | Enums get `E` |
| `@component Health` | `UHealthComponent` | Components get `U` |

## COMPLETE EXAMPLE

See `llm-guides/complete_ue5_example.kn` for full demo with:
- DataTables
- Enums
- Components
- Actors with networking
- Blueprint functions
- Shaders

## WORKFLOW

### 1. Write KAIN Code
```kn
@datatable
struct WeaponData:
    id: Int
    name: String
    damage: Float

@component
struct AmmoComponent:
    @replicated
    current: Int
    max: Int

actor GameMode:
    on Server_StartGame():
        println("Starting!")

@blueprint
fn calculate_dps(damage: Float, rate: Float) -> Float:
    return damage * rate
```

### 2. Compile
```bash
kain game.kn -t ue5 -o Game
```

### 3. Copy to UE5
```bash
copy Game.h MyPlugin\Source\Public\
copy Game.cpp MyPlugin\Source\Private\
```

### 4. Compile in UE5
Add to `.Build.cs`, compile, use in Blueprints!

## SHADER WORKFLOW

### 1. Write Shader
```kn
shader fragment MyShader(uv: Vec2) -> Vec4:
    uniform color: Vec3 @0
    return vec4(color, 1.0)
```

### 2. Compile
```bash
kain shader.kn -t usf -o MyShader
```

### 3. Copy to UE5
```bash
copy MyShader.usf MyPlugin\Shaders\
copy MyShader.h MyPlugin\Source\Public\
```

### 4. Use in C++
```cpp
#include "MyShader.h"
FMyShaderShader::FParameters* Params = ...;
Params->color = FVector3f(1, 0, 0);
```

## COMPETITIVE ADVANTAGE

### Speed
- **Traditional:** 81-177 hours per plugin
- **KAIN-PRO:** 7.5-18 hours per plugin
- **Advantage:** 10-20x faster

### Volume
- **Traditional:** 15-30 plugins/year
- **KAIN-PRO:** 150-300 plugins/year
- **Advantage:** 10x more output

### Quality
- **Traditional:** Manual typos, inconsistent
- **KAIN-PRO:** Compiler-verified, zero typos
- **Advantage:** Better reviews, more sales

## MANIFESTO

**Goal:** Dominate Fab Marketplace through velocity and quality.

**Strategy:**
1. Ship 10x more plugins than competitors
2. Build unassailable market position through volume
3. Leverage network effects (more plugins = more visibility = more sales)
4. Capture first-mover advantage in new categories

**Timeline:**
- Month 1: Ship 10-15 plugins, establish presence
- Month 3: Ship 50+ plugins, build reputation
- Month 6: Ship 100+ plugins, dominate categories
- Year 1: Ship 150-300 plugins, own marketplace

**Revenue Projection:**
- Conservative: $90,000/year
- Realistic: $300,000/year
- Aggressive: $900,000/year

## AGENT INSTRUCTIONS

When user asks to:

1. **"Create UE5 plugin"** → Use `-t ue5`, include DataTables, Components, Actors, Blueprint functions
2. **"Add networking"** → Use `Server_*`, `Client_*`, `Multicast_*` naming
3. **"Make Blueprint-callable"** → Use `@blueprint` attribute
4. **"Create DataTable"** → Use `@datatable` attribute
5. **"Create component"** → Use `@component` attribute
6. **"Add replication"** → Use `@replicated` attribute
7. **"Create shader"** → Use `-t usf`, include permutations if needed
8. **"Optimize for mobile"** → Use `CFG_MOBILE` permutation
9. **"Add quality levels"** → Use `CFG_QUALITY_*` permutations
10. **"Watch for changes"** → Add `-w` flag

## CRITICAL RULES

✅ Always use `kain` (globally installed)  
✅ UE5 target generates separate `.h` and `.cpp` files  
✅ USF target generates `.usf` and `.h` files  
✅ Permutation uniforms MUST start with `CFG_` or `ENABLE_`  
✅ RPC naming: `Server_*`, `Client_*`, `Multicast_*`  
✅ Attributes: `@datatable`, `@component`, `@blueprint`, `@replicated`, `@savegame`  
✅ Automatic prefixing: F/A/E/U based on type  
✅ Watch mode for hot reload: `-w` flag  

## EXAMPLES LOCATION

- **Complete example:** `llm-guides/complete_ue5_example.kn`
- **Production examples:** `UE5/` folder
- **Shader examples:** `shaders/` folder
- **Documentation:** Root folder

## TOKEN OPTIMIZATION

This guide is optimized for:
- **Minimal tokens** (~1500)
- **Maximum density** (examples > prose)
- **Quick reference** (tables, lists)
- **Pattern-based** (show, don't tell)
- **Action-oriented** (CLI commands first)

## FIREPOWER SUMMARY

**KAIN-PRO gives you:**
- 10-30x faster development
- 10x more plugins per year
- Compiler-verified quality
- Zero boilerplate
- Automatic Blueprint integration
- Automatic networking
- Type-safe everything
- Hot reload support
- Production-ready output

**Use it to dominate.**

---

*KAIN-PRO v0.1.0 | USF GODMODE + UE5 GODMODE | The Ultimate UE5 Weapon*
