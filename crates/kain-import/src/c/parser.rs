//! C parser using lang-c

use lang_c::driver::{Config, parse_preprocessed};
use lang_c::ast::TranslationUnit;
use std::path::Path;
use crate::{ImportError, Result};

/// Parse a C file using lang-c
pub fn parse_c_file(path: &Path) -> Result<TranslationUnit> {
    let config = Config::default();
    let source = std::fs::read_to_string(path)
        .map_err(|e| ImportError::IoError(e))?;
    
    let parse_result = parse_preprocessed(&config, source)
        .map_err(|e| ImportError::CParseError(format!("{:?}", e)))?;
    
    Ok(parse_result.unit)
}

/// Parse C source code from a string
pub fn parse_c_source(source: &str) -> Result<TranslationUnit> {
    let config = Config::default();
    
    let parse_result = parse_preprocessed(&config, source.to_string())
        .map_err(|e| ImportError::CParseError(format!("{:?}", e)))?;
    
    Ok(parse_result.unit)
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_parse_simple_function() {
        let source = r#"
            int add(int a, int b) {
                return a + b;
            }
        "#;
        
        let result = parse_c_source(source);
        assert!(result.is_ok());
    }
    
    #[test]
    fn test_parse_struct() {
        let source = r#"
            struct Point {
                float x;
                float y;
            };
        "#;
        
        let result = parse_c_source(source);
        assert!(result.is_ok());
    }
}
