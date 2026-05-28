//! Diagnostic labels and fix-it hints.
//!
//! Labels annotate spans with messages. Fix-its propose source-code
//! replacements that would resolve the diagnostic.

use crate::source::SourceRange;
use crate::span::Span;

/// A label attached to a source span with an explanatory message.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DiagnosticLabel {
    /// The byte-offset span being labeled.
    pub span: Span,
    /// Resolved source location (populated by the renderer).
    pub range: Option<SourceRange>,
    /// Human-readable message.
    pub message: String,
    /// When true, this is the primary label (the main error site).
    /// Secondary labels provide additional context.
    pub primary: bool,
    /// Optional hint about how this label relates to the error.
    pub kind: LabelKind,
}

/// The semantic role of a label in a diagnostic.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum LabelKind {
    /// Default — just a contextual annotation.
    #[default]
    Annotation,
    /// "This value originates here" — shows where a value came from.
    Origin,
    /// "This is where the type was defined" — points to a type definition.
    Definition,
    /// "This borrow expires here" — borrow-checking context.
    BorrowEnd,
    /// "Value moved here" — ownership transfer.
    MovedHere,
    /// "Required by this bound" — trait/effect constraint.
    RequiredBy,
}

/// A fix-it: a suggested source-code replacement.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DiagnosticFixIt {
    /// The byte-offset span to replace.
    pub span: Span,
    /// Resolved source location (populated by the renderer).
    pub range: Option<SourceRange>,
    /// The replacement text.
    pub replacement: String,
    /// Human-readable description of what this fix does.
    pub message: String,
    /// When true, this fix-it is the "primary" suggestion.
    pub primary: bool,
    /// Confidence level: how sure the compiler is about this fix.
    pub confidence: FixItConfidence,
}

/// How strongly the compiler believes a fix-it is correct.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum FixItConfidence {
    /// The fix is definitely correct (e.g., a missing delimiter).
    Certain,
    /// The fix is likely correct but may need review.
    #[default]
    Likely,
    /// The fix is a guess — the user should verify.
    Tentative,
}

impl DiagnosticLabel {
    pub fn new(span: Span, message: impl Into<String>) -> Self {
        Self {
            span,
            range: None,
            message: message.into(),
            primary: false,
            kind: LabelKind::default(),
        }
    }

    pub fn primary(mut self) -> Self {
        self.primary = true;
        self
    }

    pub fn kind(mut self, kind: LabelKind) -> Self {
        self.kind = kind;
        self
    }

    pub fn with_range(mut self, range: SourceRange) -> Self {
        self.range = Some(range);
        self
    }
}

impl DiagnosticFixIt {
    pub fn new(
        span: Span,
        replacement: impl Into<String>,
        message: impl Into<String>,
    ) -> Self {
        Self {
            span,
            range: None,
            replacement: replacement.into(),
            message: message.into(),
            primary: false,
            confidence: FixItConfidence::default(),
        }
    }

    pub fn primary(mut self) -> Self {
        self.primary = true;
        self
    }

    pub fn confidence(mut self, confidence: FixItConfidence) -> Self {
        self.confidence = confidence;
        self
    }

    pub fn with_range(mut self, range: SourceRange) -> Self {
        self.range = Some(range);
        self
    }
}
