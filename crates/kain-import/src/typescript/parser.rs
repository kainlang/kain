//! TypeScript parser using SWC
//!
//! This module wraps the `swc_ecma_parser` crate to parse TypeScript source files.

use swc_common::{
    errors::{ColorConfig, Handler},
    sync::Lrc,
    FileName, FilePathMapping, SourceMap,
};
use swc_ecma_ast::Module;
use swc_ecma_parser::{lexer::Lexer, Parser, StringInput, Syntax, TsConfig, EsConfig};
use std::path::Path;
use crate::{ImportError, Result};

/// Parse a TypeScript source file into a SWC Module AST.
///
/// This function:
/// 1. Creates a SWC SourceMap for error reporting
/// 2. Configures the parser for TypeScript syntax
/// 3. Parses the source into a Module AST
pub fn parse_typescript(source: &str, path: &Path) -> Result<Module> {
    // Create source map for error reporting
    let cm: Lrc<SourceMap> = Default::default();
    
    // Create source file
    let fm = cm.new_source_file(
        Lrc::new(FileName::Real(path.to_path_buf())),
        source.to_string(),
    );

    // Configure TypeScript parser
    let syntax = Syntax::Typescript(TsConfig {
        tsx: path.extension().and_then(|e| e.to_str()) == Some("tsx"),
        decorators: true,
        dts: false,
        no_early_errors: false,
        disallow_ambiguous_jsx_like: true,
    });

    // Create lexer
    let lexer = Lexer::new(
        syntax,
        EsConfig::default(), // ES2022
        StringInput::from(&*fm),
        None,
    );

    // Create parser
    let mut parser = Parser::new_from(lexer);

    // Parse module
    parser
        .parse_module()
        .map_err(|e| {
            let msg = format!("TypeScript parse error: {:?}", e);
            ImportError::TypeScriptParseError(msg)
        })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;

    #[test]
    fn test_parse_simple_function() {
        let source = r#"
            function add(a: number, b: number): number {
                return a + b;
            }
        "#;
        let path = PathBuf::from("test.ts");
        let result = parse_typescript(source, &path);
        assert!(result.is_ok());
    }

    #[test]
    fn test_parse_interface() {
        let source = r#"
            interface User {
                name: string;
                age: number;
            }
        "#;
        let path = PathBuf::from("test.ts");
        let result = parse_typescript(source, &path);
        assert!(result.is_ok());
    }

    #[test]
    fn test_parse_enum() {
        let source = r#"
            enum Color {
                Red,
                Green,
                Blue
            }
        "#;
        let path = PathBuf::from("test.ts");
        let result = parse_typescript(source, &path);
        assert!(result.is_ok());
    }

    #[test]
    fn test_parse_class() {
        let source = r#"
            class Point {
                x: number;
                y: number;
                
                constructor(x: number, y: number) {
                    this.x = x;
                    this.y = y;
                }
            }
        "#;
        let path = PathBuf::from("test.ts");
        let result = parse_typescript(source, &path);
        assert!(result.is_ok());
    }

    #[test]
    fn test_parse_error() {
        let source = "function broken(";
        let path = PathBuf::from("test.ts");
        let result = parse_typescript(source, &path);
        assert!(result.is_err());
    }
}
