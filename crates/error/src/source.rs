//! Source-file mapping: byte spans → line:column locations.
//!
//! `SpanMapper` is the bridge between the byte-offset world of the lexer/
//! parser and the human-readable line:column world of diagnostics. It
//! handles multi-file origins (e.g., expanded macros, included files) and
//! Unicode display-width calculations for correct terminal highlight
//! placement.

use crate::span::Span;
use std::path::Path;
use unicode_width::UnicodeWidthStr;

/// A resolved source location with both character and display columns.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SourceLocation {
    /// Canonical file path or synthetic name (e.g. `<comptime>`).
    pub file: String,
    /// 1-based line number.
    pub line: usize,
    /// 1-based character column (codepoints).
    pub col: usize,
    /// 1-based display column (terminal cells, accounting for wide chars).
    pub display_col: usize,
    /// Byte offset from the start of the file.
    pub offset: usize,
}

impl SourceLocation {
    pub fn new(
        file: impl Into<String>,
        line: usize,
        col: usize,
        display_col: usize,
        offset: usize,
    ) -> Self {
        Self {
            file: file.into(),
            line,
            col,
            display_col,
            offset,
        }
    }
}

/// A contiguous range of source text with resolved start/end locations.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SourceRange {
    pub file: String,
    pub start: SourceLocation,
    pub end: SourceLocation,
}

/// Describes a segment of combined source that originated from a
/// different file (e.g., a macro expansion or `include!`).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SourceOriginSegment {
    /// Original file path.
    pub file: String,
    /// Byte span in the combined source where this origin appears.
    pub combined_span: Span,
    /// The original source text of this segment.
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

/// Maps byte spans back to source files, lines, columns, and display
/// widths. Supports multi-origin combined sources (e.g., post-expansion
/// or concatenated files).
#[derive(Debug, Clone)]
pub struct SpanMapper {
    source: String,
    line_starts: Vec<usize>,
    origins: Vec<MappedOriginSegment>,
}

impl SpanMapper {
    /// Create a mapper for a single source file.
    pub fn new(source: &str) -> Self {
        Self::with_origins(source, Vec::new())
    }

    /// Create a mapper for a combined source with origin segments.
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

    /// Resolve a span to a (location, line_content) pair.
    pub fn span_to_line_info(&self, span: Span, fallback_file: &str) -> (SourceLocation, &str) {
        let (range, line_content, _) = self.span_to_line_context(span, fallback_file);
        (range.start, line_content)
    }

    /// Resolve a span to a `SourceLocation` (start only).
    pub fn span_to_location(&self, span: Span, fallback_file: &str) -> SourceLocation {
        self.span_to_range(span, fallback_file).start
    }

    /// Resolve a span to a full `SourceRange`.
    pub fn span_to_range(&self, span: Span, fallback_file: &str) -> SourceRange {
        if let Some((origin, local_span)) = self.mapped_origin_for_span(span) {
            return range_from_source(
                &origin.source,
                &origin.line_starts,
                local_span,
                &origin.file,
            );
        }
        range_from_source(&self.source, &self.line_starts, span, fallback_file)
    }

    /// Resolve a span to (range, line_content, local_span_within_line).
    pub fn span_to_line_context(
        &self,
        span: Span,
        fallback_file: &str,
    ) -> (SourceRange, &str, Span) {
        if let Some((origin, local_span)) = self.mapped_origin_for_span(span) {
            return line_context_from_source(
                &origin.source,
                &origin.line_starts,
                local_span,
                &origin.file,
            );
        }
        line_context_from_source(&self.source, &self.line_starts, span, fallback_file)
    }

    /// Resolve a span to (range, line_content, display_offset, display_width).
    /// The display offset/width account for Unicode wide characters so
    /// terminal `^~~~` highlights align correctly.
    pub fn span_to_display_context(
        &self,
        span: Span,
        fallback_file: &str,
    ) -> (SourceRange, &str, usize, usize) {
        let (range, line_content, local_span) = self.span_to_line_context(span, fallback_file);
        let safe_start = local_span.start.min(line_content.len());
        let safe_end = local_span.end.min(line_content.len()).max(safe_start);
        let prefix = &line_content[..safe_start];
        let highlight = &line_content[safe_start..safe_end];
        let highlight_width = UnicodeWidthStr::width(highlight).max(1);
        (
            range,
            line_content,
            UnicodeWidthStr::width(prefix),
            highlight_width,
        )
    }

    /// Return the origin file for a span if it comes from an origin segment.
    pub fn span_origin_file(&self, span: Span) -> Option<&str> {
        self.mapped_origin_for_span(span)
            .map(|(origin, _)| origin.file.as_str())
    }

    /// Check whether the mapper has an origin segment for the given file.
    pub fn has_origin_file(&self, file: &str) -> bool {
        let normalized_file = normalize_origin_file_key(file);
        self.origins
            .iter()
            .any(|origin| origin.normalized_file_key == normalized_file)
    }

    pub fn line_starts(&self) -> &[usize] {
        &self.line_starts
    }

    pub fn source(&self) -> &str {
        &self.source
    }

    fn mapped_origin_for_span(&self, span: Span) -> Option<(&MappedOriginSegment, Span)> {
        let offset = span.start.min(self.source.len());
        self.origins.iter().find_map(|origin| {
            let start = origin.combined_span.start;
            let end = origin.combined_span.end;
            if offset < start || offset >= end {
                return None;
            }
            let local_start = offset.saturating_sub(start);
            let local_end = span
                .end
                .min(end)
                .saturating_sub(start)
                .max(local_start)
                .min(origin.source.len());
            Some((origin, Span::new(local_start, local_end)))
        })
    }
}

// ── Internal helpers ──────────────────────────────────────────────────

fn build_line_starts(source: &str) -> Vec<usize> {
    let mut line_starts = vec![0];
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

fn clamp_span(span: Span, source_len: usize) -> Span {
    let start = span.start.min(source_len);
    let end = span.end.max(start).min(source_len);
    Span::new(start, end)
}

fn position_from_source(
    source: &str,
    line_starts: &[usize],
    offset: usize,
    file: &str,
) -> SourceLocation {
    let offset = offset.min(source.len());
    let line_idx = match line_starts.binary_search(&offset) {
        Ok(idx) => idx,
        Err(idx) => idx.saturating_sub(1),
    };
    let line_start = line_starts
        .get(line_idx)
        .copied()
        .unwrap_or_default()
        .min(source.len());
    let prefix = &source[line_start..offset];
    let col = prefix.chars().count() + 1;
    let display_col = UnicodeWidthStr::width(prefix) + 1;
    SourceLocation::new(file, line_idx + 1, col, display_col, offset)
}

fn line_bounds(source: &str, line_starts: &[usize], offset: usize) -> (usize, usize, usize) {
    let offset = offset.min(source.len());
    let line_idx = match line_starts.binary_search(&offset) {
        Ok(idx) => idx,
        Err(idx) => idx.saturating_sub(1),
    };
    let line_start = line_starts
        .get(line_idx)
        .copied()
        .unwrap_or_default()
        .min(source.len());
    let line_end = source[line_start..]
        .find('\n')
        .map(|idx| line_start + idx)
        .unwrap_or(source.len());
    (line_idx, line_start, line_end)
}

fn range_from_source(source: &str, line_starts: &[usize], span: Span, file: &str) -> SourceRange {
    let span = clamp_span(span, source.len());
    let start = position_from_source(source, line_starts, span.start, file);
    let end = position_from_source(source, line_starts, span.end, file);
    SourceRange {
        file: file.to_string(),
        start,
        end,
    }
}

fn line_context_from_source<'a>(
    source: &'a str,
    line_starts: &[usize],
    span: Span,
    file: &str,
) -> (SourceRange, &'a str, Span) {
    let span = clamp_span(span, source.len());
    let range = range_from_source(source, line_starts, span, file);
    let (_, line_start, line_end) = line_bounds(source, line_starts, span.start);
    let line_content = &source[line_start..line_end];
    let local_start = span
        .start
        .saturating_sub(line_start)
        .min(line_content.len());
    let local_end = span
        .end
        .min(line_end)
        .saturating_sub(line_start)
        .max(local_start)
        .min(line_content.len());
    (range, line_content, Span::new(local_start, local_end))
}

/// True when a filename is synthetic (e.g. `<comptime>`, `<macro>`).
pub fn is_synthetic_filename(file: &str) -> bool {
    file.starts_with('<') && file.ends_with('>')
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn unicode_columns_track_chars_and_display_width() {
        let mapper = SpanMapper::new("let x = 🚀\nnext");
        let loc = mapper.span_to_location(Span::new(12, 13), "unicode.kn");
        assert_eq!(loc.line, 1);
        assert_eq!(loc.col, 10);
        assert_eq!(loc.display_col, 11);
    }

    #[test]
    fn display_context_uses_terminal_width_for_highlight() {
        let mapper = SpanMapper::new("a🚀z");
        let (_range, line, offset, width) =
            mapper.span_to_display_context(Span::new(1, 5), "x.kn");
        assert_eq!(line, "a🚀z");
        assert_eq!(offset, 1);
        assert_eq!(width, 2);
    }

    #[test]
    fn span_to_range_maps_entire_file() {
        let source = "line1\nline2\nline3";
        let mapper = SpanMapper::new(source);
        let range = mapper.span_to_range(Span::new(0, source.len()), "test.kn");
        assert_eq!(range.start.line, 1);
        assert_eq!(range.end.line, 3);
    }
}
