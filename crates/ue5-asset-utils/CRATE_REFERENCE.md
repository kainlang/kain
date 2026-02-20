# ue5-asset-utils — Shared UE5 Asset Generation Utilities

> **Purpose:** Common building blocks for `.uasset` binary generation  
> **Status:** Production-ready — Used by `ue5-blueprints`, `ue5-materials`, `ue5-editor`  
> **Version:** 0.1.0

---

## Overview

`ue5-asset-utils` is the **shared foundation** for all KAIN asset writers. It provides:

1. **Engine version authority** — Single source of truth for UE5 version targeting
2. **Property IR types** — Universal intermediate representation for UE5 tagged properties
3. **Property conversion** — IR → `unreal_asset` serialized properties
4. **Import deduplication** — Helpers for managing asset import tables

**Key principle:** Don't repeat yourself. Every asset writer (`ue5-blueprints`, `ue5-materials`, future `ue5-datatables`) uses these utilities instead of reimplementing property conversion or import management.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    ue5-asset-utils                          │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │
│  │ engine_target│  │property_types│  │import_builder│    │
│  │              │  │              │  │              │    │
│  │ Version      │  │ PropertyDef  │  │ Dedup imports│    │
│  │ authority    │  │ PropertyValue│  │ Resolve paths│    │
│  └──────────────┘  └──────────────┘  └──────────────┘    │
│                           │                                │
│                           ▼                                │
│                  ┌──────────────────┐                     │
│                  │property_converter│                     │
│                  │                  │                     │
│                  │ IR → unreal_asset│                     │
│                  │ Property objects │                     │
│                  └──────────────────┘                     │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
        ┌──────────────────────────────────────┐
        │  Consumers (asset writers)           │
        │                                      │
        │  • ue5-blueprints                   │
        │  • ue5-materials                    │
        │  • ue5-editor (DataAsset writer)    │
        │  • Future: ue5-datatables           │
        └──────────────────────────────────────┘
```

---

## Module Reference

### 1. `engine_target` — Version Authority

**Problem:** The vendored `unreal_asset` serializer has a hard ceiling at the highest `EngineVersion` it knows about. Adding new engine versions shouldn't require touching every `Asset::new_empty(VER_UE5_2)` call site.

**Solution:** `KainEngineTarget` is the single source of truth for version handling.

#### `KainEngineTarget` Enum

```rust
pub enum KainEngineTarget {
    Ue5_0,
    Ue5_1,
    Ue5_2,
    Ue5_3,  // Special case: shares 5.2 binary format
    Ue5_4,
    Ue5_5,
    Ue5_6,
    Ue5_7,
}
```

**Key methods:**

```rust
// Convert to unreal_asset's EngineVersion (only place that touches raw enum)
pub fn as_serializer_version(self) -> EngineVersion

// Human-readable version string ("5.4")
pub fn as_str(self) -> &'static str

// Parse from string
pub fn from_str(s: &str) -> Option<Self>

// Get the effective binary format ceiling
pub fn serializer_ceiling(self) -> KainEngineTarget

// Check if target is above what serializer natively supports
pub fn is_above_serializer_ceiling(self) -> bool
```

**Special case — UE 5.3:**
- Epic shipped UE 5.3 with **no new global `ObjectVersionUE5` variants**
- Binary format is identical to 5.2 (`DATA_RESOURCES` watermark)
- `Ue5_3.as_serializer_version()` returns `VER_UE5_2`
- `Ue5_3.is_above_serializer_ceiling()` returns `true`

**Upgrade path:**
1. Add new variant to `KainEngineTarget`
2. Update `as_serializer_version()` to return new `EngineVersion`
3. Nothing else changes

**Usage:**

```rust
use ue5_asset_utils::KainEngineTarget;

let target = KainEngineTarget::Ue5_4;
let engine_ver = target.as_serializer_version();
let asset = Asset::new_empty(engine_ver);
```

---

### 2. `property_types` — IR Types

Universal intermediate representation for UE5 tagged properties. Maps 1:1 to UE5 serialized property types.

#### `PropertyValue` Enum

```rust
pub enum PropertyValue {
    Bool(bool),
    Int(i32),
    Int64(i64),
    Float(f32),
    Double(f64),
    Str(String),
    Name(String),
    Text(String),
    
    // Object references
    SoftObject(String),      // FSoftObjectPath
    ObjectRef(String),       // Hard reference
    
    // Math types
    Vector { x: f32, y: f32, z: f32 },
    Rotator { pitch: f32, yaw: f32, roll: f32 },
    LinearColor { r: f32, g: f32, b: f32, a: f32 },
    
    // Complex types
    Enum { enum_type: String, value: String },
    Array { inner_type: String, values: Vec<PropertyValue> },
    Struct { struct_type: String, fields: Vec<PropertyDef> },
}
```

#### `PropertyDef` Struct

```rust
pub struct PropertyDef {
    pub name: String,
    pub value: PropertyValue,
}
```

**Ergonomic constructors:**

```rust
// Primitives
PropertyDef::bool("bEnabled", true)
PropertyDef::int("Health", 100)
PropertyDef::float("Speed", 600.0)
PropertyDef::str("DisplayName", "Test Actor")

// Math types
PropertyDef::vector("Location", 0.0, 0.0, 100.0)
PropertyDef::rotator("Rotation", 0.0, 90.0, 0.0)
PropertyDef::color("BaseColor", 1.0, 0.0, 0.0, 1.0)

// Object references
PropertyDef::soft_object("Mesh", "/Game/Meshes/SM_Cube.SM_Cube")
PropertyDef::object_ref("ParentClass", "/Script/Engine.Actor")

// Enums
PropertyDef::enum_val("BlendMode", "EBlendMode", "Translucent")

// Nested structs
PropertyDef::new("Transform", PropertyValue::Struct {
    struct_type: "Transform".to_string(),
    fields: vec![
        PropertyDef::vector("Location", 0.0, 0.0, 0.0),
        PropertyDef::vector("Scale", 1.0, 1.0, 1.0),
    ],
})
```

---

### 3. `property_converter` — IR → Serialized Properties

Converts `PropertyDef` IR into `unreal_asset` `Property` objects ready for serialization.

#### Core Functions

```rust
// Convert a single property
pub fn convert_property_def(
    asset: &mut Asset<Cursor<Vec<u8>>>,
    def: &PropertyDef,
) -> Option<Property>

// Convert a slice of properties
pub fn convert_property_defs(
    asset: &mut Asset<Cursor<Vec<u8>>>,
    defs: &[PropertyDef],
) -> Vec<Property>
```

**What it handles:**

| PropertyValue | UE5 Property Type | Notes |
|---------------|-------------------|-------|
| `Bool` | `BoolProperty` | Direct mapping |
| `Int` | `IntProperty` | 32-bit signed |
| `Int64` | `Int64Property` | 64-bit signed |
| `Float` | `FloatProperty` | Uses `OrderedFloat` |
| `Double` | `DoubleProperty` | Uses `OrderedFloat` |
| `Str` | `StrProperty` | UTF-8 string |
| `Name` | `NameProperty` | FName (interned string) |
| `Text` | `StrProperty` | FText (localized) |
| `SoftObject` | `SoftObjectPathProperty` | FSoftObjectPath with asset path + sub path |
| `ObjectRef` | `ObjectProperty` | Hard reference via import table |
| `Vector` | `StructProperty` with `VectorProperty` | FVector (x, y, z) |
| `Rotator` | `StructProperty` with `RotatorProperty` | FRotator (pitch, yaw, roll) |
| `LinearColor` | `StructProperty` with `LinearColorProperty` | FLinearColor (r, g, b, a) |
| `Enum` | `EnumProperty` | Enum type + value string |
| `Array` | `ArrayProperty` | Recursive conversion of inner values |
| `Struct` | `StructProperty` | Recursive conversion of fields |

**Usage:**

```rust
use ue5_asset_utils::{PropertyDef, convert_property_def};

let mut asset = Asset::new_empty(engine_ver);

let def = PropertyDef::float("Speed", 600.0);
let prop = convert_property_def(&mut asset, &def).unwrap();

// Add to CDO or component defaults
cdo_export.properties.push(prop);
```

---

### 4. `import_builder` — Import Table Management

Stateless utility for building and deduplicating imports in an `Asset`. Prevents duplicate import entries.

#### `ImportBuilder` Methods

```rust
// Find existing import by name
pub fn find_import_by_name(
    asset: &Asset<Cursor<Vec<u8>>>,
    name: &str,
) -> Option<PackageIndex>

// Get or create import (deduplicates)
pub fn get_or_add_import(
    asset: &mut Asset<Cursor<Vec<u8>>>,
    class_package: &str,
    class_name: &str,
    outer: PackageIndex,
    object_name: &str,
) -> PackageIndex

// Add package import (e.g. "/Script/Engine")
pub fn get_or_add_package(
    asset: &mut Asset<Cursor<Vec<u8>>>,
    package_path: &str,
) -> PackageIndex

// Add class import (e.g. "Actor" under "/Script/Engine")
pub fn get_or_add_class(
    asset: &mut Asset<Cursor<Vec<u8>>>,
    class_name: &str,
    outer_package: PackageIndex,
) -> PackageIndex

// Parse "/Script/Engine.Actor" → ("/Script/Engine", "Actor")
pub fn parse_class_path(path: &str) -> (String, String)

// Resolve object path to import (handles both /Script and /Game paths)
pub fn resolve_object_import(
    asset: &mut Asset<Cursor<Vec<u8>>>,
    path: &str,
) -> PackageIndex
```

**Path resolution:**

| Input Path | Result |
|------------|--------|
| `"/Script/Engine.Actor"` | Class import under `/Script/Engine` package |
| `"/Game/Meshes/SM_Cube.SM_Cube"` | Object import under `/Game/Meshes` package |
| `""` (empty) | `PackageIndex(0)` (null) |

**Deduplication:**
- All methods check for existing imports before creating new ones
- Searches by `object_name` field
- Returns existing `PackageIndex` if found

**Usage:**

```rust
use ue5_asset_utils::ImportBuilder;

let mut asset = Asset::new_empty(engine_ver);

// Add package
let engine_pkg = ImportBuilder::get_or_add_package(&mut asset, "/Script/Engine");

// Add class under package
let actor_class = ImportBuilder::get_or_add_class(&mut asset, "Actor", engine_pkg);

// Resolve full path (creates package + class if needed)
let mesh_import = ImportBuilder::resolve_object_import(
    &mut asset,
    "/Game/Meshes/SM_Cube.SM_Cube"
);

// Second call returns same index (deduplicated)
let mesh_import2 = ImportBuilder::resolve_object_import(
    &mut asset,
    "/Game/Meshes/SM_Cube.SM_Cube"
);
assert_eq!(mesh_import, mesh_import2);
```

---

## Integration with Other Crates

### `ue5-blueprints`

**Uses:**
- `KainEngineTarget` — Version targeting
- `PropertyDef` / `PropertyValue` — CDO defaults, component properties
- `convert_property_defs()` — Convert IR to serialized properties
- `ImportBuilder` — Manage component class imports, parent class imports

**Example:**

```rust
use ue5_asset_utils::{KainEngineTarget, PropertyDef, ImportBuilder};
use ue5_asset_utils::property_converter::convert_property_defs;

let target = KainEngineTarget::Ue5_4;
let mut asset = Asset::new_empty(target.as_serializer_version());

// Add parent class import
let parent = ImportBuilder::resolve_object_import(&mut asset, "/Script/Engine.Actor");

// Convert CDO defaults
let defaults = vec![
    PropertyDef::float("Speed", 600.0),
    PropertyDef::bool("bEnabled", true),
];
let props = convert_property_defs(&mut asset, &defaults);
```

### `ue5-materials`

**Uses:**
- `KainEngineTarget` — Version targeting
- `ImportBuilder` — Manage material function imports, texture imports

**Example:**

```rust
use ue5_asset_utils::{KainEngineTarget, ImportBuilder};

let target = KainEngineTarget::Ue5_5;
let mut asset = Asset::new_empty(target.as_serializer_version());

// Add material function import
let func = ImportBuilder::resolve_object_import(
    &mut asset,
    "/Engine/Functions/Engine_MaterialFunctions02/Texturing/WorldAlignedTexture.WorldAlignedTexture"
);
```

### `ue5-editor`

**Uses:**
- `KainEngineTarget` — Version targeting
- `PropertyDef` / `PropertyValue` — DataAsset property values
- `convert_property_defs()` — Convert IR to serialized properties
- `ImportBuilder` — Manage DataAsset class imports

**Example (DataAsset writer):**

```rust
use ue5_asset_utils::{PropertyDef, ImportBuilder};
use ue5_asset_utils::property_converter::convert_property_defs;

// Create DataAsset with properties
let properties = vec![
    PropertyDef::str("AssetName", "MyDataAsset"),
    PropertyDef::int("Priority", 10),
    PropertyDef::soft_object("Mesh", "/Game/Meshes/SM_Cube.SM_Cube"),
];

let props = convert_property_defs(&mut asset, &properties);
```

---

## File Structure

```
ue5-asset-utils/
├── Cargo.toml
└── src/
    ├── lib.rs                    # Public API + re-exports
    ├── engine_target.rs          # KainEngineTarget enum
    ├── property_types.rs         # PropertyDef / PropertyValue IR
    ├── property_converter.rs     # IR → unreal_asset Property
    └── import_builder.rs         # Import deduplication helpers
```

**Dependencies:**
- `serde` / `serde_json` — Serialization support
- `unreal_asset` — Asset serialization
- `unreal_asset_base` — Engine version types
- `unreal_asset_properties` — Property types
- `ordered-float` — Deterministic float comparison

---

## Testing

All modules have comprehensive unit tests:

### `engine_target` Tests (10 tests)
- Version mapping validation
- UE 5.3 special case (shares 5.2 format)
- UE 5.4–5.7 native format validation
- Serializer ceiling detection
- String round-trip parsing
- Default version stability

### `import_builder` Tests (9 tests)
- Path parsing (`/Script/Engine.Actor` → package + class)
- Package deduplication
- Class deduplication
- Object import resolution (Script paths)
- Object import resolution (Game paths)
- Empty path handling (returns null)
- Cross-call deduplication
- Multiple classes under same package

### `property_converter` Tests (11 tests)
- All primitive types (bool, int, float, string)
- Math types (vector, rotator, color)
- Enum conversion
- Soft object path conversion
- Nested struct conversion
- Soft path splitting (`/Game/Meshes/SM_Cube.SM_Cube` → asset + sub)

**Run tests:**

```bash
cd crates/ue5-asset-utils
cargo test
```

**Expected:** 30 tests passing

---

## Design Principles

### 1. Single Source of Truth
- **Version handling:** Only `KainEngineTarget::as_serializer_version()` touches raw `EngineVersion`
- **Property conversion:** Only `property_converter` knows how to serialize properties
- **Import management:** Only `ImportBuilder` creates imports

### 2. Stateless Utilities
- `ImportBuilder` has no instance state — all methods take `&mut Asset`
- `property_converter` functions are pure (aside from mutating asset)
- Composable, easy to test

### 3. Deduplication by Default
- `ImportBuilder` always checks for existing imports before creating new ones
- Prevents bloated import tables
- Reduces asset file size

### 4. Type Safety
- `PropertyValue` enum prevents invalid property types
- `KainEngineTarget` prevents invalid version combinations
- Compile-time guarantees over runtime checks

### 5. Ergonomic API
- `PropertyDef::float("Speed", 600.0)` instead of verbose struct construction
- `ImportBuilder::resolve_object_import()` handles both Script and Game paths
- Re-exports at crate root for convenience

---

## Common Patterns

### Pattern 1: Create Asset with Version Target

```rust
use ue5_asset_utils::KainEngineTarget;
use unreal_asset::Asset;

let target = KainEngineTarget::Ue5_4;
let engine_ver = target.as_serializer_version();
let mut asset = Asset::new_empty(engine_ver);
```

### Pattern 2: Build Property List

```rust
use ue5_asset_utils::PropertyDef;

let defaults = vec![
    PropertyDef::float("Speed", 600.0),
    PropertyDef::bool("bEnabled", true),
    PropertyDef::vector("Location", 0.0, 0.0, 100.0),
    PropertyDef::soft_object("Mesh", "/Game/Meshes/SM_Cube.SM_Cube"),
];
```

### Pattern 3: Convert and Add to Export

```rust
use ue5_asset_utils::property_converter::convert_property_defs;

let props = convert_property_defs(&mut asset, &defaults);

// Add to CDO export
cdo_export.properties.extend(props);
```

### Pattern 4: Resolve Object References

```rust
use ue5_asset_utils::ImportBuilder;

// Parent class
let parent = ImportBuilder::resolve_object_import(&mut asset, "/Script/Engine.Actor");

// Component class
let comp_class = ImportBuilder::resolve_object_import(
    &mut asset,
    "/Script/Engine.StaticMeshComponent"
);

// Asset reference
let mesh = ImportBuilder::resolve_object_import(
    &mut asset,
    "/Game/Meshes/SM_Cube.SM_Cube"
);
```

### Pattern 5: Nested Structs

```rust
use ue5_asset_utils::{PropertyDef, PropertyValue};

let transform = PropertyDef::new("Transform", PropertyValue::Struct {
    struct_type: "Transform".to_string(),
    fields: vec![
        PropertyDef::vector("Location", 0.0, 0.0, 100.0),
        PropertyDef::rotator("Rotation", 0.0, 90.0, 0.0),
        PropertyDef::vector("Scale", 1.0, 1.0, 1.0),
    ],
});
```

---

## Future Enhancements

### Planned Features

1. **Asset path validation** — Verify `/Game/...` paths exist before serialization
2. **Property schema validation** — Validate property types against UE5 class schemas
3. **Bulk import optimization** — Batch import creation for large asset graphs
4. **Asset registry integration** — Query asset registry for type information
5. **Content browser helpers** — Utilities for content browser path resolution

### Potential Additions

- **DataTable row conversion** — `PropertyDef` → CSV row
- **Config file parsing** — `.ini` file property extraction
- **Localization helpers** — FText namespace management
- **Asset dependency tracking** — Build dependency graphs from imports

---

## Troubleshooting

### Issue: "Import not found" errors

**Cause:** Object path doesn't exist or is malformed

**Solution:**
```rust
// Verify path format
let path = "/Script/Engine.Actor";  // ✓ Correct
let path = "Actor";                 // ✗ Missing package

// Use resolve_object_import for automatic package handling
let import = ImportBuilder::resolve_object_import(&mut asset, path);
```

### Issue: Duplicate imports in asset

**Cause:** Not using `ImportBuilder` deduplication

**Solution:**
```rust
// ✗ Don't manually create imports
asset.imports.push(Import { ... });

// ✓ Use ImportBuilder
let import = ImportBuilder::get_or_add_import(&mut asset, ...);
```

### Issue: Property conversion returns `None`

**Cause:** Unsupported property type or malformed IR

**Solution:**
```rust
// Check PropertyValue variant is supported
match convert_property_def(&mut asset, &def) {
    Some(prop) => { /* use prop */ },
    None => eprintln!("Unsupported property: {:?}", def),
}
```

### Issue: Version mismatch errors

**Cause:** Using wrong `KainEngineTarget` for project

**Solution:**
```rust
// Match project's UE5 version
let target = KainEngineTarget::Ue5_4;  // For UE 5.4 projects

// Check if above serializer ceiling
if target.is_above_serializer_ceiling() {
    eprintln!("Warning: Using backwards-compat format");
}
```

---

## API Quick Reference

```rust
// Version targeting
use ue5_asset_utils::KainEngineTarget;
let target = KainEngineTarget::Ue5_4;
let engine_ver = target.as_serializer_version();

// Property IR
use ue5_asset_utils::{PropertyDef, PropertyValue};
let prop = PropertyDef::float("Speed", 600.0);

// Property conversion
use ue5_asset_utils::property_converter::convert_property_defs;
let props = convert_property_defs(&mut asset, &[prop]);

// Import management
use ue5_asset_utils::ImportBuilder;
let import = ImportBuilder::resolve_object_import(&mut asset, "/Script/Engine.Actor");
```

---

## Summary

`ue5-asset-utils` is the **shared foundation** for all KAIN `.uasset` generation:

- **Single version authority** — `KainEngineTarget` handles UE5 version targeting
- **Universal property IR** — `PropertyDef` / `PropertyValue` work across all asset types
- **Automatic conversion** — `property_converter` handles serialization complexity
- **Deduplicating imports** — `ImportBuilder` prevents bloated import tables

**Used by:** `ue5-blueprints`, `ue5-materials`, `ue5-editor`  
**Status:** Production-ready, comprehensive test coverage  
**Philosophy:** Don't repeat yourself — shared utilities for shared problems
