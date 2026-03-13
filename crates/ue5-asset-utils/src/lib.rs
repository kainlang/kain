//! Shared utilities for UE5 `.uasset` generation.
//!
//! This crate provides the common building blocks used by multiple asset writers
//! (`ue5-blueprints`, `ue5-materials`, future `ue5-datatables`, etc.):
//!
//! - **`engine_target`** — [`KainEngineTarget`]: single version authority (UE5.0–5.7)
//! - **`property_types`** — `PropertyDef` / `PropertyValue` IR types
//! - **`property_converter`** — Converts `PropertyDef` → unreal_asset `Property`
//! - **`import_builder`** — Deduplicating import creation helpers

pub mod engine_target;
pub mod import_builder;
pub mod property_converter;
pub mod property_types;

// Re-export the most commonly used types at crate root
pub use engine_target::KainEngineTarget;
pub use import_builder::ImportBuilder;
pub use property_converter::convert_property_def;
pub use property_types::{PropertyDef, PropertyValue};
