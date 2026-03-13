use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::path::PathBuf;

const REGISTRY_URL: &str = "https://greeble.co/KAIN/index.json";

#[derive(Debug, Serialize, Deserialize)]
pub struct PackageManifest {
    pub package: PackageInfo,
    #[serde(default)]
    pub build: BuildConfig,
    #[serde(default)]
    pub dependencies: HashMap<String, String>,
    #[serde(default)]
    pub ue5: Option<Ue5Config>,
    #[serde(default)]
    pub r#rust: Option<RustBuildConfig>,
}
#[derive(Debug, Serialize, Deserialize, Clone, Copy, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum RustBuildArtifact {
    Source,
    ShaderHost,
    ShaderReflection,
    Spirv,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct RustNativeUiAppConfig {
    #[serde(default)]
    pub root_component: Option<String>,
    #[serde(default)]
    pub window_title: Option<String>,
    #[serde(default)]
    pub app_name: Option<String>,
    #[serde(default)]
    pub output: Option<PathBuf>,
    #[serde(default = "default_native_ui_window_size")]
    pub initial_window_size: [f32; 2],
    #[serde(default = "default_true")]
    pub build_executable: bool,
    #[serde(default)]
    pub release: bool,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct RustBuildConfig {
    #[serde(default)]
    pub output: Option<PathBuf>,
    #[serde(default = "default_rust_build_artifacts")]
    pub artifacts: Vec<RustBuildArtifact>,
    #[serde(default)]
    pub native_ui: Option<RustNativeUiAppConfig>,
}

fn default_rust_build_artifacts() -> Vec<RustBuildArtifact> {
    vec![
        RustBuildArtifact::Source,
        RustBuildArtifact::ShaderHost,
        RustBuildArtifact::ShaderReflection,
        RustBuildArtifact::Spirv,
    ]
}

fn default_native_ui_window_size() -> [f32; 2] {
    [1440.0, 920.0]
}

fn default_true() -> bool {
    true
}

impl Default for RustBuildConfig {
    fn default() -> Self {
        Self {
            output: None,
            artifacts: default_rust_build_artifacts(),
            native_ui: None,
        }
    }
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct Ue5Config {
    pub plugin_name: String,
    #[serde(default = "default_plugin_dir")]
    pub plugin_dir: PathBuf,
    #[serde(default)]
    pub sources: Vec<PathBuf>, // Multiple .kn files - GODMODE ENABLED
    #[serde(default)]
    pub shaders: Vec<String>,
    #[serde(default)]
    pub copyright: Option<String>,
    #[serde(default)]
    pub modular_output: bool, // Generate separate .h/.cpp per source file
    #[serde(default)]
    pub stdlib_path: Option<PathBuf>, // Optional custom stdlib path
    /// Target UE5 engine version, e.g. `"5.4"`, `"5.6"`.
    /// Drives the binary format for .uasset / AssetRegistry.bin output.
    /// Supported range: `"5.0"` – `"5.7"`. Defaults to `"5.4"` if not specified.
    #[serde(default)]
    pub engine_version: Option<String>,
    /// Optional data-driven module topology.
    ///
    /// If empty, packager falls back to legacy single/split-module behavior.
    #[serde(default)]
    pub modules: Vec<Ue5ModuleConfig>,
    /// Optional plugin-level dependencies emitted into .uplugin `Plugins`.
    ///
    /// Use this when a module depends on engine/plugin modules coming from
    /// external plugins (e.g. ChaosClothAssetEngine -> ChaosClothAsset).
    #[serde(default)]
    pub plugin_dependencies: Vec<String>,
}

#[derive(Debug, Serialize, Deserialize, Clone, PartialEq, Eq)]
pub enum Ue5ModuleType {
    Runtime,
    Editor,
    UncookedOnly,
    Developer,
    Program,
}

impl Ue5ModuleType {
    pub fn as_uplugin_type(&self) -> &'static str {
        match self {
            Self::Runtime => "Runtime",
            Self::Editor => "Editor",
            Self::UncookedOnly => "UncookedOnly",
            Self::Developer => "Developer",
            Self::Program => "Program",
        }
    }

    pub fn is_editorish(&self) -> bool {
        matches!(self, Self::Editor | Self::UncookedOnly | Self::Developer)
    }
}

fn default_runtime_module_type() -> Ue5ModuleType {
    Ue5ModuleType::Runtime
}
fn default_loading_phase() -> String {
    "PostConfigInit".to_string()
}

#[derive(Debug, Serialize, Deserialize, Clone, Default)]
pub struct Ue5ModuleOutputConfig {
    /// Optional explicit module public output root.
    /// Relative paths are resolved from plugin root.
    #[serde(default)]
    pub public: Option<PathBuf>,
    /// Optional explicit module private output root.
    /// Relative paths are resolved from plugin root.
    #[serde(default)]
    pub private: Option<PathBuf>,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct Ue5ModuleConfig {
    pub name: String,
    #[serde(rename = "type")]
    #[serde(default = "default_runtime_module_type")]
    pub module_type: Ue5ModuleType,
    #[serde(default = "default_loading_phase")]
    pub loading_phase: String,
    /// Optional source partitions for this module.
    #[serde(default)]
    pub source_globs: Vec<PathBuf>,
    #[serde(default)]
    pub public_deps: Vec<String>,
    #[serde(default)]
    pub private_deps: Vec<String>,
    #[serde(default)]
    pub depends_on: Vec<String>,
    #[serde(default)]
    pub output: Ue5ModuleOutputConfig,
    /// Optional style policy: logical bucket -> relative folder.
    /// Example: "actors" = "Runtime/Actors"
    #[serde(default)]
    pub folders: HashMap<String, PathBuf>,
}

impl Ue5Config {
    pub fn has_module_plan(&self) -> bool {
        !self.modules.is_empty()
    }

    pub fn validate_module_configs(&self) -> Result<(), String> {
        if self.modules.is_empty() {
            return Ok(());
        }

        let mut names = std::collections::HashSet::new();
        for module in &self.modules {
            if module.name.trim().is_empty() {
                return Err("[ue5.modules] has an entry with empty name".to_string());
            }
            if !names.insert(module.name.clone()) {
                return Err(format!("Duplicate ue5 module name: {}", module.name));
            }
        }

        // Validate dependency references
        for module in &self.modules {
            for dep in &module.depends_on {
                if !names.contains(dep) {
                    return Err(format!(
                        "Module '{}' depends_on unknown module '{}'",
                        module.name, dep
                    ));
                }
                if dep == &module.name {
                    return Err(format!("Module '{}' cannot depend on itself", module.name));
                }
            }
        }

        // Detect cycles with DFS
        let by_name: std::collections::HashMap<_, _> =
            self.modules.iter().map(|m| (m.name.clone(), m)).collect();
        let mut visiting = std::collections::HashSet::new();
        let mut visited = std::collections::HashSet::new();

        fn dfs(
            node: &str,
            by_name: &std::collections::HashMap<String, &Ue5ModuleConfig>,
            visiting: &mut std::collections::HashSet<String>,
            visited: &mut std::collections::HashSet<String>,
        ) -> Result<(), String> {
            if visited.contains(node) {
                return Ok(());
            }
            if !visiting.insert(node.to_string()) {
                return Err(format!("Cycle detected in ue5.modules at '{}'", node));
            }
            if let Some(module) = by_name.get(node) {
                for dep in &module.depends_on {
                    dfs(dep, by_name, visiting, visited)?;
                }
            }
            visiting.remove(node);
            visited.insert(node.to_string());
            Ok(())
        }

        for module in &self.modules {
            dfs(&module.name, &by_name, &mut visiting, &mut visited)?;
        }

        Ok(())
    }
}

fn default_plugin_dir() -> PathBuf {
    PathBuf::from("Plugins")
}

#[derive(Debug, Serialize, Deserialize)]
pub struct BuildConfig {
    #[serde(default = "default_entry")]
    pub entry: PathBuf,
    #[serde(default = "default_output")]
    pub output: PathBuf,
    #[serde(default)]
    pub targets: Vec<String>,
}

fn default_entry() -> PathBuf {
    PathBuf::from("src/main.kn")
}
fn default_output() -> PathBuf {
    PathBuf::from("dist")
}

impl Default for BuildConfig {
    fn default() -> Self {
        Self {
            entry: default_entry(),
            output: default_output(),
            targets: vec!["wasm".to_string()],
        }
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct PackageInfo {
    pub name: String,
    pub version: String,
    #[serde(default)]
    pub authors: Vec<String>,
    #[serde(default)]
    pub description: Option<String>,
}

// Registry Structures
#[derive(Debug, Deserialize)]
pub(crate) struct RegistryIndex {
    pub packages: HashMap<String, String>, // name -> meta.json path
}

#[derive(Debug, Deserialize)]
pub(crate) struct PackageMeta {
    pub versions: HashMap<String, PackageVersion>,
}

#[derive(Debug, Deserialize)]
pub(crate) struct PackageVersion {
    pub url: String,
    pub checksum: String,
}

impl PackageManifest {
    pub fn default(name: &str) -> Self {
        Self {
            package: PackageInfo {
                name: name.to_string(),
                version: "0.1.0".to_string(),
                authors: vec![],
                description: None,
            },
            build: BuildConfig::default(),
            dependencies: HashMap::new(),
            ue5: None,
            r#rust: None,
        }
    }
}

pub(crate) fn registry_url() -> &'static str {
    REGISTRY_URL
}
