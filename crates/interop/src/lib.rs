use std::collections::HashMap;
use std::sync::{Arc, Once, RwLock};

use kain_core::error::{KainError, KainResult};
use kain_core::gpu_storage_element_stride_bytes;
use kain_core::runtime::{register_env_extension, Env, Value};
use kain_core::stdlib::{register_stdlib_extension, BuiltinFn, StdLib};

const INTEROP_EXTENSION_KEY: &str = "kain.interop.shared";
pub const KAIN_SHARED_CONTRACT_VERSION: i64 = 1;

static REGISTER: Once = Once::new();

#[derive(Debug, Clone)]
pub struct SharedBufferMetadata {
    pub element_type: String,
    pub element_size: i64,
    pub shape: Vec<i64>,
    pub strides: Vec<i64>,
    pub format: Option<String>,
    pub mime_type: Option<String>,
    pub source_runtime: String,
    pub source_backend: Option<String>,
    pub ownership: String,
    pub labels: Vec<String>,
}

#[derive(Debug, Clone)]
pub struct SharedImageMetadata {
    pub representation: String,
    pub width: i64,
    pub height: i64,
    pub channels: i64,
    pub layout: String,
    pub pixel_format: String,
    pub mime_type: String,
    pub row_stride: i64,
    pub color_space: String,
    pub alpha_mode: String,
    pub source_runtime: String,
    pub source_backend: Option<String>,
    pub ownership: String,
    pub labels: Vec<String>,
}

#[derive(Clone)]
pub struct KainSharedBuffer {
    pub metadata: SharedBufferMetadata,
    bytes: Arc<RwLock<Vec<u8>>>,
}

#[derive(Clone)]
pub struct KainSharedImage {
    pub metadata: SharedImageMetadata,
    pub buffer: Arc<KainSharedBuffer>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum GpuDescriptorKind {
    StorageBuffer,
    UniformBuffer,
}

impl GpuDescriptorKind {
    pub fn as_str(self) -> &'static str {
        match self {
            GpuDescriptorKind::StorageBuffer => "storage_buffer",
            GpuDescriptorKind::UniformBuffer => "uniform_buffer",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum GpuBindingAccess {
    Read,
    Write,
    ReadWrite,
}

impl GpuBindingAccess {
    pub fn as_str(self) -> &'static str {
        match self {
            GpuBindingAccess::Read => "read",
            GpuBindingAccess::Write => "write",
            GpuBindingAccess::ReadWrite => "read_write",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SharedBufferGpuBindingView {
    pub contract: &'static str,
    pub byte_length: usize,
    pub element_type: String,
    pub element_size: i64,
    pub shape: Vec<i64>,
    pub strides: Vec<i64>,
    pub descriptor_kind: GpuDescriptorKind,
    pub access: GpuBindingAccess,
}

impl KainSharedBuffer {
    pub fn owned(metadata: SharedBufferMetadata, bytes: Vec<u8>) -> Arc<Self> {
        Arc::new(Self {
            metadata,
            bytes: Arc::new(RwLock::new(bytes)),
        })
    }

    pub fn replace_bytes(&self, bytes: Vec<u8>) -> KainResult<()> {
        let mut guard = self
            .bytes
            .write()
            .map_err(|_| KainError::runtime("failed to write shared buffer bytes"))?;
        *guard = bytes;
        Ok(())
    }

    pub fn bytes(&self) -> Vec<u8> {
        self.bytes.read().unwrap().clone()
    }

    pub fn byte_length(&self) -> usize {
        self.bytes.read().unwrap().len()
    }
}

pub fn shared_buffer_gpu_binding_view(
    buffer: &KainSharedBuffer,
    descriptor_kind: GpuDescriptorKind,
    access: GpuBindingAccess,
) -> KainResult<SharedBufferGpuBindingView> {
    let byte_length = buffer.byte_length();
    let required = required_shared_buffer_byte_length(&buffer.metadata)?;

    if required > 0 && byte_length != required {
        return Err(KainError::runtime(format!(
            "shared buffer byte length mismatch for GPU binding: expected {required}, got {byte_length}"
        )));
    }

    if matches!(descriptor_kind, GpuDescriptorKind::UniformBuffer)
        && !matches!(access, GpuBindingAccess::Read)
    {
        return Err(KainError::runtime(
            "uniform-buffer GPU bindings must be read-only".to_string(),
        ));
    }

    Ok(SharedBufferGpuBindingView {
        contract: "kain.shared.buffer",
        byte_length,
        element_type: buffer.metadata.element_type.clone(),
        element_size: buffer.metadata.element_size,
        shape: buffer.metadata.shape.clone(),
        strides: buffer.metadata.strides.clone(),
        descriptor_kind,
        access,
    })
}

impl KainSharedImage {
    pub fn owned(metadata: SharedImageMetadata, bytes: Vec<u8>) -> KainResult<Arc<Self>> {
        validate_shared_image_bytes(&metadata, bytes.len())?;

        let shape = if metadata.representation == "raster" && metadata.height > 0 {
            if metadata.channels > 0 {
                match metadata.layout.as_str() {
                    "HWC" => vec![metadata.height, metadata.width, metadata.channels],
                    "CHW" => vec![metadata.channels, metadata.height, metadata.width],
                    _ => vec![metadata.height, metadata.width, metadata.channels],
                }
            } else {
                vec![metadata.height, metadata.width]
            }
        } else {
            vec![bytes.len() as i64]
        };

        let strides = if metadata.representation == "raster" && metadata.height > 0 {
            if metadata.channels > 0 {
                match metadata.layout.as_str() {
                    "CHW" => vec![metadata.height * metadata.width, metadata.width, 1],
                    _ => vec![metadata.width * metadata.channels, metadata.channels, 1],
                }
            } else {
                vec![metadata.width, 1]
            }
        } else {
            vec![1]
        };

        let buffer = KainSharedBuffer::owned(
            SharedBufferMetadata {
                element_type: "u8".to_string(),
                element_size: 1,
                shape,
                strides,
                format: Some(metadata.pixel_format.clone()),
                mime_type: Some(metadata.mime_type.clone()),
                source_runtime: metadata.source_runtime.clone(),
                source_backend: metadata.source_backend.clone(),
                ownership: metadata.ownership.clone(),
                labels: metadata.labels.clone(),
            },
            bytes,
        );
        Ok(Arc::new(Self { metadata, buffer }))
    }

    pub fn bytes(&self) -> Vec<u8> {
        self.buffer.bytes()
    }

    pub fn replace_bytes(&self, bytes: Vec<u8>) -> KainResult<()> {
        validate_shared_image_bytes(&self.metadata, bytes.len())?;
        self.buffer.replace_bytes(bytes)
    }
}

fn validate_shared_image_bytes(metadata: &SharedImageMetadata, byte_len: usize) -> KainResult<()> {
    if metadata.representation == "raster" {
        let row_stride = if metadata.row_stride > 0 {
            metadata.row_stride as usize
        } else if metadata.width > 0 && metadata.channels > 0 {
            (metadata.width * metadata.channels) as usize
        } else {
            0
        };
        let expected = if metadata.height > 0 && row_stride > 0 {
            metadata.height as usize * row_stride
        } else {
            0
        };
        if expected > 0 && byte_len != expected {
            return Err(KainError::runtime(format!(
                "kain_shared_image: raster byte length mismatch, expected {expected}, got {byte_len}",
            )));
        }
    }
    Ok(())
}

pub fn register() {
    REGISTER.call_once(|| {
        register_stdlib_extension(INTEROP_EXTENSION_KEY, register_interop_stdlib);
        register_env_extension(INTEROP_EXTENSION_KEY, register_interop_env);
    });
}

pub fn shared_buffer_value(buffer: Arc<KainSharedBuffer>) -> Value {
    Value::host_object("kain.shared.buffer", buffer)
}

pub fn shared_image_value(image: Arc<KainSharedImage>) -> Value {
    Value::host_object("kain.shared.image", image)
}

pub fn extract_shared_buffer(value: &Value) -> KainResult<Arc<KainSharedBuffer>> {
    value
        .downcast_host_object::<KainSharedBuffer>()
        .ok_or_else(|| KainError::runtime("expected a Kain shared buffer"))
}

pub fn extract_shared_image(value: &Value) -> KainResult<Arc<KainSharedImage>> {
    value
        .downcast_host_object::<KainSharedImage>()
        .ok_or_else(|| KainError::runtime("expected a Kain shared image"))
}

pub fn shared_buffer_info_value(buffer: &KainSharedBuffer) -> Value {
    let mut fields = HashMap::new();
    fields.insert(
        "contract".to_string(),
        Value::String("kain.shared.buffer".to_string()),
    );
    fields.insert(
        "contract_version".to_string(),
        Value::Int(KAIN_SHARED_CONTRACT_VERSION),
    );
    fields.insert(
        "element_type".to_string(),
        Value::String(buffer.metadata.element_type.clone()),
    );
    fields.insert(
        "element_size".to_string(),
        Value::Int(buffer.metadata.element_size),
    );
    fields.insert(
        "shape".to_string(),
        int_list_to_value(&buffer.metadata.shape),
    );
    fields.insert(
        "strides".to_string(),
        int_list_to_value(&buffer.metadata.strides),
    );
    fields.insert(
        "format".to_string(),
        optional_string_to_value(buffer.metadata.format.clone()),
    );
    fields.insert(
        "mime_type".to_string(),
        optional_string_to_value(buffer.metadata.mime_type.clone()),
    );
    fields.insert(
        "source_runtime".to_string(),
        Value::String(buffer.metadata.source_runtime.clone()),
    );
    fields.insert(
        "source_backend".to_string(),
        optional_string_to_value(buffer.metadata.source_backend.clone()),
    );
    fields.insert(
        "ownership".to_string(),
        Value::String(buffer.metadata.ownership.clone()),
    );
    fields.insert("adoption_path".to_string(), Value::None);
    fields.insert("fallback_reason".to_string(), Value::None);
    fields.insert(
        "labels".to_string(),
        string_list_to_value(&buffer.metadata.labels),
    );
    fields.insert(
        "byte_length".to_string(),
        Value::Int(buffer.byte_length() as i64),
    );
    fields.insert(
        "element_count".to_string(),
        Value::Int(element_count(&buffer.metadata.shape)),
    );
    fields.insert(
        "zero_copy".to_string(),
        Value::Bool(buffer.metadata.ownership != "owned"),
    );
    Value::Struct(
        "KainSharedBufferInfo".to_string(),
        Arc::new(RwLock::new(fields)),
    )
}

pub fn shared_image_info_value(image: &KainSharedImage) -> Value {
    let mut fields = HashMap::new();
    fields.insert(
        "contract".to_string(),
        Value::String("kain.shared.image".to_string()),
    );
    fields.insert(
        "contract_version".to_string(),
        Value::Int(KAIN_SHARED_CONTRACT_VERSION),
    );
    fields.insert(
        "representation".to_string(),
        Value::String(image.metadata.representation.clone()),
    );
    fields.insert("width".to_string(), Value::Int(image.metadata.width));
    fields.insert("height".to_string(), Value::Int(image.metadata.height));
    fields.insert("channels".to_string(), Value::Int(image.metadata.channels));
    fields.insert(
        "layout".to_string(),
        Value::String(image.metadata.layout.clone()),
    );
    fields.insert(
        "pixel_format".to_string(),
        Value::String(image.metadata.pixel_format.clone()),
    );
    fields.insert(
        "mime_type".to_string(),
        Value::String(image.metadata.mime_type.clone()),
    );
    fields.insert(
        "row_stride".to_string(),
        Value::Int(image.metadata.row_stride),
    );
    fields.insert(
        "color_space".to_string(),
        Value::String(image.metadata.color_space.clone()),
    );
    fields.insert(
        "alpha_mode".to_string(),
        Value::String(image.metadata.alpha_mode.clone()),
    );
    fields.insert(
        "source_runtime".to_string(),
        Value::String(image.metadata.source_runtime.clone()),
    );
    fields.insert(
        "source_backend".to_string(),
        optional_string_to_value(image.metadata.source_backend.clone()),
    );
    fields.insert(
        "ownership".to_string(),
        Value::String(image.metadata.ownership.clone()),
    );
    fields.insert(
        "labels".to_string(),
        string_list_to_value(&image.metadata.labels),
    );
    fields.insert(
        "byte_length".to_string(),
        Value::Int(image.buffer.byte_length() as i64),
    );
    fields.insert(
        "zero_copy".to_string(),
        Value::Bool(image.buffer.metadata.ownership != "owned"),
    );
    Value::Struct(
        "KainSharedImageInfo".to_string(),
        Arc::new(RwLock::new(fields)),
    )
}

fn register_interop_stdlib(stdlib: &mut StdLib) {
    for builtin in [
        BuiltinFn {
            name: "kain_shared_buffer_info",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Inspect a neutral Kain shared buffer contract",
        },
        BuiltinFn {
            name: "kain_shared_buffer_bytes",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Snapshot the bytes behind a neutral Kain shared buffer",
        },
        BuiltinFn {
            name: "kain_shared_buffer_from_bytes",
            params: vec![
                ("bytes", "Any"),
                ("element_type", "String"),
                ("shape", "Any"),
                ("format", "String"),
                ("mime_type", "String"),
            ],
            return_type: "Any",
            doc: "Create a neutral Kain shared buffer from raw bytes",
        },
        BuiltinFn {
            name: "kain_shared_buffer_replace_bytes",
            params: vec![("target", "Any"), ("bytes", "Any")],
            return_type: "Unit",
            doc: "Replace the bytes behind a neutral Kain shared buffer",
        },
        BuiltinFn {
            name: "kain_shared_image_info",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Inspect a neutral Kain shared image contract",
        },
        BuiltinFn {
            name: "kain_shared_image_bytes",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Snapshot the bytes behind a neutral Kain shared image",
        },
        BuiltinFn {
            name: "kain_shared_image_from_bytes",
            params: vec![
                ("bytes", "Any"),
                ("width", "Int"),
                ("height", "Int"),
                ("channels", "Int"),
                ("layout", "String"),
                ("pixel_format", "String"),
                ("mime_type", "String"),
            ],
            return_type: "Any",
            doc: "Create a neutral Kain shared raster image from bytes",
        },
        BuiltinFn {
            name: "kain_shared_image_replace_bytes",
            params: vec![("target", "Any"), ("bytes", "Any")],
            return_type: "Unit",
            doc: "Replace the bytes behind a neutral Kain shared image",
        },
    ] {
        stdlib.functions.insert(
            builtin.name.to_string(),
            crate::kain_core_builtin_stub(builtin),
        );
    }
}

fn register_interop_env(env: &mut Env) {
    env.register_native_fn("kain_shared_buffer_info", builtin_shared_buffer_info);
    env.register_native_fn("kain_shared_buffer_bytes", builtin_shared_buffer_bytes);
    env.register_native_fn(
        "kain_shared_buffer_from_bytes",
        builtin_shared_buffer_from_bytes,
    );
    env.register_native_fn(
        "kain_shared_buffer_replace_bytes",
        builtin_shared_buffer_replace_bytes,
    );
    env.register_native_fn("kain_shared_image_info", builtin_shared_image_info);
    env.register_native_fn("kain_shared_image_bytes", builtin_shared_image_bytes);
    env.register_native_fn(
        "kain_shared_image_from_bytes",
        builtin_shared_image_from_bytes,
    );
    env.register_native_fn(
        "kain_shared_image_replace_bytes",
        builtin_shared_image_replace_bytes,
    );
}

fn builtin_shared_buffer_info(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "kain_shared_buffer_info: expected 1 argument (target)",
        ));
    }
    let buffer = extract_shared_buffer(&args[0])?;
    Ok(shared_buffer_info_value(&buffer))
}

fn builtin_shared_buffer_bytes(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "kain_shared_buffer_bytes: expected 1 argument (target)",
        ));
    }
    let buffer = extract_shared_buffer(&args[0])?;
    Ok(bytes_to_value(&buffer.bytes()))
}

fn builtin_shared_buffer_from_bytes(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 5 {
        return Err(KainError::runtime(
            "kain_shared_buffer_from_bytes: expected (bytes, element_type, shape, format, mime_type)",
        ));
    }
    let bytes = value_to_bytes("kain_shared_buffer_from_bytes", &args[0])?;
    let element_type =
        value_to_string_arg("kain_shared_buffer_from_bytes", "element_type", &args[1])?;
    let shape = value_to_int_list("kain_shared_buffer_from_bytes", "shape", &args[2])?;
    let format = value_to_string_arg("kain_shared_buffer_from_bytes", "format", &args[3])?;
    let mime_type = value_to_string_arg("kain_shared_buffer_from_bytes", "mime_type", &args[4])?;
    let metadata = SharedBufferMetadata {
        element_size: element_size_for(&element_type)?,
        element_type,
        strides: compact_strides(&shape),
        shape,
        format: if format.is_empty() {
            None
        } else {
            Some(format)
        },
        mime_type: if mime_type.is_empty() {
            None
        } else {
            Some(mime_type)
        },
        source_runtime: "kain".to_string(),
        source_backend: None,
        ownership: "owned".to_string(),
        labels: vec!["kain".to_string(), "buffer".to_string()],
    };
    Ok(shared_buffer_value(KainSharedBuffer::owned(
        metadata, bytes,
    )))
}

fn builtin_shared_buffer_replace_bytes(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 2 {
        return Err(KainError::runtime(
            "kain_shared_buffer_replace_bytes: expected (target, bytes)",
        ));
    }
    let buffer = extract_shared_buffer(&args[0])?;
    let bytes = value_to_bytes("kain_shared_buffer_replace_bytes", &args[1])?;
    buffer.replace_bytes(bytes)?;
    Ok(Value::Unit)
}

fn builtin_shared_image_info(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "kain_shared_image_info: expected 1 argument (target)",
        ));
    }
    let image = extract_shared_image(&args[0])?;
    Ok(shared_image_info_value(&image))
}

fn builtin_shared_image_bytes(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 1 {
        return Err(KainError::runtime(
            "kain_shared_image_bytes: expected 1 argument (target)",
        ));
    }
    let image = extract_shared_image(&args[0])?;
    Ok(bytes_to_value(&image.bytes()))
}

fn builtin_shared_image_from_bytes(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 7 {
        return Err(KainError::runtime(
            "kain_shared_image_from_bytes: expected (bytes, width, height, channels, layout, pixel_format, mime_type)",
        ));
    }
    let bytes = value_to_bytes("kain_shared_image_from_bytes", &args[0])?;
    let width = value_to_int_arg("kain_shared_image_from_bytes", "width", &args[1])?;
    let height = value_to_int_arg("kain_shared_image_from_bytes", "height", &args[2])?;
    let channels = value_to_int_arg("kain_shared_image_from_bytes", "channels", &args[3])?;
    let layout = value_to_string_arg("kain_shared_image_from_bytes", "layout", &args[4])?;
    let pixel_format =
        value_to_string_arg("kain_shared_image_from_bytes", "pixel_format", &args[5])?;
    let mime_type = value_to_string_arg("kain_shared_image_from_bytes", "mime_type", &args[6])?;
    let row_stride = if width > 0 && channels > 0 {
        width * channels
    } else {
        0
    };
    let image = KainSharedImage::owned(
        SharedImageMetadata {
            representation: "raster".to_string(),
            width,
            height,
            channels,
            layout,
            pixel_format,
            mime_type,
            row_stride,
            color_space: "srgb".to_string(),
            alpha_mode: if channels == 4 {
                "straight".to_string()
            } else {
                "opaque".to_string()
            },
            source_runtime: "kain".to_string(),
            source_backend: None,
            ownership: "owned".to_string(),
            labels: vec!["kain".to_string(), "image".to_string()],
        },
        bytes,
    )?;
    Ok(shared_image_value(image))
}

fn builtin_shared_image_replace_bytes(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    if args.len() != 2 {
        return Err(KainError::runtime(
            "kain_shared_image_replace_bytes: expected (target, bytes)",
        ));
    }
    let image = extract_shared_image(&args[0])?;
    let bytes = value_to_bytes("kain_shared_image_replace_bytes", &args[1])?;
    image.replace_bytes(bytes)?;
    Ok(Value::Unit)
}

fn int_list_to_value(values: &[i64]) -> Value {
    Value::Array(Arc::new(RwLock::new(
        values.iter().copied().map(Value::Int).collect(),
    )))
}

fn string_list_to_value(values: &[String]) -> Value {
    Value::Array(Arc::new(RwLock::new(
        values.iter().cloned().map(Value::String).collect(),
    )))
}

fn optional_string_to_value(value: Option<String>) -> Value {
    match value {
        Some(value) => Value::String(value),
        None => Value::None,
    }
}

fn bytes_to_value(bytes: &[u8]) -> Value {
    Value::Array(Arc::new(RwLock::new(
        bytes
            .iter()
            .map(|value| Value::Int(*value as i64))
            .collect(),
    )))
}

fn value_to_bytes(fn_name: &str, value: &Value) -> KainResult<Vec<u8>> {
    let Value::Array(values) = value else {
        return Err(KainError::runtime(format!(
            "{fn_name}: expected bytes to be Array<Int>"
        )));
    };
    let values = values.read().unwrap();
    let mut bytes = Vec::with_capacity(values.len());
    for (index, value) in values.iter().enumerate() {
        let Value::Int(value) = value else {
            return Err(KainError::runtime(format!(
                "{fn_name}: byte {index} must be Int"
            )));
        };
        let byte = u8::try_from(*value).map_err(|_| {
            KainError::runtime(format!(
                "{fn_name}: byte {index} value {value} is outside u8 range"
            ))
        })?;
        bytes.push(byte);
    }
    Ok(bytes)
}

fn value_to_int_list(fn_name: &str, label: &str, value: &Value) -> KainResult<Vec<i64>> {
    let Value::Array(values) = value else {
        return Err(KainError::runtime(format!(
            "{fn_name}: {label} must be Array<Int>"
        )));
    };
    let values = values.read().unwrap();
    let mut result = Vec::with_capacity(values.len());
    for entry in values.iter() {
        let Value::Int(value) = entry else {
            return Err(KainError::runtime(format!(
                "{fn_name}: {label} must contain only Int values"
            )));
        };
        result.push(*value);
    }
    Ok(result)
}

fn value_to_int_arg(fn_name: &str, label: &str, value: &Value) -> KainResult<i64> {
    match value {
        Value::Int(value) => Ok(*value),
        other => Err(KainError::runtime(format!(
            "{fn_name}: {label} must be Int, got {other:?}"
        ))),
    }
}

fn value_to_string_arg(fn_name: &str, label: &str, value: &Value) -> KainResult<String> {
    match value {
        Value::String(value) => Ok(value.clone()),
        other => Err(KainError::runtime(format!(
            "{fn_name}: {label} must be String, got {other:?}"
        ))),
    }
}

fn compact_strides(shape: &[i64]) -> Vec<i64> {
    if shape.is_empty() {
        return vec![1];
    }
    let mut strides = vec![0; shape.len()];
    let mut stride = 1;
    for (index, dim) in shape.iter().enumerate().rev() {
        strides[index] = stride;
        stride *= (*dim).max(1);
    }
    strides
}

fn element_count(shape: &[i64]) -> i64 {
    if shape.is_empty() {
        0
    } else {
        shape.iter().copied().product()
    }
}

fn element_size_for(element_type: &str) -> KainResult<i64> {
    gpu_storage_element_stride_bytes(element_type)
        .map(|bytes| bytes as i64)
        .ok_or_else(|| {
            KainError::runtime(format!("Unsupported shared element type: {element_type}"))
        })
}

fn required_shared_buffer_byte_length(metadata: &SharedBufferMetadata) -> KainResult<usize> {
    if metadata.element_size <= 0 {
        return Err(KainError::runtime(
            "shared buffer metadata must declare a positive element_size".to_string(),
        ));
    }
    if metadata.shape.iter().any(|dim| *dim < 0) {
        return Err(KainError::runtime(
            "shared buffer shape dimensions must be non-negative".to_string(),
        ));
    }

    let element_count = element_count(&metadata.shape);
    if element_count < 0 {
        return Err(KainError::runtime(
            "shared buffer element_count must be non-negative".to_string(),
        ));
    }

    let required = element_count
        .checked_mul(metadata.element_size)
        .ok_or_else(|| KainError::runtime("shared buffer byte length overflow".to_string()))?;

    usize::try_from(required)
        .map_err(|_| KainError::runtime("shared buffer byte length does not fit usize".to_string()))
}

fn kain_core_builtin_stub(builtin: BuiltinFn) -> BuiltinFn {
    builtin
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn shared_image_contract_reports_expected_metadata() {
        let image = KainSharedImage::owned(
            SharedImageMetadata {
                representation: "raster".to_string(),
                width: 4,
                height: 2,
                channels: 3,
                layout: "HWC".to_string(),
                pixel_format: "rgb8".to_string(),
                mime_type: "image/x-kain-rgb8".to_string(),
                row_stride: 12,
                color_space: "srgb".to_string(),
                alpha_mode: "opaque".to_string(),
                source_runtime: "python".to_string(),
                source_backend: Some("numpy".to_string()),
                ownership: "owned".to_string(),
                labels: vec!["test".to_string()],
            },
            vec![0; 24],
        )
        .unwrap();
        let info = shared_image_info_value(&image);
        let Value::Struct(_, fields) = info else {
            panic!("expected struct info");
        };
        let fields = fields.read().unwrap();
        assert!(matches!(fields.get("width"), Some(Value::Int(4))));
        assert!(matches!(
            fields.get("source_runtime"),
            Some(Value::String(value)) if value == "python"
        ));
        assert!(matches!(fields.get("byte_length"), Some(Value::Int(24))));
    }

    #[test]
    fn shared_buffer_contract_reports_expected_metadata() {
        let buffer = KainSharedBuffer::owned(
            SharedBufferMetadata {
                element_type: "u8".to_string(),
                element_size: 1,
                shape: vec![6],
                strides: vec![1],
                format: Some("u8".to_string()),
                mime_type: Some("application/octet-stream".to_string()),
                source_runtime: "javascript".to_string(),
                source_backend: Some("node".to_string()),
                ownership: "owned".to_string(),
                labels: vec!["typed-array".to_string()],
            },
            vec![1, 2, 3, 4, 5, 6],
        );
        let info = shared_buffer_info_value(&buffer);
        let Value::Struct(_, fields) = info else {
            panic!("expected struct info");
        };
        let fields = fields.read().unwrap();
        assert!(matches!(fields.get("byte_length"), Some(Value::Int(6))));
        assert!(matches!(
            fields.get("element_type"),
            Some(Value::String(value)) if value == "u8"
        ));
    }

    #[test]
    fn shared_buffer_gpu_binding_view_reports_descriptor_ready_shape() {
        let buffer = KainSharedBuffer::owned(
            SharedBufferMetadata {
                element_type: "f32".to_string(),
                element_size: 4,
                shape: vec![4, 4],
                strides: vec![4, 1],
                format: Some("f32".to_string()),
                mime_type: Some("application/octet-stream".to_string()),
                source_runtime: "kain".to_string(),
                source_backend: Some("residency-sidecar".to_string()),
                ownership: "owned".to_string(),
                labels: vec!["gpu".to_string()],
            },
            vec![0; 64],
        );

        let view = shared_buffer_gpu_binding_view(
            &buffer,
            GpuDescriptorKind::StorageBuffer,
            GpuBindingAccess::ReadWrite,
        )
        .expect("binding view");

        assert_eq!(view.contract, "kain.shared.buffer");
        assert_eq!(view.byte_length, 64);
        assert_eq!(view.element_type, "f32");
        assert_eq!(view.descriptor_kind.as_str(), "storage_buffer");
        assert_eq!(view.access.as_str(), "read_write");
    }

    #[test]
    fn shared_buffer_gpu_binding_view_rejects_uniform_write_access() {
        let buffer = KainSharedBuffer::owned(
            SharedBufferMetadata {
                element_type: "f32".to_string(),
                element_size: 4,
                shape: vec![4],
                strides: vec![1],
                format: Some("f32".to_string()),
                mime_type: Some("application/octet-stream".to_string()),
                source_runtime: "kain".to_string(),
                source_backend: Some("residency-sidecar".to_string()),
                ownership: "owned".to_string(),
                labels: vec!["gpu".to_string()],
            },
            vec![0; 16],
        );

        let error = shared_buffer_gpu_binding_view(
            &buffer,
            GpuDescriptorKind::UniformBuffer,
            GpuBindingAccess::Write,
        )
        .expect_err("uniform write access should fail");

        assert!(format!("{error}").contains("read-only"));
    }

    #[test]
    fn shared_buffer_element_sizes_follow_gpu_storage_stride_contract() {
        assert_eq!(element_size_for("bool").unwrap(), 4);
        assert_eq!(element_size_for("vec3<f32>").unwrap(), 16);

        let metadata = SharedBufferMetadata {
            element_type: "vec3<f32>".to_string(),
            element_size: element_size_for("vec3<f32>").unwrap(),
            shape: vec![2],
            strides: vec![1],
            format: Some("vec3<f32>".to_string()),
            mime_type: Some("application/octet-stream".to_string()),
            source_runtime: "test".to_string(),
            source_backend: Some("unit".to_string()),
            ownership: "owned".to_string(),
            labels: vec!["gpu".to_string()],
        };
        assert_eq!(required_shared_buffer_byte_length(&metadata).unwrap(), 32);
    }
}
