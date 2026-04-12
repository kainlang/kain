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
    OpaqueHandle,
    Stubbed,
    Unsupported,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ItemKind {
    Function,
    Struct,
    Enum,
    Typedef,
    Callback,
    Global,
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
    pub parser_backend: String,
    pub header_path: String,
    pub shared_lib_path: Option<String>,
    pub cache_dir: String,
    pub report_json_path: String,
    pub report_text_path: String,
    pub manifest_json_path: String,
    pub supported_targets: Vec<String>,
    pub capabilities: Vec<String>,
    pub entries: Vec<BindingReportEntry>,
    pub source_fingerprints: Vec<FileFingerprint>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PackagedBridgeBinaryArtifact {
    pub file_name: String,
    pub source_path: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HostBridgeModuleDescriptor {
    pub module_id: String,
    pub module_name: String,
    pub provider: String,
    pub lane: String,
    pub abi_version: u32,
    pub required_capability_mask: u32,
    pub required_runtime_services: Vec<String>,
    pub hot_reload_capable: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HostBridgeServiceDescriptor {
    pub service_key: String,
    pub service_name: String,
    pub provider: String,
    pub abi_version: u32,
    pub capability_mask: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PackagedBridgeSymbolDescriptor {
    pub symbol_name: String,
    pub exported_aliases: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PackagedBridgeImport {
    pub import_name: String,
    pub module: HostBridgeModuleDescriptor,
    pub services: Vec<HostBridgeServiceDescriptor>,
    pub bridge_library: PackagedBridgeBinaryArtifact,
    pub shared_library: Option<PackagedBridgeBinaryArtifact>,
    pub binding_manifest_file_name: String,
    pub binding_report_file_name: String,
    pub symbols: Vec<PackagedBridgeSymbolDescriptor>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PackagedBridgeManifest {
    pub schema_version: String,
    pub lane: String,
    pub imports: Vec<PackagedBridgeImport>,
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
    ByteBuffer { mutable: bool, element_type: String },
    OpaqueHandle { mutable: bool, pointee: String },
}

impl BridgeType {
    pub fn render_kain(&self) -> String {
        match self {
            Self::Unit => "Void".to_string(),
            Self::Bool => "Bool".to_string(),
            Self::SignedInt(_) | Self::UnsignedInt(_) => "Int".to_string(),
            Self::Float32 | Self::Float64 => "Float".to_string(),
            Self::CString => "String".to_string(),
            Self::ByteBuffer { .. } | Self::OpaqueHandle { .. } => "Any".to_string(),
        }
    }

    pub fn default_literal(&self) -> &'static str {
        match self {
            Self::Unit => "()",
            Self::Bool => "false",
            Self::SignedInt(_) | Self::UnsignedInt(_) => "0",
            Self::Float32 | Self::Float64 => "0.0",
            Self::CString => "\"\"",
            Self::ByteBuffer { .. } | Self::OpaqueHandle { .. } => "()",
        }
    }

    pub fn render_rust_ffi(&self) -> String {
        match self {
            Self::Unit => "()".to_string(),
            Self::Bool => "bool".to_string(),
            Self::SignedInt(name) | Self::UnsignedInt(name) => name.clone(),
            Self::Float32 => "f32".to_string(),
            Self::Float64 => "f64".to_string(),
            Self::CString => "*const std::os::raw::c_char".to_string(),
            Self::ByteBuffer { mutable, .. } => {
                if *mutable {
                    "*mut u8".to_string()
                } else {
                    "*const u8".to_string()
                }
            }
            Self::OpaqueHandle { mutable, .. } => {
                if *mutable {
                    "*mut std::ffi::c_void".to_string()
                } else {
                    "*const std::ffi::c_void".to_string()
                }
            }
        }
    }
}

#[derive(Debug, Clone)]
pub struct BindingBundle {
    pub functions: Vec<CFunctionBinding>,
    pub report_entries: Vec<BindingReportEntry>,
    pub source_fingerprints: Vec<FileFingerprint>,
}

#[allow(dead_code)]
#[derive(Debug, Clone)]
pub struct GeneratedArtifacts {
    pub canonical_module_source: String,
    pub prelude_source: String,
    pub bridge_source: String,
    pub report: BindingReport,
    pub report_text: String,
    pub manifest_json: String,
    pub packaged_bridge_manifest_json: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BindingManifest {
    pub schema_version: String,
    pub library_name: String,
    pub parser_backend: String,
    pub supported_targets: Vec<String>,
    pub capabilities: Vec<String>,
    pub generated_module: String,
    pub generated_prelude: String,
    pub entries: Vec<BindingReportEntry>,
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
    pub manifest_json_path: PathBuf,
    pub packaged_bridge_manifest_path: PathBuf,
    pub bridge_manifest_path: PathBuf,
    pub bridge_source_path: PathBuf,
    pub dylib_path: Option<PathBuf>,
    pub canonical_module_source: String,
    pub prelude_source: String,
    pub packaged_bridge_manifest: PackagedBridgeImport,
    pub cache_hit: bool,
}
