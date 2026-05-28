//! Source-code span tracking for error reporting.
//!
//! A `Span` is a zero-width or non-zero-width byte-offset range into a
//! source file. Spans are the lingua franca connecting AST nodes, tokens,
//! and diagnostics back to human-readable line:column locations.

use std::ops::Range;

/// A half-open `[start, end)` byte-offset range in source text.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub struct Span {
    pub start: usize,
    pub end: usize,
}

impl Span {
    pub const fn new(start: usize, end: usize) -> Self {
        Self { start, end }
    }

    /// Create a zero-width span at a single byte offset.
    pub const fn point(at: usize) -> Self {
        Self {
            start: at,
            end: at,
        }
    }

    /// Merge two spans into the smallest span that contains both.
    pub fn merge(self, other: Span) -> Span {
        Span {
            start: self.start.min(other.start),
            end: self.end.max(other.end),
        }
    }

    /// Extend this span to include `other`.
    pub fn extend(self, other: Span) -> Span {
        self.merge(other)
    }

    /// Length in bytes. Zero for point spans.
    pub fn len(self) -> usize {
        self.end.saturating_sub(self.start)
    }

    /// True when `len() == 0`.
    pub fn is_empty(self) -> bool {
        self.start >= self.end
    }

    pub fn to_range(self) -> Range<usize> {
        self.start..self.end
    }

    /// Shift both start and end by `offset` bytes (used during
    /// source-combination or expansion).
    pub fn shift(self, offset: usize) -> Span {
        Span {
            start: self.start.saturating_add(offset),
            end: self.end.saturating_add(offset),
        }
    }
}

impl From<Range<usize>> for Span {
    fn from(range: Range<usize>) -> Self {
        Span::new(range.start, range.end)
    }
}

impl From<Span> for Range<usize> {
    fn from(span: Span) -> Self {
        span.to_range()
    }
}

impl std::fmt::Display for Span {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}..{}", self.start, self.end)
    }
}

/// A value annotated with its source span. The canonical carrier for
/// AST nodes, tokens, and type-inference results that need to be
/// reported back to the user.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Spanned<T> {
    pub value: T,
    pub span: Span,
}

impl<T> Spanned<T> {
    pub fn new(value: T, span: Span) -> Self {
        Self { value, span }
    }

    pub fn map<U, F: FnOnce(T) -> U>(self, f: F) -> Spanned<U> {
        Spanned {
            value: f(self.value),
            span: self.span,
        }
    }

    pub fn as_ref(&self) -> Spanned<&T> {
        Spanned {
            value: &self.value,
            span: self.span,
        }
    }
}
