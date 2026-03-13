use std::fs;
use std::path::{Path, PathBuf};

use crate::{frontend_to_typed_program, target_extension, CompileTarget};
use kain_core::error::KainError;

#[cfg(all(feature = "gpu", feature = "sys"))]
use kain_sys_codegen::{
    collect_gpu_artifacts, collect_gpu_artifacts_json, generate_rust_gpu_host_wrappers,
    RustGpuArtifactOutput,
};

#[cfg(feature = "gpu")]
use gpu;

#[cfg(all(feature = "gpu", feature = "sys"))]
#[derive(Debug, Clone)]
pub struct GpuArtifactOutput {
    pub spirv: Vec<u8>,
    pub rust_host: String,
    pub reflection_json: String,
    pub metadata: RustGpuArtifactOutput,
}

#[cfg(all(feature = "gpu", feature = "sys"))]
pub fn compile_gpu_artifacts(source: &str) -> Result<GpuArtifactOutput, KainError> {
    let typed_program = frontend_to_typed_program(source, CompileTarget::Spirv)?;
    let spirv = gpu::generate_spirv(&typed_program)?;
    let rust_host = generate_rust_gpu_host_wrappers(&typed_program)?;
    let reflection_json = collect_gpu_artifacts_json(&typed_program).map_err(|err| {
        KainError::runtime(format!("Failed to serialize GPU reflection JSON: {}", err))
    })?;
    let metadata = collect_gpu_artifacts(&typed_program);

    Ok(GpuArtifactOutput {
        spirv,
        rust_host,
        reflection_json,
        metadata,
    })
}

#[cfg(not(all(feature = "gpu", feature = "sys")))]
pub fn compile_gpu_artifacts(_source: &str) -> Result<(), KainError> {
    Err(KainError::runtime(
        "GPU artifact generation requires both gpu and sys features",
    ))
}

#[cfg(all(feature = "gpu", feature = "sys"))]
pub fn write_gpu_artifacts_bundle(
    input: &Path,
    output: Option<&PathBuf>,
    artifacts: &GpuArtifactOutput,
) -> Result<Vec<PathBuf>, KainError> {
    let base_path = output
        .cloned()
        .unwrap_or_else(|| input.with_extension(target_extension(CompileTarget::Spirv)));

    let spirv_path = with_file_name_suffix(&base_path, "", "spv");
    let rust_path = with_file_name_suffix(&base_path, ".gpu", "rs");
    let json_path = with_file_name_suffix(&base_path, ".reflect", "json");

    for path in [&spirv_path, &rust_path, &json_path] {
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent).map_err(|err| {
                KainError::runtime(format!(
                    "Failed to create output directory {}: {}",
                    parent.display(),
                    err
                ))
            })?;
        }
    }

    fs::write(&spirv_path, &artifacts.spirv).map_err(|err| {
        KainError::runtime(format!(
            "Failed to write SPIR-V output {}: {}",
            spirv_path.display(),
            err
        ))
    })?;
    fs::write(&rust_path, artifacts.rust_host.as_bytes()).map_err(|err| {
        KainError::runtime(format!(
            "Failed to write Rust GPU host output {}: {}",
            rust_path.display(),
            err
        ))
    })?;
    fs::write(&json_path, artifacts.reflection_json.as_bytes()).map_err(|err| {
        KainError::runtime(format!(
            "Failed to write GPU reflection output {}: {}",
            json_path.display(),
            err
        ))
    })?;

    Ok(vec![spirv_path, rust_path, json_path])
}

#[cfg(all(feature = "gpu", feature = "sys"))]
pub fn run_gpu_artifact_pipeline(
    input: &Path,
    output: Option<&PathBuf>,
) -> Result<Vec<PathBuf>, KainError> {
    let source = fs::read_to_string(input).map_err(|err| {
        KainError::runtime(format!("Failed to read {}: {}", input.display(), err))
    })?;
    let artifacts = compile_gpu_artifacts(&source)?;
    write_gpu_artifacts_bundle(input, output, &artifacts)
}

fn with_file_name_suffix(base: &Path, suffix: &str, extension: &str) -> PathBuf {
    let parent = base.parent().map(Path::to_path_buf).unwrap_or_default();
    let stem = base
        .file_stem()
        .and_then(|stem| stem.to_str())
        .unwrap_or("shader");
    let file_name = format!("{}{}.{extension}", stem, suffix);
    parent.join(file_name)
}
