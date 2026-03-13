//! UDeveloperSettings Codegen — Generate UE5 C++ for Configuration Systems
//!
//! Generates complete UDeveloperSettings subclasses with:
//! - UCLASS specifiers (Config=X, DefaultConfig, meta=(DisplayName=...))
//! - UPROPERTY specifiers (Config, EditAnywhere, Category, meta)
//! - Constructor with default value initialization
//! - Singleton Get() accessor
//! - GetContainerName() override
//! - PostInitProperties() override
//! - PostEditChangeProperty() override (WITH_EDITOR)

use crate::config_ir::ConfigStruct;
use kain_core::ast::{Expr, Type};
use kain_core::error::{KainError, KainResult};
use minijinja::{context, Environment};

/// Output structure for developer settings codegen
#[derive(Debug, Clone)]
pub struct DeveloperSettingsOutput {
    pub header: String,
    pub source: String,
}

/// Generate complete C++ code for a UDeveloperSettings subclass
pub fn generate(config: &ConfigStruct, plugin_name: &str) -> KainResult<DeveloperSettingsOutput> {
    let header = generate_developer_settings_header(config, plugin_name)?;
    let source = generate_developer_settings_cpp(config, plugin_name)?;

    Ok(DeveloperSettingsOutput { header, source })
}

/// Generate header file (.h)
pub fn generate_developer_settings_header(
    config: &ConfigStruct,
    plugin_name: &str,
) -> KainResult<String> {
    let mut env = Environment::new();
    env.add_template(
        "developer_settings.h",
        include_str!("templates/developer_settings.h.jinja"),
    )
    .map_err(|e| KainError::codegen_error(format!("Failed to add template: {}", e)))?;

    let template = env
        .get_template("developer_settings.h")
        .map_err(|e| KainError::codegen_error(format!("Failed to get template: {}", e)))?;

    let class_name = config.ue5_class_name();
    let api_macro = format!("{}_API", plugin_name.to_uppercase());
    let display_name = config.get_display_name();
    let config_category = config.category.uclass_specifier();

    // Convert fields to template-friendly format
    let fields: Vec<_> = config
        .fields
        .iter()
        .map(|field| {
            let property_name = field.ue5_property_name();
            let cpp_type = map_type_to_cpp(&field.ty);
            let category = "Settings"; // Default category
            let display_name = field.get_display_name();

            // Build meta specifiers
            let mut meta_parts = vec![format!("DisplayName=\"{}\"", display_name)];

            if let Some(tooltip) = &field.tooltip {
                meta_parts.push(format!("ToolTip=\"{}\"", tooltip));
            }

            if let Some(min) = field.min {
                meta_parts.push(format!("ClampMin=\"{}\"", min));
            }

            if let Some(max) = field.max {
                meta_parts.push(format!("ClampMax=\"{}\"", max));
            }

            let meta = if meta_parts.is_empty() {
                String::new()
            } else {
                format!(", meta=({})", meta_parts.join(", "))
            };

            context! {
                property_name => property_name,
                cpp_type => cpp_type,
                category => category,
                meta => meta,
            }
        })
        .collect();

    let ctx = context! {
        class_name => class_name,
        api_macro => api_macro,
        display_name => display_name,
        config_category => config_category,
        fields => fields,
        struct_name => &config.name,
    };

    template
        .render(ctx)
        .map_err(|e| KainError::codegen_error(format!("Failed to render template: {}", e)))
}

/// Generate implementation file (.cpp)
pub fn generate_developer_settings_cpp(
    config: &ConfigStruct,
    _plugin_name: &str,
) -> KainResult<String> {
    let mut env = Environment::new();
    env.add_template(
        "developer_settings.cpp",
        include_str!("templates/developer_settings.cpp.jinja"),
    )
    .map_err(|e| KainError::codegen_error(format!("Failed to add template: {}", e)))?;

    let template = env
        .get_template("developer_settings.cpp")
        .map_err(|e| KainError::codegen_error(format!("Failed to get template: {}", e)))?;

    let class_name = config.ue5_class_name();
    let display_name = config.get_display_name();

    // Convert fields to template-friendly format with default values
    let fields: Vec<_> = config
        .fields
        .iter()
        .map(|field| {
            let property_name = field.ue5_property_name();
            let default_value = field
                .default
                .as_ref()
                .map(|expr| format_default_value(expr, &field.ty))
                .unwrap_or_else(|| get_default_value_for_type(&field.ty));

            context! {
                property_name => property_name,
                default_value => default_value,
            }
        })
        .collect();

    let ctx = context! {
        class_name => class_name,
        display_name => display_name,
        struct_name => &config.name,
        fields => fields,
    };

    template
        .render(ctx)
        .map_err(|e| KainError::codegen_error(format!("Failed to render template: {}", e)))
}

/// Map KAIN type to C++ type
fn map_type_to_cpp(ty: &Type) -> String {
    match ty {
        Type::Named { name, .. } => match name.as_str() {
            "Float" => "float".to_string(),
            "Int" => "int32".to_string(),
            "Bool" => "bool".to_string(),
            "String" => "FString".to_string(),
            other => other.to_string(), // Pass through for custom types
        },
        _ => "float".to_string(), // Default fallback
    }
}

/// Format default value expression to C++ literal
fn format_default_value(expr: &Expr, ty: &Type) -> String {
    match expr {
        Expr::Float(val, _) => {
            // Ensure we always have a decimal point
            let s = val.to_string();
            if s.contains('.') {
                format!("{}f", s)
            } else {
                format!("{}.0f", s)
            }
        }
        Expr::Int(val, _) => val.to_string(),
        Expr::Bool(val, _) => {
            if *val {
                "true".to_string()
            } else {
                "false".to_string()
            }
        }
        Expr::String(val, _) => format!("TEXT(\"{}\")", val),
        _ => get_default_value_for_type(ty),
    }
}

/// Get default value for a type when no explicit default is provided
fn get_default_value_for_type(ty: &Type) -> String {
    match ty {
        Type::Named { name, .. } => match name.as_str() {
            "Float" => "0.0f".to_string(),
            "Int" => "0".to_string(),
            "Bool" => "false".to_string(),
            "String" => "TEXT(\"\")".to_string(),
            _ => "{}".to_string(), // Default constructor
        },
        _ => "{}".to_string(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::config_ir::{ConfigCategory, ConfigField};
    use kain_core::ast::{Field, Struct, Visibility};
    use kain_core::span::Span;

    fn create_test_config() -> ConfigStruct {
        ConfigStruct {
            name: "VoxelSettings".to_string(),
            category: ConfigCategory::Game,
            ini_file: None,
            ini_section: None,
            display_name: None,
            fields: vec![ConfigField {
                name: "chunk_size".to_string(),
                ty: Type::Named {
                    name: "Float".to_string(),
                    generics: vec![],
                    span: Span::default(),
                },
                default: Some(Expr::Float(100.0, Span::default())),
                display_name: Some("Chunk Size".to_string()),
                tooltip: Some("Size of voxel chunks in world units".to_string()),
                cvar: Some("voxel.ChunkSize".to_string()),
                blueprint: true,
                min: Some(10.0),
                max: Some(1000.0),
                writable: false,
                original_field: Field {
                    name: "chunk_size".to_string(),
                    ty: Type::Named {
                        name: "Float".to_string(),
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
            }],
            original_struct: Struct {
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

    #[test]
    fn test_map_type_to_cpp() {
        let float_type = Type::Named {
            name: "Float".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(map_type_to_cpp(&float_type), "float");

        let int_type = Type::Named {
            name: "Int".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(map_type_to_cpp(&int_type), "int32");

        let bool_type = Type::Named {
            name: "Bool".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(map_type_to_cpp(&bool_type), "bool");

        let string_type = Type::Named {
            name: "String".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(map_type_to_cpp(&string_type), "FString");
    }

    #[test]
    fn test_format_default_value_float() {
        let expr = Expr::Float(100.0, Span::default());
        let ty = Type::Named {
            name: "Float".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(format_default_value(&expr, &ty), "100.0f");
    }

    #[test]
    fn test_format_default_value_int() {
        let expr = Expr::Int(42, Span::default());
        let ty = Type::Named {
            name: "Int".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(format_default_value(&expr, &ty), "42");
    }

    #[test]
    fn test_format_default_value_bool() {
        let expr = Expr::Bool(true, Span::default());
        let ty = Type::Named {
            name: "Bool".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(format_default_value(&expr, &ty), "true");
    }

    #[test]
    fn test_format_default_value_string() {
        let expr = Expr::String("Hello".to_string(), Span::default());
        let ty = Type::Named {
            name: "String".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(format_default_value(&expr, &ty), "TEXT(\"Hello\")");
    }

    #[test]
    fn test_get_default_value_for_type() {
        let float_type = Type::Named {
            name: "Float".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(get_default_value_for_type(&float_type), "0.0f");

        let int_type = Type::Named {
            name: "Int".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(get_default_value_for_type(&int_type), "0");

        let bool_type = Type::Named {
            name: "Bool".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(get_default_value_for_type(&bool_type), "false");

        let string_type = Type::Named {
            name: "String".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(get_default_value_for_type(&string_type), "TEXT(\"\")");
    }

    #[test]
    fn test_generate_header() {
        let config = create_test_config();
        let result = generate_developer_settings_header(&config, "MyPlugin");
        assert!(result.is_ok());

        let header = result.unwrap();
        assert!(header.contains("UVoxelSettings"));
        assert!(header.contains("MYPLUGIN_API"));
        assert!(header.contains("Config=Game"));
        assert!(header.contains("DefaultConfig"));
        assert!(header.contains("DisplayName=\"Voxel Settings\""));
        assert!(header.contains("UPROPERTY"));
        assert!(header.contains("float ChunkSize"));
        assert!(header.contains("ClampMin=\"10\""));
        assert!(header.contains("ClampMax=\"1000\""));
    }

    #[test]
    fn test_generate_cpp() {
        let config = create_test_config();
        let result = generate_developer_settings_cpp(&config, "MyPlugin");
        assert!(result.is_ok());

        let cpp = result.unwrap();
        println!("Generated CPP:\n{}", cpp);
        assert!(cpp.contains("UVoxelSettings::UVoxelSettings()"));
        assert!(cpp.contains("ChunkSize(100.0f)"));
        assert!(cpp.contains("CategoryName = TEXT(\"Plugins\")"));
        assert!(cpp.contains("SectionName = TEXT(\"Voxel Settings\")"));
        assert!(cpp.contains("const UVoxelSettings* UVoxelSettings::Get()"));
        assert!(cpp.contains("GetDefault<UVoxelSettings>()"));
        assert!(cpp.contains("FName UVoxelSettings::GetContainerName()"));
        assert!(cpp.contains("PostInitProperties()"));
        assert!(cpp.contains("PostEditChangeProperty"));
    }
}
