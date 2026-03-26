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
use kain_core::diagnostics::SpanMapper;
use kain_core::runtime::{self, Env, Value};
use kain_core::{
    CompileTarget, ComputeMetadata, ComputeStreamPlan, ComputeTensorPlan, Item, Lexer, Parser,
    ShaderStage,
};
use kain_crate_ffi::{
    import_crate, ArtifactMode as RustArtifactMode, ImportCrateOptions,
    PrepareContext as RustPrepareContext,
};
use kain_gpu_runtime::VulkanComputeExecutor;
use kain_interop::{
    extract_shared_buffer, extract_shared_image, shared_buffer_value, shared_image_value,
    KainSharedBuffer, SharedBufferMetadata,
};
use kain_omni::fabric::{
    resolve_fabric_path, topological_step_order, unix_timestamp_ms, write_fabric_json,
    FabricComputeDispatchSnapshot, FabricContractKind, FabricEventRecord, FabricExecutionResult,
    FabricFailureReason, FabricManifest, FabricOutputPayloadSnapshot, FabricProducedOutput,
    FabricResolvedInputRecord, FabricRuntimeKind, FabricSessionStatus, FabricSharedBufferSnapshot,
    FabricSharedImageSnapshot, FabricStep, FabricStepExecution, FabricStepStatus,
    FabricValidationResult, FabricValueSnapshot,
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
                Box::new(GpuComputeAdapter),
            ],
        }
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
                let resolved_shader_source = step
                    .shader_source
                    .as_ref()
                    .map(|path| resolve_fabric_path(&session.workspace_root, path));
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
                        shader_source: step.shader_source.clone(),
                        compute_key: step.compute_key.clone(),
                        adapter: None,
                        resolved_entry,
                        resolved_shader_source,
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
                        resolved_inputs: &resolved_inputs,
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

trait FabricRuntimeAdapter {
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
    resolved_inputs: &'a [FabricResolvedInputRecord],
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
        let fabric_entry_source =
            strip_runtime_module_import(&entry_source, "rust", crate_name.as_str());
        let source = format!(
            "{}\n{}",
            imported.canonical_module_source, fabric_entry_source
        );
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
        let fabric_entry_source = strip_runtime_module_import(&entry_source, "c", &import_name);
        let source = format!(
            "{}\n{}",
            imported.canonical_module_source, fabric_entry_source
        );
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

pub struct GpuComputeAdapter;

impl FabricRuntimeAdapter for GpuComputeAdapter {
    fn label(&self) -> &'static str {
        "gpu-compute-vulkan"
    }

    fn supports(&self, kind: &FabricRuntimeKind) -> bool {
        matches!(kind, FabricRuntimeKind::GpuCompute)
    }

    fn execute(&self, context: &FabricAdapterContext) -> Result<Value, FabricFailureReason> {
        let shader_source = context.step.shader_source.as_ref().ok_or_else(|| {
            fabric_failure(
                "missing_shader_source",
                format!(
                    "Fabric step '{}' with runtime 'gpu_compute' must declare 'shader_source'",
                    context.step.id
                ),
            )
        })?;
        let compute_key = context.step.compute_key.as_ref().ok_or_else(|| {
            fabric_failure(
                "missing_compute_key",
                format!(
                    "Fabric step '{}' with runtime 'gpu_compute' must declare 'compute_key'",
                    context.step.id
                ),
            )
        })?;

        let resolved_shader = resolve_fabric_path(context.workspace_root, shader_source);
        let shader_text = fs::read_to_string(&resolved_shader).map_err(|err| {
            fabric_failure(
                "shader_read_failed",
                format!(
                    "Fabric step '{}': failed to read shader source '{}': {}",
                    context.step.id,
                    resolved_shader.display(),
                    err
                ),
            )
        })?;

        // Let kain-driver own GPU compile-time registration so Fabric does not
        // double-register bridge extensions and diverge from `kain gpu-artifacts`.
        let session = kain_driver::DriverSession::default();
        let artifact_output = session
            .compile_shader_artifact_bundle(&shader_text)
            .map_err(|err| runtime_bridge_failure(context.step, "shader_compile_failed", err))?;

        // Write shader bundle JSON to session directory
        let shader_bundle_path = context
            .session_directory
            .join(format!("{}_shader_bundle.json", context.step.id));
        fs::write(&shader_bundle_path, &artifact_output.bundle_json).map_err(|err| {
            fabric_failure(
                "shader_bundle_write_failed",
                format!("Fabric step '{}': {}", context.step.id, err),
            )
        })?;

        // Build compute residency from artifact bundle metadata plus authored compute plan details.
        let compute_metadata = parse_compute_metadata_for_shader(&shader_text, compute_key)?;
        let residency = build_compute_residency_from_artifact(
            context,
            compute_key,
            &artifact_output.bundle,
            compute_metadata.as_ref(),
        )?;
        let residency_json = serde_json::to_string_pretty(&residency).map_err(|err| {
            fabric_failure(
                "residency_serialize_failed",
                format!("Fabric step '{}': {}", context.step.id, err),
            )
        })?;
        let residency_path = context
            .session_directory
            .join(format!("{}_compute_residency.json", context.step.id));
        fs::write(&residency_path, &residency_json).map_err(|err| {
            fabric_failure(
                "residency_write_failed",
                format!("Fabric step '{}': {}", context.step.id, err),
            )
        })?;

        // Write payload sidecar files for each binding
        let residency_root = residency_path.parent().unwrap_or(Path::new("."));
        write_residency_payload_files(context, &residency, residency_root)?;

        // Create executor and dispatch
        let executor = VulkanComputeExecutor::try_new().map_err(|err| {
            fabric_failure(
                "vulkan_init_failed",
                format!(
                    "Fabric step '{}': Vulkan compute executor init failed: {}",
                    context.step.id, err
                ),
            )
        })?;
        let dispatch_result = executor
            .dispatch_from_sidecars(&shader_bundle_path, &residency_path, compute_key)
            .map_err(|err| {
                fabric_failure(
                    "compute_dispatch_failed",
                    format!(
                        "Fabric step '{}': dispatch failed: {}",
                        context.step.id, err
                    ),
                )
            })?;

        // Build output value from dispatch results
        let output_buffers = dispatch_result
            .output_bindings
            .iter()
            .map(|(slot, bytes)| {
                let binding_key = residency
                    .compute_shaders
                    .first()
                    .and_then(|entry| {
                        entry
                            .bindings
                            .iter()
                            .find(|b| b.slot == *slot)
                            .map(|b| b.key.clone())
                    })
                    .unwrap_or_else(|| format!("output_slot_{}", slot));
                let metadata = SharedBufferMetadata {
                    element_type: "f32".to_string(),
                    element_size: 4,
                    shape: vec![(bytes.len() / 4) as i64],
                    strides: vec![1],
                    format: Some("f32".to_string()),
                    mime_type: Some("application/octet-stream".to_string()),
                    source_runtime: "gpu-compute".to_string(),
                    source_backend: Some("vulkan".to_string()),
                    ownership: "owned".to_string(),
                    labels: vec![binding_key.clone(), "gpu-output".to_string()],
                };
                let shared = KainSharedBuffer::owned(metadata, bytes.clone());
                (binding_key, shared_buffer_value(shared))
            })
            .collect::<Vec<_>>();

        // Build dispatch snapshot value for downstream report
        let dispatch_snapshot = FabricComputeDispatchSnapshot {
            compute_key: compute_key.clone(),
            dispatch_invocations: dispatch_result.dispatch_invocations,
            tensor_binding_count: dispatch_result.tensor_binding_count,
            stream_binding_count: dispatch_result.stream_binding_count,
            neural_node_count: dispatch_result.neural_node_count,
            output_binding_count: dispatch_result.output_bindings.len(),
            total_output_bytes: dispatch_result
                .output_bindings
                .iter()
                .map(|(_, b)| b.len())
                .sum(),
        };

        // If step has outputs, return output buffers mapped by name
        // If single output buffer, return it directly
        // If no declared outputs, return dispatch summary as value
        if output_buffers.len() == 1 && context.step.outputs.len() == 1 {
            Ok(output_buffers.into_iter().next().unwrap().1)
        } else if output_buffers.is_empty() {
            // Return metadata about the dispatch as a value
            Ok(Value::String(format!(
                "gpu-compute:{}:invocations={}:tensors={}:outputs=0",
                compute_key,
                dispatch_snapshot.dispatch_invocations,
                dispatch_snapshot.tensor_binding_count,
            )))
        } else {
            let mut fields = HashMap::new();
            for (key, value) in output_buffers {
                fields.insert(key, value);
            }
            Ok(Value::Struct(
                "GpuComputeOutputs".to_string(),
                Arc::new(RwLock::new(fields)),
            ))
        }
    }
}

/// Internal compute residency model for sidecar generation.
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
struct FabricComputeResidency {
    target: String,
    compute_shaders: Vec<FabricComputeResidencyEntry>,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
struct FabricComputeResidencyEntry {
    key: String,
    module_name: String,
    entry_point: String,
    workgroup_size: Option<[u32; 3]>,
    dispatch_size: Option<[u32; 3]>,
    tensor_binding_count: usize,
    stream_binding_count: usize,
    neural_node_count: usize,
    bindings: Vec<FabricComputeResidencyBinding>,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
struct FabricComputeResidencyBinding {
    key: String,
    contract: String,
    descriptor_kind: String,
    element_type: String,
    shape: Vec<i64>,
    strides: Vec<i64>,
    access_mode: String,
    slot: u32,
    payload_file: String,
}

/// Build compute residency from a compiled ShaderArtifactBundle.
///
/// Uses the bundle's `entry_points` to locate the compute entry matching `compute_key`,
/// and the `resource_layouts` to fill binding descriptors.
fn build_compute_residency_from_artifact(
    context: &FabricAdapterContext,
    compute_key: &str,
    bundle: &kain_core::ShaderArtifactBundle,
    compute_metadata: Option<&ComputeMetadata>,
) -> Result<FabricComputeResidency, FabricFailureReason> {
    // Find a matching entry point — the compute_key is typically "shader::Name::compute"
    let entry = bundle
        .entry_points
        .iter()
        .find(|ep| {
            ep.stage == "compute"
                && (ep.shader == compute_key
                    || ep.entry_point == compute_key
                    || format!("shader::{}::compute", ep.shader) == compute_key)
        })
        .ok_or_else(|| {
            fabric_failure(
                "compute_key_not_found",
                format!(
                    "Fabric step '{}': compute_key '{}' was not found in shader bundle entry_points: [{}]",
                    context.step.id,
                    compute_key,
                    bundle
                        .entry_points
                        .iter()
                        .map(|ep| format!("{}:{}", ep.shader, ep.entry_point))
                        .collect::<Vec<_>>()
                        .join(", ")
                ),
            )
        })?;

    // Collect resource bindings for this shader from the bundle's resource layouts
    let layouts: Vec<_> = bundle
        .resource_layouts
        .iter()
        .filter(|layout| layout.shader == entry.shader)
        .collect();

    let workgroup_size = compute_metadata
        .and_then(|metadata| metadata.workgroup_size)
        .unwrap_or([8, 1, 1]);
    let dispatch_size = compute_metadata
        .map(|metadata| metadata.dispatch_size)
        .unwrap_or([1, 1, 1]);
    let stream_plans = compute_metadata.and_then(|metadata| metadata.stream_plans.as_ref());
    let mut bindings = Vec::new();
    for layout in &layouts {
        let descriptor_kind = match layout.kind.as_str() {
            "storage_buffer" | "read_write" => "storage_buffer",
            _ => "uniform_buffer",
        };
        let access_mode = infer_compute_binding_access_mode(
            compute_metadata,
            stream_plans,
            &layout.name,
            layout.kind.as_str(),
        );
        let payload_file = format!("{}_binding_{}.bin", context.step.id, layout.binding);
        let element_type = infer_element_type_from_layout(&layout.ty);
        let inferred_shape = resolve_upstream_binding_shape(context, &layout.name, &element_type)
            .or_else(|| {
                compute_metadata.and_then(|metadata| {
                    metadata
                        .tensor_plans
                        .iter()
                        .find(|plan| plan.key == layout.name)
                        .map(resolve_tensor_plan_shape)
                })
            })
            .unwrap_or_else(|| vec![1]);
        bindings.push(FabricComputeResidencyBinding {
            key: layout.name.clone(),
            contract: "kain.shared.buffer".to_string(),
            descriptor_kind: descriptor_kind.to_string(),
            element_type,
            shape: inferred_shape,
            strides: vec![1],
            access_mode: access_mode.to_string(),
            slot: layout.binding,
            payload_file,
        });
    }

    Ok(FabricComputeResidency {
        target: "spirv".to_string(),
        compute_shaders: vec![FabricComputeResidencyEntry {
            key: compute_key.to_string(),
            module_name: entry.module_name.clone(),
            entry_point: entry.entry_point.clone(),
            workgroup_size: Some(workgroup_size),
            dispatch_size: Some(dispatch_size),
            tensor_binding_count: 0,
            stream_binding_count: stream_plans.map(|plans| plans.len()).unwrap_or(0),
            neural_node_count: compute_metadata
                .map(|metadata| metadata.neural_node_plans.len())
                .unwrap_or(0),
            bindings,
        }],
    })
}

fn parse_compute_metadata_for_shader(
    shader_source: &str,
    compute_key: &str,
) -> Result<Option<ComputeMetadata>, FabricFailureReason> {
    let tokens = Lexer::new(shader_source).tokenize().map_err(|err| {
        fabric_failure(
            "shader_metadata_tokenize_failed",
            format!("Fabric compute metadata tokenization failed: {err}"),
        )
    })?;
    let span_mapper = SpanMapper::new(shader_source);
    let program = Parser::new(&tokens, &span_mapper, "<fabric-gpu-shader>")
        .parse()
        .map_err(|err| {
            fabric_failure(
                "shader_metadata_parse_failed",
                format!("Fabric compute metadata parse failed: {err}"),
            )
        })?;
    let shader_name = compute_key
        .strip_prefix("shader::")
        .and_then(|value| value.strip_suffix("::compute"))
        .unwrap_or(compute_key);
    let shader = program
        .items
        .iter()
        .find_map(|item| match item {
            Item::Shader(shader)
                if matches!(shader.stage, ShaderStage::Compute)
                    && (shader.name == shader_name || shader.name == compute_key) =>
            {
                Some(shader)
            }
            _ => None,
        })
        .ok_or_else(|| {
            fabric_failure(
                "shader_metadata_missing_entry",
                format!(
                    "Fabric compute metadata could not find compute shader '{}' in source",
                    compute_key
                ),
            )
        })?;
    shader.explicit_compute_metadata().map_err(|err| {
        fabric_failure(
            "shader_metadata_invalid",
            format!(
                "Fabric compute metadata for '{}' is invalid: {err}",
                shader.name
            ),
        )
    })
}

fn infer_compute_binding_access_mode(
    compute_metadata: Option<&ComputeMetadata>,
    stream_plans: Option<&Vec<ComputeStreamPlan>>,
    binding_name: &str,
    layout_kind: &str,
) -> String {
    if let Some(stream_plans) = stream_plans {
        if let Some(plan) = stream_plans.iter().find(|plan| plan.key == binding_name) {
            return match plan.direction.as_str() {
                "egress" | "output" => "write".to_string(),
                "bidirectional" | "duplex" | "read_write" => "read_write".to_string(),
                _ => "read".to_string(),
            };
        }
    }
    if let Some(metadata) = compute_metadata {
        if let Some(plan) = metadata
            .tensor_plans
            .iter()
            .find(|plan| plan.key == binding_name)
        {
            return match plan.role.as_str() {
                "output" | "egress" => "write".to_string(),
                "state" | "scratch" | "read_write" | "inout" => "read_write".to_string(),
                _ => "read".to_string(),
            };
        }
        if metadata
            .neural_node_plans
            .iter()
            .any(|plan| plan.outputs.iter().any(|output| output == binding_name))
        {
            return "write".to_string();
        }
    }
    match layout_kind {
        "read_write" => "read_write".to_string(),
        "write" => "write".to_string(),
        _ => "read".to_string(),
    }
}

fn resolve_tensor_plan_shape(plan: &ComputeTensorPlan) -> Vec<i64> {
    plan.shape
        .iter()
        .map(|dimension| match dimension.as_str() {
            "dispatch.x" | "dispatch.y" | "dispatch.z" => 1,
            _ => dimension.parse::<i64>().unwrap_or(1),
        })
        .collect()
}

fn infer_element_type_from_layout(ty: &str) -> String {
    let lower = ty.to_ascii_lowercase();
    if lower.contains("vec4") || lower.contains("float") {
        "f32".to_string()
    } else if lower.contains("int") || lower.contains("i32") {
        "i32".to_string()
    } else if lower.contains("uint") || lower.contains("u32") {
        "u32".to_string()
    } else {
        "f32".to_string()
    }
}

fn resolve_upstream_binding_bytes(
    context: &FabricAdapterContext,
    binding_key: &str,
) -> Option<Vec<u8>> {
    // Walk through fabric_inputs looking for a shared buffer matching this key
    if let Value::Struct(_, fields) = &context.fabric_inputs {
        let fields = fields.read().ok()?;
        for (_, step_outputs) in fields.iter() {
            if let Value::Struct(_, output_fields) = step_outputs {
                let output_fields = output_fields.read().ok()?;
                if let Some(value) = output_fields.get(binding_key) {
                    if let Ok(buffer) = extract_shared_buffer(value) {
                        return Some(buffer.bytes());
                    }
                }
            }
        }
    }
    None
}

fn resolve_upstream_binding_shape(
    context: &FabricAdapterContext,
    binding_key: &str,
    element_type: &str,
) -> Option<Vec<i64>> {
    for input in context.resolved_inputs {
        if input.output_name != binding_key {
            continue;
        }
        if let FabricOutputPayloadSnapshot::SharedBuffer { buffer } = &input.payload {
            if !buffer.shape.is_empty() {
                return Some(buffer.shape.clone());
            }
        }
    }

    let bytes = resolve_upstream_binding_bytes(context, binding_key)?;
    let element_size = match element_type {
        "u8" | "i8" | "bool" => 1usize,
        "u16" | "i16" => 2usize,
        "u32" | "i32" | "f32" => 4usize,
        "u64" | "i64" | "f64" => 8usize,
        _ => 4usize,
    };
    let element_count = (bytes.len() / element_size.max(1)).max(1) as i64;
    Some(vec![element_count])
}

fn zero_fill_binding(shape: &[i64], element_type: &str) -> Vec<u8> {
    let element_size = match element_type {
        "u8" | "i8" | "bool" => 1,
        "u16" | "i16" => 2,
        "u32" | "i32" | "f32" => 4,
        "u64" | "i64" | "f64" => 8,
        _ => 4,
    };
    let total_elements: i64 = shape.iter().copied().product();
    vec![0u8; (total_elements.max(1) as usize) * element_size]
}

fn write_residency_payload_files(
    context: &FabricAdapterContext,
    residency: &FabricComputeResidency,
    root: &Path,
) -> Result<(), FabricFailureReason> {
    for entry in &residency.compute_shaders {
        for binding in &entry.bindings {
            let payload_path = root.join(&binding.payload_file);
            let bytes = resolve_upstream_binding_bytes(context, &binding.key)
                .unwrap_or_else(|| zero_fill_binding(&binding.shape, &binding.element_type));
            fs::write(&payload_path, &bytes).map_err(|err| {
                fabric_failure(
                    "payload_write_failed",
                    format!(
                        "Fabric step '{}': failed to write payload '{}': {}",
                        context.step.id,
                        payload_path.display(),
                        err
                    ),
                )
            })?;
        }
    }
    Ok(())
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
        FabricContractKind::ComputePlan => {
            // ComputePlan outputs are passed through as-is (the adapter handles the GPU dispatch)
            Ok(value)
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
        FabricContractKind::ComputePlan => Ok(FabricOutputPayloadSnapshot::ComputePlan {
            dispatch: FabricComputeDispatchSnapshot {
                compute_key: "unknown".to_string(),
                dispatch_invocations: 0,
                tensor_binding_count: 0,
                stream_binding_count: 0,
                neural_node_count: 0,
                output_binding_count: 0,
                total_output_bytes: 0,
            },
        }),
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

fn render_declared_output_struct(
    struct_name: &str,
    outputs: &[kain_omni::fabric::FabricOutputBinding],
) -> Option<String> {
    if outputs.len() <= 1 {
        return None;
    }

    let mut lines = vec![format!("struct {struct_name}:")];
    for output in outputs {
        lines.push(format!("    {}: Any", output.name));
    }
    Some(lines.join("\n"))
}

fn render_python_output_projection(
    struct_name: &str,
    outputs: &[kain_omni::fabric::FabricOutputBinding],
) -> String {
    let fields = outputs
        .iter()
        .map(|output| {
            let field_name = kain_string_literal(&output.name);
            let value_expr = match output.kind {
                FabricContractKind::Value => format!(
                    "py_bridge_call(\"__kain_fabric_output_field\", [fabric_result, {field_name}])"
                ),
                FabricContractKind::SharedBuffer => format!(
                    "py_bridge_shared_buffer(py_bridge_call_raw(\"__kain_fabric_output_field\", [fabric_result, {field_name}]))"
                ),
                FabricContractKind::SharedImage => format!(
                    "py_bridge_shared_image(py_bridge_call_raw(\"__kain_fabric_output_field\", [fabric_result, {field_name}]))"
                ),
                FabricContractKind::ComputePlan => format!(
                    "py_bridge_call(\"__kain_fabric_output_field\", [fabric_result, {field_name}])"
                ),
            };
            format!("{}: {value_expr}", output.name)
        })
        .collect::<Vec<_>>()
        .join(", ");
    format!("return {struct_name} {{ {fields} }}")
}

fn render_node_output_projection(
    struct_name: &str,
    outputs: &[kain_omni::fabric::FabricOutputBinding],
) -> String {
    let assertions = outputs
        .iter()
        .map(|output| {
            let field_name = kain_string_literal(&output.name);
            let marker = kain_string_literal(&fabric_missing_output_marker(&output.name));
            format!("assert(js_bridge_hasattr(fabric_result, {field_name}), {marker})")
        })
        .collect::<Vec<_>>();
    let fields = outputs
        .iter()
        .map(|output| {
            let field_name = kain_string_literal(&output.name);
            let value_expr = match output.kind {
                FabricContractKind::Value => {
                    format!("js_bridge_getattr(fabric_result, {field_name})")
                }
                FabricContractKind::SharedBuffer => format!(
                    "js_web_shared_buffer(js_bridge_getattr_raw(fabric_result, {field_name}))"
                ),
                FabricContractKind::SharedImage => format!(
                    "js_web_shared_image(js_bridge_getattr_raw(fabric_result, {field_name}))"
                ),
                FabricContractKind::ComputePlan => {
                    format!("js_bridge_getattr(fabric_result, {field_name})")
                }
            };
            format!("{}: {value_expr}", output.name)
        })
        .collect::<Vec<_>>()
        .join(", ");
    let mut lines = assertions;
    lines.push(format!("return {struct_name} {{ {fields} }}"));
    lines.join("\n    ")
}

fn render_python_harness(context: &FabricAdapterContext) -> Result<String, FabricFailureReason> {
    let runtime_call = python_call_expression(context)?;
    let output_struct = render_declared_output_struct("FabricPythonOutputs", &context.step.outputs)
        .map(|definition| format!("{definition}\n\n"))
        .unwrap_or_default();
    Ok(format!(
        "use std::python::bridge\n\n{output_struct}fn main() -> Any:\n    {runtime_call}\n"
    ))
}

fn python_call_expression(context: &FabricAdapterContext) -> Result<String, FabricFailureReason> {
    let multi_output_step = context.step.outputs.len() > 1;
    let python_output_helper = kain_string_literal(&format!(
        "def __kain_fabric_output_field(payload, name):\n    try:\n        if isinstance(payload, dict):\n            return payload[name]\n        return getattr(payload, name)\n    except KeyError:\n        raise KeyError(\"{FABRIC_MISSING_OUTPUT_MARKER}:\" + str(name))\n    except AttributeError:\n        raise AttributeError(\"{FABRIC_MISSING_OUTPUT_MARKER}:\" + str(name))\n"
    ));

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
        if multi_output_step {
            let projection =
                render_python_output_projection("FabricPythonOutputs", &context.step.outputs);
            Ok(format!(
                "py_bridge_exec({exec_source})\n    py_bridge_exec({python_output_helper})\n    let fabric_result = py_bridge_call_raw({callable}, [fabric_inputs])\n    {projection}"
            ))
        } else {
            let single_output_kind = context
                .step
                .outputs
                .first()
                .map(|output| output.kind)
                .unwrap_or(FabricContractKind::Value);
            let result_expr = match single_output_kind {
                FabricContractKind::Value => {
                    format!("py_bridge_call({callable}, [fabric_inputs])")
                }
                FabricContractKind::SharedBuffer => {
                    format!(
                        "py_bridge_shared_buffer(py_bridge_call_raw({callable}, [fabric_inputs]))"
                    )
                }
                FabricContractKind::SharedImage => {
                    format!(
                        "py_bridge_shared_image(py_bridge_call_raw({callable}, [fabric_inputs]))"
                    )
                }
                FabricContractKind::ComputePlan => {
                    format!("py_bridge_call({callable}, [fabric_inputs])")
                }
            };
            Ok(format!(
                "py_bridge_exec({exec_source})\n    return {result_expr}"
            ))
        }
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
        if multi_output_step {
            let projection =
                render_python_output_projection("FabricPythonOutputs", &context.step.outputs);
            Ok(format!(
                "py_bridge_exec({python_output_helper})\n    let fabric_module = py_bridge_require_module({module_literal})\n    let fabric_result = py_bridge_call_attr_raw(fabric_module, \"run\", [fabric_inputs])\n    {projection}"
            ))
        } else {
            let single_output_kind = context
                .step
                .outputs
                .first()
                .map(|output| output.kind)
                .unwrap_or(FabricContractKind::Value);
            let result_expr = match single_output_kind {
                FabricContractKind::Value => {
                    format!(
                        "py_bridge_call_attr(py_bridge_require_module({module_literal}), \"run\", [fabric_inputs])"
                    )
                }
                FabricContractKind::SharedBuffer => format!(
                    "py_bridge_shared_buffer(py_bridge_call_attr_raw(py_bridge_require_module({module_literal}), \"run\", [fabric_inputs]))"
                ),
                FabricContractKind::SharedImage => format!(
                    "py_bridge_shared_image(py_bridge_call_attr_raw(py_bridge_require_module({module_literal}), \"run\", [fabric_inputs]))"
                ),
                FabricContractKind::ComputePlan => format!(
                    "py_bridge_call_attr(py_bridge_require_module({module_literal}), \"run\", [fabric_inputs])"
                ),
            };
            Ok(format!("return {result_expr}"))
        }
    }
}

fn render_node_harness(context: &FabricAdapterContext) -> Result<String, FabricFailureReason> {
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

    let import_literal = kain_string_literal(&import_specifier);
    let export_literal = kain_string_literal(&export_name);
    let output_struct = render_declared_output_struct("FabricNodeOutputs", &context.step.outputs)
        .map(|definition| format!("{definition}\n\n"))
        .unwrap_or_default();
    let runtime_lines = if context.step.outputs.len() > 1 {
        let projection = render_node_output_projection("FabricNodeOutputs", &context.step.outputs);
        format!(
            "let module_ref = js_bridge_import({import_literal})\n    let fabric_result = js_bridge_call_method_raw(module_ref, {export_literal}, [fabric_inputs])\n    {projection}"
        )
    } else {
        let single_output_kind = context
            .step
            .outputs
            .first()
            .map(|output| output.kind)
            .unwrap_or(FabricContractKind::Value);
        let result_expr = match single_output_kind {
            FabricContractKind::Value => {
                format!("js_bridge_call_method(module_ref, {export_literal}, [fabric_inputs])")
            }
            FabricContractKind::SharedBuffer => format!(
                "js_web_shared_buffer(js_bridge_call_method_raw(module_ref, {export_literal}, [fabric_inputs]))"
            ),
            FabricContractKind::SharedImage => format!(
                "js_web_shared_image(js_bridge_call_method_raw(module_ref, {export_literal}, [fabric_inputs]))"
            ),
            FabricContractKind::ComputePlan => {
                format!("js_bridge_call_method(module_ref, {export_literal}, [fabric_inputs])")
            }
        };
        format!("let module_ref = js_bridge_import({import_literal})\n    return {result_expr}")
    };

    Ok(format!(
        "use std::javascript::bridge\nuse std::javascript::web\n\n{output_struct}fn main() -> Any:\n    {runtime_lines}\n"
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
    let normalized_path = fs::canonicalize(path).unwrap_or_else(|_| path.to_path_buf());
    let mut rendered = normalized_path.display().to_string().replace('\\', "/");
    if let Some(stripped) = rendered.strip_prefix("//?/") {
        rendered = stripped.to_string();
    }
    if normalized_path.is_absolute() {
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

const FABRIC_MISSING_OUTPUT_MARKER: &str = "__kain_fabric_missing_output__";

fn fabric_missing_output_marker(output_name: &str) -> String {
    format!("{FABRIC_MISSING_OUTPUT_MARKER}:{output_name}")
}

fn strip_runtime_module_import(source: &str, lane: &str, module_name: &str) -> String {
    let expected = format!("use {lane}::{module_name}");
    let mut kept_lines = Vec::new();
    for line in source.lines() {
        let trimmed = line.trim();
        if trimmed == expected || trimmed == format!("{expected};") {
            continue;
        }
        kept_lines.push(line);
    }
    kept_lines.join("\n")
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
    let rendered = err.to_string();
    if let Some(failure) = structured_missing_output_failure(step, &rendered) {
        return failure;
    }
    FabricFailureReason {
        code: code.into(),
        message: format!("Fabric step '{}' failed: {rendered}", step.id),
        details: Some(json!({
            "step_id": step.id,
            "runtime": step.runtime.display_name(),
        })),
    }
}

fn structured_missing_output_failure(
    step: &FabricStep,
    message: &str,
) -> Option<FabricFailureReason> {
    let marker_index = message.find(FABRIC_MISSING_OUTPUT_MARKER)?;
    let marker = &message[marker_index..];
    let output_name = marker
        .strip_prefix(FABRIC_MISSING_OUTPUT_MARKER)?
        .strip_prefix(':')?
        .chars()
        .take_while(|ch| ch.is_ascii_alphanumeric() || matches!(ch, '_' | '-' | '.'))
        .collect::<String>();
    if output_name.is_empty() {
        return None;
    }
    Some(FabricFailureReason {
        code: "missing_output_field".to_string(),
        message: format!(
            "Fabric step '{}' did not return required output field '{}'",
            step.id, output_name
        ),
        details: Some(json!({
            "step_id": step.id,
            "runtime": step.runtime.display_name(),
            "output_name": output_name,
            "declared_outputs": step.outputs.iter().map(|output| output.name.clone()).collect::<Vec<_>>(),
        })),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn local_fabric_manifest_executes_and_records_typed_outputs() {
        let dir = tempfile::tempdir().unwrap();
        let init =
            kain_omni::init_fabric_manifest(dir.path(), kain_omni::FabricTemplateKind::Local)
                .unwrap();

        let result = execute_fabric_manifest_path(&init.manifest_path).unwrap();

        assert_eq!(
            result.status,
            FabricSessionStatus::Succeeded,
            "{}",
            fs::read_to_string(&result.report_path).unwrap_or_else(|_| {
                format!(
                    "failed to read Fabric report at {}",
                    result.report_path.display()
                )
            })
        );
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
    fn polyglot_init_template_executes_end_to_end() {
        let fixture_root = stable_fabric_test_root("fab-init");
        prepare_fabric_test_workspace(&fixture_root);
        let init =
            kain_omni::init_fabric_manifest(&fixture_root, kain_omni::FabricTemplateKind::Polyglot)
                .unwrap();
        compile_fixture_shared_library(&fixture_root);

        let result = execute_fabric_manifest_path(&init.manifest_path).unwrap();

        assert_eq!(
            result.status,
            FabricSessionStatus::Succeeded,
            "{}",
            fs::read_to_string(&result.report_path).unwrap_or_else(|_| {
                format!(
                    "failed to read Fabric report at {}",
                    result.report_path.display()
                )
            })
        );
        assert_eq!(result.step_results.len(), 5);
        assert!(result
            .step_results
            .iter()
            .all(|step| step.status == FabricStepStatus::Succeeded));
        assert!(result
            .step_results
            .iter()
            .any(|step| step.id == "python_source"));
        assert!(result
            .step_results
            .iter()
            .any(|step| step.id == "node_packager"));
    }

    #[test]
    fn python_harness_supports_mixed_multi_output_steps() {
        let step = FabricStep {
            id: "python_bridge".to_string(),
            runtime: FabricRuntimeKind::Python,
            entry: None,
            module: Some("fabric_python_bridge".to_string()),
            crate_name: None,
            manifest_path: None,
            library: None,
            shader_source: None,
            compute_key: None,
            depends_on: Vec::new(),
            requires: Vec::new(),
            outputs: vec![
                kain_omni::fabric::FabricOutputBinding {
                    name: "report".to_string(),
                    kind: FabricContractKind::Value,
                },
                kain_omni::fabric::FabricOutputBinding {
                    name: "image".to_string(),
                    kind: FabricContractKind::SharedImage,
                },
                kain_omni::fabric::FabricOutputBinding {
                    name: "snapshot".to_string(),
                    kind: FabricContractKind::SharedBuffer,
                },
            ],
        };
        let manifest_root = Path::new("M:/Code/Kain");
        let manifest_path = manifest_root.join("KAIN.fabric.toml");
        let context = FabricAdapterContext {
            manifest_path: &manifest_path,
            manifest_root,
            workspace_root: manifest_root,
            session_directory: manifest_root,
            step: &step,
            fabric_inputs: Value::Unit,
            resolved_inputs: &[],
        };

        let harness = render_python_harness(&context).unwrap();

        assert!(harness.contains("struct FabricPythonOutputs:"));
        assert!(harness.contains("__kain_fabric_output_field"));
        assert!(harness.contains("py_bridge_call_attr_raw(fabric_module, \"run\""));
        assert!(harness.contains("[fabric_inputs]"));
        assert!(!harness.contains("fabric_serialized_inputs"));
        assert!(harness.contains("py_bridge_shared_image("));
        assert!(harness.contains("py_bridge_shared_buffer("));
    }

    #[test]
    fn node_harness_supports_mixed_multi_output_steps() {
        let step = FabricStep {
            id: "node_bridge".to_string(),
            runtime: FabricRuntimeKind::Node,
            entry: None,
            module: Some("fabric_node_bridge".to_string()),
            crate_name: None,
            manifest_path: None,
            library: None,
            shader_source: None,
            compute_key: None,
            depends_on: Vec::new(),
            requires: Vec::new(),
            outputs: vec![
                kain_omni::fabric::FabricOutputBinding {
                    name: "report".to_string(),
                    kind: FabricContractKind::Value,
                },
                kain_omni::fabric::FabricOutputBinding {
                    name: "image".to_string(),
                    kind: FabricContractKind::SharedImage,
                },
                kain_omni::fabric::FabricOutputBinding {
                    name: "snapshot".to_string(),
                    kind: FabricContractKind::SharedBuffer,
                },
            ],
        };
        let manifest_root = Path::new("M:/Code/Kain");
        let manifest_path = manifest_root.join("KAIN.fabric.toml");
        let context = FabricAdapterContext {
            manifest_path: &manifest_path,
            manifest_root,
            workspace_root: manifest_root,
            session_directory: manifest_root,
            step: &step,
            fabric_inputs: Value::Unit,
            resolved_inputs: &[],
        };

        let harness = render_node_harness(&context).unwrap();

        assert!(harness.contains("struct FabricNodeOutputs:"));
        assert!(harness.contains("js_bridge_call_method_raw"));
        assert!(harness.contains("[fabric_inputs]"));
        assert!(!harness.contains("fabric_serialized_inputs"));
        assert!(harness.contains("__kain_fabric_missing_output__:report"));
        assert!(harness.contains("js_bridge_hasattr(fabric_result"));
        assert!(harness.contains("js_bridge_getattr(fabric_result"));
        assert!(harness.contains("js_web_shared_image("));
        assert!(harness.contains("js_web_shared_buffer("));
    }

    #[test]
    fn python_missing_output_field_reports_structured_failure() {
        assert_missing_output_failure(
            FabricRuntimeKind::Python,
            "scripts/python_missing.py",
            "def run(fabric_inputs):\n    return {\"report\": \"only-one\"}\n",
        );
    }

    #[test]
    fn node_missing_output_field_reports_structured_failure() {
        assert_missing_output_failure(
            FabricRuntimeKind::Node,
            "scripts/node_missing.mjs",
            "export function run(fabricInputs) {\n  return { report: \"only-one\" };\n}\n",
        );
    }

    #[test]
    fn python_step_consumes_shared_inputs_via_fabric_inputs() {
        assert_runtime_consumes_canonical_shared_inputs(
            FabricRuntimeKind::Python,
            "scripts/consumer.py",
            "def run(fabric_inputs):\n    image = fabric_inputs[\"seed\"][\"image\"]\n    snapshot = fabric_inputs[\"seed\"][\"snapshot\"]\n    assert image[\"contract\"] == \"kain.shared.image\"\n    assert snapshot[\"contract\"] == \"kain.shared.buffer\"\n    assert isinstance(image[\"bytes\"], bytearray)\n    assert isinstance(snapshot[\"bytes\"], bytearray)\n    return f\"{type(image['bytes']).__name__}:{image['width']}:{snapshot['element_type']}:{snapshot['bytes'][2]}\"\n",
            "bytearray:1:u8:30",
        );
    }

    #[test]
    fn node_step_consumes_shared_inputs_via_fabric_inputs() {
        assert_runtime_consumes_canonical_shared_inputs(
            FabricRuntimeKind::Node,
            "scripts/consumer.mjs",
            "export function run(fabricInputs) {\n  const image = fabricInputs.seed.image;\n  const snapshot = fabricInputs.seed.snapshot;\n  if (image.contract !== 'kain.shared.image') throw new Error('expected shared image contract');\n  if (snapshot.contract !== 'kain.shared.buffer') throw new Error('expected shared buffer contract');\n  if (!(image.bytes instanceof Uint8Array)) throw new Error('expected typed image bytes');\n  if (!(snapshot.bytes instanceof Uint8Array)) throw new Error('expected typed buffer bytes');\n  return `${image.bytes.constructor.name}:${image.width}:${snapshot.element_type}:${snapshot.bytes[2]}`;\n}\n",
            "Uint8Array:1:u8:30",
        );
    }

    #[test]
    fn gpu_compute_residency_prefers_compute_metadata_and_resolved_input_shapes() {
        let repo_root = Path::new(env!("CARGO_MANIFEST_DIR"))
            .parent()
            .unwrap()
            .parent()
            .unwrap()
            .to_path_buf();
        let shader_path =
            repo_root.join("smoketest/fabric/gpu_compute_convergence/shaders/gpu_step.kn");
        let shader_text = fs::read_to_string(&shader_path).expect("read gpu fabric shader");
        let bundle = kain_driver::DriverSession::default()
            .compile_shader_artifact_bundle(&shader_text)
            .expect("compile shader artifact bundle");
        let compute_key = "shader::FabricGpuCopy::compute";
        let compute_metadata = parse_compute_metadata_for_shader(&shader_text, compute_key)
            .expect("parse compute metadata");

        let step = FabricStep {
            id: "gpu_enrich".to_string(),
            runtime: FabricRuntimeKind::GpuCompute,
            entry: None,
            module: None,
            crate_name: None,
            manifest_path: None,
            library: None,
            shader_source: Some(shader_path.clone()),
            compute_key: Some(compute_key.to_string()),
            depends_on: vec!["kain_orchestrator".to_string()],
            requires: Vec::new(),
            outputs: vec![kain_omni::fabric::FabricOutputBinding {
                name: "dst".to_string(),
                kind: FabricContractKind::SharedBuffer,
            }],
        };
        let manifest_root = repo_root.join("smoketest/fabric/gpu_compute_convergence");
        let manifest_path = manifest_root.join("KAIN.fabric.toml");
        let resolved_inputs = vec![
            FabricResolvedInputRecord {
                from_step_id: "kain_orchestrator".to_string(),
                output_name: "src".to_string(),
                declared_kind: FabricContractKind::SharedBuffer,
                payload: FabricOutputPayloadSnapshot::SharedBuffer {
                    buffer: kain_omni::fabric::FabricSharedBufferSnapshot {
                        contract: "kain.shared.buffer".to_string(),
                        byte_length: 32,
                        element_type: "f32".to_string(),
                        element_size: 4,
                        shape: vec![8],
                        strides: vec![1],
                        format: None,
                        mime_type: None,
                        source_runtime: "kain".to_string(),
                        source_backend: None,
                        ownership: "shared".to_string(),
                        labels: Vec::new(),
                    },
                },
            },
            FabricResolvedInputRecord {
                from_step_id: "kain_orchestrator".to_string(),
                output_name: "dst".to_string(),
                declared_kind: FabricContractKind::SharedBuffer,
                payload: FabricOutputPayloadSnapshot::SharedBuffer {
                    buffer: kain_omni::fabric::FabricSharedBufferSnapshot {
                        contract: "kain.shared.buffer".to_string(),
                        byte_length: 32,
                        element_type: "f32".to_string(),
                        element_size: 4,
                        shape: vec![8],
                        strides: vec![1],
                        format: None,
                        mime_type: None,
                        source_runtime: "kain".to_string(),
                        source_backend: None,
                        ownership: "shared".to_string(),
                        labels: Vec::new(),
                    },
                },
            },
        ];
        let context = FabricAdapterContext {
            manifest_path: &manifest_path,
            manifest_root: &manifest_root,
            workspace_root: &manifest_root,
            session_directory: &manifest_root,
            step: &step,
            fabric_inputs: Value::Unit,
            resolved_inputs: &resolved_inputs,
        };

        let residency = build_compute_residency_from_artifact(
            &context,
            compute_key,
            &bundle.bundle,
            compute_metadata.as_ref(),
        )
        .expect("build compute residency");

        assert_eq!(residency.compute_shaders.len(), 1);
        let shader = &residency.compute_shaders[0];
        assert_eq!(shader.workgroup_size, Some([8, 1, 1]));
        assert_eq!(shader.dispatch_size, Some([8, 1, 1]));

        let src_binding = shader
            .bindings
            .iter()
            .find(|binding| binding.key == "src")
            .expect("src binding");
        assert_eq!(src_binding.shape, vec![8]);
        assert_eq!(src_binding.access_mode, "read");

        let dst_binding = shader
            .bindings
            .iter()
            .find(|binding| binding.key == "dst")
            .expect("dst binding");
        assert_eq!(dst_binding.shape, vec![8]);
        assert_eq!(dst_binding.access_mode, "write");
    }

    #[test]
    fn gpu_compute_convergence_fixture_executes_python_kain_gpu_node() {
        let fixture_dir = tempfile::tempdir().expect("temp gpu fabric fixture");
        let fixture_root = fixture_dir.path();
        copy_fixture(
            Path::new(env!("CARGO_MANIFEST_DIR"))
                .parent()
                .unwrap()
                .parent()
                .unwrap()
                .join("smoketest")
                .join("fabric")
                .join("gpu_compute_convergence")
                .as_path(),
            fixture_root,
        );

        let result = execute_fabric_manifest_path(&fixture_root.join("KAIN.fabric.toml")).unwrap();

        assert_eq!(
            result.status,
            FabricSessionStatus::Succeeded,
            "{}",
            fs::read_to_string(&result.report_path).unwrap_or_else(|_| {
                format!(
                    "failed to read Fabric report at {}",
                    result.report_path.display()
                )
            })
        );
        assert_eq!(result.step_results.len(), 4);
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
            .any(|step| step.runtime == FabricRuntimeKind::GpuCompute));
        assert!(result
            .step_results
            .iter()
            .any(|step| step.runtime == FabricRuntimeKind::Node));

        let node_step = result
            .step_results
            .iter()
            .find(|step| step.id == "node_packager")
            .expect("node step");
        let output = node_step.outputs.first().expect("node output");
        let FabricOutputPayloadSnapshot::Value { value } = &output.payload else {
            panic!("expected node summary value");
        };
        assert_eq!(
            value.json.as_ref().and_then(|json| json.as_str()),
            Some("gpu-fabric-convergence:1:1:7|gpu=1,2,3,4,0,0,0,0|bytes=32")
        );
    }

    #[test]
    fn polyglot_fixture_executes_all_runtime_kinds() {
        let fixture_root = stable_fabric_test_root("fab-smoke");
        prepare_fabric_test_workspace(&fixture_root);
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

        assert_eq!(
            result.status,
            FabricSessionStatus::Succeeded,
            "{}",
            fs::read_to_string(&result.report_path).unwrap_or_else(|_| {
                format!(
                    "failed to read Fabric report at {}",
                    result.report_path.display()
                )
            })
        );
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

    fn stable_fabric_test_root(label: &str) -> PathBuf {
        Path::new(env!("CARGO_MANIFEST_DIR"))
            .parent()
            .unwrap()
            .parent()
            .unwrap()
            .join("target")
            .join(label)
    }

    fn prepare_fabric_test_workspace(root: &Path) {
        fs::create_dir_all(root).expect("create fabric test root");
        clear_directory_except(root, Some(".kain"));
        let kain_dir = root.join(".kain");
        if kain_dir.exists() {
            clear_directory_except(&kain_dir, Some("cache"));
        }
    }

    fn clear_directory_except(root: &Path, preserved_name: Option<&str>) {
        for entry in fs::read_dir(root).expect("read test workspace") {
            let entry = entry.expect("workspace entry");
            let path = entry.path();
            if preserved_name
                .map(|name| entry.file_name().to_string_lossy() == name)
                .unwrap_or(false)
            {
                continue;
            }
            let entry_type = entry.file_type().expect("workspace entry type");
            if entry_type.is_dir() {
                fs::remove_dir_all(path).expect("remove workspace directory");
            } else {
                fs::remove_file(path).expect("remove workspace file");
            }
        }
    }

    fn assert_missing_output_failure(runtime: FabricRuntimeKind, entry: &str, source: &str) {
        let dir = tempfile::tempdir().expect("temp dir");
        let entry_path = dir.path().join(entry);
        fs::create_dir_all(entry_path.parent().expect("entry parent"))
            .expect("create entry parent");
        fs::write(&entry_path, source).expect("write step source");

        let manifest_path = dir.path().join("KAIN.fabric.toml");
        fs::write(
            &manifest_path,
            format!(
                "version = 1\n\n[workspace]\nroot = \".\"\nsearch_roots = [\"scripts\"]\n\n[[requires]]\nkey = \"session.local\"\nversion = 1\noptional = false\n\n[[steps]]\nid = \"bridge_step\"\nruntime = \"{}\"\nentry = \"{}\"\n\n[[steps.outputs]]\nname = \"report\"\nkind = \"value\"\n\n[[steps.outputs]]\nname = \"missing\"\nkind = \"value\"\n",
                runtime.display_name(),
                entry.replace('\\', "/"),
            ),
        )
        .expect("write manifest");

        let result = execute_fabric_manifest_path(&manifest_path).expect("execute manifest");
        assert_eq!(result.status, FabricSessionStatus::Failed);
        let step = result.step_results.first().expect("step result");
        assert_eq!(step.status, FabricStepStatus::Failed);
        let error = step.error.as_ref().expect("missing step failure");
        assert_eq!(error.code, "missing_output_field");
        assert!(error.message.contains("bridge_step"));
        assert!(error.message.contains("missing"));
        let details = error.details.as_ref().expect("failure details");
        assert_eq!(
            details.get("output_name").and_then(JsonValue::as_str),
            Some("missing")
        );
        assert_eq!(
            details.get("runtime").and_then(JsonValue::as_str),
            Some(runtime.display_name())
        );
    }

    fn assert_runtime_consumes_canonical_shared_inputs(
        runtime: FabricRuntimeKind,
        entry: &str,
        source: &str,
        expected_report: &str,
    ) {
        let dir = tempfile::tempdir().expect("temp dir");
        let seed_path = dir.path().join("src/seed.kn");
        fs::create_dir_all(seed_path.parent().expect("seed parent")).expect("create seed parent");
        fs::write(
            &seed_path,
            "use std::interop::bridge\n\nstruct SeedOutputs:\n    image: Any\n    snapshot: Any\n\nfn main() -> SeedOutputs:\n    let image = interop_shared_image_from_bytes([1, 2, 3, 4], 1, 1, 4, \"HWC\", \"rgba8\", \"image/x-kain-raster\")\n    let snapshot = interop_shared_buffer_from_bytes([10, 20, 30, 40], \"u8\", [4], \"bytes\", \"application/octet-stream\")\n    return SeedOutputs { image: image, snapshot: snapshot }\n",
        )
        .expect("write seed source");

        let entry_path = dir.path().join(entry);
        fs::create_dir_all(entry_path.parent().expect("entry parent"))
            .expect("create entry parent");
        fs::write(&entry_path, source).expect("write consumer source");

        let manifest_path = dir.path().join("KAIN.fabric.toml");
        fs::write(
            &manifest_path,
            format!(
                "version = 1\n\n[workspace]\nroot = \".\"\nsearch_roots = [\"src\", \"scripts\"]\n\n[[requires]]\nkey = \"session.local\"\nversion = 1\noptional = false\n\n[[steps]]\nid = \"seed\"\nruntime = \"kain\"\nentry = \"src/seed.kn\"\n\n[[steps.outputs]]\nname = \"image\"\nkind = \"shared_image\"\n\n[[steps.outputs]]\nname = \"snapshot\"\nkind = \"shared_buffer\"\n\n[[steps]]\nid = \"consumer\"\nruntime = \"{}\"\nentry = \"{}\"\ndepends_on = [\"seed\"]\n\n[[steps.outputs]]\nname = \"report\"\nkind = \"value\"\n",
                runtime.display_name(),
                entry.replace('\\', "/"),
            ),
        )
        .expect("write manifest");

        let result = execute_fabric_manifest_path(&manifest_path).expect("execute manifest");
        assert_eq!(result.status, FabricSessionStatus::Succeeded);
        let consumer_step = result
            .step_results
            .iter()
            .find(|step| step.id == "consumer")
            .expect("consumer step");
        let output = consumer_step.outputs.first().expect("consumer output");
        let FabricOutputPayloadSnapshot::Value { value } = &output.payload else {
            panic!("expected consumer value output");
        };
        assert_eq!(
            value.json.as_ref().and_then(|json| json.as_str()),
            Some(expected_report)
        );
    }
}
