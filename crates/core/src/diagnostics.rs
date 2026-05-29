//! Pretty error reporting for KAIN
//! Shows source context with line numbers and error highlighting

use crate::diagnostic_registry::spec_for_code;
use crate::error::{DiagnosticReport, DiagnosticSeverity, KainError};
use crate::span::Span;
use crate::tooling_config::{active_color_preference, active_ui_theme_name};
pub use kain_error::{SourceLocation, SourceOriginSegment, SpanMapper};
use kain_lattice::{theme_by_name, LatticeTheme, SemanticRole};

#[derive(Debug, Clone, Copy)]
struct DiagnosticPalette {
    enabled: bool,
    theme: &'static LatticeTheme,
}

impl DiagnosticPalette {
    fn paint(&self, role: SemanticRole, text: &str) -> String {
        self.theme.ansi_paint(role, text, self.enabled)
    }

    fn severity_text(&self, severity: DiagnosticSeverity, text: &str) -> String {
        match severity {
            DiagnosticSeverity::Error => self.paint(SemanticRole::DiagError, text),
            DiagnosticSeverity::Warning => self.paint(SemanticRole::DiagWarning, text),
            DiagnosticSeverity::Note => self.paint(SemanticRole::DiagNote, text),
            DiagnosticSeverity::Help => self.paint(SemanticRole::DiagHelp, text),
        }
    }

    fn error_text(&self, text: &str) -> String {
        self.paint(SemanticRole::DiagError, text)
    }

    fn note_text(&self, text: &str) -> String {
        self.paint(SemanticRole::DiagNote, text)
    }

    fn gutter_text(&self, text: &str) -> String {
        self.paint(SemanticRole::DiagGutter, text)
    }

    fn pointer_text(&self, text: &str) -> String {
        self.paint(SemanticRole::DiagPointer, text)
    }
}

fn active_diagnostic_palette() -> DiagnosticPalette {
    let enabled = active_color_preference().should_color_stderr();
    let theme = active_ui_theme_name();
    DiagnosticPalette {
        enabled,
        theme: theme_by_name(&theme),
    }
}

fn normalize_diag_text(text: &str) -> String {
    text.split_whitespace()
        .collect::<Vec<_>>()
        .join(" ")
        .trim()
        .to_ascii_lowercase()
}

/// Diagnostic renderer for pretty error messages
pub struct Diagnostics {
    span_mapper: SpanMapper,
    filename: String,
}

impl Diagnostics {
    pub fn new(source: &str, filename: &str) -> Self {
        Self::with_mapper(SpanMapper::new(source), filename)
    }

    pub fn with_mapper(span_mapper: SpanMapper, filename: impl Into<String>) -> Self {
        Self {
            span_mapper,
            filename: filename.into(),
        }
    }

    pub fn get_line_info(&self, span: Span) -> (usize, usize, &str) {
        let (location, line_content) = self.span_mapper.span_to_line_info(span, &self.filename);
        (location.line, location.col, line_content)
    }

    /// Format an error with source context
    pub fn format_error(&self, error: &KainError) -> String {
        let palette = active_diagnostic_palette();
        match error {
            KainError::Lexer { message, span } => {
                self.format_with_context(&palette, "Lexer Error", message, *span)
            }
            KainError::Parser { message, span } => {
                self.format_with_context(&palette, "Parse Error", message, *span)
            }
            KainError::Type { message, span } => {
                self.format_with_context(&palette, "Type Error", message, *span)
            }
            KainError::Effect { message, span } => {
                self.format_with_context(&palette, "Effect Error", message, *span)
            }
            KainError::Borrow { message, span } => {
                self.format_with_context(&palette, "Borrow Error", message, *span)
            }
            KainError::Codegen { message, span } => {
                self.format_with_context(&palette, "Codegen Error", message, *span)
            }
            KainError::CodegenWithLocation {
                message,
                file,
                line,
                col,
                ..
            } => format!(
                "\n{}: {}\n  {} {}:{}:{}\n",
                palette.error_text("error[Codegen]"),
                message,
                palette.gutter_text("-->"),
                file,
                line,
                col
            ),
            KainError::Runtime { message } => {
                format!("\n{}: {}\n", palette.error_text("error"), message)
            }
            KainError::Io(e) => format!("\n{}: IO error: {}\n", palette.error_text("error"), e),
            KainError::Enhanced { .. } => error
                .to_diagnostic_reports()
                .into_iter()
                .next()
                .map(|report| self.format_diagnostic_report(&palette, &report))
                .unwrap_or_else(|| format!("\n{}\n", error)),
            KainError::Rich(report) => self.format_diagnostic_report(&palette, report),
            KainError::Multi(errors) => {
                if errors.len() == 1 {
                    return self.format_error(&errors[0]);
                }
                let mut output = format!(
                    "\n{}: {} error(s) found:\n",
                    palette.error_text("error"),
                    errors.len()
                );
                for (i, err) in errors.iter().enumerate() {
                    output.push_str(&format!("\n--- [{}/{}] ---\n", i + 1, errors.len()));
                    output.push_str(&self.format_error(err));
                }
                output
            }
        }
    }

    fn format_diagnostic_report(
        &self,
        palette: &DiagnosticPalette,
        report: &DiagnosticReport,
    ) -> String {
        let spec = spec_for_code(report.code);
        let mut output = String::new();
        output.push_str(&format!(
            "\n{}: {}\n",
            palette.severity_text(
                report.severity,
                &format!("{}[{}:{}]", report.severity, report.kind, spec.code_str)
            ),
            report.message
        ));

        if let Some(span) = report.primary_span {
            let (range, line_content, pointer_offset, pointer_len) = self
                .span_mapper
                .span_to_display_context(span, self.filename.as_str());
            let file = report
                .file
                .as_ref()
                .map(|path| path.display().to_string())
                .unwrap_or_else(|| range.file.clone());
            output.push_str(&format!(
                "  {} {}:{}:{}\n",
                palette.gutter_text("-->"),
                file,
                range.start.line,
                range.start.col
            ));
            output.push_str(&format!("   {}\n", palette.gutter_text("|")));
            output.push_str(&format!(
                "{} {}\n",
                palette.gutter_text(&format!("{:>3} |", range.start.line)),
                line_content
            ));
            let primary_label = report
                .labels
                .iter()
                .find(|label| label.primary && label.span == span)
                .or_else(|| report.labels.iter().find(|label| label.span == span));
            output.push_str(&format!(
                "   {} {}{}",
                palette.gutter_text("|"),
                " ".repeat(pointer_offset),
                palette.pointer_text(&"^".repeat(pointer_len)),
            ));
            if let Some(label) = primary_label {
                output.push_str(&format!(" {}", label.message));
            }
            output.push('\n');
            output.push_str(&format!("   {}\n", palette.gutter_text("|")));
        } else if let Some(path) = &report.file {
            output.push_str(&format!(
                "  {} {}",
                palette.gutter_text("-->"),
                path.display()
            ));
            if let Some((line, col)) = report.location {
                output.push_str(&format!(":{}:{}", line, col));
            }
            output.push('\n');
        }

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
            output.push_str(&format!(
                "   {} label {}:{}: {}\n",
                palette.note_text("="),
                loc.line,
                loc.col,
                label.message
            ));
        }
        for note in &report.notes {
            output.push_str(&format!("   {} note: {}\n", palette.note_text("="), note));
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
                output.push_str(&format!(
                    "   {} note: {}\n",
                    palette.note_text("="),
                    semantic.explanation
                ));
            }
            if semantic.cascade_probability >= 0.55 {
                output.push_str(&format!(
                    "   {} note: later diagnostics may cascade from this root error.\n",
                    palette.note_text("=")
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
                    output.push_str(&format!(
                        "   {} help: {}\n",
                        palette.note_text("="),
                        repair.description
                    ));
                }
            }
        }
        for help in &report.help {
            output.push_str(&format!("   {} help: {}\n", palette.note_text("="), help));
        }
        for fixit in &report.fixits {
            if let Some(range) = &fixit.range {
                output.push_str(&format!(
                    "   {} fix-it {}:{}:{}: {} -> {:?}\n",
                    palette.note_text("="),
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
                output.push_str(&format!(
                    "   {} fix-it {}:{}:{}: {} -> {:?}\n",
                    palette.note_text("="),
                    loc.file,
                    loc.line,
                    loc.col,
                    fixit.message,
                    fixit.replacement
                ));
            }
        }
        for tag in &report.tags {
            output.push_str(&format!("   {} tag: {}\n", palette.note_text("="), tag));
        }
        let semantic_help_available = report
            .semantic
            .as_ref()
            .and_then(|semantic| semantic.repairs.first())
            .map(|repair| !normalize_diag_text(&repair.description).is_empty())
            .unwrap_or(false);
        if report.help.is_empty() && !semantic_help_available {
            if let Some(default_suggestion) = spec.default_suggestion {
                output.push_str(&format!(
                    "   {} help: {}\n",
                    palette.note_text("="),
                    default_suggestion
                ));
            }
        }
        if let Some(docs_key) = spec.docs_key {
            output.push_str(&format!(
                "   {} reference: {}\n",
                palette.note_text("="),
                docs_key
            ));
        }
        output
    }

    fn format_with_context(
        &self,
        palette: &DiagnosticPalette,
        error_type: &str,
        message: &str,
        span: Span,
    ) -> String {
        let (range, line_content, pointer_offset, pointer_len) = self
            .span_mapper
            .span_to_display_context(span, self.filename.as_str());

        let mut output = String::new();

        output.push_str(&format!(
            "\n{}: {}\n",
            palette.error_text(&format!("error[{error_type}]")),
            message
        ));

        output.push_str(&format!(
            "  {} {}:{}:{}\n",
            palette.gutter_text("-->"),
            range.file,
            range.start.line,
            range.start.col
        ));

        output.push_str(&format!("   {}\n", palette.gutter_text("|")));

        output.push_str(&format!(
            "{} {}\n",
            palette.gutter_text(&format!("{:>3} |", range.start.line)),
            line_content
        ));

        output.push_str(&format!(
            "   {} {}{}\n",
            palette.gutter_text("|"),
            " ".repeat(pointer_offset),
            palette.pointer_text(&"^".repeat(pointer_len))
        ));

        output.push_str(&format!("   {}\n", palette.gutter_text("|")));

        output
    }
}

/// Format an error without source context (for runtime errors)
pub fn format_simple_error(error: &KainError) -> String {
    match error {
        KainError::Runtime { message } => format!("Runtime Error: {}", message),
        _ => format!("{}", error),
    }
}

/// Convert a KainError with span to one with file:line:col location
/// This is used to enhance codegen errors with human-readable locations
pub fn enhance_error_with_location(
    error: KainError,
    span_mapper: &SpanMapper,
    file: &str,
) -> KainError {
    match error {
        KainError::Codegen { message, span } => {
            let loc = span_mapper.span_to_location(span, file);
            KainError::codegen_with_location(message, loc.file, loc.line, loc.col, span)
        }
        // Pass through other error types unchanged
        other => other,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_line_info() {
        let source = "let x = 5\nlet y = x + 1\nprint(y)";
        let diag = Diagnostics::new(source, "test.kn");

        // First line
        let (line, col, content) = diag.get_line_info(Span::new(0, 3));
        assert_eq!(line, 1);
        assert_eq!(col, 1);
        assert_eq!(content, "let x = 5");

        // Second line
        let (line, _col, content) = diag.get_line_info(Span::new(14, 15));
        assert_eq!(line, 2);
        assert_eq!(content, "let y = x + 1");
    }

    // SpanMapper tests

    #[test]
    fn test_span_mapper_basic() {
        let source = "let x = 5\nlet y = x + 1\nprint(y)";
        let mapper = SpanMapper::new(source);

        // First line, first character
        let loc = mapper.span_to_location(Span::new(0, 3), "test.kn");
        assert_eq!(loc.file, "test.kn");
        assert_eq!(loc.line, 1);
        assert_eq!(loc.col, 1);

        // First line, middle
        let loc = mapper.span_to_location(Span::new(4, 5), "test.kn");
        assert_eq!(loc.line, 1);
        assert_eq!(loc.col, 5);

        // Second line, first character
        let loc = mapper.span_to_location(Span::new(10, 13), "test.kn");
        assert_eq!(loc.line, 2);
        assert_eq!(loc.col, 1);

        // Second line, middle
        let loc = mapper.span_to_location(Span::new(14, 15), "test.kn");
        assert_eq!(loc.line, 2);
        assert_eq!(loc.col, 5);

        // Third line
        let loc = mapper.span_to_location(Span::new(24, 29), "test.kn");
        assert_eq!(loc.line, 3);
        assert_eq!(loc.col, 1);
    }

    #[test]
    fn test_span_mapper_empty_file() {
        let source = "";
        let mapper = SpanMapper::new(source);

        let loc = mapper.span_to_location(Span::new(0, 0), "empty.kn");
        assert_eq!(loc.file, "empty.kn");
        assert_eq!(loc.line, 1);
        assert_eq!(loc.col, 1);
    }

    #[test]
    fn test_span_mapper_single_line() {
        let source = "let x = 5";
        let mapper = SpanMapper::new(source);

        // Start of line
        let loc = mapper.span_to_location(Span::new(0, 3), "single.kn");
        assert_eq!(loc.line, 1);
        assert_eq!(loc.col, 1);

        // Middle of line
        let loc = mapper.span_to_location(Span::new(4, 5), "single.kn");
        assert_eq!(loc.line, 1);
        assert_eq!(loc.col, 5);

        // End of line
        let loc = mapper.span_to_location(Span::new(8, 9), "single.kn");
        assert_eq!(loc.line, 1);
        assert_eq!(loc.col, 9);
    }

    #[test]
    fn test_span_mapper_multi_byte_chars() {
        // Unicode characters: "let x = 🚀" (rocket emoji is 4 bytes)
        let source = "let x = 🚀\nlet y = 5";
        let mapper = SpanMapper::new(source);

        // First line
        let loc = mapper.span_to_location(Span::new(0, 3), "unicode.kn");
        assert_eq!(loc.line, 1);
        assert_eq!(loc.col, 1);

        // After the emoji (byte offset 12 is the newline character).
        // Columns are character-based, while display_col tracks terminal width.
        let loc = mapper.span_to_location(Span::new(12, 13), "unicode.kn");
        assert_eq!(loc.line, 1);
        assert_eq!(loc.col, 10);
        assert_eq!(loc.display_col, 11);

        // Second line
        let loc = mapper.span_to_location(Span::new(13, 16), "unicode.kn");
        assert_eq!(loc.line, 2);
        assert_eq!(loc.col, 1);
    }

    #[test]
    fn test_span_mapper_newline_variations() {
        // Test with just \n
        let source = "line1\nline2\nline3";
        let mapper = SpanMapper::new(source);

        let loc = mapper.span_to_location(Span::new(0, 5), "test.kn");
        assert_eq!(loc.line, 1);

        let loc = mapper.span_to_location(Span::new(6, 11), "test.kn");
        assert_eq!(loc.line, 2);

        let loc = mapper.span_to_location(Span::new(12, 17), "test.kn");
        assert_eq!(loc.line, 3);
    }

    #[test]
    fn test_span_mapper_empty_lines() {
        let source = "line1\n\nline3\n\nline5";
        let mapper = SpanMapper::new(source);

        // Line 1
        let loc = mapper.span_to_location(Span::new(0, 5), "test.kn");
        assert_eq!(loc.line, 1);

        // Empty line 2 (just the newline position)
        let loc = mapper.span_to_location(Span::new(6, 6), "test.kn");
        assert_eq!(loc.line, 2);

        // Line 3
        let loc = mapper.span_to_location(Span::new(7, 12), "test.kn");
        assert_eq!(loc.line, 3);

        // Empty line 4
        let loc = mapper.span_to_location(Span::new(13, 13), "test.kn");
        assert_eq!(loc.line, 4);

        // Line 5
        let loc = mapper.span_to_location(Span::new(14, 19), "test.kn");
        assert_eq!(loc.line, 5);
    }

    #[test]
    fn test_span_mapper_out_of_bounds() {
        let source = "let x = 5";
        let mapper = SpanMapper::new(source);

        // Span beyond source length should be clamped
        let loc = mapper.span_to_location(Span::new(100, 200), "test.kn");
        assert_eq!(loc.line, 1);
        assert_eq!(loc.col, 10); // Clamped to source.len() + 1
    }

    #[test]
    fn test_span_mapper_exact_line_start() {
        let source = "line1\nline2\nline3";
        let mapper = SpanMapper::new(source);

        // Span starting exactly at line start (after newline)
        let loc = mapper.span_to_location(Span::new(6, 11), "test.kn");
        assert_eq!(loc.line, 2);
        assert_eq!(loc.col, 1);

        let loc = mapper.span_to_location(Span::new(12, 17), "test.kn");
        assert_eq!(loc.line, 3);
        assert_eq!(loc.col, 1);
    }

    #[test]
    fn test_span_mapper_line_starts_vector() {
        let source = "a\nb\nc";
        let mapper = SpanMapper::new(source);

        // Verify line_starts vector is correct
        assert_eq!(mapper.line_starts(), &[0, 2, 4]);

        let loc = mapper.span_to_location(Span::new(0, 1), "test.kn");
        assert_eq!(loc.line, 1);

        let loc = mapper.span_to_location(Span::new(2, 3), "test.kn");
        assert_eq!(loc.line, 2);

        let loc = mapper.span_to_location(Span::new(4, 5), "test.kn");
        assert_eq!(loc.line, 3);
    }

    #[test]
    fn test_span_mapper_maps_combined_spans_back_to_origin_files() {
        let helper_source = "pub fn helper() -> Int:\n    return 7\n";
        let entry_source = "fn main() -> Int:\n    return helper()\n";
        let combined = format!("{helper_source}{entry_source}");
        let mapper = SpanMapper::with_origins(
            &combined,
            vec![
                SourceOriginSegment {
                    file: "helper.kn".to_string(),
                    combined_span: Span::new(0, helper_source.len()),
                    source: helper_source.to_string(),
                },
                SourceOriginSegment {
                    file: "main.kn".to_string(),
                    combined_span: Span::new(helper_source.len(), combined.len()),
                    source: entry_source.to_string(),
                },
            ],
        );

        let helper_loc = mapper.span_to_location(Span::new(7, 8), "bundle.kn");
        assert_eq!(helper_loc.file, "helper.kn");
        assert_eq!(helper_loc.line, 1);
        assert_eq!(helper_loc.col, 8);

        let (entry_loc, entry_line) = mapper.span_to_line_info(
            Span::new(helper_source.len(), helper_source.len() + 2),
            "bundle.kn",
        );
        assert_eq!(entry_loc.file, "main.kn");
        assert_eq!(entry_loc.line, 1);
        assert_eq!(entry_line, "fn main() -> Int:");
    }
}
