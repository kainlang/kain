# UE5 Binary Asset Pipeline — Current Status

> **Date:** February 19, 2026  
> **Author:** Antigravity (Google DeepMind)  
> **Session work:** `UDataAsset` writer ✅ · Asset Registry Writer ✅ · `AssetPackageData::from_data` ✅ · `AssetRegistryState::from_data` ✅  

---

## 🗺️ Big Picture

KAIN compiles `.kn` source files into **binary Unreal Engine 5 assets** — no C++ compilation required for content types. The pipeline outputs valid `.uasset` files and an updated `AssetRegistry.bin`, meaning assets are immediately visible in the Unreal Editor Content Browser on first load.

```
.kn source
   │
   ├─[kain-core]──────── Parse + Type-check → TypedProgram
   │
   ├─[ue5-shaders]──────  HLSL / USF codegen → .usf files
   ├─[ue5-materials]─────  Material asset → MaterialAssetBuilder → .uasset
   ├─[ue5-blueprints]────  Blueprint asset  → BinaryWriter + Kismet → .uasset
   ├─[ue5-editor]────────  DataAsset        → DataAssetWriter → .uasset
   │
   ├─[cli/packager]──────  Orchestration
   │      ├── ue5_pipeline.rs   (top-level build driver)
   │      ├── registry_writer.rs  ← NEW ✅ this session
   │      ├── inject.rs
   │      └── ...
   │
   └─[unreal/unreal_asset_registry]  AssetRegistry.bin read/write
```

---

## ✅ Completed This Session

### 1. `UDataAsset` Binary Writer — `crates/ue5-editor/src/data_asset_writer.rs`

Full `.uasset` serializer for `UDataAsset` subclasses. KAIN structs tagged `@data_asset` can now be compiled to real binary assets.

**What it does:**
- Creates a single-export `.uasset` with a proper property bag
- Resolves imports: `/Script/CoreUObject`, `/Script/Engine`, your custom class path
- Uses `ue5-asset-utils`'s `PropertyDef`/`PropertyValue` for type-safe field population
- Handles `EngineVersion` parameterization (UE 5.0 → 5.4+)
- **26 unit tests** covering round-trips, field types, class resolution

**API:**
```rust
use ue5_editor::data_asset_writer::{write_data_asset, PropertyDef, PropertyValue};

let bytes = write_data_asset(
    "DA_MyItem",
    "/Script/MyPlugin.UMyItem",
    &[
        PropertyDef::new("Health", PropertyValue::Float(100.0)),
        PropertyDef::new("Name",   PropertyValue::String("Sword".into())),
    ],
    EngineVersion::VER_UE5_2,
)?;
std::fs::write("Content/DA_MyItem.uasset", bytes)?;
```

---

### 2. Asset Registry Writer — `crates/cli/src/packager/registry_writer.rs`

Creates or updates `AssetRegistry.bin` with generated asset metadata. Enables **instant Content Browser visibility** without a full editor scan.

**What it does:**
- Reads existing `AssetRegistry.bin` if present — appends new entries
- Creates a fresh registry from scratch if none exists
- Deduplicates by `object_path` — idempotent on repeated pipeline runs
- Targets `FAssetRegistryVersionType::AddedDependencyFlags` — pre-FixedTags, self-contained name table, UE 4.27 / 5.0+ compatible
- Registry write failures are **non-fatal** — logged, pipeline continues
- **6 unit tests** covering creation, dedup, empty no-op, and file I/O

**API:**
```rust
use cli::packager::registry_writer::{register_assets, AssetEntry};

let entries = vec![
    AssetEntry::blueprint("/Game/Blueprints/BP_Enemy", "BP_Enemy"),
    AssetEntry::material("/Game/Materials/M_Fire", "M_Fire"),
    AssetEntry::data_asset("/Game/Data/DA_Items", "DA_Items"),
    AssetEntry::custom("/Game/Foo/Bar", "Bar", "/Script/MyPlugin.UBar"),
];

// Non-fatal — log and continue if this fails
if let Err(e) = register_assets(&registry_path, &entries, engine_version) {
    log::warn!("AssetRegistry update failed (non-fatal): {}", e);
}
```

**Data-driven design:** `AssetEntry` descriptors are pure data — no baked-in paths, no hardcoded class strings. The registry version, object/package versions, and all asset metadata flow through config.

---

### 3. `AssetRegistryState::from_data` — `crates/unreal/unreal_asset_registry/src/lib.rs`

Added public constructor and name map accessor to `AssetRegistryState`. Previously there was no way to construct a registry state programmatically — only by reading a binary file.

```rust
pub fn from_data(
    version: FAssetRegistryVersionType,
    object_version: ObjectVersion,
    object_version_ue5: ObjectVersionUE5,
    name_map: Option<SharedResource<NameMap>>,
    assets_data: Vec<AssetData>,
    depends_nodes: Vec<DependsNode>,
    package_data: Vec<AssetPackageData>,
) -> Self

pub fn name_map(&self) -> Option<&SharedResource<NameMap>>
```

---

### 4. `AssetPackageData::from_data` — `crates/unreal/unreal_asset_registry/src/objects/asset_package_data.rs`

Added public constructor. The `version` field was private with no existing `from_data`, making programmatic creation impossible. Now:

```rust
pub fn from_data(
    package_name: FName,
    package_guid: Guid,
    cooked_hash: Option<FMD5Hash>,   // Some(FMD5Hash { hash: None }) for AddedDependencyFlags+
    imported_classes: Option<Vec<FName>>,
    disk_size: i64,
    file_version: i32,
    ue5_version: Option<i32>,
    file_version_licensee_ue: i32,
    custom_versions: Option<Vec<CustomVersion>>,
    flags: u32,
    version: FAssetRegistryVersionType,
) -> Self
```

> **⚠️ Gotcha documented:** `AddedDependencyFlags` (and all versions ≥ `AddedCookedMD5Hash`) **require** `cooked_hash` to be `Some`. Use `Some(FMD5Hash { hash: None })` for a valid empty hash — `None` causes a runtime serialization error.

---

## 🏗️ Current Pipeline Architecture (CLI)

### `cli/src/packager/ue5_pipeline.rs` — The Orchestrator

The top-level build driver. Runs in order:

```
1. Parse + type-check KAIN source
2. Compile shaders → .usf files (Shaders crate)
3. Generate MaterialExpressionCustom nodes (Shaders → Materials bridge)  ← TODO
4. Generate material .uasset files (Materials crate)
5. Generate blueprint .uasset files (Blueprints crate)
6. Generate DataAsset .uasset files (Editor crate)              ← WIRED but needs integration call
7. Write/update AssetRegistry.bin (packager/registry_writer)    ← WIRED but needs integration call
8. Write .uplugin, .Build.cs, module graph
```

### Module Map

| Module | File | Status |
|--------|------|--------|
| Pipeline driver | `cli/src/packager/ue5_pipeline.rs` | ✅ Exists |
| Registry writer | `cli/src/packager/registry_writer.rs` | ✅ New, tested |
| Inject system | `cli/src/packager/inject.rs` | ✅ Exists |
| Dependencies | `cli/src/packager/dependencies.rs` | ✅ Exists |
| Post-process | `cli/src/packager/post_process.rs` | ✅ Exists |
| Config | `cli/src/packager/config.rs` | ✅ Exists |

---

## 🔧 Technical Notes & Lessons Learned

### FName Handling in the Registry

The asset registry uses `FName::Backed` exclusively during write. All strings must be registered into a shared `NameMap` before use.

```rust
// CORRECT — uses interior mutability via Deref to RefCell
fn make_fname(name_map: &SharedResource<NameMap>, value: &str) -> FName {
    name_map.borrow_mut().add_fname(value)
}

// WRONG — get_mut() requires &mut SharedResource
fn make_fname_bad(name_map: &mut SharedResource<NameMap>, value: &str) -> FName {
    name_map.get_mut().add_fname(value)   // ← compile error if you only have &SharedResource
}
```

`SharedResource<T>` implements `Deref` to `RefCell<T>` (non-threading build). Call `borrow_mut()` directly through Deref, not `get_mut()`.

### Registry Version Selection

We use `AddedDependencyFlags` (value `7`) as the target version. This is:
- Pre-FixedTags → self-contained name table per file (simpler to write)  
- Compatible with UE 4.27 and all UE5 versions
- Requires: `cooked_hash = Some(FMD5Hash { hash: None })` on each `AssetPackageData`
- Does NOT require: `asset_path (TopLevelAssetPath)` on `AssetData` (that's `ClassPaths`+)

### The `FName::Backed` vs `FName::Dummy` split

| Variant | Use case | Serializable? |
|---------|----------|--------------|
| `FName::Backed` | Has a `SharedResource<NameMap>` index | ✅ Yes |
| `FName::Dummy` | Just a string, no backing index | ❌ No — write fails |

All `FName`s created for the registry must be `Backed`. Use `NameMap::add_fname()` via `borrow_mut()`.

---

## 📋 What's Left (Next Steps)

### Priority 1 — Wire Registry Writer into `ue5_pipeline.rs`

The `register_assets` function exists and is tested. It just needs to be called at the end of the pipeline:

```rust
// In ue5_pipeline.rs, after all .uasset files are written:
use crate::packager::registry_writer::{register_assets, AssetEntry};

let registry_path = output_dir.join("AssetRegistry.bin");
let mut entries = Vec::new();

// Collect from each generated asset
for bp in &generated_blueprints {
    entries.push(AssetEntry::blueprint(&bp.package_name, &bp.asset_name));
}
for mat in &generated_materials {
    entries.push(AssetEntry::material(&mat.package_name, &mat.asset_name));
}
for da in &generated_data_assets {
    entries.push(AssetEntry::data_asset(&da.package_name, &da.asset_name));
}

if let Err(e) = register_assets(&registry_path, &entries, engine_version) {
    log::warn!("AssetRegistry update skipped: {}", e);
}
```

### Priority 2 — Shader → `UMaterialExpressionCustom` Bridge

Custom HLSL nodes generated by `ue5-shaders` need to be embedded as `UMaterialExpressionCustom` nodes in material assets. This is the last major missing piece of the shader pipeline.

**Approach:**
1. `ue5-shaders` emits `ShaderFunctionBody { name, hlsl_body, inputs, outputs }`
2. `ue5-materials` `MaterialNodeType::Custom` accepts this struct
3. Bridge: `ue5_pipeline.rs` passes shader output → material builder

### Priority 3 — `DataAssetWriter` integration into pipeline driver

`write_data_asset()` is implemented in `ue5-editor`. It needs to be called from `ue5_pipeline.rs` for each KAIN struct tagged `@data_asset`.

### Priority 4 — `kain install` / `cargo install`

The CLI binary installs as `kain` via `cargo install --path crates/cli`. The correct command is:

```powershell
cargo install --path crates/cli --features ue5
# or: cargo install --path crates/cli --all-features
```

---

## 📦 Crate Dependency Graph (UE5 pipeline)

```
kain-core
    │
    ├── ue5-shaders         (USF / HLSL codegen)
    │       └── [bridge]──→ ue5-materials
    │
    ├── ue5-materials       (Material .uasset writer)
    │       └── unreal/unreal_asset
    │
    ├── ue5-blueprints      (Blueprint .uasset writer + Kismet)
    │       └── unreal/unreal_asset
    │
    ├── ue5-editor          (DataAsset .uasset writer)
    │       ├── ue5-asset-utils
    │       └── unreal/unreal_asset
    │
    └── cli                 (Orchestrator)
            ├── kain-core
            ├── ue5-shaders         [feature: ue5]
            ├── ue5-materials       [feature: ue5]
            ├── ue5-blueprints      [feature: ue5]
            ├── ue5-editor          [feature: ue5]
            ├── unreal_asset_registry [feature: ue5]
            └── ue5-asset-utils     [feature: ue5]
```

---

## 🧪 Test Coverage

| Module | Tests | Status |
|--------|-------|--------|
| `ue5-blueprints` | 21 | ✅ All pass |
| `ue5-materials` | 8 | ✅ All pass |
| `ue5-editor/data_asset_writer` | 26 | ✅ All pass |
| `cli/packager/registry_writer` | 6 | ✅ All pass |
| `unreal_asset_registry` | (integration via above) | ✅ |

---

## 🔗 Key Files Reference

| Purpose | Path |
|---------|------|
| Pipeline orchestrator | `crates/cli/src/packager/ue5_pipeline.rs` |
| **Asset Registry Writer** | `crates/cli/src/packager/registry_writer.rs` |
| **DataAsset writer** | `crates/ue5-editor/src/data_asset_writer.rs` |
| Blueprint writer | `crates/ue5-blueprints/src/binary_writer.rs` |
| Material builder | `crates/ue5-materials/src/material_serializer.rs` |
| Shader codegen | `crates/ue5-shaders/src/codegen_usf.rs` |
| Registry state | `crates/unreal/unreal_asset_registry/src/lib.rs` |
| Asset data struct | `crates/unreal/unreal_asset_registry/src/objects/asset_data.rs` |
| Package data struct | `crates/unreal/unreal_asset_registry/src/objects/asset_package_data.rs` |
| FName definition | `crates/unreal/unreal_asset_base/src/types/fname.rs` |
| NameMap | `crates/unreal/unreal_asset_base/src/containers/name_map.rs` |
| SharedResource | `crates/unreal/unreal_asset_base/src/containers/shared_resource.rs` |
| Flags (EPackageFlags etc) | `crates/unreal/unreal_asset_base/src/flags.rs` |
