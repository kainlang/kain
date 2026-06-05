//! Terminal diagnostic renderer with lattice-backed ANSI coloring,
//! Unicode-aware highlight placement, and multi-line source context.
//!
//! Renders `DiagnosticReport` into human-readable terminal output
//! inspired by Rust's diagnostic style: file locations, line-number
//! gutters, colored severity labels, and `^~~~` underlines.
//!
//! Now uses `kain_lattice::Painter` instead of hardcoded ANSI codes.

use crate::label::FixItConfidence;
use crate::registry::spec_for_code;
use crate::report::DiagnosticReport;
use crate::severity::DiagnosticSeverity;
use crate::source::SpanMapper;
use crate::trace::format_trace;
use kain_lattice::{Painter, SemanticRole, theme_by_name};

fn severity_role(severity: DiagnosticSeverity) -> SemanticRole {
    match severity {
        DiagnosticSeverity::Error => SemanticRole::DiagError,
        DiagnosticSeverity::Warning => SemanticRole::DiagWarning,
        DiagnosticSeverity::Note => SemanticRole::DiagNote,
        DiagnosticSeverity::Help => SemanticRole::DiagHelp,
    }
}

fn painter_severity(painter: &Painter, severity: DiagnosticSeverity, text: &str) -> String {
    let role = severity_role(severity);
    painter.bold(role, text)
}

fn normalize_diag_text(text: &str) -> String {
    text.split_whitespace()
        .collect::<Vec<_>>()
        .join(" ")
        .trim()
        .to_ascii_lowercase()
}

// ── Renderer ──────────────────────────────────────────────────────────

/// Terminal diagnostic renderer.
pub struct DiagnosticRenderer {
    span_mapper: SpanMapper,
    filename: String,
    painter: Painter,
    /// When true, include debug traces in output.
    show_debug_trace: bool,
}

impl DiagnosticRenderer {
    pub fn new(source: &str, filename: &str, use_color: bool) -> Self {
        let theme = theme_by_name("slate");
        Self {
            span_mapper: SpanMapper::new(source),
            filename: filename.to_string(),
            painter: Painter::new(theme, use_color),
            show_debug_trace: false,
        }
    }

    pub fn with_mapper(
        span_mapper: SpanMapper,
        filename: impl Into<String>,
        use_color: bool,
    ) -> Self {
        let theme = theme_by_name("slate");
        Self {
            span_mapper,
            filename: filename.into(),
            painter: Painter::new(theme, use_color),
            show_debug_trace: false,
        }
    }

    pub fn show_debug_trace(mut self, show: bool) -> Self {
        self.show_debug_trace = show;
        self
    }

    /// Render a single diagnostic report with source context.
    pub fn render(&self, report: &DiagnosticReport) -> String {
        let spec = spec_for_code(report.code);
        let code_str = spec.as_ref().map(|s| s.code.as_str()).unwrap_or("UNKNOWN");

        let mut out = String::new();

        // ── Header line ──────────────────────────────────────────────
        let severity_label = format!("{}[{}:{}]", report.severity, report.kind, code_str);
        out.push_str(&format!(
            "\n{}: {}\n",
            painter_severity(&self.painter, report.severity, &severity_label),
            self.painter.bold(severity_role(report.severity), &report.message)
        ));

        // ── Phase attribution ───────────────────────────────────────
        if report.phase != crate::report::CompilerPhase::Unknown {
            out.push_str(&format!("   {} phase: {}\n", self.painter.dim(SemanticRole::DiagGutter, "="), report.phase));
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
                self.painter.gutter("-->"),
                file,
                line_num,
                range.start.col
            ));
            out.push_str(&format!("   {}\n", self.painter.gutter("|")));

            // Source line
            let highlighted_line = self.painter.source_line(line_content);
            out.push_str(&format!(
                "{} {}\n",
                self.painter.gutter(&format!("{line_num:>3} |")),
                highlighted_line
            ));

            // Pointer underline
            let primary_label = report
                .labels
                .iter()
                .find(|label| label.primary && label.span == span)
                .or_else(|| report.labels.iter().find(|label| label.span == span));

            out.push_str(&format!(
                "   {} {}{}",
                self.painter.gutter("|"),
                " ".repeat(pointer_offset),
                self.painter.bold(severity_role(report.severity), &"^".repeat(pointer_len)),
            ));
            if let Some(label) = primary_label {
                out.push_str(&format!(
                    " {}",
                    self.painter.bold(severity_role(report.severity), &label.message)
                ));
            }
            out.push('\n');
            out.push_str(&format!("   {}\n", self.painter.gutter("|")));
        } else if let Some(path) = &report.file {
            out.push_str(&format!("  {} {}", self.painter.gutter("-->"), path.display()));
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
                    self.painter.diag_error("="),
                    loc.file,
                    loc.line,
                    loc.col,
                    self.painter.bold(SemanticRole::DiagError, &label.message)
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
                    self.painter.note("="),
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
            out.push_str(&format!("   {} note: {}\n", self.painter.note("="), note));
        }
        let normalized_notes = report
            .notes
            .iter()
            .map(|note| normalize_diag_text(note))
            .collect::<Vec<_>>();
        let normalized_help = report
            .help
            .iter()
            .map(|help| normalize_diag_text(help))
            .collect::<Vec<_>>();
        if let Some(semantic) = &report.semantic {
            let normalized_explanation = normalize_diag_text(&semantic.explanation);
            if !normalized_explanation.is_empty()
                && !normalized_notes.contains(&normalized_explanation)
            {
                out.push_str(&format!(
                    "   {} note: {}\n",
                    self.painter.note("="),
                    semantic.explanation
                ));
            }
            if semantic.cascade_probability >= 0.55 {
                out.push_str(&format!(
                    "   {} note: later diagnostics may cascade from this root error.\n",
                    self.painter.note("=")
                ));
            }
            if let Some(repair) = semantic.repairs.first() {
                let normalized_repair = normalize_diag_text(&repair.description);
                let fixit_duplicate = report
                    .fixits
                    .iter()
                    .any(|fixit| normalize_diag_text(&fixit.message) == normalized_repair);
                if !normalized_repair.is_empty()
                    && !normalized_help.contains(&normalized_repair)
                    && !fixit_duplicate
                {
                    out.push_str(&format!(
                        "   {} help: {}\n",
                        self.painter.help("="),
                        repair.description
                    ));
                }
            }
        }

        // ── Help ───────────────────────────────────────────────────
        for help in &report.help {
            out.push_str(&format!("   {} help: {}\n", self.painter.help("="), help));
        }

        // ── Registry help ──────────────────────────────────────────
        if let Some(spec) = &spec {
            if let Some(ref fix) = spec.fixit {
                out.push_str(&format!(
                    "   {} help: suggested fix: `{fix}`\n",
                    self.painter.help("=")
                ));
            }
            if !spec.see_also.is_empty() {
                out.push_str(&format!(
                    "   {} see also: {}\n",
                    self.painter.dim(SemanticRole::DiagGutter, "="),
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
                    self.painter.help("="),
                    range.file,
                    range.start.line,
                    range.start.col,
                    fixit.message,
                    fixit.replacement
                ));
            } else {
                let loc = self
                    .span_mapper
                    .span_to_location(fixit.span, self.filename.as_str());
                out.push_str(&format!(
                    "   {} fix-it{confidence} {}:{}:{}: {} -> {:?}\n",
                    self.painter.help("="),
                    loc.file,
                    loc.line,
                    loc.col,
                    fixit.message,
                    fixit.replacement
                ));
            }
        }

        // ── Tags ───────────────────────────────────────────────────
        for tag in &report.tags {
            out.push_str(&format!("   {} tag: {}\n", self.painter.dim(SemanticRole::DiagGutter, "="), tag));
        }

        // ── Registry docs reference ────────────────────────────────
        if let Some(spec) = &spec {
            if !spec.docs_key.is_empty() {
                out.push_str(&format!("   {} docs: {}\n", self.painter.dim(SemanticRole::DiagGutter, "="), spec.docs_key));
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
            let mut summary = format!("\n{}", self.painter.banner("── Diagnostics summary ──\n"));
            if error_count > 0 {
                summary.push_str(&format!("   {} error(s): {error_count}\n", self.painter.diag_error("•")));
            }
            if warning_count > 0 {
                summary.push_str(&format!(
                    "   {} warning(s): {warning_count}\n",
                    self.painter.diag_warning("•")
                ));
            }
            let other = total - error_count - warning_count;
            if other > 0 {
                summary.push_str(&format!("   {} other: {other}\n", self.painter.dim(SemanticRole::DiagGutter, "•")));
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
