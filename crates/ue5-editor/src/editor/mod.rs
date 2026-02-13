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

pub mod codegen;
pub mod slate;
pub mod viewport;
pub mod details;
pub mod assets;
pub mod style;
pub mod reactive;

pub use codegen::{generate, generate_with_context, generate_per_item, Ue5EditorOutput, EditorItem, is_editor_attribute, EDITOR_ATTRIBUTES};
