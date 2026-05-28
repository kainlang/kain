//! Kain Error — data-driven diagnostic engine with legacy drop-in facade.
//!
//! The scratch crate keeps the richer report/registry/render/json layers,
//! but it now also preserves the historical `kain_error::error::*` and
//! `kain_error::diagnostic_registry::*` surfaces so `kain-core` can swap
//! onto it without a repo-wide edit storm.

pub mod builder;
pub mod chain;
pub mod code;
pub mod diagnostic_registry;
pub mod error;
pub mod explain;
pub mod json;
pub mod label;
pub mod registry;
pub mod render;
pub mod report;
pub mod severity;
pub mod source;
pub mod span;
pub mod spec;
pub mod trace;

// Legacy-compatible module paths still exist under `error` and
// `diagnostic_registry`, but the crate root stays scratch-native.
pub use builder::*;
pub use chain::*;
pub use code::DiagnosticCode;
pub use error::*;
pub use explain::*;
pub use json::*;
pub use registry::{registry, spec_for_code};
pub use render::*;
pub use source::*;
pub use span::*;
pub use spec::DiagnosticSpec;
pub use trace::*;
