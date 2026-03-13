use std::fs;
use std::path::{Path, PathBuf};

use crate::packager::config::{RustBuildArtifact, RustBuildConfig};
use crate::{frontend_to_typed_program, CompileTarget};
use kain_core::error::KainError;

#[cfg(feature = "sys")]
use kain_sys_codegen::{generate_rust_artifact_bundle, RustArtifactBundle, RustArtifactKind};

#[derive(Debug, Clone)]
pub struct RustBuildOutput {
    #[cfg(feature = "sys")]
    pub bundle: RustArtifactBundle,
    pub spirv: Option<Vec<u8>>,
}

#[cfg(feature = "sys")]
pub fn compile_rust_build(
    source: &str,
    config: &RustBuildConfig,
) -> Result<RustBuildOutput, KainError> {
    let typed_program = frontend_to_typed_program(source, CompileTarget::Rust)?;
    let bundle = generate_rust_artifact_bundle(&typed_program)?;

    let spirv = if config.artifacts.contains(&RustBuildArtifact::Spirv)
        && bundle
            .shader_metadata
            .as_ref()
            .is_some_and(|metadata| !metadata.shaders.is_empty())
    {
        #[cfg(feature = "gpu")]
        {
            Some(crate::compile_spirv_binary(source)?)
        }
        #[cfg(not(feature = "gpu"))]
        {
            return Err(KainError::runtime(
                "Rust shader bundle requested SPIR-V output but gpu feature is disabled",
            ));
        }
    } else {
        None
    };

    Ok(RustBuildOutput { bundle, spirv })
}

#[cfg(not(feature = "sys"))]
pub fn compile_rust_build(
    _source: &str,
    _config: &RustBuildConfig,
) -> Result<RustBuildOutput, KainError> {
    Err(KainError::runtime(
        "Rust build bundling requires the sys feature",
    ))
}

pub fn run_rust_build_pipeline(
    input: &Path,
    output: Option<&PathBuf>,
    config: Option<&RustBuildConfig>,
) -> Result<Vec<PathBuf>, KainError> {
    let config = config.cloned().unwrap_or_default();
    let source = fs::read_to_string(input).map_err(|err| {
        KainError::runtime(format!("Failed to read {}: {}", input.display(), err))
    })?;
    let compiled = compile_rust_build(&source, &config)?;
    let base_name = input
        .file_stem()
        .and_then(|stem| stem.to_str())
        .unwrap_or("kain")
        .to_string();
    let output_root = resolve_file_mode_output_root(input, output, config.output.as_ref());
    write_rust_build_outputs(&output_root, &base_name, &config, &compiled)
}

pub fn write_rust_build_outputs(
    output_root: &Path,
    base_name: &str,
    config: &RustBuildConfig,
    compiled: &RustBuildOutput,
) -> Result<Vec<PathBuf>, KainError> {
    fs::create_dir_all(output_root).map_err(|err| {
        KainError::runtime(format!(
            "Failed to create Rust output directory {}: {}",
            output_root.display(),
            err
        ))
    })?;

    let mut written = Vec::new();

    #[cfg(feature = "sys")]
    {
        if config.artifacts.contains(&RustBuildArtifact::Source) {
            let path = output_root.join(format!("{}.rs", base_name));
            fs::write(&path, compiled.bundle.primary.contents.as_bytes()).map_err(|err| {
                KainError::runtime(format!(
                    "Failed to write Rust source output {}: {}",
                    path.display(),
                    err
                ))
            })?;
            written.push(path);
        }

        for artifact in &compiled.bundle.supplemental {
            let should_write = match artifact.kind {
                RustArtifactKind::PrimarySource => {
                    config.artifacts.contains(&RustBuildArtifact::Source)
                }
                RustArtifactKind::ShaderHost => {
                    config.artifacts.contains(&RustBuildArtifact::ShaderHost)
                }
                RustArtifactKind::ShaderReflection => config
                    .artifacts
                    .contains(&RustBuildArtifact::ShaderReflection),
            };
            if !should_write {
                continue;
            }

            let path = match artifact.kind {
                RustArtifactKind::PrimarySource => output_root.join(format!("{}.rs", base_name)),
                RustArtifactKind::ShaderHost => output_root.join(format!("{}.gpu.rs", base_name)),
                RustArtifactKind::ShaderReflection => {
                    output_root.join(format!("{}.reflect.json", base_name))
                }
            };
            fs::write(&path, artifact.contents.as_bytes()).map_err(|err| {
                KainError::runtime(format!(
                    "Failed to write Rust artifact output {}: {}",
                    path.display(),
                    err
                ))
            })?;
            written.push(path);
        }
    }

    if config.artifacts.contains(&RustBuildArtifact::Spirv) {
        if let Some(spirv) = &compiled.spirv {
            let path = output_root.join(format!("{}.spv", base_name));
            fs::write(&path, spirv).map_err(|err| {
                KainError::runtime(format!(
                    "Failed to write SPIR-V output {}: {}",
                    path.display(),
                    err
                ))
            })?;
            written.push(path);
        }
    }

    Ok(written)
}

fn resolve_file_mode_output_root(
    input: &Path,
    output: Option<&PathBuf>,
    configured_output: Option<&PathBuf>,
) -> PathBuf {
    if let Some(output) = output {
        if output.extension().is_some() {
            return output
                .parent()
                .map(Path::to_path_buf)
                .unwrap_or_else(|| PathBuf::from("."));
        }
        return output.clone();
    }

    if let Some(configured_output) = configured_output {
        if configured_output.is_absolute() {
            return configured_output.clone();
        }
        return input
            .parent()
            .map(|parent| parent.join(configured_output))
            .unwrap_or_else(|| configured_output.clone());
    }

    input
        .parent()
        .map(Path::to_path_buf)
        .unwrap_or_else(|| PathBuf::from("."))
}
