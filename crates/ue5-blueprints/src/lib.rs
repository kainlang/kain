//! # ue5-blueprints
//!
//! KAIN → UE5 Blueprint `.uasset` generation pipeline.
//!
//! ## Architecture
//!
//! ```text
//! KAIN .kn source
//!     → BlueprintDef (IR)         ← data-driven, serialize/deserialize
//!         → BlueprintFactoryGenerator  → C++ factory code (Phase 1, works now)
//!         → BlueprintBinaryWriter      → .uasset bytes   (Phase 2, in progress)
//! ```
//!
//! ## Example
//!
//! ```rust,ignore
//! use ue5_blueprints::{
//!     ir::{BlueprintDef, ComponentDef, PropertyDef, EventGraphNode, KismetCall},
//!     factory::BlueprintFactoryGenerator,
//! };
//!
//! let bp = BlueprintDef::new(
//!     "BP_Player",
//!     "/Game/MyPlugin/Blueprints",
//!     "/Script/MyPlugin.APlayerBase",
//! )
//! .add_component(
//!     ComponentDef::new("StaticMeshComponent", "Mesh")
//!         .with_default(PropertyDef::soft_object(
//!             "StaticMesh",
//!             "/Game/Meshes/SM_Player.SM_Player",
//!         ))
//!         .with_default(PropertyDef::bool("bCastShadow", true)),
//! )
//! .add_default(PropertyDef::float("MaxWalkSpeed", 600.0))
//! .add_event(EventGraphNode::begin_play(vec![
//!     KismetCall::function("InitializeAbilitySystem"),
//!     KismetCall::function("SetupHUD"),
//! ]));
//!
//! let header = BlueprintFactoryGenerator::generate_header(&bp);
//! let source = BlueprintFactoryGenerator::generate_source(&bp);
//! ```

pub mod conversion;
pub mod error;
pub mod factory;
pub mod ir;
pub mod kismet;
pub mod writer;

// Re-export the most common types at crate root
pub use error::{BlueprintError, Result};
pub use factory::BlueprintFactoryGenerator;
pub use ir::{
    BlueprintDef, BlueprintEngineVersion, ComponentDef, EventGraphNode, KismetCall, PropertyDef,
    PropertyValue,
};
pub use writer::BlueprintBinaryWriter;

/// Convenience: generate both header and source for a blueprint.
/// Returns `(header_content, source_content)`.
pub fn generate_factory(bp: &BlueprintDef) -> (String, String) {
    (
        BlueprintFactoryGenerator::generate_header(bp),
        BlueprintFactoryGenerator::generate_source(bp),
    )
}

/// Convenience: attempt binary .uasset generation, fall back to factory
/// if binary writer is not yet supported for this blueprint.
///
/// Returns:
///   - `Ok(Some(bytes))` — binary .uasset generated successfully
///   - `Ok(None)`        — binary writer not supported, use factory fallback
///   - `Err(e)`          — hard error
pub fn generate_uasset(bp: &BlueprintDef) -> Result<Option<Vec<u8>>> {
    match BlueprintBinaryWriter::check_support(bp) {
        Ok(_) => BlueprintBinaryWriter::write(bp).map(Some),
        Err(_) => Ok(None), // graceful fallback to factory
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ir::{ComponentDef, EventGraphNode, KismetCall, PropertyDef};
    #[allow(unused_imports)]
    use crate::{BlueprintBinaryWriter, BlueprintFactoryGenerator};

    fn sample_blueprint() -> BlueprintDef {
        BlueprintDef::new(
            "BP_TestPlayer",
            "/Game/Tests/Blueprints",
            "/Script/MyPlugin.APlayerBase",
        )
        .add_component(
            ComponentDef::new("CapsuleComponent", "Capsule")
                .with_default(PropertyDef::float("CapsuleRadius", 42.0))
                .with_default(PropertyDef::float("CapsuleHalfHeight", 96.0)),
        )
        .add_component(
            ComponentDef::new("SkeletalMeshComponent", "Mesh")
                .with_parent("Capsule")
                .with_default(PropertyDef::soft_object(
                    "SkeletalMesh",
                    "/Game/Characters/SK_TestPlayer.SK_TestPlayer",
                )),
        )
        .add_default(PropertyDef::float("MaxWalkSpeed", 600.0))
        .add_default(PropertyDef::bool("bCanCrouch", true))
        .add_event(EventGraphNode::begin_play(vec![
            KismetCall::function("InitializeAbilitySystem"),
            KismetCall::function("SetupHUD"),
        ]))
    }

    #[test]
    fn test_factory_header_contains_class_name() {
        let bp = sample_blueprint();
        let header = BlueprintFactoryGenerator::generate_header(&bp);
        assert!(header.contains("FBP_TestPlayerFactory"));
        assert!(header.contains("BP_TestPlayer"));
        assert!(header.contains("/Script/MyPlugin.APlayerBase"));
    }

    #[test]
    fn test_factory_source_contains_package_path() {
        let bp = sample_blueprint();
        let source = BlueprintFactoryGenerator::generate_source(&bp);
        assert!(source.contains("/Game/Tests/Blueprints"));
        assert!(source.contains("BP_TestPlayer"));
    }

    #[test]
    fn test_factory_source_contains_components() {
        let bp = sample_blueprint();
        let source = BlueprintFactoryGenerator::generate_source(&bp);
        assert!(source.contains("CapsuleComponent"));
        assert!(source.contains("SkeletalMeshComponent"));
        assert!(source.contains("CapsuleRadius"));
    }

    #[test]
    fn test_factory_source_contains_event_graph() {
        let bp = sample_blueprint();
        let source = BlueprintFactoryGenerator::generate_source(&bp);
        assert!(source.contains("ReceiveBeginPlay"));
        assert!(source.contains("InitializeAbilitySystem"));
        assert!(source.contains("SetupHUD"));
    }

    #[test]
    fn test_asset_path_generation() {
        let bp = sample_blueprint();
        assert_eq!(bp.asset_path(), "/Game/Tests/Blueprints/BP_TestPlayer");
        assert_eq!(
            bp.generated_class_path(),
            "/Game/Tests/Blueprints/BP_TestPlayer.BP_TestPlayer_C"
        );
    }

    #[test]
    fn test_ir_round_trips_json() {
        let bp = sample_blueprint();
        let json = serde_json::to_string(&bp).expect("serialize");
        let back: BlueprintDef = serde_json::from_str(&json).expect("deserialize");
        assert_eq!(bp.name, back.name);
        assert_eq!(bp.components.len(), back.components.len());
        assert_eq!(bp.event_graph.len(), back.event_graph.len());
    }

    #[test]
    fn test_binary_writer_handles_event_graph() {
        let bp = sample_blueprint(); // has event graph → now fully supported
        let result = generate_uasset(&bp);
        assert!(result.is_ok());
        assert!(result.unwrap().is_some()); // binary generation succeeds!
    }
}
