use std::fs;
use std::path::{Path, PathBuf};

use crate::{target_extension, CompileTarget};
use kain_core::error::KainError;
#[cfg(all(feature = "gpu", feature = "sys"))]
use kain_core::ShaderArtifactFormat;
#[cfg(all(feature = "gpu", feature = "sys"))]
use kain_driver::{
    compile_gpu_artifacts as compile_gpu_artifacts_from_driver, compile_realtime_app_bundle,
    write_compute_residency_sidecars,
};

#[cfg(all(feature = "gpu", feature = "sys"))]
pub type GpuArtifactOutput = kain_driver::ShaderArtifactBundleOutput;

// ── Target filtering ────────────────────────────────────────────────────

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum GpuArtifactTarget {
    All,
    Spirv,
    Cuda,
    Hlsl,
    Wgsl,
}

impl GpuArtifactTarget {
    pub fn from_arg(arg: &str) -> Self {
        match arg {
            "all" => GpuArtifactTarget::All,
            "spirv" | "vulkan" | "spv" => GpuArtifactTarget::Spirv,
            "cuda" | "ptx" | "nvidia" => GpuArtifactTarget::Cuda,
            "hlsl" | "d3d" | "dx" => GpuArtifactTarget::Hlsl,
            "wgsl" | "webgpu" | "wgpu" => GpuArtifactTarget::Wgsl,
            _ => GpuArtifactTarget::All, // unknown → default to all
        }
    }

    pub fn emit_spirv(&self) -> bool {
        matches!(
            self,
            GpuArtifactTarget::All
                | GpuArtifactTarget::Spirv
                | GpuArtifactTarget::Hlsl
                | GpuArtifactTarget::Wgsl
        )
    }

    pub fn emit_ptx(&self) -> bool {
        matches!(self, GpuArtifactTarget::All | GpuArtifactTarget::Cuda)
    }

    pub fn emit_hlsl(&self) -> bool {
        matches!(self, GpuArtifactTarget::All | GpuArtifactTarget::Hlsl)
    }

    pub fn emit_wgsl(&self) -> bool {
        matches!(self, GpuArtifactTarget::All | GpuArtifactTarget::Wgsl)
    }

    pub fn is_cuda_primary(&self) -> bool {
        matches!(self, GpuArtifactTarget::Cuda)
    }
}

#[cfg(all(feature = "gpu", feature = "sys"))]
pub fn compile_gpu_artifacts(source: &str) -> Result<GpuArtifactOutput, KainError> {
    compile_gpu_artifacts_from_driver(source)
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
    target: GpuArtifactTarget,
    no_derived: bool,
) -> Result<Vec<PathBuf>, KainError> {
    let base_path = resolve_output_base_path(input, output, target);

    let spirv_path = with_file_name_suffix(&base_path, "", "spv");
    let rust_path = with_file_name_suffix(&base_path, ".gpu", "rs");
    let json_path = with_file_name_suffix(&base_path, ".reflect", "json");
    let bundle_path = with_file_name_suffix(&base_path, ".shader_bundle", "json");
    let hlsl_path = with_file_name_suffix(&base_path, ".derived", "hlsl");
    let wgsl_path = with_file_name_suffix(&base_path, ".derived", "wgsl");
    let ptx_path = with_file_name_suffix(&base_path, ".derived", "ptx");

    let paths: &[&Path] = if target.is_cuda_primary() {
        // CUDA-primary: no SPIRV, no HLSL
        &[&rust_path, &json_path, &bundle_path, &ptx_path]
    } else if target == GpuArtifactTarget::Spirv && no_derived {
        &[&spirv_path, &rust_path, &json_path, &bundle_path]
    } else if target == GpuArtifactTarget::Spirv {
        &[
            &spirv_path,
            &rust_path,
            &json_path,
            &bundle_path,
            &ptx_path,
            &hlsl_path,
            &wgsl_path,
        ]
    } else if target == GpuArtifactTarget::Hlsl {
        &[
            &spirv_path,
            &rust_path,
            &json_path,
            &bundle_path,
            &hlsl_path,
        ]
    } else if target == GpuArtifactTarget::Wgsl {
        &[
            &spirv_path,
            &rust_path,
            &json_path,
            &bundle_path,
            &wgsl_path,
        ]
    } else {
        // All
        &[
            &spirv_path,
            &rust_path,
            &json_path,
            &bundle_path,
            &hlsl_path,
            &wgsl_path,
            &ptx_path,
        ]
    };

    for path in paths {
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

    // Always write Rust host wrappers, reflection, and bundle
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

    let mut written = vec![rust_path, json_path, bundle_path];

    // SPIR-V — only write if target asks for it and we have bytes
    if target.emit_spirv() && !artifacts.spirv.is_empty() {
        fs::write(&spirv_path, &artifacts.spirv).map_err(|err| {
            KainError::runtime(format!(
                "Failed to write SPIR-V output {}: {}",
                spirv_path.display(),
                err
            ))
        })?;
        written.push(spirv_path);
    }

    // HLSL — only write if target asks and the artifact has it
    if target.emit_hlsl() && !no_derived {
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
    }

    // WGSL — only write if target asks and the artifact has it
    if target.emit_wgsl() && !no_derived {
        if let Some(wgsl) = &artifacts.derived_wgsl {
            fs::write(&wgsl_path, wgsl.as_bytes()).map_err(|err| {
                KainError::runtime(format!(
                    "Failed to write derived WGSL output {}: {}",
                    wgsl_path.display(),
                    err
                ))
            })?;
            written.push(wgsl_path);
        }
    }

    // PTX — only write if target asks and the artifact has it
    if target.emit_ptx() && !no_derived {
        if let Some(ptx) = &artifacts.derived_ptx {
            fs::write(&ptx_path, ptx.as_bytes()).map_err(|err| {
                KainError::runtime(format!(
                    "Failed to write derived PTX output {}: {}",
                    ptx_path.display(),
                    err
                ))
            })?;
            written.push(ptx_path.clone());
        }

        let ptx_variants = artifacts
            .bundle
            .derived_outputs
            .iter()
            .filter(|artifact| artifact.format == ShaderArtifactFormat::Ptx)
            .collect::<Vec<_>>();
        for variant in ptx_variants {
            let Some(metadata) = variant.ptx.as_ref() else {
                continue;
            };
            let variant_path = with_file_name_suffix(
                &ptx_path,
                &format!(".{}", metadata.required_target_arch),
                "ptx",
            );
            fs::write(&variant_path, variant.contents.as_bytes()).map_err(|err| {
                KainError::runtime(format!(
                    "Failed to write derived PTX variant output {}: {}",
                    variant_path.display(),
                    err
                ))
            })?;
            written.push(variant_path);
        }
    }

    Ok(written)
}

#[cfg(all(feature = "gpu", feature = "sys"))]
pub fn run_gpu_artifact_pipeline(
    input: &Path,
    output: Option<&PathBuf>,
    target: GpuArtifactTarget,
    no_residency: bool,
    no_derived: bool,
) -> Result<Vec<PathBuf>, KainError> {
    let source = fs::read_to_string(input).map_err(|err| {
        KainError::runtime(format!("Failed to read {}: {}", input.display(), err))
    })?;
    let artifacts = compile_gpu_artifacts(&source)?;
    let mut written = write_gpu_artifacts_bundle(input, output, &artifacts, target, no_derived)?;

    if !no_residency {
        let base_path = resolve_output_base_path(input, output, target);
        let sidecar_root = base_path.parent().unwrap_or_else(|| Path::new("."));

        // For CUDA-primary, use CUDA target for residency sidecars
        let realtime_target = if target.is_cuda_primary() {
            CompileTarget::Cuda
        } else {
            CompileTarget::Cuda
        };

        let realtime_bundle = compile_realtime_app_bundle(&source, realtime_target, None)?;
        let compute_sidecars = write_compute_residency_sidecars(
            &realtime_bundle.bundle,
            Some(&artifacts.bundle),
            sidecar_root,
        )?;
        written.extend(compute_sidecars);
    }

    Ok(written)
}

fn resolve_output_base_path(
    input: &Path,
    output: Option<&PathBuf>,
    target: GpuArtifactTarget,
) -> PathBuf {
    match output {
        Some(path) if path.is_dir() => {
            let stem = input
                .file_stem()
                .and_then(|stem| stem.to_str())
                .unwrap_or("shader");
            path.join(stem)
        }
        Some(path) => path.clone(),
        None => input.with_extension(target_extension(if target.is_cuda_primary() {
            CompileTarget::Cuda
        } else {
            CompileTarget::Spirv
        })),
    }
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

#[cfg(all(test, feature = "gpu", feature = "sys"))]
mod tests {
    use super::*;

    const CUDA_TENSOR_CORE_PROBE: &str = r#"
shader compute cuda_tensor_floor(id: UVec3) -> Void:
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1
    cuda_require_tensor_cores()
    let lane = id.x
    dst[lane] = src[lane]
    return
"#;

    #[test]
    fn gpu_artifact_pipeline_writes_cuda_variants_into_output_directory() {
        std::thread::Builder::new()
            .stack_size(32 * 1024 * 1024)
            .spawn(|| {
                let temp = tempfile::tempdir().expect("temp dir");
                let input = temp.path().join("probe.kn");
                fs::write(&input, CUDA_TENSOR_CORE_PROBE).expect("write source");
                let output_dir = temp.path().join("artifacts");
                fs::create_dir_all(&output_dir).expect("create output dir");

                let written = run_gpu_artifact_pipeline(
                    &input,
                    Some(&output_dir),
                    GpuArtifactTarget::Cuda,
                    false,
                    false,
                )
                .expect("gpu artifact pipeline should succeed");

                assert!(output_dir.join("probe.derived.ptx").exists());
                assert!(output_dir.join("probe.derived.sm_75.ptx").exists());
                assert!(output_dir.join("probe.derived.sm_120.ptx").exists());
                assert!(output_dir.join("probe.shader_bundle.json").exists());
                assert!(output_dir.join("kain_compute_residency.json").exists());
                assert!(written.iter().all(|path| path.starts_with(&output_dir)));
            })
            .expect("spawn cli gpu artifact test")
            .join()
            .expect("cli gpu artifact test thread");
    }
}
