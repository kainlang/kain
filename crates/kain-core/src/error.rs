//! Error types for the KAIN compiler

use crate::diagnostic_registry::{default_code_for_kind, spec_for_code, DiagnosticCode};
use crate::span::Span;
use std::fmt;
use std::path::PathBuf;
use thiserror::Error;

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

    /// Codegen error with file:line:col location information
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
        location: Option<(usize, usize)>, // (line, column)
        context: String,
        message: String,
        suggestion: Option<String>,
    },

    /// Multiple errors collected during error-recovery parsing
    #[error("{}", format_multi_errors(.0))]
    Multi(Vec<KainError>),
}

#[derive(Debug, Clone, PartialEq)]
pub enum ErrorKind {
    Parse,
    Type,
    Validation,
    Codegen,
    Io,
    Config,
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

    // Error header
    output.push_str(&format!("❌ [{}:{}] {}", kind, spec.code_str, spec.title));

    // File and location
    if let Some(path) = file {
        output.push_str(&format!(" in {}", path.display()));
        if let Some((line, col)) = location {
            output.push_str(&format!(":{}:{}", line, col));
        }
    }

    output.push_str("\n\n");

    // Context if provided
    if !context.is_empty() {
        output.push_str(&format!("   Context: {}\n", context));
    }

    // Main error message
    output.push_str(&format!("   {}\n", message));

    // Suggestion if provided
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

    /// Create a multi-error from collected parse errors
    pub fn multi(errors: Vec<KainError>) -> Self {
        debug_assert!(
            !errors.is_empty(),
            "Multi error must contain at least one error"
        );
        KainError::Multi(errors)
    }

    // New enhanced error constructors
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
}

/// Result type for KAIN operations
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

/// Trait for adding context to errors
pub trait ErrorContext<T> {
    /// Add file path context to the error
    fn with_file(self, path: PathBuf) -> Result<T, KainError>;

    /// Add location (line, column) context to the error
    fn with_location(self, line: usize, col: usize) -> Result<T, KainError>;

    /// Add contextual information about what operation was being performed
    fn with_context(self, ctx: impl Into<String>) -> Result<T, KainError>;

    /// Add a suggestion for how to fix the error
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
            // For legacy error types, convert to Enhanced
            KainError::Lexer { message, span } => KainError::Enhanced {
                kind: ErrorKind::Parse,
                code: default_code_for_kind(ErrorKind::Parse),
                file: Some(path),
                location: Some((span.start, span.end)),
                context: String::new(),
                message,
                suggestion: None,
            },
            KainError::Parser { message, span } => KainError::Enhanced {
                kind: ErrorKind::Parse,
                code: default_code_for_kind(ErrorKind::Parse),
                file: Some(path),
                location: Some((span.start, span.end)),
                context: String::new(),
                message,
                suggestion: None,
            },
            KainError::Type { message, span } => KainError::Enhanced {
                kind: ErrorKind::Type,
                code: default_code_for_kind(ErrorKind::Type),
                file: Some(path),
                location: Some((span.start, span.end)),
                context: String::new(),
                message,
                suggestion: None,
            },
            KainError::Codegen { message, span } => KainError::Enhanced {
                kind: ErrorKind::Codegen,
                code: default_code_for_kind(ErrorKind::Codegen),
                file: Some(path),
                location: Some((span.start, span.end)),
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
                file: Some(PathBuf::from(file)),
                location: Some((line, col)),
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
                context: ctx.into(),
                message,
                suggestion,
            },
            // For legacy error types, convert to Enhanced
            KainError::Lexer { message, span } => KainError::Enhanced {
                kind: ErrorKind::Parse,
                code: default_code_for_kind(ErrorKind::Parse),
                file: None,
                location: Some((span.start, span.end)),
                context: ctx.into(),
                message,
                suggestion: None,
            },
            KainError::Parser { message, span } => KainError::Enhanced {
                kind: ErrorKind::Parse,
                code: default_code_for_kind(ErrorKind::Parse),
                file: None,
                location: Some((span.start, span.end)),
                context: ctx.into(),
                message,
                suggestion: None,
            },
            KainError::Type { message, span } => KainError::Enhanced {
                kind: ErrorKind::Type,
                code: default_code_for_kind(ErrorKind::Type),
                file: None,
                location: Some((span.start, span.end)),
                context: ctx.into(),
                message,
                suggestion: None,
            },
            KainError::Codegen { message, span } => KainError::Enhanced {
                kind: ErrorKind::Codegen,
                code: default_code_for_kind(ErrorKind::Codegen),
                file: None,
                location: Some((span.start, span.end)),
                context: ctx.into(),
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
                file: Some(PathBuf::from(file)),
                location: Some((line, col)),
                context: ctx.into(),
                message,
                suggestion: None,
            },
            KainError::Io(io_err) => KainError::Enhanced {
                kind: ErrorKind::Io,
                code: default_code_for_kind(ErrorKind::Io),
                file: None,
                location: None,
                context: ctx.into(),
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

/// Convert a TokenKind to user-friendly string representation
/// This replaces debug formatting ({:?}) with readable syntax
pub fn token_kind_to_user_string(kind: &crate::lexer::TokenKind) -> String {
    use crate::lexer::TokenKind;
    match kind {
        // Keywords
        TokenKind::Fn => "keyword 'fn'".to_string(),
        TokenKind::Let => "keyword 'let'".to_string(),
        TokenKind::Mut => "keyword 'mut'".to_string(),
        TokenKind::Var => "keyword 'var'".to_string(),
        TokenKind::Const => "keyword 'const'".to_string(),
        TokenKind::If => "keyword 'if'".to_string(),
        TokenKind::Else => "keyword 'else'".to_string(),
        TokenKind::Elif => "keyword 'elif'".to_string(),
        TokenKind::Match => "keyword 'match'".to_string(),
        TokenKind::For => "keyword 'for'".to_string(),
        TokenKind::While => "keyword 'while'".to_string(),
        TokenKind::Loop => "keyword 'loop'".to_string(),
        TokenKind::Break => "keyword 'break'".to_string(),
        TokenKind::Continue => "keyword 'continue'".to_string(),
        TokenKind::Return => "keyword 'return'".to_string(),
        TokenKind::Await => "keyword 'await'".to_string(),
        TokenKind::In => "keyword 'in'".to_string(),
        TokenKind::With => "keyword 'with'".to_string(),
        TokenKind::As => "keyword 'as'".to_string(),
        TokenKind::TypeKw => "keyword 'type'".to_string(),
        TokenKind::Struct => "keyword 'struct'".to_string(),
        TokenKind::Enum => "keyword 'enum'".to_string(),
        TokenKind::Trait => "keyword 'trait'".to_string(),
        TokenKind::Impl => "keyword 'impl'".to_string(),
        TokenKind::Pub => "keyword 'pub'".to_string(),
        TokenKind::Mod => "keyword 'mod'".to_string(),
        TokenKind::Use => "keyword 'use'".to_string(),
        TokenKind::SelfLower => "keyword 'self'".to_string(),
        TokenKind::SelfUpper => "keyword 'Self'".to_string(),
        TokenKind::True => "keyword 'true'".to_string(),
        TokenKind::False => "keyword 'false'".to_string(),
        TokenKind::None => "keyword 'none'".to_string(),

        // Special keywords
        TokenKind::Component => "keyword 'component'".to_string(),
        TokenKind::Shader => "keyword 'shader'".to_string(),
        TokenKind::Actor => "keyword 'actor'".to_string(),
        TokenKind::State => "keyword 'state'".to_string(),
        TokenKind::Spawn => "keyword 'spawn'".to_string(),
        TokenKind::Send => "keyword 'send'".to_string(),
        TokenKind::Receive => "keyword 'receive'".to_string(),
        TokenKind::Emit => "keyword 'emit'".to_string(),
        TokenKind::Comptime => "keyword 'comptime'".to_string(),
        TokenKind::Macro => "keyword 'macro'".to_string(),
        TokenKind::Vertex => "keyword 'vertex'".to_string(),
        TokenKind::Fragment => "keyword 'fragment'".to_string(),
        TokenKind::Collapse => "keyword 'collapse'".to_string(),
        TokenKind::Observe => "keyword 'observe'".to_string(),
        TokenKind::Decay => "keyword 'decay'".to_string(),
        TokenKind::Test => "keyword 'test'".to_string(),

        // Effect keywords
        TokenKind::Pure => "keyword 'Pure'".to_string(),
        TokenKind::Io => "keyword 'IO'".to_string(),
        TokenKind::AsyncKw => "keyword 'async'".to_string(),
        TokenKind::Async => "keyword 'Async'".to_string(),
        TokenKind::Gpu => "keyword 'GPU'".to_string(),
        TokenKind::Reactive => "keyword 'Reactive'".to_string(),
        TokenKind::Unsafe => "keyword 'Unsafe'".to_string(),

        // Literals
        TokenKind::Int(n) => format!("number {}", n),
        TokenKind::Float(f) => format!("number {}", f),
        TokenKind::String(s) => format!("string \"{}\"", s),
        TokenKind::FString(s) => format!("f-string f\"{}\"", s),
        TokenKind::Char(c) => format!("character '{}'", c),
        TokenKind::Ident(name) => format!("identifier '{}'", name),

        // Operators
        TokenKind::PlusPlus => "'++'".to_string(),
        TokenKind::MinusMinus => "'--'".to_string(),
        TokenKind::Plus => "'+'".to_string(),
        TokenKind::Minus => "'-'".to_string(),
        TokenKind::Star => "'*'".to_string(),
        TokenKind::Slash => "'/'".to_string(),
        TokenKind::Percent => "'%'".to_string(),
        TokenKind::Power => "'**'".to_string(),
        TokenKind::EqEq => "'=='".to_string(),
        TokenKind::NotEq => "'!='".to_string(),
        TokenKind::Lt => "'<'".to_string(),
        TokenKind::Gt => "'>'".to_string(),
        TokenKind::LtEq => "'<='".to_string(),
        TokenKind::GtEq => "'>='".to_string(),
        TokenKind::And => "'&&' or 'and'".to_string(),
        TokenKind::Or => "'||' or 'or'".to_string(),
        TokenKind::Not => "'!'".to_string(),
        TokenKind::Amp => "'&'".to_string(),
        TokenKind::Pipe => "'|'".to_string(),
        TokenKind::Caret => "'^'".to_string(),
        TokenKind::Tilde => "'~'".to_string(),
        TokenKind::Shl => "'<<'".to_string(),
        TokenKind::Shr => "'>>'".to_string(),

        // Assignment
        TokenKind::Eq => "'='".to_string(),
        TokenKind::PlusEq => "'+='".to_string(),
        TokenKind::MinusEq => "'-='".to_string(),
        TokenKind::StarEq => "'*='".to_string(),
        TokenKind::SlashEq => "'/='".to_string(),
        TokenKind::PercentEq => "'%='".to_string(),
        TokenKind::AmpEq => "'&='".to_string(),
        TokenKind::PipeEq => "'|='".to_string(),
        TokenKind::CaretEq => "'^='".to_string(),
        TokenKind::ShlEq => "'<<='".to_string(),
        TokenKind::ShrEq => "'>>='".to_string(),

        // Punctuation
        TokenKind::LParen => "'('".to_string(),
        TokenKind::RParen => "')'".to_string(),
        TokenKind::LBracket => "'['".to_string(),
        TokenKind::RBracket => "']'".to_string(),
        TokenKind::LBrace => "'{'".to_string(),
        TokenKind::RBrace => "'}'".to_string(),
        TokenKind::Comma => "','".to_string(),
        TokenKind::Dot => "'.'".to_string(),
        TokenKind::DotDot => "'..".to_string(),
        TokenKind::DotDotDot => "'...'".to_string(),
        TokenKind::Colon => "':'".to_string(),
        TokenKind::ColonColon => "'::'".to_string(),
        TokenKind::Semi => "';'".to_string(),
        TokenKind::Arrow => "'->'".to_string(),
        TokenKind::FatArrow => "'=>'".to_string(),
        TokenKind::At => "'@'".to_string(),
        TokenKind::QuestionQuestion => "'??'".to_string(),
        TokenKind::QuestionDot => "'?.'".to_string(),
        TokenKind::Question => "'?'".to_string(),

        // JSX-like
        TokenKind::LtSlash => "'</'".to_string(),

        // Whitespace
        TokenKind::Newline(_) => "newline".to_string(),
        TokenKind::Comment => "comment".to_string(),
        TokenKind::HashComment => "comment".to_string(),
        TokenKind::Indent => "indentation".to_string(),
        TokenKind::Dedent => "dedentation".to_string(),
        TokenKind::Eof => "end of file".to_string(),
    }
}

/// Convert a Token to user-friendly string representation
pub fn token_to_user_string(token: &crate::lexer::Token) -> String {
    token_kind_to_user_string(&token.kind)
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
}
