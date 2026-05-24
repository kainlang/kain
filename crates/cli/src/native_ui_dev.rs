use std::collections::BTreeSet;
use std::ffi::OsStr;
use std::fs;
use std::io::{BufRead, BufReader};
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc;
use std::sync::Arc;
use std::time::{Duration, Instant};

use kain_core::error::KainError;
use kain_core::tooling_config::apply_cargo_command_defaults;
use notify::{Event, RecursiveMode, Watcher};
use serde::Deserialize;

use crate::native_ui_build::{
    run_native_ui_build_pipeline, NativeUiBuildConfig, NativeUiBuildResult, NativeUiHostKind,
    NativeUiLaunchTarget,
};

const APP_MANIFEST_FILE_NAME: &str = "app_manifest.json";
const RUNTIME_SNAPSHOT_FILE_NAME: &str = "runtime_snapshot.json";
const CONFIG_DIR_NAME: &str = "config";
const STATE_DIR_NAME: &str = "state";
const DEFAULT_DEBOUNCE_MS: u64 = 100;
const NOOP_ONLY_ARTIFACT_ROLES: &[&str] = &["source_input"];
const DEFAULT_IGNORED_DIR_NAMES: &[&str] = &[".git", "node_modules", "target"];
const DEFAULT_EDITOR_TEMP_SUFFIXES: &[&str] =
    &[".swp", ".swo", ".tmp", ".temp", ".bak", "~", ".orig"];

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ReloadDecision {
    Noop,
    HotReloadInProcess,
    RestartProcess,
}

#[derive(Debug, Clone)]
pub struct NativeUiDevConfig {
    pub input: PathBuf,
    pub watch_root: PathBuf,
    pub build: NativeUiBuildConfig,
    pub debounce_window: Duration,
    pub ignored_dir_names: BTreeSet<String>,
}

impl NativeUiDevConfig {
    pub fn new(input: PathBuf, build: NativeUiBuildConfig) -> Result<Self, KainError> {
        let input = absolute_path(&input)?;
        let watch_root = input.parent().map(Path::to_path_buf).ok_or_else(|| {
            KainError::runtime(format!(
                "Native UI dev requires an input file inside a readable directory: {}",
                input.display()
            ))
        })?;

        Ok(Self {
            input,
            watch_root,
            build,
            debounce_window: Duration::from_millis(DEFAULT_DEBOUNCE_MS),
            ignored_dir_names: DEFAULT_IGNORED_DIR_NAMES
                .iter()
                .map(|value| value.to_string())
                .collect(),
        })
    }
}

#[derive(Debug, Clone)]
pub struct NativeUiDevEventReport {
    pub changed_paths: Vec<PathBuf>,
    pub changed_artifact_roles: Vec<String>,
    pub decision: ReloadDecision,
    pub elapsed_ms: u128,
    pub note: String,
}

#[derive(Debug)]
pub struct NativeUiDevSession {
    config: NativeUiDevConfig,
    project_dir: PathBuf,
    artifact_dir: PathBuf,
    launch_target: NativeUiLaunchTarget,
    launch_working_dir: PathBuf,
    child: Option<Child>,
    running: Arc<AtomicBool>,
    previous_manifest: NativeUiDevManifestState,
}

impl NativeUiDevSession {
    pub fn start(config: NativeUiDevConfig) -> Result<Self, KainError> {
        let running = Arc::new(AtomicBool::new(true));
        let ctrlc_flag = running.clone();
        ctrlc::set_handler(move || {
            println!("\n Native UI dev: stopping...");
            ctrlc_flag.store(false, Ordering::SeqCst);
        })
        .map_err(|err| KainError::runtime(format!("Failed to install Ctrl-C handler: {err}")))?;

        let initial_result = build_native_ui(config.input.as_path(), &config.build, true)?;
        let launch_target = initial_result
            .generated
            .launch_target
            .clone()
            .ok_or_else(|| {
                KainError::runtime(
                    "Native UI dev requires a launch target; initial materialization skipped it",
                )
            })?;

        sync_artifacts_to_launch_target(&initial_result, &launch_target)?;
        let previous_manifest = load_manifest_state(&initial_result)?;
        let project_dir = initial_result.generated.project_dir.clone();
        let artifact_dir = resolve_artifact_dir(&initial_result, &config)?;
        let launch_working_dir = resolve_launch_working_dir(&launch_target, &project_dir);

        let mut session = Self {
            config,
            project_dir,
            artifact_dir,
            launch_target,
            launch_working_dir,
            child: None,
            running,
            previous_manifest,
        };
        session.launch_child()?;
        Ok(session)
    }

    pub fn run(mut self) -> Result<(), KainError> {
        println!(" Native UI dev root: {}", self.config.watch_root.display());
        println!(
            " Native UI launch target: {}",
            self.launch_target.path().display()
        );
        println!(" Watching for Kain/native app changes... (Ctrl+C to stop)");

        let (tx, rx) = mpsc::channel();
        let mut watcher = notify::recommended_watcher(move |res: Result<Event, notify::Error>| {
            let _ = tx.send(res);
        })
        .map_err(|err| KainError::runtime(format!("Failed to create file watcher: {err}")))?;

        watcher
            .watch(&self.config.watch_root, RecursiveMode::Recursive)
            .map_err(|err| {
                KainError::runtime(format!(
                    "Failed to watch native UI root {}: {}",
                    self.config.watch_root.display(),
                    err
                ))
            })?;

        let mut pending_paths = BTreeSet::new();
        let mut last_event_at = None;

        while self.running.load(Ordering::SeqCst) {
            self.observe_child_exit()?;

            match rx.recv_timeout(Duration::from_millis(50)) {
                Ok(Ok(event)) => {
                    self.collect_relevant_paths(&event, &mut pending_paths)?;
                    if !pending_paths.is_empty() {
                        last_event_at = Some(Instant::now());
                    }
                }
                Ok(Err(err)) => {
                    eprintln!(" Native UI watcher error: {}", err);
                }
                Err(mpsc::RecvTimeoutError::Timeout) => {}
                Err(mpsc::RecvTimeoutError::Disconnected) => break,
            }

            if let Some(last_event_instant) = last_event_at {
                if !pending_paths.is_empty()
                    && last_event_instant.elapsed() >= self.config.debounce_window
                {
                    let changed_paths = pending_paths.iter().cloned().collect::<Vec<_>>();
                    pending_paths.clear();
                    last_event_at = None;

                    let report = self.rebuild(changed_paths)?;
                    print_event_report(&report);
                }
            }
        }

        self.stop_child()?;
        Ok(())
    }

    fn rebuild(
        &mut self,
        changed_paths: Vec<PathBuf>,
    ) -> Result<NativeUiDevEventReport, KainError> {
        let started = Instant::now();
        let bundle_result =
            build_native_ui(self.config.input.as_path(), &self.config.build, false)?;
        sync_artifacts_to_launch_target(&bundle_result, &self.launch_target)?;
        let bundle_manifest = load_manifest_state(&bundle_result)?;
        let (initial_decision, initial_note) = classify_reload_decision(
            &self.previous_manifest,
            &bundle_manifest,
            &self.launch_target,
            self.child.is_some(),
        );

        let (decision, note, final_result, manifest) = if restart_requires_launch_target_rebuild(
            self.config.build.host,
            &self.previous_manifest,
            &bundle_manifest,
        ) {
            let launch_result =
                build_native_ui(self.config.input.as_path(), &self.config.build, true)?;
            let next_launch_target =
                launch_result
                    .generated
                    .launch_target
                    .clone()
                    .ok_or_else(|| {
                        KainError::runtime(
                    "Native UI dev expected a launch target after restart-triggering rebuild",
                )
                    })?;
            sync_artifacts_to_launch_target(&launch_result, &next_launch_target)?;
            let launch_manifest = load_manifest_state(&launch_result)?;
            let (rebuilt_decision, rebuilt_note) = classify_reload_decision(
                &self.previous_manifest,
                &launch_manifest,
                &next_launch_target,
                self.child.is_some(),
            );
            self.launch_target = next_launch_target;
            self.launch_working_dir = resolve_launch_working_dir(
                &self.launch_target,
                &launch_result.generated.project_dir,
            );
            (
                rebuilt_decision,
                rebuilt_note,
                launch_result,
                launch_manifest,
            )
        } else {
            (
                initial_decision,
                initial_note,
                bundle_result,
                bundle_manifest,
            )
        };

        self.project_dir = final_result.generated.project_dir.clone();
        self.artifact_dir = resolve_artifact_dir(&final_result, &self.config)?;
        if let Some(launch_target) = final_result.generated.launch_target.clone() {
            self.launch_target = launch_target;
            self.launch_working_dir =
                resolve_launch_working_dir(&self.launch_target, &self.project_dir);
        }

        match decision {
            ReloadDecision::Noop => {}
            ReloadDecision::HotReloadInProcess => {}
            ReloadDecision::RestartProcess => {
                self.restart_child()?;
            }
        }

        let changed_artifact_roles = manifest
            .hot_reload
            .changed_artifact_roles
            .iter()
            .filter(|role| !NOOP_ONLY_ARTIFACT_ROLES.contains(&role.as_str()))
            .cloned()
            .collect::<Vec<_>>();
        self.previous_manifest = manifest;

        Ok(NativeUiDevEventReport {
            changed_paths,
            changed_artifact_roles,
            decision,
            elapsed_ms: started.elapsed().as_millis(),
            note,
        })
    }

    fn collect_relevant_paths(
        &self,
        event: &Event,
        pending_paths: &mut BTreeSet<PathBuf>,
    ) -> Result<(), KainError> {
        for path in &event.paths {
            let absolute = absolute_path(path)?;
            if self.should_watch_path(&absolute)? {
                pending_paths.insert(absolute);
            }
        }
        Ok(())
    }

    fn should_watch_path(&self, path: &Path) -> Result<bool, KainError> {
        if !path.starts_with(&self.config.watch_root) {
            return Ok(false);
        }
        if is_editor_temp_path(path) {
            return Ok(false);
        }

        if path.starts_with(&self.project_dir) {
            return Ok(false);
        }

        if path.starts_with(&self.artifact_dir) {
            return Ok(false);
        }

        let ignored = path.components().any(|component| {
            let std::path::Component::Normal(value) = component else {
                return false;
            };
            let Some(value) = value.to_str() else {
                return false;
            };
            self.config
                .ignored_dir_names
                .iter()
                .any(|ignored| ignored.eq_ignore_ascii_case(value))
        });
        Ok(!ignored)
    }

    fn observe_child_exit(&mut self) -> Result<(), KainError> {
        let Some(child) = self.child.as_mut() else {
            return Ok(());
        };
        if let Some(status) = child
            .try_wait()
            .map_err(|err| KainError::runtime(format!("Failed to poll native UI child: {err}")))?
        {
            println!(" Native UI child exited: {}", status);
            self.child = None;
        }
        Ok(())
    }

    fn launch_child(&mut self) -> Result<(), KainError> {
        if !self.launch_target.path().exists() {
            return Err(KainError::runtime(format!(
                "Native UI launch target does not exist: {}",
                self.launch_target.path().display()
            )));
        }

        let mut command = match &self.launch_target {
            NativeUiLaunchTarget::Executable(path) => {
                let mut command = Command::new(path);
                command.current_dir(path.parent().unwrap_or(self.launch_working_dir.as_path()));
                command
            }
            NativeUiLaunchTarget::CargoManifest(manifest_path) => {
                let mut command = Command::new("cargo");
                command.arg("run").arg("--manifest-path").arg(manifest_path);
                apply_cargo_command_defaults(&mut command);
                if self.config.build.release {
                    command.arg("--release");
                }
                command.current_dir(&self.launch_working_dir);
                command
            }
        };
        command.stdout(Stdio::piped()).stderr(Stdio::piped());

        let mut child = command.spawn().map_err(|err| {
            KainError::runtime(format!(
                "Failed to launch native UI target {}: {}",
                self.launch_target.path().display(),
                err
            ))
        })?;

        if let Some(stdout) = child.stdout.take() {
            std::thread::spawn(move || {
                let reader = BufReader::new(stdout);
                for line in reader.lines().map_while(Result::ok) {
                    println!(" [native-ui] {}", line);
                }
            });
        }

        if let Some(stderr) = child.stderr.take() {
            std::thread::spawn(move || {
                let reader = BufReader::new(stderr);
                for line in reader.lines().map_while(Result::ok) {
                    eprintln!(" [native-ui] {}", line);
                }
            });
        }

        self.child = Some(child);
        Ok(())
    }

    fn restart_child(&mut self) -> Result<(), KainError> {
        self.stop_child()?;
        self.launch_child()
    }

    fn stop_child(&mut self) -> Result<(), KainError> {
        let Some(mut child) = self.child.take() else {
            return Ok(());
        };

        if let Err(err) = child.kill() {
            let already_exited = child.try_wait().ok().flatten().is_some();
            if !already_exited {
                return Err(KainError::runtime(format!(
                    "Failed to stop native UI child {}: {}",
                    self.launch_target.path().display(),
                    err
                )));
            }
        }

        let _ = child.wait();
        Ok(())
    }
}

pub fn run_native_ui_dev(config: NativeUiDevConfig) -> Result<(), KainError> {
    NativeUiDevSession::start(config)?.run()
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
struct NativeUiDevLauncher {
    kind: String,
    function_name: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
struct NativeUiDevHotReloadIdentity {
    app_id: String,
    name: String,
    window_title: String,
    root_component: String,
    active_world: Option<String>,
    layout_id: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Deserialize)]
struct NativeUiDevReloadParticipantField {
    name: String,
    type_name: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Deserialize)]
struct NativeUiDevReloadWorldParticipant {
    name: String,
    #[serde(default)]
    state_fields: Vec<NativeUiDevReloadParticipantField>,
    #[serde(default)]
    surface_kinds: Vec<String>,
    migration_mode: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Deserialize)]
struct NativeUiDevReloadActorParticipant {
    name: String,
    #[serde(default)]
    state_type: Option<String>,
    #[serde(default)]
    state_fields: Vec<NativeUiDevReloadParticipantField>,
    #[serde(default)]
    message_types: Vec<String>,
    migration_mode: String,
    quiesce_boundary: String,
    mailbox_transfer: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Deserialize)]
struct NativeUiDevReloadGpuHooks {
    swap_boundary: String,
    #[serde(default)]
    shader_bundle_role: Option<String>,
    resource_graph_reload: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Deserialize)]
struct NativeUiDevReloadParticipants {
    package_surface: String,
    default_state_migration: String,
    default_actor_quiesce: String,
    #[serde(default)]
    default_restart_mode: String,
    #[serde(default)]
    compatibility_lanes: Vec<String>,
    #[serde(default)]
    worlds: Vec<NativeUiDevReloadWorldParticipant>,
    #[serde(default)]
    actors: Vec<NativeUiDevReloadActorParticipant>,
    #[serde(default)]
    gpu_hooks: NativeUiDevReloadGpuHooks,
}

#[derive(Debug, Clone, Default, PartialEq, Eq, Deserialize)]
struct NativeUiDevHotReloadTransition {
    #[serde(default)]
    class: String,
    #[serde(default)]
    restart_required: bool,
    #[serde(default)]
    reasons: Vec<String>,
    #[serde(default)]
    actions: Vec<String>,
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
struct NativeUiDevHotReload {
    changed_artifact_roles: Vec<String>,
    reload_compatible_with_previous: bool,
    identity: NativeUiDevHotReloadIdentity,
    #[serde(default)]
    participants: NativeUiDevReloadParticipants,
    #[serde(default)]
    transition: NativeUiDevHotReloadTransition,
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
struct NativeUiDevRuntimeSidecars {
    runtime_bundle: String,
    runtime_contract: String,
    runtime_compatibility: String,
    realtime_bundle: String,
    shader_bundle: Option<String>,
    #[serde(default)]
    reflection_payload: Option<String>,
    runtime_snapshot: String,
}

#[derive(Debug, Clone, Deserialize)]
struct NativeUiDevManifest {
    launcher: NativeUiDevLauncher,
    hot_reload: NativeUiDevHotReload,
    runtime_sidecars: NativeUiDevRuntimeSidecars,
}

#[derive(Debug, Clone, Deserialize)]
struct NativeUiDevSnapshotPanel {
    id: String,
}

#[derive(Debug, Clone, Deserialize)]
struct NativeUiDevSnapshotCommand {
    id: String,
}

#[derive(Debug, Clone, Deserialize)]
struct NativeUiDevSnapshotProvider {
    id: String,
}

#[derive(Debug, Clone, Deserialize)]
struct NativeUiDevSnapshotTool {
    id: String,
}

#[derive(Debug, Clone, Deserialize)]
struct NativeUiDevSnapshot {
    app_id: String,
    name: String,
    version: String,
    window_title: String,
    root_component: String,
    active_world: Option<String>,
    layout_id: String,
    required_runtime_capabilities: Vec<String>,
    panels: Vec<NativeUiDevSnapshotPanel>,
    commands: Vec<NativeUiDevSnapshotCommand>,
    providers: Vec<NativeUiDevSnapshotProvider>,
    tools: Vec<NativeUiDevSnapshotTool>,
    #[serde(default)]
    reload: NativeUiDevReloadParticipants,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct NativeUiDevSnapshotContract {
    app_id: String,
    name: String,
    version: String,
    window_title: String,
    root_component: String,
    active_world: Option<String>,
    layout_id: String,
    required_runtime_capabilities: Vec<String>,
    panel_ids: Vec<String>,
    command_ids: Vec<String>,
    provider_ids: Vec<String>,
    tool_ids: Vec<String>,
    reload: NativeUiDevReloadParticipants,
}

#[derive(Debug, Clone)]
struct NativeUiDevManifestState {
    launcher: NativeUiDevLauncher,
    hot_reload: NativeUiDevHotReload,
    runtime_sidecars: NativeUiDevRuntimeSidecars,
    snapshot_contract: NativeUiDevSnapshotContract,
}

fn build_native_ui(
    input: &Path,
    config: &NativeUiBuildConfig,
    build_executable: bool,
) -> Result<NativeUiBuildResult, KainError> {
    let mut config = config.clone();
    config.build_executable = match config.host {
        NativeUiHostKind::Qt => build_executable,
        NativeUiHostKind::Tauri => false,
    };
    run_native_ui_build_pipeline(input, &config)
}

fn load_manifest_state(
    result: &NativeUiBuildResult,
) -> Result<NativeUiDevManifestState, KainError> {
    let manifest_path = result
        .generated
        .project_dir
        .join(CONFIG_DIR_NAME)
        .join(APP_MANIFEST_FILE_NAME);
    let manifest_source = fs::read_to_string(&manifest_path).map_err(|err| {
        KainError::runtime(format!(
            "Failed to read native UI manifest {}: {}",
            manifest_path.display(),
            err
        ))
    })?;
    let manifest: NativeUiDevManifest = serde_json::from_str(&manifest_source).map_err(|err| {
        KainError::runtime(format!(
            "Failed to parse native UI manifest {}: {}",
            manifest_path.display(),
            err
        ))
    })?;

    let snapshot_path = result
        .generated
        .project_dir
        .join(STATE_DIR_NAME)
        .join(RUNTIME_SNAPSHOT_FILE_NAME);
    let snapshot_source = fs::read_to_string(&snapshot_path).map_err(|err| {
        KainError::runtime(format!(
            "Failed to read native UI snapshot {}: {}",
            snapshot_path.display(),
            err
        ))
    })?;
    let snapshot: NativeUiDevSnapshot = serde_json::from_str(&snapshot_source).map_err(|err| {
        KainError::runtime(format!(
            "Failed to parse native UI snapshot {}: {}",
            snapshot_path.display(),
            err
        ))
    })?;

    Ok(NativeUiDevManifestState {
        launcher: manifest.launcher,
        hot_reload: manifest.hot_reload,
        runtime_sidecars: manifest.runtime_sidecars,
        snapshot_contract: snapshot_contract_from_snapshot(snapshot),
    })
}

fn snapshot_contract_from_snapshot(snapshot: NativeUiDevSnapshot) -> NativeUiDevSnapshotContract {
    let mut required_runtime_capabilities = snapshot.required_runtime_capabilities;
    let mut panel_ids = snapshot
        .panels
        .into_iter()
        .map(|panel| panel.id)
        .collect::<Vec<_>>();
    let mut command_ids = snapshot
        .commands
        .into_iter()
        .map(|command| command.id)
        .collect::<Vec<_>>();
    let mut provider_ids = snapshot
        .providers
        .into_iter()
        .map(|provider| provider.id)
        .collect::<Vec<_>>();
    let mut tool_ids = snapshot
        .tools
        .into_iter()
        .map(|tool| tool.id)
        .collect::<Vec<_>>();

    required_runtime_capabilities.sort();
    panel_ids.sort();
    command_ids.sort();
    provider_ids.sort();
    tool_ids.sort();

    NativeUiDevSnapshotContract {
        app_id: snapshot.app_id,
        name: snapshot.name,
        version: snapshot.version,
        window_title: snapshot.window_title,
        root_component: snapshot.root_component,
        active_world: snapshot.active_world,
        layout_id: snapshot.layout_id,
        required_runtime_capabilities,
        panel_ids,
        command_ids,
        provider_ids,
        tool_ids,
        reload: snapshot.reload,
    }
}

fn classify_reload_decision(
    previous: &NativeUiDevManifestState,
    current: &NativeUiDevManifestState,
    launch_target: &NativeUiLaunchTarget,
    has_running_child: bool,
) -> (ReloadDecision, String) {
    let changed_roles = current
        .hot_reload
        .changed_artifact_roles
        .iter()
        .filter(|role| !NOOP_ONLY_ARTIFACT_ROLES.contains(&role.as_str()))
        .cloned()
        .collect::<Vec<_>>();

    let launcher_changed = previous.launcher != current.launcher;
    let sidecar_contract_changed = previous.runtime_sidecars != current.runtime_sidecars;
    let snapshot_contract_changed = previous.snapshot_contract != current.snapshot_contract;
    let transition_requires_restart = current.hot_reload.transition.restart_required;
    let needs_restart = launcher_changed
        || sidecar_contract_changed
        || snapshot_contract_changed
        || transition_requires_restart
        || !current.hot_reload.reload_compatible_with_previous
        || !launch_target.path().exists()
        || !has_running_child;

    if changed_roles.is_empty() && !needs_restart {
        return (
            ReloadDecision::Noop,
            "source changed without a runtime-sidecar delta".to_string(),
        );
    }

    if needs_restart {
        let reason = if launcher_changed {
            "launcher contract changed"
        } else if sidecar_contract_changed {
            "runtime sidecar contract changed"
        } else if snapshot_contract_changed {
            "runtime snapshot contract changed"
        } else if transition_requires_restart {
            current
                .hot_reload
                .transition
                .reasons
                .first()
                .map(String::as_str)
                .unwrap_or("hot reload transition requires restart")
        } else if !current.hot_reload.reload_compatible_with_previous {
            "hot reload compatibility gate failed"
        } else if !launch_target.path().exists() {
            "native UI launch target is missing"
        } else {
            "native UI child is not running"
        };
        return (ReloadDecision::RestartProcess, reason.to_string());
    }

    let transition_class = if current.hot_reload.transition.class.is_empty() {
        "hot-reload"
    } else {
        current.hot_reload.transition.class.as_str()
    };
    let transition_actions = if current.hot_reload.transition.actions.is_empty() {
        String::new()
    } else {
        format!(
            " | actions={}",
            current.hot_reload.transition.actions.join(", ")
        )
    };
    (
        ReloadDecision::HotReloadInProcess,
        if changed_roles.is_empty() {
            format!(
                "{} | runtime-sidecar rewrite requested without a semantic role delta{}",
                transition_class, transition_actions
            )
        } else {
            format!(
                "{} | updated runtime sidecars: {}{}",
                transition_class,
                changed_roles.join(", "),
                transition_actions
            )
        },
    )
}

fn restart_requires_launch_target_rebuild(
    host: NativeUiHostKind,
    previous: &NativeUiDevManifestState,
    current: &NativeUiDevManifestState,
) -> bool {
    matches!(host, NativeUiHostKind::Qt) && previous.launcher != current.launcher
}

fn sync_artifacts_to_launch_target(
    result: &NativeUiBuildResult,
    launch_target: &NativeUiLaunchTarget,
) -> Result<(), KainError> {
    let NativeUiLaunchTarget::Executable(executable_path) = launch_target else {
        return Ok(());
    };
    let Some(executable_dir) = executable_path.parent() else {
        return Ok(());
    };
    fs::create_dir_all(executable_dir).map_err(|err| {
        KainError::runtime(format!(
            "Failed to create native UI executable directory {}: {}",
            executable_dir.display(),
            err
        ))
    })?;

    for artifact_path in &result.generated.artifact_paths {
        if !artifact_path.is_file() {
            continue;
        }
        let Some(file_name) = artifact_path.file_name() else {
            continue;
        };
        let destination = executable_dir.join(file_name);
        if destination == *artifact_path {
            continue;
        }
        fs::copy(artifact_path, &destination).map_err(|err| {
            KainError::runtime(format!(
                "Failed to sync native UI sidecar {} -> {}: {}",
                artifact_path.display(),
                destination.display(),
                err
            ))
        })?;
    }

    Ok(())
}

fn resolve_launch_working_dir(launch_target: &NativeUiLaunchTarget, project_dir: &Path) -> PathBuf {
    match launch_target {
        NativeUiLaunchTarget::Executable(path) => path
            .parent()
            .map(Path::to_path_buf)
            .unwrap_or_else(|| project_dir.to_path_buf()),
        NativeUiLaunchTarget::CargoManifest(manifest_path) => manifest_path
            .parent()
            .and_then(Path::parent)
            .map(Path::to_path_buf)
            .unwrap_or_else(|| project_dir.to_path_buf()),
    }
}

fn resolve_artifact_dir(
    result: &NativeUiBuildResult,
    config: &NativeUiDevConfig,
) -> Result<PathBuf, KainError> {
    let artifact_dir = &config.build.artifact_output_dir;
    if artifact_dir.is_absolute() {
        Ok(artifact_dir.clone())
    } else {
        Ok(result.generated.project_dir.join(artifact_dir))
    }
}

fn print_event_report(report: &NativeUiDevEventReport) {
    let decision = match report.decision {
        ReloadDecision::Noop => "noop",
        ReloadDecision::HotReloadInProcess => "hot-reload",
        ReloadDecision::RestartProcess => "restart",
    };

    println!(
        " Native UI dev [{} ms] {}: {}",
        report.elapsed_ms, decision, report.note
    );

    if !report.changed_paths.is_empty() {
        let preview = report
            .changed_paths
            .iter()
            .take(6)
            .map(|path| path.display().to_string())
            .collect::<Vec<_>>();
        println!("   changed: {}", preview.join(", "));
        if report.changed_paths.len() > preview.len() {
            println!(
                "   changed: ... {} more paths",
                report.changed_paths.len() - preview.len()
            );
        }
    }

    if !report.changed_artifact_roles.is_empty() {
        println!("   artifacts: {}", report.changed_artifact_roles.join(", "));
    }
}

fn is_editor_temp_path(path: &Path) -> bool {
    let Some(name) = path.file_name().and_then(OsStr::to_str) else {
        return false;
    };
    if name.starts_with('.') && name.ends_with(".swp") {
        return true;
    }
    DEFAULT_EDITOR_TEMP_SUFFIXES
        .iter()
        .any(|suffix| name.ends_with(suffix))
}

fn absolute_path(path: &Path) -> Result<PathBuf, KainError> {
    if path.is_absolute() {
        return Ok(path.to_path_buf());
    }
    let cwd = std::env::current_dir()
        .map_err(|err| KainError::runtime(format!("Failed to resolve current directory: {err}")))?;
    Ok(cwd.join(path))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::path::PathBuf;
    use std::time::{SystemTime, UNIX_EPOCH};

    fn test_executable_path() -> PathBuf {
        let unique = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_nanos();
        let dir = std::env::temp_dir().join(format!("kain_native_ui_dev_test_{unique}"));
        fs::create_dir_all(&dir).expect("create temp native-ui test directory");
        let executable = dir.join("chronos_native_test.exe");
        fs::write(&executable, b"").expect("write temp native-ui executable");
        executable
    }

    fn manifest_state(
        changed_roles: &[&str],
        reload_compatible_with_previous: bool,
        transition_class: &str,
    ) -> NativeUiDevManifestState {
        let restart_required = transition_class == "restart-with-restore";
        let transition_actions = match transition_class {
            "frame-boundary-gpu-swap" => {
                vec![
                    "preserve-ui-state".to_string(),
                    "swap-gpu-at-frame-boundary".to_string(),
                ]
            }
            "quiesce-and-migrate" => vec![
                "preserve-ui-state".to_string(),
                "quiesce-actors-at-turn-boundary".to_string(),
                "transfer-queued-actor-messages".to_string(),
                "migrate-world-state-structurally".to_string(),
            ],
            "structural-migrate" => vec![
                "preserve-ui-state".to_string(),
                "migrate-world-state-structurally".to_string(),
            ],
            "restart-with-restore" => {
                vec![
                    "restart-process".to_string(),
                    "restore-runtime-snapshot".to_string(),
                ]
            }
            "presentation-only" => {
                vec![
                    "preserve-ui-state".to_string(),
                    "patch-runtime-presentation".to_string(),
                ]
            }
            _ => vec!["preserve-ui-state".to_string()],
        };
        let reload = NativeUiDevReloadParticipants {
            package_surface: "std::reload".to_string(),
            default_state_migration: "auto-structural".to_string(),
            default_actor_quiesce: "turn-boundary".to_string(),
            default_restart_mode: "restart-with-snapshot-restore".to_string(),
            compatibility_lanes: vec![
                "cold-start".to_string(),
                "noop".to_string(),
                "presentation-only".to_string(),
                "structural-migrate".to_string(),
                "quiesce-and-migrate".to_string(),
                "frame-boundary-gpu-swap".to_string(),
                "restart-with-restore".to_string(),
            ],
            worlds: vec![NativeUiDevReloadWorldParticipant {
                name: "ChronosLab".to_string(),
                state_fields: vec![NativeUiDevReloadParticipantField {
                    name: "counter".to_string(),
                    type_name: "Int".to_string(),
                }],
                surface_kinds: vec!["native_ui".to_string(), "viewport3d".to_string()],
                migration_mode: "auto-structural".to_string(),
            }],
            actors: vec![NativeUiDevReloadActorParticipant {
                name: "ChronosDriver".to_string(),
                state_type: Some("ChronosDriverState".to_string()),
                state_fields: vec![NativeUiDevReloadParticipantField {
                    name: "tick".to_string(),
                    type_name: "Int".to_string(),
                }],
                message_types: vec!["Ping".to_string()],
                migration_mode: "auto-structural".to_string(),
                quiesce_boundary: "turn-boundary".to_string(),
                mailbox_transfer: "preserve-queued-messages".to_string(),
            }],
            gpu_hooks: NativeUiDevReloadGpuHooks {
                swap_boundary: "frame-boundary".to_string(),
                shader_bundle_role: Some("shader_bundle".to_string()),
                resource_graph_reload: "planned".to_string(),
            },
        };
        NativeUiDevManifestState {
            launcher: NativeUiDevLauncher {
                kind: "run_bundled_app_json".to_string(),
                function_name: "run_bundled_app_json".to_string(),
            },
            hot_reload: NativeUiDevHotReload {
                changed_artifact_roles: changed_roles
                    .iter()
                    .map(|value| value.to_string())
                    .collect(),
                reload_compatible_with_previous,
                identity: NativeUiDevHotReloadIdentity {
                    app_id: "chronos.native".to_string(),
                    name: "Chronos".to_string(),
                    window_title: "Chronos".to_string(),
                    root_component: "App".to_string(),
                    active_world: Some("ChronosLab".to_string()),
                    layout_id: "chronos_shell".to_string(),
                },
                participants: reload.clone(),
                transition: NativeUiDevHotReloadTransition {
                    class: transition_class.to_string(),
                    restart_required,
                    reasons: if restart_required {
                        vec!["std::reload participant contract changed".to_string()]
                    } else if transition_class == "noop" {
                        vec!["source changed without a runtime-sidecar delta".to_string()]
                    } else {
                        vec![format!(
                            "changed runtime artifacts: {}",
                            changed_roles.join(", ")
                        )]
                    },
                    actions: transition_actions,
                },
            },
            runtime_sidecars: NativeUiDevRuntimeSidecars {
                runtime_bundle: "native_app_bundle.json".to_string(),
                runtime_contract: "contract.json".to_string(),
                runtime_compatibility: "compatibility.json".to_string(),
                realtime_bundle: "kain_realtime_app_bundle.json".to_string(),
                shader_bundle: Some("kain_shader_bundle.json".to_string()),
                reflection_payload: Some("kain_reflection_payload.json".to_string()),
                runtime_snapshot: "runtime_snapshot.json".to_string(),
            },
            snapshot_contract: NativeUiDevSnapshotContract {
                app_id: "chronos.native".to_string(),
                name: "Chronos".to_string(),
                version: "0.1.0".to_string(),
                window_title: "Chronos".to_string(),
                root_component: "App".to_string(),
                active_world: Some("ChronosLab".to_string()),
                layout_id: "chronos_shell".to_string(),
                required_runtime_capabilities: vec!["world.viewport3d".to_string()],
                panel_ids: vec!["left".to_string(), "center".to_string()],
                command_ids: vec!["rebuild".to_string()],
                provider_ids: vec!["native_runtime".to_string()],
                tool_ids: vec!["exec".to_string()],
                reload,
            },
        }
    }

    #[test]
    fn reload_decision_treats_source_only_delta_as_noop() {
        let previous = manifest_state(&[], true, "noop");
        let current = manifest_state(&["source_input"], true, "noop");
        let executable_path = test_executable_path();
        let launch_target = NativeUiLaunchTarget::Executable(executable_path);
        let (decision, note) = classify_reload_decision(&previous, &current, &launch_target, true);
        assert_eq!(decision, ReloadDecision::Noop);
        assert!(note.contains("source changed"));
    }

    #[test]
    fn reload_decision_hot_reloads_runtime_sidecar_changes() {
        let previous = manifest_state(&[], true, "noop");
        let current = manifest_state(
            &["runtime_bundle", "shader_bundle"],
            true,
            "quiesce-and-migrate",
        );
        let executable_path = test_executable_path();
        let launch_target = NativeUiLaunchTarget::Executable(executable_path);
        let (decision, note) = classify_reload_decision(&previous, &current, &launch_target, true);
        assert_eq!(decision, ReloadDecision::HotReloadInProcess);
        assert!(note.contains("quiesce-and-migrate"));
        assert!(note.contains("runtime_bundle"));
    }

    #[test]
    fn reload_decision_restarts_when_compatibility_breaks() {
        let previous = manifest_state(&[], true, "noop");
        let current = manifest_state(&["runtime_bundle"], false, "presentation-only");
        let executable_path = test_executable_path();
        let launch_target = NativeUiLaunchTarget::Executable(executable_path);
        let (decision, note) = classify_reload_decision(&previous, &current, &launch_target, true);
        assert_eq!(decision, ReloadDecision::RestartProcess);
        assert!(note.contains("compatibility"));
    }

    #[test]
    fn reload_decision_restarts_when_reload_participant_contract_changes() {
        let previous = manifest_state(&[], true, "noop");
        let mut current = manifest_state(&["runtime_bundle"], true, "restart-with-restore");
        current.snapshot_contract.reload.actors[0]
            .state_fields
            .push(NativeUiDevReloadParticipantField {
                name: "phase".to_string(),
                type_name: "Int".to_string(),
            });
        let executable_path = test_executable_path();
        let launch_target = NativeUiLaunchTarget::Executable(executable_path);
        let (decision, note) = classify_reload_decision(&previous, &current, &launch_target, true);
        assert_eq!(decision, ReloadDecision::RestartProcess);
        assert!(note.contains("snapshot"));
    }

    #[test]
    fn reload_decision_surfaces_frame_boundary_gpu_swap_transition() {
        let previous = manifest_state(&[], true, "noop");
        let current = manifest_state(&["shader_bundle"], true, "frame-boundary-gpu-swap");
        let executable_path = test_executable_path();
        let launch_target = NativeUiLaunchTarget::Executable(executable_path);
        let (decision, note) = classify_reload_decision(&previous, &current, &launch_target, true);
        assert_eq!(decision, ReloadDecision::HotReloadInProcess);
        assert!(note.contains("frame-boundary-gpu-swap"));
        assert!(note.contains("swap-gpu-at-frame-boundary"));
    }

    #[test]
    fn editor_temp_detection_catches_swap_files() {
        assert!(is_editor_temp_path(Path::new("/tmp/main.kn.swp")));
        assert!(is_editor_temp_path(Path::new("/tmp/main.kn~")));
        assert!(!is_editor_temp_path(Path::new("/tmp/main.kn")));
    }
}
