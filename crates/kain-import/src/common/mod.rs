//! Common utilities shared across all importers

pub mod preprocessor;
pub mod c_registry;
pub mod identifier_registry;
pub mod type_mapper;

use std::collections::HashMap;

/// Shared context for all importers
pub struct ImportContext {
    /// Type mappings from source language to KAIN
    pub type_map: HashMap<String, kain_core::ast::Type>,
    
    /// Current file being processed
    pub current_file: Option<String>,
    
    /// Include paths for header resolution
    pub include_paths: Vec<String>,
    
    /// Preprocessor defines
    pub defines: HashMap<String, String>,
}

impl ImportContext {
    pub fn new() -> Self {
        Self {
            type_map: HashMap::new(),
            current_file: None,
            include_paths: Vec::new(),
            defines: HashMap::new(),
        }
    }
    
    pub fn with_include_paths(mut self, paths: Vec<String>) -> Self {
        self.include_paths = paths;
        self
    }
    
    pub fn with_defines(mut self, defines: HashMap<String, String>) -> Self {
        self.defines = defines;
        self
    }
}

impl Default for ImportContext {
    fn default() -> Self {
        Self::new()
    }
}
