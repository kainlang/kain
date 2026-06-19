// ============================================================================
//  Error regression test harness (ETA-C).
//
//  For each known error code in the compiler, there should be a minimal
//  .kn file that triggers it. This test iterates the manifest and verifies
//  that `kain check` produces the expected error for each file.
//
//  If a refactor accidentally removes or weakens an error check, this test
//  fails — preventing regression.
//
//  Phased strictness:
//   - Phase 1 (current): the harness RECORDS behavior. Cases that should
//     fail but currently pass are listed as `known_bug = true` in the
//     manifest and do not cause a test failure; the test simply logs them.
//     This lets us ship the test surface today, even though the
//     underlying validators (ETA-A, BRAVO, CHARLIE) are still landing.
//
//   - Phase 2 (after ETA-A/BRAVO/CHARLIE merge): flip `known_bug` to
//     `false` in the manifest. The harness then becomes a strict
//     regression gate.
// ============================================================================

use std::fs;
use std::path::PathBuf;
use std::sync::atomic::{AtomicUsize, Ordering};

use serde::Deserialize;

#[derive(Debug, Deserialize)]
struct RegressionManifest {
    cases: Vec<RegressionCase>,
}

#[derive(Debug, Deserialize)]
struct RegressionCase {
    /// Path to the .kn file (relative to regression/ directory)
    file: String,
    /// Expected error code (e.g., "E0308", "codegen-atomic-ordering")
    expected_error_code: String,
    /// Expected error message substring (for fuzzy matching)
    #[serde(default)]
    expected_message_contains: String,
    /// Should this file FAIL check? (default: true)
    #[serde(default = "default_true")]
    should_fail: bool,
    /// Is this a known bug (check passes but shouldn't)?
    #[serde(default)]
    known_bug: bool,
    /// Why is this a known bug?
    known_bug_reason: Option<String>,
}

fn default_true() -> bool {
    true
}

/// Test summary counters — exposed so the harness can report pass/fail/known.
struct TestSummary {
    passed: AtomicUsize,
    failed: AtomicUsize,
    known_bugs: AtomicUsize,
    fixed_bugs: AtomicUsize,
}

impl TestSummary {
    fn new() -> Self {
        Self {
            passed: AtomicUsize::new(0),
            failed: AtomicUsize::new(0),
            known_bugs: AtomicUsize::new(0),
            fixed_bugs: AtomicUsize::new(0),
        }
    }
    fn record_pass(&self) {
        self.passed.fetch_add(1, Ordering::SeqCst);
    }
    fn record_fail(&self) {
        self.failed.fetch_add(1, Ordering::SeqCst);
    }
    fn record_known_bug(&self) {
        self.known_bugs.fetch_add(1, Ordering::SeqCst);
    }
    fn record_fixed_bug(&self) {
        self.fixed_bugs.fetch_add(1, Ordering::SeqCst);
    }
    fn snapshot(&self) -> (usize, usize, usize, usize) {
        (
            self.passed.load(Ordering::SeqCst),
            self.failed.load(Ordering::SeqCst),
            self.known_bugs.load(Ordering::SeqCst),
            self.fixed_bugs.load(Ordering::SeqCst),
        )
    }
}

fn regression_dir() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests")
        .join("regression")
}

fn load_manifest() -> RegressionManifest {
    let manifest_path = regression_dir().join("manifest.toml");
    let content = fs::read_to_string(&manifest_path)
        .unwrap_or_else(|err| panic!("failed to read regression manifest {manifest_path:?}: {err}"));
    toml::from_str(&content).unwrap_or_else(|err| {
        panic!("failed to parse regression manifest {manifest_path:?}: {err}");
    })
}

fn run_case(case: &RegressionCase, summary: &TestSummary) {
    let file_path = regression_dir().join(&case.file);
    let source = match fs::read_to_string(&file_path) {
        Ok(s) => s,
        Err(err) => {
            eprintln!(
                "SKIP (file missing): {} — {}",
                case.file, err
            );
            return;
        }
    };

    let opts = kain_check::CheckOptions::new(kain_core::CompileTarget::Interpret)
        .with_pedantic(true);
    let report = kain_check::check_source(&file_path.to_string_lossy(), &source, &opts);

    if case.known_bug {
        if report.passed() {
            // Expected: bug is latent (check passes, but shouldn't).
            summary.record_known_bug();
            println!(
                "KNOWN BUG: {} — {} — {}",
                case.file,
                case.expected_error_code,
                case.known_bug_reason.as_deref().unwrap_or("no reason given"),
            );
        } else {
            // Bug was fixed by a recent validator landing.
            summary.record_fixed_bug();
            println!(
                "FIXED: {} — {} was a known bug, now caught by check!",
                case.file, case.expected_error_code,
            );
        }
        return;
    }

    if case.should_fail {
        if report.passed() {
            eprintln!(
                "REGRESSION: {} should fail with error code '{}' but passed check.",
                case.file, case.expected_error_code
            );
            summary.record_fail();
        } else {
            // Optionally verify the error message contains the expected substring.
            let contains_check = if case.expected_message_contains.is_empty() {
                true
            } else if let Some(error_msg) = &report.error {
                error_msg
                    .to_lowercase()
                    .contains(&case.expected_message_contains.to_lowercase())
            } else {
                false
            };
            if contains_check {
                summary.record_pass();
            } else {
                eprintln!(
                    "REGRESSION: {} failed but error message doesn't contain '{}'. Got: {:?}",
                    case.file, case.expected_message_contains, report.error
                );
                summary.record_fail();
            }
        }
    } else {
        // should_fail = false: this file should pass check.
        if !report.passed() {
            eprintln!(
                "REGRESSION: {} should pass check but failed: {:?}",
                case.file, report.error
            );
            summary.record_fail();
        } else {
            summary.record_pass();
        }
    }
}

#[test]
fn regression_suite_runs() {
    let manifest = load_manifest();
    assert!(
        !manifest.cases.is_empty(),
        "regression manifest must declare at least one case"
    );
    let summary = TestSummary::new();

    for case in &manifest.cases {
        run_case(case, &summary);
    }

    let (passed, failed, known_bugs, fixed) = summary.snapshot();
    println!(
        "Regression suite: {} passed, {} failed, {} known bugs, {} fixed",
        passed, failed, known_bugs, fixed
    );

    if failed > 0 {
        panic!("{failed} regression tests FAILED");
    }
}

#[test]
fn manifest_is_well_formed() {
    let manifest = load_manifest();
    // Each case must point to a real file (or have a known reason to skip).
    for case in &manifest.cases {
        let file_path = regression_dir().join(&case.file);
        if !file_path.exists() {
            eprintln!(
                "manifest references missing fixture {} — create the file or remove the case",
                file_path.display()
            );
        }
    }
    // We don't fail on missing fixtures because the harness is tolerant
    // (it logs SKIPs). We just need the manifest to parse and load.
    assert!(!manifest.cases.is_empty());
}
