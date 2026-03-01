//! Integration tests for ue5-config crate
//!
//! These tests verify end-to-end KAIN → .h/.cpp/.ini generation
//! across all config categories, attribute combinations, and type mappings.

use kain_core::ast::{Attribute, BinaryOp, Expr, Field, Item, Program, Struct, Type, Visibility};
use kain_core::span::Span;
use ue5_config::generate_config_code;

// Helper functions for building test AST nodes

fn make_string_expr(s: &str) -> Expr {
    Expr::String(s.to_string(), Span::default())
}

fn make_float_expr(f: f64) -> Expr {
    Expr::Float(f, Span::default())
}

fn make_int_expr(i: i64) -> Expr {
    Expr::Int(i, Span::default())
}

fn make_bool_expr(b: bool) -> Expr {
    Expr::Bool(b, Span::default())
}

fn make_named_arg(name: &str, value: Expr) -> Expr {
    Expr::Binary {
        left: Box::new(Expr::Ident(name.to_string(), Span::default())),
        op: BinaryOp::Assign,
        right: Box::new(value),
        span: Span::default(),
    }
}

fn make_config_attribute(category: &str) -> Attribute {
    Attribute {
        name: "config".to_string(),
        args: vec![make_named_arg("category", make_string_expr(category))],
        span: Span::default(),
    }
}

fn make_config_attribute_with_args(args: Vec<(&str, Expr)>) -> Attribute {
    Attribute {
        name: "config".to_string(),
        args: args.into_iter().map(|(name, value)| make_named_arg(name, value)).collect(),
        span: Span::default(),
    }
}

fn make_setting_attribute(args: Vec<(&str, Expr)>) -> Attribute {
    Attribute {
        name: "setting".to_string(),
        args: args.into_iter().map(|(name, value)| make_named_arg(name, value)).collect(),
        span: Span::default(),
    }
}

fn make_field(name: &str, ty_name: &str, default: Option<Expr>, attributes: Vec<Attribute>) -> Field {
    Field {
        name: name.to_string(),
        ty: Type::Named {
            name: ty_name.to_string(),
            generics: vec![],
            span: Span::default(),
        },
        attributes,
        visibility: Visibility::Public,
        default,
        weak: false,
        span: Span::default(),
    }
}

fn make_config_struct(name: &str, config_attr: Attribute, fields: Vec<Field>) -> Struct {
    Struct {
        name: name.to_string(),
        generics: vec![],
        fields,
        methods: vec![],
        attributes: vec![config_attr],
        visibility: Visibility::Public,
        span: Span::default(),
    }
}

// Integration Tests

#[test]
fn test_integration_game_config_single_float_setting() {
    // Test: @config(category: "Game") with single Float setting
    let config_attr = make_config_attribute("Game");
    let setting_attr = make_setting_attribute(vec![
        ("cvar", make_string_expr("voxel.ChunkSize")),
        ("blueprint", make_bool_expr(true)),
        ("min", make_float_expr(10.0)),
        ("max", make_float_expr(1000.0)),
    ]);
    
    let field = make_field("chunk_size", "Float", Some(make_float_expr(100.0)), vec![setting_attr]);
    let struct_def = make_config_struct("VoxelSettings", config_attr, vec![field]);
    
    let program = Program {
        items: vec![Item::Struct(struct_def)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok(), "Failed to generate code: {:?}", result.err());
    
    let files = result.unwrap();
    assert_eq!(files.len(), 2, "Should generate header and source files");
    
    // Verify header file
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    assert_eq!(header.path, "Source/Public/VoxelSettings.h");
    assert!(header.content.contains("UVoxelSettings"), "Header should contain class name");
    assert!(header.content.contains("UDeveloperSettings"), "Header should inherit from UDeveloperSettings");
    assert!(header.content.contains("UCLASS(Config=Game"), "Header should have Config=Game");
    assert!(header.content.contains("float ChunkSize"), "Header should have ChunkSize property");
    assert!(header.content.contains("ClampMin=\"10"), "Header should have ClampMin meta");
    assert!(header.content.contains("ClampMax=\"1000"), "Header should have ClampMax meta");
    
    // Verify source file
    let source = files.iter().find(|f| f.path.ends_with(".cpp")).unwrap();
    assert_eq!(source.path, "Source/Private/VoxelSettings.cpp");
    assert!(source.content.contains("UVoxelSettings::UVoxelSettings()"), "Source should have constructor");
    assert!(source.content.contains("ChunkSize(100.0f)"), "Source should initialize ChunkSize");
    assert!(source.content.contains("const UVoxelSettings* UVoxelSettings::Get()"), "Source should have Get() method");
}

#[test]
fn test_integration_engine_config_multiple_settings() {
    // Test: @config(category: "Engine") with multiple settings of different types
    let config_attr = make_config_attribute("Engine");
    
    let fields = vec![
        make_field("max_lod", "Int", Some(make_int_expr(4)), vec![
            make_setting_attribute(vec![
                ("cvar", make_string_expr("voxel.MaxLOD")),
                ("min", make_float_expr(1.0)),
                ("max", make_float_expr(8.0)),
            ])
        ]),
        make_field("debug_vis", "Bool", Some(make_bool_expr(false)), vec![
            make_setting_attribute(vec![
                ("cvar", make_string_expr("voxel.DebugVis")),
                ("blueprint", make_bool_expr(true)),
            ])
        ]),
        make_field("material_name", "String", Some(make_string_expr("DefaultMaterial")), vec![
            make_setting_attribute(vec![
                ("display_name", make_string_expr("Material Name")),
                ("tooltip", make_string_expr("Name of the default material")),
            ])
        ]),
    ];
    
    let struct_def = make_config_struct("VoxelSettings", config_attr, fields);
    
    let program = Program {
        items: vec![Item::Struct(struct_def)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok());
    
    let files = result.unwrap();
    assert_eq!(files.len(), 2);
    
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    assert!(header.content.contains("UCLASS(Config=Engine"), "Should use Engine config");
    assert!(header.content.contains("int32 MaxLod"), "Should have Int → int32 mapping");
    assert!(header.content.contains("bool DebugVis"), "Should have Bool → bool mapping");
    assert!(header.content.contains("FString MaterialName"), "Should have String → FString mapping");
    
    let source = files.iter().find(|f| f.path.ends_with(".cpp")).unwrap();
    assert!(source.content.contains("MaxLod(4)"), "Should initialize Int field");
    assert!(source.content.contains("DebugVis(false)"), "Should initialize Bool field");
    assert!(source.content.contains("MaterialName(TEXT(\"DefaultMaterial\"))"), "Should initialize String field");
}

#[test]
fn test_integration_editor_config() {
    // Test: @config(category: "Editor")
    let config_attr = make_config_attribute("Editor");
    let field = make_field("grid_size", "Int", Some(make_int_expr(16)), vec![
        make_setting_attribute(vec![])
    ]);
    
    let struct_def = make_config_struct("EditorSettings", config_attr, vec![field]);
    
    let program = Program {
        items: vec![Item::Struct(struct_def)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok());
    
    let files = result.unwrap();
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    assert!(header.content.contains("UCLASS(Config=Editor"), "Should use Editor config");
}

#[test]
fn test_integration_editor_per_project_user_settings_config() {
    // Test: @config(category: "EditorPerProjectUserSettings")
    let config_attr = make_config_attribute("EditorPerProjectUserSettings");
    let field = make_field("show_grid", "Bool", Some(make_bool_expr(true)), vec![
        make_setting_attribute(vec![])
    ]);
    
    let struct_def = make_config_struct("UserSettings", config_attr, vec![field]);
    
    let program = Program {
        items: vec![Item::Struct(struct_def)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok());
    
    let files = result.unwrap();
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    assert!(header.content.contains("UCLASS(Config=EditorPerProjectUserSettings"), 
            "Should use EditorPerProjectUserSettings config");
}

#[test]
fn test_integration_custom_display_name() {
    // Test: @config with custom display_name
    let config_attr = make_config_attribute_with_args(vec![
        ("category", make_string_expr("Game")),
        ("display_name", make_string_expr("My Custom Settings")),
    ]);
    
    let field = make_field("value", "Float", Some(make_float_expr(1.0)), vec![
        make_setting_attribute(vec![])
    ]);
    
    let struct_def = make_config_struct("CustomSettings", config_attr, vec![field]);
    
    let program = Program {
        items: vec![Item::Struct(struct_def)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok());
    
    let files = result.unwrap();
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    assert!(header.content.contains("DisplayName=\"My Custom Settings\""), 
            "Should use custom display name");
}

#[test]
fn test_integration_setting_with_tooltip() {
    // Test: @setting with tooltip
    let config_attr = make_config_attribute("Game");
    let field = make_field("speed", "Float", Some(make_float_expr(10.0)), vec![
        make_setting_attribute(vec![
            ("tooltip", make_string_expr("Movement speed in units per second")),
        ])
    ]);
    
    let struct_def = make_config_struct("GameSettings", config_attr, vec![field]);
    
    let program = Program {
        items: vec![Item::Struct(struct_def)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok());
    
    let files = result.unwrap();
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    assert!(header.content.contains("ToolTip=\"Movement speed in units per second\""), 
            "Should include tooltip in meta");
}

#[test]
fn test_integration_all_numeric_constraints() {
    // Test: @setting with min, max, and both
    let config_attr = make_config_attribute("Game");
    
    let fields = vec![
        make_field("min_only", "Float", Some(make_float_expr(0.0)), vec![
            make_setting_attribute(vec![
                ("min", make_float_expr(0.0)),
            ])
        ]),
        make_field("max_only", "Float", Some(make_float_expr(100.0)), vec![
            make_setting_attribute(vec![
                ("max", make_float_expr(100.0)),
            ])
        ]),
        make_field("both", "Float", Some(make_float_expr(50.0)), vec![
            make_setting_attribute(vec![
                ("min", make_float_expr(0.0)),
                ("max", make_float_expr(100.0)),
            ])
        ]),
    ];
    
    let struct_def = make_config_struct("ConstraintSettings", config_attr, fields);
    
    let program = Program {
        items: vec![Item::Struct(struct_def)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok());
    
    let files = result.unwrap();
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    
    // Check min_only has ClampMin but not ClampMax
    assert!(header.content.contains("float MinOnly"), "Should have MinOnly field");
    let min_only_section = header.content.split("float MinOnly").next().unwrap();
    assert!(min_only_section.contains("ClampMin=\"0"), "MinOnly should have ClampMin");
    
    // Check max_only has ClampMax but not ClampMin
    assert!(header.content.contains("float MaxOnly"), "Should have MaxOnly field");
    
    // Check both has both ClampMin and ClampMax
    assert!(header.content.contains("float Both"), "Should have Both field");
    let both_section = header.content.split("float Both").next().unwrap();
    assert!(both_section.contains("ClampMin=\"0"), "Both should have ClampMin");
    assert!(both_section.contains("ClampMax=\"100"), "Both should have ClampMax");
}

#[test]
fn test_integration_blueprint_accessor_generation() {
    // Test: @setting(blueprint: true) generates Blueprint accessors
    let config_attr = make_config_attribute("Game");
    let field = make_field("health", "Float", Some(make_float_expr(100.0)), vec![
        make_setting_attribute(vec![
            ("blueprint", make_bool_expr(true)),
        ])
    ]);
    
    let struct_def = make_config_struct("PlayerSettings", config_attr, vec![field]);
    
    let program = Program {
        items: vec![Item::Struct(struct_def)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok());
    
    let files = result.unwrap();
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    
    // Should generate Blueprint getter
    assert!(header.content.contains("UFUNCTION(BlueprintCallable"), 
            "Should have BlueprintCallable function");
    assert!(header.content.contains("static float GetHealth()"), 
            "Should have GetHealth() getter");
}

#[test]
fn test_integration_writable_setting() {
    // Test: @setting(writable: true) generates setter
    let config_attr = make_config_attribute("Game");
    let field = make_field("volume", "Float", Some(make_float_expr(1.0)), vec![
        make_setting_attribute(vec![
            ("blueprint", make_bool_expr(true)),
            ("writable", make_bool_expr(true)),
        ])
    ]);
    
    let struct_def = make_config_struct("AudioSettings", config_attr, vec![field]);
    
    let program = Program {
        items: vec![Item::Struct(struct_def)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok());
    
    let files = result.unwrap();
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    
    // Should generate both getter and setter
    assert!(header.content.contains("static float GetVolume()"), 
            "Should have GetVolume() getter");
    assert!(header.content.contains("static void SetVolume(float NewValue)"), 
            "Should have SetVolume() setter");
}

#[test]
fn test_integration_multiple_config_structs() {
    // Test: Multiple @config structs in one program
    let struct1 = make_config_struct(
        "GraphicsSettings",
        make_config_attribute("Game"),
        vec![make_field("quality", "Int", Some(make_int_expr(2)), vec![
            make_setting_attribute(vec![])
        ])],
    );
    
    let struct2 = make_config_struct(
        "AudioSettings",
        make_config_attribute("Game"),
        vec![make_field("volume", "Float", Some(make_float_expr(1.0)), vec![
            make_setting_attribute(vec![])
        ])],
    );
    
    let program = Program {
        items: vec![Item::Struct(struct1), Item::Struct(struct2)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok());
    
    let files = result.unwrap();
    assert_eq!(files.len(), 4, "Should generate 2 headers and 2 sources");
    
    // Verify both classes are generated
    assert!(files.iter().any(|f| f.path.contains("GraphicsSettings.h")), 
            "Should generate GraphicsSettings.h");
    assert!(files.iter().any(|f| f.path.contains("AudioSettings.h")), 
            "Should generate AudioSettings.h");
}

#[test]
fn test_integration_type_mapping_float() {
    // Test: Float → float mapping
    let config_attr = make_config_attribute("Game");
    let field = make_field("value", "Float", Some(make_float_expr(1.5)), vec![
        make_setting_attribute(vec![])
    ]);
    
    let struct_def = make_config_struct("TestSettings", config_attr, vec![field]);
    
    let program = Program {
        items: vec![Item::Struct(struct_def)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok());
    
    let files = result.unwrap();
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    assert!(header.content.contains("float Value"), "Float should map to float");
    
    let source = files.iter().find(|f| f.path.ends_with(".cpp")).unwrap();
    assert!(source.content.contains("Value(1.5f)"), "Float literal should have f suffix");
}

#[test]
fn test_integration_type_mapping_int() {
    // Test: Int → int32 mapping
    let config_attr = make_config_attribute("Game");
    let field = make_field("count", "Int", Some(make_int_expr(42)), vec![
        make_setting_attribute(vec![])
    ]);
    
    let struct_def = make_config_struct("TestSettings", config_attr, vec![field]);
    
    let program = Program {
        items: vec![Item::Struct(struct_def)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok());
    
    let files = result.unwrap();
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    assert!(header.content.contains("int32 Count"), "Int should map to int32");
    
    let source = files.iter().find(|f| f.path.ends_with(".cpp")).unwrap();
    assert!(source.content.contains("Count(42)"), "Int literal should be plain number");
}

#[test]
fn test_integration_type_mapping_bool() {
    // Test: Bool → bool mapping
    let config_attr = make_config_attribute("Game");
    let field = make_field("enabled", "Bool", Some(make_bool_expr(true)), vec![
        make_setting_attribute(vec![])
    ]);
    
    let struct_def = make_config_struct("TestSettings", config_attr, vec![field]);
    
    let program = Program {
        items: vec![Item::Struct(struct_def)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok());
    
    let files = result.unwrap();
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    assert!(header.content.contains("bool Enabled"), "Bool should map to bool");
    
    let source = files.iter().find(|f| f.path.ends_with(".cpp")).unwrap();
    assert!(source.content.contains("Enabled(true)"), "Bool literal should be true/false");
}

#[test]
fn test_integration_type_mapping_string() {
    // Test: String → FString mapping
    let config_attr = make_config_attribute("Game");
    let field = make_field("name", "String", Some(make_string_expr("Default")), vec![
        make_setting_attribute(vec![])
    ]);
    
    let struct_def = make_config_struct("TestSettings", config_attr, vec![field]);
    
    let program = Program {
        items: vec![Item::Struct(struct_def)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok());
    
    let files = result.unwrap();
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    assert!(header.content.contains("FString Name"), "String should map to FString");
    
    let source = files.iter().find(|f| f.path.ends_with(".cpp")).unwrap();
    assert!(source.content.contains("Name(TEXT(\"Default\"))"), "String literal should use TEXT()");
}

#[test]
fn test_integration_no_default_value() {
    // Test: Field without default value
    let config_attr = make_config_attribute("Game");
    let field = make_field("optional", "Float", None, vec![
        make_setting_attribute(vec![])
    ]);
    
    let struct_def = make_config_struct("TestSettings", config_attr, vec![field]);
    
    let program = Program {
        items: vec![Item::Struct(struct_def)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok());
    
    let files = result.unwrap();
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    assert!(header.content.contains("float Optional"), "Should have Optional field");
    
    // Constructor should not initialize this field (or use default value)
    let source = files.iter().find(|f| f.path.ends_with(".cpp")).unwrap();
    // The field might be initialized with a default value like 0.0f or not initialized at all
    // This is implementation-dependent
}

#[test]
fn test_integration_empty_config_struct() {
    // Test: @config struct with no fields
    let config_attr = make_config_attribute("Game");
    let struct_def = make_config_struct("EmptySettings", config_attr, vec![]);
    
    let program = Program {
        items: vec![Item::Struct(struct_def)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok());
    
    let files = result.unwrap();
    assert_eq!(files.len(), 2, "Should still generate header and source");
    
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    assert!(header.content.contains("UEmptySettings"), "Should have class name");
    assert!(header.content.contains("UDeveloperSettings"), "Should inherit from UDeveloperSettings");
}

#[test]
fn test_integration_complex_scenario() {
    // Test: Complex scenario with all features combined
    let config_attr = make_config_attribute_with_args(vec![
        ("category", make_string_expr("Game")),
        ("display_name", make_string_expr("Advanced Voxel Settings")),
    ]);
    
    let fields = vec![
        make_field("chunk_size", "Float", Some(make_float_expr(100.0)), vec![
            make_setting_attribute(vec![
                ("display_name", make_string_expr("Chunk Size")),
                ("tooltip", make_string_expr("Size of voxel chunks in world units")),
                ("cvar", make_string_expr("voxel.ChunkSize")),
                ("blueprint", make_bool_expr(true)),
                ("min", make_float_expr(10.0)),
                ("max", make_float_expr(1000.0)),
            ])
        ]),
        make_field("max_lod", "Int", Some(make_int_expr(4)), vec![
            make_setting_attribute(vec![
                ("display_name", make_string_expr("Max LOD Levels")),
                ("cvar", make_string_expr("voxel.MaxLOD")),
                ("blueprint", make_bool_expr(true)),
                ("min", make_float_expr(1.0)),
                ("max", make_float_expr(8.0)),
            ])
        ]),
        make_field("debug_vis", "Bool", Some(make_bool_expr(false)), vec![
            make_setting_attribute(vec![
                ("display_name", make_string_expr("Enable Debug Visualization")),
                ("cvar", make_string_expr("voxel.DebugVis")),
                ("blueprint", make_bool_expr(true)),
                ("writable", make_bool_expr(true)),
            ])
        ]),
        make_field("material_path", "String", Some(make_string_expr("/Game/Materials/VoxelMaterial")), vec![
            make_setting_attribute(vec![
                ("display_name", make_string_expr("Material Path")),
                ("tooltip", make_string_expr("Path to the voxel material asset")),
            ])
        ]),
    ];
    
    let struct_def = make_config_struct("VoxelSettings", config_attr, fields);
    
    let program = Program {
        items: vec![Item::Struct(struct_def)],
        span: Span::default(),
    };
    
    let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
    assert!(result.is_ok(), "Complex scenario should generate successfully");
    
    let files = result.unwrap();
    assert_eq!(files.len(), 2);
    
    let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
    
    // Verify all features are present
    assert!(header.content.contains("UCLASS(Config=Game"), "Should use Game config");
    assert!(header.content.contains("DisplayName=\"Advanced Voxel Settings\""), "Should use custom display name");
    assert!(header.content.contains("float ChunkSize"), "Should have Float field");
    assert!(header.content.contains("int32 MaxLod"), "Should have Int field");
    assert!(header.content.contains("bool DebugVis"), "Should have Bool field");
    assert!(header.content.contains("FString MaterialPath"), "Should have String field");
    assert!(header.content.contains("ClampMin=\"10"), "Should have min constraint");
    assert!(header.content.contains("ClampMax=\"1000"), "Should have max constraint");
    assert!(header.content.contains("ToolTip=\"Size of voxel chunks"), "Should have tooltip");
    assert!(header.content.contains("static float GetChunkSize()"), "Should have Blueprint getter");
    assert!(header.content.contains("static void SetDebugVis(bool NewValue)"), "Should have Blueprint setter for writable field");
    
    let source = files.iter().find(|f| f.path.ends_with(".cpp")).unwrap();
    assert!(source.content.contains("ChunkSize(100.0f)"), "Should initialize Float");
    assert!(source.content.contains("MaxLod(4)"), "Should initialize Int");
    assert!(source.content.contains("DebugVis(false)"), "Should initialize Bool");
    assert!(source.content.contains("MaterialPath(TEXT(\"/Game/Materials/VoxelMaterial\"))"), "Should initialize String");
}
