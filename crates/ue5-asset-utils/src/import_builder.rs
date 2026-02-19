//! Shared import builder for UE5 .uasset generation.
//!
//! Provides deduplicating helpers that create or reuse imports in an asset.
//! Used by every asset writer to avoid duplicate import entries.

use std::io::Cursor;

use unreal_asset::{types::PackageIndex, Asset, Import};

// ─── ImportBuilder ───────────────────────────────────────────────────────────

/// Stateless utility for building and deduplicating imports in an `Asset`.
///
/// All methods are associated functions taking `&mut Asset` — no instance state
/// needed, keeping the API composable.
pub struct ImportBuilder;

impl ImportBuilder {
    /// Find an existing import by its object name string.
    /// Returns `Some(PackageIndex)` (negative) if found, `None` otherwise.
    pub fn find_import_by_name(
        asset: &Asset<Cursor<Vec<u8>>>,
        name: &str,
    ) -> Option<PackageIndex> {
        for (i, imp) in asset.imports.iter().enumerate() {
            let matches = imp.object_name.get_content(|n| n == name);
            if matches {
                return Some(PackageIndex::new(-((i + 1) as i32)));
            }
        }
        None
    }

    /// Get an existing import or create a new one.
    ///
    /// Searches by `object_name`. If found, returns the existing index.
    /// Otherwise pushes a new `Import` and returns its index.
    pub fn get_or_add_import(
        asset: &mut Asset<Cursor<Vec<u8>>>,
        class_package: &str,
        class_name: &str,
        outer: PackageIndex,
        object_name: &str,
    ) -> PackageIndex {
        // Check for existing import with the same object name
        if let Some(existing) = Self::find_import_by_name(asset, object_name) {
            return existing;
        }

        let cp = asset.add_fname(class_package);
        let cn = asset.add_fname(class_name);
        let on = asset.add_fname(object_name);

        asset.imports.push(Import {
            class_package: cp,
            class_name: cn,
            outer_index: outer,
            object_name: on,
            optional: false,
        });

        PackageIndex::new(-(asset.imports.len() as i32))
    }

    /// Add a package import (e.g. "/Script/Engine", "/Script/CoreUObject").
    /// Deduplicates by name.
    pub fn get_or_add_package(
        asset: &mut Asset<Cursor<Vec<u8>>>,
        package_path: &str,
    ) -> PackageIndex {
        Self::get_or_add_import(
            asset,
            "/Script/CoreUObject",
            "Package",
            PackageIndex::new(0),
            package_path,
        )
    }

    /// Add a class import (e.g. "Actor" under "/Script/Engine").
    /// Deduplicates by name.
    pub fn get_or_add_class(
        asset: &mut Asset<Cursor<Vec<u8>>>,
        class_name: &str,
        outer_package: PackageIndex,
    ) -> PackageIndex {
        Self::get_or_add_import(
            asset,
            "/Script/CoreUObject",
            "Class",
            outer_package,
            class_name,
        )
    }

    /// Parse "/Script/Engine.Actor" → ("/Script/Engine", "Actor").
    /// If no dot, defaults to ("/Script/Engine", input).
    pub fn parse_class_path(path: &str) -> (String, String) {
        if let Some(dot_pos) = path.rfind('.') {
            (path[..dot_pos].to_string(), path[dot_pos + 1..].to_string())
        } else {
            ("/Script/Engine".to_string(), path.to_string())
        }
    }

    /// Resolve an object path to an import `PackageIndex`.
    ///
    /// Handles paths like:
    /// - `"/Script/Engine.StaticMesh"` → class import under /Script/Engine
    /// - `"/Game/Meshes/SM_Cube.SM_Cube"` → object import under /Game/... package
    ///
    /// Creates the package import and object import if they don't exist.
    /// Returns `PackageIndex(0)` (null) only if the path is empty.
    pub fn resolve_object_import(
        asset: &mut Asset<Cursor<Vec<u8>>>,
        path: &str,
    ) -> PackageIndex {
        if path.is_empty() {
            return PackageIndex::new(0);
        }

        let (pkg_path, obj_name) = Self::parse_class_path(path);

        // Check if the object import already exists
        if let Some(existing) = Self::find_import_by_name(asset, &obj_name) {
            return existing;
        }

        // Get or create the package import
        let pkg_import = Self::get_or_add_package(asset, &pkg_path);

        // Determine the import class based on path prefix
        let (class_pkg, class_name) = if pkg_path.starts_with("/Script/") {
            ("/Script/CoreUObject", "Class")
        } else {
            ("/Script/CoreUObject", "Object")
        };

        Self::get_or_add_import(asset, class_pkg, class_name, pkg_import, &obj_name)
    }
}

// ─── Tests ───────────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;
    use unreal_asset::engine_version::EngineVersion;

    fn empty_asset() -> Asset<Cursor<Vec<u8>>> {
        Asset::new_empty(EngineVersion::VER_UE5_2)
    }

    #[test]
    fn test_parse_class_path() {
        let (pkg, cls) = ImportBuilder::parse_class_path("/Script/Engine.Actor");
        assert_eq!(pkg, "/Script/Engine");
        assert_eq!(cls, "Actor");
    }

    #[test]
    fn test_parse_class_path_no_dot() {
        let (pkg, cls) = ImportBuilder::parse_class_path("Actor");
        assert_eq!(pkg, "/Script/Engine");
        assert_eq!(cls, "Actor");
    }

    #[test]
    fn test_get_or_add_package_deduplicates() {
        let mut asset = empty_asset();
        let idx1 = ImportBuilder::get_or_add_package(&mut asset, "/Script/Engine");
        let idx2 = ImportBuilder::get_or_add_package(&mut asset, "/Script/Engine");
        assert_eq!(idx1, idx2);
        // Should only have 1 import
        assert_eq!(asset.imports.len(), 1);
    }

    #[test]
    fn test_get_or_add_class_deduplicates() {
        let mut asset = empty_asset();
        let pkg = ImportBuilder::get_or_add_package(&mut asset, "/Script/Engine");
        let cls1 = ImportBuilder::get_or_add_class(&mut asset, "Actor", pkg);
        let cls2 = ImportBuilder::get_or_add_class(&mut asset, "Actor", pkg);
        assert_eq!(cls1, cls2);
        // Package + 1 class = 2 imports
        assert_eq!(asset.imports.len(), 2);
    }

    #[test]
    fn test_resolve_object_import_script_path() {
        let mut asset = empty_asset();
        let idx = ImportBuilder::resolve_object_import(&mut asset, "/Script/Engine.Actor");
        assert!(idx.index < 0); // negative = import
        // Should have created: /Script/Engine (package) + Actor (class)
        assert_eq!(asset.imports.len(), 2);
    }

    #[test]
    fn test_resolve_object_import_game_path() {
        let mut asset = empty_asset();
        let idx = ImportBuilder::resolve_object_import(
            &mut asset,
            "/Game/Meshes/SM_Cube.SM_Cube",
        );
        assert!(idx.index < 0);
        // Package + Object = 2 imports
        assert_eq!(asset.imports.len(), 2);
    }

    #[test]
    fn test_resolve_object_import_empty_path() {
        let mut asset = empty_asset();
        let idx = ImportBuilder::resolve_object_import(&mut asset, "");
        assert_eq!(idx.index, 0); // null
    }

    #[test]
    fn test_resolve_deduplicates_across_calls() {
        let mut asset = empty_asset();
        let idx1 = ImportBuilder::resolve_object_import(&mut asset, "/Script/Engine.Actor");
        let idx2 = ImportBuilder::resolve_object_import(&mut asset, "/Script/Engine.Actor");
        assert_eq!(idx1, idx2);
        assert_eq!(asset.imports.len(), 2); // no duplicates
    }

    #[test]
    fn test_multiple_classes_same_package() {
        let mut asset = empty_asset();
        ImportBuilder::resolve_object_import(&mut asset, "/Script/Engine.Actor");
        ImportBuilder::resolve_object_import(&mut asset, "/Script/Engine.Pawn");
        // 1 package + 2 classes = 3 imports
        assert_eq!(asset.imports.len(), 3);
    }
}
