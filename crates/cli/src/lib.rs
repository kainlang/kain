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

/// Compile with backend selection
pub fn compile(source: &str, target: CompileTarget) -> Result<String, KainError> {
    // Load stdlib
    let stdlib = stdlib::load_stdlib();
    let full_source = format!("{}\n{}", stdlib, source);

    // 1. Lex
    let tokens = Lexer::new(&full_source).tokenize()?;
    
    // 2. Parse
    let mut ast = Parser::new(&tokens).parse()?;
    
    // 2.5 Comptime
    comptime::eval_program(&mut ast)?;
    
    // 3. Type check
    let typed_ast = types::check(&ast)?;
    
    // 4. Codegen based on target
    match target {
        #[cfg(feature = "ue5")]
        CompileTarget::Ue5 => {
            let output = ue5::generate(&typed_ast, None, None)?;
            Ok(output.header + "\n" + &output.source)
        }
        
        #[cfg(feature = "ue5")]
        CompileTarget::Usf => {
            ue5_shaders::generate_usf(&typed_ast)
        }
        
        #[cfg(feature = "gpu")]
        CompileTarget::Spirv => {
            gpu::generate_spirv(&typed_ast).map(|bytes| format!("{} bytes", bytes.len()))
        }
        
        #[cfg(feature = "gpu")]
        CompileTarget::Hlsl => {
            gpu::generate_hlsl(&typed_ast)
        }
        
        #[cfg(feature = "web")]
        CompileTarget::Wasm => {
            web::generate_wasm(&typed_ast).map(|bytes| format!("{} bytes", bytes.len()))
        }
        
        #[cfg(feature = "web")]
        CompileTarget::Js => {
            web::generate_js(&typed_ast)
        }
        
        #[cfg(feature = "web")]
        CompileTarget::Hybrid => {
            let output = web::generate_hybrid(&typed_ast)?;
            Ok(output.js)
        }
        
        #[cfg(feature = "sys")]
        CompileTarget::Llvm => {
            sys::generate_llvm(&typed_ast).map(|_| "LLVM IR generated".to_string())
        }
        
        #[cfg(feature = "sys")]
        CompileTarget::Rust => {
            sys::generate_rust(&typed_ast)
        }
        
        #[cfg(feature = "sys")]
        CompileTarget::Cpp => {
            sys::generate_cpp(&typed_ast)
        }
        
        CompileTarget::Interpret | CompileTarget::Test => {
            // Use runtime interpreter
            Err(KainError::runtime("Interpret/Test targets not yet implemented in workspace"))
        }
        
        #[cfg(feature = "ue5")]
        CompileTarget::Ue5Editor => {
            let output = ue5_editor::generate(&typed_ast, "EditorTools", None)?;
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


// Helper functions for main.rs

#[cfg(feature = "ue5")]
pub fn compile_ue5(source: &str, output_name: Option<&str>, copyright: Option<&str>) -> Result<ue5::Ue5Output, KainError> {
    let stdlib = stdlib::load_stdlib();
    let full_source = format!("{}\n{}", stdlib, source);
    let tokens = Lexer::new(&full_source).tokenize()?;
    let mut ast = Parser::new(&tokens).parse()?;
    comptime::eval_program(&mut ast)?;
    let typed_ast = types::check(&ast)?;
    ue5::generate(&typed_ast, output_name, copyright)
}

#[cfg(feature = "ue5")]
pub fn generate_usf_header(source: &str, shader_name: &str) -> Result<String, KainError> {
    let stdlib = stdlib::load_stdlib();
    let full_source = format!("{}\n{}", stdlib, source);
    let tokens = Lexer::new(&full_source).tokenize()?;
    let mut ast = Parser::new(&tokens).parse()?;
    comptime::eval_program(&mut ast)?;
    let typed_ast = types::check(&ast)?;
    Ok(ue5_shaders::generate_cpp_header(&typed_ast, shader_name))
}

#[cfg(feature = "ue5")]
pub fn generate_usf_implementation(source: &str, shader_name: &str, plugin_name: &str) -> Result<String, KainError> {
    let stdlib = stdlib::load_stdlib();
    let full_source = format!("{}\n{}", stdlib, source);
    let tokens = Lexer::new(&full_source).tokenize()?;
    let mut ast = Parser::new(&tokens).parse()?;
    comptime::eval_program(&mut ast)?;
    let typed_ast = types::check(&ast)?;
    Ok(ue5_shaders::generate_cpp_implementation(&typed_ast, shader_name, plugin_name))
}

#[cfg(feature = "ue5")]
pub fn compile_ue5editor(source: &str, plugin_name: &str, copyright: Option<&str>) -> Result<ue5_editor::Ue5EditorOutput, KainError> {
    let stdlib = stdlib::load_stdlib();
    let full_source = format!("{}\n{}", stdlib, source);
    let tokens = Lexer::new(&full_source).tokenize()?;
    let mut ast = Parser::new(&tokens).parse()?;
    comptime::eval_program(&mut ast)?;
    let typed_ast = types::check(&ast)?;
    ue5_editor::generate(&typed_ast, plugin_name, copyright)
}
