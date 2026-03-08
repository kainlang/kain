//! # kain-import
//!
//! Universal import system for KAIN - import multiple source languages into KAIN IR.
//!
//! ## Supported Languages
//!
//! - **C** (via `lang-c`) - Full C11 support with preprocessor
//! - **Rust** (via `syn`) - Import Rust code into KAIN
//! - **TypeScript** (via `swc_ecma_parser`) - Import TypeScript code into KAIN
//! - **C++** (planned) - Import C++ code into KAIN
//! - **Python** (planned) - Import Python code into KAIN
//!
//! ## Usage
//!
//! ```rust,no_run
//! use kain_import;
//! use std::path::Path;
//!
//! // Import a C file
//! let program = kain_import::import_c(Path::new("physics.c"))?;
//!
//! // Import multiple C files (project)
//! let program = kain_import::import_c_project(&[
//!     Path::new("main.c"),
//!     Path::new("utils.c"),
//! ])?;
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```

pub mod c;
pub mod common;

#[cfg(feature = "rust")]
pub mod rust;

#[cfg(feature = "typescript")]
pub mod typescript;

// pub mod cpp;   // Future: tree-sitter-cpp
// pub mod python; // Future: rustpython-parser

use kain_core::ast::Program;
use kain_core::language_features::LanguageCapabilities;
use std::path::Path;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum ImportError {
    #[error("Failed to parse C file: {0}")]
    CParseError(String),
    
    #[error("Failed to parse Rust file: {0}")]
    RustParseError(String),
    
    #[error("Failed to parse TypeScript file: {0}")]
    TypeScriptParseError(String),
    
    #[error("Failed to transform to KAIN AST: {0}")]
    TransformError(String),
    
    #[error("Unsupported language feature: {0}")]
    UnsupportedFeature(String),
    
    #[error("IO error: {0}")]
    IoError(#[from] std::io::Error),
    
    #[error("Type resolution error: {0}")]
    TypeError(String),
}

pub type Result<T> = std::result::Result<T, ImportError>;

/// Import a C file into KAIN AST
///
/// # Example
///
/// ```rust,no_run
/// use kain_import;
/// use std::path::Path;
///
/// let program = kain_import::import_c(Path::new("game_logic.c"))?;
/// # Ok::<(), Box<dyn std::error::Error>>(())
/// ```
pub fn import_c(path: &Path) -> Result<Program> {
    c::import_c_file(path)
}

/// Import a C file using an explicit KAIN language capability profile.
pub fn import_c_with_language_capabilities(
    path: &Path,
    language_capabilities: LanguageCapabilities,
) -> Result<Program> {
    c::import_c_file_with_language_capabilities(path, language_capabilities)
}

/// Import multiple C files as a single program
///
/// Handles dependencies and combines into one KAIN program.
///
/// # Example
///
/// ```rust,no_run
/// use kain_import;
/// use std::path::Path;
///
/// let program = kain_import::import_c_project(&[
///     Path::new("src/main.c"),
///     Path::new("src/physics.c"),
///     Path::new("src/math.c"),
/// ])?;
/// # Ok::<(), Box<dyn std::error::Error>>(())
/// ```
pub fn import_c_project(paths: &[&Path]) -> Result<Program> {
    c::import_c_project(paths)
}

/// Import multiple C files using an explicit KAIN language capability profile.
pub fn import_c_project_with_language_capabilities(
    paths: &[&Path],
    language_capabilities: LanguageCapabilities,
) -> Result<Program> {
    c::import_c_project_with_language_capabilities(paths, language_capabilities)
}

// Future: Rust importer
#[cfg(feature = "rust")]
pub fn import_rust(path: &std::path::Path) -> Result<Program> {
    rust::import_rust_file(path)
}

#[cfg(feature = "rust")]
pub fn import_rust_dir(dir: &std::path::Path, flat: bool) -> Result<Program> {
    rust::import_rust_dir(dir, flat)
}

#[cfg(feature = "rust")]
pub fn import_rust_selfhost_dir(
    dir: &std::path::Path,
    options: &rust::RustSelfHostOptions,
) -> Result<Program> {
    rust::import_rust_selfhost_dir(dir, options)
}

#[cfg(feature = "rust")]
pub fn import_rust_selfhost_dir_detailed(
    dir: &std::path::Path,
    options: &rust::RustSelfHostOptions,
) -> Result<rust::RustSelfHostImportResult> {
    rust::import_rust_selfhost_dir_detailed(dir, options)
}

// TypeScript importer
#[cfg(feature = "typescript")]
pub fn import_typescript(path: &std::path::Path) -> Result<Program> {
    typescript::import_typescript_file(path)
}

#[cfg(feature = "typescript")]
pub fn import_typescript_dir(dir: &std::path::Path, flat: bool) -> Result<Program> {
    typescript::import_typescript_dir(dir, flat)
}
