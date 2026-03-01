//! Rust source parser — wraps `syn` for clean error reporting.

use syn::File;
use std::path::Path;
use crate::{ImportError, Result};

/// Parse Rust source text into a `syn::File` AST.
///
/// All syntax errors are converted to `ImportError::RustParseError`
/// with the file path included for diagnostics.
pub fn parse_rust(source: &str, path: &Path) -> Result<File> {
    syn::parse_file(source).map_err(|e| {
        ImportError::RustParseError(format!(
            "{}: {}",
            path.display(),
            e
        ))
    })
}
