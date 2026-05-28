//! Diagnostic specification — the data model for a single error code.
//!
//! Each `DiagnosticSpec` is loaded from a TOML file (via build.rs codegen
//! or runtime loading) and stored in the global registry.

use crate::code::DiagnosticCode;
use crate::severity::DiagnosticSeverity;

/// The full diagnostic specification for one error code.
#[derive(Debug, Clone)]
pub struct DiagnosticSpec {
    pub code: DiagnosticCode,
    pub title: String,
    pub severity: DiagnosticSeverity,
    pub docs_key: String,
    /// Multi-paragraph help text explaining what happened, why it's wrong,
    /// and how to fix it.
    pub help: String,
    /// Optional "bad code" example.
    pub example_bad: Option<String>,
    /// Optional "good code" example.
    pub example_good: Option<String>,
    /// Suggested fix-it replacement string (for simple cases).
    pub fixit: Option<String>,
    /// Cross-references to related error codes.
    pub see_also: Vec<String>,
}

impl DiagnosticSpec {
    /// Format a single-line summary suitable for `kain explain --short`.
    pub fn short_summary(&self) -> String {
        format!("{} — {}", self.code, self.title)
    }

    /// Format a full explanation suitable for `kain explain CODE`.
    pub fn full_explanation(&self) -> String {
        let mut out = String::new();
        out.push_str(&format!("╔══ {} — {}\n", self.code, self.title));
        out.push_str(&format!("║  Severity: {}\n", self.severity));
        out.push_str(&format!("║  Docs:    {}\n", self.docs_key));
        out.push_str("╠══════════════════════════════════════════════\n");

        // Help text with indentation
        for line in self.help.lines() {
            out.push_str(&format!("║  {}\n", line.trim()));
        }

        if let Some(ref bad) = self.example_bad {
            out.push_str("╟── Example (bad) ─────────────────────────────\n");
            for line in bad.lines() {
                out.push_str(&format!("║  ❌ {}\n", line));
            }
        }
        if let Some(ref good) = self.example_good {
            out.push_str("╟── Example (good) ────────────────────────────\n");
            for line in good.lines() {
                out.push_str(&format!("║  ✅ {}\n", line));
            }
        }
        if let Some(ref fix) = self.fixit {
            out.push_str(&format!("╟── Suggested fix: replace with `{fix}`\n"));
        }
        if !self.see_also.is_empty() {
            out.push_str("╟── See also ──────────────────────────────────\n");
            for related in &self.see_also {
                out.push_str(&format!("║  • {related}\n"));
            }
        }
        out.push_str("╚══════════════════════════════════════════════\n");
        out
    }
}

// ── Build-time codegen support ────────────────────────────────────────

/// A flat, serializable representation used by the build.rs codegen.
/// Converted into a `DiagnosticSpec` at startup.
#[derive(Debug, Clone)]
pub struct GeneratedSpec {
    pub code: &'static str,
    pub title: &'static str,
    pub severity: &'static str,
    pub docs_key: &'static str,
    pub help: &'static str,
    pub example_bad: Option<&'static str>,
    pub example_good: Option<&'static str>,
    pub fixit: Option<&'static str>,
    pub see_also: &'static [&'static str],
}

impl GeneratedSpec {
    pub fn into_spec(&self) -> DiagnosticSpec {
        DiagnosticSpec {
            code: DiagnosticCode::new(self.code),
            title: self.title.to_string(),
            severity: DiagnosticSeverity::from_str(self.severity),
            docs_key: self.docs_key.to_string(),
            help: self.help.to_string(),
            example_bad: self.example_bad.map(|s| s.to_string()),
            example_good: self.example_good.map(|s| s.to_string()),
            fixit: self.fixit.map(|s| s.to_string()),
            see_also: self.see_also.iter().map(|s| s.to_string()).collect(),
        }
    }
}
