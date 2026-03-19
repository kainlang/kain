//! UE5 Trait to UInterface Code Generation
//!
//! This module implements KAIN trait → UE5 UInterface generation.
//! UE5 uses a dual-class interface system:
//! - UInterfaceName (UINTERFACE) - UObject-based interface class
//! - IInterfaceName (native interface) - Pure virtual C++ interface

use kain_core::ast::{Function, Impl, Trait};
use std::collections::HashMap;

/// Generate UE5 dual-class interface header from a KAIN trait
pub fn generate_trait_header(trait_def: &Trait, module_api: &str) -> String {
    let trait_name = &trait_def.name;
    let u_class_name = format!("U{}", trait_name);
    let i_interface_name = format!("I{}", trait_name);

    let mut output = String::new();

    // Generate UINTERFACE class
    output.push_str(&format!("// UInterface class for {}\n", trait_name));
    output.push_str("UINTERFACE(MinimalAPI, Blueprintable)\n");
    output.push_str(&format!("class {} : public UInterface {{\n", u_class_name));
    output.push_str("    GENERATED_BODY()\n");
    output.push_str("};\n\n");

    // Generate native interface class
    output.push_str(&format!("// Native interface for {}\n", trait_name));
    output.push_str(&format!("class {} {} {{\n", module_api, i_interface_name));
    output.push_str("    GENERATED_BODY()\n\n");
    output.push_str("public:\n");

    // Generate method declarations
    for method in &trait_def.methods {
        let method_sig = generate_trait_method_signature(method, trait_name);
        output.push_str(&format!("    {}\n", method_sig));

        // Generate default implementation stub
        let default_impl = generate_trait_method_default_impl(method);
        output.push_str(&format!("    {}\n\n", default_impl));
    }

    output.push_str("};\n");

    output
}

fn generate_trait_method_signature(
    method: &kain_core::ast::TraitMethod,
    trait_name: &str,
) -> String {
    let method_name = to_pascal_case(&method.name);
    let return_type = method
        .return_type
        .as_ref()
        .map(|t| map_type_to_cpp(t))
        .unwrap_or_else(|| "void".to_string());

    let params = method
        .params
        .iter()
        .filter(|p| p.name != "self")
        .map(|p| format!("{} {}", map_type_to_cpp(&p.ty), to_pascal_case(&p.name)))
        .collect::<Vec<_>>()
        .join(", ");

    format!(
        "UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category=\"{}\")\n    virtual {} {}({});",
        trait_name,
        return_type,
        method_name,
        params
    )
}

fn generate_trait_method_default_impl(method: &kain_core::ast::TraitMethod) -> String {
    let method_name = to_pascal_case(&method.name);
    let return_type = method
        .return_type
        .as_ref()
        .map(|t| map_type_to_cpp(t))
        .unwrap_or_else(|| "void".to_string());

    let params = method
        .params
        .iter()
        .filter(|p| p.name != "self")
        .map(|p| format!("{} {}", map_type_to_cpp(&p.ty), to_pascal_case(&p.name)))
        .collect::<Vec<_>>()
        .join(", ");

    let default_return = if return_type == "void" {
        String::new()
    } else if return_type == "bool" {
        " return false;".to_string()
    } else if return_type.starts_with("int") || return_type.starts_with("float") {
        " return 0;".to_string()
    } else {
        format!(" return {}();", return_type)
    };

    format!(
        "virtual {} {}_Implementation({}) {{{} }}",
        return_type, method_name, params, default_return
    )
}

pub fn generate_trait_impl_methods(
    impl_block: &Impl,
    trait_def: &Trait,
    class_name: &str,
) -> String {
    let mut output = String::new();

    for method in &impl_block.methods {
        let trait_method = trait_def.methods.iter().find(|tm| tm.name == method.name);

        if let Some(_tm) = trait_method {
            let method_impl = generate_method_impl(method, class_name);
            output.push_str(&method_impl);
            output.push_str("\n\n");
        }
    }

    output
}

fn generate_method_impl(method: &Function, class_name: &str) -> String {
    let method_name = to_pascal_case(&method.name);
    let return_type = method
        .return_type
        .as_ref()
        .map(|t| map_type_to_cpp(t))
        .unwrap_or_else(|| "void".to_string());

    let params = method
        .params
        .iter()
        .filter(|p| p.name != "self")
        .map(|p| format!("{} {}", map_type_to_cpp(&p.ty), to_pascal_case(&p.name)))
        .collect::<Vec<_>>()
        .join(", ");

    format!(
        "{} {}::{}_Implementation({}) {{\n    // TODO: Implement\n    return {{}};\n}}",
        return_type, class_name, method_name, params
    )
}

pub fn generate_class_interface_list(
    impls: &[&Impl],
    _all_traits: &HashMap<String, &Trait>,
) -> String {
    let interfaces: Vec<String> = impls
        .iter()
        .filter_map(|impl_block| {
            impl_block
                .trait_name
                .as_ref()
                .map(|trait_name| format!(", public I{}", trait_name))
        })
        .collect();

    interfaces.join("")
}

fn to_pascal_case(s: &str) -> String {
    s.split('_')
        .map(|word| {
            let mut chars = word.chars();
            match chars.next() {
                None => String::new(),
                Some(first) => first.to_uppercase().collect::<String>() + chars.as_str(),
            }
        })
        .collect()
}

fn map_type_to_cpp(ty: &kain_core::ast::Type) -> String {
    use kain_core::ast::Type;

    match ty {
        Type::Named { name, .. } => match name.as_str() {
            "Bool" => "bool".to_string(),
            "Int" => "int32".to_string(),
            "Float" => "float".to_string(),
            "String" => "FString".to_string(),
            "Vec2" => "FVector2D".to_string(),
            "Vec3" => "FVector".to_string(),
            "Vec4" => "FVector4".to_string(),
            other => format!("F{}", other),
        },
        Type::Unit(_) => "void".to_string(),
        _ => "void".to_string(),
    }
}
