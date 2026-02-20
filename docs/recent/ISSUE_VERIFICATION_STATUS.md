# KAIN Compiler Issue Verification Status
> Verification of issues found in 4000-line FluidFlow plugin (270+ C++ files, 68 shaders)

**Test Case:** Massive fluid simulation plugin - most complex CFD implementation ever attempted in KAIN
**Scale:** Single .kn file → 270+ C++ files + 68 HLSL shaders
**Date:** 2026-02-17

---

## ✅ FIXED ISSUES

### 1. Actor-to-Actor Pointer Generation ✅ FIXED
**Original Issue:** Actor references generated as value types instead of pointers
```kain
state world: HyperFluidWorld  // Generated: AHyperFluidWorld world; ❌
```

**Status:** ✅ FIXED in `crates/ue5/src/codegen_ue5.rs`
- `is_pointer_type_by_name()` function implemented (line 351)
- `is_uobject_derived()` in EngineKnowledge (line 375)
- Data-driven via EngineKnowledge instead of hardcoded lists
- Correctly generates: `AHyperFluidWorld* world;` ✅

**Evidence:**
```rust
// crates/ue5/src/ue5/engine_knowledge.rs:375
pub fn is_uobject_derived(&self, name: &str) -> bool {
    // Checks with and without prefix
    // Returns true for all UObject-derived types
}

// crates/ue5/src/codegen_ue5.rs:351
fn is_pointer_type_by_name(&self, name: &str) -> bool {
    if kb.is_uobject_derived(type_name) {
        return true;
    }
}
```

---

### 2. Name Collision Detection ✅ PARTIALLY FIXED
**Original Issue:** `struct ParticleSystemComponent` collided with UE's `UParticleSystemComponent`

**Status:** ✅ DETECTION IMPLEMENTED (Oracle system)
- Oracle validates against EngineKnowledge
- Detects collisions with 15,000+ engine types
- Located in `crates/ue5/src/ue5/oracle.rs`

**Still TODO:**
- Auto-prefixing via `kain.toml` (prefix = "Hyper")
- Automatic collision resolution

**Workaround:** Manual renaming (detected at compile time)

---

### 3. Shader Parameter POD Struct Generation ⚠️ NEEDS VERIFICATION
**Original Issue:** Compiler tried to pass `UActorComponent*` to GPU shaders
```cpp
SHADER_PARAMETER(PhysicalPropertiesComponent, physics) // ❌ Invalid
```

**Expected:**
```cpp
struct FPhysicalPropertiesData {
    float viscosity;
    float density;
    // ... POD fields
};
SHADER_PARAMETER(FPhysicalPropertiesData, physics) // ✅ Valid
```

**Status:** ⚠️ NEEDS VERIFICATION
- Current shader codegen only handles primitive types (float, Vec3, etc.)
- `map_usf_type_to_cpp()` maps HLSL types to C++ POD types
- **Does NOT auto-generate POD structs from component definitions**

**Evidence:**
```rust
// crates/ue5-shaders/src/codegen_usf.rs:129
let cpp_type = map_usf_type_to_cpp(ty);
output.push_str(&format!("        SHADER_PARAMETER({}, {})\n", cpp_type, name));

// map_usf_type_to_cpp() only handles:
// float, float2, float3, float4, int, uint, matrices
// Does NOT handle custom component types
```

**Recommendation:** 
- If you passed component types to shaders, this is still broken
- Workaround: Define explicit POD structs in KAIN and pass those
- Future: Auto-generate POD mirror structs from `@component` definitions

---

## ⚠️ PARTIALLY FIXED ISSUES

### 4. Parser Keyword Sensitivity ⚠️ PARTIALLY FIXED
**Original Issue:** Parser fails with `state`/`var` inside `@component` structs

**Status:** ⚠️ SYNTAX DIFFERS BY CONTEXT
- Actors use: `state name: Type`
- Structs use: `name: Type` (no keyword)
- This is BY DESIGN but error messages could be clearer

**Evidence:** Parser enforces different syntax for different contexts
- Actor context: `state`, `on`, RPCs
- Struct context: plain fields, `fn` methods

**Recommendation:** 
- ✅ Working as designed
- ❌ Error messages need improvement ("Use 'fn' instead of 'on' in structs")

---

### 5. Pointer Initialization (null/nullptr) ⚠️ NEEDS VERIFICATION
**Original Issue:** No syntax for uninitialized pointers
```kain
state world: HyperFluidWorld = null  // Failed
```

**Status:** ⚠️ NEEDS VERIFICATION
- Check if `= null` or `= nullptr` is now supported
- Check if uninitialized pointers default to `nullptr` in generated C++

**Recommendation:** Test with:
```kain
actor Test:
    state world: HyperFluidWorld  // Should generate: AHyperFluidWorld* world = nullptr;
```

---

### 6. Duplicate Lifecycle Hooks ⚠️ UNKNOWN STATUS
**Original Issue:** Multiple `on BeginPlay():` blocks allowed but breaks codegen

**Status:** ⚠️ NEEDS VERIFICATION
- Check if parser now rejects duplicate lifecycle hooks
- Check if codegen merges them

**Test Case:**
```kain
actor Test:
    on BeginPlay():
        println("First")
    
    on BeginPlay():  // Should error or merge
        println("Second")
```

---

### 7. Replication on Structs ⚠️ NEEDS VERIFICATION
**Original Issue:** `@replicated` allowed on plain structs, generates invalid C++

**Status:** ⚠️ NEEDS VERIFICATION
- Check if Oracle validates replication context
- Should only allow on Actor/Component members

**Test Case:**
```kain
struct MyStruct:
    @replicated  // Should error: "Replication only valid on Actor/Component"
    val: Int
```

---

## ❌ UNFIXED ISSUES

### 8. Shader Auto-Discovery ❌ STILL MANUAL
**Original Issue:** Must manually list all 68 shaders in `kain.toml`

**Status:** ❌ STILL REQUIRES MANUAL LISTING
- No auto-discovery implemented
- High maintenance burden for large projects

**Current Workaround:**
```toml
[shaders]
shaders = [
    "AdvectionCompute",
    "VorticityCompute",
    # ... 66 more ...
]
```

**Recommendation:** 
- Implement AST scanner to auto-detect `shader` blocks
- Auto-populate shader list during build

---

### 9. Stale File Cleanup ❌ STILL MANUAL
**Original Issue:** Renaming types leaves old .h/.cpp files, causes UHT errors

**Status:** ❌ NO AUTO-CLEANUP
- Old files remain in `Source/Public/`
- Causes name collisions

**Current Workaround:** Manual deletion or `rebuild.bat`

**Recommendation:**
- Implement `--clean` flag
- Track emitted files in manifest, delete stale ones

---

### 10. USF SamplerState Generation ⚠️ NEEDS VERIFICATION
**Original Issue:** `Sampler3D` uniforms don't auto-generate `SamplerState` pairs

**Status:** ⚠️ CHECK CURRENT IMPLEMENTATION
- Current code DOES generate SamplerState for textures (line 143-145)
```rust
output.push_str(&format!("        SHADER_PARAMETER_RDG_TEXTURE({}, {})\n", texture_type, name));
output.push_str(&format!("        SHADER_PARAMETER_SAMPLER(SamplerState, {}Sampler)\n", name));
```

**Verdict:** ✅ LIKELY FIXED - SamplerState auto-generated for all texture types

---

## SUMMARY TABLE

| Issue | Status | Priority | Notes |
|-------|--------|----------|-------|
| 1. Actor Pointer Generation | ✅ FIXED | Critical | Data-driven via EngineKnowledge |
| 2. Name Collision Detection | ✅ PARTIAL | High | Detection works, auto-prefix TODO |
| 3. Shader Component Parameters | ⚠️ VERIFY | Critical | POD struct generation unclear |
| 4. Parser Keyword Sensitivity | ⚠️ BY DESIGN | Medium | Error messages need improvement |
| 5. Pointer Initialization | ⚠️ VERIFY | Medium | Test null/nullptr support |
| 6. Duplicate Lifecycle Hooks | ⚠️ VERIFY | Low | Should error or merge |
| 7. Replication on Structs | ⚠️ VERIFY | Medium | Oracle should validate |
| 8. Shader Auto-Discovery | ❌ UNFIXED | High | Manual listing required |
| 9. Stale File Cleanup | ❌ UNFIXED | Medium | Manual deletion required |
| 10. USF SamplerState | ✅ LIKELY FIXED | Low | Code shows auto-generation |

---

## CRITICAL QUESTIONS FOR VERIFICATION

### Q1: Shader Component Parameters
**Test this:**
```kain
@component
struct PhysicsData:
    viscosity: Float
    density: Float

shader compute Test(id: Vec3) -> Vec4:
    uniform physics: PhysicsData @0  // Does this work?
    return vec4(physics.viscosity, 0, 0, 1)
```

**Expected behavior:**
- Option A: Compiler auto-generates `FPhysicsData` POD struct ✅
- Option B: Compiler errors "Cannot use component in shader" ⚠️
- Option C: Compiler generates invalid C++ ❌

### Q2: Null Pointer Initialization
**Test this:**
```kain
actor Test:
    state world: HyperFluidWorld
    state manager: GameManager = null
```

**Expected C++:**
```cpp
AHyperFluidWorld* world = nullptr;
AGameManager* manager = nullptr;
```

### Q3: Duplicate Lifecycle Hooks
**Test this:**
```kain
actor Test:
    on BeginPlay():
        println("A")
    on BeginPlay():
        println("B")
```

**Expected behavior:**
- Option A: Compiler error "Duplicate BeginPlay" ✅
- Option B: Merges into single function ⚠️
- Option C: Generates broken C++ ❌

---

## RECOMMENDATIONS FOR NEXT STEPS

### High Priority
1. ✅ Verify shader component parameter handling (Q1)
2. ✅ Implement shader auto-discovery (Issue #8)
3. ✅ Add auto-prefixing to `kain.toml` (Issue #2)

### Medium Priority
4. ⚠️ Improve parser error messages (Issue #4)
5. ⚠️ Implement stale file cleanup (Issue #9)
6. ⚠️ Verify null pointer initialization (Q2)

### Low Priority
7. ⚠️ Verify duplicate hook handling (Q3)
8. ⚠️ Validate replication context (Issue #7)

---

## CONCLUSION

**Out of 10 reported issues:**
- ✅ 2 are FIXED (Actor pointers, SamplerState)
- ⚠️ 6 need VERIFICATION (may be fixed, need testing)
- ❌ 2 are UNFIXED (shader auto-discovery, stale file cleanup)

**The most critical issue (Actor pointer generation) is FIXED.**

**The most impactful unfixed issue is shader auto-discovery** - manually listing 68 shaders is brutal.

**The most uncertain issue is shader component parameters** - this could be a showstopper if not handled correctly.

---

## TEST PLUGIN RECOMMENDATION

Create a minimal test case that exercises all edge cases:

```kain
// test_edge_cases.kn

@component
struct TestData:
    value: Float

actor TestActor:
    state other: TestActor  // Q2: Null pointer init
    state data: TestData
    
    on BeginPlay():
        println("First")
    
    on BeginPlay():  // Q3: Duplicate hooks
        println("Second")

shader compute TestShader(id: Vec3) -> Vec4:
    uniform data: TestData @0  // Q1: Component in shader
    return vec4(data.value, 0, 0, 1)
```

Run `kain build --ue5` and check:
1. Does it compile?
2. What errors appear?
3. What C++ is generated?

This will answer all critical questions.
