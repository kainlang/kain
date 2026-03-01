//! Tests for CVar code generation

use kain_core::ast::{Expr, Field, Struct, Type, Visibility};
use kain_core::span::Span;
use ue5_config::config_ir::{ConfigCategory, ConfigField, ConfigStruct};
use ue5_config::cvar_codegen::{
    generate_cvar_callback_declarations, generate_cvar_callbacks, generate_cvar_declarations,
};

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
fn test_cvar_declaration_float() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "chunk_size",
        "Float",
        Some(Expr::Float(100.0, Span::default())),
        Some("voxel.ChunkSize".to_string()),
    ));

    let declarations = generate_cvar_declarations(&config, "MyPlugin").unwrap();
    assert_eq!(declarations.len(), 1);

    let decl = &declarations[0];
    assert!(decl.contains("static TAutoConsoleVariable<float>"));
    assert!(decl.contains("CVarChunkSize"));
    assert!(decl.contains("TEXT(\"voxel.ChunkSize\")"));
    assert!(decl.contains("100.0f"));
    assert!(decl.contains("TEXT(\"Test tooltip\")"));
    assert!(decl.contains("ECVF_Default"));
}

#[test]
fn test_cvar_declaration_int() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "max_lod",
        "Int",
        Some(Expr::Int(4, Span::default())),
        Some("voxel.MaxLOD".to_string()),
    ));

    let declarations = generate_cvar_declarations(&config, "MyPlugin").unwrap();
    assert_eq!(declarations.len(), 1);

    let decl = &declarations[0];
    assert!(decl.contains("static TAutoConsoleVariable<int32>"));
    assert!(decl.contains("CVarMaxLod"));
    assert!(decl.contains("TEXT(\"voxel.MaxLOD\")"));
    assert!(decl.contains("4"));
    assert!(decl.contains("ECVF_Default"));
}

#[test]
fn test_cvar_declaration_bool() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "debug_vis",
        "Bool",
        Some(Expr::Bool(true, Span::default())),
        Some("voxel.DebugVis".to_string()),
    ));

    let declarations = generate_cvar_declarations(&config, "MyPlugin").unwrap();
    assert_eq!(declarations.len(), 1);

    let decl = &declarations[0];
    assert!(decl.contains("static TAutoConsoleVariable<bool>"));
    assert!(decl.contains("CVarDebugVis"));
    assert!(decl.contains("TEXT(\"voxel.DebugVis\")"));
    assert!(decl.contains("true"));
    assert!(decl.contains("ECVF_Default"));
}

#[test]
fn test_cvar_declaration_string() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "server_name",
        "String",
        Some(Expr::String("MyServer".to_string(), Span::default())),
        Some("game.ServerName".to_string()),
    ));

    let declarations = generate_cvar_declarations(&config, "MyPlugin").unwrap();
    assert_eq!(declarations.len(), 1);

    let decl = &declarations[0];
    assert!(decl.contains("static TAutoConsoleVariable<FString>"));
    assert!(decl.contains("CVarServerName"));
    assert!(decl.contains("TEXT(\"game.ServerName\")"));
    assert!(decl.contains("TEXT(\"MyServer\")"));
    assert!(decl.contains("ECVF_Default"));
}

#[test]
fn test_cvar_declaration_multiple_fields() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "chunk_size",
        "Float",
        Some(Expr::Float(100.0, Span::default())),
        Some("voxel.ChunkSize".to_string()),
    ));
    config.fields.push(make_test_field(
        "max_lod",
        "Int",
        Some(Expr::Int(4, Span::default())),
        Some("voxel.MaxLOD".to_string()),
    ));

    let declarations = generate_cvar_declarations(&config, "MyPlugin").unwrap();
    assert_eq!(declarations.len(), 2);
}

#[test]
fn test_cvar_declaration_no_cvar() {
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
fn test_cvar_callback_generation() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "chunk_size",
        "Float",
        Some(Expr::Float(100.0, Span::default())),
        Some("voxel.ChunkSize".to_string()),
    ));

    let callbacks = generate_cvar_callbacks(&config, "MyPlugin").unwrap();
    assert_eq!(callbacks.len(), 1);

    let callback = &callbacks[0];
    assert!(callback.contains("void UVoxelSettings::OnChunkSizeChanged()"));
    assert!(callback.contains("ChunkSize = CVarChunkSize.GetValueOnGameThread()"));
}

#[test]
fn test_cvar_callback_declarations() {
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
fn test_cvar_naming_convention() {
    let mut config = make_test_config();
    config.fields.push(make_test_field(
        "my_long_field_name",
        "Float",
        Some(Expr::Float(1.0, Span::default())),
        Some("plugin.MyLongFieldName".to_string()),
    ));

    let declarations = generate_cvar_declarations(&config, "MyPlugin").unwrap();
    assert_eq!(declarations.len(), 1);

    let decl = &declarations[0];
    // Should use PascalCase for CVar variable name
    assert!(decl.contains("CVarMyLongFieldName"));
    // Should preserve the CVar name as specified
    assert!(decl.contains("TEXT(\"plugin.MyLongFieldName\")"));
}

#[test]
fn test_cvar_default_values() {
    let mut config = make_test_config();

    // Field with no default value
    config.fields.push(make_test_field(
        "test_field",
        "Float",
        None,
        Some("test.Field".to_string()),
    ));

    let declarations = generate_cvar_declarations(&config, "MyPlugin").unwrap();
    assert_eq!(declarations.len(), 1);

    let decl = &declarations[0];
    // Should use type-appropriate default
    assert!(decl.contains("0.0f"));
}
