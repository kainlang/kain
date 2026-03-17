use serde::Deserialize;
use std::collections::BTreeMap;
use std::path::PathBuf;

#[derive(Debug, Clone, Default, Deserialize)]
pub struct CFfiConfig {
    #[serde(default)]
    pub include_paths: Vec<PathBuf>,
    #[serde(default)]
    pub defines: Vec<String>,
    #[serde(default)]
    pub cpp_options: Vec<String>,
    #[serde(default)]
    pub cpp_command: Option<String>,
    #[serde(default)]
    pub libraries: Vec<CLibraryConfig>,
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
    pub cpp_options: Vec<String>,
    #[serde(default)]
    pub cpp_command: Option<String>,
}
