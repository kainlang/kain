//! Pretty error reporting for KAIN
//! Shows source context with line numbers and error highlighting

use crate::diagnostic_registry::spec_for_code;
use crate::error::{DiagnosticReport, DiagnosticSeverity, KainError};
use crate::span::Span;
use crate::tooling_config::{active_color_preference, active_ui_theme_name};
use std::path::Path;

/// Source location with file, line, and column information
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SourceLocation {
    pub file: String,
    pub line: usize, // 1-indexed
    pub col: usize,  // 1-indexed
}

impl SourceLocation {
    pub fn new(file: impl Into<String>, line: usize, col: usize) -> Self {
        Self {
            file: file.into(),
            line,
            col,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SourceOriginSegment {
    pub file: String,
    pub combined_span: Span,
    pub source: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct MappedOriginSegment {
    file: String,
    normalized_file_key: String,
    combined_span: Span,
    source: String,
    line_starts: Vec<usize>,
}

/// Maps byte offsets (spans) to human-readable source locations
#[derive(Debug, Clone)]
pub struct SpanMapper {
    source: String,
    line_starts: Vec<usize>, // Byte offset of each line start
    origins: Vec<MappedOriginSegment>,
}

impl SpanMapper {
    /// Create a new SpanMapper from source code
    pub fn new(source: &str) -> Self {
        Self::with_origins(source, Vec::new())
    }

    pub fn with_origins(source: &str, origins: Vec<SourceOriginSegment>) -> Self {
        let line_starts = build_line_starts(source);
        let mapped_origins = origins
            .into_iter()
            .map(|origin| MappedOriginSegment {
                line_starts: build_line_starts(&origin.source),
                normalized_file_key: normalize_origin_file_key(&origin.file),
                file: origin.file,
                combined_span: origin.combined_span,
                source: origin.source,
            })
            .collect();

        Self {
            source: source.to_string(),
            line_starts,
            origins: mapped_origins,
        }
    }

    fn line_info_from_source<'a>(
        source: &'a str,
        line_starts: &[usize],
        span: Span,
        file: &str,
    ) -> (SourceLocation, &'a str) {
        if line_starts.is_empty() {
            return (SourceLocation::new(file, 1, 1), "");
        }

        let start = span.start.min(source.len());
        let line_idx = match line_starts.binary_search(&start) {
            Ok(idx) => idx,
            Err(idx) => idx.saturating_sub(1),
        };
        let line_start = line_starts[line_idx];
        let col = start.saturating_sub(line_start);
        let line_end = source[start..]
            .find('\n')
            .map(|idx| start + idx)
            .unwrap_or(source.len());
        let line_content = &source[line_start.min(source.len())..line_end];
        (
            SourceLocation::new(file, line_idx + 1, col + 1),
            line_content,
        )
    }

    fn mapped_origin_for_offset(&self, offset: usize) -> Option<(&MappedOriginSegment, Span)> {
        let offset = offset.min(self.source.len());
        self.origins.iter().find_map(|origin| {
            let start = origin.combined_span.start;
            let end = origin.combined_span.end;
            if offset < start || offset >= end {
                return None;
            }
            let local = offset.saturating_sub(start);
            Some((
                origin,
                Span::new(local, local.saturating_add(1).min(origin.source.len())),
            ))
        })
    }

    pub fn span_to_line_info(&self, span: Span, fallback_file: &str) -> (SourceLocation, &str) {
        if let Some((origin, origin_span)) = self.mapped_origin_for_offset(span.start) {
            return Self::line_info_from_source(
                &origin.source,
                &origin.line_starts,
                origin_span,
                &origin.file,
            );
        }

        Self::line_info_from_source(&self.source, &self.line_starts, span, fallback_file)
    }

    pub fn span_origin_file(&self, span: Span) -> Option<&str> {
        self.mapped_origin_for_offset(span.start)
            .map(|(origin, _)| origin.file.as_str())
    }

    pub fn has_origin_file(&self, file: &str) -> bool {
        let normalized_file = normalize_origin_file_key(file);
        self.origins
            .iter()
            .any(|origin| origin.normalized_file_key == normalized_file)
    }

    /// Convert a span to a source location with file:line:col format
    pub fn span_to_location(&self, span: Span, file: &str) -> SourceLocation {
        self.span_to_line_info(span, file).0
    }

    /// Get the source string
    pub fn source(&self) -> &str {
        &self.source
    }
}

fn build_line_starts(source: &str) -> Vec<usize> {
    let mut line_starts = vec![0];

    // Build line_starts vector by finding all newline positions
    for (idx, ch) in source.char_indices() {
        if ch == '\n' {
            line_starts.push(idx + 1);
        }
    }

    line_starts
}

fn normalize_origin_file_key(file: &str) -> String {
    let path = Path::new(file);
    std::fs::canonicalize(path)
        .unwrap_or_else(|_| path.to_path_buf())
        .to_string_lossy()
        .to_string()
}

#[derive(Debug, Clone, Copy)]
struct DiagnosticPalette {
    enabled: bool,
    error: &'static str,
    warning: &'static str,
    note: &'static str,
    help: &'static str,
    gutter: &'static str,
    pointer: &'static str,
    reset: &'static str,
}

impl DiagnosticPalette {
    fn paint(&self, style: &'static str, text: &str) -> String {
        if self.enabled {
            format!("{style}{text}{}", self.reset)
        } else {
            text.to_string()
        }
    }

    fn severity_text(&self, severity: DiagnosticSeverity, text: &str) -> String {
        match severity {
            DiagnosticSeverity::Error => self.paint(self.error, text),
            DiagnosticSeverity::Warning => self.paint(self.warning, text),
            DiagnosticSeverity::Note => self.paint(self.note, text),
            DiagnosticSeverity::Help => self.paint(self.help, text),
        }
    }

    fn error_text(&self, text: &str) -> String {
        self.paint(self.error, text)
    }

    fn note_text(&self, text: &str) -> String {
        self.paint(self.note, text)
    }

    fn gutter_text(&self, text: &str) -> String {
        self.paint(self.gutter, text)
    }

    fn pointer_text(&self, text: &str) -> String {
        self.paint(self.pointer, text)
    }
}

fn active_diagnostic_palette() -> DiagnosticPalette {
    let enabled = active_color_preference().should_color_stderr();
    let theme = active_ui_theme_name();
    match theme.as_str() {
        "ember" => DiagnosticPalette {
            enabled,
            error: "\x1b[1;38;2;255;102;64m",
            warning: "\x1b[1;38;2;255;196;82m",
            note: "\x1b[1;38;2;255;145;94m",
            help: "\x1b[1;38;2;255;220;128m",
            gutter: "\x1b[1;38;2;255;176;120m",
            pointer: "\x1b[1;38;2;255;102;64m",
            reset: "\x1b[0m",
        },
        "glacier" => DiagnosticPalette {
            enabled,
            error: "\x1b[1;38;2;255;112;146m",
            warning: "\x1b[1;38;2;255;212;102m",
            note: "\x1b[1;38;2;113;205;255m",
            help: "\x1b[1;38;2;164;244;255m",
            gutter: "\x1b[1;38;2;151;221;255m",
            pointer: "\x1b[1;38;2;255;112;146m",
            reset: "\x1b[0m",
        },
        "oxide" => DiagnosticPalette {
            enabled,
            error: "\x1b[1;38;2;209;84;67m",
            warning: "\x1b[1;38;2;234;187;84m",
            note: "\x1b[1;38;2;171;201;92m",
            help: "\x1b[1;38;2;224;223;128m",
            gutter: "\x1b[1;38;2;224;160;96m",
            pointer: "\x1b[1;38;2;209;84;67m",
            reset: "\x1b[0m",
        },
        _ => DiagnosticPalette {
            enabled,
            error: "\x1b[1;38;2;255;89;168m",
            warning: "\x1b[1;38;2;255;206;86m",
            note: "\x1b[1;38;2;92;225;230m",
            help: "\x1b[1;38;2;171;255;118m",
            gutter: "\x1b[1;38;2;92;225;230m",
            pointer: "\x1b[1;38;2;255;89;168m",
            reset: "\x1b[0m",
        },
    }
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
            KainError::Enhanced { .. } => {
                // Enhanced errors format themselves via Display trait
                format!("\n{}\n", error)
            }
            KainError::Rich(report) => self.format_diagnostic_report(&palette, report),
            KainError::Multi(errors) => {
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
            let (loc, line_content) = self
                .span_mapper
                .span_to_line_info(span, self.filename.as_str());
            let file = report
                .file
                .as_ref()
                .map(|path| path.display().to_string())
                .unwrap_or_else(|| loc.file.clone());
            output.push_str(&format!(
                "  {} {}:{}:{}\n",
                palette.gutter_text("-->"),
                file,
                loc.line,
                loc.col
            ));
            output.push_str(&format!("   {}\n", palette.gutter_text("|")));
            output.push_str(&format!(
                "{} {}\n",
                palette.gutter_text(&format!("{:>3} |", loc.line)),
                line_content
            ));

            let pointer_offset = loc.col.saturating_sub(1);
            let content_len = line_content.len();
            let remaining_len = content_len.saturating_sub(pointer_offset);
            let span_len = span.end.saturating_sub(span.start);
            let pointer_len = span_len.min(remaining_len).max(1);
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
            let (loc, _) = self
                .span_mapper
                .span_to_line_info(label.span, self.filename.as_str());
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
        for help in &report.help {
            output.push_str(&format!("   {} help: {}\n", palette.note_text("="), help));
        }
        for fixit in &report.fixits {
            output.push_str(&format!(
                "   {} fix-it: {} at bytes {}..{} -> {:?}\n",
                palette.note_text("="),
                fixit.message,
                fixit.span.start,
                fixit.span.end,
                fixit.replacement
            ));
        }
        if let Some(default_suggestion) = spec.default_suggestion {
            output.push_str(&format!(
                "   {} help: {}\n",
                palette.note_text("="),
                default_suggestion
            ));
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
        let (loc, line_content) = self
            .span_mapper
            .span_to_line_info(span, self.filename.as_str());

        let mut output = String::new();

        // Error header
        output.push_str(&format!(
            "\n{}: {}\n",
            palette.error_text(&format!("error[{error_type}]")),
            message
        ));

        // Location
        output.push_str(&format!(
            "  {} {}:{}:{}\n",
            palette.gutter_text("-->"),
            loc.file,
            loc.line,
            loc.col
        ));

        // Separator
        output.push_str(&format!("   {}\n", palette.gutter_text("|")));

        // Source line
        output.push_str(&format!(
            "{} {}\n",
            palette.gutter_text(&format!("{:>3} |", loc.line)),
            line_content
        ));

        // Error pointer
        let pointer_offset = loc.col.saturating_sub(1);
        let content_len = line_content.len();
        let remaining_len = content_len.saturating_sub(pointer_offset);
        let span_len = span.end.saturating_sub(span.start);
        let pointer_len = span_len.min(remaining_len).max(1);

        output.push_str(&format!(
            "   {} {}{}\n",
            palette.gutter_text("|"),
            " ".repeat(pointer_offset),
            palette.pointer_text(&"^".repeat(pointer_len))
        ));

        // Separator
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

        // After the emoji (byte offset 12 is the newline character)
        // "let x = " (8 bytes) + "🚀" (4 bytes) + "\n" (1 byte at position 12)
        // Column should be 13 (1-indexed: position 12 - line_start 0 + 1 = 13)
        let loc = mapper.span_to_location(Span::new(12, 13), "unicode.kn");
        assert_eq!(loc.line, 1);
        assert_eq!(loc.col, 13); // Column is byte-based, 1-indexed

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
        assert_eq!(mapper.line_starts, vec![0, 2, 4]);

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
