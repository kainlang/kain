# Member Access Codegen Implementation Summary

## Overview
Implemented proper member access operator selection (-> vs .) for UObject-derived types in the KAIN UE5 backend.

## Problem
The previous implementation only checked if the immediate identifier was a pointer type, but didn't properly handle:
- Member access chains (e.g., `actor.component.field`)
- Field type resolution (checking if the accessed field itself is a pointer type)
- Mixed type scenarios (value types containing pointer types and vice versa)

## Solution

### 1. Enhanced `is_pointer_receiver` Method
**Location:** `Kain/crates/ue5/src/codegen_ue5.rs`

The method now:
- Recursively resolves expression types through member access chains
- Looks up field types in `type_fields_map` to determine if a field is a pointer type
- Properly handles `self.field` access patterns
- Falls back gracefully when type information is unavailable

**Key Logic:**
```rust
fn is_pointer_receiver(&self, expr: &Expr) -> bool {
    match expr {
        Expr::Ident(name, _) => {
            self.is_pointer_type_by_name(name)
        }
        Expr::Field { object, field, .. } => {
            // Resolve the object type, then check if the field is a pointer type
            if let Some(obj_type) = self.infer_expr_type(object) {
                if let Some(fields) = self.type_fields_map.get(&obj_type) {
                    for (field_name, field_type) in fields {
                        if field_name == field {
                            if let Type::Named { name, .. } = field_type {
                                return self.is_pointer_type_by_name(name);
                            }
                        }
                    }
                }
            }
            // Fallback for self.field
            ...
        }
        _ => false,
    }
}
```

### 2. New `infer_expr_type` Method
**Location:** `Kain/crates/ue5/src/codegen_ue5.rs`

Recursively infers the KAIN type name of an expression:
- Checks `var_types` map for variable type information
- Recursively resolves field access chains
- Returns `Option<String>` with the type name

**Key Logic:**
```rust
fn infer_expr_type(&self, expr: &Expr) -> Option<String> {
    match expr {
        Expr::Ident(name, _) => {
            self.var_types.get(name).cloned()
        }
        Expr::Field { object, field, .. } => {
            // Recursively resolve object type, then look up field type
            if let Some(obj_type) = self.infer_expr_type(object) {
                if let Some(fields) = self.type_fields_map.get(&obj_type) {
                    for (field_name, field_type) in fields {
                        if field_name == field {
                            if let Type::Named { name, .. } = field_type {
                                return Some(name.clone());
                            }
                        }
                    }
                }
            }
            None
        }
        _ => None,
    }
}
```

## Test Coverage

Created comprehensive test suite: `Kain/crates/ue5/tests/member_access_tests.rs`

### Test Categories (15 tests total):

#### A. Value Type Member Access (uses .)
- ✅ `test_vector_member_access_uses_dot` - Vec3 field access
- ✅ `test_primitive_struct_member_access` - Primitive fields in structs
- ✅ `test_nested_value_type_access` - Nested value type chains

#### B. UObject-Derived Type Member Access (uses ->)
- ✅ `test_component_member_access_uses_arrow` - Component field access
- ✅ `test_actor_reference_member_access` - Actor field access
- ✅ `test_subsystem_member_access` - Subsystem field access

#### C. Mixed Type Member Access
- ✅ `test_component_with_value_type_field` - Component->Vec3.field
- ✅ `test_struct_with_component_field` - Struct.Component->field
- ✅ `test_deep_nesting_mixed_types` - Component->Struct.Vec3.field

#### D. Engine Type Member Access
- ✅ `test_engine_uobject_types` - UStaticMeshComponent, UTexture2D
- ✅ `test_engine_value_types` - FVector, FRotator

#### E. Self Member Access
- ✅ `test_self_value_field_access` - self.primitive_field
- ✅ `test_self_component_field_access` - self.component->field

#### F. Array and Collection Member Access
- ✅ `test_array_length_access` - Array.Num() mapping
- ✅ `test_component_array_access` - Array of components

**All 15 tests passing ✅**

## Examples

### Before (Incorrect)
```kain
@component
struct HealthComponent:
    current: Float = 0.0

actor Player:
    state health: HealthComponent = HealthComponent()
    
    fn get_health() -> Float:
        return self.health.current
```

Generated (WRONG):
```cpp
return this->health.current;  // Should be -> not .
```

### After (Correct)
Generated (CORRECT):
```cpp
return this->health->current;  // Correctly uses ->
```

### Complex Example
```kain
struct Vec3Wrapper:
    vec: Vec3 = vec3(0.0, 0.0, 0.0)

@component
struct TransformComponent:
    position: Vec3Wrapper = Vec3Wrapper()

actor Player:
    state transform: TransformComponent = TransformComponent()
    
    fn get_x() -> Float:
        return self.transform.position.vec.x
```

Generated (CORRECT):
```cpp
return this->transform->position.vec.X;
// transform is component (pointer) -> position is struct (value) . vec is Vec3 (value) . X
```

## Type Detection Logic

The implementation uses multiple data sources to determine pointer types:

1. **var_types map** - Tracks variable name → type name mappings
2. **type_fields_map** - Maps type names → field lists with types
3. **TypeMapper** - Centralized pointer type detection using EngineKnowledge
4. **Context registries** - Actor, component, subsystem, struct registries

## Integration Points

- Works with existing `type_fields_map` populated during program pre-pass
- Integrates with `TypeMapper.is_pointer_type_by_name()` for centralized type checking
- Uses `EngineKnowledge` metadata for engine type detection
- Maintains backward compatibility with existing codegen

## Files Modified

1. **Kain/crates/ue5/src/codegen_ue5.rs**
   - Enhanced `is_pointer_receiver()` method (lines 825-890)
   - Added `infer_expr_type()` method (lines 892-920)

2. **Kain/crates/ue5/tests/member_access_tests.rs** (NEW)
   - 15 comprehensive tests covering all scenarios
   - 400+ lines of test code

## Benefits

1. **Correctness** - Generates valid C++ with proper pointer/value semantics
2. **Type Safety** - Respects UE5's type system (UObject* vs value types)
3. **Robustness** - Handles complex nested access patterns
4. **Maintainability** - Centralized logic, well-tested
5. **Extensibility** - Easy to add new type detection rules

## Future Enhancements

Potential improvements (not required for current task):
- Track current actor/struct context for better `self` type inference
- Cache inferred types to avoid repeated lookups
- Add diagnostic warnings for ambiguous type resolution
- Support for template/generic types in member access

## Verification

Run tests:
```bash
cd Kain
cargo test --package ue5 --test member_access_tests
```

Expected output:
```
test result: ok. 15 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out
```

## Task Completion

✅ Task 1.5: Fix member access codegen for UObject-derived types
- ✅ Detect UObject-derived types in member access context
- ✅ Use pointer dereferencing (->) for UObject types
- ✅ Use dot notation (.) for value types
- ✅ Add comprehensive unit tests (15 tests)
- ✅ All tests passing

**Status: COMPLETE**
