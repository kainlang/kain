//! Blueprint Accessor Code Generation
//!
//! Generates UFUNCTION(BlueprintCallable) static getter and setter methods
//! for config fields marked with @setting(blueprint: true).
//!
//! # Generated Code Pattern
//!
//! For a field `chunk_size: Float` with `@setting(blueprint: true)`:
//!
//! ```cpp
//! UFUNCTION(BlueprintCallable, Category="Voxel Settings")
//! static float GetChunkSize()
//! {
//!     return Get()->ChunkSize;
//! }
//! ```
//!
//! If `@setting(writable: true)` is also set:
//!
//! ```cpp
//! UFUNCTION(BlueprintCallable, Category="Voxel Settings")
//! static void SetChunkSize(float NewValue)
//! {
//!     UVoxelSettings* Settings = GetMutableDefault<UVoxelSettings>();
//!     Settings->ChunkSize = NewValue;
//!     Settings->SaveConfig();
//! }
//! ```

use crate::config_ir::{ConfigField, ConfigStruct};
use kain_core::ast::Type;

/// Generate Blueprint getter function declaration for header
pub fn generate_blueprint_getter_declaration(
    config: &ConfigStruct,
    field: &ConfigField,
) -> String {
    let category = format!("{} Settings", config.name);
    let getter_name = format!("Get{}", field.ue5_property_name());
    let return_type = map_type_to_cpp(&field.ty);
    let property_name = field.ue5_property_name();

    format!(
        "\t/** Get {} */\n\tUFUNCTION(BlueprintCallable, Category=\"{}\")\n\tstatic {} {}();",
        property_name, category, return_type, getter_name
    )
}

/// Generate Blueprint setter function declaration for header (if writable)
pub fn generate_blueprint_setter_declaration(
    config: &ConfigStruct,
    field: &ConfigField,
) -> Option<String> {
    if !field.writable {
        return None;
    }

    let category = format!("{} Settings", config.name);
    let setter_name = format!("Set{}", field.ue5_property_name());
    let param_type = map_type_to_cpp(&field.ty);
    let property_name = field.ue5_property_name();

    // For FString, use const FString& for parameter
    let param_decl = if is_string_type(&field.ty) {
        format!("const {}& NewValue", param_type)
    } else {
        format!("{} NewValue", param_type)
    };

    Some(format!(
        "\t/** Set {} */\n\tUFUNCTION(BlueprintCallable, Category=\"{}\")\n\tstatic void {}({});",
        property_name, category, setter_name, param_decl
    ))
}

/// Generate Blueprint getter function implementation for cpp
pub fn generate_blueprint_getter_implementation(
    config: &ConfigStruct,
    field: &ConfigField,
) -> String {
    let class_name = config.ue5_class_name();
    let getter_name = format!("Get{}", field.ue5_property_name());
    let return_type = map_type_to_cpp(&field.ty);
    let property_name = field.ue5_property_name();

    format!(
        "{} {}::{}()\n{{\n\treturn Get()->{};\n}}",
        return_type, class_name, getter_name, property_name
    )
}

/// Generate Blueprint setter function implementation for cpp (if writable)
pub fn generate_blueprint_setter_implementation(
    config: &ConfigStruct,
    field: &ConfigField,
) -> Option<String> {
    if !field.writable {
        return None;
    }

    let class_name = config.ue5_class_name();
    let setter_name = format!("Set{}", field.ue5_property_name());
    let param_type = map_type_to_cpp(&field.ty);
    let property_name = field.ue5_property_name();

    // For FString, use const FString& for parameter
    let param_decl = if is_string_type(&field.ty) {
        format!("const {}& NewValue", param_type)
    } else {
        format!("{} NewValue", param_type)
    };

    Some(format!(
        "void {}::{}({})\n{{\n\t{}* Settings = GetMutableDefault<{}>();\n\tSettings->{} = NewValue;\n\tSettings->SaveConfig();\n}}",
        class_name, setter_name, param_decl, class_name, class_name, property_name
    ))
}

/// Generate all Blueprint accessor declarations for header
pub fn generate_blueprint_accessors_header(config: &ConfigStruct) -> Vec<String> {
    let mut declarations = Vec::new();

    for field in &config.fields {
        if !field.blueprint {
            continue;
        }

        // Add getter
        declarations.push(generate_blueprint_getter_declaration(config, field));

        // Add setter if writable
        if let Some(setter) = generate_blueprint_setter_declaration(config, field) {
            declarations.push(setter);
        }
    }

    declarations
}

/// Generate all Blueprint accessor implementations for cpp
pub fn generate_blueprint_accessors_cpp(config: &ConfigStruct) -> Vec<String> {
    let mut implementations = Vec::new();

    for field in &config.fields {
        if !field.blueprint {
            continue;
        }

        // Add getter
        implementations.push(generate_blueprint_getter_implementation(config, field));

        // Add setter if writable
        if let Some(setter) = generate_blueprint_setter_implementation(config, field) {
            implementations.push(setter);
        }
    }

    implementations
}

/// Map KAIN type to UE5 C++ type
fn map_type_to_cpp(ty: &Type) -> &'static str {
    match ty {
        Type::Named { name, .. } => match name.as_str() {
            "Float" => "float",
            "Int" => "int32",
            "Bool" => "bool",
            "String" => "FString",
            _ => "float", // Default fallback
        },
        _ => "float",
    }
}

/// Check if type is FString (needs const ref for parameters)
fn is_string_type(ty: &Type) -> bool {
    match ty {
        Type::Named { name, .. } => name == "String",
        _ => false,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Field, Visibility};
    use kain_core::span::Span;
    use crate::config_ir::ConfigCategory;

    fn create_test_config() -> ConfigStruct {
        ConfigStruct {
            name: "VoxelSettings".to_string(),
            category: ConfigCategory::Game,
            ini_file: None,
            ini_section: None,
            display_name: None,
            fields: vec![],
            original_struct: kain_core::ast::Struct {
                name: "VoxelSettings".to_string(),
                generics: vec![],
                fields: vec![],
                methods: vec![],
                attributes: vec![],
                visibility: Visibility::Public,
                span: Span::default(),
            },
            span: Span::default(),
        }
    }

    fn create_test_field(name: &str, ty_name: &str, blueprint: bool, writable: bool) -> ConfigField {
        ConfigField {
            name: name.to_string(),
            ty: Type::Named {
                name: ty_name.to_string(),
                generics: vec![],
                span: Span::default(),
            },
            default: None,
            display_name: None,
            tooltip: None,
            cvar: None,
            blueprint,
            min: None,
            max: None,
            writable,
            original_field: Field {
                name: name.to_string(),
                ty: Type::Named {
                    name: ty_name.to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                attributes: vec![],
                visibility: Visibility::Public,
                default: None,
                weak: false,
                span: Span::default(),
            },
            span: Span::default(),
        }
    }

    #[test]
    fn test_generate_blueprint_getter_declaration() {
        let config = create_test_config();
        let field = create_test_field("chunk_size", "Float", true, false);

        let result = generate_blueprint_getter_declaration(&config, &field);

        assert!(result.contains("UFUNCTION(BlueprintCallable"));
        assert!(result.contains("Category=\"Voxel Settings\""));
        assert!(result.contains("static float GetChunkSize()"));
        assert!(result.contains("/** Get ChunkSize */"));
    }

    #[test]
    fn test_generate_blueprint_setter_declaration_writable() {
        let config = create_test_config();
        let field = create_test_field("chunk_size", "Float", true, true);

        let result = generate_blueprint_setter_declaration(&config, &field);

        assert!(result.is_some());
        let setter = result.unwrap();
        assert!(setter.contains("UFUNCTION(BlueprintCallable"));
        assert!(setter.contains("Category=\"Voxel Settings\""));
        assert!(setter.contains("static void SetChunkSize(float NewValue)"));
        assert!(setter.contains("/** Set ChunkSize */"));
    }

    #[test]
    fn test_generate_blueprint_setter_declaration_readonly() {
        let config = create_test_config();
        let field = create_test_field("chunk_size", "Float", true, false);

        let result = generate_blueprint_setter_declaration(&config, &field);

        assert!(result.is_none());
    }

    #[test]
    fn test_generate_blueprint_getter_implementation() {
        let config = create_test_config();
        let field = create_test_field("chunk_size", "Float", true, false);

        let result = generate_blueprint_getter_implementation(&config, &field);

        assert!(result.contains("float UVoxelSettings::GetChunkSize()"));
        assert!(result.contains("return Get()->ChunkSize;"));
    }

    #[test]
    fn test_generate_blueprint_setter_implementation_writable() {
        let config = create_test_config();
        let field = create_test_field("chunk_size", "Float", true, true);

        let result = generate_blueprint_setter_implementation(&config, &field);

        assert!(result.is_some());
        let setter = result.unwrap();
        assert!(setter.contains("void UVoxelSettings::SetChunkSize(float NewValue)"));
        assert!(setter.contains("UVoxelSettings* Settings = GetMutableDefault<UVoxelSettings>()"));
        assert!(setter.contains("Settings->ChunkSize = NewValue"));
        assert!(setter.contains("Settings->SaveConfig()"));
    }

    #[test]
    fn test_generate_blueprint_setter_implementation_readonly() {
        let config = create_test_config();
        let field = create_test_field("chunk_size", "Float", true, false);

        let result = generate_blueprint_setter_implementation(&config, &field);

        assert!(result.is_none());
    }

    #[test]
    fn test_type_mapping_float() {
        let ty = Type::Named {
            name: "Float".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(map_type_to_cpp(&ty), "float");
    }

    #[test]
    fn test_type_mapping_int() {
        let ty = Type::Named {
            name: "Int".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(map_type_to_cpp(&ty), "int32");
    }

    #[test]
    fn test_type_mapping_bool() {
        let ty = Type::Named {
            name: "Bool".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(map_type_to_cpp(&ty), "bool");
    }

    #[test]
    fn test_type_mapping_string() {
        let ty = Type::Named {
            name: "String".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(map_type_to_cpp(&ty), "FString");
    }

    #[test]
    fn test_string_type_uses_const_ref() {
        let config = create_test_config();
        let field = create_test_field("name", "String", true, true);

        let setter = generate_blueprint_setter_declaration(&config, &field).unwrap();

        assert!(setter.contains("const FString& NewValue"));
    }

    #[test]
    fn test_generate_blueprint_accessors_header() {
        let mut config = create_test_config();
        config.fields = vec![
            create_test_field("chunk_size", "Float", true, false),
            create_test_field("max_lod", "Int", true, true),
            create_test_field("debug_vis", "Bool", false, false), // Not blueprint
        ];

        let result = generate_blueprint_accessors_header(&config);

        // Should have 3 declarations: GetChunkSize, GetMaxLod, SetMaxLod
        assert_eq!(result.len(), 3);
        assert!(result[0].contains("GetChunkSize"));
        assert!(result[1].contains("GetMaxLod"));
        assert!(result[2].contains("SetMaxLod"));
    }

    #[test]
    fn test_generate_blueprint_accessors_cpp() {
        let mut config = create_test_config();
        config.fields = vec![
            create_test_field("chunk_size", "Float", true, false),
            create_test_field("max_lod", "Int", true, true),
            create_test_field("debug_vis", "Bool", false, false), // Not blueprint
        ];

        let result = generate_blueprint_accessors_cpp(&config);

        // Should have 3 implementations: GetChunkSize, GetMaxLod, SetMaxLod
        assert_eq!(result.len(), 3);
        assert!(result[0].contains("UVoxelSettings::GetChunkSize"));
        assert!(result[1].contains("UVoxelSettings::GetMaxLod"));
        assert!(result[2].contains("UVoxelSettings::SetMaxLod"));
    }

    #[test]
    fn test_category_naming() {
        let config = create_test_config();
        let field = create_test_field("chunk_size", "Float", true, false);

        let result = generate_blueprint_getter_declaration(&config, &field);

        assert!(result.contains("Category=\"Voxel Settings\""));
    }
}
