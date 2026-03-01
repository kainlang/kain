//! C language importer
//!
//! Imports C source code into KAIN AST using the `lang-c` parser.
//!
//! ## Features
//!
//! - Full C11 support
//! - Preprocessor handling (#include, #define, #ifdef)
//! - Struct, enum, typedef support
//! - Function definitions
//! - Pointer and array types
//! - Type inference where possible
//!
//! ## Example
//!
//! ```rust,no_run
//! use kain_import::c;
//! use std::path::Path;
//!
//! let program = c::import_c_file(Path::new("physics.c"))?;
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```

mod parser;
mod transformer;
mod types;

use kain_core::ast::Program;
use kain_core::language_features::LanguageCapabilities;
use std::path::Path;
use crate::Result;

#[derive(Debug, Clone, Default)]
pub struct CImportOptions {
    /// Extra include search paths (`-I`).
    pub include_paths: Vec<String>,
    /// Extra preprocessor defines (`-DNAME[=VALUE]`).
    pub defines: Vec<String>,
    /// Additional raw cpp flags appended after include/define flags.
    pub cpp_options: Vec<String>,
    /// Optional explicit preprocessor command override.
    pub cpp_command: Option<String>,
}

/// Import a single C file into KAIN AST
pub fn import_c_file(path: &Path) -> Result<Program> {
    import_c_file_with_language_capabilities(path, kain_core::default_language_capabilities())
}

/// Import a single C file with importer options.
pub fn import_c_file_with_options(path: &Path, options: &CImportOptions) -> Result<Program> {
    import_c_file_with_language_capabilities_and_options(
        path,
        kain_core::default_language_capabilities(),
        options,
    )
}

/// Import a single C file with an explicit KAIN language capability profile.
pub fn import_c_file_with_language_capabilities(
    path: &Path,
    language_capabilities: LanguageCapabilities,
) -> Result<Program> {
    import_c_file_with_language_capabilities_and_options(
        path,
        language_capabilities,
        &CImportOptions::default(),
    )
}

/// Import a single C file with explicit capability profile and importer options.
pub fn import_c_file_with_language_capabilities_and_options(
    path: &Path,
    language_capabilities: LanguageCapabilities,
    options: &CImportOptions,
) -> Result<Program> {
    let parsed = parser::parse_c_file_with_metadata(path, options)?;
    let kain_ast = transformer::transform_with_language_capabilities_and_layout_metadata(
        parsed.unit,
        language_capabilities,
        parsed.layout,
    )?;

    Ok(kain_ast)
}

/// Import multiple C files as a single program
pub fn import_c_project(paths: &[&Path]) -> Result<Program> {
    import_c_project_with_language_capabilities(paths, kain_core::default_language_capabilities())
}

/// Import multiple C files with an explicit KAIN language capability profile.
pub fn import_c_project_with_language_capabilities(
    paths: &[&Path],
    language_capabilities: LanguageCapabilities,
) -> Result<Program> {
    let mut all_items = Vec::new();
    
    for path in paths {
        let program = import_c_file_with_language_capabilities_and_options(
            path,
            language_capabilities.clone(),
            &CImportOptions::default(),
        )?;
        all_items.extend(program.items);
    }
    
    Ok(Program { 
        items: all_items,
        span: kain_core::span::Span::default(),
    })
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_import_simple_c() {
        // Test will be added once we have test fixtures
    }
}
