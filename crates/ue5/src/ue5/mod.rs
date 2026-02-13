//! UE5 Runtime Support Library
//! 
//! This module provides the core infrastructure for all Unreal Engine 5 code generation.
//! It serves as the "Single Source of Truth" for naming conventions, type mappings,
//! macro generation, and project configuration.
//!
//! ## Architecture
//! 
//! - `naming`: Centralized naming conventions (PascalCase, UE5 prefixes, etc.)
//! - `types`: Type mapping from KAIN to UE5 C++ types
//! - `logging`: Smart UE_LOG generation with proper format specifiers
//! - `syntax`: Macro builders for UFUNCTION, UPROPERTY, UCLASS, etc.
//! - `project`: .uplugin, .uproject, and .Build.cs generation
//! - `context`: Shared state and symbol table for cross-module intelligence
//! - `traits`: Common traits for UE5 code generation

pub mod naming;
pub mod types;
pub mod logging;
pub mod syntax;
pub mod project;
pub mod context;
pub mod traits;
pub mod resolver;
pub mod templates;
pub mod oracle;
pub mod engine_knowledge;
pub mod widget_registry;

// Re-export commonly used items
pub use naming::*;
pub use types::*;
pub use logging::*;
pub use syntax::*;
pub use project::*;
pub use context::*;
pub use traits::*;
pub use resolver::*;
pub use templates::TEMPLATES;
pub use oracle::{validate_program, validate_program_with_knowledge};
pub use engine_knowledge::EngineKnowledge;
