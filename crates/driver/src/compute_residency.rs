use std::fs;
use std::path::{Path, PathBuf};

use kain_core::error::KainError;
use kain_core::{
    gpu_storage_element_stride_bytes, RealtimeAppBundle, RealtimeResourceBinding,
    RealtimeShaderBundleRef, ShaderArtifactBundle, ShaderArtifactFormat,
};
use serde::{Deserialize, Serialize};

pub const COMPUTE_RESIDENCY_FILE_NAME: &str = "kain_compute_residency.json";
pub const COMPUTE_RESIDENCY_ENV_VAR: &str = "KAIN_COMPUTE_RESIDENCY";

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ComputeResidencyBundle {
    pub schema_version: u32,
    pub target: String,
    pub compute_shader_count: usize,
    pub compute_shaders: Vec<ComputeResidencyEntry>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ComputeResidencyEntry {
    pub key: String,
    pub shader: String,
    pub module_name: String,
    pub stage: String,
    pub entry_point: String,
    pub source: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub execution_domain: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub workgroup_size: Option<[u32; 3]>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub dispatch_size: Option<[u32; 3]>,
    #[serde(default)]
    pub dynamic_shared_memory_bytes: u32,
    #[serde(default = "default_cuda_stream_policy")]
    pub cuda_stream_policy: String,
    #[serde(default = "default_cuda_graph_policy")]
    pub cuda_graph_policy: String,
    pub resource_binding_count: usize,
    pub tensor_binding_count: usize,
    pub stream_binding_count: usize,
    pub neural_node_count: usize,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub ptx_sidecar: Option<ComputeResidencyPtxSidecar>,
    pub bindings: Vec<ComputeResidencyBinding>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ComputeResidencyPtxSidecar {
    pub module_name: String,
    pub entry_point: String,
    pub ptx_version: String,
    pub required_target_arch: String,
    pub minimum_compute_capability: String,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub binding_slots: Vec<u32>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct ComputeResidencyBinding {
    pub key: String,
    pub contract: String,
    pub descriptor_kind: String,
    pub element_type: String,
    pub shape: Vec<i64>,
    pub strides: Vec<i64>,
    pub access_mode: String,
    pub residency_role: String,
    pub slot: u32,
    pub byte_length: usize,
    pub payload_file: String,
}

fn default_cuda_stream_policy() -> String {
    "default".to_string()
}

fn default_cuda_graph_policy() -> String {
    "disabled".to_string()
}

pub fn write_compute_residency_sidecars(
    realtime: &RealtimeAppBundle,
    shader_bundle: Option<&ShaderArtifactBundle>,
    artifact_root: &Path,
) -> Result<Vec<PathBuf>, KainError> {
    let Some(bundle) = build_compute_residency_bundle(realtime, shader_bundle) else {
        return Ok(Vec::new());
    };

    fs::create_dir_all(artifact_root).map_err(|err| {
        KainError::runtime(format!(
            "Failed to create compute residency artifact directory {}: {}",
            artifact_root.display(),
            err
        ))
    })?;

    let mut written = Vec::new();
    let main_path = artifact_root.join(COMPUTE_RESIDENCY_FILE_NAME);
    let bundle_json = serde_json::to_string_pretty(&bundle).map_err(|err| {
        KainError::runtime(format!(
            "Failed to serialize compute residency bundle for {}: {}",
            realtime.target, err
        ))
    })?;
    fs::write(&main_path, bundle_json.as_bytes()).map_err(|err| {
        KainError::runtime(format!(
            "Failed to write compute residency bundle {}: {}",
            main_path.display(),
            err
        ))
    })?;
    written.push(main_path);

    for entry in &bundle.compute_shaders {
        for binding in &entry.bindings {
            let payload_path = artifact_root.join(&binding.payload_file);
            let zero_bytes = vec![0u8; binding.byte_length];
            fs::write(&payload_path, zero_bytes).map_err(|err| {
                KainError::runtime(format!(
                    "Failed to write compute residency payload {}: {}",
                    payload_path.display(),
                    err
                ))
            })?;
            written.push(payload_path);
        }
    }

    Ok(written)
}

fn build_compute_residency_bundle(
    realtime: &RealtimeAppBundle,
    shader_bundle: Option<&ShaderArtifactBundle>,
) -> Option<ComputeResidencyBundle> {
    let mut compute_shaders = realtime
        .shader_bundle_refs
        .iter()
        .filter(|shader| shader.stage.eq_ignore_ascii_case("compute"))
        .cloned()
        .collect::<Vec<_>>();

    if compute_shaders.is_empty() {
        return None;
    }

    compute_shaders.sort_by(|left, right| {
        left.key
            .cmp(&right.key)
            .then(left.module_name.cmp(&right.module_name))
            .then(left.entry_point.cmp(&right.entry_point))
    });

    let entries = compute_shaders
        .into_iter()
        .enumerate()
        .map(|(_index, shader)| {
            let resolved_bundle_entry =
                shader_bundle.and_then(|bundle| resolve_shader_bundle_entry(bundle, &shader));
            let bindings = build_binding_residency_entries(&shader);
            let ptx_sidecar = resolve_ptx_sidecar(shader_bundle, &shader, resolved_bundle_entry);
            let module_name = resolved_bundle_entry
                .map(|entry| entry.module_name.clone())
                .unwrap_or_else(|| shader.module_name.clone());
            let entry_point = resolved_bundle_entry
                .map(|entry| entry.entry_point.clone())
                .unwrap_or_else(|| shader.entry_point.clone());
            ComputeResidencyEntry {
                key: shader.key.clone(),
                shader: shader.shader.clone(),
                module_name,
                stage: shader.stage.clone(),
                entry_point,
                source: shader.source.clone(),
                execution_domain: shader.execution_domain.clone(),
                workgroup_size: shader.workgroup_size,
                dispatch_size: shader.dispatch_size,
                dynamic_shared_memory_bytes: 0,
                cuda_stream_policy: "default".to_string(),
                cuda_graph_policy: "disabled".to_string(),
                resource_binding_count: shader.resource_bindings.len(),
                tensor_binding_count: shader.tensor_bindings.len(),
                stream_binding_count: shader.stream_bindings.len(),
                neural_node_count: shader.neural_nodes.len(),
                ptx_sidecar,
                bindings,
            }
        })
        .collect::<Vec<_>>();

    Some(ComputeResidencyBundle {
        schema_version: 1,
        target: realtime.target.clone(),
        compute_shader_count: entries.len(),
        compute_shaders: entries,
    })
}

fn build_binding_residency_entries(
    shader: &RealtimeShaderBundleRef,
) -> Vec<ComputeResidencyBinding> {
    shader
        .resource_bindings
        .iter()
        .map(|binding| build_binding_residency_entry(shader, binding))
        .collect()
}

fn build_binding_residency_entry(
    shader: &RealtimeShaderBundleRef,
    binding: &RealtimeResourceBinding,
) -> ComputeResidencyBinding {
    let tensor = shader
        .tensor_bindings
        .iter()
        .find(|tensor| tensor.key == binding.key);
    let dispatch_size = shader.dispatch_size.unwrap_or([1, 1, 1]);
    let shape = tensor
        .map(|tensor| resolve_tensor_shape(&tensor.shape, dispatch_size))
        .unwrap_or_else(|| vec![1]);
    let strides = compact_strides(&shape);
    let element_type = tensor
        .map(|tensor| tensor.element_type.clone())
        .unwrap_or_else(|| fallback_element_type(binding).to_string());
    let element_size = element_size_for(&element_type);
    let byte_length =
        element_size.saturating_mul(shape.iter().copied().product::<i64>().max(1) as usize);

    ComputeResidencyBinding {
        key: binding.key.clone(),
        contract: tensor
            .map(|tensor| tensor.contract.clone())
            .unwrap_or_else(|| "kain.shared.buffer".to_string()),
        descriptor_kind: descriptor_kind_for(binding).to_string(),
        element_type,
        shape,
        strides,
        access_mode: binding.access.clone(),
        residency_role: tensor
            .map(|tensor| tensor.role.clone())
            .unwrap_or_else(|| residency_role_from_access(&binding.access).to_string()),
        slot: binding.slot,
        byte_length,
        payload_file: compute_binding_payload_file_name(shader, binding),
    }
}

fn resolve_ptx_sidecar(
    shader_bundle: Option<&ShaderArtifactBundle>,
    shader: &RealtimeShaderBundleRef,
    bundle_entry: Option<&kain_core::ShaderEntryPoint>,
) -> Option<ComputeResidencyPtxSidecar> {
    if !shader.stage.eq_ignore_ascii_case("compute") {
        return None;
    }
    let bundle = shader_bundle?;
    let entry = bundle_entry.or_else(|| resolve_shader_bundle_entry(bundle, shader))?;
    let artifact = resolve_ptx_artifact(
        bundle,
        entry.module_name.as_str(),
        entry.entry_point.as_str(),
    )?;
    let ptx = artifact.ptx.as_ref()?;
    let mut binding_slots = shader
        .resource_bindings
        .iter()
        .map(|binding| binding.slot)
        .collect::<Vec<_>>();
    if binding_slots.is_empty() {
        binding_slots = artifact.binding_slots.clone();
    } else {
        binding_slots.sort_unstable();
        binding_slots.dedup();
    }

    Some(ComputeResidencyPtxSidecar {
        module_name: artifact.module_name.clone(),
        entry_point: entry.entry_point.clone(),
        ptx_version: ptx.ptx_version.clone(),
        required_target_arch: ptx.required_target_arch.clone(),
        minimum_compute_capability: ptx.minimum_compute_capability.clone(),
        binding_slots,
    })
}

fn resolve_shader_bundle_entry<'a>(
    bundle: &'a ShaderArtifactBundle,
    shader: &RealtimeShaderBundleRef,
) -> Option<&'a kain_core::ShaderEntryPoint> {
    bundle
        .entry_points
        .iter()
        .find(|entry| {
            entry.stage.eq_ignore_ascii_case(shader.stage.as_str()) && entry.shader == shader.shader
        })
        .or_else(|| {
            bundle.entry_points.iter().find(|entry| {
                entry.stage.eq_ignore_ascii_case(shader.stage.as_str())
                    && entry.module_name == shader.module_name
            })
        })
}

fn resolve_ptx_artifact<'a>(
    bundle: &'a ShaderArtifactBundle,
    module_name: &str,
    entry_point: &str,
) -> Option<&'a kain_core::DerivedShaderArtifact> {
    bundle
        .derived_outputs
        .iter()
        .find(|artifact| {
            artifact.format == ShaderArtifactFormat::Ptx
                && artifact.module_name == module_name
                && (artifact.entry_points.is_empty()
                    || artifact
                        .entry_points
                        .iter()
                        .any(|value| value == entry_point))
        })
        .or_else(|| {
            let mut artifacts = bundle
                .derived_outputs
                .iter()
                .filter(|artifact| artifact.format == ShaderArtifactFormat::Ptx);
            let first = artifacts.next()?;
            artifacts.next().is_none().then_some(first)
        })
}

fn sanitize_sidecar_stem(value: &str) -> String {
    let mut sanitized = String::with_capacity(value.len());
    let mut last_was_separator = false;

    for ch in value.chars() {
        let mapped = if ch.is_ascii_alphanumeric() {
            ch.to_ascii_lowercase()
        } else {
            '_'
        };
        if mapped == '_' {
            if !last_was_separator {
                sanitized.push(mapped);
                last_was_separator = true;
            }
        } else {
            sanitized.push(mapped);
            last_was_separator = false;
        }
    }

    let trimmed = sanitized.trim_matches('_');
    if trimmed.is_empty() {
        "compute".to_string()
    } else {
        trimmed.to_string()
    }
}

fn compute_binding_payload_file_name(
    shader: &RealtimeShaderBundleRef,
    binding: &RealtimeResourceBinding,
) -> String {
    format!(
        "kain_compute_residency_{}_{}.bin",
        sanitize_sidecar_stem(&shader.key),
        sanitize_sidecar_stem(&binding.key)
    )
}

fn descriptor_kind_for(binding: &RealtimeResourceBinding) -> &'static str {
    match binding.resource_type.as_str() {
        "storage_buffer" => "storage_buffer",
        _ => "uniform_buffer",
    }
}

fn residency_role_from_access(access: &str) -> &'static str {
    match access {
        "read" => "required_input",
        "write" => "required_output",
        _ => "scratch_state",
    }
}

fn fallback_element_type(binding: &RealtimeResourceBinding) -> &'static str {
    if binding.resource_type == "storage_buffer" {
        "f32"
    } else {
        "u32"
    }
}

fn resolve_tensor_shape(shape: &[String], dispatch_size: [u32; 3]) -> Vec<i64> {
    shape
        .iter()
        .map(|dim| match dim.as_str() {
            "dispatch.x" => dispatch_size[0] as i64,
            "dispatch.y" => dispatch_size[1] as i64,
            "dispatch.z" => dispatch_size[2] as i64,
            other => other.parse::<i64>().unwrap_or(1).max(1),
        })
        .collect()
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

fn element_size_for(element_type: &str) -> usize {
    gpu_storage_element_stride_bytes(element_type).unwrap_or(4)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::native_app::{compile_native_app_bundle, NativeAppBundleConfig};
    use tempfile::TempDir;

    #[test]
    fn compute_residency_uses_gpu_storage_stride_for_bool_and_vec3() {
        assert_eq!(element_size_for("bool"), 4);
        assert_eq!(element_size_for("vec3<f32>"), 16);
    }

    #[test]
    fn writes_deterministic_compute_residency_sidecars() {
        let temp = TempDir::new().expect("temp dir");
        let artifact_root = temp.path().join("generated");
        let source = r#"
component App():
    render <panel title="Residency" />

shader compute SampleCompute(id: UVec3) -> Vec4:
    uniform src: StorageBuffer<Vec4> @0
    uniform dst: StorageBuffer<Vec4> @1
    return vec4(1.0, 1.0, 1.0, 1.0)
"#;

        let bundle = compile_native_app_bundle(
            source,
            &NativeAppBundleConfig {
                source_file_name: Some("residency.kn".to_string()),
                ..Default::default()
            },
        )
        .expect("bundle should compile");

        let built_a = build_compute_residency_bundle(
            &bundle.realtime,
            bundle.shader_bundle.as_ref().map(|output| &output.bundle),
        )
        .expect("expected compute residency bundle");
        let built_b = build_compute_residency_bundle(
            &bundle.realtime,
            bundle.shader_bundle.as_ref().map(|output| &output.bundle),
        )
        .expect("expected compute residency bundle");
        assert_eq!(built_a, built_b);

        let written = write_compute_residency_sidecars(
            &bundle.realtime,
            bundle.shader_bundle.as_ref().map(|output| &output.bundle),
            &artifact_root,
        )
        .expect("compute residency sidecars should write");
        assert_eq!(written.len(), 3);

        let main_path = artifact_root.join(COMPUTE_RESIDENCY_FILE_NAME);
        assert!(main_path.exists());

        let main_json = fs::read_to_string(&main_path).expect("main residency json");
        let main_bundle: ComputeResidencyBundle =
            serde_json::from_str(&main_json).expect("parse residency json");
        assert_eq!(main_bundle.compute_shader_count, 1);
        assert_eq!(main_bundle.compute_shaders.len(), 1);
        assert_eq!(
            main_bundle.compute_shaders[0].key,
            "shader::SampleCompute::compute"
        );
        assert_eq!(main_bundle.compute_shaders[0].module_name, "SampleCompute");
        assert_eq!(main_bundle.compute_shaders[0].entry_point, "SampleCompute");
        assert_eq!(main_bundle.compute_shaders[0].resource_binding_count, 2);
        assert_eq!(
            main_bundle.compute_shaders[0].dynamic_shared_memory_bytes,
            0
        );
        assert_eq!(main_bundle.compute_shaders[0].cuda_stream_policy, "default");
        assert_eq!(main_bundle.compute_shaders[0].cuda_graph_policy, "disabled");
        assert_eq!(main_bundle.compute_shaders[0].bindings.len(), 2);
        assert_eq!(
            main_bundle.compute_shaders[0]
                .ptx_sidecar
                .as_ref()
                .expect("ptx sidecar")
                .module_name,
            main_bundle.compute_shaders[0].module_name
        );
        assert_eq!(
            main_bundle.compute_shaders[0]
                .ptx_sidecar
                .as_ref()
                .expect("ptx sidecar")
                .entry_point,
            main_bundle.compute_shaders[0].entry_point
        );
        assert_eq!(
            main_bundle.compute_shaders[0]
                .ptx_sidecar
                .as_ref()
                .expect("ptx sidecar")
                .required_target_arch,
            "sm_30"
        );
        assert_eq!(
            main_bundle.compute_shaders[0]
                .ptx_sidecar
                .as_ref()
                .expect("ptx sidecar")
                .minimum_compute_capability,
            "3.0"
        );
        assert_eq!(
            main_bundle.compute_shaders[0]
                .ptx_sidecar
                .as_ref()
                .expect("ptx sidecar")
                .binding_slots,
            vec![0, 1]
        );
        assert_eq!(
            main_bundle.compute_shaders[0].bindings[0].descriptor_kind,
            "storage_buffer"
        );
        assert_eq!(main_bundle.compute_shaders[0].bindings[0].shape, vec![1]);
        assert!(written.iter().any(|path| {
            path.file_name()
                .and_then(|value| value.to_str())
                .is_some_and(|value| value.ends_with(".bin"))
        }));
    }
}
