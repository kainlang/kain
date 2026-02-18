//! POD Mirror Struct Generation
//!
//! When `@component` types are used as shader uniforms they cannot be passed
//! directly to the GPU (they are UObject subclasses with vtables, GC headers,
//! and non-POD fields such as TArray).  This module:
//!
//! 1. Scans a `TypedProgram` for `@component` structs that are referenced by
//!    shader uniforms.
//! 2. Extracts only the POD-compatible fields (primitives, vectors, enums).
//! 3. Generates a `F{ComponentName}Data` mirror struct usable on both C++ and
//!    HLSL sides.
//! 4. Generates C++ population code to copy data from a component pointer into
//!    a zero-initialised POD variable before `AddPass_*` dispatch.
//!
//! Policy (v1)
//! -----------
//! - Non-POD fields (Array, nested components, etc.) are **silently skipped**.
//! - If a shader-used component has **zero** extractable POD fields, a hard
//!   error string is returned via `Err(...)`.
//! - Enums are included: mapped to `int` in HLSL, `E{Name}` in C++.

use kain_core::types::{TypedProgram, TypedItem};
use kain_core::ast::Type;
use std::collections::{HashMap, HashSet};

/// A single POD-compatible field extracted from a `@component` struct.
#[derive(Debug, Clone)]
pub struct PodField {
    /// Original KAIN field name (used verbatim).
    pub name: String,
    /// UE5 C++ type (e.g. `"float"`, `"FVector3f"`, `"EFluidClass"`).
    pub cpp_type: String,
    /// Corresponding HLSL type (e.g. `"float"`, `"float3"`, `"int"`).
    pub hlsl_type: String,
}

/// A generated POD mirror struct for a `@component`.
#[derive(Debug, Clone)]
pub struct PodMirrorStruct {
    /// Original KAIN component name, e.g. `"PhysicalPropertiesComponent"`.
    pub component_name: String,
    /// Generated struct name, e.g. `"FPhysicalPropertiesComponentData"`.
    pub pod_struct_name: String,
    /// Extracted POD-compatible fields.
    pub fields: Vec<PodField>,
}

impl PodMirrorStruct {
    /// C++ struct definition emitted **before** the shader class in `.h` files.
    pub fn generate_cpp_struct(&self) -> String {
        let mut out = String::new();
        out.push_str(&format!(
            "// POD mirror for {} (GPU-compatible)\n",
            self.component_name
        ));
        out.push_str(&format!("struct {} {{\n", self.pod_struct_name));
        for field in &self.fields {
            out.push_str(&format!("    {} {};\n", field.cpp_type, field.name));
        }
        out.push_str("};\n\n");
        out
    }

    /// HLSL struct definition emitted at the top of `.usf` files.
    pub fn generate_hlsl_struct(&self) -> String {
        let mut out = String::new();
        out.push_str(&format!(
            "// POD mirror for {}\n",
            self.component_name
        ));
        out.push_str(&format!("struct {} {{\n", self.pod_struct_name));
        for field in &self.fields {
            out.push_str(&format!("    {} {};\n", field.hlsl_type, field.name));
        }
        out.push_str("};\n\n");
        out
    }

    /// C++ population code that copies data from a component pointer into a
    /// zero-initialised POD variable.  Uses zero-init fallback when the pointer
    /// is `nullptr`.
    ///
    /// # Arguments
    /// * `component_var` – the C++ expression for the component pointer
    ///   (e.g. `"this->physics"`).
    /// * `pod_var`       – name for the local POD variable (e.g. `"physics_pod"`).
    /// * `indent`        – prefix string for each generated line.
    pub fn generate_population_code(
        &self,
        component_var: &str,
        pod_var: &str,
        indent: &str,
    ) -> String {
        let mut out = String::new();
        // Zero-initialise the POD struct.
        out.push_str(&format!(
            "{}{} {} {{}};\n",
            indent, self.pod_struct_name, pod_var
        ));
        // Guard: only copy when the component pointer is valid.
        out.push_str(&format!(
            "{}if ({} != nullptr) {{\n",
            indent, component_var
        ));
        for field in &self.fields {
            out.push_str(&format!(
                "{}    {}.{} = static_cast<{}>({}->{});\n",
                indent, pod_var, field.name, field.cpp_type, component_var, field.name
            ));
        }
        out.push_str(&format!("{}}}\n", indent));
        out
    }
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/// Map a KAIN AST `Type` to `(cpp_type, hlsl_type)` for POD generation.
/// Returns `None` for any non-POD type (Arrays, generic types, nested structs
/// that are not known enums, etc.).
fn map_field_to_pod_types(ty: &Type, enum_names: &HashSet<String>) -> Option<(String, String)> {
    match ty {
        Type::Named { name, generics, .. } if generics.is_empty() => {
            match name.as_str() {
                // Primitive floats
                "Float" | "f32" => Some(("float".into(), "float".into())),
                "Double" | "f64" => Some(("double".into(), "double".into())),
                // Primitive ints
                "Int" | "i32" | "i64" => Some(("int32".into(), "int".into())),
                "UInt" | "u32" | "u64" => Some(("uint32".into(), "uint".into())),
                // Bool
                "Bool" => Some(("bool".into(), "bool".into())),
                // Math vectors (GPU-friendly precision)
                "Vec2" => Some(("FVector2f".into(), "float2".into())),
                "Vec3" => Some(("FVector3f".into(), "float3".into())),
                "Vec4" => Some(("FVector4f".into(), "float4".into())),
                // Enum types — POD compatible (underlying int)
                other => {
                    if enum_names.contains(other) {
                        Some((format!("E{}", other), "int".into()))
                    } else {
                        None // Nested struct / component / unknown — skip
                    }
                }
            }
        }
        // Generics (e.g. `Array<T>`), Refs, Functions, etc. — not POD
        _ => None,
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Collect POD mirror structs for every `@component` type in `program` that is
/// referenced by at least one shader uniform.
///
/// # Errors
/// Returns `Err(message)` if any shader-used component has **zero** extractable
/// POD fields (nothing to generate — hard error so the caller can surface a
/// diagnostic).
pub fn collect_component_mirrors(
    program: &TypedProgram,
) -> Result<HashMap<String, PodMirrorStruct>, String> {
    // Pass 1: collect the names of all user-defined enums.
    let enum_names: HashSet<String> = program
        .items
        .iter()
        .filter_map(|item| {
            if let TypedItem::Enum(en) = item {
                Some(en.ast.name.clone())
            } else {
                None
            }
        })
        .collect();

    // Pass 2: build mirrors for all `@component` structs.
    let mut all_mirrors: HashMap<String, PodMirrorStruct> = HashMap::new();
    for item in &program.items {
        if let TypedItem::Struct(st) = item {
            let is_component = st.ast.attributes.iter().any(|a| a.name == "component");
            if !is_component {
                continue;
            }

            let mut fields = Vec::new();
            for field in &st.ast.fields {
                if let Some((cpp_type, hlsl_type)) =
                    map_field_to_pod_types(&field.ty, &enum_names)
                {
                    fields.push(PodField {
                        name: field.name.clone(),
                        cpp_type,
                        hlsl_type,
                    });
                }
                // Non-POD fields are silently skipped (see module doc).
            }

            all_mirrors.insert(
                st.ast.name.clone(),
                PodMirrorStruct {
                    component_name: st.ast.name.clone(),
                    pod_struct_name: format!("F{}Data", st.ast.name),
                    fields,
                },
            );
        }
    }

    // Pass 3: find which component names are actually used in shader uniforms.
    let mut used: HashSet<String> = HashSet::new();
    for item in &program.items {
        if let TypedItem::Shader(shader) = item {
            for uniform in &shader.ast.uniforms {
                if let Type::Named { name, generics, .. } = &uniform.ty {
                    if generics.is_empty() && all_mirrors.contains_key(name.as_str()) {
                        used.insert(name.clone());
                    }
                }
            }
        }
    }

    // Validate: hard error if a used component has no extractable POD fields.
    for comp_name in &used {
        if let Some(mirror) = all_mirrors.get(comp_name) {
            if mirror.fields.is_empty() {
                return Err(format!(
                    "Component '{}' is used as a shader uniform but has no \
                     GPU-compatible (POD) fields. Only primitive types, vectors, \
                     and enums can cross the CPU/GPU boundary.",
                    comp_name
                ));
            }
        }
    }

    // Return only mirrors that are actually needed by shaders.
    Ok(all_mirrors
        .into_iter()
        .filter(|(name, _)| used.contains(name))
        .collect())
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{
        Attribute, Enum, Field, Shader, ShaderStage, Struct, Uniform, Variant, VariantFields,
        Block, Param, Visibility,
    };
    use kain_core::span::Span;
    use kain_core::types::{TypedEnum, TypedItem, TypedProgram, TypedShader, TypedStruct, ResolvedType};
    use std::collections::HashMap as HM;

    fn span() -> Span {
        Span::default()
    }

    fn named(name: &str) -> Type {
        Type::Named { name: name.to_string(), generics: vec![], span: span() }
    }

    fn array_of(inner: &str) -> Type {
        Type::Named {
            name: "Array".to_string(),
            generics: vec![named(inner)],
            span: span(),
        }
    }

    fn make_component_struct(name: &str, fields: Vec<(&str, Type)>) -> TypedStruct {
        TypedStruct {
            ast: Struct {
                name: name.to_string(),
                generics: vec![],
                fields: fields
                    .into_iter()
                    .map(|(n, ty)| Field {
                        name: n.to_string(),
                        ty,
                        attributes: vec![],
                        visibility: Visibility::Public,
                        default: None,
                        weak: false,
                        span: span(),
                    })
                    .collect(),
                methods: vec![],
                attributes: vec![Attribute {
                    name: "component".to_string(),
                    args: vec![],
                    span: span(),
                }],
                visibility: Visibility::Public,
                span: span(),
            },
            field_types: HM::new(),
        }
    }

    fn make_enum(name: &str) -> TypedEnum {
        TypedEnum {
            ast: Enum {
                name: name.to_string(),
                generics: vec![],
                variants: vec![Variant {
                    name: "Variant1".to_string(),
                    fields: VariantFields::Unit,
                    span: span(),
                }],
                visibility: Visibility::Public,
                span: span(),
            },
            variant_payload_types: HM::new(),
        }
    }

    fn make_compute_shader(name: &str, uniforms: Vec<(&str, Type)>) -> TypedShader {
        TypedShader {
            ast: Shader {
                name: name.to_string(),
                stage: ShaderStage::Compute,
                inputs: vec![Param {
                    name: "id".to_string(),
                    ty: named("Vec3"),
                    mutable: false,
                    default: None,
                    span: span(),
                }],
                outputs: named("Vec4"),
                uniforms: uniforms
                    .into_iter()
                    .enumerate()
                    .map(|(i, (n, ty))| Uniform {
                        name: n.to_string(),
                        ty,
                        binding: i as u32,
                        span: span(),
                    })
                    .collect(),
                body: Block { stmts: vec![], span: span() },
                span: span(),
            },
            input_types: vec![],
            output_type: ResolvedType::Unknown,
        }
    }

    fn program(items: Vec<TypedItem>) -> TypedProgram {
        TypedProgram { items }
    }

    // -----------------------------------------------------------------------

    #[test]
    fn test_simple_pod_extraction() {
        let prog = program(vec![
            TypedItem::Struct(make_component_struct(
                "PhysicsComp",
                vec![("viscosity", named("Float")), ("density", named("Float"))],
            )),
            TypedItem::Shader(make_compute_shader(
                "TestShader",
                vec![("physics", named("PhysicsComp"))],
            )),
        ]);

        let mirrors = collect_component_mirrors(&prog).unwrap();
        assert!(mirrors.contains_key("PhysicsComp"));
        let m = &mirrors["PhysicsComp"];
        assert_eq!(m.pod_struct_name, "FPhysicsCompData");
        assert_eq!(m.fields.len(), 2);
        assert_eq!(m.fields[0].name, "viscosity");
        assert_eq!(m.fields[0].cpp_type, "float");
        assert_eq!(m.fields[0].hlsl_type, "float");
    }

    #[test]
    fn test_non_pod_fields_skipped() {
        let prog = program(vec![
            TypedItem::Struct(make_component_struct(
                "MixedComp",
                vec![
                    ("viscosity", named("Float")),
                    ("particles", array_of("Int")), // non-POD
                    ("density", named("Float")),
                ],
            )),
            TypedItem::Shader(make_compute_shader(
                "TestShader",
                vec![("comp", named("MixedComp"))],
            )),
        ]);

        let mirrors = collect_component_mirrors(&prog).unwrap();
        let m = &mirrors["MixedComp"];
        // Only viscosity and density; particles (Array) is skipped
        assert_eq!(m.fields.len(), 2);
        assert_eq!(m.fields[0].name, "viscosity");
        assert_eq!(m.fields[1].name, "density");
    }

    #[test]
    fn test_enum_fields_included() {
        let prog = program(vec![
            TypedItem::Enum(make_enum("FluidClass")),
            TypedItem::Struct(make_component_struct(
                "PhysComp",
                vec![
                    ("fluid_class", named("FluidClass")),
                    ("viscosity", named("Float")),
                ],
            )),
            TypedItem::Shader(make_compute_shader(
                "TestShader",
                vec![("phys", named("PhysComp"))],
            )),
        ]);

        let mirrors = collect_component_mirrors(&prog).unwrap();
        let m = &mirrors["PhysComp"];
        assert_eq!(m.fields.len(), 2);
        assert_eq!(m.fields[0].cpp_type, "EFluidClass");
        assert_eq!(m.fields[0].hlsl_type, "int");
        assert_eq!(m.fields[1].cpp_type, "float");
    }

    #[test]
    fn test_unused_component_not_returned() {
        // PhysicsComp is defined but not referenced by any shader uniform
        let prog = program(vec![
            TypedItem::Struct(make_component_struct(
                "PhysicsComp",
                vec![("viscosity", named("Float"))],
            )),
            TypedItem::Shader(make_compute_shader(
                "TestShader",
                vec![("time", named("Float"))], // scalar, not a component
            )),
        ]);

        let mirrors = collect_component_mirrors(&prog).unwrap();
        assert!(mirrors.is_empty(), "Unused component should not appear in mirrors");
    }

    #[test]
    fn test_hard_error_on_all_non_pod_fields() {
        let prog = program(vec![
            TypedItem::Struct(make_component_struct(
                "BadComp",
                vec![
                    ("items", array_of("Int")),   // non-POD
                    ("more", array_of("Float")),  // non-POD
                ],
            )),
            TypedItem::Shader(make_compute_shader(
                "TestShader",
                vec![("bad", named("BadComp"))],
            )),
        ]);

        let result = collect_component_mirrors(&prog);
        assert!(result.is_err(), "All-non-POD component used in shader must be a hard error");
        assert!(result.unwrap_err().contains("BadComp"));
    }

    #[test]
    fn test_generate_cpp_struct() {
        let mirror = PodMirrorStruct {
            component_name: "PhysicsComp".into(),
            pod_struct_name: "FPhysicsCompData".into(),
            fields: vec![
                PodField { name: "viscosity".into(), cpp_type: "float".into(), hlsl_type: "float".into() },
                PodField { name: "density".into(),   cpp_type: "float".into(), hlsl_type: "float".into() },
            ],
        };
        let cpp = mirror.generate_cpp_struct();
        assert!(cpp.contains("struct FPhysicsCompData"));
        assert!(cpp.contains("float viscosity;"));
        assert!(cpp.contains("float density;"));
    }

    #[test]
    fn test_generate_hlsl_struct() {
        let mirror = PodMirrorStruct {
            component_name: "PhysicsComp".into(),
            pod_struct_name: "FPhysicsCompData".into(),
            fields: vec![
                PodField { name: "viscosity".into(), cpp_type: "float".into(), hlsl_type: "float".into() },
                PodField { name: "fluid_class".into(), cpp_type: "EFluidClass".into(), hlsl_type: "int".into() },
            ],
        };
        let hlsl = mirror.generate_hlsl_struct();
        assert!(hlsl.contains("struct FPhysicsCompData"));
        assert!(hlsl.contains("float viscosity;"));
        assert!(hlsl.contains("int fluid_class;"));
    }

    #[test]
    fn test_generate_population_code() {
        let mirror = PodMirrorStruct {
            component_name: "PhysicsComp".into(),
            pod_struct_name: "FPhysicsCompData".into(),
            fields: vec![
                PodField { name: "viscosity".into(), cpp_type: "float".into(), hlsl_type: "float".into() },
            ],
        };
        let code = mirror.generate_population_code("this->physics", "physics_pod", "\t\t\t");
        assert!(code.contains("FPhysicsCompData physics_pod {}"));
        assert!(code.contains("if (this->physics != nullptr)"));
        assert!(code.contains("physics_pod.viscosity = static_cast<float>(this->physics->viscosity)"));
    }
}
