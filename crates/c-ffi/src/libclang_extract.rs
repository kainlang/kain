#![allow(non_upper_case_globals)]

//! libclang-backed C header extractor.
//!
//! Uses the system's libclang (via `clang-sys`) to parse C headers with full
//! preprocessing, macro expansion, and type resolution.  Replaces the `lang_c`
//! + regex fallback pipeline for complex headers like `<windows.h>`.
//!
//! Extracts:
//! - Function declarations (name, return type, parameter types)
//! - Struct/union definitions (field names, types, offsets)
//! - Typedef aliases
//! - Enum constants
//! - Macro constants (`#define NAME VALUE`)

use crate::model::{
    BindingBundle, BindingReportEntry, BridgeParam, BridgeType, CFunctionBinding, FileFingerprint,
    ItemKind, ItemStatus, ResolvedCLibrary,
};
use clang_sys::*;
use kain_core::error::KainError;
use sha2::{Digest, Sha256};
use std::ffi::{CStr, CString};

use std::ptr::null_mut;

/// Extract a `BindingBundle` from a C header using libclang.
pub fn extract_binding_bundle_libclang(
    resolved: &ResolvedCLibrary,
) -> Result<BindingBundle, KainError> {
    let header_path = &resolved.header_path;
    let source = std::fs::read_to_string(header_path).map_err(|e| {
        KainError::runtime(format!(
            "libclang: could not read header '{}': {e}",
            header_path.display()
        ))
    })?;
    let fingerprint = FileFingerprint {
        path: header_path.display().to_string(),
        sha256: hex_sha256(source.as_bytes()),
    };

    let include_paths = collect_include_paths(resolved);
    let defines = collect_defines(resolved);

    let mut functions = Vec::new();
    let mut report_entries = Vec::new();
    let mut struct_names: Vec<String> = Vec::new();
    let mut typedef_map: std::collections::BTreeMap<String, String> = std::collections::BTreeMap::new();
    let mut enum_values: Vec<(String, i64)> = Vec::new();

    // Build clang command-line arguments
    let mut args: Vec<CString> = Vec::new();
    args.push(CString::new("-xc").unwrap()); // treat as C
    args.push(CString::new("-std=c11").unwrap());
    for path in &include_paths {
        args.push(CString::new(format!("-I{}", path.display())).unwrap());
    }
    for def in &defines {
        args.push(CString::new(format!("-D{}", def)).unwrap());
    }
    // Platform-specific defines (e.g. _WIN32 for Windows SDK, _GNU_SOURCE for Linux)
    // are injected by the resolution layer via system_registry::system_default_defines().
    // The extractor trusts the config it receives.

    let arg_ptrs: Vec<*const i8> = args.iter().map(|a| a.as_ptr()).collect();

    unsafe {
        let index = clang_createIndex(0, 0);
        if index.is_null() {
            return Err(KainError::runtime(
                "libclang: failed to create index".to_string(),
            ));
        }

        let header_cstr = CString::new(header_path.to_string_lossy().as_bytes()).map_err(|_| {
            KainError::runtime("libclang: header path contains null byte".to_string())
        })?;

        let tu = clang_parseTranslationUnit(
            index,
            header_cstr.as_ptr(),
            arg_ptrs.as_ptr(),
            arg_ptrs.len() as i32,
            null_mut(),
            0,
            CXTranslationUnit_DetailedPreprocessingRecord
                | CXTranslationUnit_SkipFunctionBodies,
        );

        if tu.is_null() {
            clang_disposeIndex(index);
            return Err(KainError::runtime(format!(
                "libclang: failed to parse '{}'",
                header_path.display()
            )));
        }

        // Walk the AST
        let cursor = clang_getTranslationUnitCursor(tu);
        let mut ctx = WalkerContext {
            import_name: &resolved.import_name,
            functions: &mut functions,
            report_entries: &mut report_entries,
            struct_names: &mut struct_names,
            typedef_map: &mut typedef_map,
            enum_values: &mut enum_values,
        };

        clang_visitChildren(
            cursor,
            visit_cursor,
            &mut ctx as *mut WalkerContext as *mut _,
        );

        // Add struct entries to report
        for name in &struct_names {
            report_entries.push(BindingReportEntry {
                symbol_path: format!("c::{}::{}", resolved.import_name, name),
                kind: ItemKind::Struct,
                status: ItemStatus::Callable,
                reason: None,
                emitted_symbol: None,
            });
        }

        // Add enum entries to report
        for (name, _value) in &enum_values {
            report_entries.push(BindingReportEntry {
                symbol_path: format!("c::{}::{}", resolved.import_name, name),
                kind: ItemKind::Enum,
                status: ItemStatus::Callable,
                reason: None,
                emitted_symbol: None,
            });
        }

        // Add typedef entries to report
        for (name, _) in &typedef_map {
            report_entries.push(BindingReportEntry {
                symbol_path: format!("c::{}::{}", resolved.import_name, name),
                kind: ItemKind::Typedef,
                status: ItemStatus::TypeOnly,
                reason: None,
                emitted_symbol: None,
            });
        }

        clang_disposeTranslationUnit(tu);
        clang_disposeIndex(index);
    }

    if functions.is_empty() {
        return Err(KainError::runtime(format!(
            "libclang: no functions extracted from '{}'",
            header_path.display()
        )));
    }

    Ok(BindingBundle {
        functions,
        report_entries,
        source_fingerprints: vec![fingerprint],
    })
}

struct WalkerContext<'a> {
    import_name: &'a str,
    functions: &'a mut Vec<CFunctionBinding>,
    report_entries: &'a mut Vec<BindingReportEntry>,
    struct_names: &'a mut Vec<String>,
    typedef_map: &'a mut std::collections::BTreeMap<String, String>,
    enum_values: &'a mut Vec<(String, i64)>,
}

extern "C" fn visit_cursor(
    cursor: CXCursor,
    _parent: CXCursor,
    client_data: CXClientData,
) -> CXChildVisitResult {
    unsafe {
        let ctx = &mut *(client_data as *mut WalkerContext);
        let kind = clang_getCursorKind(cursor);

        match kind {
            CXCursor_FunctionDecl => {
                if let Some(binding) = extract_function(cursor, ctx.import_name) {
                    ctx.report_entries.push(BindingReportEntry {
                        symbol_path: format!("c::{}::{}", ctx.import_name, binding.emitted_name),
                        kind: ItemKind::Function,
                        status: ItemStatus::Callable,
                        reason: None,
                        emitted_symbol: Some(binding.exported_aliases.last().cloned().unwrap_or_default()),
                    });
                    ctx.functions.push(binding);
                }
            }
            CXCursor_StructDecl | CXCursor_UnionDecl => {
                let name = cursor_spelling(cursor);
                if !name.is_empty() {
                    ctx.struct_names.push(name);
                }
            }
            CXCursor_TypedefDecl => {
                let name = cursor_spelling(cursor);
                if !name.is_empty() {
                    let underlying = clang_getTypedefDeclUnderlyingType(cursor);
                    let underlying_name = type_spelling(underlying);
                    ctx.typedef_map.insert(name, underlying_name);
                }
            }
            CXCursor_EnumDecl => {
                clang_visitChildren(
                    cursor,
                    visit_enum_child,
                    client_data,
                );
            }
            CXCursor_MacroDefinition => {
                if let Some((name, value)) = extract_macro_constant(cursor) {
                    ctx.enum_values.push((name, value));
                }
            }
            _ => {}
        }

        CXChildVisit_Continue
    }
}

extern "C" fn visit_enum_child(
    cursor: CXCursor,
    _parent: CXCursor,
    client_data: CXClientData,
) -> CXChildVisitResult {
    unsafe {
        let ctx = &mut *(client_data as *mut WalkerContext);
        if clang_getCursorKind(cursor) == CXCursor_EnumConstantDecl {
            let name = cursor_spelling(cursor);
            let value = clang_getEnumConstantDeclValue(cursor);
            if !name.is_empty() {
                ctx.enum_values.push((name, value));
            }
        }
        CXChildVisit_Continue
    }
}

unsafe fn extract_function(cursor: CXCursor, import_name: &str) -> Option<CFunctionBinding> {
    let name = cursor_spelling(cursor);
    if name.is_empty() {
        return None;
    }

    let func_type = clang_getCursorType(cursor);
    let ret_type = clang_getResultType(func_type);
    let return_bridge = map_type_to_bridge(ret_type)?;

    let num_args = clang_Cursor_getNumArguments(cursor);
    let mut params = Vec::new();

    for i in 0..num_args {
        let arg_cursor = clang_Cursor_getArgument(cursor, i as u32);
        let arg_name = cursor_spelling(arg_cursor);
        let arg_type = clang_getCursorType(arg_cursor);

        let param_name = if arg_name.is_empty() {
            format!("arg{i}")
        } else {
            sanitize_identifier(&arg_name)
        };

        let bridge_type = map_type_to_bridge(arg_type)
            .unwrap_or(BridgeType::OpaqueHandle {
                mutable: true,
                pointee: "unknown".to_string(),
            });

        params.push(BridgeParam {
            name: param_name,
            ty: bridge_type,
        });
    }

    // Check if variadic
    let _is_variadic = clang_isFunctionTypeVariadic(func_type) != 0;

    let emitted_name = name.clone();
    let canonical = format!("c_{}_{}", import_name, name);
    let aliases = if name != canonical {
        vec![canonical, name]
    } else {
        vec![name]
    };

    Some(CFunctionBinding {
        emitted_name,
        exported_aliases: aliases,
        symbol_name: String::new(),
        params,
        return_type: return_bridge,
    })
}

unsafe fn map_type_to_bridge(ty: CXType) -> Option<BridgeType> {
    let kind = ty.kind;

    match kind {
        CXType_Void => Some(BridgeType::Unit),
        CXType_Bool => Some(BridgeType::Bool),

        CXType_Char_S | CXType_SChar | CXType_Char16 | CXType_Short => {
            Some(BridgeType::SignedInt("i16".to_string()))
        }
        CXType_Char_U | CXType_UChar | CXType_UShort => {
            Some(BridgeType::UnsignedInt("u16".to_string()))
        }
        // int is always 32 bits on all 64-bit platforms (LLP64, LP64, ILP32).
        CXType_Int => Some(BridgeType::SignedInt("i32".to_string())),
        CXType_UInt => Some(BridgeType::UnsignedInt("u32".to_string())),
        // long is platform-dependent: 32-bit on Windows LLP64, 64-bit on Linux LP64.
        // Since we parse on the host without a cross target triple, use cfg!() to
        // match the host ABI that libclang will see. For cross-compilation, a
        // `-target` flag should be passed and `clang_Type_getSizeOf` (clang-sys 1.x)
        // should be used instead.
        CXType_Long | CXType_ULong => {
            let is_unsigned = kind == CXType_ULong;
            // On 64-bit Windows (LLP64), long is 32 bits. On 64-bit Linux (LP64), long is 64 bits.
            let is_llp64 = cfg!(all(target_os = "windows", target_pointer_width = "64"));
            if is_llp64 {
                if is_unsigned {
                    Some(BridgeType::UnsignedInt("u32".to_string()))
                } else {
                    Some(BridgeType::SignedInt("i32".to_string()))
                }
            } else {
                if is_unsigned {
                    Some(BridgeType::UnsignedInt("u64".to_string()))
                } else {
                    Some(BridgeType::SignedInt("i64".to_string()))
                }
            }
        }
        CXType_LongLong => Some(BridgeType::SignedInt("i64".to_string())),
        CXType_ULongLong => Some(BridgeType::UnsignedInt("u64".to_string())),
        CXType_Float => Some(BridgeType::Float32),
        CXType_Double => Some(BridgeType::Float64),

        CXType_Pointer => {
            let pointee = clang_getPointeeType(ty);
            let pointee_kind = pointee.kind;

            match pointee_kind {
                CXType_Void => Some(BridgeType::OpaqueHandle {
                    mutable: true,
                    pointee: "void".to_string(),
                }),
                CXType_Char_S | CXType_Char_U => Some(BridgeType::CString),
                _ => {
                    let pointee_name = type_spelling(pointee);
                    Some(BridgeType::RawPointer {
                        mutable: true,
                        pointee: pointee_name,
                        pointer_depth: 1,
                    })
                }
            }
        }

        CXType_Record => {
            // Struct or union — opaque handle for now
            let name = type_spelling(ty);
            Some(BridgeType::OpaqueHandle {
                mutable: false,
                pointee: if name.is_empty() { "struct".to_string() } else { name },
            })
        }

        CXType_Enum => Some(BridgeType::SignedInt("i64".to_string())),

        CXType_Typedef => {
            let canonical = clang_getCanonicalType(ty);
            map_type_to_bridge(canonical)
        }

        CXType_ConstantArray | CXType_IncompleteArray => {
            let element = clang_getArrayElementType(ty);
            let elem_name = type_spelling(element);
            Some(BridgeType::RawPointer {
                mutable: false,
                pointee: elem_name,
                pointer_depth: 1,
            })
        }

        CXType_FunctionProto => {
            // Function pointer — treat as opaque handle
            let sig = type_spelling(ty);
            Some(BridgeType::Callback {
                mutable: false,
                signature: sig,
            })
        }

        _ => {
            // Unknown type — treat as opaque handle
            let name = type_spelling(ty);
            Some(BridgeType::OpaqueHandle {
                mutable: true,
                pointee: if name.is_empty() { "unknown".to_string() } else { name },
            })
        }
    }
}

unsafe fn extract_macro_constant(cursor: CXCursor) -> Option<(String, i64)> {
    let name = cursor_spelling(cursor);
    if name.is_empty() {
        return None;
    }

    // Try to get the macro expansion tokens
    let tu = clang_Cursor_getTranslationUnit(cursor);
    let extent = clang_getCursorExtent(cursor);
    let mut tokens: *mut CXToken = null_mut();
    let mut num_tokens: u32 = 0;
    clang_tokenize(tu, extent, &mut tokens, &mut num_tokens);

    if tokens.is_null() || num_tokens < 2 {
        return None;
    }

    // First token is the macro name, second should be the value
    let mut result_name = String::new();
    let mut result_value = String::new();

    for i in 0..num_tokens {
        let token = *tokens.add(i as usize);
        let spelling = clang_getTokenSpelling(tu, token);
        let text = cxstring_to_string(spelling);
        clang_disposeString(spelling);

        if i == 0 {
            result_name = text;
        } else if i == 1 {
            result_value = text;
        }
    }

    clang_disposeTokens(tu, tokens, num_tokens);

    // Parse the value as an integer (decimal, hex, octal)
    if !result_name.is_empty() && !result_value.is_empty() {
        let value = if result_value.starts_with("0x") || result_value.starts_with("0X") {
            i64::from_str_radix(&result_value[2..], 16).ok()
        } else if result_value.starts_with('0') && result_value.len() > 1 {
            i64::from_str_radix(&result_value[1..], 8).ok()
        } else {
            result_value.parse::<i64>().ok()
        };

        if let Some(v) = value {
            return Some((result_name, v));
        }
    }

    None
}

// ─── Utility helpers ────────────────────────────────────────────────────────

unsafe fn cursor_spelling(cursor: CXCursor) -> String {
    let spelling = clang_getCursorSpelling(cursor);
    let s = cxstring_to_string(spelling);
    clang_disposeString(spelling);
    s
}

unsafe fn type_spelling(ty: CXType) -> String {
    let spelling = clang_getTypeSpelling(ty);
    let s = cxstring_to_string(spelling);
    clang_disposeString(spelling);
    s
}

unsafe fn cxstring_to_string(cx: CXString) -> String {
    let ptr = clang_getCString(cx);
    if ptr.is_null() {
        return String::new();
    }
    CStr::from_ptr(ptr).to_string_lossy().into_owned()
}

/// Sanitize a C identifier so it doesn't collide with Kain reserved keywords.
fn sanitize_identifier(name: &str) -> String {
    // Kain reserved keywords that commonly collide with C parameter names
    static KAIN_RESERVED: &[&str] = &[
        "sampler", "input", "output", "type", "world", "actor", "teleport",
        "struct", "enum", "fn", "let", "mut", "use", "mod", "pub",
        "as", "if", "else", "while", "for", "return", "match",
        "impl", "trait", "where", "self", "super", "extern",
        "true", "false", "void", "int", "float", "bool",
        "const", "static", "unsafe", "ref", "move",
        "pulse", "shader", "converge", "orchestrate",
        "entangle", "resonate", "patch", "law", "axiom",
        "shatter", "collapse", "observe", "decay",
    ];
    let lower = name.to_ascii_lowercase();
    if KAIN_RESERVED.contains(&lower.as_str()) {
        format!("{name}_")
    } else {
        name.to_string()
    }
}

fn hex_sha256(data: &[u8]) -> String {
    let hash = Sha256::digest(data);
    format!("{:x}", hash)
}

fn collect_include_paths(resolved: &ResolvedCLibrary) -> Vec<std::path::PathBuf> {
    let mut paths: Vec<std::path::PathBuf> = Vec::new();
    for p in &resolved.global_config.include_paths {
        push_unique(&mut paths, p.clone());
    }
    for p in &resolved.config.include_paths {
        push_unique(&mut paths, p.clone());
    }
    // Add parent of the header itself
    if let Some(parent) = resolved.header_path.parent() {
        push_unique(&mut paths, parent.to_path_buf());
    }
    paths
}

fn collect_defines(resolved: &ResolvedCLibrary) -> Vec<String> {
    let mut defs = Vec::new();
    for d in &resolved.global_config.defines {
        defs.push(d.clone());
    }
    for d in &resolved.config.defines {
        defs.push(d.clone());
    }
    defs
}

fn push_unique(paths: &mut Vec<std::path::PathBuf>, p: std::path::PathBuf) {
    if !paths.contains(&p) {
        paths.push(p);
    }
}
