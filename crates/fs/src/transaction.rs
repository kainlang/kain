use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

use crate::{
    append_text, copy_path, create_temp_dir, move_path, remove_path, write_text, FsError,
    FsErrorKind, FsResult,
};

#[derive(Debug, Clone)]
pub enum FsTransactionOp {
    WriteText {
        path: PathBuf,
        content: String,
    },
    AppendText {
        path: PathBuf,
        content: String,
    },
    RemovePath {
        path: PathBuf,
    },
    CopyPath {
        source: PathBuf,
        destination: PathBuf,
    },
    MovePath {
        source: PathBuf,
        destination: PathBuf,
    },
}

#[derive(Debug, Clone)]
pub struct FsJournalEntry {
    pub operation: String,
    pub path: PathBuf,
    pub other_path: Option<PathBuf>,
    pub status: String,
    pub message: String,
}

#[derive(Debug, Clone, Default)]
pub struct FsTransaction {
    operations: Vec<FsTransactionOp>,
    journal: Vec<FsJournalEntry>,
}

impl FsTransaction {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn operations(&self) -> &[FsTransactionOp] {
        &self.operations
    }

    pub fn journal(&self) -> &[FsJournalEntry] {
        &self.journal
    }

    pub fn write_text(&mut self, path: impl Into<PathBuf>, content: impl Into<String>) {
        self.operations.push(FsTransactionOp::WriteText {
            path: path.into(),
            content: content.into(),
        });
    }

    pub fn append_text(&mut self, path: impl Into<PathBuf>, content: impl Into<String>) {
        self.operations.push(FsTransactionOp::AppendText {
            path: path.into(),
            content: content.into(),
        });
    }

    pub fn remove_path(&mut self, path: impl Into<PathBuf>) {
        self.operations
            .push(FsTransactionOp::RemovePath { path: path.into() });
    }

    pub fn copy_path(&mut self, source: impl Into<PathBuf>, destination: impl Into<PathBuf>) {
        self.operations.push(FsTransactionOp::CopyPath {
            source: source.into(),
            destination: destination.into(),
        });
    }

    pub fn move_path(&mut self, source: impl Into<PathBuf>, destination: impl Into<PathBuf>) {
        self.operations.push(FsTransactionOp::MovePath {
            source: source.into(),
            destination: destination.into(),
        });
    }

    pub fn commit(&mut self) -> FsResult<Vec<FsJournalEntry>> {
        let backup_root = PathBuf::from(create_temp_dir("kain-fs-tx")?);
        let mut backups = BTreeMap::<PathBuf, Option<PathBuf>>::new();
        let operations = self.operations.clone();

        for operation in &operations {
            for path in touched_paths(operation) {
                if !backups.contains_key(&path) {
                    backups.insert(path.clone(), backup_path(&backup_root, &path)?);
                }
            }
        }

        for operation in &operations {
            match apply_operation(operation) {
                Ok(()) => self.journal.push(journal_entry(operation, "ok", "")),
                Err(error) => {
                    self.journal
                        .push(journal_entry(operation, "error", error.to_string()));
                    rollback(&backups);
                    let _ = remove_path(&backup_root);
                    return Err(error);
                }
            }
        }

        let _ = remove_path(&backup_root);
        Ok(self.journal.clone())
    }

    pub fn rollback_only(&mut self) -> Vec<FsJournalEntry> {
        self.operations.clear();
        self.journal.push(FsJournalEntry {
            operation: "rollback".to_string(),
            path: PathBuf::new(),
            other_path: None,
            status: "ok".to_string(),
            message: "cleared pending transaction operations".to_string(),
        });
        self.journal.clone()
    }
}

fn touched_paths(operation: &FsTransactionOp) -> Vec<PathBuf> {
    match operation {
        FsTransactionOp::WriteText { path, .. }
        | FsTransactionOp::AppendText { path, .. }
        | FsTransactionOp::RemovePath { path } => vec![path.clone()],
        FsTransactionOp::CopyPath { destination, .. } => vec![destination.clone()],
        FsTransactionOp::MovePath {
            source,
            destination,
        } => vec![source.clone(), destination.clone()],
    }
}

fn backup_path(backup_root: &Path, path: &Path) -> FsResult<Option<PathBuf>> {
    if !path.exists() {
        return Ok(None);
    }
    let backup_name = format!(
        "backup-{}",
        path.to_string_lossy()
            .chars()
            .map(|ch| if ch.is_ascii_alphanumeric() { ch } else { '_' })
            .collect::<String>()
    );
    let backup = backup_root.join(backup_name);
    copy_path(path, &backup)?;
    Ok(Some(backup))
}

fn apply_operation(operation: &FsTransactionOp) -> FsResult<()> {
    match operation {
        FsTransactionOp::WriteText { path, content } => write_text(path, content),
        FsTransactionOp::AppendText { path, content } => append_text(path, content),
        FsTransactionOp::RemovePath { path } => remove_path(path),
        FsTransactionOp::CopyPath {
            source,
            destination,
        } => copy_path(source, destination),
        FsTransactionOp::MovePath {
            source,
            destination,
        } => move_path(source, destination),
    }
}

fn rollback(backups: &BTreeMap<PathBuf, Option<PathBuf>>) {
    for (path, backup) in backups.iter().rev() {
        let _ = remove_path(path);
        if let Some(backup) = backup {
            let _ = copy_path(backup, path);
        }
    }
}

fn journal_entry(
    operation: &FsTransactionOp,
    status: impl Into<String>,
    message: impl Into<String>,
) -> FsJournalEntry {
    match operation {
        FsTransactionOp::WriteText { path, .. } => FsJournalEntry {
            operation: "write_text".to_string(),
            path: path.clone(),
            other_path: None,
            status: status.into(),
            message: message.into(),
        },
        FsTransactionOp::AppendText { path, .. } => FsJournalEntry {
            operation: "append_text".to_string(),
            path: path.clone(),
            other_path: None,
            status: status.into(),
            message: message.into(),
        },
        FsTransactionOp::RemovePath { path } => FsJournalEntry {
            operation: "remove_path".to_string(),
            path: path.clone(),
            other_path: None,
            status: status.into(),
            message: message.into(),
        },
        FsTransactionOp::CopyPath {
            source,
            destination,
        } => FsJournalEntry {
            operation: "copy_path".to_string(),
            path: source.clone(),
            other_path: Some(destination.clone()),
            status: status.into(),
            message: message.into(),
        },
        FsTransactionOp::MovePath {
            source,
            destination,
        } => FsJournalEntry {
            operation: "move_path".to_string(),
            path: source.clone(),
            other_path: Some(destination.clone()),
            status: status.into(),
            message: message.into(),
        },
    }
}

pub fn transaction_missing(id: i64) -> FsError {
    FsError::new(
        "fs_transaction",
        id.to_string(),
        FsErrorKind::NotFound,
        format!("filesystem transaction {id} does not exist"),
    )
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::read_text;

    #[test]
    fn transaction_commits_text_operations() {
        let temp = tempfile::tempdir().expect("tempdir");
        let file = temp.path().join("a.txt");
        let mut transaction = FsTransaction::new();
        transaction.write_text(&file, "one");
        transaction.append_text(&file, " two");
        let journal = transaction.commit().expect("commit");
        assert_eq!(journal.len(), 2);
        assert_eq!(read_text(&file).expect("read"), "one two");
    }

    #[test]
    fn transaction_rolls_back_on_failure() {
        let temp = tempfile::tempdir().expect("tempdir");
        let file = temp.path().join("a.txt");
        let missing = temp.path().join("missing").join("source.txt");
        write_text(&file, "before").expect("before");

        let mut transaction = FsTransaction::new();
        transaction.write_text(&file, "after");
        transaction.copy_path(&missing, temp.path().join("dest.txt"));
        assert!(transaction.commit().is_err());
        assert_eq!(read_text(&file).expect("rolled back"), "before");
    }
}
