//! Shared property converter: `PropertyDef` → unreal_asset `Property`.
//!
//! This is the single implementation for converting the IR property types
//! into serialized UE5 tagged properties. Used by `ue5-blueprints`,
//! `ue5-materials`, and any future asset writer.

use std::io::Cursor;

use unreal_asset::Asset;
use unreal_asset_properties::{
    int_property::{
        BoolProperty, DoubleProperty, FloatProperty, Int64Property, IntProperty,
    },
    object_property::{ObjectProperty, SoftObjectPath, TopLevelAssetPath},
    str_property::{NameProperty, StrProperty},
    struct_property::StructProperty,
    enum_property::EnumProperty,
    array_property::ArrayProperty,
    color_property::LinearColorProperty,
    soft_path_property::{
        SoftObjectPathProperty, SoftObjectPathPropertyValue,
    },
    vector_property::{VectorProperty, RotatorProperty},
    Property,
};
use unreal_asset_base::types::vector::{Color, Vector};
use ordered_float::OrderedFloat;

use crate::property_types::{PropertyDef, PropertyValue};
use crate::import_builder::ImportBuilder;

// ─── Public API ──────────────────────────────────────────────────────────────

/// Convert a slice of `PropertyDef` into UE5 `Property` objects.
pub fn convert_property_defs(
    asset: &mut Asset<Cursor<Vec<u8>>>,
    defs: &[PropertyDef],
) -> Vec<Property> {
    defs.iter()
        .filter_map(|def| convert_property_def(asset, def))
        .collect()
}

/// Convert a single `PropertyDef` into a UE5 `Property`.
///
/// Returns `None` for types that can't be mapped (shouldn't happen with
/// well-formed IR).
pub fn convert_property_def(
    asset: &mut Asset<Cursor<Vec<u8>>>,
    def: &PropertyDef,
) -> Option<Property> {
    let name = asset.add_fname(&def.name);
    match &def.value {
        PropertyValue::Bool(v) => Some(
            BoolProperty {
                name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: *v,
            }
            .into(),
        ),

        PropertyValue::Int(v) => Some(
            IntProperty {
                name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: *v,
            }
            .into(),
        ),

        PropertyValue::Int64(v) => Some(
            Int64Property {
                name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: *v,
            }
            .into(),
        ),

        PropertyValue::Float(v) => Some(
            FloatProperty {
                name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: OrderedFloat(*v),
            }
            .into(),
        ),

        PropertyValue::Double(v) => Some(
            DoubleProperty {
                name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: OrderedFloat(*v),
            }
            .into(),
        ),

        PropertyValue::Str(v) => Some(
            StrProperty {
                name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: Some(v.clone()),
            }
            .into(),
        ),

        PropertyValue::Name(v) => {
            let val = asset.add_fname(v);
            Some(
                NameProperty {
                    name,
                    ancestry: Default::default(),
                    property_guid: None,
                    duplication_index: 0,
                    value: val,
                }
                .into(),
            )
        }

        PropertyValue::Text(v) => Some(
            StrProperty {
                name,
                ancestry: Default::default(),
                property_guid: None,
                duplication_index: 0,
                value: Some(v.clone()),
            }
            .into(),
        ),

        PropertyValue::SoftObject(path) => {
            let (asset_path_str, sub_path) = split_soft_path(path);
            let asset_path_fname = asset.add_fname(&asset_path_str);
            Some(
                SoftObjectPathProperty {
                    name,
                    ancestry: Default::default(),
                    property_guid: None,
                    duplication_index: 0,
                    value: SoftObjectPathPropertyValue::New(SoftObjectPath {
                        asset_path: TopLevelAssetPath {
                            package_name: Some(asset_path_fname),
                            asset_name: asset.add_fname(""),
                        },
                        sub_path_string: sub_path,
                    }),
                }
                .into(),
            )
        }

        PropertyValue::Vector { x, y, z } => {
            let struct_type = asset.add_fname("Vector");
            Some(
                StructProperty {
                    name,
                    ancestry: Default::default(),
                    property_guid: None,
                    duplication_index: 0,
                    struct_type: Some(struct_type),
                    struct_guid: Some(Default::default()),
                    serialize_none: true,
                    value: vec![VectorProperty {
                        name: Default::default(),
                        ancestry: Default::default(),
                        property_guid: None,
                        duplication_index: 0,
                        value: Vector::new(
                            OrderedFloat(*x as f64),
                            OrderedFloat(*y as f64),
                            OrderedFloat(*z as f64),
                        ),
                    }
                    .into()],
                }
                .into(),
            )
        }

        PropertyValue::Rotator { pitch, yaw, roll } => {
            let struct_type = asset.add_fname("Rotator");
            Some(
                StructProperty {
                    name,
                    ancestry: Default::default(),
                    property_guid: None,
                    duplication_index: 0,
                    struct_type: Some(struct_type),
                    struct_guid: Some(Default::default()),
                    serialize_none: true,
                    value: vec![RotatorProperty {
                        name: Default::default(),
                        ancestry: Default::default(),
                        property_guid: None,
                        duplication_index: 0,
                        value: Vector::new(
                            OrderedFloat(*pitch as f64),
                            OrderedFloat(*yaw as f64),
                            OrderedFloat(*roll as f64),
                        ),
                    }
                    .into()],
                }
                .into(),
            )
        }

        PropertyValue::LinearColor { r, g, b, a } => {
            let struct_type = asset.add_fname("LinearColor");
            Some(
                StructProperty {
                    name,
                    ancestry: Default::default(),
                    property_guid: None,
                    duplication_index: 0,
                    struct_type: Some(struct_type),
                    struct_guid: Some(Default::default()),
                    serialize_none: true,
                    value: vec![LinearColorProperty {
                        name: Default::default(),
                        ancestry: Default::default(),
                        property_guid: None,
                        duplication_index: 0,
                        color: Color::new(
                            OrderedFloat(*r),
                            OrderedFloat(*g),
                            OrderedFloat(*b),
                            OrderedFloat(*a),
                        ),
                    }
                    .into()],
                }
                .into(),
            )
        }

        PropertyValue::Enum { enum_type, value } => {
            let enum_fname = asset.add_fname(enum_type);
            let val_str = format!("{}::{}", enum_type, value);
            let val_fname = asset.add_fname(&val_str);
            Some(
                EnumProperty {
                    name,
                    ancestry: Default::default(),
                    property_guid: None,
                    duplication_index: 0,
                    enum_type: Some(enum_fname),
                    inner_type: None,
                    value: Some(val_fname),
                }
                .into(),
            )
        }

        PropertyValue::ObjectRef(path) => {
            let resolved = ImportBuilder::resolve_object_import(asset, path);
            Some(
                ObjectProperty {
                    name,
                    ancestry: Default::default(),
                    property_guid: None,
                    duplication_index: 0,
                    value: resolved,
                }
                .into(),
            )
        }

        PropertyValue::Array {
            inner_type: _,
            values,
        } => {
            let inner_props: Vec<Property> = values
                .iter()
                .filter_map(|v| {
                    let temp_def = PropertyDef {
                        name: def.name.clone(),
                        value: v.clone(),
                    };
                    convert_property_def(asset, &temp_def)
                })
                .collect();

            let arr_type = asset.add_fname(&infer_array_inner_type(&def.value));
            Some(
                ArrayProperty {
                    name,
                    ancestry: Default::default(),
                    property_guid: None,
                    duplication_index: 0,
                    array_type: Some(arr_type),
                    value: inner_props,
                    dummy_property: None,
                }
                .into(),
            )
        }

        PropertyValue::Struct { struct_type, fields } => {
            let st = asset.add_fname(struct_type);
            let inner = convert_property_defs(asset, fields);
            Some(
                StructProperty {
                    name,
                    ancestry: Default::default(),
                    property_guid: None,
                    duplication_index: 0,
                    struct_type: Some(st),
                    struct_guid: Some(Default::default()),
                    serialize_none: true,
                    value: inner,
                }
                .into(),
            )
        }
    }
}

// ─── Internal helpers ────────────────────────────────────────────────────────

/// Split a soft object path like "/Game/Meshes/SM_Player.SM_Player" into
/// (asset_path, sub_path).
fn split_soft_path(path: &str) -> (String, Option<String>) {
    if let Some(dot_pos) = path.rfind('.') {
        let sub = &path[dot_pos + 1..];
        (path.to_string(), Some(sub.to_string()))
    } else {
        (path.to_string(), None)
    }
}

/// Infer the UE property type name for an array's inner type.
fn infer_array_inner_type(val: &PropertyValue) -> String {
    match val {
        PropertyValue::Array { inner_type, .. } => match inner_type.as_str() {
            "float" | "Float" => "FloatProperty",
            "int" | "Int" => "IntProperty",
            "bool" | "Bool" => "BoolProperty",
            "string" | "String" => "StrProperty",
            other => other,
        }
        .to_string(),
        _ => "StructProperty".to_string(),
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
    fn test_convert_bool() {
        let mut asset = empty_asset();
        let def = PropertyDef::bool("bEnabled", true);
        let prop = convert_property_def(&mut asset, &def);
        assert!(prop.is_some());
    }

    #[test]
    fn test_convert_int() {
        let mut asset = empty_asset();
        let def = PropertyDef::int("Health", 100);
        let prop = convert_property_def(&mut asset, &def);
        assert!(prop.is_some());
    }

    #[test]
    fn test_convert_float() {
        let mut asset = empty_asset();
        let def = PropertyDef::float("Speed", 600.0);
        let prop = convert_property_def(&mut asset, &def);
        assert!(prop.is_some());
    }

    #[test]
    fn test_convert_string() {
        let mut asset = empty_asset();
        let def = PropertyDef::str("DisplayName", "Test Actor");
        let prop = convert_property_def(&mut asset, &def);
        assert!(prop.is_some());
    }

    #[test]
    fn test_convert_vector() {
        let mut asset = empty_asset();
        let def = PropertyDef::vector("Location", 1.0, 2.0, 3.0);
        let prop = convert_property_def(&mut asset, &def);
        assert!(prop.is_some());
    }

    #[test]
    fn test_convert_color() {
        let mut asset = empty_asset();
        let def = PropertyDef::color("BaseColor", 1.0, 0.0, 0.0, 1.0);
        let prop = convert_property_def(&mut asset, &def);
        assert!(prop.is_some());
    }

    #[test]
    fn test_convert_enum() {
        let mut asset = empty_asset();
        let def = PropertyDef::enum_val("BlendMode", "EBlendMode", "Translucent");
        let prop = convert_property_def(&mut asset, &def);
        assert!(prop.is_some());
    }

    #[test]
    fn test_convert_soft_object() {
        let mut asset = empty_asset();
        let def = PropertyDef::soft_object("Mesh", "/Game/Meshes/SM_Cube.SM_Cube");
        let prop = convert_property_def(&mut asset, &def);
        assert!(prop.is_some());
    }

    #[test]
    fn test_convert_nested_struct() {
        let mut asset = empty_asset();
        let def = PropertyDef::new(
            "Transform",
            PropertyValue::Struct {
                struct_type: "Transform".to_string(),
                fields: vec![
                    PropertyDef::vector("Location", 0.0, 0.0, 0.0),
                    PropertyDef::float("Scale", 1.0),
                ],
            },
        );
        let prop = convert_property_def(&mut asset, &def);
        assert!(prop.is_some());
    }

    #[test]
    fn test_split_soft_path() {
        let (path, sub) = split_soft_path("/Game/Meshes/SM_Cube.SM_Cube");
        assert_eq!(path, "/Game/Meshes/SM_Cube.SM_Cube");
        assert_eq!(sub, Some("SM_Cube".to_string()));
    }

    #[test]
    fn test_split_soft_path_no_dot() {
        let (path, sub) = split_soft_path("/Game/Meshes/SM_Cube");
        assert_eq!(path, "/Game/Meshes/SM_Cube");
        assert_eq!(sub, None);
    }
}
