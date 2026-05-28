//! Integration tests for the kain-error diagnostic engine.
//!
//! These tests exercise the full pipeline: TOML specs → codegen →
//! registry → report construction → terminal rendering → JSON output.

use kain_error::*;

// ── Registry tests ──────────────────────────────────────────────────

#[test]
fn registry_loads_all_specs() {
    let guard = registry();
    let count = guard.len();
    // We expect at least 100 codes from our 15 TOML spec files
    assert!(
        count >= 100,
        "Expected at least 100 diagnostic codes loaded from TOML, got {count}"
    );
    println!("Registry loaded {count} diagnostic codes");
}

#[test]
fn spec_lookup_works() {
    let spec = spec_for_code(DiagnosticCode::new("KAIN-PARSE-0005"))
        .expect("KAIN-PARSE-0005 should be registered");
    assert_eq!(spec.title, "Missing Delimiter Before Newline");
    assert!(spec.help.contains("Kain block headers"));
    assert_eq!(spec.fixit.as_deref(), Some(":"));
}

#[test]
fn spec_lookup_unknown_returns_none() {
    assert!(spec_for_code(DiagnosticCode::new("KAIN-FAKE-9999")).is_none());
}

#[test]
fn registry_search_finds_results() {
    let guard = registry();
    let results = guard.search("delimiter");
    assert!(!results.is_empty(), "Should find delimiter-related codes");
    let found = results.iter().any(|s| s.code == "KAIN-PARSE-0005");
    assert!(found, "Should find KAIN-PARSE-0005 when searching 'delimiter'");
}

#[test]
fn registry_list_category() {
    let guard = registry();
    let results = guard.list_category("PARSE");
    assert!(!results.is_empty());
    // All should be PARSE codes
    for spec in &results {
        assert!(
            spec.code.as_str().contains("PARSE"),
            "{} should be a PARSE code",
            spec.code
        );
    }
}

// ── DiagnosticReport builder tests ──────────────────────────────────

#[test]
fn report_builder_basic() {
    let report = DiagnosticReport::new(
        ErrorKind::Parse,
        DiagnosticCode::new("KAIN-PARSE-0005"),
        "Expected ':' before newline",
    )
    .severity(DiagnosticSeverity::Error)
    .file("test.kn")
    .location(5, 12)
    .primary_label(Span::new(42, 48), "expected ':' here")
    .note("Block headers in Kain end with ':'")
    .help("Add ':' at the end of line 4")
    .fixit(Span::new(48, 48), ":", "insert missing ':'")
    .tag("syntax")
    .phase(CompilerPhase::Parser);

    assert_eq!(report.kind, ErrorKind::Parse);
    assert_eq!(report.code, "KAIN-PARSE-0005");
    assert_eq!(report.severity, DiagnosticSeverity::Error);
    assert_eq!(report.file.as_ref().unwrap().to_str().unwrap(), "test.kn");
    assert_eq!(report.location, Some((5, 12)));
    assert_eq!(report.labels.len(), 1);
    assert!(report.labels[0].primary);
    assert_eq!(report.notes.len(), 1);
    assert_eq!(report.help.len(), 1);
    assert_eq!(report.fixits.len(), 1);
    assert_eq!(report.tags, vec!["syntax"]);
    assert_eq!(report.phase, CompilerPhase::Parser);
}

#[test]
fn report_registry_help_augments() {
    let report = ParseDiagnostic::missing_delimiter("test")
        .with_registry_help();
    // Registry help should have been appended
    assert!(!report.help.is_empty(), "Registry help should be added");
    // The fixit from the spec should have been added
    assert_eq!(report.fixits.len(), 1);
    assert_eq!(report.fixits[0].replacement, ":");
}

#[test]
fn typed_builder_parse() {
    let report = ParseDiagnostic::expected_token("Expected 'fn' keyword");
    assert_eq!(report.code, "KAIN-PARSE-0002");
    assert_eq!(report.kind, ErrorKind::Parse);
}

#[test]
fn typed_builder_type() {
    let report = TypeDiagnostic::type_mismatch("Expected i32, found string");
    assert_eq!(report.code, "KAIN-TYPE-0003");
}

#[test]
fn typed_builder_borrow() {
    let report = BorrowDiagnostic::use_after_move("value moved here");
    assert_eq!(report.code, "KAIN-BORROW-0004");
}

// ── JSON output tests ───────────────────────────────────────────────

#[test]
fn json_output_single_report() {
    let report = DiagnosticReport::new(
        ErrorKind::Parse,
        DiagnosticCode::new("KAIN-PARSE-0005"),
        "Expected ':'",
    )
    .file("test.kn")
    .location(3, 10)
    .primary_label(Span::new(20, 25), "here");

    let json = report_to_json(&report);
    assert_eq!(json["severity"], "error");
    assert_eq!(json["code"], "KAIN-PARSE-0005");
    assert_eq!(json["message"], "Expected ':'");
    assert_eq!(json["file"], "test.kn");
    assert_eq!(json["location"]["line"], 3);
    assert_eq!(json["labels"][0]["primary"], true);
    assert_eq!(json["labels"][0]["message"], "here");
}

#[test]
fn json_output_diagnostics_envelope() {
    let reports = vec![
        DiagnosticReport::new_default(ErrorKind::Parse, "parse error"),
        DiagnosticReport::new_default(ErrorKind::Type, "type error")
            .severity(DiagnosticSeverity::Warning),
    ];
    let json = diagnostics_to_json(&reports);
    assert_eq!(json["summary"]["total"], 2);
    assert_eq!(json["summary"]["errors"], 1);
    assert_eq!(json["summary"]["warnings"], 1);
    assert_eq!(json["diagnostics"].as_array().unwrap().len(), 2);
}

#[test]
fn json_to_string_produces_valid_json() {
    let reports = vec![DiagnosticReport::new_default(
        ErrorKind::Parse,
        "test error",
    )];
    let s = diagnostics_to_json_string(&reports);
    let parsed: serde_json::Value = serde_json::from_str(&s).unwrap();
    assert_eq!(parsed["summary"]["total"], 1);
}

// ── KainError tests ────────────────────────────────────────────────

#[test]
fn kain_error_rich_roundtrip() {
    let report = ParseDiagnostic::expected_token("test");
    let err = KainError::rich(report.clone());
    let extracted = err.to_diagnostic_reports();
    assert_eq!(extracted.len(), 1);
    assert_eq!(extracted[0].code, report.code);
}

#[test]
fn kain_error_json_output() {
    let report = ParseDiagnostic::expected_token("expected token");
    let err = KainError::rich(report);
    let json = err.to_json_value();
    assert_eq!(json["summary"]["total"], 1);
    assert_eq!(json["diagnostics"][0]["code"], "KAIN-PARSE-0002");
}

#[test]
fn kain_error_multi() {
    let reports = vec![
        ParseDiagnostic::generic("err1"),
        TypeDiagnostic::generic("err2"),
    ];
    let err = KainError::multi(reports);
    let extracted = err.to_diagnostic_reports();
    assert_eq!(extracted.len(), 2);
    let display = err.to_string();
    assert!(display.contains("2 error(s)"));
}

#[test]
fn kain_error_display() {
    let err = KainError::simple(
        DiagnosticCode::new("KAIN-TYPE-0003"),
        "type mismatch",
    )
    .with_file("src/main.kn")
    .with_location(10, 5);
    let display = err.to_string();
    assert!(display.contains("KAIN-TYPE-0003"));
    assert!(display.contains("src/main.kn"));
    assert!(display.contains("10:5"));
}

// ── Renderer tests ─────────────────────────────────────────────────

#[test]
fn renderer_produces_output() {
    let source = "fn greet\n    return \"hi\"";
    let report = ParseDiagnostic::missing_delimiter(
        "Missing ':' before newline in function header"
    )
    .primary_label(Span::new(8, 8), "expected ':' here")
    .with_registry_help();

    let output = format_diagnostic(source, "test.kn", &report, false);
    assert!(output.contains("error[PARSE:KAIN-PARSE-0005]"));
    assert!(output.contains("Missing ':' before newline"));
    assert!(output.contains("--> test.kn:1"));
    assert!(output.contains("expected ':' here"));
}

#[test]
fn renderer_no_color_works() {
    let report = DiagnosticReport::new_default(ErrorKind::Type, "type error");
    let output = format_diagnostic("let x = 5", "test.kn", &report, false);
    // Should not contain ANSI escape codes
    assert!(!output.contains("\x1b["));
}

#[test]
fn renderer_with_color_has_ansi() {
    let report = DiagnosticReport::new_default(ErrorKind::Type, "type error");
    let output = format_diagnostic("let x = 5", "test.kn", &report, true);
    // Should contain ANSI escape codes
    assert!(output.contains("\x1b["));
}

// ── Chain tests ────────────────────────────────────────────────────

#[test]
fn deduplicate_removes_duplicates() {
    let r1 = DiagnosticReport::new_default(ErrorKind::Parse, "err1")
        .primary_span(Span::new(10, 15));
    let r2 = DiagnosticReport::new_default(ErrorKind::Parse, "err1")
        .primary_span(Span::new(10, 15)); // same code + span
    let r3 = DiagnosticReport::new_default(ErrorKind::Parse, "err2")
        .primary_span(Span::new(20, 25)); // different span

    let deduped = deduplicate(vec![r1.clone(), r2, r3.clone()]);
    assert_eq!(deduped.len(), 2);
}

#[test]
fn enforce_budget_truncates() {
    let reports: Vec<_> = (0..10)
        .map(|i| DiagnosticReport::new_default(ErrorKind::Parse, format!("err{i}")))
        .collect();
    let budgeted = enforce_budget(reports, 5);
    assert_eq!(budgeted.len(), 6); // 5 errors + 1 summary note
    assert!(budgeted.last().unwrap().message.contains("suppressed"));
}

// ── Debug trace tests ──────────────────────────────────────────────

#[test]
fn debug_trace_collects_entries() {
    let mut trace = DebugTrace::new().enabled();
    trace.trace(CompilerPhase::TypeChecking, "checking type of x");
    trace.trace_span(
        CompilerPhase::TypeChecking,
        Span::new(0, 10),
        "x defined here",
    );
    assert_eq!(trace.len(), 2);
    assert!(trace.is_enabled());
}

#[test]
fn debug_trace_disabled_noops() {
    let mut trace = DebugTrace::new();
    trace.trace(CompilerPhase::TypeChecking, "should not appear");
    assert!(trace.is_empty());
}

// ── Explain tests ──────────────────────────────────────────────────

#[test]
fn explain_known_code() {
    let output = explain("KAIN-PARSE-0005");
    assert!(output.contains("Missing Delimiter Before Newline"));
    assert!(output.contains("Kain block headers"));
}

#[test]
fn explain_unknown_code_reports_error() {
    let output = explain("KAIN-FAKE-9999");
    assert!(output.contains("Unknown diagnostic code"));
}

#[test]
fn explain_list_produces_output() {
    let output = list();
    assert!(output.contains("PARSE"));
    assert!(output.contains("TYPE"));
    assert!(output.contains("KAIN-PARSE-0001"));
}

#[test]
fn explain_search_finds_code() {
    let output = search("expected token");
    assert!(output.contains("KAIN-PARSE-0002"));
}

#[test]
fn explain_category() {
    let output = list_category("PARSE");
    assert!(output.contains("KAIN-PARSE-0001"));
    assert!(!output.contains("KAIN-TYPE-0001"));
}
