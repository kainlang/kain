use crate::model::{
    BindingBundle, BindingReportEntry, BridgeParam, BridgeType, CFunctionBinding, FileFingerprint,
    ItemKind, ItemStatus, ResolvedCLibrary,
};
use kain_core::error::KainError;
use kain_import::c::{import_c_file_with_options, CImportOptions};
use once_cell::sync::Lazy;
use regex::Regex;
use sha2::{Digest, Sha256};
use std::fs;

pub fn extract_binding_bundle(resolved: &ResolvedCLibrary) -> Result<BindingBundle, KainError> {
    validate_header_with_kain_import(resolved)?;
    let source = fs::read_to_string(&resolved.header_path).map_err(KainError::Io)?;
    let sanitized = strip_comments(&source);
    let prototypes = collect_function_prototypes(&sanitized);
    let fingerprint = FileFingerprint {
        path: resolved.header_path.display().to_string(),
        sha256: hex_sha256(source.as_bytes()),
    };

    let mut functions = Vec::new();
    let mut report_entries = collect_type_entries(&resolved.import_name, &sanitized);

    for prototype in prototypes {
        match parse_function_binding(&resolved.import_name, &prototype, &resolved.config.symbols) {
            Ok(binding) => {
                report_entries.push(BindingReportEntry {
                    symbol_path: format!("c::{}::{}", resolved.import_name, binding.emitted_name),
                    kind: ItemKind::Function,
                    status: ItemStatus::Callable,
                    reason: None,
                    emitted_symbol: Some(binding.exported_aliases[1].clone()),
                });
                functions.push(binding);
            }
            Err((name, status, reason)) => {
                report_entries.push(BindingReportEntry {
                    symbol_path: format!("c::{}::{}", resolved.import_name, name),
                    kind: ItemKind::Function,
                    status,
                    reason: Some(reason),
                    emitted_symbol: None,
                });
            }
        }
    }

    Ok(BindingBundle {
        functions,
        report_entries,
        source_fingerprints: vec![fingerprint],
    })
}

#[derive(Debug, Clone)]
struct RawPrototype {
    return_type: String,
    name: String,
    args: String,
}

fn validate_header_with_kain_import(resolved: &ResolvedCLibrary) -> Result<(), KainError> {
    let mut options = CImportOptions::default();
    options.include_paths = resolved
        .global_config
        .include_paths
        .iter()
        .chain(resolved.config.include_paths.iter())
        .map(|value| value.display().to_string())
        .collect();
    options.defines = resolved
        .global_config
        .defines
        .iter()
        .chain(resolved.config.defines.iter())
        .cloned()
        .collect();
    options.cpp_options = resolved
        .global_config
        .cpp_options
        .iter()
        .chain(resolved.config.cpp_options.iter())
        .cloned()
        .collect();
    options.cpp_command = resolved
        .config
        .cpp_command
        .clone()
        .or_else(|| resolved.global_config.cpp_command.clone());
    let _ = import_c_file_with_options(&resolved.header_path, &options);
    Ok(())
}

fn collect_function_prototypes(source: &str) -> Vec<RawPrototype> {
    static PROTOTYPE_REGEX: Lazy<Regex> = Lazy::new(|| {
        Regex::new(
            r"(?ms)(?:^|\n)\s*(?:extern\s+)?(?P<ret>[A-Za-z_][A-Za-z0-9_\s\*]*?)\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\((?P<args>[^;{}()]*(?:\([^)]*\)[^;{}()]*)*)\)\s*;",
        )
        .expect("prototype regex")
    });

    PROTOTYPE_REGEX
        .captures_iter(source)
        .filter_map(|caps| {
            Some(RawPrototype {
                return_type: caps.name("ret")?.as_str().trim().to_string(),
                name: caps.name("name")?.as_str().trim().to_string(),
                args: caps.name("args")?.as_str().trim().to_string(),
            })
        })
        .collect()
}

fn collect_type_entries(import_name: &str, source: &str) -> Vec<BindingReportEntry> {
    static TYPEDEF_STRUCT_REGEX: Lazy<Regex> = Lazy::new(|| {
        Regex::new(r"(?m)^\s*typedef\s+struct\s+([A-Za-z_][A-Za-z0-9_]*)").expect("struct regex")
    });
    static TYPEDEF_ENUM_REGEX: Lazy<Regex> = Lazy::new(|| {
        Regex::new(r"(?m)^\s*typedef\s+enum\s+([A-Za-z_][A-Za-z0-9_]*)").expect("enum regex")
    });
    static TYPEDEF_ALIAS_REGEX: Lazy<Regex> = Lazy::new(|| {
        Regex::new(r"(?m)^\s*typedef\s+.+?\s+([A-Za-z_][A-Za-z0-9_]*)\s*;").expect("typedef regex")
    });

    let mut entries = Vec::new();
    for captures in TYPEDEF_STRUCT_REGEX.captures_iter(source) {
        let Some(name) = captures.get(1) else {
            continue;
        };
        entries.push(BindingReportEntry {
            symbol_path: format!("c::{}::{}", import_name, name.as_str()),
            kind: ItemKind::Struct,
            status: ItemStatus::OpaqueHandle,
            reason: Some("C struct discovered in header; emitted as type metadata only for now".to_string()),
            emitted_symbol: None,
        });
    }
    for captures in TYPEDEF_ENUM_REGEX.captures_iter(source) {
        let Some(name) = captures.get(1) else {
            continue;
        };
        entries.push(BindingReportEntry {
            symbol_path: format!("c::{}::{}", import_name, name.as_str()),
            kind: ItemKind::Enum,
            status: ItemStatus::TypeOnly,
            reason: Some("C enum discovered in header; emitted as type metadata only for now".to_string()),
            emitted_symbol: None,
        });
    }
    for captures in TYPEDEF_ALIAS_REGEX.captures_iter(source) {
        let Some(name) = captures.get(1) else {
            continue;
        };
        entries.push(BindingReportEntry {
            symbol_path: format!("c::{}::{}", import_name, name.as_str()),
            kind: ItemKind::Typedef,
            status: ItemStatus::TypeOnly,
            reason: Some("C typedef discovered in header; emitted as type metadata only for now".to_string()),
            emitted_symbol: None,
        });
    }
    entries.sort_by(|left, right| left.symbol_path.cmp(&right.symbol_path));
    entries.dedup_by(|left, right| left.symbol_path == right.symbol_path && left.kind == right.kind);
    entries
}

fn parse_function_binding(
    import_name: &str,
    prototype: &RawPrototype,
    symbol_overrides: &std::collections::BTreeMap<String, String>,
) -> Result<CFunctionBinding, (String, ItemStatus, String)> {
    let return_type = parse_c_type(&prototype.return_type)
        .map_err(|reason| (prototype.name.clone(), ItemStatus::Unsupported, reason))?;
    let params = parse_params(&prototype.args)
        .map_err(|reason| (prototype.name.clone(), ItemStatus::Unsupported, reason))?;
    let emitted_name = prototype.name.clone();
    let prefixed = format!("c_{}_{}", import_name, emitted_name);
    let symbol_name = symbol_overrides
        .get(&emitted_name)
        .cloned()
        .unwrap_or_else(|| emitted_name.clone());
    Ok(CFunctionBinding {
        emitted_name,
        exported_aliases: vec![prototype.name.clone(), prefixed],
        symbol_name,
        params,
        return_type,
    })
}

fn parse_params(args: &str) -> Result<Vec<BridgeParam>, String> {
    let trimmed = args.trim();
    if trimmed.is_empty() || trimmed == "void" {
        return Ok(Vec::new());
    }

    let mut params = Vec::new();
    for (index, raw) in trimmed.split(',').enumerate() {
        let token = raw.trim();
        if token.contains('(') || token.contains(')') {
            return Err(format!(
                "function-pointer or callback parameters are not supported yet: '{token}'"
            ));
        }
        let (ty_raw, name) = split_type_and_name(token, index);
        let ty = parse_c_type(ty_raw.as_str())?;
        params.push(BridgeParam { name, ty });
    }
    Ok(params)
}

fn split_type_and_name(token: &str, index: usize) -> (String, String) {
    let token = token.trim();
    if token.ends_with('*') {
        return (token.to_string(), format!("arg{}", index + 1));
    }

    if let Some(pos) = token.rfind(char::is_whitespace) {
        let type_part = token[..pos].trim();
        let name_part = token[pos + 1..].trim();
        if !name_part.is_empty()
            && name_part
                .chars()
                .all(|ch| ch.is_ascii_alphanumeric() || ch == '_')
        {
            if let Some(stripped) = name_part.strip_prefix('*') {
                return (format!("{} *", type_part), stripped.to_string());
            }
            return (type_part.to_string(), name_part.to_string());
        }
    }

    (token.to_string(), format!("arg{}", index + 1))
}

fn parse_c_type(raw: &str) -> Result<BridgeType, String> {
    let mut normalized = raw
        .replace('\t', " ")
        .replace("extern ", "")
        .replace("static ", "")
        .replace("inline ", "")
        .trim()
        .to_string();
    normalized = strip_declspec_like(&normalized);
    normalized = strip_uppercase_prefix_tokens(&normalized);
    while normalized.contains("  ") {
        normalized = normalized.replace("  ", " ");
    }
    let is_const = normalized.contains("const ");
    let pointer_depth = normalized.chars().filter(|ch| *ch == '*').count();
    normalized = normalized.replace('*', " ");
    while normalized.contains("  ") {
        normalized = normalized.replace("  ", " ");
    }
    normalized = normalized.trim().to_string();
    let normalized_no_const = normalized
        .replace("const ", "")
        .replace(" volatile", "")
        .replace("volatile ", "")
        .trim()
        .to_string();

    if pointer_depth > 1 {
        return Err(format!(
            "multi-level pointers are not supported yet: '{raw}'"
        ));
    }
    if pointer_depth == 1 {
        if normalized_no_const == "char" {
            return Ok(BridgeType::CString);
        }
        if is_byte_buffer_type(&normalized_no_const) {
            return Ok(BridgeType::ByteBuffer {
                mutable: !is_const,
                element_type: normalized_no_const,
            });
        }
        return Ok(BridgeType::OpaqueHandle {
            mutable: !is_const,
            pointee: normalized_no_const,
        });
    }

    match normalized_no_const.as_str() {
        "void" => Ok(BridgeType::Unit),
        "bool" | "_Bool" => Ok(BridgeType::Bool),
        "float" => Ok(BridgeType::Float32),
        "double" => Ok(BridgeType::Float64),
        "int" | "signed" | "signed int" => {
            Ok(BridgeType::SignedInt("std::os::raw::c_int".to_string()))
        }
        "unsigned" | "unsigned int" => {
            Ok(BridgeType::UnsignedInt("std::os::raw::c_uint".to_string()))
        }
        "short" | "short int" | "signed short" | "signed short int" => {
            Ok(BridgeType::SignedInt("std::os::raw::c_short".to_string()))
        }
        "unsigned short" | "unsigned short int" => {
            Ok(BridgeType::UnsignedInt("std::os::raw::c_ushort".to_string()))
        }
        "long" | "long int" | "signed long" | "signed long int" => {
            Ok(BridgeType::SignedInt("std::os::raw::c_long".to_string()))
        }
        "unsigned long" | "unsigned long int" => {
            Ok(BridgeType::UnsignedInt("std::os::raw::c_ulong".to_string()))
        }
        "long long" | "long long int" | "signed long long" | "signed long long int" => {
            Ok(BridgeType::SignedInt(
                "std::os::raw::c_longlong".to_string(),
            ))
        }
        "unsigned long long" | "unsigned long long int" => Ok(BridgeType::UnsignedInt(
            "std::os::raw::c_ulonglong".to_string(),
        )),
        "size_t" => Ok(BridgeType::UnsignedInt("usize".to_string())),
        "ptrdiff_t" | "intptr_t" => Ok(BridgeType::SignedInt("isize".to_string())),
        "uintptr_t" => Ok(BridgeType::UnsignedInt("usize".to_string())),
        "int8_t" => Ok(BridgeType::SignedInt("i8".to_string())),
        "uint8_t" => Ok(BridgeType::UnsignedInt("u8".to_string())),
        "int16_t" => Ok(BridgeType::SignedInt("i16".to_string())),
        "uint16_t" => Ok(BridgeType::UnsignedInt("u16".to_string())),
        "int32_t" => Ok(BridgeType::SignedInt("i32".to_string())),
        "uint32_t" => Ok(BridgeType::UnsignedInt("u32".to_string())),
        "int64_t" => Ok(BridgeType::SignedInt("i64".to_string())),
        "uint64_t" => Ok(BridgeType::UnsignedInt("u64".to_string())),
        other => Err(format!("unsupported value type '{other}'")),
    }
}

fn is_byte_buffer_type(raw: &str) -> bool {
    matches!(
        raw,
        "uint8_t" | "unsigned char" | "char8_t" | "std::byte" | "int8_t"
    )
}

fn strip_declspec_like(raw: &str) -> String {
    static DECLSPEC_RE: Lazy<Regex> = Lazy::new(|| {
        Regex::new(r"__declspec\s*\([^)]*\)|__attribute__\s*\(\([^)]*\)\)")
            .expect("declspec regex")
    });
    DECLSPEC_RE.replace_all(raw, " ").to_string()
}

fn strip_uppercase_prefix_tokens(raw: &str) -> String {
    let mut remaining = raw.trim();
    loop {
        let Some((first, rest)) = remaining.split_once(' ') else {
            return remaining.to_string();
        };
        let token = first.trim();
        if token.is_empty() {
            remaining = rest.trim();
            continue;
        }
        let looks_like_macro = token
            .chars()
            .all(|ch| ch.is_ascii_uppercase() || ch == '_' || ch.is_ascii_digit());
        if looks_like_macro {
            remaining = rest.trim();
            continue;
        }
        return remaining.to_string();
    }
}

fn strip_comments(source: &str) -> String {
    static BLOCK_RE: Lazy<Regex> =
        Lazy::new(|| Regex::new(r"(?s)/\*.*?\*/").expect("block regex"));
    static LINE_RE: Lazy<Regex> =
        Lazy::new(|| Regex::new(r"//.*").expect("line regex"));
    let without_block = BLOCK_RE.replace_all(source, " ");
    let without_line = LINE_RE.replace_all(&without_block, "");
    without_line.to_string()
}

fn hex_sha256(bytes: &[u8]) -> String {
    let mut hasher = Sha256::new();
    hasher.update(bytes);
    format!("{:x}", hasher.finalize())
}
