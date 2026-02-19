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
pub mod editor_attributes;
pub mod uht_rules;
pub mod module_graph;
pub mod virtual_obligations;
pub mod metadata_validation;
pub mod metadata_hotreload;
pub mod validation_rules;

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
pub use editor_attributes::EditorAttributesRegistry;
pub use uht_rules::UhtRules;
pub use module_graph::ModuleGraph;
pub use virtual_obligations::VirtualObligations;
pub use metadata_validation::{MetadataValidator, ValidationError, ValidationResult};
pub use metadata_hotreload::{MetadataWatcher, HotReloadManager};
pub use validation_rules::{ValidationRules, ValidationRule, RuleCategory, Severity, RuleCondition};
