# 03 Language To UE5 Mapping

This document explains how core Kain constructs map into Unreal Engine 5 output.

## Core Mapping Table

| Kain construct | UE5 output |
|---|---|
| `actor Name` | `AName : public AActor` |
| `@component struct Name` | `UNameComponent : public UActorComponent` |
| `@subsystem struct Name` | `UNameSubsystem : public UWorldSubsystem` |
| `@datatable struct Name` | `FName : public FTableRowBase` |
| `struct Name` | `USTRUCT(BlueprintType) struct FName` |
| `enum Name` | `UENUM(BlueprintType) enum class EName : uint8` |
| delegate alias | UE delegate declaration and support code |
| `@blueprint_callable fn` | `UFUNCTION(BlueprintCallable)` |
| `@blueprint_event fn` | `UFUNCTION(BlueprintNativeEvent)` with `_Implementation` |
| `on Server_*` | server RPC declaration and implementation pattern |
| `@replicated` field | replication declarations plus generated support |
| `@async_task struct` | `FRunnable` plus task queue and callback glue |
| `@state_machine struct` | state enum plus runtime state machine support |

## Naming Rules

The UE5 backend applies Unreal naming conventions automatically:

- actors get `A`
- structs get `F`
- enums get `E`
- UObject-style generated classes get `U`

The backend also tries to avoid double-prefixing when you already named something in Unreal style.

## Actors

Kain actor authoring maps to normal Unreal actor output with lifecycle support.

```kain
actor TestCharacter:
    @replicated
    state health: Float = 100.0

    on BeginPlay():
        println("spawned")
```

Typical generated UE5 features:

- `UCLASS`
- `GENERATED_BODY`
- actor field storage
- lifecycle overrides such as `BeginPlay` and `Tick`
- replicated properties when valid

## Components

Use `@component` when the concept is reusable actor-attached behavior.

```kain
@component
struct HealthComponent:
    @replicated
    current: Float
```

This maps to a generated `UActorComponent` class with UE-style naming.

## Subsystems

Use `@subsystem` for plugin-wide or world-scoped managers.

```kain
@subsystem
struct GameStateSubsystem:
    current_level: Int
```

This maps to `UWorldSubsystem`-style output in the current backend.

## Structs And Data Tables

Plain `struct` becomes a UE-friendly reflected struct.

```kain
struct CharacterStats:
    health: Float
    mana: Float
```

`@datatable` adds the data-table row semantics:

```kain
@datatable
struct ItemData:
    id: Int
    name: String
```

## Enums

Kain enums become UE enums with Blueprint type metadata.

```kain
enum DamageType:
    Physical
    Fire
```

## Replication

Replication exists, but it is one of the areas where UE rules matter a lot.

Valid high-level usage:

- replicated actor state
- replicated component fields
- UE-compatible replicated attribute patterns

Important current rule:

- `@replicated` on plain structs can still lead to invalid Unreal output patterns in some edge cases
- replicate actor or component-owned properties instead of assuming all struct members can replicate independently

## RPCs

RPC generation is driven by naming patterns like:

- `Server_*`
- `Client_*`
- `Multicast_*`

Example:

```kain
on Server_ApplyDamage(damage: Float, attacker: Actor):
    TakeDamage(damage)
```

## Blueprint Surfaces

There are two common Blueprint-facing paths:

### Function-level Blueprint exposure

```kain
@blueprint_callable
fn ApplyDamage(base_damage: Float) -> Float:
    return base_damage
```

### Library-style Blueprint functions

```kain
@blueprint
fn CalculateDamage(base: Float, multiplier: Float) -> Float:
    return base * multiplier
```

## Async Tasks

`@async_task` lowers to UE task-oriented runtime code.

```kain
@async_task
struct DataProcessingTask:
    @input
    input_data: Array<Float>
```

Current backend support includes:

- `FRunnable`-style task generation
- task queue patterns
- callback handling back onto the game thread

## State Machines

Use `@state_machine` for authored state-driven runtime logic.

```kain
@state_machine
struct CombatAnimations:
    @state(entry: true)
    idle:
        animation: "Idle_Anim"
```

## Type Mapping

| Kain | UE5 C++ |
|---|---|
| `Int` | `int32` |
| `Float` | `float` |
| `Bool` | `bool` |
| `String` | `FString` |
| `Array<T>` | `TArray<T>` |
| `Option<T>` | `TOptional<T>` |

## Design Advice

Use Kain authoring at the level where Unreal is verbose:

- reflection-heavy gameplay code
- delegate-heavy code
- RPC-heavy code
- repeated boilerplate around Blueprint surfaces
- subsystem and plugin scaffolding
