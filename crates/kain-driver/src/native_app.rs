use std::ffi::OsStr;
use std::fs;
use std::path::{Component, Path, PathBuf};
use std::process::Command;

use crate::{DriverSession, RustBundleOutput, ShaderArtifactBundleOutput};
use kain_core::ast::Item;
use kain_core::diagnostics::SpanMapper;
use kain_core::error::KainError;
use kain_core::{
    build_ui_output_from_source, realtime_app_bundle_to_json, runtime_contract_bundle_to_json,
    CompileTarget, Lexer, Parser, RealtimeAppBundle, RuntimeContractBundle,
};
use kain_ui::{
    ui_runtime_bundle_from_output, ui_runtime_bundle_to_json, UiBuildOutput, UiRuntimeMetadata,
};

const NATIVE_APP_RUNTIME_BUNDLE_FILE_NAME: &str = "native_app_bundle.json";
const NATIVE_APP_RUNTIME_CONTRACT_FILE_NAME: &str = "kain_runtime_contract.json";
const NATIVE_APP_REALTIME_BUNDLE_FILE_NAME: &str = "kain_realtime_app_bundle.json";
const NATIVE_APP_SHADER_BUNDLE_FILE_NAME: &str = "kain_shader_bundle.json";
const NATIVE_RUNTIME_VERSION_METADATA_FILE_NAME: &str = "kain_runtime_version.json";

#[derive(Debug, Clone)]
pub struct NativeAppBundleConfig {
    pub app_name: Option<String>,
    pub window_title: Option<String>,
    pub root_component: Option<String>,
    pub source_file_name: Option<String>,
    pub initial_window_size: [f32; 2],
    pub include_spirv: bool,
}

impl Default for NativeAppBundleConfig {
    fn default() -> Self {
        Self {
            app_name: None,
            window_title: None,
            root_component: None,
            source_file_name: Some("app.kn".to_string()),
            initial_window_size: [1440.0, 920.0],
            include_spirv: true,
        }
    }
}

/// Runtime version metadata loaded from native_runtime.toml
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct RuntimeVersionMetadata {
    pub runtime_major: u32,
    pub runtime_minor: u32,
    pub runtime_patch: u32,
    pub abi_major: u32,
    pub abi_minor: u32,
    pub abi_patch: u32,
    pub runtime_version_string: String,
    pub abi_version_string: String,
    pub compatibility_class: String,
    pub runtime_lane: String,
}

impl RuntimeVersionMetadata {
    /// Load runtime version metadata from native_runtime.toml
    pub fn load_from_runtime_manifest() -> Result<Self, KainError> {
        let manifest_path = find_native_runtime_manifest()
            .ok_or_else(|| KainError::runtime("Could not locate runtime/native_runtime.toml"))?;
        
        let manifest_source = fs::read_to_string(&manifest_path).map_err(|err| {
            KainError::runtime(format!(
                "Failed to read runtime manifest {}: {}",
                manifest_path.display(),
                err
            ))
        })?;

        #[derive(serde::Deserialize)]
        struct VersionSection {
            runtime_major: u32,
            runtime_minor: u32,
            runtime_patch: u32,
            abi_major: u32,
            abi_minor: u32,
            abi_patch: u32,
        }

        #[derive(serde::Deserialize)]
        struct MetadataSection {
            compatibility_class: String,
            runtime_lane: String,
        }

        #[derive(serde::Deserialize)]
        struct RuntimeManifest {
            version: VersionSection,
            metadata: MetadataSection,
        }

        let manifest: RuntimeManifest = toml::from_str(&manifest_source).map_err(|err| {
            KainError::runtime(format!(
                "Failed to parse runtime manifest {}: {}",
                manifest_path.display(),
                err
            ))
        })?;

        let runtime_version_string = format!(
            "{}.{}.{}",
            manifest.version.runtime_major,
            manifest.version.runtime_minor,
            manifest.version.runtime_patch
        );

        let abi_version_string = format!(
            "{}.{}.{}",
            manifest.version.abi_major,
            manifest.version.abi_minor,
            manifest.version.abi_patch
        );

        Ok(Self {
            runtime_major: manifest.version.runtime_major,
            runtime_minor: manifest.version.runtime_minor,
            runtime_patch: manifest.version.runtime_patch,
            abi_major: manifest.version.abi_major,
            abi_minor: manifest.version.abi_minor,
            abi_patch: manifest.version.abi_patch,
            runtime_version_string,
            abi_version_string,
            compatibility_class: manifest.metadata.compatibility_class,
            runtime_lane: manifest.metadata.runtime_lane,
        })
    }
}

fn find_native_runtime_manifest() -> Option<PathBuf> {
    // Try KAIN_RUNTIME_MANIFEST environment variable
    if let Ok(explicit) = std::env::var("KAIN_RUNTIME_MANIFEST") {
        let candidate = PathBuf::from(explicit);
        if candidate.exists() {
            return Some(candidate);
        }
    }

    // Try relative to current directory
    if let Ok(mut dir) = std::env::current_dir() {
        for _ in 0..10 {
            let candidate = dir.join("runtime").join("native_runtime.toml");
            if candidate.exists() {
                return Some(candidate);
            }
            match dir.parent() {
                Some(parent) => dir = parent.to_path_buf(),
                None => break,
            }
        }
    }

    // Try relative to executable
    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(mut dir) = exe_path.parent().map(|p| p.to_path_buf()) {
            loop {
                let candidate = dir.join("runtime").join("native_runtime.toml");
                if candidate.exists() {
                    return Some(candidate);
                }
                if !dir.pop() {
                    break;
                }
            }
        }
    }

    None
}

#[derive(Debug, Clone)]
pub struct NativeAppMetadata {
    pub app_name: String,
    pub window_title: String,
    pub root_component: String,
    pub source_file_name: String,
    pub initial_window_size: [f32; 2],
}

#[derive(Debug, Clone)]
pub struct NativeAppBundle {
    pub metadata: NativeAppMetadata,
    pub runtime_contract: RuntimeContractBundle,
    pub realtime: RealtimeAppBundle,
    pub shader_bundle: Option<ShaderArtifactBundleOutput>,
    pub ui: UiBuildOutput,
    pub rust: RustBundleOutput,
    pub runtime_version: Option<RuntimeVersionMetadata>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum NativeAppRuntimeDependency {
    Path(PathBuf),
    Version(String),
}

#[derive(Debug, Clone)]
pub struct NativeAppMaterializationConfig {
    pub project_dir: PathBuf,
    pub runtime_crate_name: String,
    pub runtime_dependency: NativeAppRuntimeDependency,
    pub artifact_output_dir: PathBuf,
    pub build_executable: bool,
    pub release: bool,
    pub executable_output_dir: Option<PathBuf>,
}

#[derive(Debug, Clone)]
pub struct NativeAppMaterializedPaths {
    pub project_dir: PathBuf,
    pub manifest_path: PathBuf,
    pub main_rs_path: PathBuf,
    pub source_copy_path: PathBuf,
    pub artifact_paths: Vec<PathBuf>,
    pub executable_path: Option<PathBuf>,
}

impl DriverSession {
    pub fn compile_native_app_bundle(
        &self,
        source: &str,
        config: &NativeAppBundleConfig,
    ) -> Result<NativeAppBundle, KainError> {
        let source_file_name = normalized_source_file_name(config.source_file_name.as_deref());
        let source_name = source_file_name.as_str();
        let root_component = discover_native_app_root_component(
            source,
            config.root_component.as_deref(),
            source_name,
        )?
        .ok_or_else(|| {
            KainError::runtime(format!(
                "Native app bundle generation requires at least one component in {source_name}"
            ))
        })?;
        let base_name = source_stem(source_name);
        let app_name = sanitize_cargo_name(config.app_name.as_deref().unwrap_or(&base_name));
        let window_title = config
            .window_title
            .clone()
            .filter(|value| !value.trim().is_empty())
            .unwrap_or_else(|| root_component.clone());
        let runtime_contract = self.compile_runtime_contract_bundle(source, CompileTarget::Rust)?;
        let realtime = self
            .compile_realtime_app_bundle(source, CompileTarget::Rust, Some(&root_component))?
            .bundle;
        let shader_bundle = self.compile_shader_artifact_bundle(source).ok();
        let ui = build_ui_output_from_source(source, &root_component)?;
        let rust = self.compile_rust_artifact_bundle(source, config.include_spirv)?;

        // Load runtime version metadata
        let runtime_version = RuntimeVersionMetadata::load_from_runtime_manifest().ok();

        Ok(NativeAppBundle {
            metadata: NativeAppMetadata {
                app_name,
                window_title,
                root_component,
                source_file_name,
                initial_window_size: config.initial_window_size,
            },
            runtime_contract,
            realtime,
            shader_bundle,
            ui,
            rust,
            runtime_version,
        })
    }

    pub fn materialize_native_app_bundle(
        &self,
        source: &str,
        bundle: &NativeAppBundle,
        config: &NativeAppMaterializationConfig,
    ) -> Result<NativeAppMaterializedPaths, KainError> {
        let project_dir = &config.project_dir;
        fs::create_dir_all(project_dir.join("src"))
            .map_err(io_error("create native app source directory"))?;

        let source_copy_path = project_dir.join(&bundle.metadata.source_file_name);
        fs::write(&source_copy_path, source.as_bytes())
            .map_err(io_error("write embedded native app Kain source"))?;

        let artifact_root = if config.artifact_output_dir.is_absolute() {
            config.artifact_output_dir.clone()
        } else {
            project_dir.join(&config.artifact_output_dir)
        };
        fs::create_dir_all(&artifact_root)
            .map_err(io_error("create native app artifact directory"))?;

        let mut artifact_paths = Vec::new();
        let runtime_bundle_path = artifact_root.join(NATIVE_APP_RUNTIME_BUNDLE_FILE_NAME);
        let runtime_bundle_json = render_runtime_bundle_json(bundle)?;
        fs::write(&runtime_bundle_path, runtime_bundle_json.as_bytes())
            .map_err(io_error("write native app runtime bundle"))?;
        artifact_paths.push(runtime_bundle_path.clone());

        let runtime_contract_path = artifact_root.join(NATIVE_APP_RUNTIME_CONTRACT_FILE_NAME);
        let runtime_contract_json = render_runtime_contract_json(bundle)?;
        fs::write(&runtime_contract_path, runtime_contract_json.as_bytes())
            .map_err(io_error("write native app runtime contract"))?;
        artifact_paths.push(runtime_contract_path);

        let realtime_bundle_path = artifact_root.join(NATIVE_APP_REALTIME_BUNDLE_FILE_NAME);
        let realtime_bundle_json = render_realtime_bundle_json(bundle)?;
        fs::write(&realtime_bundle_path, realtime_bundle_json.as_bytes())
            .map_err(io_error("write native app realtime bundle"))?;
        artifact_paths.push(realtime_bundle_path.clone());

        // Write runtime version metadata if available
        if let Some(runtime_version) = &bundle.runtime_version {
            let version_metadata_path = artifact_root.join(NATIVE_RUNTIME_VERSION_METADATA_FILE_NAME);
            let version_metadata_json = serde_json::to_string_pretty(runtime_version)
                .map_err(|err| {
                    KainError::runtime(format!(
                        "Failed to serialize runtime version metadata: {}",
                        err
                    ))
                })?;
            fs::write(&version_metadata_path, version_metadata_json.as_bytes())
                .map_err(io_error("write native runtime version metadata"))?;
            artifact_paths.push(version_metadata_path);
        }

        let shader_bundle_path = if let Some(shader_bundle) = &bundle.shader_bundle {
            let path = artifact_root.join(NATIVE_APP_SHADER_BUNDLE_FILE_NAME);
            fs::write(&path, shader_bundle.bundle_json.as_bytes())
                .map_err(io_error("write native app shader bundle"))?;
            artifact_paths.push(path.clone());
            Some(path)
        } else {
            None
        };

        let primary_path = artifact_root.join(&bundle.rust.bundle.primary.suggested_file_name);
        fs::write(
            &primary_path,
            bundle.rust.bundle.primary.contents.as_bytes(),
        )
        .map_err(io_error("write native app primary Rust artifact"))?;
        artifact_paths.push(primary_path);

        for artifact in &bundle.rust.bundle.supplemental {
            let path = artifact_root.join(&artifact.suggested_file_name);
            fs::write(&path, artifact.contents.as_bytes())
                .map_err(io_error("write native app supplemental artifact"))?;
            artifact_paths.push(path);
        }

        if let Some(spirv) = &bundle.rust.spirv {
            let spirv_path = artifact_root.join("kain_gpu.spv");
            fs::write(&spirv_path, spirv).map_err(io_error("write native app SPIR-V artifact"))?;
            artifact_paths.push(spirv_path);
        }

        let manifest_path = project_dir.join("Cargo.toml");
        let manifest = render_manifest(
            &bundle.metadata.app_name,
            &config.runtime_crate_name,
            &config.runtime_dependency,
        );
        fs::write(&manifest_path, manifest.as_bytes())
            .map_err(io_error("write native app Cargo manifest"))?;

        let main_rs_path = project_dir.join("src").join("main.rs");
        let runtime_bundle_include_path = relative_path_from_directory(
            main_rs_path.parent().unwrap_or(project_dir),
            &runtime_bundle_path,
        )
        .unwrap_or_else(|| runtime_bundle_path.clone());
        let realtime_bundle_file_name = realtime_bundle_path
            .file_name()
            .and_then(OsStr::to_str)
            .unwrap_or(NATIVE_APP_REALTIME_BUNDLE_FILE_NAME);
        let shader_bundle_file_name = shader_bundle_path.as_ref().and_then(|path| {
            path.file_name()
                .and_then(OsStr::to_str)
                .map(ToOwned::to_owned)
        });
        let main_rs = render_main_rs(
            &runtime_bundle_include_path,
            &config.runtime_crate_name,
            realtime_bundle_file_name,
            shader_bundle_file_name.as_deref(),
        );
        fs::write(&main_rs_path, main_rs.as_bytes())
            .map_err(io_error("write native app entrypoint"))?;

        let executable_path = if config.build_executable {
            Some(build_native_app_executable(
                project_dir,
                &bundle.metadata.app_name,
                config.release,
                config.executable_output_dir.as_deref(),
            )?)
        } else {
            None
        };

        if let (Some(executable_path), Some(output_dir)) = (
            executable_path.as_ref(),
            config.executable_output_dir.as_deref(),
        ) {
            copy_runtime_sidecars_to_executable_dir(
                executable_path,
                output_dir,
                &artifact_paths,
                &[
                    NATIVE_APP_RUNTIME_CONTRACT_FILE_NAME,
                    NATIVE_APP_REALTIME_BUNDLE_FILE_NAME,
                    NATIVE_APP_SHADER_BUNDLE_FILE_NAME,
                    NATIVE_RUNTIME_VERSION_METADATA_FILE_NAME,
                ],
            )?;
        }

        Ok(NativeAppMaterializedPaths {
            project_dir: project_dir.clone(),
            manifest_path,
            main_rs_path,
            source_copy_path,
            artifact_paths,
            executable_path,
        })
    }
}

pub fn compile_native_app_bundle(
    source: &str,
    config: &NativeAppBundleConfig,
) -> Result<NativeAppBundle, KainError> {
    DriverSession::default().compile_native_app_bundle(source, config)
}

pub fn materialize_native_app_bundle(
    source: &str,
    bundle: &NativeAppBundle,
    config: &NativeAppMaterializationConfig,
) -> Result<NativeAppMaterializedPaths, KainError> {
    DriverSession::default().materialize_native_app_bundle(source, bundle, config)
}

pub fn discover_native_app_root_component(
    source: &str,
    configured_root: Option<&str>,
    source_name: &str,
) -> Result<Option<String>, KainError> {
    let tokens = Lexer::new(source).tokenize()?;
    let span_mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &span_mapper, source_name).parse()?;
    let component_names: Vec<_> = program
        .items
        .iter()
        .filter_map(|item| match item {
            Item::Component(component) => Some(component.name.clone()),
            _ => None,
        })
        .collect();

    if let Some(root) = configured_root.filter(|value| !value.trim().is_empty()) {
        if component_names.iter().any(|name| name == root) {
            return Ok(Some(root.to_string()));
        }
        return Err(KainError::runtime(format!(
            "Configured native app root component '{}' was not found in {}",
            root, source_name
        )));
    }

    if component_names.is_empty() {
        return Ok(None);
    }

    if let Some(app) = component_names.iter().find(|name| name.as_str() == "App") {
        return Ok(Some(app.clone()));
    }

    Ok(component_names.into_iter().next())
}

fn render_manifest(
    app_name: &str,
    runtime_crate_name: &str,
    runtime_dependency: &NativeAppRuntimeDependency,
) -> String {
    let dependency = match runtime_dependency {
        NativeAppRuntimeDependency::Path(path) => {
            format!(r#"{{ path = "{}" }}"#, path_for_toml(path))
        }
        NativeAppRuntimeDependency::Version(version) => {
            format!(r#"{{ version = "{version}" }}"#)
        }
    };

    format!(
        "[package]\nname = \"{app_name}\"\nversion = \"0.1.0\"\nedition = \"2021\"\n\n[workspace]\n\n[dependencies]\n{runtime_crate_name} = {dependency}\n"
    )
}

fn render_main_rs(
    runtime_bundle_include_path: &Path,
    runtime_crate_name: &str,
    realtime_bundle_file_name: &str,
    shader_bundle_file_name: Option<&str>,
) -> String {
    let runtime_bundle_include_path =
        rust_string_literal(&path_for_toml(runtime_bundle_include_path));
    let runtime_module_name = runtime_crate_name.replace('-', "_");
    let realtime_bundle_file_name = rust_string_literal(realtime_bundle_file_name);
    let shader_bundle_env = shader_bundle_file_name.map(rust_string_literal);
    let shader_bundle_setter = shader_bundle_env
        .as_deref()
        .map(|file_name| {
            format!(
                "    if let Some(path) = resolve_runtime_sidecar({file_name}) {{\n        std::env::set_var(\"KAIN_UI_NATIVE_SHADER_BUNDLE\", &path);\n    }}\n"
            )
        })
        .unwrap_or_default();

    format!(
        "#![cfg_attr(all(target_os = \"windows\", not(debug_assertions)), windows_subsystem = \"windows\")]\n\nuse std::path::PathBuf;\n\nuse {runtime_module_name}::run_bundled_app_json;\n\nconst KAIN_RUNTIME_BUNDLE: &str = include_str!({runtime_bundle_include_path});\n\nfn resolve_runtime_sidecar(file_name: &str) -> Option<PathBuf> {{\n    if let Some(current_exe_candidate) = std::env::current_exe().ok().and_then(|exe| {{\n        exe.parent().map(|dir| dir.join(file_name)).filter(|path| path.exists())\n    }}) {{\n        return Some(current_exe_candidate);\n    }}\n    let manifest_candidate = PathBuf::from(env!(\"CARGO_MANIFEST_DIR\")).join(\"generated\").join(file_name);\n    if manifest_candidate.exists() {{\n        return Some(manifest_candidate);\n    }}\n    None\n}}\n\nfn main() -> Result<(), Box<dyn std::error::Error>> {{\n    if let Some(path) = resolve_runtime_sidecar({realtime_bundle_file_name}) {{\n        std::env::set_var(\"KAIN_UI_NATIVE_REALTIME_BUNDLE\", &path);\n    }}\n{shader_bundle_setter}    run_bundled_app_json(KAIN_RUNTIME_BUNDLE)\n}}\n"
    )
}

fn copy_runtime_sidecars_to_executable_dir(
    executable_path: &Path,
    output_dir: &Path,
    artifact_paths: &[PathBuf],
    file_names: &[&str],
) -> Result<(), KainError> {
    let Some(executable_dir) = executable_path.parent() else {
        return Ok(());
    };
    if executable_dir != output_dir {
        return Ok(());
    }
    for file_name in file_names {
        if let Some(source) = artifact_paths
            .iter()
            .find(|path| path.file_name().and_then(OsStr::to_str) == Some(*file_name))
        {
            let destination = executable_dir.join(file_name);
            fs::copy(source, &destination).map_err(io_error("copy native app runtime sidecar"))?;
        }
    }
    Ok(())
}

fn build_native_app_executable(
    project_dir: &Path,
    app_name: &str,
    release: bool,
    output_dir: Option<&Path>,
) -> Result<PathBuf, KainError> {
    let mut command = Command::new("cargo");
    command.arg("build");
    if release {
        command.arg("--release");
    }
    command.current_dir(project_dir);

    let output = command.output().map_err(|err| {
        KainError::runtime(format!(
            "Failed to invoke cargo to build native app at {}: {}",
            project_dir.display(),
            err
        ))
    })?;

    if !output.status.success() {
        return Err(KainError::runtime(format!(
            "Native app cargo build failed for {}:\n{}\n{}",
            project_dir.display(),
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr)
        )));
    }

    let built_executable = project_dir
        .join("target")
        .join(if release { "release" } else { "debug" })
        .join(binary_file_name(app_name));

    if !built_executable.exists() {
        return Err(KainError::runtime(format!(
            "Cargo reported success but no executable was found at {}",
            built_executable.display()
        )));
    }

    if let Some(output_dir) = output_dir {
        fs::create_dir_all(output_dir)
            .map_err(io_error("create native app executable output directory"))?;
        let copied_executable = output_dir.join(binary_file_name(app_name));
        fs::copy(&built_executable, &copied_executable)
            .map_err(io_error("copy native app executable"))?;
        return Ok(copied_executable);
    }

    Ok(built_executable)
}

fn binary_file_name(app_name: &str) -> String {
    if cfg!(windows) {
        format!("{app_name}.exe")
    } else {
        app_name.to_string()
    }
}

fn normalized_source_file_name(source_file_name: Option<&str>) -> String {
    source_file_name
        .and_then(|value| {
            let trimmed = value.trim();
            if trimmed.is_empty() {
                None
            } else {
                Some(trimmed.to_string())
            }
        })
        .unwrap_or_else(|| "app.kn".to_string())
}

fn source_stem(source_name: &str) -> String {
    Path::new(source_name)
        .file_stem()
        .and_then(OsStr::to_str)
        .filter(|value| !value.trim().is_empty())
        .unwrap_or("kain")
        .to_string()
}

fn sanitize_cargo_name(raw: &str) -> String {
    let mut sanitized = String::with_capacity(raw.len());
    let mut last_was_dash = false;

    for ch in raw.chars() {
        let mapped = match ch {
            'A'..='Z' => ch.to_ascii_lowercase(),
            'a'..='z' | '0'..='9' => ch,
            _ => '-',
        };

        if mapped == '-' {
            if !last_was_dash {
                sanitized.push(mapped);
                last_was_dash = true;
            }
        } else {
            sanitized.push(mapped);
            last_was_dash = false;
        }
    }

    let trimmed = sanitized.trim_matches('-');
    let mut result = if trimmed.is_empty() {
        "kain-ui-app".to_string()
    } else {
        trimmed.to_string()
    };

    if result
        .chars()
        .next()
        .is_some_and(|ch| !ch.is_ascii_alphabetic())
    {
        result.insert_str(0, "kain-ui-");
    }

    result
}

fn rust_string_literal(value: &str) -> String {
    format!("{value:?}")
}

fn render_runtime_bundle_json(bundle: &NativeAppBundle) -> Result<String, KainError> {
    let runtime_bundle = ui_runtime_bundle_from_output(
        UiRuntimeMetadata {
            app_name: Some(bundle.metadata.app_name.clone()),
            window_title: bundle.metadata.window_title.clone(),
            root_component: bundle.metadata.root_component.clone(),
            source_file_name: Some(bundle.metadata.source_file_name.clone()),
            initial_window_size: bundle.metadata.initial_window_size,
        },
        bundle.ui.clone(),
    );

    ui_runtime_bundle_to_json(&runtime_bundle).map_err(|err| {
        KainError::runtime(format!(
            "Failed to serialize native app runtime bundle for {}: {err}",
            bundle.metadata.app_name
        ))
    })
}

fn render_runtime_contract_json(bundle: &NativeAppBundle) -> Result<String, KainError> {
    runtime_contract_bundle_to_json(&bundle.runtime_contract).map_err(|err| {
        KainError::runtime(format!(
            "Failed to serialize runtime contract bundle for {}: {err}",
            bundle.metadata.app_name
        ))
    })
}

fn render_realtime_bundle_json(bundle: &NativeAppBundle) -> Result<String, KainError> {
    realtime_app_bundle_to_json(&bundle.realtime).map_err(|err| {
        KainError::runtime(format!(
            "Failed to serialize realtime app bundle for {}: {err}",
            bundle.metadata.app_name
        ))
    })
}

fn relative_path_from_directory(from_dir: &Path, to_path: &Path) -> Option<PathBuf> {
    let from_components: Vec<_> = from_dir.components().collect();
    let to_components: Vec<_> = to_path.components().collect();

    if from_components.is_empty() || to_components.is_empty() {
        return None;
    }

    if !components_share_prefix(&from_components, &to_components) {
        return None;
    }

    let shared_prefix_len = from_components
        .iter()
        .zip(&to_components)
        .take_while(|(left, right)| left == right)
        .count();

    let mut relative = PathBuf::new();
    for _ in shared_prefix_len..from_components.len() {
        relative.push("..");
    }
    for component in &to_components[shared_prefix_len..] {
        relative.push(component.as_os_str());
    }

    if relative.as_os_str().is_empty() {
        None
    } else {
        Some(relative)
    }
}

fn components_share_prefix(left: &[Component<'_>], right: &[Component<'_>]) -> bool {
    match (left.first(), right.first()) {
        (Some(Component::Prefix(left_prefix)), Some(Component::Prefix(right_prefix))) => {
            left_prefix.kind() == right_prefix.kind()
        }
        (Some(Component::RootDir), Some(Component::RootDir)) => true,
        (Some(Component::Normal(left_normal)), Some(Component::Normal(right_normal))) => {
            left_normal == right_normal
        }
        (Some(Component::CurDir), Some(Component::CurDir)) => true,
        _ => false,
    }
}

fn path_for_toml(path: &Path) -> String {
    path.to_string_lossy().replace('\\', "/")
}

fn io_error(context: &'static str) -> impl Fn(std::io::Error) -> KainError {
    move |err| KainError::runtime(format!("Failed to {context}: {err}"))
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    #[test]
    fn discover_root_component_prefers_app_when_present() {
        let source = r#"
component Shell():
    render <panel />

component App():
    render <panel />
"#;

        let root = discover_native_app_root_component(source, None, "app.kn")
            .expect("component parse should succeed");
        assert_eq!(root.as_deref(), Some("App"));
    }

    #[test]
    fn compile_native_app_bundle_collects_ui_and_rust_outputs() {
        let source = r#"
component App():
    render <panel title="Studio" />
"#;

        let bundle = compile_native_app_bundle(
            source,
            &NativeAppBundleConfig {
                app_name: Some("Studio Shell".to_string()),
                window_title: Some("Studio Shell".to_string()),
                root_component: None,
                source_file_name: Some("studio.kn".to_string()),
                initial_window_size: [1600.0, 900.0],
                include_spirv: true,
            },
        )
        .expect("native app bundle generation should succeed");

        assert_eq!(bundle.metadata.app_name, "studio-shell");
        assert_eq!(bundle.metadata.root_component, "App");
        assert_eq!(bundle.metadata.source_file_name, "studio.kn");
        assert_eq!(bundle.runtime_contract.target, "rust");
        assert!(bundle
            .runtime_contract
            .required_capabilities
            .iter()
            .any(|capability| capability.key == "ui.runtime-bundle"));
        assert!(bundle.ui.tree.root.is_some());
        assert!(bundle.rust.bundle.primary.contents.contains("fn"));
    }

    #[test]
    fn materialize_native_app_bundle_writes_scaffold_and_artifacts() {
        let temp = TempDir::new().expect("temp dir");
        let project_dir = temp.path().join("native-app");
        let source = r#"
component App():
    render <panel title="Bundle Test" />
"#;
        let bundle = compile_native_app_bundle(
            source,
            &NativeAppBundleConfig {
                source_file_name: Some("bundle_test.kn".to_string()),
                ..Default::default()
            },
        )
        .expect("bundle should compile");

        let materialized = materialize_native_app_bundle(
            source,
            &bundle,
            &NativeAppMaterializationConfig {
                project_dir: project_dir.clone(),
                runtime_crate_name: "kain-ui-native".to_string(),
                runtime_dependency: NativeAppRuntimeDependency::Version("0.1.0".to_string()),
                artifact_output_dir: PathBuf::from("generated"),
                build_executable: false,
                release: false,
                executable_output_dir: None,
            },
        )
        .expect("materialization should succeed");

        assert_eq!(materialized.project_dir, project_dir);
        assert!(materialized.manifest_path.exists());
        assert!(materialized.main_rs_path.exists());
        assert!(materialized.source_copy_path.exists());
        assert!(!materialized.artifact_paths.is_empty());

        let manifest = fs::read_to_string(&materialized.manifest_path).expect("manifest");
        assert!(manifest.contains("kain-ui-native"));
        let main_rs = fs::read_to_string(&materialized.main_rs_path).expect("main.rs");
        assert!(main_rs.contains("run_bundled_app_json"));
        assert!(materialized
            .artifact_paths
            .iter()
            .any(|path| path.ends_with(NATIVE_APP_RUNTIME_BUNDLE_FILE_NAME)));
        assert!(materialized
            .artifact_paths
            .iter()
            .any(|path| path.ends_with(NATIVE_APP_RUNTIME_CONTRACT_FILE_NAME)));
    }

    #[test]
    fn materialize_native_app_bundle_includes_runtime_version_metadata() {
        let temp = TempDir::new().expect("temp dir");
        let project_dir = temp.path().join("native-app-with-version");
        let source = r#"
component App():
    render <panel title="Version Test" />
"#;
        let bundle = compile_native_app_bundle(
            source,
            &NativeAppBundleConfig {
                source_file_name: Some("version_test.kn".to_string()),
                ..Default::default()
            },
        )
        .expect("bundle should compile");

        // Check that runtime version metadata was loaded (if manifest is available)
        if bundle.runtime_version.is_some() {
            let version = bundle.runtime_version.as_ref().unwrap();
            assert_eq!(version.abi_major, 0);
            assert_eq!(version.abi_minor, 1);
            assert_eq!(version.runtime_major, 0);
            assert_eq!(version.runtime_minor, 1);
            assert_eq!(version.abi_version_string, "0.1.0");
            assert_eq!(version.runtime_version_string, "0.1.0");
            assert_eq!(version.compatibility_class, "experimental");
            assert_eq!(version.runtime_lane, "raw-native");
        }

        let materialized = materialize_native_app_bundle(
            source,
            &bundle,
            &NativeAppMaterializationConfig {
                project_dir: project_dir.clone(),
                runtime_crate_name: "kain-ui-native".to_string(),
                runtime_dependency: NativeAppRuntimeDependency::Version("0.1.0".to_string()),
                artifact_output_dir: PathBuf::from("generated"),
                build_executable: false,
                release: false,
                executable_output_dir: None,
            },
        )
        .expect("materialization should succeed");

        // Check if runtime version metadata file was written (if metadata was available)
        if bundle.runtime_version.is_some() {
            let version_metadata_path = materialized
                .artifact_paths
                .iter()
                .find(|path| path.ends_with(NATIVE_RUNTIME_VERSION_METADATA_FILE_NAME));
            
            assert!(
                version_metadata_path.is_some(),
                "Runtime version metadata file should be written when metadata is available"
            );

            if let Some(path) = version_metadata_path {
                assert!(path.exists(), "Runtime version metadata file should exist");
                let metadata_json = fs::read_to_string(path).expect("read version metadata");
                assert!(metadata_json.contains("abi_version_string"));
                assert!(metadata_json.contains("runtime_version_string"));
                assert!(metadata_json.contains("0.1.0"));
            }
        }
    }
}
