//! CLI Error Types

use std::fmt;

/// CLI-specific error type
#[derive(Debug)]
pub enum KainError {
    /// Runtime error with message
    Runtime(String),
    /// IO error
    Io(std::io::Error),
    /// Core compiler error
    Core(kain_core::error::KainError),
}

impl fmt::Display for KainError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            KainError::Runtime(msg) => write!(f, "{}", msg),
            KainError::Io(e) => write!(f, "IO error: {}", e),
            KainError::Core(e) => write!(f, "{}", e),
        }
    }
}

impl std::error::Error for KainError {}

impl From<std::io::Error> for KainError {
    fn from(e: std::io::Error) -> Self {
        KainError::Io(e)
    }
}

impl From<kain_core::error::KainError> for KainError {
    fn from(e: kain_core::error::KainError) -> Self {
        KainError::Core(e)
    }
}

impl KainError {
    /// Create a runtime error
    pub fn runtime(msg: impl Into<String>) -> Self {
        KainError::Runtime(msg.into())
    }
}

/// CLI Result type
pub type KainResult<T> = Result<T, KainError>;
