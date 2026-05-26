//! Error types for the KAIN compiler

use crate::diagnostic_registry::{default_code_for_kind, spec_for_code, DiagnosticCode};
use crate::source::{SourceRange, SpanMapper};
use crate::span::Span;
use serde_json::{json, Value as JsonValue};
use std::fmt;
use std::path::PathBuf;
use thiserror::Error;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DiagnosticSeverity {
    Error,
    Warning,
    Note,
    Help,
}

impl fmt::Display for DiagnosticSeverity {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DiagnosticSeverity::Error => write!(f, "error"),
            DiagnosticSeverity::Warning => write!(f, "warning"),
            DiagnosticSeverity::Note => write!(f, "note"),
            DiagnosticSeverity::Help => write!(f, "help"),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DiagnosticLabel {
    pub span: Span,
    pub range: Option<SourceRange>,
    pub message: String,
    pub primary: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DiagnosticFixIt {
    pub span: Span,
    pub range: Option<SourceRange>,
    pub replacement: String,
    pub message: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DiagnosticReport {
    pub kind: ErrorKind,
    pub code: DiagnosticCode,
    pub severity: DiagnosticSeverity,
    pub message: String,
    pub file: Option<PathBuf>,
    pub location: Option<(usize, usize)>,
    pub primary_span: Option<Span>,
    pub primary_range: Option<SourceRange>,
    pub labels: Vec<DiagnosticLabel>,
    pub notes: Vec<String>,
    pub help: Vec<String>,
    pub fixits: Vec<DiagnosticFixIt>,
    pub origin: Option<String>,
    pub tags: Vec<String>,
}

impl DiagnosticReport {
    pub fn new(kind: ErrorKind, code: DiagnosticCode, message: impl Into<String>) -> Self {
        Self {
            kind,
            code,
            severity: DiagnosticSeverity::Error,
            message: message.into(),
            file: None,
            location: None,
            primary_span: None,
            primary_range: None,
            labels: Vec::new(),
            notes: Vec::new(),
            help: Vec::new(),
            fixits: Vec::new(),
            origin: None,
            tags: Vec::new(),
        }
    }

    pub fn severity(mut self, severity: DiagnosticSeverity) -> Self {
        self.severity = severity;
        self
    }

    pub fn file(mut self, path: impl Into<PathBuf>) -> Self {
        self.file = Some(path.into());
        self
    }

    pub fn location(mut self, line: usize, col: usize) -> Self {
        self.location = Some((line, col));
        self
    }

    pub fn primary_span(mut self, span: Span) -> Self {
        self.primary_span = Some(span);
        self
    }

    pub fn primary_range(mut self, range: SourceRange) -> Self {
        self.adopt_range_metadata(&range);
        self.primary_range = Some(range);
        self
    }

    pub fn at_source(mut self, mapper: &SpanMapper, span: Span, fallback_file: &str) -> Self {
        let range = mapper.span_to_range(span, fallback_file);
        self.primary_span = Some(span);
        self.adopt_range_metadata(&range);
        self.primary_range = Some(range.clone());
        for label in &mut self.labels {
            if label.primary && label.span == span && label.range.is_none() {
                label.range = Some(range.clone());
            }
        }
        self
    }

    pub fn label(mut self, span: Span, message: impl Into<String>) -> Self {
        self.labels.push(DiagnosticLabel {
            span,
            range: None,
            message: message.into(),
            primary: false,
        });
        self
    }

    pub fn label_with_range(
        mut self,
        span: Span,
        range: SourceRange,
        message: impl Into<String>,
    ) -> Self {
        self.labels.push(DiagnosticLabel {
            span,
            range: Some(range),
            message: message.into(),
            primary: false,
        });
        self
    }

    pub fn label_from_source(
        self,
        mapper: &SpanMapper,
        span: Span,
        fallback_file: &str,
        message: impl Into<String>,
    ) -> Self {
        self.label_with_range(span, mapper.span_to_range(span, fallback_file), message)
    }

    pub fn primary_label(mut self, span: Span, message: impl Into<String>) -> Self {
        self.primary_span = Some(span);
        self.labels.push(DiagnosticLabel {
            span,
            range: None,
            message: message.into(),
            primary: true,
        });
        self
    }

    pub fn primary_label_with_range(
        mut self,
        span: Span,
        range: SourceRange,
        message: impl Into<String>,
    ) -> Self {
        self.primary_span = Some(span);
        self.adopt_range_metadata(&range);
        self.primary_range = Some(range.clone());
        self.labels.push(DiagnosticLabel {
            span,
            range: Some(range),
            message: message.into(),
            primary: true,
        });
        self
    }

    pub fn primary_label_from_source(
        self,
        mapper: &SpanMapper,
        span: Span,
        fallback_file: &str,
        message: impl Into<String>,
    ) -> Self {
        self.primary_label_with_range(span, mapper.span_to_range(span, fallback_file), message)
    }

    pub fn note(mut self, note: impl Into<String>) -> Self {
        self.notes.push(note.into());
        self
    }

    pub fn help(mut self, help: impl Into<String>) -> Self {
        self.help.push(help.into());
        self
    }

    pub fn fixit(
        mut self,
        span: Span,
        replacement: impl Into<String>,
        message: impl Into<String>,
    ) -> Self {
        self.fixits.push(DiagnosticFixIt {
            span,
            range: None,
            replacement: replacement.into(),
            message: message.into(),
        });
        self
    }

    pub fn fixit_with_range(
        mut self,
        span: Span,
        range: SourceRange,
        replacement: impl Into<String>,
        message: impl Into<String>,
    ) -> Self {
        self.fixits.push(DiagnosticFixIt {
            span,
            range: Some(range),
            replacement: replacement.into(),
            message: message.into(),
        });
        self
    }

    pub fn fixit_from_source(
        self,
        mapper: &SpanMapper,
        span: Span,
        fallback_file: &str,
        replacement: impl Into<String>,
        message: impl Into<String>,
    ) -> Self {
        self.fixit_with_range(
            span,
            mapper.span_to_range(span, fallback_file),
            replacement,
            message,
        )
    }

    pub fn origin(mut self, origin: impl Into<String>) -> Self {
        self.origin = Some(origin.into());
        self
    }

    pub fn tag(mut self, tag: impl Into<String>) -> Self {
        self.tags.push(tag.into());
        self
    }

    pub fn to_json_value(&self) -> JsonValue {
        let spec = spec_for_code(self.code);
        let labels: Vec<JsonValue> = self
            .labels
            .iter()
            .map(|label| {
                json!({
                    "span": {"start": label.span.start, "end": label.span.end},
                    "range": label.range.as_ref().map(range_to_json),
                    "message": label.message,
                    "primary": label.primary,
                })
            })
            .collect();
        let fixits: Vec<JsonValue> = self
            .fixits
            .iter()
            .map(|fixit| {
                json!({
                    "span": {"start": fixit.span.start, "end": fixit.span.end},
                    "range": fixit.range.as_ref().map(range_to_json),
                    "replacement": fixit.replacement,
                    "message": fixit.message,
                })
            })
            .collect();

        json!({
            "severity": self.severity.to_string(),
            "kind": self.kind.to_string(),
            "code": spec.code_str,
            "title": spec.title,
            "category": spec.category.to_string(),
            "message": self.message,
            "file": self.file.as_ref().map(|path| path.display().to_string()),
            "location": self.location.map(|(line, col)| json!({"line": line, "column": col})),
            "primary_span": self.primary_span.map(|span| json!({"start": span.start, "end": span.end})),
            "primary_range": self.primary_range.as_ref().map(range_to_json),
            "labels": labels,
            "notes": self.notes,
            "help": self.help,
            "fixits": fixits,
            "origin": self.origin,
            "tags": self.tags,
            "docs_key": spec.docs_key,
            "default_help": spec.default_suggestion,
        })
    }

    fn adopt_range_metadata(&mut self, range: &SourceRange) {
        if self.file.is_none() {
            self.file = Some(PathBuf::from(range.file.clone()));
        }
        if self.location.is_none() {
            self.location = Some((range.start.line, range.start.col));
        }
    }
}

impl fmt::Display for DiagnosticReport {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let spec = spec_for_code(self.code);
        write!(
            f,
            "{}[{}:{}]: {}",
            self.severity, self.kind, spec.code_str, self.message
        )?;

        if let Some(path) = &self.file {
            write!(f, "\n  --> {}", path.display())?;
            if let Some((line, col)) = self.location {
                write!(f, ":{}:{}", line, col)?;
            }
        } else if let Some(origin) = &self.origin {
            write!(f, "\n  = origin: {}", origin)?;
        }

        for label in &self.labels {
            let marker = if label.primary { "primary" } else { "label" };
            write!(
                f,
                "\n  = {}[{}..{}]: {}",
                marker, label.span.start, label.span.end, label.message
            )?;
            if let Some(range) = &label.range {
                if !synthetic_filename(&range.file) {
                    write!(
                        f,
                        " ({}:{}:{}-{}:{})",
                        range.file,
                        range.start.line,
                        range.start.col,
                        range.end.line,
                        range.end.col
                    )?;
                }
            }
        }
        for note in &self.notes {
            write!(f, "\n  = note: {}", note)?;
        }
        for help in &self.help {
            write!(f, "\n  = help: {}", help)?;
        }
        for fixit in &self.fixits {
            write!(
                f,
                "\n  = fix-it[{}..{}]: {} -> {:?}",
                fixit.span.start, fixit.span.end, fixit.message, fixit.replacement
            )?;
        }
        if let Some(default_suggestion) = spec.default_suggestion {
            write!(f, "\n  = help: {}", default_suggestion)?;
        }
        if let Some(docs_key) = spec.docs_key {
            write!(f, "\n  = reference: {}", docs_key)?;
        }
        Ok(())
    }
}

#[derive(Error, Debug)]
pub enum KainError {
    #[error("Lexer error at {span:?}: {message}")]
    Lexer { message: String, span: Span },

    #[error("Parser error at {span:?}: {message}")]
    Parser { message: String, span: Span },

    #[error("Type error at {span:?}: {message}")]
    Type { message: String, span: Span },

    #[error("Effect error at {span:?}: {message}")]
    Effect { message: String, span: Span },

    #[error("Borrow error at {span:?}: {message}")]
    Borrow { message: String, span: Span },

    #[error("Codegen error at {span:?}: {message}")]
    Codegen { message: String, span: Span },

    #[error("{file}:{line}:{col}: {message}")]
    CodegenWithLocation {
        message: String,
        file: String,
        line: usize,
        col: usize,
        span: Span,
    },

    #[error("Runtime error: {message}")]
    Runtime { message: String },

    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),

    #[error("{}", format_enhanced_error(.kind, .code, .file, .location, .context, .message, .suggestion))]
    Enhanced {
        kind: ErrorKind,
        code: DiagnosticCode,
        file: Option<PathBuf>,
        location: Option<(usize, usize)>,
        context: String,
        message: String,
        suggestion: Option<String>,
    },

    #[error("{0}")]
    Rich(Box<DiagnosticReport>),

    #[error("{}", format_multi_errors(.0))]
    Multi(Vec<KainError>),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ErrorKind {
    Parse,
    Type,
    Validation,
    Codegen,
    Io,
    Config,
    Effect,
    Borrow,
    Runtime,
}

impl fmt::Display for ErrorKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ErrorKind::Parse => write!(f, "PARSE"),
            ErrorKind::Type => write!(f, "TYPE"),
            ErrorKind::Validation => write!(f, "VALIDATION"),
            ErrorKind::Codegen => write!(f, "CODEGEN"),
            ErrorKind::Io => write!(f, "IO"),
            ErrorKind::Config => write!(f, "CONFIG"),
            ErrorKind::Effect => write!(f, "EFFECT"),
            ErrorKind::Borrow => write!(f, "BORROW"),
            ErrorKind::Runtime => write!(f, "RUNTIME"),
        }
    }
}

fn format_enhanced_error(
    kind: &ErrorKind,
    code: &DiagnosticCode,
    file: &Option<PathBuf>,
    location: &Option<(usize, usize)>,
    context: &str,
    message: &str,
    suggestion: &Option<String>,
) -> String {
    let spec = spec_for_code(*code);
    let mut output = String::new();

    output.push_str(&format!("❌ [{}:{}] {}", kind, spec.code_str, spec.title));

    if let Some(path) = file {
        output.push_str(&format!(" in {}", path.display()));
        if let Some((line, col)) = location {
            output.push_str(&format!(":{}:{}", line, col));
        }
    }

    output.push_str("\n\n");

    if !context.is_empty() {
        output.push_str(&format!("   Context: {}\n", context));
    }

    output.push_str(&format!("   {}\n", message));

    if let Some(suggestion) = suggestion.as_deref().or(spec.default_suggestion) {
        output.push_str(&format!("\n   Help: {}\n", suggestion));
    }

    if let Some(docs_key) = spec.docs_key {
        output.push_str(&format!("\n   Reference: {}\n", docs_key));
    }

    output
}

fn format_multi_errors(errors: &[KainError]) -> String {
    let mut output = String::new();
    output.push_str(&format!("Found {} error(s):\n", errors.len()));
    for (i, err) in errors.iter().enumerate() {
        output.push_str(&format!("\n[{}/{}] {}\n", i + 1, errors.len(), err));
    }
    output
}

impl KainError {
    pub fn lexer(message: impl Into<String>, span: Span) -> Self {
        KainError::Lexer {
            message: message.into(),
            span,
        }
    }

    pub fn parser(message: impl Into<String>, span: Span) -> Self {
        KainError::Parser {
            message: message.into(),
            span,
        }
    }

    pub fn type_error(message: impl Into<String>, span: Span) -> Self {
        KainError::Type {
            message: message.into(),
            span,
        }
    }

    pub fn effect_error(message: impl Into<String>, span: Span) -> Self {
        KainError::Effect {
            message: message.into(),
            span,
        }
    }

    pub fn borrow_error(message: impl Into<String>, span: Span) -> Self {
        KainError::Borrow {
            message: message.into(),
            span,
        }
    }

    pub fn codegen(message: impl Into<String>, span: Span) -> Self {
        KainError::Codegen {
            message: message.into(),
            span,
        }
    }

    pub fn codegen_with_location(
        message: impl Into<String>,
        file: impl Into<String>,
        line: usize,
        col: usize,
        span: Span,
    ) -> Self {
        KainError::CodegenWithLocation {
            message: message.into(),
            file: file.into(),
            line,
            col,
            span,
        }
    }

    pub fn runtime(message: impl Into<String>) -> Self {
        KainError::Runtime {
            message: message.into(),
        }
    }

    pub fn multi(errors: Vec<KainError>) -> Self {
        debug_assert!(
            !errors.is_empty(),
            "Multi error must contain at least one error"
        );
        KainError::Multi(errors)
    }

    pub fn parse_error(message: impl Into<String>) -> Self {
        KainError::Enhanced {
            kind: ErrorKind::Parse,
            code: default_code_for_kind(ErrorKind::Parse),
            file: None,
            location: None,
            context: String::new(),
            message: message.into(),
            suggestion: None,
        }
    }

    pub fn type_err(message: impl Into<String>) -> Self {
        KainError::Enhanced {
            kind: ErrorKind::Type,
            code: default_code_for_kind(ErrorKind::Type),
            file: None,
            location: None,
            context: String::new(),
            message: message.into(),
            suggestion: None,
        }
    }

    pub fn validation_error(message: impl Into<String>) -> Self {
        KainError::Enhanced {
            kind: ErrorKind::Validation,
            code: default_code_for_kind(ErrorKind::Validation),
            file: None,
            location: None,
            context: String::new(),
            message: message.into(),
            suggestion: None,
        }
    }

    pub fn codegen_error(message: impl Into<String>) -> Self {
        KainError::Enhanced {
            kind: ErrorKind::Codegen,
            code: default_code_for_kind(ErrorKind::Codegen),
            file: None,
            location: None,
            context: String::new(),
            message: message.into(),
            suggestion: None,
        }
    }

    pub fn io_error(message: impl Into<String>) -> Self {
        KainError::Enhanced {
            kind: ErrorKind::Io,
            code: default_code_for_kind(ErrorKind::Io),
            file: None,
            location: None,
            context: String::new(),
            message: message.into(),
            suggestion: None,
        }
    }

    pub fn config_error(message: impl Into<String>) -> Self {
        KainError::Enhanced {
            kind: ErrorKind::Config,
            code: default_code_for_kind(ErrorKind::Config),
            file: None,
            location: None,
            context: String::new(),
            message: message.into(),
            suggestion: None,
        }
    }

    pub fn diagnostic(kind: ErrorKind, code: DiagnosticCode, message: impl Into<String>) -> Self {
        KainError::Enhanced {
            kind,
            code,
            file: None,
            location: None,
            context: String::new(),
            message: message.into(),
            suggestion: None,
        }
    }

    pub fn rich(report: DiagnosticReport) -> Self {
        KainError::Rich(Box::new(report))
    }

    pub fn to_diagnostic_reports(&self) -> Vec<DiagnosticReport> {
        match self {
            KainError::Multi(errors) => errors
                .iter()
                .flat_map(KainError::to_diagnostic_reports)
                .collect(),
            _ => self.as_diagnostic_report().into_iter().collect(),
        }
    }

    pub fn diagnostic_json(&self) -> Option<JsonValue> {
        let diagnostics = self
            .to_diagnostic_reports()
            .into_iter()
            .map(|report| report.to_json_value())
            .collect::<Vec<_>>();
        if diagnostics.is_empty() {
            None
        } else {
            Some(json!({ "diagnostics": diagnostics }))
        }
    }

    fn as_diagnostic_report(&self) -> Option<DiagnosticReport> {
        match self {
            KainError::Lexer { message, span } => Some(
                DiagnosticReport::new(
                    ErrorKind::Parse,
                    default_code_for_kind(ErrorKind::Parse),
                    message.clone(),
                )
                .primary_label(*span, "lexer stopped here"),
            ),
            KainError::Parser { message, span } => Some(
                DiagnosticReport::new(
                    ErrorKind::Parse,
                    default_code_for_kind(ErrorKind::Parse),
                    message.clone(),
                )
                .primary_label(*span, "parser stopped here"),
            ),
            KainError::Type { message, span } => Some(
                DiagnosticReport::new(
                    ErrorKind::Type,
                    default_code_for_kind(ErrorKind::Type),
                    message.clone(),
                )
                .primary_label(*span, "typechecker stopped here"),
            ),
            KainError::Effect { message, span } => Some(
                DiagnosticReport::new(
                    ErrorKind::Effect,
                    default_code_for_kind(ErrorKind::Effect),
                    message.clone(),
                )
                .primary_label(*span, "effect checker stopped here"),
            ),
            KainError::Borrow { message, span } => Some(
                DiagnosticReport::new(
                    ErrorKind::Borrow,
                    default_code_for_kind(ErrorKind::Borrow),
                    message.clone(),
                )
                .primary_label(*span, "borrow checker stopped here"),
            ),
            KainError::Codegen { message, span } => Some(
                DiagnosticReport::new(
                    ErrorKind::Codegen,
                    default_code_for_kind(ErrorKind::Codegen),
                    message.clone(),
                )
                .primary_label(*span, "code generation stopped here"),
            ),
            KainError::CodegenWithLocation {
                message,
                file,
                line,
                col,
                span,
            } => Some(
                DiagnosticReport::new(
                    ErrorKind::Codegen,
                    default_code_for_kind(ErrorKind::Codegen),
                    message.clone(),
                )
                .file(PathBuf::from(file.clone()))
                .location(*line, *col)
                .primary_label(*span, "code generation stopped here"),
            ),
            KainError::Runtime { message } => Some(DiagnosticReport::new(
                ErrorKind::Runtime,
                default_code_for_kind(ErrorKind::Runtime),
                message.clone(),
            )),
            KainError::Io(error) => Some(DiagnosticReport::new(
                ErrorKind::Io,
                default_code_for_kind(ErrorKind::Io),
                error.to_string(),
            )),
            KainError::Enhanced {
                kind,
                code,
                file,
                location,
                context,
                message,
                suggestion,
            } => {
                let mut report = DiagnosticReport::new(*kind, *code, message.clone());
                if let Some(path) = file {
                    report = report.file(path.clone());
                }
                if let Some((line, col)) = location {
                    report = report.location(*line, *col);
                }
                if !context.is_empty() {
                    report = report.note(format!("Context: {context}"));
                }
                if let Some(suggestion) = suggestion {
                    report = report.help(suggestion.clone());
                }
                Some(report)
            }
            KainError::Rich(report) => Some((**report).clone()),
            KainError::Multi(_) => None,
        }
    }
}

pub type KainResult<T> = Result<T, KainError>;

pub struct DiagnosticBuilder {
    kind: ErrorKind,
    code: DiagnosticCode,
    file: Option<PathBuf>,
    location: Option<(usize, usize)>,
    context: String,
    message: String,
    suggestion: Option<String>,
}

impl DiagnosticBuilder {
    pub fn new(kind: ErrorKind, code: DiagnosticCode, message: impl Into<String>) -> Self {
        Self {
            kind,
            code,
            file: None,
            location: None,
            context: String::new(),
            message: message.into(),
            suggestion: None,
        }
    }

    pub fn context(mut self, context: impl Into<String>) -> Self {
        self.context = context.into();
        self
    }

    pub fn file(mut self, path: impl Into<PathBuf>) -> Self {
        self.file = Some(path.into());
        self
    }

    pub fn location(mut self, line: usize, col: usize) -> Self {
        self.location = Some((line, col));
        self
    }

    pub fn suggestion(mut self, suggestion: impl Into<String>) -> Self {
        self.suggestion = Some(suggestion.into());
        self
    }

    pub fn build(self) -> KainError {
        KainError::Enhanced {
            kind: self.kind,
            code: self.code,
            file: self.file,
            location: self.location,
            context: self.context,
            message: self.message,
            suggestion: self.suggestion,
        }
    }
}

pub trait ErrorContext<T> {
    fn with_file(self, path: PathBuf) -> Result<T, KainError>;

    fn with_location(self, line: usize, col: usize) -> Result<T, KainError>;

    fn with_context(self, ctx: impl Into<String>) -> Result<T, KainError>;

    fn with_suggestion(self, suggestion: impl Into<String>) -> Result<T, KainError>;
}

impl<T> ErrorContext<T> for Result<T, KainError> {
    fn with_file(self, path: PathBuf) -> Result<T, KainError> {
        self.map_err(|e| match e {
            KainError::Enhanced {
                kind,
                code,
                file: _,
                location,
                context,
                message,
                suggestion,
            } => KainError::Enhanced {
                kind,
                code,
                file: Some(path),
                location,
                context,
                message,
                suggestion,
            },
            KainError::Lexer { message, .. } => KainError::Enhanced {
                kind: ErrorKind::Parse,
                code: default_code_for_kind(ErrorKind::Parse),
                file: Some(path),
                location: None,
                context: String::new(),
                message,
                suggestion: None,
            },
            KainError::Parser { message, .. } => KainError::Enhanced {
                kind: ErrorKind::Parse,
                code: default_code_for_kind(ErrorKind::Parse),
                file: Some(path),
                location: None,
                context: String::new(),
                message,
                suggestion: None,
            },
            KainError::Type { message, .. } => KainError::Enhanced {
                kind: ErrorKind::Type,
                code: default_code_for_kind(ErrorKind::Type),
                file: Some(path),
                location: None,
                context: String::new(),
                message,
                suggestion: None,
            },
            KainError::Effect { message, .. } => KainError::Enhanced {
                kind: ErrorKind::Effect,
                code: default_code_for_kind(ErrorKind::Effect),
                file: Some(path),
                location: None,
                context: String::new(),
                message,
                suggestion: None,
            },
            KainError::Borrow { message, .. } => KainError::Enhanced {
                kind: ErrorKind::Borrow,
                code: default_code_for_kind(ErrorKind::Borrow),
                file: Some(path),
                location: None,
                context: String::new(),
                message,
                suggestion: None,
            },
            KainError::Codegen { message, .. } => KainError::Enhanced {
                kind: ErrorKind::Codegen,
                code: default_code_for_kind(ErrorKind::Codegen),
                file: Some(path),
                location: None,
                context: String::new(),
                message,
                suggestion: None,
            },
            KainError::CodegenWithLocation {
                message,
                file,
                line,
                col,
                span: _,
            } => KainError::Enhanced {
                kind: ErrorKind::Codegen,
                code: default_code_for_kind(ErrorKind::Codegen),
                file: Some(PathBuf::from(file.clone())),
                location: Some((line, col)),
                context: String::new(),
                message,
                suggestion: None,
            },
            KainError::Runtime { message } => KainError::Enhanced {
                kind: ErrorKind::Runtime,
                code: default_code_for_kind(ErrorKind::Runtime),
                file: Some(path),
                location: None,
                context: String::new(),
                message,
                suggestion: None,
            },
            KainError::Io(io_err) => KainError::Enhanced {
                kind: ErrorKind::Io,
                code: default_code_for_kind(ErrorKind::Io),
                file: Some(path),
                location: None,
                context: String::new(),
                message: io_err.to_string(),
                suggestion: None,
            },
            other => other,
        })
    }

    fn with_location(self, line: usize, col: usize) -> Result<T, KainError> {
        self.map_err(|e| match e {
            KainError::Enhanced {
                kind,
                code,
                file,
                location: _,
                context,
                message,
                suggestion,
            } => KainError::Enhanced {
                kind,
                code,
                file,
                location: Some((line, col)),
                context,
                message,
                suggestion,
            },
            other => other,
        })
    }

    fn with_context(self, ctx: impl Into<String>) -> Result<T, KainError> {
        let ctx = ctx.into();
        self.map_err(|e| match e {
            KainError::Enhanced {
                kind,
                code,
                file,
                location,
                context: _,
                message,
                suggestion,
            } => KainError::Enhanced {
                kind,
                code,
                file,
                location,
                context: ctx.clone(),
                message,
                suggestion,
            },
            KainError::Lexer { message, .. } => KainError::Enhanced {
                kind: ErrorKind::Parse,
                code: default_code_for_kind(ErrorKind::Parse),
                file: None,
                location: None,
                context: ctx.clone(),
                message,
                suggestion: None,
            },
            KainError::Parser { message, .. } => KainError::Enhanced {
                kind: ErrorKind::Parse,
                code: default_code_for_kind(ErrorKind::Parse),
                file: None,
                location: None,
                context: ctx.clone(),
                message,
                suggestion: None,
            },
            KainError::Type { message, .. } => KainError::Enhanced {
                kind: ErrorKind::Type,
                code: default_code_for_kind(ErrorKind::Type),
                file: None,
                location: None,
                context: ctx.clone(),
                message,
                suggestion: None,
            },
            KainError::Effect { message, .. } => KainError::Enhanced {
                kind: ErrorKind::Effect,
                code: default_code_for_kind(ErrorKind::Effect),
                file: None,
                location: None,
                context: ctx.clone(),
                message,
                suggestion: None,
            },
            KainError::Borrow { message, .. } => KainError::Enhanced {
                kind: ErrorKind::Borrow,
                code: default_code_for_kind(ErrorKind::Borrow),
                file: None,
                location: None,
                context: ctx.clone(),
                message,
                suggestion: None,
            },
            KainError::Codegen { message, .. } => KainError::Enhanced {
                kind: ErrorKind::Codegen,
                code: default_code_for_kind(ErrorKind::Codegen),
                file: None,
                location: None,
                context: ctx.clone(),
                message,
                suggestion: None,
            },
            KainError::CodegenWithLocation {
                message,
                file,
                line,
                col,
                span: _,
            } => KainError::Enhanced {
                kind: ErrorKind::Codegen,
                code: default_code_for_kind(ErrorKind::Codegen),
                file: Some(PathBuf::from(file.clone())),
                location: Some((line, col)),
                context: ctx.clone(),
                message,
                suggestion: None,
            },
            KainError::Runtime { message } => KainError::Enhanced {
                kind: ErrorKind::Runtime,
                code: default_code_for_kind(ErrorKind::Runtime),
                file: None,
                location: None,
                context: ctx.clone(),
                message,
                suggestion: None,
            },
            KainError::Io(io_err) => KainError::Enhanced {
                kind: ErrorKind::Io,
                code: default_code_for_kind(ErrorKind::Io),
                file: None,
                location: None,
                context: ctx,
                message: io_err.to_string(),
                suggestion: None,
            },
            other => other,
        })
    }

    fn with_suggestion(self, suggestion: impl Into<String>) -> Result<T, KainError> {
        self.map_err(|e| match e {
            KainError::Enhanced {
                kind,
                code,
                file,
                location,
                context,
                message,
                suggestion: _,
            } => KainError::Enhanced {
                kind,
                code,
                file,
                location,
                context,
                message,
                suggestion: Some(suggestion.into()),
            },
            other => other,
        })
    }
}

fn range_to_json(range: &SourceRange) -> JsonValue {
    json!({
        "file": range.file,
        "start": {
            "line": range.start.line,
            "column": range.start.col,
            "display_column": range.start.display_col,
            "offset": range.start.offset,
        },
        "end": {
            "line": range.end.line,
            "column": range.end.col,
            "display_column": range.end.display_col,
            "offset": range.end.offset,
        }
    })
}

fn synthetic_filename(file: &str) -> bool {
    file.starts_with('<') && file.ends_with('>')
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::diagnostic_registry::DiagnosticCode;

    #[test]
    fn enhanced_diagnostic_uses_registry_metadata() {
        let err = DiagnosticBuilder::new(
            ErrorKind::Validation,
            DiagnosticCode::MemoryLoweringRequired,
            "Lowering policy missing for pointer semantics",
        )
        .context("Lowering low-level memory IR")
        .build();

        let rendered = err.to_string();
        assert!(rendered.contains("KAIN-MEM-0001"));
        assert!(rendered.contains("Memory Lowering Required"));
        assert!(rendered.contains("Reference: memory/lowering"));
        assert!(rendered.contains("Help:"));
    }

    #[test]
    fn diagnostic_json_is_normalized_to_diagnostics_array() {
        let err = KainError::rich(
            DiagnosticReport::new(
                ErrorKind::Parse,
                DiagnosticCode::ParseExpectedToken,
                "Expected ':', got newline",
            )
            .primary_label(Span::new(4, 4), "expected ':' here"),
        );

        let json = err.diagnostic_json().expect("json diagnostics");
        assert_eq!(json["diagnostics"][0]["code"], "KAIN-PARSE-0002");
        assert_eq!(json["diagnostics"][0]["title"], "Expected Token");
        assert_eq!(json["diagnostics"][0]["category"], "parse");
    }
}
