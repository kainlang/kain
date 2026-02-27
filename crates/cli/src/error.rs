//! CLI error surface.
//!
//! Keep CLI error handling aligned with `kain-core` so all compiler/LSP/packager
//! paths share the same diagnostics and constructors.

pub use kain_core::error::KainError;
pub type KainResult<T> = kain_core::error::KainResult<T>;
