# KAIN Pipeline Debt & Architectural Bugs

This document tracks fundamental flaws in the KAIN compiler's code generation pipeline discovered during the production of the `FluidFlow` and `ToonShaderz` plugins. 

**Status:** These issues are verified to be compiler logic bugs and cannot be fixed by modifying `.kn` source files without compromising project architecture.

---

## 1. Shader POD Redefinition (C2011)
**Location:** `kain\crates\ue5-shaders\src\codegen_usf.rs` (Lines 664-668)
**Issue:** The `generate_cpp_header_cached` function calls `mirror.generate_cpp_struct()` inline. 
**Verification:** 
- If `ShaderA` and `ShaderB` both use `PhysicalPropertiesComponent`, both `ShaderA.h` and `ShaderB.h` will contain the full definition of `struct FPhysicalPropertiesComponentData`.
- Including both headers in a single dispatch C++ file results in `error C2011: 'FPhysicalPropertiesComponentData': 'struct' type redefinition`.
**Fix Needed:** Codegen must emit shared POD types into a single `{PluginName}ShaderTypes.h` and have individual shaders include that header.

## 2. Invalid Shader Parameter Macros
**Location:** `kain\crates\ue5-shaders\src\codegen_usf.rs` (Line 708)
**Issue:** Uniforms mapped to component POD mirrors are emitted using the `SHADER_PARAMETER` macro.
**Verification:**
- UE5's `BEGIN_SHADER_PARAMETER_STRUCT` requires `SHADER_PARAMETER_STRUCT(FMyStruct, VarName)` for nested structures.
- KAIN currently emits `SHADER_PARAMETER(FMyStructData, VarName)`, which causes a template specialization failure in the Unreal build system.
**Fix Needed:** Update the scalar parameter loop to distinguish between primitives and KAIN-generated POD structs.

## 3. Component-as-Value-Type Bug
**Location:** `kain\crates\ue5\src\ue5\syntax.rs` (Reflected in `ARPGPlayer.h`)
**Issue:** The compiler generates `@component` fields inside Actors as inline members: `HealthComponent health;`.
**Verification:**
- In UE5, components must be pointers (`UHealthComponent*`) and ideally decorated with `Instanced` or `Export` if they are subobjects.
- Bare value types of `UObject` derived classes are illegal in C++.
**Fix Needed:** Actor state generation must detect if a type is a Component and automatically emit it as a pointer.

## 4. Delegate Type Mapping Failure
**Location:** `kain\crates\cli\src\packager\codegen.rs` (Lines 165-195)
**Issue:** The type mapper for delegates incorrectly assumes KAIN standard names map to UE5 `F` prefixes for everything.
**Verification:**
- References to `Actor` in a delegate became `#include "FActor.h"`.
- References to `DamageType` (enum) became `FDamageType`.
**Fix Needed:** The delegate generator must use the central `ue5::naming` module to correctly identify prefixes (A for Actors, E for Enums, U for Components).

## 5. Global Type Collision
**Location:** Core Type Registry
**Issue:** Names like `QualityTier` or `ItemData` collide when multiple KAIN plugins are installed in the same project.
**Verification:**
- `FluidFlow` and `ToonShaderz` both defined `QualityTier`.
- Since KAIN generates `enum class EQualityTier`, the second plugin causes a redefinition error because they both land in the global C++ namespace.
**Fix Needed:** Introduction of a `@namespace` or automatic plugin-prefixing for all generated UE5 types.
