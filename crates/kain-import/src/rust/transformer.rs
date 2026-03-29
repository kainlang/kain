//! Rust AST → KAIN AST transformer.
//!
//! Mirrors the structure of `crate::c::transformer` (CTransformer) but maps
//! `syn` nodes instead of `lang-c` nodes. Because Rust and KAIN share the same
//! paradigms (algebraic types, ownership, pattern matching, effects) the mapping
//! is dramatically cleaner — most constructs translate ~1:1.
//!
//! ## Unsupported / Lowered
//!
//! - Lifetimes → erased (KAIN manages memory via effects + low-level layer)
//! - Trait aliases → skipped for the first self-host slice
//! - `macro_rules!` / proc-macro invocations → comment stub
//! - where clauses → dropped (KAIN has simpler generics for now)
//! - `extern crate` → skipped
//! - Associated types in traits → skipped for first slice

use super::types::RustTypeMapper;
use crate::common::identifier_registry::{IdentifierDomain, StableIdentifierRenamer};
use crate::Result;
use kain_core::ast::*;
use kain_core::effects::Effect;
use kain_core::span::Span;
use std::collections::{HashMap, HashSet};
use syn::{self};

// ── Transformer state ─────────────────────────────────────────────────────────

#[derive(Debug, Clone, Default)]
pub struct RustTransformOptions {
    pub strict_selfhost: bool,
    pub macro_policy: RustMacroPolicy,
}

#[derive(Debug, Clone, Default)]
pub struct RustMacroPolicy {
    pub lower_directly: HashSet<String>,
    pub preserve: HashSet<String>,
    pub reject: HashSet<String>,
}

impl RustMacroPolicy {
    pub fn phase1_default() -> Self {
        Self {
            lower_directly: [
                "assert",
                "assert_eq",
                "debug_assert",
                "eprint",
                "eprintln",
                "format",
                "matches",
                "panic",
                "print",
                "println",
                "unreachable",
                "vec",
                "write",
                "writeln",
            ]
            .into_iter()
            .map(str::to_string)
            .collect(),
            preserve: ["cfg", "derive", "arg", "command", "error", "from", "test"]
                .into_iter()
                .map(str::to_string)
                .collect(),
            reject: HashSet::new(),
        }
    }
}

pub struct RustTransformer {
    /// Maps Rust types to KAIN types.
    type_mapper: RustTypeMapper,

    /// Renames identifiers that clash with KAIN keywords.
    identifier_renamer: StableIdentifierRenamer,

    /// Generic type params in scope for the current item (for generic functions/structs).
    generics_in_scope: Vec<String>,

    /// Current function name (for diagnostics).
    current_function: Option<String>,

    /// Local variable type map (for type-directed lowering when needed).
    local_types: Vec<HashMap<String, Type>>,

    /// Scoped value substitutions used for lowered destructuring bindings.
    value_substitutions: Vec<HashMap<String, Expr>>,

    /// Monotonic counter for synthetic closure parameter names.
    closure_param_counter: usize,

    /// Accumulated warnings / unsupported construct notes.
    pub diagnostics: Vec<String>,

    /// Strict self-host mode rejects lossy lowering.
    options: RustTransformOptions,
}

impl RustTransformer {
    pub fn new() -> Self {
        Self::with_options(RustTransformOptions::default())
    }

    pub fn new_selfhost() -> Self {
        Self::with_options(RustTransformOptions {
            strict_selfhost: true,
            macro_policy: RustMacroPolicy::phase1_default(),
        })
    }

    pub fn with_options(options: RustTransformOptions) -> Self {
        let type_mapper = if options.strict_selfhost {
            RustTypeMapper::new_selfhost()
        } else {
            RustTypeMapper::new()
        };
        Self {
            type_mapper,
            identifier_renamer: StableIdentifierRenamer::default(),
            generics_in_scope: Vec::new(),
            current_function: None,
            local_types: vec![HashMap::new()],
            value_substitutions: vec![HashMap::new()],
            closure_param_counter: 0,
            diagnostics: Vec::new(),
            options,
        }
    }

    fn transform_attributes(&mut self, attrs: &[syn::Attribute]) -> Vec<Attribute> {
        let mut lowered = Vec::new();
        for attr in attrs {
            let name = path_to_ident(attr.path());
            let args = match &attr.meta {
                syn::Meta::Path(_) => Vec::new(),
                syn::Meta::List(_) => {
                    let parsed = attr.parse_args_with(
                        syn::punctuated::Punctuated::<syn::Expr, syn::Token![,]>::parse_terminated,
                    );
                    match parsed {
                        Ok(exprs) => exprs
                            .iter()
                            .filter_map(|expr| self.transform_attr_expr(expr).ok())
                            .collect(),
                        Err(err) => {
                            self.note_lossy_class(
                                "attribute_lowering",
                                format!("attribute {} args skipped: {}", name, err),
                            );
                            Vec::new()
                        }
                    }
                }
                syn::Meta::NameValue(value) => {
                    let target = Expr::Ident(path_to_ident(&value.path), S);
                    let value = self
                        .transform_attr_expr(&value.value)
                        .unwrap_or_else(|_| Expr::None(S));
                    vec![Expr::Assign {
                        target: Box::new(target),
                        value: Box::new(value),
                        span: S,
                    }]
                }
            };
            lowered.push(Attribute {
                name,
                args,
                span: S,
            });
        }
        lowered
    }

    fn transform_attr_expr(&mut self, expr: &syn::Expr) -> Result<Expr> {
        self.transform_expr(expr)
    }

    // ── Identifier helpers ────────────────────────────────────────────────

    fn rename_value(&mut self, raw: &str) -> String {
        // `self` is a KAIN keyword clash — map to `_self`
        if raw == "self" {
            return "_self".to_string();
        }
        self.identifier_renamer
            .resolve(IdentifierDomain::Value, raw)
    }

    fn rename_type(&mut self, raw: &str) -> String {
        self.identifier_renamer.resolve(IdentifierDomain::Type, raw)
    }

    fn rename_field(&mut self, raw: &str) -> String {
        self.identifier_renamer
            .resolve(IdentifierDomain::Field, raw)
    }

    fn rename_variant(&mut self, raw: &str) -> String {
        self.identifier_renamer
            .resolve(IdentifierDomain::Variant, raw)
    }

    #[allow(dead_code)]
    fn note(&mut self, msg: impl Into<String>) {
        self.diagnostics.push(msg.into());
    }

    fn note_lossy(&mut self, msg: impl Into<String>) {
        let msg = msg.into();
        if self.options.strict_selfhost {
            self.diagnostics.push(format!("SELFHOST_STRICT: {msg}"));
        } else {
            self.diagnostics.push(msg);
        }
    }

    fn note_lossy_class(&mut self, class: &str, msg: impl Into<String>) {
        let msg = msg.into();
        if self.options.strict_selfhost {
            self.diagnostics
                .push(format!("SELFHOST_STRICT [class:{class}]: {msg}"));
        } else {
            self.diagnostics.push(msg);
        }
    }

    fn lit_kind_name(lit: &syn::Lit) -> &'static str {
        match lit {
            syn::Lit::Bool(_) => "bool",
            syn::Lit::Byte(_) => "byte",
            syn::Lit::ByteStr(_) => "byte-str",
            syn::Lit::Char(_) => "char",
            syn::Lit::Float(_) => "float",
            syn::Lit::Int(_) => "int",
            syn::Lit::Str(_) => "str",
            syn::Lit::Verbatim(_) => "verbatim",
            _ => "unknown",
        }
    }

    fn expr_kind_name(expr: &syn::Expr) -> &'static str {
        match expr {
            syn::Expr::Array(_) => "array",
            syn::Expr::Assign(_) => "assign",
            syn::Expr::Async(_) => "async",
            syn::Expr::Await(_) => "await",
            syn::Expr::Binary(_) => "binary",
            syn::Expr::Block(_) => "block",
            syn::Expr::Break(_) => "break",
            syn::Expr::Call(_) => "call",
            syn::Expr::Cast(_) => "cast",
            syn::Expr::Closure(_) => "closure",
            syn::Expr::Const(_) => "const",
            syn::Expr::Continue(_) => "continue",
            syn::Expr::Field(_) => "field",
            syn::Expr::ForLoop(_) => "for-loop",
            syn::Expr::Group(_) => "group",
            syn::Expr::If(_) => "if",
            syn::Expr::Index(_) => "index",
            syn::Expr::Infer(_) => "infer",
            syn::Expr::Let(_) => "let",
            syn::Expr::Lit(_) => "literal",
            syn::Expr::Loop(_) => "loop",
            syn::Expr::Macro(_) => "macro",
            syn::Expr::Match(_) => "match",
            syn::Expr::MethodCall(_) => "method-call",
            syn::Expr::Paren(_) => "paren",
            syn::Expr::Path(_) => "path",
            syn::Expr::Range(_) => "range",
            syn::Expr::Reference(_) => "reference",
            syn::Expr::Repeat(_) => "repeat",
            syn::Expr::Return(_) => "return",
            syn::Expr::Struct(_) => "struct",
            syn::Expr::Try(_) => "try",
            syn::Expr::TryBlock(_) => "try-block",
            syn::Expr::Tuple(_) => "tuple",
            syn::Expr::Unary(_) => "unary",
            syn::Expr::Unsafe(_) => "unsafe",
            syn::Expr::While(_) => "while",
            syn::Expr::Yield(_) => "yield",
            _ => "unknown",
        }
    }

    fn should_skip_item(&self, item: &syn::Item) -> bool {
        match item {
            syn::Item::Const(node) => has_cfg_test_attr(&node.attrs),
            syn::Item::Enum(node) => has_cfg_test_attr(&node.attrs),
            syn::Item::ExternCrate(node) => has_cfg_test_attr(&node.attrs),
            syn::Item::Fn(node) => has_cfg_test_attr(&node.attrs),
            syn::Item::ForeignMod(node) => has_cfg_test_attr(&node.attrs),
            syn::Item::Impl(node) => has_cfg_test_attr(&node.attrs),
            syn::Item::Macro(node) => has_cfg_test_attr(&node.attrs),
            syn::Item::Mod(module) => self.should_skip_module(module),
            syn::Item::Static(node) => has_cfg_test_attr(&node.attrs),
            syn::Item::Struct(node) => has_cfg_test_attr(&node.attrs),
            syn::Item::Trait(node) => has_cfg_test_attr(&node.attrs),
            syn::Item::TraitAlias(node) => has_cfg_test_attr(&node.attrs),
            syn::Item::Type(node) => has_cfg_test_attr(&node.attrs),
            syn::Item::Union(node) => has_cfg_test_attr(&node.attrs),
            syn::Item::Use(node) => has_cfg_test_attr(&node.attrs),
            _ => false,
        }
    }

    fn should_skip_module(&self, module: &syn::ItemMod) -> bool {
        if module.ident == "tests" {
            return true;
        }
        has_cfg_test_attr(&module.attrs)
    }

    // ── Scope helpers ─────────────────────────────────────────────────────

    fn push_scope(&mut self) {
        self.local_types.push(HashMap::new());
    }
    fn pop_scope(&mut self) {
        if self.local_types.len() > 1 {
            self.local_types.pop();
        }
    }
    fn define(&mut self, name: &str, ty: Type) {
        if let Some(scope) = self.local_types.last_mut() {
            scope.insert(name.to_string(), ty);
        }
    }

    fn push_value_substitution_scope(&mut self) {
        self.value_substitutions.push(HashMap::new());
    }

    fn pop_value_substitution_scope(&mut self) {
        if self.value_substitutions.len() > 1 {
            self.value_substitutions.pop();
        }
    }

    fn add_value_substitution(&mut self, name: String, expr: Expr) {
        if let Some(scope) = self.value_substitutions.last_mut() {
            scope.insert(name, expr);
        }
    }

    fn lookup_value_substitution(&self, path: &syn::Path) -> Option<Expr> {
        if path.leading_colon.is_some() || path.segments.len() != 1 {
            return None;
        }
        let name = path.segments.first()?.ident.to_string();
        self.value_substitutions
            .iter()
            .rev()
            .find_map(|scope| scope.get(&name).cloned())
    }

    fn next_closure_param_name(&mut self) -> String {
        let name = format!("__kain_closure_arg{}", self.closure_param_counter);
        self.closure_param_counter += 1;
        name
    }

    fn closure_param_can_bind_directly(&self, pat: &syn::Pat) -> bool {
        match pat {
            syn::Pat::Ident(_) | syn::Pat::Wild(_) => true,
            syn::Pat::Paren(paren) => self.closure_param_can_bind_directly(&paren.pat),
            syn::Pat::Type(typed) => self.closure_param_can_bind_directly(&typed.pat),
            syn::Pat::Reference(_) => false,
            _ => false,
        }
    }

    fn tuple_binding_expr(&self, access: &Expr, index: usize) -> Expr {
        Expr::Field {
            object: Box::new(access.clone()),
            field: format!("__kain_tuple_{index}"),
            span: S,
        }
    }

    fn register_pattern_substitutions(&mut self, pat: &syn::Pat, access: Expr) {
        match pat {
            syn::Pat::Ident(ident) => {
                let name = ident.ident.to_string();
                let bound = if ident.by_ref.is_some() {
                    Expr::Ref {
                        mutable: ident.mutability.is_some(),
                        value: Box::new(access.clone()),
                        span: S,
                    }
                } else {
                    access.clone()
                };
                self.add_value_substitution(name, bound);
                if let Some((_, subpat)) = &ident.subpat {
                    self.register_pattern_substitutions(subpat, access);
                }
            }
            syn::Pat::Paren(paren) => self.register_pattern_substitutions(&paren.pat, access),
            syn::Pat::Type(typed) => self.register_pattern_substitutions(&typed.pat, access),
            syn::Pat::Reference(reference) => {
                self.register_pattern_substitutions(
                    &reference.pat,
                    Expr::Deref(Box::new(access), S),
                );
            }
            syn::Pat::Tuple(tuple) => {
                for (index, element) in tuple.elems.iter().enumerate() {
                    self.register_pattern_substitutions(
                        element,
                        self.tuple_binding_expr(&access, index),
                    );
                }
            }
            syn::Pat::TupleStruct(tuple_struct) => {
                for (index, element) in tuple_struct.elems.iter().enumerate() {
                    self.register_pattern_substitutions(
                        element,
                        Expr::Field {
                            object: Box::new(access.clone()),
                            field: format!("field_{index}"),
                            span: S,
                        },
                    );
                }
            }
            syn::Pat::Struct(struct_pat) => {
                for field in &struct_pat.fields {
                    let field_name = self.rename_field(&member_name(&field.member));
                    self.register_pattern_substitutions(
                        &field.pat,
                        Expr::Field {
                            object: Box::new(access.clone()),
                            field: field_name,
                            span: S,
                        },
                    );
                }
            }
            syn::Pat::Slice(slice) => {
                for (index, element) in slice.elems.iter().enumerate() {
                    self.register_pattern_substitutions(
                        element,
                        Expr::Index {
                            object: Box::new(access.clone()),
                            index: Box::new(Expr::Int(index as i64, S)),
                            span: S,
                        },
                    );
                }
            }
            syn::Pat::Wild(_)
            | syn::Pat::Lit(_)
            | syn::Pat::Path(_)
            | syn::Pat::Range(_)
            | syn::Pat::Rest(_) => {}
            _ => {
                self.note_lossy_class(
                    "closure_pattern_lowering",
                    "unsupported closure pattern binding lowered lossy".to_string(),
                );
            }
        }
    }

    fn build_closure_param(&mut self, pat: &syn::Pat) -> Param {
        let ty = self.type_from_local_pat(pat).unwrap_or(Type::Infer(S));
        if self.closure_param_can_bind_directly(pat) {
            let (name, mutable) = self.local_pat_name(pat);
            return Param {
                name,
                ty,
                mutable,
                default: None,
                span: S,
            };
        }

        let name = self.next_closure_param_name();
        self.register_pattern_substitutions(pat, Expr::Ident(name.clone(), S));
        Param {
            name,
            ty,
            mutable: false,
            default: None,
            span: S,
        }
    }

    fn map_type_checked(&mut self, ty: &syn::Type) -> Type {
        if let Some(trait_name) = dyn_trait_name(ty) {
            self.note_lossy_class(
                "dyn_trait_lowering",
                format!(
                    "dyn trait type lowered to impl {} (dynamic dispatch semantics narrowed)",
                    trait_name
                ),
            );
        }
        self.type_mapper.map_type(ty)
    }

    fn pattern_variant_head(&self, path: &syn::Path) -> (Option<String>, String) {
        let resolved = self.type_mapper.resolve_path_segments(path);
        let variant = resolved
            .last()
            .cloned()
            .or_else(|| {
                path.segments
                    .last()
                    .map(|segment| segment.ident.to_string())
            })
            .unwrap_or_default();
        let enum_name = if resolved.len() > 1 {
            Some(resolved[..resolved.len() - 1].join("::"))
        } else {
            None
        };
        (enum_name, variant)
    }

    // ─────────────────────────────────────────────────────────────────────
    // ── Entry point ───────────────────────────────────────────────────────
    // ─────────────────────────────────────────────────────────────────────

    pub fn transform(&mut self, file: syn::File) -> Result<Program> {
        let mut items = Vec::new();
        for item in &file.items {
            items.extend(self.transform_item(item)?);
        }
        Ok(Program { items, span: S })
    }

    // ─────────────────────────────────────────────────────────────────────
    // ── Items ─────────────────────────────────────────────────────────────
    // ─────────────────────────────────────────────────────────────────────

    fn transform_item(&mut self, item: &syn::Item) -> Result<Vec<Item>> {
        if self.should_skip_item(item) {
            return Ok(vec![]);
        }

        match item {
            syn::Item::Fn(f) => Ok(self
                .transform_fn(f)?
                .into_iter()
                .map(Item::Function)
                .collect()),
            syn::Item::Struct(s) => Ok(vec![Item::Struct(self.transform_struct(s)?)]),
            syn::Item::Enum(e) => Ok(vec![Item::Enum(self.transform_enum(e)?)]),
            syn::Item::Impl(i) => Ok(vec![Item::Impl(self.transform_impl(i)?)]),
            syn::Item::Const(c) => Ok(vec![Item::Const(self.transform_const(c)?)]),
            syn::Item::Static(s) => Ok(vec![Item::Const(self.transform_static(s)?)]),
            syn::Item::Type(t) => Ok(vec![Item::TypeAlias(self.transform_type_alias(t)?)]),
            syn::Item::Mod(m) => self.transform_mod(m),
            syn::Item::Use(u) => self.transform_use(u),
            syn::Item::Trait(t) => Ok(vec![Item::Trait(self.transform_trait(t)?)]),
            syn::Item::TraitAlias(t) => {
                self.note_lossy(format!("trait alias {} skipped", t.ident));
                Ok(vec![])
            }
            syn::Item::ExternCrate(_) => Ok(vec![]),
            // Macro rules / foreign items → skip
            syn::Item::Macro(_) | syn::Item::ForeignMod(_) => {
                self.note_lossy("macro/extern/foreign item skipped".to_string());
                Ok(vec![])
            }
            _ => {
                self.note_lossy("unknown item kind skipped".to_string());
                Ok(vec![])
            }
        }
    }

    // ── mod blocks ───────────────────────────────────────────────────────

    fn transform_mod(&mut self, m: &syn::ItemMod) -> Result<Vec<Item>> {
        if self.should_skip_module(m) {
            return Ok(vec![]);
        }

        match &m.content {
            Some((_, items)) => {
                let mut kain_items = Vec::new();
                for item in items {
                    kain_items.extend(self.transform_item(item)?);
                }
                let mod_name = self.rename_value(&m.ident.to_string());
                Ok(vec![Item::Mod(Mod {
                    name: mod_name,
                    inline: Some(kain_items),
                    visibility: visibility(&m.vis),
                    span: S,
                })])
            }
            None => {
                // `mod foo;` with external file — just note it, CLI handles multi-file
                self.note_lossy_class(
                    "external_mod_decl",
                    format!("mod {}; (external file — import separately)", m.ident),
                );
                Ok(vec![])
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    // ── Functions ─────────────────────────────────────────────────────────
    // ─────────────────────────────────────────────────────────────────────

    fn transform_fn(&mut self, f: &syn::ItemFn) -> Result<Vec<Function>> {
        let name = self.rename_value(&f.sig.ident.to_string());
        self.current_function = Some(name.clone());

        // Generic params
        let generics = self.type_mapper.map_generic_params(&f.sig.generics.params);
        self.generics_in_scope = generics.iter().map(|g| g.name.clone()).collect();

        // Params
        let params = self.transform_sig_inputs(&f.sig.inputs)?;

        // Return type
        let return_type = match &f.sig.output {
            syn::ReturnType::Default => None,
            syn::ReturnType::Type(_, ty) => Some(self.map_type_checked(ty)),
        };

        // Effects
        let mut effects: Vec<Effect> = Vec::new();
        if f.sig.unsafety.is_some() {
            effects.push(Effect::Unsafe);
        }
        if f.sig.asyncness.is_some() {
            effects.push(Effect::Async);
        }

        // Body
        self.push_scope();
        for p in &params {
            self.define(&p.name, p.ty.clone());
        }
        let body = self.transform_block(&f.block)?;
        self.pop_scope();

        self.generics_in_scope.clear();
        self.current_function = None;

        Ok(vec![Function {
            name,
            generics,
            params,
            return_type,
            body,
            effects,
            attributes: self.transform_attributes(&f.attrs),
            visibility: visibility(&f.vis),
            span: S,
        }])
    }

    fn transform_sig_inputs(
        &mut self,
        inputs: &syn::punctuated::Punctuated<syn::FnArg, syn::token::Comma>,
    ) -> Result<Vec<Param>> {
        let mut params = Vec::new();
        for input in inputs {
            match input {
                syn::FnArg::Receiver(r) => {
                    // `self` / `&self` / `&mut self`
                    let ty = if r.reference.is_some() {
                        Type::Ref {
                            mutable: r.mutability.is_some(),
                            inner: Box::new(Type::Named {
                                name: "Self".to_string(),
                                generics: vec![],
                                span: S,
                            }),
                            lifetime: None,
                            span: S,
                        }
                    } else {
                        Type::Named {
                            name: "Self".to_string(),
                            generics: vec![],
                            span: S,
                        }
                    };
                    params.push(Param {
                        name: "_self".to_string(),
                        ty,
                        mutable: r.mutability.is_some(),
                        default: None,
                        span: S,
                    });
                }
                syn::FnArg::Typed(pt) => {
                    let (name, mutable) = self.local_pat_name(&pt.pat);
                    let ty = self.map_type_checked(&pt.ty);
                    params.push(Param {
                        name,
                        ty,
                        mutable,
                        default: None,
                        span: S,
                    });
                }
            }
        }
        Ok(params)
    }

    fn transform_trait(&mut self, t: &syn::ItemTrait) -> Result<Trait> {
        let name = self.rename_type(&t.ident.to_string());
        let generics = self.type_mapper.map_generic_params(&t.generics.params);

        if t.unsafety.is_some() {
            self.note_lossy_class(
                "trait_surface_lowering",
                format!("unsafe trait {} lowered without unsafe marker", name),
            );
        }
        if t.auto_token.is_some() {
            self.note_lossy_class(
                "trait_surface_lowering",
                format!("auto trait {} lowered as normal trait", name),
            );
        }
        if !t.supertraits.is_empty() {
            self.note_lossy_class(
                "trait_surface_lowering",
                format!("trait {} supertraits skipped", name),
            );
        }
        if t.generics.where_clause.is_some() {
            self.note_lossy_class(
                "trait_surface_lowering",
                format!("trait {} where-clause skipped", name),
            );
        }

        let mut methods = Vec::new();
        for item in &t.items {
            match item {
                syn::TraitItem::Fn(method) => {
                    let method_name = self.rename_value(&method.sig.ident.to_string());
                    let params = self.transform_sig_inputs(&method.sig.inputs)?;
                    let return_type = match &method.sig.output {
                        syn::ReturnType::Default => None,
                        syn::ReturnType::Type(_, ty) => Some(self.map_type_checked(ty)),
                    };

                    if !method.sig.generics.params.is_empty() {
                        self.note_lossy_class(
                            "trait_surface_lowering",
                            format!("trait method {}::{} generics skipped", name, method_name),
                        );
                    }
                    if method.sig.generics.where_clause.is_some() {
                        self.note_lossy_class(
                            "trait_surface_lowering",
                            format!(
                                "trait method {}::{} where-clause skipped",
                                name, method_name
                            ),
                        );
                    }

                    let mut effects = Vec::new();
                    if method.sig.unsafety.is_some() {
                        effects.push(Effect::Unsafe);
                    }
                    if method.sig.asyncness.is_some() {
                        effects.push(Effect::Async);
                    }

                    let default_impl = if let Some(block) = &method.default {
                        Some(self.transform_block(block)?)
                    } else {
                        None
                    };

                    methods.push(TraitMethod {
                        name: method_name,
                        params,
                        return_type,
                        effects,
                        default_impl,
                        span: S,
                    });
                }
                syn::TraitItem::Const(item_const) => {
                    self.note_lossy_class(
                        "trait_surface_lowering",
                        format!(
                            "trait {} associated const {} skipped",
                            name, item_const.ident
                        ),
                    );
                }
                syn::TraitItem::Type(item_type) => {
                    self.note_lossy_class(
                        "trait_surface_lowering",
                        format!("trait {} associated type {} skipped", name, item_type.ident),
                    );
                }
                syn::TraitItem::Macro(_) => {
                    self.note_lossy_class(
                        "trait_surface_lowering",
                        format!("trait {} macro item skipped", name),
                    );
                }
                syn::TraitItem::Verbatim(_) => {
                    self.note_lossy_class(
                        "trait_surface_lowering",
                        format!("trait {} verbatim item skipped", name),
                    );
                }
                _ => {
                    self.note_lossy_class(
                        "trait_surface_lowering",
                        format!("trait {} unsupported item skipped", name),
                    );
                }
            }
        }

        Ok(Trait {
            name,
            generics,
            methods,
            visibility: visibility(&t.vis),
            span: S,
        })
    }

    // ─────────────────────────────────────────────────────────────────────
    // ── Structs ───────────────────────────────────────────────────────────
    // ─────────────────────────────────────────────────────────────────────

    fn transform_struct(&mut self, s: &syn::ItemStruct) -> Result<Struct> {
        let name = self.rename_type(&s.ident.to_string());
        let generics = self.type_mapper.map_generic_params(&s.generics.params);

        let fields = match &s.fields {
            syn::Fields::Named(named) => named
                .named
                .iter()
                .map(|f| {
                    let field_name = self
                        .rename_field(&f.ident.as_ref().map(|i| i.to_string()).unwrap_or_default());
                    let ty = self.map_type_checked(&f.ty);
                    Field {
                        name: field_name,
                        ty,
                        attributes: self.transform_attributes(&f.attrs),
                        visibility: visibility(&f.vis),
                        default: None,
                        weak: false,
                        span: S,
                    }
                })
                .collect(),
            syn::Fields::Unnamed(unnamed) => unnamed
                .unnamed
                .iter()
                .enumerate()
                .map(|(i, f)| {
                    let ty = self.map_type_checked(&f.ty);
                    Field {
                        name: format!("field_{}", i),
                        ty,
                        attributes: self.transform_attributes(&f.attrs),
                        visibility: visibility(&f.vis),
                        default: None,
                        weak: false,
                        span: S,
                    }
                })
                .collect(),
            syn::Fields::Unit => vec![],
        };

        Ok(Struct {
            name,
            generics,
            fields,
            methods: Vec::new(),
            attributes: self.transform_attributes(&s.attrs),
            visibility: visibility(&s.vis),
            span: S,
        })
    }

    // ─────────────────────────────────────────────────────────────────────
    // ── Enums ─────────────────────────────────────────────────────────────
    // ─────────────────────────────────────────────────────────────────────

    fn transform_enum(&mut self, e: &syn::ItemEnum) -> Result<Enum> {
        let name = self.rename_type(&e.ident.to_string());
        let generics = self.type_mapper.map_generic_params(&e.generics.params);

        let variants = e
            .variants
            .iter()
            .map(|v| {
                let variant_name = self.rename_variant(&v.ident.to_string());
                let fields = match &v.fields {
                    syn::Fields::Unit => VariantFields::Unit,
                    syn::Fields::Unnamed(un) => VariantFields::Tuple(
                        un.unnamed
                            .iter()
                            .map(|f| self.map_type_checked(&f.ty))
                            .collect(),
                    ),
                    syn::Fields::Named(named) => VariantFields::Struct(
                        named
                            .named
                            .iter()
                            .map(|f| {
                                let field_name = f
                                    .ident
                                    .as_ref()
                                    .map(|i| self.rename_field(&i.to_string()))
                                    .unwrap_or_default();
                                let ty = self.map_type_checked(&f.ty);
                                Field {
                                    name: field_name,
                                    ty,
                                    attributes: Vec::new(),
                                    visibility: Visibility::Public,
                                    default: None,
                                    weak: false,
                                    span: S,
                                }
                            })
                            .collect(),
                    ),
                };
                Variant {
                    name: variant_name,
                    fields,
                    span: S,
                }
            })
            .collect();

        Ok(Enum {
            name,
            generics,
            variants,
            visibility: visibility(&e.vis),
            span: S,
        })
    }

    // ─────────────────────────────────────────────────────────────────────
    // ── Impl blocks ───────────────────────────────────────────────────────
    // ─────────────────────────────────────────────────────────────────────

    fn transform_impl(&mut self, i: &syn::ItemImpl) -> Result<Impl> {
        let target_type = self.map_type_checked(&i.self_ty);
        let generics = self.type_mapper.map_generic_params(&i.generics.params);
        if i.generics.where_clause.is_some() {
            self.note_lossy_class(
                "trait_surface_lowering",
                "impl where-clause skipped".to_string(),
            );
        }

        // `impl Trait for Type` → note the trait, still emit the methods
        let trait_name = i
            .trait_
            .as_ref()
            .map(|(_, path, _)| self.resolve_type_path(path));
        let trait_generics = i
            .trait_
            .as_ref()
            .and_then(|(_, path, _)| path.segments.last())
            .map(|segment| self.type_mapper.generic_args(&segment.arguments))
            .unwrap_or_default();

        let mut methods = Vec::new();
        for item in &i.items {
            match item {
                syn::ImplItem::Fn(method) => {
                    let name = self.rename_value(&method.sig.ident.to_string());
                    self.current_function = Some(name.clone());

                    let method_generics = self
                        .type_mapper
                        .map_generic_params(&method.sig.generics.params);
                    self.generics_in_scope =
                        method_generics.iter().map(|g| g.name.clone()).collect();
                    if method.sig.generics.where_clause.is_some() {
                        self.note_lossy_class(
                            "trait_surface_lowering",
                            format!("impl method {} where-clause skipped", name),
                        );
                    }

                    let params = self.transform_sig_inputs(&method.sig.inputs)?;
                    let return_type = match &method.sig.output {
                        syn::ReturnType::Default => None,
                        syn::ReturnType::Type(_, ty) => Some(self.map_type_checked(ty)),
                    };
                    let mut effects = Vec::new();
                    if method.sig.unsafety.is_some() {
                        effects.push(Effect::Unsafe);
                    }
                    if method.sig.asyncness.is_some() {
                        effects.push(Effect::Async);
                    }

                    self.push_scope();
                    for p in &params {
                        self.define(&p.name, p.ty.clone());
                    }
                    let body = self.transform_block(&method.block)?;
                    self.pop_scope();

                    self.generics_in_scope.clear();
                    self.current_function = None;

                    methods.push(Function {
                        name,
                        generics: method_generics,
                        params,
                        return_type,
                        body,
                        effects,
                        attributes: self.transform_attributes(&method.attrs),
                        visibility: visibility(&method.vis),
                        span: S,
                    });
                }
                syn::ImplItem::Const(item_const) => {
                    self.note_lossy_class(
                        "trait_surface_lowering",
                        format!("impl associated const {} skipped", item_const.ident),
                    );
                }
                syn::ImplItem::Type(item_type) => {
                    self.note_lossy_class(
                        "trait_surface_lowering",
                        format!("impl associated type {} skipped", item_type.ident),
                    );
                }
                syn::ImplItem::Macro(_) => {
                    self.note_lossy_class(
                        "trait_surface_lowering",
                        "impl macro item skipped".to_string(),
                    );
                }
                syn::ImplItem::Verbatim(_) => {
                    self.note_lossy_class(
                        "trait_surface_lowering",
                        "impl verbatim item skipped".to_string(),
                    );
                }
                _ => {
                    self.note_lossy_class(
                        "trait_surface_lowering",
                        "impl unsupported item skipped".to_string(),
                    );
                }
            }
        }

        Ok(Impl {
            generics,
            target_type,
            trait_name,
            trait_generics,
            methods,
            span: S,
        })
    }

    // ─────────────────────────────────────────────────────────────────────
    // ── Const / Static ────────────────────────────────────────────────────
    // ─────────────────────────────────────────────────────────────────────

    fn transform_const(&mut self, c: &syn::ItemConst) -> Result<Const> {
        let name = self.rename_value(&c.ident.to_string());
        let ty = self.map_type_checked(&c.ty);
        let value = self.transform_expr(&c.expr)?;
        Ok(Const {
            name,
            ty,
            value,
            visibility: visibility(&c.vis),
            span: S,
        })
    }

    fn transform_static(&mut self, s: &syn::ItemStatic) -> Result<Const> {
        let name = self.rename_value(&s.ident.to_string());
        let ty = self.map_type_checked(&s.ty);
        let value = self.transform_expr(&s.expr)?;
        Ok(Const {
            name,
            ty,
            value,
            visibility: visibility(&s.vis),
            span: S,
        })
    }

    // ── Type alias ─────────────────────────────────────────────────────

    fn transform_type_alias(&mut self, t: &syn::ItemType) -> Result<TypeAlias> {
        let name = self.rename_type(&t.ident.to_string());
        let generics = self.type_mapper.map_generic_params(&t.generics.params);
        if t.generics.where_clause.is_some() {
            self.note_lossy(format!("type alias {} where-clause skipped", name));
        }
        let target = self.map_type_checked(&t.ty);
        Ok(TypeAlias {
            name,
            generics,
            target,
            visibility: visibility(&t.vis),
            span: S,
        })
    }

    // ─────────────────────────────────────────────────────────────────────
    // ── Blocks & Statements ───────────────────────────────────────────────
    // ─────────────────────────────────────────────────────────────────────

    fn transform_block(&mut self, block: &syn::Block) -> Result<Block> {
        self.push_scope();
        let mut stmts = Vec::new();
        for stmt in &block.stmts {
            stmts.extend(self.transform_stmt(stmt)?);
        }
        self.pop_scope();
        Ok(Block { stmts, span: S })
    }

    fn transform_stmt(&mut self, stmt: &syn::Stmt) -> Result<Vec<Stmt>> {
        match stmt {
            // `let x = expr;`  /  `let x: T = expr;`
            syn::Stmt::Local(local) => {
                let pattern = self.transform_pattern(&local.pat);
                let ty = self.type_from_local_pat(&local.pat);
                let (ty_ann, value) = if let Some(init) = &local.init {
                    let ty_from_pat = self.type_from_local_pat(&local.pat);
                    let val = self.transform_expr(&init.expr)?;
                    (ty_from_pat, Some(val))
                } else {
                    (None, None)
                };
                let ty_ann = ty.or(ty_ann);
                if let (Some(ty_ann), Pattern::Binding { name, .. }) = (&ty_ann, &pattern) {
                    self.define(name, ty_ann.clone());
                }
                Ok(vec![Stmt::Let {
                    pattern,
                    ty: ty_ann,
                    value,
                    span: S,
                }])
            }

            // Expression statement (with or without trailing semicolon)
            syn::Stmt::Expr(expr, semi) => {
                let kain_expr = self.transform_expr(expr)?;
                if semi.is_some() {
                    Ok(vec![Stmt::Expr(kain_expr)])
                } else {
                    // Block-final expression without `;` → implicit return value
                    Ok(vec![Stmt::Expr(kain_expr)])
                }
            }

            // `use` / `item` inside a function → skip
            syn::Stmt::Item(item) => {
                // Nested items in functions are hoisted by Rust; emit them
                let items = self.transform_item(item)?;
                Ok(items
                    .into_iter()
                    .map(|item| Stmt::Item(Box::new(item)))
                    .collect())
            }
            syn::Stmt::Macro(stmt_macro) => {
                let macro_name = path_to_ident(&stmt_macro.mac.path);
                let expr = self.transform_macro_expr(&macro_name, &stmt_macro.mac.tokens);
                Ok(vec![Stmt::Expr(expr)])
            }
        }
    }

    fn transform_use(&mut self, u: &syn::ItemUse) -> Result<Vec<Item>> {
        let mut items = Vec::new();
        self.collect_use_tree(Vec::new(), &u.tree, &mut items)?;
        Ok(items)
    }

    fn collect_use_tree(
        &mut self,
        prefix: Vec<String>,
        tree: &syn::UseTree,
        items: &mut Vec<Item>,
    ) -> Result<()> {
        match tree {
            syn::UseTree::Path(path) => {
                let mut next_prefix = prefix;
                next_prefix.push(path.ident.to_string());
                self.collect_use_tree(next_prefix, &path.tree, items)
            }
            syn::UseTree::Name(name) => {
                if name.ident == "self" {
                    if let Some(visible) = prefix.last().cloned() {
                        self.type_mapper
                            .register_visible_path(visible, prefix.clone());
                    }
                    items.push(Item::Use(Use {
                        path: prefix,
                        alias: None,
                        glob: false,
                        span: S,
                    }));
                } else {
                    let mut full_path = prefix;
                    full_path.push(name.ident.to_string());
                    self.type_mapper
                        .register_visible_path(name.ident.to_string(), full_path.clone());
                    items.push(Item::Use(Use {
                        path: full_path,
                        alias: None,
                        glob: false,
                        span: S,
                    }));
                }
                Ok(())
            }
            syn::UseTree::Rename(rename) => {
                let mut full_path = prefix;
                full_path.push(rename.ident.to_string());
                self.type_mapper
                    .register_visible_path(rename.rename.to_string(), full_path.clone());
                items.push(Item::Use(Use {
                    path: full_path,
                    alias: Some(rename.rename.to_string()),
                    glob: false,
                    span: S,
                }));
                Ok(())
            }
            syn::UseTree::Glob(_) => {
                items.push(Item::Use(Use {
                    path: prefix,
                    alias: None,
                    glob: true,
                    span: S,
                }));
                Ok(())
            }
            syn::UseTree::Group(group) => {
                for item in &group.items {
                    self.collect_use_tree(prefix.clone(), item, items)?;
                }
                Ok(())
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    // ── Expressions ───────────────────────────────────────────────────────
    // ─────────────────────────────────────────────────────────────────────

    fn transform_expr(&mut self, expr: &syn::Expr) -> Result<Expr> {
        match expr {
            // ── Literals ─────────────────────────────────────────────────
            syn::Expr::Lit(lit) => self.transform_lit(lit),

            // ── Identifiers ───────────────────────────────────────────────
            syn::Expr::Path(p) => {
                if let Some(expr) = self.lookup_value_substitution(&p.path) {
                    return Ok(expr);
                }
                let name = self.resolve_value_path(&p.path);
                Ok(Expr::Ident(name, S))
            }

            // ── Blocks ────────────────────────────────────────────────────
            syn::Expr::Block(b) => {
                let block = self.transform_block(&b.block)?;
                Ok(Expr::Block(block, S))
            }

            // ── Unary ─────────────────────────────────────────────────────
            syn::Expr::Unary(u) => {
                let operand = self.transform_expr(&u.expr)?;
                let op = match u.op {
                    syn::UnOp::Neg(_) => UnaryOp::Neg,
                    syn::UnOp::Not(_) => UnaryOp::Not,
                    syn::UnOp::Deref(_) => UnaryOp::Deref,
                    _ => UnaryOp::Not,
                };
                Ok(Expr::Unary {
                    op,
                    operand: Box::new(operand),
                    span: S,
                })
            }

            // ── Binary ────────────────────────────────────────────────────
            syn::Expr::Binary(b) => {
                let left = self.transform_expr(&b.left)?;
                let right = self.transform_expr(&b.right)?;
                if let Some(assign_op) = compound_assign_rhs_op(&b.op) {
                    let target = left.clone();
                    let value = Expr::Binary {
                        left: Box::new(left),
                        op: assign_op,
                        right: Box::new(right),
                        span: S,
                    };
                    Ok(Expr::Assign {
                        target: Box::new(target),
                        value: Box::new(value),
                        span: S,
                    })
                } else {
                    let op = binop(&b.op);
                    Ok(Expr::Binary {
                        left: Box::new(left),
                        op,
                        right: Box::new(right),
                        span: S,
                    })
                }
            }

            // ── Assignment ────────────────────────────────────────────────
            syn::Expr::Assign(a) => {
                let target = self.transform_expr(&a.left)?;
                let value = self.transform_expr(&a.right)?;
                Ok(Expr::Assign {
                    target: Box::new(target),
                    value: Box::new(value),
                    span: S,
                })
            }

            // ── Field access ──────────────────────────────────────────────
            syn::Expr::Field(f) => {
                let object = self.transform_expr(&f.base)?;
                let field = match &f.member {
                    syn::Member::Named(ident) => self.rename_field(&ident.to_string()),
                    syn::Member::Unnamed(idx) => format!("field_{}", idx.index),
                };
                Ok(Expr::Field {
                    object: Box::new(object),
                    field,
                    span: S,
                })
            }

            // ── Index ─────────────────────────────────────────────────────
            syn::Expr::Index(i) => {
                let object = self.transform_expr(&i.expr)?;
                let index = self.transform_expr(&i.index)?;
                Ok(Expr::Index {
                    object: Box::new(object),
                    index: Box::new(index),
                    span: S,
                })
            }

            // ── Function call ─────────────────────────────────────────────
            syn::Expr::Call(c) => {
                let callee = self.transform_expr(&c.func)?;
                let args = self.transform_call_args(&c.args)?;
                Ok(Expr::Call {
                    callee: Box::new(callee),
                    args,
                    span: S,
                })
            }

            // ── Method call ───────────────────────────────────────────────
            syn::Expr::MethodCall(m) => {
                let receiver = self.transform_expr(&m.receiver)?;
                let method = self.rename_value(&m.method.to_string());
                let args = self.transform_call_args(&m.args)?;
                Ok(Expr::MethodCall {
                    receiver: Box::new(receiver),
                    method,
                    args,
                    span: S,
                })
            }

            // ── Struct construction ───────────────────────────────────────
            syn::Expr::Struct(s) => {
                let name = self.resolve_type_path(&s.path);
                let fields = s
                    .fields
                    .iter()
                    .map(|fv| {
                        let field_name = self.rename_field(&member_name(&fv.member));
                        let val = self.transform_expr(&fv.expr)?;
                        Ok((field_name, val))
                    })
                    .collect::<Result<Vec<_>>>()?;
                let rest = s
                    .rest
                    .as_ref()
                    .map(|expr| self.transform_expr(expr).map(Box::new))
                    .transpose()?;
                Ok(Expr::Struct {
                    name,
                    fields,
                    rest,
                    span: S,
                })
            }

            // ── Array ─────────────────────────────────────────────────────
            syn::Expr::Array(a) => {
                let items = a
                    .elems
                    .iter()
                    .map(|e| self.transform_expr(e))
                    .collect::<Result<Vec<_>>>()?;
                Ok(Expr::Array(items, S))
            }

            syn::Expr::Repeat(r) => {
                // [expr; N] → fill array
                let elem = self.transform_expr(&r.expr)?;
                self.note_lossy("array repeat simplified to single-element array".to_string());
                Ok(Expr::Array(vec![elem], S)) // simplified — just emit one element
            }

            // ── Tuple ─────────────────────────────────────────────────────
            syn::Expr::Tuple(t) => {
                if t.elems.is_empty() {
                    return Ok(Expr::Block(
                        Block {
                            stmts: vec![],
                            span: S,
                        },
                        S,
                    ));
                }
                let items = t
                    .elems
                    .iter()
                    .map(|e| self.transform_expr(e))
                    .collect::<Result<Vec<_>>>()?;
                Ok(Expr::Tuple(items, S))
            }

            // ── Closure / lambda ──────────────────────────────────────────
            syn::Expr::Closure(cl) => {
                self.push_scope();
                self.push_value_substitution_scope();
                let mut params = Vec::with_capacity(cl.inputs.len());
                for pat in &cl.inputs {
                    let param = self.build_closure_param(pat);
                    self.define(&param.name, param.ty.clone());
                    params.push(param);
                }
                let return_type = match &cl.output {
                    syn::ReturnType::Default => None,
                    syn::ReturnType::Type(_, ty) => Some(self.map_type_checked(ty)),
                };
                let body = self.transform_expr(&cl.body)?;
                self.pop_value_substitution_scope();
                self.pop_scope();
                Ok(Expr::Lambda {
                    params,
                    return_type,
                    body: Box::new(body),
                    span: S,
                })
            }

            // ── If / if let ───────────────────────────────────────────────
            syn::Expr::If(i) => self.transform_if_expr(i),

            // ── Match ─────────────────────────────────────────────────────
            syn::Expr::Match(m) => {
                let scrutinee = self.transform_expr(&m.expr)?;
                let arms = m
                    .arms
                    .iter()
                    .map(|arm| self.transform_arm(arm))
                    .collect::<Result<Vec<_>>>()?;
                Ok(Expr::Match {
                    scrutinee: Box::new(scrutinee),
                    arms,
                    span: S,
                })
            }

            // ── Loop / while / for ────────────────────────────────────────
            syn::Expr::Loop(l) => {
                let body = self.transform_block(&l.body)?;
                Ok(Expr::Block(
                    Block {
                        stmts: vec![Stmt::Loop { body, span: S }],
                        span: S,
                    },
                    S,
                ))
            }

            syn::Expr::While(w) => self.transform_while_expr(w),

            syn::Expr::ForLoop(f) => {
                let binding = self.transform_pattern(&f.pat);
                let iter = self.transform_expr(&f.expr)?;
                let body = self.transform_block(&f.body)?;
                Ok(Expr::Block(
                    Block {
                        stmts: vec![Stmt::For {
                            binding,
                            iter,
                            body,
                            span: S,
                        }],
                        span: S,
                    },
                    S,
                ))
            }

            // ── Return / break / continue ─────────────────────────────────
            syn::Expr::Return(r) => {
                let val = r
                    .expr
                    .as_ref()
                    .map(|e| self.transform_expr(e))
                    .transpose()?;
                Ok(Expr::Return(val.map(Box::new), S))
            }

            syn::Expr::Break(b) => {
                let val = b
                    .expr
                    .as_ref()
                    .map(|e| self.transform_expr(e))
                    .transpose()?;
                Ok(Expr::Break(val.map(Box::new), S))
            }

            syn::Expr::Continue(_) => Ok(Expr::Continue(S)),

            // ── ? operator ───────────────────────────────────────────────
            syn::Expr::Try(t) => {
                let inner = self.transform_expr(&t.expr)?;
                Ok(Expr::Try(Box::new(inner), S))
            }

            // ── await ─────────────────────────────────────────────────────
            syn::Expr::Await(a) => {
                let inner = self.transform_expr(&a.base)?;
                Ok(Expr::Await(Box::new(inner), S))
            }

            // ── async block ───────────────────────────────────────────────
            syn::Expr::Async(a) => {
                let block = self.transform_block(&a.block)?;
                Ok(Expr::AsyncBlock(Box::new(Expr::Block(block, S)), S))
            }

            // ── Cast: `expr as T` ─────────────────────────────────────────
            syn::Expr::Cast(c) => {
                let value = self.transform_expr(&c.expr)?;
                let target = self.map_type_checked(&c.ty);
                Ok(Expr::Cast {
                    value: Box::new(value),
                    target,
                    span: S,
                })
            }

            // ── Reference: `&expr` / `&mut expr` ─────────────────────────
            syn::Expr::Reference(r) => {
                let value = self.transform_expr(&r.expr)?;
                Ok(Expr::Ref {
                    mutable: r.mutability.is_some(),
                    value: Box::new(value),
                    span: S,
                })
            }

            // ── Dereference: `*expr` ──────────────────────────────────────
            // ── Paren ─────────────────────────────────────────────────────
            syn::Expr::Paren(p) => {
                let inner = self.transform_expr(&p.expr)?;
                Ok(Expr::Paren(Box::new(inner), S))
            }

            // ── Range ─────────────────────────────────────────────────────
            syn::Expr::Range(r) => {
                let start = r
                    .start
                    .as_ref()
                    .map(|e| self.transform_expr(e))
                    .transpose()?;
                let end = r.end.as_ref().map(|e| self.transform_expr(e)).transpose()?;
                let inclusive = matches!(r.limits, syn::RangeLimits::Closed(_));
                Ok(Expr::Range {
                    start: start.map(Box::new),
                    end: end.map(Box::new),
                    inclusive,
                    span: S,
                })
            }

            // ── Macro calls (println!, vec!, format!, etc.) ───────────────
            syn::Expr::Macro(m) => {
                let macro_name = path_to_ident(&m.mac.path);
                Ok(self.transform_macro_expr(&macro_name, &m.mac.tokens))
            }

            // ── Unsafe block ─────────────────────────────────────────────
            syn::Expr::Unsafe(u) => {
                // Unwrap unsafe block — the unsafe effect is tracked at function level
                let block = self.transform_block(&u.block)?;
                Ok(Expr::Block(block, S))
            }

            // ── Type ascription / const blocks / etc. ─────────────────────
            syn::Expr::Const(c) => {
                let block = self.transform_block(&c.block)?;
                Ok(Expr::Comptime(Box::new(Expr::Block(block, S)), S))
            }

            // ── Verbatim / unknown ─────────────────────────────────────────
            other => {
                self.note_lossy_class(
                    "unsupported_expr_lowering",
                    format!("unsupported expression kind: {}", Self::expr_kind_name(other)),
                );
                Ok(Expr::None(S))
            }
        }
    }

    // ── Literals ─────────────────────────────────────────────────────────

    fn transform_lit(&mut self, lit: &syn::ExprLit) -> Result<Expr> {
        match &lit.lit {
            syn::Lit::Int(i) => {
                let val = i.base10_parse::<i64>().unwrap_or(0);
                Ok(Expr::Int(val, S))
            }
            syn::Lit::Float(f) => {
                let val = f.base10_parse::<f64>().unwrap_or(0.0);
                Ok(Expr::Float(val, S))
            }
            syn::Lit::Bool(b) => Ok(Expr::Bool(b.value, S)),
            syn::Lit::Str(s) => Ok(Expr::String(s.value(), S)),
            syn::Lit::Char(c) => Ok(Expr::String(c.value().to_string(), S)),
            syn::Lit::Byte(b) => Ok(Expr::Int(b.value() as i64, S)),
            syn::Lit::ByteStr(bs) => {
                // Byte strings → array of ints
                let items = bs.value().iter().map(|&b| Expr::Int(b as i64, S)).collect();
                Ok(Expr::Array(items, S))
            }
            _ => {
                self.note_lossy_class(
                    "unsupported_literal_lowering",
                    format!("unsupported literal kind: {}", Self::lit_kind_name(&lit.lit)),
                );
                Ok(Expr::None(S))
            }
        }
    }

    // ── If conditions (handles `if let`) ─────────────────────────────────

    fn transform_if_expr(&mut self, expr: &syn::ExprIf) -> Result<Expr> {
        if let syn::Expr::Let(let_expr) = expr.cond.as_ref() {
            return self.desugar_if_let(
                let_expr,
                &expr.then_branch,
                expr.else_branch
                    .as_ref()
                    .map(|(_, else_expr)| else_expr.as_ref()),
            );
        }

        let condition = self.transform_if_condition(&expr.cond)?;
        let then_branch = self.transform_block(&expr.then_branch)?;
        let else_branch = if let Some((_, else_expr)) = &expr.else_branch {
            Some(Box::new(self.transform_else(else_expr)?))
        } else {
            None
        };
        Ok(Expr::If {
            condition: Box::new(condition),
            then_branch,
            else_branch,
            span: S,
        })
    }

    fn desugar_if_let(
        &mut self,
        let_expr: &syn::ExprLet,
        then_branch: &syn::Block,
        else_expr: Option<&syn::Expr>,
    ) -> Result<Expr> {
        let scrutinee = self.transform_expr(&let_expr.expr)?;
        let pattern = self.transform_pattern(&let_expr.pat);
        let then_branch = self.transform_block(then_branch)?;
        let else_body = match else_expr {
            Some(expr) => self.transform_expr(expr)?,
            None => Expr::None(S),
        };

        Ok(Expr::Match {
            scrutinee: Box::new(scrutinee),
            arms: vec![
                MatchArm {
                    pattern,
                    guard: None,
                    body: Expr::Block(then_branch, S),
                    span: S,
                },
                MatchArm {
                    pattern: Pattern::Wildcard(S),
                    guard: None,
                    body: else_body,
                    span: S,
                },
            ],
            span: S,
        })
    }

    fn transform_while_expr(&mut self, expr: &syn::ExprWhile) -> Result<Expr> {
        if let syn::Expr::Let(let_expr) = expr.cond.as_ref() {
            return self.desugar_while_let(let_expr, &expr.body);
        }

        let condition = self.transform_if_condition(&expr.cond)?;
        let body = self.transform_block(&expr.body)?;
        Ok(Expr::Block(
            Block {
                stmts: vec![Stmt::While {
                    condition,
                    body,
                    span: S,
                }],
                span: S,
            },
            S,
        ))
    }

    fn desugar_while_let(&mut self, let_expr: &syn::ExprLet, body: &syn::Block) -> Result<Expr> {
        let scrutinee = self.transform_expr(&let_expr.expr)?;
        let pattern = self.transform_pattern(&let_expr.pat);
        let then_body = self.transform_block(body)?;
        let match_expr = Expr::Match {
            scrutinee: Box::new(scrutinee),
            arms: vec![
                MatchArm {
                    pattern,
                    guard: None,
                    body: Expr::Block(then_body, S),
                    span: S,
                },
                MatchArm {
                    pattern: Pattern::Wildcard(S),
                    guard: None,
                    body: Expr::Break(None, S),
                    span: S,
                },
            ],
            span: S,
        };

        Ok(Expr::Block(
            Block {
                stmts: vec![Stmt::Loop {
                    body: Block {
                        stmts: vec![Stmt::Expr(match_expr)],
                        span: S,
                    },
                    span: S,
                }],
                span: S,
            },
            S,
        ))
    }

    fn transform_if_condition(&mut self, cond: &syn::Expr) -> Result<Expr> {
        match cond {
            syn::Expr::Let(let_expr) => {
                // Let-chains are handled at the containing `if` / `while` expression level.
                // Any remaining bare `let` condition would still be lossy.
                self.note_lossy("if-let condition simplified (binding erased)".to_string());
                self.transform_expr(&let_expr.expr)
            }
            other => self.transform_expr(other),
        }
    }

    fn transform_else(&mut self, else_expr: &syn::Expr) -> Result<ElseBranch> {
        match else_expr {
            syn::Expr::Block(b) => {
                let block = self.transform_block(&b.block)?;
                Ok(ElseBranch::Else(block))
            }
            syn::Expr::If(i) => match self.transform_if_expr(i)? {
                Expr::If {
                    condition,
                    then_branch,
                    else_branch,
                    ..
                } => Ok(ElseBranch::ElseIf(condition, then_branch, else_branch)),
                expr => {
                    let block = Block {
                        stmts: vec![Stmt::Expr(expr)],
                        span: S,
                    };
                    Ok(ElseBranch::Else(block))
                }
            },
            other => {
                let expr = self.transform_expr(other)?;
                let block = Block {
                    stmts: vec![Stmt::Expr(expr)],
                    span: S,
                };
                Ok(ElseBranch::Else(block))
            }
        }
    }

    // ── Match arms ───────────────────────────────────────────────────────

    fn transform_arm(&mut self, arm: &syn::Arm) -> Result<MatchArm> {
        let pattern = self.transform_pattern(&arm.pat);
        let guard = arm
            .guard
            .as_ref()
            .map(|(_, expr)| self.transform_expr(expr))
            .transpose()?;
        let body = self.transform_expr(&arm.body)?;
        Ok(MatchArm {
            pattern,
            guard,
            body,
            span: S,
        })
    }

    // ── Patterns ─────────────────────────────────────────────────────────

    fn transform_pattern(&mut self, pat: &syn::Pat) -> Pattern {
        match pat {
            syn::Pat::Ident(pi) => {
                let name = self.rename_value(&pi.ident.to_string());
                let mutable = pi.mutability.is_some();
                Pattern::Binding {
                    name,
                    mutable,
                    span: S,
                }
            }
            syn::Pat::Wild(_) => Pattern::Wildcard(S),
            syn::Pat::Lit(pl) => {
                if let Ok(expr) = self.transform_lit(pl) {
                    Pattern::Literal(expr)
                } else {
                    Pattern::Wildcard(S)
                }
            }
            syn::Pat::Tuple(pt) => {
                let pats = pt.elems.iter().map(|p| self.transform_pattern(p)).collect();
                Pattern::Tuple(pats, S)
            }
            syn::Pat::TupleStruct(pts) => {
                let (enum_name, name) = self.pattern_variant_head(&pts.path);
                let inner = pts
                    .elems
                    .iter()
                    .map(|p| self.transform_pattern(p))
                    .collect();
                Pattern::Variant {
                    enum_name,
                    variant: name,
                    fields: VariantPatternFields::Tuple(inner),
                    span: S,
                }
            }
            syn::Pat::Struct(ps) => {
                let (enum_name, name) = self.pattern_variant_head(&ps.path);
                let fields = ps
                    .fields
                    .iter()
                    .map(|fv| {
                        let field = member_name(&fv.member);
                        let pat = self.transform_pattern(&fv.pat);
                        (field, pat)
                    })
                    .collect();
                Pattern::Variant {
                    enum_name,
                    variant: name,
                    fields: VariantPatternFields::Struct(fields),
                    span: S,
                }
            }
            syn::Pat::Path(pp) => {
                let (enum_name, name) = self.pattern_variant_head(&pp.path);
                Pattern::Variant {
                    enum_name,
                    variant: name,
                    fields: VariantPatternFields::Unit,
                    span: S,
                }
            }
            syn::Pat::Range(pr) => {
                let start = pr
                    .start
                    .as_ref()
                    .and_then(|e| self.transform_expr(e).ok())
                    .map(Box::new);
                let end = pr
                    .end
                    .as_ref()
                    .and_then(|e| self.transform_expr(e).ok())
                    .map(Box::new);
                if start.is_some() || end.is_some() {
                    Pattern::Range {
                        start,
                        end,
                        inclusive: matches!(pr.limits, syn::RangeLimits::Closed(_)),
                        span: S,
                    }
                } else {
                    Pattern::Wildcard(S)
                }
            }
            syn::Pat::Or(po) => {
                let cases = po.cases.iter().map(|p| self.transform_pattern(p)).collect();
                Pattern::Or(cases, S)
            }
            syn::Pat::Paren(pp) => self.transform_pattern(&pp.pat),
            syn::Pat::Reference(pr) => {
                // `&x` / `&mut x` pattern — unwrap to inner
                self.transform_pattern(&pr.pat)
            }
            syn::Pat::Rest(_) => Pattern::Wildcard(S),
            syn::Pat::Slice(ps) => {
                let pats = ps.elems.iter().map(|p| self.transform_pattern(p)).collect();
                Pattern::Slice {
                    patterns: pats,
                    rest: None,
                    span: S,
                }
            }
            syn::Pat::Type(pt) => self.transform_pattern(&pt.pat),
            _ => {
                self.note_lossy_class(
                    "unsupported_pattern_lowering",
                    "unsupported pattern lowered to wildcard (binding loss possible)".to_string(),
                );
                Pattern::Wildcard(S)
            }
        }
    }

    #[allow(dead_code)]
    fn pattern_to_name(&self, pat: &syn::Pat) -> String {
        match pat {
            syn::Pat::Ident(pi) => {
                let raw = pi.ident.to_string();
                if raw == "self" {
                    "_self".to_string()
                } else {
                    raw
                }
            }
            syn::Pat::Wild(_) => "_".to_string(),
            syn::Pat::Reference(r) => self.pattern_to_name(&r.pat),
            _ => "_".to_string(),
        }
    }

    fn local_pat_name(&self, pat: &syn::Pat) -> (String, bool) {
        match pat {
            syn::Pat::Ident(pi) => {
                let raw = pi.ident.to_string();
                let name = if raw == "self" {
                    "_self".to_string()
                } else {
                    raw
                };
                (name, pi.mutability.is_some())
            }
            syn::Pat::Wild(_) => ("_".to_string(), false),
            syn::Pat::Type(pt) => self.local_pat_name(&pt.pat),
            _ => ("_".to_string(), false),
        }
    }

    fn type_from_local_pat(&mut self, pat: &syn::Pat) -> Option<Type> {
        if let syn::Pat::Type(pt) = pat {
            Some(self.map_type_checked(&pt.ty))
        } else {
            None
        }
    }

    // ── Call args ────────────────────────────────────────────────────────

    fn transform_call_args(
        &mut self,
        args: &syn::punctuated::Punctuated<syn::Expr, syn::token::Comma>,
    ) -> Result<Vec<CallArg>> {
        args.iter()
            .map(|e| {
                let value = self.transform_expr(e)?;
                Ok(CallArg {
                    name: None,
                    value,
                    span: S,
                })
            })
            .collect()
    }

    // ── Macro arg parsing ─────────────────────────────────────────────────

    fn parse_macro_args(&mut self, tokens: &proc_macro2::TokenStream) -> Vec<Expr> {
        // Best-effort: try parse as comma-separated syn expressions
        struct CommaSep(syn::punctuated::Punctuated<syn::Expr, syn::token::Comma>);
        impl syn::parse::Parse for CommaSep {
            fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
                Ok(CommaSep(syn::punctuated::Punctuated::parse_terminated(
                    input,
                )?))
            }
        }
        if let Ok(CommaSep(exprs)) = syn::parse2::<CommaSep>(tokens.clone()) {
            exprs
                .iter()
                .filter_map(|e| self.transform_expr(e).ok())
                .collect()
        } else {
            // Raw string fallback for format strings etc.
            vec![]
        }
    }

    fn transform_macro_expr(
        &mut self,
        macro_name: &str,
        tokens: &proc_macro2::TokenStream,
    ) -> Expr {
        if self
            .options
            .macro_policy
            .lower_directly
            .contains(macro_name)
        {
            match macro_name {
                "assert" | "debug_assert" => {
                    if let Some(expr) = self.lower_assert_macro(macro_name, tokens) {
                        return expr;
                    }
                    self.note_lossy_class(
                        "macro_direct_lowering_miss",
                        format!("{macro_name}! could not be lowered directly"),
                    );
                }
                "assert_eq" => {
                    if let Some(expr) = self.lower_assert_eq_macro(tokens) {
                        return expr;
                    }
                    self.note_lossy_class(
                        "macro_direct_lowering_miss",
                        "assert_eq! could not be lowered directly".to_string(),
                    );
                }
                "vec" => {
                    let args = self.parse_macro_args(tokens);
                    return Expr::Array(args, S);
                }
                "format" => {
                    if let Some(expr) = self.lower_format_macro(tokens) {
                        return expr;
                    }
                    self.note_lossy_class(
                        "macro_direct_lowering_miss",
                        "format! could not be lowered directly".to_string(),
                    );
                }
                "matches" => {
                    if let Some(expr) = self.lower_matches_macro(tokens) {
                        return expr;
                    }
                    self.note_lossy_class(
                        "macro_direct_lowering_miss",
                        "matches! could not be lowered directly".to_string(),
                    );
                }
                "panic" => {
                    if let Some(expr) = self.lower_panic_macro(tokens) {
                        return expr;
                    }
                    self.note_lossy_class(
                        "macro_direct_lowering_miss",
                        "panic! could not be lowered directly".to_string(),
                    );
                }
                "print" | "println" | "eprint" | "eprintln" => {
                    if let Some(expr) = self.lower_print_macro(macro_name, tokens) {
                        return expr;
                    }
                    self.note_lossy_class(
                        "macro_direct_lowering_miss",
                        format!("{macro_name}! could not be lowered directly"),
                    );
                }
                "unreachable" => {
                    if let Some(expr) = self.lower_unreachable_macro(tokens) {
                        return expr;
                    }
                    self.note_lossy_class(
                        "macro_direct_lowering_miss",
                        "unreachable! could not be lowered directly".to_string(),
                    );
                }
                "write" | "writeln" => {
                    if let Some(expr) = self.lower_write_macro(macro_name, tokens) {
                        return expr;
                    }
                    self.note_lossy_class(
                        "macro_direct_lowering_miss",
                        format!("{macro_name}! could not be lowered directly"),
                    );
                }
                _ => {}
            }
        }

        if self.options.macro_policy.reject.contains(macro_name) {
            self.note_lossy_class(
                "macro_policy_rejected",
                format!("macro {macro_name}! is rejected by self-host policy"),
            );
        }

        let args = self.parse_macro_args(tokens);
        Expr::MacroCall {
            name: macro_name.to_string(),
            args,
            span: S,
        }
    }

    fn panic_call(&self, message: Expr) -> Expr {
        Expr::Call {
            callee: Box::new(Expr::Ident("panic".to_string(), S)),
            args: vec![CallArg {
                name: None,
                value: message,
                span: S,
            }],
            span: S,
        }
    }

    fn lower_assert_macro(
        &mut self,
        macro_name: &str,
        tokens: &proc_macro2::TokenStream,
    ) -> Option<Expr> {
        struct AssertMacroInput {
            condition: syn::Expr,
            tail: Option<(syn::token::Comma, proc_macro2::TokenStream)>,
        }

        impl syn::parse::Parse for AssertMacroInput {
            fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
                let condition = input.parse()?;
                let tail = if input.is_empty() {
                    None
                } else {
                    Some((input.parse()?, input.parse()?))
                };
                Ok(Self { condition, tail })
            }
        }

        let parsed = syn::parse2::<AssertMacroInput>(tokens.clone()).ok()?;
        let condition = self.transform_expr(&parsed.condition).ok()?;
        let message = parsed
            .tail
            .as_ref()
            .and_then(|(_, rest)| self.lower_format_macro(rest))
            .or_else(|| {
                parsed.tail.as_ref().map(|(_, rest)| {
                    let args = self.parse_macro_args(rest);
                    match args.as_slice() {
                        [] => Expr::String(format!("{macro_name}! failed"), S),
                        [single] => single.clone(),
                        _ => Expr::MacroCall {
                            name: "format".to_string(),
                            args,
                            span: S,
                        },
                    }
                })
            })
            .unwrap_or_else(|| Expr::String(format!("{macro_name}! failed"), S));

        Some(Expr::If {
            condition: Box::new(Expr::Unary {
                op: UnaryOp::Not,
                operand: Box::new(condition),
                span: S,
            }),
            then_branch: Block {
                stmts: vec![Stmt::Expr(self.panic_call(message))],
                span: S,
            },
            else_branch: None,
            span: S,
        })
    }

    fn lower_assert_eq_macro(&mut self, tokens: &proc_macro2::TokenStream) -> Option<Expr> {
        struct AssertEqMacroInput {
            left: syn::Expr,
            _comma: syn::token::Comma,
            right: syn::Expr,
            tail: Option<(syn::token::Comma, proc_macro2::TokenStream)>,
        }

        impl syn::parse::Parse for AssertEqMacroInput {
            fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
                let left = input.parse()?;
                let _comma = input.parse()?;
                let right = input.parse()?;
                let tail = if input.is_empty() {
                    None
                } else {
                    Some((input.parse()?, input.parse()?))
                };
                Ok(Self {
                    left,
                    _comma,
                    right,
                    tail,
                })
            }
        }

        let parsed = syn::parse2::<AssertEqMacroInput>(tokens.clone()).ok()?;
        let left = self.transform_expr(&parsed.left).ok()?;
        let right = self.transform_expr(&parsed.right).ok()?;
        let message = parsed
            .tail
            .as_ref()
            .and_then(|(_, rest)| self.lower_format_macro(rest))
            .unwrap_or_else(|| Expr::String("assert_eq! failed".to_string(), S));

        Some(Expr::If {
            condition: Box::new(Expr::Binary {
                left: Box::new(left),
                op: BinaryOp::Ne,
                right: Box::new(right),
                span: S,
            }),
            then_branch: Block {
                stmts: vec![Stmt::Expr(self.panic_call(message))],
                span: S,
            },
            else_branch: None,
            span: S,
        })
    }

    fn lower_panic_macro(&mut self, tokens: &proc_macro2::TokenStream) -> Option<Expr> {
        let message = self.lower_format_macro(tokens).or_else(|| {
            let args = self.parse_macro_args(tokens);
            match args.as_slice() {
                [] => Some(Expr::String("panic!".to_string(), S)),
                [single] => Some(single.clone()),
                _ => Some(Expr::MacroCall {
                    name: "format".to_string(),
                    args,
                    span: S,
                }),
            }
        })?;
        Some(self.panic_call(message))
    }

    fn lower_unreachable_macro(&mut self, tokens: &proc_macro2::TokenStream) -> Option<Expr> {
        let message = self.lower_format_macro(tokens).or_else(|| {
            let args = self.parse_macro_args(tokens);
            match args.as_slice() {
                [] => Some(Expr::String("unreachable!".to_string(), S)),
                [single] => Some(single.clone()),
                _ => Some(Expr::MacroCall {
                    name: "format".to_string(),
                    args,
                    span: S,
                }),
            }
        })?;
        Some(self.panic_call(message))
    }

    fn lower_format_macro(&mut self, tokens: &proc_macro2::TokenStream) -> Option<Expr> {
        struct FormatMacroInput {
            format: syn::Expr,
            args: syn::punctuated::Punctuated<FormatMacroArg, syn::token::Comma>,
        }

        enum FormatMacroArg {
            Positional(syn::Expr),
            Named(String, syn::Expr),
        }

        impl syn::parse::Parse for FormatMacroArg {
            fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
                if input.peek(syn::Ident) && input.peek2(syn::Token![=]) {
                    let name: syn::Ident = input.parse()?;
                    let _eq: syn::Token![=] = input.parse()?;
                    let value: syn::Expr = input.parse()?;
                    Ok(Self::Named(name.to_string(), value))
                } else {
                    Ok(Self::Positional(input.parse()?))
                }
            }
        }

        impl syn::parse::Parse for FormatMacroInput {
            fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
                let format = input.parse()?;
                let args = if input.is_empty() {
                    syn::punctuated::Punctuated::new()
                } else {
                    let _comma: syn::token::Comma = input.parse()?;
                    syn::punctuated::Punctuated::parse_terminated(input)?
                };
                Ok(Self { format, args })
            }
        }

        let parsed = syn::parse2::<FormatMacroInput>(tokens.clone()).ok()?;
        let fmt = match self.transform_expr(&parsed.format).ok()? {
            Expr::String(fmt, _) => fmt,
            _ => return None,
        };
        let mut positional = Vec::new();
        let mut named = HashMap::new();
        for arg in parsed.args {
            match arg {
                FormatMacroArg::Positional(value) => {
                    positional.push(self.transform_expr(&value).ok()?)
                }
                FormatMacroArg::Named(name, value) => {
                    named.insert(name, self.transform_expr(&value).ok()?);
                }
            }
        }
        let parts = self.interpolate_format_string(&fmt, positional, &named)?;
        Some(match parts.as_slice() {
            [] => Expr::String(String::new(), S),
            [single] => single.clone(),
            _ => Expr::FString(parts, S),
        })
    }

    fn lower_print_macro(
        &mut self,
        macro_name: &str,
        tokens: &proc_macro2::TokenStream,
    ) -> Option<Expr> {
        let args = if let Some(expr) = self.lower_format_macro(tokens) {
            vec![CallArg {
                name: None,
                value: expr,
                span: S,
            }]
        } else {
            self.parse_macro_args(tokens)
                .into_iter()
                .map(|value| CallArg {
                    name: None,
                    value,
                    span: S,
                })
                .collect::<Vec<_>>()
        };

        Some(Expr::Call {
            callee: Box::new(Expr::Ident(macro_name.to_string(), S)),
            args,
            span: S,
        })
    }

    fn lower_write_macro(
        &mut self,
        macro_name: &str,
        tokens: &proc_macro2::TokenStream,
    ) -> Option<Expr> {
        struct WriteMacroInput {
            dest: syn::Expr,
            rest: Option<(syn::token::Comma, proc_macro2::TokenStream)>,
        }

        impl syn::parse::Parse for WriteMacroInput {
            fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
                Ok(Self {
                    dest: input.parse()?,
                    rest: if input.is_empty() {
                        None
                    } else {
                        Some((input.parse()?, input.parse()?))
                    },
                })
            }
        }

        let parsed = syn::parse2::<WriteMacroInput>(tokens.clone()).ok()?;
        let dest = self.transform_expr(&parsed.dest).ok()?;
        let message = match &parsed.rest {
            Some((_, rest)) => self
                .lower_format_macro(rest)
                .unwrap_or_else(|| Expr::MacroCall {
                    name: "format".to_string(),
                    args: self.parse_macro_args(rest),
                    span: S,
                }),
            None => Expr::String(String::new(), S),
        };
        let helper_name = if macro_name == "writeln" {
            "__kain_writeln_fmt"
        } else {
            "__kain_write_fmt"
        };
        Some(Expr::MacroCall {
            name: helper_name.to_string(),
            args: vec![dest, message],
            span: S,
        })
    }

    fn lower_matches_macro(&mut self, tokens: &proc_macro2::TokenStream) -> Option<Expr> {
        struct MatchesMacroInput {
            scrutinee: syn::Expr,
            _comma: syn::token::Comma,
            pattern: syn::Pat,
            guard: Option<(syn::Token![if], syn::Expr)>,
        }

        impl syn::parse::Parse for MatchesMacroInput {
            fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
                Ok(Self {
                    scrutinee: input.parse()?,
                    _comma: input.parse()?,
                    pattern: input.call(syn::Pat::parse_multi)?,
                    guard: if input.peek(syn::Token![if]) {
                        Some((input.parse()?, input.parse()?))
                    } else {
                        None
                    },
                })
            }
        }

        let parsed = syn::parse2::<MatchesMacroInput>(tokens.clone()).ok()?;
        let scrutinee = self.transform_expr(&parsed.scrutinee).ok()?;
        let pattern = self.transform_pattern(&parsed.pattern);
        let guard = parsed
            .guard
            .as_ref()
            .and_then(|(_, expr)| self.transform_expr(expr).ok());

        Some(Expr::Match {
            scrutinee: Box::new(scrutinee),
            arms: vec![
                MatchArm {
                    pattern,
                    guard,
                    body: Expr::Bool(true, S),
                    span: S,
                },
                MatchArm {
                    pattern: Pattern::Wildcard(S),
                    guard: None,
                    body: Expr::Bool(false, S),
                    span: S,
                },
            ],
            span: S,
        })
    }

    fn interpolate_format_string(
        &mut self,
        fmt: &str,
        positional: Vec<Expr>,
        named: &HashMap<String, Expr>,
    ) -> Option<Vec<Expr>> {
        let mut parts = Vec::new();
        let mut literal = String::new();
        let mut chars = fmt.chars().peekable();
        let mut next_positional = 0usize;

        while let Some(ch) = chars.next() {
            if ch == '{' {
                if chars.peek() == Some(&'{') {
                    chars.next();
                    literal.push('{');
                    continue;
                }

                if !literal.is_empty() {
                    parts.push(Expr::String(std::mem::take(&mut literal), S));
                }

                let mut placeholder = String::new();
                let mut closed = false;
                for next in chars.by_ref() {
                    if next == '}' {
                        closed = true;
                        break;
                    }
                    placeholder.push(next);
                }
                if !closed {
                    return None;
                }

                let selector = placeholder
                    .split_once(':')
                    .map(|(head, _)| head)
                    .unwrap_or(placeholder.as_str())
                    .trim();

                let value = if selector.is_empty() {
                    let value = positional.get(next_positional)?.clone();
                    next_positional += 1;
                    value
                } else if selector.chars().all(|ch| ch.is_ascii_digit()) {
                    positional.get(selector.parse::<usize>().ok()?)?.clone()
                } else if let Some(value) = named.get(selector) {
                    value.clone()
                } else if let Ok(expr) = syn::parse_str::<syn::Expr>(selector) {
                    self.transform_expr(&expr).ok()?
                } else {
                    return None;
                };

                parts.push(value);
            } else if ch == '}' {
                if chars.peek() == Some(&'}') {
                    chars.next();
                    literal.push('}');
                } else {
                    return None;
                }
            } else {
                literal.push(ch);
            }
        }

        if !literal.is_empty() {
            parts.push(Expr::String(literal, S));
        }

        Some(parts)
    }

    fn resolve_value_path(&mut self, path: &syn::Path) -> String {
        let resolved = self.type_mapper.resolve_path_segments(path);
        if resolved.len() == 1 {
            self.rename_value(&resolved[0])
        } else {
            resolved.join("::")
        }
    }

    fn resolve_type_path(&mut self, path: &syn::Path) -> String {
        let resolved = self.type_mapper.resolve_path_segments(path);
        if resolved.len() == 1 {
            self.rename_type(&resolved[0])
        } else {
            resolved.join("::")
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ── Free helpers ──────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

const S: Span = Span { start: 0, end: 0 };

fn visibility(vis: &syn::Visibility) -> Visibility {
    match vis {
        syn::Visibility::Public(_) => Visibility::Public,
        syn::Visibility::Restricted(restricted) => {
            if restricted.in_token.is_none() {
                match &*restricted.path {
                    syn::Path {
                        leading_colon: None,
                        segments,
                    } if segments.len() == 1 => {
                        let segment = segments.first().map(|value| value.ident.to_string());
                        match segment.as_deref() {
                            Some("crate") => Visibility::Crate,
                            Some("super") => Visibility::Super,
                            _ => Visibility::Private,
                        }
                    }
                    _ => Visibility::Private,
                }
            } else {
                Visibility::Private
            }
        }
        _ => Visibility::Private,
    }
}

fn path_to_ident(path: &syn::Path) -> String {
    path.segments
        .iter()
        .map(|s| s.ident.to_string())
        .collect::<Vec<_>>()
        .join("::")
}

fn has_cfg_test_attr(attrs: &[syn::Attribute]) -> bool {
    attrs.iter().any(|attr| {
        attr.path().is_ident("cfg")
            && attr
                .parse_args::<syn::Ident>()
                .is_ok_and(|ident| ident == "test")
    })
}

#[allow(dead_code)]
fn variant_name(path: &syn::Path) -> String {
    path.segments
        .last()
        .map(|segment| segment.ident.to_string())
        .unwrap_or_default()
}

fn member_name(member: &syn::Member) -> String {
    match member {
        syn::Member::Named(ident) => ident.to_string(),
        syn::Member::Unnamed(index) => format!("field_{}", index.index),
    }
}

fn dyn_trait_name(ty: &syn::Type) -> Option<String> {
    match ty {
        syn::Type::TraitObject(obj) => obj.bounds.iter().find_map(|bound| {
            if let syn::TypeParamBound::Trait(trait_bound) = bound {
                trait_bound
                    .path
                    .segments
                    .last()
                    .map(|segment| segment.ident.to_string())
            } else {
                None
            }
        }),
        syn::Type::Reference(reference) => dyn_trait_name(&reference.elem),
        syn::Type::Ptr(pointer) => dyn_trait_name(&pointer.elem),
        syn::Type::Array(array) => dyn_trait_name(&array.elem),
        syn::Type::Slice(slice) => dyn_trait_name(&slice.elem),
        syn::Type::Group(group) => dyn_trait_name(&group.elem),
        syn::Type::Paren(paren) => dyn_trait_name(&paren.elem),
        syn::Type::Tuple(tuple) => tuple.elems.iter().find_map(dyn_trait_name),
        syn::Type::BareFn(function) => {
            for input in &function.inputs {
                if let Some(name) = dyn_trait_name(&input.ty) {
                    return Some(name);
                }
            }
            match &function.output {
                syn::ReturnType::Default => None,
                syn::ReturnType::Type(_, output) => dyn_trait_name(output),
            }
        }
        syn::Type::Path(path) => {
            for segment in &path.path.segments {
                if let syn::PathArguments::AngleBracketed(arguments) = &segment.arguments {
                    for arg in &arguments.args {
                        match arg {
                            syn::GenericArgument::Type(inner) => {
                                if let Some(name) = dyn_trait_name(inner) {
                                    return Some(name);
                                }
                            }
                            syn::GenericArgument::AssocType(binding) => {
                                if let Some(name) = dyn_trait_name(&binding.ty) {
                                    return Some(name);
                                }
                            }
                            _ => {}
                        }
                    }
                }
            }
            None
        }
        _ => None,
    }
}

fn binop(op: &syn::BinOp) -> BinaryOp {
    match op {
        syn::BinOp::Add(_) => BinaryOp::Add,
        syn::BinOp::Sub(_) => BinaryOp::Sub,
        syn::BinOp::Mul(_) => BinaryOp::Mul,
        syn::BinOp::Div(_) => BinaryOp::Div,
        syn::BinOp::Rem(_) => BinaryOp::Mod,
        syn::BinOp::And(_) => BinaryOp::And,
        syn::BinOp::Or(_) => BinaryOp::Or,
        syn::BinOp::BitAnd(_) => BinaryOp::BitAnd,
        syn::BinOp::BitOr(_) => BinaryOp::BitOr,
        syn::BinOp::BitXor(_) => BinaryOp::BitXor,
        syn::BinOp::Shl(_) => BinaryOp::Shl,
        syn::BinOp::Shr(_) => BinaryOp::Shr,
        syn::BinOp::Eq(_) => BinaryOp::Eq,
        syn::BinOp::Ne(_) => BinaryOp::Ne,
        syn::BinOp::Lt(_) => BinaryOp::Lt,
        syn::BinOp::Le(_) => BinaryOp::Le,
        syn::BinOp::Gt(_) => BinaryOp::Gt,
        syn::BinOp::Ge(_) => BinaryOp::Ge,
        _ => BinaryOp::Add, // fallback
    }
}

fn compound_assign_rhs_op(op: &syn::BinOp) -> Option<BinaryOp> {
    match op {
        syn::BinOp::AddAssign(_) => Some(BinaryOp::Add),
        syn::BinOp::SubAssign(_) => Some(BinaryOp::Sub),
        syn::BinOp::MulAssign(_) => Some(BinaryOp::Mul),
        syn::BinOp::DivAssign(_) => Some(BinaryOp::Div),
        syn::BinOp::RemAssign(_) => Some(BinaryOp::Mod),
        syn::BinOp::BitAndAssign(_) => Some(BinaryOp::BitAnd),
        syn::BinOp::BitOrAssign(_) => Some(BinaryOp::BitOr),
        syn::BinOp::BitXorAssign(_) => Some(BinaryOp::BitXor),
        syn::BinOp::ShlAssign(_) => Some(BinaryOp::Shl),
        syn::BinOp::ShrAssign(_) => Some(BinaryOp::Shr),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn transform_source(source: &str) -> Program {
        let file = syn::parse_file(source).expect("rust should parse");
        RustTransformer::new_selfhost()
            .transform(file)
            .expect("transform should succeed")
    }

    fn transform_with_diagnostics(source: &str) -> (Program, Vec<String>) {
        let file = syn::parse_file(source).expect("rust should parse");
        let mut transformer = RustTransformer::new_selfhost();
        let program = transformer
            .transform(file)
            .expect("transform should succeed");
        (program, transformer.diagnostics)
    }

    #[test]
    fn lowers_format_macro_to_fstring() {
        let program = transform_source(
            r#"
            fn demo(name: String, count: i32) {
                let msg = format!("Hello {}, {}", name, count);
            }
            "#,
        );

        let Item::Function(func) = &program.items[0] else {
            panic!("expected function");
        };

        let Stmt::Let {
            value: Some(Expr::FString(parts, _)),
            ..
        } = &func.body.stmts[0]
        else {
            panic!("expected lowered fstring");
        };

        assert_eq!(parts.len(), 4);
        assert!(matches!(&parts[0], Expr::String(s, _) if s == "Hello "));
        assert!(matches!(&parts[1], Expr::Ident(name, _) if name == "name"));
        assert!(matches!(&parts[2], Expr::String(s, _) if s == ", "));
        assert!(matches!(&parts[3], Expr::Ident(name, _) if name == "count"));
    }

    #[test]
    fn lowers_matches_macro_to_match_expr() {
        let program = transform_source(
            r#"
            fn demo(value: Option<i32>) {
                let ok = matches!(value, Some(v) if v > 0);
            }
            "#,
        );

        let Item::Function(func) = &program.items[0] else {
            panic!("expected function");
        };

        let Stmt::Let {
            value: Some(Expr::Match { arms, .. }),
            ..
        } = &func.body.stmts[0]
        else {
            panic!("expected lowered match");
        };

        assert_eq!(arms.len(), 2);
        assert!(matches!(arms[0].body, Expr::Bool(true, _)));
        assert!(arms[0].guard.is_some());
        assert!(matches!(arms[1].pattern, Pattern::Wildcard(_)));
        assert!(matches!(arms[1].body, Expr::Bool(false, _)));
    }

    #[test]
    fn lowers_named_capture_format_macro() {
        let program = transform_source(
            r#"
            fn demo(name: String) {
                let msg = format!("Hello {name}");
            }
            "#,
        );

        let Item::Function(func) = &program.items[0] else {
            panic!("expected function");
        };

        let Stmt::Let {
            value: Some(Expr::FString(parts, _)),
            ..
        } = &func.body.stmts[0]
        else {
            panic!("expected lowered fstring");
        };

        assert_eq!(parts.len(), 2);
        assert!(matches!(&parts[0], Expr::String(s, _) if s == "Hello "));
        assert!(matches!(&parts[1], Expr::Ident(name, _) if name == "name"));
    }

    #[test]
    fn lowers_if_let_to_match_without_strict_diagnostic() {
        let (program, diagnostics) = transform_with_diagnostics(
            r#"
            fn demo(value: Option<i32>) {
                if let Some(found) = value {
                    println!("{}", found);
                } else {
                    println!("missing");
                }
            }
            "#,
        );

        let Item::Function(func) = &program.items[0] else {
            panic!("expected function");
        };

        let Stmt::Expr(Expr::Match { arms, .. }) = &func.body.stmts[0] else {
            panic!("expected if-let to lower to match");
        };

        assert_eq!(arms.len(), 2);
        assert!(matches!(
            &arms[0].pattern,
            Pattern::Variant { variant, .. } if variant == "Some"
        ));
        assert!(matches!(&arms[1].pattern, Pattern::Wildcard(_)));
        assert!(!diagnostics
            .iter()
            .any(|diag| diag.contains("if-let condition simplified")));
    }

    #[test]
    fn lowers_while_let_to_loop_and_match_without_strict_diagnostic() {
        let (program, diagnostics) = transform_with_diagnostics(
            r#"
            fn demo(values: Option<i32>) {
                while let Some(found) = values {
                    println!("{}", found);
                    break;
                }
            }
            "#,
        );

        let Item::Function(func) = &program.items[0] else {
            panic!("expected function");
        };

        let Stmt::Expr(Expr::Block(block, _)) = &func.body.stmts[0] else {
            panic!("expected while-let to lower to loop block");
        };
        let Stmt::Loop { body, .. } = &block.stmts[0] else {
            panic!("expected loop");
        };
        let Stmt::Expr(Expr::Match { arms, .. }) = &body.stmts[0] else {
            panic!("expected loop body to contain match");
        };

        assert_eq!(arms.len(), 2);
        assert!(matches!(&arms[1].body, Expr::Break(None, _)));
        assert!(!diagnostics
            .iter()
            .any(|diag| diag.contains("if-let condition simplified")));
    }

    #[test]
    fn lowers_writeln_without_arguments_to_helper_macro() {
        let program = transform_source(
            r#"
            fn demo(output: String) {
                writeln!(output);
            }
            "#,
        );

        let Item::Function(func) = &program.items[0] else {
            panic!("expected function");
        };

        let Stmt::Expr(Expr::MacroCall { name, args, .. }) = &func.body.stmts[0] else {
            panic!("expected lowered helper macro");
        };

        assert_eq!(name, "__kain_writeln_fmt");
        assert_eq!(args.len(), 2);
        assert!(matches!(&args[1], Expr::String(text, _) if text.is_empty()));
    }

    #[test]
    fn skips_test_modules_and_cfg_test_items_by_default() {
        let program = transform_source(
            r#"
            #[cfg(test)]
            fn helper() {}

            mod tests {
                pub fn hidden() {}
            }

            fn visible() {}
            "#,
        );

        assert_eq!(program.items.len(), 1);
        assert!(matches!(&program.items[0], Item::Function(func) if func.name == "visible"));
    }

    #[test]
    fn lowers_compound_assign_to_assignment_expression() {
        let program = transform_source(
            r#"
            fn demo(mut count: i32, delta: i32) {
                count += delta;
            }
            "#,
        );

        let Item::Function(func) = &program.items[0] else {
            panic!("expected function");
        };

        let Stmt::Expr(Expr::Assign { target, value, .. }) = &func.body.stmts[0] else {
            panic!("expected lowered assignment");
        };

        assert!(matches!(target.as_ref(), Expr::Ident(name, _) if name == "count"));
        assert!(matches!(
            value.as_ref(),
            Expr::Binary { left, op: BinaryOp::Add, right, .. }
                if matches!(left.as_ref(), Expr::Ident(name, _) if name == "count")
                && matches!(right.as_ref(), Expr::Ident(name, _) if name == "delta")
        ));
    }

    #[test]
    fn preserves_use_alias_and_resolves_expression_path() {
        let program = transform_source(
            r#"
            use crate::diagnostics::SpanMapper as Mapper;

            fn demo() {
                let _x = Mapper::new();
            }
            "#,
        );

        assert!(matches!(
            &program.items[0],
            Item::Use(Use { alias: Some(alias), .. }) if alias == "Mapper"
        ));

        let Item::Function(func) = &program.items[1] else {
            panic!("expected function");
        };

        let Stmt::Let {
            value: Some(Expr::Call { callee, .. }),
            ..
        } = &func.body.stmts[0]
        else {
            panic!("expected call");
        };

        assert!(matches!(
            callee.as_ref(),
            Expr::Ident(path, _) if path == "crate::diagnostics::SpanMapper::new"
        ));
    }

    #[test]
    fn preserves_trait_definitions_as_items() {
        let program = transform_source(
            r#"
            pub trait Renderer {
                fn draw(&self, label: String) -> bool;
                fn reset(&mut self) {
                    println!("reset");
                }
            }
            "#,
        );

        let Item::Trait(value) = &program.items[0] else {
            panic!("expected trait item");
        };

        assert_eq!(value.name, "Renderer");
        assert_eq!(value.methods.len(), 2);
        assert_eq!(value.methods[0].name, "draw");
        assert!(matches!(
            value.methods[0].params.first(),
            Some(Param { name, .. }) if name == "_self"
        ));
        assert!(matches!(
            value.methods[0].return_type,
            Some(Type::Named { ref name, .. }) if name == "Bool"
        ));
        assert!(value.methods[0].default_impl.is_none());
        assert!(value.methods[1].default_impl.is_some());
    }

    #[test]
    fn records_dyn_trait_lowering_diagnostics() {
        let (program, diagnostics) = transform_with_diagnostics(
            r#"
            fn install(writer: Box<dyn std::fmt::Write>) {}
            "#,
        );

        let Item::Function(function) = &program.items[0] else {
            panic!("expected function");
        };

        assert!(matches!(
            function.params[0].ty,
            Type::Named { ref name, ref generics, .. }
                if name == "Box"
                && matches!(
                    generics.first(),
                    Some(Type::Impl { trait_name, .. }) if trait_name == "Write"
                )
        ));
        assert!(diagnostics
            .iter()
            .any(|diag| diag.contains("dyn trait type lowered to impl Write")));
        assert!(diagnostics
            .iter()
            .any(|diag| diag.contains("class:dyn_trait_lowering")));
    }

    #[test]
    fn records_external_mod_decl_class_marker() {
        let (_program, diagnostics) = transform_with_diagnostics(
            r#"
            mod diagnostics;
            "#,
        );

        assert!(diagnostics
            .iter()
            .any(|diag| diag.contains("class:external_mod_decl")));
    }

    #[test]
    fn records_trait_surface_lowering_class_marker() {
        let (_program, diagnostics) = transform_with_diagnostics(
            r#"
            trait ParserBridge {
                type Output;
            }
            "#,
        );

        assert!(diagnostics
            .iter()
            .any(|diag| diag.contains("class:trait_surface_lowering")));
    }

    #[test]
    fn lowers_async_blocks_without_strict_diagnostics() {
        let (_program, diagnostics) = transform_with_diagnostics(
            r#"
            fn demo() {
                let _value = async { 41 };
            }
            "#,
        );

        assert!(!diagnostics
            .iter()
            .any(|diag| diag.contains("class:unsupported_expr_lowering")));
    }

    #[test]
    fn lowers_async_move_blocks_without_strict_diagnostics() {
        let (_program, diagnostics) = transform_with_diagnostics(
            r#"
            fn demo() {
                let _value = async move { 41 };
            }
            "#,
        );

        assert!(!diagnostics
            .iter()
            .any(|diag| diag.contains("class:unsupported_expr_lowering")));
    }

    #[test]
    fn preserves_tuple_destructured_closure_bindings() {
        let program = transform_source(
            r#"
            fn demo() {
                let pair = |(left, right)| left + right;
            }
            "#,
        );

        let Item::Function(func) = &program.items[0] else {
            panic!("expected function");
        };

        let Stmt::Let {
            value: Some(Expr::Lambda { params, body, .. }),
            ..
        } = &func.body.stmts[0]
        else {
            panic!("expected lambda");
        };

        assert_eq!(params.len(), 1);
        assert!(params[0].name.starts_with("__kain_closure_arg"));
        assert!(matches!(
            body.as_ref(),
            Expr::Binary { left, op: BinaryOp::Add, right, .. }
                if matches!(
                    left.as_ref(),
                    Expr::Field { field, .. } if field == "__kain_tuple_0"
                )
                && matches!(
                    right.as_ref(),
                    Expr::Field { field, .. } if field == "__kain_tuple_1"
                )
        ));
    }

    #[test]
    fn preserves_struct_destructured_closure_bindings() {
        let program = transform_source(
            r#"
            struct Pair { left: i32, right: i32 }

            fn demo() {
                let pair = |Pair { left, right }| left + right;
            }
            "#,
        );

        let Item::Function(func) = &program.items[1] else {
            panic!("expected function");
        };

        let Stmt::Let {
            value: Some(Expr::Lambda { params, body, .. }),
            ..
        } = &func.body.stmts[0]
        else {
            panic!("expected lambda");
        };

        assert_eq!(params.len(), 1);
        assert!(params[0].name.starts_with("__kain_closure_arg"));
        assert!(matches!(
            body.as_ref(),
            Expr::Binary { left, op: BinaryOp::Add, right, .. }
                if matches!(left.as_ref(), Expr::Field { field, .. } if field == "left")
                && matches!(right.as_ref(), Expr::Field { field, .. } if field == "right")
        ));
    }

    #[test]
    fn preserves_lifetime_generics_in_imported_types() {
        let program = transform_source(
            r#"
            pub struct SourceLocation<'a> {
                file: &'a str,
            }

            fn span_to_location<'a>(file: &'a str) -> SourceLocation<'a> {
                SourceLocation { file }
            }
            "#,
        );

        let Item::Struct(struct_def) = &program.items[0] else {
            panic!("expected struct");
        };
        assert!(matches!(
            struct_def.generics.as_slice(),
            [Generic { name, .. }] if name == "a"
        ));

        let Item::Function(function) = &program.items[1] else {
            panic!("expected function");
        };
        assert!(matches!(
            function.generics.as_slice(),
            [Generic { name, .. }] if name == "a"
        ));
        assert!(matches!(
            function.return_type.as_ref(),
            Some(Type::Named { name, generics, .. })
                if name == "SourceLocation"
                    && matches!(
                        generics.as_slice(),
                        [Type::Named { name, generics, .. }]
                            if name == "a" && generics.is_empty()
                    )
        ));
    }

    #[test]
    fn preserves_tuple_destructured_local_bindings() {
        let program = transform_source(
            r#"
            fn demo(pair: (i32, i32)) {
                let (left, right) = pair;
                let sum = left + right;
            }
            "#,
        );

        let Item::Function(func) = &program.items[0] else {
            panic!("expected function");
        };

        let Stmt::Let {
            pattern: Pattern::Tuple(patterns, _),
            value: Some(Expr::Ident(name, _)),
            ..
        } = &func.body.stmts[0]
        else {
            panic!("expected tuple destructuring let");
        };

        assert_eq!(name, "pair");
        assert!(matches!(
            patterns.as_slice(),
            [
                Pattern::Binding { name: left, .. },
                Pattern::Binding { name: right, .. }
            ] if left == "left" && right == "right"
        ));
    }

    #[test]
    fn lowers_unit_tuple_literal_to_empty_block_expr() {
        let program = transform_source(
            r#"
            fn demo() {
                let value = ();
            }
            "#,
        );

        let Item::Function(func) = &program.items[0] else {
            panic!("expected function");
        };

        let Stmt::Let {
            value: Some(Expr::Block(block, _)),
            ..
        } = &func.body.stmts[0]
        else {
            panic!("expected unit tuple to lower to empty block");
        };

        assert!(block.stmts.is_empty());
    }

    #[test]
    fn preserves_restricted_visibility_markers() {
        let program = transform_source(
            r#"
            pub(crate) fn crate_visible() {}
            pub(super) fn super_visible() {}
            "#,
        );

        assert!(matches!(
            &program.items[0],
            Item::Function(Function {
                visibility: Visibility::Crate,
                ..
            })
        ));
        assert!(matches!(
            &program.items[1],
            Item::Function(Function {
                visibility: Visibility::Super,
                ..
            })
        ));
    }

    #[test]
    fn records_unsupported_literal_lowering_class_marker() {
        let (_program, diagnostics) = transform_with_diagnostics(
            r#"
            fn demo() {
                let _value = c"hello";
            }
            "#,
        );

        assert!(diagnostics
            .iter()
            .any(|diag| diag.contains("class:unsupported_literal_lowering")));
    }

    #[test]
    fn records_unsupported_pattern_lowering_class_marker() {
        let (_program, diagnostics) = transform_with_diagnostics(
            r#"
            macro_rules! pat {
                () => { _ };
            }

            fn demo() {
                let value = 1;
                match value {
                    pat!() => {}
                }
            }
            "#,
        );

        assert!(diagnostics
            .iter()
            .any(|diag| diag.contains("class:unsupported_pattern_lowering")));
    }
}
