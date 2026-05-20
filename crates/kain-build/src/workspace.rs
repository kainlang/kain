use blade::{
    discover_workspace, load_kain_manifest, BladeError, BladeWorkspace, KainBuildTaskSection,
    KainManifest, ResolvedBlade, ResolvedCffiLibrary, FABRIC_MANIFEST_NAME,
};
use kain_core::ast::{Item, Program};
use kain_core::diagnostics::SpanMapper;
use kain_core::format_program;
use kain_core::lexer::Lexer;
use kain_core::parser::Parser;
use kain_core::CompileTarget;
use kain_fs as kfs;
use kain_omni::fabric::{FabricRuntimeKind, FabricSessionStatus};
use serde::{Deserialize, Serialize};
use serde_json::json;
use sha2::{Digest, Sha256};
use std::collections::{BTreeMap, BTreeSet};
use std::ffi::OsStr;
use std::path::{Component, Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::{SystemTime, UNIX_EPOCH};

const DEFAULT_PROFILE: &str = "debug";
const DEFAULT_ARTIFACT_ROOT: &str = ".kain/out";
const DEFAULT_CACHE_ROOT: &str = ".kain/cache/build";
const DEFAULT_REPORT_ROOT: &str = ".kain/reports/build";
const BUILD_ADAPTER_VERSION: &str = "kain-build-v2";
const BUILD_ARTIFACT_SCHEMA_VERSION: u32 = 2;
const BUILD_GRAPH_SCRIPT_NAMES: [&str; 2] = ["build.kn", "platform.kn"];

pub type BuildResult<T> = Result<T, BuildError>;

#[derive(Debug, thiserror::Error)]
pub enum BuildError {
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
    #[error("filesystem error: {0}")]
    Fs(#[from] kain_fs::FsError),
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

#[derive(Debug, Clone, Serialize)]
pub struct BuildTask {
    pub id: String,
    pub kind: BuildTaskKind,
    pub blade: Option<String>,
    pub description: String,
    pub depends_on: Vec<String>,
    pub inputs: Vec<PathBuf>,
    pub outputs: Vec<PathBuf>,
    pub cacheable: bool,
    #[serde(skip)]
    adapter: BuildTaskAdapter,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
#[serde(rename_all = "kebab-case")]
pub enum BuildTaskKind {
    BladeCheck,
    KainCheck,
    KainCompile,
    RustArtifacts,
    NativeUiApp,
    CargoBuild,
    CSharedLibrary,
    GpuArtifacts,
    FabricValidate,
    FabricRun,
    Node,
    Bun,
}

impl BuildTaskKind {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::BladeCheck => "blade-check",
            Self::KainCheck => "kain-check",
            Self::KainCompile => "kain-compile",
            Self::RustArtifacts => "rust-artifacts",
            Self::NativeUiApp => "native-ui-app",
            Self::CargoBuild => "cargo-build",
            Self::CSharedLibrary => "c-shared-library",
            Self::GpuArtifacts => "gpu-artifacts",
            Self::FabricValidate => "fabric-validate",
            Self::FabricRun => "fabric-run",
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
        primary_output: PathBuf,
        materialized_primary_output: Option<PathBuf>,
        root_component: Option<String>,
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
        cpp_options: Vec<String>,
        canonical_output: PathBuf,
        materialized_output: Option<PathBuf>,
    },
    GpuArtifacts {
        source: PathBuf,
        output_base: PathBuf,
    },
    Fabric {
        manifest_path: PathBuf,
        run: bool,
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
    pub profile: Option<String>,
    pub lane: Option<BuildLane>,
    pub dry_run: bool,
    pub clean: bool,
    pub fail_fast: bool,
}

impl KainFileBuildOptions {
    pub fn new(input: impl Into<PathBuf>, target: CompileTarget) -> Self {
        Self {
            input: input.into(),
            output: None,
            target,
            profile: None,
            lane: None,
            dry_run: false,
            clean: false,
            fail_fast: true,
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
    let root_manifest = workspace
        .manifest_path
        .as_ref()
        .map(|path| load_kain_manifest(path))
        .transpose()?;
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
                .filter_map(|blade| blade.manifest_path.clone())
                .collect(),
            outputs: Vec::new(),
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
                cacheable: false,
                adapter: BuildTaskAdapter::Fabric {
                    manifest_path,
                    run: true,
                },
            });
        }
    }

    if let Some(manifest) = root_manifest.as_ref() {
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
            "Compile {} to {}",
            source.display(),
            kain_driver::target_extension(options.target)
        ),
        depends_on: Vec::new(),
        inputs: vec![source.clone()],
        outputs,
        cacheable: true,
        adapter: BuildTaskAdapter::KainCompile {
            source,
            target: options.target,
            primary_output,
            materialized_primary_output,
            root_component: None,
        },
    }];
    let plan = config.into_plan(kain_driver::target_extension(options.target), tasks);
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
    let manifest = load_project_manifest(&manifest_path)?;
    let lane = options
        .lane
        .or_else(|| options.profile.as_deref().and_then(BuildLane::parse))
        .unwrap_or_default();
    let profile = options
        .profile
        .clone()
        .unwrap_or_else(|| lane.cargo_profile().to_string());
    let config = StandaloneBuildConfig::new(
        workspace_root.clone(),
        Some(profile),
        Some(lane),
        Some("project".to_string()),
    );
    let package_name = manifest
        .package
        .as_ref()
        .map(|package| sanitize_id(&package.name))
        .filter(|value| !value.is_empty())
        .unwrap_or_else(|| {
            workspace_root
                .file_name()
                .and_then(OsStr::to_str)
                .map(sanitize_id)
                .filter(|value| !value.is_empty())
                .unwrap_or_else(|| "project".to_string())
        });
    let build = manifest.build.unwrap_or_default();
    let entry = resolve_workspace_path(&workspace_root, &build.entry);
    if !entry.exists() {
        return Err(BuildError::Config(format!(
            "Entry file not found: {}",
            entry.display()
        )));
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
            inputs: vec![entry.clone(), manifest_path.clone()],
            outputs: vec![
                task_root.join(format!("{package_name}.rs")),
                artifact_manifest_path(&task_root),
            ],
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
                inputs: vec![entry.clone(), manifest_path.clone()],
                outputs,
                cacheable: true,
                adapter: BuildTaskAdapter::KainCompile {
                    source: entry.clone(),
                    target,
                    primary_output,
                    materialized_primary_output: None,
                    root_component: None,
                },
            });
        }
    }
    let tasks = order_tasks(tasks)?;
    let build_graph = discover_build_graph_provenance(&workspace_root, Some(&manifest_path))?;
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
}

impl From<&BladeBuildOptions> for BuildExecutionOptions {
    fn from(options: &BladeBuildOptions) -> Self {
        Self {
            dry_run: options.dry_run,
            clean: options.clean,
            fail_fast: options.fail_fast,
        }
    }
}

impl From<&KainFileBuildOptions> for BuildExecutionOptions {
    fn from(options: &KainFileBuildOptions) -> Self {
        Self {
            dry_run: options.dry_run,
            clean: options.clean,
            fail_fast: options.fail_fast,
        }
    }
}

impl From<&KainRustBuildOptions> for BuildExecutionOptions {
    fn from(options: &KainRustBuildOptions) -> Self {
        Self {
            dry_run: options.dry_run,
            clean: options.clean,
            fail_fast: options.fail_fast,
        }
    }
}

impl From<&KainNativeUiBuildOptions> for BuildExecutionOptions {
    fn from(options: &KainNativeUiBuildOptions) -> Self {
        Self {
            dry_run: options.dry_run,
            clean: options.clean,
            fail_fast: options.fail_fast,
        }
    }
}

impl From<&KainProjectBuildOptions> for BuildExecutionOptions {
    fn from(options: &KainProjectBuildOptions) -> Self {
        Self {
            dry_run: options.dry_run,
            clean: options.clean,
            fail_fast: options.fail_fast,
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
    let build_script = BUILD_GRAPH_SCRIPT_NAMES
        .iter()
        .map(|name| workspace_root.join(name))
        .find(|path| path.exists());

    if build_script.is_none() && manifest_path.is_none() {
        return Ok(None);
    }

    let Some(build_script) = build_script else {
        return Ok(Some(KainBuildGraphProvenance {
            graph_source: "KAIN.toml".to_string(),
            defaults_merged_from: None,
            build_script: None,
            overrides: Vec::new(),
            platform_packages: Vec::new(),
        }));
    };

    let graph_source = build_script
        .file_name()
        .and_then(OsStr::to_str)
        .unwrap_or("build.kn")
        .to_string();
    let source = kfs::read_text(&build_script)?;
    let platform_packages = extract_build_graph_platform_packages(&source, &graph_source);
    let mut overrides = Vec::new();
    if manifest_path.is_some() {
        overrides.push(format!(
            "{graph_source} is build graph authority; KAIN.toml contributes defaults"
        ));
    }

    Ok(Some(KainBuildGraphProvenance {
        graph_source,
        defaults_merged_from: manifest_path,
        build_script: Some(build_script),
        overrides,
        platform_packages,
    }))
}

fn extract_build_graph_platform_packages(
    source: &str,
    graph_source: &str,
) -> Vec<KainBuildGraphPlatformPackage> {
    let mut packages = Vec::new();
    let mut offset = 0usize;
    while let Some(relative) = source[offset..].find("platform_package") {
        let function_start = offset + relative + "platform_package".len();
        if let Some((package, after_call)) = parse_string_call_argument(source, function_start) {
            let provider =
                parse_provider_chain(source, after_call).unwrap_or_else(|| "system".to_string());
            packages.push(KainBuildGraphPlatformPackage {
                package,
                provider,
                source: graph_source.to_string(),
            });
            offset = after_call;
        } else {
            offset = function_start;
        }
    }
    packages.sort_by(|left, right| {
        left.package
            .cmp(&right.package)
            .then(left.provider.cmp(&right.provider))
    });
    packages
        .dedup_by(|left, right| left.package == right.package && left.provider == right.provider);
    packages
}

fn parse_provider_chain(source: &str, offset: usize) -> Option<String> {
    let line_end = source[offset..]
        .find(|ch| matches!(ch, '\n' | '\r' | ';'))
        .map(|relative| offset + relative)
        .unwrap_or(source.len());
    let limit = line_end.min(offset.saturating_add(512));
    let tail = &source[offset..limit];
    let provider_offset = tail.find(".provider")?;
    parse_string_call_argument(tail, provider_offset + ".provider".len()).map(|(value, _)| value)
}

fn parse_string_call_argument(source: &str, mut index: usize) -> Option<(String, usize)> {
    let bytes = source.as_bytes();
    index = skip_ascii_whitespace(bytes, index);
    if bytes.get(index).copied()? != b'(' {
        return None;
    }
    index += 1;
    index = skip_ascii_whitespace(bytes, index);
    parse_quoted_string(source, index)
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
            cacheable: true,
            adapter: BuildTaskAdapter::CSharedLibrary {
                library_name: library.name.clone(),
                sources,
                header: library.header.clone(),
                include_paths: library.include_paths.clone(),
                defines: library.defines.clone(),
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
        let outputs = gpu_output_paths(&output_base);
        tasks.push(BuildTask {
            id: task_id,
            kind: BuildTaskKind::GpuArtifacts,
            blade: Some(blade.name.clone()),
            description: format!("Emit GPU artifacts for {}", source.display()),
            depends_on: Vec::new(),
            inputs: vec![source.clone()],
            outputs,
            cacheable: true,
            adapter: BuildTaskAdapter::GpuArtifacts {
                source: source.clone(),
                output_base,
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
    let Some(manifest_path) = &blade.kain_manifest else {
        return Ok(());
    };
    let manifest = load_kain_manifest(manifest_path)?;
    for task in &manifest.build.tasks {
        let resolved = build_explicit_task(config, Some(blade), &blade.root, task)?;
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
    for task in &manifest.build.tasks {
        tasks.push(build_explicit_task(
            config,
            None,
            &config.workspace_root,
            task,
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
        "cargo" | "rust" | "rust-crate" => BuildTaskKind::CargoBuild,
        "c" | "c-shared-library" | "c_ffi" => BuildTaskKind::CSharedLibrary,
        "gpu" | "gpu-artifacts" => BuildTaskKind::GpuArtifacts,
        "fabric" | "fabric-run" => BuildTaskKind::FabricRun,
        "fabric-validate" => BuildTaskKind::FabricValidate,
        "node" => BuildTaskKind::Node,
        "bun" => BuildTaskKind::Bun,
        other => {
            return Err(BuildError::Config(format!(
                "explicit build task '{}' has unsupported kind '{}'",
                task.id, other
            )));
        }
    };
    let task_id = if let Some(blade) = blade {
        format!("{}:{}", sanitize_id(&blade.name), sanitize_id(&task.id))
    } else {
        sanitize_id(&task.id)
    };
    let task_root = config.task_root(
        blade
            .map(|value| value.name.as_str())
            .unwrap_or("workspace"),
        &task_id,
    );
    let inputs = task
        .inputs
        .iter()
        .map(|path| resolve_workspace_path(root, path))
        .collect();
    let outputs = if task.outputs.is_empty() {
        vec![task_root]
    } else {
        task.outputs
            .iter()
            .map(|path| resolve_workspace_path(root, path))
            .collect()
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
                entry: resolve_workspace_path(root, entry),
                target,
            }
        }
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
                manifest_path: resolve_workspace_path(root, manifest_path),
                target_dir: outputs[0].clone(),
                release: task.profile.as_deref().unwrap_or(&config.profile) == "release",
            }
        }
        BuildTaskKind::CSharedLibrary => {
            let sources = task
                .inputs
                .iter()
                .filter(|path| path.extension().and_then(|value| value.to_str()) == Some("c"))
                .map(|path| resolve_workspace_path(root, path))
                .collect::<Vec<_>>();
            let header = task
                .entry
                .as_ref()
                .or_else(|| {
                    task.inputs
                        .iter()
                        .find(|path| path.extension().and_then(|value| value.to_str()) == Some("h"))
                })
                .map(|path| resolve_workspace_path(root, path))
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
            let source = resolve_workspace_path(root, source);
            let output_base = outputs.first().cloned().unwrap_or_else(|| {
                config
                    .task_root("workspace", &task_id)
                    .join(path_stem_or_name(&source))
            });
            BuildTaskAdapter::GpuArtifacts {
                source,
                output_base,
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
                manifest_path: resolve_workspace_path(root, manifest_path),
                run: kind == BuildTaskKind::FabricRun,
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
                entry: task
                    .entry
                    .as_ref()
                    .map(|path| resolve_workspace_path(root, path)),
                command: task.command.clone(),
                args: task.args.clone(),
                cwd: task
                    .cwd
                    .as_ref()
                    .map(|path| resolve_workspace_path(root, path))
                    .unwrap_or_else(|| root.to_path_buf()),
            }
        }
        _ => unreachable!("explicit task kinds are filtered above"),
    };
    let outputs = match &adapter {
        BuildTaskAdapter::GpuArtifacts { output_base, .. } => gpu_output_paths(output_base),
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
            .map(|value| sanitize_id(value))
            .collect(),
        inputs,
        outputs,
        cacheable: !matches!(kind, BuildTaskKind::Node | BuildTaskKind::Bun),
        adapter,
    })
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
    let mut event_writer = EventWriter::new(&events_path)?;
    let mut executions = Vec::new();
    let mut failed = false;
    let workspace = discover_workspace(&plan.workspace_root)?;

    for task in &plan.tasks {
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
                message: task.description.clone(),
                error: None,
            }
        } else {
            execute_task(task, &plan, &workspace)?
        };
        event_writer.write(&execution)?;
        if execution.status == BuildTaskStatus::Failed {
            failed = true;
            if options.fail_fast {
                executions.push(execution);
                break;
            }
        }
        executions.push(execution);
    }

    if failed && !options.fail_fast {
        for task in plan.tasks.iter().skip(executions.len()) {
            executions.push(BuildTaskExecution {
                id: task.id.clone(),
                kind: task.kind,
                blade: task.blade.clone(),
                status: BuildTaskStatus::Skipped,
                cache_hit: false,
                started_unix_ms: None,
                finished_unix_ms: None,
                inputs: task.inputs.clone(),
                outputs: task.outputs.clone(),
                message: "skipped after previous failure".to_string(),
                error: None,
            });
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
            "blade build failed; report written to {}",
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
            message: "cache hit".to_string(),
            error: None,
        });
    }

    let result = match &task.adapter {
        BuildTaskAdapter::BladeCheck => run_blade_check(workspace),
        BuildTaskAdapter::KainCheck { entry, target } => run_kain_check(entry, *target),
        BuildTaskAdapter::KainCompile {
            source,
            target,
            primary_output,
            materialized_primary_output,
            root_component,
        } => run_kain_compile(
            task,
            plan,
            source,
            *target,
            primary_output,
            materialized_primary_output.as_ref(),
            root_component.as_deref(),
        ),
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
            cpp_options,
            canonical_output,
            materialized_output.as_ref(),
        ),
        BuildTaskAdapter::GpuArtifacts {
            source,
            output_base,
        } => run_gpu_artifacts(source, output_base),
        BuildTaskAdapter::Fabric { manifest_path, run } => run_fabric(manifest_path, *run),
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

fn run_kain_check(entry: &Path, target: CompileTarget) -> BuildResult<String> {
    let options = kain_check::CheckOptions::new(target);
    let report = kain_check::check_file(entry, &options);
    if report.passed() {
        Ok(format!("checked {}", entry.display()))
    } else {
        Err(BuildError::Config(report.error.unwrap_or_else(|| {
            format!("Kain check failed for {}", entry.display())
        })))
    }
}

fn run_kain_compile(
    task: &BuildTask,
    plan: &BladeBuildPlan,
    source_path: &Path,
    target: CompileTarget,
    primary_output: &Path,
    materialized_primary_output: Option<&PathBuf>,
    root_component: Option<&str>,
) -> BuildResult<String> {
    let source = kfs::read_text(source_path)?;
    let mut artifacts = Vec::new();
    if let Some(parent) = primary_output.parent() {
        kfs::create_dir_all(parent)?;
    }

    match target {
        CompileTarget::Wasm => {
            let bytes = kain_driver::compile_wasm_binary(&source)?;
            kfs::atomic_write_bytes(primary_output, &bytes)?;
            artifacts.push(record_artifact("primary", primary_output)?);
        }
        CompileTarget::Spirv => {
            let bytes = kain_driver::compile_spirv_binary(&source)?;
            kfs::atomic_write_bytes(primary_output, &bytes)?;
            artifacts.push(record_artifact("primary", primary_output)?);
        }
        CompileTarget::Hybrid => {
            let hybrid = kain_driver::compile_hybrid_artifacts(&source)?;
            artifacts.extend(write_hybrid_artifacts(primary_output, hybrid)?);
        }
        _ => {
            let compiled = kain_driver::compile(&source, target)?;
            kfs::atomic_write_text(primary_output, &compiled)?;
            artifacts.push(record_artifact("primary", primary_output)?);
        }
    }

    if matches!(target, CompileTarget::Llvm | CompileTarget::C) {
        artifacts.extend(stage_native_backend_artifacts(
            &source,
            target,
            primary_output,
            root_component,
        )?);
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

fn run_rust_artifacts(
    task: &BuildTask,
    plan: &BladeBuildPlan,
    source_path: &Path,
    output_base: &Path,
    materialized_output_base: Option<&PathBuf>,
    include_spirv: bool,
) -> BuildResult<String> {
    let source = kfs::read_text(source_path)?;
    if let Some(parent) = output_base.parent() {
        kfs::create_dir_all(parent)?;
    }
    let typed = kain_driver::frontend_to_typed_program(&source, CompileTarget::Rust)?;
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
        let spirv = kain_driver::compile_spirv_binary(&source)?;
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
    source: &str,
    target: CompileTarget,
    output_path: &Path,
    root_component: Option<&str>,
) -> BuildResult<Vec<BuildArtifactRecord>> {
    let mut artifacts = Vec::new();
    let contract_bundle = kain_driver::compile_runtime_contract_bundle(source, target)?;
    let runtime_contract_path = output_path.with_extension("runtime_contract.json");
    kfs::atomic_write_text(
        &runtime_contract_path,
        &kain_core::runtime_contract_bundle_to_json(&contract_bundle).map_err(|err| {
            BuildError::Config(format!("failed to serialize runtime contract: {err}"))
        })?,
    )?;
    artifacts.push(record_artifact("runtime-contract", &runtime_contract_path)?);

    let realtime_bundle = kain_driver::compile_realtime_app_bundle(source, target, root_component)?;
    let realtime_app_path = output_path.with_extension("realtime_app.json");
    kfs::atomic_write_text(&realtime_app_path, &realtime_bundle.bundle_json)?;
    artifacts.push(record_artifact("realtime-app", &realtime_app_path)?);

    let sidecar_root = output_path.parent().unwrap_or_else(|| Path::new("."));
    let compute_paths =
        kain_driver::write_compute_residency_sidecars(&realtime_bundle.bundle, sidecar_root)?;
    artifacts.extend(record_existing_artifacts(
        "compute-residency",
        &compute_paths,
    )?);

    let shader_bundle_source;
    let shader_source = match shader_artifact_source(source) {
        Some(source) => {
            shader_bundle_source = source;
            shader_bundle_source.as_str()
        }
        None => source,
    };

    match kain_driver::compile_shader_artifact_bundle(shader_source) {
        Ok(bundle) => {
            let shader_path = output_path.with_extension("shader_bundle.json");
            kfs::atomic_write_text(&shader_path, &bundle.bundle_json)?;
            artifacts.push(record_artifact("shader-bundle", &shader_path)?);
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

fn run_gpu_artifacts(source: &Path, output_base: &Path) -> BuildResult<String> {
    let source_text = kfs::read_text(source)?;
    let artifacts = kain_driver::compile_shader_artifact_bundle(&source_text)?;
    if let Some(parent) = output_base.parent() {
        kfs::create_dir_all(parent)?;
    }
    kfs::atomic_write_bytes(output_base.with_extension("spv"), &artifacts.spirv)?;
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
    if let Some(hlsl) = artifacts.derived_hlsl {
        kfs::atomic_write_text(output_base.with_extension("hlsl"), &hlsl)?;
    }
    if let Some(ptx) = artifacts.derived_ptx {
        kfs::atomic_write_text(output_base.with_extension("ptx"), &ptx)?;
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

fn run_node_like(
    runtime: NodeRuntimeKind,
    entry: Option<&PathBuf>,
    command: Option<&String>,
    args: &[String],
    cwd: &Path,
) -> BuildResult<String> {
    let program = command.cloned().unwrap_or_else(|| match runtime {
        NodeRuntimeKind::Node => "node".to_string(),
        NodeRuntimeKind::Bun => "bun".to_string(),
    });
    let mut process = Command::new(&program);
    process
        .current_dir(cwd)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    match (runtime, entry, command) {
        (NodeRuntimeKind::Node, Some(entry), None) => {
            process.arg("--check").arg(entry);
        }
        (NodeRuntimeKind::Bun, Some(entry), None) => {
            process.arg(entry);
        }
        _ => {
            for arg in args {
                process.arg(arg);
            }
            if let Some(entry) = entry {
                process.arg(entry);
            }
        }
    }
    run_command_capture(process, &format!("{program} task"))
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
    plan.cache_root
        .join("stamps")
        .join(format!("{}.stamp", sanitize_id(&task.id)))
}

fn task_stamp(task: &BuildTask, plan: &BladeBuildPlan) -> BuildResult<String> {
    let mut hasher = Sha256::new();
    hasher.update(BUILD_ADAPTER_VERSION.as_bytes());
    hasher.update(plan.host.as_bytes());
    hasher.update(plan.lane.as_str().as_bytes());
    hasher.update(plan.profile.as_bytes());
    hasher.update(plan.target.as_bytes());
    hasher.update(task.id.as_bytes());
    hasher.update(task.kind.as_str().as_bytes());
    hasher.update(format!("{:?}", task.adapter).as_bytes());
    for input in &task.inputs {
        hash_path_into(&mut hasher, input)?;
    }
    for output in &task.outputs {
        hasher.update(output.display().to_string().as_bytes());
    }
    Ok(format!("{:x}", hasher.finalize()))
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
    ensure_safe_clean_root(&plan.workspace_root, &plan.artifact_root)?;
    ensure_safe_clean_root(&plan.workspace_root, &plan.cache_root)?;
    ensure_safe_clean_root(&plan.workspace_root, &plan.report_root)?;
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
            "cycle detected in blade build task graph at '{id}'"
        )));
    }
    let task = by_id
        .get(id)
        .ok_or_else(|| BuildError::Config(format!("unknown blade build task '{id}'")))?;
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

fn gpu_output_paths(output_base: &Path) -> Vec<PathBuf> {
    vec![
        output_base.with_extension("spv"),
        with_file_name_suffix(output_base, ".gpu", "rs"),
        with_file_name_suffix(output_base, ".reflect", "json"),
        with_file_name_suffix(output_base, ".shader_bundle", "json"),
        output_base.with_extension("hlsl"),
    ]
}

fn artifact_manifest_path(task_root: &Path) -> PathBuf {
    task_root.join("kain-artifacts.json")
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
    if let Ok(path) = std::env::var("KAIN_CLANG_PATH") {
        let candidate = PathBuf::from(path);
        if candidate.exists() {
            return Ok(candidate);
        }
    }
    for ancestor in workspace_root.ancestors() {
        for relative in [
            "toolchain/llvm/bin/clang.exe",
            "toolchain/llvm/bin/clang",
            "third_party/llvm/bin/clang.exe",
            "third_party/llvm/bin/clang",
        ] {
            let candidate = ancestor.join(relative);
            if candidate.exists() {
                return Ok(candidate);
            }
        }
    }
    Ok(PathBuf::from("clang"))
}

fn clean_build_roots(plan: &BladeBuildPlan) -> BuildResult<()> {
    for root in [&plan.artifact_root, &plan.cache_root, &plan.report_root] {
        if root.exists() {
            ensure_safe_clean_root(&plan.workspace_root, root)?;
            kfs::remove_dir_all(root)?;
        }
    }
    Ok(())
}

fn ensure_safe_clean_root(workspace_root: &Path, root: &Path) -> BuildResult<()> {
    let workspace_raw = if workspace_root.is_absolute() {
        workspace_root.to_path_buf()
    } else {
        std::env::current_dir()?.join(workspace_root)
    };
    let workspace = PathBuf::from(kfs::canonicalize_path(&workspace_raw)?);
    let target_raw = if root.is_absolute() {
        root.to_path_buf()
    } else {
        workspace_raw.join(root)
    };
    let target = kfs::canonicalize_path(&target_raw)
        .map(PathBuf::from)
        .unwrap_or_else(|_| target_raw.clone());
    if paths_equivalent(&target, &workspace)
        || (!path_starts_with_equivalent(&target, &workspace)
            && !path_starts_with_equivalent(&target, &workspace_raw))
    {
        return Err(BuildError::Config(format!(
            "refusing to clean build path outside workspace: {}",
            root.display()
        )));
    }
    if !target
        .components()
        .any(|component| component.as_os_str() == std::ffi::OsStr::new(".kain"))
    {
        return Err(BuildError::Config(format!(
            "refusing to clean non-.kain build path: {}",
            root.display()
        )));
    }
    Ok(())
}

fn paths_equivalent(left: &Path, right: &Path) -> bool {
    comparable_path(left) == comparable_path(right)
}

fn path_starts_with_equivalent(path: &Path, base: &Path) -> bool {
    path.starts_with(base) || comparable_path(path).starts_with(&comparable_path(base))
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

    fn write(&mut self, execution: &BuildTaskExecution) -> BuildResult<()> {
        let encoded = serde_json::to_string(&json!({
            "timestamp_unix_ms": unix_timestamp_ms(),
            "task": execution.id,
            "kind": execution.kind,
            "status": execution.status,
            "cache_hit": execution.cache_hit,
            "message": execution.message,
            "error": execution.error,
        }))
        .map_err(|err| BuildError::Config(format!("failed to serialize build event: {err}")))?;
        kfs::append_text(&self.path, &format!("{encoded}\n"))?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

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
            "[package]\nname = \"probe\"\n\n[build]\nentry = \"src/main.kn\"\n",
        )
        .expect("write KAIN.toml");
        std::fs::write(
            root.join("build.kn"),
            r#"
fn build(ctx: BuildContext) -> BuildGraph:
    let vk = platform_package("vulkan").provider("system")
    let tiny = platform_package("tiny_math")
    return build_graph().require(vk).require(tiny)
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
    fn shader_artifact_source_extracts_kain_example_shaders_without_native_body() {
        let source = include_str!("../../../blades/kain-example/src/main.kn");

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
}
