use std::fs::{self, OpenOptions};
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

use sha2::{Digest, Sha256};

use crate::{DirectoryEntry, FsError, FsErrorKind, FsFileType, FsMetadata, FsResult};

#[derive(Debug, Clone)]
pub struct WalkOptions {
    pub max_depth: Option<usize>,
    pub include_files: bool,
    pub include_dirs: bool,
    pub follow_symlinks: bool,
}

impl Default for WalkOptions {
    fn default() -> Self {
        Self {
            max_depth: None,
            include_files: true,
            include_dirs: true,
            follow_symlinks: false,
        }
    }
}

pub fn read_text(path: impl AsRef<Path>) -> FsResult<String> {
    fs::read_to_string(path.as_ref()).map_err(|error| FsError::from_io("read_text", path, error))
}

pub fn write_text(path: impl AsRef<Path>, content: &str) -> FsResult<()> {
    ensure_parent(path.as_ref())?;
    fs::write(path.as_ref(), content).map_err(|error| FsError::from_io("write_text", path, error))
}

pub fn append_text(path: impl AsRef<Path>, content: &str) -> FsResult<()> {
    ensure_parent(path.as_ref())?;
    let mut file = OpenOptions::new()
        .append(true)
        .create(true)
        .open(path.as_ref())
        .map_err(|error| FsError::from_io("append_text", path.as_ref(), error))?;
    file.write_all(content.as_bytes())
        .map_err(|error| FsError::from_io("append_text", path, error))
}

pub fn read_bytes(path: impl AsRef<Path>) -> FsResult<Vec<u8>> {
    fs::read(path.as_ref()).map_err(|error| FsError::from_io("read_bytes", path, error))
}

pub fn write_bytes(path: impl AsRef<Path>, bytes: &[u8]) -> FsResult<()> {
    ensure_parent(path.as_ref())?;
    fs::write(path.as_ref(), bytes).map_err(|error| FsError::from_io("write_bytes", path, error))
}

pub fn append_bytes(path: impl AsRef<Path>, bytes: &[u8]) -> FsResult<()> {
    ensure_parent(path.as_ref())?;
    let mut file = OpenOptions::new()
        .append(true)
        .create(true)
        .open(path.as_ref())
        .map_err(|error| FsError::from_io("append_bytes", path.as_ref(), error))?;
    file.write_all(bytes)
        .map_err(|error| FsError::from_io("append_bytes", path, error))
}

pub fn exists(path: impl AsRef<Path>) -> bool {
    path.as_ref().exists()
}

pub fn is_file(path: impl AsRef<Path>) -> bool {
    path.as_ref().is_file()
}

pub fn is_dir(path: impl AsRef<Path>) -> bool {
    path.as_ref().is_dir()
}

pub fn is_symlink(path: impl AsRef<Path>) -> bool {
    fs::symlink_metadata(path.as_ref())
        .map(|metadata| metadata.file_type().is_symlink())
        .unwrap_or(false)
}

pub fn metadata(path: impl AsRef<Path>) -> FsResult<FsMetadata> {
    fs::metadata(path.as_ref())
        .map(FsMetadata::from_std)
        .map_err(|error| FsError::from_io("metadata", path, error))
}

pub fn symlink_metadata(path: impl AsRef<Path>) -> FsResult<FsMetadata> {
    fs::symlink_metadata(path.as_ref())
        .map(FsMetadata::from_std)
        .map_err(|error| FsError::from_io("symlink_metadata", path, error))
}

pub fn create_dir(path: impl AsRef<Path>) -> FsResult<()> {
    fs::create_dir(path.as_ref()).map_err(|error| FsError::from_io("create_dir", path, error))
}

pub fn create_dir_all(path: impl AsRef<Path>) -> FsResult<()> {
    fs::create_dir_all(path.as_ref())
        .map_err(|error| FsError::from_io("create_dir_all", path, error))
}

pub fn remove_file(path: impl AsRef<Path>) -> FsResult<()> {
    fs::remove_file(path.as_ref()).map_err(|error| FsError::from_io("remove_file", path, error))
}

pub fn remove_dir(path: impl AsRef<Path>) -> FsResult<()> {
    fs::remove_dir(path.as_ref()).map_err(|error| FsError::from_io("remove_dir", path, error))
}

pub fn remove_dir_all(path: impl AsRef<Path>) -> FsResult<()> {
    fs::remove_dir_all(path.as_ref())
        .map_err(|error| FsError::from_io("remove_dir_all", path, error))
}

pub fn remove_path(path: impl AsRef<Path>) -> FsResult<()> {
    let path = path.as_ref();
    if !path.exists() {
        return Ok(());
    }
    if path.is_dir() {
        remove_dir_all(path)
    } else {
        remove_file(path)
    }
}

pub fn copy_file(source: impl AsRef<Path>, destination: impl AsRef<Path>) -> FsResult<u64> {
    ensure_parent(destination.as_ref())?;
    fs::copy(source.as_ref(), destination.as_ref())
        .map_err(|error| FsError::from_two_path_io("copy_file", source, destination, error))
}

pub fn copy_path(source: impl AsRef<Path>, destination: impl AsRef<Path>) -> FsResult<()> {
    let source = source.as_ref();
    let destination = destination.as_ref();
    if source.is_dir() {
        copy_dir_recursive(source, destination)
    } else {
        copy_file(source, destination).map(|_| ())
    }
}

pub fn move_path(source: impl AsRef<Path>, destination: impl AsRef<Path>) -> FsResult<()> {
    ensure_parent(destination.as_ref())?;
    fs::rename(source.as_ref(), destination.as_ref())
        .map_err(|error| FsError::from_two_path_io("move_path", source, destination, error))
}

pub fn read_dir_paths(path: impl AsRef<Path>) -> FsResult<Vec<String>> {
    let mut paths = Vec::new();
    for entry in fs::read_dir(path.as_ref())
        .map_err(|error| FsError::from_io("read_dir_paths", path.as_ref(), error))?
    {
        let entry =
            entry.map_err(|error| FsError::from_io("read_dir_paths", path.as_ref(), error))?;
        paths.push(entry.path().to_string_lossy().into_owned());
    }
    paths.sort();
    Ok(paths)
}

pub fn read_dir_entries(path: impl AsRef<Path>) -> FsResult<Vec<DirectoryEntry>> {
    let mut entries = Vec::new();
    for entry in fs::read_dir(path.as_ref())
        .map_err(|error| FsError::from_io("read_dir_entries", path.as_ref(), error))?
    {
        let entry =
            entry.map_err(|error| FsError::from_io("read_dir_entries", path.as_ref(), error))?;
        entries.push(DirectoryEntry::from_std("read_dir_entries", entry)?);
    }
    entries.sort_by(|left, right| left.path.cmp(&right.path));
    Ok(entries)
}

pub fn walk_dir_entries(
    path: impl AsRef<Path>,
    options: WalkOptions,
) -> FsResult<Vec<DirectoryEntry>> {
    let mut entries = Vec::new();
    walk_inner(path.as_ref(), &options, 0, &mut entries)?;
    entries.sort_by(|left, right| left.path.cmp(&right.path));
    Ok(entries)
}

pub fn glob_paths(pattern: &str) -> FsResult<Vec<String>> {
    let mut paths = Vec::new();
    let matches = glob::glob(pattern).map_err(|error| {
        FsError::new(
            "glob_paths",
            pattern,
            FsErrorKind::GlobPattern,
            error.to_string(),
        )
    })?;
    for entry in matches {
        match entry {
            Ok(path) => paths.push(path.to_string_lossy().into_owned()),
            Err(error) => {
                return Err(FsError::new(
                    "glob_paths",
                    error.path(),
                    FsErrorKind::GlobRead,
                    error.to_string(),
                ))
            }
        }
    }
    paths.sort();
    Ok(paths)
}

pub fn atomic_write_text(path: impl AsRef<Path>, content: &str) -> FsResult<()> {
    atomic_write_bytes(path, content.as_bytes())
}

pub fn atomic_write_bytes(path: impl AsRef<Path>, bytes: &[u8]) -> FsResult<()> {
    let path = path.as_ref();
    ensure_parent(path)?;
    let temp_path = sibling_temp_path(path);
    write_bytes(&temp_path, bytes)?;
    replace_with_temp(path, &temp_path)
}

pub fn create_temp_file(prefix: &str) -> FsResult<String> {
    let temp_dir = std::env::temp_dir();
    for attempt in 0..128 {
        let path = temp_dir.join(unique_name(prefix, attempt));
        match OpenOptions::new().write(true).create_new(true).open(&path) {
            Ok(_) => return Ok(path.to_string_lossy().into_owned()),
            Err(error) if error.kind() == std::io::ErrorKind::AlreadyExists => continue,
            Err(error) => return Err(FsError::from_io("create_temp_file", path, error)),
        }
    }
    Err(FsError::new(
        "create_temp_file",
        temp_dir,
        FsErrorKind::AlreadyExists,
        "failed to allocate a unique temporary file after 128 attempts",
    ))
}

pub fn create_temp_dir(prefix: &str) -> FsResult<String> {
    let temp_dir = std::env::temp_dir();
    for attempt in 0..128 {
        let path = temp_dir.join(unique_name(prefix, attempt));
        match fs::create_dir(&path) {
            Ok(_) => return Ok(path.to_string_lossy().into_owned()),
            Err(error) if error.kind() == std::io::ErrorKind::AlreadyExists => continue,
            Err(error) => return Err(FsError::from_io("create_temp_dir", path, error)),
        }
    }
    Err(FsError::new(
        "create_temp_dir",
        temp_dir,
        FsErrorKind::AlreadyExists,
        "failed to allocate a unique temporary directory after 128 attempts",
    ))
}

pub fn hash_file(path: impl AsRef<Path>) -> FsResult<String> {
    let mut file = fs::File::open(path.as_ref())
        .map_err(|error| FsError::from_io("hash_file", path.as_ref(), error))?;
    let mut hasher = Sha256::new();
    let mut buffer = [0_u8; 64 * 1024];
    loop {
        let read = file
            .read(&mut buffer)
            .map_err(|error| FsError::from_io("hash_file", path.as_ref(), error))?;
        if read == 0 {
            break;
        }
        hasher.update(&buffer[..read]);
    }
    Ok(format!("{:x}", hasher.finalize()))
}

fn ensure_parent(path: &Path) -> FsResult<()> {
    if let Some(parent) = path.parent() {
        if !parent.as_os_str().is_empty() {
            fs::create_dir_all(parent)
                .map_err(|error| FsError::from_io("ensure_parent", parent, error))?;
        }
    }
    Ok(())
}

fn copy_dir_recursive(source: &Path, destination: &Path) -> FsResult<()> {
    create_dir_all(destination)?;
    for entry in read_dir_entries(source)? {
        let child_source = entry.path;
        let child_destination = destination.join(entry.file_name);
        match entry.file_type {
            FsFileType::Directory => copy_dir_recursive(&child_source, &child_destination)?,
            FsFileType::File | FsFileType::Symlink | FsFileType::Other => {
                copy_file(&child_source, &child_destination)?;
            }
        }
    }
    Ok(())
}

fn walk_inner(
    path: &Path,
    options: &WalkOptions,
    depth: usize,
    entries: &mut Vec<DirectoryEntry>,
) -> FsResult<()> {
    if options.max_depth.is_some_and(|max_depth| depth > max_depth) {
        return Ok(());
    }
    let children = read_dir_entries(path)?;
    for entry in children {
        let should_include = match entry.file_type {
            FsFileType::Directory => options.include_dirs,
            FsFileType::File | FsFileType::Symlink | FsFileType::Other => options.include_files,
        };
        let should_descend = entry.file_type == FsFileType::Directory
            || (options.follow_symlinks
                && entry.file_type == FsFileType::Symlink
                && entry.path.is_dir());
        let child_path = entry.path.clone();
        if should_include {
            entries.push(entry);
        }
        if should_descend {
            walk_inner(&child_path, options, depth + 1, entries)?;
        }
    }
    Ok(())
}

fn sibling_temp_path(path: &Path) -> PathBuf {
    let parent = path.parent().unwrap_or_else(|| Path::new("."));
    let file_name = path
        .file_name()
        .map(|value| value.to_string_lossy().into_owned())
        .unwrap_or_else(|| "atomic".to_string());
    parent.join(format!(".{file_name}.{}.tmp", unique_suffix(0)))
}

fn replace_with_temp(path: &Path, temp_path: &Path) -> FsResult<()> {
    match fs::rename(temp_path, path) {
        Ok(_) => Ok(()),
        Err(error) if error.kind() == std::io::ErrorKind::AlreadyExists => {
            fs::remove_file(path).map_err(|remove_error| {
                FsError::from_io("atomic_write_remove_existing", path, remove_error)
            })?;
            fs::rename(temp_path, path)
                .map_err(|rename_error| FsError::from_io("atomic_write_rename", path, rename_error))
        }
        Err(error) => Err(FsError::from_io("atomic_write_rename", path, error)),
    }
}

fn unique_name(prefix: &str, attempt: usize) -> String {
    let clean_prefix = if prefix.trim().is_empty() {
        "kain"
    } else {
        prefix
    };
    format!("{}-{}", clean_prefix, unique_suffix(attempt))
}

fn unique_suffix(attempt: usize) -> String {
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_nanos())
        .unwrap_or_default();
    format!("{}-{}-{}", std::process::id(), nanos, attempt)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn text_bytes_metadata_and_hash_round_trip() {
        let temp = tempfile::tempdir().expect("tempdir");
        let path = temp.path().join("nested").join("hello.txt");

        write_text(&path, "hello").expect("write");
        append_text(&path, " world").expect("append");

        assert_eq!(read_text(&path).expect("read"), "hello world");
        assert!(exists(&path));
        assert!(is_file(&path));
        assert!(!is_dir(&path));

        let meta = metadata(&path).expect("metadata");
        assert_eq!(meta.file_type, FsFileType::File);
        assert_eq!(meta.len, 11);
        assert_eq!(
            hash_file(&path).expect("hash"),
            "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9"
        );

        let bytes = temp.path().join("bytes.bin");
        write_bytes(&bytes, &[1, 2, 3]).expect("write bytes");
        append_bytes(&bytes, &[4, 5]).expect("append bytes");
        assert_eq!(read_bytes(&bytes).expect("read bytes"), vec![1, 2, 3, 4, 5]);
    }

    #[test]
    fn directory_walk_copy_remove_and_glob_are_deterministic() {
        let temp = tempfile::tempdir().expect("tempdir");
        let source = temp.path().join("source");
        write_text(source.join("b.txt"), "b").expect("b");
        write_text(source.join("nested").join("a.txt"), "a").expect("a");

        let paths = read_dir_paths(&source).expect("read_dir");
        assert_eq!(paths.len(), 2);
        assert!(paths[0].ends_with("b.txt") || paths[0].ends_with("nested"));

        let walked = walk_dir_entries(&source, WalkOptions::default()).expect("walk");
        let names = walked
            .iter()
            .map(|entry| entry.file_name.as_str())
            .collect::<Vec<_>>();
        assert!(names.contains(&"a.txt"));
        assert!(names.contains(&"b.txt"));

        let pattern = source
            .join("**")
            .join("*.txt")
            .to_string_lossy()
            .into_owned();
        let globbed = glob_paths(&pattern).expect("glob");
        assert_eq!(globbed.len(), 2);
        assert!(globbed[0] <= globbed[1]);

        let destination = temp.path().join("destination");
        copy_path(&source, &destination).expect("copy path");
        assert_eq!(
            read_text(destination.join("nested").join("a.txt")).expect("copied text"),
            "a"
        );
        remove_path(&destination).expect("remove path");
        assert!(!destination.exists());
    }

    #[test]
    fn temp_and_atomic_writes_create_real_paths() {
        let temp_file = create_temp_file("kain-fs-test").expect("temp file");
        assert!(Path::new(&temp_file).is_file());
        remove_file(&temp_file).expect("remove temp file");

        let temp_dir = create_temp_dir("kain-fs-test").expect("temp dir");
        assert!(Path::new(&temp_dir).is_dir());
        remove_dir(&temp_dir).expect("remove temp dir");

        let temp = tempfile::tempdir().expect("tempdir");
        let path = temp.path().join("atomic.txt");
        atomic_write_text(&path, "one").expect("atomic one");
        atomic_write_text(&path, "two").expect("atomic two");
        assert_eq!(read_text(&path).expect("atomic read"), "two");
    }

    #[test]
    fn missing_file_returns_typed_error() {
        let temp = tempfile::tempdir().expect("tempdir");
        let error = read_text(temp.path().join("missing.txt")).expect_err("missing");
        assert_eq!(error.kind, FsErrorKind::NotFound);
        assert_eq!(error.operation, "read_text");
    }
}
