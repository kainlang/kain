use std::fs;
use std::path::{Component, Path, PathBuf};

use kain_core::error::KainError;
use kain_driver::{
    compile_native_app_bundle, discover_native_app_root_component, materialize_native_app_bundle,
    NativeAppBundleConfig, NativeAppHostSidecarBinding, NativeAppLauncherEntrypoint,
    NativeAppMaterializationConfig, NativeAppMetadata, NativeAppRuntimeDependency,
};
#[cfg(feature = "tauri")]
use kain_driver::{
    compile_tauri_app_bundle, materialize_tauri_app_bundle, TauriAppBundleConfig,
    TauriAppMaterializationConfig,
};

const DEFAULT_RUNTIME_CRATE_NAME: &str = "kain-ui-native";
const DEFAULT_ARTIFACT_OUTPUT_DIR: &str = "generated";
const DEFAULT_WINDOW_SIZE: [f32; 2] = [1440.0, 920.0];

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, serde::Serialize, serde::Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum NativeUiHostKind {
    #[default]
    Qt,
    Tauri,
}

impl NativeUiHostKind {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Qt => "qt",
            Self::Tauri => "tauri",
        }
    }
}

#[derive(Debug, Clone, Default, serde::Serialize, serde::Deserialize)]
#[serde(default)]
pub struct NativeUiTauriConfig {
    pub bundle_identifier: Option<String>,
    pub window_label: Option<String>,
    pub cargo_package_name: Option<String>,
    pub plugin_presets: Vec<String>,
    pub capability_presets: Vec<String>,
    pub permission_presets: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum NativeUiRuntimeDependencyConfig {
    WorkspacePath,
    Path(PathBuf),
    Version(String),
}

impl Default for NativeUiRuntimeDependencyConfig {
    fn default() -> Self {
        Self::WorkspacePath
    }
}

#[derive(Debug, Clone)]
pub struct NativeUiBuildConfig {
    pub host: NativeUiHostKind,
    pub tauri: NativeUiTauriConfig,
    pub root_component: Option<String>,
    pub window_title: Option<String>,
    pub app_name: Option<String>,
    pub project_dir: Option<PathBuf>,
    pub artifact_output_dir: PathBuf,
    pub initial_window_size: [f32; 2],
    pub build_executable: bool,
    pub executable_output_dir: Option<PathBuf>,
    pub release: bool,
    pub runtime_crate_name: String,
    pub runtime_dependency: NativeUiRuntimeDependencyConfig,
    pub include_spirv: bool,
}

impl Default for NativeUiBuildConfig {
    fn default() -> Self {
        Self {
            host: NativeUiHostKind::Qt,
            tauri: NativeUiTauriConfig::default(),
            root_component: None,
            window_title: None,
            app_name: None,
            project_dir: None,
            artifact_output_dir: PathBuf::from(DEFAULT_ARTIFACT_OUTPUT_DIR),
            initial_window_size: DEFAULT_WINDOW_SIZE,
            build_executable: true,
            executable_output_dir: None,
            release: false,
            runtime_crate_name: DEFAULT_RUNTIME_CRATE_NAME.to_string(),
            runtime_dependency: NativeUiRuntimeDependencyConfig::WorkspacePath,
            include_spirv: true,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum NativeUiLaunchTarget {
    Executable(PathBuf),
    CargoManifest(PathBuf),
}

impl NativeUiLaunchTarget {
    pub fn path(&self) -> &Path {
        match self {
            Self::Executable(path) | Self::CargoManifest(path) => path.as_path(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct NativeUiGeneratedPaths {
    pub host: NativeUiHostKind,
    pub project_dir: PathBuf,
    pub manifest_path: PathBuf,
    pub main_entry_path: PathBuf,
    pub source_copy_path: PathBuf,
    pub artifact_paths: Vec<PathBuf>,
    pub executable_path: Option<PathBuf>,
    pub launch_target: Option<NativeUiLaunchTarget>,
}

#[derive(Debug, Clone)]
pub struct NativeUiBuildResult {
    pub metadata: NativeAppMetadata,
    pub generated: NativeUiGeneratedPaths,
}

impl NativeUiBuildResult {
    pub fn written_paths(&self) -> Vec<PathBuf> {
        let mut written = vec![
            self.generated.project_dir.clone(),
            self.generated.manifest_path.clone(),
            self.generated.main_entry_path.clone(),
            self.generated.source_copy_path.clone(),
        ];
        written.extend(self.generated.artifact_paths.clone());
        if let Some(executable_path) = &self.generated.executable_path {
            written.push(executable_path.clone());
        }
        written
    }
}

pub fn run_native_ui_build_pipeline(
    input: &Path,
    config: &NativeUiBuildConfig,
) -> Result<NativeUiBuildResult, KainError> {
    let source = fs::read_to_string(input).map_err(|err| {
        KainError::runtime(format!(
            "Failed to read native UI source {}: {}",
            input.display(),
            err
        ))
    })?;
    let base_name = input
        .file_stem()
        .and_then(|stem| stem.to_str())
        .filter(|value| !value.trim().is_empty())
        .unwrap_or("kain")
        .to_string();
    let bundle_config = native_app_bundle_config_from_cli(input, &base_name, config)?;
    let source_file_name = bundle_config
        .source_file_name
        .clone()
        .unwrap_or_else(|| "app.kn".to_string());

    let Some(_) = discover_native_app_root_component(
        &source,
        bundle_config.root_component.as_deref(),
        &source_file_name,
    )?
    else {
        return Err(KainError::runtime(format!(
            "Native UI build requires at least one component in {}",
            input.display()
        )));
    };

    let project_dir = resolve_project_dir(input, &base_name, config)?;
    match config.host {
        NativeUiHostKind::Qt => {
            build_qt_native_ui(&source, &project_dir, &bundle_config, input, config)
        }
        NativeUiHostKind::Tauri => {
            build_tauri_native_ui(&source, &project_dir, &bundle_config, config)
        }
    }
}

fn build_qt_native_ui(
    source: &str,
    project_dir: &Path,
    bundle_config: &NativeAppBundleConfig,
    input: &Path,
    config: &NativeUiBuildConfig,
) -> Result<NativeUiBuildResult, KainError> {
    let bundle = compile_native_app_bundle(source, bundle_config)?;
    let runtime_dependency = resolve_runtime_dependency(
        project_dir,
        &config.runtime_crate_name,
        &config.runtime_dependency,
    )?;
    let executable_output_dir = resolve_executable_output_dir(project_dir, config)?;
    let host_sidecars = resolve_native_ui_host_sidecars(input)?;
    let generated = materialize_native_app_bundle(
        source,
        &bundle,
        &NativeAppMaterializationConfig {
            project_dir: project_dir.to_path_buf(),
            runtime_crate_name: config.runtime_crate_name.clone(),
            runtime_dependency,
            artifact_output_dir: config.artifact_output_dir.clone(),
            build_executable: config.build_executable,
            release: config.release,
            executable_output_dir,
            launcher_entrypoint: NativeAppLauncherEntrypoint::default(),
            host_sidecars,
        },
    )?;

    Ok(NativeUiBuildResult {
        metadata: bundle.metadata,
        generated: NativeUiGeneratedPaths {
            host: NativeUiHostKind::Qt,
            project_dir: generated.project_dir.clone(),
            manifest_path: generated.manifest_path.clone(),
            main_entry_path: generated.main_rs_path.clone(),
            source_copy_path: generated.source_copy_path.clone(),
            artifact_paths: generated.artifact_paths.clone(),
            executable_path: generated.executable_path.clone(),
            launch_target: generated
                .executable_path
                .clone()
                .map(NativeUiLaunchTarget::Executable),
        },
    })
}

#[cfg(feature = "tauri")]
fn build_tauri_native_ui(
    source: &str,
    project_dir: &Path,
    bundle_config: &NativeAppBundleConfig,
    config: &NativeUiBuildConfig,
) -> Result<NativeUiBuildResult, KainError> {
    let bundle = compile_tauri_app_bundle(
        source,
        &TauriAppBundleConfig {
            native_app: bundle_config.clone(),
        },
    )?;
    let generated = materialize_tauri_app_bundle(
        source,
        &bundle,
        &tauri_materialization_config_from_cli(project_dir, config)?,
    )?;

    Ok(NativeUiBuildResult {
        metadata: bundle.metadata,
        generated: NativeUiGeneratedPaths {
            host: NativeUiHostKind::Tauri,
            project_dir: generated.project_dir.clone(),
            manifest_path: generated.src_tauri_manifest_path.clone(),
            main_entry_path: generated.src_tauri_main_rs_path.clone(),
            source_copy_path: generated.source_copy_path.clone(),
            artifact_paths: generated.artifact_paths.clone(),
            executable_path: generated.executable_path.clone(),
            launch_target: Some(NativeUiLaunchTarget::CargoManifest(
                generated.src_tauri_manifest_path.clone(),
            )),
        },
    })
}

#[cfg(not(feature = "tauri"))]
fn build_tauri_native_ui(
    _source: &str,
    _project_dir: &Path,
    _bundle_config: &NativeAppBundleConfig,
    _config: &NativeUiBuildConfig,
) -> Result<NativeUiBuildResult, KainError> {
    Err(KainError::runtime(
        "Tauri native UI support requires the cli tauri feature",
    ))
}

fn native_app_bundle_config_from_cli(
    input: &Path,
    base_name: &str,
    config: &NativeUiBuildConfig,
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
        include_spirv: config.include_spirv,
    })
}

fn resolve_project_dir(
    input: &Path,
    base_name: &str,
    config: &NativeUiBuildConfig,
) -> Result<PathBuf, KainError> {
    let path = match &config.project_dir {
        Some(path) => path.clone(),
        None => input
            .parent()
            .map(|parent| parent.join(default_project_dir_name(base_name, config.host)))
            .unwrap_or_else(|| PathBuf::from(default_project_dir_name(base_name, config.host))),
    };
    absolute_path(&path)
}

fn default_project_dir_name(base_name: &str, host: NativeUiHostKind) -> String {
    match host {
        NativeUiHostKind::Qt => format!("{base_name}-native-ui"),
        NativeUiHostKind::Tauri => format!("{base_name}-tauri-ui"),
    }
}

fn resolve_runtime_dependency(
    project_dir: &Path,
    runtime_crate_name: &str,
    dependency: &NativeUiRuntimeDependencyConfig,
) -> Result<NativeAppRuntimeDependency, KainError> {
    match dependency {
        NativeUiRuntimeDependencyConfig::WorkspacePath => {
            let workspace_root = resolve_workspace_root()?;
            let dependency_root = workspace_root.join("crates").join(runtime_crate_name);
            Ok(NativeAppRuntimeDependency::Path(relative_path_or_absolute(
                &dependency_root,
                project_dir,
            )?))
        }
        NativeUiRuntimeDependencyConfig::Path(path) => Ok(NativeAppRuntimeDependency::Path(
            relative_path_or_absolute(path, project_dir)?,
        )),
        NativeUiRuntimeDependencyConfig::Version(version) => Ok(
            NativeAppRuntimeDependency::Version(version.trim().to_string()),
        ),
    }
}

fn resolve_executable_output_dir(
    project_dir: &Path,
    config: &NativeUiBuildConfig,
) -> Result<Option<PathBuf>, KainError> {
    if !config.build_executable {
        return Ok(None);
    }

    match &config.executable_output_dir {
        Some(path) => Ok(Some(absolute_path(path)?)),
        None => Ok(Some(project_dir.to_path_buf())),
    }
}

fn resolve_native_ui_host_sidecars(
    input: &Path,
) -> Result<Vec<NativeAppHostSidecarBinding>, KainError> {
    let Some(input_directory) = input.parent() else {
        return Ok(Vec::new());
    };

    let preview_image_path =
        absolute_path(&input_directory.join("generic_scene_visual_reference.png"))?;
    if !preview_image_path.exists() {
        return Ok(Vec::new());
    }

    Ok(vec![NativeAppHostSidecarBinding {
        source_path: preview_image_path,
        packaged_file_name: Some("generic_scene_visual_reference.png".to_string()),
        env_var: Some("KAIN_UI_NATIVE_QT_VIEWPORT_IMAGE_PATH".to_string()),
    }])
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

fn absolute_path(path: &Path) -> Result<PathBuf, KainError> {
    if path.is_absolute() {
        return Ok(path.to_path_buf());
    }

    let cwd = std::env::current_dir()
        .map_err(|err| KainError::runtime(format!("Failed to resolve current directory: {err}")))?;
    Ok(cwd.join(path))
}

fn relative_path_or_absolute(path: &Path, base: &Path) -> Result<PathBuf, KainError> {
    let absolute_target = absolute_path(path)?;
    Ok(diff_paths(&absolute_target, base).unwrap_or(absolute_target))
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

#[cfg(feature = "tauri")]
fn tauri_materialization_config_from_cli(
    project_dir: &Path,
    config: &NativeUiBuildConfig,
) -> Result<TauriAppMaterializationConfig, KainError> {
    Ok(TauriAppMaterializationConfig {
        project_dir: project_dir.to_path_buf(),
        artifact_output_dir: config.artifact_output_dir.clone(),
        build_executable: config.build_executable,
        release: config.release,
        bundle_identifier: config.tauri.bundle_identifier.clone(),
        window_label: config.tauri.window_label.clone(),
        cargo_package_name: config.tauri.cargo_package_name.clone(),
        plugin_presets: parse_tauri_plugin_presets(&config.tauri.plugin_presets)?,
        capability_presets: parse_tauri_capability_presets(&config.tauri.capability_presets)?,
        permission_presets: parse_tauri_permission_presets(&config.tauri.permission_presets)?,
        host_type_registry: Default::default(),
    })
}

#[cfg(feature = "tauri")]
fn parse_tauri_plugin_presets(
    values: &[String],
) -> Result<Vec<kain_driver::TauriPluginPreset>, KainError> {
    values
        .iter()
        .map(|value| match normalize_tauri_preset_value(value).as_str() {
            "app" => Ok(kain_driver::TauriPluginPreset::App),
            "window" => Ok(kain_driver::TauriPluginPreset::Window),
            "webview" => Ok(kain_driver::TauriPluginPreset::Webview),
            "event" => Ok(kain_driver::TauriPluginPreset::Event),
            "path" => Ok(kain_driver::TauriPluginPreset::Path),
            "fs" => Ok(kain_driver::TauriPluginPreset::Fs),
            "dialog" => Ok(kain_driver::TauriPluginPreset::Dialog),
            "shell" => Ok(kain_driver::TauriPluginPreset::Shell),
            "process" => Ok(kain_driver::TauriPluginPreset::Process),
            "menu" => Ok(kain_driver::TauriPluginPreset::Menu),
            "tray" => Ok(kain_driver::TauriPluginPreset::Tray),
            "clipboard" | "clipboardmanager" => Ok(kain_driver::TauriPluginPreset::Clipboard),
            "notification" => Ok(kain_driver::TauriPluginPreset::Notification),
            "opener" => Ok(kain_driver::TauriPluginPreset::Opener),
            "store" => Ok(kain_driver::TauriPluginPreset::Store),
            "sql" => Ok(kain_driver::TauriPluginPreset::Sql),
            "http" => Ok(kain_driver::TauriPluginPreset::Http),
            "updater" => Ok(kain_driver::TauriPluginPreset::Updater),
            "globalshortcut" => Ok(kain_driver::TauriPluginPreset::GlobalShortcut),
            other => Err(KainError::runtime(format!(
                "Unsupported Tauri plugin preset '{other}'"
            ))),
        })
        .collect()
}

#[cfg(feature = "tauri")]
fn parse_tauri_capability_presets(
    values: &[String],
) -> Result<Vec<kain_driver::TauriCapabilityPreset>, KainError> {
    values
        .iter()
        .map(|value| match normalize_tauri_preset_value(value).as_str() {
            "mainwindow" => Ok(kain_driver::TauriCapabilityPreset::MainWindow),
            "devwindow" => Ok(kain_driver::TauriCapabilityPreset::DevWindow),
            other => Err(KainError::runtime(format!(
                "Unsupported Tauri capability preset '{other}'"
            ))),
        })
        .collect()
}

#[cfg(feature = "tauri")]
fn parse_tauri_permission_presets(
    values: &[String],
) -> Result<Vec<kain_driver::TauriPermissionPreset>, KainError> {
    values
        .iter()
        .map(|value| match normalize_tauri_preset_value(value).as_str() {
            "coredefault" => Ok(kain_driver::TauriPermissionPreset::CoreDefault),
            "fsdefault" => Ok(kain_driver::TauriPermissionPreset::FsDefault),
            "dialogdefault" => Ok(kain_driver::TauriPermissionPreset::DialogDefault),
            "shellallowopen" => Ok(kain_driver::TauriPermissionPreset::ShellAllowOpen),
            "shellallowspawn" => Ok(kain_driver::TauriPermissionPreset::ShellAllowSpawn),
            "shellallowexecute" => Ok(kain_driver::TauriPermissionPreset::ShellAllowExecute),
            "shellallowkill" => Ok(kain_driver::TauriPermissionPreset::ShellAllowKill),
            "shellallowstdinwrite" => Ok(kain_driver::TauriPermissionPreset::ShellAllowStdinWrite),
            "processdefault" => Ok(kain_driver::TauriPermissionPreset::ProcessDefault),
            "clipboardallowreadtext" => {
                Ok(kain_driver::TauriPermissionPreset::ClipboardAllowReadText)
            }
            "clipboardallowwritetext" => {
                Ok(kain_driver::TauriPermissionPreset::ClipboardAllowWriteText)
            }
            "notificationdefault" => Ok(kain_driver::TauriPermissionPreset::NotificationDefault),
            "openerdefault" => Ok(kain_driver::TauriPermissionPreset::OpenerDefault),
            "storedefault" => Ok(kain_driver::TauriPermissionPreset::StoreDefault),
            "sqldefault" => Ok(kain_driver::TauriPermissionPreset::SqlDefault),
            "sqlallowexecute" => Ok(kain_driver::TauriPermissionPreset::SqlAllowExecute),
            "httpdefault" => Ok(kain_driver::TauriPermissionPreset::HttpDefault),
            "updaterdefault" => Ok(kain_driver::TauriPermissionPreset::UpdaterDefault),
            "globalshortcutallowisregistered" => {
                Ok(kain_driver::TauriPermissionPreset::GlobalShortcutAllowIsRegistered)
            }
            "globalshortcutallowregister" => {
                Ok(kain_driver::TauriPermissionPreset::GlobalShortcutAllowRegister)
            }
            "globalshortcutallowunregister" => {
                Ok(kain_driver::TauriPermissionPreset::GlobalShortcutAllowUnregister)
            }
            "kainbridge" => Ok(kain_driver::TauriPermissionPreset::KainBridge),
            other => Err(KainError::runtime(format!(
                "Unsupported Tauri permission preset '{other}'"
            ))),
        })
        .collect()
}

#[cfg(feature = "tauri")]
fn normalize_tauri_preset_value(value: &str) -> String {
    value
        .trim()
        .to_ascii_lowercase()
        .chars()
        .filter(|ch| *ch != '-' && *ch != '_' && !ch.is_whitespace())
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    #[test]
    fn native_ui_build_materializes_qt_project_without_executable() {
        let temp = TempDir::new().expect("temp dir");
        let input = temp.path().join("app.kn");
        fs::write(
            &input,
            r#"
component App():
    render <panel title="Studio" />
"#,
        )
        .expect("write source");

        let result = run_native_ui_build_pipeline(
            &input,
            &NativeUiBuildConfig {
                project_dir: Some(temp.path().join("dist").join("studio-app")),
                build_executable: false,
                runtime_dependency: NativeUiRuntimeDependencyConfig::Version("0.1.0".to_string()),
                ..Default::default()
            },
        )
        .expect("native ui build should succeed");

        assert_eq!(result.generated.host, NativeUiHostKind::Qt);
        assert_eq!(result.metadata.root_component, "App");
        assert!(result.generated.project_dir.exists());
        assert!(result.generated.manifest_path.exists());
        assert!(result.generated.main_entry_path.exists());
        assert!(result
            .generated
            .artifact_paths
            .iter()
            .any(|path| path.ends_with("native_app_bundle.json")));
        assert!(result.generated.executable_path.is_none());
    }

    #[test]
    fn native_ui_build_materializes_compute_residency_sidecars() {
        let temp = TempDir::new().expect("temp dir");
        let input = temp.path().join("app.kn");
        fs::write(
            &input,
            r#"
component App():
    render <panel title="Compute Residency" />

shader compute SampleCompute(id: UVec3) -> Vec4:
    uniform src: StorageBuffer<Vec4> @0
    uniform dst: StorageBuffer<Vec4> @1
    return vec4(1.0, 1.0, 1.0, 1.0)
"#,
        )
        .expect("write source");

        let result = run_native_ui_build_pipeline(
            &input,
            &NativeUiBuildConfig {
                project_dir: Some(temp.path().join("dist").join("compute-app")),
                build_executable: false,
                runtime_dependency: NativeUiRuntimeDependencyConfig::Version("0.1.0".to_string()),
                ..Default::default()
            },
        )
        .expect("native ui build should succeed");

        assert!(result
            .generated
            .artifact_paths
            .iter()
            .any(|path| path.ends_with("kain_compute_residency.json")));
        assert!(result.generated.artifact_paths.iter().any(|path| {
            path.file_name()
                .and_then(|value| value.to_str())
                .is_some_and(|value| value.starts_with("kain_compute_residency_"))
        }));

        let main_rs = fs::read_to_string(&result.generated.main_entry_path).expect("main.rs");
        assert!(main_rs.contains("KAIN_COMPUTE_RESIDENCY"));
    }

    #[test]
    fn native_ui_build_packages_geometry_fixture_preview_sidecar_when_present() {
        let temp = TempDir::new().expect("temp dir");
        let input = temp.path().join("app.kn");
        let preview_path = temp.path().join("generic_scene_visual_reference.png");
        fs::write(
            &input,
            r#"
component App():
    render <panel title="Atrium" />
"#,
        )
        .expect("write source");
        fs::write(&preview_path, b"preview-bytes").expect("write preview");

        let result = run_native_ui_build_pipeline(
            &input,
            &NativeUiBuildConfig {
                project_dir: Some(temp.path().join("dist").join("atrium-app")),
                build_executable: false,
                runtime_dependency: NativeUiRuntimeDependencyConfig::Version("0.1.0".to_string()),
                ..Default::default()
            },
        )
        .expect("native ui build should succeed");

        let main_rs = fs::read_to_string(&result.generated.main_entry_path).expect("main.rs");
        assert!(main_rs.contains("KAIN_UI_NATIVE_QT_VIEWPORT_IMAGE_PATH"));
        assert!(result.generated.artifact_paths.iter().any(|path| {
            path.file_name()
                .and_then(|value| value.to_str())
                .is_some_and(|value| value == "generic_scene_visual_reference.png")
        }));
    }

    #[cfg(feature = "tauri")]
    #[test]
    fn native_ui_build_materializes_tauri_project_without_binary() {
        let temp = TempDir::new().expect("temp dir");
        let input = temp.path().join("app.kn");
        fs::write(
            &input,
            r#"
component App():
    render <panel title="Studio" />
"#,
        )
        .expect("write source");

        let result = run_native_ui_build_pipeline(
            &input,
            &NativeUiBuildConfig {
                host: NativeUiHostKind::Tauri,
                project_dir: Some(temp.path().join("dist").join("studio-tauri")),
                build_executable: false,
                ..Default::default()
            },
        )
        .expect("tauri native ui build should succeed");

        assert_eq!(result.generated.host, NativeUiHostKind::Tauri);
        assert!(result
            .generated
            .manifest_path
            .ends_with("src-tauri/Cargo.toml"));
        assert!(result
            .generated
            .main_entry_path
            .ends_with("src-tauri/src/main.rs"));
        assert!(matches!(
            result.generated.launch_target,
            Some(NativeUiLaunchTarget::CargoManifest(_))
        ));
        assert!(result
            .generated
            .artifact_paths
            .iter()
            .any(|path| path.ends_with("kain_tauri_bridge_manifest.json")));
    }

    #[test]
    fn explicit_runtime_path_is_rebased_relative_to_project() {
        let temp = TempDir::new().expect("temp dir");
        let project_dir = temp.path().join("build").join("app");
        let runtime_dir = temp.path().join("vendor").join("kain-ui-native");

        let dependency = resolve_runtime_dependency(
            &project_dir,
            DEFAULT_RUNTIME_CRATE_NAME,
            &NativeUiRuntimeDependencyConfig::Path(runtime_dir.clone()),
        )
        .expect("dependency should resolve");

        assert_eq!(
            dependency,
            NativeAppRuntimeDependency::Path(
                PathBuf::from("..")
                    .join("..")
                    .join("vendor")
                    .join("kain-ui-native")
            )
        );
    }
}
