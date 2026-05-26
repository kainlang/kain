use std::fmt;
use std::io;
use std::path::{Path, PathBuf};

pub type FsResult<T> = Result<T, FsError>;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum FsErrorKind {
    NotFound,
    AlreadyExists,
    AccessDenied,
    InvalidInput,
    NotADirectory,
    IsDirectory,
    DirectoryNotEmpty,
    CrossDevice,
    Interrupted,
    Unsupported,
    GlobPattern,
    GlobRead,
    Other,
}

impl FsErrorKind {
    pub fn as_str(&self) -> &'static str {
        match self {
            FsErrorKind::NotFound => "not_found",
            FsErrorKind::AlreadyExists => "already_exists",
            FsErrorKind::AccessDenied => "access_denied",
            FsErrorKind::InvalidInput => "invalid_input",
            FsErrorKind::NotADirectory => "not_a_directory",
            FsErrorKind::IsDirectory => "is_directory",
            FsErrorKind::DirectoryNotEmpty => "directory_not_empty",
            FsErrorKind::CrossDevice => "cross_device",
            FsErrorKind::Interrupted => "interrupted",
            FsErrorKind::Unsupported => "unsupported",
            FsErrorKind::GlobPattern => "glob_pattern",
            FsErrorKind::GlobRead => "glob_read",
            FsErrorKind::Other => "other",
        }
    }
}

#[derive(Debug, Clone)]
pub struct FsError {
    pub kind: FsErrorKind,
    pub operation: String,
    pub path: PathBuf,
    pub other_path: Option<PathBuf>,
    pub message: String,
    pub raw_code: Option<i32>,
}

impl FsError {
    pub fn new(
        operation: impl Into<String>,
        path: impl Into<PathBuf>,
        kind: FsErrorKind,
        message: impl Into<String>,
    ) -> Self {
        Self {
            kind,
            operation: operation.into(),
            path: path.into(),
            other_path: None,
            message: message.into(),
            raw_code: None,
        }
    }

    pub fn with_other_path(mut self, other_path: impl Into<PathBuf>) -> Self {
        self.other_path = Some(other_path.into());
        self
    }

    pub fn with_raw_code(mut self, raw_code: Option<i32>) -> Self {
        self.raw_code = raw_code;
        self
    }

    pub fn from_io(operation: impl Into<String>, path: impl AsRef<Path>, error: io::Error) -> Self {
        let kind = match error.kind() {
            io::ErrorKind::NotFound => FsErrorKind::NotFound,
            io::ErrorKind::PermissionDenied => FsErrorKind::AccessDenied,
            io::ErrorKind::AlreadyExists => FsErrorKind::AlreadyExists,
            io::ErrorKind::InvalidInput | io::ErrorKind::InvalidData => FsErrorKind::InvalidInput,
            io::ErrorKind::NotADirectory => FsErrorKind::NotADirectory,
            io::ErrorKind::IsADirectory => FsErrorKind::IsDirectory,
            io::ErrorKind::DirectoryNotEmpty => FsErrorKind::DirectoryNotEmpty,
            io::ErrorKind::CrossesDevices => FsErrorKind::CrossDevice,
            io::ErrorKind::Interrupted => FsErrorKind::Interrupted,
            io::ErrorKind::Unsupported => FsErrorKind::Unsupported,
            _ => FsErrorKind::Other,
        };
        Self {
            kind,
            operation: operation.into(),
            path: path.as_ref().to_path_buf(),
            other_path: None,
            message: error.to_string(),
            raw_code: error.raw_os_error(),
        }
    }

    pub fn from_two_path_io(
        operation: impl Into<String>,
        path: impl AsRef<Path>,
        other_path: impl AsRef<Path>,
        error: io::Error,
    ) -> Self {
        Self::from_io(operation, path, error).with_other_path(other_path.as_ref())
    }
}

impl fmt::Display for FsError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            formatter,
            "{} failed for '{}': {}",
            self.operation,
            self.path.display(),
            self.message
        )?;
        if let Some(other_path) = &self.other_path {
            write!(formatter, " (other path: '{}')", other_path.display())?;
        }
        if let Some(raw_code) = self.raw_code {
            write!(formatter, " [os code {}]", raw_code)?;
        }
        Ok(())
    }
}

impl std::error::Error for FsError {}
