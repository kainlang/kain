# Backend Fix Action Plan — Critical Path to Phase 6 Completion

**Priority:** CRITICAL  
**Blocking:** Phase 6 regression suite and final documentation  
**Estimated Time:** 4-8 hours total

---

## Overview

5 backend fixes are required to unblock compilation of all plugins. Each fix is documented below with specific file locations, code changes, and test procedures.

---

## Fix 1: Name Collision Detection (BACK-004)

**Priority:** CRITICAL  
**Affects:** Materialize (4 types), TemporalBlueprint (1 type)  
**Estimated Time:** 2 hours

### Current Errors
```
Materialize:
- EBlendMode shares engine name with Engine/EngineTypes.h
- FLayer shares engine name with Engine/Layers/Layer.h
- FMaterialStatistics shares engine name with MaterialEditor/MaterialEditingLibrary.h
- ENoiseType shares engine name with TextureGraph plugin

TemporalBlueprint:
- ETransitionType shares engine name with Engine/Engine.h
```

### Files to Modify
1. `Kain/crates/ue5/src/ue5/oracle.rs`
2. `Kain/crates/ue5/src/codegen_ue5.rs`

### Implementation Strategy

#### Option A: Auto-Prefixing (Recommended)
Add automatic plugin-specific prefixing when collision detected.

```rust
// In oracle.rs - enhance collision detection
fn check_name_collision(type_name: &str, plugin_name: &str) -> Result<String, String> {
    if is_engine_collision(type_name) {
        // Auto-generate safe name
        let prefix = match type_name.chars().next() {
            Some('E') => format!("E{}", plugin_name), // Enum
            Some('F') => format!("F{}", plugin_name), // Struct
            Some('U') => format!("U{}", plugin_name), // Class
            Some('A') => format!("A{}", plugin_name), // Actor
            _ => plugin_name.to_string(),
        };
        
        let base_name = type_name.trim_start_matches(|c| c == 'E' || c == 'F' || c == 'U' || c == 'A');
        let safe_name = format!("{}{}", prefix, base_name);
        
        return Ok(safe_name);
    }
    Ok(type_name.to_string())
}

// In codegen_ue5.rs - apply safe names
fn gen_enum_name(enum_item: &EnumItem, plugin_name: &str) -> String {
    match check_name_collision(&enum_item.name, plugin_name) {
        Ok(safe_name) => safe_name,
        Err(_) => enum_item.name.clone(),
    }
}
```

#### Option B: Error with Suggestion
Emit clear error with suggested fix.

```rust
// In oracle.rs
if is_engine_collision(type_name) {
    let suggested_name = generate_safe_name(type_name, plugin_name);
    return Err(format!(
        "Type '{}' collides with engine type.\n\
         Suggestion: Rename to '{}' in source file.\n\
         Or add @engine_safe_name(\"{}\") attribute.",
        type_name, suggested_name, suggested_name
    ));
}
```

### Testing
```bash
cd Factory/Materialize
cargo install --path ../../Kain/crates/cli --force
kain build --ue5
# Should succeed or show clear error with suggestion

cd ../TemporalBlueprint
kain build --ue5
# Should succeed or show clear error with suggestion
```

---

## Fix 2: Struct Field Codegen (BACK-005)

**Priority:** CRITICAL  
**Affects:** VoxelForgePro  
**Estimated Time:** 1 hour

### Current Errors
```
error C2039: 'X': is not a member of 'FVoxelCoord'
error C2039: 'Y': is not a member of 'FVoxelCoord'
error C2039: 'Z': is not a member of 'FVoxelCoord'
```

### Files to Modify
1. `Kain/crates/ue5/src/codegen_ue5.rs`

### Root Cause Analysis
Check if `gen_struct()` is:
1. Skipping fields
2. Not capitalizing field names correctly
3. Not emitting UPROPERTY macros

### Implementation
```rust
// In codegen_ue5.rs - gen_struct()
fn gen_struct(struct_item: &StructItem, plugin_name: &str) -> String {
    let mut output = String::new();
    
    let struct_name = format!("F{}", struct_item.name);
    output.push_str(&format!("USTRUCT(BlueprintType)\n"));
    output.push_str(&format!("struct {} {{\n", struct_name));
    output.push_str("    GENERATED_BODY()\n\n");
    
    // Ensure ALL fields are emitted
    for field in &struct_item.fields {
        // Capitalize first letter for UE5 convention
        let field_name = capitalize_first(&field.name);
        let field_type = map_type(&field.type_name);
        
        output.push_str("    UPROPERTY(EditAnywhere, BlueprintReadWrite)\n");
        output.push_str(&format!("    {} {};\n\n", field_type, field_name));
    }
    
    output.push_str("};\n\n");
    output
}

fn capitalize_first(s: &str) -> String {
    let mut chars = s.chars();
    match chars.next() {
        None => String::new(),
        Some(first) => first.to_uppercase().chain(chars).collect(),
    }
}
```

### Testing
```bash
cd Factory/VoxelForgePro
cargo install --path ../../Kain/crates/cli --force
kain build --ue5

# Check generated struct
cat Source/VoxelForgePro/Public/FVoxelCoord.h
# Should show X, Y, Z fields with UPROPERTY macros

./FULLBUILD.bat
# Should compile successfully
```

---

## Fix 3: RPC Parameter Handling (BACK-006)

**Priority:** CRITICAL  
**Affects:** VoxelForgePro  
**Estimated Time:** 1-2 hours

### Current Errors
```
error C2660: 'AVoxelPlayer::Server_Mine': function does not take 1 arguments
error C2660: 'AVoxelPlayer::Server_Place': function does not take 2 arguments
```

### Files to Modify
1. `Kain/crates/ue5/src/codegen_ue5.rs`

### Root Cause Analysis
RPC function signatures inconsistent between:
1. Header declaration
2. Implementation
3. _Validate function

### Implementation
```rust
// In codegen_ue5.rs - gen_rpc_function()
fn gen_rpc_params(params: &[Param]) -> String {
    params.iter().map(|p| {
        let param_type = map_type(&p.type_name);
        let param_name = &p.name;
        
        // Structs should be passed by const reference
        if is_struct_type(&p.type_name) {
            format!("const {}& {}", param_type, param_name)
        } else if is_primitive_type(&p.type_name) {
            format!("{} {}", param_type, param_name)
        } else {
            // Pointers for UObject-derived types
            format!("{}* {}", param_type, param_name)
        }
    }).collect::<Vec<_>>().join(", ")
}

// Ensure consistency across all three locations:
// 1. Header declaration: UFUNCTION(Server, Reliable, WithValidation)
// 2. Implementation: void AClass::FunctionName_Implementation(params)
// 3. Validation: bool AClass::FunctionName_Validate(params)
```

### Testing
```bash
cd Factory/VoxelForgePro
cargo install --path ../../Kain/crates/cli --force
kain build --ue5

# Check generated RPC signatures
grep -A 5 "Server_Mine" Source/VoxelForgePro/Public/AVoxelPlayer.h
grep -A 5 "Server_Mine" Source/VoxelForgePro/Private/AVoxelPlayer.cpp

# Signatures should match exactly
./FULLBUILD.bat
```

---

## Fix 4: Asset Pointer Type Codegen (BACK-007)

**Priority:** CRITICAL  
**Affects:** Cinema4DMograph  
**Estimated Time:** 1-2 hours

### Current Error
```
ZenMographBlueprintLibrary.h(151): Error: Found 'end of type' when expecting '*'
```

### Files to Modify
1. `Kain/crates/ue5/src/type_mapper.rs`
2. `Kain/unreal/metadata/engine_knowledge.json`

### Root Cause Analysis
`UAnimSequence*` not being generated correctly. Likely:
1. Missing from engine type registry
2. Pointer suffix not emitted
3. Missing forward declaration

### Implementation

#### Step 1: Add to engine_knowledge.json
```json
{
  "classes": [
    {
      "name": "UAnimSequence",
      "module": "Engine",
      "header": "Animation/AnimSequence.h",
      "is_asset": true,
      "requires_forward_decl": true
    }
  ]
}
```

#### Step 2: Update type_mapper.rs
```rust
// In type_mapper.rs
pub fn map_type(type_name: &str) -> String {
    // Check if it's a known UE5 asset type
    if is_ue5_asset_type(type_name) {
        return format!("{}*", type_name);
    }
    
    // Check if it's a UObject-derived type
    if type_name.starts_with('U') || type_name.starts_with('A') {
        return format!("{}*", type_name);
    }
    
    // ... existing mapping logic
}

fn is_ue5_asset_type(type_name: &str) -> bool {
    matches!(type_name,
        "UAnimSequence" |
        "USkeletalMesh" |
        "UStaticMesh" |
        "UTexture2D" |
        "UMaterial" |
        "UMaterialInstance"
        // Add more as needed
    )
}
```

#### Step 3: Ensure forward declarations
```rust
// In codegen_ue5.rs - gen_header()
fn gen_forward_declarations(items: &[Item]) -> String {
    let mut forward_decls = HashSet::new();
    
    // Collect all UObject-derived types used
    for item in items {
        collect_used_types(item, &mut forward_decls);
    }
    
    let mut output = String::new();
    for type_name in forward_decls {
        if needs_forward_decl(type_name) {
            output.push_str(&format!("class {};\n", type_name));
        }
    }
    output
}
```

### Testing
```bash
cd Factory/Cinema4DMograph
cargo install --path ../../Kain/crates/cli --force
kain build --ue5

# Check generated header
grep "UAnimSequence" Source/ZenMograph/Public/ZenMographBlueprintLibrary.h
# Should show: UAnimSequence* (with asterisk)

./FULLBUILD.bat
```

---

## Fix 5: Component Naming Convention (BACK-008)

**Priority:** CRITICAL  
**Affects:** MetaFitter  
**Estimated Time:** 1 hour

### Current Error
```
Error: Unable to find 'class', 'delegate', 'enum', or 'struct' with name 'UClothingLayerManagerComponent'
```

### Files to Modify
1. `Kain/crates/ue5/src/codegen_ue5.rs`

### Root Cause Analysis
Component name generation bug. Possible issues:
1. Missing `U` prefix
2. Missing `Component` suffix
3. Double-prefixing
4. Name transformation error

### Implementation
```rust
// In codegen_ue5.rs - gen_component()
fn gen_component_name(name: &str) -> String {
    // Remove existing prefixes/suffixes if present
    let base_name = name
        .trim_start_matches('U')
        .trim_end_matches("Component");
    
    // Apply correct UE5 naming: U + Name + Component
    format!("U{}Component", base_name)
}

// Example transformations:
// "ClothingLayerManager" -> "UClothingLayerManagerComponent"
// "UClothingLayerManager" -> "UClothingLayerManagerComponent"
// "ClothingLayerManagerComponent" -> "UClothingLayerManagerComponent"
// "UClothingLayerManagerComponent" -> "UClothingLayerManagerComponent"

fn gen_component(component_item: &ComponentItem, plugin_name: &str) -> String {
    let component_name = gen_component_name(&component_item.name);
    
    let mut output = String::new();
    output.push_str(&format!("UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))\n"));
    output.push_str(&format!("class {} : public UActorComponent {{\n", component_name));
    output.push_str("    GENERATED_BODY()\n\n");
    output.push_str("public:\n");
    output.push_str(&format!("    {}();\n\n", component_name));
    
    // ... rest of component generation
    
    output
}
```

### Testing
```bash
cd Factory/MetaFitter
cargo install --path ../../Kain/crates/cli --force
kain build --ue5

# Check generated component
ls Source/MetaFitter/Public/*Component.h
# Should show UClothingLayerManagerComponent.h

grep "class U" Source/MetaFitter/Public/UClothingLayerManagerComponent.h
# Should show: class UClothingLayerManagerComponent : public UActorComponent

./FULLBUILD.bat
```

---

## Execution Order

### Sequential Approach (Recommended)
Fix in order of dependency and test each:

1. **Fix 2 (Struct Fields)** - Independent, affects VoxelForgePro only
2. **Fix 3 (RPC Params)** - Depends on Fix 2, affects VoxelForgePro only
3. **Fix 4 (Asset Pointers)** - Independent, affects Cinema4DMograph only
4. **Fix 5 (Component Naming)** - Independent, affects MetaFitter only
5. **Fix 1 (Name Collision)** - Affects multiple plugins, test last

### Parallel Approach (If Multiple Agents)
- **Agent 1:** Fix 2 + Fix 3 (VoxelForgePro)
- **Agent 2:** Fix 4 (Cinema4DMograph)
- **Agent 3:** Fix 5 (MetaFitter) + Fix 1 (Materialize, TemporalBlueprint)

---

## Verification Checklist

After each fix:
- [ ] Run `cargo install --path crates/cli --force`
- [ ] Verify kain.exe timestamp updated
- [ ] Run `kain build --ue5` for affected plugin
- [ ] Check generated C++ files for correctness
- [ ] Run `FULLBUILD.bat` for affected plugin
- [ ] Verify exit code 0
- [ ] Check COMBINEDLOG.md for errors
- [ ] Run Materialize regression test

After all fixes:
- [ ] Run `Factory/_scripts/validate_all_plugins.bat`
- [ ] Verify all 5 plugins compile successfully
- [ ] No errors in COMBINEDLOG.md
- [ ] Proceed to Phase 6 Task 28 (Regression Suite)

---

## Communication

When a fix is complete, update:
1. `Factory/_Docs/PHASE6_STATUS_TRACKER.md` - Mark fix as complete
2. `Factory/_Docs/PATTERNS_BACKEND_FIXES.md` - Update status to ✅ FIXED
3. `Factory/COMBINEDLOG.md` - Should show successful build

---

**End of Action Plan**  
**Ready for Execution by Backend Fix Agents**
