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
use notify::{Event, RecursiveMode, Watcher};
use serde::Deserialize;

use crate::native_ui_build::{run_native_ui_build_pipeline, NativeUiBuildConfig, NativeUiBuildResult};

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
    executable_dir: PathBuf,
    executable_path: PathBuf,
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
        let executable_path = initial_result
            .generated
            .executable_path
            .clone()
            .ok_or_else(|| {
                KainError::runtime(
                    "Native UI dev requires an executable; initial materialization skipped it",
                )
            })?;

        sync_artifacts_to_executable_dir(&initial_result, &executable_path)?;
        let previous_manifest = load_manifest_state(&initial_result)?;
        let project_dir = initial_result.generated.project_dir.clone();
        let artifact_dir = resolve_artifact_dir(&initial_result, &config)?;
        let executable_dir = executable_path
            .parent()
            .map(Path::to_path_buf)
            .unwrap_or_else(|| project_dir.clone());

        let mut session = Self {
            config,
            project_dir,
            artifact_dir,
            executable_dir,
            executable_path,
            child: None,
            running,
            previous_manifest,
        };
        session.launch_child()?;
        Ok(session)
    }

    pub fn run(mut self) -> Result<(), KainError> {
        println!(
            " Native UI dev root: {}",
            self.config.watch_root.display()
        );
        println!(
            " Native UI executable: {}",
            self.executable_path.display()
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

    fn rebuild(&mut self, changed_paths: Vec<PathBuf>) -> Result<NativeUiDevEventReport, KainError> {
        let started = Instant::now();
        let bundle_result = build_native_ui(self.config.input.as_path(), &self.config.build, false)?;
        sync_artifacts_to_executable_dir(&bundle_result, &self.executable_path)?;
        let bundle_manifest = load_manifest_state(&bundle_result)?;
        let (initial_decision, initial_note) = classify_reload_decision(
            &self.previous_manifest,
            &bundle_manifest,
            &self.executable_path,
            self.child.is_some(),
        );

        let (decision, note, final_result, manifest) =
            if restart_requires_executable_rebuild(&self.previous_manifest, &bundle_manifest) {
                let executable_result =
                    build_native_ui(self.config.input.as_path(), &self.config.build, true)?;
                let executable_path = executable_result.generated.executable_path.clone().ok_or_else(|| {
                    KainError::runtime(
                        "Native UI dev expected an executable after restart-triggering rebuild",
                    )
                })?;
                self.executable_path = executable_path;
                self.executable_dir = self
                    .executable_path
                    .parent()
                    .map(Path::to_path_buf)
                    .unwrap_or_else(|| executable_result.generated.project_dir.clone());
                sync_artifacts_to_executable_dir(&executable_result, &self.executable_path)?;
                let executable_manifest = load_manifest_state(&executable_result)?;
                let (rebuilt_decision, rebuilt_note) = classify_reload_decision(
                    &self.previous_manifest,
                    &executable_manifest,
                    &self.executable_path,
                    self.child.is_some(),
                );
                (
                    rebuilt_decision,
                    rebuilt_note,
                    executable_result,
                    executable_manifest,
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

        if path.starts_with(&self.executable_dir) && self.executable_dir != self.config.watch_root {
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
        if !self.executable_path.exists() {
            return Err(KainError::runtime(format!(
                "Native UI executable does not exist: {}",
                self.executable_path.display()
            )));
        }

        let mut command = Command::new(&self.executable_path);
        command
            .current_dir(
                self.executable_path
                    .parent()
                    .unwrap_or(self.config.watch_root.as_path()),
            )
            .stdout(Stdio::piped())
            .stderr(Stdio::piped());

        let mut child = command.spawn().map_err(|err| {
            KainError::runtime(format!(
                "Failed to launch native UI executable {}: {}",
                self.executable_path.display(),
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
                    self.executable_path.display(),
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

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
struct NativeUiDevHotReload {
    changed_artifact_roles: Vec<String>,
    reload_compatible_with_previous: bool,
    identity: NativeUiDevHotReloadIdentity,
}

#[derive(Debug, Clone, PartialEq, Eq, Deserialize)]
struct NativeUiDevRuntimeSidecars {
    runtime_bundle: String,
    runtime_contract: String,
    runtime_compatibility: String,
    realtime_bundle: String,
    shader_bundle: Option<String>,
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
    config.build_executable = build_executable;
    run_native_ui_build_pipeline(input, &config)
}

fn load_manifest_state(result: &NativeUiBuildResult) -> Result<NativeUiDevManifestState, KainError> {
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
    }
}

fn classify_reload_decision(
    previous: &NativeUiDevManifestState,
    current: &NativeUiDevManifestState,
    executable_path: &Path,
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
    let needs_restart = launcher_changed
        || sidecar_contract_changed
        || snapshot_contract_changed
        || !current.hot_reload.reload_compatible_with_previous
        || !executable_path.exists()
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
        } else if !current.hot_reload.reload_compatible_with_previous {
            "hot reload compatibility gate failed"
        } else if !executable_path.exists() {
            "native UI executable is missing"
        } else {
            "native UI child is not running"
        };
        return (ReloadDecision::RestartProcess, reason.to_string());
    }

    (
        ReloadDecision::HotReloadInProcess,
        if changed_roles.is_empty() {
            "runtime-sidecar rewrite requested without a semantic role delta".to_string()
        } else {
            format!("updated runtime sidecars: {}", changed_roles.join(", "))
        },
    )
}

fn restart_requires_executable_rebuild(
    previous: &NativeUiDevManifestState,
    current: &NativeUiDevManifestState,
) -> bool {
    previous.launcher != current.launcher
}

fn sync_artifacts_to_executable_dir(
    result: &NativeUiBuildResult,
    executable_path: &Path,
) -> Result<(), KainError> {
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
        println!(
            "   artifacts: {}",
            report.changed_artifact_roles.join(", ")
        );
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
    ) -> NativeUiDevManifestState {
        NativeUiDevManifestState {
            launcher: NativeUiDevLauncher {
                kind: "run_bundled_app_json".to_string(),
                function_name: "run_bundled_app_json".to_string(),
            },
            hot_reload: NativeUiDevHotReload {
                changed_artifact_roles: changed_roles.iter().map(|value| value.to_string()).collect(),
                reload_compatible_with_previous,
                identity: NativeUiDevHotReloadIdentity {
                    app_id: "chronos.native".to_string(),
                    name: "Chronos".to_string(),
                    window_title: "Chronos".to_string(),
                    root_component: "App".to_string(),
                    active_world: Some("ChronosLab".to_string()),
                    layout_id: "chronos_shell".to_string(),
                },
            },
            runtime_sidecars: NativeUiDevRuntimeSidecars {
                runtime_bundle: "native_app_bundle.json".to_string(),
                runtime_contract: "kain_runtime_contract.json".to_string(),
                runtime_compatibility: "kain_runtime_compatibility.json".to_string(),
                realtime_bundle: "kain_realtime_app_bundle.json".to_string(),
                shader_bundle: Some("kain_shader_bundle.json".to_string()),
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
            },
        }
    }

    #[test]
    fn reload_decision_treats_source_only_delta_as_noop() {
        let previous = manifest_state(&[], true);
        let current = manifest_state(&["source_input"], true);
        let executable_path = test_executable_path();
        let (decision, note) =
            classify_reload_decision(&previous, &current, executable_path.as_path(), true);
        assert_eq!(decision, ReloadDecision::Noop);
        assert!(note.contains("source changed"));
    }

    #[test]
    fn reload_decision_hot_reloads_runtime_sidecar_changes() {
        let previous = manifest_state(&[], true);
        let current = manifest_state(&["runtime_bundle", "shader_bundle"], true);
        let executable_path = test_executable_path();
        let (decision, note) =
            classify_reload_decision(&previous, &current, executable_path.as_path(), true);
        assert_eq!(decision, ReloadDecision::HotReloadInProcess);
        assert!(note.contains("runtime_bundle"));
    }

    #[test]
    fn reload_decision_restarts_when_compatibility_breaks() {
        let previous = manifest_state(&[], true);
        let current = manifest_state(&["runtime_bundle"], false);
        let executable_path = test_executable_path();
        let (decision, note) =
            classify_reload_decision(&previous, &current, executable_path.as_path(), true);
        assert_eq!(decision, ReloadDecision::RestartProcess);
        assert!(note.contains("compatibility"));
    }

    #[test]
    fn editor_temp_detection_catches_swap_files() {
        assert!(is_editor_temp_path(Path::new("/tmp/main.kn.swp")));
        assert!(is_editor_temp_path(Path::new("/tmp/main.kn~")));
        assert!(!is_editor_temp_path(Path::new("/tmp/main.kn")));
    }
}
