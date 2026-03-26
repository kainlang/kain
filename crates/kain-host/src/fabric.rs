use std::collections::BTreeMap;
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};

use kain_core::runtime::Env;
use kain_core::CompileTarget;
use kain_omni::fabric::{
    resolve_fabric_path, topological_step_order, unix_timestamp_ms, write_fabric_json,
    FabricEventRecord, FabricExecutionResult, FabricManifest, FabricRuntimeKind,
    FabricSessionStatus, FabricStep, FabricStepExecution, FabricStepStatus, FabricValidationResult,
};
use kain_omni::{OmniError, OmniResult};
use pyo3::prelude::*;

pub struct FabricSession {
    pub manifest_path: PathBuf,
    pub manifest: FabricManifest,
    pub validation: FabricValidationResult,
    pub session_id: String,
    pub workspace_root: PathBuf,
    pub session_directory: PathBuf,
    pub report_path: PathBuf,
    pub lock_path: PathBuf,
    pub events_path: Option<PathBuf>,
    pub started_unix_ms: u128,
    event_writer: FabricEventWriter,
}

impl FabricSession {
    pub fn new(manifest_path: &Path, manifest: FabricManifest) -> OmniResult<Self> {
        let validation = kain_omni::fabric::validate_fabric_manifest(manifest_path, &manifest)?;
        let manifest_root = manifest_path
            .parent()
            .map(Path::to_path_buf)
            .unwrap_or_else(|| PathBuf::from("."));
        let workspace_root = resolve_fabric_path(&manifest_root, &manifest.workspace.root);
        let report_root = resolve_fabric_path(&workspace_root, &manifest.reports.directory);
        fs::create_dir_all(&report_root)?;

        let started_unix_ms = unix_timestamp_ms();
        let session_id = format!("session-{}-{}", started_unix_ms, std::process::id());
        let session_directory = report_root.join(&session_id);
        fs::create_dir_all(&session_directory)?;

        let report_path = session_directory.join("report.json");
        let lock_path = session_directory.join("session.lock.json");
        let events_path = manifest
            .reports
            .emit_jsonl_events
            .then(|| session_directory.join("events.jsonl"));

        let mut event_writer = FabricEventWriter::new(events_path.as_deref())?;
        event_writer.write(&FabricEventRecord {
            timestamp_unix_ms: started_unix_ms,
            kind: "session_started".to_string(),
            step_id: None,
            runtime: None,
            status: None,
            message: format!("Executing Fabric manifest {}", manifest_path.display()),
        })?;

        Ok(Self {
            manifest_path: manifest_path.to_path_buf(),
            manifest,
            validation,
            session_id,
            workspace_root,
            session_directory,
            report_path,
            lock_path,
            events_path,
            started_unix_ms,
            event_writer,
        })
    }

    pub fn log_event(
        &mut self,
        kind: &str,
        step: Option<&FabricStep>,
        message: impl Into<String>,
    ) -> OmniResult<()> {
        let timestamp_unix_ms = unix_timestamp_ms();
        self.event_writer.write(&FabricEventRecord {
            timestamp_unix_ms,
            kind: kind.to_string(),
            step_id: step.map(|s| s.id.clone()),
            runtime: step.map(|s| s.runtime.clone()),
            status: None,
            message: message.into(),
        })
    }

    pub fn log_step_event(
        &mut self,
        kind: &str,
        step: &FabricStep,
        status: FabricStepStatus,
        message: impl Into<String>,
    ) -> OmniResult<()> {
        let timestamp_unix_ms = unix_timestamp_ms();
        self.event_writer.write(&FabricEventRecord {
            timestamp_unix_ms,
            kind: kind.to_string(),
            step_id: Some(step.id.clone()),
            runtime: Some(step.runtime.clone()),
            status: Some(status),
            message: message.into(),
        })
    }

    pub fn finish(
        mut self,
        step_results: Vec<FabricStepExecution>,
    ) -> OmniResult<FabricExecutionResult> {
        let finished_unix_ms = unix_timestamp_ms();
        let status = if step_results
            .iter()
            .all(|result| result.status == FabricStepStatus::Succeeded)
        {
            FabricSessionStatus::Succeeded
        } else {
            FabricSessionStatus::Failed
        };

        let result = FabricExecutionResult {
            manifest_path: self.manifest_path.clone(),
            validation: self.validation.clone(),
            session_id: self.session_id.clone(),
            workspace_root: self.workspace_root.clone(),
            session_directory: self.session_directory.clone(),
            report_path: self.report_path.clone(),
            lock_path: self.lock_path.clone(),
            events_path: self.events_path.clone(),
            status,
            started_unix_ms: self.started_unix_ms,
            finished_unix_ms,
            step_results,
        };

        self.log_event(
            "session_finished",
            None,
            format!("Fabric session finished with status {:?}", result.status),
        )?;
        write_fabric_json(&self.report_path, &result)?;
        write_fabric_json(&self.lock_path, &result)?;
        Ok(result)
    }
}

pub struct FabricExecutor {
    adapters: Vec<Box<dyn FabricRuntimeAdapter>>,
}

impl FabricExecutor {
    pub fn new() -> Self {
        Self {
            adapters: vec![Box::new(KainAdapter), Box::new(PythonAdapter)],
        }
    }

    pub fn add_adapter(&mut self, adapter: Box<dyn FabricRuntimeAdapter>) {
        self.adapters.push(adapter);
    }

    pub fn execute(&self, mut session: FabricSession) -> OmniResult<FabricExecutionResult> {
        let execution_order = topological_step_order(&session.manifest.steps)?;
        let steps_by_id = session
            .manifest
            .steps
            .iter()
            .cloned()
            .map(|step| (step.id.clone(), step))
            .collect::<BTreeMap<_, _>>();

        let mut step_results = session
            .manifest
            .steps
            .iter()
            .map(|step| {
                (
                    step.id.clone(),
                    FabricStepExecution {
                        id: step.id.clone(),
                        runtime: step.runtime.clone(),
                        entry: step.entry.clone(),
                        depends_on: step.depends_on.clone(),
                        status: FabricStepStatus::Pending,
                        started_unix_ms: None,
                        finished_unix_ms: None,
                        output: None,
                        error: None,
                    },
                )
            })
            .collect::<BTreeMap<_, _>>();

        for step_id in &execution_order {
            let step = steps_by_id.get(step_id).ok_or_else(|| {
                OmniError::Config(format!(
                    "Fabric execution lost step definition for '{step_id}'"
                ))
            })?;

            let blocked_by = step
                .depends_on
                .iter()
                .filter(|dep| {
                    step_results
                        .get(*dep)
                        .is_some_and(|res| res.status != FabricStepStatus::Succeeded)
                })
                .cloned()
                .collect::<Vec<_>>();

            let step_result = step_results.get_mut(step_id).ok_or_else(|| {
                OmniError::Config(format!("Fabric execution lost step state for '{step_id}'"))
            })?;

            if !blocked_by.is_empty() {
                let finished_unix_ms = unix_timestamp_ms();
                let message = format!(
                    "Blocked by incomplete dependencies: {}",
                    blocked_by.join(", ")
                );
                step_result.status = FabricStepStatus::Blocked;
                step_result.finished_unix_ms = Some(finished_unix_ms);
                step_result.error = Some(message.clone());
                session.log_step_event("step_blocked", step, FabricStepStatus::Blocked, message)?;
                continue;
            }

            let step_started_unix_ms = unix_timestamp_ms();
            step_result.started_unix_ms = Some(step_started_unix_ms);
            session.log_step_event(
                "step_started",
                step,
                FabricStepStatus::Pending,
                format!("Executing Fabric step '{}'", step.id),
            )?;

            let adapter = self.adapters.iter().find(|a| a.supports(&step.runtime));

            match adapter {
                Some(adapter) => match adapter.execute(&session.workspace_root, step) {
                    Ok(output) => {
                        let finished_unix_ms = unix_timestamp_ms();
                        step_result.status = FabricStepStatus::Succeeded;
                        step_result.finished_unix_ms = Some(finished_unix_ms);
                        step_result.output = Some(output);
                        session.log_step_event(
                            "step_succeeded",
                            step,
                            FabricStepStatus::Succeeded,
                            format!("Step '{}' completed", step.id),
                        )?;
                    }
                    Err(error) => {
                        let finished_unix_ms = unix_timestamp_ms();
                        step_result.status = FabricStepStatus::Failed;
                        step_result.finished_unix_ms = Some(finished_unix_ms);
                        step_result.error = Some(error.clone());
                        session.log_step_event(
                            "step_failed",
                            step,
                            FabricStepStatus::Failed,
                            error,
                        )?;
                    }
                },
                None => {
                    let finished_unix_ms = unix_timestamp_ms();
                    let error = format!(
                        "No adapter found for runtime '{}'",
                        step.runtime.display_name()
                    );
                    step_result.status = FabricStepStatus::Failed;
                    step_result.finished_unix_ms = Some(finished_unix_ms);
                    step_result.error = Some(error.clone());
                    session.log_step_event("step_failed", step, FabricStepStatus::Failed, error)?;
                }
            }
        }

        let ordered_step_results = execution_order
            .into_iter()
            .map(|id| step_results.remove(&id).unwrap())
            .collect();

        session.finish(ordered_step_results)
    }
}

pub trait FabricRuntimeAdapter {
    fn supports(&self, kind: &FabricRuntimeKind) -> bool;
    fn execute(&self, workspace_root: &Path, step: &FabricStep) -> Result<String, String>;
}

pub struct KainAdapter;

impl FabricRuntimeAdapter for KainAdapter {
    fn supports(&self, kind: &FabricRuntimeKind) -> bool {
        matches!(kind, FabricRuntimeKind::Kain)
    }

    fn execute(&self, workspace_root: &Path, step: &FabricStep) -> Result<String, String> {
        let entry = step.entry.as_ref().ok_or_else(|| {
            format!(
                "Fabric step '{}' is missing an entry path for runtime 'kain'",
                step.id
            )
        })?;
        let resolved_entry = resolve_fabric_path(workspace_root, entry);
        let source = fs::read_to_string(&resolved_entry).map_err(|err| {
            format!(
                "Failed to read Kain Fabric entry '{}' for step '{}': {err}",
                resolved_entry.display(),
                step.id
            )
        })?;
        kain_driver::compile(&source, CompileTarget::Interpret).map_err(|err| {
            format!(
                "Kain Fabric step '{}' failed during execution: {err}",
                step.id
            )
        })
    }
}

pub struct PythonAdapter;

impl FabricRuntimeAdapter for PythonAdapter {
    fn supports(&self, kind: &FabricRuntimeKind) -> bool {
        matches!(kind, FabricRuntimeKind::Python)
    }

    fn execute(&self, workspace_root: &Path, step: &FabricStep) -> Result<String, String> {
        let entry = step.entry.as_ref().ok_or_else(|| {
            format!(
                "Fabric step '{}' is missing an entry path for runtime 'python'",
                step.id
            )
        })?;
        let resolved_entry = resolve_fabric_path(workspace_root, entry);
        let source = fs::read_to_string(&resolved_entry).map_err(|err| {
            format!(
                "Failed to read Python Fabric entry '{}' for step '{}': {err}",
                resolved_entry.display(),
                step.id
            )
        })?;

        kain_python::register();
        let env = Env::new();
        let state = kain_python::python_scope_state(&env).map_err(|err| err.to_string())?;

        Python::with_gil(|py| {
            let scope = state.scope.read().unwrap();
            let scope_dict =
                kain_python::scope_dict_from_guard(py, &scope).map_err(|err| err.to_string())?;

            py.run(&source, Some(scope_dict), Some(scope_dict))
                .map_err(|err| format!("Python Error: {err}"))?;

            if let Ok(result_var) = scope_dict.get_item("result") {
                if let Some(res) = result_var {
                    let val = kain_python::py_to_value(res).map_err(|err| err.to_string())?;
                    return Ok(format!("{val:?}"));
                }
            }

            if let Ok(run_fn) = scope_dict.get_item("run") {
                if let Some(f) = run_fn {
                    if f.is_callable() {
                        let result = f
                            .call0()
                            .map_err(|err| format!("Python Error in run(): {err}"))?;
                        let val =
                            kain_python::py_to_value(result).map_err(|err| err.to_string())?;
                        return Ok(format!("{val:?}"));
                    }
                }
            }

            Ok("Unit".to_string())
        })
    }
}

struct FabricEventWriter {
    file: Option<fs::File>,
}

impl FabricEventWriter {
    fn new(path: Option<&Path>) -> OmniResult<Self> {
        let file = match path {
            Some(path) => Some(fs::File::create(path)?),
            None => None,
        };
        Ok(Self { file })
    }

    fn write(&mut self, event: &FabricEventRecord) -> OmniResult<()> {
        if let Some(file) = &mut self.file {
            let encoded = serde_json::to_string(event).map_err(|err| {
                OmniError::Config(format!("Failed to serialize Fabric event: {err}"))
            })?;
            writeln!(file, "{encoded}")?;
        }
        Ok(())
    }
}

pub fn execute_fabric_manifest_path(path: &Path) -> OmniResult<FabricExecutionResult> {
    let manifest = kain_omni::fabric::load_fabric_manifest(path)?;
    let session = FabricSession::new(path, manifest)?;
    let executor = FabricExecutor::new();
    executor.execute(session)
}
