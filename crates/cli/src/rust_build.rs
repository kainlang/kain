use std::fs;
use std::path::{Component, Path, PathBuf};

use crate::packager::config::{RustBuildArtifact, RustBuildConfig};
use crate::{frontend_to_typed_program, CompileTarget};
use kain_core::error::KainError;
use kain_driver::{
    compile_native_app_bundle, discover_native_app_root_component, NativeAppBundle,
    NativeAppBundleConfig,
};

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
    let should_auto_configure_native_ui = config.is_none();
    let native_ui_was_explicit = config
        .and_then(|config| config.native_ui.as_ref())
        .is_some();
    let mut config = config.cloned().unwrap_or_default();
    let source = fs::read_to_string(input).map_err(|err| {
        KainError::runtime(format!("Failed to read {}: {}", input.display(), err))
    })?;
    let base_name = input
        .file_stem()
        .and_then(|stem| stem.to_str())
        .unwrap_or("kain")
        .to_string();
    let output_root = resolve_file_mode_output_root(input, output, config.output.as_ref());

    if config.native_ui.is_none() && should_auto_configure_native_ui {
        config.native_ui = Some(default_file_build_native_ui_config(&base_name));
    }

    let native_app_bundle = if let Some(native_ui) = &config.native_ui {
        let bundle_config = native_app_bundle_config_from_cli(
            input,
            &base_name,
            native_ui,
            config.artifacts.contains(&RustBuildArtifact::Spirv),
        )?;
        match discover_native_app_root_component(
            &source,
            bundle_config.root_component.as_deref(),
            &bundle_config
                .source_file_name
                .clone()
                .unwrap_or_else(|| "app.kn".to_string()),
        )? {
            Some(_) => Some(compile_native_app_bundle(&source, &bundle_config)?),
            None if native_ui_was_explicit => {
                return Err(KainError::runtime(format!(
                    "Rust native UI build was requested for {} but no components were found",
                    input.display()
                )));
            }
            None => None,
        }
    } else {
        None
    };

    let compiled = native_app_bundle
        .as_ref()
        .map(rust_build_output_from_native_app_bundle)
        .transpose()?
        .unwrap_or(compile_rust_build(&source, &config)?);

    let mut written = write_rust_build_outputs(&output_root, &base_name, &config, &compiled)?;

    if let (Some(native_ui), Some(bundle)) = (&config.native_ui, &native_app_bundle) {
        let project_dir = resolve_project_dir(
            &output_root,
            &bundle.metadata.app_name,
            native_ui.output.as_ref(),
        );
        let generated = crate::native_ui_build::run_native_ui_build_pipeline(
            input,
            &crate::native_ui_build::NativeUiBuildConfig {
                host: native_ui.host,
                tauri: native_ui.tauri.clone(),
                root_component: native_ui.root_component.clone(),
                window_title: native_ui.window_title.clone(),
                app_name: native_ui.app_name.clone(),
                project_dir: Some(project_dir),
                artifact_output_dir: PathBuf::from("generated"),
                initial_window_size: native_ui.initial_window_size,
                build_executable: native_ui.build_executable,
                executable_output_dir: native_ui.build_executable.then(|| output_root.clone()),
                release: native_ui.release,
                runtime_crate_name: "kain-ui-native".to_string(),
                runtime_dependency:
                    crate::native_ui_build::NativeUiRuntimeDependencyConfig::WorkspacePath,
                include_spirv: config.artifacts.contains(&RustBuildArtifact::Spirv),
            },
        )?;
        written.extend(generated.written_paths());
    }

    Ok(written)
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

fn default_file_build_native_ui_config(
    base_name: &str,
) -> crate::packager::config::RustNativeUiAppConfig {
    crate::packager::config::RustNativeUiAppConfig {
        host: crate::native_ui_build::NativeUiHostKind::Qt,
        tauri: crate::native_ui_build::NativeUiTauriConfig::default(),
        root_component: None,
        window_title: Some(base_name.to_string()),
        app_name: Some(base_name.to_string()),
        output: None,
        initial_window_size: [1440.0, 920.0],
        build_executable: true,
        release: false,
    }
}

fn native_app_bundle_config_from_cli(
    input: &Path,
    base_name: &str,
    config: &crate::packager::config::RustNativeUiAppConfig,
    include_spirv: bool,
) -> Result<NativeAppBundleConfig, KainError> {
    Ok(NativeAppBundleConfig {
        app_name: config
            .app_name
            .clone()
            .filter(|value| !value.trim().is_empty())
            .or_else(|| Some(base_name.to_string())),
        window_title: config
            .window_title
            .clone()
            .filter(|value| !value.trim().is_empty())
            .or_else(|| Some(base_name.to_string())),
        root_component: config.root_component.clone(),
        source_file_name: input
            .file_name()
            .and_then(|value| value.to_str())
            .map(|value| value.to_string()),
        source_root: absolute_path(input)?.parent().map(Path::to_path_buf),
        initial_window_size: config.initial_window_size,
        include_spirv,
    })
}

fn rust_build_output_from_native_app_bundle(
    bundle: &NativeAppBundle,
) -> Result<RustBuildOutput, KainError> {
    Ok(RustBuildOutput {
        bundle: bundle.rust.bundle.clone(),
        spirv: bundle.rust.spirv.clone(),
    })
}

fn resolve_project_dir(
    output_root: &Path,
    app_name: &str,
    configured_output: Option<&PathBuf>,
) -> PathBuf {
    match configured_output {
        Some(path) if path.is_absolute() => path.clone(),
        Some(path) => output_root.join(path),
        None => output_root.join(format!("{app_name}-native-ui")),
    }
}

fn absolute_path(path: &Path) -> Result<PathBuf, KainError> {
    if path.is_absolute() {
        return Ok(path.to_path_buf());
    }

    let cwd = std::env::current_dir()
        .map_err(|err| KainError::runtime(format!("Failed to resolve current directory: {err}")))?;
    Ok(cwd.join(path))
}

fn resolve_workspace_root() -> Result<PathBuf, KainError> {
    let cli_manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let Some(workspace_root) = cli_manifest_dir.parent().and_then(Path::parent) else {
        return Err(KainError::runtime(
            "Failed to derive the Kain workspace root from the CLI crate path",
        ));
    };

    Ok(workspace_root.to_path_buf())
}

fn diff_paths(path: &Path, base: &Path) -> Option<PathBuf> {
    let path_components: Vec<_> = path.components().collect();
    let base_components: Vec<_> = base.components().collect();

    let shared = shared_path_prefix_len(&path_components, &base_components);
    if shared == 0 {
        return None;
    }

    let mut result = PathBuf::new();
    for _ in shared..base_components.len() {
        result.push("..");
    }
    for component in &path_components[shared..] {
        match component {
            Component::Normal(part) => result.push(part),
            Component::CurDir => result.push("."),
            Component::ParentDir => result.push(".."),
            Component::RootDir => {}
            Component::Prefix(prefix) => result.push(prefix.as_os_str()),
        }
    }

    if result.as_os_str().is_empty() {
        result.push(".");
    }

    Some(result)
}

fn shared_path_prefix_len(path: &[Component<'_>], base: &[Component<'_>]) -> usize {
    let mut shared = 0;
    while shared < path.len() && shared < base.len() && path[shared] == base[shared] {
        shared += 1;
    }
    shared
}
