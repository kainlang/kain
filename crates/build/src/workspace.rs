use blade::{
    discover_workspace, find_build_script_in, load_effective_kain_manifest, BladeError,
    BladeWorkspace, KainBuildTaskSection, KainManifest, ResolvedBlade, ResolvedCffiLibrary,
    FABRIC_MANIFEST_NAME, KAIN_BUILD_SCRIPT_NAMES,
};
use crate::native_link::{
    link_native_binary, NativeEmit, NativeLinkRequest, NativeRuntimeArtifacts,
};
use kain_amalgamate::{
    pack_capsule, CapsuleCompression, CapsuleContents, CapsuleHeaderStyle, CapsuleIndexMode,
    CapsuleStorage, PackOptions, DEFAULT_PREVIEW_SYMBOL_LIMIT,
};
use kain_core::ast::{Item, Program};
use kain_core::diagnostics::SpanMapper;
use kain_core::lexer::Lexer;
use kain_core::parser::Parser;
use kain_core::tooling_config::apply_cargo_command_defaults;
use kain_core::CompileTarget;
use kain_driver::{
    DriverSession, ToolingProgressEvent, ToolingProgressRecord, ToolingProgressSink,
    ToolingProgressStatus,
};
use kain_fmt::format_program;
use kain_fs as kfs;
use kain_omni::fabric::{FabricRuntimeKind, FabricSessionStatus};
use serde::{Deserialize, Serialize};
use serde_json::json;
use sha2::{Digest, Sha256};
use std::collections::{BTreeMap, BTreeSet};
use std::ffi::OsStr;
use std::io::Read;
use std::path::{Component, Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

const DEFAULT_PROFILE: &str = "debug";
const DEFAULT_ARTIFACT_ROOT: &str = ".kain/out";
const DEFAULT_CACHE_ROOT: &str = ".kain/cache/build";
const DEFAULT_REPORT_ROOT: &str = ".kain/reports/build";
const BUILD_ADAPTER_VERSION: &str = "kain-build-v8";
const BUILD_ARTIFACT_SCHEMA_VERSION: u32 = 2;

pub type BuildResult<T> = Result<T, BuildError>;

#[derive(Debug, thiserror::Error)]
pub enum BuildError {
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
    #[error("filesystem error: {0}")]
    Fs(#[from] kain_fs::FsError),
    #[error("clean error: {0}")]
    Clean(#[from] kain_clean::CleanError),
    #[error("blade workspace error: {0}")]
    Blade(#[from] BladeError),
    #[error("Fabric error: {0}")]
    Omni(#[from] kain_omni::OmniError),
    #[error("Kain error: {0}")]
    Kain(#[from] kain_core::KainError),
    #[error("{0}")]
    Config(String),
    #[error("command failed: {0}")]
    Command(String),
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
#[serde(rename_all = "kebab-case")]
pub enum BuildLane {
    Bootstrap,
    Dev,
    Release,
    Dist,
    Selfhost,
}

impl BuildLane {
    pub fn parse(value: &str) -> Option<Self> {
        match value.trim().to_ascii_lowercase().as_str() {
            "bootstrap" => Some(Self::Bootstrap),
            "dev" | "debug" => Some(Self::Dev),
            "release" => Some(Self::Release),
            "dist" | "distribution" => Some(Self::Dist),
            "selfhost" | "self-host" => Some(Self::Selfhost),
            _ => None,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
            Self::Bootstrap => "bootstrap",
            Self::Dev => "dev",
            Self::Release => "release",
            Self::Dist => "dist",
            Self::Selfhost => "selfhost",
        }
    }

    pub fn cargo_profile(self) -> &'static str {
        match self {
            Self::Release | Self::Dist | Self::Selfhost => "release",
            Self::Bootstrap | Self::Dev => "debug",
        }
    }
}

impl Default for BuildLane {
    fn default() -> Self {
        Self::Dev
    }
}

#[derive(Debug, Clone)]
pub struct BladeBuildOptions {
    pub path: PathBuf,
    pub profile: Option<String>,
    pub lane: Option<BuildLane>,
    pub target: Option<String>,
    pub dry_run: bool,
    pub clean: bool,
    pub include_vulkan: bool,
    pub fail_fast: bool,
    pub progress: Option<ToolingProgressSink>,
}

impl BladeBuildOptions {
    pub fn new(path: impl Into<PathBuf>) -> Self {
        Self {
            path: path.into(),
            profile: None,
            lane: None,
            target: None,
            dry_run: false,
            clean: false,
            include_vulkan: false,
            fail_fast: true,
            progress: None,
        }
    }
}

#[derive(Debug, Clone, Serialize)]
pub struct BladeBuildPlan {
    pub schema_version: u32,
    pub workspace_root: PathBuf,
    pub artifact_root: PathBuf,
    pub cache_root: PathBuf,
    pub report_root: PathBuf,
    pub host: String,
    pub lane: BuildLane,
    pub profile: String,
    pub target: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub build_graph: Option<KainBuildGraphProvenance>,
    pub tasks: Vec<BuildTask>,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default, PartialEq, Eq)]
pub struct KainBuildGraphProvenance {
    pub graph_source: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub defaults_merged_from: Option<PathBuf>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub build_script: Option<PathBuf>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub overrides: Vec<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub platform_packages: Vec<KainBuildGraphPlatformPackage>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct KainBuildGraphPlatformPackage {
    pub package: String,
    pub provider: String,
    pub source: String,
}

#[derive(Debug, Clone)]
struct DiscoveredBuildGraphScript {
    graph_source: String,
    build_script: PathBuf,
    evaluated_manifest: Option<KainManifest>,
    evaluator_error: Option<String>,
    platform_packages: Vec<KainBuildGraphPlatformPackage>,
    explicit_tasks: Vec<KainBuildTaskSection>,
}

#[derive(Debug, Clone, Serialize)]
pub struct BuildTask {
    pub id: String,
    pub kind: BuildTaskKind,
    pub blade: Option<String>,
    pub description: String,
    pub depends_on: Vec<String>,
    pub inputs: Vec<PathBuf>,
    pub outputs: Vec<PathBuf>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub required_capabilities: Vec<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub matrix_axes: Vec<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub telemetry: Vec<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub certifies: Vec<String>,
    pub cacheable: bool,
    #[serde(skip)]
    adapter: BuildTaskAdapter,
}

#[derive(Debug, Default, Deserialize)]
struct NativeRuntimeManifestCacheInputs {
    #[serde(default)]
    sources: Vec<PathBuf>,
    #[serde(default)]
    windows_sources: Vec<PathBuf>,
    #[serde(default)]
    linux_sources: Vec<PathBuf>,
    #[serde(default)]
    macos_sources: Vec<PathBuf>,
    #[serde(default)]
    include_dirs: Vec<PathBuf>,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
#[serde(rename_all = "kebab-case")]
pub enum BuildTaskKind {
    BladeCheck,
    KainCheck,
    KainCompile,
    NativeExecutable,
    Test,
    Proof,
    Benchmark,
    Attrition,
    Certify,
    RustArtifacts,
    NativeUiApp,
    CargoBuild,
    CSharedLibrary,
    GpuArtifacts,
    FabricValidate,
    FabricRun,
    Exec,
    Amalgamate,
    Node,
    Bun,
}

impl BuildTaskKind {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::BladeCheck => "blade-check",
            Self::KainCheck => "kain-check",
            Self::KainCompile => "kain-compile",
            Self::NativeExecutable => "native-executable",
            Self::Test => "test",
            Self::Proof => "proof",
            Self::Benchmark => "benchmark",
            Self::Attrition => "attrition",
            Self::Certify => "certify",
            Self::RustArtifacts => "rust-artifacts",
            Self::NativeUiApp => "native-ui-app",
            Self::CargoBuild => "cargo-build",
            Self::CSharedLibrary => "c-shared-library",
            Self::GpuArtifacts => "gpu-artifacts",
            Self::FabricValidate => "fabric-validate",
            Self::FabricRun => "fabric-run",
            Self::Exec => "exec",
            Self::Amalgamate => "amalgamate",
            Self::Node => "node",
            Self::Bun => "bun",
        }
    }
}

#[derive(Debug, Clone)]
enum BuildTaskAdapter {
    BladeCheck,
    KainCheck {
        entry: PathBuf,
        target: CompileTarget,
    },
    KainCompile {
        source: PathBuf,
        target: CompileTarget,
        emit: NativeEmit,
        primary_output: PathBuf,
        materialized_primary_output: Option<PathBuf>,
        root_component: Option<String>,
    },
    NativeExecutable {
        entry: PathBuf,
        output: PathBuf,
        script_path: PathBuf,
        report_path: PathBuf,
        verify_llvm: bool,
    },
    KainTest {
        entry: PathBuf,
        target: CompileTarget,
        mode_override: Option<kain_test::KainTestMode>,
        report_path: PathBuf,
        proof_required: bool,
        fail_fast: bool,
        run_ignored: bool,
    },
    ExternalEvidence {
        label: String,
        program: String,
        args: Vec<String>,
        cwd: PathBuf,
        report_path: PathBuf,
    },
    Certify {
        report_path: PathBuf,
    },
    RustArtifacts {
        source: PathBuf,
        output_base: PathBuf,
        materialized_output_base: Option<PathBuf>,
        include_spirv: bool,
    },
    NativeUiApp {
        source: PathBuf,
        host: NativeUiBuildHost,
        project_dir: PathBuf,
        artifact_output_dir: PathBuf,
        cargo_target_dir: PathBuf,
        gpu_runtime_cargo_target_dir: PathBuf,
        executable_output_dir: Option<PathBuf>,
        app_name: Option<String>,
        window_title: Option<String>,
        root_component: Option<String>,
        tauri_bundle_identifier: Option<String>,
        tauri_window_label: Option<String>,
        runtime_crate_name: String,
        runtime_dependency: NativeUiRuntimeDependency,
        build_executable: bool,
        release: bool,
    },
    CargoBuild {
        manifest_path: PathBuf,
        target_dir: PathBuf,
        release: bool,
    },
    CSharedLibrary {
        library_name: String,
        sources: Vec<PathBuf>,
        header: PathBuf,
        include_paths: Vec<PathBuf>,
        defines: Vec<String>,
        link_libs: Vec<String>,
        cpp_options: Vec<String>,
        canonical_output: PathBuf,
        materialized_output: Option<PathBuf>,
    },
    GpuArtifacts {
        source: PathBuf,
        output_base: PathBuf,
        target: GpuArtifactTarget,
        no_residency: bool,
        no_derived: bool,
    },
    Fabric {
        manifest_path: PathBuf,
        run: bool,
    },
    Exec {
        label: String,
        program: String,
        args: Vec<String>,
        cwd: PathBuf,
        env: BTreeMap<String, String>,
        report_path: PathBuf,
        stdout_path: Option<PathBuf>,
        stderr_path: Option<PathBuf>,
        timeout_ms: Option<u64>,
        required_outputs: Vec<PathBuf>,
    },
    Amalgamate {
        source_root: PathBuf,
        output_path: PathBuf,
        report_path: PathBuf,
        settings: AmalgamateTaskSettings,
    },
    NodeLike {
        runtime: NodeRuntimeKind,
        entry: Option<PathBuf>,
        command: Option<String>,
        args: Vec<String>,
        cwd: PathBuf,
    },
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
enum NodeRuntimeKind {
    Node,
    Bun,
}

#[derive(Debug, Clone)]
struct AmalgamateTaskSettings {
    storage: CapsuleStorage,
    contents: CapsuleContents,
    name: Option<String>,
    capsule_set: Option<String>,
    version: Option<String>,
    authors: Vec<String>,
    notes: Vec<String>,
    tags: Vec<String>,
    meta: BTreeMap<String, String>,
    header_style: CapsuleHeaderStyle,
    preview_symbol_limit: usize,
    compression: CapsuleCompression,
    api_index: CapsuleIndexMode,
    module_index: CapsuleIndexMode,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum NativeUiBuildHost {
    Qt,
    Tauri,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum NativeUiRuntimeDependency {
    WorkspacePath,
    Path(PathBuf),
    Version(String),
}

impl NativeUiBuildHost {
    pub fn parse(value: &str) -> Option<Self> {
        match value.trim().to_ascii_lowercase().as_str() {
            "qt" => Some(Self::Qt),
            "tauri" => Some(Self::Tauri),
            _ => None,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
            Self::Qt => "qt",
            Self::Tauri => "tauri",
        }
    }
}

#[derive(Debug, Clone)]
pub struct KainFileBuildOptions {
    pub input: PathBuf,
    pub output: Option<PathBuf>,
    pub target: CompileTarget,
    pub emit: NativeEmit,
    pub profile: Option<String>,
    pub lane: Option<BuildLane>,
    pub dry_run: bool,
    pub clean: bool,
    pub fail_fast: bool,
    pub progress: Option<ToolingProgressSink>,
}

impl KainFileBuildOptions {
    pub fn new(input: impl Into<PathBuf>, target: CompileTarget) -> Self {
        Self {
            input: input.into(),
            output: None,
            target,
            emit: NativeEmit::default(),
            profile: None,
            lane: None,
            dry_run: false,
            clean: false,
            fail_fast: true,
            progress: None,
        }
    }
}

#[derive(Debug, Clone)]
pub struct KainRustBuildOptions {
    pub input: PathBuf,
    pub output: Option<PathBuf>,
    pub profile: Option<String>,
    pub lane: Option<BuildLane>,
    pub dry_run: bool,
    pub clean: bool,
    pub fail_fast: bool,
    pub include_spirv: bool,
    pub progress: Option<ToolingProgressSink>,
}

impl KainRustBuildOptions {
    pub fn new(input: impl Into<PathBuf>) -> Self {
        Self {
            input: input.into(),
            output: None,
            profile: None,
            lane: None,
            dry_run: false,
            clean: false,
            fail_fast: true,
            include_spirv: true,
            progress: None,
        }
    }
}

#[derive(Debug, Clone)]
pub struct KainNativeUiBuildOptions {
    pub input: PathBuf,
    pub host: NativeUiBuildHost,
    pub project_dir: Option<PathBuf>,
    pub artifact_output_dir: PathBuf,
    pub executable_output_dir: Option<PathBuf>,
    pub app_name: Option<String>,
    pub window_title: Option<String>,
    pub root_component: Option<String>,
    pub tauri_bundle_identifier: Option<String>,
    pub tauri_window_label: Option<String>,
    pub runtime_crate_name: String,
    pub runtime_dependency: NativeUiRuntimeDependency,
    pub build_executable: bool,
    pub release: bool,
    pub profile: Option<String>,
    pub lane: Option<BuildLane>,
    pub dry_run: bool,
    pub clean: bool,
    pub fail_fast: bool,
    pub progress: Option<ToolingProgressSink>,
}

impl KainNativeUiBuildOptions {
    pub fn new(input: impl Into<PathBuf>) -> Self {
        Self {
            input: input.into(),
            host: NativeUiBuildHost::Qt,
            project_dir: None,
            artifact_output_dir: PathBuf::from("generated"),
            executable_output_dir: None,
            app_name: None,
            window_title: None,
            root_component: None,
            tauri_bundle_identifier: None,
            tauri_window_label: None,
            runtime_crate_name: "kain-ui-native".to_string(),
            runtime_dependency: NativeUiRuntimeDependency::WorkspacePath,
            build_executable: true,
            release: false,
            profile: None,
            lane: None,
            dry_run: false,
            clean: false,
            fail_fast: true,
            progress: None,
        }
    }
}

#[derive(Debug, Clone)]
pub struct KainProjectBuildOptions {
    pub path: PathBuf,
    pub target_overrides: Option<Vec<String>>,
    pub rust_only: bool,
    pub profile: Option<String>,
    pub lane: Option<BuildLane>,
    pub dry_run: bool,
    pub clean: bool,
    pub fail_fast: bool,
    pub progress: Option<ToolingProgressSink>,
}

impl KainProjectBuildOptions {
    pub fn new(path: impl Into<PathBuf>) -> Self {
        Self {
            path: path.into(),
            target_overrides: None,
            rust_only: false,
            profile: None,
            lane: None,
            dry_run: false,
            clean: false,
            fail_fast: true,
            progress: None,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BladeBuildReport {
    pub schema_version: u32,
    pub workspace_root: PathBuf,
    pub artifact_root: PathBuf,
    pub cache_root: PathBuf,
    pub report_path: PathBuf,
    pub events_path: PathBuf,
    pub host: String,
    pub lane: BuildLane,
    pub profile: String,
    pub target: String,
    pub status: BladeBuildStatus,
    pub started_unix_ms: u128,
    pub finished_unix_ms: u128,
    pub dry_run: bool,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub build_graph: Option<KainBuildGraphProvenance>,
    pub tasks: Vec<BuildTaskExecution>,
}

impl BladeBuildReport {
    pub fn is_success(&self) -> bool {
        self.status == BladeBuildStatus::Succeeded
    }
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum BladeBuildStatus {
    Succeeded,
    Failed,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BuildTaskExecution {
    pub id: String,
    pub kind: BuildTaskKind,
    pub blade: Option<String>,
    pub status: BuildTaskStatus,
    pub cache_hit: bool,
    pub started_unix_ms: Option<u128>,
    pub finished_unix_ms: Option<u128>,
    pub inputs: Vec<PathBuf>,
    pub outputs: Vec<PathBuf>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub required_capabilities: Vec<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub matrix_axes: Vec<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub telemetry: Vec<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub certifies: Vec<String>,
    pub message: String,
    pub error: Option<String>,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum BuildTaskStatus {
    Planned,
    Cached,
    Succeeded,
    Failed,
    Skipped,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BuildArtifactManifest {
    pub schema_version: u32,
    pub task_id: String,
    pub lane: BuildLane,
    pub target: String,
    pub artifacts: Vec<BuildArtifactRecord>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BuildArtifactRecord {
    pub role: String,
    pub path: PathBuf,
    pub sha256: Option<String>,
    pub byte_length: Option<u64>,
}

#[derive(Debug, Deserialize)]
struct ProjectManifest {
    #[serde(default)]
    package: Option<ProjectPackageSection>,
    #[serde(default)]
    build: Option<ProjectBuildSection>,
}

#[derive(Debug, Deserialize)]
struct ProjectPackageSection {
    name: String,
}

#[derive(Debug, Deserialize)]
#[serde(default)]
struct ProjectBuildSection {
    entry: PathBuf,
    targets: Vec<String>,
}

impl Default for ProjectBuildSection {
    fn default() -> Self {
        Self {
            entry: PathBuf::from("src/main.kn"),
            targets: vec!["wasm".to_string()],
        }
    }
}

pub fn plan_blade_workspace(options: &BladeBuildOptions) -> BuildResult<BladeBuildPlan> {
    let workspace = discover_workspace(&options.path)?;
    let mut root_manifest = load_effective_kain_manifest(&workspace.root)?;
    if let Some(evaluated_manifest) = discover_evaluated_build_graph_manifest(&workspace.root)? {
        root_manifest = Some(match root_manifest {
            Some(manifest) => merge_build_graph_manifest(manifest, evaluated_manifest),
            None => evaluated_manifest,
        });
    }
    let config = BuildWorkspaceConfig::from_workspace(&workspace, root_manifest.as_ref(), options);

    let mut tasks = Vec::new();
    let mut sidecar_task_ids = Vec::new();
    let mut fabric_validate_task_ids = Vec::new();

    for blade in &workspace.blades {
        add_c_tasks(&mut tasks, &mut sidecar_task_ids, &config, blade)?;
        add_cargo_task(&mut tasks, &mut sidecar_task_ids, &config, blade)?;
        add_gpu_tasks(&mut tasks, &config, blade);
        add_explicit_blade_tasks(&mut tasks, &mut sidecar_task_ids, &config, blade)?;
    }

    if !workspace.blades.is_empty() {
        tasks.push(BuildTask {
            id: "blade-check".to_string(),
            kind: BuildTaskKind::BladeCheck,
            blade: None,
            description: "Validate blade manifests and materialized local artifacts".to_string(),
            depends_on: sidecar_task_ids.clone(),
            inputs: workspace
                .blades
                .iter()
                .flat_map(blade_authority_inputs)
                .collect(),
            outputs: Vec::new(),
            required_capabilities: Vec::new(),
            matrix_axes: Vec::new(),
            telemetry: Vec::new(),
            certifies: Vec::new(),
            cacheable: false,
            adapter: BuildTaskAdapter::BladeCheck,
        });
    }

    let fabric_manifests = discover_fabric_manifests(&workspace)?;
    for manifest_path in fabric_manifests {
        let validate_id = format!(
            "fabric-validate:{}",
            sanitize_id(&path_stem_or_name(&manifest_path))
        );
        let mut depends_on = sidecar_task_ids.clone();
        if !workspace.blades.is_empty() {
            depends_on.push("blade-check".to_string());
        }
        tasks.push(BuildTask {
            id: validate_id.clone(),
            kind: BuildTaskKind::FabricValidate,
            blade: None,
            description: format!("Validate Fabric manifest {}", manifest_path.display()),
            depends_on,
            inputs: vec![manifest_path.clone()],
            outputs: Vec::new(),
            required_capabilities: Vec::new(),
            matrix_axes: Vec::new(),
            telemetry: Vec::new(),
            certifies: Vec::new(),
            cacheable: false,
            adapter: BuildTaskAdapter::Fabric {
                manifest_path: manifest_path.clone(),
                run: false,
            },
        });
        fabric_validate_task_ids.push(validate_id.clone());

        if should_run_fabric_manifest(&manifest_path, options.include_vulkan)? {
            tasks.push(BuildTask {
                id: format!(
                    "fabric-run:{}",
                    sanitize_id(&path_stem_or_name(&manifest_path))
                ),
                kind: BuildTaskKind::FabricRun,
                blade: None,
                description: format!("Run Fabric manifest {}", manifest_path.display()),
                depends_on: vec![validate_id],
                inputs: vec![manifest_path.clone()],
                outputs: Vec::new(),
                required_capabilities: Vec::new(),
                matrix_axes: Vec::new(),
                telemetry: Vec::new(),
                certifies: Vec::new(),
                cacheable: false,
                adapter: BuildTaskAdapter::Fabric {
                    manifest_path,
                    run: true,
                },
            });
        }
    }

    let root_manifest_is_discovered_blade = workspace
        .blades
        .iter()
        .any(|blade| paths_equivalent(&blade.root, &workspace.root));
    if let Some(manifest) = root_manifest
        .as_ref()
        .filter(|_| !root_manifest_is_discovered_blade)
    {
        add_explicit_root_tasks(&mut tasks, &config, manifest)?;
    }

    let tasks = order_tasks(tasks)?;
    let build_graph =
        discover_build_graph_provenance(&workspace.root, workspace.manifest_path.as_deref())?;
    let plan = BladeBuildPlan {
        schema_version: BUILD_ARTIFACT_SCHEMA_VERSION,
        workspace_root: config.workspace_root,
        artifact_root: config.artifact_root,
        cache_root: config.cache_root,
        report_root: config.report_root,
        host: config.host,
        lane: config.lane,
        profile: config.profile,
        target: config.target,
        build_graph,
        tasks,
    };
    validate_plan_safety(&plan)?;
    Ok(plan)
}

pub fn build_blade_workspace(options: &BladeBuildOptions) -> BuildResult<BladeBuildReport> {
    let plan = plan_blade_workspace(options)?;
    execute_plan(plan, &BuildExecutionOptions::from(options))
}

pub fn plan_kain_file(options: &KainFileBuildOptions) -> BuildResult<BladeBuildPlan> {
    let workspace_root = workspace_root_for_input(&options.input)?;
    let config = StandaloneBuildConfig::new(
        workspace_root,
        options.profile.clone(),
        options.lane,
        Some(kain_driver::target_extension(options.target).to_string()),
    );
    let source = absolute_workspace_path(&config.workspace_root, &options.input)?;
    let unit = source_unit_name(&source);
    let task_root = config.task_root(&unit, "compile");
    let primary_output = task_root.join(format!(
        "{}.{}",
        unit,
        kain_driver::target_extension(options.target)
    ));
    let materialized_primary_output = options
        .output
        .as_ref()
        .map(|path| resolve_materialized_output_path(path, options.target, &config.workspace_root));
    let mut outputs = kain_compile_expected_outputs(
        options.target,
        &primary_output,
        materialized_primary_output.as_ref(),
    );
    outputs.push(artifact_manifest_path(&task_root));
    let tasks = vec![BuildTask {
        id: format!("kain-compile:{unit}"),
        kind: BuildTaskKind::KainCompile,
        blade: None,
        description: format!(
            "Compile {} for {}",
            source.display(),
            kain_driver::compile_target_name(options.target)
        ),
        depends_on: Vec::new(),
        inputs: vec![source.clone()],
        outputs,
        required_capabilities: Vec::new(),
        matrix_axes: Vec::new(),
        telemetry: Vec::new(),
        certifies: Vec::new(),
        cacheable: true,
        adapter: BuildTaskAdapter::KainCompile {
            source,
            target: options.target,
            emit: options.emit,
            primary_output,
            materialized_primary_output,
            root_component: None,
        },
    }];
    let plan = config.into_plan(kain_driver::compile_target_name(options.target), tasks);
    validate_plan_safety(&plan)?;
    Ok(plan)
}

pub fn build_kain_file(options: &KainFileBuildOptions) -> BuildResult<BladeBuildReport> {
    let plan = plan_kain_file(options)?;
    execute_plan(plan, &BuildExecutionOptions::from(options))
}

pub fn plan_kain_rust_file(options: &KainRustBuildOptions) -> BuildResult<BladeBuildPlan> {
    let workspace_root = workspace_root_for_input(&options.input)?;
    let config = StandaloneBuildConfig::new(
        workspace_root,
        options.profile.clone(),
        options.lane,
        Some("rust".to_string()),
    );
    let source = absolute_workspace_path(&config.workspace_root, &options.input)?;
    let unit = source_unit_name(&source);
    let task_root = config.task_root(&unit, "rust-artifacts");
    let output_base = task_root.join(&unit);
    let materialized_base = options.output.as_ref().map(|path| {
        let resolved = resolve_workspace_path(&config.workspace_root, path);
        if resolved.extension().is_some() {
            resolved.parent().unwrap_or_else(|| Path::new(".")).join(
                resolved
                    .file_stem()
                    .and_then(OsStr::to_str)
                    .unwrap_or(&unit),
            )
        } else {
            resolved.join(&unit)
        }
    });
    let mut outputs = vec![
        output_base.with_extension("rs"),
        artifact_manifest_path(&task_root),
    ];
    if let Some(materialized_base) = &materialized_base {
        outputs.push(materialized_base.with_extension("rs"));
    }
    let tasks = vec![BuildTask {
        id: format!("rust-artifacts:{unit}"),
        kind: BuildTaskKind::RustArtifacts,
        blade: None,
        description: format!("Emit Rust artifact bundle for {}", source.display()),
        depends_on: Vec::new(),
        inputs: vec![source.clone()],
        outputs,
        required_capabilities: Vec::new(),
        matrix_axes: Vec::new(),
        telemetry: Vec::new(),
        certifies: Vec::new(),
        cacheable: true,
        adapter: BuildTaskAdapter::RustArtifacts {
            source,
            output_base,
            materialized_output_base: materialized_base,
            include_spirv: options.include_spirv,
        },
    }];
    let plan = config.into_plan("rust", tasks);
    validate_plan_safety(&plan)?;
    Ok(plan)
}

pub fn build_kain_rust_file(options: &KainRustBuildOptions) -> BuildResult<BladeBuildReport> {
    let plan = plan_kain_rust_file(options)?;
    execute_plan(plan, &BuildExecutionOptions::from(options))
}

pub fn plan_kain_native_ui(options: &KainNativeUiBuildOptions) -> BuildResult<BladeBuildPlan> {
    let workspace_root = workspace_root_for_input(&options.input)?;
    let lane = options.lane.unwrap_or(if options.release {
        BuildLane::Release
    } else {
        BuildLane::Dev
    });
    let profile = options
        .profile
        .clone()
        .unwrap_or_else(|| lane.cargo_profile().to_string());
    let config = StandaloneBuildConfig::new(
        workspace_root,
        Some(profile),
        Some(lane),
        Some("native-ui".to_string()),
    );
    let source = absolute_workspace_path(&config.workspace_root, &options.input)?;
    let unit = source_unit_name(&source);
    let task_root = config.task_root(&unit, "native-ui");
    let project_dir = options
        .project_dir
        .as_ref()
        .map(|path| resolve_workspace_path(&config.workspace_root, path))
        .unwrap_or_else(|| task_root.join("project"));
    let artifact_output_dir = if options.artifact_output_dir.is_absolute() {
        options.artifact_output_dir.clone()
    } else {
        PathBuf::from("artifacts")
    };
    let cargo_target_dir = task_root.join("cargo-target");
    let gpu_runtime_cargo_target_dir = task_root.join("gpu-runtime-cargo-target");
    let executable_output_dir = options
        .executable_output_dir
        .as_ref()
        .map(|path| resolve_workspace_path(&config.workspace_root, path))
        .or_else(|| options.build_executable.then(|| task_root.join("bin")));
    let outputs = vec![project_dir.clone(), artifact_manifest_path(&task_root)];
    let tasks = vec![BuildTask {
        id: format!("native-ui:{unit}"),
        kind: BuildTaskKind::NativeUiApp,
        blade: None,
        description: format!(
            "Build {} native-ui app for {}",
            source.display(),
            options.host.as_str()
        ),
        depends_on: Vec::new(),
        inputs: vec![source.clone()],
        outputs,
        required_capabilities: Vec::new(),
        matrix_axes: Vec::new(),
        telemetry: Vec::new(),
        certifies: Vec::new(),
        cacheable: true,
        adapter: BuildTaskAdapter::NativeUiApp {
            source,
            host: options.host,
            project_dir,
            artifact_output_dir,
            cargo_target_dir,
            gpu_runtime_cargo_target_dir,
            executable_output_dir,
            app_name: options.app_name.clone(),
            window_title: options.window_title.clone(),
            root_component: options.root_component.clone(),
            tauri_bundle_identifier: options.tauri_bundle_identifier.clone(),
            tauri_window_label: options.tauri_window_label.clone(),
            runtime_crate_name: options.runtime_crate_name.clone(),
            runtime_dependency: options.runtime_dependency.clone(),
            build_executable: options.build_executable,
            release: options.release,
        },
    }];
    let plan = config.into_plan("native-ui", tasks);
    validate_plan_safety(&plan)?;
    Ok(plan)
}

pub fn build_kain_native_ui(options: &KainNativeUiBuildOptions) -> BuildResult<BladeBuildReport> {
    let plan = plan_kain_native_ui(options)?;
    execute_plan(plan, &BuildExecutionOptions::from(options))
}

pub fn plan_kain_project(options: &KainProjectBuildOptions) -> BuildResult<BladeBuildPlan> {
    let workspace_root = PathBuf::from(kfs::canonicalize_path(&options.path)?);
    let manifest_path = workspace_root.join("KAIN.toml");
    let mut manifest = load_effective_kain_manifest(&workspace_root)?.ok_or_else(|| {
        BuildError::Config(format!(
            "No Kain project authority found under {}; add build.kn or KAIN.toml",
            workspace_root.display()
        ))
    })?;
    if let Some(evaluated_manifest) = discover_evaluated_build_graph_manifest(&workspace_root)? {
        manifest = merge_build_graph_manifest(manifest, evaluated_manifest);
    }
    let lane = options
        .lane
        .or_else(|| options.profile.as_deref().and_then(BuildLane::parse))
        .or_else(|| manifest.build.profile.as_deref().and_then(BuildLane::parse))
        .unwrap_or_default();
    let profile = options
        .profile
        .clone()
        .or_else(|| manifest.build.profile.clone())
        .unwrap_or_else(|| lane.cargo_profile().to_string());
    let mut config = StandaloneBuildConfig::new(
        workspace_root.clone(),
        Some(profile),
        Some(lane),
        Some("project".to_string()),
    );
    if let Some(artifact_root) = manifest.build.artifact_root.as_deref() {
        config.artifact_root = resolve_workspace_path(&workspace_root, artifact_root);
    }
    if let Some(cache_root) = manifest.build.cache_root.as_deref() {
        config.cache_root = resolve_workspace_path(&workspace_root, cache_root);
    }
    let package_name = manifest
        .package
        .name
        .as_deref()
        .or(manifest.blade.name.as_deref())
        .map(sanitize_id)
        .filter(|value| !value.is_empty())
        .unwrap_or_else(|| {
            workspace_root
                .file_name()
                .and_then(OsStr::to_str)
                .map(sanitize_id)
                .filter(|value| !value.is_empty())
                .unwrap_or_else(|| "project".to_string())
        });
    let build = manifest.build.clone();
    let entry = resolve_workspace_path(
        &workspace_root,
        build
            .entry
            .as_deref()
            .or(manifest.blade.entry.as_deref())
            .unwrap_or_else(|| Path::new("src/main.kn")),
    );
    if !entry.exists() {
        return Err(BuildError::Config(format!(
            "Entry file not found: {}",
            entry.display()
        )));
    }
    let mut authority_inputs = vec![entry.clone()];
    if manifest_path.is_file() {
        authority_inputs.push(manifest_path.clone());
    }
    if let Some(path) = find_build_script_in(&workspace_root) {
        if !authority_inputs.iter().any(|existing| existing == &path) {
            authority_inputs.push(path);
        }
    }
    let mut tasks = Vec::new();
    if options.rust_only {
        let task_root = config.task_root(&package_name, "rust-artifacts");
        tasks.push(BuildTask {
            id: format!("rust-artifacts:{package_name}"),
            kind: BuildTaskKind::RustArtifacts,
            blade: None,
            description: format!("Emit Rust artifact bundle for {}", entry.display()),
            depends_on: Vec::new(),
            inputs: authority_inputs.clone(),
            outputs: vec![
                task_root.join(format!("{package_name}.rs")),
                artifact_manifest_path(&task_root),
            ],
            required_capabilities: Vec::new(),
            matrix_axes: Vec::new(),
            telemetry: Vec::new(),
            certifies: Vec::new(),
            cacheable: true,
            adapter: BuildTaskAdapter::RustArtifacts {
                source: entry,
                output_base: task_root.join(&package_name),
                materialized_output_base: None,
                include_spirv: true,
            },
        });
    } else {
        let target_values = options
            .target_overrides
            .clone()
            .unwrap_or_else(|| build.targets.clone());
        if target_values.is_empty() {
            return Err(BuildError::Config("No build targets specified".to_string()));
        }
        for target_value in target_values {
            let target = kain_driver::parse_compile_target(&target_value)
                .ok_or_else(|| BuildError::Config(format!("Unknown target: {target_value}")))?;
            let task_root = config.task_root(&package_name, target_value.as_str());
            let primary_output = task_root.join(format!(
                "{}.{}",
                package_name,
                kain_driver::target_extension(target)
            ));
            let mut outputs = kain_compile_expected_outputs(target, &primary_output, None);
            outputs.push(artifact_manifest_path(&task_root));
            tasks.push(BuildTask {
                id: format!("kain-compile:{package_name}:{}", sanitize_id(&target_value)),
                kind: BuildTaskKind::KainCompile,
                blade: None,
                description: format!("Compile project target {target_value}"),
                depends_on: Vec::new(),
                inputs: authority_inputs.clone(),
                outputs,
                required_capabilities: Vec::new(),
                matrix_axes: Vec::new(),
                telemetry: Vec::new(),
                certifies: Vec::new(),
                cacheable: true,
                adapter: BuildTaskAdapter::KainCompile {
                    source: entry.clone(),
                    target,
                    emit: NativeEmit::default(),
                    primary_output,
                    materialized_primary_output: None,
                    root_component: None,
                },
            });
        }
    }
    if !options.rust_only {
        let explicit_config = config.as_workspace_config();
        for task in select_explicit_build_task_sections(&workspace_root, &manifest.build.tasks)? {
            tasks.push(build_explicit_task(
                &explicit_config,
                None,
                &workspace_root,
                &task,
            )?);
        }
    }
    let tasks = order_tasks(tasks)?;
    let build_graph = discover_build_graph_provenance(
        &workspace_root,
        manifest_path.is_file().then_some(manifest_path.as_path()),
    )?;
    let mut plan = config.into_plan("project", tasks);
    plan.build_graph = build_graph;
    validate_plan_safety(&plan)?;
    Ok(plan)
}

pub fn build_kain_project(options: &KainProjectBuildOptions) -> BuildResult<BladeBuildReport> {
    let plan = plan_kain_project(options)?;
    execute_plan(plan, &BuildExecutionOptions::from(options))
}

struct BuildExecutionOptions {
    dry_run: bool,
    clean: bool,
    fail_fast: bool,
    progress: Option<ToolingProgressSink>,
}

impl From<&BladeBuildOptions> for BuildExecutionOptions {
    fn from(options: &BladeBuildOptions) -> Self {
        Self {
            dry_run: options.dry_run,
            clean: options.clean,
            fail_fast: options.fail_fast,
            progress: options.progress.clone(),
        }
    }
}

impl From<&KainFileBuildOptions> for BuildExecutionOptions {
    fn from(options: &KainFileBuildOptions) -> Self {
        Self {
            dry_run: options.dry_run,
            clean: options.clean,
            fail_fast: options.fail_fast,
            progress: options.progress.clone(),
        }
    }
}

impl From<&KainRustBuildOptions> for BuildExecutionOptions {
    fn from(options: &KainRustBuildOptions) -> Self {
        Self {
            dry_run: options.dry_run,
            clean: options.clean,
            fail_fast: options.fail_fast,
            progress: options.progress.clone(),
        }
    }
}

impl From<&KainNativeUiBuildOptions> for BuildExecutionOptions {
    fn from(options: &KainNativeUiBuildOptions) -> Self {
        Self {
            dry_run: options.dry_run,
            clean: options.clean,
            fail_fast: options.fail_fast,
            progress: options.progress.clone(),
        }
    }
}

impl From<&KainProjectBuildOptions> for BuildExecutionOptions {
    fn from(options: &KainProjectBuildOptions) -> Self {
        Self {
            dry_run: options.dry_run,
            clean: options.clean,
            fail_fast: options.fail_fast,
            progress: options.progress.clone(),
        }
    }
}

struct BuildWorkspaceConfig {
    workspace_root: PathBuf,
    artifact_root: PathBuf,
    cache_root: PathBuf,
    report_root: PathBuf,
    host: String,
    lane: BuildLane,
    profile: String,
    target: String,
}

struct StandaloneBuildConfig {
    workspace_root: PathBuf,
    artifact_root: PathBuf,
    cache_root: PathBuf,
    report_root: PathBuf,
    host: String,
    lane: BuildLane,
    profile: String,
    target: String,
}

impl StandaloneBuildConfig {
    fn new(
        workspace_root: PathBuf,
        profile: Option<String>,
        lane: Option<BuildLane>,
        target: Option<String>,
    ) -> Self {
        let lane = lane
            .or_else(|| profile.as_deref().and_then(BuildLane::parse))
            .unwrap_or_default();
        let profile = profile
            .unwrap_or_else(|| lane.cargo_profile().to_string())
            .trim()
            .to_string();
        let profile = if profile.is_empty() {
            DEFAULT_PROFILE.to_string()
        } else {
            profile
        };
        Self {
            artifact_root: workspace_root.join(DEFAULT_ARTIFACT_ROOT),
            cache_root: workspace_root.join(DEFAULT_CACHE_ROOT),
            report_root: workspace_root.join(DEFAULT_REPORT_ROOT),
            host: default_target_name(),
            lane,
            profile,
            target: target.unwrap_or_else(default_target_name),
            workspace_root,
        }
    }

    fn task_root(&self, unit: &str, task_id: &str) -> PathBuf {
        self.artifact_root
            .join(&self.host)
            .join(self.lane.as_str())
            .join(&self.target)
            .join(sanitize_id(unit))
            .join(sanitize_id(task_id))
    }

    fn as_workspace_config(&self) -> BuildWorkspaceConfig {
        BuildWorkspaceConfig {
            workspace_root: self.workspace_root.clone(),
            artifact_root: self.artifact_root.clone(),
            cache_root: self.cache_root.clone(),
            report_root: self.report_root.clone(),
            host: self.host.clone(),
            lane: self.lane,
            profile: self.profile.clone(),
            target: self.target.clone(),
        }
    }

    fn into_plan(self, target_label: &str, tasks: Vec<BuildTask>) -> BladeBuildPlan {
        BladeBuildPlan {
            schema_version: BUILD_ARTIFACT_SCHEMA_VERSION,
            workspace_root: self.workspace_root,
            artifact_root: self.artifact_root,
            cache_root: self.cache_root,
            report_root: self.report_root,
            host: self.host,
            lane: self.lane,
            profile: self.profile,
            target: target_label.to_string(),
            build_graph: None,
            tasks,
        }
    }
}

impl BuildWorkspaceConfig {
    fn from_workspace(
        workspace: &BladeWorkspace,
        manifest: Option<&KainManifest>,
        options: &BladeBuildOptions,
    ) -> Self {
        let workspace_root = workspace.root.clone();
        let lane = options
            .lane
            .or_else(|| options.profile.as_deref().and_then(BuildLane::parse))
            .or_else(|| {
                manifest
                    .and_then(|value| value.build.profile.as_deref())
                    .and_then(BuildLane::parse)
            })
            .unwrap_or_default();
        let profile = options
            .profile
            .clone()
            .or_else(|| manifest.and_then(|value| value.build.profile.clone()))
            .unwrap_or_else(|| lane.cargo_profile().to_string())
            .trim()
            .to_string();
        let profile = if profile.is_empty() {
            DEFAULT_PROFILE.to_string()
        } else {
            profile
        };
        let host = default_target_name();
        let target = options.target.clone().unwrap_or_else(default_target_name);
        let artifact_root = resolve_workspace_path(
            &workspace_root,
            manifest
                .and_then(|value| value.build.artifact_root.as_ref())
                .map(PathBuf::as_path)
                .unwrap_or_else(|| Path::new(DEFAULT_ARTIFACT_ROOT)),
        );
        let cache_root = resolve_workspace_path(
            &workspace_root,
            manifest
                .and_then(|value| value.build.cache_root.as_ref())
                .map(PathBuf::as_path)
                .unwrap_or_else(|| Path::new(DEFAULT_CACHE_ROOT)),
        );
        let report_root = workspace_root.join(DEFAULT_REPORT_ROOT);
        Self {
            workspace_root,
            artifact_root,
            cache_root,
            report_root,
            host,
            lane,
            profile,
            target,
        }
    }

    fn task_root(&self, blade_name: &str, task_id: &str) -> PathBuf {
        self.artifact_root
            .join(&self.host)
            .join(self.lane.as_str())
            .join(&self.target)
            .join(sanitize_id(blade_name))
            .join(sanitize_id(task_id))
    }
}

fn discover_build_graph_provenance(
    workspace_root: &Path,
    manifest_path: Option<&Path>,
) -> BuildResult<Option<KainBuildGraphProvenance>> {
    let manifest_path = manifest_path
        .filter(|path| path.exists())
        .map(Path::to_path_buf);
    let manifest_source = if let Some(path) = &manifest_path {
        Some(kfs::read_text(path)?)
    } else {
        None
    };
    let manifest_platform_packages = manifest_source
        .as_deref()
        .map(extract_manifest_build_graph_platform_packages)
        .unwrap_or_default();
    let manifest_explicit_tasks = manifest_source
        .as_deref()
        .map(extract_manifest_build_graph_explicit_tasks)
        .unwrap_or_default();
    let script = discover_build_graph_script(workspace_root)?;

    if script.is_none() && manifest_path.is_none() {
        return Ok(None);
    }

    let Some(script) = script else {
        return Ok(Some(KainBuildGraphProvenance {
            graph_source: "KAIN.toml".to_string(),
            defaults_merged_from: None,
            build_script: None,
            overrides: Vec::new(),
            platform_packages: manifest_platform_packages,
        }));
    };

    let graph_source = script.graph_source.clone();
    let build_script = script.build_script.clone();
    let evaluator_error = script.evaluator_error.clone();
    let platform_packages = script.platform_packages;
    let script_explicit_tasks = script.explicit_tasks;
    let mut overrides = Vec::new();
    if let Some(error) = evaluator_error {
        overrides.push(format!(
            "{graph_source} fell back to scanner because the evaluator could not lower it: {error}"
        ));
    }
    if manifest_path.is_some() {
        overrides.push(format!(
            "{graph_source} is build graph authority; KAIN.toml contributes defaults"
        ));
    }
    if !manifest_platform_packages.is_empty() {
        let manifest_pairs = platform_package_pairs(&manifest_platform_packages);
        let script_pairs = platform_package_pairs(&platform_packages);
        if manifest_pairs != script_pairs {
            overrides.push(format!(
                "{graph_source} overrides KAIN.toml platform packages: script={script_pairs:?}, manifest={manifest_pairs:?}"
            ));
        }
    }
    if manifest_path.is_some() && !manifest_explicit_tasks.is_empty() {
        let manifest_signatures = explicit_build_task_signatures(&manifest_explicit_tasks);
        let script_signatures = explicit_build_task_signatures(&script_explicit_tasks);
        if script_explicit_tasks.is_empty() {
            overrides.push(format!(
                "{graph_source} defers explicit build tasks to KAIN.toml because no build_task(...) declarations were found"
            ));
        } else if manifest_signatures != script_signatures {
            overrides.push(format!(
                "{graph_source} overrides KAIN.toml explicit build tasks: script={script_signatures:?}, manifest={manifest_signatures:?}"
            ));
        }
    }

    Ok(Some(KainBuildGraphProvenance {
        graph_source,
        defaults_merged_from: manifest_path,
        build_script: Some(build_script),
        overrides,
        platform_packages,
    }))
}

fn extract_manifest_build_graph_platform_packages(
    source: &str,
) -> Vec<KainBuildGraphPlatformPackage> {
    let Ok(value) = source.parse::<toml::Value>() else {
        return Vec::new();
    };
    let Some(packages) = value
        .get("platform")
        .and_then(|platform| platform.get("packages"))
        .and_then(toml::Value::as_array)
    else {
        return Vec::new();
    };

    let mut output = Vec::new();
    for package in packages {
        let Some(table) = package.as_table() else {
            continue;
        };
        let package_name = table
            .get("package")
            .or_else(|| table.get("name"))
            .and_then(toml::Value::as_str)
            .map(str::trim)
            .filter(|value| !value.is_empty());
        let Some(package_name) = package_name else {
            continue;
        };
        let provider = table
            .get("provider")
            .and_then(toml::Value::as_str)
            .map(str::trim)
            .filter(|value| !value.is_empty())
            .unwrap_or("system");
        output.push(KainBuildGraphPlatformPackage {
            package: package_name.to_string(),
            provider: provider.to_string(),
            source: "KAIN.toml".to_string(),
        });
    }
    sort_build_graph_platform_packages(&mut output);
    output
}

fn extract_manifest_build_graph_explicit_tasks(source: &str) -> Vec<KainBuildTaskSection> {
    toml::from_str::<KainManifest>(source)
        .map(|manifest| manifest.build.tasks)
        .unwrap_or_default()
}

fn discover_build_graph_script(
    workspace_root: &Path,
) -> BuildResult<Option<DiscoveredBuildGraphScript>> {
    let Some(build_script) = KAIN_BUILD_SCRIPT_NAMES
        .iter()
        .map(|name| workspace_root.join(name))
        .find(|path| path.exists())
    else {
        return Ok(None);
    };
    let graph_source = build_script
        .file_name()
        .and_then(OsStr::to_str)
        .unwrap_or("build.kn")
        .to_string();
    let source = kfs::read_text(&build_script)?;
    let evaluated_graph_source = format!("{graph_source}:evaluated");
    match crate::evaluated_build::evaluate_build_script(
        &source,
        workspace_root,
        &evaluated_graph_source,
    ) {
        Ok(evaluated) => Ok(Some(DiscoveredBuildGraphScript {
            platform_packages: evaluated.platform_packages,
            explicit_tasks: evaluated.explicit_tasks,
            evaluated_manifest: Some(evaluated.manifest),
            evaluator_error: None,
            graph_source: evaluated_graph_source,
            build_script,
        })),
        Err(error) => Ok(Some(DiscoveredBuildGraphScript {
            platform_packages: extract_build_graph_platform_packages(&source, &graph_source),
            explicit_tasks: extract_build_graph_explicit_tasks(&source),
            evaluated_manifest: None,
            evaluator_error: Some(compact_evaluator_error(&error.to_string())),
            graph_source,
            build_script,
        })),
    }
}

fn compact_evaluator_error(message: &str) -> String {
    let mut compact = message.split_whitespace().collect::<Vec<_>>().join(" ");
    const MAX_LEN: usize = 240;
    if compact.len() > MAX_LEN {
        compact.truncate(MAX_LEN);
        compact.push_str("...");
    }
    compact
}

fn discover_evaluated_build_graph_manifest(
    workspace_root: &Path,
) -> BuildResult<Option<KainManifest>> {
    Ok(discover_build_graph_script(workspace_root)?.and_then(|script| script.evaluated_manifest))
}

fn merge_build_graph_manifest(mut base: KainManifest, overlay: KainManifest) -> KainManifest {
    merge_optional(&mut base.package.name, overlay.package.name);
    merge_optional(&mut base.package.version, overlay.package.version);
    merge_optional(&mut base.package.description, overlay.package.description);
    merge_paths(&mut base.workspace.blades, overlay.workspace.blades);
    merge_paths(
        &mut base.workspace.blade_roots,
        overlay.workspace.blade_roots,
    );
    merge_paths(&mut base.workspace.members, overlay.workspace.members);
    merge_paths(
        &mut base.workspace.search_roots,
        overlay.workspace.search_roots,
    );
    merge_optional(
        &mut base.workspace.stdlib_root,
        overlay.workspace.stdlib_root,
    );
    merge_optional(
        &mut base.workspace.manifest_root,
        overlay.workspace.manifest_root,
    );
    merge_optional(
        &mut base.workspace.generated_root,
        overlay.workspace.generated_root,
    );
    merge_optional(&mut base.build.entry, overlay.build.entry);
    merge_optional(&mut base.build.entry_module, overlay.build.entry_module);
    merge_optional(&mut base.build.source_root, overlay.build.source_root);
    merge_paths(&mut base.build.source_order, overlay.build.source_order);
    merge_paths(&mut base.build.module_roots, overlay.build.module_roots);
    merge_paths(
        &mut base.build.module_search_paths,
        overlay.build.module_search_paths,
    );
    merge_strings(&mut base.build.targets, overlay.build.targets);
    merge_optional(&mut base.build.artifact_root, overlay.build.artifact_root);
    merge_optional(&mut base.build.cache_root, overlay.build.cache_root);
    merge_optional(&mut base.build.profile, overlay.build.profile);
    merge_tasks(&mut base.build.tasks, overlay.build.tasks);
    merge_optional(&mut base.run.entry, overlay.run.entry);
    merge_optional(&mut base.run.blade, overlay.run.blade);
    merge_optional(&mut base.run.target, overlay.run.target);
    merge_strings(&mut base.run.args, overlay.run.args);
    if !overlay.run.env.is_empty() {
        base.run.env = overlay.run.env;
    }
    merge_optional(&mut base.run.cwd, overlay.run.cwd);
    merge_paths(&mut base.run.watch, overlay.run.watch);
    merge_optional(&mut base.blade.name, overlay.blade.name);
    merge_optional(&mut base.blade.version, overlay.blade.version);
    merge_optional(&mut base.blade.kind, overlay.blade.kind);
    merge_optional(&mut base.blade.entry, overlay.blade.entry);
    merge_paths(&mut base.blade.source_roots, overlay.blade.source_roots);
    merge_paths(&mut base.blade.module_roots, overlay.blade.module_roots);
    merge_strings(&mut base.blade.build_targets, overlay.blade.build_targets);
    base
}

fn merge_optional<T>(slot: &mut Option<T>, overlay: Option<T>) {
    if overlay.is_some() {
        *slot = overlay;
    }
}

fn merge_strings(slot: &mut Vec<String>, overlay: Vec<String>) {
    if !overlay.is_empty() {
        *slot = overlay;
    }
}

fn merge_paths(slot: &mut Vec<PathBuf>, overlay: Vec<PathBuf>) {
    if !overlay.is_empty() {
        *slot = overlay;
    }
}

fn merge_tasks(slot: &mut Vec<KainBuildTaskSection>, overlay: Vec<KainBuildTaskSection>) {
    if !overlay.is_empty() {
        *slot = overlay;
    }
}

fn blade_authority_inputs(blade: &ResolvedBlade) -> Vec<PathBuf> {
    let mut inputs = Vec::new();
    if let Some(path) = &blade.manifest_path {
        inputs.push(path.clone());
    }
    if let Some(path) = find_build_script_in(&blade.root) {
        if !inputs.iter().any(|existing| existing == &path) {
            inputs.push(path);
        }
    }
    inputs
}

fn extract_build_graph_platform_packages(
    source: &str,
    graph_source: &str,
) -> Vec<KainBuildGraphPlatformPackage> {
    let mut packages = Vec::new();
    for constructor in PLATFORM_PACKAGE_CONSTRUCTORS {
        for (args, methods) in scan_string_call_chains(source, constructor) {
            let Some(package) = args.first() else {
                continue;
            };
            let provider = methods
                .iter()
                .find_map(|(method, values, _)| {
                    (method == "provider")
                        .then(|| values.first().cloned())
                        .flatten()
                })
                .unwrap_or_else(|| "system".to_string());
            packages.push(KainBuildGraphPlatformPackage {
                package: package.clone(),
                provider,
                source: graph_source.to_string(),
            });
        }
    }
    sort_build_graph_platform_packages(&mut packages);
    packages
}

const PLATFORM_PACKAGE_CONSTRUCTORS: &[&str] = &[
    "platform_package",
    "build_platform_package",
    "platform_requirement",
    "requires_platform_package",
];

const BUILD_TASK_CONSTRUCTORS: &[(&str, Option<&str>)] = &[
    ("build_task", None),
    ("build_check", Some("check")),
    ("check_task", Some("check")),
    ("exec_task", Some("exec")),
    ("command_task", Some("exec")),
    ("amalgamate_capsule", Some("amalgamate")),
    ("capsule_task", Some("amalgamate")),
    ("native_executable", Some("native-executable")),
    ("root_executable", Some("native-executable")),
    ("build_native_executable", Some("native-executable")),
    ("test_task", Some("test")),
    ("test_suite", Some("test")),
    ("proof_task", Some("proof")),
    ("proof_obligation", Some("proof")),
    ("z3_proof", Some("proof")),
    ("bench_task", Some("benchmark")),
    ("bench_case", Some("benchmark")),
    ("benchmark_task", Some("benchmark")),
    ("attrition_task", Some("attrition")),
    ("attrition_case", Some("attrition")),
    ("certify_task", Some("certify")),
    ("certify_gate", Some("certify")),
    ("release_gate", Some("certify")),
];

fn extract_build_graph_explicit_tasks(source: &str) -> Vec<KainBuildTaskSection> {
    let mut tasks = Vec::new();
    for (constructor, default_kind) in BUILD_TASK_CONSTRUCTORS {
        for (args, methods) in scan_string_call_chains(source, constructor) {
            let Some(id) = args.first() else {
                continue;
            };
            let mut task = KainBuildTaskSection {
                id: id.clone(),
                kind: default_kind.unwrap_or_default().to_string(),
                ..KainBuildTaskSection::default()
            };
            for (method, values, _) in methods {
                match method.as_str() {
                    "kind" => assign_first_string(&values, &mut task.kind),
                    "blade" => assign_first_optional_string(&values, &mut task.blade),
                    "entry" | "source" | "path" => {
                        assign_first_optional_path(&values, &mut task.entry)
                    }
                    "manifest" => assign_first_optional_path(&values, &mut task.manifest),
                    "command" => assign_first_optional_string(&values, &mut task.command),
                    "arg" | "args" => task.args.extend(values),
                    "cwd" => assign_first_optional_path(&values, &mut task.cwd),
                    "target" => assign_first_optional_string(&values, &mut task.target),
                    "profile" => assign_first_optional_string(&values, &mut task.profile),
                    "input" | "inputs" => {
                        task.inputs.extend(values.into_iter().map(PathBuf::from));
                    }
                    "output" | "outputs" | "root_output" | "blade_output" | "artifact" => {
                        task.outputs.extend(values.into_iter().map(PathBuf::from));
                    }
                    "depends_on" | "depends" | "dependency" | "requires" | "requires_task" => {
                        task.depends_on.extend(values);
                    }
                    "requires_capability" | "when_capability" | "capability" => {
                        task.required_capabilities.extend(values);
                    }
                    "axis" | "matrix_axis" | "matrix_value" | "matrix" => task
                        .matrix_axes
                        .extend(canonical_matrix_axis_values(values)),
                    "telemetry" | "telemetry_channel" => task.telemetry.extend(values),
                    "certifies" | "certificate" => task.certifies.extend(values),
                    "env" => insert_task_pair(&values, &mut task.env),
                    "meta" => insert_task_pair(&values, &mut task.meta),
                    "option" => insert_task_pair(&values, &mut task.options),
                    "tag" => task.tags.extend(values),
                    "note" => task.notes.extend(values),
                    "author" => task.authors.extend(values),
                    "name" | "version" | "storage" | "contents" | "capsule_set" | "header"
                    | "compression" | "preview_symbols" | "api_index" | "module_index"
                    | "timeout_ms" | "stdout" | "stderr" => {
                        if let Some(value) = values.first() {
                            task.options.insert(method.clone(), value.clone());
                        }
                    }
                    "archive" => {
                        let enabled = values
                            .first()
                            .map_or(true, |value| parse_bool_string(value));
                        task.options.insert(
                            "storage".to_string(),
                            if enabled { "archive" } else { "editable" }.to_string(),
                        );
                    }
                    "editable" => {
                        task.options
                            .insert("storage".to_string(), "editable".to_string());
                    }
                    "always_run" => {
                        task.options
                            .insert("always_run".to_string(), "true".to_string());
                    }
                    "proof_mode" | "mode" => task.args.extend(values),
                    _ => {}
                }
            }
            tasks.push(task);
        }
    }
    tasks
}

fn assign_first_string(values: &[String], slot: &mut String) {
    if let Some(value) = values.first() {
        *slot = value.clone();
    }
}

fn sort_build_graph_platform_packages(packages: &mut Vec<KainBuildGraphPlatformPackage>) {
    packages.sort_by(|left, right| {
        left.package
            .cmp(&right.package)
            .then(left.provider.cmp(&right.provider))
            .then(left.source.cmp(&right.source))
    });
    packages.dedup_by(|left, right| {
        left.package == right.package
            && left.provider == right.provider
            && left.source == right.source
    });
}

fn platform_package_pairs(packages: &[KainBuildGraphPlatformPackage]) -> Vec<(String, String)> {
    packages
        .iter()
        .map(|package| (package.package.clone(), package.provider.clone()))
        .collect()
}

fn explicit_build_task_signatures(tasks: &[KainBuildTaskSection]) -> Vec<(String, String)> {
    let mut signatures = tasks
        .iter()
        .map(|task| {
            (
                sanitize_build_task_reference(&task.id),
                task.kind.trim().to_ascii_lowercase(),
            )
        })
        .collect::<Vec<_>>();
    signatures.sort();
    signatures
}

fn assign_first_optional_string(values: &[String], slot: &mut Option<String>) {
    if let Some(value) = values.first() {
        *slot = Some(value.clone());
    }
}

fn assign_first_optional_path(values: &[String], slot: &mut Option<PathBuf>) {
    if let Some(value) = values.first() {
        *slot = Some(PathBuf::from(value));
    }
}

fn canonical_matrix_axis_values(values: Vec<String>) -> Vec<String> {
    if values.len() == 2 {
        vec![format!("{}={}", values[0], values[1])]
    } else {
        values
    }
}

fn parse_bool_string(value: &str) -> bool {
    matches!(
        value.trim().to_ascii_lowercase().as_str(),
        "1" | "true" | "yes" | "y" | "on"
    )
}

fn parse_bool_env_value(name: &str, value: &str) -> BuildResult<bool> {
    match value.trim().to_ascii_lowercase().as_str() {
        "1" | "true" | "yes" | "on" => Ok(true),
        "0" | "false" | "no" | "off" => Ok(false),
        _ => Err(BuildError::Config(format!(
            "{name} must be one of 1, 0, true, false, yes, no, on, off"
        ))),
    }
}

fn native_llvm_ir_slicing_enabled() -> BuildResult<bool> {
    match std::env::var("KAIN_NATIVE_LLVM_IR_SLICING") {
        Ok(value) => parse_bool_env_value("KAIN_NATIVE_LLVM_IR_SLICING", &value),
        Err(_) => Ok(true),
    }
}

fn insert_task_pair(values: &[String], slot: &mut BTreeMap<String, String>) {
    if values.len() >= 2 {
        slot.insert(values[0].clone(), values[1].clone());
    }
}

fn scan_string_call_chains(
    source: &str,
    function_name: &str,
) -> Vec<(Vec<String>, Vec<(String, Vec<String>, usize)>)> {
    let mut matches = Vec::new();
    let mut offset = 0usize;
    while let Some(call_start) = find_function_call(source, function_name, offset) {
        let function_end = call_start + function_name.len();
        if let Some((args, after_call)) = parse_string_call_arguments(source, function_end) {
            let methods = parse_string_method_chain(source, after_call);
            let next_offset = methods
                .last()
                .map(|(_, _, after)| *after)
                .unwrap_or(after_call);
            matches.push((args, methods));
            offset = next_offset;
        } else {
            offset = function_end;
        }
    }
    matches
}

fn find_function_call(source: &str, function_name: &str, mut offset: usize) -> Option<usize> {
    while let Some(relative) = source[offset..].find(function_name) {
        let start = offset + relative;
        let before = start
            .checked_sub(1)
            .and_then(|index| source.as_bytes().get(index).copied());
        let after = source.as_bytes().get(start + function_name.len()).copied();
        if !matches!(before, Some(byte) if is_identifier_byte(byte))
            && matches!(after, Some(b'(' | b' ' | b'\n' | b'\r' | b'\t'))
        {
            return Some(start);
        }
        offset = start + function_name.len();
    }
    None
}

fn is_identifier_byte(byte: u8) -> bool {
    matches!(byte, b'a'..=b'z' | b'A'..=b'Z' | b'0'..=b'9' | b'_')
}

fn is_unquoted_literal_start(byte: u8) -> bool {
    byte.is_ascii_digit() || byte == b'-' || byte == b't' || byte == b'f'
}

fn parse_unquoted_literal(source: &str, mut index: usize) -> Option<(String, usize)> {
    let bytes = source.as_bytes();
    let start = index;
    while let Some(byte) = bytes.get(index).copied() {
        if matches!(byte, b',' | b')' | b' ' | b'\n' | b'\r' | b'\t') {
            break;
        }
        index += 1;
    }
    let literal = source.get(start..index)?.trim().to_string();
    if !is_supported_unquoted_literal(&literal) {
        return None;
    }
    Some((literal, index))
}

fn is_supported_unquoted_literal(value: &str) -> bool {
    if matches!(value.trim().to_ascii_lowercase().as_str(), "true" | "false") {
        return true;
    }
    let digits = value.strip_prefix('-').unwrap_or(value);
    !digits.is_empty() && digits.bytes().all(|byte| byte.is_ascii_digit())
}

fn parse_string_method_chain(source: &str, mut index: usize) -> Vec<(String, Vec<String>, usize)> {
    let bytes = source.as_bytes();
    let mut methods = Vec::new();
    loop {
        index = skip_ascii_whitespace(bytes, index);
        if bytes.get(index).copied() != Some(b'.') {
            break;
        }
        index += 1;
        let method_start = index;
        while matches!(bytes.get(index).copied(), Some(byte) if is_identifier_byte(byte)) {
            index += 1;
        }
        if method_start == index {
            break;
        }
        let Some(method) = source.get(method_start..index) else {
            break;
        };
        let Some((args, after_call)) = parse_string_call_arguments(source, index) else {
            break;
        };
        methods.push((method.to_string(), args, after_call));
        index = after_call;
    }
    methods
}

fn parse_string_call_arguments(source: &str, mut index: usize) -> Option<(Vec<String>, usize)> {
    let bytes = source.as_bytes();
    index = skip_ascii_whitespace(bytes, index);
    if bytes.get(index).copied()? != b'(' {
        return None;
    }
    index += 1;
    let mut values = Vec::new();
    loop {
        index = skip_ascii_whitespace(bytes, index);
        match bytes.get(index).copied()? {
            b')' => return Some((values, index + 1)),
            b'"' => {
                let (value, after_string) = parse_quoted_string(source, index)?;
                values.push(value);
                index = skip_ascii_whitespace(bytes, after_string);
                match bytes.get(index).copied()? {
                    b',' => {
                        index += 1;
                    }
                    b')' => return Some((values, index + 1)),
                    _ => return None,
                }
            }
            byte if is_unquoted_literal_start(byte) => {
                let (value, after_literal) = parse_unquoted_literal(source, index)?;
                values.push(value);
                index = skip_ascii_whitespace(bytes, after_literal);
                match bytes.get(index).copied()? {
                    b',' => {
                        index += 1;
                    }
                    b')' => return Some((values, index + 1)),
                    _ => return None,
                }
            }
            _ => return None,
        }
    }
}

fn select_explicit_build_task_sections(
    root: &Path,
    manifest_tasks: &[KainBuildTaskSection],
) -> BuildResult<Vec<KainBuildTaskSection>> {
    let Some(script) = discover_build_graph_script(root)? else {
        return Ok(manifest_tasks.to_vec());
    };
    if script.explicit_tasks.is_empty() {
        Ok(manifest_tasks.to_vec())
    } else {
        Ok(script.explicit_tasks)
    }
}

fn parse_quoted_string(source: &str, mut index: usize) -> Option<(String, usize)> {
    let bytes = source.as_bytes();
    if bytes.get(index).copied()? != b'"' {
        return None;
    }
    index += 1;
    let mut value = String::new();
    while index < bytes.len() {
        match bytes[index] {
            b'\\' => {
                let escaped = *bytes.get(index + 1)?;
                value.push(match escaped {
                    b'n' => '\n',
                    b'r' => '\r',
                    b't' => '\t',
                    b'"' => '"',
                    b'\\' => '\\',
                    other => other as char,
                });
                index += 2;
            }
            b'"' => return Some((value, index + 1)),
            byte => {
                value.push(byte as char);
                index += 1;
            }
        }
    }
    None
}

fn skip_ascii_whitespace(bytes: &[u8], mut index: usize) -> usize {
    while matches!(bytes.get(index), Some(b' ' | b'\n' | b'\r' | b'\t')) {
        index += 1;
    }
    index
}

fn add_c_tasks(
    tasks: &mut Vec<BuildTask>,
    sidecar_task_ids: &mut Vec<String>,
    config: &BuildWorkspaceConfig,
    blade: &ResolvedBlade,
) -> BuildResult<()> {
    for library in &blade.c_ffi_libraries {
        let Some(materialized_output) = &library.shared_lib else {
            continue;
        };
        let task_id = format!(
            "c:{}:{}",
            sanitize_id(&blade.name),
            sanitize_id(&library.name)
        );
        let task_root = config.task_root(&blade.name, &task_id);
        let canonical_output =
            task_root.join(platform_dynamic_library_name(&library.name).as_str());
        let sources = resolve_c_sources(library);
        let mut inputs = vec![library.header.clone()];
        inputs.extend(sources.iter().cloned());
        inputs.extend(library.include_paths.iter().cloned());
        let outputs = vec![canonical_output.clone(), materialized_output.clone()];
        tasks.push(BuildTask {
            id: task_id.clone(),
            kind: BuildTaskKind::CSharedLibrary,
            blade: Some(blade.name.clone()),
            description: format!("Build C ABI shared library {}", library.name),
            depends_on: Vec::new(),
            inputs,
            outputs,
            required_capabilities: Vec::new(),
            matrix_axes: Vec::new(),
            telemetry: Vec::new(),
            certifies: Vec::new(),
            cacheable: true,
            adapter: BuildTaskAdapter::CSharedLibrary {
                library_name: library.name.clone(),
                sources,
                header: library.header.clone(),
                include_paths: library.include_paths.clone(),
                defines: library.defines.clone(),
                link_libs: library.link_libs.clone(),
                cpp_options: library.cpp_options.clone(),
                canonical_output,
                materialized_output: Some(materialized_output.clone()),
            },
        });
        sidecar_task_ids.push(task_id);
    }
    Ok(())
}

fn add_cargo_task(
    tasks: &mut Vec<BuildTask>,
    sidecar_task_ids: &mut Vec<String>,
    config: &BuildWorkspaceConfig,
    blade: &ResolvedBlade,
) -> BuildResult<()> {
    let Some(manifest_path) = &blade.cargo_manifest else {
        return Ok(());
    };
    let task_id = format!("cargo:{}", sanitize_id(&blade.name));
    let target_dir = config.task_root(&blade.name, &task_id).join("cargo-target");
    let crate_root = manifest_path.parent().unwrap_or(&blade.root).to_path_buf();
    let src_root = crate_root.join("src");
    let mut inputs = vec![manifest_path.clone()];
    if src_root.exists() {
        inputs.push(src_root);
    }
    tasks.push(BuildTask {
        id: task_id.clone(),
        kind: BuildTaskKind::CargoBuild,
        blade: Some(blade.name.clone()),
        description: format!("Build Cargo blade {}", blade.name),
        depends_on: Vec::new(),
        inputs,
        outputs: vec![target_dir.clone()],
        required_capabilities: Vec::new(),
        matrix_axes: Vec::new(),
        telemetry: Vec::new(),
        certifies: Vec::new(),
        cacheable: true,
        adapter: BuildTaskAdapter::CargoBuild {
            manifest_path: manifest_path.clone(),
            target_dir,
            release: config.profile == "release",
        },
    });
    sidecar_task_ids.push(task_id);
    Ok(())
}

fn add_gpu_tasks(tasks: &mut Vec<BuildTask>, config: &BuildWorkspaceConfig, blade: &ResolvedBlade) {
    for source in &blade.gpu_shader_sources {
        let task_id = format!(
            "gpu:{}:{}",
            sanitize_id(&blade.name),
            sanitize_id(&path_stem_or_name(source))
        );
        let output_base = config
            .task_root(&blade.name, &task_id)
            .join(path_stem_or_name(source));
        let target = GpuArtifactTarget::All;
        let no_derived = false;
        let outputs = gpu_output_paths(&output_base, target, no_derived);
        tasks.push(BuildTask {
            id: task_id,
            kind: BuildTaskKind::GpuArtifacts,
            blade: Some(blade.name.clone()),
            description: format!("Emit GPU artifacts for {}", source.display()),
            depends_on: Vec::new(),
            inputs: vec![source.clone()],
            outputs,
            required_capabilities: Vec::new(),
            matrix_axes: Vec::new(),
            telemetry: Vec::new(),
            certifies: Vec::new(),
            cacheable: true,
            adapter: BuildTaskAdapter::GpuArtifacts {
                source: source.clone(),
                output_base,
                target,
                no_residency: false,
                no_derived,
            },
        });
    }
}

fn add_explicit_blade_tasks(
    tasks: &mut Vec<BuildTask>,
    sidecar_task_ids: &mut Vec<String>,
    config: &BuildWorkspaceConfig,
    blade: &ResolvedBlade,
) -> BuildResult<()> {
    let Some(manifest) = load_effective_kain_manifest(&blade.root)? else {
        return Ok(());
    };
    for task in select_explicit_build_task_sections(&blade.root, &manifest.build.tasks)? {
        let resolved = build_explicit_task(config, Some(blade), &blade.root, &task)?;
        if matches!(
            resolved.kind,
            BuildTaskKind::CSharedLibrary | BuildTaskKind::CargoBuild
        ) {
            sidecar_task_ids.push(resolved.id.clone());
        }
        tasks.push(resolved);
    }
    Ok(())
}

fn add_explicit_root_tasks(
    tasks: &mut Vec<BuildTask>,
    config: &BuildWorkspaceConfig,
    manifest: &KainManifest,
) -> BuildResult<()> {
    for task in select_explicit_build_task_sections(&config.workspace_root, &manifest.build.tasks)?
    {
        tasks.push(build_explicit_task(
            config,
            None,
            &config.workspace_root,
            &task,
        )?);
    }
    Ok(())
}

fn build_explicit_task(
    config: &BuildWorkspaceConfig,
    blade: Option<&ResolvedBlade>,
    root: &Path,
    task: &KainBuildTaskSection,
) -> BuildResult<BuildTask> {
    if task.id.trim().is_empty() {
        return Err(BuildError::Config(
            "explicit build task requires a non-empty id".to_string(),
        ));
    }
    let kind = match task.kind.trim().to_ascii_lowercase().as_str() {
        "kain" | "kain-check" | "check" => BuildTaskKind::KainCheck,
        "native-executable" | "root-executable" | "executable" | "exe" => {
            BuildTaskKind::NativeExecutable
        }
        "test" | "kain-test" | "std-test" | "std::test" => BuildTaskKind::Test,
        "proof" | "prove" | "z3" | "smt" => BuildTaskKind::Proof,
        "benchmark" | "bench" => BuildTaskKind::Benchmark,
        "attrition" | "abuse" => BuildTaskKind::Attrition,
        "certify" | "certificate" | "evidence" => BuildTaskKind::Certify,
        "cargo" | "rust" | "rust-crate" => BuildTaskKind::CargoBuild,
        "c" | "c-shared-library" | "c_ffi" => BuildTaskKind::CSharedLibrary,
        "gpu" | "gpu-artifacts" => BuildTaskKind::GpuArtifacts,
        "fabric" | "fabric-run" => BuildTaskKind::FabricRun,
        "fabric-validate" => BuildTaskKind::FabricValidate,
        "exec" | "command" => BuildTaskKind::Exec,
        "amalgamate" | "capsule" => BuildTaskKind::Amalgamate,
        "node" => BuildTaskKind::Node,
        "bun" => BuildTaskKind::Bun,
        other => {
            return Err(BuildError::Config(format!(
                "explicit build task '{}' has unsupported kind '{}'",
                task.id, other
            )));
        }
    };
    let task_id = build_explicit_task_id(task.id.trim(), blade);
    let task_root = config.task_root(
        blade
            .map(|value| value.name.as_str())
            .unwrap_or("workspace"),
        &task_id,
    );
    let mut inputs: Vec<PathBuf> = task
        .inputs
        .iter()
        .map(|path| resolve_build_graph_path(&config.workspace_root, root, &task_root, path))
        .collect();
    let requested_outputs = task
        .outputs
        .iter()
        .map(|path| resolve_build_graph_path(&config.workspace_root, root, &task_root, path))
        .collect::<Vec<_>>();
    let default_evidence_report = evidence_report_path(&task_root);
    let default_exec_report = exec_report_path(&task_root);
    let default_amalgamate_report = amalgamate_report_path(&task_root);
    let outputs = if task.outputs.is_empty() {
        match kind {
            kind if is_evidence_task_kind(kind) => vec![default_evidence_report.clone()],
            BuildTaskKind::Exec => vec![default_exec_report.clone()],
            BuildTaskKind::Amalgamate => {
                vec![task_root.join(format!("{}.kn", sanitize_id(&task.id)))]
            }
            _ => vec![task_root.clone()],
        }
    } else {
        requested_outputs.clone()
    };
    let adapter = match kind {
        BuildTaskKind::KainCheck => {
            let entry = task.entry.as_ref().ok_or_else(|| {
                BuildError::Config(format!("Kain build task '{}' requires entry", task.id))
            })?;
            let target = task
                .target
                .as_deref()
                .and_then(CompileTarget::from_str)
                .unwrap_or(CompileTarget::Interpret);
            BuildTaskAdapter::KainCheck {
                entry: resolve_build_graph_path(&config.workspace_root, root, &task_root, entry),
                target,
            }
        }
        BuildTaskKind::NativeExecutable => {
            let entry = task.entry.as_ref().ok_or_else(|| {
                BuildError::Config(format!(
                    "native executable task '{}' requires entry",
                    task.id
                ))
            })?;
            let entry = resolve_build_graph_path(&config.workspace_root, root, &task_root, entry);
            let output = requested_outputs
                .first()
                .cloned()
                .unwrap_or_else(|| root.join(default_executable_name(blade, root)));
            let script_path = find_lang_projects_compile_script(root);
            if !inputs.iter().any(|path| path == &script_path) {
                inputs.push(script_path.clone());
            }
            append_native_runtime_cache_inputs(&mut inputs, root)?;
            BuildTaskAdapter::NativeExecutable {
                entry,
                output,
                script_path,
                report_path: default_evidence_report.clone(),
                verify_llvm: !task.args.iter().any(|arg| arg == "--no-verify-llvm"),
            }
        }
        BuildTaskKind::Test | BuildTaskKind::Proof => {
            let entry = task.entry.as_ref().ok_or_else(|| {
                BuildError::Config(format!(
                    "{} task '{}' requires entry",
                    kind.as_str(),
                    task.id
                ))
            })?;
            let target = task
                .target
                .as_deref()
                .and_then(CompileTarget::from_str)
                .unwrap_or(CompileTarget::Interpret);
            let requested_mode = task
                .args
                .iter()
                .filter_map(|arg| kain_test::KainTestMode::parse(arg))
                .next();
            let mode_override = match kind {
                BuildTaskKind::Proof => {
                    Some(requested_mode.unwrap_or(kain_test::KainTestMode::ProvePass))
                }
                _ => requested_mode,
            };
            BuildTaskAdapter::KainTest {
                entry: resolve_build_graph_path(&config.workspace_root, root, &task_root, entry),
                target,
                mode_override,
                report_path: default_evidence_report.clone(),
                proof_required: kind == BuildTaskKind::Proof,
                fail_fast: task.args.iter().any(|arg| arg == "--fail-fast"),
                run_ignored: task
                    .args
                    .iter()
                    .any(|arg| arg == "--ignored" || arg == "--run-ignored"),
            }
        }
        BuildTaskKind::Benchmark | BuildTaskKind::Attrition => {
            let default_runner = match kind {
                BuildTaskKind::Benchmark => Path::new("benchmark").join("run.py"),
                BuildTaskKind::Attrition => Path::new("attrition").join("run.py"),
                _ => unreachable!(),
            };
            let entry = task
                .entry
                .as_ref()
                .map(|path| {
                    resolve_build_graph_path(&config.workspace_root, root, &task_root, path)
                })
                .unwrap_or_else(|| config.workspace_root.join(default_runner));
            let program = task.command.clone().unwrap_or_else(python_command);
            let mut args = vec![entry.display().to_string()];
            args.extend(task.args.clone());
            let cwd = task
                .cwd
                .as_ref()
                .map(|path| {
                    resolve_build_graph_path(&config.workspace_root, root, &task_root, path)
                })
                .unwrap_or_else(|| config.workspace_root.clone());
            BuildTaskAdapter::ExternalEvidence {
                label: kind.as_str().to_string(),
                program,
                args,
                cwd,
                report_path: default_evidence_report.clone(),
            }
        }
        BuildTaskKind::Certify => BuildTaskAdapter::Certify {
            report_path: default_evidence_report.clone(),
        },
        BuildTaskKind::CargoBuild => {
            let manifest_path =
                task.manifest
                    .as_ref()
                    .or(task.entry.as_ref())
                    .ok_or_else(|| {
                        BuildError::Config(format!(
                            "Cargo build task '{}' requires manifest",
                            task.id
                        ))
                    })?;
            BuildTaskAdapter::CargoBuild {
                manifest_path: resolve_build_graph_path(
                    &config.workspace_root,
                    root,
                    &task_root,
                    manifest_path,
                ),
                target_dir: outputs[0].clone(),
                release: task.profile.as_deref().unwrap_or(&config.profile) == "release",
            }
        }
        BuildTaskKind::CSharedLibrary => {
            let sources = task
                .inputs
                .iter()
                .filter(|path| path.extension().and_then(|value| value.to_str()) == Some("c"))
                .map(|path| {
                    resolve_build_graph_path(&config.workspace_root, root, &task_root, path)
                })
                .collect::<Vec<_>>();
            let header = task
                .entry
                .as_ref()
                .or_else(|| {
                    task.inputs
                        .iter()
                        .find(|path| path.extension().and_then(|value| value.to_str()) == Some("h"))
                })
                .map(|path| {
                    resolve_build_graph_path(&config.workspace_root, root, &task_root, path)
                })
                .unwrap_or_else(|| root.to_path_buf());
            let canonical_output = outputs.first().cloned().ok_or_else(|| {
                BuildError::Config(format!("C build task '{}' requires an output", task.id))
            })?;
            BuildTaskAdapter::CSharedLibrary {
                library_name: task.id.clone(),
                sources,
                header,
                include_paths: Vec::new(),
                defines: Vec::new(),
                link_libs: Vec::new(),
                cpp_options: task.args.clone(),
                canonical_output,
                materialized_output: None,
            }
        }
        BuildTaskKind::GpuArtifacts => {
            let source = task
                .entry
                .as_ref()
                .or_else(|| task.inputs.first())
                .ok_or_else(|| {
                    BuildError::Config(format!("GPU build task '{}' requires entry", task.id))
                })?;
            let source = resolve_build_graph_path(&config.workspace_root, root, &task_root, source);
            let output_base = outputs.first().cloned().unwrap_or_else(|| {
                config
                    .task_root("workspace", &task_id)
                    .join(path_stem_or_name(&source))
            });
            // Parse GPU artifact target from options.target (or task.target), default to "all"
            let target_str = task
                .options
                .get("target")
                .or_else(|| task.options.get("artifact_target"))
                .or_else(|| {
                    task.target.as_ref().filter(|_| {
                        !task.kind.contains("gpu") || task.options.contains_key("target")
                    })
                })
                .map(|s| s.as_str())
                .unwrap_or("all");
            let target = GpuArtifactTarget::from_arg(target_str);
            let no_residency = task
                .options
                .get("no_residency")
                .map(|s| s == "true" || s == "1" || s == "yes")
                .unwrap_or(false);
            let no_derived = task
                .options
                .get("no_derived")
                .map(|s| s == "true" || s == "1" || s == "yes")
                .unwrap_or(false);
            BuildTaskAdapter::GpuArtifacts {
                source,
                output_base,
                target,
                no_residency,
                no_derived,
            }
        }
        BuildTaskKind::FabricValidate | BuildTaskKind::FabricRun => {
            let manifest_path =
                task.manifest
                    .as_ref()
                    .or(task.entry.as_ref())
                    .ok_or_else(|| {
                        BuildError::Config(format!(
                            "Fabric build task '{}' requires manifest",
                            task.id
                        ))
                    })?;
            BuildTaskAdapter::Fabric {
                manifest_path: resolve_build_graph_path(
                    &config.workspace_root,
                    root,
                    &task_root,
                    manifest_path,
                ),
                run: kind == BuildTaskKind::FabricRun,
            }
        }
        BuildTaskKind::Exec => {
            let program = task.command.clone().ok_or_else(|| {
                BuildError::Config(format!("exec task '{}' requires command", task.id))
            })?;
            let stdout_path = task.options.get("stdout").map(|path| {
                resolve_build_graph_path(&config.workspace_root, root, &task_root, Path::new(path))
            });
            let stderr_path = task.options.get("stderr").map(|path| {
                resolve_build_graph_path(&config.workspace_root, root, &task_root, Path::new(path))
            });
            let timeout_ms = task
                .options
                .get("timeout_ms")
                .map(|value| parse_usize_option(&task.id, "timeout_ms", value))
                .transpose()?
                .map(|value| value as u64);
            BuildTaskAdapter::Exec {
                label: task.id.clone(),
                program,
                args: task
                    .args
                    .iter()
                    .map(|value| {
                        resolve_build_graph_string_value(
                            &config.workspace_root,
                            root,
                            &task_root,
                            value,
                        )
                    })
                    .collect(),
                cwd: task
                    .cwd
                    .as_ref()
                    .map(|path| {
                        resolve_build_graph_path(&config.workspace_root, root, &task_root, path)
                    })
                    .unwrap_or_else(|| root.to_path_buf()),
                env: task
                    .env
                    .iter()
                    .map(|(key, value)| {
                        (
                            key.clone(),
                            resolve_build_graph_string_value(
                                &config.workspace_root,
                                root,
                                &task_root,
                                value,
                            ),
                        )
                    })
                    .collect(),
                report_path: default_exec_report.clone(),
                stdout_path,
                stderr_path,
                timeout_ms,
                required_outputs: requested_outputs.clone(),
            }
        }
        BuildTaskKind::Amalgamate => {
            if requested_outputs.len() > 1 {
                return Err(BuildError::Config(format!(
                    "amalgamate task '{}' accepts exactly one output capsule path",
                    task.id
                )));
            }
            let source_root = task.entry.as_ref().ok_or_else(|| {
                BuildError::Config(format!(
                    "amalgamate task '{}' requires path/source/entry",
                    task.id
                ))
            })?;
            let source_root =
                resolve_build_graph_path(&config.workspace_root, root, &task_root, source_root);
            if !inputs.iter().any(|path| path == &source_root) {
                inputs.push(source_root.clone());
            }
            let output_path = outputs.first().cloned().ok_or_else(|| {
                BuildError::Config(format!(
                    "amalgamate task '{}' requires an output capsule path",
                    task.id
                ))
            })?;
            BuildTaskAdapter::Amalgamate {
                source_root,
                output_path,
                report_path: default_amalgamate_report.clone(),
                settings: build_amalgamate_task_settings(task)?,
            }
        }
        BuildTaskKind::Node | BuildTaskKind::Bun => {
            let runtime = if kind == BuildTaskKind::Bun {
                NodeRuntimeKind::Bun
            } else {
                NodeRuntimeKind::Node
            };
            BuildTaskAdapter::NodeLike {
                runtime,
                entry: task.entry.as_ref().map(|path| {
                    resolve_build_graph_path(&config.workspace_root, root, &task_root, path)
                }),
                command: task.command.clone(),
                args: task.args.clone(),
                cwd: task
                    .cwd
                    .as_ref()
                    .map(|path| {
                        resolve_build_graph_path(&config.workspace_root, root, &task_root, path)
                    })
                    .unwrap_or_else(|| root.to_path_buf()),
            }
        }
        _ => unreachable!("explicit task kinds are filtered above"),
    };
    if task_kind_uses_kain_frontend(kind) {
        append_stdlib_cache_inputs(&mut inputs, root);
    }
    let outputs = match &adapter {
        BuildTaskAdapter::GpuArtifacts {
            output_base,
            target,
            no_derived,
            ..
        } => gpu_output_paths(output_base, *target, *no_derived),
        BuildTaskAdapter::NativeExecutable {
            output,
            report_path,
            ..
        } => dedup_paths(vec![output.clone(), report_path.clone()]),
        BuildTaskAdapter::Exec {
            report_path,
            stdout_path,
            stderr_path,
            ..
        } => {
            let mut command_outputs = outputs;
            if !command_outputs.iter().any(|path| path == report_path) {
                command_outputs.push(report_path.clone());
            }
            if let Some(stdout_path) = stdout_path {
                if !command_outputs.iter().any(|path| path == stdout_path) {
                    command_outputs.push(stdout_path.clone());
                }
            }
            if let Some(stderr_path) = stderr_path {
                if !command_outputs.iter().any(|path| path == stderr_path) {
                    command_outputs.push(stderr_path.clone());
                }
            }
            dedup_paths(command_outputs)
        }
        BuildTaskAdapter::Amalgamate {
            output_path,
            report_path,
            ..
        } => {
            let mut capsule_outputs = outputs;
            if !capsule_outputs.iter().any(|path| path == output_path) {
                capsule_outputs.push(output_path.clone());
            }
            if !capsule_outputs.iter().any(|path| path == report_path) {
                capsule_outputs.push(report_path.clone());
            }
            dedup_paths(capsule_outputs)
        }
        BuildTaskAdapter::KainTest { report_path, .. }
        | BuildTaskAdapter::ExternalEvidence { report_path, .. }
        | BuildTaskAdapter::Certify { report_path } => {
            let mut evidence_outputs = outputs;
            if !evidence_outputs.iter().any(|path| path == report_path) {
                evidence_outputs.push(report_path.clone());
            }
            dedup_paths(evidence_outputs)
        }
        _ => outputs,
    };
    Ok(BuildTask {
        id: task_id,
        kind,
        blade: blade
            .map(|value| value.name.clone())
            .or_else(|| task.blade.clone()),
        description: format!("Explicit build task {}", task.id),
        depends_on: task
            .depends_on
            .iter()
            .filter_map(|value| explicit_build_task_dependency_id(value, blade))
            .collect(),
        inputs: dedup_paths(inputs),
        outputs,
        required_capabilities: task.required_capabilities.clone(),
        matrix_axes: task.matrix_axes.clone(),
        telemetry: task.telemetry.clone(),
        certifies: task.certifies.clone(),
        cacheable: !matches!(
            kind,
            BuildTaskKind::Exec
                | BuildTaskKind::Node
                | BuildTaskKind::Bun
                | BuildTaskKind::Test
                | BuildTaskKind::Proof
                | BuildTaskKind::Benchmark
                | BuildTaskKind::Attrition
                | BuildTaskKind::Certify
        ),
        adapter,
    })
}

fn task_kind_uses_kain_frontend(kind: BuildTaskKind) -> bool {
    matches!(
        kind,
        BuildTaskKind::KainCheck
            | BuildTaskKind::KainCompile
            | BuildTaskKind::NativeExecutable
            | BuildTaskKind::Test
            | BuildTaskKind::Proof
            | BuildTaskKind::RustArtifacts
            | BuildTaskKind::NativeUiApp
            | BuildTaskKind::GpuArtifacts
    )
}

fn build_host_capability_set(plan: &BladeBuildPlan) -> BTreeSet<String> {
    let mut capabilities = BTreeSet::new();
    capabilities.insert("host".to_string());
    capabilities.insert(format!("host.os.{}", std::env::consts::OS));
    capabilities.insert(format!("host.arch.{}", std::env::consts::ARCH));
    capabilities.insert(format!("os.{}", std::env::consts::OS));
    capabilities.insert(format!("arch.{}", std::env::consts::ARCH));
    capabilities.insert(format!("lane.{}", plan.lane.as_str()));
    capabilities.insert(format!("profile.{}", plan.profile));
    capabilities.insert(format!("target.{}", plan.target));
    if cfg!(target_arch = "x86_64") {
        capabilities.insert("cpu.x86_64".to_string());
    }
    if cfg!(target_os = "windows") {
        capabilities.insert("platform.windows".to_string());
    }
    capabilities
}

fn first_missing_required_capability(
    task: &BuildTask,
    host_capabilities: &BTreeSet<String>,
) -> Option<String> {
    task.required_capabilities
        .iter()
        .map(|capability| capability.trim())
        .filter(|capability| !capability.is_empty())
        .find(|capability| !host_capabilities.contains(*capability))
        .map(str::to_string)
}

fn execute_plan(
    plan: BladeBuildPlan,
    options: &BuildExecutionOptions,
) -> BuildResult<BladeBuildReport> {
    if options.clean && !options.dry_run {
        clean_build_roots(&plan)?;
    }
    kfs::create_dir_all(&plan.report_root)?;
    let started_unix_ms = unix_timestamp_ms();
    let report_path = plan.report_root.join(format!(
        "session-{started_unix_ms}-{}.json",
        std::process::id()
    ));
    let events_path = report_path.with_extension("jsonl");
    let event_writer = Arc::new(Mutex::new(EventWriter::new(&events_path)?));
    let driver_progress =
        build_driver_progress_sink(event_writer.clone(), options.progress.clone());
    let mut executions = Vec::new();
    let mut task_statuses = BTreeMap::<String, BuildTaskStatus>::new();
    let mut failed = false;
    let workspace = discover_workspace(&plan.workspace_root)?;
    let host_capabilities = build_host_capability_set(&plan);
    publish_progress_event(
        &event_writer,
        options.progress.as_ref(),
        &ToolingProgressEvent::BuildPlanReady {
            workspace_root: plan.workspace_root.clone(),
            lane: plan.lane.as_str().to_string(),
            target: plan.target.clone(),
            total_tasks: plan.tasks.len(),
        },
    )?;

    for (index, task) in plan.tasks.iter().enumerate() {
        let execution = if options.dry_run {
            BuildTaskExecution {
                id: task.id.clone(),
                kind: task.kind,
                blade: task.blade.clone(),
                status: BuildTaskStatus::Planned,
                cache_hit: false,
                started_unix_ms: None,
                finished_unix_ms: None,
                inputs: task.inputs.clone(),
                outputs: task.outputs.clone(),
                required_capabilities: task.required_capabilities.clone(),
                matrix_axes: task.matrix_axes.clone(),
                telemetry: task.telemetry.clone(),
                certifies: task.certifies.clone(),
                message: task.description.clone(),
                error: None,
            }
        } else if let Some(blocking_dependency) = task.depends_on.iter().find(|dependency| {
            matches!(
                task_statuses.get(*dependency),
                Some(BuildTaskStatus::Failed | BuildTaskStatus::Skipped)
            )
        }) {
            BuildTaskExecution {
                id: task.id.clone(),
                kind: task.kind,
                blade: task.blade.clone(),
                status: BuildTaskStatus::Skipped,
                cache_hit: false,
                started_unix_ms: None,
                finished_unix_ms: None,
                inputs: task.inputs.clone(),
                outputs: task.outputs.clone(),
                required_capabilities: task.required_capabilities.clone(),
                matrix_axes: task.matrix_axes.clone(),
                telemetry: task.telemetry.clone(),
                certifies: task.certifies.clone(),
                message: format!(
                    "skipped because dependency '{}' did not pass",
                    blocking_dependency
                ),
                error: None,
            }
        } else if let Some(missing_capability) =
            first_missing_required_capability(task, &host_capabilities)
        {
            BuildTaskExecution {
                id: task.id.clone(),
                kind: task.kind,
                blade: task.blade.clone(),
                status: BuildTaskStatus::Skipped,
                cache_hit: false,
                started_unix_ms: None,
                finished_unix_ms: None,
                inputs: task.inputs.clone(),
                outputs: task.outputs.clone(),
                required_capabilities: task.required_capabilities.clone(),
                matrix_axes: task.matrix_axes.clone(),
                telemetry: task.telemetry.clone(),
                certifies: task.certifies.clone(),
                message: format!(
                    "skipped because host does not advertise required capability '{}'",
                    missing_capability
                ),
                error: None,
            }
        } else {
            publish_progress_event(
                &event_writer,
                options.progress.as_ref(),
                &ToolingProgressEvent::BuildTaskStarted {
                    current: index + 1,
                    total: plan.tasks.len(),
                    task_id: task.id.clone(),
                    description: task.description.clone(),
                    task_kind: task.kind.as_str().to_string(),
                    blade: task.blade.clone(),
                },
            )?;
            execute_task(task, &plan, &workspace, Some(&driver_progress))?
        };
        publish_progress_event(
            &event_writer,
            options.progress.as_ref(),
            &ToolingProgressEvent::BuildTaskFinished {
                current: index + 1,
                total: plan.tasks.len(),
                task_id: task.id.clone(),
                description: task.description.clone(),
                task_kind: task.kind.as_str().to_string(),
                blade: task.blade.clone(),
                status: tooling_status_for_build_status(execution.status),
                cache_hit: execution.cache_hit,
                message: execution.message.clone(),
                error: execution.error.clone(),
            },
        )?;
        if execution.status == BuildTaskStatus::Failed {
            failed = true;
            if options.fail_fast {
                executions.push(execution);
                break;
            }
        }
        task_statuses.insert(task.id.clone(), execution.status);
        executions.push(execution);
    }

    if failed && !options.fail_fast {
        let skipped_start = executions.len();
        for (offset, task) in plan.tasks.iter().skip(skipped_start).enumerate() {
            let execution = BuildTaskExecution {
                id: task.id.clone(),
                kind: task.kind,
                blade: task.blade.clone(),
                status: BuildTaskStatus::Skipped,
                cache_hit: false,
                started_unix_ms: None,
                finished_unix_ms: None,
                inputs: task.inputs.clone(),
                outputs: task.outputs.clone(),
                required_capabilities: task.required_capabilities.clone(),
                matrix_axes: task.matrix_axes.clone(),
                telemetry: task.telemetry.clone(),
                certifies: task.certifies.clone(),
                message: "skipped after previous failure".to_string(),
                error: None,
            };
            publish_progress_event(
                &event_writer,
                options.progress.as_ref(),
                &ToolingProgressEvent::BuildTaskFinished {
                    current: skipped_start + offset + 1,
                    total: plan.tasks.len(),
                    task_id: task.id.clone(),
                    description: task.description.clone(),
                    task_kind: task.kind.as_str().to_string(),
                    blade: task.blade.clone(),
                    status: ToolingProgressStatus::Skipped,
                    cache_hit: false,
                    message: execution.message.clone(),
                    error: execution.error.clone(),
                },
            )?;
            executions.push(execution);
        }
    }

    let finished_unix_ms = unix_timestamp_ms();
    let status = if failed {
        BladeBuildStatus::Failed
    } else {
        BladeBuildStatus::Succeeded
    };
    let report = BladeBuildReport {
        schema_version: plan.schema_version,
        workspace_root: plan.workspace_root,
        artifact_root: plan.artifact_root,
        cache_root: plan.cache_root,
        report_path: report_path.clone(),
        events_path,
        host: plan.host,
        lane: plan.lane,
        profile: plan.profile,
        target: plan.target,
        status,
        started_unix_ms,
        finished_unix_ms,
        dry_run: options.dry_run,
        build_graph: plan.build_graph,
        tasks: executions,
    };
    let encoded = serde_json::to_string_pretty(&report)
        .map_err(|err| BuildError::Config(format!("failed to serialize build report: {err}")))?;
    kfs::atomic_write_text(&report_path, &encoded)?;
    if failed {
        Err(BuildError::Command(format!(
            "project build failed; report written to {}",
            report_path.display()
        )))
    } else {
        Ok(report)
    }
}

fn execute_task(
    task: &BuildTask,
    plan: &BladeBuildPlan,
    workspace: &BladeWorkspace,
    progress: Option<&ToolingProgressSink>,
) -> BuildResult<BuildTaskExecution> {
    let started_unix_ms = unix_timestamp_ms();
    if task.cacheable && task_is_cached(task, plan)? {
        return Ok(BuildTaskExecution {
            id: task.id.clone(),
            kind: task.kind,
            blade: task.blade.clone(),
            status: BuildTaskStatus::Cached,
            cache_hit: true,
            started_unix_ms: Some(started_unix_ms),
            finished_unix_ms: Some(unix_timestamp_ms()),
            inputs: task.inputs.clone(),
            outputs: task.outputs.clone(),
            required_capabilities: task.required_capabilities.clone(),
            matrix_axes: task.matrix_axes.clone(),
            telemetry: task.telemetry.clone(),
            certifies: task.certifies.clone(),
            message: "cache hit".to_string(),
            error: None,
        });
    }

    let result = match &task.adapter {
        BuildTaskAdapter::BladeCheck => run_blade_check(workspace),
        BuildTaskAdapter::KainCheck { entry, target } => {
            run_kain_check(entry, *target, progress, task.outputs.first())
        }
        BuildTaskAdapter::KainCompile {
            source,
            target,
            emit,
            primary_output,
            materialized_primary_output,
            root_component,
        } => run_kain_compile(
            task,
            plan,
            source,
            *target,
            *emit,
            primary_output,
            materialized_primary_output.as_ref(),
            root_component.as_deref(),
            progress,
        ),
        BuildTaskAdapter::NativeExecutable {
            entry,
            output,
            script_path,
            report_path,
            verify_llvm,
        } => run_native_executable_task(entry, output, script_path, report_path, *verify_llvm),
        BuildTaskAdapter::KainTest {
            entry,
            target,
            mode_override,
            report_path,
            proof_required,
            fail_fast,
            run_ignored,
        } => run_kain_test_task(
            entry,
            *target,
            *mode_override,
            report_path,
            *proof_required,
            *fail_fast,
            *run_ignored,
        ),
        BuildTaskAdapter::ExternalEvidence {
            label,
            program,
            args,
            cwd,
            report_path,
        } => run_external_evidence_command(label, program, args, cwd, report_path),
        BuildTaskAdapter::Certify { report_path } => run_certify_task(task, plan, report_path),
        BuildTaskAdapter::RustArtifacts {
            source,
            output_base,
            materialized_output_base,
            include_spirv,
        } => run_rust_artifacts(
            task,
            plan,
            source,
            output_base,
            materialized_output_base.as_ref(),
            *include_spirv,
            progress,
        ),
        BuildTaskAdapter::NativeUiApp {
            source,
            host,
            project_dir,
            artifact_output_dir,
            cargo_target_dir,
            gpu_runtime_cargo_target_dir,
            executable_output_dir,
            app_name,
            window_title,
            root_component,
            tauri_bundle_identifier,
            tauri_window_label,
            runtime_crate_name,
            runtime_dependency,
            build_executable,
            release,
        } => run_native_ui_app(
            task,
            plan,
            source,
            *host,
            project_dir,
            artifact_output_dir,
            cargo_target_dir,
            gpu_runtime_cargo_target_dir,
            executable_output_dir.as_ref(),
            app_name.as_ref(),
            window_title.as_ref(),
            root_component.as_ref(),
            tauri_bundle_identifier.as_ref(),
            tauri_window_label.as_ref(),
            runtime_crate_name,
            runtime_dependency,
            *build_executable,
            *release,
        ),
        BuildTaskAdapter::CargoBuild {
            manifest_path,
            target_dir,
            release,
        } => run_cargo_build(manifest_path, target_dir, *release),
        BuildTaskAdapter::CSharedLibrary {
            library_name,
            sources,
            header,
            include_paths,
            defines,
            link_libs,
            cpp_options,
            canonical_output,
            materialized_output,
        } => run_c_shared_library(
            &plan.workspace_root,
            library_name,
            sources,
            header,
            include_paths,
            defines,
            link_libs,
            cpp_options,
            canonical_output,
            materialized_output.as_ref(),
        ),
        BuildTaskAdapter::GpuArtifacts {
            source,
            output_base,
            target,
            no_residency,
            no_derived,
        } => run_gpu_artifacts(source, output_base, *target, *no_residency, *no_derived),
        BuildTaskAdapter::Fabric { manifest_path, run } => run_fabric(manifest_path, *run),
        BuildTaskAdapter::Exec {
            label,
            program,
            args,
            cwd,
            env,
            report_path,
            stdout_path,
            stderr_path,
            timeout_ms,
            required_outputs,
        } => run_exec_task(
            label,
            program,
            args,
            cwd,
            env,
            report_path,
            stdout_path.as_ref(),
            stderr_path.as_ref(),
            *timeout_ms,
            required_outputs,
        ),
        BuildTaskAdapter::Amalgamate {
            source_root,
            output_path,
            report_path,
            settings,
        } => run_amalgamate_task(source_root, output_path, report_path, settings),
        BuildTaskAdapter::NodeLike {
            runtime,
            entry,
            command,
            args,
            cwd,
        } => run_node_like(*runtime, entry.as_ref(), command.as_ref(), args, cwd),
    };

    let finished_unix_ms = unix_timestamp_ms();
    match result {
        Ok(message) => {
            if task.cacheable {
                write_task_stamp(task, plan)?;
            }
            Ok(BuildTaskExecution {
                id: task.id.clone(),
                kind: task.kind,
                blade: task.blade.clone(),
                status: BuildTaskStatus::Succeeded,
                cache_hit: false,
                started_unix_ms: Some(started_unix_ms),
                finished_unix_ms: Some(finished_unix_ms),
                inputs: task.inputs.clone(),
                outputs: task.outputs.clone(),
                required_capabilities: task.required_capabilities.clone(),
                matrix_axes: task.matrix_axes.clone(),
                telemetry: task.telemetry.clone(),
                certifies: task.certifies.clone(),
                message,
                error: None,
            })
        }
        Err(error) => Ok(BuildTaskExecution {
            id: task.id.clone(),
            kind: task.kind,
            blade: task.blade.clone(),
            status: BuildTaskStatus::Failed,
            cache_hit: false,
            started_unix_ms: Some(started_unix_ms),
            finished_unix_ms: Some(finished_unix_ms),
            inputs: task.inputs.clone(),
            outputs: task.outputs.clone(),
            required_capabilities: task.required_capabilities.clone(),
            matrix_axes: task.matrix_axes.clone(),
            telemetry: task.telemetry.clone(),
            certifies: task.certifies.clone(),
            message: "task failed".to_string(),
            error: Some(error.to_string()),
        }),
    }
}

fn run_blade_check(workspace: &BladeWorkspace) -> BuildResult<String> {
    let missing = missing_blade_paths(workspace);
    if missing.is_empty() && workspace.diagnostics.is_empty() {
        Ok("all referenced local blade paths exist".to_string())
    } else {
        let mut message = String::new();
        for diagnostic in &workspace.diagnostics {
            message.push_str(&format!(
                "{}: {}\n",
                diagnostic.severity, diagnostic.message
            ));
        }
        for missing in missing {
            message.push_str(&format!(
                "{} {} -> {}\n",
                missing.blade,
                missing.field,
                missing.path.display()
            ));
        }
        Err(BuildError::Config(message))
    }
}

fn run_kain_check(
    entry: &Path,
    target: CompileTarget,
    progress: Option<&ToolingProgressSink>,
    output_path: Option<&PathBuf>,
) -> BuildResult<String> {
    let mut options = kain_check::CheckOptions::new(target);
    options.progress = progress.cloned();
    let report = kain_check::check_file(entry, &options);
    if report.passed() {
        if let Some(output_path) = output_path {
            if let Some(parent) = output_path.parent() {
                kfs::create_dir_all(parent)?;
            }
            let encoded = serde_json::to_string_pretty(&report).map_err(|err| {
                BuildError::Config(format!(
                    "failed to serialize Kain check report for {}: {err}",
                    entry.display()
                ))
            })?;
            kfs::atomic_write_text(output_path, &encoded)?;
        }
        Ok(format!("checked {}", entry.display()))
    } else {
        Err(BuildError::Config(report.error.unwrap_or_else(|| {
            format!("Kain check failed for {}", entry.display())
        })))
    }
}

fn run_kain_test_task(
    entry: &Path,
    target: CompileTarget,
    mode_override: Option<kain_test::KainTestMode>,
    report_path: &Path,
    proof_required: bool,
    fail_fast: bool,
    run_ignored: bool,
) -> BuildResult<String> {
    let mut options = kain_test::KainTestOptions::new(target);
    options.mode_override = mode_override;
    options.fail_fast = fail_fast;
    options.run_ignored = run_ignored;

    let report = kain_test::run_path(entry, &options);
    let proof_count = report
        .cases
        .iter()
        .filter(|case| case.proof.is_some())
        .count();
    write_json_report(
        report_path,
        &json!({
            "schema_version": 1,
            "kind": if proof_required { "proof" } else { "test" },
            "entry": entry,
            "target": kain_check::compile_target_name(target),
            "mode_override": mode_override.map(kain_test::KainTestMode::as_str),
            "fail_fast": fail_fast,
            "run_ignored": run_ignored,
            "proof_cases": proof_count,
            "suite": &report,
        }),
    )?;

    if proof_required && proof_count == 0 {
        return Err(BuildError::Config(format!(
            "proof task for {} produced no Z3 proof evidence",
            entry.display()
        )));
    }
    if !report.is_success() {
        return Err(BuildError::Config(format!(
            "{} task failed for {}; report {}",
            if proof_required { "proof" } else { "test" },
            entry.display(),
            report_path.display()
        )));
    }
    Ok(format!(
        "{} passed: {}/{} cases; {} skipped; report {}",
        if proof_required { "proof" } else { "test" },
        report.passed,
        report.total,
        report.skipped,
        report_path.display()
    ))
}

fn run_native_executable_task(
    entry: &Path,
    output: &Path,
    script_path: &Path,
    report_path: &Path,
    verify_llvm: bool,
) -> BuildResult<String> {
    if !script_path.exists() {
        return Err(BuildError::Config(format!(
            "native executable helper not found: {}",
            script_path.display()
        )));
    }
    if let Some(parent) = output.parent() {
        kfs::create_dir_all(parent)?;
    }
    let mut args = vec![
        "-ExecutionPolicy".to_string(),
        "Bypass".to_string(),
        "-File".to_string(),
        script_path.display().to_string(),
        "-Entry".to_string(),
        entry.display().to_string(),
        "-OutputName".to_string(),
        output.display().to_string(),
    ];
    if let Ok(current_exe) = std::env::current_exe() {
        if current_exe.exists() {
            args.push("-KainBin".to_string());
            args.push(current_exe.display().to_string());
        }
    }
    if verify_llvm {
        args.push("-VerifyLlvm".to_string());
    }
    run_external_evidence_command(
        "native-executable",
        powershell_command(),
        &args,
        script_path.parent().unwrap_or_else(|| Path::new(".")),
        report_path,
    )?;
    if !output.exists() {
        return Err(BuildError::Command(format!(
            "native executable task completed but output was not created: {}",
            output.display()
        )));
    }
    Ok(format!(
        "native executable {} built from {}; report {}",
        output.display(),
        entry.display(),
        report_path.display()
    ))
}

fn run_external_evidence_command(
    label: &str,
    program: impl AsRef<str>,
    args: &[String],
    cwd: &Path,
    report_path: &Path,
) -> BuildResult<String> {
    let program = process_portable_string(program.as_ref());
    let args = process_portable_args(args);
    let cwd = process_portable_path(cwd);
    let started_unix_ms = unix_timestamp_ms();
    let output = Command::new(&program)
        .args(&args)
        .current_dir(&cwd)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .output();
    let finished_unix_ms = unix_timestamp_ms();
    let output = match output {
        Ok(output) => output,
        Err(error) => {
            write_json_report(
                report_path,
                &json!({
                    "schema_version": 1,
                    "kind": label,
                    "program": program,
                    "args": args,
                    "cwd": cwd,
                    "status": "spawn_failed",
                    "error": error.to_string(),
                    "started_unix_ms": started_unix_ms,
                    "finished_unix_ms": finished_unix_ms,
                }),
            )?;
            return Err(BuildError::Command(format!(
                "failed to invoke {label} command '{program}': {error}"
            )));
        }
    };
    let stdout = String::from_utf8_lossy(&output.stdout).to_string();
    let stderr = String::from_utf8_lossy(&output.stderr).to_string();
    write_json_report(
        report_path,
        &json!({
            "schema_version": 1,
            "kind": label,
            "program": program,
            "args": args,
            "cwd": cwd,
            "status": if output.status.success() { "passed" } else { "failed" },
            "exit_code": output.status.code(),
            "stdout": stdout,
            "stderr": stderr,
            "started_unix_ms": started_unix_ms,
            "finished_unix_ms": finished_unix_ms,
        }),
    )?;
    if !output.status.success() {
        return Err(BuildError::Command(format!(
            "{label} exited with status {}; report {}",
            output.status,
            report_path.display()
        )));
    }
    Ok(format!(
        "{label} succeeded; report {}",
        report_path.display()
    ))
}

fn run_certify_task(
    task: &BuildTask,
    plan: &BladeBuildPlan,
    report_path: &Path,
) -> BuildResult<String> {
    write_json_report(
        report_path,
        &json!({
            "schema_version": 1,
            "kind": "certify",
            "task_id": &task.id,
            "blade": &task.blade,
            "lane": plan.lane,
            "target": &plan.target,
            "profile": &plan.profile,
            "certified_dependencies": &task.depends_on,
            "inputs": &task.inputs,
            "outputs": &task.outputs,
            "status": "certified",
            "timestamp_unix_ms": unix_timestamp_ms(),
        }),
    )?;
    Ok(format!(
        "certified {} evidence dependencies; report {}",
        task.depends_on.len(),
        report_path.display()
    ))
}

fn run_kain_compile(
    task: &BuildTask,
    plan: &BladeBuildPlan,
    source_path: &Path,
    target: CompileTarget,
    emit: NativeEmit,
    primary_output: &Path,
    materialized_primary_output: Option<&PathBuf>,
    root_component: Option<&str>,
    progress: Option<&ToolingProgressSink>,
) -> BuildResult<String> {
    let source = kfs::read_text(source_path)?;
    let session = DriverSession::default();
    let mut artifacts = Vec::new();
    if let Some(parent) = primary_output.parent() {
        kfs::create_dir_all(parent)?;
    }

    match target {
        CompileTarget::Wasm => {
            let bytes = session.compile_wasm_binary_with_progress(&source, progress)?;
            kfs::atomic_write_bytes(primary_output, &bytes)?;
            artifacts.push(record_artifact("primary", primary_output)?);
        }
        CompileTarget::Spirv => {
            let bytes = session.compile_spirv_binary_with_progress(&source, progress)?;
            kfs::atomic_write_bytes(primary_output, &bytes)?;
            artifacts.push(record_artifact("primary", primary_output)?);
        }
        CompileTarget::Hybrid => {
            let hybrid = session.compile_hybrid_artifacts_with_progress(&source, progress)?;
            artifacts.extend(write_hybrid_artifacts(primary_output, hybrid)?);
        }
        _ => {
            let mut compiled = session.compile_with_source_path_and_progress(
                &source,
                Some(source_path),
                target,
                progress,
            )?;
            if target == CompileTarget::Llvm && native_llvm_ir_slicing_enabled()? {
                if let Some((sliced, stats)) =
                    kain_driver::slice_llvm_native_executable_ir(&compiled)
                {
                    eprintln!(
                        " Native LLVM IR sliced: {} -> {} bytes, kept {}/{} functions (removed {}, declarations {})",
                        stats.original_bytes,
                        stats.sliced_bytes,
                        stats.kept_functions,
                        stats.original_functions,
                        stats.removed_functions,
                        stats.removed_declarations
                    );
                    compiled = sliced;
                }
            }
            kfs::atomic_write_text(primary_output, &compiled)?;
            artifacts.push(record_artifact("primary", primary_output)?);
        }
    }

    if matches!(target, CompileTarget::Llvm | CompileTarget::C) {
        artifacts.extend(stage_native_backend_artifacts(
            &session,
            &source,
            Some(source_path),
            target,
            primary_output,
            root_component,
        )?);

        // ── Native binary linking ────────────────────────────────────
        // After IR generation and artifact staging, produce the final
        // native binary. Pure-compute programs link with -nostdlib;
        // runtime-using programs need the native runtime bundle.
        if emit != NativeEmit::Exe || target == CompileTarget::Llvm {
            // For LLVM target + any emit mode, link the native binary.
            // For sharedlib/staticlib/object, always link.
            // For exe with C target, the C compiler handles linking.
            let native_output = resolve_native_output_path(source_path, primary_output, emit);
            let link_req = NativeLinkRequest {
                emit,
                llvm_ir_path: primary_output,
                output_path: &native_output,
                source_text: &source,
                runtime_artifacts: NativeRuntimeArtifacts::default(),
            };
            match link_native_binary(&link_req) {
                Ok(exe_path) => {
                    eprintln!(" Native binary: {}", exe_path.display());
                    artifacts.push(record_artifact("native-binary", &exe_path)?);
                }
                Err(err) => {
                    eprintln!(" Native link skipped: {err}");
                    eprintln!("   (clang may not be installed — install LLVM for native binaries)");
                }
            }
        }
    }

    if let Some(materialized) = materialized_primary_output {
        if materialized != primary_output {
            if let Some(parent) = materialized.parent() {
                kfs::create_dir_all(parent)?;
            }
            kfs::copy_file(primary_output, materialized)?;
            artifacts.push(record_artifact("materialized-primary", materialized)?);
        }
    }

    write_artifact_manifest(task, plan, artifacts)?;
    Ok(format!(
        "compiled {} to {}",
        source_path.display(),
        primary_output.display()
    ))
}

/// Determine the native binary output path based on emit mode.
/// For exe: `source_stem.exe` next to the .ll file.
/// For other emits: `source_stem.<ext>` next to the .ll file.
fn resolve_native_output_path(
    source_path: &Path,
    primary_output: &Path,
    emit: NativeEmit,
) -> PathBuf {
    let stem = source_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("output");
    if let Some(parent) = primary_output.parent() {
        parent.join(format!("{}.{}", stem, emit.extension()))
    } else {
        PathBuf::from(format!("{}.{}", stem, emit.extension()))
    }
}

fn run_rust_artifacts(
    task: &BuildTask,
    plan: &BladeBuildPlan,
    source_path: &Path,
    output_base: &Path,
    materialized_output_base: Option<&PathBuf>,
    include_spirv: bool,
    progress: Option<&ToolingProgressSink>,
) -> BuildResult<String> {
    let source = kfs::read_text(source_path)?;
    let session = DriverSession::default();
    if let Some(parent) = output_base.parent() {
        kfs::create_dir_all(parent)?;
    }
    let typed = session.frontend_to_typed_program_with_source_path_and_progress(
        &source,
        Some(source_path),
        CompileTarget::Rust,
        progress,
    )?;
    let bundle = kain_sys_codegen::generate_rust_artifact_bundle(&typed)
        .map_err(|err| BuildError::Config(err.to_string()))?;
    let mut artifacts = Vec::new();

    let primary_path = output_base.with_extension("rs");
    kfs::atomic_write_text(&primary_path, &bundle.primary.contents)?;
    artifacts.push(record_artifact("rust-primary", &primary_path)?);

    for artifact in &bundle.supplemental {
        let path = match artifact.kind {
            kain_sys_codegen::RustArtifactKind::PrimarySource => primary_path.clone(),
            kain_sys_codegen::RustArtifactKind::ShaderHost => {
                with_file_name_suffix(output_base, ".gpu", "rs")
            }
            kain_sys_codegen::RustArtifactKind::ShaderReflection => {
                with_file_name_suffix(output_base, ".reflect", "json")
            }
        };
        if path == primary_path {
            continue;
        }
        kfs::atomic_write_text(&path, &artifact.contents)?;
        artifacts.push(record_artifact("rust-supplemental", &path)?);
    }

    if include_spirv
        && bundle
            .shader_metadata
            .as_ref()
            .is_some_and(|metadata| !metadata.shaders.is_empty())
    {
        let spirv = session.compile_spirv_binary_with_progress(&source, progress)?;
        let path = output_base.with_extension("spv");
        kfs::atomic_write_bytes(&path, &spirv)?;
        artifacts.push(record_artifact("spirv", &path)?);
    }

    if let Some(materialized_output_base) = materialized_output_base {
        if let Some(parent) = materialized_output_base.parent() {
            kfs::create_dir_all(parent)?;
        }
        let canonical_parent = output_base.parent().unwrap_or_else(|| Path::new("."));
        for artifact in artifacts.clone() {
            if artifact.role == "artifact-manifest" {
                continue;
            }
            let Some(file_name) = artifact.path.file_name() else {
                continue;
            };
            if artifact.path.parent() != Some(canonical_parent) {
                continue;
            }
            let destination = materialized_output_base
                .parent()
                .unwrap_or_else(|| Path::new("."))
                .join(file_name);
            kfs::copy_file(&artifact.path, &destination)?;
        }
    }

    write_artifact_manifest(task, plan, artifacts)?;
    Ok(format!(
        "emitted Rust artifacts for {}",
        source_path.display()
    ))
}

#[allow(clippy::too_many_arguments)]
fn run_native_ui_app(
    task: &BuildTask,
    plan: &BladeBuildPlan,
    source_path: &Path,
    host: NativeUiBuildHost,
    project_dir: &Path,
    artifact_output_dir: &Path,
    cargo_target_dir: &Path,
    gpu_runtime_cargo_target_dir: &Path,
    executable_output_dir: Option<&PathBuf>,
    app_name: Option<&String>,
    window_title: Option<&String>,
    root_component: Option<&String>,
    tauri_bundle_identifier: Option<&String>,
    tauri_window_label: Option<&String>,
    runtime_crate_name: &str,
    runtime_dependency: &NativeUiRuntimeDependency,
    build_executable: bool,
    release: bool,
) -> BuildResult<String> {
    let source = kfs::read_text(source_path)?;
    let base_name = source_unit_name(source_path);
    let source_file_name = source_path
        .file_name()
        .and_then(OsStr::to_str)
        .unwrap_or("app.kn")
        .to_string();
    let bundle_config = kain_driver::NativeAppBundleConfig {
        app_name: app_name.cloned().or_else(|| Some(base_name.clone())),
        window_title: window_title.cloned().or_else(|| Some(base_name.clone())),
        root_component: root_component.cloned(),
        source_file_name: Some(source_file_name.clone()),
        source_root: source_path.parent().map(Path::to_path_buf),
        initial_window_size: [1440.0, 920.0],
        include_spirv: true,
    };
    if kain_driver::discover_native_app_root_component(
        &source,
        bundle_config.root_component.as_deref(),
        &source_file_name,
    )?
    .is_none()
    {
        return Err(BuildError::Config(format!(
            "Native UI build requires at least one component in {}",
            source_path.display()
        )));
    }

    let mut artifacts = Vec::new();
    match host {
        NativeUiBuildHost::Qt => {
            let bundle = kain_driver::compile_native_app_bundle(&source, &bundle_config)?;
            let runtime_dependency = resolve_native_runtime_dependency(
                plan,
                project_dir,
                runtime_crate_name,
                runtime_dependency,
            )?;
            let generated = kain_driver::materialize_native_app_bundle(
                &source,
                &bundle,
                &kain_driver::NativeAppMaterializationConfig {
                    project_dir: project_dir.to_path_buf(),
                    runtime_crate_name: runtime_crate_name.to_string(),
                    runtime_dependency,
                    artifact_output_dir: artifact_output_dir.to_path_buf(),
                    build_executable,
                    release,
                    executable_output_dir: executable_output_dir.cloned(),
                    cargo_target_dir: Some(cargo_target_dir.to_path_buf()),
                    gpu_runtime_cargo_target_dir: Some(gpu_runtime_cargo_target_dir.to_path_buf()),
                    launcher_entrypoint: kain_driver::NativeAppLauncherEntrypoint::default(),
                    host_sidecars: resolve_native_ui_host_sidecars(source_path)?,
                },
            )?;
            artifacts.extend(record_existing_artifacts(
                "native-ui",
                &generated.artifact_paths,
            )?);
            artifacts.push(record_artifact(
                "native-ui-manifest",
                &generated.manifest_path,
            )?);
            artifacts.push(record_artifact("native-ui-main", &generated.main_rs_path)?);
            if let Some(executable_path) = generated.executable_path {
                artifacts.push(record_artifact("native-ui-executable", &executable_path)?);
            }
        }
        NativeUiBuildHost::Tauri => {
            run_tauri_native_ui_app(
                &source,
                &bundle_config,
                project_dir,
                artifact_output_dir,
                cargo_target_dir,
                build_executable,
                release,
                tauri_bundle_identifier,
                tauri_window_label,
                &mut artifacts,
            )?;
        }
    }
    write_artifact_manifest(task, plan, artifacts)?;
    Ok(format!(
        "built native-ui app for {} under {}",
        source_path.display(),
        project_dir.display()
    ))
}

#[cfg(feature = "tauri")]
fn run_tauri_native_ui_app(
    source: &str,
    bundle_config: &kain_driver::NativeAppBundleConfig,
    project_dir: &Path,
    artifact_output_dir: &Path,
    cargo_target_dir: &Path,
    build_executable: bool,
    release: bool,
    tauri_bundle_identifier: Option<&String>,
    tauri_window_label: Option<&String>,
    artifacts: &mut Vec<BuildArtifactRecord>,
) -> BuildResult<()> {
    let bundle = kain_driver::compile_tauri_app_bundle(
        source,
        &kain_driver::TauriAppBundleConfig {
            native_app: bundle_config.clone(),
        },
    )?;
    let generated = kain_driver::materialize_tauri_app_bundle(
        source,
        &bundle,
        &kain_driver::TauriAppMaterializationConfig {
            project_dir: project_dir.to_path_buf(),
            artifact_output_dir: artifact_output_dir.to_path_buf(),
            build_executable,
            release,
            cargo_target_dir: Some(cargo_target_dir.to_path_buf()),
            bundle_identifier: tauri_bundle_identifier.cloned(),
            window_label: tauri_window_label.cloned(),
            ..Default::default()
        },
    )?;
    artifacts.extend(record_existing_artifacts(
        "native-ui",
        &generated.artifact_paths,
    )?);
    artifacts.push(record_artifact(
        "native-ui-manifest",
        &generated.src_tauri_manifest_path,
    )?);
    artifacts.push(record_artifact(
        "native-ui-main",
        &generated.src_tauri_main_rs_path,
    )?);
    if let Some(executable_path) = generated.executable_path {
        artifacts.push(record_artifact("native-ui-executable", &executable_path)?);
    }
    Ok(())
}

#[cfg(not(feature = "tauri"))]
fn run_tauri_native_ui_app(
    _source: &str,
    _bundle_config: &kain_driver::NativeAppBundleConfig,
    _project_dir: &Path,
    _artifact_output_dir: &Path,
    _cargo_target_dir: &Path,
    _build_executable: bool,
    _release: bool,
    _tauri_bundle_identifier: Option<&String>,
    _tauri_window_label: Option<&String>,
    _artifacts: &mut Vec<BuildArtifactRecord>,
) -> BuildResult<()> {
    Err(BuildError::Config(
        "Tauri native-ui builds require the kain-build tauri feature".to_string(),
    ))
}

fn run_cargo_build(manifest_path: &Path, target_dir: &Path, release: bool) -> BuildResult<String> {
    kfs::create_dir_all(target_dir)?;
    let mut command = Command::new("cargo");
    command
        .arg("build")
        .arg("--manifest-path")
        .arg(manifest_path)
        .arg("--message-format=json-render-diagnostics")
        .env("CARGO_TARGET_DIR", target_dir)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    apply_cargo_command_defaults(&mut command);
    if release {
        command.arg("--release");
    }
    let output = command
        .output()
        .map_err(|err| BuildError::Command(format!("failed to invoke cargo build: {err}")))?;
    if !output.status.success() {
        return Err(BuildError::Command(format!(
            "cargo build exited with status {}\n{}\n{}",
            output.status,
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr)
        )));
    }
    let stdout = String::from_utf8_lossy(&output.stdout);
    let artifacts = parse_cargo_artifacts(&stdout);
    let manifest_path = target_dir.join(".kain").join("cargo-artifacts.json");
    if let Some(parent) = manifest_path.parent() {
        kfs::create_dir_all(parent)?;
    }
    let encoded = serde_json::to_string_pretty(&artifacts)
        .map_err(|err| BuildError::Config(format!("failed to serialize Cargo artifacts: {err}")))?;
    kfs::atomic_write_text(&manifest_path, &encoded)?;
    Ok(format!(
        "cargo build succeeded; harvested {} artifact records into {}",
        artifacts.len(),
        manifest_path.display()
    ))
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct CargoArtifactHarvest {
    package_id: Option<String>,
    target_name: Option<String>,
    target_kinds: Vec<String>,
    fresh: bool,
    executable: Option<PathBuf>,
    filenames: Vec<PathBuf>,
}

#[derive(Debug, Deserialize)]
struct CargoJsonMessage {
    reason: Option<String>,
    package_id: Option<String>,
    target: Option<CargoJsonTarget>,
    filenames: Option<Vec<PathBuf>>,
    executable: Option<PathBuf>,
    fresh: Option<bool>,
}

#[derive(Debug, Deserialize)]
struct CargoJsonTarget {
    name: Option<String>,
    kind: Option<Vec<String>>,
}

fn parse_cargo_artifacts(stdout: &str) -> Vec<CargoArtifactHarvest> {
    stdout
        .lines()
        .filter_map(|line| serde_json::from_str::<CargoJsonMessage>(line).ok())
        .filter(|message| message.reason.as_deref() == Some("compiler-artifact"))
        .map(|message| CargoArtifactHarvest {
            package_id: message.package_id,
            target_name: message
                .target
                .as_ref()
                .and_then(|target| target.name.clone()),
            target_kinds: message
                .target
                .and_then(|target| target.kind)
                .unwrap_or_default(),
            fresh: message.fresh.unwrap_or(false),
            executable: message.executable,
            filenames: message.filenames.unwrap_or_default(),
        })
        .collect()
}

fn write_hybrid_artifacts(
    descriptor_path: &Path,
    artifacts: kain_driver::HybridArtifactOutput,
) -> BuildResult<Vec<BuildArtifactRecord>> {
    let js_path = descriptor_path.with_extension("js");
    let ts_path = descriptor_path.with_extension("ts");
    let wasm_path = descriptor_path.with_extension("wasm");
    let wasm_file_name = wasm_path
        .file_name()
        .and_then(OsStr::to_str)
        .unwrap_or("main.wasm");
    let descriptor = json!({
        "schema_version": 1,
        "target": "hybrid",
        "js": js_path.file_name().and_then(OsStr::to_str).unwrap_or("main.js"),
        "ts": ts_path.file_name().and_then(OsStr::to_str).unwrap_or("main.ts"),
        "wasm": wasm_file_name,
        "wasm_exports": artifacts.wasm_export_names,
    });
    let descriptor_json = serde_json::to_string_pretty(&descriptor).map_err(|err| {
        BuildError::Config(format!("failed to serialize hybrid descriptor: {err}"))
    })?;
    kfs::atomic_write_text(descriptor_path, &descriptor_json)?;
    kfs::atomic_write_text(
        &js_path,
        &patch_hybrid_wasm_reference(artifacts.js, wasm_file_name),
    )?;
    kfs::atomic_write_text(
        &ts_path,
        &patch_hybrid_wasm_reference(artifacts.ts, wasm_file_name),
    )?;
    kfs::atomic_write_bytes(&wasm_path, &artifacts.wasm)?;
    Ok(vec![
        record_artifact("hybrid-descriptor", descriptor_path)?,
        record_artifact("hybrid-js", &js_path)?,
        record_artifact("hybrid-ts", &ts_path)?,
        record_artifact("hybrid-wasm", &wasm_path)?,
    ])
}

fn patch_hybrid_wasm_reference(source: String, wasm_file_name: &str) -> String {
    let wasm_url_expression = format!(
        "new URL('{wasm_file_name}', document.currentScript?.src ?? window.location.href).toString()"
    );
    source
        .replace("'main.wasm'", &wasm_url_expression)
        .replace("\"main.wasm\"", &wasm_url_expression)
}

fn stage_native_backend_artifacts(
    session: &DriverSession,
    source: &str,
    source_path: Option<&Path>,
    target: CompileTarget,
    output_path: &Path,
    root_component: Option<&str>,
) -> BuildResult<Vec<BuildArtifactRecord>> {
    let mut artifacts = Vec::new();
    let contract_bundle =
        session.compile_runtime_contract_bundle_with_source_path(source, source_path, target)?;
    let runtime_contract_path = output_path.with_extension("runtime_contract.json");
    kfs::atomic_write_text(
        &runtime_contract_path,
        &kain_core::runtime_contract_bundle_to_json(&contract_bundle).map_err(|err| {
            BuildError::Config(format!("failed to serialize runtime contract: {err}"))
        })?,
    )?;
    artifacts.push(record_artifact("runtime-contract", &runtime_contract_path)?);

    let realtime_bundle = session.compile_realtime_app_bundle_with_source_path(
        source,
        source_path,
        target,
        root_component,
    )?;
    let realtime_app_path = output_path.with_extension("realtime_app.json");
    kfs::atomic_write_text(&realtime_app_path, &realtime_bundle.bundle_json)?;
    artifacts.push(record_artifact("realtime-app", &realtime_app_path)?);

    let mut shader_bundle_for_residency = None;
    if let Some(shader_source) = shader_artifact_source(source) {
        match session.compile_shader_artifact_bundle(&shader_source) {
            Ok(bundle) => {
                let shader_path = output_path.with_extension("shader_bundle.json");
                kfs::atomic_write_text(&shader_path, &bundle.bundle_json)?;
                artifacts.push(record_artifact("shader-bundle", &shader_path)?);
                shader_bundle_for_residency = Some(bundle.bundle);
            }
            Err(err) => {
                let message = err.to_string();
                if !(message.contains("no entry points")
                    || message.contains("expected a shader item")
                    || message.contains("SPIR-V backend emitted no entry points"))
                {
                    return Err(BuildError::Kain(err));
                }
            }
        }
    }

    let sidecar_root = output_path.parent().unwrap_or_else(|| Path::new("."));
    let compute_paths = kain_driver::write_compute_residency_sidecars(
        &realtime_bundle.bundle,
        shader_bundle_for_residency.as_ref(),
        sidecar_root,
    )?;
    artifacts.extend(record_existing_artifacts(
        "compute-residency",
        &compute_paths,
    )?);

    Ok(artifacts)
}

fn shader_artifact_source(source: &str) -> Option<String> {
    let tokens = Lexer::new(source).tokenize().ok()?;
    let mapper = SpanMapper::new(source);
    let program = Parser::new(&tokens, &mapper, "<native-backend-shader-extract>")
        .parse()
        .ok()?;
    let shader_items = filter_shader_items(&program.items);
    if shader_items.is_empty() {
        return None;
    }

    let shader_program = Program {
        items: shader_items,
        span: program.span,
    };
    format_program(&shader_program).ok()
}

fn filter_shader_items(items: &[Item]) -> Vec<Item> {
    items
        .iter()
        .filter_map(filter_shader_item)
        .collect::<Vec<_>>()
}

fn filter_shader_item(item: &Item) -> Option<Item> {
    match item {
        Item::Shader(shader) => Some(Item::Shader(shader.clone())),
        Item::Mod(module) => {
            let inline = module.inline.as_ref()?;
            let filtered_inline = filter_shader_items(inline);
            if filtered_inline.is_empty() {
                return None;
            }

            let mut filtered_module = module.clone();
            filtered_module.inline = Some(filtered_inline);
            Some(Item::Mod(filtered_module))
        }
        _ => None,
    }
}

fn record_existing_artifacts(
    role: &str,
    paths: &[PathBuf],
) -> BuildResult<Vec<BuildArtifactRecord>> {
    paths
        .iter()
        .filter(|path| path.exists())
        .map(|path| record_artifact(role, path))
        .collect()
}

fn record_artifact(role: &str, path: &Path) -> BuildResult<BuildArtifactRecord> {
    let metadata = std::fs::metadata(path).ok();
    let sha256 = if path.is_file() {
        Some(kfs::hash_file(path)?)
    } else {
        None
    };
    Ok(BuildArtifactRecord {
        role: role.to_string(),
        path: path.to_path_buf(),
        sha256,
        byte_length: metadata.map(|value| value.len()),
    })
}

fn write_artifact_manifest(
    task: &BuildTask,
    plan: &BladeBuildPlan,
    artifacts: Vec<BuildArtifactRecord>,
) -> BuildResult<()> {
    let path = task
        .outputs
        .iter()
        .find(|path| path.file_name().and_then(OsStr::to_str) == Some("kain-artifacts.json"))
        .cloned()
        .unwrap_or_else(|| {
            task.outputs
                .first()
                .and_then(|path| path.parent())
                .unwrap_or(&plan.artifact_root)
                .join("kain-artifacts.json")
        });
    if let Some(parent) = path.parent() {
        kfs::create_dir_all(parent)?;
    }
    let manifest = BuildArtifactManifest {
        schema_version: BUILD_ARTIFACT_SCHEMA_VERSION,
        task_id: task.id.clone(),
        lane: plan.lane,
        target: plan.target.clone(),
        artifacts,
    };
    let encoded = serde_json::to_string_pretty(&manifest).map_err(|err| {
        BuildError::Config(format!("failed to serialize artifact manifest: {err}"))
    })?;
    kfs::atomic_write_text(path, &encoded)?;
    Ok(())
}

#[allow(clippy::too_many_arguments)]
fn run_c_shared_library(
    workspace_root: &Path,
    library_name: &str,
    sources: &[PathBuf],
    header: &Path,
    include_paths: &[PathBuf],
    defines: &[String],
    link_libs: &[String],
    cpp_options: &[String],
    canonical_output: &Path,
    materialized_output: Option<&PathBuf>,
) -> BuildResult<String> {
    if sources.is_empty() {
        return Err(BuildError::Config(format!(
            "C ABI library '{library_name}' has no sources; add sources = [...] beside header {}",
            header.display()
        )));
    }
    let clang = find_clang(workspace_root)?;
    if let Some(parent) = canonical_output.parent() {
        kfs::create_dir_all(parent)?;
    }
    let mut command = Command::new(&clang);
    kain_core::install_layout::apply_windows_msvc_link_env(&mut command);
    command.arg("-shared").arg("-O2");
    if !cfg!(target_os = "windows") {
        command.arg("-fPIC");
    }
    let header_parent = header.parent().unwrap_or_else(|| Path::new("."));
    command.arg("-I").arg(header_parent);
    for include_path in include_paths {
        command.arg("-I").arg(include_path);
    }
    for define in defines {
        command.arg(format!("-D{define}"));
    }
    for option in cpp_options {
        command.arg(option);
    }
    for source in sources {
        command.arg(source);
    }
    for link_lib in link_libs {
        command.arg(format!("-l{link_lib}"));
    }
    command.arg("-o").arg(canonical_output);
    command.stdout(Stdio::piped()).stderr(Stdio::piped());
    let message = run_command_capture(command, "clang shared library build")?;
    if let Some(materialized_output) = materialized_output {
        if materialized_output != canonical_output {
            if let Some(parent) = materialized_output.parent() {
                kfs::create_dir_all(parent)?;
            }
            kfs::copy_file(canonical_output, materialized_output)?;
        }
    }
    Ok(message)
}

fn run_gpu_artifacts(
    source: &Path,
    output_base: &Path,
    target: GpuArtifactTarget,
    no_residency: bool,
    no_derived: bool,
) -> BuildResult<String> {
    let source_text = kfs::read_text(source)?;
    let artifacts = kain_driver::compile_shader_artifact_bundle(&source_text)?;
    if let Some(parent) = output_base.parent() {
        kfs::create_dir_all(parent)?;
    }

    // SPIR-V — only write if target wants it
    if target.emit_spirv() {
        kfs::atomic_write_bytes(output_base.with_extension("spv"), &artifacts.spirv)?;
    }

    // Always write the standard metadata sidecars
    kfs::atomic_write_text(
        with_file_name_suffix(output_base, ".gpu", "rs"),
        &artifacts.rust_host,
    )?;
    kfs::atomic_write_text(
        with_file_name_suffix(output_base, ".reflect", "json"),
        &artifacts.reflection_json,
    )?;
    kfs::atomic_write_text(
        with_file_name_suffix(output_base, ".shader_bundle", "json"),
        &artifacts.bundle_json,
    )?;

    // HLSL — only if target asks and not suppressed
    if target.emit_hlsl() && !no_derived {
        if let Some(hlsl) = artifacts.derived_hlsl {
            kfs::atomic_write_text(output_base.with_extension("hlsl"), &hlsl)?;
        }
    }

    // WGSL — only if target asks and not suppressed
    if target.emit_wgsl() && !no_derived {
        if let Some(wgsl) = artifacts.derived_wgsl {
            kfs::atomic_write_text(output_base.with_extension("wgsl"), &wgsl)?;
        }
    }

    // PTX — only if target asks and not suppressed
    if target.emit_ptx() && !no_derived {
        if let Some(ptx) = artifacts.derived_ptx {
            kfs::atomic_write_text(output_base.with_extension("ptx"), &ptx)?;
        }
    }

    // Residency sidecars — optional
    if !no_residency {
        let realtime_bundle =
            kain_driver::compile_realtime_app_bundle(&source_text, CompileTarget::Cuda, None)?;
        let compute_paths = kain_driver::write_compute_residency_sidecars(
            &realtime_bundle.bundle,
            Some(&artifacts.bundle),
            output_base.parent().unwrap_or_else(|| Path::new(".")),
        )?;
        for path in compute_paths {
            if path.exists() {
                let _ = record_artifact("compute-residency", &path)?;
            }
        }
    }

    Ok(format!("emitted GPU artifacts for {}", source.display()))
}

fn run_fabric(manifest_path: &Path, run: bool) -> BuildResult<String> {
    if run {
        let result = kain_host::fabric::execute_fabric_manifest_path(manifest_path)?;
        if result.status == FabricSessionStatus::Succeeded {
            Ok(format!(
                "Fabric run succeeded; report {}",
                result.report_path.display()
            ))
        } else {
            Err(BuildError::Command(format!(
                "Fabric run failed for {}; report {}",
                manifest_path.display(),
                result.report_path.display()
            )))
        }
    } else {
        let result = kain_omni::fabric::validate_fabric_manifest_path(manifest_path)?;
        Ok(format!(
            "validated Fabric manifest with {} steps",
            result.step_count
        ))
    }
}

fn build_amalgamate_task_settings(
    task: &KainBuildTaskSection,
) -> BuildResult<AmalgamateTaskSettings> {
    Ok(AmalgamateTaskSettings {
        storage: parse_capsule_storage_option(task, "storage")?.unwrap_or(CapsuleStorage::Editable),
        contents: parse_capsule_contents_option(task, "contents")?
            .unwrap_or(CapsuleContents::Source),
        name: task.options.get("name").cloned(),
        capsule_set: task.options.get("capsule_set").cloned(),
        version: task.options.get("version").cloned(),
        authors: task.authors.clone(),
        notes: task.notes.clone(),
        tags: task.tags.clone(),
        meta: task.meta.clone(),
        header_style: parse_capsule_header_style_option(task, "header")?
            .unwrap_or(CapsuleHeaderStyle::Rich),
        preview_symbol_limit: task
            .options
            .get("preview_symbols")
            .map(|value| parse_usize_option(&task.id, "preview_symbols", value))
            .transpose()?
            .unwrap_or(DEFAULT_PREVIEW_SYMBOL_LIMIT),
        compression: parse_capsule_compression_option(task, "compression")?
            .unwrap_or(CapsuleCompression::Zstd),
        api_index: parse_capsule_index_option(task, "api_index")?.unwrap_or(CapsuleIndexMode::Auto),
        module_index: parse_capsule_index_option(task, "module_index")?
            .unwrap_or(CapsuleIndexMode::Auto),
    })
}

fn parse_usize_option(task_id: &str, key: &str, value: &str) -> BuildResult<usize> {
    value.parse::<usize>().map_err(|_| {
        BuildError::Config(format!(
            "task '{}' has invalid {} value '{}'; expected an unsigned integer",
            task_id, key, value
        ))
    })
}

fn parse_capsule_contents_option(
    task: &KainBuildTaskSection,
    key: &str,
) -> BuildResult<Option<CapsuleContents>> {
    let Some(value) = task.options.get(key) else {
        return Ok(None);
    };
    match value.trim().to_ascii_lowercase().as_str() {
        "source" => Ok(Some(CapsuleContents::Source)),
        "snapshot" => Ok(Some(CapsuleContents::Snapshot)),
        "assets" => Ok(Some(CapsuleContents::Assets)),
        "artifacts" => Ok(Some(CapsuleContents::Artifacts)),
        "evidence" => Ok(Some(CapsuleContents::Evidence)),
        other => Err(BuildError::Config(format!(
            "task '{}' has invalid {} value '{}'; expected source, snapshot, assets, artifacts, or evidence",
            task.id, key, other
        ))),
    }
}

fn parse_capsule_storage_option(
    task: &KainBuildTaskSection,
    key: &str,
) -> BuildResult<Option<CapsuleStorage>> {
    let Some(value) = task.options.get(key) else {
        return Ok(None);
    };
    match value.trim().to_ascii_lowercase().as_str() {
        "editable" => Ok(Some(CapsuleStorage::Editable)),
        "archive" => Ok(Some(CapsuleStorage::Archive)),
        other => Err(BuildError::Config(format!(
            "task '{}' has invalid {} value '{}'; expected editable or archive",
            task.id, key, other
        ))),
    }
}

fn parse_capsule_header_style_option(
    task: &KainBuildTaskSection,
    key: &str,
) -> BuildResult<Option<CapsuleHeaderStyle>> {
    let Some(value) = task.options.get(key) else {
        return Ok(None);
    };
    match value.trim().to_ascii_lowercase().as_str() {
        "minimal" => Ok(Some(CapsuleHeaderStyle::Minimal)),
        "rich" => Ok(Some(CapsuleHeaderStyle::Rich)),
        "off" => Ok(Some(CapsuleHeaderStyle::Off)),
        other => Err(BuildError::Config(format!(
            "task '{}' has invalid {} value '{}'; expected minimal, rich, or off",
            task.id, key, other
        ))),
    }
}

fn parse_capsule_compression_option(
    task: &KainBuildTaskSection,
    key: &str,
) -> BuildResult<Option<CapsuleCompression>> {
    let Some(value) = task.options.get(key) else {
        return Ok(None);
    };
    match value.trim().to_ascii_lowercase().as_str() {
        "zstd" => Ok(Some(CapsuleCompression::Zstd)),
        "none" => Ok(Some(CapsuleCompression::None)),
        other => Err(BuildError::Config(format!(
            "task '{}' has invalid {} value '{}'; expected zstd or none",
            task.id, key, other
        ))),
    }
}

fn parse_capsule_index_option(
    task: &KainBuildTaskSection,
    key: &str,
) -> BuildResult<Option<CapsuleIndexMode>> {
    let Some(value) = task.options.get(key) else {
        return Ok(None);
    };
    match value.trim().to_ascii_lowercase().as_str() {
        "auto" => Ok(Some(CapsuleIndexMode::Auto)),
        "off" => Ok(Some(CapsuleIndexMode::Off)),
        other => Err(BuildError::Config(format!(
            "task '{}' has invalid {} value '{}'; expected auto or off",
            task.id, key, other
        ))),
    }
}

fn capsule_index_mode_name(mode: CapsuleIndexMode) -> &'static str {
    match mode {
        CapsuleIndexMode::Auto => "auto",
        CapsuleIndexMode::Off => "off",
    }
}

#[derive(Debug)]
struct CapturedCommandResult {
    exit_code: Option<i32>,
    timed_out: bool,
    stdout: String,
    stderr: String,
}

fn run_exec_task(
    label: &str,
    program: &str,
    args: &[String],
    cwd: &Path,
    env: &BTreeMap<String, String>,
    report_path: &Path,
    stdout_path: Option<&PathBuf>,
    stderr_path: Option<&PathBuf>,
    timeout_ms: Option<u64>,
    required_outputs: &[PathBuf],
) -> BuildResult<String> {
    let started_unix_ms = unix_timestamp_ms();
    let program = process_portable_string(program);
    let args = process_portable_args(args);
    let cwd = process_portable_path(cwd);
    let env = process_portable_env(env);
    let mut command = Command::new(&program);
    command
        .args(&args)
        .current_dir(&cwd)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    for (key, value) in &env {
        command.env(key, value);
    }
    let captured = run_captured_command(command, &format!("exec task '{label}'"), timeout_ms)?;
    if let Some(stdout_path) = stdout_path {
        write_optional_capture(stdout_path, &captured.stdout)?;
    }
    if let Some(stderr_path) = stderr_path {
        write_optional_capture(stderr_path, &captured.stderr)?;
    }
    let finished_unix_ms = unix_timestamp_ms();
    write_json_report(
        report_path,
        &json!({
            "schema_version": 1,
            "kind": "exec",
            "task_id": label,
            "program": program,
            "args": args,
            "cwd": cwd,
            "env": env,
            "timed_out": captured.timed_out,
            "timeout_ms": timeout_ms,
            "exit_code": captured.exit_code,
            "status": if captured.timed_out {
                "timed_out"
            } else if captured.exit_code == Some(0) {
                "passed"
            } else {
                "failed"
            },
            "stdout_path": stdout_path,
            "stderr_path": stderr_path,
            "stdout": captured.stdout,
            "stderr": captured.stderr,
            "started_unix_ms": started_unix_ms,
            "finished_unix_ms": finished_unix_ms,
        }),
    )?;
    if captured.timed_out {
        return Err(BuildError::Command(format!(
            "exec task '{}' timed out after {}ms; report {}",
            label,
            timeout_ms.unwrap_or(0),
            report_path.display()
        )));
    }
    if captured.exit_code != Some(0) {
        return Err(BuildError::Command(format!(
            "exec task '{}' exited with code {:?}; report {}",
            label,
            captured.exit_code,
            report_path.display()
        )));
    }
    validate_required_outputs(label, required_outputs)?;
    Ok(format!(
        "exec task '{}' succeeded; report {}",
        label,
        report_path.display()
    ))
}

fn run_amalgamate_task(
    source_root: &Path,
    output_path: &Path,
    report_path: &Path,
    settings: &AmalgamateTaskSettings,
) -> BuildResult<String> {
    if let Some(parent) = output_path.parent() {
        kfs::create_dir_all(parent)?;
    }
    let mut options = PackOptions::new(source_root, output_path);
    options.storage = settings.storage;
    options.contents = settings.contents;
    options.name = settings.name.clone();
    options.capsule_set = settings.capsule_set.clone();
    options.version = settings.version.clone();
    options.authors = settings.authors.clone();
    options.notes = settings.notes.clone();
    options.tags = settings.tags.clone();
    options.meta = settings.meta.clone();
    options.header_style = settings.header_style;
    options.preview_symbol_limit = settings.preview_symbol_limit;
    options.compression = settings.compression;
    options.api_index = settings.api_index;
    options.module_index = settings.module_index;
    let report = pack_capsule(&options)
        .map_err(|err| BuildError::Command(format!("amalgamate failed: {err}")))?;
    write_json_report(
        report_path,
        &json!({
            "schema_version": 1,
            "kind": "amalgamate",
            "source_root": source_root,
            "output_path": output_path,
            "storage": settings.storage.as_str(),
            "contents": settings.contents.as_str(),
            "capsule_set": &settings.capsule_set,
            "header": settings.header_style.as_str(),
            "compression": settings.compression.as_str(),
            "preview_symbol_limit": settings.preview_symbol_limit,
            "api_index": capsule_index_mode_name(settings.api_index),
            "module_index": capsule_index_mode_name(settings.module_index),
            "name": &settings.name,
            "version": &settings.version,
            "authors": &settings.authors,
            "notes": &settings.notes,
            "tags": &settings.tags,
            "meta": &settings.meta,
            "report": report,
        }),
    )?;
    validate_required_outputs("amalgamate", &[output_path.to_path_buf()])?;
    Ok(format!(
        "amalgamated {} into {} ({})",
        source_root.display(),
        output_path.display(),
        settings.storage.as_str()
    ))
}

fn run_captured_command(
    mut command: Command,
    label: &str,
    timeout_ms: Option<u64>,
) -> BuildResult<CapturedCommandResult> {
    let mut child = command
        .spawn()
        .map_err(|err| BuildError::Command(format!("failed to invoke {label}: {err}")))?;
    let stdout_reader = child.stdout.take().map(spawn_output_reader);
    let stderr_reader = child.stderr.take().map(spawn_output_reader);
    let start = Instant::now();
    let (status, timed_out) = loop {
        if let Some(status) = child.try_wait().map_err(|err| {
            BuildError::Command(format!("failed while waiting for {label}: {err}"))
        })? {
            break (status, false);
        }
        if let Some(timeout_ms) = timeout_ms {
            if start.elapsed() >= Duration::from_millis(timeout_ms) {
                let _ = child.kill();
                let status = child.wait().map_err(|err| {
                    BuildError::Command(format!(
                        "failed to reap timed-out command for {label}: {err}"
                    ))
                })?;
                break (status, true);
            }
        }
        thread::sleep(Duration::from_millis(15));
    };
    let stdout = join_output_reader(stdout_reader, "stdout", label)?;
    let stderr = join_output_reader(stderr_reader, "stderr", label)?;
    Ok(CapturedCommandResult {
        exit_code: status.code(),
        timed_out,
        stdout,
        stderr,
    })
}

fn spawn_output_reader<R>(mut reader: R) -> thread::JoinHandle<std::io::Result<Vec<u8>>>
where
    R: Read + Send + 'static,
{
    thread::spawn(move || {
        let mut buffer = Vec::new();
        reader.read_to_end(&mut buffer)?;
        Ok(buffer)
    })
}

fn join_output_reader(
    handle: Option<thread::JoinHandle<std::io::Result<Vec<u8>>>>,
    stream_name: &str,
    label: &str,
) -> BuildResult<String> {
    let Some(handle) = handle else {
        return Ok(String::new());
    };
    let bytes = handle
        .join()
        .map_err(|_| BuildError::Command(format!("{label} {stream_name} reader panicked")))?
        .map_err(|err| {
            BuildError::Command(format!(
                "failed to capture {stream_name} for {label}: {err}"
            ))
        })?;
    Ok(String::from_utf8_lossy(&bytes).to_string())
}

fn write_optional_capture(path: &Path, contents: &str) -> BuildResult<()> {
    if let Some(parent) = path.parent() {
        kfs::create_dir_all(parent)?;
    }
    kfs::atomic_write_text(path, contents)?;
    Ok(())
}

fn validate_required_outputs(label: &str, outputs: &[PathBuf]) -> BuildResult<()> {
    let missing = outputs
        .iter()
        .filter(|path| !path.exists())
        .map(|path| path.display().to_string())
        .collect::<Vec<_>>();
    if missing.is_empty() {
        return Ok(());
    }
    Err(BuildError::Command(format!(
        "{label} completed but did not create declared outputs: {}",
        missing.join(", ")
    )))
}

fn run_node_like(
    runtime: NodeRuntimeKind,
    entry: Option<&PathBuf>,
    command: Option<&String>,
    args: &[String],
    cwd: &Path,
) -> BuildResult<String> {
    let program = process_portable_string(&command.cloned().unwrap_or_else(|| match runtime {
        NodeRuntimeKind::Node => "node".to_string(),
        NodeRuntimeKind::Bun => "bun".to_string(),
    }));
    let args = process_portable_args(args);
    let cwd = process_portable_path(cwd);
    let entry = entry.map(|value| process_portable_path(value));
    let mut process = Command::new(&program);
    process
        .current_dir(&cwd)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    match (runtime, entry.as_ref(), command) {
        (NodeRuntimeKind::Node, Some(entry), None) => {
            process.arg("--check").arg(entry);
        }
        (NodeRuntimeKind::Bun, Some(entry), None) => {
            process.arg(entry);
        }
        _ => {
            for arg in &args {
                process.arg(arg);
            }
            if let Some(entry) = entry {
                process.arg(entry);
            }
        }
    }
    run_command_capture(process, &format!("{program} task"))
}

fn process_portable_args(args: &[String]) -> Vec<String> {
    args.iter()
        .map(|value| process_portable_string(value))
        .collect()
}

fn process_portable_env(env: &BTreeMap<String, String>) -> BTreeMap<String, String> {
    env.iter()
        .map(|(key, value)| (key.clone(), process_portable_string(value)))
        .collect()
}

fn process_portable_string(value: &str) -> String {
    #[cfg(windows)]
    {
        if let Some(stripped) = value.strip_prefix(r"\\?\UNC\") {
            return format!(r"\\{stripped}");
        }
        if let Some(stripped) = value.strip_prefix(r"\\?\") {
            return stripped.to_string();
        }
    }
    value.to_string()
}

fn process_portable_path(path: &Path) -> PathBuf {
    #[cfg(windows)]
    {
        let rendered = path.as_os_str().to_string_lossy();
        if let Some(stripped) = rendered.strip_prefix(r"\\?\UNC\") {
            return PathBuf::from(format!(r"\\{stripped}"));
        }
        if let Some(stripped) = rendered.strip_prefix(r"\\?\") {
            return PathBuf::from(stripped);
        }
    }
    path.to_path_buf()
}

fn run_command_capture(mut command: Command, label: &str) -> BuildResult<String> {
    let output = command
        .output()
        .map_err(|err| BuildError::Command(format!("failed to invoke {label}: {err}")))?;
    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout).trim().to_string();
        Ok(if stdout.is_empty() {
            format!("{label} succeeded")
        } else {
            stdout
        })
    } else {
        let stdout = String::from_utf8_lossy(&output.stdout);
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(BuildError::Command(format!(
            "{label} exited with status {}\n{}\n{}",
            output.status, stdout, stderr
        )))
    }
}

fn write_json_report(path: &Path, value: &serde_json::Value) -> BuildResult<()> {
    if let Some(parent) = path.parent() {
        kfs::create_dir_all(parent)?;
    }
    let encoded = serde_json::to_string_pretty(value)
        .map_err(|err| BuildError::Config(format!("failed to serialize evidence report: {err}")))?;
    kfs::atomic_write_text(path, &encoded)?;
    Ok(())
}

fn python_command() -> String {
    std::env::var("PYTHON").unwrap_or_else(|_| {
        if cfg!(target_os = "windows") {
            "python".to_string()
        } else {
            "python3".to_string()
        }
    })
}

fn powershell_command() -> &'static str {
    if cfg!(target_os = "windows") {
        "powershell"
    } else {
        "pwsh"
    }
}

fn dedup_paths(paths: Vec<PathBuf>) -> Vec<PathBuf> {
    let mut seen = BTreeSet::new();
    let mut deduped = Vec::new();
    for path in paths {
        let key = path.display().to_string();
        if seen.insert(key) {
            deduped.push(path);
        }
    }
    deduped
}

fn task_is_cached(task: &BuildTask, plan: &BladeBuildPlan) -> BuildResult<bool> {
    if task.outputs.is_empty() || !task.outputs.iter().all(|path| path.exists()) {
        return Ok(false);
    }
    let stamp_path = stamp_path(task, plan);
    if !stamp_path.exists() {
        return Ok(false);
    }
    let expected = task_stamp(task, plan)?;
    let actual = kfs::read_text(stamp_path)?;
    Ok(actual == expected)
}

fn write_task_stamp(task: &BuildTask, plan: &BladeBuildPlan) -> BuildResult<()> {
    let stamp_path = stamp_path(task, plan);
    if let Some(parent) = stamp_path.parent() {
        kfs::create_dir_all(parent)?;
    }
    let stamp = task_stamp(task, plan)?;
    kfs::atomic_write_text(stamp_path, &stamp)?;
    Ok(())
}

fn stamp_path(task: &BuildTask, plan: &BladeBuildPlan) -> PathBuf {
    stamp_path_for_id(&task.id, plan)
}

fn stamp_path_for_id(task_id: &str, plan: &BladeBuildPlan) -> PathBuf {
    plan.cache_root
        .join("stamps")
        .join(format!("{}.stamp", sanitize_id(task_id)))
}

fn task_stamp(task: &BuildTask, plan: &BladeBuildPlan) -> BuildResult<String> {
    let mut hasher = Sha256::new();
    hasher.update(BUILD_ADAPTER_VERSION.as_bytes());
    if let Ok(current_exe) = std::env::current_exe() {
        hash_current_exe_identity_into(&mut hasher, &current_exe)?;
    }
    hasher.update(plan.host.as_bytes());
    hasher.update(plan.lane.as_str().as_bytes());
    hasher.update(plan.profile.as_bytes());
    hasher.update(plan.target.as_bytes());
    hasher.update(task.id.as_bytes());
    hasher.update(task.kind.as_str().as_bytes());
    hasher.update(format!("{:?}", task.adapter).as_bytes());
    for dependency in &task.depends_on {
        hasher.update(b"dependency=");
        hasher.update(dependency.as_bytes());
        let dependency_stamp = stamp_path_for_id(dependency, plan);
        if dependency_stamp.exists() {
            hash_path_into(&mut hasher, &dependency_stamp)?;
        } else {
            hasher.update(b":stamp-missing");
        }
    }
    for input in &task.inputs {
        hash_path_into(&mut hasher, input)?;
    }
    for output in &task.outputs {
        hasher.update(output.display().to_string().as_bytes());
    }
    Ok(format!("{:x}", hasher.finalize()))
}

fn hash_current_exe_identity_into(hasher: &mut Sha256, path: &Path) -> BuildResult<()> {
    hasher.update(path.display().to_string().as_bytes());
    if let Ok(metadata) = kfs::metadata(path) {
        hasher.update(b":len=");
        hasher.update(metadata.len.to_string().as_bytes());
        if let Some(modified_millis) = metadata.modified_millis {
            hasher.update(b":modified=");
            hasher.update(modified_millis.to_string().as_bytes());
        }
    }
    Ok(())
}

fn hash_path_into(hasher: &mut Sha256, path: &Path) -> BuildResult<()> {
    hasher.update(path.display().to_string().as_bytes());
    if path.is_file() {
        hasher.update(kfs::hash_file(path)?.as_bytes());
    } else if path.is_dir() {
        for entry in kfs::read_dir_entries(path)? {
            let name = entry
                .path
                .file_name()
                .and_then(|value| value.to_str())
                .unwrap_or("");
            if matches!(name, ".kain" | "target" | "node_modules" | ".git") {
                continue;
            }
            hash_path_into(hasher, &entry.path)?;
        }
    }
    Ok(())
}

fn order_tasks(tasks: Vec<BuildTask>) -> BuildResult<Vec<BuildTask>> {
    let mut by_id = BTreeMap::new();
    for task in tasks {
        if by_id.insert(task.id.clone(), task).is_some() {
            return Err(BuildError::Config(
                "duplicate build task id detected".to_string(),
            ));
        }
    }
    let mut ordered = Vec::new();
    let mut temporary = BTreeSet::new();
    let mut permanent = BTreeSet::new();
    for id in by_id.keys() {
        visit_task(id, &by_id, &mut temporary, &mut permanent, &mut ordered)?;
    }
    Ok(ordered)
}

fn validate_plan_safety(plan: &BladeBuildPlan) -> BuildResult<()> {
    let mut outputs = BTreeMap::<String, String>::new();
    for task in &plan.tasks {
        for output in &task.outputs {
            let key = stable_absolute_path(&plan.workspace_root, output);
            if let Some(previous_task) = outputs.insert(key.clone(), task.id.clone()) {
                return Err(BuildError::Config(format!(
                    "build output collision: tasks '{}' and '{}' both write {}",
                    previous_task,
                    task.id,
                    output.display()
                )));
            }
        }
    }
    kain_clean::ensure_safe_clean_root(&plan.workspace_root, &plan.artifact_root)?;
    kain_clean::ensure_safe_clean_root(&plan.workspace_root, &plan.cache_root)?;
    kain_clean::ensure_safe_clean_root(&plan.workspace_root, &plan.report_root)?;
    Ok(())
}

fn stable_absolute_path(workspace_root: &Path, path: &Path) -> String {
    let absolute = if path.is_absolute() {
        path.to_path_buf()
    } else {
        workspace_root.join(path)
    };
    kfs::canonicalize_path(&absolute).unwrap_or_else(|_| absolute.display().to_string())
}

fn visit_task(
    id: &str,
    by_id: &BTreeMap<String, BuildTask>,
    temporary: &mut BTreeSet<String>,
    permanent: &mut BTreeSet<String>,
    ordered: &mut Vec<BuildTask>,
) -> BuildResult<()> {
    if permanent.contains(id) {
        return Ok(());
    }
    if !temporary.insert(id.to_string()) {
        return Err(BuildError::Config(format!(
            "cycle detected in build authority task graph at '{id}'"
        )));
    }
    let task = by_id
        .get(id)
        .ok_or_else(|| BuildError::Config(format!("unknown build task '{id}'")))?;
    for dependency in &task.depends_on {
        if by_id.contains_key(dependency) {
            visit_task(dependency, by_id, temporary, permanent, ordered)?;
        } else {
            return Err(BuildError::Config(format!(
                "task '{}' depends on missing task '{}'",
                task.id, dependency
            )));
        }
    }
    temporary.remove(id);
    permanent.insert(id.to_string());
    ordered.push(task.clone());
    Ok(())
}

#[derive(Debug)]
struct MissingBladePath {
    blade: String,
    field: String,
    path: PathBuf,
}

fn missing_blade_paths(workspace: &BladeWorkspace) -> Vec<MissingBladePath> {
    let mut missing = Vec::new();
    for blade in &workspace.blades {
        check_optional_path(&mut missing, blade, "entry", blade.entry.as_ref());
        check_optional_path(
            &mut missing,
            blade,
            "kain_manifest",
            blade.kain_manifest.as_ref(),
        );
        check_optional_path(
            &mut missing,
            blade,
            "cargo_manifest",
            blade.cargo_manifest.as_ref(),
        );
        check_optional_path(
            &mut missing,
            blade,
            "fabric_manifest",
            blade.fabric_manifest.as_ref(),
        );
        for root in &blade.module_roots {
            check_path(&mut missing, blade, "module_root", root);
        }
        for library in &blade.c_ffi_libraries {
            check_path(&mut missing, blade, "c_ffi.header", &library.header);
            for source in &library.sources {
                check_path(&mut missing, blade, "c_ffi.source", source);
            }
            check_optional_path(
                &mut missing,
                blade,
                "c_ffi.shared_lib",
                library.shared_lib.as_ref(),
            );
        }
        for shader_source in &blade.gpu_shader_sources {
            check_path(&mut missing, blade, "gpu.shader_source", shader_source);
        }
        for shader_root in &blade.gpu_shader_roots {
            check_path(&mut missing, blade, "gpu.shader_root", shader_root);
        }
        for path in blade.artifacts.values() {
            check_path(&mut missing, blade, "artifact", path);
        }
    }
    missing
}

fn check_optional_path(
    missing: &mut Vec<MissingBladePath>,
    blade: &ResolvedBlade,
    field: &str,
    path: Option<&PathBuf>,
) {
    if let Some(path) = path {
        check_path(missing, blade, field, path);
    }
}

fn check_path(
    missing: &mut Vec<MissingBladePath>,
    blade: &ResolvedBlade,
    field: &str,
    path: &Path,
) {
    if !path.exists() {
        missing.push(MissingBladePath {
            blade: blade.name.clone(),
            field: field.to_string(),
            path: path.to_path_buf(),
        });
    }
}

fn discover_fabric_manifests(workspace: &BladeWorkspace) -> BuildResult<Vec<PathBuf>> {
    let mut manifests = BTreeSet::new();
    let root_conventional = workspace.root.join(FABRIC_MANIFEST_NAME);
    if root_conventional.exists() {
        manifests.insert(root_conventional);
    }
    for entry in kfs::read_dir_entries(&workspace.root)? {
        let path = entry.path;
        let Some(name) = path.file_name().and_then(|value| value.to_str()) else {
            continue;
        };
        if name.starts_with("KAIN") && name.ends_with(".fabric.toml") {
            manifests.insert(path);
        }
    }
    for blade in &workspace.blades {
        if let Some(path) = &blade.fabric_manifest {
            manifests.insert(path.clone());
        }
    }
    Ok(manifests.into_iter().collect())
}

fn should_run_fabric_manifest(path: &Path, include_vulkan: bool) -> BuildResult<bool> {
    let manifest = kain_omni::fabric::load_fabric_manifest(path)?;
    let has_gpu = manifest
        .steps
        .iter()
        .any(|step| step.runtime == FabricRuntimeKind::GpuCompute);
    Ok(!has_gpu || include_vulkan)
}

fn resolve_c_sources(library: &ResolvedCffiLibrary) -> Vec<PathBuf> {
    if !library.sources.is_empty() {
        return library.sources.clone();
    }
    let Some(parent) = library.header.parent() else {
        return Vec::new();
    };
    let Some(stem) = library.header.file_stem().and_then(|value| value.to_str()) else {
        return Vec::new();
    };
    let exact = parent.join(format!("{stem}.c"));
    if exact.exists() {
        return vec![exact];
    }
    let Ok(entries) = kfs::read_dir_entries(parent) else {
        return Vec::new();
    };
    let mut sources = entries
        .into_iter()
        .map(|entry| entry.path)
        .filter(|path| path.extension().and_then(|value| value.to_str()) == Some("c"))
        .collect::<Vec<_>>();
    sources.sort();
    sources
}

fn load_project_manifest(path: &Path) -> BuildResult<ProjectManifest> {
    if !path.exists() {
        return Err(BuildError::Config(format!(
            "Project manifest not found: {}",
            path.display()
        )));
    }
    let source = kfs::read_text(path)?;
    toml::from_str(&source).map_err(|err| {
        BuildError::Config(format!(
            "failed to parse project manifest {}: {}",
            path.display(),
            err
        ))
    })
}

/// Target filter for GPU artifact generation.
/// Mirrors the cli::gpu_artifacts::GpuArtifactTarget enum to avoid
/// cross-crate feature-gate dependency on the `gpu` + `sys` features.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum GpuArtifactTarget {
    All,
    Spirv,
    Cuda,
    Hlsl,
    Wgsl,
}

impl GpuArtifactTarget {
    pub fn from_arg(arg: &str) -> Self {
        match arg {
            "all" => GpuArtifactTarget::All,
            "spirv" | "vulkan" | "spv" => GpuArtifactTarget::Spirv,
            "cuda" | "ptx" | "nvidia" => GpuArtifactTarget::Cuda,
            "hlsl" | "d3d" | "dx" => GpuArtifactTarget::Hlsl,
            "wgsl" | "webgpu" | "wgpu" => GpuArtifactTarget::Wgsl,
            _ => GpuArtifactTarget::All,
        }
    }

    pub fn emit_spirv(&self) -> bool {
        matches!(
            self,
            GpuArtifactTarget::All
                | GpuArtifactTarget::Spirv
                | GpuArtifactTarget::Hlsl
                | GpuArtifactTarget::Wgsl
        )
    }

    pub fn emit_ptx(&self) -> bool {
        matches!(self, GpuArtifactTarget::All | GpuArtifactTarget::Cuda)
    }

    pub fn emit_hlsl(&self) -> bool {
        matches!(self, GpuArtifactTarget::All | GpuArtifactTarget::Hlsl)
    }

    pub fn emit_wgsl(&self) -> bool {
        matches!(self, GpuArtifactTarget::All | GpuArtifactTarget::Wgsl)
    }

    pub fn is_cuda_primary(&self) -> bool {
        matches!(self, GpuArtifactTarget::Cuda)
    }
}

fn gpu_output_paths(
    output_base: &Path,
    target: GpuArtifactTarget,
    no_derived: bool,
) -> Vec<PathBuf> {
    let mut paths = Vec::new();
    if target.emit_spirv() {
        paths.push(output_base.with_extension("spv"));
    }
    paths.push(with_file_name_suffix(output_base, ".gpu", "rs"));
    paths.push(with_file_name_suffix(output_base, ".reflect", "json"));
    paths.push(with_file_name_suffix(output_base, ".shader_bundle", "json"));
    if target.emit_hlsl() && !no_derived {
        paths.push(output_base.with_extension("hlsl"));
    }
    if target.emit_wgsl() && !no_derived {
        paths.push(output_base.with_extension("wgsl"));
    }
    if target.emit_ptx() && !no_derived {
        paths.push(output_base.with_extension("ptx"));
    }
    paths
}

fn artifact_manifest_path(task_root: &Path) -> PathBuf {
    task_root.join("kain-artifacts.json")
}

fn exec_report_path(task_root: &Path) -> PathBuf {
    task_root.join("kain-exec.json")
}

fn amalgamate_report_path(task_root: &Path) -> PathBuf {
    task_root.join("kain-amalgamate.json")
}

fn evidence_report_path(task_root: &Path) -> PathBuf {
    task_root.join("kain-evidence.json")
}

fn is_evidence_task_kind(kind: BuildTaskKind) -> bool {
    matches!(
        kind,
        BuildTaskKind::Test
            | BuildTaskKind::Proof
            | BuildTaskKind::Benchmark
            | BuildTaskKind::Attrition
            | BuildTaskKind::Certify
    )
}

fn kain_compile_expected_outputs(
    target: CompileTarget,
    primary_output: &Path,
    materialized_primary_output: Option<&PathBuf>,
) -> Vec<PathBuf> {
    let mut outputs = match target {
        CompileTarget::Hybrid => vec![
            primary_output.to_path_buf(),
            primary_output.with_extension("js"),
            primary_output.with_extension("ts"),
            primary_output.with_extension("wasm"),
        ],
        CompileTarget::Llvm | CompileTarget::C => vec![
            primary_output.to_path_buf(),
            primary_output.with_extension("runtime_contract.json"),
            primary_output.with_extension("realtime_app.json"),
        ],
        _ => vec![primary_output.to_path_buf()],
    };
    if let Some(materialized) = materialized_primary_output {
        outputs.push(materialized.clone());
    }
    outputs
}

fn resolve_materialized_output_path(
    output: &Path,
    target: CompileTarget,
    workspace_root: &Path,
) -> PathBuf {
    let mut resolved = resolve_workspace_path(workspace_root, output);
    let expected_extension = kain_driver::target_extension(target);
    if matches!(
        target,
        CompileTarget::Llvm | CompileTarget::C | CompileTarget::Usf
    ) || resolved.extension().is_none()
    {
        resolved.set_extension(expected_extension);
    }
    resolved
}

fn workspace_root_for_input(input: &Path) -> BuildResult<PathBuf> {
    let start = if input == Path::new("-") {
        std::env::current_dir()?
    } else if input.is_absolute() {
        input
            .parent()
            .map(Path::to_path_buf)
            .unwrap_or_else(|| PathBuf::from("."))
    } else {
        std::env::current_dir()?
            .join(input)
            .parent()
            .map(Path::to_path_buf)
            .unwrap_or_else(|| std::env::current_dir().unwrap_or_else(|_| PathBuf::from(".")))
    };
    if let Ok(workspace) = discover_workspace(&start) {
        return Ok(workspace.root);
    }
    Ok(PathBuf::from(kfs::canonicalize_path(&start)?))
}

fn absolute_workspace_path(workspace_root: &Path, path: &Path) -> BuildResult<PathBuf> {
    if path.is_absolute() {
        Ok(path.to_path_buf())
    } else {
        Ok(std::env::current_dir()?.join(path))
            .map(|path| normalize_path_for_display(workspace_root, &path))
    }
}

fn normalize_path_for_display(_workspace_root: &Path, path: &Path) -> PathBuf {
    PathBuf::from(kfs::canonicalize_path(path).unwrap_or_else(|_| path.display().to_string()))
}

fn source_unit_name(path: &Path) -> String {
    path.file_stem()
        .or_else(|| path.file_name())
        .and_then(OsStr::to_str)
        .map(sanitize_id)
        .filter(|value| !value.is_empty())
        .unwrap_or_else(|| "source".to_string())
}

fn resolve_native_runtime_dependency(
    plan: &BladeBuildPlan,
    project_dir: &Path,
    runtime_crate_name: &str,
    dependency: &NativeUiRuntimeDependency,
) -> BuildResult<kain_driver::NativeAppRuntimeDependency> {
    match dependency {
        NativeUiRuntimeDependency::WorkspacePath => {}
        NativeUiRuntimeDependency::Path(path) => {
            return Ok(kain_driver::NativeAppRuntimeDependency::Path(
                relative_path_or_absolute(
                    &resolve_workspace_path(&plan.workspace_root, path),
                    project_dir,
                ),
            ));
        }
        NativeUiRuntimeDependency::Version(version) => {
            return Ok(kain_driver::NativeAppRuntimeDependency::Version(
                version.trim().to_string(),
            ));
        }
    }
    let dependency_root = plan.workspace_root.join("crates").join(runtime_crate_name);
    if dependency_root.join("Cargo.toml").exists() {
        let path = relative_path_or_absolute(&dependency_root, project_dir);
        return Ok(kain_driver::NativeAppRuntimeDependency::Path(path));
    }
    Ok(kain_driver::NativeAppRuntimeDependency::Version(
        "0.1.0".to_string(),
    ))
}

fn resolve_native_ui_host_sidecars(
    input: &Path,
) -> BuildResult<Vec<kain_driver::NativeAppHostSidecarBinding>> {
    let Some(input_directory) = input.parent() else {
        return Ok(Vec::new());
    };
    let preview_image_path = input_directory.join("generic_scene_visual_reference.png");
    if !preview_image_path.exists() {
        return Ok(Vec::new());
    }
    Ok(vec![kain_driver::NativeAppHostSidecarBinding {
        source_path: preview_image_path,
        packaged_file_name: Some("generic_scene_visual_reference.png".to_string()),
        env_var: Some("KAIN_UI_NATIVE_QT_VIEWPORT_IMAGE_PATH".to_string()),
    }])
}

fn relative_path_or_absolute(path: &Path, base: &Path) -> PathBuf {
    diff_paths(path, base).unwrap_or_else(|| path.to_path_buf())
}

fn diff_paths(path: &Path, base: &Path) -> Option<PathBuf> {
    let path_components: Vec<_> = path.components().collect();
    let base_components: Vec<_> = base.components().collect();
    let shared = shared_path_prefix_len(&path_components, &base_components);
    if shared == 0 {
        return None;
    }
    let mut result = PathBuf::new();
    for _ in shared..base_components.len() {
        result.push("..");
    }
    for component in &path_components[shared..] {
        match component {
            Component::Normal(part) => result.push(part),
            Component::CurDir => result.push("."),
            Component::ParentDir => result.push(".."),
            Component::RootDir => {}
            Component::Prefix(prefix) => result.push(prefix.as_os_str()),
        }
    }
    if result.as_os_str().is_empty() {
        result.push(".");
    }
    Some(result)
}

fn shared_path_prefix_len(path: &[Component<'_>], base: &[Component<'_>]) -> usize {
    let mut shared = 0;
    while shared < path.len() && shared < base.len() && path[shared] == base[shared] {
        shared += 1;
    }
    shared
}

fn with_file_name_suffix(base: &Path, suffix: &str, extension: &str) -> PathBuf {
    let stem = base
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("artifact");
    base.with_file_name(format!("{stem}{suffix}.{extension}"))
}

fn find_clang(workspace_root: &Path) -> BuildResult<PathBuf> {
    if let Some(candidate) = kain_core::install_layout::resolve_bundled_clang_path() {
        return Ok(candidate);
    }
    for ancestor in workspace_root.ancestors() {
        for relative in ["toolchain/llvm/bin/clang.exe", "toolchain/llvm/bin/clang"] {
            let candidate = ancestor.join(relative);
            if candidate.exists() {
                return Ok(candidate);
            }
        }
    }
    Ok(PathBuf::from("clang"))
}

fn clean_build_roots(plan: &BladeBuildPlan) -> BuildResult<()> {
    let _ = kain_clean::clean_paths(
        &plan.workspace_root,
        vec![
            plan.artifact_root.clone(),
            plan.cache_root.clone(),
            plan.report_root.clone(),
        ],
        false,
    )?;
    Ok(())
}

fn paths_equivalent(left: &Path, right: &Path) -> bool {
    comparable_path(left) == comparable_path(right)
}

fn comparable_path(path: &Path) -> String {
    path.display()
        .to_string()
        .trim_start_matches(r"\\?\")
        .replace('\\', "/")
        .to_ascii_lowercase()
}

fn path_stem_or_name(path: &Path) -> String {
    path.file_stem()
        .or_else(|| path.file_name())
        .and_then(|value| value.to_str())
        .unwrap_or("artifact")
        .to_string()
}

fn platform_dynamic_library_name(name: &str) -> String {
    if cfg!(target_os = "windows") {
        format!("{name}.dll")
    } else if cfg!(target_os = "macos") {
        format!("lib{name}.dylib")
    } else {
        format!("lib{name}.so")
    }
}

fn sanitize_id(value: &str) -> String {
    let mut output = String::new();
    for ch in value.chars() {
        if ch.is_ascii_alphanumeric() || ch == '-' || ch == '_' {
            output.push(ch.to_ascii_lowercase());
        } else {
            output.push('-');
        }
    }
    while output.contains("--") {
        output = output.replace("--", "-");
    }
    output.trim_matches('-').to_string()
}

fn sanitize_build_task_reference(value: &str) -> String {
    value
        .split(':')
        .map(sanitize_id)
        .filter(|segment| !segment.is_empty())
        .collect::<Vec<_>>()
        .join(":")
}

fn build_explicit_task_id(task_id: &str, blade: Option<&ResolvedBlade>) -> String {
    if let Some(blade) = blade {
        if task_id.contains(':') {
            sanitize_build_task_reference(task_id)
        } else {
            sanitize_build_task_reference(&format!("{}:{task_id}", blade.name))
        }
    } else {
        sanitize_build_task_reference(task_id)
    }
}

fn explicit_build_task_dependency_id(value: &str, blade: Option<&ResolvedBlade>) -> Option<String> {
    let trimmed = value.trim();
    if trimmed.is_empty() {
        return None;
    }
    let scoped = if let Some(blade) = blade {
        if trimmed.contains(':') {
            trimmed.to_string()
        } else {
            format!("{}:{trimmed}", blade.name)
        }
    } else {
        trimmed.to_string()
    };
    let normalized = sanitize_build_task_reference(&scoped);
    (!normalized.is_empty()).then_some(normalized)
}

fn default_executable_name(blade: Option<&ResolvedBlade>, root: &Path) -> String {
    let stem = blade
        .map(|value| value.name.as_str())
        .or_else(|| root.file_name().and_then(OsStr::to_str))
        .map(sanitize_id)
        .filter(|value| !value.is_empty())
        .unwrap_or_else(|| "kain-app".to_string());
    if cfg!(target_os = "windows") {
        format!("{stem}.exe")
    } else {
        stem
    }
}

fn find_lang_projects_compile_script(start: &Path) -> PathBuf {
    let relative = Path::new(".agents")
        .join("skills")
        .join("lang-projects")
        .join("scripts")
        .join("compile_kain_project_to_root.ps1");
    for ancestor in start.ancestors() {
        let candidate = ancestor.join(&relative);
        if candidate.exists() {
            return candidate;
        }
    }
    start.join(relative)
}

fn append_stdlib_cache_inputs(inputs: &mut Vec<PathBuf>, start: &Path) {
    if let Some(stdlib_root) = find_stdlib_root(start) {
        if !inputs.iter().any(|path| path == &stdlib_root) {
            inputs.push(stdlib_root);
        }
    }
}

fn find_stdlib_root(start: &Path) -> Option<PathBuf> {
    for ancestor in start.ancestors() {
        let candidate = ancestor.join("stdlib");
        if candidate.is_dir() {
            return Some(canonicalize_input_path(&candidate));
        }
    }
    if let Ok(kain_home) = std::env::var("KAIN_HOME") {
        let candidate = PathBuf::from(kain_home).join("stdlib");
        if candidate.is_dir() {
            return Some(canonicalize_input_path(&candidate));
        }
    }
    None
}

fn append_native_runtime_cache_inputs(inputs: &mut Vec<PathBuf>, start: &Path) -> BuildResult<()> {
    let manifest_path = find_native_runtime_manifest(start);
    if !inputs.iter().any(|path| path == &manifest_path) {
        inputs.push(manifest_path.clone());
    }
    if !manifest_path.exists() {
        return Ok(());
    }
    for path in collect_native_runtime_cache_inputs(&manifest_path)? {
        if !inputs.iter().any(|existing| existing == &path) {
            inputs.push(path);
        }
    }
    Ok(())
}

fn find_native_runtime_manifest(start: &Path) -> PathBuf {
    for ancestor in start.ancestors() {
        for suffix in kain_core::install_layout::native_runtime_manifest_candidate_suffixes() {
            let candidate = ancestor.join(suffix);
            if candidate.exists() {
                return canonicalize_input_path(&candidate);
            }
        }
    }
    if let Some(candidate) = kain_core::install_layout::resolve_native_runtime_manifest_path() {
        return canonicalize_input_path(&candidate);
    }
    let fallback = kain_core::install_layout::native_runtime_manifest_candidate_suffixes()
        .first()
        .copied()
        .unwrap_or("runtime/native_core_runtime.toml");
    start.join(fallback)
}

fn collect_native_runtime_cache_inputs(manifest_path: &Path) -> BuildResult<Vec<PathBuf>> {
    let manifest_source = kfs::read_text(manifest_path)?;
    let manifest: NativeRuntimeManifestCacheInputs =
        toml::from_str(&manifest_source).map_err(|error| {
            BuildError::Config(format!(
                "unable to parse native runtime manifest {}: {}",
                manifest_path.display(),
                error
            ))
        })?;
    let platform_sources = current_platform_native_runtime_sources(&manifest);
    if manifest.sources.is_empty() && platform_sources.is_empty() {
        return Err(BuildError::Config(format!(
            "native runtime manifest {} does not declare any sources",
            manifest_path.display()
        )));
    }
    let manifest_dir = manifest_path.parent().ok_or_else(|| {
        BuildError::Config(format!(
            "native runtime manifest {} has no parent directory",
            manifest_path.display()
        ))
    })?;
    let source_paths = manifest
        .sources
        .iter()
        .chain(platform_sources.iter())
        .map(|path| canonicalize_input_path(&resolve_runtime_manifest_path(manifest_dir, path)))
        .collect::<Vec<_>>();
    for source_path in &source_paths {
        if !source_path.exists() {
            return Err(BuildError::Config(format!(
                "native runtime source {} does not exist",
                source_path.display()
            )));
        }
    }
    let include_dirs = manifest
        .include_dirs
        .iter()
        .map(|path| canonicalize_input_path(&resolve_runtime_manifest_path(manifest_dir, path)))
        .collect::<Vec<_>>();
    for include_dir in &include_dirs {
        if !include_dir.exists() {
            return Err(BuildError::Config(format!(
                "native runtime include directory {} does not exist",
                include_dir.display()
            )));
        }
    }
    let mut resolved_inputs = source_paths.clone();
    resolved_inputs.extend(include_dirs.clone());
    resolved_inputs.extend(discover_native_runtime_local_include_inputs(
        &source_paths,
        &include_dirs,
    )?);
    Ok(dedup_paths(resolved_inputs))
}

fn current_platform_native_runtime_sources(
    manifest: &NativeRuntimeManifestCacheInputs,
) -> &[PathBuf] {
    if cfg!(windows) {
        &manifest.windows_sources
    } else if cfg!(target_os = "macos") {
        &manifest.macos_sources
    } else {
        &manifest.linux_sources
    }
}

fn resolve_runtime_manifest_path(root: &Path, value: &Path) -> PathBuf {
    if value.is_absolute() {
        value.to_path_buf()
    } else {
        root.join(value)
    }
}

fn discover_native_runtime_local_include_inputs(
    source_paths: &[PathBuf],
    include_dirs: &[PathBuf],
) -> BuildResult<Vec<PathBuf>> {
    let mut visited = BTreeSet::new();
    let mut discovered = Vec::new();
    let mut pending = source_paths.to_vec();
    while let Some(path) = pending.pop() {
        let normalized = canonicalize_input_path(&path);
        let key = normalized.display().to_string();
        if !visited.insert(key) || !normalized.is_file() {
            continue;
        }
        let source = kfs::read_text(&normalized)?;
        for include_path in parse_quoted_include_paths(&source) {
            let Some(resolved) =
                resolve_native_runtime_quoted_include(&normalized, &include_path, include_dirs)
            else {
                continue;
            };
            let resolved = canonicalize_input_path(&resolved);
            if !path_is_within_any_dir(&resolved, include_dirs)
                && !discovered.iter().any(|existing| existing == &resolved)
            {
                discovered.push(resolved.clone());
            }
            pending.push(resolved);
        }
    }
    Ok(discovered)
}

fn parse_quoted_include_paths(source: &str) -> Vec<PathBuf> {
    source
        .lines()
        .filter_map(|line| {
            let trimmed = line.trim_start();
            let rest = trimmed.strip_prefix("#include \"")?;
            let end = rest.find('"')?;
            Some(PathBuf::from(&rest[..end]))
        })
        .collect()
}

fn resolve_native_runtime_quoted_include(
    owner: &Path,
    include_path: &Path,
    include_dirs: &[PathBuf],
) -> Option<PathBuf> {
    if let Some(parent) = owner.parent() {
        let candidate = parent.join(include_path);
        if candidate.exists() {
            return Some(candidate);
        }
    }
    for include_dir in include_dirs {
        let candidate = include_dir.join(include_path);
        if candidate.exists() {
            return Some(candidate);
        }
    }
    None
}

fn path_is_within_any_dir(path: &Path, dirs: &[PathBuf]) -> bool {
    dirs.iter().any(|dir| path.starts_with(dir))
}

fn canonicalize_input_path(path: &Path) -> PathBuf {
    PathBuf::from(kfs::canonicalize_path(path).unwrap_or_else(|_| path.display().to_string()))
}

fn resolve_build_graph_path(
    workspace_root: &Path,
    blade_or_task_root: &Path,
    task_root: &Path,
    path: &Path,
) -> PathBuf {
    if path.is_absolute() {
        return path.to_path_buf();
    }
    let raw = path.to_string_lossy().replace('\\', "/");
    for (prefix, base) in [
        ("$root", workspace_root),
        ("$repo", workspace_root),
        ("$workspace", workspace_root),
        ("$blade", blade_or_task_root),
        ("$task", task_root),
        ("$out", task_root),
    ] {
        if raw == prefix {
            return base.to_path_buf();
        }
        let slash_prefix = format!("{prefix}/");
        if let Some(rest) = raw.strip_prefix(&slash_prefix) {
            return base.join(rest);
        }
    }
    blade_or_task_root.join(path)
}

fn resolve_build_graph_string_value(
    workspace_root: &Path,
    blade_or_task_root: &Path,
    task_root: &Path,
    value: &str,
) -> String {
    let raw = value.replace('\\', "/");
    if ["$root", "$repo", "$workspace", "$blade", "$task", "$out"]
        .iter()
        .any(|prefix| raw == *prefix || raw.starts_with(&format!("{prefix}/")))
    {
        return process_portable_path(&resolve_build_graph_path(
            workspace_root,
            blade_or_task_root,
            task_root,
            Path::new(value),
        ))
        .display()
        .to_string();
    }
    value.to_string()
}

fn resolve_workspace_path(root: &Path, path: &Path) -> PathBuf {
    if path.is_absolute() {
        path.to_path_buf()
    } else {
        root.join(path)
    }
}

fn default_target_name() -> String {
    format!("{}-{}", std::env::consts::ARCH, std::env::consts::OS)
}

fn unix_timestamp_ms() -> u128 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|value| value.as_millis())
        .unwrap_or(0)
}

fn tooling_status_for_build_status(status: BuildTaskStatus) -> ToolingProgressStatus {
    match status {
        BuildTaskStatus::Planned => ToolingProgressStatus::Planned,
        BuildTaskStatus::Cached => ToolingProgressStatus::Cached,
        BuildTaskStatus::Succeeded => ToolingProgressStatus::Succeeded,
        BuildTaskStatus::Failed => ToolingProgressStatus::Failed,
        BuildTaskStatus::Skipped => ToolingProgressStatus::Skipped,
    }
}

fn publish_progress_event(
    event_writer: &Arc<Mutex<EventWriter>>,
    forward: Option<&ToolingProgressSink>,
    event: &ToolingProgressEvent,
) -> BuildResult<()> {
    let mut writer = event_writer
        .lock()
        .map_err(|_| BuildError::Config("build progress writer lock was poisoned".to_string()))?;
    writer.write_progress(event)?;
    drop(writer);
    if let Some(forward) = forward {
        forward.emit(event);
    }
    Ok(())
}

fn build_driver_progress_sink(
    event_writer: Arc<Mutex<EventWriter>>,
    forward: Option<ToolingProgressSink>,
) -> ToolingProgressSink {
    ToolingProgressSink::new(move |event| {
        if let Ok(mut writer) = event_writer.lock() {
            let _ = writer.write_progress(event);
        }
        if let Some(forward) = forward.as_ref() {
            forward.emit(event);
        }
    })
}

struct EventWriter {
    path: PathBuf,
}

impl EventWriter {
    fn new(path: &Path) -> BuildResult<Self> {
        if let Some(parent) = path.parent() {
            kfs::create_dir_all(parent)?;
        }
        kfs::atomic_write_text(path, "")?;
        Ok(Self {
            path: path.to_path_buf(),
        })
    }

    fn write_progress(&mut self, event: &ToolingProgressEvent) -> BuildResult<()> {
        let encoded = serde_json::to_string(&ToolingProgressRecord::new(
            unix_timestamp_ms(),
            event.clone(),
        ))
        .map_err(|err| BuildError::Config(format!("failed to serialize build event: {err}")))?;
        kfs::append_text(&self.path, &format!("{encoded}\n"))?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::{Arc, Mutex};

    fn capture_progress() -> (Arc<Mutex<Vec<ToolingProgressEvent>>>, ToolingProgressSink) {
        let events = Arc::new(Mutex::new(Vec::new()));
        let capture = events.clone();
        (
            events,
            ToolingProgressSink::new(move |event| {
                capture
                    .lock()
                    .expect("capture progress")
                    .push(event.clone());
            }),
        )
    }

    #[test]
    fn orders_dependencies_before_dependents() {
        let tasks = vec![
            test_task("b", vec!["a"]),
            test_task("a", Vec::new()),
            test_task("c", vec!["b"]),
        ];
        let ordered = order_tasks(tasks).unwrap();
        let ids = ordered.into_iter().map(|task| task.id).collect::<Vec<_>>();
        assert_eq!(ids, vec!["a", "b", "c"]);
    }

    #[test]
    fn detects_task_cycles() {
        let tasks = vec![test_task("a", vec!["b"]), test_task("b", vec!["a"])];
        assert!(order_tasks(tasks).is_err());
    }

    #[test]
    fn detects_duplicate_task_ids() {
        let tasks = vec![test_task("a", Vec::new()), test_task("a", Vec::new())];
        assert!(order_tasks(tasks).is_err());
    }

    #[test]
    fn build_kain_file_emits_task_and_compiler_progress() {
        let root = unique_test_dir("progress-events");
        let entry = root.join("main.kn");
        kfs::write_text(&entry, "fn main() -> Int:\n    return 7\n").expect("entry source");
        let (events, sink) = capture_progress();
        let mut options = KainFileBuildOptions::new(entry, CompileTarget::Interpret);
        options.progress = Some(sink);

        let report = build_kain_file(&options).expect("build report");

        assert!(report.is_success());
        let events = events.lock().expect("lock events");
        assert!(events.iter().any(|event| matches!(
            event,
            ToolingProgressEvent::BuildPlanReady { total_tasks: 1, .. }
        )));
        assert!(events
            .iter()
            .any(|event| matches!(event, ToolingProgressEvent::BuildTaskStarted { .. })));
        assert!(events.iter().any(|event| matches!(
            event,
            ToolingProgressEvent::CompilerPhase {
                phase: kain_driver::CompilerProgressPhase::Parse,
                ..
            }
        )));
        assert!(events.iter().any(|event| matches!(
            event,
            ToolingProgressEvent::CompilerPhase {
                phase: kain_driver::CompilerProgressPhase::Interpret,
                ..
            }
        )));
        assert!(events.iter().any(|event| matches!(
            event,
            ToolingProgressEvent::BuildTaskFinished {
                status: ToolingProgressStatus::Succeeded,
                ..
            }
        )));
    }

    #[test]
    fn build_kain_file_keeps_c_include_aliases_visible_to_native_sidecar_staging() {
        let root = unique_test_dir("c-include-sidecars");
        let native_dir = root.join("native");
        kfs::create_dir_all(&native_dir).expect("native dir");
        kfs::write_text(
            native_dir.join("tiny_math.h"),
            "int tiny_math_add(int a, int b);\nint tiny_math_ping(void);\n",
        )
        .expect("header");
        kfs::write_text(
            native_dir.join("tiny_math.c"),
            "#include \"tiny_math.h\"\nint tiny_math_add(int a, int b) { return a + b; }\nint tiny_math_ping(void) { return 1; }\n",
        )
        .expect("source");
        let entry = root.join("main.kn");
        kfs::write_text(
            &entry,
            "\
include native/tiny_math.h as tm

fn main() -> Int:
    let ping = tm_ping()
    return tm_add(ping, 41)
",
        )
        .expect("entry source");

        let report = build_kain_file(&KainFileBuildOptions::new(entry, CompileTarget::Llvm))
            .expect("build report");

        assert!(report.is_success(), "{:#?}", report.tasks);
        let manifest_path = report.tasks[0]
            .outputs
            .iter()
            .find(|path| path.file_name().and_then(OsStr::to_str) == Some("kain-artifacts.json"))
            .expect("artifact manifest output");
        let manifest = kfs::read_text(manifest_path).expect("artifact manifest");
        assert!(manifest.contains("runtime-contract"));
        assert!(manifest.contains("realtime-app"));
        assert!(!manifest.contains("shader-bundle"));
    }

    #[test]
    fn build_graph_extracts_explicit_tasks_from_build_kn() {
        let source = r#"
fn build(ctx: BuildContext) -> BuildGraph:
    let check = build_task("check-llvm")
        .kind("check")
        .entry("src/main.kn")
        .target("llvm")
        .input("src/main.kn")
        .input("src/vulkain.kn")
        .depends_on("prep-assets")
    let bundle = build_task("bundle-web")
        .kind("bun")
        .entry("tools/bundle.ts")
        .command("bun")
        .arg("run")
        .arg("build")
        .cwd("tools")
        .output("dist/app.js")
    return build_graph().task(check).task(bundle)
"#;

        let tasks = extract_build_graph_explicit_tasks(source);
        assert_eq!(tasks.len(), 2);
        assert_eq!(tasks[0].id, "check-llvm");
        assert_eq!(tasks[0].kind, "check");
        assert_eq!(tasks[0].entry, Some(PathBuf::from("src/main.kn")));
        assert_eq!(tasks[0].target.as_deref(), Some("llvm"));
        assert_eq!(
            tasks[0].inputs,
            vec![
                PathBuf::from("src/main.kn"),
                PathBuf::from("src/vulkain.kn")
            ]
        );
        assert_eq!(tasks[0].depends_on, vec!["prep-assets".to_string()]);
        assert_eq!(tasks[1].id, "bundle-web");
        assert_eq!(tasks[1].kind, "bun");
        assert_eq!(tasks[1].entry, Some(PathBuf::from("tools/bundle.ts")));
        assert_eq!(tasks[1].command.as_deref(), Some("bun"));
        assert_eq!(tasks[1].args, vec!["run".to_string(), "build".to_string()]);
        assert_eq!(tasks[1].cwd, Some(PathBuf::from("tools")));
        assert_eq!(tasks[1].outputs, vec![PathBuf::from("dist/app.js")]);
    }

    #[test]
    fn build_graph_extracts_first_class_std_build_api_tasks() {
        let source = r#"
use std::build
use std::test
use std::proof
use std::bench
use std::attrition
use std::certify

fn build(ctx: BuildContext) -> BuildGraph:
    let check = build_check("check-llvm")
        .entry("src/main.kn")
        .target("llvm")
        .requires_capability("target.llvm")
        .axis("target", "llvm")
        .telemetry("llm")
    let suite = test_suite("source-tests")
        .entry("src/main.kn")
        .requires("check-llvm")
    let proof = proof_obligation("z3-layout-proof")
        .entry("z3/proof.kn")
        .requires("source-tests")
        .proof_mode("prove-pass")
    let bench = bench_case("bench-ui")
        .arg("--case")
        .arg("kaintana_layout")
    let abuse = attrition_case("attrition-small")
        .arg("--scale")
        .arg("small")
    let exe = native_executable("root-executable")
        .entry("src/main.kn")
        .root_output("$blade/app.exe")
        .requires("z3-layout-proof")
    let gate = certify_gate("certify")
        .requires("root-executable")
        .requires("bench-ui")
        .requires("attrition-small")
        .certifies("release.local")
    return build_graph().task(check).task(suite).task(proof).task(bench).task(abuse).task(exe).task(gate)
"#;

        let tasks = extract_build_graph_explicit_tasks(source);
        assert_eq!(tasks.len(), 7);
        let by_id = tasks
            .iter()
            .map(|task| (task.id.as_str(), task))
            .collect::<BTreeMap<_, _>>();
        assert_eq!(by_id["check-llvm"].kind, "check");
        assert_eq!(
            by_id["check-llvm"].required_capabilities,
            vec!["target.llvm".to_string()]
        );
        assert_eq!(
            by_id["check-llvm"].matrix_axes,
            vec!["target=llvm".to_string()]
        );
        assert_eq!(by_id["source-tests"].kind, "test");
        assert_eq!(
            by_id["source-tests"].depends_on,
            vec!["check-llvm".to_string()]
        );
        assert_eq!(by_id["z3-layout-proof"].kind, "proof");
        assert_eq!(
            by_id["z3-layout-proof"].args,
            vec!["prove-pass".to_string()]
        );
        assert_eq!(by_id["bench-ui"].kind, "benchmark");
        assert_eq!(by_id["attrition-small"].kind, "attrition");
        assert_eq!(by_id["root-executable"].kind, "native-executable");
        assert_eq!(
            by_id["root-executable"].outputs,
            vec![PathBuf::from("$blade/app.exe")]
        );
        assert_eq!(by_id["certify"].kind, "certify");
        assert_eq!(
            by_id["certify"].certifies,
            vec!["release.local".to_string()]
        );
    }

    #[test]
    fn build_graph_extracts_polyglot_adapter_tasks() {
        let source = r#"
use std::build

fn build(ctx: BuildContext) -> BuildGraph:
    let cargo = build_task("cargo-helper")
        .kind("cargo")
        .manifest("tools/cargo-helper/Cargo.toml")
    let bridge = build_task("bridge-c")
        .kind("c-shared-library")
        .entry("native/smoke_bridge.h")
        .input("native/smoke_bridge.c")
        .output("$task/smoke_bridge.dll")
    let gpu = build_task("gpu-smoke")
        .kind("gpu")
        .entry("gpu/smoke_shader.kn")
        .output("$task/smoke_shader")
    let fabric = build_task("fabric-validate")
        .kind("fabric-validate")
        .manifest("KAIN.fabric.toml")
    let nodeish = build_task("node-ish")
        .kind("node")
        .command("python")
        .arg("scripts/echo_lane.py")
        .arg("--lane")
        .arg("node")
    let bunish = build_task("bun-ish")
        .kind("bun")
        .command("python")
        .arg("scripts/echo_lane.py")
        .arg("--lane")
        .arg("bun")
    return build_graph()
        .task(cargo)
        .task(bridge)
        .task(gpu)
        .task(fabric)
        .task(nodeish)
        .task(bunish)
"#;

        let tasks = extract_build_graph_explicit_tasks(source);
        let by_id = tasks
            .iter()
            .map(|task| (task.id.as_str(), task))
            .collect::<BTreeMap<_, _>>();

        assert_eq!(by_id["cargo-helper"].kind, "cargo");
        assert_eq!(
            by_id["cargo-helper"].manifest,
            Some(PathBuf::from("tools/cargo-helper/Cargo.toml"))
        );
        assert_eq!(by_id["bridge-c"].kind, "c-shared-library");
        assert_eq!(
            by_id["bridge-c"].outputs,
            vec![PathBuf::from("$task/smoke_bridge.dll")]
        );
        assert_eq!(by_id["gpu-smoke"].kind, "gpu");
        assert_eq!(by_id["fabric-validate"].kind, "fabric-validate");
        assert_eq!(by_id["node-ish"].kind, "node");
        assert_eq!(
            by_id["node-ish"].args,
            vec![
                "scripts/echo_lane.py".to_string(),
                "--lane".to_string(),
                "node".to_string()
            ]
        );
        assert_eq!(by_id["bun-ish"].kind, "bun");
    }

    #[test]
    fn auto_c_sidecar_tasks_keep_manifest_link_libs() {
        let workspace_root = unique_test_dir("smoketest-link-libs");
        let config = BuildWorkspaceConfig {
            workspace_root: workspace_root.clone(),
            artifact_root: workspace_root.join(".kain/out/llvm"),
            cache_root: workspace_root.join(".kain/cache/build"),
            report_root: workspace_root.join(".kain/reports/build"),
            host: "x86_64-windows".to_string(),
            lane: BuildLane::Dev,
            profile: "debug".to_string(),
            target: "x86_64-windows".to_string(),
        };
        let blade = ResolvedBlade {
            name: "smoketest".to_string(),
            version: Some("0.1.0".to_string()),
            kind: "kain_app".to_string(),
            root: workspace_root.clone(),
            manifest_path: Some(workspace_root.join("KAIN.toml")),
            kain_manifest: None,
            cargo_manifest: None,
            rust_crate_name: None,
            fabric_manifest: None,
            entry: Some(workspace_root.join("src/main.kn")),
            source_roots: vec![workspace_root.join("src")],
            module_roots: vec![workspace_root.join("src")],
            build_targets: vec!["llvm".to_string()],
            dependencies: Vec::new(),
            artifacts: BTreeMap::new(),
            c_ffi_libraries: vec![ResolvedCffiLibrary {
                name: "smoketest_visualizer_bridge".to_string(),
                header: workspace_root.join("native/smoketest_visualizer_bridge.h"),
                sources: vec![workspace_root.join("native/smoketest_visualizer_bridge.c")],
                shared_lib: Some(
                    workspace_root.join(".kain/native/smoketest_visualizer_bridge.obj"),
                ),
                include_paths: vec![workspace_root.join("native")],
                defines: vec!["_CRT_SECURE_NO_WARNINGS".to_string()],
                link_libs: vec![
                    "user32".to_string(),
                    "gdi32".to_string(),
                    "opengl32".to_string(),
                ],
                cpp_options: Vec::new(),
            }],
            gpu_shader_sources: Vec::new(),
            gpu_shader_roots: Vec::new(),
            compute_keys: Vec::new(),
            discovery_source: "unit-test".to_string(),
        };

        let mut tasks = Vec::new();
        let mut sidecar_task_ids = Vec::new();
        add_c_tasks(&mut tasks, &mut sidecar_task_ids, &config, &blade).unwrap();

        assert_eq!(
            sidecar_task_ids,
            vec!["c:smoketest:smoketest_visualizer_bridge".to_string()]
        );
        let adapter = &tasks[0].adapter;
        match adapter {
            BuildTaskAdapter::CSharedLibrary { link_libs, .. } => assert_eq!(
                link_libs,
                &vec![
                    "user32".to_string(),
                    "gdi32".to_string(),
                    "opengl32".to_string(),
                ]
            ),
            other => panic!("unexpected adapter: {other:?}"),
        }
    }

    #[cfg(windows)]
    fn windows_test_drive_prefix() -> String {
        let cwd = std::env::current_dir().expect("cwd");
        let cwd_text = cwd.display().to_string();
        if cwd_text.len() >= 2 && cwd_text.as_bytes()[1] == b':' {
            cwd_text[..2].to_string()
        } else {
            "C:".to_string()
        }
    }

    #[test]
    fn build_graph_extracts_exec_and_amalgamate_task_metadata() {
        let source = r#"
use std::build

fn build(ctx: BuildContext) -> BuildGraph:
    let prep = exec_task("refresh-generated")
        .command("cargo")
        .arg("run")
        .arg("-q")
        .env("CARGO_TARGET_DIR", "$root/target/codex-build-graph")
        .stdout("$task/stdout.txt")
        .stderr("$task/stderr.txt")
        .timeout_ms(60000)
        .always_run()
    let capsule = amalgamate_capsule("smoketest-capsule")
        .path("smoketest")
        .output("$root/.kain/capsules/smoketest.kn")
        .name("smoketest")
        .version("0.1.0")
        .tag("portable")
        .meta("album", "smoketest")
        .storage("editable")
        .header("rich")
        .preview_symbols(32)
        .archive(false)
    return build_graph().task(prep).task(capsule)
"#;

        let tasks = extract_build_graph_explicit_tasks(source);
        let by_id = tasks
            .iter()
            .map(|task| (task.id.as_str(), task))
            .collect::<BTreeMap<_, _>>();
        assert_eq!(by_id["refresh-generated"].kind, "exec");
        assert_eq!(
            by_id["refresh-generated"]
                .env
                .get("CARGO_TARGET_DIR")
                .map(String::as_str),
            Some("$root/target/codex-build-graph")
        );
        assert_eq!(
            by_id["refresh-generated"]
                .options
                .get("timeout_ms")
                .map(String::as_str),
            Some("60000")
        );
        assert_eq!(
            by_id["refresh-generated"]
                .options
                .get("always_run")
                .map(String::as_str),
            Some("true")
        );
        assert_eq!(by_id["smoketest-capsule"].kind, "amalgamate");
        assert_eq!(
            by_id["smoketest-capsule"].outputs,
            vec![PathBuf::from("$root/.kain/capsules/smoketest.kn")]
        );
        assert_eq!(
            by_id["smoketest-capsule"]
                .options
                .get("storage")
                .map(String::as_str),
            Some("editable")
        );
        assert_eq!(
            by_id["smoketest-capsule"]
                .options
                .get("preview_symbols")
                .map(String::as_str),
            Some("32")
        );
        assert_eq!(
            by_id["smoketest-capsule"]
                .meta
                .get("album")
                .map(String::as_str),
            Some("smoketest")
        );
    }

    #[test]
    fn standalone_task_root_uses_host_lane_target_unit_schema() {
        let root = std::env::current_dir().expect("cwd");
        let config = StandaloneBuildConfig::new(
            root.clone(),
            None,
            Some(BuildLane::Dist),
            Some("llvm".to_string()),
        );
        let task_root = config.task_root("App", "Compile");
        let relative = task_root
            .strip_prefix(root.join(DEFAULT_ARTIFACT_ROOT))
            .expect("task root under artifact root");
        let parts = relative
            .components()
            .map(|component| component.as_os_str().to_string_lossy().to_string())
            .collect::<Vec<_>>();
        assert_eq!(parts[1], "dist");
        assert_eq!(parts[2], "llvm");
        assert_eq!(parts[3], "app");
        assert_eq!(parts[4], "compile");
    }

    #[test]
    fn plan_safety_rejects_output_collisions() {
        let root = std::env::current_dir().expect("cwd");
        let output = root.join(".kain").join("out").join("collision.txt");
        let plan = BladeBuildPlan {
            schema_version: BUILD_ARTIFACT_SCHEMA_VERSION,
            workspace_root: root.clone(),
            artifact_root: root.join(".kain").join("out"),
            cache_root: root.join(".kain").join("cache").join("build"),
            report_root: root.join(".kain").join("reports").join("build"),
            host: default_target_name(),
            lane: BuildLane::Dev,
            profile: "debug".to_string(),
            target: "test".to_string(),
            build_graph: None,
            tasks: vec![
                test_task_with_outputs("a", vec![output.clone()]),
                test_task_with_outputs("b", vec![output]),
            ],
        };
        assert!(validate_plan_safety(&plan).is_err());
    }

    #[test]
    fn build_graph_provenance_prefers_build_kn_over_manifest_defaults() {
        let root = unique_test_dir("build-graph-authority");
        let manifest_path = root.join("KAIN.toml");
        std::fs::write(
            &manifest_path,
            "[package]\nname = \"probe\"\n\n[build]\nentry = \"src/main.kn\"\n\n[[build.tasks]]\nid = \"manifest-check\"\nkind = \"check\"\nentry = \"src/main.kn\"\n\n[[platform.packages]]\nname = \"tiny_math\"\nprovider = \"fixture\"\n",
        )
        .expect("write KAIN.toml");
        std::fs::write(
            root.join("build.kn"),
            r#"
fn build(ctx: BuildContext) -> BuildGraph:
    let vk = platform_package("vulkan").provider("system")
    let tiny = platform_package("tiny_math")
    let check = build_task("script-check").kind("check").entry("src/main.kn").target("llvm")
    return build_graph().require(vk).require(tiny).task(check)
"#,
        )
        .expect("write build.kn");

        let provenance = discover_build_graph_provenance(&root, Some(&manifest_path))
            .expect("graph provenance")
            .expect("provenance present");

        assert_eq!(provenance.graph_source, "build.kn");
        assert_eq!(provenance.defaults_merged_from, Some(manifest_path));
        assert!(provenance
            .overrides
            .iter()
            .any(|value| value.contains("build graph authority")));
        assert!(provenance
            .overrides
            .iter()
            .any(|value| value.contains("overrides KAIN.toml platform packages")));
        assert!(provenance
            .overrides
            .iter()
            .any(|value| value.contains("overrides KAIN.toml explicit build tasks")));
        assert_eq!(
            provenance
                .platform_packages
                .iter()
                .map(|package| (package.package.as_str(), package.provider.as_str()))
                .collect::<Vec<_>>(),
            vec![("tiny_math", "system"), ("vulkan", "system")]
        );
        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn project_plan_consumes_evaluated_build_kn_graph() {
        let root = unique_test_dir("evaluated-project-plan");
        let src = root.join("src");
        kfs::create_dir_all(&src).expect("src dir");
        kfs::write_text(src.join("main.kn"), "fn main() -> Int:\n    return 0\n").expect("main");
        kfs::write_text(
            root.join("build.kn"),
            r#"
use std::build

const TRACKS = ["smoke", "abuse"]

fn check_track(name: String) -> BuildTask:
    return check_task("check-" + name)
        .entry("src/main.kn")
        .target("interpret")

fn build(ctx: BuildContext) -> BuildGraph:
    let app = project("eval-plan")
        .entry("src/main.kn")
        .targets("interpret")
        .artifact_root(".kain/eval-out")
    let sources = source_set("sources")
        .glob("src/**/*.kn")
    let base = check_task("check-base")
        .project(app)
        .inputs(sources)
    let tracks = map(TRACKS, check_track)
    return build_graph(app).sources(sources).tasks(base, tracks)
"#,
        )
        .expect("build script");

        let plan = plan_kain_project(&KainProjectBuildOptions::new(&root)).expect("plan");
        assert_eq!(
            comparable_test_path(&plan.artifact_root),
            comparable_test_path(&root.join(".kain").join("eval-out"))
        );
        assert_eq!(
            plan.build_graph
                .as_ref()
                .map(|graph| graph.graph_source.as_str()),
            Some("build.kn:evaluated")
        );
        let task_ids = plan
            .tasks
            .iter()
            .map(|task| task.id.as_str())
            .collect::<BTreeSet<_>>();
        assert!(task_ids.contains("kain-compile:eval-plan:interpret"));
        assert!(task_ids.contains("check-base"));
        assert!(task_ids.contains("check-smoke"));
        assert!(task_ids.contains("check-abuse"));
        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn build_graph_script_tasks_override_manifest_tasks() {
        let root = unique_test_dir("build-graph-task-authority");
        let manifest_path = root.join("KAIN.toml");
        std::fs::write(
            &manifest_path,
            "[package]\nname = \"probe\"\n\n[build]\nentry = \"src/main.kn\"\n\n[[build.tasks]]\nid = \"manifest-check\"\nkind = \"check\"\nentry = \"src/main.kn\"\n",
        )
        .expect("write KAIN.toml");
        std::fs::write(
            root.join("build.kn"),
            r#"
fn build(ctx: BuildContext) -> BuildGraph:
    let check = build_task("script-check")
        .kind("check")
        .entry("src/main.kn")
        .target("llvm")
    return build_graph().task(check)
"#,
        )
        .expect("write build.kn");

        let manifest = blade::load_kain_manifest(&manifest_path).expect("load manifest");
        let selected = select_explicit_build_task_sections(&root, &manifest.build.tasks)
            .expect("select explicit tasks");
        assert_eq!(selected.len(), 1);
        assert_eq!(selected[0].id, "script-check");
        assert_eq!(selected[0].target.as_deref(), Some("llvm"));
        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn build_graph_script_without_tasks_falls_back_to_manifest_tasks() {
        let root = unique_test_dir("build-graph-task-fallback");
        let manifest_path = root.join("KAIN.toml");
        std::fs::write(
            &manifest_path,
            "[package]\nname = \"probe\"\n\n[build]\nentry = \"src/main.kn\"\n\n[[build.tasks]]\nid = \"manifest-check\"\nkind = \"check\"\nentry = \"src/main.kn\"\n",
        )
        .expect("write KAIN.toml");
        std::fs::write(
            root.join("build.kn"),
            r#"
fn build(ctx: BuildContext) -> BuildGraph:
    return build_graph().require(platform_package("tiny_math").provider("fixture"))
"#,
        )
        .expect("write build.kn");

        let manifest = blade::load_kain_manifest(&manifest_path).expect("load manifest");
        let selected = select_explicit_build_task_sections(&root, &manifest.build.tasks)
            .expect("select explicit tasks");
        assert_eq!(selected.len(), 1);
        assert_eq!(selected[0].id, "manifest-check");

        let provenance = discover_build_graph_provenance(&root, Some(&manifest_path))
            .expect("graph provenance")
            .expect("provenance present");
        assert!(provenance
            .overrides
            .iter()
            .any(|value| value.contains("defers explicit build tasks to KAIN.toml")));
        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn build_graph_provenance_manifest_platform_packages_match_build_kn_graph() {
        let manifest_source = "[package]\nname = \"probe\"\n\n[[platform.packages]]\nname = \"tiny_math\"\nprovider = \"fixture\"\n\n[[platform.packages]]\npackage = \"vulkan\"\nprovider = \"system\"\n";
        let build_source = r#"
fn build(ctx: BuildContext) -> BuildGraph:
    let vk = platform_package("vulkan").provider("system")
    let tiny = platform_package("tiny_math").provider("fixture")
    return build_graph().require(vk).require(tiny)
"#;

        let manifest_packages = extract_manifest_build_graph_platform_packages(manifest_source);
        let build_packages = extract_build_graph_platform_packages(build_source, "build.kn");
        assert_eq!(
            platform_package_pairs(&manifest_packages),
            platform_package_pairs(&build_packages)
        );
    }

    #[test]
    fn build_graph_provenance_uses_kain_toml_when_no_script_exists() {
        let root = unique_test_dir("manifest-platform-graph");
        let manifest_path = root.join("KAIN.toml");
        std::fs::write(
            &manifest_path,
            "[package]\nname = \"probe\"\n\n[[platform.packages]]\nname = \"tiny_math\"\nprovider = \"fixture\"\n",
        )
        .expect("write KAIN.toml");

        let provenance = discover_build_graph_provenance(&root, Some(&manifest_path))
            .expect("graph provenance")
            .expect("provenance present");

        assert_eq!(provenance.graph_source, "KAIN.toml");
        assert_eq!(provenance.platform_packages.len(), 1);
        assert_eq!(provenance.platform_packages[0].package, "tiny_math");
        assert_eq!(provenance.platform_packages[0].provider, "fixture");
        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn build_graph_provenance_uses_platform_kn_when_build_kn_absent() {
        let root = unique_test_dir("platform-graph-authority");
        std::fs::write(
            root.join("platform.kn"),
            r#"
fn build(ctx: BuildContext) -> BuildGraph:
    return build_graph().require(platform_package("tiny_math").provider("fixture"))
"#,
        )
        .expect("write platform.kn");

        let provenance = discover_build_graph_provenance(&root, None)
            .expect("graph provenance")
            .expect("provenance present");

        assert_eq!(provenance.graph_source, "platform.kn");
        assert_eq!(provenance.defaults_merged_from, None);
        assert_eq!(provenance.platform_packages.len(), 1);
        assert_eq!(provenance.platform_packages[0].package, "tiny_math");
        assert_eq!(provenance.platform_packages[0].provider, "fixture");
        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn explicit_build_task_dependencies_use_blade_scope() {
        let root = unique_test_dir("explicit-task-deps");
        let config = BuildWorkspaceConfig {
            workspace_root: root.clone(),
            artifact_root: root.join(".kain").join("out"),
            cache_root: root.join(".kain").join("cache").join("build"),
            report_root: root.join(".kain").join("reports").join("build"),
            host: default_target_name(),
            lane: BuildLane::Dev,
            profile: "debug".to_string(),
            target: default_target_name(),
        };
        let blade = ResolvedBlade {
            name: "ProbeBlade".to_string(),
            version: None,
            kind: "kain_library".to_string(),
            root: root.clone(),
            manifest_path: None,
            kain_manifest: None,
            cargo_manifest: None,
            rust_crate_name: None,
            fabric_manifest: None,
            entry: Some(root.join("src").join("main.kn")),
            source_roots: Vec::new(),
            module_roots: Vec::new(),
            build_targets: vec!["llvm".to_string()],
            dependencies: Vec::new(),
            artifacts: BTreeMap::new(),
            c_ffi_libraries: Vec::new(),
            gpu_shader_sources: Vec::new(),
            gpu_shader_roots: Vec::new(),
            compute_keys: Vec::new(),
            discovery_source: "test".to_string(),
        };
        let task = KainBuildTaskSection {
            id: "check".to_string(),
            kind: "check".to_string(),
            entry: Some(PathBuf::from("src/main.kn")),
            depends_on: vec!["prep-assets".to_string(), "shared:bundle".to_string()],
            ..KainBuildTaskSection::default()
        };

        let resolved =
            build_explicit_task(&config, Some(&blade), &blade.root, &task).expect("build task");
        assert_eq!(resolved.id, "probeblade:check");
        assert_eq!(
            resolved.depends_on,
            vec![
                "probeblade:prep-assets".to_string(),
                "shared:bundle".to_string()
            ]
        );
        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn explicit_evidence_task_kinds_are_first_class_build_tasks() {
        let root = unique_test_dir("explicit-evidence-kinds");
        let config = test_workspace_config(&root);
        let cases = [
            ("suite", "test", BuildTaskKind::Test, true),
            ("proofs", "proof", BuildTaskKind::Proof, true),
            ("bench", "benchmark", BuildTaskKind::Benchmark, false),
            ("abuse", "attrition", BuildTaskKind::Attrition, false),
            ("gate", "certify", BuildTaskKind::Certify, false),
        ];

        for (id, kind, expected_kind, needs_entry) in cases {
            let mut task = KainBuildTaskSection {
                id: id.to_string(),
                kind: kind.to_string(),
                ..KainBuildTaskSection::default()
            };
            if needs_entry {
                task.entry = Some(PathBuf::from("src/main.kn"));
            }
            let resolved =
                build_explicit_task(&config, None, &root, &task).expect("build evidence task");
            assert_eq!(resolved.kind, expected_kind);
            assert!(!resolved.cacheable);
            assert!(
                resolved
                    .outputs
                    .iter()
                    .any(|path| path.file_name().and_then(OsStr::to_str)
                        == Some("kain-evidence.json"))
            );
        }
        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn explicit_exec_and_amalgamate_tasks_are_first_class_build_tasks() {
        let root = unique_test_dir("explicit-exec-amalgamate");
        let config = test_workspace_config(&root);

        let exec_task = KainBuildTaskSection {
            id: "refresh-generated".to_string(),
            kind: "exec".to_string(),
            command: Some("cargo".to_string()),
            args: vec!["run".to_string(), "-q".to_string()],
            env: BTreeMap::from([(
                "CARGO_TARGET_DIR".to_string(),
                "$root/target/codex-build".to_string(),
            )]),
            options: BTreeMap::from([
                ("stdout".to_string(), "$task/stdout.txt".to_string()),
                ("stderr".to_string(), "$task/stderr.txt".to_string()),
                ("timeout_ms".to_string(), "60000".to_string()),
                ("always_run".to_string(), "true".to_string()),
            ]),
            outputs: vec![PathBuf::from("$task/out.txt")],
            ..KainBuildTaskSection::default()
        };
        let resolved_exec =
            build_explicit_task(&config, None, &root, &exec_task).expect("build exec task");
        assert_eq!(resolved_exec.kind, BuildTaskKind::Exec);
        assert!(!resolved_exec.cacheable);
        assert!(resolved_exec
            .outputs
            .iter()
            .any(|path| path.file_name().and_then(OsStr::to_str) == Some("kain-exec.json")));
        assert!(resolved_exec
            .outputs
            .iter()
            .any(|path| path.file_name().and_then(OsStr::to_str) == Some("stdout.txt")));

        let capsule_task = KainBuildTaskSection {
            id: "smoketest-capsule".to_string(),
            kind: "amalgamate".to_string(),
            entry: Some(PathBuf::from(".")),
            outputs: vec![PathBuf::from("$root/.kain/capsules/smoketest.kn")],
            options: BTreeMap::from([
                ("name".to_string(), "smoketest".to_string()),
                ("version".to_string(), "0.1.0".to_string()),
                ("storage".to_string(), "editable".to_string()),
                ("contents".to_string(), "source".to_string()),
                ("capsule_set".to_string(), "smoketest".to_string()),
                ("header".to_string(), "rich".to_string()),
                ("preview_symbols".to_string(), "32".to_string()),
            ]),
            tags: vec!["portable".to_string()],
            meta: BTreeMap::from([("album".to_string(), "smoketest".to_string())]),
            ..KainBuildTaskSection::default()
        };
        let resolved_capsule = build_explicit_task(&config, None, &root, &capsule_task)
            .expect("build amalgamate task");
        assert_eq!(resolved_capsule.kind, BuildTaskKind::Amalgamate);
        assert!(resolved_capsule.cacheable);
        assert!(resolved_capsule.inputs.iter().any(|path| path == &root));
        assert_eq!(
            capsule_task.options.get("contents").map(String::as_str),
            Some("source")
        );
        assert_eq!(
            capsule_task.options.get("capsule_set").map(String::as_str),
            Some("smoketest")
        );
        assert!(resolved_capsule
            .outputs
            .iter()
            .any(|path| { path.file_name().and_then(OsStr::to_str) == Some("smoketest.kn") }));
        assert!(resolved_capsule.outputs.iter().any(|path| {
            path.file_name().and_then(OsStr::to_str) == Some("kain-amalgamate.json")
        }));

        let _ = std::fs::remove_dir_all(root);
    }

    #[cfg(windows)]
    #[test]
    fn process_portable_values_strip_windows_verbatim_prefixes() {
        let drive = windows_test_drive_prefix();
        assert_eq!(
            process_portable_string(&format!(r"\\?\{}\repo\smoketest\telemetry\full", drive)),
            format!(r"{}\repo\smoketest\telemetry\full", drive)
        );
        assert_eq!(
            process_portable_string(r"\\?\UNC\server\share\album.kn"),
            r"\\server\share\album.kn"
        );
        assert_eq!(
            process_portable_path(Path::new(&format!(
                r"\\?\{}\repo\smoketest\smoketest.exe",
                drive
            ))),
            PathBuf::from(format!(r"{}\repo\smoketest\smoketest.exe", drive))
        );
    }

    #[cfg(windows)]
    #[test]
    fn resolve_build_graph_string_value_renders_portable_windows_paths() {
        let drive = windows_test_drive_prefix();
        let workspace_root_value = format!(r"\\?\{}\repo\smoketest", drive);
        let task_root_value = format!(r"\\?\{}\repo\smoketest\.kain\out\task", drive);
        let workspace_root = Path::new(&workspace_root_value);
        let root = workspace_root;
        let task_root = Path::new(&task_root_value);
        assert_eq!(
            resolve_build_graph_string_value(
                workspace_root,
                root,
                task_root,
                "$root/telemetry/attrition"
            ),
            format!(r"{}\repo\smoketest\telemetry\attrition", drive)
        );
        assert_eq!(
            resolve_build_graph_string_value(workspace_root, root, task_root, "$task/runner.json"),
            format!(r"{}\repo\smoketest\.kain\out\task\runner.json", drive)
        );
    }

    #[test]
    fn native_executable_task_can_target_repo_root_output() {
        let root = unique_test_dir("native-executable-root-output");
        let config = test_workspace_config(&root);
        let task = KainBuildTaskSection {
            id: "root-exe".to_string(),
            kind: "native-executable".to_string(),
            entry: Some(PathBuf::from("src/main.kn")),
            outputs: vec![PathBuf::from("$root/bin/probe.exe")],
            ..KainBuildTaskSection::default()
        };

        let resolved =
            build_explicit_task(&config, None, &root, &task).expect("build native exe task");
        assert_eq!(resolved.kind, BuildTaskKind::NativeExecutable);
        assert!(resolved.cacheable);
        assert!(resolved
            .outputs
            .contains(&root.join("bin").join("probe.exe")));
        assert!(resolved
            .outputs
            .iter()
            .any(|path| path.file_name().and_then(OsStr::to_str) == Some("kain-evidence.json")));
        assert!(resolved
            .inputs
            .iter()
            .any(|path| path.ends_with("compile_kain_project_to_root.ps1")));
        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn kain_frontend_task_stamp_changes_when_stdlib_changes() {
        let root = unique_test_dir("kain-frontend-stdlib-cache");
        std::fs::create_dir_all(root.join("stdlib")).expect("create stdlib");
        std::fs::write(
            root.join("stdlib").join("reflect.kn"),
            "pub fn marker() -> Int:\n    return 1\n",
        )
        .expect("write stdlib");
        let config = test_workspace_config(&root);
        let task = KainBuildTaskSection {
            id: "root-exe".to_string(),
            kind: "native-executable".to_string(),
            entry: Some(PathBuf::from("src/main.kn")),
            outputs: vec![PathBuf::from("$root/bin/probe.exe")],
            ..KainBuildTaskSection::default()
        };
        let resolved =
            build_explicit_task(&config, None, &root, &task).expect("build native exe task");
        assert!(resolved
            .inputs
            .iter()
            .any(|path| path.file_name().and_then(OsStr::to_str) == Some("stdlib")));

        let plan = BladeBuildPlan {
            schema_version: BUILD_ARTIFACT_SCHEMA_VERSION,
            workspace_root: config.workspace_root.clone(),
            artifact_root: config.artifact_root.clone(),
            cache_root: config.cache_root.clone(),
            report_root: config.report_root.clone(),
            host: config.host.clone(),
            lane: config.lane,
            profile: config.profile.clone(),
            target: config.target.clone(),
            build_graph: None,
            tasks: vec![resolved.clone()],
        };
        let before = task_stamp(&resolved, &plan).expect("stamp before");
        std::fs::write(
            root.join("stdlib").join("reflect.kn"),
            "pub fn marker() -> Int:\n    return 2\n",
        )
        .expect("rewrite stdlib");
        let after = task_stamp(&resolved, &plan).expect("stamp after");
        assert_ne!(before, after);

        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn task_stamp_changes_when_dependency_stamp_changes() {
        let root = unique_test_dir("dependency-stamp-cache");
        let config = test_workspace_config(&root);
        let mut parent = test_task_with_outputs("parent", vec![root.join("parent.out")]);
        parent.depends_on = vec!["child".to_string()];
        let plan = BladeBuildPlan {
            schema_version: BUILD_ARTIFACT_SCHEMA_VERSION,
            workspace_root: config.workspace_root.clone(),
            artifact_root: config.artifact_root.clone(),
            cache_root: config.cache_root.clone(),
            report_root: config.report_root.clone(),
            host: config.host.clone(),
            lane: config.lane,
            profile: config.profile.clone(),
            target: config.target.clone(),
            build_graph: None,
            tasks: vec![parent.clone()],
        };
        let child_stamp = stamp_path_for_id("child", &plan);
        std::fs::create_dir_all(child_stamp.parent().expect("stamp parent"))
            .expect("create stamps");
        std::fs::write(&child_stamp, "first").expect("write first stamp");
        let before = task_stamp(&parent, &plan).expect("stamp before");
        std::fs::write(&child_stamp, "second").expect("write second stamp");
        let after = task_stamp(&parent, &plan).expect("stamp after");
        assert_ne!(before, after);

        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn native_executable_task_stamp_changes_when_runtime_header_changes() {
        let root = unique_test_dir("native-executable-runtime-cache");
        std::fs::create_dir_all(root.join("src")).expect("create src");
        std::fs::write(
            root.join("src").join("main.kn"),
            "fn main() -> Int:\n    return 0\n",
        )
        .expect("write source");
        std::fs::create_dir_all(
            root.join(".agents")
                .join("skills")
                .join("lang-projects")
                .join("scripts"),
        )
        .expect("create script dir");
        std::fs::write(
            root.join(".agents")
                .join("skills")
                .join("lang-projects")
                .join("scripts")
                .join("compile_kain_project_to_root.ps1"),
            "# stub\n",
        )
        .expect("write helper script");
        std::fs::create_dir_all(root.join("runtime").join("native").join("include"))
            .expect("create include dir");
        std::fs::create_dir_all(root.join("runtime").join("native").join("src").join("core"))
            .expect("create runtime source dir");
        std::fs::write(
            root.join("runtime").join("native_core_runtime.toml"),
            r#"
name = "test-runtime"
sources = ["native/src/core/probe.c"]
include_dirs = ["native/include"]
"#,
        )
        .expect("write runtime manifest");
        std::fs::write(
            root.join("runtime")
                .join("native")
                .join("include")
                .join("probe.h"),
            "#define PROBE_VALUE 7\n",
        )
        .expect("write public header");
        let local_header = root
            .join("runtime")
            .join("native")
            .join("src")
            .join("core")
            .join("probe_local.h");
        std::fs::write(&local_header, "#define PROBE_LOCAL 11\n").expect("write local header");
        std::fs::write(
            root.join("runtime")
                .join("native")
                .join("src")
                .join("core")
                .join("probe.c"),
            "#include \"../../include/probe.h\"\n#include \"probe_local.h\"\nint probe_value(void) { return PROBE_VALUE + PROBE_LOCAL; }\n",
        )
        .expect("write runtime source");

        let config = test_workspace_config(&root);
        let task = KainBuildTaskSection {
            id: "root-exe".to_string(),
            kind: "native-executable".to_string(),
            entry: Some(PathBuf::from("src/main.kn")),
            outputs: vec![PathBuf::from("$root/bin/probe.exe")],
            ..KainBuildTaskSection::default()
        };
        let resolved =
            build_explicit_task(&config, None, &root, &task).expect("build native exe task");
        assert!(resolved
            .inputs
            .iter()
            .any(|path| path.ends_with(Path::new("probe_local.h"))));
        assert!(resolved
            .inputs
            .iter()
            .any(|path| path.ends_with(Path::new("runtime").join("native_core_runtime.toml"))));

        let plan = BladeBuildPlan {
            schema_version: BUILD_ARTIFACT_SCHEMA_VERSION,
            workspace_root: config.workspace_root.clone(),
            artifact_root: config.artifact_root.clone(),
            cache_root: config.cache_root.clone(),
            report_root: config.report_root.clone(),
            host: config.host.clone(),
            lane: config.lane,
            profile: config.profile.clone(),
            target: config.target.clone(),
            build_graph: None,
            tasks: vec![resolved.clone()],
        };
        let before = task_stamp(&resolved, &plan).expect("stamp before");
        std::fs::write(&local_header, "#define PROBE_LOCAL 29\n").expect("rewrite local header");
        let after = task_stamp(&resolved, &plan).expect("stamp after");
        assert_ne!(before, after);

        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn kain_check_task_writes_output_marker_for_cache_hits() {
        let root = unique_test_dir("kain-check-cache-output");
        std::fs::create_dir_all(root.join("src")).expect("create src");
        std::fs::write(
            root.join("src").join("main.kn"),
            "fn main() -> Int:\n    return 0\n",
        )
        .expect("write source");

        let config = test_workspace_config(&root);
        let output_path = config.artifact_root.join("llvm").join("check-main.json");
        let task = BuildTask {
            id: "check-main".to_string(),
            kind: BuildTaskKind::KainCheck,
            blade: None,
            description: "check-main".to_string(),
            depends_on: Vec::new(),
            inputs: vec![root.join("src").join("main.kn")],
            outputs: vec![output_path.clone()],
            required_capabilities: Vec::new(),
            matrix_axes: Vec::new(),
            telemetry: Vec::new(),
            certifies: Vec::new(),
            cacheable: true,
            adapter: BuildTaskAdapter::KainCheck {
                entry: root.join("src").join("main.kn"),
                target: CompileTarget::Llvm,
            },
        };
        let plan = BladeBuildPlan {
            schema_version: BUILD_ARTIFACT_SCHEMA_VERSION,
            workspace_root: config.workspace_root.clone(),
            artifact_root: config.artifact_root.clone(),
            cache_root: config.cache_root.clone(),
            report_root: config.report_root.clone(),
            host: config.host.clone(),
            lane: config.lane,
            profile: config.profile.clone(),
            target: config.target.clone(),
            build_graph: None,
            tasks: vec![task.clone()],
        };

        let message = run_kain_check(
            &root.join("src").join("main.kn"),
            CompileTarget::Llvm,
            None,
            Some(&output_path),
        )
        .expect("kain check should pass");
        assert!(message.contains("checked"));
        assert!(
            output_path.exists(),
            "kain-check should materialize its declared output"
        );
        write_task_stamp(&task, &plan).expect("write stamp");
        assert!(
            task_is_cached(&task, &plan).expect("cache probe"),
            "kain-check task should become cacheable once it writes its output marker"
        );

        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn blade_root_workspace_does_not_duplicate_explicit_tasks_as_root_tasks() {
        let root = unique_test_dir("blade-root-task-dedup");
        std::fs::create_dir_all(root.join("src")).expect("create src");
        std::fs::write(
            root.join("src").join("main.kn"),
            "fn main() -> Int:\n    return 0\n",
        )
        .expect("write source");
        std::fs::write(
            root.join("build.kn"),
            r#"
fn build(ctx: BuildContext) -> BuildGraph:
    let spec = blade("probe").entry("src/main.kn").source_root("src").build_target("llvm")
    let root_exe = build_task("root-executable")
        .kind("native-executable")
        .entry("src/main.kn")
        .output("$blade/probe.exe")
    return build_graph().blade(spec).task(root_exe)
"#,
        )
        .expect("write build.kn");

        let options = BladeBuildOptions {
            dry_run: true,
            ..BladeBuildOptions::new(&root)
        };
        let plan = plan_blade_workspace(&options).expect("plan blade workspace");
        let root_executable_ids = plan
            .tasks
            .iter()
            .filter(|task| task.id.ends_with("root-executable"))
            .map(|task| task.id.as_str())
            .collect::<Vec<_>>();

        assert_eq!(root_executable_ids, vec!["probe:root-executable"]);
        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn shader_artifact_source_extracts_kain_example_shaders_without_native_body() {
        let source = include_str!("../../../blades/example/src/main.kn");

        let extracted = shader_artifact_source(source)
            .expect("kain-example native source should yield shader-only source");
        let bundle = kain_driver::compile_shader_artifact_bundle(&extracted)
            .expect("kain-example extracted shader source should compile");

        assert!(extracted.contains("shader fragment NativeExampleGradient"));
        assert!(extracted.contains("shader compute NativeExampleBlendKernel"));
        assert!(!extracted.contains("native_runtime_heap_validate"));
        assert!(!extracted.contains("fn main()"));
        assert!(bundle.bundle_json.contains("NativeExampleGradient"));
    }

    fn test_task(id: &str, depends_on: Vec<&str>) -> BuildTask {
        BuildTask {
            id: id.to_string(),
            kind: BuildTaskKind::BladeCheck,
            blade: None,
            description: id.to_string(),
            depends_on: depends_on.into_iter().map(str::to_string).collect(),
            inputs: Vec::new(),
            outputs: Vec::new(),
            required_capabilities: Vec::new(),
            matrix_axes: Vec::new(),
            telemetry: Vec::new(),
            certifies: Vec::new(),
            cacheable: false,
            adapter: BuildTaskAdapter::BladeCheck,
        }
    }

    fn test_task_with_outputs(id: &str, outputs: Vec<PathBuf>) -> BuildTask {
        BuildTask {
            outputs,
            ..test_task(id, Vec::new())
        }
    }

    fn comparable_test_path(path: &Path) -> String {
        path.display()
            .to_string()
            .trim_start_matches(r"\\?\")
            .replace('\\', "/")
    }

    fn unique_test_dir(name: &str) -> PathBuf {
        let root = std::env::temp_dir().join(format!(
            "kain-build-{name}-{}-{}",
            std::process::id(),
            unix_timestamp_ms()
        ));
        if root.exists() {
            let _ = std::fs::remove_dir_all(&root);
        }
        std::fs::create_dir_all(&root).expect("create test directory");
        root
    }

    fn test_workspace_config(root: &Path) -> BuildWorkspaceConfig {
        BuildWorkspaceConfig {
            workspace_root: root.to_path_buf(),
            artifact_root: root.join(".kain").join("out"),
            cache_root: root.join(".kain").join("cache").join("build"),
            report_root: root.join(".kain").join("reports").join("build"),
            host: default_target_name(),
            lane: BuildLane::Dev,
            profile: "debug".to_string(),
            target: default_target_name(),
        }
    }
}
