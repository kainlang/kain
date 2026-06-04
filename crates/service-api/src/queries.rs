use crate::index::{
    identifier_at, position_to_offset, CompletionResult, LocationResult, SemanticTokenResult,
    SymbolResult, WorkspaceIndex,
};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct HoverResult {
    pub contents: String,
    pub location: LocationResult,
}

pub fn hover_at(
    index: &WorkspaceIndex,
    path: &str,
    source: &str,
    line: u32,
    column: u32,
) -> Option<HoverResult> {
    let name = symbol_name_at(index, path, source, line, column)?;
    let symbol = index.first_symbol(&name)?;
    Some(HoverResult {
        contents: symbol.detail.clone(),
        location: LocationResult {
            path: symbol.path,
            range: symbol.name_range,
            name: symbol.name,
        },
    })
}

pub fn definition_at(
    index: &WorkspaceIndex,
    path: &str,
    source: &str,
    line: u32,
    column: u32,
) -> Vec<LocationResult> {
    symbol_name_at(index, path, source, line, column)
        .map(|name| index.definitions(&name))
        .unwrap_or_default()
}

pub fn references_at(
    index: &WorkspaceIndex,
    path: &str,
    source: &str,
    line: u32,
    column: u32,
) -> Vec<LocationResult> {
    symbol_name_at(index, path, source, line, column)
        .map(|name| index.references(&name))
        .unwrap_or_default()
}

pub fn completions_at(
    index: &WorkspaceIndex,
    source: &str,
    line: u32,
    column: u32,
) -> Vec<CompletionResult> {
    let prefix = completion_prefix(source, line, column).unwrap_or_default();
    index.completions(&prefix)
}

pub fn document_symbols(index: &WorkspaceIndex, path: &str) -> Vec<SymbolResult> {
    index.document_symbols(path)
}

pub fn workspace_symbols(index: &WorkspaceIndex, query: &str) -> Vec<SymbolResult> {
    index.workspace_symbols(query)
}

pub fn semantic_tokens(index: &WorkspaceIndex, path: &str) -> Vec<SemanticTokenResult> {
    index.semantic_tokens(path)
}

fn symbol_name_at(
    index: &WorkspaceIndex,
    path: &str,
    source: &str,
    line: u32,
    column: u32,
) -> Option<String> {
    index.symbol_at(path, source, line, column)
}

fn completion_prefix(source: &str, line: u32, column: u32) -> Option<String> {
    let offset = position_to_offset(source, line, column)?;
    if let Some((_, range)) = identifier_at(source, offset) {
        let start = range.start.offset;
        let end = offset.min(source.len());
        return source.get(start..end).map(str::to_string);
    }
    Some(String::new())
}
