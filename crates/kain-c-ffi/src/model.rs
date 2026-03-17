use crate::config::{CFfiConfig, CLibraryConfig};
use serde::{Deserialize, Serialize};
use std::path::PathBuf;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ArtifactMode {
    Live,
    Generate,
    Both,
}

impl ArtifactMode {
    pub fn wants_live(self) -> bool {
        matches!(self, Self::Live | Self::Both)
    }

    pub fn wants_generate(self) -> bool {
        matches!(self, Self::Generate | Self::Both)
    }
}

impl Default for ArtifactMode {
    fn default() -> Self {
        Self::Both
    }
}

#[derive(Debug, Clone, Default)]
pub struct PrepareContext {
    pub current_dir: Option<PathBuf>,
    pub manifest_path: Option<PathBuf>,
}

#[derive(Debug, Clone, Default)]
pub struct ImportCOptions {
    pub output_dir: Option<PathBuf>,
    pub report_json: Option<PathBuf>,
    pub mode: ArtifactMode,
}

#[derive(Debug, Clone)]
pub struct ManifestContext {
    pub root_dir: Option<PathBuf>,
    pub config: Option<CFfiConfig>,
}

#[derive(Debug, Clone)]
pub struct ResolvedCLibrary {
    pub import_name: String,
    pub manifest_root: PathBuf,
    pub header_path: PathBuf,
    pub shared_lib_path: Option<PathBuf>,
    pub config: CLibraryConfig,
    pub global_config: CFfiConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FileFingerprint {
    pub path: String,
    pub sha256: String,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ItemStatus {
    Callable,
    TypeOnly,
    Stubbed,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ItemKind {
    Function,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BindingReportEntry {
    pub symbol_path: String,
    pub kind: ItemKind,
    pub status: ItemStatus,
    pub reason: Option<String>,
    pub emitted_symbol: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BindingReport {
    pub library_name: String,
    pub header_path: String,
    pub shared_lib_path: Option<String>,
    pub cache_dir: String,
    pub report_json_path: String,
    pub report_text_path: String,
    pub entries: Vec<BindingReportEntry>,
    pub source_fingerprints: Vec<FileFingerprint>,
}

#[derive(Debug, Clone)]
pub struct BridgeParam {
    pub name: String,
    pub ty: BridgeType,
}

#[derive(Debug, Clone)]
pub struct CFunctionBinding {
    pub emitted_name: String,
    pub exported_aliases: Vec<String>,
    pub symbol_name: String,
    pub params: Vec<BridgeParam>,
    pub return_type: BridgeType,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum BridgeType {
    Unit,
    Bool,
    SignedInt(String),
    UnsignedInt(String),
    Float32,
    Float64,
    CString,
}

impl BridgeType {
    pub fn render_kain(&self) -> &'static str {
        match self {
            Self::Unit => "Unit",
            Self::Bool => "Bool",
            Self::SignedInt(_) | Self::UnsignedInt(_) => "Int",
            Self::Float32 | Self::Float64 => "Float",
            Self::CString => "String",
        }
    }

    pub fn default_literal(&self) -> &'static str {
        match self {
            Self::Unit => "()",
            Self::Bool => "false",
            Self::SignedInt(_) | Self::UnsignedInt(_) => "0",
            Self::Float32 | Self::Float64 => "0.0",
            Self::CString => "\"\"",
        }
    }

    pub fn render_rust_ffi(&self) -> &str {
        match self {
            Self::Unit => "()",
            Self::Bool => "bool",
            Self::SignedInt(name) | Self::UnsignedInt(name) => name.as_str(),
            Self::Float32 => "f32",
            Self::Float64 => "f64",
            Self::CString => "*const std::os::raw::c_char",
        }
    }
}

#[derive(Debug, Clone)]
pub struct BindingBundle {
    pub functions: Vec<CFunctionBinding>,
    pub report_entries: Vec<BindingReportEntry>,
    pub source_fingerprints: Vec<FileFingerprint>,
}

#[derive(Debug, Clone)]
pub struct GeneratedArtifacts {
    pub canonical_module_source: String,
    pub prelude_source: String,
    pub bridge_source: String,
    pub report: BindingReport,
    pub report_text: String,
}

#[derive(Debug, Clone)]
pub struct ImportCOutput {
    pub resolved: ResolvedCLibrary,
    pub config_root: Option<PathBuf>,
    pub c_ffi_config: Option<CFfiConfig>,
    pub cache_dir: PathBuf,
    pub canonical_module_path: PathBuf,
    pub prelude_path: PathBuf,
    pub report_json_path: PathBuf,
    pub report_text_path: PathBuf,
    pub bridge_manifest_path: PathBuf,
    pub bridge_source_path: PathBuf,
    pub dylib_path: Option<PathBuf>,
    pub canonical_module_source: String,
    pub prelude_source: String,
    pub cache_hit: bool,
}
