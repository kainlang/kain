// ============================================================================
//  Audit mode: runs both check and build, compares results.
//
//  `kain check --audit` runs the full pipeline (check then build) and
//  reports every error that build caught but check missed. This is the
//  self-healing feedback loop — Phase 1 (manual, not automatic learning).
//
//  The audit module is intentionally pure: it does not spawn a build
//  process. The CLI is responsible for invoking the build pipeline and
//  piping its stderr into `audit_build_vs_check`. This keeps the module
//  deterministic, testable, and free of subprocess concerns.
// ============================================================================

use serde::{Deserialize, Serialize};
use std::collections::HashSet;

/// An audit gap: an error caught by build but not by check.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct AuditGap {
    /// The error message from build.
    pub error_message: String,
    /// Which phase caught it (e.g., "codegen", "link", "runtime").
    pub phase: String,
    /// The source span if available (currently not extracted by callers).
    pub span: Option<String>,
    /// Category: which validator WOULD have caught this?
    pub missing_validator_category: String,
    /// Can this be extracted to a check-time validator?
    pub extractable_to_check: bool,
}

/// Full audit report comparing check vs build.
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct AuditReport {
    /// Total errors from check.
    pub check_errors: usize,
    /// Total errors from build.
    pub build_errors: usize,
    /// Errors caught by build but missed by check.
    pub gaps: Vec<AuditGap>,
    /// Summary: how many gaps are extractable to check time?
    pub extractable_count: usize,
    /// Summary: how many gaps are truly codegen-only?
    pub codegen_only_count: usize,
    /// Number of validator categories surfaced as missing.
    pub missing_category_count: usize,
}

/// Classify a build-only error into a validator category.
///
/// Returns `(category, extractable_to_check)`. The category is a short
/// label like "atomic" or "target" — when a new validator is added in
/// `telemetry::all_validators`, the corresponding classification should
/// appear here too.
pub fn classify_gap(error_message: &str) -> (&'static str, bool) {
    let lower = error_message.to_lowercase();
    if lower.contains("atomic") || lower.contains("ordering") {
        ("atomic", true)
    } else if lower.contains("x86_64-only") || lower.contains("unsupported target") {
        ("target", true)
    } else if lower.contains("bitcast") || lower.contains("width mismatch") {
        ("bitcast", true)
    } else if lower.contains("argument") || lower.contains("expects") {
        ("method-arity", true)
    } else if lower.contains("actor") || lower.contains("message") {
        ("actor", true)
    } else if lower.contains("break") || lower.contains("continue") {
        ("control-flow", true)
    } else if lower.contains("enum") || lower.contains("variant") {
        ("pattern", true)
    } else if lower.contains("struct update") || lower.contains("..") {
        ("struct", true)
    } else if lower.contains("shader") || lower.contains("stage") {
        ("gpu", true)
    } else if lower.contains("trait bound") || lower.contains("satisfy") {
        ("generics", true)
    } else if lower.contains("ownership")
        || lower.contains("collapse")
        || lower.contains("decay")
    {
        ("ownership", true)
    } else if lower.contains("entangle") || lower.contains("world") {
        ("semantic-contract", true)
    } else if lower.contains("inline asm") || lower.contains("callconv") {
        ("target", true)
    } else {
        ("codegen-only", false)
    }
}

/// Heuristic: which phase of the build pipeline produced this error line?
pub fn classify_phase(error_message: &str) -> &'static str {
    let lower = error_message.to_lowercase();
    if lower.contains("link") || lower.contains("undefined reference") {
        "link"
    } else if lower.contains("runtime") || lower.contains("panic") {
        "runtime"
    } else if lower.contains("type") {
        "type"
    } else if lower.contains("lex") || lower.contains("token") {
        "lexer"
    } else if lower.contains("parse") {
        "parser"
    } else {
        "codegen"
    }
}

/// Is a build error line likely an "error" we should compare against?
pub fn is_error_line(line: &str) -> bool {
    let trimmed = line.trim_start();
    if trimmed.is_empty() {
        return false;
    }
    // Treat rustc/clang-style "error:" prefixed lines as build errors.
    if trimmed.starts_with("error:") || trimmed.starts_with("error[") {
        return true;
    }
    // Treat `error = ...` and our own `eprintln!`-style messages with "error"
    // as well, but skip the warning/info levels.
    if (trimmed.contains("error") || trimmed.contains("Error"))
        && !trimmed.starts_with("warning")
        && !trimmed.starts_with("info")
        && !trimmed.starts_with("note")
    {
        return true;
    }
    false
}

/// Compare check errors against build output and produce an audit report.
pub fn audit_build_vs_check(check_errors: &[String], build_output: &str) -> AuditReport {
    let build_error_lines: Vec<&str> = build_output
        .lines()
        .filter(|line| is_error_line(line))
        .collect();

    // Build a normalized set of check error fragments for fuzzy containment
    // comparison. We extract the error message payload (post "error:") and
    // use it as a substring fingerprint.
    let check_fragments: HashSet<String> = check_errors
        .iter()
        .map(|s| normalize_error_fragment(s))
        .filter(|s| !s.is_empty())
        .collect();

    let mut gaps = Vec::new();

    for line in &build_error_lines {
        let normalized = normalize_error_fragment(line);
        // Skip if a similar fragment was caught by check.
        let found_in_check = if normalized.is_empty() {
            // Couldn't normalize — assume check already covered it.
            true
        } else {
            check_fragments.iter().any(|fragment| {
                fragment.contains(&normalized)
                    || normalized.contains(fragment.as_str())
                    || fragment == &normalized
            })
        };

        if !found_in_check {
            let (category, extractable) = classify_gap(line);
            let phase = classify_phase(line).to_string();
            gaps.push(AuditGap {
                error_message: line.to_string(),
                phase,
                span: None,
                missing_validator_category: category.to_string(),
                extractable_to_check: extractable,
            });
        }
    }

    let extractable = gaps.iter().filter(|g| g.extractable_to_check).count();
    let codegen_only = gaps.len() - extractable;

    let missing_categories: HashSet<String> = gaps
        .iter()
        .map(|g| g.missing_validator_category.clone())
        .collect();

    AuditReport {
        check_errors: check_errors.len(),
        build_errors: build_error_lines.len(),
        extractable_count: extractable,
        codegen_only_count: codegen_only,
        missing_category_count: missing_categories.len(),
        gaps,
    }
}

/// Normalize an error string into a fingerprint suitable for fuzzy match.
///
/// Strips path prefixes, error codes, and trailing punctuation. Returns
/// the most distinctive substring.
fn normalize_error_fragment(input: &str) -> String {
    let trimmed = input.trim();
    if trimmed.is_empty() {
        return String::new();
    }
    // Strip the leading "error: " or "error[E0xxx]: " prefix.
    let payload = if let Some(idx) = trimmed.find("error") {
        let after = &trimmed[idx..];
        // Skip past "error" and optional "[...]" / ":"
        let mut chars = after.chars().peekable();
        let mut out = String::new();
        // consume "error"
        for _ in 0..5 {
            chars.next();
        }
        // consume optional "[E1234]"
        if chars.peek() == Some(&'[') {
            while let Some(&c) = chars.peek() {
                chars.next();
                if c == ']' {
                    break;
                }
            }
        }
        // consume optional ": "
        while let Some(&c) = chars.peek() {
            if c == ':' || c == ' ' {
                chars.next();
            } else {
                break;
            }
        }
        out.extend(chars);
        out
    } else {
        trimmed.to_string()
    };

    // Collapse whitespace and trim.
    payload
        .split_whitespace()
        .collect::<Vec<_>>()
        .join(" ")
        .to_lowercase()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn classify_gap_routes_atomic_to_extractable() {
        let (cat, extractable) = classify_gap("error: atomic_store_release requires failure");
        assert_eq!(cat, "atomic");
        assert!(extractable);
    }

    #[test]
    fn classify_gap_routes_x86_64_only_to_target() {
        let (cat, extractable) = classify_gap("error: x86_64-only feature on wasm target");
        assert_eq!(cat, "target");
        assert!(extractable);
    }

    #[test]
    fn classify_gap_default_to_codegen_only() {
        let (cat, extractable) = classify_gap("some unknown build error");
        assert_eq!(cat, "codegen-only");
        assert!(!extractable);
    }

    #[test]
    fn classify_phase_routes_linker_errors() {
        assert_eq!(classify_phase("undefined reference to symbol"), "link");
    }

    #[test]
    fn classify_phase_defaults_to_codegen() {
        assert_eq!(classify_phase("some random build error"), "codegen");
    }

    #[test]
    fn is_error_line_rejects_warnings() {
        assert!(!is_error_line("warning: unused variable"));
        assert!(!is_error_line("note: previous definition"));
        assert!(is_error_line("error: something broke"));
        assert!(is_error_line("error[E0308]: mismatched types"));
    }

    #[test]
    fn audit_empty_inputs_yields_no_gaps() {
        let report = audit_build_vs_check(&[], "");
        assert_eq!(report.check_errors, 0);
        assert_eq!(report.build_errors, 0);
        assert!(report.gaps.is_empty());
        assert_eq!(report.extractable_count, 0);
        assert_eq!(report.codegen_only_count, 0);
    }

    #[test]
    fn audit_flags_build_error_not_in_check() {
        let check_errors = vec!["error: type mismatch".to_string()];
        let build_output = "\
warning: unused variable
error: atomic_store_release requires failure ordering
note: candidate is acq_rel
";
        let report = audit_build_vs_check(&check_errors, build_output);
        assert_eq!(report.check_errors, 1);
        assert!(report.build_errors >= 1);
        assert!(report
            .gaps
            .iter()
            .any(|g| g.missing_validator_category == "atomic"));
        assert!(report.extractable_count >= 1);
    }

    #[test]
    fn audit_dedupes_when_check_already_caught_it() {
        let check_errors = vec!["error: type mismatch in argument".to_string()];
        let build_output = "\
error: type mismatch in argument 1
note: expected Int, got String
";
        let report = audit_build_vs_check(&check_errors, build_output);
        // The build error should match the check error by fuzzy containment.
        assert!(report.gaps.is_empty(), "expected no gaps: {:?}", report.gaps);
    }

    #[test]
    fn audit_classifies_unrelated_build_only_error_as_codegen() {
        let check_errors = vec!["error: type mismatch".to_string()];
        let build_output = "error: internal compiler segfault at codegen\n";
        let report = audit_build_vs_check(&check_errors, build_output);
        assert!(report
            .gaps
            .iter()
            .any(|g| g.missing_validator_category == "codegen-only"));
        assert!(report.codegen_only_count >= 1);
    }

    #[test]
    fn normalize_strips_error_code_and_whitespace() {
        let norm = normalize_error_fragment("error[E0308]:   mismatched   types  ");
        assert!(norm.starts_with("mismatched types"), "got: {norm}");
    }
}
