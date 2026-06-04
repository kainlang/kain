use crate::diagnostics::CheckResult;
use crate::formatting::FormatResult;
use crate::index::{
    CompletionResult, LocationResult, Position, Range, SemanticTokenResult, SymbolResult,
};
use crate::queries::HoverResult;
use crate::workspace::{
    CloseDocumentParams, OpenDocumentParams, ServiceError, ServiceHost, UpdateDocumentParams,
    WorkspaceConfig, WorkspaceId,
};
use kain_core::CompileTarget;
use kain_error::label::LabelKind;
use kain_error::{DiagnosticFixIt, DiagnosticLabel, DiagnosticReport, DiagnosticSeverity};
use std::ffi::c_void;
use std::path::PathBuf;
use std::ptr;

const STATUS_OK: u32 = 0;
const STATUS_ERROR: u32 = 1;

#[repr(C)]
pub struct KainServiceWorkspace {
    host: ServiceHost,
    workspace_id: WorkspaceId,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceString {
    pub ptr: *const u8,
    pub len: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct KainServicePosition {
    pub line: u32,
    pub column: u32,
    pub offset: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct KainServiceRange {
    pub start: KainServicePosition,
    pub end: KainServicePosition,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceStatus {
    pub status: u32,
    pub error_code: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceOpenWorkspaceResult {
    pub status: u32,
    pub error_code: u32,
    pub workspace: *mut KainServiceWorkspace,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceDocumentResult {
    pub status: u32,
    pub error_code: u32,
    pub document_id: u64,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceDiagnosticLabel {
    pub message: KainServiceString,
    pub range: KainServiceRange,
    pub primary: bool,
    pub kind: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceDiagnosticFixIt {
    pub message: KainServiceString,
    pub replacement: KainServiceString,
    pub range: KainServiceRange,
    pub primary: bool,
    pub confidence: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceDiagnostic {
    pub code: KainServiceString,
    pub severity: u32,
    pub kind: KainServiceString,
    pub message: KainServiceString,
    pub file: KainServiceString,
    pub has_primary_range: bool,
    pub primary_range: KainServiceRange,
    pub labels: *const KainServiceDiagnosticLabel,
    pub labels_len: usize,
    pub notes: *const KainServiceString,
    pub notes_len: usize,
    pub help: *const KainServiceString,
    pub help_len: usize,
    pub fixits: *const KainServiceDiagnosticFixIt,
    pub fixits_len: usize,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceDiagnosticsResult {
    pub status: u32,
    pub error_code: u32,
    pub diagnostics: *const KainServiceDiagnostic,
    pub diagnostics_len: usize,
    pub typed_program_available: bool,
    pub owner: *mut c_void,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceLocation {
    pub path: KainServiceString,
    pub name: KainServiceString,
    pub range: KainServiceRange,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceLocationsResult {
    pub status: u32,
    pub error_code: u32,
    pub locations: *const KainServiceLocation,
    pub locations_len: usize,
    pub owner: *mut c_void,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceHoverResult {
    pub status: u32,
    pub error_code: u32,
    pub has_hover: bool,
    pub contents: KainServiceString,
    pub location: KainServiceLocation,
    pub owner: *mut c_void,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceCompletion {
    pub label: KainServiceString,
    pub detail: KainServiceString,
    pub kind: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceCompletionsResult {
    pub status: u32,
    pub error_code: u32,
    pub completions: *const KainServiceCompletion,
    pub completions_len: usize,
    pub owner: *mut c_void,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceSymbol {
    pub name: KainServiceString,
    pub detail: KainServiceString,
    pub kind: u32,
    pub location: KainServiceLocation,
    pub container: KainServiceString,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceSymbolsResult {
    pub status: u32,
    pub error_code: u32,
    pub symbols: *const KainServiceSymbol,
    pub symbols_len: usize,
    pub owner: *mut c_void,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceSemanticToken {
    pub range: KainServiceRange,
    pub token_type: u32,
    pub token_modifiers: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceSemanticTokensResult {
    pub status: u32,
    pub error_code: u32,
    pub tokens: *const KainServiceSemanticToken,
    pub tokens_len: usize,
    pub owner: *mut c_void,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct KainServiceFormatResult {
    pub status: u32,
    pub error_code: u32,
    pub formatted: KainServiceString,
    pub already_formatted: bool,
    pub diagnostics: *const KainServiceDiagnostic,
    pub diagnostics_len: usize,
    pub owner: *mut c_void,
}

#[no_mangle]
pub extern "C" fn kain_service_open_workspace(
    root_ptr: *const u8,
    root_len: usize,
    target: u32,
) -> KainServiceOpenWorkspaceResult {
    let host = ServiceHost::new();
    let root = ffi_string(root_ptr, root_len).and_then(|value| {
        if value.is_empty() {
            None
        } else {
            Some(PathBuf::from(value))
        }
    });
    let target = target_from_code(target);
    match host.open_workspace(WorkspaceConfig { root, target, ..Default::default() }) {
        Ok(workspace_id) => KainServiceOpenWorkspaceResult {
            status: STATUS_OK,
            error_code: 0,
            workspace: Box::into_raw(Box::new(KainServiceWorkspace { host, workspace_id })),
        },
        Err(error) => KainServiceOpenWorkspaceResult {
            status: STATUS_ERROR,
            error_code: service_error_code(&error),
            workspace: ptr::null_mut(),
        },
    }
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_close_workspace(
    workspace: *mut KainServiceWorkspace,
) -> KainServiceStatus {
    if workspace.is_null() {
        return status_error(ServiceError::WorkspaceNotFound);
    }
    let boxed = Box::from_raw(workspace);
    let result = boxed.host.close_workspace(boxed.workspace_id);
    status_from_result(result)
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_open_document(
    workspace: *mut KainServiceWorkspace,
    path_ptr: *const u8,
    path_len: usize,
    source_ptr: *const u8,
    source_len: usize,
    version: i64,
) -> KainServiceDocumentResult {
    let Some(handle) = workspace.as_ref() else {
        return document_error(ServiceError::WorkspaceNotFound);
    };
    let Some(path) = ffi_string(path_ptr, path_len) else {
        return document_error(ServiceError::InvalidPath);
    };
    let Some(source) = ffi_string(source_ptr, source_len) else {
        return document_error(ServiceError::InvalidPath);
    };
    match handle.host.open_document(OpenDocumentParams {
        workspace_id: handle.workspace_id,
        path: PathBuf::from(path),
        source,
        version,
    }) {
        Ok(document_id) => KainServiceDocumentResult {
            status: STATUS_OK,
            error_code: 0,
            document_id,
        },
        Err(error) => document_error(error),
    }
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_update_document(
    workspace: *mut KainServiceWorkspace,
    document_id: u64,
    source_ptr: *const u8,
    source_len: usize,
    version: i64,
) -> KainServiceStatus {
    let Some(handle) = workspace.as_ref() else {
        return status_error(ServiceError::WorkspaceNotFound);
    };
    let Some(source) = ffi_string(source_ptr, source_len) else {
        return status_error(ServiceError::InvalidPath);
    };
    status_from_result(handle.host.update_document(UpdateDocumentParams {
        workspace_id: handle.workspace_id,
        document_id,
        source,
        version,
    }))
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_close_document(
    workspace: *mut KainServiceWorkspace,
    document_id: u64,
) -> KainServiceStatus {
    let Some(handle) = workspace.as_ref() else {
        return status_error(ServiceError::WorkspaceNotFound);
    };
    status_from_result(handle.host.close_document(CloseDocumentParams {
        workspace_id: handle.workspace_id,
        document_id,
    }))
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_check_document(
    workspace: *mut KainServiceWorkspace,
    document_id: u64,
) -> KainServiceDiagnosticsResult {
    with_workspace(workspace, |handle| {
        handle
            .host
            .check_document(handle.workspace_id, document_id)
            .map(diagnostics_result)
    })
    .unwrap_or_else(diagnostics_error)
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_hover_at(
    workspace: *mut KainServiceWorkspace,
    document_id: u64,
    line: u32,
    column: u32,
) -> KainServiceHoverResult {
    with_workspace(workspace, |handle| {
        handle
            .host
            .hover_at(handle.workspace_id, document_id, line, column)
            .map(hover_result)
    })
    .unwrap_or_else(hover_error)
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_definition_at(
    workspace: *mut KainServiceWorkspace,
    document_id: u64,
    line: u32,
    column: u32,
) -> KainServiceLocationsResult {
    with_workspace(workspace, |handle| {
        handle
            .host
            .definition_at(handle.workspace_id, document_id, line, column)
            .map(locations_result)
    })
    .unwrap_or_else(locations_error)
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_references_at(
    workspace: *mut KainServiceWorkspace,
    document_id: u64,
    line: u32,
    column: u32,
) -> KainServiceLocationsResult {
    with_workspace(workspace, |handle| {
        handle
            .host
            .references_at(handle.workspace_id, document_id, line, column)
            .map(locations_result)
    })
    .unwrap_or_else(locations_error)
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_completions_at(
    workspace: *mut KainServiceWorkspace,
    document_id: u64,
    line: u32,
    column: u32,
) -> KainServiceCompletionsResult {
    with_workspace(workspace, |handle| {
        handle
            .host
            .completions_at(handle.workspace_id, document_id, line, column)
            .map(completions_result)
    })
    .unwrap_or_else(completions_error)
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_document_symbols(
    workspace: *mut KainServiceWorkspace,
    document_id: u64,
) -> KainServiceSymbolsResult {
    with_workspace(workspace, |handle| {
        handle
            .host
            .document_symbols(handle.workspace_id, document_id)
            .map(symbols_result)
    })
    .unwrap_or_else(symbols_error)
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_workspace_symbols(
    workspace: *mut KainServiceWorkspace,
    query_ptr: *const u8,
    query_len: usize,
) -> KainServiceSymbolsResult {
    let query = ffi_string(query_ptr, query_len).unwrap_or_default();
    with_workspace(workspace, |handle| {
        handle
            .host
            .workspace_symbols(handle.workspace_id, &query)
            .map(symbols_result)
    })
    .unwrap_or_else(symbols_error)
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_semantic_tokens(
    workspace: *mut KainServiceWorkspace,
    document_id: u64,
) -> KainServiceSemanticTokensResult {
    with_workspace(workspace, |handle| {
        handle
            .host
            .semantic_tokens(handle.workspace_id, document_id)
            .map(semantic_tokens_result)
    })
    .unwrap_or_else(semantic_tokens_error)
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_format_document(
    workspace: *mut KainServiceWorkspace,
    document_id: u64,
) -> KainServiceFormatResult {
    with_workspace(workspace, |handle| {
        handle
            .host
            .format_document(handle.workspace_id, document_id)
            .map(format_result)
    })
    .unwrap_or_else(format_error)
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_free_diagnostics_result(
    result: KainServiceDiagnosticsResult,
) {
    free_owner::<OwnedDiagnostics>(result.owner);
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_free_locations_result(result: KainServiceLocationsResult) {
    free_owner::<OwnedLocations>(result.owner);
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_free_hover_result(result: KainServiceHoverResult) {
    free_owner::<OwnedHover>(result.owner);
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_free_completions_result(
    result: KainServiceCompletionsResult,
) {
    free_owner::<OwnedCompletions>(result.owner);
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_free_symbols_result(result: KainServiceSymbolsResult) {
    free_owner::<OwnedSymbols>(result.owner);
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_free_semantic_tokens_result(
    result: KainServiceSemanticTokensResult,
) {
    free_owner::<OwnedSemanticTokens>(result.owner);
}

#[no_mangle]
pub unsafe extern "C" fn kain_service_free_format_result(result: KainServiceFormatResult) {
    free_owner::<OwnedFormat>(result.owner);
}

fn with_workspace<T>(
    workspace: *mut KainServiceWorkspace,
    f: impl FnOnce(&KainServiceWorkspace) -> Result<T, ServiceError>,
) -> Result<T, ServiceError> {
    if workspace.is_null() {
        return Err(ServiceError::WorkspaceNotFound);
    }
    let handle = unsafe { workspace.as_ref() }.ok_or(ServiceError::WorkspaceNotFound)?;
    f(handle)
}

fn ffi_string(ptr: *const u8, len: usize) -> Option<String> {
    if len == 0 {
        return Some(String::new());
    }
    if ptr.is_null() {
        return None;
    }
    let bytes = unsafe { std::slice::from_raw_parts(ptr, len) };
    std::str::from_utf8(bytes).ok().map(str::to_string)
}

pub fn target_from_code(code: u32) -> CompileTarget {
    match code {
        1 => CompileTarget::Wasm,
        2 => CompileTarget::Js,
        3 => CompileTarget::Ts,
        4 => CompileTarget::Hybrid,
        5 => CompileTarget::C,
        6 => CompileTarget::Llvm,
        7 => CompileTarget::Rust,
        8 => CompileTarget::Cpp,
        9 => CompileTarget::Ue5,
        10 => CompileTarget::Ue5Editor,
        11 => CompileTarget::Usf,
        12 => CompileTarget::Spirv,
        13 => CompileTarget::Hlsl,
        14 => CompileTarget::Wgsl,
        15 => CompileTarget::Cuda,
        16 => CompileTarget::Interpret,
        17 => CompileTarget::Test,
        18 => CompileTarget::Ks,
        _ => CompileTarget::Llvm,
    }
}

fn status_from_result(result: Result<(), ServiceError>) -> KainServiceStatus {
    match result {
        Ok(()) => KainServiceStatus {
            status: STATUS_OK,
            error_code: 0,
        },
        Err(error) => status_error(error),
    }
}

fn status_error(error: ServiceError) -> KainServiceStatus {
    KainServiceStatus {
        status: STATUS_ERROR,
        error_code: service_error_code(&error),
    }
}

fn document_error(error: ServiceError) -> KainServiceDocumentResult {
    KainServiceDocumentResult {
        status: STATUS_ERROR,
        error_code: service_error_code(&error),
        document_id: 0,
    }
}

fn service_error_code(error: &ServiceError) -> u32 {
    match error {
        ServiceError::WorkspaceNotFound => 1,
        ServiceError::DocumentNotFound => 2,
        ServiceError::InvalidPath => 3,
        ServiceError::Io(_) => 4,
        ServiceError::LockPoisoned => 5,
    }
}

#[derive(Default)]
struct StringArena {
    strings: Vec<Box<[u8]>>,
}

impl StringArena {
    fn push(&mut self, value: impl AsRef<str>) -> KainServiceString {
        let value = value.as_ref();
        if value.is_empty() {
            return KainServiceString {
                ptr: ptr::null(),
                len: 0,
            };
        }
        let boxed = value.as_bytes().to_vec().into_boxed_slice();
        let ffi = KainServiceString {
            ptr: boxed.as_ptr(),
            len: boxed.len(),
        };
        self.strings.push(boxed);
        ffi
    }
}

struct OwnedDiagnostics {
    arena: StringArena,
    diagnostics: Vec<KainServiceDiagnostic>,
    labels: Vec<Box<[KainServiceDiagnosticLabel]>>,
    notes: Vec<Box<[KainServiceString]>>,
    help: Vec<Box<[KainServiceString]>>,
    fixits: Vec<Box<[KainServiceDiagnosticFixIt]>>,
}

struct OwnedLocations {
    arena: StringArena,
    locations: Vec<KainServiceLocation>,
}

struct OwnedHover {
    arena: StringArena,
}

struct OwnedCompletions {
    arena: StringArena,
    completions: Vec<KainServiceCompletion>,
}

struct OwnedSymbols {
    arena: StringArena,
    symbols: Vec<KainServiceSymbol>,
}

struct OwnedSemanticTokens {
    tokens: Vec<KainServiceSemanticToken>,
}

struct OwnedFormat {
    arena: StringArena,
    diagnostics: Vec<KainServiceDiagnostic>,
    labels: Vec<Box<[KainServiceDiagnosticLabel]>>,
    notes: Vec<Box<[KainServiceString]>>,
    help: Vec<Box<[KainServiceString]>>,
    fixits: Vec<Box<[KainServiceDiagnosticFixIt]>>,
}

fn diagnostics_result(result: CheckResult) -> KainServiceDiagnosticsResult {
    let mut owner = OwnedDiagnostics {
        arena: StringArena::default(),
        diagnostics: Vec::new(),
        labels: Vec::new(),
        notes: Vec::new(),
        help: Vec::new(),
        fixits: Vec::new(),
    };
    owner.diagnostics = diagnostics_to_ffi(
        &mut owner.arena,
        &mut owner.labels,
        &mut owner.notes,
        &mut owner.help,
        &mut owner.fixits,
        &result.diagnostics,
    );
    let diagnostics = owner.diagnostics.as_ptr();
    let diagnostics_len = owner.diagnostics.len();
    KainServiceDiagnosticsResult {
        status: STATUS_OK,
        error_code: 0,
        diagnostics,
        diagnostics_len,
        typed_program_available: result.typed_program.is_some(),
        owner: Box::into_raw(Box::new(owner)) as *mut c_void,
    }
}

fn diagnostics_error(error: ServiceError) -> KainServiceDiagnosticsResult {
    KainServiceDiagnosticsResult {
        status: STATUS_ERROR,
        error_code: service_error_code(&error),
        diagnostics: ptr::null(),
        diagnostics_len: 0,
        typed_program_available: false,
        owner: ptr::null_mut(),
    }
}

fn locations_result(locations: Vec<LocationResult>) -> KainServiceLocationsResult {
    let mut owner = OwnedLocations {
        arena: StringArena::default(),
        locations: Vec::new(),
    };
    owner.locations = locations
        .iter()
        .map(|location| location_to_ffi(&mut owner.arena, location))
        .collect();
    let ptr = owner.locations.as_ptr();
    let len = owner.locations.len();
    KainServiceLocationsResult {
        status: STATUS_OK,
        error_code: 0,
        locations: ptr,
        locations_len: len,
        owner: Box::into_raw(Box::new(owner)) as *mut c_void,
    }
}

fn locations_error(error: ServiceError) -> KainServiceLocationsResult {
    KainServiceLocationsResult {
        status: STATUS_ERROR,
        error_code: service_error_code(&error),
        locations: ptr::null(),
        locations_len: 0,
        owner: ptr::null_mut(),
    }
}

fn hover_result(hover: Option<HoverResult>) -> KainServiceHoverResult {
    let mut owner = OwnedHover {
        arena: StringArena::default(),
    };
    let result = if let Some(hover) = hover {
        KainServiceHoverResult {
            status: STATUS_OK,
            error_code: 0,
            has_hover: true,
            contents: owner.arena.push(&hover.contents),
            location: location_to_ffi(&mut owner.arena, &hover.location),
            owner: ptr::null_mut(),
        }
    } else {
        KainServiceHoverResult {
            status: STATUS_OK,
            error_code: 0,
            has_hover: false,
            contents: empty_string(),
            location: empty_location(),
            owner: ptr::null_mut(),
        }
    };
    KainServiceHoverResult {
        owner: Box::into_raw(Box::new(owner)) as *mut c_void,
        ..result
    }
}

fn hover_error(error: ServiceError) -> KainServiceHoverResult {
    KainServiceHoverResult {
        status: STATUS_ERROR,
        error_code: service_error_code(&error),
        has_hover: false,
        contents: empty_string(),
        location: empty_location(),
        owner: ptr::null_mut(),
    }
}

fn completions_result(completions: Vec<CompletionResult>) -> KainServiceCompletionsResult {
    let mut owner = OwnedCompletions {
        arena: StringArena::default(),
        completions: Vec::new(),
    };
    owner.completions = completions
        .iter()
        .map(|completion| KainServiceCompletion {
            label: owner.arena.push(&completion.label),
            detail: owner.arena.push(&completion.detail),
            kind: completion.kind.code(),
        })
        .collect();
    let ptr = owner.completions.as_ptr();
    let len = owner.completions.len();
    KainServiceCompletionsResult {
        status: STATUS_OK,
        error_code: 0,
        completions: ptr,
        completions_len: len,
        owner: Box::into_raw(Box::new(owner)) as *mut c_void,
    }
}

fn completions_error(error: ServiceError) -> KainServiceCompletionsResult {
    KainServiceCompletionsResult {
        status: STATUS_ERROR,
        error_code: service_error_code(&error),
        completions: ptr::null(),
        completions_len: 0,
        owner: ptr::null_mut(),
    }
}

fn symbols_result(symbols: Vec<SymbolResult>) -> KainServiceSymbolsResult {
    let mut owner = OwnedSymbols {
        arena: StringArena::default(),
        symbols: Vec::new(),
    };
    owner.symbols = symbols
        .iter()
        .map(|symbol| KainServiceSymbol {
            name: owner.arena.push(&symbol.name),
            detail: owner.arena.push(&symbol.detail),
            kind: symbol.kind.code(),
            location: location_to_ffi(&mut owner.arena, &symbol.location),
            container: owner
                .arena
                .push(symbol.container.as_deref().unwrap_or_default()),
        })
        .collect();
    let ptr = owner.symbols.as_ptr();
    let len = owner.symbols.len();
    KainServiceSymbolsResult {
        status: STATUS_OK,
        error_code: 0,
        symbols: ptr,
        symbols_len: len,
        owner: Box::into_raw(Box::new(owner)) as *mut c_void,
    }
}

fn symbols_error(error: ServiceError) -> KainServiceSymbolsResult {
    KainServiceSymbolsResult {
        status: STATUS_ERROR,
        error_code: service_error_code(&error),
        symbols: ptr::null(),
        symbols_len: 0,
        owner: ptr::null_mut(),
    }
}

fn semantic_tokens_result(tokens: Vec<SemanticTokenResult>) -> KainServiceSemanticTokensResult {
    let owner = OwnedSemanticTokens {
        tokens: tokens
            .iter()
            .map(|token| KainServiceSemanticToken {
                range: range_to_ffi(token.range),
                token_type: token.token_type,
                token_modifiers: token.token_modifiers,
            })
            .collect(),
    };
    let ptr = owner.tokens.as_ptr();
    let len = owner.tokens.len();
    KainServiceSemanticTokensResult {
        status: STATUS_OK,
        error_code: 0,
        tokens: ptr,
        tokens_len: len,
        owner: Box::into_raw(Box::new(owner)) as *mut c_void,
    }
}

fn semantic_tokens_error(error: ServiceError) -> KainServiceSemanticTokensResult {
    KainServiceSemanticTokensResult {
        status: STATUS_ERROR,
        error_code: service_error_code(&error),
        tokens: ptr::null(),
        tokens_len: 0,
        owner: ptr::null_mut(),
    }
}

fn format_result(result: FormatResult) -> KainServiceFormatResult {
    let mut owner = OwnedFormat {
        arena: StringArena::default(),
        diagnostics: Vec::new(),
        labels: Vec::new(),
        notes: Vec::new(),
        help: Vec::new(),
        fixits: Vec::new(),
    };
    let formatted = owner.arena.push(&result.formatted);
    owner.diagnostics = diagnostics_to_ffi(
        &mut owner.arena,
        &mut owner.labels,
        &mut owner.notes,
        &mut owner.help,
        &mut owner.fixits,
        &result.diagnostics,
    );
    let diagnostics = owner.diagnostics.as_ptr();
    let diagnostics_len = owner.diagnostics.len();
    KainServiceFormatResult {
        status: STATUS_OK,
        error_code: 0,
        formatted,
        already_formatted: result.already_formatted,
        diagnostics,
        diagnostics_len,
        owner: Box::into_raw(Box::new(owner)) as *mut c_void,
    }
}

fn format_error(error: ServiceError) -> KainServiceFormatResult {
    KainServiceFormatResult {
        status: STATUS_ERROR,
        error_code: service_error_code(&error),
        formatted: empty_string(),
        already_formatted: false,
        diagnostics: ptr::null(),
        diagnostics_len: 0,
        owner: ptr::null_mut(),
    }
}

fn diagnostics_to_ffi(
    arena: &mut StringArena,
    label_storage: &mut Vec<Box<[KainServiceDiagnosticLabel]>>,
    note_storage: &mut Vec<Box<[KainServiceString]>>,
    help_storage: &mut Vec<Box<[KainServiceString]>>,
    fixit_storage: &mut Vec<Box<[KainServiceDiagnosticFixIt]>>,
    reports: &[DiagnosticReport],
) -> Vec<KainServiceDiagnostic> {
    reports
        .iter()
        .map(|report| {
            let labels = report
                .labels
                .iter()
                .map(|label| label_to_ffi(arena, label))
                .collect::<Vec<_>>()
                .into_boxed_slice();
            let notes = report
                .notes
                .iter()
                .map(|note| arena.push(note))
                .collect::<Vec<_>>()
                .into_boxed_slice();
            let help = report
                .help
                .iter()
                .map(|item| arena.push(item))
                .collect::<Vec<_>>()
                .into_boxed_slice();
            let fixits = report
                .fixits
                .iter()
                .map(|fixit| fixit_to_ffi(arena, fixit))
                .collect::<Vec<_>>()
                .into_boxed_slice();

            let labels_ptr = labels.as_ptr();
            let labels_len = labels.len();
            let notes_ptr = notes.as_ptr();
            let notes_len = notes.len();
            let help_ptr = help.as_ptr();
            let help_len = help.len();
            let fixits_ptr = fixits.as_ptr();
            let fixits_len = fixits.len();
            label_storage.push(labels);
            note_storage.push(notes);
            help_storage.push(help);
            fixit_storage.push(fixits);

            KainServiceDiagnostic {
                code: arena.push(report.code.as_str()),
                severity: severity_code(report.severity),
                kind: arena.push(report.kind.to_string()),
                message: arena.push(&report.message),
                file: arena.push(
                    report
                        .file
                        .as_ref()
                        .map(|path| path.display().to_string())
                        .or_else(|| report.origin.clone())
                        .unwrap_or_default(),
                ),
                has_primary_range: report.primary_range.is_some(),
                primary_range: report
                    .primary_range
                    .as_ref()
                    .map(source_range_to_ffi)
                    .unwrap_or_default(),
                labels: labels_ptr,
                labels_len,
                notes: notes_ptr,
                notes_len,
                help: help_ptr,
                help_len,
                fixits: fixits_ptr,
                fixits_len,
            }
        })
        .collect()
}

fn label_to_ffi(arena: &mut StringArena, label: &DiagnosticLabel) -> KainServiceDiagnosticLabel {
    KainServiceDiagnosticLabel {
        message: arena.push(&label.message),
        range: label
            .range
            .as_ref()
            .map(source_range_to_ffi)
            .unwrap_or_default(),
        primary: label.primary,
        kind: label_kind_code(label.kind),
    }
}

fn fixit_to_ffi(arena: &mut StringArena, fixit: &DiagnosticFixIt) -> KainServiceDiagnosticFixIt {
    KainServiceDiagnosticFixIt {
        message: arena.push(&fixit.message),
        replacement: arena.push(&fixit.replacement),
        range: fixit
            .range
            .as_ref()
            .map(source_range_to_ffi)
            .unwrap_or_default(),
        primary: fixit.primary,
        confidence: fixit_confidence_code(&fixit.confidence),
    }
}

fn location_to_ffi(arena: &mut StringArena, location: &LocationResult) -> KainServiceLocation {
    KainServiceLocation {
        path: arena.push(&location.path),
        name: arena.push(&location.name),
        range: range_to_ffi(location.range),
    }
}

fn range_to_ffi(range: Range) -> KainServiceRange {
    KainServiceRange {
        start: position_to_ffi(range.start),
        end: position_to_ffi(range.end),
    }
}

fn position_to_ffi(position: Position) -> KainServicePosition {
    KainServicePosition {
        line: position.line,
        column: position.column,
        offset: position.offset,
    }
}

fn source_range_to_ffi(range: &kain_error::SourceRange) -> KainServiceRange {
    KainServiceRange {
        start: KainServicePosition {
            line: range.start.line as u32,
            column: range.start.col as u32,
            offset: range.start.offset,
        },
        end: KainServicePosition {
            line: range.end.line as u32,
            column: range.end.col as u32,
            offset: range.end.offset,
        },
    }
}

fn empty_location() -> KainServiceLocation {
    KainServiceLocation {
        path: empty_string(),
        name: empty_string(),
        range: KainServiceRange::default(),
    }
}

fn empty_string() -> KainServiceString {
    KainServiceString {
        ptr: ptr::null(),
        len: 0,
    }
}

fn severity_code(severity: DiagnosticSeverity) -> u32 {
    match severity {
        DiagnosticSeverity::Error => 1,
        DiagnosticSeverity::Warning => 2,
        DiagnosticSeverity::Note => 3,
        DiagnosticSeverity::Help => 4,
    }
}

fn label_kind_code(kind: LabelKind) -> u32 {
    match kind {
        LabelKind::Annotation => 0,
        LabelKind::Origin => 1,
        LabelKind::Definition => 2,
        LabelKind::BorrowEnd => 3,
        LabelKind::MovedHere => 4,
        LabelKind::RequiredBy => 5,
    }
}

fn fixit_confidence_code(confidence: &impl std::fmt::Debug) -> u32 {
    match format!("{confidence:?}").as_str() {
        "Certain" => 1,
        "Likely" => 2,
        "Tentative" => 3,
        _ => 0,
    }
}

unsafe fn free_owner<T>(owner: *mut c_void) {
    if !owner.is_null() {
        drop(Box::from_raw(owner as *mut T));
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn abi_open_query_and_free_roundtrip() {
        let source =
            b"fn helper(v: Int) -> Int:\n    return v\n\nfn main() -> Int:\n    return helper(3)\n";
        let path = b"main.kn";
        let workspace = kain_service_open_workspace(ptr::null(), 0, 6);
        assert_eq!(workspace.status, STATUS_OK);
        assert!(!workspace.workspace.is_null());

        let doc = unsafe {
            kain_service_open_document(
                workspace.workspace,
                path.as_ptr(),
                path.len(),
                source.as_ptr(),
                source.len(),
                1,
            )
        };
        assert_eq!(doc.status, STATUS_OK);

        let defs =
            unsafe { kain_service_definition_at(workspace.workspace, doc.document_id, 4, 13) };
        assert_eq!(defs.status, STATUS_OK);
        assert_eq!(defs.locations_len, 1);
        unsafe {
            kain_service_free_locations_result(defs);
            let status = kain_service_close_workspace(workspace.workspace);
            assert_eq!(status.status, STATUS_OK);
        }
    }
}
