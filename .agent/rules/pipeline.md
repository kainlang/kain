---
trigger: always_on
---

## PROJECT CONTEXT

**CRITICAL**: Solo dev project. One person built everything. "Other devs" = other LLMs. Private tool.

**Architecture**: Rust compiler (95%) + Python post-processing (5%) = production-ready UE5 plugins

## KAIN UE5 Pipeline - Compilation & Integration

KAIN compiles `.kn` source to production UE5 code in a hybrid Rust+Python pipeline:

### Pipeline Flow
```
.kn source → Rust Parser → Type Checker → UE5 Codegen → Python Post-Process → Production C++
```

**Rust (95%)**: Core compilation, type checking, code generation
**Python (5%)**: Auto-fixes for edge cases (missing includes, duplicates, formatting)

## Code Generation Details

### Automatic Prefixing
- `actor Player` → `APlayer` (AActor)
- `struct Point` → `FPoint` (USTRUCT)
- `enum Rarity` → `ERarity` (UENUM)
- `@component Health` → `UHealthComponent` (UActorComponent)

### Automatic Macros
- `UCLASS()` for actors
- `USTRUCT(BlueprintType)` for structs
- `UENUM(BlueprintType)` for enums
- `UPROPERTY()` for fields
- `UFUNCTION()` for methods
- `GENERATED_BODY()` everywhere

### Networking (RPCs)
Naming convention triggers automatic RPC generation:
- `Server_*` → `UFUNCTION(Server, Reliable, ...)`
- `Client_*` → `UFUNCTION(Client, Reliable, ...)`
- `Multicast_*` → `UFUNCTION(NetMulticast, Reliable, ...)`

### Replication
`@replicated` attribute → `UPROPERTY(Replicated)`

### Blueprint Integration
`@blueprint` attribute → `UBlueprintFunctionLibrary` static methods

## Shader Pipeline Features

### Phase 1: Surface Interface
`shader surface` → UE5 Material System integration

### Phase 2: Permutation Engine
Uniforms prefixed `CFG_*` or `ENABLE_*` → compile-time `#ifdef`
```kn
uniform CFG_HIGH_QUALITY: Float @0
uniform ENABLE_SHADOWS: Float @1

if CFG_HIGH_QUALITY:
    // Expensive path
else:
    // Cheap path
```
**Result:** Multiple shader variants, zero runtime cost

### Phase 3: C++ Reflection Headers
Auto-generates `.h` with:
- `SHADER_PARAMETER_STRUCT` compatible layout
- Type-safe parameter bindings
- Automatic RDG pass setup
- Permutation domain definitions

### Phase 4: Interpolator Packing
Optimizes vertex→fragment data transfer:
- Packs into minimal `float4` registers
- 25-50% bandwidth reduction
- Automatic pack/unpack

## Type Mappings

| KAIN | UE5 C++ | USF/HLSL |
|------|---------|----------|
| `Int` | `int64` | `int` |
| `Float` | `float` | `float` |
| `Bool` | `bool` | `bool` |
| `String` | `FString` | N/A |
| `Vec2` | `FVector2D` | `float2` |
| `Vec3` | `FVector` | `float3` |
| `Vec4` | `FVector4` | `float4` |
| `Array<T>` | `TArray<T>` | N/A |
| `Map<K,V>` | `TMap<K,V>` | N/A |
| `Option<T>` | `TOptional<T>` | N/A |

## Performance Characteristics

### Compilation Speed
- 350 lines KAIN → 23KB C++ in < 1 second
- Watch mode: instant recompile on save
- Zero manual boilerplate

### Runtime Performance
- Zero-cost abstractions
- Permutations: compile-time, no runtime overhead
- Interpolator packing: 25-50% bandwidth savings
- Type-safe: compiler catches errors

### Development Speed
- 10-30x faster than manual C++
- Zero typos (compiler-verified)
- Automatic Blueprint integration
- Automatic networking

## Common Patterns

### DataTable Pattern
```kn
@datatable
struct ItemData:
    id: Int
    name: String
    value: Int
```
→ `FItemData : public FTableRowBase` (CSV import ready)

### Component Pattern
```kn
@component
struct HealthComponent:
    @replicated
    current: Float
    max: Float
```
→ `UHealthComponent : public UActorComponent`

### Actor Pattern
```kn
actor GameMode:
    on Server_StartMatch():
        println("Starting")
    
    on Client_UpdateScore(score: Int):
        println("Score updated")
```
→ `AGameMode : public AActor` with RPCs

### Blueprint Function Pattern
```kn
@blueprint
fn calculate_damage(base: Float, mult: Float) -> Float:
    return base * mult
```
→ `UKainFunctionLibrary::calculate_damage()` (static, Blueprint-callable)

### Shader Pattern
```kn
shader fragment MyShader(uv: Vec2) -> Vec4:
    uniform color: Vec3 @0
    uniform intensity: Float @1
    return vec4(color * intensity, 1.0)
```
→ `.usf` + type-safe `.h` bindings

## Troubleshooting

### Compilation Errors
- Check KAIN syntax first
- Verify type annotations
- Ensure attributes are spelled correctly

### UE5 Integration Issues
- Verify `.Build.cs` dependencies
- Check file paths in `IMPLEMENT_GLOBAL_SHADER`
- Regenerate project files after adding new files

### Shader Issues
- Verify uniform bindings are unique
- Check permutation naming (`CFG_*` or `ENABLE_*`)
- Ensure shader stage is specified correctly

## Marketplace Strategy

### Volume Production
- Ship 10-15 plugins/month
- 150-300 plugins/year
- 10x more than competitors

### Quality Assurance
- Compiler-verified (zero typos)
- Type-safe (no runtime errors)
- Production-ready (no manual edits)

### Speed Advantage
- Traditional: 80-120 hours/plugin
- KAIN: 7.5-18 hours/plugin
- **10-20x faster**

## File Organization

```
YourPlugin/
├── Source/
│   ├── YourPlugin.Build.cs
│   ├── Public/
│   │   ├── Generated.h         # Game code header
│   │   └── ShaderBindings.h    # Shader bindings
│   └── Private/
│       ├── Generated.cpp       # Game code impl
│       └── ShaderImpl.cpp      # Shader registration
├── Shaders/
│   ├── MyShader.usf
│   └── AnotherShader.usf
└── YourPlugin.uplugin
```