use std::fs;
use std::path::{Component, Path, PathBuf};

use kain_core::error::KainError;
use kain_driver::{
    compile_native_app_bundle, discover_native_app_root_component, materialize_native_app_bundle,
    NativeAppBundle, NativeAppBundleConfig, NativeAppMaterializationConfig,
    NativeAppMaterializedPaths, NativeAppMetadata, NativeAppRuntimeDependency,
};

const DEFAULT_RUNTIME_CRATE_NAME: &str = "kain-ui-native";
const DEFAULT_ARTIFACT_OUTPUT_DIR: &str = "generated";
const DEFAULT_WINDOW_SIZE: [f32; 2] = [1440.0, 920.0];

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

#[derive(Debug, Clone)]
pub struct NativeUiBuildResult {
    pub metadata: NativeAppMetadata,
    pub generated: NativeAppMaterializedPaths,
}

impl NativeUiBuildResult {
    pub fn written_paths(&self) -> Vec<PathBuf> {
        let mut written = vec![
            self.generated.project_dir.clone(),
            self.generated.manifest_path.clone(),
            self.generated.main_rs_path.clone(),
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
    let bundle_config = native_app_bundle_config_from_cli(input, &base_name, config);
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

    let bundle = compile_native_app_bundle(&source, &bundle_config)?;
    let project_dir = resolve_project_dir(input, &bundle, config)?;
    let runtime_dependency = resolve_runtime_dependency(
        &project_dir,
        &config.runtime_crate_name,
        &config.runtime_dependency,
    )?;
    let executable_output_dir = resolve_executable_output_dir(&project_dir, config)?;

    let generated = materialize_native_app_bundle(
        &source,
        &bundle,
        &NativeAppMaterializationConfig {
            project_dir,
            runtime_crate_name: config.runtime_crate_name.clone(),
            runtime_dependency,
            artifact_output_dir: config.artifact_output_dir.clone(),
            build_executable: config.build_executable,
            release: config.release,
            executable_output_dir,
        },
    )?;

    Ok(NativeUiBuildResult {
        metadata: bundle.metadata,
        generated,
    })
}

fn native_app_bundle_config_from_cli(
    input: &Path,
    base_name: &str,
    config: &NativeUiBuildConfig,
) -> NativeAppBundleConfig {
    NativeAppBundleConfig {
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
        initial_window_size: config.initial_window_size,
        include_spirv: config.include_spirv,
    }
}

fn resolve_project_dir(
    input: &Path,
    bundle: &NativeAppBundle,
    config: &NativeUiBuildConfig,
) -> Result<PathBuf, KainError> {
    let path = match &config.project_dir {
        Some(path) => path.clone(),
        None => input
            .parent()
            .map(|parent| parent.join(format!("{}-native-ui", bundle.metadata.app_name)))
            .unwrap_or_else(|| PathBuf::from(format!("{}-native-ui", bundle.metadata.app_name))),
    };
    absolute_path(&path)
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

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    #[test]
    fn native_ui_build_materializes_project_without_executable() {
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

        assert_eq!(result.metadata.root_component, "App");
        assert!(result.generated.project_dir.exists());
        assert!(result.generated.manifest_path.exists());
        assert!(result.generated.main_rs_path.exists());
        assert!(result
            .generated
            .artifact_paths
            .iter()
            .any(|path| path.ends_with("native_app_bundle.json")));
        assert!(result.generated.executable_path.is_none());
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
