use serde::{Deserialize, Serialize};
use std::path::PathBuf;

#[derive(Debug, Clone, Serialize, Deserialize, Default, PartialEq, Eq)]
pub struct RustFfiConfig {
    #[serde(default)]
    pub manifest_path: Option<PathBuf>,
    #[serde(default)]
    pub path_crates: Vec<RustFfiPathCrate>,
    #[serde(default)]
    pub registry_crates: Vec<RustFfiRegistryCrate>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RustFfiPathCrate {
    pub name: String,
    pub path: PathBuf,
    #[serde(default)]
    pub package: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RustFfiRegistryCrate {
    pub name: String,
    pub version: String,
    #[serde(default)]
    pub package: Option<String>,
    #[serde(default)]
    pub features: Vec<String>,
    #[serde(default = "default_true")]
    pub default_features: bool,
}

fn default_true() -> bool {
    true
}
