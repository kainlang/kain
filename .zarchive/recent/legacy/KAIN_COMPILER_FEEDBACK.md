# KAIN Compiler Feedback & Codegen Bug Report (UE5 Backend)

This document outlines architectural limitations, parser bugs, and codegen inconsistencies identified during the development of the **FluidFlow** plugin.

## 1. High Priority: Actor Reference Generation
**Issue**: KAIN treats Actor-to-Actor references as value types (structs) rather than pointers in C++.
*   **KAIN Source**: `state world: HyperFluidWorld`
*   **Faulty C++**: `AHyperFluidWorld world;` (Causes UHT to fail because Actors cannot be value members).
*   **Expected C++**: `AHyperFluidWorld* world;`
*   **Workaround Used**: Moving logic into a `@component` (Struct) because KAIN correctly generates component references as `UClass*` pointers.

## 2. Name Collision & Prefixing
**Issue**: The compiler does not automatically namespace or reliably prefix generated types, leading to engine collisions.
*   **Example**: Defining `struct ParticleSystemComponent` collided with UE's `UParticleSystemComponent`.
*   **Recommendation**:
    *   Add an optional `prefix` field to `kain.toml`.
    *   Automatically prefix all generated UObjects/Structs (e.g., `UF_` or `K_`).

## 3. Shader Discovery (Manual `kain.toml`)
**Issue**: Shaders are not auto-discovered. Every `shader compute` block in a `.kn` file must be manually added to the `shaders = []` list in `kain.toml`.
*   **Impact**: High maintenance burden for large projects (e.g., this project has 77 shaders).
*   **Recommendation**: Implement a glob-based discovery or automatic harvesting of `shader` blocks during the build pass.

## 4. Parser Keyword Sensitivity
**Issue**: The parser often fails when using standard keywords (`var`, `state`) inside specific decorators like `@component`.
*   **Errors encountered**:
    *   `Expected identifier, got State` inside a struct.
    *   `Expected identifier, got Var` inside a struct.
*   **Observation**: The parser seems to expect a clean `name: type` format without keywords inside structs, but error messages are opaque.

## 5. Pointer Initialization Context
**Issue**: KAIN lacks a robust syntax for "uninitialized" or `null` pointers for Actors/Objects.
*   **Problem**: `state world: HyperFluidWorld = HyperFluidWorld()` generates a "constructor call" which is invalid for Actors in Unreal (Actors must be spawned).
*   **Attempted `null`**: `state world: HyperFluidWorld = null` failed at various points in the pipeline.

## 6. Duplicate Lifecycle Hooks
**Issue**: Defining multiple `on BeginPlay():` blocks within the same actor is allowed by the parser but causes non-sequential or broken C++ generation.
*   **Recommendation**: The compiler should merge these blocks or warn about duplicates.

## 7. Decoration-Type Mismatches (Replication)
**Issue**: `@replicated` can be placed on members of a basic struct, which generates C++ code attempting to call replication logic on non-AActor/UActorComponent classes.
*   **Impact**: Causes immediate build failure (Compiler should validate that replication only occurs on supported UE5 classes).

## 8. USF SamplerState Generation
**Issue**: When generating `.usf` for a `Sampler3D` uniform, the compiler doesn't always generate the associated `SamplerState` (e.g. `TextureSampler`) parameter required by UE's shader system.
*   **Recommendation**: Shaders with `Sampler3D` or `Sampler2D` should automatically generate the boilerplate `SamplerState` and `Texture` pairs in the C++ Parameter Struct.

## 9. Pathing & Folder Cleanup
**Issue**: The compiler doesn't always "clean up" stale `.h`/.cpp files if a class is renamed in the `.kn` file.
*   **Impact**: Old headers stay in the `Public/` folder and cause name collisions with the new ones during UHT parsing. (Hence why we built `rebuild.bat`).

---
**Status**: These observations were gathered during the transition from a monolithic Actor-based simulation to a Component-based architecture.
