// ============================================================================
// KAIN USF Importer - Research & LLM Training Mode
// ============================================================================
// Imports Unreal Engine 5 USF shaders into KAIN for:
// - Algorithm study and pattern analysis
// - LLM training corpus generation
// - Cross-compilation research (SPIR-V, HLSL, WGSL, Metal)
// - Shader optimization technique extraction
//
// LEGAL NOTICE:
// This importer is designed for RESEARCH and EDUCATIONAL purposes only.
// Imported UE5 engine shaders remain copyright Epic Games, Inc.
// Generated KAIN files should NOT be distributed or used commercially.
// Use this tool to LEARN techniques, then implement your own versions.
// ============================================================================

pub mod parser;
pub mod preprocessor;
pub mod semantic_mapper;
pub mod types;

use std::path::{Path, PathBuf};
use kain_core::ast::Program;

#[derive(Debug, Clone)]
pub struct UsfImportConfig {
    /// Enable research mode (allows engine shader imports with warnings)
    pub research_mode: bool,
    
    /// Preserve original comments for pattern analysis
    pub preserve_comments: bool,
    
    /// Add attribution headers to generated files
    pub add_attribution: bool,
    
    /// Generate LLM-friendly annotations
    pub llm_annotations: bool,
    
    /// Flatten includes (inline all dependencies)
    pub flatten_includes: bool,
    
    /// UE5 engine shader directory for include resolution
    pub engine_shaders_path: Option<PathBuf>,
}

impl Default for UsfImportConfig {
    fn default() -> Self {
        Self {
            research_mode: false,
            preserve_comments: true,
            add_attribution: true,
            llm_annotations: false,
            flatten_includes: false,
            engine_shaders_path: None,
        }
    }
}

#[derive(Debug)]
pub enum UsfImportError {
    EngineShaderWithoutResearchMode { path: PathBuf },
    FileNotFound { path: PathBuf },
    ParseError { path: PathBuf, message: String },
    IncludeResolutionFailed { include_path: String },
    UnsupportedFeature { feature: String },
}
