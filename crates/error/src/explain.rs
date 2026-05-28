//! `kain explain` — diagnostic code explanation system.
//!
//! Provides human-readable explanations for every registered diagnostic
//! code. The API is designed to be consumed by:
//! - `kain explain KAIN-PARSE-0005` (CLI)
//! - Editor hover tooltips
//! - Web-based docs generation

use crate::registry::{explain_code, list_all_codes, spec_for_code_str};

/// Explain a diagnostic code (full detail).
pub fn explain(code: &str) -> String {
    match spec_for_code_str(code) {
        Some(spec) => explain_code(spec.code),
        None => format!("Unknown diagnostic code: {code}\nUse `kain explain --list` to see all registered codes."),
    }
}

/// Search for diagnostic codes matching a query.
pub fn search(query: &str) -> String {
    let guard = crate::registry::registry();
    let results = guard.search(query);

    if results.is_empty() {
        return format!("No diagnostic codes found matching '{query}'.");
    }

    let mut out = String::new();
    out.push_str(&format!(
        "Found {n} diagnostic code(s) matching '{query}':\n\n",
        n = results.len()
    ));

    for spec in results {
        out.push_str(&format!(
            "  {:<20}  {:6}  {}\n",
            spec.code.as_str(),
            spec.severity.to_string(),
            spec.title
        ));
    }
    out.push_str("\nUse `kain explain <CODE>` for full details on any code.\n");
    out
}

/// List all registered diagnostic codes.
pub fn list() -> String {
    list_all_codes()
}

/// List codes in a specific category.
pub fn list_category(category: &str) -> String {
    let guard = crate::registry::registry();
    let results = guard.list_category(category);

    if results.is_empty() {
        return format!(
            "No diagnostic codes found in category '{category}'. \
             Available categories: PARSE, TYPE, EFFECT, BORROW, WORLD, \
             SHADER, ACTOR, COMPTIME, STATE, TEST, CODEGEN, MEM, RUNTIME, \
             IO, CONFIG, VALIDATE"
        );
    }

    let mut out = String::new();
    out.push_str(&format!("Diagnostic codes in {category}:\n\n"));

    for spec in results {
        out.push_str(&format!(
            "  {:<20}  {:6}  {}\n",
            spec.code.as_str(),
            spec.severity.to_string(),
            spec.title
        ));
    }
    out
}

/// Get the full spec as JSON (for editor integrations).
pub fn explain_json(code: &str) -> String {
    match spec_for_code_str(code) {
        Some(spec) => {
            let json = serde_json::json!({
                "code": spec.code.as_str(),
                "title": spec.title,
                "severity": spec.severity.to_string(),
                "docs_key": spec.docs_key,
                "help": spec.help,
                "example_bad": spec.example_bad,
                "example_good": spec.example_good,
                "fixit": spec.fixit,
                "see_also": spec.see_also,
            });
            serde_json::to_string_pretty(&json)
                .unwrap_or_else(|e| format!("{{\"error\": \"{e}\"}}"))
        }
        None => format!("{{\"error\": \"Unknown code: {code}\"}}"),
    }
}
