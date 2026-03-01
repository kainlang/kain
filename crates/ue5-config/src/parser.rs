//! Attribute Parser for @config and @setting
//!
//! This module parses @config and @setting attributes from KAIN AST
//! and extracts their parameters into structured IR types.

use crate::config_ir::{ConfigCategory, ConfigField, ConfigStruct};
use anyhow::{anyhow, Result};
use kain_core::ast::{Attribute, Expr, Field, Struct};

/// Parse @config attribute from a struct
///
/// Expected format:
/// ```kain
/// @config(category: "Game", file: "DefaultGame.ini", section: "MyPlugin", display_name: "My Settings")
/// struct MySettings:
///     ...
/// ```
pub fn parse_config_attribute(struct_def: &Struct) -> Result<Option<ConfigStruct>> {
    // Find @config attribute
    let config_attr = struct_def
        .attributes
        .iter()
        .find(|attr| attr.name == "config");

    let Some(config_attr) = config_attr else {
        return Ok(None);
    };

    // Parse category (required)
    let category = extract_string_param(&config_attr.args, "category")
        .ok_or_else(|| anyhow!("@config requires 'category' parameter (Game, Engine, Editor, EditorPerProjectUserSettings)"))?;

    let category = ConfigCategory::from_str(&category)
        .ok_or_else(|| anyhow!("Invalid config category '{}'. Valid values: Game, Engine, Editor, EditorPerProjectUserSettings", category))?;

    // Parse optional parameters
    let ini_file = extract_string_param(&config_attr.args, "file");
    let ini_section = extract_string_param(&config_attr.args, "section");
    let display_name = extract_string_param(&config_attr.args, "display_name");

    // Parse fields with @setting attributes
    let mut config_fields = Vec::new();
    for field in &struct_def.fields {
        if let Some(config_field) = parse_setting_attribute(field)? {
            config_fields.push(config_field);
        }
    }

    Ok(Some(ConfigStruct {
        name: struct_def.name.clone(),
        category,
        ini_file,
        ini_section,
        display_name,
        fields: config_fields,
        original_struct: struct_def.clone(),
        span: struct_def.span,
    }))
}

/// Parse @setting attribute from a field
///
/// Expected format:
/// ```kain
/// @setting(
///     display_name: "Chunk Size",
///     tooltip: "Size of voxel chunks in world units",
///     cvar: "voxel.ChunkSize",
///     blueprint: true,
///     min: 10.0,
///     max: 1000.0,
///     writable: false
/// )
/// chunk_size: Float = 100.0
/// ```
pub fn parse_setting_attribute(field: &Field) -> Result<Option<ConfigField>> {
    // Find @setting attribute
    let setting_attr = field
        .attributes
        .iter()
        .find(|attr| attr.name == "setting");

    let Some(setting_attr) = setting_attr else {
        return Ok(None);
    };

    // Parse optional parameters
    let display_name = extract_string_param(&setting_attr.args, "display_name");
    let tooltip = extract_string_param(&setting_attr.args, "tooltip");
    let cvar = extract_string_param(&setting_attr.args, "cvar");
    let blueprint = extract_bool_param(&setting_attr.args, "blueprint").unwrap_or(false);
    let min = extract_float_param(&setting_attr.args, "min");
    let max = extract_float_param(&setting_attr.args, "max");
    let writable = extract_bool_param(&setting_attr.args, "writable").unwrap_or(false);

    Ok(Some(ConfigField {
        name: field.name.clone(),
        ty: field.ty.clone(),
        default: field.default.clone(),
        display_name,
        tooltip,
        cvar,
        blueprint,
        min,
        max,
        writable,
        original_field: field.clone(),
        span: field.span,
    }))
}

/// Extract a string parameter from attribute arguments
///
/// Looks for named arguments like `name: "value"` or positional string literals
fn extract_string_param(args: &[Expr], param_name: &str) -> Option<String> {
    for arg in args {
        match arg {
            // Actual KAIN attribute encoding: @attr(name: value) => Expr::Tuple([Ident(name), value])
            Expr::Tuple(elems, _) if elems.len() == 2 => {
                if let Expr::Ident(name, _) = &elems[0] {
                    if name == param_name {
                        if let Expr::String(value, _) = &elems[1] {
                            return Some(value.clone());
                        }
                    }
                }
            }
            // Named argument via BinaryOp::Assign (name = value)
            Expr::Binary { left, op, right, .. } if matches!(op, kain_core::ast::BinaryOp::Assign) => {
                if let Expr::Ident(name, _) = &**left {
                    if name == param_name {
                        if let Expr::String(value, _) = &**right {
                            return Some(value.clone());
                        }
                    }
                }
            }
            // Struct-like syntax: Struct { name: "value" }
            Expr::Struct { fields, .. } => {
                for (field_name, field_value) in fields {
                    if field_name == param_name {
                        if let Expr::String(value, _) = field_value {
                            return Some(value.clone());
                        }
                    }
                }
            }
            _ => {}
        }
    }
    None
}

/// Extract a boolean parameter from attribute arguments
fn extract_bool_param(args: &[Expr], param_name: &str) -> Option<bool> {
    for arg in args {
        match arg {
            // Actual KAIN attribute encoding: @attr(name: value) => Expr::Tuple([Ident(name), value])
            Expr::Tuple(elems, _) if elems.len() == 2 => {
                if let Expr::Ident(name, _) = &elems[0] {
                    if name == param_name {
                        if let Expr::Bool(value, _) = &elems[1] {
                            return Some(*value);
                        }
                    }
                }
            }
            // Named argument via BinaryOp::Assign (name = value)
            Expr::Binary { left, op, right, .. } if matches!(op, kain_core::ast::BinaryOp::Assign) => {
                if let Expr::Ident(name, _) = &**left {
                    if name == param_name {
                        if let Expr::Bool(value, _) = &**right {
                            return Some(*value);
                        }
                    }
                }
            }
            // Struct-like syntax: Struct { name: true }
            Expr::Struct { fields, .. } => {
                for (field_name, field_value) in fields {
                    if field_name == param_name {
                        if let Expr::Bool(value, _) = field_value {
                            return Some(*value);
                        }
                    }
                }
            }
            _ => {}
        }
    }
    None
}

/// Extract a float parameter from attribute arguments
fn extract_float_param(args: &[Expr], param_name: &str) -> Option<f64> {
    for arg in args {
        match arg {
            // Actual KAIN attribute encoding: @attr(name: value) => Expr::Tuple([Ident(name), value])
            Expr::Tuple(elems, _) if elems.len() == 2 => {
                if let Expr::Ident(name, _) = &elems[0] {
                    if name == param_name {
                        match &elems[1] {
                            Expr::Float(value, _) => return Some(*value),
                            Expr::Int(value, _) => return Some(*value as f64),
                            _ => {}
                        }
                    }
                }
            }
            // Named argument via BinaryOp::Assign (name = value)
            Expr::Binary { left, op, right, .. } if matches!(op, kain_core::ast::BinaryOp::Assign) => {
                if let Expr::Ident(name, _) = &**left {
                    if name == param_name {
                        match &**right {
                            Expr::Float(value, _) => return Some(*value),
                            Expr::Int(value, _) => return Some(*value as f64),
                            _ => {}
                        }
                    }
                }
            }
            // Struct-like syntax: Struct { name: 10.0 }
            Expr::Struct { fields, .. } => {
                for (field_name, field_value) in fields {
                    if field_name == param_name {
                        match field_value {
                            Expr::Float(value, _) => return Some(*value),
                            Expr::Int(value, _) => return Some(*value as f64),
                            _ => {}
                        }
                    }
                }
            }
            _ => {}
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{BinaryOp, Type, Visibility};
    use kain_core::span::Span;

    fn make_string_expr(s: &str) -> Expr {
        Expr::String(s.to_string(), Span::default())
    }

    fn make_bool_expr(b: bool) -> Expr {
        Expr::Bool(b, Span::default())
    }

    fn make_float_expr(f: f64) -> Expr {
        Expr::Float(f, Span::default())
    }

    fn make_named_arg(name: &str, value: Expr) -> Expr {
        Expr::Binary {
            left: Box::new(Expr::Ident(name.to_string(), Span::default())),
            op: BinaryOp::Assign,
            right: Box::new(value),
            span: Span::default(),
        }
    }

    #[test]
    fn test_extract_string_param() {
        let args = vec![
            make_named_arg("category", make_string_expr("Game")),
            make_named_arg("file", make_string_expr("DefaultGame.ini")),
        ];

        assert_eq!(extract_string_param(&args, "category"), Some("Game".to_string()));
        assert_eq!(extract_string_param(&args, "file"), Some("DefaultGame.ini".to_string()));
        assert_eq!(extract_string_param(&args, "missing"), None);
    }

    #[test]
    fn test_extract_bool_param() {
        let args = vec![
            make_named_arg("blueprint", make_bool_expr(true)),
            make_named_arg("writable", make_bool_expr(false)),
        ];

        assert_eq!(extract_bool_param(&args, "blueprint"), Some(true));
        assert_eq!(extract_bool_param(&args, "writable"), Some(false));
        assert_eq!(extract_bool_param(&args, "missing"), None);
    }

    #[test]
    fn test_extract_float_param() {
        let args = vec![
            make_named_arg("min", make_float_expr(10.0)),
            make_named_arg("max", make_float_expr(1000.0)),
        ];

        assert_eq!(extract_float_param(&args, "min"), Some(10.0));
        assert_eq!(extract_float_param(&args, "max"), Some(1000.0));
        assert_eq!(extract_float_param(&args, "missing"), None);
    }

    #[test]
    fn test_parse_config_attribute_basic() {
        let struct_def = Struct {
            name: "VoxelSettings".to_string(),
            generics: vec![],
            fields: vec![],
            methods: vec![],
            attributes: vec![Attribute {
                name: "config".to_string(),
                args: vec![make_named_arg("category", make_string_expr("Game"))],
                span: Span::default(),
            }],
            visibility: Visibility::Public,
            span: Span::default(),
        };

        let result = parse_config_attribute(&struct_def).unwrap();
        assert!(result.is_some());

        let config = result.unwrap();
        assert_eq!(config.name, "VoxelSettings");
        assert_eq!(config.category, ConfigCategory::Game);
        assert_eq!(config.ini_file, None);
        assert_eq!(config.ini_section, None);
        assert_eq!(config.display_name, None);
    }

    #[test]
    fn test_parse_config_attribute_full() {
        let struct_def = Struct {
            name: "VoxelSettings".to_string(),
            generics: vec![],
            fields: vec![],
            methods: vec![],
            attributes: vec![Attribute {
                name: "config".to_string(),
                args: vec![
                    make_named_arg("category", make_string_expr("Game")),
                    make_named_arg("file", make_string_expr("CustomGame.ini")),
                    make_named_arg("section", make_string_expr("CustomSection")),
                    make_named_arg("display_name", make_string_expr("Voxel Settings")),
                ],
                span: Span::default(),
            }],
            visibility: Visibility::Public,
            span: Span::default(),
        };

        let result = parse_config_attribute(&struct_def).unwrap();
        assert!(result.is_some());

        let config = result.unwrap();
        assert_eq!(config.name, "VoxelSettings");
        assert_eq!(config.category, ConfigCategory::Game);
        assert_eq!(config.ini_file, Some("CustomGame.ini".to_string()));
        assert_eq!(config.ini_section, Some("CustomSection".to_string()));
        assert_eq!(config.display_name, Some("Voxel Settings".to_string()));
    }

    #[test]
    fn test_parse_config_attribute_missing() {
        let struct_def = Struct {
            name: "VoxelSettings".to_string(),
            generics: vec![],
            fields: vec![],
            methods: vec![],
            attributes: vec![],
            visibility: Visibility::Public,
            span: Span::default(),
        };

        let result = parse_config_attribute(&struct_def).unwrap();
        assert!(result.is_none());
    }

    #[test]
    fn test_parse_setting_attribute_basic() {
        let field = Field {
            name: "chunk_size".to_string(),
            ty: Type::Named {
                name: "Float".to_string(),
                generics: vec![],
                span: Span::default(),
            },
            attributes: vec![Attribute {
                name: "setting".to_string(),
                args: vec![],
                span: Span::default(),
            }],
            visibility: Visibility::Public,
            default: Some(make_float_expr(100.0)),
            weak: false,
            span: Span::default(),
        };

        let result = parse_setting_attribute(&field).unwrap();
        assert!(result.is_some());

        let setting = result.unwrap();
        assert_eq!(setting.name, "chunk_size");
        assert_eq!(setting.display_name, None);
        assert_eq!(setting.tooltip, None);
        assert_eq!(setting.cvar, None);
        assert_eq!(setting.blueprint, false);
        assert_eq!(setting.min, None);
        assert_eq!(setting.max, None);
        assert_eq!(setting.writable, false);
    }

    #[test]
    fn test_parse_setting_attribute_full() {
        let field = Field {
            name: "chunk_size".to_string(),
            ty: Type::Named {
                name: "Float".to_string(),
                generics: vec![],
                span: Span::default(),
            },
            attributes: vec![Attribute {
                name: "setting".to_string(),
                args: vec![
                    make_named_arg("display_name", make_string_expr("Chunk Size")),
                    make_named_arg("tooltip", make_string_expr("Size of voxel chunks")),
                    make_named_arg("cvar", make_string_expr("voxel.ChunkSize")),
                    make_named_arg("blueprint", make_bool_expr(true)),
                    make_named_arg("min", make_float_expr(10.0)),
                    make_named_arg("max", make_float_expr(1000.0)),
                    make_named_arg("writable", make_bool_expr(true)),
                ],
                span: Span::default(),
            }],
            visibility: Visibility::Public,
            default: Some(make_float_expr(100.0)),
            weak: false,
            span: Span::default(),
        };

        let result = parse_setting_attribute(&field).unwrap();
        assert!(result.is_some());

        let setting = result.unwrap();
        assert_eq!(setting.name, "chunk_size");
        assert_eq!(setting.display_name, Some("Chunk Size".to_string()));
        assert_eq!(setting.tooltip, Some("Size of voxel chunks".to_string()));
        assert_eq!(setting.cvar, Some("voxel.ChunkSize".to_string()));
        assert_eq!(setting.blueprint, true);
        assert_eq!(setting.min, Some(10.0));
        assert_eq!(setting.max, Some(1000.0));
        assert_eq!(setting.writable, true);
    }

    #[test]
    fn test_parse_setting_attribute_missing() {
        let field = Field {
            name: "chunk_size".to_string(),
            ty: Type::Named {
                name: "Float".to_string(),
                generics: vec![],
                span: Span::default(),
            },
            attributes: vec![],
            visibility: Visibility::Public,
            default: Some(make_float_expr(100.0)),
            weak: false,
            span: Span::default(),
        };

        let result = parse_setting_attribute(&field).unwrap();
        assert!(result.is_none());
    }
}
