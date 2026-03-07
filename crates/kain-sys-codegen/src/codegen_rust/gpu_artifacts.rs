use kain_core::ast::{ShaderStage, Type};
use kain_core::types::{TypedItem, TypedProgram};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RustGpuArtifactOutput {
    pub shaders: Vec<RustGpuShaderArtifact>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RustGpuShaderArtifact {
    pub name: String,
    pub stage: RustGpuShaderStage,
    pub entry_point: String,
    pub inputs: Vec<RustGpuInputArtifact>,
    pub bindings: Vec<RustGpuBindingArtifact>,
    pub output_type: String,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum RustGpuShaderStage {
    Vertex,
    Fragment,
    Compute,
    Surface,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RustGpuInputArtifact {
    pub name: String,
    pub ty: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RustGpuBindingArtifact {
    pub name: String,
    pub binding: u32,
    pub descriptor_set: u32,
    pub ty: String,
    pub kind: RustGpuBindingKind,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum RustGpuBindingKind {
    StorageBuffer,
    Sampler2D,
    Uniform,
    LocalSize,
    SpecializationConstant,
}

pub fn collect_gpu_artifacts(program: &TypedProgram) -> RustGpuArtifactOutput {
    let shaders = program
        .items
        .iter()
        .filter_map(|item| match item {
            TypedItem::Shader(shader) => Some(RustGpuShaderArtifact {
                name: shader.ast.name.clone(),
                stage: map_shader_stage(shader.ast.stage),
                entry_point: shader.ast.name.clone(),
                inputs: shader
                    .ast
                    .inputs
                    .iter()
                    .map(|param| RustGpuInputArtifact {
                        name: param.name.clone(),
                        ty: format_gpu_type(&param.ty),
                    })
                    .collect(),
                bindings: shader
                    .ast
                    .uniforms
                    .iter()
                    .map(|uniform| RustGpuBindingArtifact {
                        name: uniform.name.clone(),
                        binding: uniform.binding,
                        descriptor_set: 0,
                        ty: format_gpu_type(&uniform.ty),
                        kind: classify_binding(&uniform.name, &uniform.ty),
                    })
                    .collect(),
                output_type: format_gpu_type(&shader.ast.outputs),
            }),
            _ => None,
        })
        .collect();

    RustGpuArtifactOutput { shaders }
}

pub fn collect_gpu_artifacts_json(program: &TypedProgram) -> Result<String, serde_json::Error> {
    serde_json::to_string_pretty(&collect_gpu_artifacts(program))
}

pub fn format_gpu_type(ty: &Type) -> String {
    match ty {
        Type::Named { name, generics, .. } => {
            if generics.is_empty() {
                name.clone()
            } else {
                let generics = generics
                    .iter()
                    .map(format_gpu_type)
                    .collect::<Vec<_>>()
                    .join(", ");
                format!("{}<{}>", name, generics)
            }
        }
        Type::Tuple(types, _) => {
            let types = types
                .iter()
                .map(format_gpu_type)
                .collect::<Vec<_>>()
                .join(", ");
            format!("({})", types)
        }
        Type::Array(inner, size, _) => format!("[{}; {}]", format_gpu_type(inner), size),
        Type::Slice(inner, _) => format!("[{}]", format_gpu_type(inner)),
        Type::Ref { mutable, inner, .. } => {
            if *mutable {
                format!("&mut {}", format_gpu_type(inner))
            } else {
                format!("&{}", format_gpu_type(inner))
            }
        }
        Type::Ptr { mutable, inner, .. } => {
            if *mutable {
                format!("*mut {}", format_gpu_type(inner))
            } else {
                format!("*const {}", format_gpu_type(inner))
            }
        }
        Type::Function {
            params,
            return_type,
            ..
        } => {
            let params = params
                .iter()
                .map(format_gpu_type)
                .collect::<Vec<_>>()
                .join(", ");
            format!("fn({}) -> {}", params, format_gpu_type(return_type))
        }
        Type::Option(inner, _) => format!("Option<{}>", format_gpu_type(inner)),
        Type::Result(ok, err, _) => {
            format!("Result<{}, {}>", format_gpu_type(ok), format_gpu_type(err))
        }
        Type::Infer(_) => "_".to_string(),
        Type::Never(_) => "!".to_string(),
        Type::Unit(_) => "()".to_string(),
        Type::Impl {
            trait_name,
            generics,
            ..
        } => {
            if generics.is_empty() {
                format!("impl {}", trait_name)
            } else {
                let generics = generics
                    .iter()
                    .map(format_gpu_type)
                    .collect::<Vec<_>>()
                    .join(", ");
                format!("impl {}<{}>", trait_name, generics)
            }
        }
    }
}

fn map_shader_stage(stage: ShaderStage) -> RustGpuShaderStage {
    match stage {
        ShaderStage::Vertex => RustGpuShaderStage::Vertex,
        ShaderStage::Fragment => RustGpuShaderStage::Fragment,
        ShaderStage::Compute => RustGpuShaderStage::Compute,
        ShaderStage::Surface => RustGpuShaderStage::Surface,
    }
}

fn classify_binding(name: &str, ty: &Type) -> RustGpuBindingKind {
    if is_local_size_param(name) {
        RustGpuBindingKind::LocalSize
    } else if is_specialization_constant(name) {
        RustGpuBindingKind::SpecializationConstant
    } else if is_sampler_2d(ty) {
        RustGpuBindingKind::Sampler2D
    } else if is_storage_buffer(ty) {
        RustGpuBindingKind::StorageBuffer
    } else {
        RustGpuBindingKind::Uniform
    }
}

fn is_sampler_2d(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "Sampler2D")
}

fn is_storage_buffer(ty: &Type) -> bool {
    matches!(ty, Type::Named { name, .. } if name == "StorageBuffer")
}

fn is_local_size_param(name: &str) -> bool {
    matches!(name, "LOCAL_SIZE_X" | "LOCAL_SIZE_Y" | "LOCAL_SIZE_Z")
}

fn is_specialization_constant(name: &str) -> bool {
    let uppercase = name.chars().all(|ch| !ch.is_ascii_lowercase());
    let has_separator = name.contains('_');
    let has_known_prefix = ["CFG_", "ENABLE_", "USE_", "WITH_", "HAS_", "ALLOW_", "SUPPORT_"]
        .iter()
        .any(|prefix| name.starts_with(prefix));
    has_known_prefix || (uppercase && has_separator && name.len() >= 4)
}
