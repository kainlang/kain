//! KAIN Editor Module - UE5 Editor Tools Code Generation
//!
//! This module handles generation of UE5 editor-specific code:
//! - Slate UI widgets with smart slot awareness
//! - Custom viewports with preview rendering
//! - Detail customizations with metadata-driven layouts
//! - Asset types and factories
//! - Editor modules with toolbar/menu registration
//! - Sequencer tracks and sections
//! - Property editors
//! - Editor modes
//!
//! Architecture:
//! - `codegen.rs` - Main code generation orchestration
//! - `slate.rs` - Slate widget generation with hierarchy tracking
//! - `viewport.rs` - Custom viewport generation
//! - `details.rs` - Detail customization generation
//! - `assets.rs` - Asset type and factory generation
//! - `style.rs` - Slate style management
//! - `reactive.rs` - Layout optimizer (SLATE_ATTRIBUTE vs SLATE_ARGUMENT)

pub mod asset_editor_ir;
pub mod assets;
pub mod codegen;
pub mod details;
pub mod editor_module_codegen;
pub mod editor_module_ir;
pub mod reactive;
pub mod slate;
pub mod style;
pub mod viewport;

pub use codegen::{
    generate, generate_per_item, generate_with_context, get_editor_attributes, is_editor_attribute,
    EditorItem, Ue5EditorOutput,
};
