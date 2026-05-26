use std::fs::{File, OpenOptions};
use std::io::{Read, Write};
use std::path::{Path, PathBuf};

use crate::{FsError, FsResult};

#[derive(Debug, Clone)]
pub struct NativePath {
    path: PathBuf,
}

impl NativePath {
    pub fn new(path: impl Into<PathBuf>) -> Self {
        Self { path: path.into() }
    }

    pub fn as_path(&self) -> &Path {
        &self.path
    }
}

pub struct RawFile {
    file: File,
}

impl RawFile {
    pub fn open(path: &NativePath) -> FsResult<Self> {
        File::open(path.as_path())
            .map(|file| Self { file })
            .map_err(|error| FsError::from_io("raw_open", path.as_path(), error))
    }

    pub fn create(path: &NativePath) -> FsResult<Self> {
        File::create(path.as_path())
            .map(|file| Self { file })
            .map_err(|error| FsError::from_io("raw_create", path.as_path(), error))
    }

    pub fn create_new(path: &NativePath) -> FsResult<Self> {
        OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(path.as_path())
            .map(|file| Self { file })
            .map_err(|error| FsError::from_io("raw_create_new", path.as_path(), error))
    }

    pub fn read_into(&mut self, buffer: &mut [u8]) -> FsResult<usize> {
        self.file
            .read(buffer)
            .map_err(|error| FsError::from_io("raw_read_into", "", error))
    }

    pub fn write(&mut self, bytes: &[u8]) -> FsResult<usize> {
        self.file
            .write(bytes)
            .map_err(|error| FsError::from_io("raw_write", "", error))
    }
}
