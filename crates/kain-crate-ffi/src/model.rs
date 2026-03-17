use crate::config::RustFfiConfig;
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;
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

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ImportCrateOptions {
    pub manifest_path: Option<PathBuf>,
    pub crate_path: Option<PathBuf>,
    pub output_dir: Option<PathBuf>,
    pub report_json: Option<PathBuf>,
    pub mode: ArtifactMode,
    pub features: Vec<String>,
    pub all_features: bool,
    pub no_default_features: bool,
}

impl Default for ImportCrateOptions {
    fn default() -> Self {
        Self {
            manifest_path: None,
            crate_path: None,
            output_dir: None,
            report_json: None,
            mode: ArtifactMode::Both,
            features: Vec::new(),
            all_features: false,
            no_default_features: false,
        }
    }
}

#[derive(Debug, Clone, Default)]
pub struct PrepareContext {
    pub current_dir: Option<PathBuf>,
    pub manifest_path: Option<PathBuf>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ResolutionKind {
    Workspace,
    PathConfig,
    Dependency,
    RegistryConfig,
    CratePath,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DependencySpec {
    Path {
        package: String,
        dependency_name: String,
        path: PathBuf,
    },
    Registry {
        package: String,
        dependency_name: String,
        version: String,
        features: Vec<String>,
        default_features: bool,
    },
}

#[derive(Debug, Clone)]
pub struct ResolvedCrate {
    pub import_name: String,
    pub package_name: String,
    pub dependency_name: String,
    pub library_target_name: String,
    pub version: String,
    pub manifest_path: PathBuf,
    pub crate_root: PathBuf,
    pub crate_root_file: PathBuf,
    pub workspace_root: PathBuf,
    pub resolution_kind: ResolutionKind,
    pub dependency_spec: DependencySpec,
    pub features: Vec<String>,
    pub default_features: bool,
    pub all_features: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FileFingerprint {
    pub path: String,
    pub sha256: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ItemStatus {
    Callable,
    TypeOnly,
    Stubbed,
    SkippedInternal,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ItemKind {
    Function,
    Method,
    Struct,
    Enum,
    TypeAlias,
    Constant,
    Macro,
    Module,
    Trait,
    ReExport,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BindingReportEntry {
    pub symbol_path: String,
    pub module_path: Vec<String>,
    pub kind: ItemKind,
    pub status: ItemStatus,
    pub reason: Option<String>,
    pub docs: Vec<String>,
    pub emitted_symbol: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BindingReport {
    pub crate_name: String,
    pub package_name: String,
    pub version: String,
    pub manifest_path: String,
    pub resolution_kind: ResolutionKind,
    pub cache_dir: String,
    pub report_json_path: String,
    pub report_text_path: String,
    pub entries: Vec<BindingReportEntry>,
    pub source_fingerprints: Vec<FileFingerprint>,
}

#[derive(Debug, Clone)]
pub struct BridgeFunctionBinding {
    pub emitted_name: String,
    pub exported_aliases: Vec<String>,
    pub rust_call_path: String,
    pub params: Vec<BridgeParam>,
    pub return_type: BridgeType,
    pub docs: Vec<String>,
}

#[derive(Debug, Clone)]
pub struct BridgeParam {
    pub name: String,
    pub ty: BridgeType,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum BridgeType {
    Unit,
    Bool(String),
    Int(String),
    Float(String),
    StringOwned,
    StringRef,
    Option(Box<BridgeType>),
    Array(Box<BridgeType>),
}

impl BridgeType {
    pub fn render_kain(&self) -> String {
        match self {
            Self::Unit => "Unit".to_string(),
            Self::Bool(_) => "Bool".to_string(),
            Self::Int(_) => "Int".to_string(),
            Self::Float(_) => "Float".to_string(),
            Self::StringOwned | Self::StringRef => "String".to_string(),
            Self::Option(inner) => format!("Option<{}>", inner.render_kain()),
            Self::Array(inner) => format!("Array<{}>", inner.render_kain()),
        }
    }

    pub fn render_rust(&self) -> String {
        match self {
            Self::Unit => "()".to_string(),
            Self::Bool(name) | Self::Int(name) | Self::Float(name) => name.clone(),
            Self::StringOwned => "String".to_string(),
            Self::StringRef => "&str".to_string(),
            Self::Option(inner) => format!("Option<{}>", inner.render_rust()),
            Self::Array(inner) => format!("Vec<{}>", inner.render_rust()),
        }
    }

    pub fn default_literal(&self) -> &'static str {
        match self {
            Self::Unit => "()",
            Self::Bool(_) => "false",
            Self::Int(_) => "0",
            Self::Float(_) => "0.0",
            Self::StringOwned | Self::StringRef => "\"\"",
            Self::Option(_) => "none",
            Self::Array(_) => "[]",
        }
    }

    pub fn from_kain_trait_type(&self) -> Option<String> {
        match self {
            Self::Unit => Some("()".to_string()),
            Self::Bool(name) | Self::Int(name) | Self::Float(name) => Some(name.clone()),
            Self::StringOwned => Some("String".to_string()),
            Self::StringRef => None,
            Self::Option(inner) => Some(format!("Option<{}>", inner.render_rust())),
            Self::Array(inner) => Some(format!("Vec<{}>", inner.render_rust())),
        }
    }
}

#[derive(Debug, Clone)]
pub struct GeneratedModuleItem {
    pub name: String,
    pub source: String,
    pub docs: Vec<String>,
}

#[derive(Debug, Clone, Default)]
pub struct ModuleNode {
    pub items: Vec<GeneratedModuleItem>,
    pub children: BTreeMap<String, ModuleNode>,
}

#[derive(Debug, Clone)]
pub struct BindingBundle {
    pub module_root: ModuleNode,
    pub bridge_functions: Vec<BridgeFunctionBinding>,
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
pub struct ImportCrateOutput {
    pub resolved: ResolvedCrate,
    pub config_root: Option<PathBuf>,
    pub rust_ffi_config: Option<RustFfiConfig>,
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
