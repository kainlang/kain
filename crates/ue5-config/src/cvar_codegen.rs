//! Console Variable (CVar) Code Generation
//!
//! This module generates TAutoConsoleVariable<T> declarations and callback methods
//! for KAIN @config fields with @setting(cvar: "...") attributes.

use crate::config_ir::{ConfigField, ConfigStruct};
use anyhow::Result;

/// Generate TAutoConsoleVariable<T> declarations for all fields with CVars
///
/// # Example Output
///
/// ```cpp
/// static TAutoConsoleVariable<float> CVarChunkSize(
///     TEXT("voxel.ChunkSize"),
///     100.0f,
///     TEXT("Chunk Size"),
///     ECVF_Default);
/// ```
pub fn generate_cvar_declarations(config: &ConfigStruct, plugin_name: &str) -> Result<Vec<String>> {
    let mut declarations = Vec::new();

    for field in &config.fields {
        if let Some(cvar_name) = field.get_cvar_name(plugin_name) {
            let declaration = generate_single_cvar_declaration(field, &cvar_name)?;
            declarations.push(declaration);
        }
    }

    Ok(declarations)
}

/// Generate a single TAutoConsoleVariable declaration
fn generate_single_cvar_declaration(field: &ConfigField, cvar_name: &str) -> Result<String> {
    let cpp_type = get_cvar_cpp_type(&field.ty)?;
    let var_name = format!("CVar{}", field.ue5_property_name());
    let default_value = format_default_value(field)?;
    let display_name = field.get_display_name();
    let help_text = field.tooltip.as_deref().unwrap_or(&display_name);

    Ok(format!(
        r#"static TAutoConsoleVariable<{cpp_type}> {var_name}(
	TEXT("{cvar_name}"),
	{default_value},
	TEXT("{help_text}"),
	ECVF_Default);"#,
        cpp_type = cpp_type,
        var_name = var_name,
        cvar_name = cvar_name,
        default_value = default_value,
        help_text = help_text
    ))
}

/// Get the C++ type for TAutoConsoleVariable<T>
fn get_cvar_cpp_type(ty: &kain_core::ast::Type) -> Result<&'static str> {
    use kain_core::ast::Type;

    match ty {
        Type::Named { name, .. } => match name.as_str() {
            "Float" => Ok("float"),
            "Int" => Ok("int32"),
            "Bool" => Ok("bool"),
            "String" => Ok("FString"),
            _ => anyhow::bail!("Unsupported CVar type: {}", name),
        },
        _ => anyhow::bail!("Unsupported CVar type: {:?}", ty),
    }
}

/// Format the default value for a CVar declaration
fn format_default_value(field: &ConfigField) -> Result<String> {
    use kain_core::ast::Expr;

    match &field.default {
        Some(Expr::Float(val, _)) => {
            // Ensure we always have a decimal point
            let s = val.to_string();
            if s.contains('.') {
                Ok(format!("{}f", s))
            } else {
                Ok(format!("{}.0f", s))
            }
        }
        Some(Expr::Int(val, _)) => Ok(val.to_string()),
        Some(Expr::Bool(val, _)) => Ok(if *val {
            "true".to_string()
        } else {
            "false".to_string()
        }),
        Some(Expr::String(val, _)) => Ok(format!("TEXT(\"{}\")", val)),
        None => {
            // Use type-appropriate defaults
            match get_cvar_cpp_type(&field.ty)? {
                "float" => Ok("0.0f".to_string()),
                "int32" => Ok("0".to_string()),
                "bool" => Ok("false".to_string()),
                "FString" => Ok("TEXT(\"\")".to_string()),
                _ => anyhow::bail!("Cannot determine default value for field: {}", field.name),
            }
        }
        _ => anyhow::bail!(
            "Unsupported default value expression for field: {}",
            field.name
        ),
    }
}

/// Generate callback methods for syncing CVars to UPROPERTY fields
///
/// # Example Output
///
/// ```cpp
/// void UVoxelSettings::OnChunkSizeChanged()
/// {
///     ChunkSize = CVarChunkSize.GetValueOnGameThread();
/// }
/// ```
pub fn generate_cvar_callbacks(config: &ConfigStruct, plugin_name: &str) -> Result<Vec<String>> {
    let mut callbacks = Vec::new();

    for field in &config.fields {
        if field.get_cvar_name(plugin_name).is_some() {
            let callback = generate_single_cvar_callback(config, field)?;
            callbacks.push(callback);
        }
    }

    Ok(callbacks)
}

/// Generate a single CVar callback method
fn generate_single_cvar_callback(config: &ConfigStruct, field: &ConfigField) -> Result<String> {
    let class_name = config.ue5_class_name();
    let method_name = format!("On{}Changed", field.ue5_property_name());
    let property_name = field.ue5_property_name();
    let cvar_name = format!("CVar{}", field.ue5_property_name());

    Ok(format!(
        r#"void {class_name}::{method_name}()
{{
	{property_name} = {cvar_name}.GetValueOnGameThread();
}}"#,
        class_name = class_name,
        method_name = method_name,
        property_name = property_name,
        cvar_name = cvar_name
    ))
}

/// Generate callback method declarations for header file
///
/// # Example Output
///
/// ```cpp
/// void OnChunkSizeChanged();
/// ```
pub fn generate_cvar_callback_declarations(
    config: &ConfigStruct,
    plugin_name: &str,
) -> Result<Vec<String>> {
    let mut declarations = Vec::new();

    for field in &config.fields {
        if field.get_cvar_name(plugin_name).is_some() {
            let method_name = format!("On{}Changed", field.ue5_property_name());
            declarations.push(format!("void {}();", method_name));
        }
    }

    Ok(declarations)
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Expr, Field, Struct, Type, Visibility};
    use kain_core::span::Span;

    fn make_test_config() -> ConfigStruct {
        ConfigStruct {
            name: "VoxelSettings".to_string(),
            category: crate::config_ir::ConfigCategory::Game,
            ini_file: None,
            ini_section: None,
            display_name: None,
            fields: vec![],
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

    fn make_test_field(
        name: &str,
        ty_name: &str,
        default: Option<Expr>,
        cvar: Option<String>,
    ) -> ConfigField {
        ConfigField {
            name: name.to_string(),
            ty: Type::Named {
                name: ty_name.to_string(),
                generics: vec![],
                span: Span::default(),
            },
            default,
            display_name: None,
            tooltip: Some("Test tooltip".to_string()),
            cvar,
            blueprint: false,
            min: None,
            max: None,
            writable: false,
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
    fn test_generate_cvar_declarations_float() {
        let mut config = make_test_config();
        config.fields.push(make_test_field(
            "chunk_size",
            "Float",
            Some(Expr::Float(100.0, Span::default())),
            Some("voxel.ChunkSize".to_string()),
        ));

        let declarations = generate_cvar_declarations(&config, "MyPlugin").unwrap();
        assert_eq!(declarations.len(), 1);
        assert!(declarations[0].contains("TAutoConsoleVariable<float>"));
        assert!(declarations[0].contains("CVarChunkSize"));
        assert!(declarations[0].contains("voxel.ChunkSize"));
        assert!(declarations[0].contains("100.0f"));
        assert!(declarations[0].contains("ECVF_Default"));
    }

    #[test]
    fn test_generate_cvar_declarations_int() {
        let mut config = make_test_config();
        config.fields.push(make_test_field(
            "max_lod",
            "Int",
            Some(Expr::Int(4, Span::default())),
            Some("voxel.MaxLOD".to_string()),
        ));

        let declarations = generate_cvar_declarations(&config, "MyPlugin").unwrap();
        assert_eq!(declarations.len(), 1);
        assert!(declarations[0].contains("TAutoConsoleVariable<int32>"));
        assert!(declarations[0].contains("CVarMaxLod"));
        assert!(declarations[0].contains("voxel.MaxLOD"));
        assert!(declarations[0].contains("4"));
    }

    #[test]
    fn test_generate_cvar_declarations_bool() {
        let mut config = make_test_config();
        config.fields.push(make_test_field(
            "debug_vis",
            "Bool",
            Some(Expr::Bool(false, Span::default())),
            Some("voxel.DebugVis".to_string()),
        ));

        let declarations = generate_cvar_declarations(&config, "MyPlugin").unwrap();
        assert_eq!(declarations.len(), 1);
        assert!(declarations[0].contains("TAutoConsoleVariable<bool>"));
        assert!(declarations[0].contains("CVarDebugVis"));
        assert!(declarations[0].contains("voxel.DebugVis"));
        assert!(declarations[0].contains("false"));
    }

    #[test]
    fn test_generate_cvar_declarations_no_cvar() {
        let mut config = make_test_config();
        config.fields.push(make_test_field(
            "chunk_size",
            "Float",
            Some(Expr::Float(100.0, Span::default())),
            None, // No CVar
        ));

        let declarations = generate_cvar_declarations(&config, "MyPlugin").unwrap();
        assert_eq!(declarations.len(), 0);
    }

    #[test]
    fn test_generate_cvar_callbacks() {
        let mut config = make_test_config();
        config.fields.push(make_test_field(
            "chunk_size",
            "Float",
            Some(Expr::Float(100.0, Span::default())),
            Some("voxel.ChunkSize".to_string()),
        ));

        let callbacks = generate_cvar_callbacks(&config, "MyPlugin").unwrap();
        assert_eq!(callbacks.len(), 1);
        assert!(callbacks[0].contains("void UVoxelSettings::OnChunkSizeChanged()"));
        assert!(callbacks[0].contains("ChunkSize = CVarChunkSize.GetValueOnGameThread()"));
    }

    #[test]
    fn test_generate_cvar_callback_declarations() {
        let mut config = make_test_config();
        config.fields.push(make_test_field(
            "chunk_size",
            "Float",
            Some(Expr::Float(100.0, Span::default())),
            Some("voxel.ChunkSize".to_string()),
        ));

        let declarations = generate_cvar_callback_declarations(&config, "MyPlugin").unwrap();
        assert_eq!(declarations.len(), 1);
        assert_eq!(declarations[0], "void OnChunkSizeChanged();");
    }

    #[test]
    fn test_get_cvar_cpp_type() {
        let float_ty = Type::Named {
            name: "Float".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(get_cvar_cpp_type(&float_ty).unwrap(), "float");

        let int_ty = Type::Named {
            name: "Int".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(get_cvar_cpp_type(&int_ty).unwrap(), "int32");

        let bool_ty = Type::Named {
            name: "Bool".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(get_cvar_cpp_type(&bool_ty).unwrap(), "bool");

        let string_ty = Type::Named {
            name: "String".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(get_cvar_cpp_type(&string_ty).unwrap(), "FString");
    }

    #[test]
    fn test_format_default_value() {
        let float_field = make_test_field(
            "test",
            "Float",
            Some(Expr::Float(123.45, Span::default())),
            None,
        );
        assert_eq!(format_default_value(&float_field).unwrap(), "123.45f");

        let int_field = make_test_field("test", "Int", Some(Expr::Int(42, Span::default())), None);
        assert_eq!(format_default_value(&int_field).unwrap(), "42");

        let bool_field = make_test_field(
            "test",
            "Bool",
            Some(Expr::Bool(true, Span::default())),
            None,
        );
        assert_eq!(format_default_value(&bool_field).unwrap(), "true");

        let string_field = make_test_field(
            "test",
            "String",
            Some(Expr::String("hello".to_string(), Span::default())),
            None,
        );
        assert_eq!(
            format_default_value(&string_field).unwrap(),
            "TEXT(\"hello\")"
        );
    }
}
