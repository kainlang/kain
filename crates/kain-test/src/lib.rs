//! Rust-inspired test harness for Kain source suites.
//!
//! This crate is intentionally separate from the CLI. It owns directives,
//! pass/fail modes, structured reports, and suite discovery so Kain can grow a
//! compiletest-style pipeline without turning `crates/cli` into the harness.

use kain_check::{check_source, compile_target_name, discover_kain_files, CheckOptions};
use kain_core::{runtime, CompileTarget};
use kain_driver::DriverSession;
use kain_fs as kfs;
use serde::{Deserialize, Serialize};
use std::path::Path;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "kebab-case")]
pub enum KainTestMode {
    CheckPass,
    CheckFail,
    RunPass,
    RunFail,
    KainTest,
}

impl KainTestMode {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::CheckPass => "check-pass",
            Self::CheckFail => "check-fail",
            Self::RunPass => "run-pass",
            Self::RunFail => "run-fail",
            Self::KainTest => "kain-test",
        }
    }

    pub fn parse(value: &str) -> Option<Self> {
        match value.trim() {
            "check" | "check-pass" => Some(Self::CheckPass),
            "check-fail" | "compile-fail" => Some(Self::CheckFail),
            "run" | "run-pass" => Some(Self::RunPass),
            "run-fail" => Some(Self::RunFail),
            "test" | "kain-test" => Some(Self::KainTest),
            _ => None,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct KainTestOptions {
    pub default_target: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub mode_override: Option<KainTestMode>,
    pub fail_fast: bool,
    pub run_ignored: bool,
}

impl KainTestOptions {
    pub fn new(default_target: CompileTarget) -> Self {
        Self {
            default_target: compile_target_name(default_target).to_string(),
            mode_override: None,
            fail_fast: false,
            run_ignored: false,
        }
    }

    pub fn default_target(&self) -> CompileTarget {
        kain_driver::parse_compile_target(&self.default_target).unwrap_or(CompileTarget::Interpret)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum KainTestStatus {
    Passed,
    Failed,
    Skipped,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct KainTestCaseReport {
    pub path: String,
    pub mode: String,
    pub target: String,
    pub status: KainTestStatus,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub expected_error: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub skip_reason: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub error: Option<String>,
}

impl KainTestCaseReport {
    pub fn passed(&self) -> bool {
        matches!(self.status, KainTestStatus::Passed)
    }

    pub fn skipped(&self) -> bool {
        matches!(self.status, KainTestStatus::Skipped)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct KainTestSuiteReport {
    pub total: usize,
    pub passed: usize,
    pub skipped: usize,
    pub failed: usize,
    pub cases: Vec<KainTestCaseReport>,
}

impl KainTestSuiteReport {
    pub fn is_success(&self) -> bool {
        self.failed == 0
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
struct TestDirectives {
    mode: Option<KainTestMode>,
    target: Option<CompileTarget>,
    expected_error: Option<String>,
    skip_reason: Option<String>,
}

pub fn run_path(path: &Path, options: &KainTestOptions) -> KainTestSuiteReport {
    let files = match discover_kain_files(path) {
        Ok(files) => files,
        Err(error) => {
            return KainTestSuiteReport {
                total: 1,
                passed: 0,
                skipped: 0,
                failed: 1,
                cases: vec![KainTestCaseReport {
                    path: path.display().to_string(),
                    mode: options
                        .mode_override
                        .map(KainTestMode::as_str)
                        .unwrap_or("auto")
                        .to_string(),
                    target: compile_target_name(options.default_target()).to_string(),
                    status: KainTestStatus::Failed,
                    expected_error: None,
                    skip_reason: None,
                    error: Some(error),
                }],
            };
        }
    };

    let mut cases = Vec::new();
    for file in files {
        let case = run_file(&file, options);
        let failed = !case.passed();
        cases.push(case);
        if failed && options.fail_fast {
            break;
        }
    }

    summarize_cases(cases)
}

pub fn run_file(path: &Path, options: &KainTestOptions) -> KainTestCaseReport {
    match kfs::read_text(path) {
        Ok(source) => run_source(&path.display().to_string(), &source, options),
        Err(error) => KainTestCaseReport {
            path: path.display().to_string(),
            mode: options
                .mode_override
                .map(KainTestMode::as_str)
                .unwrap_or("auto")
                .to_string(),
            target: compile_target_name(options.default_target()).to_string(),
            status: KainTestStatus::Failed,
            expected_error: None,
            skip_reason: None,
            error: Some(format!("failed to read source: {error}")),
        },
    }
}

pub fn run_source(
    source_name: &str,
    source: &str,
    options: &KainTestOptions,
) -> KainTestCaseReport {
    let directives = parse_directives(source);
    let mode = options
        .mode_override
        .or(directives.mode)
        .unwrap_or_else(|| infer_default_mode(source));
    let target = directives
        .target
        .unwrap_or_else(|| options.default_target());
    let report_target = report_target_for_mode(mode, target);
    let expected_error = directives.expected_error.clone();

    if let Some(skip_reason) = directives.skip_reason.clone() {
        if !options.run_ignored {
            return KainTestCaseReport {
                path: source_name.to_string(),
                mode: mode.as_str().to_string(),
                target: compile_target_name(report_target).to_string(),
                status: KainTestStatus::Skipped,
                expected_error,
                skip_reason: Some(skip_reason),
                error: None,
            };
        }
    }

    let result = match mode {
        KainTestMode::CheckPass => run_check_pass(source_name, source, target),
        KainTestMode::CheckFail => {
            run_check_fail(source_name, source, target, expected_error.as_deref())
        }
        KainTestMode::RunPass => run_interpret_expect_pass(source),
        KainTestMode::RunFail => run_interpret_expect_fail(source, expected_error.as_deref()),
        KainTestMode::KainTest => run_kain_tests(source),
    };

    KainTestCaseReport {
        path: source_name.to_string(),
        mode: mode.as_str().to_string(),
        target: compile_target_name(report_target).to_string(),
        status: if result.is_ok() {
            KainTestStatus::Passed
        } else {
            KainTestStatus::Failed
        },
        expected_error,
        skip_reason: None,
        error: result.err(),
    }
}

fn report_target_for_mode(mode: KainTestMode, directive_target: CompileTarget) -> CompileTarget {
    match mode {
        KainTestMode::RunPass | KainTestMode::RunFail => CompileTarget::Interpret,
        KainTestMode::KainTest => CompileTarget::Test,
        KainTestMode::CheckPass | KainTestMode::CheckFail => directive_target,
    }
}

fn run_check_pass(source_name: &str, source: &str, target: CompileTarget) -> Result<(), String> {
    let report = check_source(source_name, source, &CheckOptions::new(target));
    if report.passed() {
        Ok(())
    } else {
        Err(report
            .error
            .unwrap_or_else(|| "check-pass expected success but checking failed".to_string()))
    }
}

fn run_check_fail(
    source_name: &str,
    source: &str,
    target: CompileTarget,
    expected_error: Option<&str>,
) -> Result<(), String> {
    let report = check_source(source_name, source, &CheckOptions::new(target));
    if report.passed() {
        return Err("check-fail expected checking to fail, but it passed".to_string());
    }
    let error = report.error.unwrap_or_default();
    ensure_expected_error(&error, expected_error)
}

fn run_interpret_expect_pass(source: &str) -> Result<(), String> {
    kain_driver::compile(source, CompileTarget::Interpret)
        .map(|_| ())
        .map_err(|error| error.to_string())
}

fn run_interpret_expect_fail(source: &str, expected_error: Option<&str>) -> Result<(), String> {
    match kain_driver::compile(source, CompileTarget::Interpret) {
        Ok(_) => Err("run-fail expected execution to fail, but it passed".to_string()),
        Err(error) => ensure_expected_error(&error.to_string(), expected_error),
    }
}

fn run_kain_tests(source: &str) -> Result<(), String> {
    let checked = DriverSession::default()
        .frontend_to_checked_program(source, CompileTarget::Test)
        .map_err(|error| error.to_string())?;
    let test_count = kain_check::count_typed_tests(&checked.typed);
    if test_count == 0 {
        return Err("kain-test expected at least one `test` item".to_string());
    }
    runtime::run_tests(&checked.typed).map_err(|error| error.to_string())
}

fn ensure_expected_error(actual: &str, expected: Option<&str>) -> Result<(), String> {
    if let Some(expected) = expected {
        if !actual.contains(expected) {
            return Err(format!(
                "expected error containing '{expected}', got '{actual}'"
            ));
        }
    }
    Ok(())
}

fn parse_directives(source: &str) -> TestDirectives {
    let mut directives = TestDirectives::default();
    for line in source.lines() {
        let Some(value) = directive_payload(line) else {
            continue;
        };

        if let Some(mode) = KainTestMode::parse(value) {
            directives.mode = Some(mode);
            continue;
        }

        if matches!(value, "ignore" | "skip") {
            directives.skip_reason = Some("ignored by directive".to_string());
            continue;
        }

        if value == "known-bug" {
            directives.skip_reason = Some("known bug".to_string());
            continue;
        }

        if let Some((key, raw_value)) = value.split_once(':') {
            let key = key.trim();
            let raw_value = raw_value.trim();
            match key {
                "mode" => directives.mode = KainTestMode::parse(raw_value),
                "target" => directives.target = kain_driver::parse_compile_target(raw_value),
                "error" | "expect-error" => {
                    directives.expected_error = Some(raw_value.to_string());
                }
                "ignore" | "skip" => {
                    directives.skip_reason = Some(if raw_value.is_empty() {
                        "ignored by directive".to_string()
                    } else {
                        raw_value.to_string()
                    });
                }
                "known-bug" => {
                    directives.skip_reason = Some(if raw_value.is_empty() {
                        "known bug".to_string()
                    } else {
                        format!("known bug: {raw_value}")
                    });
                }
                _ => {}
            }
        }
    }
    directives
}

fn directive_payload(line: &str) -> Option<&str> {
    let trimmed = line.trim_start();
    trimmed
        .strip_prefix("//@")
        .or_else(|| trimmed.strip_prefix("#@"))
        .map(str::trim)
}

fn infer_default_mode(source: &str) -> KainTestMode {
    if source
        .lines()
        .any(|line| line.trim_start().starts_with("test "))
    {
        KainTestMode::KainTest
    } else {
        KainTestMode::CheckPass
    }
}

fn summarize_cases(cases: Vec<KainTestCaseReport>) -> KainTestSuiteReport {
    let passed = cases.iter().filter(|case| case.passed()).count();
    let skipped = cases.iter().filter(|case| case.skipped()).count();
    let failed = cases.len().saturating_sub(passed + skipped);
    KainTestSuiteReport {
        total: cases.len(),
        passed,
        skipped,
        failed,
        cases,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn check_pass_directive_passes_when_source_typechecks() {
        let report = run_source(
            "<test>",
            "//@ check-pass\nfn main() -> Int:\n    return 0\n",
            &KainTestOptions::new(CompileTarget::Interpret),
        );

        assert!(report.passed(), "{:?}", report.error);
        assert_eq!(report.mode, "check-pass");
    }

    #[test]
    fn check_fail_directive_passes_when_source_fails() {
        let report = run_source(
            "<test>",
            "//@ check-fail\nfn main( -> Int:\n",
            &KainTestOptions::new(CompileTarget::Interpret),
        );

        assert!(report.passed(), "{:?}", report.error);
        assert_eq!(report.mode, "check-fail");
    }

    #[test]
    fn kain_test_mode_runs_test_items() {
        let report = run_source(
            "<test>",
            "test smoke:\n    assert(1 == 1, \"math\")\n",
            &KainTestOptions::new(CompileTarget::Interpret),
        );

        assert!(report.passed(), "{:?}", report.error);
        assert_eq!(report.mode, "kain-test");
    }

    #[test]
    fn kain_test_mode_executes_nested_module_tests() {
        let report = run_source(
            "<test>",
            "mod nested:\n    test failure_is_observed:\n        assert(1 == 2, \"nested tests must execute\")\n",
            &KainTestOptions::new(CompileTarget::Interpret),
        );

        assert!(!report.passed());
        assert!(report
            .error
            .as_deref()
            .unwrap_or_default()
            .contains("nested tests must execute"));
    }

    #[test]
    fn run_fail_directive_passes_when_interpreter_fails() {
        let report = run_source(
            "<test>",
            "//@ run-fail\nfn main() -> Int:\n    return missing_name\n",
            &KainTestOptions::new(CompileTarget::Interpret),
        );

        assert!(report.passed(), "{:?}", report.error);
        assert_eq!(report.mode, "run-fail");
    }

    #[test]
    fn ignore_directive_marks_case_skipped_without_failing_suite() {
        let report = run_path_for_sources(&[
            (
                "ignored.kn",
                "//@ ignore: waiting on parser edge\nfn main( -> Int:\n",
            ),
            ("passing.kn", "fn main() -> Int:\n    return 0\n"),
        ]);

        assert_eq!(report.total, 2);
        assert_eq!(report.passed, 1);
        assert_eq!(report.skipped, 1);
        assert_eq!(report.failed, 0);
        assert!(report.is_success());
    }

    fn run_path_for_sources(sources: &[(&str, &str)]) -> KainTestSuiteReport {
        let temp = tempfile::tempdir().expect("temp dir");
        for (path, source) in sources {
            kfs::write_text(temp.path().join(path), source).expect("write source");
        }
        run_path(temp.path(), &KainTestOptions::new(CompileTarget::Interpret))
    }
}
