//! Error chain management: causality, suppression, deduplication,
//! and error budgets.
//!
//! When multiple diagnostics are emitted, the chain module:
//! - Links parent/child relationships (causality).
//! - Suppresses downstream errors when an upstream error explains them.
//! - Deduplicates identical errors at the same span.
//! - Enforces error budgets (stop reporting after N errors).

use crate::report::DiagnosticReport;
use crate::span::Span;

/// A chain of related diagnostics with causality links.
#[derive(Debug, Clone)]
pub struct DiagnosticChain {
    /// The root-cause diagnostic.
    pub root: DiagnosticReport,
    /// Diagnostics caused by the root error.
    pub children: Vec<DiagnosticChainEntry>,
}

/// A child diagnostic with its relationship to the parent.
#[derive(Debug, Clone)]
pub struct DiagnosticChainEntry {
    pub report: DiagnosticReport,
    /// How this diagnostic relates to the parent.
    pub relation: ChainRelation,
}

/// How a child diagnostic relates to its parent.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ChainRelation {
    /// This error is a direct consequence of the parent.
    CausedBy,
    /// This is additional context for the parent error.
    ContextFor,
    /// This error would not exist if the parent were fixed.
    SuppressedByParent,
}

impl DiagnosticChain {
    pub fn new(root: DiagnosticReport) -> Self {
        Self {
            root,
            children: Vec::new(),
        }
    }

    pub fn with_child(mut self, report: DiagnosticReport, relation: ChainRelation) -> Self {
        self.children
            .push(DiagnosticChainEntry { report, relation });
        self
    }

    /// Total number of diagnostics in this chain.
    pub fn len(&self) -> usize {
        1 + self.children.len()
    }

    pub fn is_empty(&self) -> bool {
        false // root always exists
    }
}

// ── Deduplication ────────────────────────────────────────────────────

/// Deduplicate diagnostics: if two diagnostics share the same code and
/// primary span, keep only the first.
pub fn deduplicate(reports: Vec<DiagnosticReport>) -> Vec<DiagnosticReport> {
    let mut seen: Vec<(crate::code::DiagnosticCode, Option<Span>)> = Vec::new();
    let mut out = Vec::with_capacity(reports.len());

    for report in reports {
        let key = (report.code, report.primary_span);
        if !seen.contains(&key) {
            seen.push(key);
            out.push(report);
        }
    }

    out
}

// ── Error budget ─────────────────────────────────────────────────────

/// Enforce an error budget: after `max_errors`, suppress remaining
/// diagnostics and append a summary note.
pub fn enforce_budget(reports: Vec<DiagnosticReport>, max_errors: usize) -> Vec<DiagnosticReport> {
    if reports.len() <= max_errors {
        return reports;
    }

    let suppressed = reports.len() - max_errors;
    let mut out: Vec<DiagnosticReport> = reports.into_iter().take(max_errors).collect();

    let mut summary = DiagnosticReport::new_default(
        crate::report::ErrorKind::Internal,
        format!(
            "error budget exceeded: {suppressed} additional error(s) suppressed. \
             Re-run with `--max-errors 0` to see all errors."
        ),
    );
    summary.severity = crate::severity::DiagnosticSeverity::Note;
    out.push(summary);

    out
}

// ── Suppression ──────────────────────────────────────────────────────

/// Suppress common "cascading" errors. When a `KAIN-PARSE-0005`
/// (missing delimiter) fires, suppress any `KAIN-PARSE-0007`
/// (expected contextual keyword) at the same location since it's
/// caused by the missing delimiter.
pub fn suppress_cascading(reports: Vec<DiagnosticReport>) -> Vec<DiagnosticReport> {
    // Simple heuristic: track spans where a structural parse error fired.
    let structural_spans: Vec<Span> = reports
        .iter()
        .filter(|r| {
            r.code == "KAIN-PARSE-0005"
                || r.code == "KAIN-PARSE-0002"
                || r.code == "KAIN-PARSE-0008"
        })
        .filter_map(|r| r.primary_span)
        .collect();

    reports
        .into_iter()
        .filter(|r| {
            if let Some(span) = r.primary_span {
                // If a structural error already fired at this span, suppress
                // more specific downstream errors at the same location.
                if r.code == "KAIN-PARSE-0007" || r.code == "KAIN-PARSE-0003" {
                    return !structural_spans.iter().any(|s| s.start == span.start);
                }
            }
            true
        })
        .collect()
}
