//! Target-aware Kain source checking.
//!
//! `kain-check` is the reusable library behind `kain check`. It owns source
//! discovery, frontend validation, and structured reports so CLI, CI, and future
//! IDE/test harnesses do not need to duplicate checking logic.

use kain_core::{emit_runtime_contract_bundle, CompileTarget, TypedItem, TypedProgram};
use kain_driver::{
    DriverSession, ToolingProgressEvent, ToolingProgressSink, ToolingProgressStatus,
};
use kain_fs as kfs;
use serde::{Deserialize, Serialize};
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CheckOptions {
    pub target: CompileTargetName,
    pub fail_fast: bool,
    #[serde(skip)]
    pub progress: Option<ToolingProgressSink>,
}

impl CheckOptions {
    pub fn new(target: CompileTarget) -> Self {
        Self {
            target: CompileTargetName::from(target),
            fail_fast: false,
            progress: None,
        }
    }

    pub fn target(&self) -> CompileTarget {
        self.target.0
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

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct CheckFileReport {
    pub path: String,
    pub target: String,
    pub status: CheckStatus,
    pub item_count: usize,
    pub test_count: usize,
    pub required_capabilities: Vec<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub error: Option<String>,
}

impl CheckFileReport {
    pub fn passed(&self) -> bool {
        matches!(self.status, CheckStatus::Passed)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
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
            let bundle = emit_runtime_contract_bundle(&checked.typed, target);
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
            }
        }
        Err(error) => CheckFileReport {
            path: source_name.to_string(),
            target: compile_target_name(target).to_string(),
            status: CheckStatus::Failed,
            item_count: 0,
            test_count: 0,
            required_capabilities: Vec::new(),
            error: Some(session.format_error(source_name, source, &error)),
        },
    }
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
        CompileTarget::Rust => "rust",
        CompileTarget::Cpp => "cpp",
        CompileTarget::Ue5 => "ue5",
        CompileTarget::Ue5Editor => "ue5-editor",
        CompileTarget::Usf => "usf",
        CompileTarget::Spirv => "spirv",
        CompileTarget::Hlsl => "hlsl",
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
