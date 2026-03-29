# StdlibShowcase — KAIN Stdlib Stress-Test Plugin

> **A UE5 plugin that is also a living test harness for the KAIN standard library**

## Purpose

StdlibShowcase is the official integration test plugin for the KAIN stdlib. It exercises every callable stdlib category through real production UE5 code — actors, subsystems, components, and blueprint functions — verifying that 377 stdlib functions compile correctly and produce valid C++ output.

## What Gets Tested

| Stdlib Category | File | Functions Exercised | Test Location |
|-----------------|------|---------------------|---------------|
| **Actor** | `actor.kn` | GetActorLocation, SetActorLocation, GetActorRotation, GetActorScale, GetActorBounds, GetActorVelocity, AddActorTag, ActorHasTag, RemoveActorTag, SetLifeSpan | `actors.kn → RunActorCategoryTests()` |
| **World** | `world.kn` | GetWorldTimeSeconds, GetWorldDeltaSeconds, IsServer, IsClient, GetActorCount, LineTraceSingle, SphereTraceSingle, DrawDebugLine, DrawDebugSphere, DrawDebugBox, DrawDebugArrow, DrawDebugString, DrawDebugCapsule, GetGravityZ, PrintToScreen, GetAllActorsWithTag | `actors.kn → RunWorldCategoryTests()` |
| **Gameplay** | `gameplay.kn` | apply_damage, calculate_armor_mitigation, calculate_crit_damage, should_crit, calculate_level_from_xp, get_xp_for_level, is_on_cooldown, calculate_buff_value, apply_buff | `actors.kn → RunGameplayCategoryTests()` + `diagnostics.kn` |
| **Math** | `math.kn` | sqrt, pow, abs, min, max, floor, ceil, round, clamp, sin, cos, tan, asin, acos, atan, atan2, random, Vector_Length, Vector_Normalize, Vector_Dot, Vector_Cross, Vector_Distance, Rotator_GetForwardVector, Lerp, VLerp, RLerp, FInterpTo, VInterpTo | `actors.kn → RunMathCategoryTests()` + `diagnostics.kn` |
| **Utilities** | `utilities.kn` | remap, smooth_step, ease_in_out, ping_pong, clamp_float, random_float_range, is_nearly_equal, hsv_to_rgb, int_to_string, float_to_string, bool_to_string | `actors.kn → RunUtilityCategoryTests()` + `diagnostics.kn` |
| **Materials** | `materials.kn` | CreateDynamicMaterialInstance, SetScalarParameter, GetScalarParameter, SetVectorParameter, GetVectorParameter, SetMaterialOpacity, SetMaterialEmissive, SetMaterialRoughness, SetMaterialMetallic, SetMaterial, GetNumMaterials | `actors.kn → StdlibTestProxyActor::RunMaterialTest()` + `StdlibMaterialAnimator` |
| **Particles** | `particles.kn` | SpawnEmitterAtLocation, ActivateSystem, DeactivateSystem, SetFloatParameter, SetVectorParameter, SetColorParameter, SetBoolParameter, SetNiagaraVariableInt, IsNiagaraSystemActive, ResetNiagaraSystem | `actors.kn → StdlibTestProxyActor::RunParticleTest()` |
| **Skeletal Mesh** | `skeletal_mesh.kn` | Declaration-level coverage via StdlibHealthComponent tick | `components.kn` |

> **Shaders excluded** — known `String` type validator-codegen mismatch blocks shader stdlib in UE5 build context. Shader stdlib is validated separately.

## Plugin Architecture

```
StdlibShowcase/
├── KAIN.toml                    # Data-driven: 2-module (Runtime + Editor)
├── FULLBUILD.bat                # 3-phase build: kain → C++ → UE5 DLL
├── Kain/
│   ├── types.kn       (217 lines)  Enums, structs, helper fns (no stdlib deps)
│   ├── components.kn  (272 lines)  StdlibHealthComponent, MathComponent, UtilityComponent
│   ├── actors.kn      (605 lines)  Manager, ProxyActor, MaterialAnimator
│   ├── subsystem.kn   (268 lines)  UWorldSubsystem — aggregate stats + stress tests
│   ├── diagnostics.kn (342 lines)  @blueprint diag_ functions — pure stdlib tests
│   └── editor.kn      (124 lines)  Toolbar, Details panel, Slate widget, Viewport
└── _Builds/                     # Generated output (kain build --ue5)
```

**Total KAIN source: ~1,828 lines**  
**Estimated C++ output: 9,000–14,000 lines** (1:5 to 1:8 compression from KAIN syntax alone, targeting 1:20 combined with stdlib)

## Running the Diagnostics

### Build (Phase 1: KAIN → C++)
```cmd
cd M:\Code\Factory\StdlibShowcase
kain build --ue5 --verbose
```

### Full Build (Phase 1-3: KAIN → C++ → UE5 DLL → Validate)
```cmd
FULLBUILD.bat
```

### Quick Interpreter Run (no C++ compile needed)
```cmd
kain run Kain/diagnostics.kn
```

### What You Should See (Successful Run)
```
StdlibShowcase [MANAGER] BeginPlay — stdlib stress-test initialised
StdlibShowcase [ACTOR]  Spawn origin: 0.0
StdlibShowcase [WORLD]  WorldTime at start: 0.0
StdlibShowcase [MATH]   sqrt(16) = 4.0
StdlibShowcase [MATH]   pow(2,8) = 256.0
StdlibShowcase [UTIL]   random_float_range(10,20) = 14.37...
StdlibShowcase [GAMEPLAY] HP after 5 hits: 49.2...
StdlibShowcase [GAMEPLAY] Level after 250 XP: 3
...
=== STDLIB DIAGNOSTICS COMPLETE ===
Tests: 18
Passed: 18
Failed: 0
*** ALL STDLIB TESTS PASSED ***
```

## Module Structure

| Module | Type | Source Files | Dependencies |
|--------|------|-------------|-------------|
| `StdlibShowcase` | Runtime | types, components, actors, subsystem, diagnostics | Core, Engine, Niagara, NetCore |
| `StdlibShowcaseEditor` | Editor | editor | UnrealEd, Slate, PropertyEditor, ToolMenus |

## Key Patterns Demonstrated

### Stdlib Composition (gameplay.kn + math.kn)
```kain
fn ApplyHit(raw_damage: Float, attacker: Actor) -> Float:
    let mitigated = calculate_armor_mitigation(raw_damage, armor)  # gameplay
    if should_crit(crit_chance):                                    # gameplay
        let crit_dmg = calculate_crit_damage(mitigated, 2.0)       # gameplay
        current_health = apply_damage(current_health, max_health, crit_dmg, 0.0) # gameplay
        return crit_dmg
    current_health = apply_damage(current_health, max_health, mitigated, 0.0)    # gameplay
    return mitigated
```

### Live Material Animation (materials.kn + utilities.kn + math.kn)
```kain
on Tick(delta_time: Float):
    phase = phase + delta_time
    let hue = remap(phase, 0.0, 6.2832, 0.0, 360.0)    # utilities
    let rgb = hsv_to_rgb(vec3(hue, 0.9, 1.0))            # utilities
    SetVectorParameter(dyn_mat, "BaseColor", rgb)         # materials
    let roughness = smooth_step(0.0, 1.0, (sin(phase) + 1.0) * 0.5)  # math + utilities
    SetMaterialRoughness(dyn_mat, roughness)              # materials
```

### World-Space Debug Overlay (world.kn + math.kn)
```kain
let debug_pos = spawn_origin + vec3(sin(t) * 100.0, cos(t) * 100.0, 50.0)  # math
DrawDebugSphere(debug_pos, 25.0, vec3(0.0, 1.0, 0.5), 0.1)                  # world
DrawDebugArrow(spawn_origin, debug_pos, vec3(1.0, 0.8, 0.0), 0.1, 2.0)      # world
```

## Extend / Contribute

To add a new stdlib function test:

1. Write the function in the appropriate stdlib file (`Kain/stdlib/ue5/`)
2. Add a `diag_<category>_<feature>()` function in `diagnostics.kn`
3. Wire it into `RunFullStdlibDiagnostics()`
4. Rebuild and verify pass count increases

Following the existing pattern ensures the diagnostic runner stays data-complete and every new stdlib addition is automatically exercised on every build.

---

**KAIN Stdlib Version:** 1.0.0 (377 functions, 12 files)  
**Tested Categories:** 7/8 callable (shaders excluded — known validator issue)  
**Plugin Status:** Integration test target — rebuild after every stdlib change
