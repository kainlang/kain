//! Typed diagnostic builders — one per error category.
//!
//! Re-exported from `kain-error` so that downstream crates (including GPU,
//! codegen, runtime, and shader pipelines) can build strongly-typed
//! diagnostics without depending on `kain-error` directly.

pub use kain_error::builder::*;
