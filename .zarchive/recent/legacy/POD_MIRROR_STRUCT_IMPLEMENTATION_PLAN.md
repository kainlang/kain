# POD Mirror Struct Auto-Generation - Implementation Plan
> **Feature:** Auto-generate GPU-compatible POD structs from `@component` definitions when used in shaders
> **Priority:** CRITICAL - Blocks CFD lab generation
> **Complexity:** Medium (3-5 hours)
> **Impact:** MASSIVE - Unlocks entire class of physics simulations

---

## PROBLEM STATEMENT

**Current Behavior:**
```kain
@component
struct PhysicalPropertiesComponent:
    viscosity: Float
    density: Float
    surface_tension: Float

shader compute LatticeBoltzmannCollision(id: Vec3) -> Vec4:
    uniform physics: PhysicalPropertiesComponent @0  // ❌ FAILS
    return vec4(physics.viscosity, 0, 0, 1)
```

**Generated C++ (BROKEN):**
```cpp
SHADER_PARAMETER(PhysicalPropertiesComponent, physics)  // ❌ UActorComponent* can't go to GPU
```

**Desired Behavior:**
```cpp
// Auto-generated POD mirror struct
struct FPhysicalPropertiesData {
    float viscosity;
    float density;
    float surface_tension;
};

SHADER_PARAMETER(FPhysicalPropertiesData, physics)  // ✅ POD struct works
```

---

## ARCHITECTURE OVERVIEW

### Phase 1: Detection (Shader Codegen)
**File:** `crates/ue5-shaders/src/codegen_usf.rs`
- Scan shader uniforms for component types
- Build list of required POD mirror structs
- Track which components are used in which shaders

### Phase 2: POD Struct Generation (New Module)
**File:** `crates/ue5-shaders/src/pod_mirror.rs` (NEW)
- Extract POD-compatible fields from component definitions
- Generate `F{ComponentName}Data` structs
- Handle nested components (flatten or reference)

### Phase 3: Integration (Shader Header Generation)
**File:** `crates/ue5-shaders/src/codegen_usf.rs` (MODIFY)
- Emit POD struct definitions before shader class
- Replace component types with POD types in SHADER_PARAMETER
- Update helper function signatures

### Phase 4: Dispatch Code Generation (Actor Codegen)
**File:** `crates/ue5/src/codegen_ue5.rs` (MODIFY)
- Detect shader calls with component parameters
- Generate POD struct population code
- Pass POD structs to shader dispatch

---

## DETAILED IMPLEMENTATION

### Step 1: Create POD Mirror Module

**File:** `crates/ue5-shaders/src/pod_mirror.rs`

```rust
//! POD Mirror Struct Generation
//! 
//! Generates GPU-compatible Plain Old Data (POD) structs from KAIN component definitions.
//! Components are UActorComponent subclasses with vtables and GC data - they can't be passed
//! to GPU shaders. This module extracts their POD-compatible fields and generates mirror structs.

use kain_core::ast::{Struct, Field, Type};
use std::collections::HashMap;

/// A POD-compatible field extracted from a component
#[derive(Debug, Clone)]
pub struct PodField {
    pub name: String,
    pub cpp_type: String,  // e.g., "float", "FVector3f", "int32"
    pub original_type: Type,
}

/// A POD mirror struct definition
#[derive(Debug, Clone)]
pub struct PodMirrorStruct {
    pub component_name: String,      // e.g., "PhysicalPropertiesComponent"
    pub pod_struct_name: String,     // e.g., "FPhysicalPropertiesData"
    pub fields: Vec<PodField>,
}

impl PodMirrorStruct {
    /// Create a POD mirror from a component struct definition
    pub fn from_component(component: &Struct) -> Option<Self> {
        let mut pod_fields = Vec::new();
        
        for field in &component.fields {
            // Only extract POD-compatible types
            if let Some(cpp_type) = map_type_to_pod_cpp(&field.ty) {
                pod_fields.push(PodField {
                    name: field.name.clone(),
                    cpp_type,
                    original_type: field.ty.clone(),
                });
            }
            // Skip non-POD types (arrays, pointers, nested components)
        }
        
        if pod_fields.is_empty() {
            return None;
        }
        
        Some(PodMirrorStruct {
            component_name: component.name.clone(),
            pod_struct_name: format!("F{}Data", component.name),
            fields: pod_fields,
        })
    }
    
    /// Generate C++ struct definition
    pub fn generate_cpp(&self) -> String {
        let mut output = String::new();
        
        output.push_str(&format!("// POD mirror struct for {}\n", self.component_name));
        output.push_str(&format!("struct {}\n{{\n", self.pod_struct_name));
        
        for field in &self.fields {
            output.push_str(&format!("    {} {};\n", field.cpp_type, field.name));
        }
        
        output.push_str("};\n\n");
        output
    }
    
    /// Generate code to populate POD struct from component instance
    pub fn generate_population_code(&self, component_var: &str, pod_var: &str) -> String {
        let mut output = String::new();
        
        output.push_str(&format!("    {} {};\n", self.pod_struct_name, pod_var));
        
        for field in &self.fields {
            output.push_str(&format!(
                "    {}.{} = {}->{};\n",
                pod_var, field.name, component_var, field.name
            ));
        }
        
        output
    }
}

/// Map KAIN type to POD-compatible C++ type
/// Returns None if type is not POD-compatible (arrays, pointers, nested components)
fn map_type_to_pod_cpp(ty: &Type) -> Option<String> {
    match ty {
        Type::Named(name) => match name.as_str() {
            // Primitives
            "Bool" => Some("bool".to_string()),
            "Int" => Some("int32".to_string()),
            "Int64" => Some("int64".to_string()),
            "Float" => Some("float".to_string()),
            "Double" => Some("double".to_string()),
            "Byte" => Some("uint8".to_string()),
            
            // Math types (POD-compatible)
            "Vec2" => Some("FVector2f".to_string()),
            "Vec3" => Some("FVector3f".to_string()),
            "Vec4" => Some("FVector4f".to_string()),
            "Vector" => Some("FVector3f".to_string()),
            "Vector2D" => Some("FVector2f".to_string()),
            "Rot" | "Rotator" => Some("FRotator3f".to_string()),
            "Quat" => Some("FQuat4f".to_string()),
            "Color" | "LinearColor" => Some("FLinearColor".to_string()),
            
            // Enums are POD-compatible (underlying int)
            _ if name.chars().next().map_or(false, |c| c.is_uppercase()) => {
                Some(format!("E{}", name))
            }
            
            _ => None,  // Unknown type - not POD
        },
        
        // Arrays, pointers, generics are NOT POD-compatible
        Type::Array(_) => None,
        Type::Pointer(_) => None,
        Type::Generic(_, _) => None,
        Type::Function(_, _) => None,
        Type::Tuple(_) => None,
        
        _ => None,
    }
}

/// Collect all component types used in shader uniforms
pub fn collect_shader_component_types(program: &kain_core::types::TypedProgram) -> HashMap<String, Struct> {
    use kain_core::types::TypedItem;
    
    let mut component_map = HashMap::new();
    let mut used_components = std::collections::HashSet::new();
    
    // First pass: Build map of all components
    for item in &program.items {
        if let TypedItem::Struct(st) = item {
            if st.ast.attributes.iter().any(|a| a.name == "component") {
                component_map.insert(st.ast.name.clone(), st.ast.clone());
            }
        }
    }
    
    // Second pass: Find which components are used in shaders
    for item in &program.items {
        if let TypedItem::Shader(shader) = item {
            for uniform in &shader.ast.uniforms {
                if let Type::Named(type_name) = &uniform.ty {
                    if type_name.ends_with("Component") && component_map.contains_key(type_name) {
                        used_components.insert(type_name.clone());
                    }
                }
            }
        }
    }
    
    // Return only components that are actually used in shaders
    component_map.into_iter()
        .filter(|(name, _)| used_components.contains(name))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Struct, Field, Type, Attribute};
    use kain_core::span::Span;
    
    #[test]
    fn test_pod_mirror_generation() {
        let component = Struct {
            name: "PhysicsComponent".to_string(),
            fields: vec![
                Field {
                    name: "viscosity".to_string(),
                    ty: Type::Named("Float".to_string()),
                    mutable: false,
                    default: None,
                    attributes: vec![],
                    span: Span::default(),
                },
                Field {
                    name: "density".to_string(),
                    ty: Type::Named("Float".to_string()),
                    mutable: false,
                    default: None,
                    attributes: vec![],
                    span: Span::default(),
                },
            ],
            generics: vec![],
            visibility: kain_core::ast::Visibility::Public,
            attributes: vec![
                Attribute {
                    name: "component".to_string(),
                    args: vec![],
                    span: Span::default(),
                }
            ],
            span: Span::default(),
        };
        
        let pod = PodMirrorStruct::from_component(&component).unwrap();
        
        assert_eq!(pod.component_name, "PhysicsComponent");
        assert_eq!(pod.pod_struct_name, "FPhysicsComponentData");
        assert_eq!(pod.fields.len(), 2);
        assert_eq!(pod.fields[0].cpp_type, "float");
        
        let cpp = pod.generate_cpp();
        assert!(cpp.contains("struct FPhysicsComponentData"));
        assert!(cpp.contains("float viscosity;"));
        assert!(cpp.contains("float density;"));
    }
}
```

---

### Step 2: Integrate into Shader Codegen

**File:** `crates/ue5-shaders/src/codegen_usf.rs`

**Modify `generate_cpp_header()` function:**

```rust
pub fn generate_cpp_header(program: &TypedProgram, shader_name: &str) -> String {
    let mut output = String::new();
    
    // ... existing preamble code ...
    
    // NEW: Collect and generate POD mirror structs
    let component_types = pod_mirror::collect_shader_component_types(program);
    let mut pod_mirrors = Vec::new();
    
    for (comp_name, comp_struct) in &component_types {
        if let Some(pod) = pod_mirror::PodMirrorStruct::from_component(comp_struct) {
            output.push_str(&pod.generate_cpp());
            pod_mirrors.push((comp_name.clone(), pod));
        }
    }
    
    // ... rest of existing code ...
    
    // MODIFY: When collecting uniforms, replace component types with POD types
    for uniform in &shader.ast.uniforms {
        if is_permutation_param(&uniform.name) {
            // ... existing permutation handling ...
            continue;
        }
        
        let usf_type = if let Type::Named(type_name) = &uniform.ty {
            if type_name.ends_with("Component") {
                // Replace with POD mirror type
                if let Some((_, pod)) = pod_mirrors.iter().find(|(name, _)| name == type_name) {
                    pod.pod_struct_name.clone()
                } else {
                    map_type_to_usf(&uniform.ty)
                }
            } else {
                map_type_to_usf(&uniform.ty)
            }
        } else {
            map_type_to_usf(&uniform.ty)
        };
        
        // ... rest of existing uniform handling ...
    }
    
    // ... rest of function ...
}
```

---

### Step 3: Update Actor Dispatch Code

**File:** `crates/ue5/src/codegen_ue5.rs`

**Add method to detect shader calls with component parameters:**

```rust
impl Ue5Gen {
    /// Generate code to populate POD struct from component and dispatch shader
    fn gen_shader_dispatch_with_components(&mut self, shader_name: &str, args: &[Expr]) {
        // Detect which arguments are components
        for (i, arg) in args.iter().enumerate() {
            if let Expr::Field { object, field, .. } = arg {
                // Check if this is a component field access
                if self.is_component_type(field) {
                    // Generate POD population code
                    let pod_var = format!("{}Data", field);
                    let pod_type = format!("F{}Data", field);
                    
                    self.source.line(&format!("    {} {};", pod_type, pod_var));
                    
                    // Populate POD fields from component
                    // This requires knowing the component's fields - we need to look them up
                    if let Some(component_def) = self.context.get_component_definition(field) {
                        for comp_field in &component_def.fields {
                            if is_pod_compatible(&comp_field.ty) {
                                self.source.line(&format!(
                                    "    {}.{} = {}->{}->{}",
                                    pod_var, comp_field.name, 
                                    self.gen_expr(object), field, comp_field.name
                                ));
                            }
                        }
                    }
                }
            }
        }
        
        // Generate shader dispatch call with POD structs
        // ... existing dispatch code ...
    }
}
```

---

### Step 4: Update Lib.rs Exports

**File:** `crates/ue5-shaders/src/lib.rs`

```rust
pub mod codegen_usf;
pub mod shader_knowledge;
pub mod pod_mirror;  // NEW

pub use codegen_usf::generate as generate_usf;
pub use codegen_usf::generate_single_usf_from_program;
pub use codegen_usf::generate_cpp_header;
pub use codegen_usf::generate_cpp_implementation;
pub use shader_knowledge::ShaderKnowledge;
pub use pod_mirror::{PodMirrorStruct, collect_shader_component_types};  // NEW
```

---

## TESTING STRATEGY

### Test 1: Simple Component
```kain
@component
struct PhysicsData:
    viscosity: Float
    density: Float

shader compute Test(id: Vec3) -> Vec4:
    uniform physics: PhysicsData @0
    return vec4(physics.viscosity, physics.density, 0, 1)
```

**Expected Output:**
```cpp
struct FPhysicsDataData {
    float viscosity;
    float density;
};

SHADER_PARAMETER(FPhysicsDataData, physics)
```

### Test 2: Multiple Components
```kain
@component
struct Physics:
    viscosity: Float

@component
struct Thermal:
    temperature: Float

shader compute Test(id: Vec3) -> Vec4:
    uniform physics: Physics @0
    uniform thermal: Thermal @1
    return vec4(physics.viscosity, thermal.temperature, 0, 1)
```

### Test 3: Mixed POD and Non-POD Fields
```kain
@component
struct Complex:
    viscosity: Float           // ✅ POD
    particles: Array<Int>      // ❌ Skip
    velocity: Vec3             // ✅ POD
    actor_ref: Actor           // ❌ Skip
```

**Expected:** Only `viscosity` and `velocity` in POD struct

### Test 4: FluidFlow Integration
Run the full 73-shader FluidFlow plugin and verify:
- All component types generate POD mirrors
- All shaders compile
- Dispatch code populates POD structs correctly

---

## EDGE CASES TO HANDLE

### 1. Nested Components
```kain
@component
struct Inner:
    value: Float

@component
struct Outer:
    inner: Inner  // ❌ Not POD - skip or flatten?
```

**Solution:** Skip nested components in Phase 1, add flattening in Phase 2

### 2. Enum Fields
```kain
enum FluidType: Water, Air

@component
struct Physics:
    fluid_type: FluidType  // ✅ POD (underlying int)
```

**Solution:** Include enums - they're POD-compatible

### 3. Component Not Found
```kain
shader compute Test(id: Vec3) -> Vec4:
    uniform unknown: UnknownComponent @0  // Component doesn't exist
```

**Solution:** Emit compiler error with helpful message

### 4. Empty POD Struct
```kain
@component
struct AllPointers:
    actor: Actor
    array: Array<Int>
```

**Solution:** Emit warning, skip POD generation, error if used in shader

---

## IMPLEMENTATION CHECKLIST

### Phase 1: Core Infrastructure (1-2 hours)
- [ ] Create `pod_mirror.rs` module
- [ ] Implement `PodMirrorStruct::from_component()`
- [ ] Implement `map_type_to_pod_cpp()`
- [ ] Implement `collect_shader_component_types()`
- [ ] Add unit tests for POD extraction
- [ ] Export from `lib.rs`

### Phase 2: Shader Codegen Integration (1-2 hours)
- [ ] Modify `generate_cpp_header()` to detect component uniforms
- [ ] Generate POD struct definitions before shader class
- [ ] Replace component types with POD types in SHADER_PARAMETER
- [ ] Update helper function signatures
- [ ] Test with simple shader

### Phase 3: Actor Dispatch Integration (1 hour)
- [ ] Add component definition lookup to Ue5Context
- [ ] Implement `gen_shader_dispatch_with_components()`
- [ ] Generate POD population code in actor methods
- [ ] Test with actor calling shader

### Phase 4: Testing & Validation (30 min)
- [ ] Test with FluidFlow plugin (73 shaders)
- [ ] Verify all POD structs generate correctly
- [ ] Verify dispatch code compiles
- [ ] Run in UE5 and verify shaders execute

### Phase 5: Documentation (30 min)
- [ ] Update AI_PLUGIN_CREATION_GUIDE.md
- [ ] Add examples to docs
- [ ] Update ISSUE_VERIFICATION_STATUS.md

---

## SUCCESS CRITERIA

✅ **Compiler generates POD mirror structs automatically**
✅ **FluidFlow plugin (73 shaders) compiles without errors**
✅ **Generated C++ compiles in UE5**
✅ **Shaders execute correctly with component data**
✅ **No manual workarounds required**

---

## ESTIMATED TIMELINE

- **Core Implementation:** 3-4 hours
- **Testing & Debugging:** 1-2 hours
- **Documentation:** 30 minutes
- **Total:** 4.5-6.5 hours

---

## ALTERNATIVE APPROACHES (NOT RECOMMENDED)

### Option A: Manual POD Structs
**Pros:** Simple, no compiler changes
**Cons:** Verbose, error-prone, defeats purpose of KAIN

### Option B: Flatten All Components
**Pros:** No POD structs needed
**Cons:** Breaks abstraction, 100+ shader parameters

### Option C: CPU-Side Shader Execution
**Pros:** No GPU constraints
**Cons:** 1000x slower, defeats purpose of GPU shaders

---

## POST-IMPLEMENTATION ENHANCEMENTS

### Phase 2 Features (Future)
1. **Nested Component Flattening**
   - Flatten nested components into single POD struct
   - `outer.inner.value` → `outer_inner_value`

2. **Array Support**
   - Fixed-size arrays as POD fields
   - `values: Array<Float, 16>` → `float values[16];`

3. **Automatic Packing**
   - Optimize POD struct layout for GPU alignment
   - Pack fields to minimize padding

4. **Smart Caching**
   - Cache POD structs across frames
   - Only update changed fields

---

## CONCLUSION

This feature is **CRITICAL** and **HIGH-IMPACT**. It's the final piece needed to unlock:

- ✅ CFD lab generation (FluidFlow plugin)
- ✅ Physics simulation plugins
- ✅ Particle system plugins
- ✅ Any plugin with GPU compute + component data

**Estimated ROI:** 4-6 hours of work → Unlocks $100K+ worth of marketplace plugins

**Recommendation:** IMPLEMENT IMMEDIATELY
