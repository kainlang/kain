//! UE5 Configuration System Code Generator
//!
//! This crate generates UE5 configuration systems from KAIN `@config` structs:
//! - UDeveloperSettings subclasses (runtime-accessible settings)
//! - Config .ini files (DefaultGame.ini, DefaultEngine.ini sections)
//! - Console variables (CVars) with auto-registration and callbacks
//! - Project Settings UI (automatic Details panel)
//! - Blueprint accessors (Get/Set functions for BP access)
//!
//! # Example
//!
//! ```kain
//! @config(category: "Game")
//! struct VoxelSettings:
//!     @setting(cvar: "voxel.ChunkSize", blueprint: true, min: 10.0, max: 1000.0)
//!     chunk_size: Float = 100.0
//! ```
//!
//! Generates:
//! - UVoxelSettings.h/.cpp (UDeveloperSettings subclass)
//! - Console variable registration (TAutoConsoleVariable<float>)
//! - Blueprint accessor (GetChunkSize())
//! - DefaultGame.ini section

pub mod blueprint_accessor_codegen;
pub mod config_ir;
pub mod cvar_codegen;
pub mod developer_settings_codegen;
pub mod ini_file_generator;
pub mod parser;

use anyhow::Result;
use kain_core::ast::Program;

/// Generated file output
#[derive(Debug, Clone, PartialEq)]
pub struct GeneratedFile {
    /// File path relative to plugin root (e.g., "Source/Public/VoxelSettings.h")
    pub path: String,

    /// File content
    pub content: String,
}

/// Generate UE5 configuration code from a KAIN program (AST entry point)
pub fn generate_config_code(
    program: &Program,
    plugin_name: &str,
    _module_api: &str,
) -> Result<Vec<GeneratedFile>> {
    use crate::developer_settings_codegen::generate;
    use crate::parser::parse_config_attribute;
    use kain_core::ast::Item;

    let mut generated_files = Vec::new();

    for item in &program.items {
        if let Item::Struct(struct_def) = item {
            if let Some(config_struct) = parse_config_attribute(struct_def)? {
                let output = generate(&config_struct, plugin_name)?;
                let struct_name = &config_struct.name;
                generated_files.push(GeneratedFile {
                    path: format!("Source/Public/{}.h", struct_name),
                    content: output.header,
                });
                generated_files.push(GeneratedFile {
                    path: format!("Source/Private/{}.cpp", struct_name),
                    content: output.source,
                });
            }
        }
    }

    Ok(generated_files)
}

/// Generate UE5 configuration code from a TypedProgram (pipeline entry point)
///
/// This is the entry point used by the CLI pipeline which works with
/// TypedProgram after type-checking. Extracts struct ASTs from TypedStruct
/// items and delegates to the same codegen logic.
pub fn generate_config_code_typed(
    program: &kain_core::types::TypedProgram,
    plugin_name: &str,
    module_api: &str,
) -> Result<Vec<GeneratedFile>> {
    use crate::developer_settings_codegen::generate;
    use crate::parser::parse_config_attribute;
    use kain_core::types::TypedItem;

    let mut generated_files = Vec::new();

    for item in &program.items {
        if let TypedItem::Struct(typed_struct) = item {
            if let Some(config_struct) = parse_config_attribute(&typed_struct.ast)? {
                let output = generate(&config_struct, plugin_name)?;
                let struct_name = &config_struct.name;
                generated_files.push(GeneratedFile {
                    path: format!("Source/Public/{}.h", struct_name),
                    content: output.header,
                });
                generated_files.push(GeneratedFile {
                    path: format!("Source/Private/{}.cpp", struct_name),
                    content: output.source,
                });
                // .ini section — write alongside the .h/.cpp
                let ini_file_name = config_struct.get_ini_file();
                if let Ok(ini_content) =
                    crate::ini_file_generator::generate_ini_section(&config_struct, plugin_name)
                {
                    if !ini_content.is_empty() {
                        generated_files.push(GeneratedFile {
                            path: format!("Config/{}", ini_file_name),
                            content: ini_content,
                        });
                    }
                }
            }
        }
    }

    let _ = module_api; // reserved for future use
    Ok(generated_files)
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Attribute, BinaryOp, Expr, Field, Item, Struct, Type, Visibility};
    use kain_core::span::Span;

    fn make_string_expr(s: &str) -> Expr {
        Expr::String(s.to_string(), Span::default())
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
    fn test_generate_config_code_empty_program() {
        let program = Program {
            items: vec![],
            span: Span::default(),
        };

        let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
        assert!(result.is_ok());
        assert_eq!(result.unwrap().len(), 0);
    }

    #[test]
    fn test_generate_config_code_with_config_struct() {
        let program = Program {
            items: vec![Item::Struct(Struct {
                name: "VoxelSettings".to_string(),
                generics: vec![],
                fields: vec![Field {
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
                    default: Some(Expr::Float(100.0, Span::default())),
                    weak: false,
                    span: Span::default(),
                }],
                methods: vec![],
                attributes: vec![Attribute {
                    name: "config".to_string(),
                    args: vec![make_named_arg("category", make_string_expr("Game"))],
                    span: Span::default(),
                }],
                visibility: Visibility::Public,
                span: Span::default(),
            })],
            span: Span::default(),
        };

        let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
        assert!(result.is_ok());

        // Phase 2: Should generate .h and .cpp files
        let files = result.unwrap();
        assert_eq!(files.len(), 2); // Header and source

        // Check header file
        let header = files.iter().find(|f| f.path.ends_with(".h")).unwrap();
        assert_eq!(header.path, "Source/Public/VoxelSettings.h");
        assert!(header.content.contains("UVoxelSettings"));
        assert!(header.content.contains("UDeveloperSettings"));

        // Check source file
        let source = files.iter().find(|f| f.path.ends_with(".cpp")).unwrap();
        assert_eq!(source.path, "Source/Private/VoxelSettings.cpp");
        assert!(source.content.contains("UVoxelSettings::UVoxelSettings()"));
    }

    #[test]
    fn test_generate_config_code_without_config_attribute() {
        let program = Program {
            items: vec![Item::Struct(Struct {
                name: "RegularStruct".to_string(),
                generics: vec![],
                fields: vec![],
                methods: vec![],
                attributes: vec![],
                visibility: Visibility::Public,
                span: Span::default(),
            })],
            span: Span::default(),
        };

        let result = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API");
        assert!(result.is_ok());
        assert_eq!(result.unwrap().len(), 0);
    }
}
