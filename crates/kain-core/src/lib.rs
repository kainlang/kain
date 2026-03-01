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
pub mod diagnostic_registry;
pub mod low_level_memory;
pub mod low_level_memory_metadata;
pub mod monomorphize;
pub mod runtime;
pub mod shader_analysis;
pub mod asm_ir;
pub mod language_features;

#[cfg(test)]
mod stdlib_tests;

// Re-exports for convenience
pub use lexer::Lexer;
pub use parser::Parser;
pub use ast::*;
pub use types::*;
pub use effects::*;
pub use error::*;
pub use diagnostic_registry::*;
pub use low_level_memory::*;
pub use low_level_memory_metadata::*;
pub use span::*;
pub use monomorphize::MonomorphizedProgram;
pub use asm_ir::*;
pub use language_features::*;

/// Compilation target
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CompileTarget {
    Wasm,
    Js,
    Ts,
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
    /// KainScript — JS with embedded JSDoc types. Runs natively, fully typed.
    Ks,
}

impl CompileTarget {
    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_lowercase().as_str() {
            "wasm" => Some(Self::Wasm),
            "js" | "javascript" => Some(Self::Js),
            "ts" | "typescript" => Some(Self::Ts),
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
            "ks" | "kainscript" | "kscript" => Some(Self::Ks),
            _ => None,
        }
    }
}

/// Main compilation function
pub fn compile(source: &str, target: CompileTarget) -> Result<String, KainError> {
    // Load stdlib
    let stdlib = stdlib::load_stdlib_for_target(target);
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
    
    // 4. Codegen (handled by backend crates)
    // This is just a placeholder - actual codegen happens in cli/
    Ok(format!("Compiled to {:?}", target))
}
