use crate::model::{
    BindingBundle, BindingReportEntry, BridgeParam, BridgeType, CFunctionBinding, FileFingerprint,
    ItemKind, ItemStatus, ResolvedCLibrary,
};
use kain_core::error::KainError;
use once_cell::sync::Lazy;
use regex::Regex;
use sha2::{Digest, Sha256};
use std::fs;

pub fn extract_binding_bundle(resolved: &ResolvedCLibrary) -> Result<BindingBundle, KainError> {
    let source = fs::read_to_string(&resolved.header_path).map_err(KainError::Io)?;
    let sanitized = strip_comments(&source);
    let prototypes = collect_function_prototypes(&sanitized);
    let fingerprint = FileFingerprint {
        path: resolved.header_path.display().to_string(),
        sha256: hex_sha256(source.as_bytes()),
    };

    let mut functions = Vec::new();
    let mut report_entries = Vec::new();

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
            Err((name, reason)) => {
                report_entries.push(BindingReportEntry {
                    symbol_path: format!("c::{}::{}", resolved.import_name, name),
                    kind: ItemKind::Function,
                    status: ItemStatus::Stubbed,
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

fn parse_function_binding(
    import_name: &str,
    prototype: &RawPrototype,
    symbol_overrides: &std::collections::BTreeMap<String, String>,
) -> Result<CFunctionBinding, (String, String)> {
    let return_type = parse_c_type(&prototype.return_type).ok_or_else(|| {
        (
            prototype.name.clone(),
            format!("unsupported return type '{}'", prototype.return_type),
        )
    })?;
    let params =
        parse_params(&prototype.args).map_err(|reason| (prototype.name.clone(), reason))?;
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
            return Err(format!("unsupported parameter declaration '{}'", token));
        }
        let (ty_raw, name) = split_type_and_name(token, index);
        let ty = parse_c_type(ty_raw.as_str())
            .ok_or_else(|| format!("unsupported parameter type '{}'", ty_raw))?;
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

fn parse_c_type(raw: &str) -> Option<BridgeType> {
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

    if pointer_depth == 1 && normalized_no_const == "char" {
        return Some(BridgeType::CString);
    }
    if pointer_depth > 0 {
        return None;
    }

    match normalized_no_const.as_str() {
        "void" => Some(BridgeType::Unit),
        "bool" | "_Bool" => Some(BridgeType::Bool),
        "float" => Some(BridgeType::Float32),
        "double" => Some(BridgeType::Float64),
        "int" | "signed" | "signed int" => {
            Some(BridgeType::SignedInt("std::os::raw::c_int".to_string()))
        }
        "unsigned" | "unsigned int" => {
            Some(BridgeType::UnsignedInt("std::os::raw::c_uint".to_string()))
        }
        "short" | "short int" | "signed short" | "signed short int" => {
            Some(BridgeType::SignedInt("std::os::raw::c_short".to_string()))
        }
        "unsigned short" | "unsigned short int" => {
            Some(BridgeType::UnsignedInt("std::os::raw::c_ushort".to_string()))
        }
        "long" | "long int" | "signed long" | "signed long int" => {
            Some(BridgeType::SignedInt("std::os::raw::c_long".to_string()))
        }
        "unsigned long" | "unsigned long int" => {
            Some(BridgeType::UnsignedInt("std::os::raw::c_ulong".to_string()))
        }
        "long long" | "long long int" | "signed long long" | "signed long long int" => {
            Some(BridgeType::SignedInt(
                "std::os::raw::c_longlong".to_string(),
            ))
        }
        "unsigned long long" | "unsigned long long int" => Some(BridgeType::UnsignedInt(
            "std::os::raw::c_ulonglong".to_string(),
        )),
        "size_t" => Some(BridgeType::UnsignedInt("usize".to_string())),
        "ptrdiff_t" | "intptr_t" => Some(BridgeType::SignedInt("isize".to_string())),
        "uintptr_t" => Some(BridgeType::UnsignedInt("usize".to_string())),
        "int8_t" => Some(BridgeType::SignedInt("i8".to_string())),
        "uint8_t" => Some(BridgeType::UnsignedInt("u8".to_string())),
        "int16_t" => Some(BridgeType::SignedInt("i16".to_string())),
        "uint16_t" => Some(BridgeType::UnsignedInt("u16".to_string())),
        "int32_t" => Some(BridgeType::SignedInt("i32".to_string())),
        "uint32_t" => Some(BridgeType::UnsignedInt("u32".to_string())),
        "int64_t" => Some(BridgeType::SignedInt("i64".to_string())),
        "uint64_t" => Some(BridgeType::UnsignedInt("u64".to_string())),
        _ => None,
    }
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
