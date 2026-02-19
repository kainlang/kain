//! Error types for the KAIN compiler

use crate::span::Span;
use std::path::PathBuf;
use std::fmt;
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

    #[error("Runtime error: {message}")]
    Runtime { message: String },

    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
    
    #[error("{}", format_enhanced_error(.kind, .file, .location, .context, .message, .suggestion))]
    Enhanced {
        kind: ErrorKind,
        file: Option<PathBuf>,
        location: Option<(usize, usize)>, // (line, column)
        context: String,
        message: String,
        suggestion: Option<String>,
    },
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
    file: &Option<PathBuf>,
    location: &Option<(usize, usize)>,
    context: &str,
    message: &str,
    suggestion: &Option<String>,
) -> String {
    let mut output = String::new();
    
    // Error header
    output.push_str(&format!("❌ [{}] Error", kind));
    
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
    if let Some(suggestion) = suggestion {
        output.push_str(&format!("\n   Help: {}\n", suggestion));
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

    pub fn runtime(message: impl Into<String>) -> Self {
        KainError::Runtime {
            message: message.into(),
        }
    }
    
    // New enhanced error constructors
    pub fn parse_error(message: impl Into<String>) -> Self {
        KainError::Enhanced {
            kind: ErrorKind::Parse,
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
            KainError::Enhanced { kind, file: _, location, context, message, suggestion } => {
                KainError::Enhanced {
                    kind,
                    file: Some(path),
                    location,
                    context,
                    message,
                    suggestion,
                }
            }
            // For legacy error types, convert to Enhanced
            KainError::Lexer { message, span } => {
                KainError::Enhanced {
                    kind: ErrorKind::Parse,
                    file: Some(path),
                    location: Some((span.start, span.end)),
                    context: String::new(),
                    message,
                    suggestion: None,
                }
            }
            KainError::Parser { message, span } => {
                KainError::Enhanced {
                    kind: ErrorKind::Parse,
                    file: Some(path),
                    location: Some((span.start, span.end)),
                    context: String::new(),
                    message,
                    suggestion: None,
                }
            }
            KainError::Type { message, span } => {
                KainError::Enhanced {
                    kind: ErrorKind::Type,
                    file: Some(path),
                    location: Some((span.start, span.end)),
                    context: String::new(),
                    message,
                    suggestion: None,
                }
            }
            KainError::Codegen { message, span } => {
                KainError::Enhanced {
                    kind: ErrorKind::Codegen,
                    file: Some(path),
                    location: Some((span.start, span.end)),
                    context: String::new(),
                    message,
                    suggestion: None,
                }
            }
            KainError::Io(io_err) => {
                KainError::Enhanced {
                    kind: ErrorKind::Io,
                    file: Some(path),
                    location: None,
                    context: String::new(),
                    message: io_err.to_string(),
                    suggestion: None,
                }
            }
            other => other,
        })
    }
    
    fn with_location(self, line: usize, col: usize) -> Result<T, KainError> {
        self.map_err(|e| match e {
            KainError::Enhanced { kind, file, location: _, context, message, suggestion } => {
                KainError::Enhanced {
                    kind,
                    file,
                    location: Some((line, col)),
                    context,
                    message,
                    suggestion,
                }
            }
            other => other,
        })
    }
    
    fn with_context(self, ctx: impl Into<String>) -> Result<T, KainError> {
        self.map_err(|e| match e {
            KainError::Enhanced { kind, file, location, context: _, message, suggestion } => {
                KainError::Enhanced {
                    kind,
                    file,
                    location,
                    context: ctx.into(),
                    message,
                    suggestion,
                }
            }
            // For legacy error types, convert to Enhanced
            KainError::Lexer { message, span } => {
                KainError::Enhanced {
                    kind: ErrorKind::Parse,
                    file: None,
                    location: Some((span.start, span.end)),
                    context: ctx.into(),
                    message,
                    suggestion: None,
                }
            }
            KainError::Parser { message, span } => {
                KainError::Enhanced {
                    kind: ErrorKind::Parse,
                    file: None,
                    location: Some((span.start, span.end)),
                    context: ctx.into(),
                    message,
                    suggestion: None,
                }
            }
            KainError::Type { message, span } => {
                KainError::Enhanced {
                    kind: ErrorKind::Type,
                    file: None,
                    location: Some((span.start, span.end)),
                    context: ctx.into(),
                    message,
                    suggestion: None,
                }
            }
            KainError::Codegen { message, span } => {
                KainError::Enhanced {
                    kind: ErrorKind::Codegen,
                    file: None,
                    location: Some((span.start, span.end)),
                    context: ctx.into(),
                    message,
                    suggestion: None,
                }
            }
            KainError::Io(io_err) => {
                KainError::Enhanced {
                    kind: ErrorKind::Io,
                    file: None,
                    location: None,
                    context: ctx.into(),
                    message: io_err.to_string(),
                    suggestion: None,
                }
            }
            other => other,
        })
    }
    
    fn with_suggestion(self, suggestion: impl Into<String>) -> Result<T, KainError> {
        self.map_err(|e| match e {
            KainError::Enhanced { kind, file, location, context, message, suggestion: _ } => {
                KainError::Enhanced {
                    kind,
                    file,
                    location,
                    context,
                    message,
                    suggestion: Some(suggestion.into()),
                }
            }
            other => other,
        })
    }
}
