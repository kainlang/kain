//! Kain service API.
//!
//! This crate is the compiler-owned service surface for editor and tooling
//! frontends. It exposes a Rust API for in-repo consumers and a flat C ABI for
//! Kain-authored tooling. Transport protocols such as LSP belong above this
//! crate.

pub mod abi;

pub use abi::target_from_code;
pub mod diagnostics;
pub mod formatting;
pub mod index;
pub mod queries;
pub mod workspace;

pub use diagnostics::{check_document, check_workspace, CheckResult};
pub use formatting::{format_document, FormatResult};
pub use index::{
    CompletionKind, CompletionResult, DocumentAnalysis, LocationResult, Position, Range,
    SemanticTokenResult, SymbolKind, SymbolRecord, SymbolResult, WorkspaceIndex,
};
pub use queries::{
    completions_at, definition_at, document_symbols, hover_at, references_at, semantic_tokens,
    workspace_symbols, HoverResult,
};
pub use workspace::{
    CloseDocumentParams, DocumentId, OpenDocumentParams, ServiceError, ServiceHost, ServiceResult,
    UpdateDocumentParams, WorkspaceConfig, WorkspaceId,
};
