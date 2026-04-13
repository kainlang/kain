# POD Mirror Struct System — Implementation Log

**Date:** 2026-02-17  
**Status:** ✅ Complete & Validated

---

## Problem

`@component` types used as shader uniforms generated invalid output across all three artifact types:

| Artifact | Before | After |
|---|---|---|
| `.h` SHADER_PARAMETER | `SHADER_PARAMETER(PhysicalPropertiesComponent, physics)` | `SHADER_PARAMETER(FPhysicalPropertiesComponentData, physics)` |
| `.usf` uniform | `PhysicalPropertiesComponent physics;` | `FPhysicalPropertiesComponentData physics;` |
| `.cpp` AddPass_ signature | `PhysicalPropertiesComponent physics` | `FPhysicalPropertiesComponentData physics` |
| Actor dispatch | `this->physics` (UObject* → GPU boundary violation) | `FPhysicalPropertiesComponentData physics_pod {};` (zero-init POD) |

Root cause: components are `UActorComponent` subclasses with vtables, GC headers, and non-POD fields (`TArray`, etc.) — they cannot cross the CPU/GPU boundary directly.

---

## Solution: POD Mirror Struct Auto-Generation

### New Module: `crates/ue5-shaders/src/pod_mirror.rs`

Scans `TypedProgram` for `@component` structs referenced by shader uniforms and extracts only GPU-compatible fields:

**POD-compatible:** `Float`, `Int`, `UInt`, `Bool`, `Vec2/3/4`, enum types  
**Skipped silently:** `Array<T>`, nested components, unknown named types  
**Hard error:** component used in shader with zero extractable POD fields

Generates `F{ComponentName}Data` mirror structs. Example:

```cpp
// C++ header (.h)
struct FPhysicalPropertiesComponentData {
    EFluidClass fluid_class;
    ESolverFamily solver_family;
    float viscosity;
    float density;
    // ... 21 fields total (coupling_fields: Array<CouplingField> silently skipped)
};
```

```hlsl
// HLSL (.usf)
struct FPhysicalPropertiesComponentData {
    int fluid_class;      // enums → int in HLSL
    int solver_family;
    float viscosity;
    float density;
    // ...
};
```

### Files Modified

- `crates/ue5-shaders/src/pod_mirror.rs` — new module (524 lines, 8 unit tests)
- `crates/ue5-shaders/src/lib.rs` — export `pod_mirror`, `PodMirrorStruct`, `PodField`, `collect_component_mirrors`
- `crates/ue5-shaders/src/codegen_usf.rs` — 4 integration points:
  - `generate_cpp_header()`: emit POD struct defs before class, use in `SHADER_PARAMETER` + forward decl
  - `generate_cpp_implementation()`: use POD type in `AddPass_` signature
  - `generate()` (USF): emit HLSL struct defs, replace component type declarations
  - `generate_single_usf_from_program()`: forward all `Struct`/`Enum` items into filtered program (bug fix — was stripping them)
- `crates/ue5/src/codegen_ue5.rs` — actor dispatch:
  - `Ue5Gen` struct: added `component_mirrors` field
  - Pre-pass: compute mirrors once from `TypedProgram`
  - Dispatch loop: detect component uniforms, emit `{}` scoped POD population + zero-init fallback

### Bugs Fixed During Validation

1. **`generate_single_usf_from_program` stripped context** — filtered program had no `@component`/enum definitions → mirrors returned empty. Fixed: forward all struct/enum items before appending the target shader.

2. **`nullptr->field` compile error** — when no actor state matched a component uniform (e.g., `physics` not a direct state on `HyperFluidController`), fell back to `"nullptr"` → emitted `nullptr->viscosity`. Fixed: when `component_var == "nullptr"`, emit zero-init declaration only.

3. **Variable redeclaration across 73 shaders** — all shaders in the same ENQUEUE_RENDER_COMMAND lambda → `FPhysicalPropertiesComponentData physics_pod {}` declared 73× in the same scope. Fixed: wrap each shader's prep+dispatch in its own `{}` block scope.

---

## Proof of Success

### Test Results

```
ue5-shaders: 26/26 passing
ue5:         66/66 passing
Total:       92/92 passing, 0 failures
```

### FluidFlow Plugin (73 shaders, 1 KAIN source file)

```
✅ Plugin build complete!
📍 Location: M:\Kain-Lang\kain-private\kain\FluidFlow
⚡ Total shaders: 73
✅ Applied 77 auto-fixes
```

### Generated Output Samples

**`VortexMethodCore.h`** (was broken, now correct):
```cpp
struct FPhysicalPropertiesComponentData {
    EFluidClass fluid_class;
    // ... 25 POD fields
};
class FVortexMethodCoreShader : public FGlobalShader {
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(FPhysicalPropertiesComponentData, physics)  // ✅
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
    END_SHADER_PARAMETER_STRUCT()
```

**`VortexMethodCore.usf`** (was broken, now correct):
```hlsl
struct FPhysicalPropertiesComponentData {     // ✅ HLSL struct definition
    int fluid_class;
    float viscosity;
    // ...
};
FPhysicalPropertiesComponentData physics;     // ✅ POD type declaration
```

**`AHyperFluidController.cpp`** dispatch (was broken, now correct):
```cpp
{
    FPhysicalPropertiesComponentData physics_pod {};   // ✅ zero-init, scoped
    FTurbulenceComponentData turbulence_pod {};        // ✅
    AddPass_LatticeBoltzmannCollision(GraphBuilder, physics_pod, turbulence_pod, ...);
}
{
    FPhysicalPropertiesComponentData physics_pod {};   // ✅ no redeclaration
    AddPass_LatticeBoltzmannStreaming(GraphBuilder, physics_pod, ...);
}
```

---

## Policy Decisions (v1)

| Scenario | Policy |
|---|---|
| Non-POD field in shader-used component | Silently skipped (zero impact on GPU-accessible fields) |
| Component with zero extractable POD fields | Hard error via `Err(String)` |
| No matching actor state for component uniform | Zero-init POD fallback (no field copy) |
| Shader scope | Compute + fragment (both supported) |
| Nested component flattening | Not implemented in v1 |
