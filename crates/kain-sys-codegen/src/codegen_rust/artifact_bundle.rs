use crate::codegen_rust::gpu_artifacts::RustGpuArtifactOutput;
use crate::codegen_rust::{
    collect_gpu_artifacts, collect_gpu_artifacts_json, generate, generate_gpu_host_wrappers,
};
use kain_core::error::KainResult;
use kain_core::types::TypedProgram;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RustArtifactBundle {
    pub primary: RustTextArtifact,
    pub supplemental: Vec<RustTextArtifact>,
    pub shader_metadata: Option<RustGpuArtifactOutput>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RustTextArtifact {
    pub logical_name: String,
    pub suggested_file_name: String,
    pub kind: RustArtifactKind,
    pub contents: String,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum RustArtifactKind {
    PrimarySource,
    ShaderHost,
    ShaderReflection,
}

pub fn generate_rust_artifact_bundle(program: &TypedProgram) -> KainResult<RustArtifactBundle> {
    let primary_source = generate(program)?;
    let shader_metadata = collect_gpu_artifacts(program);
    let shader_support = if shader_metadata.shaders.is_empty() {
        None
    } else {
        Some(shader_metadata)
    };

    let mut supplemental = Vec::new();
    if let Some(shader_metadata) = &shader_support {
        let shader_host = generate_gpu_host_wrappers(program)?;
        let shader_reflection = collect_gpu_artifacts_json(program).map_err(|err| {
            kain_core::error::KainError::runtime(format!(
                "Failed to serialize Rust shader reflection bundle: {}",
                err
            ))
        })?;

        supplemental.push(RustTextArtifact {
            logical_name: "shader_host".to_string(),
            suggested_file_name: "kain_gpu.rs".to_string(),
            kind: RustArtifactKind::ShaderHost,
            contents: shader_host,
        });
        supplemental.push(RustTextArtifact {
            logical_name: "shader_reflection".to_string(),
            suggested_file_name: "kain_gpu.reflect.json".to_string(),
            kind: RustArtifactKind::ShaderReflection,
            contents: shader_reflection,
        });

        return Ok(RustArtifactBundle {
            primary: RustTextArtifact {
                logical_name: "rust_source".to_string(),
                suggested_file_name: "lib.rs".to_string(),
                kind: RustArtifactKind::PrimarySource,
                contents: primary_source,
            },
            supplemental,
            shader_metadata: Some(shader_metadata.clone()),
        });
    }

    Ok(RustArtifactBundle {
        primary: RustTextArtifact {
            logical_name: "rust_source".to_string(),
            suggested_file_name: "lib.rs".to_string(),
            kind: RustArtifactKind::PrimarySource,
            contents: primary_source,
        },
        supplemental,
        shader_metadata: None,
    })
}
