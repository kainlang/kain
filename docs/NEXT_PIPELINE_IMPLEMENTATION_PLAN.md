# Next Pipeline: Implementation Plan

> **Date:** February 19, 2026  
> **Foundation:** `ue5-asset-utils` ✅ · Blueprints ✅ · Materials ✅ · Kismet ✅  
> **This doc covers:** The three next items from `UNREAL_ASSET_EXPANSION_GUIDE.md`  
> **⚡ STATUS UPDATE:** Task 1 ✅ DONE · Task 2 (Shader Bridge) 🔄 TODO · Task 3 ✅ DONE  
> **→ See `UE5_BINARY_PIPELINE_STATUS.md` for the full current-state doc**

---

## Prerequisites (All Done)

Before reading this plan, the following are **complete and working**:

| Crate | What exists |
|---|---|
| `crates/ue5-asset-utils` | `ImportBuilder`, `PropertyDef`/`PropertyValue`, `convert_property_defs()` |
| `crates/ue5-blueprints` | Full binary writer, Kismet emitter, 21/21 tests |
| `crates/ue5-materials` | Full `MaterialAssetBuilder`, 30+ node types, 8/8 tests |
| `crates/unreal/unreal_asset` | `Asset::new_empty()`, `write_data()`, full read/write |
| `crates/unreal/unreal_asset_registry` | `AssetRegistryState` with full read/write, `AssetData`, `AssetPackageData` |

All three tasks below are **self-contained** — no cross-task dependencies. They can be implemented in any order.

---

## Task 1 — `UDataAsset` Binary Writer

**Priority:** 🔴 Highest — replaces a full TODO stub  
**File to create:** `crates/ue5-editor/src/data_asset_writer.rs`  
**File to edit:** `crates/ue5-editor/src/editor/assets.rs`  
**Estimated effort:** ~60-80 lines of implementation + tests

### The Problem

`crates/ue5-editor/src/editor/assets.rs` is currently a dead stub:

```rust
pub struct AssetGenerator { /* TODO */ }

impl AssetGenerator {
    pub fn generate_asset_type(&mut self, _st: &TypedStruct) -> (String, String) {
        (String::new(), String::new())  // ← returns nothing
    }
}
```

KAIN structs marked `@data_asset` should produce a real `.uasset` file that can be dropped into `Content/` and loaded in the Editor immediately as a `UDataAsset` subclass.

### Asset Structure

A `UDataAsset` `.uasset` has the simplest possible object graph — **one export**:

```
Imports:
  [-1] Package  "/Script/CoreUObject"  (outer: 0)
  [-2] Package  "/Script/Engine"       (outer: 0)
  [-3] Class    "DataAsset"            (outer: -2)
       — OR your custom class: "/Script/MyPlugin.UMyDataAsset"

Exports:
  [1] NormalExport  "DA_ItemData"
        class_index  → import: DataAsset (or custom subclass)
        outer_index  → PackageIndex(0)
        object_flags → RF_PUBLIC | RF_STANDALONE
        properties   → [all struct fields as PropertyData]
```

That's it. No SCS, no CDO, no generated class. Just a flat property bag on one export.

### Implementation

**Step 1.1 — Create `data_asset_writer.rs`**

```rust
// crates/ue5-editor/src/data_asset_writer.rs

use std::io::Cursor;
use unreal_asset::{
    engine_version::EngineVersion,
    exports::{base_export::BaseExport, normal_export::NormalExport, Export},
    flags::EObjectFlags,
    types::PackageIndex,
    Asset,
};
use ue5_asset_utils::{ImportBuilder, PropertyDef, property_converter::convert_property_defs};
use crate::error::Result; // or KainResult

/// Write a UDataAsset .uasset file from a flat list of PropertyDef fields.
///
/// `name`        — asset object name, e.g. "DA_ItemData"  
/// `class_path`  — e.g. "/Script/Engine.DataAsset" or "/Script/MyPlugin.UMyItemData"  
/// `fields`      — property values from the KAIN struct fields  
pub fn write_data_asset(
    name: &str,
    class_path: &str,
    fields: &[PropertyDef],
    engine_version: EngineVersion,
) -> Result<Vec<u8>> {
    let mut asset = Asset::new_empty(engine_version);

    // ── Imports ──────────────────────────────────────────────────────────────
    // Package for the class
    let (pkg_path, class_name) = ImportBuilder::parse_class_path(class_path);
    let pkg_import = ImportBuilder::get_or_add_package(&mut asset, &pkg_path);
    let class_import = ImportBuilder::get_or_add_class(&mut asset, &class_name, pkg_import);

    // ── Export ───────────────────────────────────────────────────────────────
    let asset_name = asset.add_fname(name);
    let properties = convert_property_defs(&mut asset, fields);

    let export = NormalExport {
        base_export: BaseExport {
            class_index: class_import,
            super_index: PackageIndex::new(0),
            template_index: PackageIndex::new(0),
            outer_index: PackageIndex::new(0),
            object_name: asset_name,
            object_flags: EObjectFlags::RF_PUBLIC | EObjectFlags::RF_STANDALONE,
            ..Default::default()
        },
        properties,
        extras: Vec::new(),
    };
    asset.asset_data.exports.push(Export::NormalExport(export));

    // ── Serialize ─────────────────────────────────────────────────────────────
    asset.rebuild_name_map();
    let mut cursor = Cursor::new(Vec::new());
    asset.write_data(&mut cursor, None)
        .map_err(|e| /* your error type */ e)?;
    Ok(cursor.into_inner())
}
```

**Step 1.2 — Wire `TypedStruct` → `PropertyDef` conversion**

In `ue5-editor` (or reuse from `ue5-blueprints/conversion.rs`):

```rust
// Map each kain_core::types::TypedStruct field → PropertyDef
fn typed_struct_to_fields(st: &TypedStruct) -> Vec<PropertyDef> {
    st.fields.iter().filter_map(|field| {
        // Same logic as conversion.rs:convert_property()
        // Uses PropertyDef::float(), ::str(), ::bool(), ::vector() etc.
        convert_field_to_property_def(field)
    }).collect()
}
```

This is near-identical to `crates/ue5-blueprints/src/conversion.rs`'s `convert_property()` function. Consider moving it to `ue5-asset-utils` as `from_typed_field()` if it's needed in multiple places.

**Step 1.3 — Replace the stub in `assets.rs`**

```rust
// crates/ue5-editor/src/editor/assets.rs

use crate::data_asset_writer::write_data_asset;
use ue5_asset_utils::PropertyDef;
use unreal_asset::engine_version::EngineVersion;

pub fn generate_data_asset_binary(
    st: &TypedStruct,
    engine_version: EngineVersion,
) -> Result<Vec<u8>> {
    let name = format!("DA_{}", st.ast.name);
    let class_path = resolve_data_asset_class(st); // reads @data_asset("ClassName") attr
    let fields = typed_struct_to_fields(st);
    write_data_asset(&name, &class_path, &fields, engine_version)
}
```

**Step 1.4 — Wire into `ue5_pipeline.rs` (STEP 3.7)**

```rust
// In build_ue5_plugin(), after Blueprint step:

// STEP 3.7: Generate DataAsset .uasset files
let data_structs: Vec<&TypedStruct> = typed_program.items.iter()
    .filter_map(|item| {
        if let TypedItem::Struct(st) = item {
            let is_data_asset = st.ast.attributes.iter()
                .any(|a| a.name == "data_asset");
            if is_data_asset { Some(st) } else { None }
        } else {
            None
        }
    })
    .collect();

if !data_structs.is_empty() {
    let da_dir = layout.plugin_root.join("Content").join("DataAssets");
    fs::create_dir_all(&da_dir).ok();

    for st in data_structs {
        match ue5_editor::generate_data_asset_binary(st, engine_version) {
            Ok(bytes) => {
                let path = da_dir.join(format!("DA_{}.uasset", st.ast.name));
                fs::write(&path, bytes).ok();
                println!("   ✓ Generated: {}", path.display());
            }
            Err(e) => eprintln!("   ❌ DataAsset error for {}: {}", st.ast.name, e),
        }
    }
}
```

### `@data_asset` Attribute Resolution

Read the class path from the attribute argument:
- `@data_asset` with no args → `/Script/Engine.DataAsset`
- `@data_asset("UMyItemData")` → `/Script/YourPlugin.UMyItemData`
- `@data_asset("/Script/GameplayAbilities.UPrimaryDataAsset")` → used as-is (full path)

### Tests

```rust
#[test]
fn test_write_data_asset_empty() {
    let bytes = write_data_asset(
        "DA_Empty", "/Script/Engine.DataAsset", &[], EngineVersion::VER_UE5_2
    ).unwrap();
    assert!(!bytes.is_empty());
    assert_eq!(&bytes[0..4], &[0xC1, 0x83, 0x2A, 0x9E]); // UE5 magic
}

#[test]
fn test_write_data_asset_with_fields() {
    let fields = vec![
        PropertyDef::str("Name", "Iron Sword"),
        PropertyDef::int("Damage", 45),
        PropertyDef::float("Weight", 2.5),
        PropertyDef::bool("bEquippable", true),
        PropertyDef::soft_object("Icon", "/Game/UI/Icons/T_IronSword.T_IronSword"),
    ];
    let bytes = write_data_asset(
        "DA_IronSword", "/Script/Engine.DataAsset", &fields, EngineVersion::VER_UE5_2
    ).unwrap();
    assert!(bytes.len() > 200);
}
```

### Done Criteria

- [ ] `write_data_asset()` compiles and returns well-formed UASSET magic header
- [ ] `assets.rs` stub replaced — `generate_data_asset_binary()` returns bytes not empty string
- [ ] `ue5_pipeline.rs` STEP 3.7 filters `@data_asset` structs and writes `.uasset` files to `Content/DataAssets/`
- [ ] At least 2 unit tests passing

---

## Task 2 — Shader → `UMaterialExpressionCustom` Bridge

**Priority:** 🟡 Medium — closes the material/shader gap  
**Files to edit:** `crates/ue5-materials/src/material_serializer.rs`, `crates/ue5-shaders/src/lib.rs`  
**Estimated effort:** ~40 lines new code, mostly wiring

### The Problem

`ue5-shaders` already generates valid HLSL function bodies. `MaterialAssetBuilder` already has `add_custom_hlsl_node()`. The gap is: **no function that extracts just the function body** (without the `.usf` headers, parameter declarations, and `[numthreads]`) for embedding in a material node.

### What `add_custom_hlsl_node()` Expects

A `UMaterialExpressionCustom` node in a `.uasset` has these serialized properties:

```
Code         → StrProperty     — the raw HLSL expression (NOT a full .usf file)
OutputType   → EnumProperty    — ECustomMaterialOutputType: CMOT_Float1/2/3/4
Inputs       → ArrayProperty   — one entry per input pin
Description  → StrProperty     — shows as node label in editor
```

The `Code` field should be the **function body only** — no `#include`, no global declarations, no entry point signature. For example:

```hlsl
// Full .usf (what ue5-shaders generates today):
#include "/Engine/Public/Platform.ush"
float Roughness;
float DoToonEdge(float2 UV) {
    float edge = fwidth(UV.x) * Roughness;
    return saturate(1.0 - edge * 10.0);
}

// What the Custom HLSL node Code field needs:
float edge = fwidth(UV.x) * Roughness;
return saturate(1.0 - edge * 10.0);
```

### New Function: `emit_material_function_body()`

Add to `crates/ue5-shaders/src/codegen_usf.rs` (or a new `src/material_embed.rs`):

```rust
/// Generate the HLSL body of a shader suitable for embedding in a
/// UMaterialExpressionCustom node's `Code` property.
///
/// This is NOT a full .usf file — it contains only the function body
/// statements, no headers, no parameter declarations, no entry point.
///
/// The material graph pins supply the inputs; the return statement
/// supplies the output.
pub fn emit_material_function_body(
    shader: &TypedShader,
) -> KainResult<MaterialFunctionBody> {
    // 1. Generate the full function body string from the shader's AST
    // 2. Strip the entry point signature (keep only the { ... } contents)
    // 3. Return inputs as (name, type) pairs for pin generation
    todo!()
}

pub struct MaterialFunctionBody {
    /// The raw HLSL code for the Custom node's `Code` field
    pub code: String,
    /// Input pin definitions: (name, CustomOutputType)
    pub inputs: Vec<(String, CustomOutputType)>,
    /// Output type
    pub output_type: CustomOutputType,
}
```

### Integration in `MaterialAssetBuilder`

The `MaterialNodeType::CustomHLSL` variant already exists in `material_graph.rs`. The serializer's `add_custom_hlsl_node()` method already writes the export. The only change is making the pipeline call `emit_material_function_body()` when it encounters a `shader` reference in a material:

```rust
// In ue5_pipeline.rs, inside the material generation loop:
// When MaterialGraphConverter produces a CustomHLSL node whose
// `code` field is "shader::<name>" (a shader reference, not inline HLSL):

if code.starts_with("shader::") {
    let shader_name = &code["shader::".len()..];
    if let Some(shader) = find_shader(typed_program, shader_name) {
        let body = ue5_shaders::emit_material_function_body(shader)?;
        node_id = builder.add_custom_hlsl_node(
            &body.code,
            body.inputs.iter().map(|(n, _)| n.clone()).collect(),
            body.output_type as usize,
        );
    }
}
```

### Alternative: Inline `@shader` reference in KAIN syntax

A cleaner long-term approach adds an attribute to material graphs:

```kain
@material_graph
material M_ToonEdge:
    input UV: Vec2
    
    -- Reference the shader directly — KAIN extracts its body automatically
    let edge = @shader ToonEdgeDetect(UV)
    output base_color = vec3(edge, edge, edge)
```

The `ast_converter.rs` would recognize `@shader Name(args)` and produce a `CustomHLSL` node referencing `shader::ToonEdgeDetect`, which the pipeline resolves at write time.

### The `.usf` Pipeline Is Unchanged

Shaders that are:
- Compute stage → always generate `.usf` + C++ class
- Pixel/Vertex stage not referenced by a material graph → always generate `.usf` + C++ class  
- Pixel stage referenced inside a `@material_graph` block → **additionally** get embedded as a Custom node

The full `.usf` output for standalone use is **never removed**. This is additive.

### Done Criteria

- [ ] `emit_material_function_body()` exists and returns a non-empty code string for a simple pixel shader
- [ ] `MaterialAssetBuilder::add_custom_hlsl_node()` correctly sets `Code`, `OutputType`, `Inputs` properties (verify against an existing `.uasset` from the editor)
- [ ] Pipeline test: write a material with one Custom node → load in UE5 → node appears with correct pins

---

## Task 3 — Asset Registry Writer

**Priority:** 🟡 Medium — pure UX win, zero risk  
**File to create:** `crates/cli/src/packager/registry_writer.rs`  
**Crates used:** `unreal_asset_registry` (already vendored, zero new dependencies)  
**Estimated effort:** ~80 lines

### The Problem

When KAIN drops `.uasset` files into `Content/`, the UE5 Editor needs to scan all assets to populate the Content Browser. On large projects this takes **30+ seconds**. 

`AssetRegistry.bin` is a binary cache at `Saved/AssetRegistry.bin` (or per-plugin at `<Plugin>/Content/AssetRegistry.bin`). If we append our generated assets to it before the editor launches, they appear **immediately** with no scan delay.

### What `AssetRegistryState` Gives Us

From `crates/unreal/unreal_asset_registry/src/`:

```rust
// Already implemented — full read/write:
pub struct AssetRegistryState {
    pub assets_data: Vec<AssetData>,     // ← append here
    pub depends_nodes: Vec<DependsNode>,
    pub package_data: Vec<AssetPackageData>,
}

pub struct AssetData {
    pub object_path: FName,       // "/Game/Blueprints/BP_Player.BP_Player"
    pub package_name: FName,      // "/Game/Blueprints/BP_Player"
    pub package_path: FName,      // "/Game/Blueprints"
    pub asset_name: FName,        // "BP_Player"
    pub asset_class: FName,       // "Blueprint" or "Material" or "DataAsset"
    pub tags_and_values: IndexedMap<FName, Option<FName>>,
    // ...
}
```

### Implementation

**Step 3.1 — Create `registry_writer.rs`**

```rust
// crates/cli/src/packager/registry_writer.rs

use std::path::Path;
use std::io::{Cursor, Seek, SeekFrom};
use unreal_asset_registry::AssetRegistryState;
use unreal_asset_base::custom_version::FAssetRegistryVersionType;

/// Describes one asset to register in the Content Browser cache.
pub struct GeneratedAsset {
    /// Full UE5 package path, e.g. "/Game/Blueprints/BP_Player"
    pub package_path: String,
    /// Asset name (last segment), e.g. "BP_Player"
    pub asset_name: String,
    /// UE5 asset class name, e.g. "Blueprint", "Material", "DataAsset"
    pub asset_class: String,
}

/// Append generated assets to the plugin's AssetRegistry.bin.
///
/// If the registry file doesn't exist, creates a new one.
/// If it exists, reads it, appends the new entries, and writes it back.
pub fn register_assets(
    registry_path: &Path,
    assets: &[GeneratedAsset],
) -> Result<(), Box<dyn std::error::Error>> {
    if assets.is_empty() {
        return Ok(());
    }
    
    // Load existing registry or start fresh
    let mut state = if registry_path.exists() {
        let data = std::fs::read(registry_path)?;
        let mut cursor = Cursor::new(data);
        AssetRegistryState::read(&mut cursor)?
    } else {
        AssetRegistryState::new()
    };

    // Build a set of already-registered package paths to avoid duplicates
    let existing: std::collections::HashSet<String> = state.assets_data.iter()
        .map(|ad| ad.package_name.get_content(|s| s.to_string()))
        .collect();

    for asset in assets {
        if existing.contains(&asset.package_path) {
            continue; // already registered
        }
        
        let entry = AssetData {
            package_name: state.add_fname(&asset.package_path),
            package_path: state.add_fname(
                asset.package_path.rsplit_once('/')
                    .map(|(parent, _)| parent)
                    .unwrap_or("/Game")
            ),
            asset_name: state.add_fname(&asset.asset_name),
            asset_class: state.add_fname(&asset.asset_class),
            // object_path = package_name + "." + asset_name
            object_path: state.add_fname(
                &format!("{}.{}", asset.package_path, asset.asset_name)
            ),
            tags_and_values: Default::default(),
            ..Default::default()
        };
        state.assets_data.push(entry);
    }

    // Write back
    if let Some(parent) = registry_path.parent() {
        std::fs::create_dir_all(parent)?;
    }
    let mut cursor = Cursor::new(Vec::new());
    state.write(&mut cursor)?;
    std::fs::write(registry_path, cursor.into_inner())?;
    
    Ok(())
}
```

> **Note:** The exact `AssetRegistryState` API (whether it's `read/write` or `load/save`) should be verified against the crate source before writing. Check `crates/unreal/unreal_asset_registry/src/lib.rs` for the actual method names. The `AssetData` struct fields are accurate from what we read above.

**Step 3.2 — Collect all generated assets in the pipeline**

In `ue5_pipeline.rs`, accumulate a `Vec<GeneratedAsset>` as each file is written:

```rust
let mut generated_assets: Vec<registry_writer::GeneratedAsset> = Vec::new();

// After each fs::write() for a Blueprint:
generated_assets.push(registry_writer::GeneratedAsset {
    package_path: format!("/Game/Blueprints/{}", bp_ir.name),
    asset_name: bp_ir.name.clone(),
    asset_class: "Blueprint".to_string(),
});

// After each Material write:
generated_assets.push(registry_writer::GeneratedAsset {
    package_path: format!("/Game/Materials/{}", mat.name),
    asset_name: mat.name.clone(),
    asset_class: "Material".to_string(),
});

// After each DataAsset write:
generated_assets.push(registry_writer::GeneratedAsset {
    package_path: format!("/Game/DataAssets/DA_{}", st.ast.name),
    asset_name: format!("DA_{}", st.ast.name),
    asset_class: "DataAsset".to_string(),
});
```

**Step 3.3 — Write registry at end of pipeline**

```rust
// Final step in build_ue5_plugin():
// STEP 3.8: Update Asset Registry
if !generated_assets.is_empty() {
    // Plugin-local registry: fastest, doesn't require touching the project
    let registry_path = layout.plugin_root
        .join("Content")
        .join("AssetRegistry.bin");
    
    match registry_writer::register_assets(&registry_path, &generated_assets) {
        Ok(()) => println!("📋 Asset registry updated ({} entries)", generated_assets.len()),
        Err(e) => eprintln!("   ⚠️  Asset registry write failed (non-fatal): {}", e),
    }
}
```

> The registry write is **non-fatal** — if it fails, the `.uasset` files are still valid, the editor just has to scan. Never block the build on this.

### Done Criteria

- [ ] `register_assets()` creates `AssetRegistry.bin` from scratch if it doesn't exist
- [ ] `register_assets()` is idempotent — calling it twice with the same assets doesn't duplicate entries
- [ ] Running `kain build --ue5` with assets produces a `Content/AssetRegistry.bin` file in the plugin folder
- [ ] Non-fatal: if `AssetRegistryState::read()` fails (corrupted file), log a warning and write a fresh registry

---

## Implementation Order Recommendation

```
Week 1:
  Task 1 (DataAsset)   ← ~2 hours, highest impact, pure green-field
  Task 3 (Registry)    ← ~2 hours, zero risk, pure additive
  
Week 2:
  Task 2 (Shader→HLSL) ← requires understanding emit_material_function_body
                          surface area is larger but foundation is solid
```

Task 1 and Task 3 can be done in a single session — they don't touch each other and each has a clear "done" binary (does the file appear + does the Content Browser show it). Task 2 requires more design work around the KAIN syntax for `@shader` references and how the `ast_converter.rs` should handle them.

---

## Integration Test (Run After All Three)

```kain
-- test.kn

@data_asset
struct ItemData:
    name: String = "Iron Sword"
    damage: Int = 45
    weight: Float = 2.5
    
@material_graph
material M_IronSword:
    input Albedo: Texture2D
    input Roughness: Float = 0.3
    output base_color = Albedo

actor BP_Player:
    state capsule: CapsuleComponent @component
    state mesh: StaticMeshComponent @component @attach("capsule")
    state MaxWalkSpeed: Float = 600.0
    
    on begin_play:
        InitAbilities()
```

Running `kain build --ue5` should produce:
```
Content/
  Blueprints/
    BP_Player.uasset          ← ✅ done
  Materials/
    M_IronSword.uasset        ← ✅ done  
  DataAssets/
    DA_ItemData.uasset        ← Task 1
  AssetRegistry.bin           ← Task 3
```

All four files drop into the plugin content folder. Editor launch → Content Browser shows all four immediately, no scan.
