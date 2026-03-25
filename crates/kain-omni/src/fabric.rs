use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::{Path, PathBuf};
use std::str::FromStr;
use std::time::{SystemTime, UNIX_EPOCH};

use serde::{Deserialize, Serialize};

use crate::{OmniError, OmniResult};

pub const FABRIC_MANIFEST_FILE_NAME: &str = "KAIN.fabric.toml";
const FABRIC_SCHEMA_VERSION: u32 = 1;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FabricTemplateKind {
    Local,
    Polyglot,
}

impl FromStr for FabricTemplateKind {
    type Err = OmniError;

    fn from_str(value: &str) -> Result<Self, Self::Err> {
        match value.trim().to_ascii_lowercase().as_str() {
            "local" => Ok(Self::Local),
            "polyglot" | "default" => Ok(Self::Polyglot),
            other => Err(OmniError::Config(format!(
                "unknown Fabric template '{other}'. expected 'local' or 'polyglot'"
            ))),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricManifest {
    #[serde(default = "default_fabric_schema_version")]
    pub version: u32,
    #[serde(default)]
    pub workspace: FabricWorkspace,
    #[serde(default)]
    pub requires: Vec<FabricCapabilityRequirement>,
    #[serde(default)]
    pub steps: Vec<FabricStep>,
    #[serde(default)]
    pub reports: FabricReportConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricWorkspace {
    #[serde(default = "default_workspace_root")]
    pub root: PathBuf,
    #[serde(default)]
    pub search_roots: Vec<PathBuf>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum FabricRuntimeKind {
    Kain,
    Python,
    RustCrate,
    CAbi,
    Node,
}

impl FabricRuntimeKind {
    pub fn implied_capability_key(&self) -> &'static str {
        match self {
            Self::Kain => "runtime.kain",
            Self::Python => "runtime.python",
            Self::RustCrate => "runtime.rust-crate",
            Self::CAbi => "runtime.c-abi",
            Self::Node => "runtime.node",
        }
    }

    pub fn display_name(&self) -> &'static str {
        match self {
            Self::Kain => "kain",
            Self::Python => "python",
            Self::RustCrate => "rust_crate",
            Self::CAbi => "c_abi",
            Self::Node => "node",
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricCapabilityRequirement {
    pub key: String,
    #[serde(default = "default_capability_version")]
    pub version: u32,
    #[serde(default)]
    pub optional: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricStep {
    pub id: String,
    pub runtime: FabricRuntimeKind,
    #[serde(default)]
    pub entry: Option<PathBuf>,
    #[serde(default)]
    pub module: Option<String>,
    #[serde(default)]
    pub crate_name: Option<String>,
    #[serde(default)]
    pub manifest_path: Option<PathBuf>,
    #[serde(default)]
    pub library: Option<PathBuf>,
    #[serde(default)]
    pub depends_on: Vec<String>,
    #[serde(default)]
    pub requires: Vec<FabricCapabilityRequirement>,
    #[serde(default)]
    pub outputs: Vec<FabricOutputBinding>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricOutputBinding {
    pub name: String,
    pub kind: FabricContractKind,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum FabricContractKind {
    SharedBuffer,
    SharedImage,
    Value,
}

impl FabricContractKind {
    pub fn implied_capability_key(&self) -> &'static str {
        match self {
            Self::SharedBuffer => "contract.shared-buffer",
            Self::SharedImage => "contract.shared-image",
            Self::Value => "contract.value",
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricReportConfig {
    #[serde(default = "default_report_directory")]
    pub directory: PathBuf,
    #[serde(default)]
    pub emit_jsonl_events: bool,
}

#[derive(Debug, Clone)]
pub struct FabricInitResult {
    pub manifest_path: PathBuf,
    pub created_paths: Vec<PathBuf>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricValidationResult {
    pub manifest_path: PathBuf,
    pub step_count: usize,
    pub runtime_counts: BTreeMap<String, usize>,
    pub required_capabilities: Vec<String>,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum FabricSessionStatus {
    Succeeded,
    Failed,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum FabricStepStatus {
    Pending,
    Succeeded,
    Failed,
    Blocked,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricStepExecution {
    pub id: String,
    pub runtime: FabricRuntimeKind,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub entry: Option<PathBuf>,
    #[serde(default)]
    pub depends_on: Vec<String>,
    pub status: FabricStepStatus,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub started_unix_ms: Option<u128>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub finished_unix_ms: Option<u128>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub output: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub error: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricExecutionResult {
    pub manifest_path: PathBuf,
    pub validation: FabricValidationResult,
    pub session_id: String,
    pub workspace_root: PathBuf,
    pub session_directory: PathBuf,
    pub report_path: PathBuf,
    pub lock_path: PathBuf,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub events_path: Option<PathBuf>,
    pub status: FabricSessionStatus,
    pub started_unix_ms: u128,
    pub finished_unix_ms: u128,
    pub step_results: Vec<FabricStepExecution>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricEventRecord {
    pub timestamp_unix_ms: u128,
    pub kind: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub step_id: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub runtime: Option<FabricRuntimeKind>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub status: Option<FabricStepStatus>,
    pub message: String,
}

impl Default for FabricManifest {
    fn default() -> Self {
        polyglot_manifest_template()
    }
}

impl Default for FabricWorkspace {
    fn default() -> Self {
        Self {
            root: default_workspace_root(),
            search_roots: vec![
                PathBuf::from("src"),
                PathBuf::from("scripts"),
                PathBuf::from("native"),
            ],
        }
    }
}

impl Default for FabricReportConfig {
    fn default() -> Self {
        Self {
            directory: default_report_directory(),
            emit_jsonl_events: true,
        }
    }
}

pub fn init_fabric_manifest(
    root: &Path,
    template: FabricTemplateKind,
) -> OmniResult<FabricInitResult> {
    fs::create_dir_all(root)?;

    let manifest = match template {
        FabricTemplateKind::Local => local_manifest_template(),
        FabricTemplateKind::Polyglot => polyglot_manifest_template(),
    };
    let manifest_path = root.join(FABRIC_MANIFEST_FILE_NAME);
    fs::write(&manifest_path, toml::to_string_pretty(&manifest)?)?;

    let mut created_paths = vec![manifest_path.clone()];

    match template {
        FabricTemplateKind::Local => {
            let src_dir = root.join("src");
            fs::create_dir_all(&src_dir)?;
            let kain_entry = src_dir.join("main.kn");
            write_if_missing(
                &kain_entry,
                "fn main() -> String:\n    return \"kain-fabric-local\"\n",
            )?;
            created_paths.push(kain_entry);
        }
        FabricTemplateKind::Polyglot => {
            let src_dir = root.join("src");
            let scripts_dir = root.join("scripts");
            let native_dir = root.join("native");
            fs::create_dir_all(&src_dir)?;
            fs::create_dir_all(&scripts_dir)?;
            fs::create_dir_all(&native_dir)?;

            let kain_entry = src_dir.join("main.kn");
            write_if_missing(
                &kain_entry,
                "fn main() -> String:\n    println(\"kain fabric kain step\")\n    return \"fabric-kain-step\"\n",
            )?;
            created_paths.push(kain_entry);

            let python_step = scripts_dir.join("python_step.py");
            write_if_missing(
                &python_step,
                "def run():\n    return {\"status\": \"python-step-ready\"}\n",
            )?;
            created_paths.push(python_step);

            let node_step = scripts_dir.join("node_step.mjs");
            write_if_missing(
                &node_step,
                "export function run() {\n  return { status: 'node-step-ready' };\n}\n",
            )?;
            created_paths.push(node_step);

            let native_readme = native_dir.join("README.md");
            write_if_missing(
                &native_readme,
                "# Native bridge placeholders\n\nPlace future C ABI libraries or bridge artifacts here for Fabric sessions.\n",
            )?;
            created_paths.push(native_readme);
        }
    }

    created_paths.sort();
    created_paths.dedup();
    Ok(FabricInitResult {
        manifest_path,
        created_paths,
    })
}

pub fn load_fabric_manifest(path: &Path) -> OmniResult<FabricManifest> {
    let content = fs::read_to_string(path)?;
    Ok(toml::from_str(&content)?)
}

pub fn validate_fabric_manifest_path(path: &Path) -> OmniResult<FabricValidationResult> {
    let manifest = load_fabric_manifest(path)?;
    validate_fabric_manifest(path, &manifest)
}

pub fn validate_fabric_manifest(
    manifest_path: &Path,
    manifest: &FabricManifest,
) -> OmniResult<FabricValidationResult> {
    if manifest.version != FABRIC_SCHEMA_VERSION {
        return Err(OmniError::Config(format!(
            "Fabric manifest version {} is not supported. Expected {}",
            manifest.version, FABRIC_SCHEMA_VERSION
        )));
    }

    if manifest.steps.is_empty() {
        return Err(OmniError::Config(
            "Fabric manifest must declare at least one step".to_string(),
        ));
    }

    let mut ids = BTreeSet::new();
    let mut runtime_counts = BTreeMap::<String, usize>::new();
    let mut required_capabilities = BTreeSet::new();
    let supported_capabilities = supported_local_fabric_capabilities();

    for requirement in &manifest.requires {
        validate_capability_requirement(requirement, &supported_capabilities)?;
        required_capabilities.insert(requirement.key.clone());
    }

    for step in &manifest.steps {
        let trimmed_id = step.id.trim();
        if trimmed_id.is_empty() {
            return Err(OmniError::Config(
                "Fabric steps must have a non-empty id".to_string(),
            ));
        }
        if !ids.insert(trimmed_id.to_string()) {
            return Err(OmniError::Config(format!(
                "Fabric step id '{trimmed_id}' is duplicated"
            )));
        }

        validate_step_shape(step)?;
        *runtime_counts
            .entry(step.runtime.display_name().to_string())
            .or_insert(0) += 1;
        required_capabilities.insert(step.runtime.implied_capability_key().to_string());

        for requirement in &step.requires {
            validate_capability_requirement(requirement, &supported_capabilities)?;
            required_capabilities.insert(requirement.key.clone());
        }

        for output in &step.outputs {
            required_capabilities.insert(output.kind.implied_capability_key().to_string());
        }
    }

    for step in &manifest.steps {
        for dependency in &step.depends_on {
            if !ids.contains(dependency) {
                return Err(OmniError::Config(format!(
                    "Fabric step '{}' depends on unknown step '{}'",
                    step.id, dependency
                )));
            }
            if dependency == &step.id {
                return Err(OmniError::Config(format!(
                    "Fabric step '{}' cannot depend on itself",
                    step.id
                )));
            }
        }
    }

    detect_dependency_cycles(&manifest.steps)?;

    Ok(FabricValidationResult {
        manifest_path: manifest_path.to_path_buf(),
        step_count: manifest.steps.len(),
        runtime_counts,
        required_capabilities: required_capabilities.into_iter().collect(),
    })
}

pub fn detect_dependency_cycles(steps: &[FabricStep]) -> OmniResult<()> {
    #[derive(Clone, Copy, PartialEq, Eq)]
    enum VisitState {
        Visiting,
        Done,
    }

    fn visit(
        step_id: &str,
        graph: &BTreeMap<String, Vec<String>>,
        states: &mut BTreeMap<String, VisitState>,
        stack: &mut Vec<String>,
    ) -> OmniResult<()> {
        if let Some(state) = states.get(step_id).copied() {
            if state == VisitState::Done {
                return Ok(());
            }
            if state == VisitState::Visiting {
                stack.push(step_id.to_string());
                return Err(OmniError::Config(format!(
                    "Fabric dependency cycle detected: {}",
                    stack.join(" -> ")
                )));
            }
        }

        states.insert(step_id.to_string(), VisitState::Visiting);
        stack.push(step_id.to_string());
        if let Some(dependencies) = graph.get(step_id) {
            for dependency in dependencies {
                visit(dependency, graph, states, stack)?;
            }
        }
        stack.pop();
        states.insert(step_id.to_string(), VisitState::Done);
        Ok(())
    }

    let graph = steps
        .iter()
        .map(|step| (step.id.clone(), step.depends_on.clone()))
        .collect::<BTreeMap<_, _>>();
    let mut states = BTreeMap::new();
    let mut stack = Vec::new();
    for step in steps {
        visit(&step.id, &graph, &mut states, &mut stack)?;
    }
    Ok(())
}

pub fn topological_step_order(steps: &[FabricStep]) -> OmniResult<Vec<String>> {
    let mut in_degree = steps
        .iter()
        .map(|step| (step.id.clone(), step.depends_on.len()))
        .collect::<BTreeMap<_, _>>();
    let mut dependents = BTreeMap::<String, Vec<String>>::new();
    for step in steps {
        for dependency in &step.depends_on {
            dependents
                .entry(dependency.clone())
                .or_default()
                .push(step.id.clone());
        }
    }

    let mut ready = in_degree
        .iter()
        .filter(|(_, count)| **count == 0)
        .map(|(id, _)| id.clone())
        .collect::<BTreeSet<_>>();
    let mut ordered = Vec::with_capacity(steps.len());

    while let Some(next_id) = ready.iter().next().cloned() {
        ready.remove(&next_id);
        ordered.push(next_id.clone());
        if let Some(children) = dependents.get(&next_id) {
            for child in children {
                let Some(count) = in_degree.get_mut(child) else {
                    return Err(OmniError::Config(format!(
                        "Fabric execution graph lost dependent step '{child}'"
                    )));
                };
                *count = count.saturating_sub(1);
                if *count == 0 {
                    ready.insert(child.clone());
                }
            }
        }
    }

    if ordered.len() != steps.len() {
        return Err(OmniError::Config(
            "Fabric execution order could not be resolved".to_string(),
        ));
    }

    Ok(ordered)
}

pub fn supported_local_fabric_capabilities() -> BTreeSet<String> {
    [
        "session.local",
        "runtime.kain",
        "runtime.python",
        "runtime.rust-crate",
        "runtime.c-abi",
        "runtime.node",
        "contract.value",
        "contract.shared-buffer",
        "contract.shared-image",
    ]
    .into_iter()
    .map(str::to_string)
    .collect()
}

pub fn resolve_fabric_path(base: &Path, path: &Path) -> PathBuf {
    if path.is_absolute() {
        path.to_path_buf()
    } else {
        base.join(path)
    }
}

pub fn write_fabric_json<T: Serialize>(path: &Path, value: &T) -> OmniResult<()> {
    let encoded = serde_json::to_string_pretty(value)
        .map_err(|err| OmniError::Config(format!("Failed to serialize Fabric artifact: {err}")))?;
    fs::write(path, encoded)?;
    Ok(())
}

pub fn unix_timestamp_ms() -> u128 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_millis())
        .unwrap_or(0)
}

fn validate_capability_requirement(
    requirement: &FabricCapabilityRequirement,
    supported_capabilities: &BTreeSet<String>,
) -> OmniResult<()> {
    let key = requirement.key.trim();
    if key.is_empty() {
        return Err(OmniError::Config(
            "Fabric capability keys must be non-empty".to_string(),
        ));
    }
    if !supported_capabilities.contains(key) {
        return Err(OmniError::Config(format!(
            "Fabric capability '{}' is not supported by the current local validator",
            key
        )));
    }
    Ok(())
}

fn validate_step_shape(step: &FabricStep) -> OmniResult<()> {
    match step.runtime {
        FabricRuntimeKind::Kain => {
            if step.entry.is_none() {
                return Err(OmniError::Config(format!(
                    "Fabric step '{}' with runtime 'kain' must declare 'entry'",
                    step.id
                )));
            }
        }
        FabricRuntimeKind::Python | FabricRuntimeKind::Node => {
            if step.entry.is_none()
                && step
                    .module
                    .as_ref()
                    .is_none_or(|value| value.trim().is_empty())
            {
                return Err(OmniError::Config(format!(
                    "Fabric step '{}' with runtime '{}' must declare 'entry' or 'module'",
                    step.id,
                    step.runtime.display_name()
                )));
            }
        }
        FabricRuntimeKind::RustCrate => {
            if step
                .crate_name
                .as_ref()
                .is_none_or(|value| value.trim().is_empty())
            {
                return Err(OmniError::Config(format!(
                    "Fabric step '{}' with runtime 'rust_crate' must declare 'crate_name'",
                    step.id
                )));
            }
        }
        FabricRuntimeKind::CAbi => {
            if step.library.is_none() {
                return Err(OmniError::Config(format!(
                    "Fabric step '{}' with runtime 'c_abi' must declare 'library'",
                    step.id
                )));
            }
        }
    }

    for output in &step.outputs {
        if output.name.trim().is_empty() {
            return Err(OmniError::Config(format!(
                "Fabric step '{}' has an output with an empty name",
                step.id
            )));
        }
    }

    Ok(())
}

fn write_if_missing(path: &Path, content: &str) -> OmniResult<()> {
    if !path.exists() {
        fs::write(path, content)?;
    }
    Ok(())
}

fn default_fabric_schema_version() -> u32 {
    FABRIC_SCHEMA_VERSION
}

fn default_workspace_root() -> PathBuf {
    PathBuf::from(".")
}

fn default_capability_version() -> u32 {
    1
}

fn default_report_directory() -> PathBuf {
    PathBuf::from(".kain/fabric/reports")
}

fn local_manifest_template() -> FabricManifest {
    FabricManifest {
        version: FABRIC_SCHEMA_VERSION,
        workspace: FabricWorkspace {
            root: PathBuf::from("."),
            search_roots: vec![PathBuf::from("src")],
        },
        requires: vec![FabricCapabilityRequirement {
            key: "session.local".to_string(),
            version: 1,
            optional: false,
        }],
        steps: vec![FabricStep {
            id: "main".to_string(),
            runtime: FabricRuntimeKind::Kain,
            entry: Some(PathBuf::from("src/main.kn")),
            module: None,
            crate_name: None,
            manifest_path: None,
            library: None,
            depends_on: Vec::new(),
            requires: Vec::new(),
            outputs: vec![FabricOutputBinding {
                name: "result".to_string(),
                kind: FabricContractKind::Value,
            }],
        }],
        reports: FabricReportConfig::default(),
    }
}

fn polyglot_manifest_template() -> FabricManifest {
    FabricManifest {
        version: FABRIC_SCHEMA_VERSION,
        workspace: FabricWorkspace::default(),
        requires: vec![FabricCapabilityRequirement {
            key: "session.local".to_string(),
            version: 1,
            optional: false,
        }],
        steps: vec![
            FabricStep {
                id: "python_source".to_string(),
                runtime: FabricRuntimeKind::Python,
                entry: Some(PathBuf::from("scripts/python_step.py")),
                module: None,
                crate_name: None,
                manifest_path: None,
                library: None,
                depends_on: Vec::new(),
                requires: vec![FabricCapabilityRequirement {
                    key: "contract.shared-image".to_string(),
                    version: 1,
                    optional: false,
                }],
                outputs: vec![FabricOutputBinding {
                    name: "image".to_string(),
                    kind: FabricContractKind::SharedImage,
                }],
            },
            FabricStep {
                id: "kain_orchestrator".to_string(),
                runtime: FabricRuntimeKind::Kain,
                entry: Some(PathBuf::from("src/main.kn")),
                module: None,
                crate_name: None,
                manifest_path: None,
                library: None,
                depends_on: vec!["python_source".to_string()],
                requires: vec![FabricCapabilityRequirement {
                    key: "contract.shared-image".to_string(),
                    version: 1,
                    optional: false,
                }],
                outputs: vec![FabricOutputBinding {
                    name: "report".to_string(),
                    kind: FabricContractKind::Value,
                }],
            },
            FabricStep {
                id: "rust_analyzer".to_string(),
                runtime: FabricRuntimeKind::RustCrate,
                entry: None,
                module: None,
                crate_name: Some("example_fabric_crate".to_string()),
                manifest_path: Some(PathBuf::from("Cargo.toml")),
                library: None,
                depends_on: vec!["kain_orchestrator".to_string()],
                requires: vec![FabricCapabilityRequirement {
                    key: "contract.shared-buffer".to_string(),
                    version: 1,
                    optional: true,
                }],
                outputs: vec![FabricOutputBinding {
                    name: "analysis".to_string(),
                    kind: FabricContractKind::Value,
                }],
            },
            FabricStep {
                id: "native_filter".to_string(),
                runtime: FabricRuntimeKind::CAbi,
                entry: None,
                module: None,
                crate_name: None,
                manifest_path: None,
                library: Some(PathBuf::from("native/image_fx.dll")),
                depends_on: vec!["python_source".to_string()],
                requires: vec![FabricCapabilityRequirement {
                    key: "contract.shared-image".to_string(),
                    version: 1,
                    optional: false,
                }],
                outputs: vec![FabricOutputBinding {
                    name: "filtered_image".to_string(),
                    kind: FabricContractKind::SharedImage,
                }],
            },
            FabricStep {
                id: "node_packager".to_string(),
                runtime: FabricRuntimeKind::Node,
                entry: Some(PathBuf::from("scripts/node_step.mjs")),
                module: None,
                crate_name: None,
                manifest_path: None,
                library: None,
                depends_on: vec!["kain_orchestrator".to_string(), "native_filter".to_string()],
                requires: Vec::new(),
                outputs: vec![FabricOutputBinding {
                    name: "html_bundle".to_string(),
                    kind: FabricContractKind::Value,
                }],
            },
        ],
        reports: FabricReportConfig::default(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn init_fabric_manifest_writes_polyglot_template() {
        let dir = tempfile::tempdir().unwrap();
        let result = init_fabric_manifest(dir.path(), FabricTemplateKind::Polyglot).unwrap();
        let manifest = load_fabric_manifest(&result.manifest_path).unwrap();

        assert_eq!(
            result.manifest_path,
            dir.path().join(FABRIC_MANIFEST_FILE_NAME)
        );
        assert!(result
            .created_paths
            .iter()
            .any(|path| path.ends_with("src\\main.kn")));
        assert!(manifest.steps.len() >= 3);
        assert!(matches!(
            manifest.steps[0].runtime,
            FabricRuntimeKind::Python
        ));
    }

    #[test]
    fn validate_fabric_manifest_rejects_duplicate_step_ids() {
        let manifest = FabricManifest {
            version: FABRIC_SCHEMA_VERSION,
            workspace: FabricWorkspace::default(),
            requires: Vec::new(),
            steps: vec![
                FabricStep {
                    id: "dup".to_string(),
                    runtime: FabricRuntimeKind::Kain,
                    entry: Some(PathBuf::from("src/main.kn")),
                    module: None,
                    crate_name: None,
                    manifest_path: None,
                    library: None,
                    depends_on: Vec::new(),
                    requires: Vec::new(),
                    outputs: Vec::new(),
                },
                FabricStep {
                    id: "dup".to_string(),
                    runtime: FabricRuntimeKind::Node,
                    entry: Some(PathBuf::from("scripts/node_step.mjs")),
                    module: None,
                    crate_name: None,
                    manifest_path: None,
                    library: None,
                    depends_on: Vec::new(),
                    requires: Vec::new(),
                    outputs: Vec::new(),
                },
            ],
            reports: FabricReportConfig::default(),
        };

        let error =
            validate_fabric_manifest(Path::new(FABRIC_MANIFEST_FILE_NAME), &manifest).unwrap_err();
        assert!(error.to_string().contains("duplicated"));
    }

    #[test]
    fn validate_fabric_manifest_rejects_dependency_cycles() {
        let manifest = FabricManifest {
            version: FABRIC_SCHEMA_VERSION,
            workspace: FabricWorkspace::default(),
            requires: Vec::new(),
            steps: vec![
                FabricStep {
                    id: "a".to_string(),
                    runtime: FabricRuntimeKind::Kain,
                    entry: Some(PathBuf::from("src/main.kn")),
                    module: None,
                    crate_name: None,
                    manifest_path: None,
                    library: None,
                    depends_on: vec!["b".to_string()],
                    requires: Vec::new(),
                    outputs: Vec::new(),
                },
                FabricStep {
                    id: "b".to_string(),
                    runtime: FabricRuntimeKind::Node,
                    entry: Some(PathBuf::from("scripts/node_step.mjs")),
                    module: None,
                    crate_name: None,
                    manifest_path: None,
                    library: None,
                    depends_on: vec!["a".to_string()],
                    requires: Vec::new(),
                    outputs: Vec::new(),
                },
            ],
            reports: FabricReportConfig::default(),
        };

        let error =
            validate_fabric_manifest(Path::new(FABRIC_MANIFEST_FILE_NAME), &manifest).unwrap_err();
        assert!(error.to_string().contains("cycle"));
    }

    #[test]
    fn validate_default_polyglot_template_succeeds() {
        let manifest = polyglot_manifest_template();
        let result =
            validate_fabric_manifest(Path::new(FABRIC_MANIFEST_FILE_NAME), &manifest).unwrap();

        assert_eq!(result.step_count, manifest.steps.len());
        assert!(result
            .required_capabilities
            .iter()
            .any(|capability| capability == "runtime.python"));
        assert!(result
            .required_capabilities
            .iter()
            .any(|capability| capability == "contract.shared-image"));
    }
}
