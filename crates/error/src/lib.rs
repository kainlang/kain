//! Shared Kain diagnostics, spans, and error data models.

pub mod diagnostic_registry;
pub mod error;
pub mod source;
pub mod span;

pub use diagnostic_registry::*;
pub use error::*;
pub use source::*;
pub use span::*;
