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
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct Ue5Config {
    pub plugin_name: String,
    #[serde(default = "default_plugin_dir")]
    pub plugin_dir: PathBuf,
    #[serde(default)]
    pub sources: Vec<PathBuf>,  // Multiple .kn files - GODMODE ENABLED
    #[serde(default)]
    pub shaders: Vec<String>,
    #[serde(default)]
    pub copyright: Option<String>,
    #[serde(default)]
    pub modular_output: bool,  // Generate separate .h/.cpp per source file
    #[serde(default)]
    pub stdlib_path: Option<PathBuf>,  // Optional custom stdlib path
    /// Target UE5 engine version, e.g. "5.2", "5.3", "5.4".
    /// Drives the binary format for .uasset / AssetRegistry.bin output.
    /// Defaults to "5.2" if not specified.
    #[serde(default)]
    pub engine_version: Option<String>,
}

fn default_plugin_dir() -> PathBuf { PathBuf::from("Plugins") }

#[derive(Debug, Serialize, Deserialize)]
pub struct BuildConfig {
    #[serde(default = "default_entry")]
    pub entry: PathBuf,
    #[serde(default = "default_output")]
    pub output: PathBuf,
    #[serde(default)]
    pub targets: Vec<String>,
}

fn default_entry() -> PathBuf { PathBuf::from("src/main.kn") }
fn default_output() -> PathBuf { PathBuf::from("dist") }

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
        }
    }
}

pub(crate) fn registry_url() -> &'static str {
    REGISTRY_URL
}
