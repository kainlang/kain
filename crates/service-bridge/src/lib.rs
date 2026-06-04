//! Thin bridge registering kain-service-api functions into the Kain stdlib runtime.
//!
//! This crate bridges between the Rust service-api types and Kain's `Value` runtime
//! type system. Functions are registered via `register_stdlib_extension` (metadata)
//! and `register_env_extension` (implementations), then called from `stdlib/kain.kn`.

use kain_core::error::{KainError, KainResult};
use kain_core::runtime::{register_env_extension, Env, Value};
use kain_core::stdlib::{register_stdlib_extension, BuiltinFn, StdLib};
use kain_error::DiagnosticSeverity;
use kain_service_api::{
    target_from_code, CloseDocumentParams, OpenDocumentParams, ServiceError, ServiceHost,
    UpdateDocumentParams, WorkspaceConfig,
};
use once_cell::sync::Lazy;
use std::collections::HashMap;
use std::path::PathBuf;
use std::sync::{Arc, Mutex, Once, RwLock};

const EXTENSION_KEY: &str = "kain.service.bridge";

static REGISTER: Once = Once::new();

/// Global service host. A single host manages all workspaces.
static HOST: Lazy<Mutex<ServiceHost>> = Lazy::new(|| Mutex::new(ServiceHost::new()));

// ── Registration entry point ──────────────────────────────────────────

pub fn register() {
    REGISTER.call_once(|| {
        register_stdlib_extension(EXTENSION_KEY, register_service_stdlib);
        register_env_extension(EXTENSION_KEY, register_service_env);
    });
}

// ── Stdlib metadata ───────────────────────────────────────────────────

fn register_service_stdlib(stdlib: &mut StdLib) {
    let builtins: &[BuiltinFn] = &[
        BuiltinFn {
            name: "kain_service_open_workspace",
            params: vec![("root", "String"), ("target", "Int")],
            return_type: "Any",
            doc: "Open a workspace rooted at the given path with the given compile target code. Returns a struct with status and workspace_id.",
        },
        BuiltinFn {
            name: "kain_service_close_workspace",
            params: vec![("workspace_id", "Int")],
            return_type: "Any",
            doc: "Close a workspace and all its documents. Returns a struct with status.",
        },
        BuiltinFn {
            name: "kain_service_open_document",
            params: vec![
                ("workspace_id", "Int"),
                ("path", "String"),
                ("source", "String"),
                ("version", "Int"),
            ],
            return_type: "Any",
            doc: "Open a document in a workspace. Returns a struct with status and document_id.",
        },
        BuiltinFn {
            name: "kain_service_update_document",
            params: vec![
                ("workspace_id", "Int"),
                ("document_id", "Int"),
                ("source", "String"),
                ("version", "Int"),
            ],
            return_type: "Any",
            doc: "Update a document's source content. Returns a struct with status.",
        },
        BuiltinFn {
            name: "kain_service_close_document",
            params: vec![("workspace_id", "Int"), ("document_id", "Int")],
            return_type: "Any",
            doc: "Close a document. Returns a struct with status.",
        },
        BuiltinFn {
            name: "kain_service_check_document",
            params: vec![("workspace_id", "Int"), ("document_id", "Int")],
            return_type: "Any",
            doc: "Check a document and return diagnostics. Returns a struct with status, diagnostics array, and typed_program_available.",
        },
        BuiltinFn {
            name: "kain_service_hover_at",
            params: vec![
                ("workspace_id", "Int"),
                ("document_id", "Int"),
                ("line", "Int"),
                ("column", "Int"),
            ],
            return_type: "Any",
            doc: "Get hover information at a position. Returns a struct with status, has_hover, contents, and location.",
        },
        BuiltinFn {
            name: "kain_service_definition_at",
            params: vec![
                ("workspace_id", "Int"),
                ("document_id", "Int"),
                ("line", "Int"),
                ("column", "Int"),
            ],
            return_type: "Any",
            doc: "Go to definition. Returns a struct with status and locations array.",
        },
        BuiltinFn {
            name: "kain_service_references_at",
            params: vec![
                ("workspace_id", "Int"),
                ("document_id", "Int"),
                ("line", "Int"),
                ("column", "Int"),
            ],
            return_type: "Any",
            doc: "Find references. Returns a struct with status and locations array.",
        },
        BuiltinFn {
            name: "kain_service_completions_at",
            params: vec![
                ("workspace_id", "Int"),
                ("document_id", "Int"),
                ("line", "Int"),
                ("column", "Int"),
            ],
            return_type: "Any",
            doc: "Get completions at a position. Returns a struct with status and completions array.",
        },
        BuiltinFn {
            name: "kain_service_document_symbols",
            params: vec![("workspace_id", "Int"), ("document_id", "Int")],
            return_type: "Any",
            doc: "Get document symbols. Returns a struct with status and symbols array.",
        },
        BuiltinFn {
            name: "kain_service_workspace_symbols",
            params: vec![("workspace_id", "Int"), ("query", "String")],
            return_type: "Any",
            doc: "Search workspace symbols. Returns a struct with status and symbols array.",
        },
        BuiltinFn {
            name: "kain_service_semantic_tokens",
            params: vec![("workspace_id", "Int"), ("document_id", "Int")],
            return_type: "Any",
            doc: "Get semantic tokens for a document. Returns a struct with status and tokens array.",
        },
        BuiltinFn {
            name: "kain_service_format_document",
            params: vec![("workspace_id", "Int"), ("document_id", "Int")],
            return_type: "Any",
            doc: "Format a document. Returns a struct with status, formatted, already_formatted, and diagnostics.",
        },
    ];

    for builtin in builtins {
        stdlib.functions.insert(
            builtin.name.to_string(),
            BuiltinFn {
                name: builtin.name,
                params: builtin.params.clone(),
                return_type: builtin.return_type,
                doc: builtin.doc,
            },
        );
    }
}

// ── Native function implementations ──────────────────────────────────

fn register_service_env(env: &mut Env) {
    env.register_native_fn("kain_service_open_workspace", builtin_open_workspace);
    env.register_native_fn("kain_service_close_workspace", builtin_close_workspace);
    env.register_native_fn("kain_service_open_document", builtin_open_document);
    env.register_native_fn("kain_service_update_document", builtin_update_document);
    env.register_native_fn("kain_service_close_document", builtin_close_document);
    env.register_native_fn("kain_service_check_document", builtin_check_document);
    env.register_native_fn("kain_service_hover_at", builtin_hover_at);
    env.register_native_fn("kain_service_definition_at", builtin_definition_at);
    env.register_native_fn("kain_service_references_at", builtin_references_at);
    env.register_native_fn("kain_service_completions_at", builtin_completions_at);
    env.register_native_fn("kain_service_document_symbols", builtin_document_symbols);
    env.register_native_fn("kain_service_workspace_symbols", builtin_workspace_symbols);
    env.register_native_fn("kain_service_semantic_tokens", builtin_semantic_tokens);
    env.register_native_fn("kain_service_format_document", builtin_format_document);
}

// ── Helpers ──────────────────────────────────────────────────────────

fn extract_int(args: &[Value], index: usize, name: &str) -> KainResult<i64> {
    match args.get(index) {
        Some(Value::Int(v)) => Ok(*v),
        Some(other) => Err(KainError::runtime(format!(
            "{name}: expected Int at arg {index}, got {other:?}"
        ))),
        None => Err(KainError::runtime(format!(
            "{name}: missing arg {index}"
        ))),
    }
}

fn extract_u64(args: &[Value], index: usize, name: &str) -> KainResult<u64> {
    extract_int(args, index, name).map(|v| v as u64)
}

fn extract_u32(args: &[Value], index: usize, name: &str) -> KainResult<u32> {
    extract_int(args, index, name).map(|v| v as u32)
}

fn extract_string(args: &[Value], index: usize, name: &str) -> KainResult<String> {
    match args.get(index) {
        Some(Value::String(s)) => Ok(s.clone()),
        Some(other) => Err(KainError::runtime(format!(
            "{name}: expected String at arg {index}, got {other:?}"
        ))),
        None => Err(KainError::runtime(format!(
            "{name}: missing arg {index}"
        ))),
    }
}

fn status_struct(status: u32, error_code: u32) -> Value {
    struct_with(vec![
        ("status", Value::Int(status as i64)),
        ("error_code", Value::Int(error_code as i64)),
    ])
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

fn struct_with(fields: Vec<(&str, Value)>) -> Value {
    let map: HashMap<String, Value> =
        fields.into_iter().map(|(k, v)| (k.to_string(), v)).collect();
    Value::Struct("ServiceResult".to_string(), Arc::new(RwLock::new(map)))
}

fn value_array(items: Vec<Value>) -> Value {
    Value::Array(Arc::new(RwLock::new(items)))
}

// ── Value conversion helpers ─────────────────────────────────────────

fn position_value(line: u32, column: u32, offset: usize) -> Value {
    struct_with(vec![
        ("line", Value::Int(line as i64)),
        ("column", Value::Int(column as i64)),
        ("offset", Value::Int(offset as i64)),
    ])
}

fn range_value(start_line: u32, start_col: u32, start_offset: usize, end_line: u32, end_col: u32, end_offset: usize) -> Value {
    struct_with(vec![
        ("start", position_value(start_line, start_col, start_offset)),
        ("end", position_value(end_line, end_col, end_offset)),
    ])
}

fn location_value(path: &str, name: &str, loc: &kain_service_api::LocationResult) -> Value {
    struct_with(vec![
        ("path", Value::String(path.to_string())),
        ("name", Value::String(name.to_string())),
        ("range", range_value(
            loc.range.start.line,
            loc.range.start.column,
            loc.range.start.offset,
            loc.range.end.line,
            loc.range.end.column,
            loc.range.end.offset,
        )),
    ])
}

fn diagnostic_value(
    code: &str,
    severity: u32,
    kind: &str,
    message: &str,
    file: &str,
    has_primary_range: bool,
    primary_start_line: u32,
    primary_start_col: u32,
    primary_start_offset: usize,
    primary_end_line: u32,
    primary_end_col: u32,
    primary_end_offset: usize,
    labels: Vec<Value>,
    notes: Vec<Value>,
    help: Vec<Value>,
    fixits: Vec<Value>,
) -> Value {
    struct_with(vec![
        ("code", Value::String(code.to_string())),
        ("severity", Value::Int(severity as i64)),
        ("kind", Value::String(kind.to_string())),
        ("message", Value::String(message.to_string())),
        ("file", Value::String(file.to_string())),
        ("has_primary_range", Value::Bool(has_primary_range)),
        ("primary_range", range_value(
            primary_start_line, primary_start_col, primary_start_offset,
            primary_end_line, primary_end_col, primary_end_offset,
        )),
        ("labels", value_array(labels)),
        ("notes", value_array(notes)),
        ("help", value_array(help)),
        ("fixits", value_array(fixits)),
    ])
}

fn symbol_value(
    name: &str,
    detail: &str,
    kind_code: u32,
    path: &str,
    name_range_start_line: u32,
    name_range_start_col: u32,
    name_range_start_offset: usize,
    name_range_end_line: u32,
    name_range_end_col: u32,
    name_range_end_offset: usize,
    container: Option<&str>,
) -> Value {
    struct_with(vec![
        ("name", Value::String(name.to_string())),
        ("detail", Value::String(detail.to_string())),
        ("kind", Value::Int(kind_code as i64)),
        ("location", struct_with(vec![
            ("path", Value::String(path.to_string())),
            ("name", Value::String(name.to_string())),
            ("range", range_value(
                name_range_start_line, name_range_start_col, name_range_start_offset,
                name_range_end_line, name_range_end_col, name_range_end_offset,
            )),
        ])),
        ("container", match container {
            Some(c) => Value::String(c.to_string()),
            None => Value::None,
        }),
    ])
}

fn completion_value(label: &str, detail: &str, kind_code: u32) -> Value {
    struct_with(vec![
        ("label", Value::String(label.to_string())),
        ("detail", Value::String(detail.to_string())),
        ("kind", Value::Int(kind_code as i64)),
    ])
}

fn semantic_token_value(
    start_line: u32, start_col: u32, start_offset: usize,
    end_line: u32, end_col: u32, end_offset: usize,
    token_type: u32, token_modifiers: u32,
) -> Value {
    struct_with(vec![
        ("range", range_value(start_line, start_col, start_offset, end_line, end_col, end_offset)),
        ("token_type", Value::Int(token_type as i64)),
        ("token_modifiers", Value::Int(token_modifiers as i64)),
    ])
}

fn hover_value(contents: &str, path: &str, name: &str, loc: &kain_service_api::LocationResult) -> Value {
    struct_with(vec![
        ("contents", Value::String(contents.to_string())),
        ("location", location_value(path, name, loc)),
    ])
}

// ── Builtin implementations ──────────────────────────────────────────

fn builtin_open_workspace(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let root = extract_string(&args, 0, "kain_service_open_workspace")?;
    let target_code = extract_u64(&args, 1, "kain_service_open_workspace")?;

    let root_path = if root.is_empty() {
        None
    } else {
        Some(PathBuf::from(&root))
    };

    let config = WorkspaceConfig {
        root: root_path,
        target: target_from_code(target_code as u32),
        ..Default::default()
    };

    let mut host = HOST.lock().map_err(|_| {
        KainError::runtime("kain_service_open_workspace: lock poisoned")
    })?;

    match host.open_workspace(config) {
        Ok(workspace_id) => Ok(struct_with(vec![
            ("status", Value::Int(0)),
            ("error_code", Value::Int(0)),
            ("workspace_id", Value::Int(workspace_id as i64)),
        ])),
        Err(error) => Ok(status_struct(1, service_error_code(&error))),
    }
}

fn builtin_close_workspace(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let workspace_id = extract_u64(&args, 0, "kain_service_close_workspace")?;

    let mut host = HOST.lock().map_err(|_| {
        KainError::runtime("kain_service_close_workspace: lock poisoned")
    })?;

    match host.close_workspace(workspace_id) {
        Ok(()) => Ok(status_struct(0, 0)),
        Err(error) => Ok(status_struct(1, service_error_code(&error))),
    }
}

fn builtin_open_document(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let workspace_id = extract_u64(&args, 0, "kain_service_open_document")?;
    let path = extract_string(&args, 1, "kain_service_open_document")?;
    let source = extract_string(&args, 2, "kain_service_open_document")?;
    let version = extract_int(&args, 3, "kain_service_open_document")?;

    let mut host = HOST.lock().map_err(|_| {
        KainError::runtime("kain_service_open_document: lock poisoned")
    })?;

    match host.open_document(OpenDocumentParams {
        workspace_id,
        path: PathBuf::from(&path),
        source,
        version,
    }) {
        Ok(document_id) => Ok(struct_with(vec![
            ("status", Value::Int(0)),
            ("error_code", Value::Int(0)),
            ("document_id", Value::Int(document_id as i64)),
        ])),
        Err(error) => {
            let code = service_error_code(&error);
            Ok(struct_with(vec![
                ("status", Value::Int(1)),
                ("error_code", Value::Int(code as i64)),
                ("document_id", Value::Int(0)),
            ]))
        }
    }
}

fn builtin_update_document(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let workspace_id = extract_u64(&args, 0, "kain_service_update_document")?;
    let document_id = extract_u64(&args, 1, "kain_service_update_document")?;
    let source = extract_string(&args, 2, "kain_service_update_document")?;
    let version = extract_int(&args, 3, "kain_service_update_document")?;

    let mut host = HOST.lock().map_err(|_| {
        KainError::runtime("kain_service_update_document: lock poisoned")
    })?;

    match host.update_document(UpdateDocumentParams {
        workspace_id,
        document_id,
        source,
        version,
    }) {
        Ok(()) => Ok(status_struct(0, 0)),
        Err(error) => Ok(status_struct(1, service_error_code(&error))),
    }
}

fn builtin_close_document(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let workspace_id = extract_u64(&args, 0, "kain_service_close_document")?;
    let document_id = extract_u64(&args, 1, "kain_service_close_document")?;

    let mut host = HOST.lock().map_err(|_| {
        KainError::runtime("kain_service_close_document: lock poisoned")
    })?;

    match host.close_document(CloseDocumentParams {
        workspace_id,
        document_id,
    }) {
        Ok(()) => Ok(status_struct(0, 0)),
        Err(error) => Ok(status_struct(1, service_error_code(&error))),
    }
}

fn builtin_check_document(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let workspace_id = extract_u64(&args, 0, "kain_service_check_document")?;
    let document_id = extract_u64(&args, 1, "kain_service_check_document")?;

    let mut host = HOST.lock().map_err(|_| {
        KainError::runtime("kain_service_check_document: lock poisoned")
    })?;

    match host.check_document(workspace_id, document_id) {
        Ok(check) => {
            let diagnostics: Vec<Value> = check
                .diagnostics
                .iter()
                .map(|report| {
                    let labels: Vec<Value> = report
                        .labels
                        .iter()
                        .map(|label| {
                            let (sl, sc, so, el, ec, eo) = label
                                .range
                                .as_ref()
                                .map(|r| {
                                    (
                                        r.start.line as u32,
                                        r.start.col as u32,
                                        r.start.offset,
                                        r.end.line as u32,
                                        r.end.col as u32,
                                        r.end.offset,
                                    )
                                })
                                .unwrap_or((0, 0, 0, 0, 0, 0));
                            struct_with(vec![
                                ("message", Value::String(label.message.clone())),
                                ("range", range_value(sl, sc, so, el, ec, eo)),
                                ("primary", Value::Bool(label.primary)),
                                ("kind", Value::Int(0)), // simplified: LabelKind as u32
                            ])
                        })
                        .collect();

                    let notes: Vec<Value> = report
                        .notes
                        .iter()
                        .map(|n| Value::String(n.clone()))
                        .collect();

                    let help: Vec<Value> = report
                        .help
                        .iter()
                        .map(|h| Value::String(h.clone()))
                        .collect();

                    let fixits: Vec<Value> = report
                        .fixits
                        .iter()
                        .map(|fixit| {
                            let (sl, sc, so, el, ec, eo) = fixit
                                .range
                                .as_ref()
                                .map(|r| {
                                    (
                                        r.start.line as u32,
                                        r.start.col as u32,
                                        r.start.offset,
                                        r.end.line as u32,
                                        r.end.col as u32,
                                        r.end.offset,
                                    )
                                })
                                .unwrap_or((0, 0, 0, 0, 0, 0));
                            struct_with(vec![
                                ("message", Value::String(fixit.message.clone())),
                                ("replacement", Value::String(fixit.replacement.clone())),
                                ("range", range_value(sl, sc, so, el, ec, eo)),
                                ("primary", Value::Bool(fixit.primary)),
                                ("confidence", Value::Int(1)), // simplified
                            ])
                        })
                        .collect();

                    let (psl, psc, pso, pel, pec, peo) = report
                        .primary_range
                        .as_ref()
                        .map(|r| {
                            (
                                r.start.line as u32,
                                r.start.col as u32,
                                r.start.offset,
                                r.end.line as u32,
                                r.end.col as u32,
                                r.end.offset,
                            )
                        })
                        .unwrap_or((0, 0, 0, 0, 0, 0));

                    diagnostic_value(
                        report.code.as_str(),
                        severity_code(report.severity),
                        &report.kind.to_string(),
                        &report.message,
                        report
                            .file
                            .as_ref()
                            .map(|p| p.display().to_string())
                            .or_else(|| report.origin.clone())
                            .unwrap_or_default()
                            .as_str(),
                        report.primary_range.is_some(),
                        psl, psc, pso, pel, pec, peo,
                        labels,
                        notes,
                        help,
                        fixits,
                    )
                })
                .collect();

            Ok(struct_with(vec![
                ("status", Value::Int(0)),
                ("error_code", Value::Int(0)),
                ("diagnostics", value_array(diagnostics)),
                ("typed_program_available", Value::Bool(check.typed_program.is_some())),
            ]))
        }
        Err(error) => Ok(struct_with(vec![
            ("status", Value::Int(1)),
            ("error_code", Value::Int(service_error_code(&error) as i64)),
            ("diagnostics", value_array(vec![])),
            ("typed_program_available", Value::Bool(false)),
        ])),
    }
}

fn builtin_hover_at(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let workspace_id = extract_u64(&args, 0, "kain_service_hover_at")?;
    let document_id = extract_u64(&args, 1, "kain_service_hover_at")?;
    let line = extract_u32(&args, 2, "kain_service_hover_at")?;
    let column = extract_u32(&args, 3, "kain_service_hover_at")?;

    let mut host = HOST.lock().map_err(|_| {
        KainError::runtime("kain_service_hover_at: lock poisoned")
    })?;

    match host.hover_at(workspace_id, document_id, line, column) {
        Ok(Some(hover)) => Ok(struct_with(vec![
            ("status", Value::Int(0)),
            ("error_code", Value::Int(0)),
            ("has_hover", Value::Bool(true)),
            ("contents", Value::String(hover.contents)),
            ("location", location_value(&hover.location.path, &hover.location.name, &hover.location)),
        ])),
        Ok(None) => Ok(struct_with(vec![
            ("status", Value::Int(0)),
            ("error_code", Value::Int(0)),
            ("has_hover", Value::Bool(false)),
            ("contents", Value::String(String::new())),
            ("location", struct_with(vec![
                ("path", Value::String(String::new())),
                ("name", Value::String(String::new())),
                ("range", range_value(0, 0, 0, 0, 0, 0)),
            ])),
        ])),
        Err(error) => Ok(struct_with(vec![
            ("status", Value::Int(1)),
            ("error_code", Value::Int(service_error_code(&error) as i64)),
            ("has_hover", Value::Bool(false)),
            ("contents", Value::String(String::new())),
            ("location", struct_with(vec![
                ("path", Value::String(String::new())),
                ("name", Value::String(String::new())),
                ("range", range_value(0, 0, 0, 0, 0, 0)),
            ])),
        ])),
    }
}

fn builtin_definition_at(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let workspace_id = extract_u64(&args, 0, "kain_service_definition_at")?;
    let document_id = extract_u64(&args, 1, "kain_service_definition_at")?;
    let line = extract_u32(&args, 2, "kain_service_definition_at")?;
    let column = extract_u32(&args, 3, "kain_service_definition_at")?;

    let mut host = HOST.lock().map_err(|_| {
        KainError::runtime("kain_service_definition_at: lock poisoned")
    })?;

    match host.definition_at(workspace_id, document_id, line, column) {
        Ok(locations) => {
            let loc_values: Vec<Value> = locations
                .iter()
                .map(|loc| location_value(&loc.path, &loc.name, loc))
                .collect();
            Ok(struct_with(vec![
                ("status", Value::Int(0)),
                ("error_code", Value::Int(0)),
                ("locations", value_array(loc_values)),
            ]))
        }
        Err(error) => Ok(struct_with(vec![
            ("status", Value::Int(1)),
            ("error_code", Value::Int(service_error_code(&error) as i64)),
            ("locations", value_array(vec![])),
        ])),
    }
}

fn builtin_references_at(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let workspace_id = extract_u64(&args, 0, "kain_service_references_at")?;
    let document_id = extract_u64(&args, 1, "kain_service_references_at")?;
    let line = extract_u32(&args, 2, "kain_service_references_at")?;
    let column = extract_u32(&args, 3, "kain_service_references_at")?;

    let mut host = HOST.lock().map_err(|_| {
        KainError::runtime("kain_service_references_at: lock poisoned")
    })?;

    match host.references_at(workspace_id, document_id, line, column) {
        Ok(locations) => {
            let loc_values: Vec<Value> = locations
                .iter()
                .map(|loc| location_value(&loc.path, &loc.name, loc))
                .collect();
            Ok(struct_with(vec![
                ("status", Value::Int(0)),
                ("error_code", Value::Int(0)),
                ("locations", value_array(loc_values)),
            ]))
        }
        Err(error) => Ok(struct_with(vec![
            ("status", Value::Int(1)),
            ("error_code", Value::Int(service_error_code(&error) as i64)),
            ("locations", value_array(vec![])),
        ])),
    }
}

fn builtin_completions_at(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let workspace_id = extract_u64(&args, 0, "kain_service_completions_at")?;
    let document_id = extract_u64(&args, 1, "kain_service_completions_at")?;
    let line = extract_u32(&args, 2, "kain_service_completions_at")?;
    let column = extract_u32(&args, 3, "kain_service_completions_at")?;

    let mut host = HOST.lock().map_err(|_| {
        KainError::runtime("kain_service_completions_at: lock poisoned")
    })?;

    match host.completions_at(workspace_id, document_id, line, column) {
        Ok(completions) => {
            let comp_values: Vec<Value> = completions
                .iter()
                .map(|c| completion_value(&c.label, &c.detail, c.kind.code()))
                .collect();
            Ok(struct_with(vec![
                ("status", Value::Int(0)),
                ("error_code", Value::Int(0)),
                ("completions", value_array(comp_values)),
            ]))
        }
        Err(error) => Ok(struct_with(vec![
            ("status", Value::Int(1)),
            ("error_code", Value::Int(service_error_code(&error) as i64)),
            ("completions", value_array(vec![])),
        ])),
    }
}

fn builtin_document_symbols(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let workspace_id = extract_u64(&args, 0, "kain_service_document_symbols")?;
    let document_id = extract_u64(&args, 1, "kain_service_document_symbols")?;

    let mut host = HOST.lock().map_err(|_| {
        KainError::runtime("kain_service_document_symbols: lock poisoned")
    })?;

    match host.document_symbols(workspace_id, document_id) {
        Ok(symbols) => {
            let sym_values: Vec<Value> = symbols
                .iter()
                .map(|s| {
                    symbol_value(
                        &s.name,
                        &s.detail,
                        s.kind.code(),
                        &s.location.path,
                        s.location.range.start.line,
                        s.location.range.start.column,
                        s.location.range.start.offset,
                        s.location.range.end.line,
                        s.location.range.end.column,
                        s.location.range.end.offset,
                        s.container.as_deref(),
                    )
                })
                .collect();
            Ok(struct_with(vec![
                ("status", Value::Int(0)),
                ("error_code", Value::Int(0)),
                ("symbols", value_array(sym_values)),
            ]))
        }
        Err(error) => Ok(struct_with(vec![
            ("status", Value::Int(1)),
            ("error_code", Value::Int(service_error_code(&error) as i64)),
            ("symbols", value_array(vec![])),
        ])),
    }
}

fn builtin_workspace_symbols(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let workspace_id = extract_u64(&args, 0, "kain_service_workspace_symbols")?;
    let query = extract_string(&args, 1, "kain_service_workspace_symbols")?;

    let mut host = HOST.lock().map_err(|_| {
        KainError::runtime("kain_service_workspace_symbols: lock poisoned")
    })?;

    match host.workspace_symbols(workspace_id, &query) {
        Ok(symbols) => {
            let sym_values: Vec<Value> = symbols
                .iter()
                .map(|s| {
                    symbol_value(
                        &s.name,
                        &s.detail,
                        s.kind.code(),
                        &s.location.path,
                        s.location.range.start.line,
                        s.location.range.start.column,
                        s.location.range.start.offset,
                        s.location.range.end.line,
                        s.location.range.end.column,
                        s.location.range.end.offset,
                        s.container.as_deref(),
                    )
                })
                .collect();
            Ok(struct_with(vec![
                ("status", Value::Int(0)),
                ("error_code", Value::Int(0)),
                ("symbols", value_array(sym_values)),
            ]))
        }
        Err(error) => Ok(struct_with(vec![
            ("status", Value::Int(1)),
            ("error_code", Value::Int(service_error_code(&error) as i64)),
            ("symbols", value_array(vec![])),
        ])),
    }
}

fn builtin_semantic_tokens(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let workspace_id = extract_u64(&args, 0, "kain_service_semantic_tokens")?;
    let document_id = extract_u64(&args, 1, "kain_service_semantic_tokens")?;

    let mut host = HOST.lock().map_err(|_| {
        KainError::runtime("kain_service_semantic_tokens: lock poisoned")
    })?;

    match host.semantic_tokens(workspace_id, document_id) {
        Ok(tokens) => {
            let tok_values: Vec<Value> = tokens
                .iter()
                .map(|t| {
                    semantic_token_value(
                        t.range.start.line,
                        t.range.start.column,
                        t.range.start.offset,
                        t.range.end.line,
                        t.range.end.column,
                        t.range.end.offset,
                        t.token_type,
                        t.token_modifiers,
                    )
                })
                .collect();
            Ok(struct_with(vec![
                ("status", Value::Int(0)),
                ("error_code", Value::Int(0)),
                ("tokens", value_array(tok_values)),
            ]))
        }
        Err(error) => Ok(struct_with(vec![
            ("status", Value::Int(1)),
            ("error_code", Value::Int(service_error_code(&error) as i64)),
            ("tokens", value_array(vec![])),
        ])),
    }
}

fn builtin_format_document(_env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let workspace_id = extract_u64(&args, 0, "kain_service_format_document")?;
    let document_id = extract_u64(&args, 1, "kain_service_format_document")?;

    let mut host = HOST.lock().map_err(|_| {
        KainError::runtime("kain_service_format_document: lock poisoned")
    })?;

    match host.format_document(workspace_id, document_id) {
        Ok(fmt) => {
            let diagnostics: Vec<Value> = fmt
                .diagnostics
                .iter()
                .map(|report| {
                    let (psl, psc, pso, pel, pec, peo) = report
                        .primary_range
                        .as_ref()
                        .map(|r| {
                            (
                                r.start.line as u32,
                                r.start.col as u32,
                                r.start.offset,
                                r.end.line as u32,
                                r.end.col as u32,
                                r.end.offset,
                            )
                        })
                        .unwrap_or((0, 0, 0, 0, 0, 0));
                    diagnostic_value(
                        report.code.as_str(),
                        severity_code(report.severity),
                        &report.kind.to_string(),
                        &report.message,
                        report
                            .file
                            .as_ref()
                            .map(|p| p.display().to_string())
                            .or_else(|| report.origin.clone())
                            .unwrap_or_default()
                            .as_str(),
                        report.primary_range.is_some(),
                        psl, psc, pso, pel, pec, peo,
                        vec![],
                        report.notes.iter().map(|n| Value::String(n.clone())).collect(),
                        report.help.iter().map(|h| Value::String(h.clone())).collect(),
                        vec![],
                    )
                })
                .collect();

            Ok(struct_with(vec![
                ("status", Value::Int(0)),
                ("error_code", Value::Int(0)),
                ("formatted", Value::String(fmt.formatted)),
                ("already_formatted", Value::Bool(fmt.already_formatted)),
                ("diagnostics", value_array(diagnostics)),
            ]))
        }
        Err(error) => Ok(struct_with(vec![
            ("status", Value::Int(1)),
            ("error_code", Value::Int(service_error_code(&error) as i64)),
            ("formatted", Value::String(String::new())),
            ("already_formatted", Value::Bool(false)),
            ("diagnostics", value_array(vec![])),
        ])),
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
