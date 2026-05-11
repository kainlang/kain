use std::collections::{BTreeMap, BTreeSet};
use std::path::{Path, PathBuf};

use crate::{
    metadata, read_dir_entries, walk_dir_entries, FsFileType, FsMetadata, FsResult, WalkOptions,
};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FsWatchSnapshot {
    pub path: PathBuf,
    pub file_type: FsFileType,
    pub len: u64,
    pub modified_millis: Option<u128>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum FsWatchEventKind {
    Created,
    Modified,
    Deleted,
}

impl FsWatchEventKind {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Created => "created",
            Self::Modified => "modified",
            Self::Deleted => "deleted",
        }
    }
}

#[derive(Debug, Clone)]
pub struct FsWatchEvent {
    pub kind: FsWatchEventKind,
    pub path: PathBuf,
    pub before: Option<FsWatchSnapshot>,
    pub after: Option<FsWatchSnapshot>,
}

#[derive(Debug, Clone)]
pub struct FsWatcher {
    root: PathBuf,
    recursive: bool,
    snapshot: BTreeMap<PathBuf, FsWatchSnapshot>,
}

impl FsWatcher {
    pub fn new(root: impl Into<PathBuf>, recursive: bool) -> FsResult<Self> {
        let root = root.into();
        let snapshot = snapshot_root(&root, recursive)?;
        Ok(Self {
            root,
            recursive,
            snapshot,
        })
    }

    pub fn root(&self) -> &Path {
        &self.root
    }

    pub fn poll(&mut self) -> FsResult<Vec<FsWatchEvent>> {
        let next = snapshot_root(&self.root, self.recursive)?;
        let mut events = Vec::new();
        let previous_keys = self.snapshot.keys().cloned().collect::<BTreeSet<_>>();
        let next_keys = next.keys().cloned().collect::<BTreeSet<_>>();

        for path in previous_keys.difference(&next_keys) {
            events.push(FsWatchEvent {
                kind: FsWatchEventKind::Deleted,
                path: path.clone(),
                before: self.snapshot.get(path).cloned(),
                after: None,
            });
        }

        for path in next_keys.difference(&previous_keys) {
            events.push(FsWatchEvent {
                kind: FsWatchEventKind::Created,
                path: path.clone(),
                before: None,
                after: next.get(path).cloned(),
            });
        }

        for path in previous_keys.intersection(&next_keys) {
            let before = self.snapshot.get(path).expect("previous key");
            let after = next.get(path).expect("next key");
            if before != after {
                events.push(FsWatchEvent {
                    kind: FsWatchEventKind::Modified,
                    path: path.clone(),
                    before: Some(before.clone()),
                    after: Some(after.clone()),
                });
            }
        }

        self.snapshot = next;
        events.sort_by(|left, right| {
            left.path
                .cmp(&right.path)
                .then(left.kind.as_str().cmp(right.kind.as_str()))
        });
        Ok(events)
    }
}

fn snapshot_root(root: &Path, recursive: bool) -> FsResult<BTreeMap<PathBuf, FsWatchSnapshot>> {
    let mut snapshots = BTreeMap::new();
    if root.is_file() {
        let metadata = metadata(root)?;
        snapshots.insert(root.to_path_buf(), snapshot(root, metadata));
        return Ok(snapshots);
    }

    let entries = if recursive {
        walk_dir_entries(root, WalkOptions::default())?
    } else {
        read_dir_entries(root)?
    };
    for entry in entries {
        snapshots.insert(entry.path.clone(), snapshot(&entry.path, entry.metadata));
    }
    Ok(snapshots)
}

fn snapshot(path: &Path, metadata: FsMetadata) -> FsWatchSnapshot {
    FsWatchSnapshot {
        path: path.to_path_buf(),
        file_type: metadata.file_type,
        len: metadata.len,
        modified_millis: metadata.modified_millis,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{append_text, write_text};

    #[test]
    fn polling_watcher_reports_create_modify_delete() {
        let temp = tempfile::tempdir().expect("tempdir");
        let mut watcher = FsWatcher::new(temp.path(), false).expect("watcher");
        let path = temp.path().join("watched.txt");
        write_text(&path, "one").expect("write");
        let created = watcher.poll().expect("created");
        assert_eq!(created.len(), 1);
        assert_eq!(created[0].kind, FsWatchEventKind::Created);

        append_text(&path, "two").expect("modify");
        let modified = watcher.poll().expect("modified");
        assert_eq!(modified.len(), 1);
        assert_eq!(modified[0].kind, FsWatchEventKind::Modified);

        std::fs::remove_file(&path).expect("remove");
        let deleted = watcher.poll().expect("deleted");
        assert_eq!(deleted.len(), 1);
        assert_eq!(deleted[0].kind, FsWatchEventKind::Deleted);
    }
}
