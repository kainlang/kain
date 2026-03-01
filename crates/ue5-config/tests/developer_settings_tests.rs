//! Tests for UDeveloperSettings code generation

use kain_core::ast::{Expr, Field, Struct, Type, Visibility};
use kain_core::span::Span;
use ue5_config::config_ir::{ConfigCategory, ConfigField, ConfigStruct};
use ue5_config::developer_settings_codegen::{generate, generate_developer_settings_header, generate_developer_settings_cpp};

fn create_test_field(name: &str, ty_name: &str, default_value: Option<&str>) -> ConfigField {
    let default = match (ty_name, default_value) {
        ("Float", Some(v)) => Some(Expr::Float(v.parse().unwrap(), Span::default())),
        ("Int", Some(v)) => Some(Expr::Int(v.parse().unwrap(), Span::default())),
        ("Bool", Some(v)) => Some(Expr::Bool(v == "true", Span::default())),
        ("String", Some(v)) => Some(Expr::String(v.to_string(), Span::default())),
        _ => None,
    };
    
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

fn create_test_config(name: &str, category: ConfigCategory, fields: Vec<ConfigField>) -> ConfigStruct {
    ConfigStruct {
        name: name.to_string(),
        category,
        ini_file: None,
        ini_section: None,
        display_name: None,
        fields,
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

#[test]
fn test_generate_header_basic() {
    let config = create_test_config(
        "VoxelSettings",
        ConfigCategory::Game,
        vec![create_test_field("chunk_size", "Float", Some("100.0"))],
    );

    let result = generate_developer_settings_header(&config, "MyPlugin");
    assert!(result.is_ok());

    let header = result.unwrap();
    assert!(header.contains("#pragma once"));
    assert!(header.contains("#include \"CoreMinimal.h\""));
    assert!(header.contains("#include \"Engine/DeveloperSettings.h\""));
    assert!(header.contains("#include \"VoxelSettings.generated.h\""));
    assert!(header.contains("UCLASS(Config=Game, DefaultConfig"));
    assert!(header.contains("class MYPLUGIN_API UVoxelSettings : public UDeveloperSettings"));
    assert!(header.contains("GENERATED_BODY()"));
}

#[test]
fn test_generate_header_with_float_field() {
    let config = create_test_config(
        "TestSettings",
        ConfigCategory::Game,
        vec![create_test_field("test_value", "Float", Some("42.0"))],
    );

    let result = generate_developer_settings_header(&config, "TestPlugin");
    assert!(result.is_ok());

    let header = result.unwrap();
    assert!(header.contains("UPROPERTY(Config, EditAnywhere, Category=\"Settings\""));
    assert!(header.contains("float TestValue;"));
}

#[test]
fn test_generate_header_with_int_field() {
    let config = create_test_config(
        "TestSettings",
        ConfigCategory::Game,
        vec![create_test_field("max_count", "Int", Some("10"))],
    );

    let result = generate_developer_settings_header(&config, "TestPlugin");
    assert!(result.is_ok());

    let header = result.unwrap();
    assert!(header.contains("int32 MaxCount;"));
}

#[test]
fn test_generate_header_with_bool_field() {
    let config = create_test_config(
        "TestSettings",
        ConfigCategory::Game,
        vec![create_test_field("is_enabled", "Bool", Some("true"))],
    );

    let result = generate_developer_settings_header(&config, "TestPlugin");
    assert!(result.is_ok());

    let header = result.unwrap();
    assert!(header.contains("bool IsEnabled;"));
}

#[test]
fn test_generate_header_with_string_field() {
    let config = create_test_config(
        "TestSettings",
        ConfigCategory::Game,
        vec![create_test_field("player_name", "String", Some("Player"))],
    );

    let result = generate_developer_settings_header(&config, "TestPlugin");
    assert!(result.is_ok());

    let header = result.unwrap();
    assert!(header.contains("FString PlayerName;"));
}

#[test]
fn test_generate_header_with_clamp_min() {
    let mut field = create_test_field("chunk_size", "Float", Some("100.0"));
    field.min = Some(10.0);

    let config = create_test_config("VoxelSettings", ConfigCategory::Game, vec![field]);

    let result = generate_developer_settings_header(&config, "MyPlugin");
    assert!(result.is_ok());

    let header = result.unwrap();
    assert!(header.contains("ClampMin=\"10\""));
}

#[test]
fn test_generate_header_with_clamp_max() {
    let mut field = create_test_field("chunk_size", "Float", Some("100.0"));
    field.max = Some(1000.0);

    let config = create_test_config("VoxelSettings", ConfigCategory::Game, vec![field]);

    let result = generate_developer_settings_header(&config, "MyPlugin");
    assert!(result.is_ok());

    let header = result.unwrap();
    assert!(header.contains("ClampMax=\"1000\""));
}

#[test]
fn test_generate_header_with_clamp_min_max() {
    let mut field = create_test_field("chunk_size", "Float", Some("100.0"));
    field.min = Some(10.0);
    field.max = Some(1000.0);

    let config = create_test_config("VoxelSettings", ConfigCategory::Game, vec![field]);

    let result = generate_developer_settings_header(&config, "MyPlugin");
    assert!(result.is_ok());

    let header = result.unwrap();
    assert!(header.contains("ClampMin=\"10\""));
    assert!(header.contains("ClampMax=\"1000\""));
}

#[test]
fn test_generate_header_with_display_name() {
    let mut field = create_test_field("chunk_size", "Float", Some("100.0"));
    field.display_name = Some("Chunk Size".to_string());

    let config = create_test_config("VoxelSettings", ConfigCategory::Game, vec![field]);

    let result = generate_developer_settings_header(&config, "MyPlugin");
    assert!(result.is_ok());

    let header = result.unwrap();
    assert!(header.contains("DisplayName=\"Chunk Size\""));
}

#[test]
fn test_generate_header_with_tooltip() {
    let mut field = create_test_field("chunk_size", "Float", Some("100.0"));
    field.tooltip = Some("Size of voxel chunks in world units".to_string());

    let config = create_test_config("VoxelSettings", ConfigCategory::Game, vec![field]);

    let result = generate_developer_settings_header(&config, "MyPlugin");
    assert!(result.is_ok());

    let header = result.unwrap();
    assert!(header.contains("ToolTip=\"Size of voxel chunks in world units\""));
}

#[test]
fn test_generate_header_singleton_accessor() {
    let config = create_test_config("VoxelSettings", ConfigCategory::Game, vec![]);

    let result = generate_developer_settings_header(&config, "MyPlugin");
    assert!(result.is_ok());

    let header = result.unwrap();
    assert!(header.contains("static const UVoxelSettings* Get();"));
}

#[test]
fn test_generate_header_lifecycle_methods() {
    let config = create_test_config("VoxelSettings", ConfigCategory::Game, vec![]);

    let result = generate_developer_settings_header(&config, "MyPlugin");
    assert!(result.is_ok());

    let header = result.unwrap();
    assert!(header.contains("virtual FName GetContainerName() const override;"));
    assert!(header.contains("virtual void PostInitProperties() override;"));
    assert!(header.contains("#if WITH_EDITOR"));
    assert!(header.contains("virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;"));
    assert!(header.contains("#endif"));
}

#[test]
fn test_generate_header_config_category_game() {
    let config = create_test_config("TestSettings", ConfigCategory::Game, vec![]);

    let result = generate_developer_settings_header(&config, "TestPlugin");
    assert!(result.is_ok());

    let header = result.unwrap();
    assert!(header.contains("Config=Game"));
}

#[test]
fn test_generate_header_config_category_engine() {
    let config = create_test_config("TestSettings", ConfigCategory::Engine, vec![]);

    let result = generate_developer_settings_header(&config, "TestPlugin");
    assert!(result.is_ok());

    let header = result.unwrap();
    assert!(header.contains("Config=Engine"));
}

#[test]
fn test_generate_header_config_category_editor() {
    let config = create_test_config("TestSettings", ConfigCategory::Editor, vec![]);

    let result = generate_developer_settings_header(&config, "TestPlugin");
    assert!(result.is_ok());

    let header = result.unwrap();
    assert!(header.contains("Config=Editor"));
}

#[test]
fn test_generate_header_multiple_fields() {
    let config = create_test_config(
        "VoxelSettings",
        ConfigCategory::Game,
        vec![
            create_test_field("chunk_size", "Float", Some("100.0")),
            create_test_field("max_lod", "Int", Some("4")),
            create_test_field("debug_vis", "Bool", Some("false")),
        ],
    );

    let result = generate_developer_settings_header(&config, "MyPlugin");
    assert!(result.is_ok());

    let header = result.unwrap();
    assert!(header.contains("float ChunkSize;"));
    assert!(header.contains("int32 MaxLod;"));
    assert!(header.contains("bool DebugVis;"));
}

#[test]
fn test_generate_cpp_basic() {
    let config = create_test_config(
        "VoxelSettings",
        ConfigCategory::Game,
        vec![create_test_field("chunk_size", "Float", Some("100.0"))],
    );

    let result = generate_developer_settings_cpp(&config, "MyPlugin");
    assert!(result.is_ok());

    let cpp = result.unwrap();
    assert!(cpp.contains("#include \"VoxelSettings.h\""));
    assert!(cpp.contains("UVoxelSettings::UVoxelSettings()"));
}

#[test]
fn test_generate_cpp_constructor_initialization() {
    let config = create_test_config(
        "VoxelSettings",
        ConfigCategory::Game,
        vec![create_test_field("chunk_size", "Float", Some("100.0"))],
    );

    let result = generate_developer_settings_cpp(&config, "MyPlugin");
    assert!(result.is_ok());

    let cpp = result.unwrap();
    assert!(cpp.contains(": ChunkSize(100.0f)"));
}

#[test]
fn test_generate_cpp_constructor_category_name() {
    let config = create_test_config("VoxelSettings", ConfigCategory::Game, vec![]);

    let result = generate_developer_settings_cpp(&config, "MyPlugin");
    assert!(result.is_ok());

    let cpp = result.unwrap();
    assert!(cpp.contains("CategoryName = TEXT(\"Plugins\")"));
}

#[test]
fn test_generate_cpp_constructor_section_name() {
    let config = create_test_config("VoxelSettings", ConfigCategory::Game, vec![]);

    let result = generate_developer_settings_cpp(&config, "MyPlugin");
    assert!(result.is_ok());

    let cpp = result.unwrap();
    assert!(cpp.contains("SectionName = TEXT(\"Voxel Settings\")"));
}

#[test]
fn test_generate_cpp_singleton_get() {
    let config = create_test_config("VoxelSettings", ConfigCategory::Game, vec![]);

    let result = generate_developer_settings_cpp(&config, "MyPlugin");
    assert!(result.is_ok());

    let cpp = result.unwrap();
    assert!(cpp.contains("const UVoxelSettings* UVoxelSettings::Get()"));
    assert!(cpp.contains("return GetDefault<UVoxelSettings>();"));
}

#[test]
fn test_generate_cpp_get_container_name() {
    let config = create_test_config("VoxelSettings", ConfigCategory::Game, vec![]);

    let result = generate_developer_settings_cpp(&config, "MyPlugin");
    assert!(result.is_ok());

    let cpp = result.unwrap();
    assert!(cpp.contains("FName UVoxelSettings::GetContainerName() const"));
    assert!(cpp.contains("return TEXT(\"Project\");"));
}

#[test]
fn test_generate_cpp_post_init_properties() {
    let config = create_test_config("VoxelSettings", ConfigCategory::Game, vec![]);

    let result = generate_developer_settings_cpp(&config, "MyPlugin");
    assert!(result.is_ok());

    let cpp = result.unwrap();
    assert!(cpp.contains("void UVoxelSettings::PostInitProperties()"));
    assert!(cpp.contains("Super::PostInitProperties();"));
    assert!(cpp.contains("#if WITH_EDITOR"));
    assert!(cpp.contains("if (IsTemplate())"));
    assert!(cpp.contains("ImportConsoleVariableValues();"));
    assert!(cpp.contains("#endif"));
}

#[test]
fn test_generate_cpp_post_edit_change_property() {
    let config = create_test_config("VoxelSettings", ConfigCategory::Game, vec![]);

    let result = generate_developer_settings_cpp(&config, "MyPlugin");
    assert!(result.is_ok());

    let cpp = result.unwrap();
    assert!(cpp.contains("#if WITH_EDITOR"));
    assert!(cpp.contains("void UVoxelSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)"));
    assert!(cpp.contains("Super::PostEditChangeProperty(PropertyChangedEvent);"));
    assert!(cpp.contains("if (PropertyChangedEvent.Property)"));
    assert!(cpp.contains("ExportValuesToConsoleVariables(PropertyChangedEvent.Property);"));
    assert!(cpp.contains("#endif"));
}

#[test]
fn test_generate_cpp_float_default() {
    let config = create_test_config(
        "TestSettings",
        ConfigCategory::Game,
        vec![create_test_field("test_value", "Float", Some("42.5"))],
    );

    let result = generate_developer_settings_cpp(&config, "TestPlugin");
    assert!(result.is_ok());

    let cpp = result.unwrap();
    assert!(cpp.contains("TestValue(42.5f)"));
}

#[test]
fn test_generate_cpp_int_default() {
    let config = create_test_config(
        "TestSettings",
        ConfigCategory::Game,
        vec![create_test_field("max_count", "Int", Some("10"))],
    );

    let result = generate_developer_settings_cpp(&config, "TestPlugin");
    assert!(result.is_ok());

    let cpp = result.unwrap();
    assert!(cpp.contains("MaxCount(10)"));
}

#[test]
fn test_generate_cpp_bool_default_true() {
    let config = create_test_config(
        "TestSettings",
        ConfigCategory::Game,
        vec![create_test_field("is_enabled", "Bool", Some("true"))],
    );

    let result = generate_developer_settings_cpp(&config, "TestPlugin");
    assert!(result.is_ok());

    let cpp = result.unwrap();
    assert!(cpp.contains("IsEnabled(true)"));
}

#[test]
fn test_generate_cpp_bool_default_false() {
    let config = create_test_config(
        "TestSettings",
        ConfigCategory::Game,
        vec![create_test_field("is_enabled", "Bool", Some("false"))],
    );

    let result = generate_developer_settings_cpp(&config, "TestPlugin");
    assert!(result.is_ok());

    let cpp = result.unwrap();
    assert!(cpp.contains("IsEnabled(false)"));
}

#[test]
fn test_generate_cpp_string_default() {
    let config = create_test_config(
        "TestSettings",
        ConfigCategory::Game,
        vec![create_test_field("player_name", "String", Some("Player"))],
    );

    let result = generate_developer_settings_cpp(&config, "TestPlugin");
    assert!(result.is_ok());

    let cpp = result.unwrap();
    assert!(cpp.contains("PlayerName(TEXT(\"Player\"))"));
}

#[test]
fn test_generate_cpp_multiple_fields() {
    let config = create_test_config(
        "VoxelSettings",
        ConfigCategory::Game,
        vec![
            create_test_field("chunk_size", "Float", Some("100.0")),
            create_test_field("max_lod", "Int", Some("4")),
            create_test_field("debug_vis", "Bool", Some("false")),
        ],
    );

    let result = generate_developer_settings_cpp(&config, "MyPlugin");
    assert!(result.is_ok());

    let cpp = result.unwrap();
    assert!(cpp.contains("ChunkSize(100.0f)"));
    assert!(cpp.contains("MaxLod(4)"));
    assert!(cpp.contains("DebugVis(false)"));
}

#[test]
fn test_generate_complete_output() {
    let config = create_test_config(
        "VoxelSettings",
        ConfigCategory::Game,
        vec![create_test_field("chunk_size", "Float", Some("100.0"))],
    );

    let result = generate(&config, "MyPlugin");
    assert!(result.is_ok());

    let output = result.unwrap();
    assert!(!output.header.is_empty());
    assert!(!output.source.is_empty());
    assert!(output.header.contains("UVoxelSettings"));
    assert!(output.source.contains("UVoxelSettings::UVoxelSettings()"));
}

#[test]
fn test_generate_no_fields() {
    let config = create_test_config("EmptySettings", ConfigCategory::Game, vec![]);

    let result = generate(&config, "TestPlugin");
    assert!(result.is_ok());

    let output = result.unwrap();
    assert!(output.header.contains("UEmptySettings"));
    assert!(output.source.contains("UEmptySettings::UEmptySettings()"));
}
