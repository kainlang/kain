//! Shared utilities for UE5 `.uasset` generation.
//!
//! This crate provides the common building blocks used by multiple asset writers
//! (`ue5-blueprints`, `ue5-materials`, future `ue5-datatables`, etc.):
//!
//! - **`property_types`** — `PropertyDef` / `PropertyValue` IR types
//! - **`property_converter`** — Converts `PropertyDef` → unreal_asset `Property`
//! - **`import_builder`** — Deduplicating import creation helpers

pub mod property_types;
pub mod property_converter;
pub mod import_builder;

// Re-export the most commonly used types at crate root
pub use property_types::{PropertyDef, PropertyValue};
pub use property_converter::convert_property_def;
pub use import_builder::ImportBuilder;
