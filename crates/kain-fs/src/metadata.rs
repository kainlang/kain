use std::fs::{DirEntry, Metadata};
use std::path::PathBuf;
use std::time::UNIX_EPOCH;

use crate::{FsError, FsResult};

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum FsFileType {
    File,
    Directory,
    Symlink,
    Other,
}

impl FsFileType {
    pub fn as_str(&self) -> &'static str {
        match self {
            FsFileType::File => "file",
            FsFileType::Directory => "directory",
            FsFileType::Symlink => "symlink",
            FsFileType::Other => "other",
        }
    }
}

#[derive(Debug, Clone)]
pub struct FsMetadata {
    pub file_type: FsFileType,
    pub len: u64,
    pub readonly: bool,
    pub created_millis: Option<u128>,
    pub modified_millis: Option<u128>,
    pub accessed_millis: Option<u128>,
}

impl FsMetadata {
    pub fn from_std(metadata: Metadata) -> Self {
        let file_type = metadata.file_type();
        Self {
            file_type: if file_type.is_file() {
                FsFileType::File
            } else if file_type.is_dir() {
                FsFileType::Directory
            } else if file_type.is_symlink() {
                FsFileType::Symlink
            } else {
                FsFileType::Other
            },
            len: metadata.len(),
            readonly: metadata.permissions().readonly(),
            created_millis: system_time_millis(metadata.created().ok()),
            modified_millis: system_time_millis(metadata.modified().ok()),
            accessed_millis: system_time_millis(metadata.accessed().ok()),
        }
    }
}

#[derive(Debug, Clone)]
pub struct DirectoryEntry {
    pub path: PathBuf,
    pub file_name: String,
    pub file_type: FsFileType,
    pub metadata: FsMetadata,
}

impl DirectoryEntry {
    pub fn from_std(operation: &'static str, entry: DirEntry) -> FsResult<Self> {
        let path = entry.path();
        let file_name = entry.file_name().to_string_lossy().into_owned();
        let metadata = entry
            .metadata()
            .map_err(|error| FsError::from_io(operation, &path, error))?;
        let metadata = FsMetadata::from_std(metadata);
        Ok(Self {
            path,
            file_name,
            file_type: metadata.file_type.clone(),
            metadata,
        })
    }
}

fn system_time_millis(value: Option<std::time::SystemTime>) -> Option<u128> {
    value
        .and_then(|time| time.duration_since(UNIX_EPOCH).ok())
        .map(|duration| duration.as_millis())
}
