use crate::model::{
    BindingBundle, BindingReportEntry, BridgeFunctionBinding, BridgeParam, BridgeType,
    FileFingerprint, GeneratedModuleItem, ItemKind, ItemStatus, ModuleNode, ResolvedCrate,
};
use crate::resolve::simple_file_sha256;
use heck::ToSnakeCase;
use kain_core::error::KainError;
use std::collections::HashSet;
use std::fs;
use std::path::{Path, PathBuf};
use syn::spanned::Spanned;
use syn::{
    Attribute, Expr, Fields, File, ImplItem, Item, ItemConst, ItemEnum, ItemFn, ItemImpl, ItemMod,
    ItemStruct, ItemType, Lit, Meta, Type, Visibility,
};

pub fn extract_binding_bundle(resolved: &ResolvedCrate) -> Result<BindingBundle, KainError> {
    let mut state = ExtractionState {
        resolved,
        module_root: ModuleNode::default(),
        bridge_functions: Vec::new(),
        report_entries: Vec::new(),
        source_fingerprints: Vec::new(),
        parsed_files: HashSet::new(),
        public_type_names: HashSet::new(),
    };
    state.parse_module_file(&resolved.crate_root_file, &[])?;
    Ok(BindingBundle {
        module_root: state.module_root,
        bridge_functions: state.bridge_functions,
        report_entries: state.report_entries,
        source_fingerprints: state.source_fingerprints,
    })
}

struct ExtractionState<'a> {
    resolved: &'a ResolvedCrate,
    module_root: ModuleNode,
    bridge_functions: Vec<BridgeFunctionBinding>,
    report_entries: Vec<BindingReportEntry>,
    source_fingerprints: Vec<FileFingerprint>,
    parsed_files: HashSet<PathBuf>,
    public_type_names: HashSet<String>,
}

impl<'a> ExtractionState<'a> {
    fn parse_module_file(
        &mut self,
        file_path: &Path,
        module_path: &[String],
    ) -> Result<(), KainError> {
        let canonical = fs::canonicalize(file_path).unwrap_or_else(|_| file_path.to_path_buf());
        if !self.parsed_files.insert(canonical.clone()) {
            return Ok(());
        }

        let source = fs::read_to_string(&canonical).map_err(KainError::Io)?;
        self.source_fingerprints.push(FileFingerprint {
            path: canonical.display().to_string(),
            sha256: simple_file_sha256(&canonical)?,
        });
        let parsed = syn::parse_file(&source).map_err(|err| {
            KainError::runtime(format!(
                "Failed to parse Rust source '{}' for crate FFI extraction: {err}",
                canonical.display()
            ))
        })?;
        self.parse_items(
            &parsed,
            canonical.parent().unwrap_or_else(|| Path::new(".")),
            module_path,
        )
    }

    fn parse_items(
        &mut self,
        parsed: &File,
        base_dir: &Path,
        module_path: &[String],
    ) -> Result<(), KainError> {
        for item in &parsed.items {
            match item {
                Item::Fn(value) => self.handle_function(value, module_path),
                Item::Struct(value) => self.handle_struct(value, module_path),
                Item::Enum(value) => self.handle_enum(value, module_path),
                Item::Type(value) => self.handle_type_alias(value, module_path),
                Item::Const(value) => self.handle_const(value, module_path),
                Item::Impl(value) => self.handle_impl(value, module_path),
                Item::Mod(value) => self.handle_module(value, base_dir, module_path)?,
                Item::Use(value) => {
                    if is_public(&value.vis) {
                        self.push_report(
                            module_path,
                            "use",
                            ItemKind::ReExport,
                            ItemStatus::Stubbed,
                            Some("pub use re-exports are not expanded yet".to_string()),
                            doc_lines(&value.attrs),
                            None,
                        );
                    }
                }
                Item::Trait(value) => {
                    if is_public(&value.vis) {
                        self.push_report(
                            module_path,
                            &value.ident.to_string(),
                            ItemKind::Trait,
                            ItemStatus::Stubbed,
                            Some("trait items are reported but not lowered into callable Kain bindings yet".to_string()),
                            doc_lines(&value.attrs),
                            None,
                        );
                    }
                }
                Item::Macro(value) => {
                    let name = value
                        .ident
                        .as_ref()
                        .map(ToString::to_string)
                        .unwrap_or_else(|| "macro".to_string());
                    self.push_report(
                        module_path,
                        &name,
                        ItemKind::Macro,
                        ItemStatus::Stubbed,
                        Some("macro items are not lowered into runtime bindings".to_string()),
                        doc_lines(&value.attrs),
                        None,
                    );
                }
                _ => {}
            }
        }
        Ok(())
    }

    fn handle_module(
        &mut self,
        value: &ItemMod,
        base_dir: &Path,
        parent_path: &[String],
    ) -> Result<(), KainError> {
        if !is_public(&value.vis) {
            self.push_report(
                parent_path,
                &value.ident.to_string(),
                ItemKind::Module,
                ItemStatus::SkippedInternal,
                Some("private modules are skipped by v1 crate FFI extraction".to_string()),
                doc_lines(&value.attrs),
                None,
            );
            return Ok(());
        }

        let mut next_module_path = parent_path.to_vec();
        next_module_path.push(value.ident.to_string());

        if let Some((_, items)) = &value.content {
            let parsed = File {
                shebang: None,
                attrs: Vec::new(),
                items: items.clone(),
            };
            self.parse_items(&parsed, base_dir, &next_module_path)?;
            return Ok(());
        }

        if let Some(path) = resolve_module_file(base_dir, &value.ident.to_string()) {
            self.parse_module_file(&path, &next_module_path)?;
            return Ok(());
        }

        self.push_report(
            parent_path,
            &value.ident.to_string(),
            ItemKind::Module,
            ItemStatus::Stubbed,
            Some("public module declaration could not be resolved to a source file".to_string()),
            doc_lines(&value.attrs),
            None,
        );
        Ok(())
    }

    fn handle_function(&mut self, value: &ItemFn, module_path: &[String]) {
        if !is_public(&value.vis) {
            return;
        }
        let docs = doc_lines(&value.attrs);
        let function_name = value.sig.ident.to_string();
        if !value.sig.generics.params.is_empty() || value.sig.asyncness.is_some() {
            self.add_stub_function(
                module_path,
                &function_name,
                docs,
                "generic and async functions are stubbed in crate FFI v1".to_string(),
            );
            return;
        }

        let mut params = Vec::new();
        for input in &value.sig.inputs {
            let syn::FnArg::Typed(pat_ty) = input else {
                self.add_stub_function(
                    module_path,
                    &function_name,
                    docs,
                    "receiver-style functions are only supported through inherent impl lowering"
                        .to_string(),
                );
                return;
            };
            let Some(name) = pat_to_ident_name(&pat_ty.pat) else {
                self.add_stub_function(
                    module_path,
                    &function_name,
                    docs,
                    "unsupported parameter pattern in public function".to_string(),
                );
                return;
            };
            let Some(ty) = bridge_type_from_syn(&pat_ty.ty) else {
                self.add_stub_function(
                    module_path,
                    &function_name,
                    docs,
                    format!(
                        "unsupported parameter type '{}'",
                        type_to_string(&pat_ty.ty)
                    ),
                );
                return;
            };
            params.push(BridgeParam { name, ty });
        }

        let return_type = match &value.sig.output {
            syn::ReturnType::Default => BridgeType::Unit,
            syn::ReturnType::Type(_, ty) => match bridge_type_from_syn(ty) {
                Some(ty) => ty,
                None => {
                    self.add_stub_function(
                        module_path,
                        &function_name,
                        docs,
                        format!("unsupported return type '{}'", type_to_string(ty)),
                    );
                    return;
                }
            },
        };

        let emitted_name = function_name.clone();
        let prefixed_alias =
            prefixed_symbol_name(&self.resolved.import_name, module_path, &function_name);
        let signature = render_kain_fn_signature(&emitted_name, &params, &return_type);
        self.insert_module_item(
            module_path,
            GeneratedModuleItem {
                name: emitted_name.clone(),
                source: format!(
                    "{}\n    return {}\n",
                    signature,
                    return_type.default_literal()
                ),
                docs: docs.clone(),
            },
        );
        self.insert_module_item(
            module_path,
            GeneratedModuleItem {
                name: prefixed_alias.clone(),
                source: format!(
                    "{}\n    return {}\n",
                    render_kain_fn_signature(&prefixed_alias, &params, &return_type),
                    return_type.default_literal()
                ),
                docs: docs.clone(),
            },
        );

        let rust_call_path = if module_path.is_empty() {
            format!("{}::{}", self.resolved.dependency_name, function_name)
        } else {
            format!(
                "{}::{}::{}",
                self.resolved.dependency_name,
                module_path.join("::"),
                function_name
            )
        };

        self.bridge_functions.push(BridgeFunctionBinding {
            emitted_name: emitted_name.clone(),
            exported_aliases: vec![emitted_name.clone(), prefixed_alias.clone()],
            rust_call_path,
            params,
            return_type,
            docs: docs.clone(),
        });
        self.push_report(
            module_path,
            &function_name,
            ItemKind::Function,
            ItemStatus::Callable,
            None,
            docs,
            Some(prefixed_alias),
        );
    }

    fn handle_struct(&mut self, value: &ItemStruct, module_path: &[String]) {
        if !is_public(&value.vis) {
            return;
        }
        let docs = doc_lines(&value.attrs);
        let name = value.ident.to_string();
        self.public_type_names.insert(name.clone());
        let source = match &value.fields {
            Fields::Named(fields) => {
                let mut lines = vec![format!("struct {name}:")];
                if fields.named.is_empty() {
                    lines.push("    __kain_placeholder: Int".to_string());
                } else {
                    for field in &fields.named {
                        let field_name = field
                            .ident
                            .as_ref()
                            .map(ToString::to_string)
                            .unwrap_or_else(|| "field".to_string());
                        let rendered_type = bridge_type_from_syn(&field.ty)
                            .map(|ty| ty.render_kain())
                            .unwrap_or_else(|| "Any".to_string());
                        lines.push(format!("    {field_name}: {rendered_type}"));
                    }
                }
                lines.join("\n") + "\n"
            }
            _ => format!("struct {name}:\n    __kain_placeholder: Int\n"),
        };

        self.insert_module_item(
            module_path,
            GeneratedModuleItem {
                name: name.clone(),
                source,
                docs: docs.clone(),
            },
        );
        self.push_report(
            module_path,
            &name,
            ItemKind::Struct,
            ItemStatus::TypeOnly,
            None,
            docs,
            None,
        );
    }

    fn handle_enum(&mut self, value: &ItemEnum, module_path: &[String]) {
        if !is_public(&value.vis) {
            return;
        }
        let docs = doc_lines(&value.attrs);
        let name = value.ident.to_string();
        self.public_type_names.insert(name.clone());
        let mut lines = vec![format!("enum {name}:")];
        if value.variants.is_empty() {
            lines.push("    Placeholder".to_string());
        } else {
            for variant in &value.variants {
                lines.push(format!("    {}", variant.ident));
            }
        }
        self.insert_module_item(
            module_path,
            GeneratedModuleItem {
                name: name.clone(),
                source: lines.join("\n") + "\n",
                docs: docs.clone(),
            },
        );
        self.push_report(
            module_path,
            &name,
            ItemKind::Enum,
            ItemStatus::TypeOnly,
            None,
            docs,
            None,
        );
    }

    fn handle_type_alias(&mut self, value: &ItemType, module_path: &[String]) {
        if !is_public(&value.vis) {
            return;
        }
        let docs = doc_lines(&value.attrs);
        let name = value.ident.to_string();
        if let Some(ty) = bridge_type_from_syn(&value.ty) {
            self.insert_module_item(
                module_path,
                GeneratedModuleItem {
                    name: name.clone(),
                    source: format!("type {name} = {}\n", ty.render_kain()),
                    docs: docs.clone(),
                },
            );
            self.push_report(
                module_path,
                &name,
                ItemKind::TypeAlias,
                ItemStatus::TypeOnly,
                None,
                docs,
                None,
            );
        } else {
            self.push_report(
                module_path,
                &name,
                ItemKind::TypeAlias,
                ItemStatus::Stubbed,
                Some("type alias target is not representable in v1 crate FFI".to_string()),
                docs,
                None,
            );
        }
    }

    fn handle_const(&mut self, value: &ItemConst, module_path: &[String]) {
        if !is_public(&value.vis) {
            return;
        }
        let docs = doc_lines(&value.attrs);
        let name = value.ident.to_string();
        let rendered_type = bridge_type_from_syn(&value.ty).map(|ty| ty.render_kain());
        let literal = render_const_literal(&value.expr);
        match (rendered_type, literal) {
            (Some(rendered_type), Some(literal)) => {
                self.insert_module_item(
                    module_path,
                    GeneratedModuleItem {
                        name: name.clone(),
                        source: format!("const {name}: {rendered_type} = {literal}\n"),
                        docs: docs.clone(),
                    },
                );
                self.push_report(
                    module_path,
                    &name,
                    ItemKind::Constant,
                    ItemStatus::Callable,
                    None,
                    docs,
                    None,
                );
            }
            _ => self.push_report(
                module_path,
                &name,
                ItemKind::Constant,
                ItemStatus::Stubbed,
                Some("const item is not a simple literal representable in Kain".to_string()),
                docs,
                None,
            ),
        }
    }

    fn handle_impl(&mut self, value: &ItemImpl, module_path: &[String]) {
        if value.trait_.is_some() {
            return;
        }
        let Type::Path(type_path) = value.self_ty.as_ref() else {
            return;
        };
        let Some(type_name) = type_path
            .path
            .segments
            .last()
            .map(|segment| segment.ident.to_string())
        else {
            return;
        };
        if !self.public_type_names.contains(&type_name) {
            return;
        }
        for item in &value.items {
            let ImplItem::Fn(method) = item else {
                continue;
            };
            if !is_public(&method.vis) {
                continue;
            }
            let method_name = format!("{}_{}", type_name.to_snake_case(), method.sig.ident);
            let docs = doc_lines(&method.attrs);
            if method.sig.receiver().is_some() || !method.sig.generics.params.is_empty() {
                self.add_stub_function(
                    module_path,
                    &method_name,
                    docs,
                    "receiver methods and generic inherent methods are stubbed in v1 crate FFI"
                        .to_string(),
                );
                continue;
            }

            let mut params = Vec::new();
            let mut unsupported = None;
            for input in &method.sig.inputs {
                let syn::FnArg::Typed(pat_ty) = input else {
                    unsupported =
                        Some("receiver methods are not lowered in v1 crate FFI".to_string());
                    break;
                };
                let Some(name) = pat_to_ident_name(&pat_ty.pat) else {
                    unsupported = Some("unsupported method parameter pattern".to_string());
                    break;
                };
                let Some(ty) = bridge_type_from_syn(&pat_ty.ty) else {
                    unsupported = Some(format!(
                        "unsupported method parameter type '{}'",
                        type_to_string(&pat_ty.ty)
                    ));
                    break;
                };
                params.push(BridgeParam { name, ty });
            }
            let return_type = match &method.sig.output {
                syn::ReturnType::Default => BridgeType::Unit,
                syn::ReturnType::Type(_, ty) => match bridge_type_from_syn(ty) {
                    Some(value) => value,
                    None => {
                        unsupported = Some(format!(
                            "unsupported method return type '{}'",
                            type_to_string(ty)
                        ));
                        BridgeType::Unit
                    }
                },
            };
            if let Some(reason) = unsupported {
                self.add_stub_function(module_path, &method_name, docs, reason);
                continue;
            }

            let emitted_name = method_name.clone();
            let prefixed_alias =
                prefixed_symbol_name(&self.resolved.import_name, module_path, &emitted_name);
            self.insert_module_item(
                module_path,
                GeneratedModuleItem {
                    name: emitted_name.clone(),
                    source: format!(
                        "{}\n    return {}\n",
                        render_kain_fn_signature(&emitted_name, &params, &return_type),
                        return_type.default_literal()
                    ),
                    docs: docs.clone(),
                },
            );
            self.insert_module_item(
                module_path,
                GeneratedModuleItem {
                    name: prefixed_alias.clone(),
                    source: format!(
                        "{}\n    return {}\n",
                        render_kain_fn_signature(&prefixed_alias, &params, &return_type),
                        return_type.default_literal()
                    ),
                    docs: docs.clone(),
                },
            );
            self.bridge_functions.push(BridgeFunctionBinding {
                emitted_name: emitted_name.clone(),
                exported_aliases: vec![emitted_name.clone(), prefixed_alias.clone()],
                rust_call_path: if module_path.is_empty() {
                    format!(
                        "{}::{}::{}",
                        self.resolved.dependency_name, type_name, method.sig.ident
                    )
                } else {
                    format!(
                        "{}::{}::{}::{}",
                        self.resolved.dependency_name,
                        module_path.join("::"),
                        type_name,
                        method.sig.ident
                    )
                },
                params,
                return_type,
                docs: docs.clone(),
            });
            self.push_report(
                module_path,
                &method_name,
                ItemKind::Method,
                ItemStatus::Callable,
                None,
                docs,
                Some(prefixed_alias),
            );
        }
    }

    fn add_stub_function(
        &mut self,
        module_path: &[String],
        name: &str,
        docs: Vec<String>,
        reason: String,
    ) {
        let message = format!(
            "Rust crate FFI stub: crate '{}' symbol '{}' is unsupported in v1: {}",
            self.resolved.import_name,
            join_symbol_path(module_path, name),
            reason
        );
        self.insert_module_item(
            module_path,
            GeneratedModuleItem {
                name: name.to_string(),
                source: format!("fn {name}() -> Unit:\n    panic({message:?})\n"),
                docs: docs.clone(),
            },
        );
        self.push_report(
            module_path,
            name,
            ItemKind::Function,
            ItemStatus::Stubbed,
            Some(reason),
            docs,
            None,
        );
    }

    fn insert_module_item(&mut self, module_path: &[String], item: GeneratedModuleItem) {
        let mut cursor = &mut self.module_root;
        for segment in module_path {
            cursor = cursor.children.entry(segment.clone()).or_default();
        }
        cursor.items.push(item);
    }

    fn push_report(
        &mut self,
        module_path: &[String],
        symbol_name: &str,
        kind: ItemKind,
        status: ItemStatus,
        reason: Option<String>,
        docs: Vec<String>,
        emitted_symbol: Option<String>,
    ) {
        self.report_entries.push(BindingReportEntry {
            symbol_path: join_symbol_path(module_path, symbol_name),
            module_path: module_path.to_vec(),
            kind,
            status,
            reason,
            docs,
            emitted_symbol,
        });
    }
}

fn resolve_module_file(base_dir: &Path, name: &str) -> Option<PathBuf> {
    let flat = base_dir.join(format!("{name}.rs"));
    if flat.exists() {
        return Some(flat);
    }
    let nested = base_dir.join(name).join("mod.rs");
    if nested.exists() {
        return Some(nested);
    }
    None
}

fn render_kain_fn_signature(
    name: &str,
    params: &[BridgeParam],
    return_type: &BridgeType,
) -> String {
    let rendered_params = params
        .iter()
        .map(|param| format!("{}: {}", param.name, param.ty.render_kain()))
        .collect::<Vec<_>>()
        .join(", ");
    format!(
        "fn {name}({rendered_params}) -> {}:",
        return_type.render_kain()
    )
}

fn prefixed_symbol_name(crate_name: &str, module_path: &[String], symbol_name: &str) -> String {
    let mut segments = vec!["rust".to_string(), crate_name.to_snake_case()];
    for segment in module_path {
        segments.push(segment.to_snake_case());
    }
    segments.push(symbol_name.to_snake_case());
    segments.join("_")
}

fn join_symbol_path(module_path: &[String], symbol_name: &str) -> String {
    if module_path.is_empty() {
        symbol_name.to_string()
    } else {
        format!("{}::{}", module_path.join("::"), symbol_name)
    }
}

fn is_public(visibility: &Visibility) -> bool {
    matches!(visibility, Visibility::Public(_))
}

fn pat_to_ident_name(pattern: &syn::Pat) -> Option<String> {
    match pattern {
        syn::Pat::Ident(value) => Some(value.ident.to_string()),
        _ => None,
    }
}

fn doc_lines(attrs: &[Attribute]) -> Vec<String> {
    attrs
        .iter()
        .filter_map(|attr| {
            if !attr.path().is_ident("doc") {
                return None;
            }
            match &attr.meta {
                Meta::NameValue(value) => match &value.value {
                    Expr::Lit(expr_lit) => match &expr_lit.lit {
                        Lit::Str(text) => Some(text.value().trim().to_string()),
                        _ => None,
                    },
                    _ => None,
                },
                _ => None,
            }
        })
        .filter(|line| !line.is_empty())
        .collect()
}

fn render_const_literal(expr: &Expr) -> Option<String> {
    match expr {
        Expr::Lit(value) => match &value.lit {
            Lit::Int(value) => Some(value.base10_digits().to_string()),
            Lit::Float(value) => Some(value.base10_digits().to_string()),
            Lit::Bool(value) => Some(value.value.to_string()),
            Lit::Str(value) => Some(format!("{:?}", value.value())),
            _ => None,
        },
        Expr::Unary(value) => {
            if let syn::UnOp::Neg(_) = value.op {
                render_const_literal(&value.expr).map(|literal| format!("-{literal}"))
            } else {
                None
            }
        }
        _ => None,
    }
}

pub fn bridge_type_from_syn(ty: &Type) -> Option<BridgeType> {
    match ty {
        Type::Tuple(value) if value.elems.is_empty() => Some(BridgeType::Unit),
        Type::Path(value) => {
            let segment = value.path.segments.last()?;
            let name = segment.ident.to_string();
            match name.as_str() {
                "bool" => Some(BridgeType::Bool("bool".to_string())),
                "i8" | "i16" | "i32" | "i64" => Some(BridgeType::Int(name)),
                "u8" | "u16" | "u32" | "u64" | "usize" => Some(BridgeType::Int(name)),
                "f32" | "f64" => Some(BridgeType::Float(name)),
                "String" => Some(BridgeType::StringOwned),
                "Option" => {
                    let inner = angle_bracket_type(segment)?;
                    bridge_type_from_syn(inner).map(|value| BridgeType::Option(Box::new(value)))
                }
                "Vec" => {
                    let inner = angle_bracket_type(segment)?;
                    bridge_type_from_syn(inner).map(|value| BridgeType::Array(Box::new(value)))
                }
                _ => None,
            }
        }
        Type::Reference(value) => {
            if value.mutability.is_some() {
                return None;
            }
            match value.elem.as_ref() {
                Type::Path(inner) if inner.path.is_ident("str") => Some(BridgeType::StringRef),
                _ => None,
            }
        }
        _ => None,
    }
}

fn angle_bracket_type<'a>(segment: &'a syn::PathSegment) -> Option<&'a Type> {
    let syn::PathArguments::AngleBracketed(value) = &segment.arguments else {
        return None;
    };
    value.args.iter().find_map(|arg| match arg {
        syn::GenericArgument::Type(value) => Some(value),
        _ => None,
    })
}

fn type_to_string(ty: &Type) -> String {
    match ty {
        Type::Path(value) => value
            .path
            .segments
            .iter()
            .map(|segment| segment.ident.to_string())
            .collect::<Vec<_>>()
            .join("::"),
        Type::Reference(value) => format!("&{}", type_to_string(&value.elem)),
        Type::Tuple(value) if value.elems.is_empty() => "()".to_string(),
        _ => format!("{:?}", ty.span()),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_supported_bridge_types() {
        let ty: Type = syn::parse_str("Vec<Option<i32>>").expect("parse type");
        let mapped = bridge_type_from_syn(&ty).expect("mapped");
        assert_eq!(mapped.render_kain(), "Array<Option<Int>>");
    }
}
