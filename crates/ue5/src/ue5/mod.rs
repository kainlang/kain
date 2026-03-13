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

pub mod context;
pub mod editor_attributes;
pub mod engine_knowledge;
pub mod kain_markers;
pub mod logging;
pub mod metadata_hotreload;
pub mod metadata_validation;
pub mod module_graph;
pub mod naming;
pub mod oracle;
pub mod project;
pub mod resolver;
pub mod stdlib_resolver;
pub mod syntax;
pub mod templates;
pub mod traits;
pub mod types;
pub mod uht_rules;
pub mod validation_rules;
pub mod virtual_obligations;
pub mod widget_registry;

// Re-export commonly used items
pub use context::*;
pub use editor_attributes::EditorAttributesRegistry;
pub use engine_knowledge::EngineKnowledge;
pub use kain_markers::{MarkerConfig, MarkerStyle};
pub use logging::*;
pub use metadata_hotreload::{HotReloadManager, MetadataWatcher};
pub use metadata_validation::{MetadataValidator, ValidationError, ValidationResult};
pub use module_graph::ModuleGraph;
pub use naming::*;
pub use oracle::{validate_program, validate_program_with_knowledge};
pub use project::*;
pub use resolver::*;
pub use stdlib_resolver::StdLibResolver;
pub use syntax::*;
pub use templates::TEMPLATES;
pub use traits::*;
pub use types::*;
pub use uht_rules::UhtRules;
pub use validation_rules::{
    RuleCategory, RuleCondition, Severity, ValidationRule, ValidationRules,
};
pub use virtual_obligations::VirtualObligations;
