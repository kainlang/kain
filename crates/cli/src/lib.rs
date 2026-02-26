//! KAIN CLI Library
//! 
//! Re-exports all compiler functionality and implements multi-backend compilation.

// Re-export core compiler
pub use kain_core::*;

// CLI-specific modules
pub mod lsp;
pub mod packager;

// Backend imports
#[cfg(feature = "ue5")]
use ue5;

#[cfg(feature = "ue5")]
use ue5_editor;

#[cfg(feature = "ue5")]
use ue5_shaders;

#[cfg(feature = "gpu")]
use gpu;

#[cfg(feature = "web")]
use web;

#[cfg(feature = "sys")]
use sys;

// Constants
pub const VERSION: &str = env!("CARGO_PKG_VERSION");
pub const LANGUAGE_NAME: &str = "KAIN";

#[derive(Clone, Copy)]
struct TargetSpec {
    target: CompileTarget,
    extension: &'static str,
    aliases: &'static [&'static str],
}

const TARGET_SPECS: &[TargetSpec] = &[
    TargetSpec {
        target: CompileTarget::Wasm,
        extension: "wasm",
        aliases: &["wasm", "w"],
    },
    TargetSpec {
        target: CompileTarget::Llvm,
        extension: "ll",
        aliases: &["llvm", "native", "n"],
    },
    TargetSpec {
        target: CompileTarget::Spirv,
        extension: "spv",
        aliases: &["spirv", "gpu", "shader", "s"],
    },
    TargetSpec {
        target: CompileTarget::Hlsl,
        extension: "hlsl",
        aliases: &["hlsl", "h"],
    },
    TargetSpec {
        target: CompileTarget::Usf,
        extension: "usf",
        aliases: &["usf"],
    },
    TargetSpec {
        target: CompileTarget::Js,
        extension: "js",
        aliases: &["js", "javascript", "j"],
    },
    TargetSpec {
        target: CompileTarget::Ts,
        extension: "ts",
        aliases: &["ts", "typescript"],
    },
    TargetSpec {
        target: CompileTarget::Rust,
        extension: "rs",
        aliases: &["rust", "rs"],
    },
    TargetSpec {
        target: CompileTarget::Hybrid,
        extension: "js",
        aliases: &["hybrid", "web"],
    },
    TargetSpec {
        target: CompileTarget::Cpp,
        extension: "cpp",
        aliases: &["cpp", "c++"],
    },
    TargetSpec {
        target: CompileTarget::Ue5,
        extension: "h",
        aliases: &["ue5", "unreal", "u"],
    },
    TargetSpec {
        target: CompileTarget::Ue5Editor,
        extension: "h",
        aliases: &["ue5editor", "ue5-editor", "editor", "slate"],
    },
    TargetSpec {
        target: CompileTarget::Interpret,
        extension: "txt",
        aliases: &["run", "r", "interpret", "i"],
    },
    TargetSpec {
        target: CompileTarget::Test,
        extension: "txt",
        aliases: &["test", "t"],
    },
];

fn find_target_spec_by_alias(alias: &str) -> Option<&'static TargetSpec> {
    let normalized = alias.trim().to_ascii_lowercase();
    TARGET_SPECS.iter().find(|spec| {
        spec.aliases
            .iter()
            .any(|candidate| *candidate == normalized)
    })
}

fn find_target_spec_by_target(target: CompileTarget) -> Option<&'static TargetSpec> {
    TARGET_SPECS.iter().find(|spec| spec.target == target)
}

pub fn parse_compile_target(alias: &str) -> Option<CompileTarget> {
    find_target_spec_by_alias(alias).map(|spec| spec.target)
}

pub fn target_extension(target: CompileTarget) -> &'static str {
    find_target_spec_by_target(target)
        .map(|spec| spec.extension)
        .unwrap_or("out")
}

pub fn supported_targets_csv() -> String {
    TARGET_SPECS
        .iter()
        .map(|spec| spec.aliases[0])
        .collect::<Vec<_>>()
        .join(", ")
}

/// Compile with backend selection
pub fn compile(source: &str, target: CompileTarget) -> Result<String, KainError> {
    // Load stdlib
    let stdlib = stdlib::load_stdlib();
    let full_source = format!("{}\n{}", stdlib, source);

    // 1. Lex
    let tokens = Lexer::new(&full_source).tokenize()?;
    
    // 2. Parse
    let span_mapper = diagnostics::SpanMapper::new(&full_source);
    let mut ast = Parser::new(&tokens, &span_mapper, "<input>").parse()?;
    
    // 2.5 Comptime
    comptime::eval_program(&mut ast)?;
    
    // 3. Type check
    let typed_ast = types::check(&ast, &span_mapper, "<input>")?;
    
    // 3.5 Monomorphize (NEW: Instantiate generic functions with concrete types)
    let mono_ast = monomorphize::monomorphize(&typed_ast)?;
    
    // 4. Codegen based on target
    match target {
        #[cfg(feature = "ue5")]
        CompileTarget::Ue5 => {
            let output = ue5::generate(&mono_ast, None, None)?;
            Ok(output.header + "\n" + &output.source)
        }
        
        #[cfg(feature = "ue5")]
        CompileTarget::Usf => {
            // Convert to TypedProgram for shader codegen (shaders don't use generics yet)
            let typed_for_codegen = TypedProgram { items: mono_ast.items };
            ue5_shaders::generate_usf(&typed_for_codegen)
        }
        
        #[cfg(feature = "gpu")]
        CompileTarget::Spirv => {
            let typed_for_codegen = TypedProgram { items: mono_ast.items };
            gpu::generate_spirv(&typed_for_codegen).map(|bytes| format!("{} bytes", bytes.len()))
        }
        
        #[cfg(feature = "gpu")]
        CompileTarget::Hlsl => {
            let typed_for_codegen = TypedProgram { items: mono_ast.items };
            gpu::generate_hlsl(&typed_for_codegen)
        }
        
        #[cfg(feature = "web")]
        CompileTarget::Wasm => {
            let typed_for_codegen = TypedProgram { items: mono_ast.items };
            web::generate_wasm(&typed_for_codegen).map(|bytes| format!("{} bytes", bytes.len()))
        }
        
        #[cfg(feature = "web")]
        CompileTarget::Js => {
            let typed_for_codegen = TypedProgram { items: mono_ast.items };
            web::generate_js(&typed_for_codegen)
        }

        #[cfg(feature = "web")]
        CompileTarget::Ts => {
            let typed_for_codegen = TypedProgram { items: mono_ast.items };
            web::generate_ts(&typed_for_codegen)
        }
        
        #[cfg(feature = "web")]
        CompileTarget::Hybrid => {
            let typed_for_codegen = TypedProgram { items: mono_ast.items };
            let output = web::generate_hybrid(&typed_for_codegen)?;
            Ok(output.js)
        }
        
        #[cfg(feature = "sys")]
        CompileTarget::Llvm => {
            let typed_for_codegen = TypedProgram { items: mono_ast.items };
            sys::generate_llvm(&typed_for_codegen).map(|_| "LLVM IR generated".to_string())
        }
        
        #[cfg(feature = "sys")]
        CompileTarget::Rust => {
            let typed_for_codegen = TypedProgram { items: mono_ast.items };
            sys::generate_rust(&typed_for_codegen)
        }
        
        #[cfg(feature = "sys")]
        CompileTarget::Cpp => {
            let typed_for_codegen = TypedProgram { items: mono_ast.items };
            sys::generate_cpp(&typed_for_codegen)
        }
        
        CompileTarget::Interpret | CompileTarget::Test => {
            // Use runtime interpreter
            Err(KainError::runtime("Interpret/Test targets not yet implemented in workspace"))
        }
        
        #[cfg(feature = "ue5")]
        CompileTarget::Ue5Editor => {
            let typed_for_codegen = TypedProgram { items: mono_ast.items };
            let output = ue5_editor::generate(&typed_for_codegen, "EditorTools", None)?;
            Ok(output.header + "\n" + &output.source)
        }
        
        #[cfg(not(feature = "ue5"))]
        CompileTarget::Ue5Editor => {
            Err(KainError::runtime("UE5 Editor target requires ue5 feature"))
        }
        
        #[allow(unreachable_patterns)]
        _ => Err(KainError::runtime(format!(
            "Target {:?} not enabled. Recompile with appropriate feature flag.",
            target
        ))),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_typescript_aliases() {
        assert_eq!(parse_compile_target("ts"), Some(CompileTarget::Ts));
        assert_eq!(
            parse_compile_target("typescript"),
            Some(CompileTarget::Ts)
        );
    }

    #[test]
    fn extension_for_typescript_is_ts() {
        assert_eq!(target_extension(CompileTarget::Ts), "ts");
    }
}


// Helper functions for main.rs

#[cfg(feature = "ue5")]
pub fn compile_ue5(source: &str, output_name: Option<&str>, copyright: Option<&str>) -> Result<ue5::Ue5Output, KainError> {
    compile_ue5_with_context(source, output_name, copyright, None)
}

#[cfg(feature = "ue5")]
pub fn compile_ue5_with_context(
    source: &str, 
    output_name: Option<&str>, 
    copyright: Option<&str>,
    metadata_dir: Option<std::path::PathBuf>
) -> Result<ue5::Ue5Output, KainError> {
    // Load stdlib
    let stdlib = stdlib::load_stdlib();
    let full_source = format!("{}\n{}", stdlib, source);
    
    // Parse and type-check
    let tokens = Lexer::new(&full_source).tokenize()?;
    let span_mapper = diagnostics::SpanMapper::new(&full_source);
    let mut ast = Parser::new(&tokens, &span_mapper, "<input>").parse()?;
    comptime::eval_program(&mut ast)?;
    let typed_ast = types::check(&ast, &span_mapper, "<input>")?;
    
    // Monomorphize (instantiate generic functions)
    let mono_ast = monomorphize::monomorphize(&typed_ast)?;
    let typed_for_codegen = TypedProgram { items: mono_ast.items };
    
    // Find metadata directory
    let metadata_path = metadata_dir.unwrap_or_else(|| find_metadata_dir());
    
    // Create Ue5Context with metadata
    let mut context = ue5::Ue5Context::new(
        output_name.unwrap_or("Kain"),
        copyright
    );
    
    // Load metadata if directory exists
    if metadata_path.exists() && metadata_path.is_dir() {
        if let Ok(entries) = std::fs::read_dir(&metadata_path) {
            for entry in entries.flatten() {
                let path = entry.path();
                if path.extension().map_or(false, |e| e == "json") {
                    if let Ok(data) = std::fs::read_to_string(&path) {
                        let filename = path.file_name().and_then(|f| f.to_str()).unwrap_or("");
                        match filename {
                            "widget_registry.json" => {
                                let _ = context.widget_registry.load(&data);
                            }
                            "editor_attributes.json" => {
                                let _ = context.editor_attributes.load(&data);
                            }
                            "shader_knowledge.json" => {
                                let _ = context.shader_knowledge.load(&data);
                            }
                            "uht_rules.json" => {
                                let _ = context.uht_rules.load(&data);
                            }
                            "module_graph.json" => {
                                let _ = context.module_graph.load(&data);
                            }
                            "virtual_obligations.json" => {
                                let _ = context.virtual_obligations.load(&data);
                            }
                            _ => {
                                // Feed into EngineKnowledge
                                let _ = context.knowledge.load_metadata(&data);
                                let _ = context.resolver.load_from_metadata(&data);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Run Oracle validation
    ue5::oracle::validate_program_full(&typed_for_codegen, &context.knowledge, &context.uht_rules, &span_mapper, "<input>")?;
    
    // Generate with context
    ue5::generate_with_context_typed(&typed_for_codegen, output_name, copyright, &context)
}

/// Find metadata directory by searching in order:
/// 1. KAIN_METADATA_DIR env var
/// 2. KAIN_ROOT env var + known suffixes
/// 3. Walk up from CWD with known suffixes
/// 4. Fallback to unreal/metadata relative to CWD
#[cfg(feature = "ue5")]
fn find_metadata_dir() -> std::path::PathBuf {
    if let Ok(explicit) = std::env::var("KAIN_METADATA_DIR") {
        let candidate = std::path::PathBuf::from(explicit);
        if candidate.exists() {
            return candidate;
        }
    }

    let suffixes = [
        std::path::Path::new("unreal").join("metadata"),
        std::path::Path::new("Kain").join("unreal").join("metadata"),
    ];
    
    // 1. Check KAIN_ROOT env var with candidate suffixes
    if let Ok(root) = std::env::var("KAIN_ROOT") {
        let base = std::path::PathBuf::from(root);
        for suffix in &suffixes {
            let candidate = base.join(suffix);
            if candidate.exists() {
                return candidate;
            }
        }
    }
    
    // 2. Walk up from CWD with candidate suffixes
    if let Ok(mut dir) = std::env::current_dir() {
        for _ in 0..10 {
            for suffix in &suffixes {
                let candidate = dir.join(suffix);
                if candidate.exists() {
                    return candidate;
                }
            }
            match dir.parent() {
                Some(p) => dir = p.to_path_buf(),
                None => break,
            }
        }
    }
    
    // 3. Fallback to CWD-relative
    std::env::current_dir()
        .unwrap_or_else(|_| std::path::PathBuf::from("."))
        .join(std::path::Path::new("unreal").join("metadata"))
}

#[cfg(feature = "ue5")]
pub fn generate_usf_header(source: &str, shader_name: &str) -> Result<String, KainError> {
    let stdlib = stdlib::load_stdlib();
    let full_source = format!("{}\n{}", stdlib, source);
    let tokens = Lexer::new(&full_source).tokenize()?;
    let span_mapper = diagnostics::SpanMapper::new(&full_source);
    let mut ast = Parser::new(&tokens, &span_mapper, "<input>").parse()?;
    comptime::eval_program(&mut ast)?;
    let typed_ast = types::check(&ast, &span_mapper, "<input>")?;
    let mono_ast = monomorphize::monomorphize(&typed_ast)?;
    let typed_for_codegen = TypedProgram { items: mono_ast.items };
    Ok(ue5_shaders::generate_cpp_header(&typed_for_codegen, shader_name))
}

#[cfg(feature = "ue5")]
pub fn generate_usf_implementation(source: &str, shader_name: &str, plugin_name: &str) -> Result<String, KainError> {
    let stdlib = stdlib::load_stdlib();
    let full_source = format!("{}\n{}", stdlib, source);
    let tokens = Lexer::new(&full_source).tokenize()?;
    let span_mapper = diagnostics::SpanMapper::new(&full_source);
    let mut ast = Parser::new(&tokens, &span_mapper, "<input>").parse()?;
    comptime::eval_program(&mut ast)?;
    let typed_ast = types::check(&ast, &span_mapper, "<input>")?;
    let mono_ast = monomorphize::monomorphize(&typed_ast)?;
    let typed_for_codegen = TypedProgram { items: mono_ast.items };
    Ok(ue5_shaders::generate_cpp_implementation(&typed_for_codegen, shader_name, plugin_name))
}

#[cfg(feature = "ue5")]
pub fn compile_ue5editor(source: &str, plugin_name: &str, copyright: Option<&str>) -> Result<ue5_editor::Ue5EditorOutput, KainError> {
    let stdlib = stdlib::load_stdlib();
    let full_source = format!("{}\n{}", stdlib, source);
    let tokens = Lexer::new(&full_source).tokenize()?;
    let span_mapper = diagnostics::SpanMapper::new(&full_source);
    let mut ast = Parser::new(&tokens, &span_mapper, "<input>").parse()?;
    comptime::eval_program(&mut ast)?;
    let typed_ast = types::check(&ast, &span_mapper, "<input>")?;
    let mono_ast = monomorphize::monomorphize(&typed_ast)?;
    let typed_for_codegen = TypedProgram { items: mono_ast.items };
    ue5_editor::generate(&typed_for_codegen, plugin_name, copyright)
}
