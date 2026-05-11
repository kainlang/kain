use std::env;
use std::path::{Component, Path, PathBuf};

use crate::{FsError, FsResult};

pub fn path_join(base: impl AsRef<Path>, child: impl AsRef<Path>) -> String {
    base.as_ref()
        .join(child.as_ref())
        .to_string_lossy()
        .into_owned()
}

pub fn path_parent(path: impl AsRef<Path>) -> String {
    path.as_ref()
        .parent()
        .map(|value| value.to_string_lossy().into_owned())
        .unwrap_or_default()
}

pub fn path_file_name(path: impl AsRef<Path>) -> String {
    path.as_ref()
        .file_name()
        .map(|value| value.to_string_lossy().into_owned())
        .unwrap_or_default()
}

pub fn path_extension(path: impl AsRef<Path>) -> String {
    path.as_ref()
        .extension()
        .map(|value| value.to_string_lossy().into_owned())
        .unwrap_or_default()
}

pub fn path_stem(path: impl AsRef<Path>) -> String {
    path.as_ref()
        .file_stem()
        .map(|value| value.to_string_lossy().into_owned())
        .unwrap_or_default()
}

pub fn canonicalize_path(path: impl AsRef<Path>) -> FsResult<String> {
    std::fs::canonicalize(path.as_ref())
        .map(|value| value.to_string_lossy().into_owned())
        .map_err(|error| FsError::from_io("canonicalize_path", path.as_ref(), error))
}

pub fn absolute_path(path: impl AsRef<Path>) -> FsResult<String> {
    let path = path.as_ref();
    let absolute = if path.is_absolute() {
        path.to_path_buf()
    } else {
        env::current_dir()
            .map_err(|error| FsError::from_io("absolute_path", path, error))?
            .join(path)
    };
    Ok(normalize_path(absolute))
}

pub fn normalize_path(path: impl AsRef<Path>) -> String {
    normalize_path_buf(path.as_ref())
        .to_string_lossy()
        .into_owned()
}

fn normalize_path_buf(path: &Path) -> PathBuf {
    let mut normalized = PathBuf::new();
    for component in path.components() {
        match component {
            Component::Prefix(prefix) => normalized.push(prefix.as_os_str()),
            Component::RootDir => normalized.push(component.as_os_str()),
            Component::CurDir => {}
            Component::ParentDir => {
                normalized.pop();
            }
            Component::Normal(segment) => normalized.push(segment),
        }
    }
    if normalized.as_os_str().is_empty() {
        PathBuf::from(".")
    } else {
        normalized
    }
}
