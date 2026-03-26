use std::collections::{BTreeMap, HashMap};
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::{Arc, RwLock};

use crate::bridge::struct_value;
use kain_c_ffi::{
    import_library, ArtifactMode as CArtifactMode, ImportCOptions,
    PrepareContext as CPrepareContext,
};
use kain_core::runtime::{self, Env, Value};
use kain_core::CompileTarget;
use kain_crate_ffi::{
    import_crate, ArtifactMode as RustArtifactMode, ImportCrateOptions,
    PrepareContext as RustPrepareContext,
};
use kain_interop::{
    extract_shared_buffer, extract_shared_image, shared_buffer_value, shared_image_value,
};
use kain_omni::fabric::{
    resolve_fabric_path, topological_step_order, unix_timestamp_ms, write_fabric_json,
    FabricContractKind, FabricEventRecord, FabricExecutionResult, FabricFailureReason,
    FabricManifest, FabricOutputPayloadSnapshot, FabricProducedOutput, FabricResolvedInputRecord,
    FabricRuntimeKind, FabricSessionStatus, FabricSharedBufferSnapshot, FabricSharedImageSnapshot,
    FabricStep, FabricStepExecution, FabricStepStatus, FabricValidationResult, FabricValueSnapshot,
};
use kain_omni::{OmniError, OmniResult};
use serde_json::{json, Value as JsonValue};

pub struct FabricSession {
    pub manifest_path: PathBuf,
    pub manifest_root: PathBuf,
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
            details: Some(json!({
                "manifest_path": manifest_path.display().to_string(),
                "workspace_root": workspace_root.display().to_string(),
                "session_directory": session_directory.display().to_string(),
            })),
        })?;

        Ok(Self {
            manifest_path: manifest_path.to_path_buf(),
            manifest_root,
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
        details: Option<JsonValue>,
    ) -> OmniResult<()> {
        let timestamp_unix_ms = unix_timestamp_ms();
        self.event_writer.write(&FabricEventRecord {
            timestamp_unix_ms,
            kind: kind.to_string(),
            step_id: step.map(|s| s.id.clone()),
            runtime: step.map(|s| s.runtime.clone()),
            status: None,
            message: message.into(),
            details,
        })
    }

    pub fn log_step_event(
        &mut self,
        kind: &str,
        step: &FabricStep,
        status: FabricStepStatus,
        message: impl Into<String>,
        details: Option<JsonValue>,
    ) -> OmniResult<()> {
        let timestamp_unix_ms = unix_timestamp_ms();
        self.event_writer.write(&FabricEventRecord {
            timestamp_unix_ms,
            kind: kind.to_string(),
            step_id: Some(step.id.clone()),
            runtime: Some(step.runtime.clone()),
            status: Some(status),
            message: message.into(),
            details,
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
            Some(json!({
                "status": result.status,
                "step_count": result.step_results.len(),
            })),
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
            adapters: vec![
                Box::new(KainAdapter),
                Box::new(PythonAdapter),
                Box::new(RustCrateAdapter),
                Box::new(CAbiAdapter),
                Box::new(NodeAdapter),
            ],
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
                let resolved_entry = step
                    .entry
                    .as_ref()
                    .map(|entry| resolve_fabric_path(&session.workspace_root, entry));
                let resolved_manifest_path =
                    resolve_runtime_manifest_path(&session.manifest_root, step);
                let resolved_library = step
                    .library
                    .as_ref()
                    .map(|library| resolve_fabric_path(&session.workspace_root, library));
                (
                    step.id.clone(),
                    FabricStepExecution {
                        id: step.id.clone(),
                        runtime: step.runtime.clone(),
                        entry: step.entry.clone(),
                        module: step.module.clone(),
                        crate_name: step.crate_name.clone(),
                        manifest_path: step.manifest_path.clone(),
                        library: step.library.clone(),
                        adapter: None,
                        resolved_entry,
                        resolved_manifest_path,
                        resolved_library,
                        depends_on: step.depends_on.clone(),
                        status: FabricStepStatus::Pending,
                        started_unix_ms: None,
                        finished_unix_ms: None,
                        resolved_inputs: Vec::new(),
                        outputs: Vec::new(),
                        error: None,
                    },
                )
            })
            .collect::<BTreeMap<_, _>>();

        let mut produced_outputs = BTreeMap::<String, BTreeMap<String, FabricStoredOutput>>::new();

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
                let failure = fabric_failure(
                    "blocked_dependencies",
                    format!(
                        "Blocked by incomplete dependencies: {}",
                        blocked_by.join(", ")
                    ),
                );
                step_result.status = FabricStepStatus::Blocked;
                step_result.finished_unix_ms = Some(finished_unix_ms);
                step_result.error = Some(failure.clone());
                session.log_step_event(
                    "step_blocked",
                    step,
                    FabricStepStatus::Blocked,
                    failure.message,
                    failure.details,
                )?;
                continue;
            }

            let (fabric_inputs, resolved_inputs) =
                match resolve_dependency_inputs(step, &steps_by_id, &produced_outputs) {
                    Ok(value) => value,
                    Err(failure) => {
                        let finished_unix_ms = unix_timestamp_ms();
                        step_result.status = FabricStepStatus::Failed;
                        step_result.finished_unix_ms = Some(finished_unix_ms);
                        step_result.error = Some(failure.clone());
                        session.log_step_event(
                            "step_failed",
                            step,
                            FabricStepStatus::Failed,
                            failure.message,
                            failure.details,
                        )?;
                        continue;
                    }
                };

            step_result.resolved_inputs = resolved_inputs.clone();
            let step_started_unix_ms = unix_timestamp_ms();
            step_result.started_unix_ms = Some(step_started_unix_ms);
            session.log_step_event(
                "step_started",
                step,
                FabricStepStatus::Pending,
                format!("Executing Fabric step '{}'", step.id),
                Some(json!({
                    "depends_on": step.depends_on,
                    "resolved_inputs": resolved_inputs,
                })),
            )?;

            let adapter = self.adapters.iter().find(|a| a.supports(&step.runtime));
            match adapter {
                Some(adapter) => {
                    step_result.adapter = Some(adapter.label().to_string());
                    let context = FabricAdapterContext {
                        manifest_path: &session.manifest_path,
                        manifest_root: &session.manifest_root,
                        workspace_root: &session.workspace_root,
                        session_directory: &session.session_directory,
                        step,
                        fabric_inputs,
                    };
                    match adapter.execute(&context) {
                        Ok(raw_value) => match map_declared_outputs(step, raw_value) {
                            Ok(mapped_outputs) => {
                                let finished_unix_ms = unix_timestamp_ms();
                                let public_outputs = mapped_outputs
                                    .iter()
                                    .map(|output| output.public())
                                    .collect();
                                step_result.status = FabricStepStatus::Succeeded;
                                step_result.finished_unix_ms = Some(finished_unix_ms);
                                step_result.outputs = public_outputs;
                                produced_outputs.insert(
                                    step.id.clone(),
                                    mapped_outputs
                                        .into_iter()
                                        .map(|output| (output.name.clone(), output))
                                        .collect(),
                                );
                                session.log_step_event(
                                    "step_succeeded",
                                    step,
                                    FabricStepStatus::Succeeded,
                                    format!("Step '{}' completed", step.id),
                                    Some(json!({
                                        "adapter": adapter.label(),
                                        "outputs": step_result.outputs,
                                    })),
                                )?;
                            }
                            Err(failure) => {
                                let finished_unix_ms = unix_timestamp_ms();
                                step_result.status = FabricStepStatus::Failed;
                                step_result.finished_unix_ms = Some(finished_unix_ms);
                                step_result.error = Some(failure.clone());
                                session.log_step_event(
                                    "step_failed",
                                    step,
                                    FabricStepStatus::Failed,
                                    failure.message,
                                    failure.details,
                                )?;
                            }
                        },
                        Err(failure) => {
                            let finished_unix_ms = unix_timestamp_ms();
                            step_result.status = FabricStepStatus::Failed;
                            step_result.finished_unix_ms = Some(finished_unix_ms);
                            step_result.error = Some(failure.clone());
                            session.log_step_event(
                                "step_failed",
                                step,
                                FabricStepStatus::Failed,
                                failure.message,
                                failure.details,
                            )?;
                        }
                    }
                }
                None => {
                    let finished_unix_ms = unix_timestamp_ms();
                    let failure = fabric_failure(
                        "unsupported_runtime",
                        format!(
                            "No adapter found for runtime '{}'",
                            step.runtime.display_name()
                        ),
                    );
                    step_result.status = FabricStepStatus::Failed;
                    step_result.finished_unix_ms = Some(finished_unix_ms);
                    step_result.error = Some(failure.clone());
                    session.log_step_event(
                        "step_failed",
                        step,
                        FabricStepStatus::Failed,
                        failure.message,
                        failure.details,
                    )?;
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
    fn label(&self) -> &'static str;
    fn supports(&self, kind: &FabricRuntimeKind) -> bool;
    fn execute(&self, context: &FabricAdapterContext) -> Result<Value, FabricFailureReason>;
}

struct FabricAdapterContext<'a> {
    manifest_path: &'a Path,
    manifest_root: &'a Path,
    workspace_root: &'a Path,
    session_directory: &'a Path,
    step: &'a FabricStep,
    fabric_inputs: Value,
}

#[derive(Clone)]
struct FabricStoredOutput {
    name: String,
    declared_kind: FabricContractKind,
    runtime_value: Value,
    payload: FabricOutputPayloadSnapshot,
}

impl FabricStoredOutput {
    fn public(&self) -> FabricProducedOutput {
        FabricProducedOutput {
            name: self.name.clone(),
            declared_kind: self.declared_kind,
            payload: self.payload.clone(),
        }
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

pub struct KainAdapter;

impl FabricRuntimeAdapter for KainAdapter {
    fn label(&self) -> &'static str {
        "kain-host"
    }

    fn supports(&self, kind: &FabricRuntimeKind) -> bool {
        matches!(kind, FabricRuntimeKind::Kain)
    }

    fn execute(&self, context: &FabricAdapterContext) -> Result<Value, FabricFailureReason> {
        let entry = context.step.entry.as_ref().ok_or_else(|| {
            fabric_failure(
                "missing_entry",
                format!(
                    "Fabric step '{}' is missing an entry path for runtime 'kain'",
                    context.step.id
                ),
            )
        })?;
        let resolved_entry = resolve_fabric_path(context.workspace_root, entry);
        let source = read_step_source(&resolved_entry, context.step, "Kain")?;
        interpret_fabric_kain_source(context, &source)
    }
}

pub struct PythonAdapter;

impl FabricRuntimeAdapter for PythonAdapter {
    fn label(&self) -> &'static str {
        "python-bridge"
    }

    fn supports(&self, kind: &FabricRuntimeKind) -> bool {
        matches!(kind, FabricRuntimeKind::Python)
    }

    fn execute(&self, context: &FabricAdapterContext) -> Result<Value, FabricFailureReason> {
        let source = render_python_harness(context)?;
        interpret_fabric_kain_source(context, &source)
    }
}

pub struct RustCrateAdapter;

impl FabricRuntimeAdapter for RustCrateAdapter {
    fn label(&self) -> &'static str {
        "rust-crate-ffi"
    }

    fn supports(&self, kind: &FabricRuntimeKind) -> bool {
        matches!(kind, FabricRuntimeKind::RustCrate)
    }

    fn execute(&self, context: &FabricAdapterContext) -> Result<Value, FabricFailureReason> {
        let crate_name = context.step.crate_name.as_ref().ok_or_else(|| {
            fabric_failure(
                "missing_crate_name",
                format!(
                    "Fabric step '{}' is missing crate_name for runtime 'rust_crate'",
                    context.step.id
                ),
            )
        })?;
        let entry_path = context.step.entry.as_ref().ok_or_else(|| {
            fabric_failure(
                "missing_entry",
                format!(
                    "Fabric step '{}' must declare entry for runtime 'rust_crate'",
                    context.step.id
                ),
            )
        })?;
        let resolved_entry = resolve_fabric_path(context.workspace_root, entry_path);
        let entry_source = read_step_source(&resolved_entry, context.step, "Rust crate glue")?;
        let resolved_manifest_path = resolve_runtime_manifest_path(context.manifest_root, context.step)
            .ok_or_else(|| {
                fabric_failure(
                    "missing_manifest_path",
                    format!(
                        "Fabric step '{}' with runtime 'rust_crate' requires manifest_path or a Cargo.toml beside the Fabric manifest",
                        context.step.id
                    ),
                )
            })?;
        let imported = import_crate(
            crate_name,
            &ImportCrateOptions {
                manifest_path: Some(resolved_manifest_path.clone()),
                mode: RustArtifactMode::Both,
                ..ImportCrateOptions::default()
            },
            &RustPrepareContext {
                current_dir: Some(context.workspace_root.to_path_buf()),
                manifest_path: Some(resolved_manifest_path),
            },
        )
        .map_err(|err| runtime_bridge_failure(context.step, "rust_crate_import_failed", err))?;
        let source = format!("{}\n{}", imported.canonical_module_source, entry_source);
        interpret_fabric_kain_source(context, &source)
    }
}

pub struct CAbiAdapter;

impl FabricRuntimeAdapter for CAbiAdapter {
    fn label(&self) -> &'static str {
        "c-abi-ffi"
    }

    fn supports(&self, kind: &FabricRuntimeKind) -> bool {
        matches!(kind, FabricRuntimeKind::CAbi)
    }

    fn execute(&self, context: &FabricAdapterContext) -> Result<Value, FabricFailureReason> {
        let entry_path = context.step.entry.as_ref().ok_or_else(|| {
            fabric_failure(
                "missing_entry",
                format!(
                    "Fabric step '{}' must declare entry for runtime 'c_abi'",
                    context.step.id
                ),
            )
        })?;
        let resolved_entry = resolve_fabric_path(context.workspace_root, entry_path);
        let entry_source = read_step_source(&resolved_entry, context.step, "C ABI glue")?;
        let resolved_manifest_path = resolve_runtime_manifest_path(context.manifest_root, context.step)
            .ok_or_else(|| {
                fabric_failure(
                    "missing_manifest_path",
                    format!(
                        "Fabric step '{}' with runtime 'c_abi' requires manifest_path or a KAIN.toml beside the Fabric manifest",
                        context.step.id
                    ),
                )
            })?;
        let import_name = context
            .step
            .module
            .clone()
            .or_else(|| {
                context
                    .step
                    .library
                    .as_ref()
                    .and_then(|library| library.file_stem())
                    .and_then(|value| value.to_str())
                    .map(str::to_string)
            })
            .ok_or_else(|| {
                fabric_failure(
                    "missing_import_name",
                    format!(
                        "Fabric step '{}' with runtime 'c_abi' needs module or a library file stem so kain-host can resolve the C import name",
                        context.step.id
                    ),
                )
            })?;
        let imported = import_library(
            &import_name,
            &ImportCOptions {
                mode: CArtifactMode::Both,
                ..ImportCOptions::default()
            },
            &CPrepareContext {
                current_dir: Some(context.workspace_root.to_path_buf()),
                manifest_path: Some(resolved_manifest_path),
            },
        )
        .map_err(|err| runtime_bridge_failure(context.step, "c_abi_import_failed", err))?;
        validate_c_library_alignment(context.step, &imported.resolved.shared_lib_path)?;
        let source = format!("{}\n{}", imported.canonical_module_source, entry_source);
        interpret_fabric_kain_source(context, &source)
    }
}

pub struct NodeAdapter;

impl FabricRuntimeAdapter for NodeAdapter {
    fn label(&self) -> &'static str {
        "node-bridge"
    }

    fn supports(&self, kind: &FabricRuntimeKind) -> bool {
        matches!(kind, FabricRuntimeKind::Node)
    }

    fn execute(&self, context: &FabricAdapterContext) -> Result<Value, FabricFailureReason> {
        let source = render_node_harness(context)?;
        interpret_fabric_kain_source(context, &source)
    }
}

fn resolve_dependency_inputs(
    step: &FabricStep,
    steps_by_id: &BTreeMap<String, FabricStep>,
    produced_outputs: &BTreeMap<String, BTreeMap<String, FabricStoredOutput>>,
) -> Result<(Value, Vec<FabricResolvedInputRecord>), FabricFailureReason> {
    let mut dependency_fields = HashMap::new();
    let mut resolved_inputs = Vec::new();

    for dependency_id in &step.depends_on {
        let dependency_step = steps_by_id.get(dependency_id).ok_or_else(|| {
            fabric_failure(
                "missing_dependency_step",
                format!(
                    "Fabric step '{}' depends on unknown step '{}'",
                    step.id, dependency_id
                ),
            )
        })?;
        let produced = produced_outputs.get(dependency_id);
        let mut output_fields = HashMap::new();
        for output_binding in &dependency_step.outputs {
            let output = produced
                .and_then(|outputs| outputs.get(&output_binding.name))
                .ok_or_else(|| {
                    fabric_failure(
                        "missing_dependency_output",
                        format!(
                            "Fabric step '{}' expected dependency '{}' to produce output '{}'",
                            step.id, dependency_id, output_binding.name
                        ),
                    )
                })?;
            output_fields.insert(output_binding.name.clone(), output.runtime_value.clone());
            resolved_inputs.push(FabricResolvedInputRecord {
                from_step_id: dependency_id.clone(),
                output_name: output_binding.name.clone(),
                declared_kind: output_binding.kind,
                payload: output.payload.clone(),
            });
        }
        dependency_fields.insert(
            dependency_id.clone(),
            Value::Struct(
                "FabricStepOutputs".to_string(),
                Arc::new(RwLock::new(output_fields)),
            ),
        );
    }

    Ok((
        Value::Struct(
            "FabricInputs".to_string(),
            Arc::new(RwLock::new(dependency_fields)),
        ),
        resolved_inputs,
    ))
}

fn map_declared_outputs(
    step: &FabricStep,
    raw_value: Value,
) -> Result<Vec<FabricStoredOutput>, FabricFailureReason> {
    if step.outputs.is_empty() {
        return Ok(Vec::new());
    }

    if step.outputs.len() == 1 {
        let binding = &step.outputs[0];
        let runtime_value =
            normalize_contract_value(step, binding, normalize_runtime_value(raw_value))?;
        let payload = snapshot_contract_value(binding.kind, &runtime_value)?;
        return Ok(vec![FabricStoredOutput {
            name: binding.name.clone(),
            declared_kind: binding.kind,
            runtime_value,
            payload,
        }]);
    }

    let fields = match normalize_runtime_value(raw_value) {
        Value::Struct(_, fields) => fields
            .read()
            .map_err(|_| {
                fabric_failure(
                    "output_struct_read_failed",
                    format!(
                        "Fabric step '{}' returned a struct whose fields could not be read",
                        step.id
                    ),
                )
            })?
            .clone(),
        other => {
            return Err(fabric_failure(
                "multiple_outputs_require_struct",
                format!(
                    "Fabric step '{}' declares multiple outputs and must return a struct/object, got {}",
                    step.id, other
                ),
            ))
        }
    };

    let mut outputs = Vec::with_capacity(step.outputs.len());
    for binding in &step.outputs {
        let value = fields.get(&binding.name).cloned().ok_or_else(|| {
            fabric_failure(
                "missing_output_field",
                format!(
                    "Fabric step '{}' did not return required output field '{}'",
                    step.id, binding.name
                ),
            )
        })?;
        let runtime_value =
            normalize_contract_value(step, binding, normalize_runtime_value(value))?;
        let payload = snapshot_contract_value(binding.kind, &runtime_value)?;
        outputs.push(FabricStoredOutput {
            name: binding.name.clone(),
            declared_kind: binding.kind,
            runtime_value,
            payload,
        });
    }
    Ok(outputs)
}

fn normalize_contract_value(
    step: &FabricStep,
    binding: &kain_omni::fabric::FabricOutputBinding,
    value: Value,
) -> Result<Value, FabricFailureReason> {
    match binding.kind {
        FabricContractKind::Value => {
            if extract_shared_buffer(&value).is_ok() || extract_shared_image(&value).is_ok() {
                return Err(fabric_failure(
                    "output_kind_mismatch",
                    format!(
                        "Fabric step '{}' output '{}' declared contract 'value' but returned a shared contract handle",
                        step.id, binding.name
                    ),
                ));
            }
            Ok(value)
        }
        FabricContractKind::SharedBuffer => {
            let buffer = extract_shared_buffer(&value).map_err(|_| {
                fabric_failure(
                    "output_kind_mismatch",
                    format!(
                        "Fabric step '{}' output '{}' declared contract 'shared_buffer' but did not return a shared buffer handle",
                        step.id, binding.name
                    ),
                )
            })?;
            Ok(shared_buffer_value(buffer))
        }
        FabricContractKind::SharedImage => {
            let image = extract_shared_image(&value).map_err(|_| {
                fabric_failure(
                    "output_kind_mismatch",
                    format!(
                        "Fabric step '{}' output '{}' declared contract 'shared_image' but did not return a shared image handle",
                        step.id, binding.name
                    ),
                )
            })?;
            Ok(shared_image_value(image))
        }
    }
}

fn snapshot_contract_value(
    kind: FabricContractKind,
    value: &Value,
) -> Result<FabricOutputPayloadSnapshot, FabricFailureReason> {
    match kind {
        FabricContractKind::Value => Ok(FabricOutputPayloadSnapshot::Value {
            value: FabricValueSnapshot {
                summary: value.to_string(),
                json: value_to_json_snapshot(value),
            },
        }),
        FabricContractKind::SharedBuffer => {
            let buffer = extract_shared_buffer(value).map_err(|_| {
                fabric_failure(
                    "shared_buffer_snapshot_failed",
                    "Fabric could not snapshot a shared buffer output",
                )
            })?;
            Ok(FabricOutputPayloadSnapshot::SharedBuffer {
                buffer: FabricSharedBufferSnapshot {
                    contract: "kain.shared.buffer".to_string(),
                    byte_length: buffer.byte_length(),
                    element_type: buffer.metadata.element_type.clone(),
                    element_size: buffer.metadata.element_size,
                    shape: buffer.metadata.shape.clone(),
                    strides: buffer.metadata.strides.clone(),
                    format: buffer.metadata.format.clone(),
                    mime_type: buffer.metadata.mime_type.clone(),
                    source_runtime: buffer.metadata.source_runtime.clone(),
                    source_backend: buffer.metadata.source_backend.clone(),
                    ownership: buffer.metadata.ownership.clone(),
                    labels: buffer.metadata.labels.clone(),
                },
            })
        }
        FabricContractKind::SharedImage => {
            let image = extract_shared_image(value).map_err(|_| {
                fabric_failure(
                    "shared_image_snapshot_failed",
                    "Fabric could not snapshot a shared image output",
                )
            })?;
            Ok(FabricOutputPayloadSnapshot::SharedImage {
                image: FabricSharedImageSnapshot {
                    contract: "kain.shared.image".to_string(),
                    byte_length: image.buffer.byte_length(),
                    representation: image.metadata.representation.clone(),
                    width: image.metadata.width,
                    height: image.metadata.height,
                    channels: image.metadata.channels,
                    layout: image.metadata.layout.clone(),
                    pixel_format: image.metadata.pixel_format.clone(),
                    mime_type: image.metadata.mime_type.clone(),
                    row_stride: image.metadata.row_stride,
                    color_space: image.metadata.color_space.clone(),
                    alpha_mode: image.metadata.alpha_mode.clone(),
                    source_runtime: image.metadata.source_runtime.clone(),
                    source_backend: image.metadata.source_backend.clone(),
                    ownership: image.metadata.ownership.clone(),
                    labels: image.metadata.labels.clone(),
                },
            })
        }
    }
}

fn interpret_fabric_kain_source(
    context: &FabricAdapterContext,
    source: &str,
) -> Result<Value, FabricFailureReason> {
    register_fabric_extensions();
    let typed = kain_driver::DriverSession::default()
        .frontend_to_typed_program(source, CompileTarget::Interpret)
        .map_err(|err| runtime_bridge_failure(context.step, "compile_failed", err))?;
    let mut env = Env::new();
    env.define_global("fabric_inputs", context.fabric_inputs.clone());
    env.define_global(
        "fabric_serialized_inputs",
        script_safe_value(&context.fabric_inputs),
    );
    env.define_global(
        "fabric_context",
        struct_value(
            "FabricContext",
            [
                (
                    "workspace_root",
                    Value::String(context.workspace_root.display().to_string()),
                ),
                (
                    "session_directory",
                    Value::String(context.session_directory.display().to_string()),
                ),
                (
                    "manifest_path",
                    Value::String(context.manifest_path.display().to_string()),
                ),
                ("step_id", Value::String(context.step.id.clone())),
                (
                    "runtime",
                    Value::String(context.step.runtime.display_name().to_string()),
                ),
            ],
        ),
    );
    runtime::interpret_with_env(&mut env, &typed)
        .map(normalize_runtime_value)
        .map_err(|err| runtime_bridge_failure(context.step, "execution_failed", err))
}

fn register_fabric_extensions() {
    kain_interop::register();
    kain_python::register();
    kain_node::register();
    kain_crate_ffi::register();
    kain_c_ffi::register();
}

fn render_python_harness(context: &FabricAdapterContext) -> Result<String, FabricFailureReason> {
    let runtime_call = python_call_expression(context)?;
    Ok(format!(
        "use std::python::bridge\n\nfn main() -> Any:\n    {runtime_call}\n"
    ))
}

fn python_call_expression(context: &FabricAdapterContext) -> Result<String, FabricFailureReason> {
    if context.step.outputs.len() > 1
        && context
            .step
            .outputs
            .iter()
            .any(|output| output.kind != FabricContractKind::Value)
    {
        return Err(fabric_failure(
            "unsupported_python_output_shape",
            format!(
                "Fabric step '{}' uses runtime 'python' with multiple shared outputs. Return a single shared contract output or a plain value object.",
                context.step.id
            ),
        ));
    }

    let single_output_kind = context
        .step
        .outputs
        .first()
        .map(|output| output.kind)
        .unwrap_or(FabricContractKind::Value);

    if let Some(entry) = &context.step.entry {
        let resolved_entry = resolve_fabric_path(context.workspace_root, entry);
        let source = read_step_source(&resolved_entry, context.step, "Python")?;
        let callable_name = context
            .step
            .module
            .clone()
            .unwrap_or_else(|| "run".to_string());
        let exec_source = kain_string_literal(&source);
        let callable = kain_string_literal(&callable_name);
        let result_expr = match single_output_kind {
            FabricContractKind::Value => {
                format!("py_bridge_call({callable}, [fabric_serialized_inputs])")
            }
            FabricContractKind::SharedBuffer => {
                format!("py_bridge_shared_buffer(py_bridge_call_raw({callable}, [fabric_serialized_inputs]))")
            }
            FabricContractKind::SharedImage => {
                format!("py_bridge_shared_image(py_bridge_call_raw({callable}, [fabric_serialized_inputs]))")
            }
        };
        Ok(format!(
            "py_bridge_exec({exec_source})\n    return {result_expr}"
        ))
    } else {
        let module_name = context.step.module.clone().ok_or_else(|| {
            fabric_failure(
                "missing_python_target",
                format!(
                    "Fabric step '{}' with runtime 'python' needs entry or module",
                    context.step.id
                ),
            )
        })?;
        let module_literal = kain_string_literal(&module_name);
        let result_expr = match single_output_kind {
            FabricContractKind::Value => {
                format!(
                    "py_bridge_call_attr(py_bridge_require_module({module_literal}), \"run\", [fabric_serialized_inputs])"
                )
            }
            FabricContractKind::SharedBuffer => format!(
                "py_bridge_shared_buffer(py_bridge_call_attr_raw(py_bridge_require_module({module_literal}), \"run\", [fabric_serialized_inputs]))"
            ),
            FabricContractKind::SharedImage => format!(
                "py_bridge_shared_image(py_bridge_call_attr_raw(py_bridge_require_module({module_literal}), \"run\", [fabric_serialized_inputs]))"
            ),
        };
        Ok(format!("return {result_expr}"))
    }
}

fn render_node_harness(context: &FabricAdapterContext) -> Result<String, FabricFailureReason> {
    if context.step.outputs.len() > 1
        && context
            .step
            .outputs
            .iter()
            .any(|output| output.kind != FabricContractKind::Value)
    {
        return Err(fabric_failure(
            "unsupported_node_output_shape",
            format!(
                "Fabric step '{}' uses runtime 'node' with multiple shared outputs. Return a single shared contract output or a plain value object.",
                context.step.id
            ),
        ));
    }

    let import_specifier = if let Some(entry) = &context.step.entry {
        let resolved_entry = resolve_fabric_path(context.workspace_root, entry);
        path_to_node_specifier(&resolved_entry)
    } else {
        context.step.module.clone().ok_or_else(|| {
            fabric_failure(
                "missing_node_target",
                format!(
                    "Fabric step '{}' with runtime 'node' needs entry or module",
                    context.step.id
                ),
            )
        })?
    };

    let export_name = if context.step.entry.is_some() {
        context
            .step
            .module
            .clone()
            .unwrap_or_else(|| "run".to_string())
    } else {
        "run".to_string()
    };

    let single_output_kind = context
        .step
        .outputs
        .first()
        .map(|output| output.kind)
        .unwrap_or(FabricContractKind::Value);
    let import_literal = kain_string_literal(&import_specifier);
    let export_literal = kain_string_literal(&export_name);
    let result_expr = match single_output_kind {
        FabricContractKind::Value => {
            format!("js_bridge_call_method(module_ref, {export_literal}, [fabric_serialized_inputs])")
        }
        FabricContractKind::SharedBuffer => format!(
            "js_web_shared_buffer(js_bridge_call_method_raw(module_ref, {export_literal}, [fabric_serialized_inputs]))"
        ),
        FabricContractKind::SharedImage => format!(
            "js_web_shared_image(js_bridge_call_method_raw(module_ref, {export_literal}, [fabric_serialized_inputs]))"
        ),
    };

    Ok(format!(
        "use std::javascript::bridge\nuse std::javascript::web\n\nfn main() -> Any:\n    let module_ref = js_bridge_import({import_literal})\n    return {result_expr}\n"
    ))
}

fn read_step_source(
    resolved_entry: &Path,
    step: &FabricStep,
    runtime_label: &str,
) -> Result<String, FabricFailureReason> {
    fs::read_to_string(resolved_entry).map_err(|err| {
        fabric_failure(
            "read_entry_failed",
            format!(
                "Failed to read {runtime_label} Fabric entry '{}' for step '{}': {err}",
                resolved_entry.display(),
                step.id
            ),
        )
    })
}

fn resolve_runtime_manifest_path(manifest_root: &Path, step: &FabricStep) -> Option<PathBuf> {
    if let Some(path) = &step.manifest_path {
        return Some(resolve_fabric_path(manifest_root, path));
    }

    let cargo_manifest = manifest_root.join("Cargo.toml");
    if matches!(step.runtime, FabricRuntimeKind::RustCrate) && cargo_manifest.exists() {
        return Some(cargo_manifest);
    }

    let kain_manifest = manifest_root.join("KAIN.toml");
    if matches!(step.runtime, FabricRuntimeKind::CAbi) && kain_manifest.exists() {
        return Some(kain_manifest);
    }

    None
}

fn validate_c_library_alignment(
    step: &FabricStep,
    imported_shared_library: &Option<PathBuf>,
) -> Result<(), FabricFailureReason> {
    let Some(declared_library) = &step.library else {
        return Ok(());
    };
    let declared_file_name = declared_library
        .file_name()
        .and_then(|value| value.to_str());
    let imported_file_name = imported_shared_library
        .as_ref()
        .and_then(|value| value.file_name())
        .and_then(|value| value.to_str());
    if declared_file_name.is_some()
        && imported_file_name.is_some()
        && declared_file_name != imported_file_name
    {
        return Err(fabric_failure(
            "c_library_mismatch",
            format!(
                "Fabric step '{}' declared library '{}' but the resolved C import pointed at '{}'",
                step.id,
                declared_library.display(),
                imported_shared_library
                    .as_ref()
                    .map(|value| value.display().to_string())
                    .unwrap_or_else(|| "<none>".to_string())
            ),
        ));
    }
    Ok(())
}

fn normalize_runtime_value(value: Value) -> Value {
    match value {
        Value::Return(inner) => normalize_runtime_value(*inner),
        other => other,
    }
}

fn value_to_json_snapshot(value: &Value) -> Option<JsonValue> {
    match value {
        Value::Unit | Value::None => Some(JsonValue::Null),
        Value::Bool(value) => Some(JsonValue::Bool(*value)),
        Value::Int(value) => Some(json!(value)),
        Value::Float(value) => Some(json!(value)),
        Value::String(value) => Some(JsonValue::String(value.clone())),
        Value::Array(items) => {
            let items = items.read().ok()?;
            let mut converted = Vec::with_capacity(items.len());
            for item in items.iter() {
                converted.push(value_to_json_snapshot(item)?);
            }
            Some(JsonValue::Array(converted))
        }
        Value::Tuple(items) => {
            let mut converted = Vec::with_capacity(items.len());
            for item in items {
                converted.push(value_to_json_snapshot(item)?);
            }
            Some(JsonValue::Array(converted))
        }
        Value::Struct(_, fields) => {
            let guard = fields.read().ok()?;
            let mut object = serde_json::Map::new();
            for (key, value) in guard.iter() {
                object.insert(key.clone(), value_to_json_snapshot(value)?);
            }
            Some(JsonValue::Object(object))
        }
        _ => None,
    }
}

fn script_safe_value(value: &Value) -> Value {
    if let Ok(buffer) = extract_shared_buffer(value) {
        return struct_value(
            "FabricSharedBufferInput",
            [
                ("contract", Value::String("kain.shared.buffer".to_string())),
                ("byte_length", Value::Int(buffer.byte_length() as i64)),
                (
                    "element_type",
                    Value::String(buffer.metadata.element_type.clone()),
                ),
                ("element_size", Value::Int(buffer.metadata.element_size)),
                ("shape", int_list_value(&buffer.metadata.shape)),
                ("strides", int_list_value(&buffer.metadata.strides)),
                (
                    "format",
                    optional_string_value(buffer.metadata.format.clone()),
                ),
                (
                    "mime_type",
                    optional_string_value(buffer.metadata.mime_type.clone()),
                ),
                (
                    "source_runtime",
                    Value::String(buffer.metadata.source_runtime.clone()),
                ),
                (
                    "source_backend",
                    optional_string_value(buffer.metadata.source_backend.clone()),
                ),
                (
                    "ownership",
                    Value::String(buffer.metadata.ownership.clone()),
                ),
                ("labels", string_list_value(&buffer.metadata.labels)),
                ("bytes", bytes_to_value(&buffer.bytes())),
            ],
        );
    }

    if let Ok(image) = extract_shared_image(value) {
        return struct_value(
            "FabricSharedImageInput",
            [
                ("contract", Value::String("kain.shared.image".to_string())),
                ("byte_length", Value::Int(image.buffer.byte_length() as i64)),
                (
                    "representation",
                    Value::String(image.metadata.representation.clone()),
                ),
                ("width", Value::Int(image.metadata.width)),
                ("height", Value::Int(image.metadata.height)),
                ("channels", Value::Int(image.metadata.channels)),
                ("layout", Value::String(image.metadata.layout.clone())),
                (
                    "pixel_format",
                    Value::String(image.metadata.pixel_format.clone()),
                ),
                ("mime_type", Value::String(image.metadata.mime_type.clone())),
                ("row_stride", Value::Int(image.metadata.row_stride)),
                (
                    "color_space",
                    Value::String(image.metadata.color_space.clone()),
                ),
                (
                    "alpha_mode",
                    Value::String(image.metadata.alpha_mode.clone()),
                ),
                (
                    "source_runtime",
                    Value::String(image.metadata.source_runtime.clone()),
                ),
                (
                    "source_backend",
                    optional_string_value(image.metadata.source_backend.clone()),
                ),
                ("ownership", Value::String(image.metadata.ownership.clone())),
                ("labels", string_list_value(&image.metadata.labels)),
                ("bytes", bytes_to_value(&image.bytes())),
            ],
        );
    }

    match value {
        Value::Array(items) => {
            let cloned = items
                .read()
                .map(|guard| guard.iter().map(script_safe_value).collect::<Vec<_>>())
                .unwrap_or_default();
            Value::Array(Arc::new(RwLock::new(cloned)))
        }
        Value::Tuple(items) => Value::Tuple(items.iter().map(script_safe_value).collect()),
        Value::Struct(name, fields) => {
            let cloned = fields
                .read()
                .map(|guard| {
                    guard
                        .iter()
                        .map(|(key, value)| (key.clone(), script_safe_value(value)))
                        .collect::<HashMap<_, _>>()
                })
                .unwrap_or_default();
            Value::Struct(name.clone(), Arc::new(RwLock::new(cloned)))
        }
        other => other.clone(),
    }
}

fn bytes_to_value(bytes: &[u8]) -> Value {
    Value::Array(Arc::new(RwLock::new(
        bytes
            .iter()
            .map(|value| Value::Int(*value as i64))
            .collect(),
    )))
}

fn int_list_value(values: &[i64]) -> Value {
    Value::Array(Arc::new(RwLock::new(
        values.iter().map(|value| Value::Int(*value)).collect(),
    )))
}

fn string_list_value(values: &[String]) -> Value {
    Value::Array(Arc::new(RwLock::new(
        values.iter().cloned().map(Value::String).collect(),
    )))
}

fn optional_string_value(value: Option<String>) -> Value {
    match value {
        Some(value) => Value::String(value),
        None => Value::None,
    }
}

fn path_to_node_specifier(path: &Path) -> String {
    let rendered = path.display().to_string().replace('\\', "/");
    if path.is_absolute() {
        if rendered.starts_with("//") {
            format!("file:{rendered}")
        } else {
            format!("file:///{rendered}")
        }
    } else {
        rendered
    }
}

fn kain_string_literal(value: &str) -> String {
    let mut rendered = String::with_capacity(value.len() + 8);
    rendered.push('"');
    for ch in value.chars() {
        match ch {
            '\\' => rendered.push_str("\\\\"),
            '"' => rendered.push_str("\\\""),
            '\n' => rendered.push_str("\\n"),
            '\r' => rendered.push_str("\\r"),
            '\t' => rendered.push_str("\\t"),
            _ => rendered.push(ch),
        }
    }
    rendered.push('"');
    rendered
}

fn fabric_failure(code: impl Into<String>, message: impl Into<String>) -> FabricFailureReason {
    FabricFailureReason {
        code: code.into(),
        message: message.into(),
        details: None,
    }
}

fn runtime_bridge_failure(
    step: &FabricStep,
    code: impl Into<String>,
    err: impl std::fmt::Display,
) -> FabricFailureReason {
    FabricFailureReason {
        code: code.into(),
        message: format!("Fabric step '{}' failed: {err}", step.id),
        details: Some(json!({
            "step_id": step.id,
            "runtime": step.runtime.display_name(),
        })),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    #[test]
    fn local_fabric_manifest_executes_and_records_typed_outputs() {
        let dir = tempfile::tempdir().unwrap();
        let init =
            kain_omni::init_fabric_manifest(dir.path(), kain_omni::FabricTemplateKind::Local)
                .unwrap();

        let result = execute_fabric_manifest_path(&init.manifest_path).unwrap();

        assert_eq!(result.status, FabricSessionStatus::Succeeded);
        assert!(result.report_path.exists());
        assert!(result.lock_path.exists());
        assert!(result
            .events_path
            .as_ref()
            .is_some_and(|path| path.exists()));
        assert_eq!(result.step_results.len(), 1);
        assert_eq!(result.step_results[0].outputs.len(), 1);
        assert_eq!(
            result.step_results[0].outputs[0].declared_kind,
            FabricContractKind::Value
        );
    }

    #[test]
    fn polyglot_fixture_executes_all_runtime_kinds() {
        let temp = TempDir::new().expect("temp dir");
        let fixture_root = temp.path().join("fabric_polyglot_smoke");
        copy_fixture(
            Path::new(env!("CARGO_MANIFEST_DIR"))
                .parent()
                .unwrap()
                .parent()
                .unwrap()
                .join("smoketest")
                .join("fabric")
                .join("polyglot_local")
                .as_path(),
            &fixture_root,
        );
        compile_fixture_shared_library(&fixture_root);

        let result = execute_fabric_manifest_path(&fixture_root.join("KAIN.fabric.toml")).unwrap();

        assert_eq!(result.status, FabricSessionStatus::Succeeded);
        assert_eq!(result.step_results.len(), 5);
        assert!(result
            .step_results
            .iter()
            .all(|step| step.status == FabricStepStatus::Succeeded));
        assert!(result
            .step_results
            .iter()
            .any(|step| step.runtime == FabricRuntimeKind::Python));
        assert!(result
            .step_results
            .iter()
            .any(|step| step.runtime == FabricRuntimeKind::Kain));
        assert!(result
            .step_results
            .iter()
            .any(|step| step.runtime == FabricRuntimeKind::RustCrate));
        assert!(result
            .step_results
            .iter()
            .any(|step| step.runtime == FabricRuntimeKind::CAbi));
        assert!(result
            .step_results
            .iter()
            .any(|step| step.runtime == FabricRuntimeKind::Node));
        let native_step = result
            .step_results
            .iter()
            .find(|step| step.id == "native_filter")
            .expect("native step");
        assert_eq!(native_step.outputs.len(), 2);
        assert!(native_step
            .outputs
            .iter()
            .any(|output| output.declared_kind == FabricContractKind::SharedImage));
        assert!(native_step
            .outputs
            .iter()
            .any(|output| output.declared_kind == FabricContractKind::SharedBuffer));
    }

    fn compile_fixture_shared_library(fixture_root: &Path) {
        let native_dir = fixture_root.join("native");
        let source = native_dir.join("image_fx.c");
        let output = if cfg!(target_os = "windows") {
            native_dir.join("image_fx.dll")
        } else if cfg!(target_os = "macos") {
            native_dir.join("libimage_fx.dylib")
        } else {
            native_dir.join("libimage_fx.so")
        };

        let status = std::process::Command::new("clang")
            .arg("-shared")
            .arg("-O2")
            .arg(&source)
            .arg("-o")
            .arg(&output)
            .status()
            .expect("clang should launch for fabric smoke");
        assert!(status.success(), "clang should build test shared library");
        let output_name = output.file_name().unwrap().to_string_lossy().to_string();
        rewrite_fixture_native_library_names(fixture_root, &output_name);
    }

    fn copy_fixture(source: &Path, destination: &Path) {
        fs::create_dir_all(destination).expect("create destination");
        for entry in fs::read_dir(source).expect("read fixture directory") {
            let entry = entry.expect("fixture entry");
            let entry_type = entry.file_type().expect("fixture file type");
            let destination_path = destination.join(entry.file_name());
            if entry_type.is_dir() {
                copy_fixture(&entry.path(), &destination_path);
            } else if entry_type.is_file() {
                fs::copy(entry.path(), destination_path).expect("copy fixture file");
            }
        }
    }

    fn rewrite_fixture_native_library_names(fixture_root: &Path, output_name: &str) {
        for relative in ["KAIN.toml", "KAIN.fabric.toml"] {
            let path = fixture_root.join(relative);
            let source = fs::read_to_string(&path).expect("read fixture manifest");
            fs::write(&path, source.replace("image_fx.dll", output_name))
                .expect("rewrite fixture manifest");
        }
    }
}
