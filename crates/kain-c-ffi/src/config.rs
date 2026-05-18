use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
use std::path::PathBuf;

#[derive(Debug, Clone, Default, Deserialize)]
pub struct CFfiConfig {
    #[serde(default)]
    pub include_paths: Vec<PathBuf>,
    #[serde(default)]
    pub defines: Vec<String>,
    #[serde(default)]
    pub link_libs: Vec<String>,
    #[serde(default)]
    pub cpp_options: Vec<String>,
    #[serde(default)]
    pub cpp_command: Option<String>,
    #[serde(default)]
    pub tier: CInteropTier,
    #[serde(default)]
    pub libraries: Vec<CLibraryConfig>,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum CInteropTier {
    #[default]
    Dynamic,
    Static,
    Bitcode,
    Inline,
    Fused,
}

#[derive(Debug, Clone, Default, Deserialize)]
pub struct CLibraryConfig {
    pub name: String,
    pub header: PathBuf,
    #[serde(default)]
    pub shared_lib: Option<PathBuf>,
    #[serde(default)]
    pub symbols: BTreeMap<String, String>,
    #[serde(default)]
    pub include_paths: Vec<PathBuf>,
    #[serde(default)]
    pub defines: Vec<String>,
    #[serde(default)]
    pub link_libs: Vec<String>,
    #[serde(default)]
    pub cpp_options: Vec<String>,
    #[serde(default)]
    pub cpp_command: Option<String>,
    #[serde(default)]
    pub tier: Option<CInteropTier>,
    #[serde(default)]
    pub runtime_owned: bool,
}
