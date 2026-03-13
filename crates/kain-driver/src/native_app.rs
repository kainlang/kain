use std::ffi::OsStr;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

use crate::{DriverSession, RustBundleOutput};
use kain_core::ast::Item;
use kain_core::diagnostics::SpanMapper;
use kain_core::error::KainError;
use kain_core::{build_ui_output_from_source, Lexer, Parser};
use kain_ui::UiBuildOutput;

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
    pub ui: UiBuildOutput,
    pub rust: RustBundleOutput,
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
        let ui = build_ui_output_from_source(source, &root_component)?;
        let rust = self.compile_rust_artifact_bundle(source, config.include_spirv)?;

        Ok(NativeAppBundle {
            metadata: NativeAppMetadata {
                app_name,
                window_title,
                root_component,
                source_file_name,
                initial_window_size: config.initial_window_size,
            },
            ui,
            rust,
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

        let manifest_path = project_dir.join("Cargo.toml");
        let manifest = render_manifest(
            &bundle.metadata.app_name,
            &config.runtime_crate_name,
            &config.runtime_dependency,
        );
        fs::write(&manifest_path, manifest.as_bytes())
            .map_err(io_error("write native app Cargo manifest"))?;

        let main_rs_path = project_dir.join("src").join("main.rs");
        let main_rs = render_main_rs(
            &bundle.metadata.source_file_name,
            &bundle.metadata.window_title,
            &bundle.metadata.root_component,
            bundle.metadata.initial_window_size,
            &config.runtime_crate_name,
        );
        fs::write(&main_rs_path, main_rs.as_bytes())
            .map_err(io_error("write native app entrypoint"))?;

        let artifact_root = if config.artifact_output_dir.is_absolute() {
            config.artifact_output_dir.clone()
        } else {
            project_dir.join(&config.artifact_output_dir)
        };
        fs::create_dir_all(&artifact_root)
            .map_err(io_error("create native app artifact directory"))?;

        let mut artifact_paths = Vec::new();
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
    source_file_name: &str,
    window_title: &str,
    root_component: &str,
    initial_window_size: [f32; 2],
    runtime_crate_name: &str,
) -> String {
    let source_file_name = rust_string_literal(&format!("../{source_file_name}"));
    let window_title = rust_string_literal(window_title);
    let root_component = rust_string_literal(root_component);
    let runtime_module_name = runtime_crate_name.replace('-', "_");

    format!(
        "use {runtime_module_name}::{{run_app, KainUiNativeAppConfig}};\n\nconst KAIN_SOURCE: &str = include_str!({source_file_name});\n\nfn main() -> Result<(), Box<dyn std::error::Error>> {{\n    run_app(KainUiNativeAppConfig {{\n        window_title: {window_title}.to_string(),\n        root_component: {root_component}.to_string(),\n        source: KAIN_SOURCE.to_string(),\n        initial_window_size: [{:?}, {:?}],\n    }})\n}}\n",
        initial_window_size[0], initial_window_size[1]
    )
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
        assert!(main_rs.contains("root_component: \"App\""));
    }
}
