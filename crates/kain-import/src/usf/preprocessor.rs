//! USF Preprocessor — UE5 Shader Dependency Stripper
//!
//! Transforms UE5 USF shaders into standalone KAIN-compatible code by:
//! 1. Stripping engine includes (#include "/Engine/...")
//! 2. Expanding UE5-specific macros to their semantic meaning
//! 3. Removing UE5 pragmas
//! 4. Flattening includes for research/LLM training mode
//! 5. Preserving algorithm semantics and user comments
//!
//! ## Design Philosophy
//!
//! The preprocessor is NOT a full C preprocessor — it's a semantic-preserving
//! transformation layer that removes UE5 dependencies while keeping the core
//! shader logic intact for analysis and learning.
//!
//! ## Example
//!
//! ```hlsl
//! // Input (UE5 USF):
//! #include "/Engine/Private/Common.ush"
//! #include "/Engine/Private/SceneTexturesCommon.ush"
//! 
//! float3 Color = TEXTURE_SAMPLE(SceneColor, UV).rgb;
//! ```
//!
//! ```hlsl
//! // Output (KAIN-compatible):
//! // [Stripped: /Engine/Private/Common.ush]
//! // [Stripped: /Engine/Private/SceneTexturesCommon.ush]
//! 
//! float3 Color = SceneColor.Sample(SceneColorSampler, UV).rgb;
//! ```

use std::collections::HashMap;
use std::path::{Path, PathBuf};

/// Preprocessor state for a single USF file
pub struct UsfPreprocessor {
    /// Original source code
    source: String,
    
    /// Stripped engine includes (for attribution)
    stripped_includes: Vec<String>,
    
    /// Expanded macros (for documentation)
    expanded_macros: HashMap<String, String>,
    
    /// Flattened include content (research mode)
    flattened_content: Vec<String>,
    
    /// Whether to preserve user comments
    preserve_comments: bool,
    
    /// Whether to flatten includes
    flatten_includes: bool,
    
    /// Engine shaders path for include resolution
    engine_shaders_path: Option<PathBuf>,
}

impl UsfPreprocessor {
    /// Create a new preprocessor for the given source
    pub fn new(source: String) -> Self {
        Self {
            source,
            stripped_includes: Vec::new(),
            expanded_macros: HashMap::new(),
            flattened_content: Vec::new(),
            preserve_comments: true,
            flatten_includes: false,
            engine_shaders_path: None,
        }
    }
    
    /// Enable comment preservation (default: true)
    pub fn preserve_comments(mut self, preserve: bool) -> Self {
        self.preserve_comments = preserve;
        self
    }
    
    /// Enable include flattening for research mode
    pub fn flatten_includes(mut self, flatten: bool) -> Self {
        self.flatten_includes = flatten;
        self
    }
    
    /// Set engine shaders path for include resolution
    pub fn engine_shaders_path(mut self, path: PathBuf) -> Self {
        self.engine_shaders_path = Some(path);
        self
    }
    
    /// Run the full preprocessing pipeline
    pub fn process(mut self) -> PreprocessResult {
        let mut output = self.source.clone();
        
        // Step 1: Strip engine includes
        output = self.strip_engine_includes(output);
        
        // Step 2: Expand UE5 macros
        output = self.expand_ue5_macros(output);
        
        // Step 3: Strip UE5 pragmas
        output = self.strip_ue5_pragmas(output);
        
        // Step 4: Flatten includes if requested
        if self.flatten_includes {
            output = self.flatten_includes_impl(output);
        }
        
        PreprocessResult {
            output,
            stripped_includes: self.stripped_includes,
            expanded_macros: self.expanded_macros,
            flattened_content: self.flattened_content,
        }
    }
    
    /// Strip UE5 engine includes (#include "/Engine/...")
    fn strip_engine_includes(&mut self, source: String) -> String {
        let mut output = String::new();
        
        for line in source.lines() {
            let trimmed = line.trim();
            
            // Detect engine includes
            if trimmed.starts_with("#include") && trimmed.contains("/Engine/") {
                // Extract include path
                if let Some(path) = extract_include_path(trimmed) {
                    self.stripped_includes.push(path.clone());
                    output.push_str(&format!("// [Stripped: {}]\n", path));
                    continue;
                }
            }
            
            output.push_str(line);
            output.push('\n');
        }
        
        output
    }
    
    /// Expand UE5-specific macros to their semantic meaning
    fn expand_ue5_macros(&mut self, source: String) -> String {
        let mut output = source;
        
        // Define UE5 macro expansions
        let macros = get_ue5_macro_expansions();
        
        for (macro_name, expansion) in &macros {
            if output.contains(macro_name) {
                self.expanded_macros.insert(macro_name.clone(), expansion.clone());
                output = expand_macro(&output, macro_name, expansion);
            }
        }
        
        output
    }
    
    /// Strip UE5-specific pragmas
    fn strip_ue5_pragmas(&mut self, source: String) -> String {
        let mut output = String::new();
        
        for line in source.lines() {
            let trimmed = line.trim();
            
            // Strip UE5 pragmas
            if trimmed.starts_with("#pragma") {
                if is_ue5_pragma(trimmed) {
                    output.push_str(&format!("// [Stripped pragma: {}]\n", trimmed));
                    continue;
                }
            }
            
            output.push_str(line);
            output.push('\n');
        }
        
        output
    }
    
    /// Flatten includes by inlining their content (research mode)
    fn flatten_includes_impl(&mut self, source: String) -> String {
        if self.engine_shaders_path.is_none() {
            return source; // Can't flatten without engine path
        }
        
        let mut output = String::new();
        
        for line in source.lines() {
            let trimmed = line.trim();
            
            if trimmed.starts_with("#include") {
                if let Some(include_path) = extract_include_path(trimmed) {
                    // Try to resolve and inline
                    if let Some(content) = self.resolve_include(&include_path) {
                        self.flattened_content.push(include_path.clone());
                        output.push_str(&format!("// ──── Begin: {} ────\n", include_path));
                        output.push_str(&content);
                        output.push_str(&format!("// ──── End: {} ────\n", include_path));
                        continue;
                    }
                }
            }
            
            output.push_str(line);
            output.push('\n');
        }
        
        output
    }
    
    /// Resolve an include path to file content
    fn resolve_include(&self, include_path: &str) -> Option<String> {
        let engine_path = self.engine_shaders_path.as_ref()?;
        
        // Convert /Engine/Private/Common.ush to <engine_path>/Private/Common.ush
        let relative_path = include_path.strip_prefix("/Engine/")?;
        let full_path = engine_path.join(relative_path);
        
        std::fs::read_to_string(full_path).ok()
    }
}

/// Result of preprocessing
pub struct PreprocessResult {
    /// Preprocessed output
    pub output: String,
    
    /// List of stripped engine includes
    pub stripped_includes: Vec<String>,
    
    /// Map of expanded macros (macro_name -> expansion)
    pub expanded_macros: HashMap<String, String>,
    
    /// List of flattened includes (research mode)
    pub flattened_content: Vec<String>,
}

// ── Helper Functions ──────────────────────────────────────────────────────────

/// Extract include path from #include directive
fn extract_include_path(line: &str) -> Option<String> {
    // Handle both #include "path" and #include <path>
    let line = line.trim();
    
    if let Some(start) = line.find('"') {
        if let Some(end) = line[start + 1..].find('"') {
            return Some(line[start + 1..start + 1 + end].to_string());
        }
    }
    
    if let Some(start) = line.find('<') {
        if let Some(end) = line[start + 1..].find('>') {
            return Some(line[start + 1..start + 1 + end].to_string());
        }
    }
    
    None
}

/// Check if a pragma is UE5-specific
fn is_ue5_pragma(pragma: &str) -> bool {
    let ue5_pragmas = [
        "once",
        "warning",
        "message",
    ];
    
    for ue5_pragma in &ue5_pragmas {
        if pragma.contains(ue5_pragma) {
            return true;
        }
    }
    
    false
}


/// Get UE5 macro expansions (macro_name -> semantic expansion)
fn get_ue5_macro_expansions() -> HashMap<String, String> {
    let mut macros = HashMap::new();
    
    // Texture sampling macros
    macros.insert(
        "TEXTURE_SAMPLE".to_string(),
        "{{TEXTURE}}.Sample({{TEXTURE}}Sampler, {{UV}})".to_string(),
    );
    macros.insert(
        "TEXTURE_SAMPLE_LEVEL".to_string(),
        "{{TEXTURE}}.SampleLevel({{TEXTURE}}Sampler, {{UV}}, {{LEVEL}})".to_string(),
    );
    macros.insert(
        "TEXTURE_SAMPLE_GRAD".to_string(),
        "{{TEXTURE}}.SampleGrad({{TEXTURE}}Sampler, {{UV}}, {{DDX}}, {{DDY}})".to_string(),
    );
    
    // Platform capability macros
    macros.insert(
        "PLATFORM_SUPPORTS_SM6_0_WAVE_OPERATIONS".to_string(),
        "1".to_string(),
    );
    macros.insert(
        "PLATFORM_SUPPORTS_TYPED_UAV_LOAD".to_string(),
        "1".to_string(),
    );
    macros.insert(
        "PLATFORM_SUPPORTS_ROV".to_string(),
        "1".to_string(),
    );
    
    // Material quality macros
    macros.insert(
        "MATERIAL_FULLY_ROUGH".to_string(),
        "0".to_string(),
    );
    macros.insert(
        "MATERIAL_SINGLE_SHADINGMODEL".to_string(),
        "1".to_string(),
    );
    
    // Shader model macros
    macros.insert(
        "FEATURE_LEVEL_SM5".to_string(),
        "1".to_string(),
    );
    macros.insert(
        "FEATURE_LEVEL_SM6".to_string(),
        "1".to_string(),
    );
    
    macros
}


/// Expand a macro in the source code
fn expand_macro(source: &str, macro_name: &str, expansion: &str) -> String {
    // Simple macro expansion for function-like macros
    // This is NOT a full C preprocessor — just handles common UE5 patterns
    
    if expansion.contains("{{") {
        // Function-like macro with parameters
        expand_function_macro(source, macro_name, expansion)
    } else {
        // Simple replacement macro
        source.replace(macro_name, expansion)
    }
}

/// Expand function-like macros (e.g., TEXTURE_SAMPLE(Tex, UV))
fn expand_function_macro(source: &str, macro_name: &str, template: &str) -> String {
    let mut output = String::new();
    let mut i = 0;
    let bytes = source.as_bytes();
    
    while i < source.len() {
        // Check if we're at the start of the macro name
        if source[i..].starts_with(macro_name) {
            let after_macro = i + macro_name.len();
            
            // Check if followed by '('
            if after_macro < source.len() && bytes[after_macro] == b'(' {
                // Extract arguments
                if let Some((args, consumed)) = extract_macro_args(&source[after_macro..]) {
                    // Expand template with arguments
                    let expanded = expand_template(template, &args);
                    output.push_str(&expanded);
                    
                    // Skip past the macro invocation
                    i = after_macro + consumed;
                    continue;
                }
            }
        }
        
        // Not a macro invocation, just copy the character
        if let Some(ch) = source[i..].chars().next() {
            output.push(ch);
            i += ch.len_utf8();
        } else {
            break;
        }
    }
    
    output
}


/// Extract macro arguments from invocation
/// Returns (args, chars_consumed) or None if invalid
fn extract_macro_args(source: &str) -> Option<(Vec<String>, usize)> {
    let mut chars = source.chars().peekable();
    
    // Skip opening '('
    if chars.next()? != '(' {
        return None;
    }
    
    let mut args = Vec::new();
    let mut current_arg = String::new();
    let mut paren_depth = 1;
    let mut consumed = 1;
    
    while let Some(ch) = chars.next() {
        consumed += 1;
        
        match ch {
            '(' => {
                paren_depth += 1;
                current_arg.push(ch);
            }
            ')' => {
                paren_depth -= 1;
                if paren_depth == 0 {
                    // End of macro invocation
                    if !current_arg.trim().is_empty() {
                        args.push(current_arg.trim().to_string());
                    }
                    return Some((args, consumed));
                }
                current_arg.push(ch);
            }
            ',' if paren_depth == 1 => {
                // Argument separator
                args.push(current_arg.trim().to_string());
                current_arg.clear();
            }
            _ => {
                current_arg.push(ch);
            }
        }
    }
    
    None // Unclosed parenthesis
}


/// Expand template with arguments
fn expand_template(template: &str, args: &[String]) -> String {
    let mut output = template.to_string();
    
    // Map common parameter names to argument positions
    let param_names = ["TEXTURE", "UV", "LEVEL", "DDX", "DDY"];
    
    for (i, arg) in args.iter().enumerate() {
        if i < param_names.len() {
            let placeholder = format!("{{{{{}}}}}", param_names[i]);
            output = output.replace(&placeholder, arg);
        }
    }
    
    output
}

// ── Public API ────────────────────────────────────────────────────────────────

/// Strip engine includes from USF source
pub fn strip_engine_includes(source: &str) -> String {
    UsfPreprocessor::new(source.to_string())
        .process()
        .output
}

/// Expand UE5 macros in USF source
pub fn expand_ue5_macros(source: &str) -> String {
    let preprocessor = UsfPreprocessor::new(source.to_string());
    let mut output = source.to_string();
    
    let macros = get_ue5_macro_expansions();
    for (macro_name, expansion) in &macros {
        if output.contains(macro_name) {
            output = expand_macro(&output, macro_name, expansion);
        }
    }
    
    output
}


/// Strip UE5 pragmas from USF source
pub fn strip_ue5_pragmas(source: &str) -> String {
    let mut output = String::new();
    
    for line in source.lines() {
        let trimmed = line.trim();
        
        if trimmed.starts_with("#pragma") && is_ue5_pragma(trimmed) {
            output.push_str(&format!("// [Stripped pragma: {}]\n", trimmed));
        } else {
            output.push_str(line);
            output.push('\n');
        }
    }
    
    output
}

/// Flatten includes by inlining their content (requires engine path)
pub fn flatten_includes(source: &str, engine_shaders_path: &Path) -> String {
    UsfPreprocessor::new(source.to_string())
        .flatten_includes(true)
        .engine_shaders_path(engine_shaders_path.to_path_buf())
        .process()
        .output
}

/// Preserve semantics while removing UE5 dependencies
/// This is the main entry point for semantic-preserving transformation
pub fn preserve_semantics(source: &str) -> PreprocessResult {
    UsfPreprocessor::new(source.to_string())
        .preserve_comments(true)
        .process()
}

/// Full preprocessing pipeline with all options
pub fn preprocess_usf(
    source: &str,
    preserve_comments: bool,
    flatten: bool,
    engine_path: Option<&Path>,
) -> PreprocessResult {
    let mut preprocessor = UsfPreprocessor::new(source.to_string())
        .preserve_comments(preserve_comments)
        .flatten_includes(flatten);
    
    if let Some(path) = engine_path {
        preprocessor = preprocessor.engine_shaders_path(path.to_path_buf());
    }
    
    preprocessor.process()
}


// ── Tests ─────────────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_strip_engine_includes() {
        let source = r#"
#include "/Engine/Private/Common.ush"
#include "MyShader.ush"
#include "/Engine/Private/SceneTexturesCommon.ush"

float3 MyFunction() {
    return float3(1, 2, 3);
}
"#;
        
        let result = strip_engine_includes(source);
        
        assert!(result.contains("// [Stripped: /Engine/Private/Common.ush]"));
        assert!(result.contains("// [Stripped: /Engine/Private/SceneTexturesCommon.ush]"));
        assert!(result.contains("#include \"MyShader.ush\""));
        assert!(result.contains("float3 MyFunction()"));
    }
    
    #[test]
    fn test_expand_texture_sample_macro() {
        let source = "float3 Color = TEXTURE_SAMPLE(SceneColor, UV).rgb;";
        let result = expand_ue5_macros(source);
        
        assert!(result.contains("SceneColor.Sample(SceneColorSampler, UV)"));
    }
    
    #[test]
    fn test_expand_platform_macros() {
        let source = r#"
#if PLATFORM_SUPPORTS_SM6_0_WAVE_OPERATIONS
    float result = WaveActiveSum(value);
#endif
"#;
        let result = expand_ue5_macros(source);
        
        assert!(result.contains("#if 1"));
    }
    
    #[test]
    fn test_strip_ue5_pragmas() {
        let source = r#"
#pragma once
#pragma warning(disable: 4000)
#pragma message("Custom pragma")

float3 MyFunction() {
    return float3(1, 2, 3);
}
"#;
        
        let result = strip_ue5_pragmas(source);
        
        assert!(result.contains("// [Stripped pragma: #pragma once]"));
        assert!(result.contains("// [Stripped pragma: #pragma warning(disable: 4000)]"));
        assert!(result.contains("float3 MyFunction()"));
    }
    
    #[test]
    fn test_extract_include_path() {
        assert_eq!(
            extract_include_path("#include \"/Engine/Private/Common.ush\""),
            Some("/Engine/Private/Common.ush".to_string())
        );
        
        assert_eq!(
            extract_include_path("#include <MyHeader.h>"),
            Some("MyHeader.h".to_string())
        );
        
        assert_eq!(
            extract_include_path("  #include   \"Shader.ush\"  "),
            Some("Shader.ush".to_string())
        );
    }
    
    #[test]
    fn test_extract_macro_args() {
        let source = "(SceneColor, UV)";
        let result = extract_macro_args(source);
        
        assert!(result.is_some());
        let (args, consumed) = result.unwrap();
        assert_eq!(args.len(), 2);
        assert_eq!(args[0], "SceneColor");
        assert_eq!(args[1], "UV");
        assert_eq!(consumed, source.len());
    }
    
    #[test]
    fn test_extract_macro_args_nested() {
        let source = "(Texture.Sample(Sampler, UV), OtherArg)";
        let result = extract_macro_args(source);
        
        assert!(result.is_some());
        let (args, _) = result.unwrap();
        assert_eq!(args.len(), 2);
        assert_eq!(args[0], "Texture.Sample(Sampler, UV)");
        assert_eq!(args[1], "OtherArg");
    }
    
    #[test]
    fn test_expand_template() {
        let template = "{{TEXTURE}}.Sample({{TEXTURE}}Sampler, {{UV}})";
        let args = vec!["SceneColor".to_string(), "UV".to_string()];
        
        let result = expand_template(template, &args);
        assert_eq!(result, "SceneColor.Sample(SceneColorSampler, UV)");
    }
    
    #[test]
    fn test_full_preprocessing_pipeline() {
        let source = r#"
#include "/Engine/Private/Common.ush"
#pragma once

float3 ProcessColor(Texture2D SceneColor, float2 UV) {
    float3 Color = TEXTURE_SAMPLE(SceneColor, UV).rgb;
    return Color * 2.0;
}
"#;
        
        let result = preserve_semantics(source);
        
        // Check engine includes stripped
        assert!(result.output.contains("// [Stripped: /Engine/Private/Common.ush]"));
        
        // Check pragma stripped
        assert!(result.output.contains("// [Stripped pragma: #pragma once]"));
        
        // Check macro expanded
        assert!(result.output.contains("SceneColor.Sample(SceneColorSampler, UV)"));
        
        // Check function preserved
        assert!(result.output.contains("float3 ProcessColor"));
        
        // Check metadata
        assert_eq!(result.stripped_includes.len(), 1);
        assert!(result.expanded_macros.contains_key("TEXTURE_SAMPLE"));
    }
}
