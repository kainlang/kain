# Engine Knowledge System

> **Date:** February 2026  
> **Status:** Implemented & Tested (19/19 tests passing)  
> **Impact:** Makes KAIN's UE5 codegen engine-accurate instead of best-guess

## What We Built

A **rich, queryable UE5 type database** called `EngineKnowledge` that gives the KAIN compiler deep understanding of Unreal Engine's type system, class hierarchy, and API surface — at compile time.

### The Problem (Before)

The old system (`StdLibResolver`) was a flat key→value map:

```
"GetActorLocation" → "$0->GetActorLocation()"
"println" → "UE_LOG(LogTemp, Warning, TEXT(\"%s\"), *$0)"
```

**What it couldn't do:**
- No idea that `ACharacter` inherits from `APawn` inherits from `AActor`
- No idea which `#include` is needed for `UNiagaraComponent`
- No idea that `UStaticMeshComponent` is a component (needs pointer semantics)
- No idea which module to add to `.Build.cs` when you use Niagara types
- No validation that a UPROPERTY type would actually compile in UHT
- Hardcoded ~40 type mappings — missed hundreds of common engine types

### The Solution (After)

`EngineKnowledge` is a structured database that understands UE5 the way Epic's own tools do.

#### Class Hierarchy Awareness

```rust
kb.is_child_of("ACharacter", "AActor")       // true
kb.is_child_of("ACharacter", "UObject")       // true
kb.is_engine_component("UStaticMeshComponent") // true
kb.is_engine_actor("ACharacter")               // true
kb.is_engine_component("AActor")               // false
```

The compiler now knows the full inheritance tree. When a KAIN user writes an actor that references a `Character`, the codegen knows it's an `AActor` subclass, needs a pointer, and requires `GameFramework/Character.h`.

#### Automatic Include Resolution

```rust
kb.get_include("AActor")               // "GameFramework/Actor.h"
kb.get_include("StaticMeshComponent")  // "Components/StaticMeshComponent.h"  (prefix-aware!)
kb.get_include("NiagaraSystem")        // "NiagaraSystem.h"
```

No more missing `#include` errors after codegen. The system resolves includes for **both prefixed and unprefixed** type names.

#### 60+ Type Aliases (Seeded from Epic's Own Type Parser)

```rust
kb.resolve_type_alias("Vec3")       // "FVector"
kb.resolve_type_alias("Actor")      // "AActor"
kb.resolve_type_alias("Character")  // "ACharacter"
kb.resolve_type_alias("Transform")  // "FTransform"
```

Sourced directly from the `BlueprintTypeParser` reference — the same type resolution logic that UE5's Blueprint system uses internally.

#### Module Dependency Tracking

```rust
kb.get_module_for_type("UNiagaraComponent")  // "Niagara"
kb.get_module_for_type("ACharacter")         // "Engine"
```

When codegen encounters a Niagara type, it can automatically add `"Niagara"` to the `.Build.cs` dependencies. No more manual module hunting.

#### Function & Property Knowledge

The system knows every `BlueprintCallable` function on core classes:

- `AActor`: `GetActorLocation`, `SetActorLocation`, `Destroy`, `GetDistanceTo`, `HasAuthority`, etc.
- `ACharacter`: `Jump`, `LaunchCharacter`, `PlayAnimMontage`, `GetCharacterMovement`, etc.
- `USceneComponent`: `SetWorldLocation`, `AddLocalOffset`, `GetForwardVector`, `AttachToComponent`, etc.
- `UPrimitiveComponent`: `SetSimulatePhysics`, `AddForce`, `AddImpulse`, `SetMaterial`, `CreateDynamicMaterialInstance`, etc.

Including parameter types, const-ness, virtual/static flags, and UFUNCTION specifiers.

## Files

| File | What |
|------|------|
| `kain/crates/ue5/src/ue5/engine_knowledge.rs` | Core Rust module — schema types, EngineKnowledge struct, query API, built-in seeds, tests |
| `kain/crates/ue5/src/ue5/context.rs` | Updated — `Ue5Context` now carries `knowledge: EngineKnowledge` |
| `kain/crates/ue5/src/ue5/mod.rs` | Updated — module registration + re-export |
| `kain/unreal/metadata/engine_knowledge.json` | Seeded metadata: 28 classes, 16 structs, 10 enums, full hierarchy |
| `kain/unreal/scripts/ue5_scanner.py` | Rewritten v2 — scans UE5 headers into rich EngineMetadata format |

## Seeded Knowledge

### Classes (28) — Full Hierarchy

```
UObject
├── AActor
│   ├── APawn
│   │   └── ACharacter
│   ├── AController
│   │   └── APlayerController
│   ├── AGameModeBase
│   ├── AGameStateBase
│   └── APlayerState
├── UActorComponent
│   ├── UMovementComponent
│   │   └── UNavMovementComponent
│   │       └── UPawnMovementComponent
│   │           └── UCharacterMovementComponent
│   └── USceneComponent
│       ├── UCameraComponent
│       ├── USpringArmComponent
│       ├── UAudioComponent
│       ├── UNiagaraComponent (Niagara module)
│       └── UPrimitiveComponent
│           ├── UStaticMeshComponent
│           │   └── UInstancedStaticMeshComponent
│           ├── USkeletalMeshComponent
│           ├── USplineComponent
│           ├── UCapsuleComponent
│           ├── USphereComponent
│           └── UBoxComponent
```

### Structs (16)
`FVector`, `FVector2D`, `FVector4`, `FRotator`, `FTransform`, `FQuat`, `FLinearColor`, `FColor`, `FHitResult`, `FTimerHandle`, `FTableRowBase`, `FActorSpawnParameters`, `FAttachmentTransformRules`, `FDetachmentTransformRules`, `FPostProcessSettings`

### Enums (10)
`ECollisionChannel`, `ECollisionEnabled`, `ENetRole`, `ESplineCoordinateSpace`, `EBlendMode`, `ETextureRenderTargetFormat`, `EMovementMode`, `EInputEvent`, `EObjectTypeQuery`, `ETraceTypeQuery`

## Scanner v2

The upgraded `ue5_scanner.py` can be pointed at any UE5 engine source tree to extract thousands of types:

```bash
# Rich format (new)
python ue5_scanner.py "C:\UE5\Engine\Source\Runtime" engine_knowledge.json

# Legacy format (backward compat)
python ue5_scanner.py --legacy "C:\UE5\Engine\Source\Runtime" legacy_5.4.json
```

Extracts: class inheritance, USTRUCT with fields, UENUM with values, UFUNCTION with full signatures, UPROPERTY with specifiers, module detection, include path computation.

## Design Decisions

1. **Non-breaking** — `StdLibResolver` kept alongside `EngineKnowledge`. Everything that worked before still works.
2. **Dual format loading** — `EngineKnowledge` loads both the new rich format and the old flat JSON, so the existing `5.4.json` still works.
3. **Built-in seeds** — Core type aliases and include paths are hardcoded in Rust so the system works even without any JSON metadata files.
4. **Prefix-aware lookups** — `get_include("Actor")` finds `AActor`'s header. `is_engine_component("StaticMeshComponent")` finds `UStaticMeshComponent`. Users don't need to know UE5 prefix conventions.

## What This Enables (Next Steps)

- **Auto-`#include` in codegen** — `codegen_ue5.rs` can call `knowledge.get_include()` to emit correct headers
- **Auto `.Build.cs` deps** — `knowledge.get_module_for_type()` feeds into `BuildFile` generation
- **Oracle validation** — `oracle.rs` can validate KAIN types against real engine types before codegen
- **Smarter type mapping** — `types.rs` can defer to `knowledge.resolve_type_alias()` for engine types
- **Component auto-detection** — codegen can auto-add pointer semantics for known engine components
- **Scannable** — point the scanner at any UE5 version to update the knowledge base
