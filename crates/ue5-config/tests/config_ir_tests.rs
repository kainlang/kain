//! Tests for config_ir.rs - IR type functionality

use ue5_config::config_ir::{ConfigCategory, ConfigField, ConfigStruct};
use kain_core::ast::{Expr, Field, Struct, Type, Visibility};
use kain_core::span::Span;

#[test]
fn test_config_category_all_variants() {
    let categories = vec![
        ConfigCategory::Game,
        ConfigCategory::Engine,
        ConfigCategory::Editor,
        ConfigCategory::EditorPerProjectUserSettings,
    ];

    for category in categories {
        assert!(!category.uclass_specifier().is_empty());
        assert!(!category.default_ini_file().is_empty());
    }
}

#[test]
fn test_config_category_round_trip() {
    let test_cases = vec![
        ("game", ConfigCategory::Game),
        ("Game", ConfigCategory::Game),
        ("GAME", ConfigCategory::Game),
        ("engine", ConfigCategory::Engine),
        ("editor", ConfigCategory::Editor),
        ("editorperprojectusersettings", ConfigCategory::EditorPerProjectUserSettings),
    ];

    for (input, expected) in test_cases {
        let parsed = ConfigCategory::from_str(input);
        assert_eq!(parsed, Some(expected));
    }
}

#[test]
fn test_config_struct_ue5_class_name_adds_prefix() {
    let config = make_test_config("VoxelSettings");
    assert_eq!(config.ue5_class_name(), "UVoxelSettings");
}

#[test]
fn test_config_struct_ue5_class_name_no_double_prefix() {
    let config = make_test_config("UVoxelSettings");
    assert_eq!(config.ue5_class_name(), "UVoxelSettings");
}

#[test]
fn test_config_struct_get_display_name_default() {
    let config = make_test_config("VoxelSettings");
    assert_eq!(config.get_display_name(), "Voxel Settings");
}

#[test]
fn test_config_struct_get_display_name_custom() {
    let mut config = make_test_config("VoxelSettings");
    config.display_name = Some("Custom Display Name".to_string());
    assert_eq!(config.get_display_name(), "Custom Display Name");
}

#[test]
fn test_config_struct_get_ini_file_default() {
    let config = make_test_config("VoxelSettings");
    assert_eq!(config.get_ini_file(), "DefaultGame.ini");
}

#[test]
fn test_config_struct_get_ini_file_custom() {
    let mut config = make_test_config("VoxelSettings");
    config.ini_file = Some("CustomGame.ini".to_string());
    assert_eq!(config.get_ini_file(), "CustomGame.ini");
}

#[test]
fn test_config_struct_get_ini_section_default() {
    let config = make_test_config("VoxelSettings");
    assert_eq!(config.get_ini_section("MyPlugin"), "/Script/MyPlugin.UVoxelSettings");
}

#[test]
fn test_config_struct_get_ini_section_custom() {
    let mut config = make_test_config("VoxelSettings");
    config.ini_section = Some("CustomSection".to_string());
    assert_eq!(config.get_ini_section("MyPlugin"), "CustomSection");
}

#[test]
fn test_config_field_ue5_property_name_snake_to_pascal() {
    let field = make_test_field("chunk_size");
    assert_eq!(field.ue5_property_name(), "ChunkSize");
}

#[test]
fn test_config_field_ue5_property_name_already_pascal() {
    let field = make_test_field("ChunkSize");
    assert_eq!(field.ue5_property_name(), "ChunkSize");
}

#[test]
fn test_config_field_get_display_name_default() {
    let field = make_test_field("chunk_size");
    assert_eq!(field.get_display_name(), "Chunk Size");
}

#[test]
fn test_config_field_get_display_name_custom() {
    let mut field = make_test_field("chunk_size");
    field.display_name = Some("Custom Display".to_string());
    assert_eq!(field.get_display_name(), "Custom Display");
}

#[test]
fn test_config_field_get_cvar_name_explicit() {
    let mut field = make_test_field("chunk_size");
    field.cvar = Some("voxel.ChunkSize".to_string());
    assert_eq!(field.get_cvar_name("MyPlugin"), Some("voxel.ChunkSize".to_string()));
}

#[test]
fn test_config_field_get_cvar_name_default() {
    let mut field = make_test_field("chunk_size");
    field.cvar = Some("".to_string()); // Empty string means generate default
    assert_eq!(field.get_cvar_name("MyPlugin"), Some("MyPlugin.ChunkSize".to_string()));
}

#[test]
fn test_config_field_has_cvar_true() {
    let mut field = make_test_field("chunk_size");
    field.cvar = Some("voxel.ChunkSize".to_string());
    assert!(field.has_cvar());
}

#[test]
fn test_config_field_has_cvar_false() {
    let field = make_test_field("chunk_size");
    assert!(!field.has_cvar());
}

// Helper functions

fn make_test_config(name: &str) -> ConfigStruct {
    ConfigStruct {
        name: name.to_string(),
        category: ConfigCategory::Game,
        ini_file: None,
        ini_section: None,
        display_name: None,
        fields: vec![],
        original_struct: Struct {
            name: name.to_string(),
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

fn make_test_field(name: &str) -> ConfigField {
    ConfigField {
        name: name.to_string(),
        ty: Type::Named {
            name: "Float".to_string(),
            generics: vec![],
            span: Span::default(),
        },
        default: Some(Expr::Float(100.0, Span::default())),
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
                name: "Float".to_string(),
                generics: vec![],
                span: Span::default(),
            },
            attributes: vec![],
            visibility: Visibility::Public,
            default: Some(Expr::Float(100.0, Span::default())),
            weak: false,
            span: Span::default(),
        },
        span: Span::default(),
    }
}
