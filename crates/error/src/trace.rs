//! Compiler debug trace — decision traces for compiler developers.
//!
//! When `--debug-errors` is enabled, every diagnostic carries a trace
//! of the compiler's internal decisions that led to the error. This is
//! invaluable for debugging the compiler itself.

use crate::report::{CompilerPhase, DebugTraceEntry};
use crate::span::Span;

/// A trace accumulator that collects debugging decisions.
#[derive(Debug, Clone, Default)]
pub struct DebugTrace {
    entries: Vec<DebugTraceEntry>,
    /// When true, trace collection is active.
    enabled: bool,
}

impl DebugTrace {
    pub fn new() -> Self {
        Self {
            entries: Vec::new(),
            enabled: false,
        }
    }

    pub fn enabled(mut self) -> Self {
        self.enabled = true;
        self
    }

    pub fn with_enabled(mut self, enabled: bool) -> Self {
        self.enabled = enabled;
        self
    }

    pub fn is_enabled(&self) -> bool {
        self.enabled
    }

    /// Record a trace entry (no-op if disabled).
    pub fn trace(&mut self, phase: CompilerPhase, message: impl Into<String>) {
        if self.enabled {
            self.entries.push(DebugTraceEntry {
                phase,
                message: message.into(),
                span: None,
            });
        }
    }

    /// Record a trace entry with a source span.
    pub fn trace_span(
        &mut self,
        phase: CompilerPhase,
        span: Span,
        message: impl Into<String>,
    ) {
        if self.enabled {
            self.entries.push(DebugTraceEntry {
                phase,
                message: message.into(),
                span: Some(span),
            });
        }
    }

    /// Consume the trace and attach it to a diagnostic report.
    pub fn attach_to(
        self,
        report: &mut crate::report::DiagnosticReport,
    ) {
        report.debug_trace = self.entries;
    }

    /// Drain all entries into a `Vec`.
    pub fn drain(&mut self) -> Vec<DebugTraceEntry> {
        std::mem::take(&mut self.entries)
    }

    pub fn entries(&self) -> &[DebugTraceEntry] {
        &self.entries
    }

    pub fn len(&self) -> usize {
        self.entries.len()
    }

    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }
}

/// Format a debug trace for terminal output.
pub fn format_trace(entries: &[DebugTraceEntry]) -> String {
    if entries.is_empty() {
        return String::new();
    }

    let mut out = String::new();
    out.push_str("\n── Compiler Debug Trace ──\n");
    for (i, entry) in entries.iter().enumerate() {
        out.push_str(&format!(
            "  [{i:>3}] {:<18} {}\n",
            entry.phase.to_string(),
            entry.message
        ));
        if let Some(span) = entry.span {
            out.push_str(&format!(
                "       {:>18} at bytes {}..{}\n",
                "", span.start, span.end
            ));
        }
    }
    out.push_str(&format!("── {n} trace entries ──\n", n = entries.len()));
    out
}
