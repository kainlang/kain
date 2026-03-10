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

pub mod preprocessor;
pub mod transformer;

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


impl std::fmt::Display for UsfImportError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            UsfImportError::EngineShaderWithoutResearchMode { path } => {
                write!(f, "Engine shader detected: {}\n\n\
                    ⚠️  This appears to be a UE5 engine shader (copyright Epic Games, Inc.)\n\
                    \n\
                    For RESEARCH and EDUCATIONAL purposes only, use:\n\
                    kain import-usf \"{}\" --research\n\
                    \n\
                    Generated files should NOT be distributed or used commercially.\n\
                    Use this tool to LEARN techniques, then implement your own versions.",
                    path.display(), path.display())
            }
            UsfImportError::FileNotFound { path } => {
                write!(f, "File not found: {}", path.display())
            }
            UsfImportError::ParseError { path, message } => {
                write!(f, "Parse error in {}: {}", path.display(), message)
            }
            UsfImportError::IncludeResolutionFailed { include_path } => {
                write!(f, "Failed to resolve include: {}", include_path)
            }
            UsfImportError::UnsupportedFeature { feature } => {
                write!(f, "Unsupported USF feature: {}", feature)
            }
        }
    }
}

impl std::error::Error for UsfImportError {}

/// Main USF import function
pub fn import_usf_file(
    path: &Path,
    config: UsfImportConfig,
) -> Result<Program, UsfImportError> {
    // Check if this is an engine shader
    let is_engine_shader = path.to_str()
        .map(|s| s.contains("/Engine/Shaders/") || s.contains("\\Engine\\Shaders\\"))
        .unwrap_or(false);
    
    if is_engine_shader && !config.research_mode {
        return Err(UsfImportError::EngineShaderWithoutResearchMode {
            path: path.to_path_buf(),
        });
    }
    
    // Read source file
    let source = std::fs::read_to_string(path)
        .map_err(|_| UsfImportError::FileNotFound {
            path: path.to_path_buf(),
        })?;
    
    // Step 1: Preprocess (strip includes, expand macros)
    let preprocess_result = preprocessor::preprocess_usf(
        &source,
        config.preserve_comments,
        config.flatten_includes,
        config.engine_shaders_path.as_deref(),
    );
    
    // Step 2: Parse HLSL using tree-sitter
    let mut parser = tree_sitter::Parser::new();
    parser.set_language(&tree_sitter_hlsl::LANGUAGE_HLSL.into())
        .map_err(|e| UsfImportError::ParseError {
            path: path.to_path_buf(),
            message: format!("Failed to set tree-sitter language: {:?}", e),
        })?;
    
    let tree = parser.parse(&preprocess_result.output, None)
        .ok_or_else(|| UsfImportError::ParseError {
            path: path.to_path_buf(),
            message: "tree-sitter parse returned None".to_string(),
        })?;
    
    // Step 3: Transform tree-sitter Tree → KAIN AST
    let transformer = transformer::UsfTransformer::new(&preprocess_result.output, tree);
    let program = transformer.transform()
        .map_err(|e| UsfImportError::ParseError {
            path: path.to_path_buf(),
            message: format!("Transform error: {}", e),
        })?;
    
    // Step 4: Add attribution if requested
    if config.add_attribution && is_engine_shader {
        // Add comment to first item
        // TODO: implement comment injection
    }
    
    // Step 5: Add LLM annotations if requested
    if config.llm_annotations {
        // TODO: add pattern annotations for LLM training
    }
    
    Ok(program)
}

/// Quick import for research purposes (enables all flags)
pub fn import_for_research(path: &Path, engine_shaders_path: &Path) -> Result<Program, UsfImportError> {
    import_usf_file(path, UsfImportConfig {
        research_mode: true,
        preserve_comments: true,
        add_attribution: true,
        llm_annotations: true,
        flatten_includes: true,
        engine_shaders_path: Some(engine_shaders_path.to_path_buf()),
    })
}
