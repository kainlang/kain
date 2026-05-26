//! First-class filesystem primitives for Kain.
//!
//! Roc's tiny `fs` crate is a useful reference for the lowest layer: keep native
//! path/handle concerns explicit and avoid accidental allocation-heavy APIs in
//! hot paths. Kain's public filesystem surface is intentionally broader: typed
//! errors, deterministic directory output, byte/text helpers, temp paths,
//! atomic writes, globbing, hashing, and recursive operations.

mod capabilities;
mod error;
mod metadata;
mod ops;
mod path;
pub mod raw;
mod streaming;
mod transaction;
mod watch;

pub use capabilities::{FsCapability, FsMount, FsSandbox};
pub use error::{FsError, FsErrorKind, FsResult};
pub use metadata::{DirectoryEntry, FsFileType, FsMetadata};
pub use ops::{
    append_bytes, append_text, atomic_write_bytes, atomic_write_text, copy_file, copy_path,
    create_dir, create_dir_all, create_temp_dir, create_temp_file, exists, glob_paths, hash_file,
    is_dir, is_file, is_symlink, metadata, move_path, read_bytes, read_dir_entries, read_dir_paths,
    read_text, remove_dir, remove_dir_all, remove_file, remove_path, symlink_metadata,
    walk_dir_entries, write_bytes, write_text, WalkOptions,
};
pub use path::{
    absolute_path, canonicalize_path, normalize_path, path_extension, path_file_name, path_join,
    path_parent, path_stem,
};
pub use streaming::{
    copy_file_streaming, read_byte_range, read_text_range, stream_file_chunks, write_bytes_at,
    write_text_at, FsChunk,
};
pub use transaction::{transaction_missing, FsJournalEntry, FsTransaction, FsTransactionOp};
pub use watch::{FsWatchEvent, FsWatchEventKind, FsWatchSnapshot, FsWatcher};
