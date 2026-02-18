# Shader Codegen Pipeline — Current State

**Date:** 2026-02-18  
**Status:** ✅ Fully operational  
**Test suite:** 92/92 passing  
**Validation:** FluidFlow 73-shader plugin builds clean

---

## Pipeline Overview (End-to-End)

```
HyperFluidDynamics_EXPANDED.kn   (1 file, 3,199 lines)
        │
        ▼
  [ KAIN Compiler ]
  ├── Lexer + Parser → AST
  ├── Type Checker → TypedProgram
  └── Oracle (UE5 semantic validation)
        │
        ├─── [ Shader Pipeline ] ──────────────────────────────────────────────
        │         CachedMirrors::from_program(program)  ← computed ONCE
        │         │
        │         ├── generate_cpp_header_cached()    →  ShaderName.h
        │         │     • FXxxComponentData POD struct defs
        │         │     • SHADER_PARAMETER(FXxxComponentData, param)
        │         │     • AddPass_ forward declaration
        │         │
        │         ├── generate_cpp_impl_cached()      →  ShaderName.cpp
        │         │     • IMPLEMENT_GLOBAL_SHADER(...)
        │         │     • void AddPass_ShaderName(FRDGBuilder&, FXxxData, ...)
        │         │
        │         └── generate_single_usf_cached()    →  ShaderName.usf
        │               • struct FXxxComponentData { int/float fields; }
        │               • FXxxComponentData param;
        │               • [numthreads] void ShaderCS(...)
        │
        └─── [ Actor Dispatch ] ────────────────────────────────────────────────
                  type_fields_map (pre-built: struct/actor → fields)
                  component_mirrors (pre-built: component → PodMirrorStruct)
                  │
                  For each shader's component uniforms:
                  │
                  ├── Level 1: direct state match
                  │     actor.state["physics"] → this->physics
                  │
                  └── Level 2: depth-1 path resolution
                        actor.state["world"] (HyperFluidSimulationCore)
                          └── world.physics (PhysicalPropertiesComponent) ✓
                              → this->world->physics
                  │
                  Emits per-shader scoped block:
                  {
                      FPhysicalPropertiesComponentData physics_pod {};
                      if (this->world->physics != nullptr) {
                          physics_pod.viscosity = ...->viscosity;
                          // ... all POD fields
                      }
                      AddPass_LatticeBoltzmannCollision(GraphBuilder, physics_pod, ...);
                  }
```

---

## What Was Broken vs. What Works Now

| Layer | Before | After |
|---|---|---|
| `.h` SHADER_PARAMETER | `SHADER_PARAMETER(PhysicalPropertiesComponent, physics)` — **UHT compile error** | `SHADER_PARAMETER(FPhysicalPropertiesComponentData, physics)` ✅ |
| `.usf` uniform | `PhysicalPropertiesComponent physics;` — **HLSL compile error** | `struct FPhysicalPropertiesComponentData {...}` + `FPhysicalPropertiesComponentData physics;` ✅ |
| `.cpp` AddPass_ | `PhysicalPropertiesComponent physics` — **type error** | `FPhysicalPropertiesComponentData physics` ✅ |
| Actor dispatch (data) | `nullptr->viscosity` — **undefined behaviour / compile error** | `this->world->physics->viscosity` ✅ |
| Actor dispatch (scope) | `physics_pod` declared ×73 in same scope — **C++ redeclaration error** | Each shader in its own `{}` block ✅ |
| Mirror computation cost | `collect_component_mirrors` × 3 per shader × 73 shaders = **219 calls** | 1 call per shader via `compile_shader_artifacts` = **73 calls** ✅ |
| USF filtered program | Structs/enums stripped → mirrors empty → raw component type in USF | Structs/enums forwarded into filtered program before mirror extraction ✅ |
| Component data on GPU | All zeros (no source found) | Real component data via depth-1 path (`this->world->physics`) ✅ |

---

## New Modules / APIs

### `crates/ue5-shaders/src/pod_mirror.rs`
- `collect_component_mirrors(program)` → `HashMap<String, PodMirrorStruct>`
- `PodMirrorStruct::generate_cpp_struct()` → `struct FXxxComponentData { ... }`
- `PodMirrorStruct::generate_hlsl_struct()` → HLSL equivalent
- `PodMirrorStruct::generate_population_code(var, pod_var, indent)` → null-guarded copy block

### `crates/ue5-shaders/src/codegen_usf.rs` — new public API
- `CachedMirrors::from_program(program)` — pre-compute once, share across generators
- `compile_shader_artifacts(program, shader_name, plugin_name) -> ShaderArtifacts` — single-call batch
- `ShaderArtifacts { header, cpp, usf }` — all three artifacts from one mirror computation

### `crates/ue5/src/codegen_ue5.rs` — `Ue5Gen` additions
- `type_fields_map: HashMap<String, Vec<(String, Type)>>` — type-to-fields index
- Depth-1 path resolver in dispatch loop

---

## Proof of Success

```
cargo test -p ue5-shaders -p ue5
  ue5-shaders: 26/26 ✅
  ue5:         66/66 ✅
  Total:       92/92, 0 failures

kain build --ue5 (FluidFlow, 73 shaders)
  ✅ Plugin build complete!
  ⚡ Total shaders: 73
```

### Sample dispatch output (AHyperFluidController.cpp):
```cpp
{   // per-shader scope → no redeclaration across 73 shaders
    FPhysicalPropertiesComponentData physics_pod {};
    if (this->world->physics != nullptr) {           // depth-1 resolved
        physics_pod.viscosity     = static_cast<float>(this->world->physics->viscosity);
        physics_pod.density       = static_cast<float>(this->world->physics->density);
        // ... 25 POD fields
    }
    FTurbulenceComponentData turbulence_pod {};
    if (this->world->turbulence != nullptr) {        // depth-1 resolved
        turbulence_pod.intensity  = static_cast<float>(this->world->turbulence->intensity);
        // ...
    }
    AddPass_LatticeBoltzmannCollision(GraphBuilder, physics_pod, turbulence_pod, PositionOutput, FIntVector(32, 32, 1));
}
```
