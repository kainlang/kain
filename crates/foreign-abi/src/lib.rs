use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};

pub const FOREIGN_ABI_SCHEMA_VERSION: &str = "kain-foreign-abi-v1";
pub const MAX_REPRESENTABLE_POINTER_DEPTH: u8 = u8::MAX;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ForeignBaseKind {
    Scalar,
    Typedef,
    Struct,
    Enum,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ForeignDirection {
    In,
    Out,
    InOut,
    Return,
    Unknown,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ForeignOwnershipPolicy {
    Borrowed,
    Owned,
    Shared,
    Pinned,
    External,
    Unknown,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ForeignCallingConvention {
    C,
    Stdcall,
    Fastcall,
    Thiscall,
    Vectorcall,
    Unknown,
}

impl Default for ForeignCallingConvention {
    fn default() -> Self {
        Self::C
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ForeignAbiType {
    Void,
    Scalar(ForeignScalar),
    Named(ForeignNamedType),
    Pointer(ForeignPointer),
    Array(ForeignArray),
    FunctionPointer(ForeignFunctionPointer),
    Aggregate(ForeignAggregate),
}

impl ForeignAbiType {
    pub fn max_pointer_depth(&self) -> u8 {
        match self {
            Self::Pointer(pointer) => pointer
                .depth
                .max(pointer.pointee.max_pointer_depth().saturating_add(1)),
            Self::Array(array) => array.element.max_pointer_depth(),
            Self::FunctionPointer(callback) => callback
                .signature
                .parameters
                .iter()
                .map(|param| param.ty.max_pointer_depth())
                .chain(std::iter::once(
                    callback.signature.return_type.max_pointer_depth(),
                ))
                .max()
                .unwrap_or(0),
            Self::Aggregate(aggregate) => aggregate
                .fields
                .iter()
                .map(|field| field.ty.max_pointer_depth())
                .max()
                .unwrap_or(0),
            Self::Void | Self::Scalar(_) | Self::Named(_) => 0,
        }
    }

    pub fn contains_callback(&self) -> bool {
        match self {
            Self::FunctionPointer(_) => true,
            Self::Pointer(pointer) => pointer.pointee.contains_callback(),
            Self::Array(array) => array.element.contains_callback(),
            Self::Aggregate(aggregate) => aggregate
                .fields
                .iter()
                .any(|field| field.ty.contains_callback()),
            Self::Void | Self::Scalar(_) | Self::Named(_) => false,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ForeignNamedType {
    pub kind: ForeignBaseKind,
    pub name: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ForeignPointer {
    pub pointee: Box<ForeignAbiType>,
    pub depth: u8,
    pub mutable: bool,
    pub nullable: bool,
    pub direction: ForeignDirection,
    pub ownership: ForeignOwnershipPolicy,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ForeignArray {
    pub element: Box<ForeignAbiType>,
    pub len: Option<u64>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ForeignAggregate {
    pub name: String,
    pub fields: Vec<ForeignField>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ForeignField {
    pub name: String,
    pub ty: ForeignAbiType,
    pub offset_bits: Option<u64>,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ForeignFunctionPointer {
    pub signature: ForeignFunctionSignature,
    pub nullable: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ForeignFunctionSignature {
    pub name: Option<String>,
    pub calling_convention: ForeignCallingConvention,
    pub return_type: Box<ForeignAbiType>,
    pub parameters: Vec<ForeignParameter>,
    pub variadic: bool,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ForeignParameter {
    pub name: String,
    pub ty: ForeignAbiType,
    pub direction: ForeignDirection,
    pub ownership: ForeignOwnershipPolicy,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ForeignScalar {
    pub canonical_name: String,
    pub kind: ForeignScalarKind,
    pub rust_ffi_type: String,
    pub kain_surface_type: String,
    pub bits: Option<u16>,
    pub signed: Option<bool>,
}

impl ForeignScalar {
    pub fn bridge_class(&self) -> ForeignBridgeClass {
        match self.kind {
            ForeignScalarKind::Void => ForeignBridgeClass::Unit,
            ForeignScalarKind::Bool => ForeignBridgeClass::Bool,
            ForeignScalarKind::SignedInteger => ForeignBridgeClass::SignedInt {
                rust_ffi_type: self.rust_ffi_type.clone(),
            },
            ForeignScalarKind::UnsignedInteger => ForeignBridgeClass::UnsignedInt {
                rust_ffi_type: self.rust_ffi_type.clone(),
            },
            ForeignScalarKind::Float32 => ForeignBridgeClass::Float32,
            ForeignScalarKind::Float64 => ForeignBridgeClass::Float64,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ForeignScalarKind {
    Void,
    Bool,
    SignedInteger,
    UnsignedInteger,
    Float32,
    Float64,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ScalarTypeTable {
    entries: BTreeMap<String, ForeignScalar>,
}

impl ScalarTypeTable {
    pub fn new() -> Self {
        Self {
            entries: BTreeMap::new(),
        }
    }

    pub fn with_default_c_entries() -> Self {
        let mut table = Self::new();
        table.insert("void", ForeignScalarKind::Void, "()", "Void", None, None);
        table.insert(
            "bool",
            ForeignScalarKind::Bool,
            "bool",
            "Bool",
            Some(1),
            None,
        );
        table.insert(
            "_Bool",
            ForeignScalarKind::Bool,
            "bool",
            "Bool",
            Some(1),
            None,
        );
        table.insert(
            "char",
            ForeignScalarKind::SignedInteger,
            "std::os::raw::c_char",
            "Int",
            Some(8),
            Some(true),
        );
        table.insert(
            "signed char",
            ForeignScalarKind::SignedInteger,
            "std::os::raw::c_char",
            "Int",
            Some(8),
            Some(true),
        );
        table.insert(
            "unsigned char",
            ForeignScalarKind::UnsignedInteger,
            "u8",
            "Int",
            Some(8),
            Some(false),
        );
        table.insert(
            "short",
            ForeignScalarKind::SignedInteger,
            "std::os::raw::c_short",
            "Int",
            Some(16),
            Some(true),
        );
        table.insert(
            "unsigned short",
            ForeignScalarKind::UnsignedInteger,
            "std::os::raw::c_ushort",
            "Int",
            Some(16),
            Some(false),
        );
        table.insert(
            "int",
            ForeignScalarKind::SignedInteger,
            "std::os::raw::c_int",
            "Int",
            Some(32),
            Some(true),
        );
        table.insert(
            "unsigned int",
            ForeignScalarKind::UnsignedInteger,
            "std::os::raw::c_uint",
            "Int",
            Some(32),
            Some(false),
        );
        table.insert(
            "long",
            ForeignScalarKind::SignedInteger,
            "std::os::raw::c_long",
            "Int",
            None,
            Some(true),
        );
        table.insert(
            "unsigned long",
            ForeignScalarKind::UnsignedInteger,
            "std::os::raw::c_ulong",
            "Int",
            None,
            Some(false),
        );
        table.insert(
            "long long",
            ForeignScalarKind::SignedInteger,
            "std::os::raw::c_longlong",
            "Int",
            Some(64),
            Some(true),
        );
        table.insert(
            "unsigned long long",
            ForeignScalarKind::UnsignedInteger,
            "std::os::raw::c_ulonglong",
            "Int",
            Some(64),
            Some(false),
        );
        table.insert(
            "float",
            ForeignScalarKind::Float32,
            "f32",
            "Float",
            Some(32),
            None,
        );
        table.insert(
            "double",
            ForeignScalarKind::Float64,
            "f64",
            "Float",
            Some(64),
            None,
        );
        table.insert(
            "size_t",
            ForeignScalarKind::UnsignedInteger,
            "usize",
            "Int",
            None,
            Some(false),
        );
        table.insert(
            "ptrdiff_t",
            ForeignScalarKind::SignedInteger,
            "isize",
            "Int",
            None,
            Some(true),
        );
        table.insert(
            "intptr_t",
            ForeignScalarKind::SignedInteger,
            "isize",
            "Int",
            None,
            Some(true),
        );
        table.insert(
            "uintptr_t",
            ForeignScalarKind::UnsignedInteger,
            "usize",
            "Int",
            None,
            Some(false),
        );
        table.insert(
            "int8_t",
            ForeignScalarKind::SignedInteger,
            "i8",
            "Int",
            Some(8),
            Some(true),
        );
        table.insert(
            "uint8_t",
            ForeignScalarKind::UnsignedInteger,
            "u8",
            "Int",
            Some(8),
            Some(false),
        );
        table.insert(
            "int16_t",
            ForeignScalarKind::SignedInteger,
            "i16",
            "Int",
            Some(16),
            Some(true),
        );
        table.insert(
            "uint16_t",
            ForeignScalarKind::UnsignedInteger,
            "u16",
            "Int",
            Some(16),
            Some(false),
        );
        table.insert(
            "int32_t",
            ForeignScalarKind::SignedInteger,
            "i32",
            "Int",
            Some(32),
            Some(true),
        );
        table.insert(
            "uint32_t",
            ForeignScalarKind::UnsignedInteger,
            "u32",
            "Int",
            Some(32),
            Some(false),
        );
        table.insert(
            "int64_t",
            ForeignScalarKind::SignedInteger,
            "i64",
            "Int",
            Some(64),
            Some(true),
        );
        table.insert(
            "uint64_t",
            ForeignScalarKind::UnsignedInteger,
            "u64",
            "Int",
            Some(64),
            Some(false),
        );
        table
    }

    pub fn insert(
        &mut self,
        canonical_name: &str,
        kind: ForeignScalarKind,
        rust_ffi_type: &str,
        kain_surface_type: &str,
        bits: Option<u16>,
        signed: Option<bool>,
    ) {
        let scalar = ForeignScalar {
            canonical_name: normalize_c_type_name(canonical_name),
            kind,
            rust_ffi_type: rust_ffi_type.to_string(),
            kain_surface_type: kain_surface_type.to_string(),
            bits,
            signed,
        };
        self.entries
            .insert(normalize_c_type_name(canonical_name), scalar);
    }

    pub fn get(&self, name: &str) -> Option<&ForeignScalar> {
        self.entries.get(&normalize_c_type_name(name))
    }

    pub fn contains(&self, name: &str) -> bool {
        self.get(name).is_some()
    }

    pub fn bridge_class_for(&self, name: &str) -> Option<ForeignBridgeClass> {
        self.get(name).map(ForeignScalar::bridge_class)
    }

    pub fn entries(&self) -> &BTreeMap<String, ForeignScalar> {
        &self.entries
    }
}

impl Default for ScalarTypeTable {
    fn default() -> Self {
        Self::with_default_c_entries()
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ForeignBridgeClass {
    Unit,
    Bool,
    SignedInt {
        rust_ffi_type: String,
    },
    UnsignedInt {
        rust_ffi_type: String,
    },
    Float32,
    Float64,
    CString,
    ByteBuffer {
        mutable: bool,
        element_type: String,
    },
    OpaqueHandle {
        mutable: bool,
        pointee: String,
        pointer_depth: u8,
    },
    RawPointer {
        mutable: bool,
        pointee: String,
        pointer_depth: u8,
        ownership: ForeignOwnershipPolicy,
    },
    Callback {
        mutable: bool,
        signature: String,
    },
    ByValueAggregate {
        kind: ForeignBaseKind,
        name: String,
    },
}

impl ForeignBridgeClass {
    pub fn pointer_depth(&self) -> u8 {
        match self {
            Self::ByteBuffer { .. } | Self::OpaqueHandle { .. } => 1,
            Self::RawPointer { pointer_depth, .. } => *pointer_depth,
            Self::Callback { .. } => 1,
            Self::Unit
            | Self::Bool
            | Self::SignedInt { .. }
            | Self::UnsignedInt { .. }
            | Self::Float32
            | Self::Float64
            | Self::CString
            | Self::ByValueAggregate { .. } => 0,
        }
    }

    pub fn requires_external_ownership_contract(&self) -> bool {
        matches!(
            self,
            Self::RawPointer { .. } | Self::Callback { .. } | Self::ByValueAggregate { .. }
        )
    }

    pub fn render_kain_surface(&self) -> &'static str {
        match self {
            Self::Unit => "Void",
            Self::Bool => "Bool",
            Self::SignedInt { .. } | Self::UnsignedInt { .. } => "Int",
            Self::Float32 | Self::Float64 => "Float",
            Self::CString => "String",
            Self::ByteBuffer { .. }
            | Self::OpaqueHandle { .. }
            | Self::RawPointer { .. }
            | Self::Callback { .. }
            | Self::ByValueAggregate { .. } => "Any",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct CBridgeTypeShape {
    pub base_kind: ForeignBaseKind,
    pub base_name: String,
    pub is_const: bool,
    pub pointer_depth: u8,
    pub has_array: bool,
    pub callback_like: bool,
    pub signature: Option<String>,
}

impl CBridgeTypeShape {
    pub fn effective_pointer_depth(&self) -> Result<u8, ForeignAbiError> {
        if self.has_array {
            self.pointer_depth
                .checked_add(1)
                .ok_or_else(|| ForeignAbiError {
                    code: "pointer_depth_overflow".to_string(),
                    message: format!(
                    "array-to-pointer decay overflowed the representable pointer depth for '{}'",
                    self.base_name
                ),
                })
        } else {
            Ok(self.pointer_depth)
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ForeignAbiLoweringPolicy {
    pub byte_buffer_scalar_names: BTreeSet<String>,
    pub c_string_scalar_names: BTreeSet<String>,
    pub max_pointer_depth: u8,
    pub callbacks_as_raw_pointers: bool,
    pub multi_level_pointers_as_raw: bool,
}

impl ForeignAbiLoweringPolicy {
    pub fn c_bridge_default() -> Self {
        Self {
            byte_buffer_scalar_names: ["uint8_t", "unsigned char", "int8_t"]
                .into_iter()
                .map(normalize_c_type_name)
                .collect(),
            c_string_scalar_names: ["char"].into_iter().map(normalize_c_type_name).collect(),
            max_pointer_depth: MAX_REPRESENTABLE_POINTER_DEPTH,
            callbacks_as_raw_pointers: true,
            multi_level_pointers_as_raw: true,
        }
    }

    pub fn is_byte_buffer_scalar(&self, name: &str) -> bool {
        self.byte_buffer_scalar_names
            .contains(&normalize_c_type_name(name))
    }

    pub fn is_c_string_scalar(&self, name: &str) -> bool {
        self.c_string_scalar_names
            .contains(&normalize_c_type_name(name))
    }
}

impl Default for ForeignAbiLoweringPolicy {
    fn default() -> Self {
        Self::c_bridge_default()
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ForeignAbiError {
    pub code: String,
    pub message: String,
}

impl std::fmt::Display for ForeignAbiError {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(formatter, "{}: {}", self.code, self.message)
    }
}

impl std::error::Error for ForeignAbiError {}

pub fn classify_c_bridge_type(
    shape: &CBridgeTypeShape,
    scalars: &ScalarTypeTable,
    policy: &ForeignAbiLoweringPolicy,
) -> Result<ForeignBridgeClass, ForeignAbiError> {
    let pointer_depth = shape.effective_pointer_depth()?;
    if pointer_depth > policy.max_pointer_depth {
        return Err(ForeignAbiError {
            code: "pointer_depth_exceeds_policy".to_string(),
            message: format!(
                "pointer depth {} for '{}' exceeds policy limit {}",
                pointer_depth, shape.base_name, policy.max_pointer_depth
            ),
        });
    }

    let base_name = normalize_c_type_name(&shape.base_name);
    if shape.callback_like {
        if policy.callbacks_as_raw_pointers {
            return Ok(ForeignBridgeClass::Callback {
                mutable: !shape.is_const,
                signature: shape
                    .signature
                    .clone()
                    .unwrap_or_else(|| format!("{} callback", base_name)),
            });
        }
        return Err(ForeignAbiError {
            code: "callback_pointer_rejected_by_policy".to_string(),
            message: format!(
                "callback pointer '{}' is rejected by the active policy",
                base_name
            ),
        });
    }

    if pointer_depth > 0 {
        if pointer_depth == 1 && policy.is_c_string_scalar(&base_name) {
            return Ok(ForeignBridgeClass::CString);
        }
        if pointer_depth == 1 && policy.is_byte_buffer_scalar(&base_name) {
            return Ok(ForeignBridgeClass::ByteBuffer {
                mutable: !shape.is_const,
                element_type: base_name,
            });
        }
        if pointer_depth == 1
            && matches!(
                shape.base_kind,
                ForeignBaseKind::Struct | ForeignBaseKind::Enum | ForeignBaseKind::Typedef
            )
            && scalars.get(&base_name).is_none()
        {
            return Ok(ForeignBridgeClass::OpaqueHandle {
                mutable: !shape.is_const,
                pointee: base_name,
                pointer_depth,
            });
        }
        if pointer_depth > 1 && !policy.multi_level_pointers_as_raw {
            return Err(ForeignAbiError {
                code: "multi_level_pointer_rejected_by_policy".to_string(),
                message: format!(
                    "multi-level pointer '{}' with depth {} is rejected by the active policy",
                    base_name, pointer_depth
                ),
            });
        }
        return Ok(ForeignBridgeClass::RawPointer {
            mutable: !shape.is_const,
            pointee: base_name,
            pointer_depth,
            ownership: ForeignOwnershipPolicy::External,
        });
    }

    match shape.base_kind {
        ForeignBaseKind::Scalar => scalars
            .get(&base_name)
            .map(ForeignScalar::bridge_class)
            .ok_or_else(|| ForeignAbiError {
                code: "unknown_scalar_type".to_string(),
                message: format!("unknown scalar type '{}'", shape.base_name),
            }),
        ForeignBaseKind::Enum => Ok(ForeignBridgeClass::SignedInt {
            rust_ffi_type: "std::os::raw::c_int".to_string(),
        }),
        ForeignBaseKind::Typedef => {
            if let Some(scalar) = scalars.get(&base_name) {
                Ok(scalar.bridge_class())
            } else {
                Ok(ForeignBridgeClass::ByValueAggregate {
                    kind: shape.base_kind,
                    name: base_name,
                })
            }
        }
        ForeignBaseKind::Struct => Ok(ForeignBridgeClass::ByValueAggregate {
            kind: shape.base_kind,
            name: base_name,
        }),
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct BridgeSafetyReport {
    pub max_pointer_depth: u8,
    pub callback_pointer_count: usize,
    pub raw_pointer_count: usize,
    pub byte_buffer_count: usize,
    pub opaque_handle_count: usize,
    pub by_value_aggregate_count: usize,
    pub requires_external_ownership_contracts: bool,
    pub capabilities: BTreeSet<String>,
}

impl BridgeSafetyReport {
    pub fn from_bridge_classes<'a>(
        classes: impl IntoIterator<Item = &'a ForeignBridgeClass>,
    ) -> Self {
        let mut report = Self::default();
        for class in classes {
            report.observe(class);
        }
        report
    }

    pub fn observe(&mut self, class: &ForeignBridgeClass) {
        self.max_pointer_depth = self.max_pointer_depth.max(class.pointer_depth());
        if class.requires_external_ownership_contract() {
            self.requires_external_ownership_contracts = true;
        }
        match class {
            ForeignBridgeClass::ByteBuffer { .. } => {
                self.byte_buffer_count += 1;
                self.capabilities.insert("byte-buffer".to_string());
            }
            ForeignBridgeClass::OpaqueHandle { .. } => {
                self.opaque_handle_count += 1;
                self.capabilities.insert("opaque-handle".to_string());
            }
            ForeignBridgeClass::RawPointer { .. } => {
                self.raw_pointer_count += 1;
                self.capabilities.insert("raw-pointer".to_string());
                self.capabilities
                    .insert("external-ownership-contract".to_string());
            }
            ForeignBridgeClass::Callback { .. } => {
                self.callback_pointer_count += 1;
                self.capabilities.insert("callback-pointer".to_string());
                self.capabilities
                    .insert("external-ownership-contract".to_string());
            }
            ForeignBridgeClass::ByValueAggregate { .. } => {
                self.by_value_aggregate_count += 1;
                self.capabilities.insert("by-value-aggregate".to_string());
                self.capabilities
                    .insert("external-ownership-contract".to_string());
            }
            ForeignBridgeClass::Unit
            | ForeignBridgeClass::Bool
            | ForeignBridgeClass::SignedInt { .. }
            | ForeignBridgeClass::UnsignedInt { .. }
            | ForeignBridgeClass::Float32
            | ForeignBridgeClass::Float64
            | ForeignBridgeClass::CString => {}
        }
    }
}

pub fn normalize_c_type_name(raw: &str) -> String {
    raw.replace('\t', " ")
        .split_whitespace()
        .collect::<Vec<_>>()
        .join(" ")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn scalar_table_maps_common_typedefs() {
        let table = ScalarTypeTable::default();
        assert!(matches!(
            table.bridge_class_for("uint64_t"),
            Some(ForeignBridgeClass::UnsignedInt { rust_ffi_type }) if rust_ffi_type == "u64"
        ));
        assert!(matches!(
            table.bridge_class_for("size_t"),
            Some(ForeignBridgeClass::UnsignedInt { rust_ffi_type }) if rust_ffi_type == "usize"
        ));
    }

    #[test]
    fn multi_level_pointers_become_raw_external_handles() {
        let shape = CBridgeTypeShape {
            base_kind: ForeignBaseKind::Typedef,
            base_name: "VkInstance".to_string(),
            is_const: false,
            pointer_depth: 2,
            has_array: false,
            callback_like: false,
            signature: None,
        };
        let class = classify_c_bridge_type(
            &shape,
            &ScalarTypeTable::default(),
            &ForeignAbiLoweringPolicy::default(),
        )
        .expect("multi-level pointer should classify");
        assert!(matches!(
            class,
            ForeignBridgeClass::RawPointer {
                pointer_depth: 2,
                ownership: ForeignOwnershipPolicy::External,
                ..
            }
        ));
    }

    #[test]
    fn callback_pointers_are_represented_as_abi_shapes() {
        let shape = CBridgeTypeShape {
            base_kind: ForeignBaseKind::Typedef,
            base_name: "PFN_vkAllocationFunction".to_string(),
            is_const: false,
            pointer_depth: 1,
            has_array: false,
            callback_like: true,
            signature: Some("void* (*)(void*, size_t, size_t, int)".to_string()),
        };
        let class = classify_c_bridge_type(
            &shape,
            &ScalarTypeTable::default(),
            &ForeignAbiLoweringPolicy::default(),
        )
        .expect("callback should classify");
        assert!(matches!(class, ForeignBridgeClass::Callback { .. }));
    }

    #[test]
    fn safety_report_counts_raw_api_shapes() {
        let classes = vec![
            ForeignBridgeClass::RawPointer {
                mutable: true,
                pointee: "void".to_string(),
                pointer_depth: 3,
                ownership: ForeignOwnershipPolicy::External,
            },
            ForeignBridgeClass::Callback {
                mutable: false,
                signature: "PFN".to_string(),
            },
        ];
        let report = BridgeSafetyReport::from_bridge_classes(classes.iter());
        assert_eq!(report.max_pointer_depth, 3);
        assert_eq!(report.raw_pointer_count, 1);
        assert_eq!(report.callback_pointer_count, 1);
        assert!(report.requires_external_ownership_contracts);
    }
}
