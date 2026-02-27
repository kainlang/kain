//! Preprocessor utilities for handling #include, #define, #ifdef, etc.

use std::collections::HashMap;
use std::path::{Path, PathBuf};
use crate::{ImportError, Result};

/// Preprocessor configuration
pub struct PreprocessorConfig {
    /// Include search paths
    pub include_paths: Vec<PathBuf>,
    
    /// Predefined macros
    pub defines: HashMap<String, String>,
    
    /// Whether to follow system includes (<stdio.h>)
    pub follow_system_includes: bool,
}

impl PreprocessorConfig {
    pub fn new() -> Self {
        Self {
            include_paths: Vec::new(),
            defines: HashMap::new(),
            follow_system_includes: false,
        }
    }
    
    pub fn add_include_path(&mut self, path: impl Into<PathBuf>) {
        self.include_paths.push(path.into());
    }
    
    pub fn add_define(&mut self, name: impl Into<String>, value: impl Into<String>) {
        self.defines.insert(name.into(), value.into());
    }
}

impl Default for PreprocessorConfig {
    fn default() -> Self {
        Self::new()
    }
}

/// Resolve an include path
pub fn resolve_include(
    include_name: &str,
    current_file: &Path,
    config: &PreprocessorConfig,
) -> Result<Option<PathBuf>> {
    // Try relative to current file first
    let relative = current_file.parent()
        .map(|p| p.join(include_name));
    
    if let Some(path) = relative {
        if path.exists() {
            return Ok(Some(path));
        }
    }
    
    // Try include paths
    for include_path in &config.include_paths {
        let path = include_path.join(include_name);
        if path.exists() {
            return Ok(Some(path));
        }
    }
    
    // System includes - skip if not following
    if !config.follow_system_includes {
        return Ok(None);
    }
    
    Ok(None)
}
