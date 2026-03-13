//! Tests for .ini file generation

use kain_core::ast::{Expr, Field, Struct, Type, Visibility};
use kain_core::span::Span;
use ue5_config::config_ir::{ConfigCategory, ConfigField, ConfigStruct};
use ue5_config::ini_file_generator::{generate_ini_file, generate_ini_section};

fn make_test_config() -> ConfigStruct {
    ConfigStruct {
        name: "VoxelSettings".to_string(),
        category: ConfigCategory::Game,
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

fn make_test_field(name: &str, ty_name: &str, default: Option<Expr>) -> ConfigField {
    ConfigField {
        name: name.to_string(),
        ty: Type::Named {
            name: ty_name.to_string(),
            generics: vec![],
            span: Span::default(),
        },
        default,
        display_name: None,
        tooltip: None,
        cvar: None,
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
fn test_ini_section_basic() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "chunk_size",
        "Float",
        Some(Expr::Float(100.0, Span::default())),
    ));

    let section = generate_ini_section(&config, "MyPlugin").unwrap();
    assert!(section.contains("[/Script/MyPlugin.UVoxelSettings]"));
    assert!(section.contains("ChunkSize=100"));
}

#[test]
fn test_ini_section_multiple_fields() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "chunk_size",
        "Float",
        Some(Expr::Float(100.0, Span::default())),
    ));
    config.fields.push(make_test_field(
        "max_lod",
        "Int",
        Some(Expr::Int(4, Span::default())),
    ));
    config.fields.push(make_test_field(
        "debug_vis",
        "Bool",
        Some(Expr::Bool(false, Span::default())),
    ));

    let section = generate_ini_section(&config, "MyPlugin").unwrap();
    assert!(section.contains("[/Script/MyPlugin.UVoxelSettings]"));
    assert!(section.contains("ChunkSize=100"));
    assert!(section.contains("MaxLod=4"));
    assert!(section.contains("DebugVis=False")); // Capital F!
}

#[test]
fn test_ini_bool_true_format() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "enable_feature",
        "Bool",
        Some(Expr::Bool(true, Span::default())),
    ));

    let section = generate_ini_section(&config, "MyPlugin").unwrap();
    // CRITICAL: Must be capital T, not lowercase
    assert!(section.contains("EnableFeature=True"));
    assert!(!section.contains("EnableFeature=true"));
}

#[test]
fn test_ini_bool_false_format() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "enable_feature",
        "Bool",
        Some(Expr::Bool(false, Span::default())),
    ));

    let section = generate_ini_section(&config, "MyPlugin").unwrap();
    // CRITICAL: Must be capital F, not lowercase
    assert!(section.contains("EnableFeature=False"));
    assert!(!section.contains("EnableFeature=false"));
}

#[test]
fn test_ini_float_format() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "scale",
        "Float",
        Some(Expr::Float(1.5, Span::default())),
    ));

    let section = generate_ini_section(&config, "MyPlugin").unwrap();
    assert!(section.contains("Scale=1.5"));
}

#[test]
fn test_ini_int_format() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "count",
        "Int",
        Some(Expr::Int(42, Span::default())),
    ));

    let section = generate_ini_section(&config, "MyPlugin").unwrap();
    assert!(section.contains("Count=42"));
}

#[test]
fn test_ini_string_format() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "server_name",
        "String",
        Some(Expr::String("MyServer".to_string(), Span::default())),
    ));

    let section = generate_ini_section(&config, "MyPlugin").unwrap();
    assert!(section.contains("ServerName=MyServer"));
}

#[test]
fn test_ini_custom_section_name() {
    let mut config = make_test_config();
    config.ini_section = Some("/Script/CustomPlugin.CustomSettings".to_string());
    config.fields.push(make_test_field(
        "test_field",
        "Int",
        Some(Expr::Int(42, Span::default())),
    ));

    let section = generate_ini_section(&config, "MyPlugin").unwrap();
    assert!(section.contains("[/Script/CustomPlugin.CustomSettings]"));
    assert!(section.contains("TestField=42"));
}

#[test]
fn test_ini_file_generation() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "chunk_size",
        "Float",
        Some(Expr::Float(100.0, Span::default())),
    ));

    let content = generate_ini_file(&config, "MyPlugin").unwrap();
    assert!(content.contains("; Generated by KAIN compiler"));
    assert!(content.contains("[/Script/MyPlugin.UVoxelSettings]"));
    assert!(content.contains("ChunkSize=100"));
}

#[test]
fn test_ini_field_naming_convention() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "my_long_field_name",
        "Int",
        Some(Expr::Int(1, Span::default())),
    ));

    let section = generate_ini_section(&config, "MyPlugin").unwrap();
    // Should convert snake_case to PascalCase
    assert!(section.contains("MyLongFieldName=1"));
}

#[test]
fn test_ini_default_values() {
    let mut config = make_test_config();

    // Field with no default value - should use type-appropriate default
    config
        .fields
        .push(make_test_field("test_float", "Float", None));
    config.fields.push(make_test_field("test_int", "Int", None));
    config
        .fields
        .push(make_test_field("test_bool", "Bool", None));

    let section = generate_ini_section(&config, "MyPlugin").unwrap();
    assert!(section.contains("TestFloat=0.0"));
    assert!(section.contains("TestInt=0"));
    assert!(section.contains("TestBool=False")); // Capital F!
}

#[test]
fn test_ini_section_format() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "field1",
        "Int",
        Some(Expr::Int(1, Span::default())),
    ));
    config.fields.push(make_test_field(
        "field2",
        "Int",
        Some(Expr::Int(2, Span::default())),
    ));

    let section = generate_ini_section(&config, "MyPlugin").unwrap();

    // Check format: section header followed by fields
    let lines: Vec<&str> = section.lines().collect();
    assert_eq!(lines[0], "[/Script/MyPlugin.UVoxelSettings]");
    assert!(lines[1].starts_with("Field1="));
    assert!(lines[2].starts_with("Field2="));
}

#[test]
fn test_ini_different_categories() {
    // Test Game category
    let mut config = make_test_config();
    config.category = ConfigCategory::Game;
    config.fields.push(make_test_field(
        "test",
        "Int",
        Some(Expr::Int(1, Span::default())),
    ));

    let section = generate_ini_section(&config, "MyPlugin").unwrap();
    assert!(section.contains("[/Script/MyPlugin.UVoxelSettings]"));

    // Test Engine category
    let mut config = make_test_config();
    config.category = ConfigCategory::Engine;
    config.fields.push(make_test_field(
        "test",
        "Int",
        Some(Expr::Int(1, Span::default())),
    ));

    let section = generate_ini_section(&config, "MyPlugin").unwrap();
    assert!(section.contains("[/Script/MyPlugin.UVoxelSettings]"));
}

#[test]
fn test_ini_empty_fields() {
    let config = make_test_config();

    let section = generate_ini_section(&config, "MyPlugin").unwrap();
    // Should still have section header
    assert!(section.contains("[/Script/MyPlugin.UVoxelSettings]"));
}
