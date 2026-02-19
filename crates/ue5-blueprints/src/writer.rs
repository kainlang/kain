/// Binary .uasset writer for Blueprint assets.
///
/// Phase 2: Uses `unreal_asset` to write real .uasset binaries that land in the
/// Content folder without ever opening the UE5 editor.
///
/// Architecture:
///   BlueprintDef IR
///       → BlueprintBinaryWriter::write() 
///       → Asset<Cursor<Vec<u8>>>  (unreal_asset)
///       → Vec<u8>  (raw .uasset bytes)
///       → written to Content/Blueprints/BP_*.uasset
///
/// Status: Scaffolded — Phase 2 implementation in progress.
/// Use BlueprintFactoryGenerator (factory.rs) for production output today.

use std::io::Cursor;

use unreal_asset::{
    engine_version::EngineVersion,
    Asset,
};

use crate::{
    error::{BlueprintError, Result},
    ir::{BlueprintDef, BlueprintEngineVersion},
};

pub struct BlueprintBinaryWriter;

impl BlueprintBinaryWriter {
    /// Write a Blueprint .uasset to a byte buffer.
    ///
    /// Returns the raw .uasset bytes ready to be written to disk.
    /// Path: `<project>/Content/<package_path>/<name>.uasset`
    pub fn write(bp: &BlueprintDef) -> Result<Vec<u8>> {
        let engine_version = map_engine_version(bp.engine_version);

        // Phase 2 implementation:
        // 1. Bootstrap empty asset
        // 2. Build name table (all FNames needed)
        // 3. Add imports (engine classes: UBlueprint, UBlueprintGeneratedClass,
        //    USimpleConstructionScript, parent class, component classes)
        // 4. Add exports (UBlueprint, UBlueprintGeneratedClass, CDO, SCS nodes)
        // 5. Serialize CDO tagged properties
        // 6. Serialize Kismet event graph bytecode (unreal_asset_kismet)
        // 7. Call asset.write() → bytes

        // For now: return informative error pointing to factory fallback
        Err(BlueprintError::AssetWrite(format!(
            "Binary .uasset writer for '{}' is Phase 2 — use BlueprintFactoryGenerator for now. \
             Engine version target: {:?}",
            bp.name, engine_version
        )))
    }

    /// Check if the binary writer can handle a given blueprint definition.
    /// Returns Ok(()) if supported, Err with explanation if not yet implemented.
    pub fn check_support(bp: &BlueprintDef) -> Result<()> {
        // Currently: simple Blueprints with no event graph are most feasible first
        if !bp.event_graph.is_empty() {
            return Err(BlueprintError::UnsupportedNode(
                "Event graph nodes require Phase 2 Kismet bytecode writer".into(),
            ));
        }
        Ok(())
    }
}

/// Map KAIN's engine version enum to unreal_asset's EngineVersion.
///
/// NOTE: `unreal_asset_base` currently defines variants only up to VER_UE5_2
/// (library last updated late 2024). UE5.3, 5.4, 5.5 will need new variants
/// added to `crates/unreal/unreal_asset_base/src/engine_version.rs` once we
/// patch in the new ObjectVersionUE5 constants from UE5's ObjectVersion.h.
/// Until then, all versions >= 5.2 fall back to the highest known variant.
fn map_engine_version(v: BlueprintEngineVersion) -> EngineVersion {
    match v {
        BlueprintEngineVersion::Ue5_1 => EngineVersion::VER_UE5_1,
        // 5.2 is the highest variant currently in the vendored library.
        // TODO: patch engine_version.rs with VER_UE5_3/4/5 when ready.
        BlueprintEngineVersion::Ue5_2
        | BlueprintEngineVersion::Ue5_3
        | BlueprintEngineVersion::Ue5_4
        | BlueprintEngineVersion::Ue5_5 => EngineVersion::VER_UE5_2,
    }
}

/// Future helper: bootstrap an empty writable Asset from scratch.
/// This is the key unlock that makes binary writing feasible.
#[allow(dead_code)]
fn bootstrap_empty_asset(engine_version: EngineVersion) -> Asset<Cursor<Vec<u8>>> {
    // An empty cursor — Asset::new() will fail to parse it (expected),
    // but we can then populate the internal data structures before writing.
    // TODO: either patch unreal_asset to expose Asset::new_empty(),
    // or load a minimal template .uasset byte slice as seed.
    todo!("Phase 2: implement empty Asset bootstrap")
}
