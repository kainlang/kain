use crate::diagnostics::{check_document, check_workspace, CheckResult};
use crate::formatting::{format_document, FormatResult};
use crate::index::{analyze_document, WorkspaceIndex};
use crate::queries::{self, HoverResult};
use crate::{CompletionResult, LocationResult, SemanticTokenResult, SymbolResult};
use kain_core::CompileTarget;
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::{Arc, RwLock};

pub type WorkspaceId = u64;
pub type DocumentId = u64;

#[derive(Debug, Clone)]
pub struct WorkspaceConfig {
    pub root: Option<PathBuf>,
    pub target: CompileTarget,
    /// File extensions to discover during disk index refresh.
    /// Default: `["kn"]`
    pub included_extensions: Vec<String>,
    /// Directory names to skip during disk index traversal.
    /// Default: `[".git", ".kain", "target", "node_modules", ".vs", ".idea"]`
    pub excluded_dirs: Vec<String>,
}

impl Default for WorkspaceConfig {
    fn default() -> Self {
        Self {
            root: None,
            target: CompileTarget::Llvm,
            included_extensions: vec!["kn".to_string()],
            excluded_dirs: vec![
                ".git".to_string(),
                ".kain".to_string(),
                "target".to_string(),
                "node_modules".to_string(),
                ".vs".to_string(),
                ".idea".to_string(),
            ],
        }
    }
}

#[derive(Debug, Clone)]
pub struct OpenDocumentParams {
    pub workspace_id: WorkspaceId,
    pub path: PathBuf,
    pub source: String,
    pub version: i64,
}

#[derive(Debug, Clone)]
pub struct UpdateDocumentParams {
    pub workspace_id: WorkspaceId,
    pub document_id: DocumentId,
    pub source: String,
    pub version: i64,
}

#[derive(Debug, Clone)]
pub struct CloseDocumentParams {
    pub workspace_id: WorkspaceId,
    pub document_id: DocumentId,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ServiceError {
    WorkspaceNotFound,
    DocumentNotFound,
    InvalidPath,
    Io(String),
    LockPoisoned,
}

pub type ServiceResult<T> = Result<T, ServiceError>;

#[derive(Debug, Clone, Default)]
pub struct ServiceHost {
    state: Arc<RwLock<HostState>>,
}

#[derive(Debug, Default)]
struct HostState {
    next_workspace_id: WorkspaceId,
    next_document_id: DocumentId,
    workspaces: HashMap<WorkspaceId, WorkspaceState>,
}

#[derive(Debug)]
struct WorkspaceState {
    config: WorkspaceConfig,
    documents: HashMap<DocumentId, DocumentState>,
    path_to_document: HashMap<String, DocumentId>,
    index: WorkspaceIndex,
}

#[derive(Debug, Clone)]
struct DocumentState {
    path: PathBuf,
    source: String,
    version: i64,
    check: Option<CheckResult>,
}

impl ServiceHost {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn open_workspace(&self, config: WorkspaceConfig) -> ServiceResult<WorkspaceId> {
        let mut state = self.state.write().map_err(|_| ServiceError::LockPoisoned)?;
        state.next_workspace_id += 1;
        let id = state.next_workspace_id;
        state.workspaces.insert(
            id,
            WorkspaceState {
                config,
                documents: HashMap::new(),
                path_to_document: HashMap::new(),
                index: WorkspaceIndex::default(),
            },
        );
        Ok(id)
    }

    pub fn close_workspace(&self, workspace_id: WorkspaceId) -> ServiceResult<()> {
        let mut state = self.state.write().map_err(|_| ServiceError::LockPoisoned)?;
        state
            .workspaces
            .remove(&workspace_id)
            .map(|_| ())
            .ok_or(ServiceError::WorkspaceNotFound)
    }

    pub fn open_document(&self, params: OpenDocumentParams) -> ServiceResult<DocumentId> {
        let mut state = self.state.write().map_err(|_| ServiceError::LockPoisoned)?;
        state.next_document_id += 1;
        let document_id = state.next_document_id;
        let workspace = state
            .workspaces
            .get_mut(&params.workspace_id)
            .ok_or(ServiceError::WorkspaceNotFound)?;
        let path_key = normalize_path_key(&params.path);
        let doc = DocumentState {
            path: params.path,
            source: params.source,
            version: params.version,
            check: None,
        };
        if let Some(analysis) = analyze_document(&doc.path, &doc.source) {
            workspace.index.insert_analysis(analysis);
        }
        workspace.path_to_document.insert(path_key, document_id);
        workspace.documents.insert(document_id, doc);
        Ok(document_id)
    }

    pub fn update_document(&self, params: UpdateDocumentParams) -> ServiceResult<()> {
        let mut state = self.state.write().map_err(|_| ServiceError::LockPoisoned)?;
        let workspace = state
            .workspaces
            .get_mut(&params.workspace_id)
            .ok_or(ServiceError::WorkspaceNotFound)?;
        let doc = workspace
            .documents
            .get_mut(&params.document_id)
            .ok_or(ServiceError::DocumentNotFound)?;
        doc.source = params.source;
        doc.version = params.version;
        doc.check = None;
        let path_key = normalize_path_key(&doc.path);
        workspace.index.remove_path(&path_key);
        if let Some(analysis) = analyze_document(&doc.path, &doc.source) {
            workspace.index.insert_analysis(analysis);
        }
        Ok(())
    }

    pub fn close_document(&self, params: CloseDocumentParams) -> ServiceResult<()> {
        let mut state = self.state.write().map_err(|_| ServiceError::LockPoisoned)?;
        let workspace = state
            .workspaces
            .get_mut(&params.workspace_id)
            .ok_or(ServiceError::WorkspaceNotFound)?;
        let doc = workspace
            .documents
            .remove(&params.document_id)
            .ok_or(ServiceError::DocumentNotFound)?;
        let path_key = normalize_path_key(&doc.path);
        workspace.path_to_document.remove(&path_key);
        workspace.index.remove_path(&path_key);
        Ok(())
    }

    pub fn check_document(
        &self,
        workspace_id: WorkspaceId,
        document_id: DocumentId,
    ) -> ServiceResult<CheckResult> {
        let mut state = self.state.write().map_err(|_| ServiceError::LockPoisoned)?;
        let workspace = state
            .workspaces
            .get_mut(&workspace_id)
            .ok_or(ServiceError::WorkspaceNotFound)?;
        let doc = workspace
            .documents
            .get_mut(&document_id)
            .ok_or(ServiceError::DocumentNotFound)?;
        let result = check_document(&doc.path, &doc.source, workspace.config.target);
        if let Some(analysis) = &result.analysis {
            workspace.index.insert_analysis(analysis.clone());
        }
        doc.check = Some(result.clone());
        Ok(result)
    }

    pub fn check_workspace(&self, workspace_id: WorkspaceId) -> ServiceResult<CheckResult> {
        let mut state = self.state.write().map_err(|_| ServiceError::LockPoisoned)?;
        let workspace = state
            .workspaces
            .get_mut(&workspace_id)
            .ok_or(ServiceError::WorkspaceNotFound)?;
        refresh_disk_index(workspace)?;
        let docs = workspace
            .documents
            .values()
            .map(|doc| (doc.path.as_path(), doc.source.as_str()))
            .collect::<Vec<_>>();
        Ok(check_workspace(docs, workspace.config.target))
    }

    pub fn hover_at(
        &self,
        workspace_id: WorkspaceId,
        document_id: DocumentId,
        line: u32,
        column: u32,
    ) -> ServiceResult<Option<HoverResult>> {
        self.with_doc(workspace_id, document_id, |workspace, doc| {
            queries::hover_at(
                &workspace.index,
                &normalize_path_key(&doc.path),
                &doc.source,
                line,
                column,
            )
        })
    }

    pub fn definition_at(
        &self,
        workspace_id: WorkspaceId,
        document_id: DocumentId,
        line: u32,
        column: u32,
    ) -> ServiceResult<Vec<LocationResult>> {
        self.with_doc(workspace_id, document_id, |workspace, doc| {
            queries::definition_at(
                &workspace.index,
                &normalize_path_key(&doc.path),
                &doc.source,
                line,
                column,
            )
        })
    }

    pub fn references_at(
        &self,
        workspace_id: WorkspaceId,
        document_id: DocumentId,
        line: u32,
        column: u32,
    ) -> ServiceResult<Vec<LocationResult>> {
        self.with_doc(workspace_id, document_id, |workspace, doc| {
            queries::references_at(
                &workspace.index,
                &normalize_path_key(&doc.path),
                &doc.source,
                line,
                column,
            )
        })
    }

    pub fn completions_at(
        &self,
        workspace_id: WorkspaceId,
        document_id: DocumentId,
        line: u32,
        column: u32,
    ) -> ServiceResult<Vec<CompletionResult>> {
        self.with_doc(workspace_id, document_id, |workspace, doc| {
            queries::completions_at(&workspace.index, &doc.source, line, column)
        })
    }

    pub fn document_symbols(
        &self,
        workspace_id: WorkspaceId,
        document_id: DocumentId,
    ) -> ServiceResult<Vec<SymbolResult>> {
        self.with_doc(workspace_id, document_id, |workspace, doc| {
            queries::document_symbols(&workspace.index, &normalize_path_key(&doc.path))
        })
    }

    pub fn workspace_symbols(
        &self,
        workspace_id: WorkspaceId,
        query: &str,
    ) -> ServiceResult<Vec<SymbolResult>> {
        let mut state = self.state.write().map_err(|_| ServiceError::LockPoisoned)?;
        let workspace = state
            .workspaces
            .get_mut(&workspace_id)
            .ok_or(ServiceError::WorkspaceNotFound)?;
        refresh_disk_index(workspace)?;
        Ok(queries::workspace_symbols(&workspace.index, query))
    }

    pub fn semantic_tokens(
        &self,
        workspace_id: WorkspaceId,
        document_id: DocumentId,
    ) -> ServiceResult<Vec<SemanticTokenResult>> {
        self.with_doc(workspace_id, document_id, |workspace, doc| {
            queries::semantic_tokens(&workspace.index, &normalize_path_key(&doc.path))
        })
    }

    pub fn format_document(
        &self,
        workspace_id: WorkspaceId,
        document_id: DocumentId,
    ) -> ServiceResult<FormatResult> {
        self.with_doc(workspace_id, document_id, |workspace, doc| {
            format_document(&doc.path, &doc.source, workspace.config.target)
        })
    }

    fn with_doc<T>(
        &self,
        workspace_id: WorkspaceId,
        document_id: DocumentId,
        f: impl FnOnce(&WorkspaceState, &DocumentState) -> T,
    ) -> ServiceResult<T> {
        let state = self.state.read().map_err(|_| ServiceError::LockPoisoned)?;
        let workspace = state
            .workspaces
            .get(&workspace_id)
            .ok_or(ServiceError::WorkspaceNotFound)?;
        let doc = workspace
            .documents
            .get(&document_id)
            .ok_or(ServiceError::DocumentNotFound)?;
        Ok(f(workspace, doc))
    }
}

fn refresh_disk_index(workspace: &mut WorkspaceState) -> ServiceResult<()> {
    let Some(root) = workspace.config.root.clone() else {
        return Ok(());
    };
    let exts = workspace.config.included_extensions.clone();
    let skip = workspace.config.excluded_dirs.clone();
    for path in collect_kain_files(&root, &exts, &skip)? {
        let key = normalize_path_key(&path);
        if workspace.path_to_document.contains_key(&key) {
            continue;
        }
        let source = fs::read_to_string(&path).map_err(|err| ServiceError::Io(err.to_string()))?;
        if let Some(analysis) = analyze_document(&path, &source) {
            workspace.index.insert_analysis(analysis);
        }
    }
    Ok(())
}

fn collect_kain_files(
    root: &Path,
    extensions: &[String],
    skip_dirs: &[String],
) -> ServiceResult<Vec<PathBuf>> {
    let mut files = Vec::new();
    let mut stack = vec![root.to_path_buf()];
    while let Some(path) = stack.pop() {
        let entries = fs::read_dir(&path).map_err(|err| ServiceError::Io(err.to_string()))?;
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                let name = path
                    .file_name()
                    .and_then(|v| v.to_str())
                    .unwrap_or_default();
                if skip_dirs.iter().any(|d| d == name) {
                    continue;
                }
                stack.push(path);
            } else if let Some(ext) = path.extension().and_then(|e| e.to_str()) {
                if extensions.iter().any(|e| ext.eq_ignore_ascii_case(e)) {
                    files.push(path);
                }
            }
        }
    }
    Ok(files)
}

fn normalize_path_key(path: &Path) -> String {
    path.display().to_string()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn host_queries_open_documents() {
        let host = ServiceHost::new();
        let workspace = host
            .open_workspace(WorkspaceConfig::default())
            .expect("workspace");
        let source =
            "fn helper(v: Int) -> Int:\n    return v\n\nfn main() -> Int:\n    return helper(7)\n";
        let document = host
            .open_document(OpenDocumentParams {
                workspace_id: workspace,
                path: PathBuf::from("main.kn"),
                source: source.to_string(),
                version: 1,
            })
            .expect("document");

        let defs = host
            .definition_at(workspace, document, 4, 13)
            .expect("definitions");
        assert_eq!(defs.len(), 1);
        assert_eq!(defs[0].name, "helper");
    }
}
