//! Legacy-compatible public error surface for drop-in replacement.
//!
//! The scratch crate keeps the richer data-driven internals (`report`,
//! `registry`, `render`, `json`, etc.) but this module preserves the
//! historical API shape that `kain-core` and friends compile against.

use crate::diagnostic_registry::{default_code_for_kind, spec_for_code, DiagnosticCode};
use crate::json::diagnostics_to_json;
use crate::span::Span;
use serde_json::Value as JsonValue;
use std::fmt;
use std::path::PathBuf;

pub use crate::label::{DiagnosticFixIt, DiagnosticLabel};
pub use crate::report::{CompilerPhase, DebugTraceEntry, DiagnosticReport, ErrorKind};
pub use crate::severity::DiagnosticSeverity;

#[derive(Debug)]
pub enum KainError {
    Lexer {
        message: String,
        span: Span,
    },
    Parser {
        message: String,
        span: Span,
    },
    Type {
        message: String,
        span: Span,
    },
    Effect {
        message: String,
        span: Span,
    },
    Borrow {
        message: String,
        span: Span,
    },
    Codegen {
        message: String,
        span: Span,
    },
    CodegenWithLocation {
        message: String,
        file: String,
        line: usize,
        col: usize,
        span: Span,
    },
    Runtime {
        message: String,
    },
    Io(std::io::Error),
    Enhanced {
        kind: ErrorKind,
        code: DiagnosticCode,
        file: Option<PathBuf>,
        location: Option<(usize, usize)>,
        context: String,
        message: String,
        suggestion: Option<String>,
    },
    Rich(Box<DiagnosticReport>),
    Multi(Vec<KainError>),
}

impl fmt::Display for KainError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            KainError::Lexer { message, span } => {
                write!(f, "Lexer error at {span:?}: {message}")
            }
            KainError::Parser { message, span } => {
                write!(f, "Parser error at {span:?}: {message}")
            }
            KainError::Type { message, span } => {
                write!(f, "Type error at {span:?}: {message}")
            }
            KainError::Effect { message, span } => {
                write!(f, "Effect error at {span:?}: {message}")
            }
            KainError::Borrow { message, span } => {
                write!(f, "Borrow error at {span:?}: {message}")
            }
            KainError::Codegen { message, span } => {
                write!(f, "Codegen error at {span:?}: {message}")
            }
            KainError::CodegenWithLocation {
                message,
                file,
                line,
                col,
                ..
            } => write!(f, "{file}:{line}:{col}: {message}"),
            KainError::Runtime { message } => write!(f, "Runtime error: {message}"),
            KainError::Io(err) => write!(f, "IO error: {err}"),
            KainError::Enhanced {
                kind,
                code,
                file,
                location,
                context,
                message,
                suggestion,
            } => write!(
                f,
                "{}",
                format_enhanced_error(kind, code, file, location, context, message, suggestion)
            ),
            KainError::Rich(report) => write!(f, "{report}"),
            KainError::Multi(errors) => write!(f, "{}", format_multi_errors(errors)),
        }
    }
}

impl std::error::Error for KainError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            KainError::Io(err) => Some(err),
            _ => None,
        }
    }
}

impl From<std::io::Error> for KainError {
    fn from(err: std::io::Error) -> Self {
        KainError::Io(err)
    }
}

impl From<DiagnosticReport> for KainError {
    fn from(report: DiagnosticReport) -> Self {
        KainError::Rich(Box::new(report))
    }
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

    pub fn multi<E>(errors: Vec<E>) -> Self
    where
        E: Into<KainError>,
    {
        debug_assert!(
            !errors.is_empty(),
            "Multi error must contain at least one error"
        );
        KainError::Multi(errors.into_iter().map(Into::into).collect())
    }

    pub fn simple(code: DiagnosticCode, message: impl Into<String>) -> Self {
        KainError::Enhanced {
            kind: kind_from_code(code),
            code,
            file: None,
            location: None,
            context: String::new(),
            message: message.into(),
            suggestion: None,
        }
    }

    pub fn with_file(mut self, path: impl Into<PathBuf>) -> Self {
        let path = path.into();
        match &mut self {
            KainError::Enhanced { file, .. } => *file = Some(path),
            _ => {}
        }
        self
    }

    pub fn with_location(mut self, line: usize, col: usize) -> Self {
        match &mut self {
            KainError::Enhanced { location, .. } => *location = Some((line, col)),
            _ => {}
        }
        self
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
        let diagnostics = self.to_diagnostic_reports();
        if diagnostics.is_empty() {
            None
        } else {
            Some(diagnostics_to_json(&diagnostics))
        }
    }

    pub fn to_json_value(&self) -> JsonValue {
        diagnostics_to_json(&self.to_diagnostic_reports())
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

    if let Some(help) = suggestion.as_deref().or(spec.default_suggestion) {
        output.push_str(&format!("\n   Help: {}\n", help));
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
                file: Some(path.clone()),
                location,
                context,
                message,
                suggestion,
            },
            KainError::Lexer { message, .. } => make_enhanced(
                ErrorKind::Parse,
                message,
                Some(path.clone()),
                None,
                String::new(),
                None,
            ),
            KainError::Parser { message, .. } => make_enhanced(
                ErrorKind::Parse,
                message,
                Some(path.clone()),
                None,
                String::new(),
                None,
            ),
            KainError::Type { message, .. } => make_enhanced(
                ErrorKind::Type,
                message,
                Some(path.clone()),
                None,
                String::new(),
                None,
            ),
            KainError::Effect { message, .. } => make_enhanced(
                ErrorKind::Effect,
                message,
                Some(path.clone()),
                None,
                String::new(),
                None,
            ),
            KainError::Borrow { message, .. } => make_enhanced(
                ErrorKind::Borrow,
                message,
                Some(path.clone()),
                None,
                String::new(),
                None,
            ),
            KainError::Codegen { message, .. } => make_enhanced(
                ErrorKind::Codegen,
                message,
                Some(path.clone()),
                None,
                String::new(),
                None,
            ),
            KainError::CodegenWithLocation {
                message,
                file,
                line,
                col,
                ..
            } => KainError::Enhanced {
                kind: ErrorKind::Codegen,
                code: default_code_for_kind(ErrorKind::Codegen),
                file: Some(PathBuf::from(file)),
                location: Some((line, col)),
                context: String::new(),
                message,
                suggestion: None,
            },
            KainError::Runtime { message } => make_enhanced(
                ErrorKind::Runtime,
                message,
                Some(path),
                None,
                String::new(),
                None,
            ),
            KainError::Io(io_err) => make_enhanced(
                ErrorKind::Io,
                io_err.to_string(),
                Some(path),
                None,
                String::new(),
                None,
            ),
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
            KainError::Lexer { message, .. } => {
                make_enhanced(ErrorKind::Parse, message, None, None, ctx.clone(), None)
            }
            KainError::Parser { message, .. } => {
                make_enhanced(ErrorKind::Parse, message, None, None, ctx.clone(), None)
            }
            KainError::Type { message, .. } => {
                make_enhanced(ErrorKind::Type, message, None, None, ctx.clone(), None)
            }
            KainError::Effect { message, .. } => {
                make_enhanced(ErrorKind::Effect, message, None, None, ctx.clone(), None)
            }
            KainError::Borrow { message, .. } => {
                make_enhanced(ErrorKind::Borrow, message, None, None, ctx.clone(), None)
            }
            KainError::Codegen { message, .. } => {
                make_enhanced(ErrorKind::Codegen, message, None, None, ctx.clone(), None)
            }
            KainError::CodegenWithLocation {
                message,
                file,
                line,
                col,
                ..
            } => KainError::Enhanced {
                kind: ErrorKind::Codegen,
                code: default_code_for_kind(ErrorKind::Codegen),
                file: Some(PathBuf::from(file)),
                location: Some((line, col)),
                context: ctx.clone(),
                message,
                suggestion: None,
            },
            KainError::Runtime { message } => {
                make_enhanced(ErrorKind::Runtime, message, None, None, ctx.clone(), None)
            }
            KainError::Io(io_err) => {
                make_enhanced(ErrorKind::Io, io_err.to_string(), None, None, ctx, None)
            }
            other => other,
        })
    }

    fn with_suggestion(self, suggestion: impl Into<String>) -> Result<T, KainError> {
        let suggestion = suggestion.into();
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
                suggestion: Some(suggestion.clone()),
            },
            other => other,
        })
    }
}

fn make_enhanced(
    kind: ErrorKind,
    message: impl Into<String>,
    file: Option<PathBuf>,
    location: Option<(usize, usize)>,
    context: String,
    suggestion: Option<String>,
) -> KainError {
    KainError::Enhanced {
        kind,
        code: default_code_for_kind(kind),
        file,
        location,
        context,
        message: message.into(),
        suggestion,
    }
}

fn kind_from_code(code: DiagnosticCode) -> ErrorKind {
    match code.category_prefix() {
        "PARSE" => ErrorKind::Parse,
        "TYPE" => ErrorKind::Type,
        "VALIDATE" => ErrorKind::Validation,
        "CODEGEN" => ErrorKind::Codegen,
        "SHADER" => ErrorKind::Shader,
        "IO" => ErrorKind::Io,
        "CONFIG" => ErrorKind::Config,
        "EFFECT" => ErrorKind::Effect,
        "BORROW" => ErrorKind::Borrow,
        "RUNTIME" => ErrorKind::Runtime,
        "MEM" => ErrorKind::Memory,
        "WORLD" => ErrorKind::World,
        "ACTOR" => ErrorKind::Component,
        "COMPTIME" => ErrorKind::Comptime,
        "STATE" => ErrorKind::State,
        "TEST" => ErrorKind::Test,
        _ => ErrorKind::Internal,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

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
