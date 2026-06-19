//! Target-aware Kain source checking.
//!
//! `kain-check` is the reusable library behind `kain check`. It owns source
//! discovery, frontend validation, and structured reports so CLI, CI, and future
//! IDE/test harnesses do not need to duplicate checking logic.

use kain_core::{emit_runtime_contract_bundle, CompileTarget, TypedItem, TypedProgram};
use kain_driver::{
    DriverSession, ToolingProgressEvent, ToolingProgressSink, ToolingProgressStatus,
};
use kain_error::{diagnostics_to_json_string, KainError};
use kain_fs as kfs;
use serde::{Deserialize, Serialize};
mod audit;
mod telemetry;
mod validate;
mod validate_semantic_contracts;

use std::collections::HashMap;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CheckOptions {
    pub target: CompileTargetName,
    pub fail_fast: bool,
    /// Run ALL validators including expensive/speculative ones.
    /// Default: false. Pass --pedantic on the CLI to enable.
    #[serde(default)]
    pub pedantic: bool,
    #[serde(skip)]
    pub progress: Option<ToolingProgressSink>,
}

impl CheckOptions {
    pub fn new(target: CompileTarget) -> Self {
        Self {
            target: CompileTargetName::from(target),
            fail_fast: false,
            pedantic: false,
            progress: None,
        }
    }

    pub fn target(&self) -> CompileTarget {
        self.target.0
    }

    /// Builder: enable pedantic mode.
    pub fn with_pedantic(mut self, pedantic: bool) -> Self {
        self.pedantic = pedantic;
        self
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct CompileTargetName(pub CompileTarget);

impl From<CompileTarget> for CompileTargetName {
    fn from(value: CompileTarget) -> Self {
        Self(value)
    }
}

impl Serialize for CompileTargetName {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.serialize_str(compile_target_name(self.0))
    }
}

impl<'de> Deserialize<'de> for CompileTargetName {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let value = String::deserialize(deserializer)?;
        kain_driver::parse_compile_target(&value)
            .map(Self)
            .ok_or_else(|| serde::de::Error::custom(format!("unknown Kain target '{value}'")))
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum CheckStatus {
    Passed,
    Failed,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct CheckFileReport {
    pub path: String,
    pub target: String,
    pub status: CheckStatus,
    pub item_count: usize,
    pub test_count: usize,
    pub required_capabilities: Vec<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub error: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub diagnostic: Option<serde_json::Value>,
    /// Full `diagnostics_to_json()` envelope ({"diagnostics":[...], "summary":{...}}).
    /// Populated for CLI --json / --json-out paths so consumers get the rich,
    /// structured diagnostic payload rather than the summary-level `CheckReport`
    /// serialization. When `None`, the caller should fall back to the structured
    /// report produced by `emit_structured_report` in the CLI layer.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub diagnostics_json: Option<String>,

    // ── Telemetry fields (ETA-B) ─────────────────────────────────
    /// Confidence score (0.0-1.0) that this file would pass `kain build`.
    /// Based on which validators ran and category coverage.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub confidence_score: Option<f64>,

    /// Count of validators that executed during this check.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub validator_count: Option<usize>,

    /// Count of validators that were skipped (target mismatch, not yet implemented).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub validators_skipped: Option<usize>,

    /// Human-readable gap report: what errors might still occur at build time.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub gap_summary: Option<String>,

    /// Validator categories still missing (not covered by any ran validator).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub missing_categories: Option<Vec<String>>,

    /// Whether the check ran in pedantic mode (extra validators).
    #[serde(skip_serializing_if = "Option::is_none")]
    pub pedantic: Option<bool>,
}

impl CheckFileReport {
    pub fn passed(&self) -> bool {
        matches!(self.status, CheckStatus::Passed)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct CheckReport {
    pub target: String,
    pub total: usize,
    pub passed: usize,
    pub failed: usize,
    pub files: Vec<CheckFileReport>,
}

impl CheckReport {
    pub fn is_success(&self) -> bool {
        self.failed == 0
    }
}

pub fn check_source(source_name: &str, source: &str, options: &CheckOptions) -> CheckFileReport {
    let session = DriverSession::default();
    let source_path = PathBuf::from(source_name);
    emit_progress(
        options,
        ToolingProgressEvent::CheckFileStarted {
            current: 1,
            total: 1,
            path: source_path.clone(),
            target: compile_target_name(options.target()).to_string(),
        },
    );
    let report = check_source_with_session(&session, source_name, None, source, options);
    emit_progress(
        options,
        ToolingProgressEvent::CheckFileFinished {
            current: 1,
            total: 1,
            path: source_path,
            target: compile_target_name(options.target()).to_string(),
            status: tooling_status_for_report(&report),
            error: report.error.clone(),
        },
    );
    report
}

pub fn check_source_with_session(
    session: &DriverSession,
    source_name: &str,
    source_path: Option<&Path>,
    source: &str,
    options: &CheckOptions,
) -> CheckFileReport {
    let target = options.target();
    match session.frontend_to_checked_program_with_source_path_and_progress(
        source,
        source_path,
        target,
        options.progress.as_ref(),
    ) {
        Ok(checked) => {
            // ── Monomorphization validation ──────────────────────────────
            // Run monomorphization to catch generic instantiation errors,
            // trait bound violations (where T: Debug unsatisfied), and
            // async lowering failures at check time instead of build time.
            // This does NOT run LLVM codegen — only the monomorphization
            // pass which resolves concrete types and validates trait impls.
            if let Err(mono_error) = kain_core::monomorphize::monomorphize(&checked.typed) {
                let diagnostic = mono_error.diagnostic_json();
                let diagnostics_json = Some(diagnostics_to_json_string(&mono_error.to_diagnostic_reports()));
                return attach_telemetry(
                    CheckFileReport {
                        path: source_name.to_string(),
                        target: compile_target_name(target).to_string(),
                        status: CheckStatus::Failed,
                        item_count: 0,
                        test_count: 0,
                        required_capabilities: Vec::new(),
                        error: Some(session.format_error(source_name, source, &mono_error)),
                        diagnostic,
                        diagnostics_json,
                        confidence_score: None,
                        validator_count: None,
                        validators_skipped: None,
                        gap_summary: None,
                        missing_categories: None,
                        pedantic: None,
                    },
                    options,
                );
            }

            // ── Semantic validation pass ─────────────────────────────────
            // Runs proactive checks for invariants the typechecker doesn't
            // enforce but the codegen/runtime requires.
            let mut semantic_errors =
                validate::validate_semantic_stack(&checked.typed);

            // ── Semantic contract validation pass ────────────────────
            // Validates decision-ladder contracts: actor message completeness,
            // resonate anti-feedback, entangle completeness, converge lane
            // coverage, orchestrate liveness, pulse conflicts, teleport type
            // match, patch binding, law satisfiability, dead state detection.
            // These catch categories of logic errors that neither check
            // nor build currently detect.
            validate_semantic_contracts::validate_semantic_contracts(
                &checked.typed,
                &mut semantic_errors,
            );

            if let Some(first) = semantic_errors.first() {
                let error = KainError::rich(first.clone());
                let diagnostic = error.diagnostic_json();
                let diagnostics_json = Some(diagnostics_to_json_string(&error.to_diagnostic_reports()));
                return attach_telemetry(
                    CheckFileReport {
                        path: source_name.to_string(),
                        target: compile_target_name(target).to_string(),
                        status: CheckStatus::Failed,
                        item_count: 0,
                        test_count: 0,
                        required_capabilities: Vec::new(),
                        error: Some(session.format_error(source_name, source, &error)),
                        diagnostic,
                        diagnostics_json,
                        confidence_score: None,
                        validator_count: None,
                        validators_skipped: None,
                        gap_summary: None,
                        missing_categories: None,
                        pedantic: None,
                    },
                    options,
                );
            }

            let bundle = emit_runtime_contract_bundle(&checked.typed, target);
            attach_telemetry(
                CheckFileReport {
                    path: source_name.to_string(),
                    target: compile_target_name(target).to_string(),
                    status: CheckStatus::Passed,
                    item_count: count_typed_items(&checked.typed),
                    test_count: count_typed_tests(&checked.typed),
                    required_capabilities: bundle
                        .required_capabilities
                        .into_iter()
                        .map(|capability| capability.key)
                        .collect(),
                    error: None,
                    diagnostic: None,
                    diagnostics_json: None,
                    confidence_score: None,
                    validator_count: None,
                    validators_skipped: None,
                    gap_summary: None,
                    missing_categories: None,
                    pedantic: None,
                },
                options,
            )
        }
        Err(error) => {
            let diagnostic = error.diagnostic_json();
            let diagnostics_json = Some(diagnostics_to_json_string(&error.to_diagnostic_reports()));
            attach_telemetry(
                CheckFileReport {
                    path: source_name.to_string(),
                    target: compile_target_name(target).to_string(),
                    status: CheckStatus::Failed,
                    item_count: 0,
                    test_count: 0,
                    required_capabilities: Vec::new(),
                    error: Some(session.format_error(source_name, source, &error)),
                    diagnostic,
                    diagnostics_json,
                    confidence_score: None,
                    validator_count: None,
                    validators_skipped: None,
                    gap_summary: None,
                    missing_categories: None,
                    pedantic: None,
                },
                options,
            )
        }
    }
}

/// Compute telemetry for a check run and attach it to the report.
///
/// The "always-on" validator list mirrors what `check_source_with_session`
/// actually executes. Stream ALPHA / BRAVO / ETA-A validators are not yet
/// wired into the pipeline, so they appear as skipped; pedantic mode only
/// changes the skip messaging, not the wiring.
fn attach_telemetry(mut report: CheckFileReport, options: &CheckOptions) -> CheckFileReport {
    let ran_validators: Vec<&str> = vec![
        "typechecker",
        "validate_semantic_stack",
        "validate_reply_ports",
        "validate_converge_contracts",
    ];
    let errors_per_validator: HashMap<String, usize> = HashMap::new();
    let telemetry = telemetry::compute_telemetry(
        &ran_validators,
        &errors_per_validator,
        options.pedantic,
    );
    report.confidence_score = Some(telemetry.confidence);
    report.validator_count = Some(telemetry.validators_ran);
    report.validators_skipped = Some(telemetry.validators_skipped);
    report.gap_summary = Some(telemetry.gap_summary);
    report.missing_categories = if telemetry.missing_categories.is_empty() {
        None
    } else {
        Some(telemetry.missing_categories)
    };
    report.pedantic = Some(options.pedantic);
    report
}

pub fn check_file(path: &Path, options: &CheckOptions) -> CheckFileReport {
    let session = DriverSession::default();
    check_file_with_session(&session, path, options)
}

pub fn check_file_with_session(
    session: &DriverSession,
    path: &Path,
    options: &CheckOptions,
) -> CheckFileReport {
    emit_progress(
        options,
        ToolingProgressEvent::CheckFileStarted {
            current: 1,
            total: 1,
            path: path.to_path_buf(),
            target: compile_target_name(options.target()).to_string(),
        },
    );
    let report = check_file_with_session_raw(session, path, options);
    emit_progress(
        options,
        ToolingProgressEvent::CheckFileFinished {
            current: 1,
            total: 1,
            path: path.to_path_buf(),
            target: compile_target_name(options.target()).to_string(),
            status: tooling_status_for_report(&report),
            error: report.error.clone(),
        },
    );
    report
}

fn check_file_with_session_raw(
    session: &DriverSession,
    path: &Path,
    options: &CheckOptions,
) -> CheckFileReport {
    match kfs::read_text(path) {
        Ok(source) => check_source_with_session(
            session,
            &path.display().to_string(),
            Some(path),
            &source,
            options,
        ),
        Err(error) => CheckFileReport {
            path: path.display().to_string(),
            target: compile_target_name(options.target()).to_string(),
            status: CheckStatus::Failed,
            item_count: 0,
            test_count: 0,
            required_capabilities: Vec::new(),
            error: Some(format!("failed to read source: {error}")),
            diagnostic: None,
            confidence_score: None,
            validator_count: None,
            validators_skipped: None,
            gap_summary: None,
            missing_categories: None,
            pedantic: None,
        },
    }
}

pub fn check_path(path: &Path, options: &CheckOptions) -> CheckReport {
    emit_progress(
        options,
        ToolingProgressEvent::CheckDiscoveryStarted {
            root: path.to_path_buf(),
            target: compile_target_name(options.target()).to_string(),
        },
    );
    let files = match discover_kain_files(path) {
        Ok(files) => files,
        Err(error) => {
            return CheckReport {
                target: compile_target_name(options.target()).to_string(),
                total: 1,
                passed: 0,
                failed: 1,
                files: vec![CheckFileReport {
                    path: path.display().to_string(),
                    target: compile_target_name(options.target()).to_string(),
                    status: CheckStatus::Failed,
                    item_count: 0,
                    test_count: 0,
                    required_capabilities: Vec::new(),
                    error: Some(error),
                    diagnostic: None,
                    confidence_score: None,
                    validator_count: None,
                    validators_skipped: None,
                    gap_summary: None,
                    missing_categories: None,
                    pedantic: None,
                }],
            };
        }
    };
    emit_progress(
        options,
        ToolingProgressEvent::CheckDiscoveryFinished {
            root: path.to_path_buf(),
            target: compile_target_name(options.target()).to_string(),
            total_files: files.len(),
        },
    );

    let mut reports = Vec::new();
    let session = DriverSession::default();
    let total_files = files.len();
    for (index, file) in files.into_iter().enumerate() {
        emit_progress(
            options,
            ToolingProgressEvent::CheckFileStarted {
                current: index + 1,
                total: total_files,
                path: file.clone(),
                target: compile_target_name(options.target()).to_string(),
            },
        );
        let report = check_file_with_session_raw(&session, &file, options);
        let failed = !report.passed();
        emit_progress(
            options,
            ToolingProgressEvent::CheckFileFinished {
                current: index + 1,
                total: total_files,
                path: file,
                target: compile_target_name(options.target()).to_string(),
                status: tooling_status_for_report(&report),
                error: report.error.clone(),
            },
        );
        reports.push(report);
        if failed && options.fail_fast {
            break;
        }
    }

    summarize_reports(compile_target_name(options.target()), reports)
}

pub fn discover_kain_files(path: &Path) -> Result<Vec<PathBuf>, String> {
    if path.is_file() {
        if is_kain_source_file(path) {
            return Ok(vec![path.to_path_buf()]);
        }
        return Err(format!("{} is not a .kn or .ks file", path.display()));
    }

    if !path.exists() {
        return Err(format!("{} does not exist", path.display()));
    }

    let mut output = Vec::new();
    discover_kain_files_into(path, &mut output)?;
    output.sort();
    Ok(output)
}

pub fn count_typed_items(program: &TypedProgram) -> usize {
    count_typed_items_in_slice(&program.items)
}

pub fn count_typed_tests(program: &TypedProgram) -> usize {
    count_typed_tests_in_slice(&program.items)
}

pub fn compile_target_name(target: CompileTarget) -> &'static str {
    match target {
        CompileTarget::Wasm => "wasm",
        CompileTarget::Js => "js",
        CompileTarget::Ts => "ts",
        CompileTarget::Hybrid => "hybrid",
        CompileTarget::C => "c",
        CompileTarget::Llvm => "llvm",
        CompileTarget::BareMetal => "baremetal",
        CompileTarget::Rust => "rust",
        CompileTarget::Cpp => "cpp",
        CompileTarget::Ue5 => "ue5",
        CompileTarget::Ue5Editor => "ue5-editor",
        CompileTarget::Usf => "usf",
        CompileTarget::Spirv => "spirv",
        CompileTarget::Hlsl => "hlsl",
        CompileTarget::Wgsl => "wgsl",
        CompileTarget::Cuda => "cuda",
        CompileTarget::Interpret => "run",
        CompileTarget::Test => "test",
        CompileTarget::Ks => "ks",
    }
}

fn tooling_status_for_report(report: &CheckFileReport) -> ToolingProgressStatus {
    if report.passed() {
        ToolingProgressStatus::Succeeded
    } else {
        ToolingProgressStatus::Failed
    }
}

fn emit_progress(options: &CheckOptions, event: ToolingProgressEvent) {
    if let Some(progress) = options.progress.as_ref() {
        progress.emit(&event);
    }
}

fn summarize_reports(target: &str, files: Vec<CheckFileReport>) -> CheckReport {
    let passed = files.iter().filter(|report| report.passed()).count();
    let failed = files.len().saturating_sub(passed);
    CheckReport {
        target: target.to_string(),
        total: files.len(),
        passed,
        failed,
        files,
    }
}

fn discover_kain_files_into(path: &Path, output: &mut Vec<PathBuf>) -> Result<(), String> {
    if should_skip_directory(path) {
        return Ok(());
    }

    for entry in
        kfs::read_dir_entries(path).map_err(|error| format!("failed to read dir: {error}"))?
    {
        let path = entry.path;
        if path.is_dir() {
            discover_kain_files_into(&path, output)?;
        } else if is_kain_source_file(&path) {
            output.push(path);
        }
    }

    Ok(())
}

fn should_skip_directory(path: &Path) -> bool {
    let Some(name) = path.file_name().and_then(|value| value.to_str()) else {
        return false;
    };
    matches!(
        name,
        ".git"
            | ".hg"
            | ".svn"
            | ".kain"
            | "target"
            | "node_modules"
            | "dist"
            | "build"
            | "generated"
            | ".selfhost"
    )
}

fn is_kain_source_file(path: &Path) -> bool {
    matches!(
        path.extension().and_then(|value| value.to_str()),
        Some("kn" | "ks")
    )
}

fn count_typed_items_in_slice(items: &[TypedItem]) -> usize {
    items
        .iter()
        .map(|item| {
            1 + match item {
                TypedItem::Mod(module) => count_typed_items_in_slice(&module.items),
                _ => 0,
            }
        })
        .sum()
}

fn count_typed_tests_in_slice(items: &[TypedItem]) -> usize {
    items
        .iter()
        .map(|item| match item {
            TypedItem::Test(_) => 1,
            TypedItem::Mod(module) => count_typed_tests_in_slice(&module.items),
            _ => 0,
        })
        .sum()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
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
    fn check_source_reports_item_and_capability_summary() {
        let report = check_source(
            "<test>",
            "fn main() -> Int:\n    return 1\n",
            &CheckOptions::new(CompileTarget::Interpret),
        );

        assert!(report.passed());
        assert!(report.item_count >= 1);
        assert!(report
            .required_capabilities
            .contains(&"compiler.typed-program".to_string()));
    }

    #[test]
    fn check_source_reports_failures_without_panicking() {
        let report = check_source(
            "<test>",
            "fn main( -> Int:\n",
            &CheckOptions::new(CompileTarget::Interpret),
        );

        assert!(!report.passed());
        assert!(report.error.is_some());
        assert!(
            report
                .diagnostic
                .as_ref()
                .and_then(|json| json.get("diagnostics"))
                .and_then(|diagnostics| diagnostics.as_array())
                .is_some_and(|diagnostics| !diagnostics.is_empty()),
            "failed checks should retain structured diagnostic JSON"
        );
    }

    #[test]
    fn check_source_accumulates_multiple_same_file_diagnostics() {
        let report = check_source(
            "<test>",
            "let first: Int = \"hello\"\nlet second = missing_top + 1\nlet third: Bool = 123\n",
            &CheckOptions::new(CompileTarget::Llvm),
        );

        assert!(!report.passed());
        let diagnostics = report
            .diagnostic
            .as_ref()
            .and_then(|json| json.get("diagnostics"))
            .and_then(|diagnostics| diagnostics.as_array())
            .expect("failed checks should retain diagnostics array");
        assert!(
            diagnostics.len() >= 3,
            "expected at least 3 diagnostics, got {}: {:?}",
            diagnostics.len(),
            diagnostics
        );
    }

    #[test]
    fn check_source_allows_std_python_root_in_llvm_frontend_checks() {
        let report = check_source(
            "<test>",
            "use std::python\n\nfn main() -> Int:\n    python_exec(\"value = 1\")\n    return 0\n",
            &CheckOptions::new(CompileTarget::Llvm),
        );

        assert!(
            report.passed(),
            "llvm frontend checks should see std::python globals: {:?}",
            report.error
        );
    }

    #[test]
    fn check_source_allows_std_interop_root_in_llvm_frontend_checks() {
        let report = check_source(
            "<test>",
            "use std::interop\n\nfn main() -> Int:\n    let payload = interop_shared_buffer_from_bytes([1, 2, 3, 4], \"u8\", [4], \"bytes\", \"application/octet-stream\")\n    return interop_shared_buffer_info(payload).byte_length\n",
            &CheckOptions::new(CompileTarget::Llvm),
        );

        assert!(
            report.passed(),
            "llvm frontend checks should see std::interop globals: {:?}",
            report.error
        );
    }

    #[test]
    fn discover_kain_files_skips_generated_directories() {
        let temp = tempfile::tempdir().expect("temp dir");
        kfs::write_text(
            temp.path().join("root.kn"),
            "fn main() -> Int:\n    return 0\n",
        )
        .expect("root source");
        kfs::create_dir_all(temp.path().join("generated")).expect("generated dir");
        kfs::write_text(temp.path().join("generated").join("skip.kn"), "")
            .expect("generated source");
        kfs::create_dir_all(temp.path().join(".kain").join("cache")).expect("workspace cache dir");
        kfs::write_text(temp.path().join(".kain").join("cache").join("skip.kn"), "")
            .expect("workspace cache source");

        let files = discover_kain_files(temp.path()).expect("discover files");
        assert_eq!(files, vec![temp.path().join("root.kn")]);
    }

    #[test]
    fn check_file_resolves_imports_relative_to_source_path() {
        let temp = tempfile::tempdir().expect("tempdir");
        let main_path = temp.path().join("main.kn");
        let module_dir = temp.path().join("src");
        let module_path = module_dir.join("module_probe.kn");
        fs::create_dir_all(&module_dir).expect("module dir");
        fs::write(
            &main_path,
            r#"
use module_probe::four

fn main() -> Int:
    return four()
"#,
        )
        .expect("main source");
        fs::write(
            &module_path,
            r#"
pub fn four() -> Int:
    return 4
"#,
        )
        .expect("module source");

        let report = check_file(&main_path, &CheckOptions::new(CompileTarget::Llvm));
        assert!(
            report.passed(),
            "expected importer-relative check to pass: {:?}",
            report.error
        );
        assert!(report.item_count >= 1);
    }

    #[test]
    fn check_path_emits_discovery_and_file_progress() {
        let temp = tempfile::tempdir().expect("tempdir");
        kfs::write_text(
            temp.path().join("alpha.kn"),
            "fn main() -> Int:\n    return 0\n",
        )
        .expect("alpha source");
        kfs::write_text(
            temp.path().join("beta.kn"),
            "fn main() -> Int:\n    return 1\n",
        )
        .expect("beta source");
        let (events, sink) = capture_progress();
        let mut options = CheckOptions::new(CompileTarget::Interpret);
        options.progress = Some(sink);

        let report = check_path(temp.path(), &options);

        assert!(report.is_success());
        let events = events.lock().expect("lock events");
        assert!(matches!(
            events.first(),
            Some(ToolingProgressEvent::CheckDiscoveryStarted { .. })
        ));
        assert!(events.iter().any(|event| matches!(
            event,
            ToolingProgressEvent::CheckDiscoveryFinished { total_files: 2, .. }
        )));
        assert_eq!(
            events
                .iter()
                .filter(|event| matches!(event, ToolingProgressEvent::CheckFileStarted { .. }))
                .count(),
            2
        );
        assert_eq!(
            events
                .iter()
                .filter(|event| matches!(event, ToolingProgressEvent::CheckFileFinished { .. }))
                .count(),
            2
        );
    }
}
