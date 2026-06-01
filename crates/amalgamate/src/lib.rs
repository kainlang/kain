use std::collections::{BTreeMap, BTreeSet};
use std::fmt;
use std::fs;
use std::io::Cursor;
use std::path::{Component, Path, PathBuf};

use base64::engine::general_purpose::STANDARD as BASE64_STANDARD;
use base64::Engine as _;
use blade::{KainBuildTaskSection, KainManifest};
use chrono::Utc;
use kain_fs as kfs;
use kain_fs::{FsFileType, WalkOptions};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};

pub const CAPSULE_SCHEMA_VERSION_V1: u32 = 1;
pub const CAPSULE_SCHEMA_VERSION: u32 = 2;
pub const CAPSULE_SENTINEL_START: &str = "//!kain-capsule";
pub const CAPSULE_SENTINEL_END: &str = "//!end-kain-capsule";
pub const CAPSULE_PAYLOAD_START: &str = "//!kain-capsule-payload";
pub const CAPSULE_PAYLOAD_END: &str = "//!end-kain-capsule-payload";
pub const CAPSULE_FILE_START: &str = "//!kain-file";
pub const CAPSULE_FILE_CONTENT_START: &str = "//!kain-file-content";
pub const CAPSULE_FILE_END: &str = "//!end-kain-file";
pub const CAPSULE_PAYLOAD_FORMAT: &str = "kain-capsule-archive-json-v1";
pub const CAPSULE_PAYLOAD_ENCODING: &str = "base64";
pub const CAPSULE_TEXT_ENCODING: &str = "utf8";
pub const DEFAULT_PREVIEW_SYMBOL_LIMIT: usize = 40;

pub type CapsuleResult<T> = Result<T, CapsuleError>;

#[derive(Debug, thiserror::Error)]
pub enum CapsuleError {
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
    #[error("filesystem error: {0}")]
    Fs(#[from] kfs::FsError),
    #[error("blade error: {0}")]
    Blade(#[from] blade::BladeError),
    #[error("JSON error: {0}")]
    Json(#[from] serde_json::Error),
    #[error("TOML decode error: {0}")]
    TomlDecode(#[from] toml::de::Error),
    #[error("TOML encode error: {0}")]
    TomlEncode(#[from] toml::ser::Error),
    #[error("base64 error: {0}")]
    Base64(#[from] base64::DecodeError),
    #[error("{0}")]
    Format(String),
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum CapsuleKind {
    Entry,
    Blade,
    Workspace,
    Directory,
}

impl CapsuleKind {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Entry => "entry",
            Self::Blade => "blade",
            Self::Workspace => "workspace",
            Self::Directory => "directory",
        }
    }
}

impl fmt::Display for CapsuleKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum CapsuleCompression {
    Zstd,
    None,
}

impl CapsuleCompression {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Zstd => "zstd",
            Self::None => "none",
        }
    }
}

impl fmt::Display for CapsuleCompression {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

fn default_capsule_compression() -> CapsuleCompression {
    CapsuleCompression::None
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum CapsuleStorage {
    Editable,
    Archive,
}

impl CapsuleStorage {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Editable => "editable",
            Self::Archive => "archive",
        }
    }
}

impl Default for CapsuleStorage {
    fn default() -> Self {
        Self::Archive
    }
}

impl fmt::Display for CapsuleStorage {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CapsuleHeaderStyle {
    Minimal,
    Rich,
    Off,
}

impl CapsuleHeaderStyle {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Minimal => "minimal",
            Self::Rich => "rich",
            Self::Off => "off",
        }
    }
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum CapsuleIndexMode {
    Auto,
    Off,
}

impl Default for CapsuleIndexMode {
    fn default() -> Self {
        Self::Auto
    }
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
#[serde(rename_all = "lowercase")]
pub enum CapsuleContents {
    Source,
    Snapshot,
    Assets,
    Artifacts,
    Evidence,
}

impl CapsuleContents {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Source => "source",
            Self::Snapshot => "snapshot",
            Self::Assets => "assets",
            Self::Artifacts => "artifacts",
            Self::Evidence => "evidence",
        }
    }

    fn materialize_priority(self) -> usize {
        match self {
            Self::Source => 0,
            Self::Snapshot => 1,
            Self::Assets => 2,
            Self::Artifacts => 3,
            Self::Evidence => 4,
        }
    }
}

impl Default for CapsuleContents {
    fn default() -> Self {
        Self::Snapshot
    }
}

impl fmt::Display for CapsuleContents {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.as_str())
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CapsuleMetadata {
    pub schema: u32,
    pub kind: CapsuleKind,
    #[serde(default)]
    pub storage: CapsuleStorage,
    #[serde(default)]
    pub contents: CapsuleContents,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub source_kind: Option<String>,
    #[serde(default = "default_capsule_compression")]
    pub compression: CapsuleCompression,
    pub digest: String,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub payload_sha256: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub payload_encoding: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub payload_format: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub payload_bytes: Option<u64>,
    pub created_at: String,
    pub file_count: usize,
    pub module_count: usize,
    pub preview_symbol_limit: usize,
    #[serde(default)]
    pub api_index: CapsuleIndexMode,
    #[serde(default)]
    pub module_index: CapsuleIndexMode,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub directories: Vec<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub capsule_set: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub version: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub root_label: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub entry: Option<String>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub manifest: Option<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub authors: Vec<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub notes: Vec<String>,
    #[serde(default, skip_serializing_if = "Vec::is_empty")]
    pub tags: Vec<String>,
    #[serde(default, skip_serializing_if = "BTreeMap::is_empty")]
    pub meta: BTreeMap<String, String>,
}

impl CapsuleMetadata {
    pub fn display_kind(&self) -> &str {
        self.source_kind
            .as_deref()
            .unwrap_or_else(|| self.kind.as_str())
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CapsuleArchive {
    pub schema: u32,
    pub format: String,
    pub root_label: String,
    #[serde(default)]
    pub directories: Vec<String>,
    #[serde(default)]
    pub files: Vec<CapsuleArchiveFile>,
    pub preview: CapsulePreview,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CapsuleArchiveFile {
    pub path: String,
    pub size_bytes: u64,
    pub sha256: String,
    pub bytes_base64: String,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct CapsulePreview {
    pub module_count: usize,
    pub total_symbols: usize,
    #[serde(default)]
    pub sections: Vec<CapsulePreviewSection>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CapsulePreviewSection {
    pub title: String,
    #[serde(default)]
    pub symbols: Vec<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CapsuleFileSummary {
    pub path: String,
    pub size_bytes: u64,
    pub sha256: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct InspectReport {
    pub metadata: CapsuleMetadata,
    pub preview: CapsulePreview,
    pub directories: Vec<String>,
    pub files: Vec<CapsuleFileSummary>,
}

#[derive(Debug, Clone)]
pub struct PackOptions {
    pub input: PathBuf,
    pub output: PathBuf,
    pub storage: CapsuleStorage,
    pub contents: CapsuleContents,
    pub name: Option<String>,
    pub capsule_set: Option<String>,
    pub version: Option<String>,
    pub authors: Vec<String>,
    pub notes: Vec<String>,
    pub tags: Vec<String>,
    pub meta: BTreeMap<String, String>,
    pub header_style: CapsuleHeaderStyle,
    pub preview_symbol_limit: usize,
    pub compression: CapsuleCompression,
    pub api_index: CapsuleIndexMode,
    pub module_index: CapsuleIndexMode,
}

impl PackOptions {
    pub fn new(input: impl Into<PathBuf>, output: impl Into<PathBuf>) -> Self {
        Self {
            input: input.into(),
            output: output.into(),
            storage: CapsuleStorage::Editable,
            contents: CapsuleContents::Source,
            name: None,
            capsule_set: None,
            version: None,
            authors: Vec::new(),
            notes: Vec::new(),
            tags: Vec::new(),
            meta: BTreeMap::new(),
            header_style: CapsuleHeaderStyle::Rich,
            preview_symbol_limit: DEFAULT_PREVIEW_SYMBOL_LIMIT,
            compression: CapsuleCompression::Zstd,
            api_index: CapsuleIndexMode::Auto,
            module_index: CapsuleIndexMode::Auto,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PackReport {
    pub output_path: PathBuf,
    pub kind: CapsuleKind,
    pub name: String,
    pub digest: String,
    pub file_count: usize,
    pub module_count: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UnpackReport {
    pub output_root: PathBuf,
    pub file_count: usize,
}

#[derive(Debug, Clone)]
pub struct MaterializedCapsule {
    pub metadata: CapsuleMetadata,
    pub cache_root: PathBuf,
    pub workspace_root: PathBuf,
    pub entry_path: Option<PathBuf>,
    pub manifest_path: Option<PathBuf>,
}

impl MaterializedCapsule {
    pub fn runnable_input(&self) -> CapsuleResult<PathBuf> {
        if self.manifest_path.is_some() {
            return Ok(self.workspace_root.clone());
        }
        if let Some(entry_path) = &self.entry_path {
            return Ok(entry_path.clone());
        }
        Err(CapsuleError::Format(
            "capsule does not expose an entry file or KAIN.toml anchor".to_string(),
        ))
    }
}

#[derive(Debug, Clone)]
struct SourceSnapshot {
    kind: CapsuleKind,
    source_kind: Option<String>,
    root_label: String,
    name: String,
    capsule_set: String,
    version: Option<String>,
    manifest_rel: Option<String>,
    entry_rel: Option<String>,
    directories: Vec<String>,
    files: Vec<SourceFile>,
    preview: CapsulePreview,
}

#[derive(Debug, Clone)]
struct DirectorySnapshot {
    root: PathBuf,
    kind: CapsuleKind,
    source_kind: Option<String>,
    root_label: String,
    name: String,
    capsule_set: String,
    version: Option<String>,
    manifest_rel: Option<String>,
    entry_rel: Option<String>,
    directories: Vec<String>,
    files: Vec<SourceFile>,
}

#[derive(Debug, Clone)]
struct SourceFile {
    rel_path: String,
    bytes: Vec<u8>,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
enum EditableFileKind {
    Text,
    Binary,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct EditableFileMetadata {
    path: String,
    kind: EditableFileKind,
    encoding: String,
    bytes: u64,
    sha256: String,
    #[serde(default)]
    trailing_newline: bool,
}

#[derive(Debug, Clone)]
struct ResolvedCapsuleArchive {
    root_label: String,
    directories: Vec<String>,
    files: Vec<SourceFile>,
    preview: CapsulePreview,
}

#[derive(Debug, Clone, Default)]
struct CapsuleSelection {
    source_files: BTreeSet<String>,
    source_dirs: BTreeSet<String>,
    artifact_files: BTreeSet<String>,
    artifact_dirs: BTreeSet<String>,
    evidence_files: BTreeSet<String>,
    evidence_dirs: BTreeSet<String>,
}

#[derive(Debug, Clone)]
struct CompanionCapsule {
    path: PathBuf,
    metadata: CapsuleMetadata,
    archive: ResolvedCapsuleArchive,
}

pub fn pack_capsule(options: &PackOptions) -> CapsuleResult<PackReport> {
    let snapshot = collect_source_snapshot(options)?;
    let archive = build_archive(&snapshot)?;
    let archive_bytes = serde_json::to_vec_pretty(&archive)?;
    let digest = format!("sha256:{}", hex_sha256(&archive_bytes));
    let payload_bytes = if options.storage == CapsuleStorage::Archive {
        Some(match options.compression {
            CapsuleCompression::Zstd => zstd::stream::encode_all(Cursor::new(&archive_bytes), 19)?,
            CapsuleCompression::None => archive_bytes.clone(),
        })
    } else {
        None
    };
    let metadata = CapsuleMetadata {
        schema: CAPSULE_SCHEMA_VERSION,
        kind: snapshot.kind,
        storage: options.storage,
        contents: options.contents,
        source_kind: snapshot.source_kind.clone(),
        compression: if options.storage == CapsuleStorage::Archive {
            options.compression
        } else {
            CapsuleCompression::None
        },
        digest: digest.clone(),
        payload_sha256: payload_bytes
            .as_ref()
            .map(|payload| format!("sha256:{}", hex_sha256(payload))),
        payload_encoding: payload_bytes
            .as_ref()
            .map(|_| CAPSULE_PAYLOAD_ENCODING.to_string()),
        payload_format: payload_bytes
            .as_ref()
            .map(|_| CAPSULE_PAYLOAD_FORMAT.to_string()),
        payload_bytes: payload_bytes.as_ref().map(|payload| payload.len() as u64),
        created_at: Utc::now().to_rfc3339(),
        file_count: snapshot.files.len(),
        module_count: snapshot.preview.module_count,
        preview_symbol_limit: options.preview_symbol_limit,
        api_index: options.api_index,
        module_index: options.module_index,
        directories: snapshot.directories.clone(),
        name: Some(snapshot.name.clone()),
        capsule_set: Some(snapshot.capsule_set.clone()),
        version: snapshot.version.clone(),
        root_label: Some(snapshot.root_label.clone()),
        entry: snapshot.entry_rel.clone(),
        manifest: snapshot.manifest_rel.clone(),
        authors: options.authors.clone(),
        notes: options.notes.clone(),
        tags: options.tags.clone(),
        meta: options.meta.clone(),
    };
    let rendered = render_capsule_text(
        &metadata,
        &snapshot,
        &archive,
        payload_bytes.as_deref(),
        options.header_style,
        options.preview_symbol_limit,
    )?;
    kfs::atomic_write_text(&options.output, &rendered)?;
    Ok(PackReport {
        output_path: options.output.clone(),
        kind: metadata.kind,
        name: snapshot.name,
        digest,
        file_count: metadata.file_count,
        module_count: metadata.module_count,
    })
}

pub fn inspect_capsule(path: &Path) -> CapsuleResult<InspectReport> {
    let (_, metadata, archive) = read_capsule(path)?;
    let files = archive
        .files
        .iter()
        .map(|file| CapsuleFileSummary {
            path: file.rel_path.clone(),
            size_bytes: file.bytes.len() as u64,
            sha256: format!("sha256:{}", hex_sha256(&file.bytes)),
        })
        .collect();
    Ok(InspectReport {
        metadata,
        preview: archive.preview,
        directories: archive.directories,
        files,
    })
}

pub fn unpack_capsule(path: &Path, output_root: &Path) -> CapsuleResult<UnpackReport> {
    let (_, _, archive, _) = read_capsule_with_companions(path)?;
    unpack_resolved_archive(&archive, output_root)?;
    Ok(UnpackReport {
        output_root: output_root.to_path_buf(),
        file_count: archive.files.len(),
    })
}

pub fn maybe_capsule_metadata(path: &Path) -> CapsuleResult<Option<CapsuleMetadata>> {
    let Some(text) = read_utf8_if_file(path)? else {
        return Ok(None);
    };
    if !text.contains(CAPSULE_SENTINEL_START) {
        return Ok(None);
    }
    let metadata = parse_capsule_metadata(&text)?;
    Ok(Some(metadata))
}

pub fn default_materialize_root(base: &Path) -> PathBuf {
    base.join(".kain").join("cache").join("amalgamate")
}

pub fn materialize_capsule(
    path: &Path,
    base_cache_root: &Path,
) -> CapsuleResult<MaterializedCapsule> {
    let (_, metadata, archive, companions) = read_capsule_with_companions(path)?;
    let state_json = materialization_state_json(path, &metadata, &companions)?;
    let cache_root = base_cache_root.join(hex_sha256(state_json.as_bytes()));
    let workspace_root = cache_root.join("workspace");
    let metadata_path = cache_root.join("metadata.json");
    let needs_refresh = match fs::read_to_string(&metadata_path) {
        Ok(existing) => existing != state_json,
        Err(_) => true,
    };
    if needs_refresh {
        if cache_root.exists() {
            kfs::remove_dir_all(&cache_root)?;
        }
        kfs::create_dir_all(&workspace_root)?;
        unpack_resolved_archive(&archive, &workspace_root)?;
        kfs::write_text(&metadata_path, &state_json)?;
    }
    let manifest_path = metadata
        .manifest
        .as_ref()
        .map(|path| workspace_root.join(capsule_rel_to_path(path)));
    let entry_path = metadata
        .entry
        .as_ref()
        .map(|path| workspace_root.join(capsule_rel_to_path(path)));
    Ok(MaterializedCapsule {
        metadata,
        cache_root,
        workspace_root,
        entry_path,
        manifest_path,
    })
}

fn collect_source_snapshot(options: &PackOptions) -> CapsuleResult<SourceSnapshot> {
    let input = options.input.as_path();
    if input.is_file() {
        let canonical = PathBuf::from(kfs::canonicalize_path(input)?);
        let rel_path = canonical
            .file_name()
            .ok_or_else(|| CapsuleError::Format("input file is missing a file name".to_string()))?
            .to_string_lossy()
            .into_owned();
        let bytes = fs::read(&canonical)?;
        let preview = build_preview(
            &[SourceFile {
                rel_path: rel_path.clone(),
                bytes: bytes.clone(),
            }],
            options.preview_symbol_limit,
            options.api_index,
            options.module_index,
        );
        let name = options
            .name
            .clone()
            .unwrap_or_else(|| file_stem_string(&canonical));
        let root_label = file_stem_string(&canonical);
        return Ok(SourceSnapshot {
            kind: CapsuleKind::Entry,
            source_kind: None,
            root_label: root_label.clone(),
            name,
            capsule_set: options.capsule_set.clone().unwrap_or(root_label),
            version: options.version.clone(),
            manifest_rel: None,
            entry_rel: Some(rel_path.clone()),
            directories: Vec::new(),
            files: vec![SourceFile { rel_path, bytes }],
            preview,
        });
    }

    let root = PathBuf::from(kfs::canonicalize_path(input)?);
    let manifest = blade::load_effective_kain_manifest(&root)?;
    let kind = determine_capsule_kind(&root, manifest.as_ref());
    let source_kind = manifest
        .as_ref()
        .and_then(|manifest| manifest.blade.kind.clone());
    let name = options.name.clone().unwrap_or_else(|| {
        manifest
            .as_ref()
            .and_then(preferred_manifest_name)
            .unwrap_or_else(|| folder_name(&root))
    });
    let root_label = folder_name(&root);
    let capsule_set = options.capsule_set.clone().unwrap_or_else(|| name.clone());
    let version = options.version.clone().or_else(|| {
        manifest.as_ref().and_then(|manifest| {
            manifest
                .blade
                .version
                .clone()
                .or(manifest.package.version.clone())
        })
    });
    let manifest_rel = preferred_manifest_anchor(&root).map(|path| path_to_capsule_string(path));
    let entry_rel = manifest
        .as_ref()
        .and_then(preferred_manifest_entry)
        .map(path_to_capsule_string);
    let directory = collect_directory_snapshot(
        &root,
        kind,
        source_kind.clone(),
        root_label.clone(),
        name.clone(),
        capsule_set.clone(),
        version.clone(),
        manifest_rel.clone(),
        entry_rel.clone(),
        &options.output,
    )?;
    let resolved_blade = resolve_snapshot_blade(&root, manifest.as_ref(), &name);
    let selection = build_capsule_selection(&directory, manifest.as_ref(), resolved_blade.as_ref());
    let (files, directories) = select_directory_contents(&directory, &selection, options.contents);
    let preview = build_preview(
        &files,
        options.preview_symbol_limit,
        options.api_index,
        options.module_index,
    );
    Ok(SourceSnapshot {
        kind: directory.kind,
        source_kind: directory.source_kind,
        root_label: directory.root_label,
        name: directory.name,
        capsule_set: directory.capsule_set,
        version: directory.version,
        manifest_rel: directory.manifest_rel,
        entry_rel: directory.entry_rel,
        directories,
        files,
        preview,
    })
}

fn collect_directory_snapshot(
    root: &Path,
    kind: CapsuleKind,
    source_kind: Option<String>,
    root_label: String,
    name: String,
    capsule_set: String,
    version: Option<String>,
    manifest_rel: Option<String>,
    entry_rel: Option<String>,
    output: &Path,
) -> CapsuleResult<DirectorySnapshot> {
    let output_rel = output_relative_to_root(root, output);
    let companion_outputs = output_rel
        .as_deref()
        .map(companion_capsule_relatives)
        .unwrap_or_default();
    let mut directories = Vec::new();
    let mut files = Vec::new();
    let entries = kfs::walk_dir_entries(
        root,
        WalkOptions {
            max_depth: None,
            include_files: true,
            include_dirs: true,
            follow_symlinks: false,
        },
    )?;
    for entry in entries {
        let rel = entry
            .path
            .strip_prefix(root)
            .map_err(|_| CapsuleError::Format("failed to relativize capsule path".to_string()))?;
        if rel.as_os_str().is_empty()
            || should_skip_relative(rel)
            || companion_outputs.contains(rel)
            || output_rel.as_ref().is_some_and(|output| rel == output)
        {
            continue;
        }
        match entry.file_type {
            FsFileType::Directory => directories.push(path_to_capsule_string(rel)),
            FsFileType::File | FsFileType::Symlink | FsFileType::Other => {
                files.push(SourceFile {
                    rel_path: path_to_capsule_string(rel),
                    bytes: fs::read(&entry.path)?,
                });
            }
        }
    }
    files.sort_by(|left, right| left.rel_path.cmp(&right.rel_path));
    directories.sort();
    directories.dedup();
    Ok(DirectorySnapshot {
        root: root.to_path_buf(),
        kind,
        source_kind,
        root_label,
        name,
        capsule_set,
        version,
        manifest_rel,
        entry_rel,
        directories,
        files,
    })
}

fn build_capsule_selection(
    directory: &DirectorySnapshot,
    manifest: Option<&KainManifest>,
    resolved_blade: Option<&blade::ResolvedBlade>,
) -> CapsuleSelection {
    let mut selection = CapsuleSelection::default();
    let root = directory.root.as_path();

    for anchor in ["KAIN.toml", "kain.toml", "build.kn", "platform.kn"] {
        let path = root.join(anchor);
        if path.exists() {
            add_file_selection(&mut selection.source_files, root, &path);
        }
    }
    if let Some(manifest_rel) = directory.manifest_rel.as_deref() {
        selection.source_files.insert(manifest_rel.to_string());
    }
    if let Some(entry_rel) = directory.entry_rel.as_deref() {
        selection.source_files.insert(entry_rel.to_string());
    }

    if let Some(manifest) = manifest {
        add_optional_relative_file(
            &mut selection.source_files,
            root,
            manifest.build.entry.as_deref(),
        );
        add_optional_relative_file(
            &mut selection.source_files,
            root,
            manifest.run.entry.as_deref(),
        );
        add_optional_relative_file(
            &mut selection.source_files,
            root,
            manifest.blade.entry.as_deref(),
        );
        add_optional_relative_dir(
            &mut selection.source_dirs,
            root,
            manifest.build.source_root.as_deref(),
        );
        add_relative_dirs(
            &mut selection.source_dirs,
            root,
            &manifest.build.module_roots,
        );
        add_relative_dirs(
            &mut selection.source_dirs,
            root,
            &manifest.build.module_search_paths,
        );
        add_relative_dirs(
            &mut selection.source_dirs,
            root,
            &manifest.workspace.search_roots,
        );
        add_relative_dirs(
            &mut selection.source_dirs,
            root,
            &manifest.blade.source_roots,
        );
        add_relative_dirs(
            &mut selection.source_dirs,
            root,
            &manifest.blade.module_roots,
        );
        add_relative_paths(
            &mut selection.source_files,
            &mut selection.source_dirs,
            root,
            &manifest.run.watch,
        );
        add_relative_dir(
            &mut selection.artifact_dirs,
            root,
            manifest.build.artifact_root.as_deref(),
        );
        add_relative_dir(
            &mut selection.artifact_dirs,
            root,
            manifest.build.cache_root.as_deref(),
        );

        for task in &manifest.build.tasks {
            add_relative_file_or_dir(
                &mut selection.source_files,
                &mut selection.source_dirs,
                root,
                task.entry.as_deref(),
            );
            add_relative_file_or_dir(
                &mut selection.source_files,
                &mut selection.source_dirs,
                root,
                task.manifest.as_deref(),
            );
            add_relative_paths(
                &mut selection.source_files,
                &mut selection.source_dirs,
                root,
                &task.inputs,
            );
            classify_task_outputs_into_selection(root, task, &mut selection);
        }
    }

    if let Some(blade) = resolved_blade {
        add_absolute_file(&mut selection.source_files, root, blade.entry.as_deref());
        add_absolute_dirs(&mut selection.source_dirs, root, &blade.source_roots);
        add_absolute_dirs(&mut selection.source_dirs, root, &blade.module_roots);
        add_absolute_dirs(&mut selection.source_dirs, root, &blade.gpu_shader_roots);
        add_absolute_files(&mut selection.source_files, root, &blade.gpu_shader_sources);
        for library in &blade.c_ffi_libraries {
            add_absolute_file(&mut selection.source_files, root, Some(&library.header));
            add_absolute_files(&mut selection.source_files, root, &library.sources);
            add_absolute_dirs(&mut selection.source_dirs, root, &library.include_paths);
            add_absolute_file(
                &mut selection.artifact_files,
                root,
                library.shared_lib.as_deref(),
            );
        }
        for artifact in blade.artifacts.values() {
            add_absolute_file(&mut selection.artifact_files, root, Some(artifact));
            add_derived_artifact_sidecars(root, artifact, &mut selection);
        }
    }

    selection
}

fn classify_task_outputs_into_selection(
    root: &Path,
    task: &KainBuildTaskSection,
    selection: &mut CapsuleSelection,
) {
    let evidence_task = task_is_evidence_task(task);
    for output in &task.outputs {
        let resolved = resolve_task_graph_path(root, Some(task.id.as_str()), output);
        if evidence_task {
            add_resolved_file_or_dir(
                &mut selection.evidence_files,
                &mut selection.evidence_dirs,
                root,
                &resolved,
            );
        } else {
            add_resolved_file_or_dir(
                &mut selection.artifact_files,
                &mut selection.artifact_dirs,
                root,
                &resolved,
            );
            add_derived_artifact_sidecars(root, &resolved, selection);
        }
    }
    for key in ["stdout", "stderr"] {
        if let Some(value) = task.options.get(key) {
            let resolved = resolve_task_graph_path(root, Some(task.id.as_str()), Path::new(value));
            if evidence_task {
                add_resolved_file_or_dir(
                    &mut selection.evidence_files,
                    &mut selection.evidence_dirs,
                    root,
                    &resolved,
                );
            } else {
                add_resolved_file_or_dir(
                    &mut selection.artifact_files,
                    &mut selection.artifact_dirs,
                    root,
                    &resolved,
                );
            }
        }
    }
}

fn task_is_evidence_task(task: &KainBuildTaskSection) -> bool {
    let kind = task.kind.trim().to_ascii_lowercase();
    if matches!(
        kind.as_str(),
        "test" | "proof" | "benchmark" | "bench" | "attrition" | "certify" | "check"
    ) {
        return true;
    }
    if kind == "exec" {
        if task
            .telemetry
            .iter()
            .any(|channel| looks_like_evidence_word(channel))
        {
            return true;
        }
        if task
            .outputs
            .iter()
            .map(|path| path.to_string_lossy())
            .any(|value| looks_like_evidence_word(&value))
        {
            return true;
        }
        if task
            .options
            .iter()
            .filter(|(key, _)| matches!(key.as_str(), "stdout" | "stderr"))
            .map(|(_, value)| value.as_str())
            .any(looks_like_evidence_word)
        {
            return true;
        }
    }
    false
}

fn looks_like_evidence_word(value: &str) -> bool {
    let lower = value.replace('\\', "/").to_ascii_lowercase();
    [
        "telemetry",
        "benchmark",
        "attrition",
        "evidence",
        "proof",
        "certify",
        "report",
        "summary",
        "full",
    ]
    .iter()
    .any(|needle| lower.contains(needle))
}

fn add_derived_artifact_sidecars(root: &Path, output: &Path, selection: &mut CapsuleSelection) {
    let lower = output.to_string_lossy().to_ascii_lowercase();
    if lower.ends_with(".exe")
        || lower.ends_with(".dll")
        || lower.ends_with(".so")
        || lower.ends_with(".dylib")
        || lower.ends_with(".ll")
    {
        for suffix in [
            ".runtime_contract.json",
            ".realtime_app.json",
            ".pdb",
            ".ilk",
            ".lib",
            ".exp",
            ".obj",
            ".obj.d",
        ] {
            let derived = append_suffix_to_path(output, suffix);
            add_absolute_file(&mut selection.artifact_files, root, Some(&derived));
        }
    }
}

fn append_suffix_to_path(path: &Path, suffix: &str) -> PathBuf {
    let mut rendered = path.to_string_lossy().into_owned();
    rendered.push_str(suffix);
    PathBuf::from(rendered)
}

fn select_directory_contents(
    directory: &DirectorySnapshot,
    selection: &CapsuleSelection,
    contents: CapsuleContents,
) -> (Vec<SourceFile>, Vec<String>) {
    if contents == CapsuleContents::Snapshot {
        return (directory.files.clone(), directory.directories.clone());
    }
    let mut files = Vec::new();
    for file in &directory.files {
        if looks_like_capsule_file(file) {
            continue;
        }
        if classify_snapshot_file(file, selection) == contents {
            files.push(file.clone());
        }
    }
    let explicit_dirs = match contents {
        CapsuleContents::Source => selection.source_dirs.iter().cloned().collect::<Vec<_>>(),
        CapsuleContents::Artifacts => selection.artifact_dirs.iter().cloned().collect::<Vec<_>>(),
        CapsuleContents::Evidence => selection.evidence_dirs.iter().cloned().collect::<Vec<_>>(),
        CapsuleContents::Assets | CapsuleContents::Snapshot => Vec::new(),
    };
    (
        files.clone(),
        merge_directory_inventory(&files, &explicit_dirs),
    )
}

fn classify_snapshot_file(file: &SourceFile, selection: &CapsuleSelection) -> CapsuleContents {
    if matches_selection_path(
        &file.rel_path,
        &selection.evidence_files,
        &selection.evidence_dirs,
    ) || looks_like_evidence_path(&file.rel_path)
    {
        return CapsuleContents::Evidence;
    }
    if matches_selection_path(
        &file.rel_path,
        &selection.artifact_files,
        &selection.artifact_dirs,
    ) || looks_like_artifact_path(&file.rel_path)
    {
        return CapsuleContents::Artifacts;
    }
    if looks_like_asset_file(file) {
        return CapsuleContents::Assets;
    }
    CapsuleContents::Source
}

fn matches_selection_path(path: &str, files: &BTreeSet<String>, dirs: &BTreeSet<String>) -> bool {
    if files.contains(path) {
        return true;
    }
    dirs.iter()
        .any(|dir| path == dir || path.starts_with(&format!("{dir}/")))
}

fn looks_like_evidence_path(path: &str) -> bool {
    let lower = path.to_ascii_lowercase();
    lower.starts_with("telemetry/full/")
        || lower.starts_with("telemetry/benchmark/")
        || lower.starts_with("telemetry/attrition/")
        || lower.ends_with("kain-evidence.json")
}

fn looks_like_artifact_path(path: &str) -> bool {
    let lower = path.to_ascii_lowercase();
    if lower.starts_with("generated/native_runtime/") {
        return true;
    }
    [
        ".obj",
        ".o",
        ".a",
        ".lib",
        ".dll",
        ".so",
        ".dylib",
        ".exe",
        ".pdb",
        ".ilk",
        ".exp",
        ".wasm",
        ".ll",
        ".bc",
        ".spv",
        ".ptx",
        ".cubin",
        ".obj.d",
        ".o.d",
        ".fingerprint",
        ".runtime_contract.json",
        ".realtime_app.json",
        ".reflect.json",
        ".gpu.rs",
    ]
    .iter()
    .any(|suffix| lower.ends_with(suffix))
}

fn looks_like_capsule_file(file: &SourceFile) -> bool {
    file.rel_path.to_ascii_lowercase().ends_with(".kn")
        && std::str::from_utf8(&file.bytes)
            .ok()
            .is_some_and(|text| text.contains(CAPSULE_SENTINEL_START))
}

fn looks_like_asset_file(file: &SourceFile) -> bool {
    let lower = file.rel_path.to_ascii_lowercase();
    if [
        ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".tga", ".webp", ".ico", ".icns", ".dds", ".ktx",
        ".ktx2", ".hdr", ".exr", ".ttf", ".otf", ".woff", ".woff2", ".mp3", ".wav", ".ogg",
        ".flac", ".mp4", ".mov", ".avi", ".mkv", ".webm", ".glb", ".gltf", ".fbx", ".dae", ".zip",
        ".7z", ".tar", ".gz", ".pdf",
    ]
    .iter()
    .any(|suffix| lower.ends_with(suffix))
    {
        return true;
    }
    std::str::from_utf8(&file.bytes).is_err()
}

fn preferred_manifest_anchor(root: &Path) -> Option<PathBuf> {
    for candidate in ["KAIN.toml", "kain.toml", "build.kn", "platform.kn"] {
        let path = root.join(candidate);
        if path.exists() {
            return Some(PathBuf::from(candidate));
        }
    }
    None
}

fn resolve_snapshot_blade(
    root: &Path,
    manifest: Option<&KainManifest>,
    fallback_name: &str,
) -> Option<blade::ResolvedBlade> {
    let workspace = blade::discover_workspace(root).ok()?;
    let root_portable = portable_compare_path(root);
    workspace
        .blades
        .iter()
        .find(|blade| portable_compare_path(&blade.root) == root_portable)
        .cloned()
        .or_else(|| {
            manifest
                .and_then(preferred_manifest_name)
                .or_else(|| Some(fallback_name.to_string()))
                .and_then(|name| workspace.find_blade(&name).cloned())
        })
}

fn add_relative_paths(
    files: &mut BTreeSet<String>,
    dirs: &mut BTreeSet<String>,
    root: &Path,
    paths: &[PathBuf],
) {
    for path in paths {
        add_relative_file_or_dir(files, dirs, root, Some(path.as_path()));
    }
}

fn add_relative_dirs(dirs: &mut BTreeSet<String>, root: &Path, paths: &[PathBuf]) {
    for path in paths {
        add_relative_dir(dirs, root, Some(path.as_path()));
    }
}

fn add_absolute_files(files: &mut BTreeSet<String>, root: &Path, paths: &[PathBuf]) {
    for path in paths {
        add_absolute_file(files, root, Some(path.as_path()));
    }
}

fn add_absolute_dirs(dirs: &mut BTreeSet<String>, root: &Path, paths: &[PathBuf]) {
    for path in paths {
        add_absolute_dir(dirs, root, Some(path.as_path()));
    }
}

fn add_optional_relative_file(files: &mut BTreeSet<String>, root: &Path, path: Option<&Path>) {
    if let Some(path) = path {
        add_resolved_file(files, root, &root.join(path));
    }
}

fn add_optional_relative_dir(dirs: &mut BTreeSet<String>, root: &Path, path: Option<&Path>) {
    if let Some(path) = path {
        add_resolved_dir(dirs, root, &root.join(path));
    }
}

fn add_relative_dir(dirs: &mut BTreeSet<String>, root: &Path, path: Option<&Path>) {
    if let Some(path) = path {
        add_resolved_dir(dirs, root, &root.join(path));
    }
}

fn add_relative_file_or_dir(
    files: &mut BTreeSet<String>,
    dirs: &mut BTreeSet<String>,
    root: &Path,
    path: Option<&Path>,
) {
    if let Some(path) = path {
        add_resolved_file_or_dir(files, dirs, root, &root.join(path));
    }
}

fn add_absolute_file(files: &mut BTreeSet<String>, root: &Path, path: Option<&Path>) {
    if let Some(path) = path {
        add_resolved_file(files, root, path);
    }
}

fn add_absolute_dir(dirs: &mut BTreeSet<String>, root: &Path, path: Option<&Path>) {
    if let Some(path) = path {
        add_resolved_dir(dirs, root, path);
    }
}

fn add_file_selection(files: &mut BTreeSet<String>, root: &Path, path: &Path) {
    add_resolved_file(files, root, path);
}

fn add_resolved_file_or_dir(
    files: &mut BTreeSet<String>,
    dirs: &mut BTreeSet<String>,
    root: &Path,
    path: &Path,
) {
    if path_looks_like_dir(path) {
        add_resolved_dir(dirs, root, path);
    } else {
        add_resolved_file(files, root, path);
    }
}

fn add_resolved_file(files: &mut BTreeSet<String>, root: &Path, path: &Path) {
    if let Some(rel) = path_relative_to_root(root, path) {
        files.insert(rel);
    }
}

fn add_resolved_dir(dirs: &mut BTreeSet<String>, root: &Path, path: &Path) {
    if let Some(rel) = path_relative_to_root(root, path) {
        if !rel.is_empty() {
            dirs.insert(rel);
        }
    }
}

fn path_relative_to_root(root: &Path, path: &Path) -> Option<String> {
    let root = portable_compare_path(root);
    let path = portable_compare_path(path);
    let rel = path.strip_prefix(root).ok()?;
    let rendered = path_to_capsule_string(rel);
    if rendered.is_empty() {
        None
    } else {
        Some(rendered)
    }
}

fn path_looks_like_dir(path: &Path) -> bool {
    if path.is_dir() {
        return true;
    }
    path.extension().is_none()
        && path
            .file_name()
            .and_then(|value| value.to_str())
            .is_some_and(|value| !value.contains('.'))
}

fn resolve_task_graph_path(root: &Path, task_id: Option<&str>, path: &Path) -> PathBuf {
    let raw = path.to_string_lossy().replace('\\', "/");
    let task_root = root
        .join(".kain")
        .join("out")
        .join("capsule")
        .join(task_id.unwrap_or("task").replace([':', '/', '\\'], "_"));
    for (prefix, base) in [
        ("$root", root),
        ("$repo", root),
        ("$workspace", root),
        ("$blade", root),
        ("$task", task_root.as_path()),
        ("$out", task_root.as_path()),
    ] {
        if raw == prefix {
            return base.to_path_buf();
        }
        if let Some(suffix) = raw.strip_prefix(&format!("{prefix}/")) {
            let mut joined = base.to_path_buf();
            if !suffix.is_empty() {
                joined.push(PathBuf::from(suffix));
            }
            return joined;
        }
    }
    if path.is_absolute() {
        path.to_path_buf()
    } else {
        root.join(path)
    }
}

fn build_archive(snapshot: &SourceSnapshot) -> CapsuleResult<CapsuleArchive> {
    let files = snapshot
        .files
        .iter()
        .map(|file| CapsuleArchiveFile {
            path: file.rel_path.clone(),
            size_bytes: file.bytes.len() as u64,
            sha256: format!("sha256:{}", hex_sha256(&file.bytes)),
            bytes_base64: BASE64_STANDARD.encode(&file.bytes),
        })
        .collect();
    Ok(CapsuleArchive {
        schema: CAPSULE_SCHEMA_VERSION,
        format: CAPSULE_PAYLOAD_FORMAT.to_string(),
        root_label: snapshot.root_label.clone(),
        directories: snapshot.directories.clone(),
        files,
        preview: snapshot.preview.clone(),
    })
}

fn render_capsule_text(
    metadata: &CapsuleMetadata,
    snapshot: &SourceSnapshot,
    archive: &CapsuleArchive,
    payload_bytes: Option<&[u8]>,
    header_style: CapsuleHeaderStyle,
    preview_symbol_limit: usize,
) -> CapsuleResult<String> {
    let mut output = String::new();
    if header_style != CapsuleHeaderStyle::Off {
        render_header(
            &mut output,
            metadata,
            archive,
            header_style,
            preview_symbol_limit,
        );
    }
    render_metadata_block(&mut output, metadata)?;
    match metadata.storage {
        CapsuleStorage::Archive => {
            let payload_bytes = payload_bytes.ok_or_else(|| {
                CapsuleError::Format("archive capsules require a payload block".to_string())
            })?;
            render_payload_block(&mut output, payload_bytes);
        }
        CapsuleStorage::Editable => render_editable_file_blocks(&mut output, &snapshot.files)?,
    }
    Ok(output)
}

fn render_header(
    output: &mut String,
    metadata: &CapsuleMetadata,
    archive: &CapsuleArchive,
    header_style: CapsuleHeaderStyle,
    preview_symbol_limit: usize,
) {
    push_comment_line(
        output,
        Some(&format!(
            "-- KAIN CAPSULE v{} ----------------------------------------------------------",
            metadata.schema
        )),
    );
    push_comment_line(
        output,
        Some(&format!(
            "name:       {}",
            metadata
                .name
                .as_deref()
                .unwrap_or(archive.root_label.as_str())
        )),
    );
    if let Some(version) = metadata.version.as_deref() {
        push_comment_line(output, Some(&format!("version:    {version}")));
    }
    push_comment_line(
        output,
        Some(&format!("kind:       {}", metadata.display_kind())),
    );
    push_comment_line(output, Some(&format!("storage:    {}", metadata.storage)));
    push_comment_line(output, Some(&format!("contents:   {}", metadata.contents)));
    push_comment_line(output, Some(&format!("digest:     {}", metadata.digest)));
    if let Some(capsule_set) = metadata.capsule_set.as_deref() {
        push_comment_line(output, Some(&format!("capsule:    {capsule_set}")));
    }
    if let Some(entry) = metadata.entry.as_deref() {
        push_comment_line(output, Some(&format!("entry:      {entry}")));
    }
    push_comment_line(
        output,
        Some(&format!(
            "structure:  {} files | {} modules",
            metadata.file_count, metadata.module_count
        )),
    );
    push_comment_line(
        output,
        Some("---------------------------------------------------------------------------"),
    );
    if header_style == CapsuleHeaderStyle::Minimal {
        push_comment_line(output, None);
        return;
    }
    push_comment_line(output, None);
    if archive.preview.sections.is_empty() {
        push_comment_line(
            output,
            Some("-- PROJECT STRUCTURE PREVIEW ------------------------------------------------"),
        );
        push_comment_line(output, None);
        for path in archive.files.iter().take(8).map(|file| file.path.as_str()) {
            push_comment_line(output, Some(&format!("{path}")));
        }
        let hidden = archive.files.len().saturating_sub(8);
        if hidden > 0 {
            push_comment_line(
                output,
                Some(&format!(
                    "[+{hidden} more files hidden. use 'kain amalgamate inspect <capsule.kn>' for full log]"
                )),
            );
        }
        push_comment_line(output, None);
        return;
    }
    push_comment_line(
        output,
        Some("-- PUBLIC INTERFACE DIRECTORY -----------------------------------------------"),
    );
    push_comment_line(output, None);
    let mut shown = 0usize;
    for section in &archive.preview.sections {
        if shown >= preview_symbol_limit {
            break;
        }
        push_comment_line(output, Some(&format!("[{}]", section.title)));
        for row in render_symbol_rows(&section.symbols, preview_symbol_limit.saturating_sub(shown))
        {
            shown += row.1;
            push_comment_line(output, Some(&format!("  {}", row.0)));
            if shown >= preview_symbol_limit {
                break;
            }
        }
        push_comment_line(output, None);
    }
    let hidden = archive.preview.total_symbols.saturating_sub(shown);
    if hidden > 0 {
        push_comment_line(
            output,
            Some(&format!(
                "[+{hidden} more symbols hidden. use 'kain amalgamate inspect <capsule.kn>' for full log]"
            )),
        );
        push_comment_line(output, None);
    }
}

fn render_metadata_block(output: &mut String, metadata: &CapsuleMetadata) -> CapsuleResult<()> {
    output.push_str(CAPSULE_SENTINEL_START);
    output.push('\n');
    let toml_text = toml::to_string(metadata)?;
    for line in toml_text.lines() {
        push_comment_line(output, Some(line));
    }
    output.push_str(CAPSULE_SENTINEL_END);
    output.push('\n');
    Ok(())
}

fn render_payload_block(output: &mut String, payload_bytes: &[u8]) {
    output.push_str(CAPSULE_PAYLOAD_START);
    output.push('\n');
    let encoded = BASE64_STANDARD.encode(payload_bytes);
    for chunk in encoded.as_bytes().chunks(88) {
        push_comment_line(output, Some(std::str::from_utf8(chunk).unwrap_or("")));
    }
    output.push_str(CAPSULE_PAYLOAD_END);
    output.push('\n');
}

fn render_editable_file_blocks(output: &mut String, files: &[SourceFile]) -> CapsuleResult<()> {
    for file in files {
        render_editable_file_block(output, file)?;
    }
    Ok(())
}

fn render_editable_file_block(output: &mut String, file: &SourceFile) -> CapsuleResult<()> {
    let text = std::str::from_utf8(&file.bytes)
        .ok()
        .map(normalize_editable_text);
    let file_metadata = EditableFileMetadata {
        path: file.rel_path.clone(),
        kind: if text.is_some() {
            EditableFileKind::Text
        } else {
            EditableFileKind::Binary
        },
        encoding: if text.is_some() {
            CAPSULE_TEXT_ENCODING.to_string()
        } else {
            CAPSULE_PAYLOAD_ENCODING.to_string()
        },
        bytes: file.bytes.len() as u64,
        sha256: format!("sha256:{}", hex_sha256(&file.bytes)),
        trailing_newline: text.as_deref().is_some_and(|value| value.ends_with('\n')),
    };
    output.push_str(CAPSULE_FILE_START);
    output.push('\n');
    let toml_text = toml::to_string(&file_metadata)?;
    for line in toml_text.lines() {
        push_comment_line(output, Some(line));
    }
    output.push_str(CAPSULE_FILE_CONTENT_START);
    output.push('\n');
    if let Some(text) = text {
        let body = text.strip_suffix('\n').unwrap_or(&text);
        if !body.is_empty() {
            for line in body.split('\n') {
                push_comment_line(output, Some(line));
            }
        }
    } else {
        let encoded = BASE64_STANDARD.encode(&file.bytes);
        for chunk in encoded.as_bytes().chunks(88) {
            push_comment_line(output, Some(std::str::from_utf8(chunk).unwrap_or("")));
        }
    }
    output.push_str(CAPSULE_FILE_END);
    output.push('\n');
    Ok(())
}

fn read_capsule(path: &Path) -> CapsuleResult<(String, CapsuleMetadata, ResolvedCapsuleArchive)> {
    let text = read_utf8_file_or_error(path)?;
    let mut metadata = parse_capsule_metadata(&text)?;
    let archive = match metadata.storage {
        CapsuleStorage::Archive => read_archive_capsule(&text, &metadata)?,
        CapsuleStorage::Editable => {
            let archive = read_editable_capsule(&text, &metadata)?;
            refresh_editable_metadata(&mut metadata, &archive)?;
            archive
        }
    };
    Ok((text, metadata, archive))
}

fn read_capsule_with_companions(
    path: &Path,
) -> CapsuleResult<(
    String,
    CapsuleMetadata,
    ResolvedCapsuleArchive,
    Vec<CompanionCapsule>,
)> {
    let (text, metadata, archive) = read_capsule(path)?;
    let companions = discover_companion_capsules(path, &metadata)?;
    let merged = merge_companion_archives(path, &metadata, archive, &companions)?;
    Ok((text, metadata, merged, companions))
}

fn discover_companion_capsules(
    path: &Path,
    metadata: &CapsuleMetadata,
) -> CapsuleResult<Vec<CompanionCapsule>> {
    if metadata.contents != CapsuleContents::Source {
        return Ok(Vec::new());
    }
    let Some(capsule_set) = metadata.capsule_set.as_deref() else {
        return Ok(Vec::new());
    };
    let Some(parent) = path.parent() else {
        return Ok(Vec::new());
    };
    let mut companions = Vec::new();
    for entry in kfs::read_dir_entries(parent)? {
        if entry.file_type != FsFileType::File {
            continue;
        }
        if portable_compare_path(&entry.path) == portable_compare_path(path) {
            continue;
        }
        if entry
            .path
            .extension()
            .and_then(|value| value.to_str())
            .map(|value| !value.eq_ignore_ascii_case("kn"))
            .unwrap_or(true)
        {
            continue;
        }
        let Some(candidate_metadata) = maybe_capsule_metadata(&entry.path)? else {
            continue;
        };
        if candidate_metadata.capsule_set.as_deref() != Some(capsule_set) {
            continue;
        }
        if matches!(
            candidate_metadata.contents,
            CapsuleContents::Source | CapsuleContents::Snapshot
        ) {
            continue;
        }
        let (_, companion_metadata, archive) = read_capsule(&entry.path)?;
        companions.push(CompanionCapsule {
            path: entry.path,
            metadata: companion_metadata,
            archive,
        });
    }
    companions.sort_by(|left, right| {
        left.metadata
            .contents
            .materialize_priority()
            .cmp(&right.metadata.contents.materialize_priority())
            .then(left.path.cmp(&right.path))
    });
    Ok(companions)
}

fn merge_companion_archives(
    path: &Path,
    metadata: &CapsuleMetadata,
    archive: ResolvedCapsuleArchive,
    companions: &[CompanionCapsule],
) -> CapsuleResult<ResolvedCapsuleArchive> {
    if companions.is_empty() {
        return Ok(archive);
    }
    let mut files = BTreeMap::<String, Vec<u8>>::new();
    for file in &archive.files {
        files.insert(file.rel_path.clone(), file.bytes.clone());
    }
    let mut directories = archive.directories.clone();
    for companion in companions {
        directories.extend(companion.archive.directories.iter().cloned());
        for file in &companion.archive.files {
            if let Some(existing) = files.get(&file.rel_path) {
                if existing != &file.bytes {
                    return Err(CapsuleError::Format(format!(
                        "capsule companion '{}' conflicts with '{}' at '{}'",
                        companion.path.display(),
                        path.display(),
                        file.rel_path
                    )));
                }
                continue;
            }
            files.insert(file.rel_path.clone(), file.bytes.clone());
        }
    }
    let mut merged_files = files
        .into_iter()
        .map(|(rel_path, bytes)| SourceFile { rel_path, bytes })
        .collect::<Vec<_>>();
    merged_files.sort_by(|left, right| left.rel_path.cmp(&right.rel_path));
    directories = merge_directory_inventory(&merged_files, &directories);
    let preview = build_preview(
        &merged_files,
        metadata.preview_symbol_limit,
        metadata.api_index,
        metadata.module_index,
    );
    Ok(ResolvedCapsuleArchive {
        root_label: archive.root_label,
        directories,
        files: merged_files,
        preview,
    })
}

fn materialization_state_json(
    path: &Path,
    metadata: &CapsuleMetadata,
    companions: &[CompanionCapsule],
) -> CapsuleResult<String> {
    Ok(serde_json::to_string_pretty(&serde_json::json!({
        "schema_version": 1,
        "primary_path": process_portable_path(path),
        "primary": metadata,
        "companions": companions.iter().map(|companion| serde_json::json!({
            "path": process_portable_path(&companion.path),
            "digest": companion.metadata.digest,
            "contents": companion.metadata.contents.as_str(),
            "capsule_set": companion.metadata.capsule_set,
        })).collect::<Vec<_>>(),
    }))?)
}

fn parse_capsule_metadata(text: &str) -> CapsuleResult<CapsuleMetadata> {
    let block = extract_block_text(text, CAPSULE_SENTINEL_START, CAPSULE_SENTINEL_END)?;
    let metadata: CapsuleMetadata = toml::from_str(&block)?;
    if metadata.schema != CAPSULE_SCHEMA_VERSION_V1 && metadata.schema != CAPSULE_SCHEMA_VERSION {
        return Err(CapsuleError::Format(format!(
            "unsupported capsule schema {}; expected {} or {}",
            metadata.schema, CAPSULE_SCHEMA_VERSION_V1, CAPSULE_SCHEMA_VERSION
        )));
    }
    Ok(metadata)
}

fn read_archive_capsule(
    text: &str,
    metadata: &CapsuleMetadata,
) -> CapsuleResult<ResolvedCapsuleArchive> {
    let payload_text = extract_block_text(text, CAPSULE_PAYLOAD_START, CAPSULE_PAYLOAD_END)?;
    let payload_text = payload_text.lines().collect::<String>();
    let payload_bytes = BASE64_STANDARD.decode(payload_text.as_bytes())?;
    if metadata.payload_encoding.as_deref() != Some(CAPSULE_PAYLOAD_ENCODING) {
        return Err(CapsuleError::Format(format!(
            "unsupported capsule payload encoding '{}'",
            metadata.payload_encoding.as_deref().unwrap_or("<missing>")
        )));
    }
    if metadata.payload_format.as_deref() != Some(CAPSULE_PAYLOAD_FORMAT) {
        return Err(CapsuleError::Format(format!(
            "unsupported capsule payload format '{}'",
            metadata.payload_format.as_deref().unwrap_or("<missing>")
        )));
    }
    let payload_sha = format!("sha256:{}", hex_sha256(&payload_bytes));
    if metadata.payload_sha256.as_deref() != Some(payload_sha.as_str()) {
        return Err(CapsuleError::Format(
            "capsule payload digest does not match metadata".to_string(),
        ));
    }
    let archive_bytes = match metadata.compression {
        CapsuleCompression::Zstd => zstd::stream::decode_all(Cursor::new(payload_bytes))?,
        CapsuleCompression::None => payload_bytes,
    };
    let archive: CapsuleArchive = serde_json::from_slice(&archive_bytes)?;
    let resolved = resolved_archive_from_payload(&archive)?;
    validate_resolved_archive_against_metadata(&resolved, metadata)?;
    Ok(resolved)
}

fn read_editable_capsule(
    text: &str,
    metadata: &CapsuleMetadata,
) -> CapsuleResult<ResolvedCapsuleArchive> {
    let mut files = Vec::new();
    let mut lines = text.lines();
    while let Some(line) = lines.next() {
        if line.trim() != CAPSULE_FILE_START {
            continue;
        }
        let mut metadata_body = String::new();
        loop {
            let line = lines.next().ok_or_else(|| {
                CapsuleError::Format(
                    "editable capsule file block is missing content marker".to_string(),
                )
            })?;
            let trimmed = line.trim();
            if trimmed == CAPSULE_FILE_CONTENT_START {
                break;
            }
            if trimmed == CAPSULE_FILE_END {
                return Err(CapsuleError::Format(
                    "editable capsule file block ended before content marker".to_string(),
                ));
            }
            let stripped = strip_comment_prefix(line)?;
            metadata_body.push_str(stripped);
            metadata_body.push('\n');
        }
        let file_metadata: EditableFileMetadata = toml::from_str(&metadata_body)?;
        let mut content_lines = Vec::new();
        loop {
            let line = lines.next().ok_or_else(|| {
                CapsuleError::Format(
                    "editable capsule file block is missing an end marker".to_string(),
                )
            })?;
            if line.trim() == CAPSULE_FILE_END {
                break;
            }
            content_lines.push(strip_comment_prefix(line)?.to_string());
        }
        let bytes = match file_metadata.kind {
            EditableFileKind::Text => {
                if file_metadata.encoding != CAPSULE_TEXT_ENCODING {
                    return Err(CapsuleError::Format(format!(
                        "unsupported editable text encoding '{}'",
                        file_metadata.encoding
                    )));
                }
                let mut text = content_lines.join("\n");
                if file_metadata.trailing_newline {
                    text.push('\n');
                }
                text.into_bytes()
            }
            EditableFileKind::Binary => {
                if file_metadata.encoding != CAPSULE_PAYLOAD_ENCODING {
                    return Err(CapsuleError::Format(format!(
                        "unsupported editable binary encoding '{}'",
                        file_metadata.encoding
                    )));
                }
                BASE64_STANDARD.decode(content_lines.concat().as_bytes())?
            }
        };
        files.push(SourceFile {
            rel_path: file_metadata.path,
            bytes,
        });
    }
    let root_label = metadata
        .root_label
        .clone()
        .or_else(|| metadata.name.clone())
        .unwrap_or_else(|| "capsule".to_string());
    let preview = build_preview(
        &files,
        metadata.preview_symbol_limit,
        metadata.api_index,
        metadata.module_index,
    );
    let resolved = ResolvedCapsuleArchive {
        root_label,
        directories: merge_directory_inventory(&files, &metadata.directories),
        files,
        preview,
    };
    Ok(resolved)
}

fn extract_block_text(text: &str, start_marker: &str, end_marker: &str) -> CapsuleResult<String> {
    let mut inside = false;
    let mut body = String::new();
    for line in text.lines() {
        let trimmed = line.trim();
        if trimmed == start_marker {
            inside = true;
            continue;
        }
        if trimmed == end_marker {
            return Ok(body);
        }
        if inside {
            let stripped = strip_comment_prefix(line)?;
            body.push_str(stripped);
            body.push('\n');
        }
    }
    Err(CapsuleError::Format(format!(
        "missing capsule marker block '{}'..'{}'",
        start_marker, end_marker
    )))
}

fn strip_comment_prefix(line: &str) -> CapsuleResult<&str> {
    let trimmed = line.trim_start();
    let rest = trimmed.strip_prefix("//").ok_or_else(|| {
        CapsuleError::Format("capsule block lines must stay comment-prefixed".to_string())
    })?;
    Ok(rest.strip_prefix(' ').unwrap_or(rest))
}

fn validate_archive_against_metadata(
    archive: &CapsuleArchive,
    metadata: &CapsuleMetadata,
) -> CapsuleResult<()> {
    if archive.schema != CAPSULE_SCHEMA_VERSION_V1 && archive.schema != CAPSULE_SCHEMA_VERSION {
        return Err(CapsuleError::Format(
            "capsule archive schema does not match a supported capsule version".to_string(),
        ));
    }
    if archive.format != CAPSULE_PAYLOAD_FORMAT {
        return Err(CapsuleError::Format(
            "capsule archive payload format tag does not match v1".to_string(),
        ));
    }
    if archive.files.len() != metadata.file_count {
        return Err(CapsuleError::Format(
            "capsule file count does not match metadata".to_string(),
        ));
    }
    if archive.preview.module_count != metadata.module_count {
        return Err(CapsuleError::Format(
            "capsule module count does not match metadata".to_string(),
        ));
    }
    Ok(())
}

fn validate_resolved_archive_against_metadata(
    archive: &ResolvedCapsuleArchive,
    metadata: &CapsuleMetadata,
) -> CapsuleResult<()> {
    if metadata.storage == CapsuleStorage::Editable {
        return Ok(());
    }
    if archive.files.len() != metadata.file_count {
        return Err(CapsuleError::Format(
            "capsule file count does not match metadata".to_string(),
        ));
    }
    if archive.preview.module_count != metadata.module_count {
        return Err(CapsuleError::Format(
            "capsule module count does not match metadata".to_string(),
        ));
    }
    let encoded_archive = build_capsule_archive_from_resolved(archive)?;
    validate_archive_against_metadata(&encoded_archive, metadata)?;
    let archive_bytes = serde_json::to_vec_pretty(&encoded_archive)?;
    let digest = format!("sha256:{}", hex_sha256(&archive_bytes));
    if digest != metadata.digest {
        return Err(CapsuleError::Format(
            "capsule archive digest does not match metadata".to_string(),
        ));
    }
    Ok(())
}

fn resolved_archive_from_payload(
    archive: &CapsuleArchive,
) -> CapsuleResult<ResolvedCapsuleArchive> {
    let mut files = Vec::with_capacity(archive.files.len());
    for file in &archive.files {
        let bytes = BASE64_STANDARD.decode(file.bytes_base64.as_bytes())?;
        let digest = format!("sha256:{}", hex_sha256(&bytes));
        if digest != file.sha256 {
            return Err(CapsuleError::Format(format!(
                "archived file '{}' failed digest verification",
                file.path
            )));
        }
        files.push(SourceFile {
            rel_path: file.path.clone(),
            bytes,
        });
    }
    Ok(ResolvedCapsuleArchive {
        root_label: archive.root_label.clone(),
        directories: archive.directories.clone(),
        files,
        preview: archive.preview.clone(),
    })
}

fn build_capsule_archive_from_resolved(
    archive: &ResolvedCapsuleArchive,
) -> CapsuleResult<CapsuleArchive> {
    let files = archive
        .files
        .iter()
        .map(|file| CapsuleArchiveFile {
            path: file.rel_path.clone(),
            size_bytes: file.bytes.len() as u64,
            sha256: format!("sha256:{}", hex_sha256(&file.bytes)),
            bytes_base64: BASE64_STANDARD.encode(&file.bytes),
        })
        .collect();
    Ok(CapsuleArchive {
        schema: CAPSULE_SCHEMA_VERSION,
        format: CAPSULE_PAYLOAD_FORMAT.to_string(),
        root_label: archive.root_label.clone(),
        directories: archive.directories.clone(),
        files,
        preview: archive.preview.clone(),
    })
}

fn unpack_resolved_archive(
    archive: &ResolvedCapsuleArchive,
    output_root: &Path,
) -> CapsuleResult<()> {
    kfs::create_dir_all(output_root)?;
    for directory in &archive.directories {
        let rel = capsule_rel_to_path(directory);
        validate_output_relative_path(&rel)?;
        kfs::create_dir_all(output_root.join(rel))?;
    }
    for file in &archive.files {
        let rel = capsule_rel_to_path(&file.rel_path);
        validate_output_relative_path(&rel)?;
        let path = output_root.join(&rel);
        kfs::write_bytes(path, &file.bytes)?;
    }
    Ok(())
}

fn refresh_editable_metadata(
    metadata: &mut CapsuleMetadata,
    archive: &ResolvedCapsuleArchive,
) -> CapsuleResult<()> {
    let encoded_archive = build_capsule_archive_from_resolved(archive)?;
    let archive_bytes = serde_json::to_vec_pretty(&encoded_archive)?;
    metadata.schema = CAPSULE_SCHEMA_VERSION;
    metadata.storage = CapsuleStorage::Editable;
    metadata.compression = CapsuleCompression::None;
    metadata.payload_sha256 = None;
    metadata.payload_encoding = None;
    metadata.payload_format = None;
    metadata.payload_bytes = None;
    metadata.digest = format!("sha256:{}", hex_sha256(&archive_bytes));
    metadata.file_count = archive.files.len();
    metadata.module_count = archive.preview.module_count;
    metadata.directories = archive.directories.clone();
    metadata.root_label = Some(archive.root_label.clone());
    Ok(())
}

fn determine_capsule_kind(root: &Path, manifest: Option<&KainManifest>) -> CapsuleKind {
    let Some(manifest) = manifest else {
        return CapsuleKind::Directory;
    };
    if manifest.blade.entry.is_some()
        || !manifest.blade.source_roots.is_empty()
        || !manifest.blade.module_roots.is_empty()
        || manifest.blade.name.is_some()
    {
        return CapsuleKind::Blade;
    }
    if root.join("src").exists() {
        return CapsuleKind::Workspace;
    }
    CapsuleKind::Directory
}

fn preferred_manifest_name(manifest: &KainManifest) -> Option<String> {
    manifest
        .blade
        .name
        .clone()
        .or_else(|| manifest.package.name.clone())
}

fn preferred_manifest_entry(manifest: &KainManifest) -> Option<PathBuf> {
    manifest
        .blade
        .entry
        .clone()
        .or_else(|| manifest.run.entry.clone())
        .or_else(|| manifest.build.entry.clone())
}

fn build_preview(
    files: &[SourceFile],
    preview_symbol_limit: usize,
    api_index: CapsuleIndexMode,
    module_index: CapsuleIndexMode,
) -> CapsulePreview {
    let mut sections = ordered_preview_buckets();
    let module_count = if module_index == CapsuleIndexMode::Off {
        0
    } else {
        files
            .iter()
            .filter(|file| file.rel_path.ends_with(".kn"))
            .count()
    };
    if api_index == CapsuleIndexMode::Off {
        return CapsulePreview {
            module_count,
            total_symbols: 0,
            sections: Vec::new(),
        };
    }
    for file in files.iter().filter(|file| file.rel_path.ends_with(".kn")) {
        let Ok(source) = std::str::from_utf8(&file.bytes) else {
            continue;
        };
        for line in source.lines() {
            if line.starts_with(' ') || line.starts_with('\t') {
                continue;
            }
            let trimmed = line.trim();
            if trimmed.is_empty() || trimmed.starts_with("//") {
                continue;
            }
            if let Some((bucket, symbol)) = parse_top_level_symbol(trimmed) {
                if let Some(symbols) = sections.get_mut(bucket) {
                    push_unique(symbols, symbol);
                }
            }
        }
    }
    let ordered = [
        "constants",
        "types",
        "traits",
        "components",
        "actors",
        "worlds",
        "functions",
        "patches",
        "laws",
        "converges",
        "orchestrates",
        "shaders",
        "axioms",
    ];
    let mut preview_sections = Vec::new();
    let mut total_symbols = 0usize;
    for bucket in ordered {
        if let Some(symbols) = sections.remove(bucket) {
            if symbols.is_empty() {
                continue;
            }
            total_symbols += symbols.len();
            preview_sections.push(CapsulePreviewSection {
                title: bucket.to_string(),
                symbols,
            });
        }
    }
    if preview_symbol_limit == 0 {
        preview_sections.clear();
    }
    CapsulePreview {
        module_count,
        total_symbols,
        sections: preview_sections,
    }
}

fn ordered_preview_buckets() -> BTreeMap<&'static str, Vec<String>> {
    let mut buckets = BTreeMap::new();
    for name in [
        "constants",
        "types",
        "traits",
        "components",
        "actors",
        "worlds",
        "functions",
        "patches",
        "laws",
        "converges",
        "orchestrates",
        "shaders",
        "axioms",
    ] {
        buckets.insert(name, Vec::new());
    }
    buckets
}

fn parse_top_level_symbol(line: &str) -> Option<(&'static str, String)> {
    if let Some(rest) = line.strip_prefix("const ") {
        let name = rest
            .split(|c: char| c == ':' || c == '=' || c.is_whitespace())
            .next()
            .unwrap_or_default();
        if !name.is_empty() {
            return Some(("constants", name.to_string()));
        }
    }
    for (prefix, bucket, keep_prefix) in [
        ("shatter struct ", "types", true),
        ("struct ", "types", true),
        ("trait ", "traits", true),
        ("component ", "components", true),
        ("actor ", "actors", true),
        ("world ", "worlds", true),
        ("fn ", "functions", true),
        ("patch ", "patches", true),
        ("law ", "laws", true),
        ("converge ", "converges", true),
        ("orchestrate ", "orchestrates", true),
        ("shader ", "shaders", true),
        ("axiom ", "axioms", true),
    ] {
        if let Some(rest) = line.strip_prefix(prefix) {
            let signature = rest
                .trim_end_matches(':')
                .split('{')
                .next()
                .unwrap_or(rest)
                .trim();
            let label = if keep_prefix {
                format!("{} {}", prefix.trim(), signature)
            } else {
                signature.to_string()
            };
            return Some((bucket, label));
        }
    }
    None
}

fn render_symbol_rows(symbols: &[String], remaining_symbols: usize) -> Vec<(String, usize)> {
    let mut rows = Vec::new();
    let mut current = String::new();
    let mut current_count = 0usize;
    for symbol in symbols.iter().take(remaining_symbols) {
        let candidate = if current.is_empty() {
            symbol.clone()
        } else {
            format!("{current}, {symbol}")
        };
        if !current.is_empty() && candidate.len() > 78 {
            rows.push((current, current_count));
            current = symbol.clone();
            current_count = 1;
            continue;
        }
        current = candidate;
        current_count += 1;
    }
    if !current.is_empty() {
        rows.push((current, current_count));
    }
    rows
}

fn push_comment_line(output: &mut String, content: Option<&str>) {
    match content {
        Some(line) if line.is_empty() => output.push_str("//\n"),
        Some(line) => {
            output.push_str("// ");
            output.push_str(line);
            output.push('\n');
        }
        None => output.push_str("//\n"),
    }
}

fn push_unique(symbols: &mut Vec<String>, symbol: String) {
    if !symbols.iter().any(|existing| existing == &symbol) {
        symbols.push(symbol);
    }
}

fn merge_directory_inventory(files: &[SourceFile], metadata_directories: &[String]) -> Vec<String> {
    let file_paths = files
        .iter()
        .map(|file| file.rel_path.as_str())
        .collect::<BTreeSet<_>>();
    let mut directories = metadata_directories
        .iter()
        .filter(|value| !value.trim().is_empty())
        .filter(|value| !file_paths.contains(value.as_str()))
        .cloned()
        .collect::<Vec<_>>();
    for file in files {
        let mut current = PathBuf::new();
        let rel_path = capsule_rel_to_path(&file.rel_path);
        if let Some(parent) = rel_path.parent() {
            for component in parent.components() {
                if let Component::Normal(value) = component {
                    current.push(value);
                    let candidate = path_to_capsule_string(&current);
                    if !candidate.is_empty() {
                        directories.push(candidate);
                    }
                }
            }
        }
    }
    directories.sort();
    directories.dedup();
    directories
}

fn normalize_editable_text(text: &str) -> String {
    text.replace("\r\n", "\n").replace('\r', "\n")
}

fn output_relative_to_root(root: &Path, output: &Path) -> Option<PathBuf> {
    let output = if output.is_absolute() {
        output.to_path_buf()
    } else {
        std::env::current_dir().ok()?.join(output)
    };
    portable_compare_path(&output)
        .strip_prefix(portable_compare_path(root))
        .ok()
        .map(PathBuf::from)
}

fn portable_compare_path(path: &Path) -> PathBuf {
    #[cfg(windows)]
    {
        let rendered = path.as_os_str().to_string_lossy();
        if let Some(stripped) = rendered.strip_prefix(r"\\?\UNC\") {
            return PathBuf::from(format!(r"\\{stripped}"));
        }
        if let Some(stripped) = rendered.strip_prefix(r"\\?\") {
            return PathBuf::from(stripped);
        }
    }
    path.to_path_buf()
}

fn process_portable_path(path: &Path) -> String {
    portable_compare_path(path).display().to_string()
}

fn should_skip_relative(path: &Path) -> bool {
    path.components().any(|component| {
        if let Component::Normal(name) = component {
            matches!(
                name.to_string_lossy().as_ref(),
                ".git" | ".kain" | "target" | "node_modules" | "__pycache__"
            )
        } else {
            false
        }
    })
}

fn companion_capsule_relatives(output_rel: &Path) -> BTreeSet<PathBuf> {
    let Some(file_name) = output_rel.file_name().and_then(|value| value.to_str()) else {
        return BTreeSet::new();
    };
    let Some(stem) = file_name.strip_suffix(".kn") else {
        return BTreeSet::new();
    };
    let base = stem
        .strip_suffix(".artifacts")
        .or_else(|| stem.strip_suffix(".evidence"))
        .unwrap_or(stem);
    if base.is_empty() {
        return BTreeSet::new();
    }

    let parent = output_rel.parent().unwrap_or_else(|| Path::new(""));
    [
        format!("{base}.kn"),
        format!("{base}.artifacts.kn"),
        format!("{base}.evidence.kn"),
    ]
    .into_iter()
    .map(|name| parent.join(name))
    .collect()
}

fn validate_output_relative_path(path: &Path) -> CapsuleResult<()> {
    for component in path.components() {
        match component {
            Component::Normal(_) => {}
            Component::CurDir => {}
            Component::ParentDir | Component::RootDir | Component::Prefix(_) => {
                return Err(CapsuleError::Format(format!(
                    "capsule path '{}' is not a safe relative path",
                    path.display()
                )));
            }
        }
    }
    Ok(())
}

fn read_utf8_if_file(path: &Path) -> CapsuleResult<Option<String>> {
    if !path.is_file() {
        return Ok(None);
    }
    let bytes = fs::read(path)?;
    match String::from_utf8(bytes) {
        Ok(text) => Ok(Some(text)),
        Err(_) => Ok(None),
    }
}

fn read_utf8_file_or_error(path: &Path) -> CapsuleResult<String> {
    let bytes = fs::read(path)?;
    String::from_utf8(bytes).map_err(|_| {
        CapsuleError::Format(format!(
            "capsule file '{}' is not valid UTF-8 text",
            path.display()
        ))
    })
}

fn hex_sha256(bytes: &[u8]) -> String {
    let mut hasher = Sha256::new();
    hasher.update(bytes);
    format!("{:x}", hasher.finalize())
}

fn path_to_capsule_string(path: impl AsRef<Path>) -> String {
    path.as_ref()
        .components()
        .filter_map(|component| match component {
            Component::Normal(value) => Some(value.to_string_lossy().into_owned()),
            Component::CurDir => None,
            _ => None,
        })
        .collect::<Vec<_>>()
        .join("/")
}

fn capsule_rel_to_path(path: &str) -> PathBuf {
    path.split('/').filter(|segment| !segment.is_empty()).fold(
        PathBuf::new(),
        |mut acc, segment| {
            acc.push(segment);
            acc
        },
    )
}

fn folder_name(path: &Path) -> String {
    path.file_name()
        .map(|value| value.to_string_lossy().into_owned())
        .filter(|value| !value.is_empty())
        .unwrap_or_else(|| "capsule".to_string())
}

fn file_stem_string(path: &Path) -> String {
    path.file_stem()
        .map(|value| value.to_string_lossy().into_owned())
        .filter(|value| !value.is_empty())
        .unwrap_or_else(|| "capsule".to_string())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{SystemTime, UNIX_EPOCH};

    fn unique_temp_dir(label: &str) -> PathBuf {
        let unique = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("clock")
            .as_nanos();
        let root = std::env::temp_dir().join(format!("kain-amalgamate-{label}-{unique}"));
        fs::create_dir_all(&root).expect("create temp dir");
        root
    }

    #[test]
    fn pack_capsule_skips_existing_output_file_inside_input_root() {
        let root = unique_temp_dir("skip-output");
        fs::write(root.join("main.kn"), "fn main() -> Int:\n    return 0\n").expect("write main");
        fs::write(root.join("capsule.kn"), "old capsule").expect("seed old output");

        let report =
            pack_capsule(&PackOptions::new(&root, root.join("capsule.kn"))).expect("pack capsule");
        let inspect = inspect_capsule(&report.output_path).expect("inspect capsule");

        assert!(inspect.files.iter().any(|file| file.path == "main.kn"));
        assert!(!inspect.files.iter().any(|file| file.path == "capsule.kn"));

        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn contents_profiles_split_source_assets_artifacts_and_evidence() {
        let root = unique_temp_dir("contents-split");
        fs::create_dir_all(root.join("src")).expect("create src");
        fs::create_dir_all(root.join("native")).expect("create native");
        fs::create_dir_all(root.join("telemetry").join("full")).expect("create telemetry");
        fs::create_dir_all(
            root.join("generated")
                .join("native_runtime")
                .join("objects"),
        )
        .expect("create generated");
        fs::write(
            root.join("src").join("main.kn"),
            "fn main() -> Int:\n    return 0\n",
        )
        .expect("write main");
        fs::write(
            root.join("telemetry").join("python_bridge.kn"),
            "fn bridge() -> Int:\n    return 7\n",
        )
        .expect("write watch file");
        fs::write(
            root.join("telemetry").join("run_smoketest_mode.kn"),
            "fn mode() -> Int:\n    return 9\n",
        )
        .expect("write second watch file");
        fs::write(
            root.join("native").join("bridge.c"),
            "int bridge(void) { return 7; }\n",
        )
        .expect("write bridge");
        fs::write(root.join("README.md"), "# Smoketest\n").expect("write readme");
        fs::write(root.join("logo.png"), [0u8, 1, 2, 3]).expect("write asset");
        fs::write(root.join("manual-smoketest.exe"), b"MZ").expect("write exe");
        fs::write(root.join("manual-smoketest.runtime_contract.json"), "{}")
            .expect("write runtime contract");
        fs::write(
            root.join("generated")
                .join("native_runtime")
                .join("objects")
                .join("manual-smoketest.obj"),
            [9u8, 8, 7],
        )
        .expect("write obj");
        fs::write(
            root.join("telemetry").join("full").join("summary.json"),
            "{\"ok\":true}\n",
        )
        .expect("write telemetry");
        fs::write(
            root.join("KAIN.toml"),
            r#"[package]
name = "contents-split"
version = "0.1.0"

[blade]
name = "contents-split"
kind = "kain_app"
entry = "src/main.kn"
source_roots = ["src", "telemetry", "native"]
module_roots = ["src", "telemetry"]

[run]
entry = "src/main.kn"
watch = ["telemetry", "telemetry/python_bridge.kn", "telemetry/run_smoketest_mode.kn"]

[build]
entry = "src/main.kn"
artifact_root = ".kain/out/llvm"
cache_root = ".kain/cache/build"
"#,
        )
        .expect("write manifest");

        let mut source_options = PackOptions::new(&root, root.join("source.kn"));
        source_options.contents = CapsuleContents::Source;
        let source_report = pack_capsule(&source_options).expect("pack source");
        let source = inspect_capsule(&source_report.output_path).expect("inspect source");
        assert!(source.files.iter().any(|file| file.path == "src/main.kn"));
        assert!(source
            .files
            .iter()
            .any(|file| file.path == "native/bridge.c"));
        assert!(source.files.iter().any(|file| file.path == "README.md"));
        assert!(source
            .files
            .iter()
            .any(|file| file.path == "telemetry/python_bridge.kn"));
        assert!(source
            .files
            .iter()
            .any(|file| file.path == "telemetry/run_smoketest_mode.kn"));
        assert!(source.directories.iter().any(|dir| dir == "telemetry"));
        assert!(!source
            .directories
            .iter()
            .any(|dir| dir == "telemetry/python_bridge.kn"));
        assert!(!source
            .directories
            .iter()
            .any(|dir| dir == "telemetry/run_smoketest_mode.kn"));
        assert!(!source.files.iter().any(|file| file.path == "logo.png"));
        assert!(!source
            .files
            .iter()
            .any(|file| file.path == "manual-smoketest.exe"));
        assert!(!source
            .files
            .iter()
            .any(|file| file.path == "manual-smoketest.runtime_contract.json"));
        assert!(!source
            .files
            .iter()
            .any(|file| file.path == "telemetry/full/summary.json"));

        let mut asset_options = PackOptions::new(&root, root.join("assets.kn"));
        asset_options.contents = CapsuleContents::Assets;
        let asset_report = pack_capsule(&asset_options).expect("pack assets");
        let assets = inspect_capsule(&asset_report.output_path).expect("inspect assets");
        assert!(assets.files.iter().any(|file| file.path == "logo.png"));
        assert!(!assets.files.iter().any(|file| file.path == "src/main.kn"));

        let mut artifact_options = PackOptions::new(&root, root.join("artifacts.kn"));
        artifact_options.contents = CapsuleContents::Artifacts;
        let artifact_report = pack_capsule(&artifact_options).expect("pack artifacts");
        let artifacts = inspect_capsule(&artifact_report.output_path).expect("inspect artifacts");
        assert!(artifacts
            .files
            .iter()
            .any(|file| file.path == "manual-smoketest.exe"));
        assert!(artifacts
            .files
            .iter()
            .any(|file| file.path == "manual-smoketest.runtime_contract.json"));
        assert!(artifacts
            .files
            .iter()
            .any(|file| { file.path == "generated/native_runtime/objects/manual-smoketest.obj" }));
        assert!(!artifacts
            .files
            .iter()
            .any(|file| file.path == "telemetry/full/summary.json"));

        let mut evidence_options = PackOptions::new(&root, root.join("evidence.kn"));
        evidence_options.contents = CapsuleContents::Evidence;
        let evidence_report = pack_capsule(&evidence_options).expect("pack evidence");
        let evidence = inspect_capsule(&evidence_report.output_path).expect("inspect evidence");
        assert!(evidence
            .files
            .iter()
            .any(|file| file.path == "telemetry/full/summary.json"));
        assert!(!evidence
            .files
            .iter()
            .any(|file| file.path == "manual-smoketest.exe"));

        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn materialize_source_capsule_merges_matching_companions() {
        let root = unique_temp_dir("materialize-companions");
        fs::create_dir_all(root.join("src")).expect("create src");
        fs::create_dir_all(root.join("telemetry").join("full")).expect("create telemetry");
        fs::write(
            root.join("src").join("main.kn"),
            "fn main() -> Int:\n    return 0\n",
        )
        .expect("write main");
        fs::write(root.join("manual-smoketest.exe"), b"MZ").expect("write exe");
        fs::write(
            root.join("telemetry").join("full").join("summary.json"),
            "{\"ok\":true}\n",
        )
        .expect("write telemetry");

        let mut source_options = PackOptions::new(&root, root.join("smoketest.kn"));
        source_options.contents = CapsuleContents::Source;
        source_options.capsule_set = Some("smoketest".to_string());
        let source_report = pack_capsule(&source_options).expect("pack source");

        let mut artifact_options = PackOptions::new(&root, root.join("smoketest.artifacts.kn"));
        artifact_options.contents = CapsuleContents::Artifacts;
        artifact_options.capsule_set = Some("smoketest".to_string());
        pack_capsule(&artifact_options).expect("pack artifacts");

        let mut evidence_options = PackOptions::new(&root, root.join("smoketest.evidence.kn"));
        evidence_options.contents = CapsuleContents::Evidence;
        evidence_options.capsule_set = Some("smoketest".to_string());
        pack_capsule(&evidence_options).expect("pack evidence");

        let materialized = materialize_capsule(&source_report.output_path, &root.join(".cache"))
            .expect("materialize source capsule");
        assert!(materialized
            .workspace_root
            .join("src")
            .join("main.kn")
            .exists());
        assert!(materialized
            .workspace_root
            .join("manual-smoketest.exe")
            .exists());
        assert!(materialized
            .workspace_root
            .join("telemetry")
            .join("full")
            .join("summary.json")
            .exists());

        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn source_capsule_skips_existing_companion_outputs() {
        let root = unique_temp_dir("skip-existing-companions");
        fs::create_dir_all(root.join("src")).expect("create src");
        fs::write(
            root.join("src").join("main.kn"),
            "fn main() -> Int:\n    return 0\n",
        )
        .expect("write main");
        fs::write(root.join("smoketest.artifacts.kn"), "stale artifact capsule")
            .expect("write stale artifacts");
        fs::write(root.join("smoketest.evidence.kn"), "stale evidence capsule")
            .expect("write stale evidence");

        let mut source_options = PackOptions::new(&root, root.join("smoketest.kn"));
        source_options.contents = CapsuleContents::Source;
        source_options.capsule_set = Some("smoketest".to_string());
        let source_report = pack_capsule(&source_options).expect("pack source");
        let source = inspect_capsule(&source_report.output_path).expect("inspect source");

        assert!(source.files.iter().any(|file| file.path == "src/main.kn"));
        assert!(!source
            .files
            .iter()
            .any(|file| file.path == "smoketest.artifacts.kn"));
        assert!(!source
            .files
            .iter()
            .any(|file| file.path == "smoketest.evidence.kn"));

        let _ = fs::remove_dir_all(root);
    }
}
