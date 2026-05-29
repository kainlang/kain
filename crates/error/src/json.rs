//! JSON output mode for diagnostics.
//!
//! Produces structured, machine-readable JSON from `DiagnosticReport`s.
//! Designed for CI pipelines, editor integrations, and `kain --json`.
//!
//! The schema:
//! ```json
//! {
//!   "diagnostics": [{
//!     "severity": "error",
//!     "kind": "PARSE",
//!     "code": "KAIN-PARSE-0005",
//!     "title": "Missing Delimiter Before Newline",
//!     "category": "parse",
//!     "message": "Expected ':' before newline",
//!     "file": "src/main.kn",
//!     "location": {"line": 10, "column": 8},
//!     "primary_span": {"start": 142, "end": 148},
//!     "primary_range": {
//!       "file": "src/main.kn",
//!       "start": {"line": 10, "column": 8, "display_column": 8, "offset": 142},
//!       "end":   {"line": 10, "column": 14, "display_column": 14, "offset": 148}
//!     },
//!     "labels": [...],
//!     "notes": [...],
//!     "help": [...],
//!     "fixits": [...],
//!     "phase": "parser",
//!     "origin": null,
//!     "tags": [],
//!     "docs_key": "parser/missing-delimiter-before-newline",
//!     "see_also": ["KAIN-PARSE-0002"]
//!   }],
//!   "summary": {
//!     "total": 3,
//!     "errors": 2,
//!     "warnings": 1,
//!     "notes": 0
//!   }
//! }
//! ```

use crate::diagnostic_registry::spec_for_code;
use crate::label::DiagnosticFixIt;
use crate::report::DiagnosticReport;
use crate::source::SourceRange;
use serde_json::{json, Value as JsonValue};

/// Convert a single `DiagnosticReport` to a JSON value.
pub fn report_to_json(report: &DiagnosticReport) -> JsonValue {
    let spec = spec_for_code(report.code);

    let labels: Vec<JsonValue> = report
        .labels
        .iter()
        .map(|label| {
            json!({
                "span": {"start": label.span.start, "end": label.span.end},
                "range": label.range.as_ref().map(range_to_json),
                "message": label.message,
                "primary": label.primary,
                "kind": format!("{:?}", label.kind),
            })
        })
        .collect();

    let fixits: Vec<JsonValue> = report.fixits.iter().map(fixit_to_json).collect();

    json!({
        "severity": report.severity.to_string(),
        "kind": report.kind.to_string(),
        "code": spec.code_str,
        "title": spec.title,
        "category": spec.category.to_string(),
        "message": report.message,
        "file": report.file.as_ref().map(|path| path.display().to_string()),
        "location": report.location.map(|(line, col)| json!({"line": line, "column": col})),
        "primary_span": report.primary_span.map(|span| json!({"start": span.start, "end": span.end})),
        "primary_range": report.primary_range.as_ref().map(range_to_json),
        "labels": labels,
        "notes": report.notes,
        "help": report.help,
        "fixits": fixits,
        "phase": report.phase.to_string(),
        "origin": report.origin,
        "tags": report.tags,
        "semantic": report.semantic.as_ref().map(semantic_to_json),
        "docs_key": spec.docs_key.unwrap_or(""),
    })
}

/// Convert multiple diagnostics into a `{"diagnostics": [...], "summary": {...}}` envelope.
pub fn diagnostics_to_json(reports: &[DiagnosticReport]) -> JsonValue {
    let diags: Vec<JsonValue> = reports.iter().map(report_to_json).collect();
    let total = reports.len();
    let errors = reports.iter().filter(|r| r.severity.is_error()).count();
    let warnings = reports
        .iter()
        .filter(|r| r.severity == crate::severity::DiagnosticSeverity::Warning)
        .count();
    let notes = reports
        .iter()
        .filter(|r| r.severity == crate::severity::DiagnosticSeverity::Note)
        .count();

    json!({
        "diagnostics": diags,
        "summary": {
            "total": total,
            "errors": errors,
            "warnings": warnings,
            "notes": notes,
        }
    })
}

/// Pretty-print diagnostics as JSON string.
pub fn diagnostics_to_json_string(reports: &[DiagnosticReport]) -> String {
    serde_json::to_string_pretty(&diagnostics_to_json(reports))
        .unwrap_or_else(|e| format!("{{\"error\": \"JSON serialization failed: {e}\"}}"))
}

// ── Internal helpers ─────────────────────────────────────────────────

fn range_to_json(range: &SourceRange) -> JsonValue {
    json!({
        "file": range.file,
        "start": {
            "line": range.start.line,
            "column": range.start.col,
            "display_column": range.start.display_col,
            "offset": range.start.offset,
        },
        "end": {
            "line": range.end.line,
            "column": range.end.col,
            "display_column": range.end.display_col,
            "offset": range.end.offset,
        }
    })
}

fn fixit_to_json(fixit: &DiagnosticFixIt) -> JsonValue {
    json!({
        "span": {"start": fixit.span.start, "end": fixit.span.end},
        "range": fixit.range.as_ref().map(range_to_json),
        "replacement": fixit.replacement,
        "message": fixit.message,
        "primary": fixit.primary,
        "confidence": format!("{:?}", fixit.confidence),
    })
}

fn semantic_to_json(semantic: &crate::report::DiagnosticSemanticSummary) -> JsonValue {
    json!({
        "failure_mode": semantic.failure_mode,
        "explanation_style": semantic.explanation_style,
        "explanation": semantic.explanation,
        "root_cause_confidence": semantic.root_cause_confidence,
        "cascade_probability": semantic.cascade_probability,
        "repairs": semantic.repairs.iter().map(|repair| json!({
            "repair_id": repair.repair_id,
            "description": repair.description,
            "score": repair.score,
            "replacement_text": repair.replacement_text,
        })).collect::<Vec<_>>(),
    })
}
