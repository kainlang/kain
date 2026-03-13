//! Tests for parser.rs - Attribute parsing functionality

use kain_core::ast::{Attribute, BinaryOp, Expr, Field, Struct, Type, Visibility};
use kain_core::span::Span;
use ue5_config::config_ir::ConfigCategory;
use ue5_config::parser::{parse_config_attribute, parse_setting_attribute};

// Helper functions

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

// Config attribute parsing tests

#[test]
fn test_parse_config_attribute_game_category() {
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
}

#[test]
fn test_parse_config_attribute_engine_category() {
    let struct_def = Struct {
        name: "EngineSettings".to_string(),
        generics: vec![],
        fields: vec![],
        methods: vec![],
        attributes: vec![Attribute {
            name: "config".to_string(),
            args: vec![make_named_arg("category", make_string_expr("Engine"))],
            span: Span::default(),
        }],
        visibility: Visibility::Public,
        span: Span::default(),
    };

    let result = parse_config_attribute(&struct_def).unwrap();
    assert!(result.is_some());

    let config = result.unwrap();
    assert_eq!(config.category, ConfigCategory::Engine);
}

#[test]
fn test_parse_config_attribute_editor_category() {
    let struct_def = Struct {
        name: "EditorSettings".to_string(),
        generics: vec![],
        fields: vec![],
        methods: vec![],
        attributes: vec![Attribute {
            name: "config".to_string(),
            args: vec![make_named_arg("category", make_string_expr("Editor"))],
            span: Span::default(),
        }],
        visibility: Visibility::Public,
        span: Span::default(),
    };

    let result = parse_config_attribute(&struct_def).unwrap();
    assert!(result.is_some());

    let config = result.unwrap();
    assert_eq!(config.category, ConfigCategory::Editor);
}

#[test]
fn test_parse_config_attribute_all_params() {
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
fn test_parse_config_attribute_missing_category() {
    let struct_def = Struct {
        name: "VoxelSettings".to_string(),
        generics: vec![],
        fields: vec![],
        methods: vec![],
        attributes: vec![Attribute {
            name: "config".to_string(),
            args: vec![],
            span: Span::default(),
        }],
        visibility: Visibility::Public,
        span: Span::default(),
    };

    let result = parse_config_attribute(&struct_def);
    assert!(result.is_err());
}

#[test]
fn test_parse_config_attribute_invalid_category() {
    let struct_def = Struct {
        name: "VoxelSettings".to_string(),
        generics: vec![],
        fields: vec![],
        methods: vec![],
        attributes: vec![Attribute {
            name: "config".to_string(),
            args: vec![make_named_arg(
                "category",
                make_string_expr("InvalidCategory"),
            )],
            span: Span::default(),
        }],
        visibility: Visibility::Public,
        span: Span::default(),
    };

    let result = parse_config_attribute(&struct_def);
    assert!(result.is_err());
}

#[test]
fn test_parse_config_attribute_no_attribute() {
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

// Setting attribute parsing tests

#[test]
fn test_parse_setting_attribute_minimal() {
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
fn test_parse_setting_attribute_all_params() {
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
fn test_parse_setting_attribute_no_attribute() {
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

#[test]
fn test_parse_setting_attribute_blueprint_only() {
    let field = Field {
        name: "chunk_size".to_string(),
        ty: Type::Named {
            name: "Float".to_string(),
            generics: vec![],
            span: Span::default(),
        },
        attributes: vec![Attribute {
            name: "setting".to_string(),
            args: vec![make_named_arg("blueprint", make_bool_expr(true))],
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
    assert_eq!(setting.blueprint, true);
    assert_eq!(setting.writable, false); // Default
}

#[test]
fn test_parse_setting_attribute_cvar_only() {
    let field = Field {
        name: "chunk_size".to_string(),
        ty: Type::Named {
            name: "Float".to_string(),
            generics: vec![],
            span: Span::default(),
        },
        attributes: vec![Attribute {
            name: "setting".to_string(),
            args: vec![make_named_arg("cvar", make_string_expr("voxel.ChunkSize"))],
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
    assert_eq!(setting.cvar, Some("voxel.ChunkSize".to_string()));
}

#[test]
fn test_parse_setting_attribute_min_max() {
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
                make_named_arg("min", make_float_expr(10.0)),
                make_named_arg("max", make_float_expr(1000.0)),
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
    assert_eq!(setting.min, Some(10.0));
    assert_eq!(setting.max, Some(1000.0));
}
