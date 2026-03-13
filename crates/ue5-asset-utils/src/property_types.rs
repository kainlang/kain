//! Shared property IR types for UE5 asset generation.
//!
//! `PropertyDef` and `PropertyValue` are the universal intermediate representation
//! for UE5 tagged properties. They map 1:1 to UE5 serialized property types and
//! are consumed by `property_converter::convert_property_def()`.

use serde::{Deserialize, Serialize};

/// A typed property value for a ClassDefaultObject, component default, or any
/// UE5 tagged property. Maps 1:1 to UE5 tagged property serialization.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum PropertyValue {
    Bool(bool),
    Int(i32),
    Int64(i64),
    Float(f32),
    Double(f64),
    Str(String),
    Name(String),
    /// FSoftObjectPath — e.g. "/Game/Meshes/SM_Player.SM_Player"
    SoftObject(String),
    /// Hard object reference — e.g. "/Script/Engine.StaticMesh"
    ObjectRef(String),
    Vector {
        x: f32,
        y: f32,
        z: f32,
    },
    Rotator {
        pitch: f32,
        yaw: f32,
        roll: f32,
    },
    LinearColor {
        r: f32,
        g: f32,
        b: f32,
        a: f32,
    },
    Enum {
        enum_type: String,
        value: String,
    },
    Text(String),
    Array {
        inner_type: String,
        values: Vec<PropertyValue>,
    },
    Struct {
        struct_type: String,
        fields: Vec<PropertyDef>,
    },
}

/// One tagged property: name + value.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PropertyDef {
    pub name: String,
    pub value: PropertyValue,
}

impl PropertyDef {
    pub fn new(name: impl Into<String>, value: PropertyValue) -> Self {
        Self {
            name: name.into(),
            value,
        }
    }

    // ── Ergonomic constructors ───────────────────────────────────────────────

    pub fn bool(name: impl Into<String>, v: bool) -> Self {
        Self::new(name, PropertyValue::Bool(v))
    }
    pub fn int(name: impl Into<String>, v: i32) -> Self {
        Self::new(name, PropertyValue::Int(v))
    }
    pub fn int64(name: impl Into<String>, v: i64) -> Self {
        Self::new(name, PropertyValue::Int64(v))
    }
    pub fn float(name: impl Into<String>, v: f32) -> Self {
        Self::new(name, PropertyValue::Float(v))
    }
    pub fn double(name: impl Into<String>, v: f64) -> Self {
        Self::new(name, PropertyValue::Double(v))
    }
    pub fn str(name: impl Into<String>, v: impl Into<String>) -> Self {
        Self::new(name, PropertyValue::Str(v.into()))
    }
    pub fn name_prop(name: impl Into<String>, v: impl Into<String>) -> Self {
        Self::new(name, PropertyValue::Name(v.into()))
    }
    pub fn soft_object(name: impl Into<String>, path: impl Into<String>) -> Self {
        Self::new(name, PropertyValue::SoftObject(path.into()))
    }
    pub fn object_ref(name: impl Into<String>, path: impl Into<String>) -> Self {
        Self::new(name, PropertyValue::ObjectRef(path.into()))
    }
    pub fn vector(name: impl Into<String>, x: f32, y: f32, z: f32) -> Self {
        Self::new(name, PropertyValue::Vector { x, y, z })
    }
    pub fn rotator(name: impl Into<String>, pitch: f32, yaw: f32, roll: f32) -> Self {
        Self::new(name, PropertyValue::Rotator { pitch, yaw, roll })
    }
    pub fn color(name: impl Into<String>, r: f32, g: f32, b: f32, a: f32) -> Self {
        Self::new(name, PropertyValue::LinearColor { r, g, b, a })
    }
    pub fn enum_val(
        name: impl Into<String>,
        enum_type: impl Into<String>,
        value: impl Into<String>,
    ) -> Self {
        Self::new(
            name,
            PropertyValue::Enum {
                enum_type: enum_type.into(),
                value: value.into(),
            },
        )
    }
    pub fn text(name: impl Into<String>, v: impl Into<String>) -> Self {
        Self::new(name, PropertyValue::Text(v.into()))
    }
}
