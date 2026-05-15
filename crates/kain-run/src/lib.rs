use blade::{
    discover_workspace, load_kain_manifest, BladeWorkspace, KainRunSection, ResolvedBlade,
};
use kain_core::CompileTarget;
use kain_fs as kfs;
use kain_omni::fabric::FabricSessionStatus;
use kain_process::{ProcessEnvironmentEntry, ProcessSpec, ProcessStdioMode};
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};
use std::ffi::OsStr;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::thread;
use std::time::Duration;

const DEFAULT_RUN_REPORT_ROOT: &str = ".kain/reports/run";
const DEFAULT_RUN_CACHE_ROOT: &str = ".kain/cache/run";
pub const RUN_ADAPTER_VERSION: &str = "kain-run-v1";

pub type RunResult<T> = Result<T, RunError>;

#[derive(Debug, thiserror::Error)]
pub enum RunError {
    #[error("filesystem error: {0}")]
    Fs(#[from] kain_fs::FsError),
    #[error("blade workspace error: {0}")]
    Blade(#[from] blade::BladeError),
    #[error("Kain error: {0}")]
    Kain(#[from] kain_core::KainError),
    #[error("Fabric error: {0}")]
    Fabric(#[from] kain_omni::OmniError),
    #[error("failed to serialize run report: {0}")]
    Serialize(#[from] serde_json::Error),
    #[error("{0}")]
    Config(String),
    #[error("process failed: {0}")]
    Process(String),
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum RunMode {
    Once,
    Dev,
    Plan,
}

impl Default for RunMode {
    fn default() -> Self {
        Self::Once
    }
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum RunTarget {
    Auto,
    Kain,
    C,
    Cargo,
    Fabric,
    Node,
    Bun,
}

impl Default for RunTarget {
    fn default() -> Self {
        Self::Auto
    }
}

impl RunTarget {
    pub fn parse(value: &str) -> RunResult<Self> {
        match value.trim().to_ascii_lowercase().as_str() {
            "" | "auto" => Ok(Self::Auto),
            "kain" | "kn" | "run" | "interpret" => Ok(Self::Kain),
            "c" | "clang" | "native-c" => Ok(Self::C),
            "cargo" | "rust" | "rust-crate" => Ok(Self::Cargo),
            "fabric" | "kain-fabric" => Ok(Self::Fabric),
            "node" | "js" | "javascript" => Ok(Self::Node),
            "bun" => Ok(Self::Bun),
            other => Err(RunError::Config(format!(
                "unknown run target '{other}'; expected auto, kain, c, cargo, fabric, node, or bun"
            ))),
        }
    }
}

#[derive(Debug, Clone)]
pub struct RunRequest {
    pub input: Option<PathBuf>,
    pub mode: RunMode,
    pub target: RunTarget,
    pub args: Vec<String>,
    pub workspace_path: PathBuf,
    pub blade: Option<String>,
    pub json: bool,
    pub trace: bool,
    pub keep_artifacts: bool,
    pub dry_run: bool,
    pub watch_limit: Option<usize>,
    pub poll_interval: Duration,
}

impl RunRequest {
    pub fn new(input: Option<PathBuf>) -> Self {
        Self {
            input,
            mode: RunMode::Once,
            target: RunTarget::Auto,
            args: Vec::new(),
            workspace_path: PathBuf::from("."),
            blade: None,
            json: false,
            trace: false,
            keep_artifacts: false,
            dry_run: false,
            watch_limit: None,
            poll_interval: Duration::from_millis(250),
        }
    }

    pub fn with_mode(mut self, mode: RunMode) -> Self {
        self.mode = mode;
        self
    }

    pub fn with_target(mut self, target: RunTarget) -> Self {
        self.target = target;
        self
    }

    pub fn with_args(mut self, args: Vec<String>) -> Self {
        self.args = args;
        self
    }

    pub fn with_workspace_path(mut self, path: PathBuf) -> Self {
        self.workspace_path = path;
        self
    }

    pub fn with_blade(mut self, blade: Option<String>) -> Self {
        self.blade = blade;
        self
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RunPlan {
    pub workspace_root: PathBuf,
    pub cache_root: PathBuf,
    pub report_root: PathBuf,
    pub mode: RunMode,
    pub requested_target: RunTarget,
    pub args: Vec<String>,
    pub units: Vec<RunUnit>,
    pub watch_inputs: Vec<PathBuf>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RunUnit {
    pub id: String,
    pub target: RunTarget,
    pub label: String,
    pub cwd: PathBuf,
    pub args: Vec<String>,
    pub env: BTreeMap<String, String>,
    pub inputs: Vec<PathBuf>,
    pub adapter: RunAdapter,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "kind", rename_all = "kebab-case")]
pub enum RunAdapter {
    KainInterpreter {
        entry: PathBuf,
    },
    CExecutable {
        source: PathBuf,
        executable: PathBuf,
        compiler: PathBuf,
    },
    Cargo {
        manifest_path: PathBuf,
        package: Option<String>,
        release: bool,
        target_dir: PathBuf,
    },
    Fabric {
        manifest_path: PathBuf,
    },
    NodeLike {
        runtime: NodeRuntimeKind,
        entry: PathBuf,
        program: String,
    },
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
pub enum NodeRuntimeKind {
    Node,
    Bun,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum RunStatus {
    Succeeded,
    Failed,
    Planned,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RunReport {
    pub workspace_root: PathBuf,
    pub cache_root: PathBuf,
    pub report_path: PathBuf,
    pub events_path: PathBuf,
    pub mode: RunMode,
    pub requested_target: RunTarget,
    pub status: RunStatus,
    pub started_unix_ms: u128,
    pub finished_unix_ms: u128,
    pub dry_run: bool,
    pub units: Vec<RunUnitExecution>,
}

impl RunReport {
    pub fn is_success(&self) -> bool {
        matches!(self.status, RunStatus::Succeeded | RunStatus::Planned)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RunUnitExecution {
    pub id: String,
    pub target: RunTarget,
    pub status: RunStatus,
    pub started_unix_ms: Option<u128>,
    pub finished_unix_ms: Option<u128>,
    pub inputs: Vec<PathBuf>,
    pub process: Option<ProcessSpec>,
    pub exit_code: Option<i64>,
    pub stdout: String,
    pub stderr: String,
    pub output: String,
    pub error: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RunEvent {
    pub event: String,
    pub unix_ms: u128,
    pub unit_id: Option<String>,
    pub message: String,
}

pub fn plan_run(request: &RunRequest) -> RunResult<RunPlan> {
    let workspace_root = discover_workspace_root(request)?;
    let cache_root = workspace_root.join(DEFAULT_RUN_CACHE_ROOT);
    let report_root = workspace_root.join(DEFAULT_RUN_REPORT_ROOT);
    let mut unit = resolve_run_unit(request, &workspace_root, &cache_root)?;
    if !request.args.is_empty() {
        append_runtime_args(&mut unit, &request.args);
    }
    let mut watch_inputs = unit.inputs.clone();
    watch_inputs.push(workspace_root.join("KAIN.toml"));
    watch_inputs.sort();
    watch_inputs.dedup();
    Ok(RunPlan {
        workspace_root,
        cache_root,
        report_root,
        mode: request.mode,
        requested_target: request.target,
        args: request.args.clone(),
        units: vec![unit],
        watch_inputs,
    })
}

pub fn execute_run(request: &RunRequest) -> RunResult<RunReport> {
    let plan = plan_run(request)?;
    if matches!(request.mode, RunMode::Dev) && !request.dry_run {
        return run_dev_loop(request, plan);
    }
    execute_plan(plan, request)
}

pub fn execute_plan(plan: RunPlan, request: &RunRequest) -> RunResult<RunReport> {
    kfs::create_dir_all(&plan.report_root)?;
    kfs::create_dir_all(&plan.cache_root)?;
    let started_unix_ms = unix_timestamp_ms();
    let report_path = plan.report_root.join(format!(
        "session-{started_unix_ms}-{}.json",
        std::process::id()
    ));
    let events_path = report_path.with_extension("jsonl");
    let mut executions = Vec::new();
    write_event(
        &events_path,
        &RunEvent {
            event: "plan".to_string(),
            unix_ms: started_unix_ms,
            unit_id: None,
            message: format!("planned {} run unit(s)", plan.units.len()),
        },
    )?;

    for unit in &plan.units {
        let execution = if request.dry_run || matches!(request.mode, RunMode::Plan) {
            RunUnitExecution {
                id: unit.id.clone(),
                target: unit.target,
                status: RunStatus::Planned,
                started_unix_ms: None,
                finished_unix_ms: None,
                inputs: unit.inputs.clone(),
                process: process_spec_for_unit(unit),
                exit_code: None,
                stdout: String::new(),
                stderr: String::new(),
                output: unit.label.clone(),
                error: None,
            }
        } else {
            execute_unit(unit)?
        };
        write_event(
            &events_path,
            &RunEvent {
                event: "unit".to_string(),
                unix_ms: unix_timestamp_ms(),
                unit_id: Some(unit.id.clone()),
                message: format!("{:?}", execution.status),
            },
        )?;
        executions.push(execution);
    }

    let failed = executions
        .iter()
        .any(|execution| execution.status == RunStatus::Failed);
    let finished_unix_ms = unix_timestamp_ms();
    let status = if request.dry_run || matches!(request.mode, RunMode::Plan) {
        RunStatus::Planned
    } else if failed {
        RunStatus::Failed
    } else {
        RunStatus::Succeeded
    };
    let report = RunReport {
        workspace_root: plan.workspace_root,
        cache_root: plan.cache_root,
        report_path: report_path.clone(),
        events_path,
        mode: plan.mode,
        requested_target: plan.requested_target,
        status,
        started_unix_ms,
        finished_unix_ms,
        dry_run: request.dry_run,
        units: executions,
    };
    kfs::atomic_write_text(&report_path, &serde_json::to_string_pretty(&report)?)?;
    Ok(report)
}

fn run_dev_loop(request: &RunRequest, mut plan: RunPlan) -> RunResult<RunReport> {
    let mut last_report = execute_plan(plan.clone(), request)?;
    let mut watchers = create_watchers(&plan.watch_inputs)?;
    let limit = request.watch_limit.unwrap_or(usize::MAX);
    let mut completed = 0usize;
    while completed < limit {
        thread::sleep(request.poll_interval);
        if watchers_changed(&mut watchers)? {
            plan = plan_run(request)?;
            last_report = execute_plan(plan.clone(), request)?;
            watchers = create_watchers(&plan.watch_inputs)?;
            completed += 1;
        }
    }
    Ok(last_report)
}

fn create_watchers(paths: &[PathBuf]) -> RunResult<Vec<kfs::FsWatcher>> {
    let mut watchers = Vec::new();
    let mut roots = BTreeSet::new();
    for path in paths {
        if path.exists() {
            roots.insert(path.clone());
        } else if let Some(parent) = path.parent() {
            roots.insert(parent.to_path_buf());
        }
    }
    for root in roots {
        watchers.push(kfs::FsWatcher::new(root, false)?);
    }
    Ok(watchers)
}

fn watchers_changed(watchers: &mut [kfs::FsWatcher]) -> RunResult<bool> {
    for watcher in watchers {
        if !watcher.poll()?.is_empty() {
            return Ok(true);
        }
    }
    Ok(false)
}

fn resolve_run_unit(
    request: &RunRequest,
    workspace_root: &Path,
    cache_root: &Path,
) -> RunResult<RunUnit> {
    if let Some(input) = request.input.as_ref() {
        let input = resolve_existing_or_declared_path(workspace_root, input);
        if input.is_dir() {
            return resolve_workspace_unit(request, &input, cache_root);
        }
        return resolve_file_unit(request, workspace_root, &input, cache_root);
    }
    resolve_workspace_unit(request, &request.workspace_path, cache_root)
}

fn resolve_workspace_unit(
    request: &RunRequest,
    path: &Path,
    cache_root: &Path,
) -> RunResult<RunUnit> {
    let workspace = discover_workspace(path)?;
    let workspace_run = workspace
        .manifest_path
        .as_ref()
        .map(|path| load_run_section(path))
        .transpose()?
        .unwrap_or_default();
    let selected_by_workspace_run = request.blade.is_none() && workspace_run.blade.is_some();
    let selected_blade = select_blade(
        &workspace,
        request.blade.as_deref().or(workspace_run.blade.as_deref()),
    )?;
    if let Some(blade) = selected_blade {
        let mut request = request.clone();
        if selected_by_workspace_run && request.target == RunTarget::Auto {
            request.target = workspace_run
                .target
                .as_deref()
                .map(RunTarget::parse)
                .transpose()?
                .unwrap_or(request.target);
        }
        let mut unit = resolve_blade_unit(&request, &workspace, blade, cache_root)?;
        if selected_by_workspace_run {
            apply_run_section_to_unit(
                &mut unit,
                &workspace.root,
                &workspace_run,
                request.args.is_empty(),
            );
        }
        return Ok(unit);
    }
    let manifest_path = workspace.manifest_path.as_ref().ok_or_else(|| {
        RunError::Config("run needs an input, blade, or KAIN.toml workspace".into())
    })?;
    let manifest = load_kain_manifest(manifest_path)?;
    let run = workspace_run;
    let entry = run
        .entry
        .clone()
        .or(manifest.build.entry)
        .map(|path| resolve_path(&workspace.root, &path));
    if let Some(entry) = entry {
        let mut unit = resolve_file_unit(request, &workspace.root, &entry, cache_root)?;
        apply_run_section_to_unit(&mut unit, &workspace.root, &run, request.args.is_empty());
        return Ok(unit);
    }
    Err(RunError::Config(format!(
        "no runnable entry found in {}",
        manifest_path.display()
    )))
}

fn resolve_blade_unit(
    request: &RunRequest,
    _workspace: &BladeWorkspace,
    blade: &ResolvedBlade,
    cache_root: &Path,
) -> RunResult<RunUnit> {
    let run = blade
        .manifest_path
        .as_ref()
        .map(|path| load_run_section(path))
        .transpose()?
        .unwrap_or_default();
    let target = run
        .target
        .as_deref()
        .map(RunTarget::parse)
        .transpose()?
        .unwrap_or(request.target);
    let mut request = request.clone();
    request.target = target;
    let mut unit = if let Some(entry) = run.entry.as_ref() {
        resolve_file_unit(
            &request,
            &blade.root,
            &resolve_path(&blade.root, entry),
            cache_root,
        )?
    } else if let Some(entry) = &blade.entry {
        resolve_file_unit(&request, &blade.root, entry, cache_root)?
    } else if request.target == RunTarget::Auto || request.target == RunTarget::Cargo {
        if let Some(manifest_path) = &blade.cargo_manifest {
            cargo_unit(
                "cargo",
                &blade.root,
                manifest_path,
                blade.rust_crate_name.clone(),
                false,
                cache_root,
            )?
        } else if request.target == RunTarget::Auto || request.target == RunTarget::Fabric {
            if let Some(manifest_path) = &blade.fabric_manifest {
                fabric_unit("fabric", &blade.root, manifest_path)?
            } else {
                return Err(no_runnable_blade_error(blade));
            }
        } else {
            return Err(no_runnable_blade_error(blade));
        }
    } else if request.target == RunTarget::Auto || request.target == RunTarget::Fabric {
        if let Some(manifest_path) = &blade.fabric_manifest {
            fabric_unit("fabric", &blade.root, manifest_path)?
        } else {
            return Err(no_runnable_blade_error(blade));
        }
    } else {
        return Err(no_runnable_blade_error(blade));
    };
    apply_run_section_to_unit(&mut unit, &blade.root, &run, request.args.is_empty());
    Ok(unit)
}

fn no_runnable_blade_error(blade: &ResolvedBlade) -> RunError {
    RunError::Config(format!(
        "blade '{}' has no runnable entry, Cargo manifest, or Fabric manifest",
        blade.name
    ))
}

fn apply_run_section_to_unit(
    unit: &mut RunUnit,
    root: &Path,
    run: &KainRunSection,
    use_manifest_args: bool,
) {
    if use_manifest_args {
        append_runtime_args(unit, &run.args);
    }
    if let Some(cwd) = &run.cwd {
        unit.cwd = resolve_path(root, cwd);
    }
    unit.env.extend(run.env.clone());
    for watch_path in &run.watch {
        let input = resolve_path(root, watch_path);
        if !unit.inputs.contains(&input) {
            unit.inputs.push(input);
        }
    }
}

fn resolve_file_unit(
    request: &RunRequest,
    workspace_root: &Path,
    input: &Path,
    cache_root: &Path,
) -> RunResult<RunUnit> {
    let target = if request.target == RunTarget::Auto {
        infer_target_from_path(input)
    } else {
        request.target
    };
    match target {
        RunTarget::Kain => kain_unit("kain", workspace_root, input),
        RunTarget::C => c_unit(workspace_root, input, cache_root),
        RunTarget::Cargo => {
            let manifest = if input.file_name() == Some(OsStr::new("Cargo.toml")) {
                input.to_path_buf()
            } else {
                input.join("Cargo.toml")
            };
            cargo_unit("cargo", workspace_root, &manifest, None, false, cache_root)
        }
        RunTarget::Fabric => fabric_unit("fabric", workspace_root, input),
        RunTarget::Node => node_unit(NodeRuntimeKind::Node, workspace_root, input),
        RunTarget::Bun => node_unit(NodeRuntimeKind::Bun, workspace_root, input),
        RunTarget::Auto => unreachable!("auto target should have been resolved"),
    }
}

fn kain_unit(id: &str, workspace_root: &Path, entry: &Path) -> RunResult<RunUnit> {
    ensure_file(entry, "Kain entry")?;
    Ok(RunUnit {
        id: id.to_string(),
        target: RunTarget::Kain,
        label: format!("Interpret {}", entry.display()),
        cwd: entry.parent().unwrap_or(workspace_root).to_path_buf(),
        args: Vec::new(),
        env: BTreeMap::new(),
        inputs: vec![entry.to_path_buf()],
        adapter: RunAdapter::KainInterpreter {
            entry: entry.to_path_buf(),
        },
    })
}

fn c_unit(workspace_root: &Path, source: &Path, cache_root: &Path) -> RunResult<RunUnit> {
    ensure_file(source, "C source")?;
    let compiler = find_clang(workspace_root);
    let executable = cached_c_executable_path(cache_root, source)?;
    Ok(RunUnit {
        id: "c".to_string(),
        target: RunTarget::C,
        label: format!("Compile/cache/run C {}", source.display()),
        cwd: source.parent().unwrap_or(workspace_root).to_path_buf(),
        args: Vec::new(),
        env: BTreeMap::new(),
        inputs: vec![source.to_path_buf()],
        adapter: RunAdapter::CExecutable {
            source: source.to_path_buf(),
            executable,
            compiler,
        },
    })
}

fn cargo_unit(
    id: &str,
    workspace_root: &Path,
    manifest_path: &Path,
    package: Option<String>,
    release: bool,
    cache_root: &Path,
) -> RunResult<RunUnit> {
    ensure_file(manifest_path, "Cargo manifest")?;
    let target_dir = cache_root
        .join("cargo")
        .join(sanitize_id(&path_stem_or_name(manifest_path)));
    Ok(RunUnit {
        id: id.to_string(),
        target: RunTarget::Cargo,
        label: format!("Cargo run {}", manifest_path.display()),
        cwd: manifest_path
            .parent()
            .unwrap_or(workspace_root)
            .to_path_buf(),
        args: Vec::new(),
        env: BTreeMap::new(),
        inputs: vec![manifest_path.to_path_buf()],
        adapter: RunAdapter::Cargo {
            manifest_path: manifest_path.to_path_buf(),
            package,
            release,
            target_dir,
        },
    })
}

fn fabric_unit(id: &str, workspace_root: &Path, manifest_path: &Path) -> RunResult<RunUnit> {
    ensure_file(manifest_path, "Fabric manifest")?;
    Ok(RunUnit {
        id: id.to_string(),
        target: RunTarget::Fabric,
        label: format!("Fabric run {}", manifest_path.display()),
        cwd: manifest_path
            .parent()
            .unwrap_or(workspace_root)
            .to_path_buf(),
        args: Vec::new(),
        env: BTreeMap::new(),
        inputs: vec![manifest_path.to_path_buf()],
        adapter: RunAdapter::Fabric {
            manifest_path: manifest_path.to_path_buf(),
        },
    })
}

fn node_unit(kind: NodeRuntimeKind, workspace_root: &Path, entry: &Path) -> RunResult<RunUnit> {
    ensure_file(entry, "Node/Bun entry")?;
    let program = match kind {
        NodeRuntimeKind::Node => "node",
        NodeRuntimeKind::Bun => "bun",
    };
    Ok(RunUnit {
        id: program.to_string(),
        target: match kind {
            NodeRuntimeKind::Node => RunTarget::Node,
            NodeRuntimeKind::Bun => RunTarget::Bun,
        },
        label: format!("{program} run {}", entry.display()),
        cwd: entry.parent().unwrap_or(workspace_root).to_path_buf(),
        args: Vec::new(),
        env: BTreeMap::new(),
        inputs: vec![entry.to_path_buf()],
        adapter: RunAdapter::NodeLike {
            runtime: kind,
            entry: entry.to_path_buf(),
            program: program.to_string(),
        },
    })
}

fn execute_unit(unit: &RunUnit) -> RunResult<RunUnitExecution> {
    let started_unix_ms = unix_timestamp_ms();
    let result = match &unit.adapter {
        RunAdapter::KainInterpreter { entry } => run_kain(entry, unit),
        RunAdapter::CExecutable {
            source,
            executable,
            compiler,
        } => run_c(source, executable, compiler, unit),
        RunAdapter::Cargo {
            manifest_path,
            package,
            release,
            target_dir,
        } => run_cargo(
            manifest_path,
            package.as_deref(),
            *release,
            target_dir,
            unit,
        ),
        RunAdapter::Fabric { manifest_path } => run_fabric(manifest_path),
        RunAdapter::NodeLike { entry, program, .. } => run_node_like(program, entry, unit),
    };
    let finished_unix_ms = unix_timestamp_ms();
    Ok(match result {
        Ok(mut execution) => {
            execution.started_unix_ms = Some(started_unix_ms);
            execution.finished_unix_ms = Some(finished_unix_ms);
            execution
        }
        Err(error) => RunUnitExecution {
            id: unit.id.clone(),
            target: unit.target,
            status: RunStatus::Failed,
            started_unix_ms: Some(started_unix_ms),
            finished_unix_ms: Some(finished_unix_ms),
            inputs: unit.inputs.clone(),
            process: process_spec_for_unit(unit),
            exit_code: None,
            stdout: String::new(),
            stderr: String::new(),
            output: String::new(),
            error: Some(error.to_string()),
        },
    })
}

fn run_kain(entry: &Path, unit: &RunUnit) -> RunResult<RunUnitExecution> {
    with_temporary_process_context(&unit.cwd, &unit.env, || {
        let source = kfs::read_text(entry)?;
        let value = kain_driver::compile(&source, CompileTarget::Interpret)?;
        Ok(RunUnitExecution {
            id: "kain".to_string(),
            target: RunTarget::Kain,
            status: RunStatus::Succeeded,
            started_unix_ms: None,
            finished_unix_ms: None,
            inputs: vec![entry.to_path_buf()],
            process: None,
            exit_code: Some(0),
            stdout: String::new(),
            stderr: String::new(),
            output: value,
            error: None,
        })
    })
}

fn with_temporary_process_context<T>(
    cwd: &Path,
    env_overrides: &BTreeMap<String, String>,
    action: impl FnOnce() -> RunResult<T>,
) -> RunResult<T> {
    let previous_cwd = std::env::current_dir().map_err(|err| {
        RunError::Process(format!(
            "failed to read current directory before Kain run: {err}"
        ))
    })?;
    std::env::set_current_dir(cwd).map_err(|err| {
        RunError::Process(format!(
            "failed to set current directory to {} before Kain run: {err}",
            cwd.display()
        ))
    })?;

    let mut previous_env = Vec::with_capacity(env_overrides.len());
    for (key, value) in env_overrides {
        previous_env.push((key.clone(), std::env::var_os(key)));
        std::env::set_var(key, value);
    }

    let result = action();
    for (key, previous_value) in previous_env.into_iter().rev() {
        match previous_value {
            Some(value) => {
                std::env::set_var(&key, value);
            }
            None => {
                std::env::remove_var(&key);
            }
        }
    }

    let mut restore_error = None;
    if let Err(err) = std::env::set_current_dir(&previous_cwd) {
        if restore_error.is_none() {
            restore_error = Some(RunError::Process(format!(
                "failed to restore current directory to {} after Kain run: {err}",
                previous_cwd.display()
            )));
        }
    }

    match (result, restore_error) {
        (Ok(value), None) => Ok(value),
        (Ok(_), Some(error)) => Err(error),
        (Err(error), None) => Err(error),
        (Err(error), Some(restore_error)) => Err(RunError::Process(format!(
            "{error}; additionally failed to restore process context: {restore_error}"
        ))),
    }
}

fn run_c(
    source: &Path,
    executable: &Path,
    compiler: &Path,
    unit: &RunUnit,
) -> RunResult<RunUnitExecution> {
    compile_c_if_needed(source, executable, compiler)?;
    run_process(command_for_process(executable, unit, &unit.args), unit)
}

fn run_cargo(
    manifest_path: &Path,
    package: Option<&str>,
    release: bool,
    target_dir: &Path,
    unit: &RunUnit,
) -> RunResult<RunUnitExecution> {
    kfs::create_dir_all(target_dir)?;
    let mut command = Command::new("cargo");
    command
        .arg("run")
        .arg("--manifest-path")
        .arg(manifest_path)
        .env("CARGO_TARGET_DIR", target_dir)
        .current_dir(unit.cwd.clone());
    apply_unit_environment(&mut command, unit);
    if let Some(package) = package {
        command.arg("--package").arg(package);
    }
    if release {
        command.arg("--release");
    }
    if !unit.args.is_empty() {
        command.arg("--");
        for arg in &unit.args {
            command.arg(arg);
        }
    }
    run_process(command, unit)
}

fn run_fabric(manifest_path: &Path) -> RunResult<RunUnitExecution> {
    let result = kain_host::fabric::execute_fabric_manifest_path(manifest_path)?;
    let status = if result.status == FabricSessionStatus::Succeeded {
        RunStatus::Succeeded
    } else {
        RunStatus::Failed
    };
    Ok(RunUnitExecution {
        id: "fabric".to_string(),
        target: RunTarget::Fabric,
        status,
        started_unix_ms: None,
        finished_unix_ms: None,
        inputs: vec![manifest_path.to_path_buf()],
        process: None,
        exit_code: Some(if status == RunStatus::Succeeded { 0 } else { 1 }),
        stdout: String::new(),
        stderr: String::new(),
        output: format!("Fabric report {}", result.report_path.display()),
        error: if status == RunStatus::Succeeded {
            None
        } else {
            Some(format!("Fabric run failed for {}", manifest_path.display()))
        },
    })
}

fn run_node_like(program: &str, entry: &Path, unit: &RunUnit) -> RunResult<RunUnitExecution> {
    let mut command = Command::new(program);
    command.arg(entry).current_dir(unit.cwd.clone());
    apply_unit_environment(&mut command, unit);
    for arg in &unit.args {
        command.arg(arg);
    }
    run_process(command, unit)
}

fn run_process(mut command: Command, unit: &RunUnit) -> RunResult<RunUnitExecution> {
    command.stdout(Stdio::piped()).stderr(Stdio::piped());
    let process = process_spec_for_command(&command, unit);
    let output = command.output().map_err(|err| {
        RunError::Process(format!("failed to invoke {}: {err}", process.executable))
    })?;
    let stdout = String::from_utf8_lossy(&output.stdout).into_owned();
    let stderr = String::from_utf8_lossy(&output.stderr).into_owned();
    let exit_code = output.status.code().map(i64::from);
    Ok(RunUnitExecution {
        id: unit.id.clone(),
        target: unit.target,
        status: if output.status.success() {
            RunStatus::Succeeded
        } else {
            RunStatus::Failed
        },
        started_unix_ms: None,
        finished_unix_ms: None,
        inputs: unit.inputs.clone(),
        process: Some(process),
        exit_code,
        stdout,
        stderr,
        output: String::new(),
        error: if output.status.success() {
            None
        } else {
            Some(format!("process exited with status {}", output.status))
        },
    })
}

fn command_for_process(executable: &Path, unit: &RunUnit, args: &[String]) -> Command {
    let mut command = Command::new(executable);
    command.current_dir(unit.cwd.clone());
    apply_unit_environment(&mut command, unit);
    for arg in args {
        command.arg(arg);
    }
    command
}

fn apply_unit_environment(command: &mut Command, unit: &RunUnit) {
    for (key, value) in &unit.env {
        command.env(key, value);
    }
}

fn process_spec_for_unit(unit: &RunUnit) -> Option<ProcessSpec> {
    match &unit.adapter {
        RunAdapter::KainInterpreter { .. } | RunAdapter::Fabric { .. } => None,
        RunAdapter::CExecutable { executable, .. } => Some(process_spec(
            executable.to_string_lossy().into_owned(),
            &unit.args,
            &unit.cwd,
            &unit.env,
        )),
        RunAdapter::Cargo {
            manifest_path,
            package,
            release,
            ..
        } => {
            let mut argv = vec![
                "run".to_string(),
                "--manifest-path".to_string(),
                manifest_path.to_string_lossy().into_owned(),
            ];
            if let Some(package) = package {
                argv.push("--package".to_string());
                argv.push(package.clone());
            }
            if *release {
                argv.push("--release".to_string());
            }
            if !unit.args.is_empty() {
                argv.push("--".to_string());
                argv.extend(unit.args.clone());
            }
            Some(process_spec(
                "cargo".to_string(),
                &argv,
                &unit.cwd,
                &unit.env,
            ))
        }
        RunAdapter::NodeLike { program, entry, .. } => Some(process_spec(
            program.clone(),
            &std::iter::once(entry.to_string_lossy().into_owned())
                .chain(unit.args.clone())
                .collect::<Vec<_>>(),
            &unit.cwd,
            &unit.env,
        )),
    }
}

fn process_spec_for_command(command: &Command, unit: &RunUnit) -> ProcessSpec {
    let args = command
        .get_args()
        .map(|arg| arg.to_string_lossy().into_owned())
        .collect::<Vec<_>>();
    let cwd = command.get_current_dir().unwrap_or(unit.cwd.as_path());
    process_spec(
        command.get_program().to_string_lossy().into_owned(),
        &args,
        cwd,
        &unit.env,
    )
}

fn process_spec(
    executable: String,
    args: &[String],
    cwd: &Path,
    env: &BTreeMap<String, String>,
) -> ProcessSpec {
    ProcessSpec {
        executable,
        arguments: args.to_vec(),
        current_working_directory: Some(cwd.to_string_lossy().into_owned()),
        environment: env
            .iter()
            .map(|(key, value)| ProcessEnvironmentEntry {
                key: key.clone(),
                value: value.clone(),
            })
            .collect(),
        inherit_environment: true,
        stdin_mode: ProcessStdioMode::Inherit,
        stdout_mode: ProcessStdioMode::Pipe,
        stderr_mode: ProcessStdioMode::Pipe,
    }
}

fn compile_c_if_needed(source: &Path, executable: &Path, compiler: &Path) -> RunResult<()> {
    if executable.exists() {
        let exe_meta = kfs::metadata(executable)?;
        let src_meta = kfs::metadata(source)?;
        if exe_meta.modified_millis >= src_meta.modified_millis {
            return Ok(());
        }
    }
    if let Some(parent) = executable.parent() {
        kfs::create_dir_all(parent)?;
    }
    let mut command = Command::new(compiler);
    command
        .arg(source)
        .arg("-O2")
        .arg("-o")
        .arg(executable)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    let output = command.output().map_err(|err| {
        RunError::Process(format!(
            "failed to invoke C compiler {}: {err}",
            compiler.display()
        ))
    })?;
    if output.status.success() {
        Ok(())
    } else {
        Err(RunError::Process(format!(
            "C compile failed with status {}\n{}\n{}",
            output.status,
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr)
        )))
    }
}

fn append_runtime_args(unit: &mut RunUnit, args: &[String]) {
    if args.is_empty() {
        return;
    }
    unit.args.extend(args.iter().cloned());
}

fn discover_workspace_root(request: &RunRequest) -> RunResult<PathBuf> {
    let anchor = request
        .input
        .as_ref()
        .and_then(|input| input.parent().map(Path::to_path_buf))
        .unwrap_or_else(|| request.workspace_path.clone());
    Ok(discover_workspace(anchor)?.root)
}

fn select_blade<'a>(
    workspace: &'a BladeWorkspace,
    blade_name: Option<&str>,
) -> RunResult<Option<&'a ResolvedBlade>> {
    if let Some(blade_name) = blade_name {
        return workspace.find_blade(blade_name).map(Some).ok_or_else(|| {
            RunError::Config(format!(
                "blade '{blade_name}' not found in workspace {}",
                workspace.root.display()
            ))
        });
    }
    if workspace.blades.len() == 1 {
        return Ok(workspace.blades.first());
    }
    Ok(workspace
        .blades
        .iter()
        .find(|blade| blade.entry.is_some())
        .or_else(|| {
            workspace
                .blades
                .iter()
                .find(|blade| blade.cargo_manifest.is_some())
        })
        .or_else(|| {
            workspace
                .blades
                .iter()
                .find(|blade| blade.fabric_manifest.is_some())
        }))
}

fn infer_target_from_path(path: &Path) -> RunTarget {
    if path.file_name() == Some(OsStr::new("Cargo.toml")) || path.join("Cargo.toml").exists() {
        return RunTarget::Cargo;
    }
    if path.file_name() == Some(OsStr::new("KAIN.fabric.toml")) {
        return RunTarget::Fabric;
    }
    match path.extension().and_then(OsStr::to_str).unwrap_or_default() {
        "kn" | "god" => RunTarget::Kain,
        "c" => RunTarget::C,
        "js" | "mjs" | "cjs" => RunTarget::Node,
        "ts" => RunTarget::Bun,
        "toml" => RunTarget::Fabric,
        _ => RunTarget::Kain,
    }
}

fn load_run_section(path: &Path) -> RunResult<KainRunSection> {
    let manifest = load_kain_manifest(path)?;
    Ok(manifest.run)
}

fn resolve_existing_or_declared_path(workspace_root: &Path, path: &Path) -> PathBuf {
    if path.is_absolute() {
        return path.to_path_buf();
    }
    let candidate = PathBuf::from(path);
    if candidate.exists() {
        return candidate;
    }
    workspace_root.join(path)
}

fn resolve_path(root: &Path, path: &Path) -> PathBuf {
    if path.is_absolute() {
        path.to_path_buf()
    } else {
        root.join(path)
    }
}

fn ensure_file(path: &Path, label: &str) -> RunResult<()> {
    if path.is_file() {
        Ok(())
    } else {
        Err(RunError::Config(format!(
            "{label} does not exist or is not a file: {}",
            path.display()
        )))
    }
}

fn cached_c_executable_path(cache_root: &Path, source: &Path) -> RunResult<PathBuf> {
    let hash = kfs::hash_file(source)?;
    let name = path_stem_or_name(source);
    let ext = if cfg!(target_os = "windows") {
        "exe"
    } else {
        "bin"
    };
    Ok(cache_root
        .join("c")
        .join(format!("{}-{}.{}", sanitize_id(&name), &hash[..16], ext)))
}

fn find_clang(workspace_root: &Path) -> PathBuf {
    if let Ok(path) = std::env::var("KAIN_CLANG_PATH") {
        let candidate = PathBuf::from(path);
        if candidate.exists() {
            return candidate;
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
                return candidate;
            }
        }
    }
    PathBuf::from("clang")
}

fn path_stem_or_name(path: &Path) -> String {
    path.file_stem()
        .or_else(|| path.file_name())
        .and_then(OsStr::to_str)
        .unwrap_or("run")
        .to_string()
}

fn sanitize_id(value: &str) -> String {
    let mut output = String::with_capacity(value.len());
    for ch in value.chars() {
        if ch.is_ascii_alphanumeric() || matches!(ch, '-' | '_') {
            output.push(ch);
        } else {
            output.push('_');
        }
    }
    if output.is_empty() {
        "run".to_string()
    } else {
        output
    }
}

fn write_event(path: &Path, event: &RunEvent) -> RunResult<()> {
    let encoded = serde_json::to_string(event)?;
    kfs::append_text(path, &(encoded + "\n"))?;
    Ok(())
}

fn unix_timestamp_ms() -> u128 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map(|duration| duration.as_millis())
        .unwrap_or_default()
}

pub fn render_text_report(report: &RunReport) -> String {
    let mut out = String::new();
    out.push_str(&format!("Run {:?}: {:?}\n", report.mode, report.status));
    for unit in &report.units {
        out.push_str(&format!(
            "  {:?} {} exit={:?}\n",
            unit.status, unit.id, unit.exit_code
        ));
        if !unit.output.trim().is_empty() && unit.output.trim() != "()" {
            out.push_str(unit.output.trim());
            out.push('\n');
        }
        if !unit.stdout.trim().is_empty() {
            out.push_str(unit.stdout.trim());
            out.push('\n');
        }
        if !unit.stderr.trim().is_empty() {
            out.push_str(unit.stderr.trim());
            out.push('\n');
        }
        if let Some(error) = &unit.error {
            out.push_str(error.trim());
            out.push('\n');
        }
    }
    out.push_str(&format!("Report: {}\n", report.report_path.display()));
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::{Mutex, OnceLock};

    fn process_context_test_lock() -> &'static Mutex<()> {
        static LOCK: OnceLock<Mutex<()>> = OnceLock::new();
        LOCK.get_or_init(|| Mutex::new(()))
    }

    #[test]
    fn parses_run_targets() {
        assert_eq!(RunTarget::parse("auto").unwrap(), RunTarget::Auto);
        assert_eq!(RunTarget::parse("kain").unwrap(), RunTarget::Kain);
        assert_eq!(RunTarget::parse("rust-crate").unwrap(), RunTarget::Cargo);
    }

    #[test]
    fn infers_targets_from_file_names() {
        assert_eq!(
            infer_target_from_path(Path::new("main.kn")),
            RunTarget::Kain
        );
        assert_eq!(infer_target_from_path(Path::new("hello.c")), RunTarget::C);
        assert_eq!(
            infer_target_from_path(Path::new("KAIN.fabric.toml")),
            RunTarget::Fabric
        );
    }

    #[test]
    fn plans_single_kain_file() {
        let temp = tempfile::tempdir().unwrap();
        let entry = temp.path().join("main.kn");
        kfs::write_text(&entry, "fn main() -> Int:\n    return 1\n").unwrap();
        let request = RunRequest::new(Some(entry.clone()));
        let plan = plan_run(&request).unwrap();
        assert_eq!(plan.units.len(), 1);
        assert_eq!(plan.units[0].target, RunTarget::Kain);
        assert!(plan.watch_inputs.contains(&entry));
    }

    #[test]
    fn plans_manifest_run_section_metadata() {
        let temp = tempfile::tempdir().unwrap();
        let entry = temp.path().join("src").join("main.js");
        let watch = temp.path().join("config").join("settings.json");
        kfs::create_dir_all(entry.parent().unwrap()).unwrap();
        kfs::create_dir_all(watch.parent().unwrap()).unwrap();
        kfs::write_text(&entry, "console.log(process.env.KAIN_RUN_TEST_FLAG);\n").unwrap();
        kfs::write_text(&watch, "{}\n").unwrap();
        kfs::write_text(
            temp.path().join("KAIN.toml"),
            r#"
[package]
name = "run-section"

[run]
entry = "src/main.js"
target = "node"
args = ["from-manifest"]
cwd = "src"
watch = ["config/settings.json"]

[run.env]
KAIN_RUN_TEST_FLAG = "enabled"
"#,
        )
        .unwrap();

        let request = RunRequest::new(Some(temp.path().to_path_buf()));
        let plan = plan_run(&request).unwrap();
        let unit = &plan.units[0];
        assert_eq!(unit.target, RunTarget::Node);
        assert_eq!(unit.args, vec!["from-manifest"]);
        assert_eq!(
            unit.env.get("KAIN_RUN_TEST_FLAG").map(String::as_str),
            Some("enabled")
        );
        assert!(unit.cwd.ends_with("src"));
        assert!(plan
            .watch_inputs
            .iter()
            .any(|path| path.ends_with(Path::new("config").join("settings.json"))));
    }

    #[test]
    fn workspace_run_section_selects_named_blade() {
        let temp = tempfile::tempdir().unwrap();
        kfs::write_text(
            temp.path().join("KAIN.toml"),
            r#"
[workspace]
blades = ["blades/*"]

[run]
blade = "second"
target = "node"
args = ["from-root"]
"#,
        )
        .unwrap();
        for name in ["first", "second"] {
            let blade_root = temp.path().join("blades").join(name);
            let entry = blade_root.join("src").join("main.js");
            kfs::create_dir_all(entry.parent().unwrap()).unwrap();
            kfs::write_text(
                blade_root.join("KAIN.toml"),
                &format!("[package]\nname = \"{name}\"\n\n[build]\nentry = \"src/main.js\"\n"),
            )
            .unwrap();
            kfs::write_text(&entry, "console.log('ok');\n").unwrap();
        }

        let request = RunRequest::new(Some(temp.path().to_path_buf()));
        let plan = plan_run(&request).unwrap();
        let unit = &plan.units[0];
        assert_eq!(unit.target, RunTarget::Node);
        assert_eq!(unit.args, vec!["from-root"]);
        match &unit.adapter {
            RunAdapter::NodeLike { entry, .. } => {
                assert!(entry.ends_with(
                    Path::new("blades")
                        .join("second")
                        .join("src")
                        .join("main.js")
                ));
            }
            other => panic!("expected node adapter, got {other:?}"),
        }
    }

    #[test]
    fn executes_kain_blade_with_sibling_blade_dependency() {
        let _guard = process_context_test_lock().lock().unwrap();
        let temp = tempfile::tempdir().unwrap();
        kfs::write_text(
            temp.path().join("KAIN.toml"),
            r#"
[workspace]
blades = ["blades/*"]
"#,
        )
        .unwrap();

        let fmt_root = temp.path().join("blades").join("fmt");
        kfs::create_dir_all(fmt_root.join("src")).unwrap();
        kfs::write_text(
            fmt_root.join("KAIN.toml"),
            r#"
[package]
name = "fmt"

[blade]
name = "fmt"
kind = "kain_library"
module_roots = ["src"]
"#,
        )
        .unwrap();
        kfs::write_text(
            fmt_root.join("src").join("kain_fmt.kn"),
            r#"
pub fn fmt_join_strings(items: Array<String>, separator: String) -> String:
    let output = ""
    let index = 0
    while index < len(items):
        if index > 0:
            output = output + separator
        output = output + items[index]
        index = index + 1
    return output
"#,
        )
        .unwrap();

        let app_root = temp.path().join("blades").join("app");
        kfs::create_dir_all(app_root.join("src")).unwrap();
        kfs::write_text(
            app_root.join("KAIN.toml"),
            r#"
[package]
name = "app"

[blade]
name = "app"
entry = "src/main.kn"
source_roots = ["src"]
module_roots = ["src"]

[[blade.dependencies]]
name = "fmt"
kind = "kain"
"#,
        )
        .unwrap();
        kfs::write_text(
            app_root.join("src").join("main.kn"),
            r#"
use kain_fmt::fmt_join_strings

fn main() -> String:
    return fmt_join_strings(["one", "two"], ",")
"#,
        )
        .unwrap();

        let request = RunRequest::new(Some(app_root));
        let report = execute_run(&request).unwrap();
        assert!(report.is_success());
        assert_eq!(report.units.len(), 1);
        assert!(report.units[0].output.contains("one,two"));
    }
}
