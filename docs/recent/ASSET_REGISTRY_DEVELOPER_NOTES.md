# Unreal Asset Registry — Developer Notes

> **Date:** February 19, 2026  
> These are hard-won lessons from implementing the Asset Registry writer in KAIN.  
> Save yourself the debugging — read this first.

---

## Registry Version Reference

| Version | Value | Key additions |
|---------|-------|---------------|
| `PreVersioning` | 0 | Baseline |
| `HardSoftDependencies` | 2 | Soft/hard dep flags |
| `AddedAssetBundles` | 4 | Bundle data |
| `AddedCookedMD5Hash` | 6 | `cooked_hash` field on PackageData |
| **`AddedDependencyFlags`** | **7** | **← Use this. Pre-FixedTags. Self-contained name table.** |
| `FixedTags` | 8 | Global name table (complex to write) |
| `WorkspaceDomain` | 9 | |
| `PackageImportedClasses` | 10 | `imported_classes` on PackageData |
| `ClassPaths` | 11 | `TopLevelAssetPath` replaces `asset_class FName` |

**Recommendation:** Target `AddedDependencyFlags` for generated registries. It's compatible with UE 4.27+ and all UE5 versions.

---

## 💥 Known Gotchas

### 1. `cooked_hash` must be `Some` for `AddedDependencyFlags`+

```
Error: "Invalid value Cooked hash for asset registry with version AddedDependencyFlags"
```

The `write()` path validates that `cooked_hash` is `Some` for any version ≥ `AddedCookedMD5Hash` (6). Even though you don't have actual cook data, you need:

```rust
// ✅ CORRECT — empty hash, but the Option is Some
cooked_hash: Some(FMD5Hash { hash: None })

// ❌ WRONG — causes runtime serialization error
cooked_hash: None
```

`FMD5Hash::write()` handles `hash: None` gracefully — it just writes a 4-byte `0u32`. The `Some` wrapper is what the version check looks for.

---

### 2. `SharedResource::get_mut()` requires `&mut self`

```
error[E0596]: cannot borrow `*name_map` as mutable, as it is behind a `&` reference
```

`get_mut()` on `SharedResource<T>` requires `&mut SharedResource<T>`. But `SharedResource<T>` **also** implements `Deref` to `RefCell<T>` (non-threading) or `RwLock<T>` (threading). Use the `Deref` path:

```rust
// ✅ CORRECT — Deref to RefCell, then borrow_mut
fn make_fname(name_map: &SharedResource<NameMap>, value: &str) -> FName {
    name_map.borrow_mut().add_fname(value)
}

// ❌ WRONG — needs &mut SharedResource
fn make_fname_bad(name_map: &SharedResource<NameMap>, value: &str) -> FName {
    name_map.get_mut().add_fname(value)
}
```

---

### 3. All FNames must be `Backed` for serialization

```
// ArchiveWriter::write_fname returns Err for FName::Dummy
```

`FName::Dummy` cannot be serialized. Every `FName` you put into `AssetData` or `AssetPackageData` must be created via `NameMap::add_fname()`, which returns a `FName::Backed` referencing the shared name map.

```rust
// ✅ Creates a Backed FName
let fname = name_map.borrow_mut().add_fname("/Game/Content/DA_Stuff");

// ❌ FName::Dummy — will panic on write
let fname = FName::from_slice("/Game/Content/DA_Stuff");
```

---

### 4. `AssetData::asset_class` vs `asset_path` (TopLevelAssetPath)

- Versions **< `ClassPaths` (11)**: use `asset_class: Some(FName)` and `asset_path: None`
- Versions **≥ `ClassPaths`**: use `asset_class: None` and `asset_path: Some(TopLevelAssetPath)`

For `AddedDependencyFlags` (7):
```rust
AssetData::from_data(
    // ...
    asset_class: Some(make_fname(&nm, "/Script/Engine.Blueprint")),
    asset_path: None,
    // ...
)
```

---

### 5. The name map is shared — modification order matters

When you call `borrow_mut().add_fname(s)`, the returned `FName::Backed` holds an `index` into the name map. If you later clone the `SharedResource` and the underlying `Rc` is shared, all `FName::Backed` instances point to the same name map. This is **correct and intended** — just be aware:

- Don't create a second, independent name map and mix FNames between them
- When writing a registry, use the **same** name map for all FNames in all entries
- The `AssetRegistryState` carries its own name map — write against that one

---

## Constructing a Registry from Scratch

```rust
use unreal_asset_base::{
    containers::NameMap,
    custom_version::FAssetRegistryVersionType,
    engine_version,
    engine_version::EngineVersion,
    flags::EPackageFlags,
};
use unreal_asset_registry::{
    objects::{
        asset_data::{AssetData, TopLevelAssetPath},
        asset_package_data::AssetPackageData,
        md5_hash::FMD5Hash,
    },
    AssetRegistryState,
};

const VERSION: FAssetRegistryVersionType = FAssetRegistryVersionType::AddedDependencyFlags;

let (ov, ov5) = engine_version::get_object_versions(EngineVersion::VER_UE5_2);
let name_map = NameMap::new();

// Build FNames from the shared name map
let asset_data = AssetData::from_data(
    name_map.borrow_mut().add_fname("/Game/Items/DA_Sword.DA_Sword"),
    name_map.borrow_mut().add_fname("/Game/Items/DA_Sword"),
    name_map.borrow_mut().add_fname("/Game/Items"),
    name_map.borrow_mut().add_fname("DA_Sword"),
    Some(name_map.borrow_mut().add_fname("/Script/Engine.DataAsset")),
    None,                                // no TopLevelAssetPath (pre-ClassPaths)
    IndexedMap::new(),                   // no tags
    Default::default(),                  // no bundles
    vec![],                              // no chunk ids
    EPackageFlags::PKG_NONE,
    VERSION,
);

let pkg_data = AssetPackageData::from_data(
    name_map.borrow_mut().add_fname("/Game/Items/DA_Sword"),
    Guid::default(),
    Some(FMD5Hash { hash: None }),       // required for AddedDependencyFlags+
    None,
    0, 0, None, -1, None, 0,
    VERSION,
);

let state = AssetRegistryState::from_data(
    VERSION, ov, ov5,
    Some(name_map),
    vec![asset_data],
    vec![],            // no DependsNodes
    vec![pkg_data],
);

let mut cursor = std::io::Cursor::new(Vec::new());
state.write(&mut cursor)?;
std::fs::write("AssetRegistry.bin", cursor.into_inner())?;
```

---

## File Structure of `AssetRegistry.bin` (AddedDependencyFlags)

```
[4 bytes]  Magic / Version tag
[4 bytes]  FAssetRegistryVersionType (7 for AddedDependencyFlags)
[name table]
    [4 bytes]  name entry count
    [N entries] each: FString (len-prefixed UTF-8 or UTF-16)
[assets block]
    [4 bytes]  asset count
    [N entries] AssetData (each with FNames as [index, number] pairs)
[depends block]
    [4 bytes]  depends node count (usually 0 for generated registries)
[package data block]
    [4 bytes]  package data count
    [N entries] AssetPackageData
```

---

*See also: `UE5_BINARY_PIPELINE_STATUS.md` for the full pipeline state.*
