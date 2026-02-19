//! Asset Registry Writer
//!
//! Appends generated asset information to `AssetRegistry.bin` for immediate
//! Content Browser visibility in the Unreal Editor. If no registry file exists,
//! creates one from scratch.
//!
//! # Design
//!
//! * Uses `AddedDependencyFlags` version (pre-FixedTags) for maximum
//!   compatibility and self-contained name-tables.
//! * All FNames are `Backed` against a shared `NameMap`.
//! * Duplicate assets (same `object_path`) are detected and skipped.
//! * Registry write failures are **non-fatal** — logged and swallowed.

#[cfg(feature = "ue5")]
use std::io::Cursor;
#[cfg(feature = "ue5")]
use std::path::Path;

#[cfg(feature = "ue5")]
use unreal_asset_base::{
    containers::{IndexedMap, NameMap, SharedResource},
    custom_version::FAssetRegistryVersionType,
    engine_version,
    flags::EPackageFlags,
    types::FName,
};

#[cfg(feature = "ue5")]
use unreal_asset_registry::{
    objects::{
        asset_data::{AssetData, TopLevelAssetPath},
        asset_package_data::AssetPackageData,
    },
    AssetRegistryState,
};

// ─── Data-driven defaults ────────────────────────────────────────────────────

/// Target registry format version.
/// `AddedDependencyFlags` is pre-FixedTags, uses a self-contained name table,
/// and is compatible with UE 4.27 / 5.0+.
#[cfg(feature = "ue5")]
const REGISTRY_VERSION: FAssetRegistryVersionType =
    FAssetRegistryVersionType::AddedDependencyFlags;

// ─── Asset descriptor ────────────────────────────────────────────────────────

/// Describes a single generated asset for registry insertion.
///
/// This is the data-driven input — callers populate this struct for each
/// generated `.uasset` and pass it to [`register_assets`].
#[derive(Debug, Clone)]
pub struct AssetEntry {
    /// Full object path, e.g. `/Game/Blueprints/BP_Enemy.BP_Enemy`
    pub object_path: String,
    /// Package path (directory portion), e.g. `/Game/Blueprints`
    pub package_path: String,
    /// Package name, e.g. `/Game/Blueprints/BP_Enemy`
    pub package_name: String,
    /// Asset name (short), e.g. `BP_Enemy`
    pub asset_name: String,
    /// Full class path, e.g. `/Script/Engine.Blueprint`
    pub asset_class: String,
}

impl AssetEntry {
    /// Create a new asset entry.
    ///
    /// Convenience constructor that derives `package_path` and `object_path`
    /// from `package_name` and `asset_name`.
    pub fn new(package_name: &str, asset_name: &str, asset_class: &str) -> Self {
        // /Game/Blueprints/BP_Enemy → /Game/Blueprints
        let package_path = package_name
            .rsplit_once('/')
            .map(|(p, _)| p.to_string())
            .unwrap_or_else(|| "/Game".to_string());

        // /Game/Blueprints/BP_Enemy.BP_Enemy
        let object_path = format!("{}.{}", package_name, asset_name);

        Self {
            object_path,
            package_path,
            package_name: package_name.to_string(),
            asset_name: asset_name.to_string(),
            asset_class: asset_class.to_string(),
        }
    }

    // ─── Common asset class constructors ─────────────────────────────────

    /// Create an entry for a Blueprint asset.
    pub fn blueprint(package_name: &str, asset_name: &str) -> Self {
        Self::new(package_name, asset_name, "/Script/Engine.Blueprint")
    }

    /// Create an entry for a Material asset.
    pub fn material(package_name: &str, asset_name: &str) -> Self {
        Self::new(package_name, asset_name, "/Script/Engine.Material")
    }

    /// Create an entry for a DataAsset.
    pub fn data_asset(package_name: &str, asset_name: &str) -> Self {
        Self::new(package_name, asset_name, "/Script/Engine.DataAsset")
    }

    /// Create an entry for a custom asset class.
    pub fn custom(package_name: &str, asset_name: &str, class_path: &str) -> Self {
        Self::new(package_name, asset_name, class_path)
    }
}

// ─── Public API ──────────────────────────────────────────────────────────────

/// Write (or update) an `AssetRegistry.bin` file with the given asset entries.
///
/// # Behaviour
///
/// 1. If `registry_path` exists, reads the existing registry and appends
///    new entries (deduplicating by `object_path`).
/// 2. If the file doesn't exist, creates a fresh registry from scratch.
/// 3. Writes the result back to `registry_path`.
///
/// # Errors
///
/// Returns `Err` on I/O or serialization failure. The caller is expected
/// to treat these as **non-fatal** (log and continue).
#[cfg(feature = "ue5")]
pub fn register_assets(
    registry_path: &Path,
    entries: &[AssetEntry],
    engine_version: unreal_asset_base::engine_version::EngineVersion,
) -> Result<(), String> {
    if entries.is_empty() {
        return Ok(());
    }

    let (object_version, object_version_ue5) =
        engine_version::get_object_versions(engine_version);

    // ── Try to load existing registry ───────────────────────────────────
    let mut registry = if registry_path.exists() {
        load_existing_registry(registry_path, engine_version)?
    } else {
        create_empty_registry(object_version, object_version_ue5)
    };

    // ── Resolve the name map ────────────────────────────────────────────
    let name_map = registry
        .name_map()
        .cloned()
        .unwrap_or_else(NameMap::new);

    // ── Collect existing object paths for dedup ─────────────────────────
    let existing_paths: std::collections::HashSet<String> = registry
        .assets_data
        .iter()
        .map(|ad| ad.object_path.get_owned_content())
        .collect();

    // ── Add new entries ─────────────────────────────────────────────────
    let mut added = 0usize;
    for entry in entries {
        if existing_paths.contains(&entry.object_path) {
            continue;
        }

        let asset_data = build_asset_data(&name_map, entry, REGISTRY_VERSION);
        registry.assets_data.push(asset_data);

        // Matching PackageData entry
        let pkg_data = build_package_data(&name_map, entry, REGISTRY_VERSION);
        registry.package_data.push(pkg_data);

        added += 1;
    }

    if added == 0 {
        return Ok(()); // nothing new to write
    }

    // ── Serialize ───────────────────────────────────────────────────────
    let mut cursor = Cursor::new(Vec::new());
    registry
        .write(&mut cursor)
        .map_err(|e| format!("Failed to write AssetRegistry: {}", e))?;

    // ── Write to disk ───────────────────────────────────────────────────
    if let Some(parent) = registry_path.parent() {
        std::fs::create_dir_all(parent)
            .map_err(|e| format!("Failed to create registry directory: {}", e))?;
    }
    std::fs::write(registry_path, cursor.into_inner())
        .map_err(|e| format!("Failed to write AssetRegistry.bin: {}", e))?;

    Ok(())
}

// ─── Internal helpers ────────────────────────────────────────────────────────

/// Load an existing `AssetRegistry.bin` file.
#[cfg(feature = "ue5")]
fn load_existing_registry(
    path: &Path,
    engine_version: unreal_asset_base::engine_version::EngineVersion,
) -> Result<AssetRegistryState, String> {
    use unreal_asset_base::{
        containers::Chain,
        reader::RawReader,
    };

    let data = std::fs::read(path)
        .map_err(|e| format!("Failed to read AssetRegistry.bin: {}", e))?;

    let cursor = Cursor::new(data);
    let (ov, ov5) = engine_version::get_object_versions(engine_version);
    let name_map = NameMap::new();
    let mut reader = RawReader::new(Chain::new(cursor, None), ov, ov5, false, name_map);

    AssetRegistryState::new(&mut reader)
        .map_err(|e| format!("Failed to parse AssetRegistry.bin: {}", e))
}

/// Create an empty registry state targeting `AddedDependencyFlags` version.
#[cfg(feature = "ue5")]
fn create_empty_registry(
    object_version: unreal_asset_base::object_version::ObjectVersion,
    object_version_ue5: unreal_asset_base::object_version::ObjectVersionUE5,
) -> AssetRegistryState {
    let name_map = NameMap::new();
    AssetRegistryState::from_data(
        REGISTRY_VERSION,
        object_version,
        object_version_ue5,
        Some(name_map),
        Vec::new(),
        Vec::new(),
        Vec::new(),
    )
}

/// Build an `AssetData` entry from an `AssetEntry` descriptor.
#[cfg(feature = "ue5")]
fn build_asset_data(
    name_map: &SharedResource<NameMap>,
    entry: &AssetEntry,
    version: FAssetRegistryVersionType,
) -> AssetData {
    let object_path = make_fname(name_map, &entry.object_path);
    let package_name = make_fname(name_map, &entry.package_name);
    let package_path = make_fname(name_map, &entry.package_path);
    let asset_name = make_fname(name_map, &entry.asset_name);

    // For versions >= ClassPaths, use TopLevelAssetPath; otherwise asset_class FName
    let (asset_class, asset_path) = if version >= FAssetRegistryVersionType::ClassPaths {
        // Parse "/Script/Engine.Blueprint" → package="/Script/Engine", asset="Blueprint"
        let (pkg, cls) = entry
            .asset_class
            .rsplit_once('.')
            .unwrap_or(("/Script/Engine", &entry.asset_class));
        let path = TopLevelAssetPath {
            package_name: make_fname(name_map, pkg),
            asset_name: make_fname(name_map, cls),
        };
        (None, Some(path))
    } else {
        (Some(make_fname(name_map, &entry.asset_class)), None)
    };

    AssetData::from_data(
        object_path,
        package_name,
        package_path,
        asset_name,
        asset_class,
        asset_path,
        IndexedMap::new(), // no tags
        Default::default(), // no bundles
        Vec::new(),         // no chunk ids
        EPackageFlags::PKG_NONE,
        version,
    )
}

/// Build a matching `AssetPackageData` entry.
#[cfg(feature = "ue5")]
fn build_package_data(
    name_map: &SharedResource<NameMap>,
    entry: &AssetEntry,
    version: FAssetRegistryVersionType,
) -> AssetPackageData {
    use unreal_asset_registry::objects::md5_hash::FMD5Hash;

    AssetPackageData::from_data(
        make_fname(name_map, &entry.package_name),
        unreal_asset_base::Guid::default(),
        Some(FMD5Hash { hash: None }), // empty cooked hash (required for versions >= AddedCookedMD5Hash)
        None,  // imported_classes
        0,     // disk_size
        0,     // file_version
        None,  // ue5_version
        -1,    // file_version_licensee_ue
        None,  // custom_versions
        0,     // flags
        version,
    )
}

/// Create a `Backed` FName by adding the string to the shared name map.
///
/// Uses `Deref` to access the interior `RefCell` since we only
/// have an immutable reference to the `SharedResource`.
#[cfg(feature = "ue5")]
fn make_fname(name_map: &SharedResource<NameMap>, value: &str) -> FName {
    name_map.borrow_mut().add_fname(value)
}

// ─── Tests ───────────────────────────────────────────────────────────────────

#[cfg(all(test, feature = "ue5"))]
mod tests {
    use super::*;
    use std::path::PathBuf;

    #[test]
    fn test_asset_entry_new() {
        let entry = AssetEntry::new(
            "/Game/Blueprints/BP_Enemy",
            "BP_Enemy",
            "/Script/Engine.Blueprint",
        );
        assert_eq!(entry.object_path, "/Game/Blueprints/BP_Enemy.BP_Enemy");
        assert_eq!(entry.package_path, "/Game/Blueprints");
        assert_eq!(entry.package_name, "/Game/Blueprints/BP_Enemy");
        assert_eq!(entry.asset_name, "BP_Enemy");
        assert_eq!(entry.asset_class, "/Script/Engine.Blueprint");
    }

    #[test]
    fn test_asset_entry_convenience() {
        let bp = AssetEntry::blueprint("/Game/BP/BP_Player", "BP_Player");
        assert_eq!(bp.asset_class, "/Script/Engine.Blueprint");

        let mat = AssetEntry::material("/Game/Materials/M_Base", "M_Base");
        assert_eq!(mat.asset_class, "/Script/Engine.Material");

        let da = AssetEntry::data_asset("/Game/Data/DA_Items", "DA_Items");
        assert_eq!(da.asset_class, "/Script/Engine.DataAsset");
    }

    #[test]
    fn test_create_empty_registry() {
        let (ov, ov5) = engine_version::get_object_versions(
            unreal_asset_base::engine_version::EngineVersion::VER_UE5_2,
        );
        let registry = create_empty_registry(ov, ov5);
        assert!(registry.assets_data.is_empty());
        assert!(registry.package_data.is_empty());
        assert!(registry.depends_nodes.is_empty());
    }

    #[test]
    fn test_register_assets_creates_file() {
        let dir = std::env::temp_dir().join("kain_registry_test");
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(&dir).unwrap();
        let registry_path = dir.join("AssetRegistry.bin");

        let entries = vec![
            AssetEntry::blueprint("/Game/Blueprints/BP_Test", "BP_Test"),
            AssetEntry::material("/Game/Materials/M_Test", "M_Test"),
            AssetEntry::data_asset("/Game/Data/DA_Test", "DA_Test"),
        ];

        let result = register_assets(
            &registry_path,
            &entries,
            unreal_asset_base::engine_version::EngineVersion::VER_UE5_2,
        );
        assert!(result.is_ok(), "register_assets failed: {:?}", result);
        assert!(registry_path.exists());
        assert!(
            std::fs::metadata(&registry_path).unwrap().len() > 20,
            "registry file too small"
        );

        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn test_register_assets_dedup() {
        let dir = std::env::temp_dir().join("kain_registry_dedup_test");
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(&dir).unwrap();
        let registry_path = dir.join("AssetRegistry.bin");

        let entries = vec![
            AssetEntry::blueprint("/Game/BP/BP_A", "BP_A"),
        ];

        // First write
        register_assets(
            &registry_path,
            &entries,
            unreal_asset_base::engine_version::EngineVersion::VER_UE5_2,
        )
        .unwrap();

        let size_1 = std::fs::metadata(&registry_path).unwrap().len();

        // Second write with same entry — should not grow
        register_assets(
            &registry_path,
            &entries,
            unreal_asset_base::engine_version::EngineVersion::VER_UE5_2,
        )
        .unwrap();

        let size_2 = std::fs::metadata(&registry_path).unwrap().len();
        assert_eq!(size_1, size_2, "Registry grew despite duplicate entries");

        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn test_register_assets_empty_is_noop() {
        let path = PathBuf::from("/nonexistent/should_not_be_created.bin");
        let result = register_assets(
            &path,
            &[],
            unreal_asset_base::engine_version::EngineVersion::VER_UE5_2,
        );
        assert!(result.is_ok());
        assert!(!path.exists());
    }
}
