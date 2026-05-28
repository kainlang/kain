//! Diagnostic severity levels.

use serde::{Deserialize, Serialize};
use std::fmt;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum DiagnosticSeverity {
    /// A bug or invalid construct — compilation cannot proceed.
    Error,
    /// Suspicious but legal — compilation can continue.
    Warning,
    /// Supplemental context attached to another diagnostic.
    Note,
    /// Actionable advice for fixing an error or improving code.
    Help,
}

impl DiagnosticSeverity {
    /// Parse from a TOML severity string.
    pub fn from_str(s: &str) -> Self {
        match s {
            "warning" => Self::Warning,
            "note" => Self::Note,
            "help" => Self::Help,
            _ => Self::Error,
        }
    }

    /// True if this severity should be treated as a compilation failure.
    pub fn is_error(self) -> bool {
        matches!(self, Self::Error)
    }

    /// True if this severity should be shown by default.
    pub fn is_user_facing(self) -> bool {
        true
    }
}

impl fmt::Display for DiagnosticSeverity {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Error => write!(f, "error"),
            Self::Warning => write!(f, "warning"),
            Self::Note => write!(f, "note"),
            Self::Help => write!(f, "help"),
        }
    }
}

impl Default for DiagnosticSeverity {
    fn default() -> Self {
        Self::Error
    }
}
