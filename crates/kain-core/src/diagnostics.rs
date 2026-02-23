//! Pretty error reporting for KAIN
//! Shows source context with line numbers and error highlighting

use crate::span::Span;
use crate::error::KainError;

/// Source location with file, line, and column information
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct SourceLocation<'a> {
    pub file: &'a str,
    pub line: usize,  // 1-indexed
    pub col: usize,   // 1-indexed
}

impl<'a> SourceLocation<'a> {
    pub fn new(file: &'a str, line: usize, col: usize) -> Self {
        Self { file, line, col }
    }
}

/// Maps byte offsets (spans) to human-readable source locations
pub struct SpanMapper {
    source: String,
    line_starts: Vec<usize>,  // Byte offset of each line start
}

impl SpanMapper {
    /// Create a new SpanMapper from source code
    pub fn new(source: &str) -> Self {
        let mut line_starts = vec![0];
        
        // Build line_starts vector by finding all newline positions
        for (idx, ch) in source.char_indices() {
            if ch == '\n' {
                line_starts.push(idx + 1);
            }
        }
        
        SpanMapper {
            source: source.to_string(),
            line_starts,
        }
    }
    
    /// Convert a span to a source location with file:line:col format
    pub fn span_to_location<'a>(&self, span: Span, file: &'a str) -> SourceLocation<'a> {
        // Handle empty source
        if self.line_starts.is_empty() {
            return SourceLocation::new(file, 1, 1);
        }
        
        // Clamp span.start to valid range
        let start = span.start.min(self.source.len());
        
        // Binary search to find the line number
        // We want the largest line_start that is <= start
        let line_idx = match self.line_starts.binary_search(&start) {
            Ok(idx) => idx,  // Exact match - start of a line
            Err(idx) => idx.saturating_sub(1),  // Insert position - use previous line
        };
        
        let line_start = self.line_starts[line_idx];
        let col = start.saturating_sub(line_start);
        
        SourceLocation::new(
            file,
            line_idx + 1,  // Convert to 1-indexed
            col + 1,       // Convert to 1-indexed
        )
    }
    
    /// Get the source string
    pub fn source(&self) -> &str {
        &self.source
    }
}

/// Diagnostic renderer for pretty error messages
pub struct Diagnostics<'a> {
    source: &'a str,
    filename: &'a str,
}

impl<'a> Diagnostics<'a> {
    pub fn new(source: &'a str, filename: &'a str) -> Self {
        Self { source, filename }
    }
    
    /// Format an error with source context
    pub fn format_error(&self, error: &KainError) -> String {
        match error {
            KainError::Lexer { message, span } => self.format_with_context("Lexer Error", message, *span),
            KainError::Parser { message, span } => self.format_with_context("Parse Error", message, *span),
            KainError::Type { message, span } => self.format_with_context("Type Error", message, *span),
            KainError::Effect { message, span } => self.format_with_context("Effect Error", message, *span),
            KainError::Borrow { message, span } => self.format_with_context("Borrow Error", message, *span),
            KainError::Codegen { message, span } => self.format_with_context("Codegen Error", message, *span),
            KainError::CodegenWithLocation { message, file, line, col, .. } => format!(
                "\n\x1b[1;31merror[Codegen]\x1b[0m: {}\n  \x1b[1;34m-->\x1b[0m {}:{}:{}\n",
                message, file, line, col
            ),
            KainError::Runtime { message } => format!(
                "\n\x1b[1;31merror\x1b[0m: {}\n",
                message
            ),
            KainError::Io(e) => format!(
                "\n\x1b[1;31merror\x1b[0m: IO error: {}\n",
                e
            ),
            KainError::Enhanced { .. } => {
                // Enhanced errors format themselves via Display trait
                format!("\n{}\n", error)
            }
            KainError::Multi(errors) => {
                let mut output = format!("\n\x1b[1;31merror\x1b[0m: {} error(s) found:\n", errors.len());
                for (i, err) in errors.iter().enumerate() {
                    output.push_str(&format!("\n--- [{}/{}] ---\n", i + 1, errors.len()));
                    output.push_str(&self.format_error(err));
                }
                output
            }
        }
    }
    
    fn format_with_context(&self, error_type: &str, message: &str, span: Span) -> String {
        let (line_num, col, line_content) = self.get_line_info(span);
        
        let mut output = String::new();
        
        // Error header
        output.push_str(&format!(
            "\n\x1b[1;31merror[{}]\x1b[0m: {}\n",
            error_type, message
        ));
        
        // Location
        output.push_str(&format!(
            "  \x1b[1;34m-->\x1b[0m {}:{}:{}\n",
            self.filename, line_num, col
        ));
        
        // Separator
        output.push_str("   \x1b[1;34m|\x1b[0m\n");
        
        // Source line
        output.push_str(&format!(
            "\x1b[1;34m{:>3} |\x1b[0m {}\n",
            line_num, line_content
        ));
        
        // Error pointer
        let pointer_offset = col.saturating_sub(1);
        let content_len = line_content.len();
        let remaining_len = content_len.saturating_sub(pointer_offset);
        let span_len = span.end.saturating_sub(span.start);
        let pointer_len = span_len.min(remaining_len).max(1);
        
        output.push_str(&format!(
            "   \x1b[1;34m|\x1b[0m {}\x1b[1;31m{}\x1b[0m\n",
            " ".repeat(pointer_offset),
            "^".repeat(pointer_len)
        ));
        
        // Separator
        output.push_str("   \x1b[1;34m|\x1b[0m\n");
        
        output
    }
    
    /// Get line number, column, and line content for a span
    fn get_line_info(&self, span: Span) -> (usize, usize, &str) {
        let mut line_num = 1;
        let mut line_start = 0;
        
        // Safety check for span bounds
        let start = span.start.min(self.source.len());
        
        for (i, c) in self.source.char_indices() {
            if i >= start {
                break;
            }
            if c == '\n' {
                line_num += 1;
                line_start = i + 1;
            }
        }
        
        let col = start.saturating_sub(line_start) + 1;
        
        // Find line end
        let line_end = if start < self.source.len() {
            self.source[start..]
                .find('\n')
                .map(|i| start + i)
                .unwrap_or(self.source.len())
        } else {
            self.source.len()
        };
        
        let line_start = line_start.min(self.source.len());
        let line_content = &self.source[line_start..line_end];
        
        (line_num, col, line_content)
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
pub fn enhance_error_with_location(error: KainError, span_mapper: &SpanMapper, file: &str) -> KainError {
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
        let (line, col, content) = diag.get_line_info(Span::new(14, 15));
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
        assert_eq!(loc.col, 13);  // Column is byte-based, 1-indexed
        
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
        assert_eq!(loc.col, 10);  // Clamped to source.len() + 1
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
}

