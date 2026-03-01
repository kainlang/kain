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

pub mod config_ir;
pub mod parser;
pub mod developer_settings_codegen;
pub mod blueprint_accessor_codegen;
pub mod cvar_codegen;
pub mod ini_file_generator;

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

/// Generate UE5 configuration code from a KAIN program
///
/// This is the main entry point for the ue5-config crate.
/// It parses @config structs from the program and generates:
/// - UDeveloperSettings .h/.cpp files
/// - Console variable registration
/// - Blueprint accessor functions
/// - .ini file sections
///
/// # Arguments
///
/// * `program` - The parsed KAIN program
/// * `plugin_name` - The plugin name (used for .ini sections and CVar prefixes)
/// * `module_api` - The module API macro (e.g., "MYPLUGIN_API")
///
/// # Returns
///
/// A vector of generated files with their paths and content
///
/// # Example
///
/// ```rust,ignore
/// use ue5_config::generate_config_code;
/// use kain_core::ast::Program;
///
/// let program = /* parsed KAIN program */;
/// let files = generate_config_code(&program, "MyPlugin", "MYPLUGIN_API")?;
///
/// for file in files {
///     println!("Generated: {}", file.path);
///     std::fs::write(&file.path, &file.content)?;
/// }
/// ```
pub fn generate_config_code(
    program: &Program,
    plugin_name: &str,
    _module_api: &str,
) -> Result<Vec<GeneratedFile>> {
    use crate::parser::parse_config_attribute;
    use crate::developer_settings_codegen::generate;
    use kain_core::ast::Item;

    let mut generated_files = Vec::new();

    // Find all @config structs
    for item in &program.items {
        if let Item::Struct(struct_def) = item {
            if let Some(config_struct) = parse_config_attribute(struct_def)? {
                // Phase 2: Generate UDeveloperSettings .h/.cpp
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
                
                // TODO: Phase 3 - Generate console variables
                // TODO: Phase 3 - Generate .ini file sections
                // TODO: Phase 4 - Generate Blueprint accessors
            }
        }
    }

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
