use std::fs;
use std::path::{Path, PathBuf};

use crate::{compile_shader_artifact_bundle, target_extension, CompileTarget};
use kain_core::error::KainError;

#[cfg(all(feature = "gpu", feature = "sys"))]
pub type GpuArtifactOutput = kain_driver::ShaderArtifactBundleOutput;

#[cfg(all(feature = "gpu", feature = "sys"))]
pub fn compile_gpu_artifacts(source: &str) -> Result<GpuArtifactOutput, KainError> {
    compile_shader_artifact_bundle(source)
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
    let bundle_path = with_file_name_suffix(&base_path, ".shader_bundle", "json");
    let hlsl_path = with_file_name_suffix(&base_path, ".derived", "hlsl");

    for path in [
        &spirv_path,
        &rust_path,
        &json_path,
        &bundle_path,
        &hlsl_path,
    ] {
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
    fs::write(&bundle_path, artifacts.bundle_json.as_bytes()).map_err(|err| {
        KainError::runtime(format!(
            "Failed to write shader bundle output {}: {}",
            bundle_path.display(),
            err
        ))
    })?;
    let mut written = vec![spirv_path, rust_path, json_path, bundle_path];
    if let Some(hlsl) = &artifacts.derived_hlsl {
        fs::write(&hlsl_path, hlsl.as_bytes()).map_err(|err| {
            KainError::runtime(format!(
                "Failed to write derived HLSL output {}: {}",
                hlsl_path.display(),
                err
            ))
        })?;
        written.push(hlsl_path);
    }

    Ok(written)
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
