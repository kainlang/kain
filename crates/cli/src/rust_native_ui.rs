use std::ffi::OsStr;
use std::fs;
use std::path::{Component, Path, PathBuf};
use std::process::Command;

use crate::packager::config::RustNativeUiAppConfig;
use kain_core::ast::Item;
use kain_core::diagnostics::SpanMapper;
use kain_core::{KainError, Lexer, Parser};

#[derive(Debug, Clone)]
pub(crate) struct NativeUiAppPaths {
    pub project_dir: PathBuf,
    pub manifest_path: PathBuf,
    pub main_rs_path: PathBuf,
    pub source_copy_path: PathBuf,
    pub executable_path: Option<PathBuf>,
}

#[derive(Debug, Clone)]
pub(crate) struct GeneratedNativeUiApp {
    pub paths: NativeUiAppPaths,
}

pub(crate) fn default_file_build_native_ui_config(base_name: &str) -> RustNativeUiAppConfig {
    RustNativeUiAppConfig {
        root_component: None,
        window_title: Some(base_name.to_string()),
        app_name: Some(base_name.to_string()),
        output: None,
        initial_window_size: [1440.0, 920.0],
        build_executable: true,
        release: false,
    }
}

pub(crate) fn generate_native_ui_app(
    source: &str,
    input: &Path,
    output_root: &Path,
    base_name: &str,
    config: &RustNativeUiAppConfig,
) -> Result<Option<GeneratedNativeUiApp>, KainError> {
    let Some(root_component) =
        resolve_root_component(source, config.root_component.as_deref(), input)?
    else {
        return Ok(None);
    };

    let app_name = sanitize_cargo_name(
        config
            .app_name
            .as_deref()
            .filter(|name| !name.trim().is_empty())
            .unwrap_or(base_name),
    );
    let window_title = config
        .window_title
        .clone()
        .filter(|title| !title.trim().is_empty())
        .unwrap_or_else(|| root_component.clone());
    let project_dir = resolve_project_dir(output_root, &app_name, config.output.as_ref());

    fs::create_dir_all(project_dir.join("src"))
        .map_err(io_error("create native UI app source directory"))?;

    let source_file_name = input
        .file_name()
        .and_then(OsStr::to_str)
        .filter(|name| !name.trim().is_empty())
        .unwrap_or("app.kn");
    let source_copy_path = project_dir.join(source_file_name);
    fs::write(&source_copy_path, source.as_bytes())
        .map_err(io_error("write embedded Kain UI source"))?;

    let manifest_path = project_dir.join("Cargo.toml");
    let main_rs_path = project_dir.join("src").join("main.rs");
    let workspace_root = resolve_workspace_root()?;
    let dependency_root = workspace_root.join("crates").join("kain-ui-native");
    let dependency_path =
        diff_paths(&dependency_root, &project_dir).unwrap_or_else(|| dependency_root.clone());

    let manifest = render_manifest(&app_name, &dependency_path);
    fs::write(&manifest_path, manifest.as_bytes())
        .map_err(io_error("write native UI Cargo manifest"))?;

    let main_rs = render_main_rs(
        source_file_name,
        &window_title,
        &root_component,
        config.initial_window_size,
    );
    fs::write(&main_rs_path, main_rs.as_bytes()).map_err(io_error("write native UI main.rs"))?;

    let executable_path = if config.build_executable {
        Some(build_native_ui_executable(
            &project_dir,
            output_root,
            &app_name,
            config.release,
        )?)
    } else {
        None
    };

    Ok(Some(GeneratedNativeUiApp {
        paths: NativeUiAppPaths {
            project_dir,
            manifest_path,
            main_rs_path,
            source_copy_path,
            executable_path,
        },
    }))
}

fn resolve_root_component(
    source: &str,
    configured_root: Option<&str>,
    input: &Path,
) -> Result<Option<String>, KainError> {
    let tokens = Lexer::new(source).tokenize()?;
    let span_mapper = SpanMapper::new(source);
    let filename = input.to_str().unwrap_or("<ui>");
    let program = Parser::new(&tokens, &span_mapper, filename).parse()?;
    let component_names: Vec<_> = program
        .items
        .iter()
        .filter_map(|item| match item {
            Item::Component(component) => Some(component.name.clone()),
            _ => None,
        })
        .collect();

    if let Some(root) = configured_root.filter(|root| !root.trim().is_empty()) {
        if component_names.iter().any(|name| name == root) {
            return Ok(Some(root.to_string()));
        }
        return Err(KainError::runtime(format!(
            "Configured native UI root component '{}' was not found in {}",
            root,
            input.display()
        )));
    }

    if component_names.is_empty() {
        return Ok(None);
    }

    if let Some(app) = program.items.iter().find_map(|item| match item {
        Item::Component(component) if component.name == "App" => Some(component.name.clone()),
        _ => None,
    }) {
        return Ok(Some(app));
    }

    Ok(component_names.into_iter().next())
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

fn resolve_workspace_root() -> Result<PathBuf, KainError> {
    let cli_manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let Some(workspace_root) = cli_manifest_dir.parent().and_then(Path::parent) else {
        return Err(KainError::runtime(
            "Failed to derive the Kain workspace root from the CLI crate path",
        ));
    };

    Ok(workspace_root.to_path_buf())
}

fn render_manifest(app_name: &str, dependency_path: &Path) -> String {
    let dependency_path = path_for_toml(dependency_path);
    format!(
        "[package]\nname = \"{app_name}\"\nversion = \"0.1.0\"\nedition = \"2021\"\n\n[workspace]\n\n[dependencies]\nkain-ui-native = {{ path = \"{dependency_path}\" }}\n"
    )
}

fn render_main_rs(
    source_file_name: &str,
    window_title: &str,
    root_component: &str,
    initial_window_size: [f32; 2],
) -> String {
    let source_file_name = rust_string_literal(&format!("../{source_file_name}"));
    let window_title = rust_string_literal(window_title);
    let root_component = rust_string_literal(root_component);

    format!(
        "use kain_ui_native::{{run_app, KainUiNativeAppConfig}};\n\nconst KAIN_SOURCE: &str = include_str!({source_file_name});\n\nfn main() -> Result<(), Box<dyn std::error::Error>> {{\n    run_app(KainUiNativeAppConfig {{\n        window_title: {window_title}.to_string(),\n        root_component: {root_component}.to_string(),\n        source: KAIN_SOURCE.to_string(),\n        initial_window_size: [{:?}, {:?}],\n    }})\n}}\n",
        initial_window_size[0], initial_window_size[1]
    )
}

fn build_native_ui_executable(
    project_dir: &Path,
    output_root: &Path,
    app_name: &str,
    release: bool,
) -> Result<PathBuf, KainError> {
    let mut command = Command::new("cargo");
    command.arg("build");
    if release {
        command.arg("--release");
    }
    command.current_dir(project_dir);

    let output = command.output().map_err(|err| {
        KainError::runtime(format!(
            "Failed to invoke cargo to build native UI app at {}: {}",
            project_dir.display(),
            err
        ))
    })?;

    if !output.status.success() {
        return Err(KainError::runtime(format!(
            "Native UI cargo build failed for {}:\n{}\n{}",
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

    fs::create_dir_all(output_root).map_err(io_error("create native UI output directory"))?;
    let copied_executable = output_root.join(binary_file_name(app_name));
    fs::copy(&built_executable, &copied_executable)
        .map_err(io_error("copy native UI executable"))?;

    Ok(copied_executable)
}

fn binary_file_name(app_name: &str) -> String {
    if cfg!(windows) {
        format!("{app_name}.exe")
    } else {
        app_name.to_string()
    }
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

fn io_error(context: &'static str) -> impl Fn(std::io::Error) -> KainError {
    move |err| KainError::runtime(format!("Failed to {context}: {err}"))
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    #[test]
    fn resolve_root_component_prefers_app_when_present() {
        let source = r#"
component Shell():
    render <panel />

component App():
    render <panel />
"#;
        let root = resolve_root_component(source, None, Path::new("app.kn"))
            .expect("component parse should succeed");
        assert_eq!(root.as_deref(), Some("App"));
    }

    #[test]
    fn resolve_root_component_returns_none_without_components() {
        let source = r#"
fn main():
    println("hello")
"#;
        let root = resolve_root_component(source, None, Path::new("main.kn"))
            .expect("parse should succeed");
        assert!(root.is_none());
    }

    #[test]
    fn generate_native_ui_app_writes_expected_scaffold() {
        let temp = TempDir::new().unwrap();
        let output_root = temp.path().join("dist");
        let source_path = temp.path().join("app.kn");
        let source = r#"
component App():
    render <panel title="Test" />
"#;
        fs::write(&source_path, source).unwrap();

        let generated = generate_native_ui_app(
            source,
            &source_path,
            &output_root,
            "app",
            &RustNativeUiAppConfig {
                root_component: None,
                window_title: Some("Test Window".to_string()),
                app_name: Some("test-app".to_string()),
                output: Some(PathBuf::from("ui-shell")),
                initial_window_size: [1280.0, 720.0],
                build_executable: false,
                release: false,
            },
        )
        .expect("native UI scaffold generation should succeed")
        .expect("component app should be generated");

        assert!(generated.paths.project_dir.ends_with("ui-shell"));
        assert!(generated.paths.manifest_path.exists());
        assert!(generated.paths.main_rs_path.exists());
        assert!(generated.paths.source_copy_path.exists());

        let main_rs = fs::read_to_string(&generated.paths.main_rs_path).unwrap();
        assert!(main_rs.contains("KainUiNativeAppConfig"));
        assert!(main_rs.contains("root_component: \"App\""));
    }

    #[test]
    fn sanitize_cargo_name_handles_symbols_and_digits() {
        assert_eq!(sanitize_cargo_name("My Awesome/UI"), "my-awesome-ui");
        assert_eq!(sanitize_cargo_name("99"), "kain-ui-99");
    }
}
