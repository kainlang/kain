//! UDataAsset binary `.uasset` writer.
//!
//! Generates a UE5-loadable `.uasset` file for `UDataAsset` (and subclasses)
//! from a flat list of `PropertyDef` fields. This is the simplest possible
//! asset — a single export with a typed property bag.
//!
//! The resulting file can be dropped into a plugin's `Content/` folder and
//! will appear in the Content Browser immediately (or after an asset scan).
//!
//! # Usage
//!
//! ```ignore
//! use ue5_asset_utils::PropertyDef;
//! use ue5_editor::data_asset_writer::write_data_asset;
//! use unreal_asset::engine_version::EngineVersion;
//!
//! let fields = vec![
//!     PropertyDef::str("Name", "Iron Sword"),
//!     PropertyDef::int("Damage", 45),
//!     PropertyDef::float("Weight", 2.5),
//! ];
//! let bytes = write_data_asset(
//!     "DA_IronSword",
//!     "/Script/Engine.DataAsset",
//!     &fields,
//!     EngineVersion::VER_UE5_2,
//! ).expect("write_data_asset failed");
//! std::fs::write("DA_IronSword.uasset", bytes).ok();
//! ```

use std::io::Cursor;

use unreal_asset::{
    engine_version::EngineVersion,
    exports::{base_export::BaseExport, normal_export::NormalExport, Export},
    flags::EObjectFlags,
    types::PackageIndex,
    Asset,
};
use ue5_asset_utils::{
    import_builder::ImportBuilder,
    property_converter::convert_property_defs,
    property_types::PropertyDef,
};

// ─── Error type ──────────────────────────────────────────────────────────────

/// Errors that can occur during `UDataAsset` generation.
#[derive(Debug)]
pub enum DataAssetError {
    /// The class path string could not be parsed.
    InvalidClassPath(String),
    /// The `unreal_asset` serialization step failed.
    Serialization(String),
}

impl std::fmt::Display for DataAssetError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::InvalidClassPath(p) => write!(f, "invalid class path: {}", p),
            Self::Serialization(msg) => write!(f, "serialization error: {}", msg),
        }
    }
}

impl std::error::Error for DataAssetError {}

pub type Result<T> = std::result::Result<T, DataAssetError>;

// ─── Data-driven class aliases ───────────────────────────────────────────────

/// Short-name → full-path aliases for common data-asset base classes.
/// Extend this table instead of adding match arms.
const DATA_ASSET_CLASS_ALIASES: &[(&str, &str)] = &[
    ("DataAsset",         "/Script/Engine.DataAsset"),
    ("PrimaryDataAsset",  "/Script/Engine.PrimaryDataAsset"),
];

const DEFAULT_DATA_ASSET_CLASS: &str = "/Script/Engine.DataAsset";

// ─── Public API ──────────────────────────────────────────────────────────────

/// Write a `UDataAsset` `.uasset` file from a flat list of `PropertyDef` fields.
///
/// # Arguments
///
/// * `name`           — Asset object name, e.g. `"DA_ItemData"`
/// * `class_path`     — Full class path (e.g. `"/Script/Engine.DataAsset"`)
///                      or short alias (e.g. `"DataAsset"`, `"PrimaryDataAsset"`)
/// * `fields`         — Typed property values for the export
/// * `engine_version` — Target engine version for the binary format
///
/// # Returns
///
/// The raw bytes of a well-formed `.uasset` file.
pub fn write_data_asset(
    name: &str,
    class_path: &str,
    fields: &[PropertyDef],
    engine_version: EngineVersion,
) -> Result<Vec<u8>> {
    let mut asset = Asset::new_empty(engine_version);

    // ── Resolve class path ──────────────────────────────────────────────────
    let resolved_class = resolve_class_path(class_path);
    let (pkg_path, class_name) = ImportBuilder::parse_class_path(&resolved_class);

    // ── Imports ─────────────────────────────────────────────────────────────
    let pkg_import = ImportBuilder::get_or_add_package(&mut asset, &pkg_path);
    let class_import = ImportBuilder::get_or_add_class(&mut asset, &class_name, pkg_import);

    // ── Properties ──────────────────────────────────────────────────────────
    let properties = convert_property_defs(&mut asset, fields);

    // ── Single export — the DataAsset instance ──────────────────────────────
    let object_name = asset.add_fname(name);

    let export = NormalExport {
        base_export: BaseExport {
            class_index: class_import,
            super_index: PackageIndex::new(0),
            template_index: PackageIndex::new(0),
            outer_index: PackageIndex::new(0),
            object_name,
            object_flags: EObjectFlags::RF_PUBLIC | EObjectFlags::RF_STANDALONE,
            ..Default::default()
        },
        properties,
        extras: Vec::new(),
    };
    asset.asset_data.exports.push(Export::NormalExport(export));

    // ── Serialize ───────────────────────────────────────────────────────────
    asset.rebuild_name_map();
    let mut cursor = Cursor::new(Vec::new());
    asset
        .write_data(&mut cursor, None)
        .map_err(|e| DataAssetError::Serialization(format!("{}", e)))?;

    Ok(cursor.into_inner())
}

/// Resolve a `@data_asset` attribute value to a full class path.
///
/// Rules (data-driven via `DATA_ASSET_CLASS_ALIASES`):
/// * No argument / empty → `/Script/Engine.DataAsset`
/// * Contains `/` → used as-is (full path)
/// * Short alias hit → replaced from the alias table
/// * Otherwise → `/Script/Engine.{value}` (assume Engine class)
pub fn resolve_data_asset_class(attributes: &[kain_core::ast::Attribute]) -> String {
    for attr in attributes {
        if attr.name == "data_asset" {
            if let Some(kain_core::ast::Expr::String(value, _)) = attr.args.first() {
                return resolve_class_path(value);
            }
        }
    }
    DEFAULT_DATA_ASSET_CLASS.to_string()
}

/// Convert a possibly-short class name to a full UE5 path.
fn resolve_class_path(path: &str) -> String {
    if path.is_empty() {
        return DEFAULT_DATA_ASSET_CLASS.to_string();
    }
    // Full path?
    if path.contains('/') {
        return path.to_string();
    }
    // Alias lookup
    if let Some((_, full)) = DATA_ASSET_CLASS_ALIASES.iter().find(|(k, _)| *k == path) {
        return full.to_string();
    }
    // Assume Engine namespace
    format!("/Script/Engine.{}", path)
}

// ─── AST → PropertyDef field conversion ─────────────────────────────────────

/// Convert `Struct.fields` (from `kain_core::ast::Field`) into `PropertyDef` values.
///
/// This reuses the same conversion logic proven in `ue5-blueprints/conversion.rs`.
pub fn fields_from_struct(st: &kain_core::ast::Struct) -> Vec<PropertyDef> {
    st.fields
        .iter()
        .filter_map(|field| {
            convert_field_expr_to_property(&field.name, &field.default, Some(&field.ty))
        })
        .collect()
}

/// Convert an optional default expression + type hint to a PropertyDef.
fn convert_field_expr_to_property(
    name: &str,
    default: &Option<kain_core::ast::Expr>,
    ty_hint: Option<&kain_core::ast::Type>,
) -> Option<PropertyDef> {
    match default {
        Some(expr) => convert_expr_to_property(name, expr, ty_hint),
        None => {
            // No default — emit a type-appropriate zero value if possible
            ty_hint.and_then(|ty| zero_value_for_type(name, ty))
        }
    }
}

/// Generate a zero/empty PropertyDef for a type with no explicit default.
fn zero_value_for_type(name: &str, ty: &kain_core::ast::Type) -> Option<PropertyDef> {
    match ty {
        kain_core::ast::Type::Named { name: ty_name, .. } => {
            match ty_name.as_str() {
                "Float" | "f32" | "f64" => Some(PropertyDef::float(name, 0.0)),
                "Int" | "i32" | "i64" => Some(PropertyDef::int(name, 0)),
                "Bool" => Some(PropertyDef::bool(name, false)),
                "String" => Some(PropertyDef::str(name, "")),
                _ => None,
            }
        }
        _ => None,
    }
}

/// Convert an AST expression to a PropertyDef.
/// Mirrors `ue5-blueprints/conversion.rs :: convert_expr_to_property` exactly.
fn convert_expr_to_property(
    name: &str,
    expr: &kain_core::ast::Expr,
    _ty_hint: Option<&kain_core::ast::Type>,
) -> Option<PropertyDef> {
    use kain_core::ast;
    match expr {
        ast::Expr::Float(v, _) => Some(PropertyDef::float(name, *v as f32)),
        ast::Expr::Int(v, _) => Some(PropertyDef::int(name, *v as i32)),
        ast::Expr::Bool(v, _) => Some(PropertyDef::bool(name, *v)),
        ast::Expr::String(v, _) => {
            if v.starts_with("/Game/") || v.starts_with("/Script/") {
                Some(PropertyDef::soft_object(name, v))
            } else {
                Some(PropertyDef::str(name, v))
            }
        }
        ast::Expr::Call { callee, args, .. } => {
            if let ast::Expr::Ident(func_name, _) = &**callee {
                match func_name.as_str() {
                    "vec3" if args.len() == 3 => {
                        let x = eval_float(&args[0].value).unwrap_or(0.0);
                        let y = eval_float(&args[1].value).unwrap_or(0.0);
                        let z = eval_float(&args[2].value).unwrap_or(0.0);
                        Some(PropertyDef::vector(name, x, y, z))
                    }
                    "rotator" if args.len() == 3 => {
                        let p = eval_float(&args[0].value).unwrap_or(0.0);
                        let y = eval_float(&args[1].value).unwrap_or(0.0);
                        let r = eval_float(&args[2].value).unwrap_or(0.0);
                        Some(PropertyDef::rotator(name, p, y, r))
                    }
                    "color" | "linear_color" if args.len() >= 3 => {
                        let r = eval_float(&args[0].value).unwrap_or(0.0);
                        let g = eval_float(&args[1].value).unwrap_or(0.0);
                        let b = eval_float(&args[2].value).unwrap_or(0.0);
                        let a = if args.len() >= 4 {
                            eval_float(&args[3].value).unwrap_or(1.0)
                        } else {
                            1.0
                        };
                        Some(PropertyDef::color(name, r, g, b, a))
                    }
                    _ => None,
                }
            } else {
                None
            }
        }
        ast::Expr::EnumVariant {
            enum_name,
            variant,
            ..
        } => Some(PropertyDef::enum_val(name, enum_name, variant)),
        ast::Expr::Struct {
            name: struct_name,
            fields,
            ..
        } => {
            let inner: Vec<PropertyDef> = fields
                .iter()
                .filter_map(|(fname, fexpr)| convert_expr_to_property(fname, fexpr, None))
                .collect();
            Some(PropertyDef {
                name: name.to_string(),
                value: ue5_asset_utils::property_types::PropertyValue::Struct {
                    struct_type: struct_name.clone(),
                    fields: inner,
                },
            })
        }
        _ => None,
    }
}

/// Evaluate a simple numeric expression to f32.
fn eval_float(expr: &kain_core::ast::Expr) -> Option<f32> {
    use kain_core::ast;
    match expr {
        ast::Expr::Float(v, _) => Some(*v as f32),
        ast::Expr::Int(v, _) => Some(*v as f32),
        ast::Expr::Unary {
            op: ast::UnaryOp::Neg,
            operand,
            ..
        } => eval_float(operand).map(|v| -v),
        _ => None,
    }
}

// ─── Tests ───────────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;
    use ue5_asset_utils::property_types::PropertyDef;
    use unreal_asset::engine_version::EngineVersion;

    /// UE5 binary magic header: 0xC1832A9E
    const UE5_MAGIC: [u8; 4] = [0xC1, 0x83, 0x2A, 0x9E];

    #[test]
    fn test_write_empty_data_asset() {
        let bytes = write_data_asset(
            "DA_Empty",
            "/Script/Engine.DataAsset",
            &[],
            EngineVersion::VER_UE5_2,
        )
        .expect("write_data_asset should succeed for empty fields");
        assert!(!bytes.is_empty());
        assert_eq!(&bytes[0..4], &UE5_MAGIC);
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
            "DA_IronSword",
            "/Script/Engine.DataAsset",
            &fields,
            EngineVersion::VER_UE5_2,
        )
        .expect("write_data_asset should succeed with fields");
        assert!(bytes.len() > 200, "expected > 200 bytes, got {}", bytes.len());
        assert_eq!(&bytes[0..4], &UE5_MAGIC);
    }

    #[test]
    fn test_write_data_asset_custom_class() {
        let bytes = write_data_asset(
            "DA_Weapon",
            "/Script/GameplayAbilities.UPrimaryDataAsset",
            &[PropertyDef::str("WeaponName", "Excalibur")],
            EngineVersion::VER_UE5_2,
        )
        .expect("write_data_asset should succeed for custom class");
        assert!(!bytes.is_empty());
        assert_eq!(&bytes[0..4], &UE5_MAGIC);
    }

    #[test]
    fn test_write_data_asset_with_vector_and_color() {
        let fields = vec![
            PropertyDef::vector("SpawnPoint", 100.0, 200.0, 50.0),
            PropertyDef::color("BaseColor", 1.0, 0.0, 0.5, 1.0),
            PropertyDef::rotator("InitialRotation", 0.0, 90.0, 0.0),
        ];
        let bytes = write_data_asset(
            "DA_SpawnConfig",
            "DataAsset",
            &fields,
            EngineVersion::VER_UE5_2,
        )
        .expect("write_data_asset should handle complex types");
        assert!(!bytes.is_empty());
        assert_eq!(&bytes[0..4], &UE5_MAGIC);
    }

    #[test]
    fn test_resolve_class_path_full() {
        let r = resolve_class_path("/Script/Engine.DataAsset");
        assert_eq!(r, "/Script/Engine.DataAsset");
    }

    #[test]
    fn test_resolve_class_path_alias() {
        assert_eq!(
            resolve_class_path("DataAsset"),
            "/Script/Engine.DataAsset"
        );
        assert_eq!(
            resolve_class_path("PrimaryDataAsset"),
            "/Script/Engine.PrimaryDataAsset"
        );
    }

    #[test]
    fn test_resolve_class_path_empty() {
        assert_eq!(
            resolve_class_path(""),
            "/Script/Engine.DataAsset"
        );
    }

    #[test]
    fn test_resolve_class_path_unknown() {
        assert_eq!(
            resolve_class_path("MyCustomDataAsset"),
            "/Script/Engine.MyCustomDataAsset"
        );
    }
}
