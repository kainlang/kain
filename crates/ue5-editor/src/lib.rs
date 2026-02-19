//! KAIN UE5 Editor Code Generator
//! 
//! Generates Unreal Engine 5 Editor-specific code:
//! - Slate UI widgets with smart slot awareness
//! - Custom viewports with preview rendering
//! - Detail customizations with metadata-driven layouts
//! - Asset types and factories
//! - Editor modules with toolbar/menu registration

pub mod editor;
pub mod data_asset_writer;

pub use editor::*;
