//! # KAIN Core Compiler
//! 
//! Frontend, type system, and runtime for the KAIN programming language.

// Core modules
pub mod lexer;
pub mod ast;
pub mod parser;
pub mod types;
pub mod effects;
pub mod stdlib;
pub mod error;
pub mod span;
pub mod comptime;
pub mod diagnostics;
pub mod monomorphize;
pub mod runtime;
pub mod shader_analysis;

// Re-exports for convenience
pub use lexer::Lexer;
pub use parser::Parser;
pub use ast::*;
pub use types::*;
pub use effects::*;
pub use error::*;
pub use span::*;
pub use monomorphize::MonomorphizedProgram;

/// Compilation target
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CompileTarget {
    Wasm,
    Js,
    Hybrid,
    Llvm,
    Rust,
    Cpp,
    Ue5,
    Ue5Editor,
    Usf,
    Spirv,
    Hlsl,
    Interpret,
    Test,
}

impl CompileTarget {
    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_lowercase().as_str() {
            "wasm" => Some(Self::Wasm),
            "js" | "javascript" => Some(Self::Js),
            "hybrid" => Some(Self::Hybrid),
            "llvm" => Some(Self::Llvm),
            "rust" | "rs" => Some(Self::Rust),
            "cpp" | "c++" => Some(Self::Cpp),
            "ue5" | "unreal" => Some(Self::Ue5),
            "ue5-editor" | "editor" => Some(Self::Ue5Editor),
            "usf" | "shader" => Some(Self::Usf),
            "spirv" | "spv" => Some(Self::Spirv),
            "hlsl" => Some(Self::Hlsl),
            "interpret" | "run" => Some(Self::Interpret),
            "test" => Some(Self::Test),
            _ => None,
        }
    }
}

/// Main compilation function
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
    
    // 4. Codegen (handled by backend crates)
    // This is just a placeholder - actual codegen happens in cli/
    Ok(format!("Compiled to {:?}", target))
}
