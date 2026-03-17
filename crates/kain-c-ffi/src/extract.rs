use crate::model::{
    BindingBundle, BindingReportEntry, BridgeParam, BridgeType, CFunctionBinding, FileFingerprint,
    ItemKind, ItemStatus, ResolvedCLibrary,
};
use kain_core::error::KainError;
use kain_import::c::{parse_c_file_ast_with_options, CImportOptions};
use lang_c::ast as c_ast;
use lang_c::span::Node;
use once_cell::sync::Lazy;
use regex::Regex;
use sha2::{Digest, Sha256};
use std::fs;

pub fn extract_binding_bundle(resolved: &ResolvedCLibrary) -> Result<BindingBundle, KainError> {
    let options = build_import_options(resolved);
    let source = fs::read_to_string(&resolved.header_path).map_err(KainError::Io)?;
    let fingerprint = FileFingerprint {
        path: resolved.header_path.display().to_string(),
        sha256: hex_sha256(source.as_bytes()),
    };

    let mut bundle = match extract_binding_bundle_from_ast(resolved, &options) {
        Ok(bundle) => bundle,
        Err(_) => extract_binding_bundle_from_regex(resolved, &strip_comments(&source))?,
    };
    bundle.source_fingerprints = vec![fingerprint];
    Ok(bundle)
}

fn build_import_options(resolved: &ResolvedCLibrary) -> CImportOptions {
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
    options
}

fn extract_binding_bundle_from_ast(
    resolved: &ResolvedCLibrary,
    options: &CImportOptions,
) -> Result<BindingBundle, KainError> {
    let translation_unit =
        parse_c_file_ast_with_options(&resolved.header_path, options).map_err(|err| {
            KainError::runtime(format!(
                "kain-import could not parse C FFI header '{}': {err}",
                resolved.header_path.display()
            ))
        })?;

    let mut functions = Vec::new();
    let mut report_entries = Vec::new();

    for external in translation_unit.0 {
        match external.node {
            c_ast::ExternalDeclaration::FunctionDefinition(func) => {
                collect_function_definition(
                    &resolved.import_name,
                    &func.node,
                    &resolved.config.symbols,
                    &mut functions,
                    &mut report_entries,
                );
            }
            c_ast::ExternalDeclaration::Declaration(decl) => {
                collect_declaration_items(
                    &resolved.import_name,
                    &decl.node,
                    &resolved.config.symbols,
                    &mut functions,
                    &mut report_entries,
                );
            }
            _ => {}
        }
    }

    functions.sort_by(|left, right| left.emitted_name.cmp(&right.emitted_name));
    functions.dedup_by(|left, right| left.emitted_name == right.emitted_name);
    report_entries.sort_by(|left, right| {
        left.symbol_path
            .cmp(&right.symbol_path)
            .then_with(|| kind_rank(left.kind).cmp(&kind_rank(right.kind)))
    });
    report_entries
        .dedup_by(|left, right| left.symbol_path == right.symbol_path && left.kind == right.kind);

    Ok(BindingBundle {
        functions,
        report_entries,
        source_fingerprints: Vec::new(),
    })
}

fn kind_rank(kind: ItemKind) -> u8 {
    match kind {
        ItemKind::Function => 0,
        ItemKind::Struct => 1,
        ItemKind::Enum => 2,
        ItemKind::Typedef => 3,
        ItemKind::Callback => 4,
        ItemKind::Global => 5,
    }
}

fn collect_function_definition(
    import_name: &str,
    func: &c_ast::FunctionDefinition,
    symbol_overrides: &std::collections::BTreeMap<String, String>,
    functions: &mut Vec<CFunctionBinding>,
    report_entries: &mut Vec<BindingReportEntry>,
) {
    let raw_name = match extract_declarator_name(&func.declarator.node) {
        Ok(name) if !name.is_empty() => name,
        _ => return,
    };

    match build_function_binding(
        import_name,
        &raw_name,
        &func.specifiers,
        &func.declarator.node,
        symbol_overrides,
    ) {
        Ok(binding) => {
            report_entries.push(BindingReportEntry {
                symbol_path: format!("c::{}::{}", import_name, binding.emitted_name),
                kind: ItemKind::Function,
                status: ItemStatus::Callable,
                reason: None,
                emitted_symbol: Some(binding.exported_aliases[1].clone()),
            });
            functions.push(binding);
        }
        Err((status, reason)) => {
            report_entries.push(BindingReportEntry {
                symbol_path: format!("c::{}::{}", import_name, raw_name),
                kind: ItemKind::Function,
                status,
                reason: Some(reason),
                emitted_symbol: None,
            });
        }
    }
}

fn collect_declaration_items(
    import_name: &str,
    decl: &c_ast::Declaration,
    symbol_overrides: &std::collections::BTreeMap<String, String>,
    functions: &mut Vec<CFunctionBinding>,
    report_entries: &mut Vec<BindingReportEntry>,
) {
    let is_typedef = declaration_is_typedef(&decl.specifiers);
    collect_named_type_entries(import_name, &decl.specifiers, report_entries);

    for init_decl in &decl.declarators {
        let declarator = &init_decl.node.declarator.node;
        let raw_name = match extract_declarator_name(declarator) {
            Ok(name) if !name.is_empty() => name,
            _ => continue,
        };
        let analysis = analyze_declarator(declarator);

        if analysis.callback_like {
            report_entries.push(BindingReportEntry {
                symbol_path: format!("c::{}::{}", import_name, raw_name),
                kind: ItemKind::Callback,
                status: ItemStatus::Unsupported,
                reason: Some(
                    "function-pointer callbacks are not emitted yet; generate a stable wrapper C API first"
                        .to_string(),
                ),
                emitted_symbol: None,
            });
            continue;
        }

        if analysis.is_direct_function {
            match build_function_binding(
                import_name,
                &raw_name,
                &decl.specifiers,
                declarator,
                symbol_overrides,
            ) {
                Ok(binding) => {
                    report_entries.push(BindingReportEntry {
                        symbol_path: format!("c::{}::{}", import_name, binding.emitted_name),
                        kind: ItemKind::Function,
                        status: ItemStatus::Callable,
                        reason: None,
                        emitted_symbol: Some(binding.exported_aliases[1].clone()),
                    });
                    functions.push(binding);
                }
                Err((status, reason)) => {
                    report_entries.push(BindingReportEntry {
                        symbol_path: format!("c::{}::{}", import_name, raw_name),
                        kind: ItemKind::Function,
                        status,
                        reason: Some(reason),
                        emitted_symbol: None,
                    });
                }
            }
            continue;
        }

        if is_typedef {
            report_entries.push(BindingReportEntry {
                symbol_path: format!("c::{}::{}", import_name, raw_name),
                kind: ItemKind::Typedef,
                status: ItemStatus::TypeOnly,
                reason: Some(
                    "C typedef discovered in header; emitted as type metadata only for now"
                        .to_string(),
                ),
                emitted_symbol: None,
            });
            continue;
        }

        report_entries.push(BindingReportEntry {
            symbol_path: format!("c::{}::{}", import_name, raw_name),
            kind: ItemKind::Global,
            status: ItemStatus::Stubbed,
            reason: Some(
                "global variable bindings are not emitted yet; expose accessors through a stable C API first"
                    .to_string(),
            ),
            emitted_symbol: None,
        });
    }
}

fn collect_named_type_entries(
    import_name: &str,
    specifiers: &[Node<c_ast::DeclarationSpecifier>],
    report_entries: &mut Vec<BindingReportEntry>,
) {
    for specifier in specifiers {
        if let c_ast::DeclarationSpecifier::TypeSpecifier(type_specifier) = &specifier.node {
            match &type_specifier.node {
                c_ast::TypeSpecifier::Struct(struct_type) => {
                    if let Some(name) = struct_type
                        .node
                        .identifier
                        .as_ref()
                        .map(|value| value.node.name.clone())
                    {
                        report_entries.push(BindingReportEntry {
                            symbol_path: format!("c::{}::{}", import_name, name),
                            kind: ItemKind::Struct,
                            status: ItemStatus::OpaqueHandle,
                            reason: Some(
                                "C struct discovered in header; pointer-based usage can bind as an opaque handle"
                                    .to_string(),
                            ),
                            emitted_symbol: None,
                        });
                    }
                }
                c_ast::TypeSpecifier::Enum(enum_type) => {
                    if let Some(name) = enum_type
                        .node
                        .identifier
                        .as_ref()
                        .map(|value| value.node.name.clone())
                    {
                        report_entries.push(BindingReportEntry {
                            symbol_path: format!("c::{}::{}", import_name, name),
                            kind: ItemKind::Enum,
                            status: ItemStatus::TypeOnly,
                            reason: Some(
                                "C enum discovered in header; by-value enum parameters currently lower through integer ABI handling"
                                    .to_string(),
                            ),
                            emitted_symbol: None,
                        });
                    }
                }
                _ => {}
            }
        }
    }
}

fn declaration_is_typedef(specifiers: &[Node<c_ast::DeclarationSpecifier>]) -> bool {
    specifiers.iter().any(|specifier| {
        matches!(
            &specifier.node,
            c_ast::DeclarationSpecifier::StorageClass(storage)
                if matches!(storage.node, c_ast::StorageClassSpecifier::Typedef)
        )
    })
}

fn build_function_binding(
    import_name: &str,
    raw_name: &str,
    specifiers: &[Node<c_ast::DeclarationSpecifier>],
    declarator: &c_ast::Declarator,
    symbol_overrides: &std::collections::BTreeMap<String, String>,
) -> Result<CFunctionBinding, (ItemStatus, String)> {
    let params =
        extract_function_params(declarator).map_err(|reason| (ItemStatus::Unsupported, reason))?;
    let return_type = resolve_bridge_type_from_declaration(specifiers, Some(declarator))
        .map_err(|reason| (ItemStatus::Unsupported, reason))?;

    let emitted_name = raw_name.to_string();
    let prefixed = format!("c_{}_{}", import_name, emitted_name);
    let symbol_name = symbol_overrides
        .get(&emitted_name)
        .cloned()
        .unwrap_or_else(|| emitted_name.clone());

    Ok(CFunctionBinding {
        emitted_name: emitted_name.clone(),
        exported_aliases: vec![emitted_name, prefixed],
        symbol_name,
        params,
        return_type,
    })
}

fn extract_function_params(declarator: &c_ast::Declarator) -> Result<Vec<BridgeParam>, String> {
    for derived in &declarator.derived {
        match &derived.node {
            c_ast::DerivedDeclarator::Function(func_decl) => {
                let mut params = Vec::new();
                for (index, param_decl) in func_decl.node.parameters.iter().enumerate() {
                    let param_decl = &param_decl.node;
                    let param_name = param_decl
                        .declarator
                        .as_ref()
                        .and_then(|decl| extract_declarator_name(&decl.node).ok())
                        .filter(|name| !name.is_empty())
                        .unwrap_or_else(|| format!("arg{}", index + 1));
                    let param_type = resolve_bridge_type_from_declaration(
                        &param_decl.specifiers,
                        param_decl.declarator.as_ref().map(|decl| &decl.node),
                    )?;
                    params.push(BridgeParam {
                        name: param_name,
                        ty: param_type,
                    });
                }
                return Ok(params);
            }
            c_ast::DerivedDeclarator::KRFunction(_) => {
                return Err(
                    "K&R-style function declarations are not supported in the C ABI bridge"
                        .to_string(),
                );
            }
            _ => {}
        }
    }

    Ok(Vec::new())
}

fn resolve_bridge_type_from_declaration(
    specifiers: &[Node<c_ast::DeclarationSpecifier>],
    declarator: Option<&c_ast::Declarator>,
) -> Result<BridgeType, String> {
    let facts = collect_type_facts(specifiers)?;
    let analysis = declarator.map(analyze_declarator).unwrap_or_default();

    if analysis.callback_like {
        return Err(
            "function-pointer callbacks are not supported yet; expose a stable wrapper function"
                .to_string(),
        );
    }
    if analysis.has_array {
        return Err("array declarators are not supported yet in the C ABI bridge".to_string());
    }
    if analysis.pointer_depth > 1 {
        return Err("multi-level pointers are not supported yet in the C ABI bridge".to_string());
    }

    if analysis.pointer_depth == 1 {
        return match facts.named {
            Some(NamedType::Enum(name))
            | Some(NamedType::Struct(name))
            | Some(NamedType::Typedef(name)) => Ok(BridgeType::OpaqueHandle {
                mutable: !facts.is_const,
                pointee: name,
            }),
            None => {
                if facts.scalar_key == "char" {
                    Ok(BridgeType::CString)
                } else if is_byte_buffer_scalar(facts.scalar_key.as_str()) {
                    Ok(BridgeType::ByteBuffer {
                        mutable: !facts.is_const,
                        element_type: facts.scalar_key,
                    })
                } else if facts.scalar_key == "void" {
                    Ok(BridgeType::OpaqueHandle {
                        mutable: !facts.is_const,
                        pointee: "void".to_string(),
                    })
                } else {
                    Ok(BridgeType::OpaqueHandle {
                        mutable: !facts.is_const,
                        pointee: facts.scalar_key,
                    })
                }
            }
        };
    }

    match facts.named {
        Some(NamedType::Enum(_)) => Ok(BridgeType::SignedInt("std::os::raw::c_int".to_string())),
        Some(NamedType::Struct(name)) | Some(NamedType::Typedef(name)) => {
            if let Some(mapped) = map_common_typedef_name(name.as_str()) {
                Ok(mapped)
            } else {
                Err(format!(
                    "unsupported by-value C type '{}'; pointer-based opaque handles are supported first",
                    name
                ))
            }
        }
        None => map_scalar_key_to_bridge_type(facts.scalar_key.as_str()),
    }
}

#[derive(Debug, Clone, Default)]
struct CTypeFacts {
    named: Option<NamedType>,
    scalar_key: String,
    is_const: bool,
}

#[derive(Debug, Clone)]
enum NamedType {
    Struct(String),
    Enum(String),
    Typedef(String),
}

fn collect_type_facts(
    specifiers: &[Node<c_ast::DeclarationSpecifier>],
) -> Result<CTypeFacts, String> {
    let mut facts = CTypeFacts::default();
    let mut scalar_tokens = Vec::new();

    for specifier in specifiers {
        match &specifier.node {
            c_ast::DeclarationSpecifier::TypeQualifier(qualifier) => {
                if matches!(qualifier.node, c_ast::TypeQualifier::Const) {
                    facts.is_const = true;
                }
            }
            c_ast::DeclarationSpecifier::TypeSpecifier(type_specifier) => {
                match &type_specifier.node {
                    c_ast::TypeSpecifier::Void => scalar_tokens.push("void"),
                    c_ast::TypeSpecifier::Bool => scalar_tokens.push("bool"),
                    c_ast::TypeSpecifier::Float => scalar_tokens.push("float"),
                    c_ast::TypeSpecifier::Double => scalar_tokens.push("double"),
                    c_ast::TypeSpecifier::Char => scalar_tokens.push("char"),
                    c_ast::TypeSpecifier::Short => scalar_tokens.push("short"),
                    c_ast::TypeSpecifier::Int => scalar_tokens.push("int"),
                    c_ast::TypeSpecifier::Long => scalar_tokens.push("long"),
                    c_ast::TypeSpecifier::Signed => scalar_tokens.push("signed"),
                    c_ast::TypeSpecifier::Unsigned => scalar_tokens.push("unsigned"),
                    c_ast::TypeSpecifier::TypedefName(name) => {
                        facts.named = Some(NamedType::Typedef(name.node.name.clone()));
                    }
                    c_ast::TypeSpecifier::Struct(struct_type) => {
                        facts.named = Some(NamedType::Struct(
                            struct_type
                                .node
                                .identifier
                                .as_ref()
                                .map(|value| value.node.name.clone())
                                .unwrap_or_else(|| "AnonymousStruct".to_string()),
                        ));
                    }
                    c_ast::TypeSpecifier::Enum(enum_type) => {
                        facts.named = Some(NamedType::Enum(
                            enum_type
                                .node
                                .identifier
                                .as_ref()
                                .map(|value| value.node.name.clone())
                                .unwrap_or_else(|| "AnonymousEnum".to_string()),
                        ));
                    }
                    other => {
                        return Err(format!(
                            "unsupported C type specifier in ABI bridge: {:?}",
                            other
                        ))
                    }
                }
            }
            _ => {}
        }
    }

    if facts.named.is_none() {
        facts.scalar_key = canonical_scalar_key(&scalar_tokens)?;
    }

    Ok(facts)
}

fn canonical_scalar_key(tokens: &[&str]) -> Result<String, String> {
    if tokens.is_empty() {
        return Ok("void".to_string());
    }

    let mut saw_void = false;
    let mut saw_bool = false;
    let mut saw_char = false;
    let mut saw_short = false;
    let mut saw_int = false;
    let mut saw_long_count = 0usize;
    let mut saw_float = false;
    let mut saw_double = false;
    let mut saw_signed = false;
    let mut saw_unsigned = false;

    for token in tokens {
        match *token {
            "void" => saw_void = true,
            "bool" => saw_bool = true,
            "char" => saw_char = true,
            "short" => saw_short = true,
            "int" => saw_int = true,
            "long" => saw_long_count += 1,
            "float" => saw_float = true,
            "double" => saw_double = true,
            "signed" => saw_signed = true,
            "unsigned" => saw_unsigned = true,
            other => {
                return Err(format!(
                    "unsupported C scalar specifier token in ABI bridge: {other}"
                ))
            }
        }
    }

    if saw_void {
        return Ok("void".to_string());
    }
    if saw_bool {
        return Ok("bool".to_string());
    }
    if saw_float {
        return Ok("float".to_string());
    }
    if saw_double && saw_long_count == 0 {
        return Ok("double".to_string());
    }
    if saw_char {
        if saw_unsigned {
            return Ok("unsigned char".to_string());
        }
        if saw_signed {
            return Ok("signed char".to_string());
        }
        return Ok("char".to_string());
    }
    if saw_short {
        return Ok(if saw_unsigned {
            "unsigned short".to_string()
        } else {
            "short".to_string()
        });
    }
    if saw_long_count >= 2 {
        return Ok(if saw_unsigned {
            "unsigned long long".to_string()
        } else {
            "long long".to_string()
        });
    }
    if saw_long_count == 1 {
        return Ok(if saw_unsigned {
            "unsigned long".to_string()
        } else {
            "long".to_string()
        });
    }
    if saw_int || saw_signed || saw_unsigned {
        return Ok(if saw_unsigned {
            "unsigned int".to_string()
        } else {
            "int".to_string()
        });
    }

    Err(format!(
        "unsupported C scalar declaration in ABI bridge: {}",
        tokens.join(" ")
    ))
}

fn map_scalar_key_to_bridge_type(raw: &str) -> Result<BridgeType, String> {
    match raw {
        "void" => Ok(BridgeType::Unit),
        "bool" => Ok(BridgeType::Bool),
        "float" => Ok(BridgeType::Float32),
        "double" => Ok(BridgeType::Float64),
        "char" | "signed char" => Ok(BridgeType::SignedInt("std::os::raw::c_char".to_string())),
        "unsigned char" => Ok(BridgeType::UnsignedInt("u8".to_string())),
        "short" => Ok(BridgeType::SignedInt("std::os::raw::c_short".to_string())),
        "unsigned short" => Ok(BridgeType::UnsignedInt(
            "std::os::raw::c_ushort".to_string(),
        )),
        "int" => Ok(BridgeType::SignedInt("std::os::raw::c_int".to_string())),
        "unsigned int" => Ok(BridgeType::UnsignedInt("std::os::raw::c_uint".to_string())),
        "long" => Ok(BridgeType::SignedInt("std::os::raw::c_long".to_string())),
        "unsigned long" => Ok(BridgeType::UnsignedInt("std::os::raw::c_ulong".to_string())),
        "long long" => Ok(BridgeType::SignedInt(
            "std::os::raw::c_longlong".to_string(),
        )),
        "unsigned long long" => Ok(BridgeType::UnsignedInt(
            "std::os::raw::c_ulonglong".to_string(),
        )),
        other => Err(format!("unsupported value type '{other}'")),
    }
}

fn map_common_typedef_name(name: &str) -> Option<BridgeType> {
    match name {
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

fn is_byte_buffer_scalar(raw: &str) -> bool {
    matches!(raw, "uint8_t" | "unsigned char" | "int8_t")
}

#[derive(Debug, Clone, Default)]
struct DeclaratorAnalysis {
    name: Option<String>,
    pointer_depth: usize,
    is_const: bool,
    has_function: bool,
    has_array: bool,
    has_block: bool,
    wrapped_kind: bool,
    is_direct_function: bool,
    callback_like: bool,
}

fn analyze_declarator(declarator: &c_ast::Declarator) -> DeclaratorAnalysis {
    let mut analysis = DeclaratorAnalysis::default();

    match &declarator.kind.node {
        c_ast::DeclaratorKind::Identifier(identifier) => {
            analysis.name = Some(identifier.node.name.clone());
        }
        c_ast::DeclaratorKind::Declarator(inner) => {
            analysis = analyze_declarator(&inner.node);
            analysis.wrapped_kind = true;
        }
        _ => {}
    }

    for derived in &declarator.derived {
        match &derived.node {
            c_ast::DerivedDeclarator::Pointer(qualifiers) => {
                analysis.pointer_depth += 1;
                if qualifiers.iter().any(|qualifier| {
                    matches!(
                        &qualifier.node,
                        c_ast::PointerQualifier::TypeQualifier(type_qualifier)
                            if matches!(type_qualifier.node, c_ast::TypeQualifier::Const)
                    )
                }) {
                    analysis.is_const = true;
                }
            }
            c_ast::DerivedDeclarator::Array(_) => {
                analysis.has_array = true;
            }
            c_ast::DerivedDeclarator::Function(_) | c_ast::DerivedDeclarator::KRFunction(_) => {
                analysis.has_function = true;
            }
            c_ast::DerivedDeclarator::Block(_) => {
                analysis.has_block = true;
            }
        }
    }

    analysis.is_direct_function = analysis.has_function && !analysis.wrapped_kind;
    analysis.callback_like = analysis.has_block || (analysis.has_function && analysis.wrapped_kind);
    analysis
}

fn extract_declarator_name(declarator: &c_ast::Declarator) -> Result<String, String> {
    match &declarator.kind.node {
        c_ast::DeclaratorKind::Identifier(identifier) => Ok(identifier.node.name.clone()),
        c_ast::DeclaratorKind::Declarator(inner) => extract_declarator_name(&inner.node),
        _ => Ok(String::new()),
    }
}

fn extract_binding_bundle_from_regex(
    resolved: &ResolvedCLibrary,
    sanitized_source: &str,
) -> Result<BindingBundle, KainError> {
    let prototypes = collect_function_prototypes(sanitized_source);
    let mut functions = Vec::new();
    let mut report_entries = collect_regex_type_entries(&resolved.import_name, sanitized_source);

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
        source_fingerprints: Vec::new(),
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

fn collect_regex_type_entries(import_name: &str, source: &str) -> Vec<BindingReportEntry> {
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
            reason: Some(
                "C struct discovered in header; emitted as type metadata only for now".to_string(),
            ),
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
            reason: Some(
                "C enum discovered in header; emitted as type metadata only for now".to_string(),
            ),
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
            reason: Some(
                "C typedef discovered in header; emitted as type metadata only for now".to_string(),
            ),
            emitted_symbol: None,
        });
    }
    entries.sort_by(|left, right| left.symbol_path.cmp(&right.symbol_path));
    entries
        .dedup_by(|left, right| left.symbol_path == right.symbol_path && left.kind == right.kind);
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
        if is_byte_buffer_scalar(&normalized_no_const) {
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

    if let Some(mapped) = map_common_typedef_name(&normalized_no_const) {
        return Ok(mapped);
    }
    map_scalar_key_to_bridge_type(&normalized_no_const)
}

fn strip_declspec_like(raw: &str) -> String {
    static DECLSPEC_RE: Lazy<Regex> = Lazy::new(|| {
        Regex::new(r"__declspec\s*\([^)]*\)|__attribute__\s*\(\([^)]*\)\)").expect("declspec regex")
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
    static BLOCK_RE: Lazy<Regex> = Lazy::new(|| Regex::new(r"(?s)/\*.*?\*/").expect("block regex"));
    static LINE_RE: Lazy<Regex> = Lazy::new(|| Regex::new(r"//.*").expect("line regex"));
    let without_block = BLOCK_RE.replace_all(source, " ");
    let without_line = LINE_RE.replace_all(&without_block, "");
    without_line.to_string()
}

fn hex_sha256(bytes: &[u8]) -> String {
    let mut hasher = Sha256::new();
    hasher.update(bytes);
    format!("{:x}", hasher.finalize())
}
