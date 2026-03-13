//! Tests for Blueprint Accessor Code Generation
//!
//! Tests the generation of UFUNCTION(BlueprintCallable) getter and setter methods
//! for config fields marked with @setting(blueprint: true).

use kain_core::ast::{Field, Type, Visibility};
use kain_core::span::Span;
use ue5_config::blueprint_accessor_codegen::*;
use ue5_config::config_ir::{ConfigCategory, ConfigField, ConfigStruct};

fn create_test_config(name: &str) -> ConfigStruct {
    ConfigStruct {
        name: name.to_string(),
        category: ConfigCategory::Game,
        ini_file: None,
        ini_section: None,
        display_name: None,
        fields: vec![],
        original_struct: kain_core::ast::Struct {
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
fn test_getter_declaration_float() {
    let config = create_test_config("VoxelSettings");
    let field = create_test_field("chunk_size", "Float", true, false);

    let result = generate_blueprint_getter_declaration(&config, &field);

    assert!(result.contains("UFUNCTION(BlueprintCallable"));
    assert!(result.contains("Category=\"Voxel Settings\""));
    assert!(result.contains("static float GetChunkSize()"));
}

#[test]
fn test_getter_declaration_int() {
    let config = create_test_config("VoxelSettings");
    let field = create_test_field("max_lod", "Int", true, false);

    let result = generate_blueprint_getter_declaration(&config, &field);

    assert!(result.contains("static int32 GetMaxLod()"));
}

#[test]
fn test_getter_declaration_bool() {
    let config = create_test_config("VoxelSettings");
    let field = create_test_field("debug_vis", "Bool", true, false);

    let result = generate_blueprint_getter_declaration(&config, &field);

    assert!(result.contains("static bool GetDebugVis()"));
}

#[test]
fn test_getter_declaration_string() {
    let config = create_test_config("VoxelSettings");
    let field = create_test_field("plugin_name", "String", true, false);

    let result = generate_blueprint_getter_declaration(&config, &field);

    assert!(result.contains("static FString GetPluginName()"));
}

#[test]
fn test_setter_declaration_writable() {
    let config = create_test_config("VoxelSettings");
    let field = create_test_field("chunk_size", "Float", true, true);

    let result = generate_blueprint_setter_declaration(&config, &field);

    assert!(result.is_some());
    let setter = result.unwrap();
    assert!(setter.contains("UFUNCTION(BlueprintCallable"));
    assert!(setter.contains("Category=\"Voxel Settings\""));
    assert!(setter.contains("static void SetChunkSize(float NewValue)"));
}

#[test]
fn test_setter_declaration_readonly() {
    let config = create_test_config("VoxelSettings");
    let field = create_test_field("chunk_size", "Float", true, false);

    let result = generate_blueprint_setter_declaration(&config, &field);

    assert!(result.is_none());
}

#[test]
fn test_setter_declaration_string_uses_const_ref() {
    let config = create_test_config("VoxelSettings");
    let field = create_test_field("plugin_name", "String", true, true);

    let result = generate_blueprint_setter_declaration(&config, &field);

    assert!(result.is_some());
    let setter = result.unwrap();
    assert!(setter.contains("const FString& NewValue"));
}

#[test]
fn test_getter_implementation() {
    let config = create_test_config("VoxelSettings");
    let field = create_test_field("chunk_size", "Float", true, false);

    let result = generate_blueprint_getter_implementation(&config, &field);

    assert!(result.contains("float UVoxelSettings::GetChunkSize()"));
    assert!(result.contains("return Get()->ChunkSize;"));
}

#[test]
fn test_setter_implementation_writable() {
    let config = create_test_config("VoxelSettings");
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
fn test_setter_implementation_readonly() {
    let config = create_test_config("VoxelSettings");
    let field = create_test_field("chunk_size", "Float", true, false);

    let result = generate_blueprint_setter_implementation(&config, &field);

    assert!(result.is_none());
}

#[test]
fn test_category_naming_matches_struct_name() {
    let config = create_test_config("NarrativeSettings");
    let field = create_test_field("dialogue_speed", "Float", true, false);

    let result = generate_blueprint_getter_declaration(&config, &field);

    assert!(result.contains("Category=\"Narrative Settings\""));
}

#[test]
fn test_multiple_fields_header() {
    let mut config = create_test_config("VoxelSettings");
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
fn test_multiple_fields_cpp() {
    let mut config = create_test_config("VoxelSettings");
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
fn test_no_blueprint_fields_generates_nothing() {
    let mut config = create_test_config("VoxelSettings");
    config.fields = vec![
        create_test_field("chunk_size", "Float", false, false),
        create_test_field("max_lod", "Int", false, false),
    ];

    let header = generate_blueprint_accessors_header(&config);
    let cpp = generate_blueprint_accessors_cpp(&config);

    assert_eq!(header.len(), 0);
    assert_eq!(cpp.len(), 0);
}

#[test]
fn test_ufunction_specifiers_correct() {
    let config = create_test_config("VoxelSettings");
    let field = create_test_field("chunk_size", "Float", true, false);

    let result = generate_blueprint_getter_declaration(&config, &field);

    // Should have UFUNCTION with BlueprintCallable and Category
    assert!(result.contains("UFUNCTION(BlueprintCallable, Category="));
}

#[test]
fn test_getter_has_doc_comment() {
    let config = create_test_config("VoxelSettings");
    let field = create_test_field("chunk_size", "Float", true, false);

    let result = generate_blueprint_getter_declaration(&config, &field);

    assert!(result.contains("/** Get ChunkSize */"));
}

#[test]
fn test_setter_has_doc_comment() {
    let config = create_test_config("VoxelSettings");
    let field = create_test_field("chunk_size", "Float", true, true);

    let result = generate_blueprint_setter_declaration(&config, &field).unwrap();

    assert!(result.contains("/** Set ChunkSize */"));
}

#[test]
fn test_all_types_supported() {
    let config = create_test_config("TestSettings");

    let float_field = create_test_field("float_val", "Float", true, false);
    let int_field = create_test_field("int_val", "Int", true, false);
    let bool_field = create_test_field("bool_val", "Bool", true, false);
    let string_field = create_test_field("string_val", "String", true, false);

    let float_getter = generate_blueprint_getter_declaration(&config, &float_field);
    let int_getter = generate_blueprint_getter_declaration(&config, &int_field);
    let bool_getter = generate_blueprint_getter_declaration(&config, &bool_field);
    let string_getter = generate_blueprint_getter_declaration(&config, &string_field);

    assert!(float_getter.contains("static float GetFloatVal()"));
    assert!(int_getter.contains("static int32 GetIntVal()"));
    assert!(bool_getter.contains("static bool GetBoolVal()"));
    assert!(string_getter.contains("static FString GetStringVal()"));
}
