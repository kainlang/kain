# Backend Fix Patterns — Cross-Plugin Database

**Last Updated:** Phase 6 Documentation  
**Source:** Materialize, VoxelForgePro, Cinema4DMograph, TemporalBlueprint, MetaFitter

---

## Backend Fix 1: RPC Parameter Serialization Validation

**ID:** `BACK-001`  
**Category:** Oracle  
**Priority:** CRITICAL  
**Status:** FIXED (Materialize)

### Problem
Oracle's `validate_rpcs()` only checked enum names when verifying RPC parameters were serializable. User-defined structs used as RPC parameters were incorrectly rejected.

### Error Message
```
Oracle error: RPC parameter type 'MaterialLayer' is not serializable
```

### Root Cause
Oracle only collected enum names in serializable types list, not struct names.

### Files Modified
- `Kain/crates/ue5/src/ue5/oracle.rs`

### Solution
Modified `validate_rpcs()` to collect both enum and struct names as serializable types.

### Code Changes
```rust
// Before
let serializable_types: HashSet<String> = ast.items.iter()
    .filter_map(|item| match item {
        Item::Enum(e) => Some(e.name.clone()),
        _ => None,
    })
    .collect();

// After
let serializable_types: HashSet<String> = ast.items.iter()
    .filter_map(|item| match item {
        Item::Enum(e) => Some(e.name.clone()),
        Item::Struct(s) => Some(s.name.clone()),
        _ => None,
    })
    .collect();
```

### Impact
- **Materialize:** Fixed 4 RPC validation errors
- **All Plugins:** Any plugin using structs as RPC parameters benefits

---

## Backend Fix 2: USF Type Allowlist Synchronization

**ID:** `BACK-002`  
**Category:** Shader Validation  
**Priority:** CRITICAL  
**Status:** FIXED (Materialize)

### Problem
`validation.rs` had hardcoded allowlist of valid HLSL types that didn't include KAIN's type aliases (`UVec2`, `UInt`, `Mat4`, etc.), even though codegen correctly mapped them.

### Error Message
```
Shader validation error: Type 'UVec2' is not a valid HLSL type
```

### Root Cause
Validator and codegen had separate type lists that were out of sync.

### Files Modified
- `Kain/crates/ue5-shaders/src/validation.rs`

### Solution
Added all KAIN type aliases to the validator's allowlist.

### Code Changes
```rust
// Before
let valid_types = vec![
    "float", "float2", "float3", "float4",
    "int", "int2", "int3", "int4",
    // Missing: UVec2, UInt, Mat4, IVec2, etc.
];

// After
let valid_types = vec![
    "float", "float2", "float3", "float4",
    "int", "int2", "int3", "int4",
    "uint", "uint2", "uint3", "uint4",
    "Float", "Vec2", "Vec3", "Vec4",
    "Int", "IVec2", "IVec3", "IVec4",
    "UInt", "UVec2", "UVec3", "UVec4",
    "Mat2", "Mat3", "Mat4",
    "Sampler2D", "Sampler3D", "SamplerCube",
    "RWBuffer", "RWTexture2D", "RWTexture3D",
];
```

### Recommendation
Replace hardcoded list with call to `TYPE_MAPPER.can_map()` for single source of truth.

### Impact
- **Materialize:** Fixed all shader type validation errors
- **All Plugins:** Any plugin using KAIN type aliases in shaders benefits

---

## Backend Fix 3: Constant Buffer Slot Overflow Check

**ID:** `BACK-003`  
**Category:** Shader Validation  
**Priority:** CRITICAL  
**Status:** FIXED (Materialize)

### Problem
Validator enforced `binding > 13` limit on scalar uniform parameters, treating KAIN's `@N` ordering annotation as D3D11 b-register index. Shaders with 14+ scalar params failed validation.

### Error Message
```
Shader validation error: Constant buffer slot 14 exceeds maximum (13)
```

### Root Cause
Validator misunderstood `@N` semantics. `@N` is an ordering index for scalar params, not a register binding. Scalar params are packed into `SHADER_PARAMETER_STRUCT`, not individual cbuffer registers.

### Files Modified
- `Kain/crates/ue5-shaders/src/validation.rs`

### Solution
Removed incorrect slot range check for scalar parameters. Only textures and UAVs have register limits.

### Code Changes
```rust
// Before
if binding > 13 {
    return Err(format!("Constant buffer slot {} exceeds maximum (13)", binding));
}

// After
// Removed check - scalar params use SHADER_PARAMETER_STRUCT, not individual registers
// Only textures (t0-t127) and UAVs (u0-u63) have register limits
```

### Impact
- **Materialize:** Fixed 16+ errors on `FinalPBRCS` shader (30 scalar params)
- **All Plugins:** Any shader with 14+ scalar parameters benefits

---

## Backend Fix 4: Name Collision Detection (PENDING)

**ID:** `BACK-004`  
**Category:** Oracle  
**Priority:** CRITICAL  
**Status:** NEEDS FIX

### Problem
Oracle detects name collisions with engine types but doesn't provide automatic prefixing strategy or clear resolution path.

### Error Message
```
Error: Enum 'EBlendMode' shares engine name 'EBlendMode' with enum 'EBlendMode' in Engine/EngineTypes.h
Error: Enum 'ETransitionType' shares engine name 'ETransitionType' with enum 'ETransitionType' in Engine/Engine.h
```

### Root Cause
UE5's UHT requires globally unique type names. Plugin types can collide with engine types.

### Files Modified (Proposed)
- `Kain/crates/ue5/src/ue5/oracle.rs`
- `Kain/crates/ue5/src/codegen_ue5.rs`

### Solution (Proposed)
1. Add plugin-specific prefix suggestion in error message
2. Add `@engine_safe_name("ECustomBlendMode")` attribute for manual override
3. Add automatic prefixing in codegen if collision detected and no override provided

### Code Changes (Proposed)
```rust
// In oracle.rs
if collision_detected {
    let suggested_name = format!("E{}{}", plugin_name, original_name.trim_start_matches('E'));
    return Err(format!(
        "Enum '{}' collides with engine type. Suggestion: rename to '{}' or use @engine_safe_name attribute",
        original_name, suggested_name
    ));
}

// In codegen_ue5.rs
fn gen_enum_name(enum_item: &EnumItem, plugin_name: &str) -> String {
    if let Some(override_name) = enum_item.get_attribute("engine_safe_name") {
        return override_name;
    }
    
    // Check for collision and auto-prefix if needed
    if is_engine_collision(&enum_item.name) {
        return format!("E{}{}", plugin_name, enum_item.name.trim_start_matches('E'));
    }
    
    enum_item.name.clone()
}
```

### Impact
- **Materialize:** Would fix 4 enum collision errors
- **TemporalBlueprint:** Would fix 1 enum collision error
- **All Plugins:** Prevents future name collision issues

---

## Backend Fix 5: Struct Field Codegen (PENDING)

**ID:** `BACK-005`  
**Category:** Codegen  
**Priority:** CRITICAL  
**Status:** NEEDS FIX

### Problem
Struct fields not being generated correctly. Missing X, Y, Z members on `FVoxelCoord`.

### Error Message
```
error C2039: 'X': is not a member of 'FVoxelCoord'
error C2039: 'Y': is not a member of 'FVoxelCoord'
error C2039: 'Z': is not a member of 'FVoxelCoord'
```

### Root Cause
Struct field generation in `gen_struct()` may be skipping fields or not emitting them correctly.

### Files Modified (Proposed)
- `Kain/crates/ue5/src/codegen_ue5.rs`

### Solution (Proposed)
1. Verify all fields from KAIN struct are emitted in C++ struct
2. Ensure field names are capitalized correctly (UE5 convention)
3. Add test case for struct with X, Y, Z fields

### Code Changes (Proposed)
```rust
// In codegen_ue5.rs - gen_struct()
fn gen_struct(struct_item: &StructItem) -> String {
    let mut output = String::new();
    
    output.push_str(&format!("USTRUCT(BlueprintType)\nstruct F{} {{\n", struct_item.name));
    output.push_str("    GENERATED_BODY()\n\n");
    
    // Ensure all fields are emitted
    for field in &struct_item.fields {
        let field_name = capitalize_first(&field.name); // X, Y, Z
        let field_type = map_type(&field.type_name);
        output.push_str(&format!("    UPROPERTY(EditAnywhere, BlueprintReadWrite)\n"));
        output.push_str(&format!("    {} {};\n\n", field_type, field_name));
    }
    
    output.push_str("};\n");
    output
}
```

### Impact
- **VoxelForgePro:** Would fix struct field access errors
- **All Plugins:** Ensures struct codegen correctness

---

## Backend Fix 6: RPC Parameter Handling (PENDING)

**ID:** `BACK-006`  
**Category:** Codegen  
**Priority:** CRITICAL  
**Status:** NEEDS FIX

### Problem
RPC function signatures don't match between declaration and implementation. Struct parameters may be passed incorrectly (by value vs by reference).

### Error Message
```
error C2660: 'AVoxelPlayer::Server_Mine': function does not take 1 arguments
error C2660: 'AVoxelPlayer::Server_Place': function does not take 2 arguments
```

### Root Cause
RPC parameter codegen may be inconsistent between header declaration and implementation.

### Files Modified (Proposed)
- `Kain/crates/ue5/src/codegen_ue5.rs`

### Solution (Proposed)
1. Verify RPC parameter codegen in `gen_rpc_function()`
2. Ensure struct parameters use `const FStructName&` (const reference)
3. Add validation for RPC signature consistency between .h and .cpp

### Code Changes (Proposed)
```rust
// In codegen_ue5.rs - gen_rpc_function()
fn gen_rpc_params(params: &[Param]) -> String {
    params.iter().map(|p| {
        let param_type = map_type(&p.type_name);
        
        // Structs should be passed by const reference
        if is_struct_type(&p.type_name) {
            format!("const {}&", param_type)
        } else {
            param_type
        }
    }).collect::<Vec<_>>().join(", ")
}
```

### Impact
- **VoxelForgePro:** Would fix RPC signature mismatch errors
- **All Plugins:** Ensures RPC codegen correctness

---

## Backend Fix 7: Pointer Type Codegen for UE5 Assets (PENDING)

**ID:** `BACK-007`  
**Category:** Type Mapping  
**Priority:** CRITICAL  
**Status:** NEEDS FIX

### Problem
`UAnimSequence*` pointer type not being generated correctly, causing UHT parse error "Found 'end of type' when expecting '*'".

### Error Message
```
Error: Found 'end of type' when expecting '*'
```

### Root Cause
`UAnimSequence` may not be in engine type registry, or pointer suffix not being emitted correctly.

### Files Modified (Proposed)
- `Kain/crates/ue5/src/type_mapper.rs`
- `Kain/unreal/metadata/engine_knowledge.json`

### Solution (Proposed)
1. Add `UAnimSequence` to engine type registry
2. Ensure pointer types are emitted with `*` suffix
3. Verify forward declarations for asset types

### Code Changes (Proposed)
```rust
// In type_mapper.rs
fn map_type(type_name: &str) -> String {
    // Check if it's a UE5 asset type
    if is_ue5_asset_type(type_name) {
        return format!("{}*", type_name);
    }
    
    // ... existing mapping logic
}

// In engine_knowledge.json
{
    "classes": [
        {
            "name": "UAnimSequence",
            "module": "Engine",
            "header": "Animation/AnimSequence.h",
            "is_asset": true
        }
    ]
}
```

### Impact
- **Cinema4DMograph:** Would fix UAnimSequence pointer error
- **All Plugins:** Ensures asset pointer types are handled correctly

---

## Backend Fix 8: Component Naming Convention (PENDING)

**ID:** `BACK-008`  
**Category:** Codegen  
**Priority:** CRITICAL  
**Status:** NEEDS FIX

### Problem
Component names not following UE5 convention. `UClothingLayerManagerComponent` not found.

### Error Message
```
Error: Unable to find 'class', 'delegate', 'enum', or 'struct' with name 'UClothingLayerManagerComponent'
```

### Root Cause
Component naming in `gen_component()` may have bugs:
- Missing `U` prefix
- Missing `Component` suffix
- Double-prefixing issue
- Name transformation bug

### Files Modified (Proposed)
- `Kain/crates/ue5/src/codegen_ue5.rs`

### Solution (Proposed)
1. Verify component naming in `gen_component()`
2. Ensure `U` prefix applied correctly
3. Ensure `Component` suffix applied correctly
4. Check for double-prefixing bugs

### Code Changes (Proposed)
```rust
// In codegen_ue5.rs - gen_component()
fn gen_component_name(component_item: &ComponentItem) -> String {
    let base_name = component_item.name.clone();
    
    // Remove existing U prefix if present
    let base_name = base_name.trim_start_matches('U');
    
    // Remove existing Component suffix if present
    let base_name = base_name.trim_end_matches("Component");
    
    // Apply correct naming: U + Name + Component
    format!("U{}Component", base_name)
}
```

### Impact
- **MetaFitter:** Would fix component naming error
- **All Plugins:** Ensures component naming correctness

---

## Summary Statistics

| Fix ID | Name | Priority | Status | Plugins Affected |
|--------|------|----------|--------|------------------|
| BACK-001 | RPC Param Validation | CRITICAL | ✅ FIXED | Materialize |
| BACK-002 | USF Type Allowlist | CRITICAL | ✅ FIXED | Materialize |
| BACK-003 | CB Slot Overflow | CRITICAL | ✅ FIXED | Materialize |
| BACK-004 | Name Collision | CRITICAL | ⏳ PENDING | Materialize, TemporalBlueprint |
| BACK-005 | Struct Field Codegen | CRITICAL | ⏳ PENDING | VoxelForgePro |
| BACK-006 | RPC Parameter Handling | CRITICAL | ⏳ PENDING | VoxelForgePro |
| BACK-007 | Asset Pointer Types | CRITICAL | ⏳ PENDING | Cinema4DMograph |
| BACK-008 | Component Naming | CRITICAL | ⏳ PENDING | MetaFitter |

**Total Backend Fixes:** 8  
**Completed:** 3  
**Pending:** 5  
**All Critical Priority**
