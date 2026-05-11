use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::{Path, PathBuf};
use std::str::FromStr;
use std::time::{SystemTime, UNIX_EPOCH};

use serde::{Deserialize, Serialize};
use serde_json::Value as JsonValue;

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
    GpuCompute,
}

impl FabricRuntimeKind {
    pub fn implied_capability_key(&self) -> &'static str {
        match self {
            Self::Kain => "runtime.kain",
            Self::Python => "runtime.python",
            Self::RustCrate => "runtime.rust-crate",
            Self::CAbi => "runtime.c-abi",
            Self::Node => "runtime.node",
            Self::GpuCompute => "runtime.gpu-compute",
        }
    }

    pub fn display_name(&self) -> &'static str {
        match self {
            Self::Kain => "kain",
            Self::Python => "python",
            Self::RustCrate => "rust_crate",
            Self::CAbi => "c_abi",
            Self::Node => "node",
            Self::GpuCompute => "gpu_compute",
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
    pub blade: Option<String>,
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
    pub shader_source: Option<PathBuf>,
    #[serde(default)]
    pub compute_key: Option<String>,
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

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum FabricContractKind {
    SharedBuffer,
    SharedImage,
    Value,
    ComputePlan,
}

impl FabricContractKind {
    pub fn implied_capability_key(&self) -> &'static str {
        match self {
            Self::SharedBuffer => "contract.shared-buffer",
            Self::SharedImage => "contract.shared-image",
            Self::Value => "contract.value",
            Self::ComputePlan => "contract.compute-plan",
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
pub struct FabricFailureReason {
    pub code: String,
    pub message: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub details: Option<JsonValue>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricValueSnapshot {
    pub summary: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub json: Option<JsonValue>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricSharedBufferSnapshot {
    pub contract: String,
    pub byte_length: usize,
    pub element_type: String,
    pub element_size: i64,
    pub shape: Vec<i64>,
    pub strides: Vec<i64>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub format: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub mime_type: Option<String>,
    pub source_runtime: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub source_backend: Option<String>,
    pub ownership: String,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub labels: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricSharedImageSnapshot {
    pub contract: String,
    pub byte_length: usize,
    pub representation: String,
    pub width: i64,
    pub height: i64,
    pub channels: i64,
    pub layout: String,
    pub pixel_format: String,
    pub mime_type: String,
    pub row_stride: i64,
    pub color_space: String,
    pub alpha_mode: String,
    pub source_runtime: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub source_backend: Option<String>,
    pub ownership: String,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub labels: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricComputeDispatchSnapshot {
    pub compute_key: String,
    pub dispatch_invocations: u64,
    pub tensor_binding_count: usize,
    pub stream_binding_count: usize,
    pub neural_node_count: usize,
    pub output_binding_count: usize,
    pub total_output_bytes: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "snake_case")]
pub enum FabricOutputPayloadSnapshot {
    Value {
        value: FabricValueSnapshot,
    },
    SharedBuffer {
        buffer: FabricSharedBufferSnapshot,
    },
    SharedImage {
        image: FabricSharedImageSnapshot,
    },
    ComputePlan {
        dispatch: FabricComputeDispatchSnapshot,
    },
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricProducedOutput {
    pub name: String,
    pub declared_kind: FabricContractKind,
    pub payload: FabricOutputPayloadSnapshot,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricResolvedInputRecord {
    pub from_step_id: String,
    pub output_name: String,
    pub declared_kind: FabricContractKind,
    pub payload: FabricOutputPayloadSnapshot,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FabricStepExecution {
    pub id: String,
    pub runtime: FabricRuntimeKind,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub blade: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub entry: Option<PathBuf>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub module: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub crate_name: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub manifest_path: Option<PathBuf>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub library: Option<PathBuf>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub shader_source: Option<PathBuf>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub compute_key: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub adapter: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub resolved_entry: Option<PathBuf>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub resolved_shader_source: Option<PathBuf>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub resolved_manifest_path: Option<PathBuf>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub resolved_library: Option<PathBuf>,
    #[serde(default)]
    pub depends_on: Vec<String>,
    pub status: FabricStepStatus,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub started_unix_ms: Option<u128>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub finished_unix_ms: Option<u128>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub resolved_inputs: Vec<FabricResolvedInputRecord>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub outputs: Vec<FabricProducedOutput>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub error: Option<FabricFailureReason>,
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
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub details: Option<JsonValue>,
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
            let local_crate_dir = root.join("local_crate");
            let local_crate_src_dir = local_crate_dir.join("src");
            fs::create_dir_all(&src_dir)?;
            fs::create_dir_all(&scripts_dir)?;
            fs::create_dir_all(&native_dir)?;
            fs::create_dir_all(&local_crate_src_dir)?;

            let kain_entry = src_dir.join("main.kn");
            write_if_missing(&kain_entry, POLYGLOT_KAIN_ENTRY)?;
            created_paths.push(kain_entry);

            let python_step = scripts_dir.join("python_step.py");
            write_if_missing(&python_step, POLYGLOT_PYTHON_STEP)?;
            created_paths.push(python_step);

            let node_step = scripts_dir.join("node_step.mjs");
            write_if_missing(&node_step, POLYGLOT_NODE_STEP)?;
            created_paths.push(node_step);

            let rust_entry = src_dir.join("rust_step.kn");
            write_if_missing(&rust_entry, POLYGLOT_RUST_ENTRY)?;
            created_paths.push(rust_entry);

            let native_entry = src_dir.join("native_step.kn");
            write_if_missing(&native_entry, POLYGLOT_NATIVE_ENTRY)?;
            created_paths.push(native_entry);

            let kain_manifest = root.join("KAIN.toml");
            write_if_missing(&kain_manifest, &render_polyglot_kain_manifest())?;
            created_paths.push(kain_manifest);

            let local_crate_manifest = local_crate_dir.join("Cargo.toml");
            write_if_missing(&local_crate_manifest, POLYGLOT_LOCAL_CRATE_MANIFEST)?;
            created_paths.push(local_crate_manifest);

            let local_crate_lib = local_crate_src_dir.join("lib.rs");
            write_if_missing(&local_crate_lib, POLYGLOT_LOCAL_CRATE_LIB)?;
            created_paths.push(local_crate_lib);

            let native_header = native_dir.join("image_fx.h");
            write_if_missing(&native_header, POLYGLOT_NATIVE_HEADER)?;
            created_paths.push(native_header);

            let native_source = native_dir.join("image_fx.c");
            write_if_missing(&native_source, POLYGLOT_NATIVE_SOURCE)?;
            created_paths.push(native_source);

            let readme = root.join("FABRIC.README.md");
            write_if_missing(&readme, &render_polyglot_readme())?;
            created_paths.push(readme);
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
        "runtime.gpu-compute",
        "contract.value",
        "contract.shared-buffer",
        "contract.shared-image",
        "contract.compute-plan",
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
    let has_blade = step
        .blade
        .as_ref()
        .is_some_and(|value| !value.trim().is_empty());
    match step.runtime {
        FabricRuntimeKind::Kain => {
            if step.entry.is_none() && !has_blade {
                return Err(OmniError::Config(format!(
                    "Fabric step '{}' with runtime 'kain' must declare 'entry' or 'blade'",
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
                && !has_blade
            {
                return Err(OmniError::Config(format!(
                    "Fabric step '{}' with runtime 'rust_crate' must declare 'crate_name' or 'blade'",
                    step.id
                )));
            }
            if step.entry.is_none() && !has_blade {
                return Err(OmniError::Config(format!(
                    "Fabric step '{}' with runtime 'rust_crate' must declare 'entry' or a blade with a Kain glue entry",
                    step.id
                )));
            }
        }
        FabricRuntimeKind::CAbi => {
            if step.library.is_none() && !has_blade {
                return Err(OmniError::Config(format!(
                    "Fabric step '{}' with runtime 'c_abi' must declare 'library' or 'blade'",
                    step.id
                )));
            }
            if step.entry.is_none() && !has_blade {
                return Err(OmniError::Config(format!(
                    "Fabric step '{}' with runtime 'c_abi' must declare 'entry' or a blade with a Kain glue entry",
                    step.id
                )));
            }
        }
        FabricRuntimeKind::GpuCompute => {
            if step.shader_source.is_none() && !has_blade {
                return Err(OmniError::Config(format!(
                    "Fabric step '{}' with runtime 'gpu_compute' must declare 'shader_source' or 'blade'",
                    step.id
                )));
            }
            if step
                .compute_key
                .as_ref()
                .is_none_or(|k| k.trim().is_empty())
                && !has_blade
            {
                return Err(OmniError::Config(format!(
                    "Fabric step '{}' with runtime 'gpu_compute' must declare 'compute_key' or a blade compute key",
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

const POLYGLOT_PYTHON_STEP: &str = r#"def run(fabric_inputs):
    return {
        "width": 6,
        "height": 4,
        "accent": 29,
        "title": "fabric-local",
    }
"#;

const POLYGLOT_NODE_STEP: &str = r#"export function run(fabricInputs) {
  const report = fabricInputs.kain_orchestrator.report;
  const analysis = fabricInputs.rust_analyzer.analysis;
  const image = fabricInputs.native_filter.filtered_image;
  const snapshot = fabricInputs.native_filter.snapshot;
  return [
    "<article data-fabric='local-first'>",
    `<h1>${analysis}</h1>`,
    `<p>${report}</p>`,
    `<p>image=${image.width}x${image.height} channels=${image.channels}</p>`,
    `<p>snapshot-bytes=${snapshot.byte_length}</p>`,
    "</article>",
  ].join("");
}
"#;

const POLYGLOT_KAIN_ENTRY: &str = r#"use std::interop::bridge

struct KainOutputs:
    image: Any
    report: String

fn build_pixels(width: Int, height: Int, accent: Int) -> Array<Int>:
    let bytes = []
    let y = 0
    while y < height:
        let x = 0
        while x < width:
            let base = (x * 17 + y * 23 + accent) % 255
            bytes.push(base)
            bytes.push((base + accent) % 255)
            bytes.push((base + x + y) % 255)
            bytes.push(255)
            x = x + 1
        y = y + 1
    return bytes

fn main() -> KainOutputs:
    let settings = fabric_inputs.python_source.settings
    let width = settings.width
    let height = settings.height
    let accent = settings.accent
    let pixels = build_pixels(width, height, accent)
    let image = interop_shared_image_from_bytes(
        pixels,
        width,
        height,
        4,
        "HWC",
        "rgba8",
        "image/x-kain-raster",
    )
    let info = interop_shared_image_info(image)
    let report = settings.title + ":" + str(info.width) + "x" + str(info.height) + ":" + str(accent)
    return KainOutputs { image: image, report: report }
"#;

const POLYGLOT_NATIVE_ENTRY: &str = r#"use c::image_fx
use std::interop::bridge

struct NativeOutputs:
    filtered_image: Any
    snapshot: Any

fn main() -> NativeOutputs:
    let image = fabric_inputs.kain_orchestrator.image
    let info = interop_shared_image_info(image)
    let before_bytes = interop_shared_image_bytes(image)
    let before_first_channel = before_bytes[0]
    imagefx_halo_rgba(image, info.byte_length, 37)
    let after_bytes = interop_shared_image_bytes(image)
    assert(
        before_first_channel != after_bytes[0],
        "expected native image filter to mutate the shared image"
    )
    let bytes = interop_shared_image_bytes(image)
    let snapshot = interop_shared_buffer_from_bytes(
        bytes,
        "u8",
        [len(bytes)],
        "rgba8",
        "application/octet-stream",
    )
    return NativeOutputs { filtered_image: image, snapshot: snapshot }
"#;

const POLYGLOT_RUST_ENTRY: &str = r#"use rust::fabric_runtime_lab
use std::interop::bridge

fn main() -> String:
    let snapshot = fabric_inputs.native_filter.snapshot
    let snapshot_info = interop_shared_buffer_info(snapshot)
    let bytes = interop_shared_buffer_bytes(snapshot)
    let checksum = buffer_checksum(bytes)
    return analysis_label(
        snapshot_info.byte_length,
        checksum,
        fabric_inputs.kain_orchestrator.report,
    )
"#;

const POLYGLOT_LOCAL_CRATE_MANIFEST: &str = r#"[package]
name = "fabric_runtime_lab"
version = "0.1.0"
edition = "2021"

[lib]
name = "fabric_runtime_lab"
path = "src/lib.rs"

[workspace]
"#;

const POLYGLOT_LOCAL_CRATE_LIB: &str = r#"pub fn buffer_checksum(bytes: Vec<i64>) -> i64 {
    let mut total = 0i64;
    for (index, value) in bytes.iter().enumerate() {
        let weight = ((index as i64) % 19) + 3;
        total = (total + value * weight + index as i64) % 1_000_003;
    }
    total
}

pub fn analysis_label(byte_length: i64, checksum: i64, upstream_report: String) -> String {
    format!(
        "rust-analysis:{}:checksum={}:{}",
        byte_length, checksum, upstream_report
    )
}
"#;

const POLYGLOT_NATIVE_HEADER: &str = r#"#if defined(_WIN32)
#define IMAGEFX_EXPORT __declspec(dllexport)
#else
#define IMAGEFX_EXPORT
#endif

#include <stddef.h>
#include <stdint.h>

IMAGEFX_EXPORT uint64_t imagefx_checksum(const uint8_t* pixels, size_t len);
IMAGEFX_EXPORT void imagefx_halo_rgba(uint8_t* pixels, size_t len, int accent);
IMAGEFX_EXPORT const char* imagefx_signature(int width, int height, uint64_t checksum);
"#;

const POLYGLOT_NATIVE_SOURCE: &str = r#"#include "image_fx.h"

#include <stdio.h>

static char G_SIGNATURE[128];

uint64_t imagefx_checksum(const uint8_t* pixels, size_t len) {
    uint64_t checksum = 1469598103934665603ull;
    size_t index = 0;
    while (index < len) {
        checksum ^= (uint64_t)pixels[index];
        checksum *= 1099511628211ull;
        index += 1;
    }
    return checksum;
}

void imagefx_halo_rgba(uint8_t* pixels, size_t len, int accent) {
    size_t index = 0;
    while (index + 3 < len) {
        pixels[index + 0] = (uint8_t)((pixels[index + 0] + accent) % 255);
        pixels[index + 1] = (uint8_t)((pixels[index + 1] + (accent / 2)) % 255);
        pixels[index + 2] = (uint8_t)(255 - pixels[index + 2]);
        pixels[index + 3] = 255;
        index += 4;
    }
}

const char* imagefx_signature(int width, int height, uint64_t checksum) {
    snprintf(
        G_SIGNATURE,
        sizeof(G_SIGNATURE),
        "imagefx:%dx%d:%llu",
        width,
        height,
        (unsigned long long)checksum
    );
    return G_SIGNATURE;
}
"#;

fn polyglot_native_compile_command() -> &'static str {
    if cfg!(target_os = "windows") {
        "clang -shared -O2 native/image_fx.c -o native/image_fx.dll"
    } else if cfg!(target_os = "macos") {
        "clang -shared -O2 native/image_fx.c -o native/libimage_fx.dylib"
    } else {
        "clang -shared -O2 -fPIC native/image_fx.c -o native/libimage_fx.so"
    }
}

fn render_polyglot_kain_manifest() -> String {
    "[c_ffi]\n\n[[c_ffi.libraries]]\nname = \"image_fx\"\nheader = \"native/image_fx.h\"\nshared_lib = \"native/${kain_dynlib:image_fx}\"\n".to_string()
}

fn render_polyglot_readme() -> String {
    format!(
        "# Fabric Polyglot Starter\n\nThis scaffold mirrors `smoketest/fabric/polyglot_local` and is meant to be runnable as a local-first proof, not just a validation example.\n\n## Pipeline Shape\n\n- `python_source` produces settings\n- `kain_orchestrator` builds a shared image and report\n- `native_filter` mutates the shared image and snapshots bytes through the C ABI lane\n- `rust_analyzer` computes a report from the shared buffer snapshot\n- `node_packager` renders the final HTML bundle\n\n## Quickstart\n\n1. Build the native library:\n   `{}`\n2. Validate the manifest:\n   `kain fabric validate --manifest KAIN.fabric.toml`\n3. Run the pipeline:\n   `kain fabric run --manifest KAIN.fabric.toml`\n\nFabric reports land under `.kain/fabric/reports/<session>/` with `report.json`, `session.lock.json`, and `events.jsonl`.\n",
        polyglot_native_compile_command()
    )
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
            blade: None,
            entry: Some(PathBuf::from("src/main.kn")),
            module: None,
            crate_name: None,
            manifest_path: None,
            library: None,
            shader_source: None,
            compute_key: None,
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
        workspace: FabricWorkspace {
            root: PathBuf::from("."),
            search_roots: vec![
                PathBuf::from("src"),
                PathBuf::from("scripts"),
                PathBuf::from("native"),
                PathBuf::from("local_crate"),
            ],
        },
        requires: vec![FabricCapabilityRequirement {
            key: "session.local".to_string(),
            version: 1,
            optional: false,
        }],
        steps: vec![
            FabricStep {
                id: "python_source".to_string(),
                runtime: FabricRuntimeKind::Python,
                blade: None,
                entry: Some(PathBuf::from("scripts/python_step.py")),
                module: None,
                crate_name: None,
                manifest_path: None,
                library: None,
                shader_source: None,
                compute_key: None,
                depends_on: Vec::new(),
                requires: Vec::new(),
                outputs: vec![FabricOutputBinding {
                    name: "settings".to_string(),
                    kind: FabricContractKind::Value,
                }],
            },
            FabricStep {
                id: "kain_orchestrator".to_string(),
                runtime: FabricRuntimeKind::Kain,
                blade: None,
                entry: Some(PathBuf::from("src/main.kn")),
                module: None,
                crate_name: None,
                manifest_path: None,
                library: None,
                shader_source: None,
                compute_key: None,
                depends_on: vec!["python_source".to_string()],
                requires: Vec::new(),
                outputs: vec![
                    FabricOutputBinding {
                        name: "image".to_string(),
                        kind: FabricContractKind::SharedImage,
                    },
                    FabricOutputBinding {
                        name: "report".to_string(),
                        kind: FabricContractKind::Value,
                    },
                ],
            },
            FabricStep {
                id: "native_filter".to_string(),
                runtime: FabricRuntimeKind::CAbi,
                blade: None,
                entry: Some(PathBuf::from("src/native_step.kn")),
                module: Some("image_fx".to_string()),
                crate_name: None,
                manifest_path: None,
                library: Some(PathBuf::from("native/${kain_dynlib:image_fx}")),
                shader_source: None,
                compute_key: None,
                depends_on: vec!["kain_orchestrator".to_string()],
                requires: Vec::new(),
                outputs: vec![
                    FabricOutputBinding {
                        name: "filtered_image".to_string(),
                        kind: FabricContractKind::SharedImage,
                    },
                    FabricOutputBinding {
                        name: "snapshot".to_string(),
                        kind: FabricContractKind::SharedBuffer,
                    },
                ],
            },
            FabricStep {
                id: "rust_analyzer".to_string(),
                runtime: FabricRuntimeKind::RustCrate,
                blade: None,
                entry: Some(PathBuf::from("src/rust_step.kn")),
                module: None,
                crate_name: Some("fabric_runtime_lab".to_string()),
                manifest_path: Some(PathBuf::from("local_crate/Cargo.toml")),
                library: None,
                shader_source: None,
                compute_key: None,
                depends_on: vec!["native_filter".to_string(), "kain_orchestrator".to_string()],
                requires: Vec::new(),
                outputs: vec![FabricOutputBinding {
                    name: "analysis".to_string(),
                    kind: FabricContractKind::Value,
                }],
            },
            FabricStep {
                id: "node_packager".to_string(),
                runtime: FabricRuntimeKind::Node,
                blade: None,
                entry: Some(PathBuf::from("scripts/node_step.mjs")),
                module: None,
                crate_name: None,
                manifest_path: None,
                library: None,
                shader_source: None,
                compute_key: None,
                depends_on: vec![
                    "kain_orchestrator".to_string(),
                    "native_filter".to_string(),
                    "rust_analyzer".to_string(),
                ],
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
        assert!(result
            .created_paths
            .iter()
            .any(|path| path.ends_with("KAIN.toml")));
        assert!(result
            .created_paths
            .iter()
            .any(|path| path.ends_with("FABRIC.README.md")));
        assert!(result
            .created_paths
            .iter()
            .any(|path| path.ends_with("local_crate\\Cargo.toml")));
        let readme = fs::read_to_string(dir.path().join("FABRIC.README.md")).unwrap();
        assert!(readme.contains("smoketest/fabric/polyglot_local"));
        assert!(readme.contains("kain fabric run --manifest KAIN.fabric.toml"));
        assert_eq!(manifest.steps.len(), 5);
        assert!(matches!(
            manifest.steps[0].runtime,
            FabricRuntimeKind::Python
        ));
        assert_eq!(manifest.steps[0].outputs[0].name, "settings");
        assert_eq!(
            manifest.steps[2].library.as_deref(),
            Some(Path::new("native/${kain_dynlib:image_fx}"))
        );
        assert_eq!(
            manifest.steps[3].crate_name.as_deref(),
            Some("fabric_runtime_lab")
        );
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
                    blade: None,
                    entry: Some(PathBuf::from("src/main.kn")),
                    module: None,
                    crate_name: None,
                    manifest_path: None,
                    library: None,
                    shader_source: None,
                    compute_key: None,
                    depends_on: Vec::new(),
                    requires: Vec::new(),
                    outputs: Vec::new(),
                },
                FabricStep {
                    id: "dup".to_string(),
                    runtime: FabricRuntimeKind::Node,
                    blade: None,
                    entry: Some(PathBuf::from("scripts/node_step.mjs")),
                    module: None,
                    crate_name: None,
                    manifest_path: None,
                    library: None,
                    shader_source: None,
                    compute_key: None,
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
                    blade: None,
                    entry: Some(PathBuf::from("src/main.kn")),
                    module: None,
                    crate_name: None,
                    manifest_path: None,
                    library: None,
                    shader_source: None,
                    compute_key: None,
                    depends_on: vec!["b".to_string()],
                    requires: Vec::new(),
                    outputs: Vec::new(),
                },
                FabricStep {
                    id: "b".to_string(),
                    runtime: FabricRuntimeKind::Node,
                    blade: None,
                    entry: Some(PathBuf::from("scripts/node_step.mjs")),
                    module: None,
                    crate_name: None,
                    manifest_path: None,
                    library: None,
                    shader_source: None,
                    compute_key: None,
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
