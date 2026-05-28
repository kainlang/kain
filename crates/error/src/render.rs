//! Terminal diagnostic renderer with ANSI color, Unicode-aware
//! highlight placement, and multi-line source context.
//!
//! Renders `DiagnosticReport` into human-readable terminal output
//! inspired by Rust's diagnostic style: file locations, line-number
//! gutters, colored severity labels, and `^~~~` underlines.

use crate::label::FixItConfidence;
use crate::registry::spec_for_code;
use crate::report::DiagnosticReport;
use crate::severity::DiagnosticSeverity;
use crate::source::SpanMapper;
use crate::trace::format_trace;

// ── ANSI palette ─────────────────────────────────────────────────────

#[derive(Debug, Clone, Copy)]
pub struct TerminalPalette {
    pub enabled: bool,
}

impl TerminalPalette {
    pub fn new(enabled: bool) -> Self {
        Self { enabled }
    }

    fn sgr(&self, code: &str, text: &str) -> String {
        if self.enabled {
            format!("\x1b[{code}m{text}\x1b[0m")
        } else {
            text.to_string()
        }
    }

    fn bold(&self, text: &str) -> String {
        self.sgr("1", text)
    }

    fn red(&self, text: &str) -> String {
        self.sgr("1;31", text)
    }

    fn yellow(&self, text: &str) -> String {
        self.sgr("1;33", text)
    }

    fn cyan(&self, text: &str) -> String {
        self.sgr("1;36", text)
    }

    fn blue(&self, text: &str) -> String {
        self.sgr("1;34", text)
    }

    fn dim(&self, text: &str) -> String {
        self.sgr("2", text)
    }

    fn green(&self, text: &str) -> String {
        self.sgr("1;32", text)
    }

    pub fn severity_color(&self, severity: DiagnosticSeverity, text: &str) -> String {
        match severity {
            DiagnosticSeverity::Error => self.red(text),
            DiagnosticSeverity::Warning => self.yellow(text),
            DiagnosticSeverity::Note => self.cyan(text),
            DiagnosticSeverity::Help => self.green(text),
        }
    }

    pub fn gutter(&self, text: &str) -> String {
        self.dim(text)
    }

    pub fn pointer(&self, text: &str) -> String {
        self.bold(text)
    }
}

// ── Renderer ──────────────────────────────────────────────────────────

/// Terminal diagnostic renderer.
pub struct DiagnosticRenderer {
    span_mapper: SpanMapper,
    filename: String,
    palette: TerminalPalette,
    /// When true, include debug traces in output.
    show_debug_trace: bool,
}

impl DiagnosticRenderer {
    pub fn new(source: &str, filename: &str, use_color: bool) -> Self {
        Self {
            span_mapper: SpanMapper::new(source),
            filename: filename.to_string(),
            palette: TerminalPalette::new(use_color),
            show_debug_trace: false,
        }
    }

    pub fn with_mapper(
        span_mapper: SpanMapper,
        filename: impl Into<String>,
        use_color: bool,
    ) -> Self {
        Self {
            span_mapper,
            filename: filename.into(),
            palette: TerminalPalette::new(use_color),
            show_debug_trace: false,
        }
    }

    pub fn show_debug_trace(mut self, show: bool) -> Self {
        self.show_debug_trace = show;
        self
    }

    /// Render a single diagnostic report with source context.
    pub fn render(&self, report: &DiagnosticReport) -> String {
        let p = &self.palette;
        let spec = spec_for_code(report.code);
        let code_str = spec.as_ref().map(|s| s.code.as_str()).unwrap_or("UNKNOWN");

        let mut out = String::new();

        // ── Header line ──────────────────────────────────────────────
        let severity_label = format!("{}[{}:{}]", report.severity, report.kind, code_str);
        out.push_str(&format!(
            "\n{}: {}\n",
            p.severity_color(report.severity, &severity_label),
            p.bold(&report.message)
        ));

        // ── Phase attribution ───────────────────────────────────────
        if report.phase != crate::report::CompilerPhase::Unknown {
            out.push_str(&format!("   {} phase: {}\n", p.dim("="), report.phase));
        }

        // ── Source location + code snippet ──────────────────────────
        if let Some(span) = report.primary_span {
            let (range, line_content, pointer_offset, pointer_len) = self
                .span_mapper
                .span_to_display_context(span, self.filename.as_str());

            let file = report
                .file
                .as_ref()
                .map(|path| path.display().to_string())
                .unwrap_or_else(|| range.file.clone());

            let line_num = range.start.line;

            // File location
            out.push_str(&format!(
                "  {} {}:{}:{}\n",
                p.gutter("-->"),
                file,
                line_num,
                range.start.col
            ));
            out.push_str(&format!("   {}\n", p.gutter("|")));

            // Source line
            out.push_str(&format!(
                "{} {}\n",
                p.gutter(&format!("{line_num:>3} |")),
                line_content
            ));

            // Pointer underline
            let primary_label = report
                .labels
                .iter()
                .find(|label| label.primary && label.span == span)
                .or_else(|| report.labels.iter().find(|label| label.span == span));

            out.push_str(&format!(
                "   {} {}{}",
                p.gutter("|"),
                " ".repeat(pointer_offset),
                p.severity_color(report.severity, &"^".repeat(pointer_len)),
            ));
            if let Some(label) = primary_label {
                out.push_str(&format!(
                    " {}",
                    p.severity_color(report.severity, &label.message)
                ));
            }
            out.push('\n');
            out.push_str(&format!("   {}\n", p.gutter("|")));
        } else if let Some(path) = &report.file {
            out.push_str(&format!("  {} {}", p.gutter("-->"), path.display()));
            if let Some((line, col)) = report.location {
                out.push_str(&format!(":{}:{}", line, col));
            }
            out.push('\n');
        }

        // ── Secondary labels ────────────────────────────────────────
        for label in report
            .labels
            .iter()
            .filter(|label| Some(label.span) != report.primary_span)
        {
            let loc = label
                .range
                .as_ref()
                .map(|range| range.start.clone())
                .unwrap_or_else(|| {
                    self.span_mapper
                        .span_to_location(label.span, self.filename.as_str())
                });

            if label.primary {
                out.push_str(&format!(
                    "   {} {}:{}:{}: {}\n",
                    p.red("="),
                    loc.file,
                    loc.line,
                    loc.col,
                    p.bold(&label.message)
                ));
            } else {
                let kind_hint = match label.kind {
                    crate::label::LabelKind::Origin => "(origin) ",
                    crate::label::LabelKind::Definition => "(defined here) ",
                    crate::label::LabelKind::BorrowEnd => "(borrow ends here) ",
                    crate::label::LabelKind::MovedHere => "(moved here) ",
                    crate::label::LabelKind::RequiredBy => "(required by) ",
                    _ => "",
                };
                out.push_str(&format!(
                    "   {} {}:{}:{}: {}{}\n",
                    p.blue("="),
                    loc.file,
                    loc.line,
                    loc.col,
                    kind_hint,
                    label.message
                ));
            }
        }

        // ── Notes ──────────────────────────────────────────────────
        for note in &report.notes {
            out.push_str(&format!("   {} note: {}\n", p.cyan("="), note));
        }

        // ── Help ───────────────────────────────────────────────────
        for help in &report.help {
            out.push_str(&format!("   {} help: {}\n", p.green("="), help));
        }

        // ── Registry help ──────────────────────────────────────────
        if let Some(spec) = &spec {
            if let Some(ref fix) = spec.fixit {
                out.push_str(&format!(
                    "   {} help: suggested fix: `{fix}`\n",
                    p.green("=")
                ));
            }
            if !spec.see_also.is_empty() {
                out.push_str(&format!(
                    "   {} see also: {}\n",
                    p.dim("="),
                    spec.see_also.join(", ")
                ));
            }
        }

        // ── Fix-its ────────────────────────────────────────────────
        for fixit in &report.fixits {
            let confidence = match fixit.confidence {
                FixItConfidence::Certain => "",
                FixItConfidence::Likely => " (likely)",
                FixItConfidence::Tentative => " (tentative)",
            };
            if let Some(range) = &fixit.range {
                out.push_str(&format!(
                    "   {} fix-it{confidence} {}:{}:{}: {} -> {:?}\n",
                    p.green("="),
                    range.file,
                    range.start.line,
                    range.start.col,
                    fixit.message,
                    fixit.replacement
                ));
            } else {
                out.push_str(&format!(
                    "   {} fix-it{confidence}: {} -> {:?}\n",
                    p.green("="),
                    fixit.message,
                    fixit.replacement
                ));
            }
        }

        // ── Tags ───────────────────────────────────────────────────
        for tag in &report.tags {
            out.push_str(&format!("   {} tag: {}\n", p.dim("="), tag));
        }

        // ── Registry docs reference ────────────────────────────────
        if let Some(spec) = &spec {
            if !spec.docs_key.is_empty() {
                out.push_str(&format!("   {} docs: {}\n", p.dim("="), spec.docs_key));
            }
        }

        // ── Debug trace ────────────────────────────────────────────
        if self.show_debug_trace && !report.debug_trace.is_empty() {
            out.push_str(&format_trace(&report.debug_trace));
        }

        out
    }

    /// Render multiple diagnostics with an optional error budget summary.
    pub fn render_all(&self, reports: &[DiagnosticReport]) -> String {
        let mut out = String::new();
        let total = reports.len();
        let error_count = reports.iter().filter(|r| r.severity.is_error()).count();
        let warning_count = reports
            .iter()
            .filter(|r| r.severity == DiagnosticSeverity::Warning)
            .count();

        for report in reports {
            out.push_str(&self.render(report));
        }

        // Summary footer
        if total > 0 {
            let p = &self.palette;
            let mut summary = format!("\n{}", p.bold("── Diagnostics summary ──\n"));
            if error_count > 0 {
                summary.push_str(&format!("   {} error(s): {error_count}\n", p.red("•")));
            }
            if warning_count > 0 {
                summary.push_str(&format!(
                    "   {} warning(s): {warning_count}\n",
                    p.yellow("•")
                ));
            }
            let other = total - error_count - warning_count;
            if other > 0 {
                summary.push_str(&format!("   {} other: {other}\n", p.dim("•")));
            }
            out.push_str(&format!("{summary}\n"));
        }

        out
    }
}

// ── Convenience functions ────────────────────────────────────────────

/// Format a single diagnostic for terminal display (with color).
pub fn format_diagnostic(
    source: &str,
    filename: &str,
    report: &DiagnosticReport,
    use_color: bool,
) -> String {
    DiagnosticRenderer::new(source, filename, use_color).render(report)
}

/// Format multiple diagnostics for terminal display (with color).
pub fn format_diagnostics(
    source: &str,
    filename: &str,
    reports: &[DiagnosticReport],
    use_color: bool,
) -> String {
    DiagnosticRenderer::new(source, filename, use_color).render_all(reports)
}
