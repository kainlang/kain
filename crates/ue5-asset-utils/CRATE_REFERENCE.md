# ue5-asset-utils — Binary Asset Utilities Reference

> **Last Updated:** 2026-03-01
> **Status:** Production — UDataAsset writer and asset registry writer both complete. 26+ tests across both writers.

---

## Purpose

Low-level binary asset writing primitives for UE5 `.uasset` format. Used by `ue5-blueprints`, `ue5-materials`, and `ue5-graphs` as the foundation layer for binary asset output. Also hosts the engine target metadata system.

---

## Source Files

| File | Size | Purpose |
|---|---|---|
| `engine_target.rs` | 11KB | `EngineTarget` — UE5 installation detection, version management |
| `property_converter.rs` | 15KB | `PropertyConverter` — KAIN property values → UE5 serialized bytes |
| `import_builder.rs` | 8KB | `ImportTableBuilder` — UAsset import table construction |
| `property_types.rs` | 4KB | Property type enum + serialization metadata |

---

## Public API (`lib.rs`)

```rust
pub struct EngineTarget { ... }
pub struct PropertyConverter { ... }
pub struct ImportTableBuilder { ... }

impl EngineTarget {
    pub fn detect() -> Option<Self>
    pub fn detect_all() -> Vec<Self>
    pub fn version(&self) -> UE5Version
    pub fn engine_path(&self) -> &Path
    pub fn content_path(&self) -> &Path
}
```

---

## Engine Target (`engine_target.rs`, 11KB)

Detects UE5 installations across multiple drives:

- Scans all drive letters (A:\ through Z:\) for `Engine/Build/Build.version`
- Parses `Build.version` JSON for `MajorVersion`, `MinorVersion`, `PatchVersion`
- Supports UE5 versions 5.0 through 5.7
- Provides path helpers: `engine_path()`, `content_path()`, `plugins_path()`, `source_path()`

Multi-installation: `detect_all()` returns all found installations when multiple UE5 versions are installed.

### Version Parameterization

Binary asset serialization format differs between UE5 versions:

| UE5 Version | Format differences |
|---|---|
| 5.0 | Base format |
| 5.4+ | `AddedDependencyFlags` in asset registry format |
| 5.4+ | Additional `FPackageFileSummary` fields |

`EngineTarget::version()` feeds into serializer switches for these differences.

---

## Property Converter (`property_converter.rs`, 15KB)

Serializes KAIN property values to UE5 binary property format:

### Supported Property Types (14 total)

| KAIN type | UE5 property tag | Serialization |
|---|---|---|
| `Bool` | `BoolProperty` | 1-byte tag-value |
| `Int` | `IntProperty` | 4-byte little-endian |
| `Float` | `FloatProperty` | 4-byte IEEE 754 |
| `String` | `StrProperty` | Length-prefixed UTF-16 |
| `Name` (FName) | `NameProperty` | Index into name table |
| `Text` (FText) | `TextProperty` | Namespace/key/value triple |
| `Object` reference | `ObjectProperty` | Import/export index |
| `Class` reference | `ClassProperty` | Import index |
| `Soft Object` | `SoftObjectProperty` | Asset path string |
| `Soft Class` | `SoftClassProperty` | Asset class path string |
| `Enum` | `EnumProperty` | uint8 + enum class name |
| `Struct` | `StructProperty` | Recursive property block |
| `Array<T>` | `ArrayProperty` | Count + element blocks |
| `Map<K,V>` | `MapProperty` | Count + key-value pair blocks |

---

## Import Table Builder (`import_builder.rs`, 8KB)

Constructs the `ImportTable` section of a UAsset:

```rust
let mut builder = ImportTableBuilder::new();
let material_idx = builder.add_import("/Script/Engine", "Material", "/Game/Materials/M_Ground");
let texture_idx  = builder.add_import("/Script/Engine", "Texture2D", "/Game/Textures/T_Ground");
let table = builder.build();
```

Used by all binary asset writers (`ue5-blueprints`, `ue5-materials`, `ue5-graphs`) to reference engine-provided and project-provided assets.

---

## Tests

26+ tests across the module:

| Test category | Count | Coverage |
|---|---|---|
| Property round-trip | 14 | One test per property type — serialize then verify bytes |
| Engine detection | 4 | Drive scan, version parse, path helpers |
| Import table | 4 | Add import, index correctness, name table |
| Version parameterization | 4 | UE 5.0 vs 5.4+ format differences |
