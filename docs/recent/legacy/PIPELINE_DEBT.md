# KAIN Pipeline Debt & Architectural Bugs

This document tracks fundamental flaws in the KAIN compiler's code generation pipeline discovered during the production of the `FluidFlow` and `ToonShaderz` plugins. 

**Status:** These issues are verified to be compiler logic bugs and cannot be fixed by modifying `.kn` source files without compromising project architecture.

---

## 1. Shader POD Redefinition (C2011) ✅ FIXED
**Location:** `kain\crates\ue5-shaders\src\codegen_usf.rs`
**Issue:** The `generate_cpp_header_cached` function called `mirror.generate_cpp_struct()` inline.
**Root Cause:** Each shader header independently emitted the full POD struct definition. Including two such headers from the same dispatch `.cpp` caused `error C2011: 'FPhysicalPropertiesComponentData': 'struct' type redefinition`.
**Fix Applied:**
- Added `pub fn generate_shared_types_header(program, plugin_name) -> Option<String>` that emits all POD structs in sorted order into a single `{Plugin}ShaderTypes.h`.
- `generate_cpp_header_cached` now emits `#include "{Plugin}ShaderTypes.h"` instead of inlining struct definitions.
- `compile_shaders()` in `cli/packager/codegen.rs` writes `{Plugin}ShaderTypes.h` to `Public/` once before the per-shader loop.

---

## 2. Invalid Shader Parameter Macros ✅ FIXED
**Location:** `kain\crates\ue5-shaders\src\codegen_usf.rs`
**Issue:** Uniforms mapped to component POD mirrors were emitted using the `SHADER_PARAMETER` macro.
**Root Cause:** UE5's `BEGIN_SHADER_PARAMETER_STRUCT` requires `SHADER_PARAMETER_STRUCT(FMyStruct, VarName)` for nested structures. `SHADER_PARAMETER` is only valid for scalar primitives and math types (FVector, FMatrix, etc.). Using it with a struct type causes a template specialization failure.
**Fix Applied:** The scalar parameter loop now branches on `component_mirrors.contains_key(ty)`: mirrors use `SHADER_PARAMETER_STRUCT`, all other types use `SHADER_PARAMETER`.

---

## 3. Component-as-Value-Type Bug ✅ FIXED
**Location:** `kain\crates\ue5\src\codegen_ue5.rs`
**Issue:** The compiler generated `@component` fields inside Actors as inline members: `HealthComponent health;`.
**Root Cause:** The `gen_program` pre-pass at line ~660 only registered `TypedItem::Enum` and `TypedItem::Struct` (with `@component` attribute) into the context. `TypedItem::Component` items (declared with the `component` keyword) were silently skipped via `_ => {}`. This caused `context.is_component()` to return `false` for native-keyword components, making `map_type` fall through to a bare name without `U` prefix or `*` pointer.
**Fix Applied:** Added `TypedItem::Component` and `TypedItem::Actor` arms to the pre-pass registration loop. Components now always resolve to `UComponentName*` regardless of source declaration order.

---

## 4. Delegate Type Mapping Failure ✅ FIXED
**Location:** `kain\crates\cli\src\packager\codegen.rs`
**Issue:** The local `map_type` closure inside the delegate generation section used `to_struct_name()` (F prefix) as the fallback for all unknown uppercase types.
**Root Cause:** The closure only checked for enums; any non-enum type (Actor, Component, Struct) fell through to `to_struct_name(name)` → `F{name}`. This caused `Actor` delegate params to emit `FActor*` and actor includes to resolve to `FActor.h`.
**Fix Applied:** Added `is_actor` and `is_component` checks before the struct fallback. Actor params now emit `AActorName*`, component params emit `UComponentName*`, with correct header names for each.

---

## 5. Global Type Collision ⚠️ DEFERRED
**Location:** Core Type Registry / all codegen naming functions
**Issue:** Names like `QualityTier` or `ItemData` collide when multiple KAIN plugins are installed in the same project.
**Verification:**
- `FluidFlow` and `ToonShaderz` both define `QualityTier`.
- KAIN generates `enum class EQualityTier` for both, landing in the global C++ namespace → redefinition error.
**Fix Needed:** Two viable approaches:
1. **`@namespace` attribute** — opt-in per-declaration: `@namespace("ToonShaderz") enum QualityTier` → `EToonShaderzQualityTier`. Low risk, no breaking change.
2. **Auto plugin-prefix** — enabled via `KAIN.toml`: `type_prefix = "ToonShaderz"`. Affects all generated names. Higher impact but fully automatic.
**Design Note:** Approach 1 is preferred as a first step. It requires `Attribute` support on `Enum` and `Struct` AST nodes (already present), a `@namespace` handler in `to_enum_name`/`to_struct_name`, and passing the namespace through all codegen call sites.
**Status:** Deferred — requires cross-cutting naming change. Prioritize when multi-plugin coexistence is needed.
