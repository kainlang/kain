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
//! - Trait definitions → emitted as a comment stub (KAIN uses structural typing)
//! - `macro_rules!` / proc-macro invocations → comment stub
//! - where clauses → dropped (KAIN has simpler generics for now)
//! - `use` declarations → skipped (KAIN resolves symbols structurally)
//! - `extern crate` → skipped
//! - Associated types in traits → skipped for first slice

use syn::{self, spanned::Spanned as _};
use kain_core::ast::*;
use kain_core::effects::Effect;
use kain_core::span::Span;
use crate::common::identifier_registry::{IdentifierDomain, StableIdentifierRenamer};
use crate::{ImportError, Result};
use std::collections::{HashMap, HashSet};
use super::types::RustTypeMapper;

// ── Transformer state ─────────────────────────────────────────────────────────

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

    /// Synthetic temp counter.
    temp_counter: usize,

    /// Accumulated warnings / unsupported construct notes.
    pub diagnostics: Vec<String>,
}

impl RustTransformer {
    pub fn new() -> Self {
        Self {
            type_mapper:       RustTypeMapper::new(),
            identifier_renamer: StableIdentifierRenamer::default(),
            generics_in_scope:  Vec::new(),
            current_function:   None,
            local_types:        vec![HashMap::new()],
            temp_counter:       0,
            diagnostics:        Vec::new(),
        }
    }

    // ── Identifier helpers ────────────────────────────────────────────────

    fn rename_value(&mut self, raw: &str) -> String {
        // `self` is a KAIN keyword clash — map to `_self`
        if raw == "self" { return "_self".to_string(); }
        self.identifier_renamer.resolve(IdentifierDomain::Value, raw)
    }

    fn rename_type(&mut self, raw: &str) -> String {
        self.identifier_renamer.resolve(IdentifierDomain::Type, raw)
    }

    fn rename_field(&mut self, raw: &str) -> String {
        self.identifier_renamer.resolve(IdentifierDomain::Field, raw)
    }

    fn rename_variant(&mut self, raw: &str) -> String {
        self.identifier_renamer.resolve(IdentifierDomain::Variant, raw)
    }

    fn fresh_temp(&mut self, prefix: &str) -> String {
        let n = self.temp_counter;
        self.temp_counter += 1;
        format!("__{}__{}", prefix, n)
    }

    fn note(&mut self, msg: impl Into<String>) {
        self.diagnostics.push(msg.into());
    }

    // ── Scope helpers ─────────────────────────────────────────────────────

    fn push_scope(&mut self) { self.local_types.push(HashMap::new()); }
    fn pop_scope(&mut self)  { if self.local_types.len() > 1 { self.local_types.pop(); } }
    fn define(&mut self, name: &str, ty: Type) {
        if let Some(scope) = self.local_types.last_mut() {
            scope.insert(name.to_string(), ty);
        }
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
        match item {
            syn::Item::Fn(f)          => Ok(self.transform_fn(f)?.into_iter().map(Item::Function).collect()),
            syn::Item::Struct(s)      => Ok(vec![Item::Struct(self.transform_struct(s)?)]),
            syn::Item::Enum(e)        => Ok(vec![Item::Enum(self.transform_enum(e)?)]),
            syn::Item::Impl(i)        => Ok(vec![Item::Impl(self.transform_impl(i)?)]),
            syn::Item::Const(c)       => Ok(vec![Item::Const(self.transform_const(c)?)]),
            syn::Item::Static(s)      => Ok(vec![Item::Const(self.transform_static(s)?)]),
            syn::Item::Type(t)        => Ok(vec![Item::TypeAlias(self.transform_type_alias(t)?)]),
            syn::Item::Mod(m)         => self.transform_mod(m),
            // Traits → stub comment, not yet modeled in KAIN item set
            syn::Item::Trait(t) => {
                self.note(format!("trait {} skipped (KAIN uses structural typing)", t.ident));
                Ok(vec![])
            }
            syn::Item::TraitAlias(t) => {
                self.note(format!("trait alias {} skipped", t.ident));
                Ok(vec![])
            }
            // Macro rules / foreign items → skip
            syn::Item::Macro(_)
            | syn::Item::ExternCrate(_)
            | syn::Item::ForeignMod(_) => Ok(vec![]),
            // Use declarations → skip (KAIN resolves structurally)
            syn::Item::Use(_) => Ok(vec![]),
            _ => {
                self.note("unknown item kind skipped".to_string());
                Ok(vec![])
            }
        }
    }

    // ── mod blocks ───────────────────────────────────────────────────────

    fn transform_mod(&mut self, m: &syn::ItemMod) -> Result<Vec<Item>> {
        match &m.content {
            Some((_, items)) => {
                let mut kain_items = Vec::new();
                for item in items {
                    kain_items.extend(self.transform_item(item)?);
                }
                let mod_name = self.rename_value(&m.ident.to_string());
                Ok(vec![Item::Module(Module {
                    name: mod_name,
                    items: kain_items,
                    visibility: visibility(&m.vis),
                    span: S,
                })])
            }
            None => {
                // `mod foo;` with external file — just note it, CLI handles multi-file
                self.note(format!("mod {}; (external file — import separately)", m.ident));
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
        self.generics_in_scope = generics.clone();

        // Params
        let params = self.transform_sig_inputs(&f.sig.inputs)?;

        // Return type
        let return_type = match &f.sig.output {
            syn::ReturnType::Default       => None,
            syn::ReturnType::Type(_, ty)   => Some(self.type_mapper.map_type(ty)),
        };

        // Effects
        let mut effects: Vec<Effect> = Vec::new();
        if f.sig.unsafety.is_some() { effects.push(Effect::Unsafe); }
        if f.sig.asyncness.is_some() { effects.push(Effect::Async); }

        // Body
        self.push_scope();
        for p in &params { self.define(&p.name, p.ty.clone()); }
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
            attributes: Vec::new(),
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
                            mutable:  r.mutability.is_some(),
                            inner:    Box::new(Type::Named { name: "Self".to_string(), generics: vec![], span: S }),
                            lifetime: None,
                            span:     S,
                        }
                    } else {
                        Type::Named { name: "Self".to_string(), generics: vec![], span: S }
                    };
                    params.push(Param {
                        name:    "_self".to_string(),
                        ty,
                        mutable:  r.mutability.is_some(),
                        default:  None,
                        span:     S,
                    });
                }
                syn::FnArg::Typed(pt) => {
                    let name = self.pattern_to_name(&pt.pat);
                    let ty   = self.type_mapper.map_type(&pt.ty);
                    params.push(Param { name, ty, mutable: false, default: None, span: S });
                }
            }
        }
        Ok(params)
    }

    // ─────────────────────────────────────────────────────────────────────
    // ── Structs ───────────────────────────────────────────────────────────
    // ─────────────────────────────────────────────────────────────────────

    fn transform_struct(&mut self, s: &syn::ItemStruct) -> Result<Struct> {
        let name     = self.rename_type(&s.ident.to_string());
        let generics = self.type_mapper.map_generic_params(&s.generics.params);

        let fields = match &s.fields {
            syn::Fields::Named(named) => named.named.iter().map(|f| {
                let field_name = self.rename_field(&f.ident.as_ref().map(|i| i.to_string()).unwrap_or_default());
                let ty = self.type_mapper.map_type(&f.ty);
                Field {
                    name:       field_name,
                    ty,
                    attributes: Vec::new(),
                    visibility: visibility(&f.vis),
                    default:    None,
                    weak:       false,
                    span:       S,
                }
            }).collect(),
            syn::Fields::Unnamed(unnamed) => unnamed.unnamed.iter().enumerate().map(|(i, f)| {
                let ty = self.type_mapper.map_type(&f.ty);
                Field {
                    name:       format!("field_{}", i),
                    ty,
                    attributes: Vec::new(),
                    visibility: visibility(&f.vis),
                    default:    None,
                    weak:       false,
                    span:       S,
                }
            }).collect(),
            syn::Fields::Unit => vec![],
        };

        Ok(Struct {
            name,
            generics,
            fields,
            methods:    Vec::new(),
            attributes: Vec::new(),
            visibility: visibility(&s.vis),
            span:       S,
        })
    }

    // ─────────────────────────────────────────────────────────────────────
    // ── Enums ─────────────────────────────────────────────────────────────
    // ─────────────────────────────────────────────────────────────────────

    fn transform_enum(&mut self, e: &syn::ItemEnum) -> Result<Enum> {
        let name     = self.rename_type(&e.ident.to_string());
        let generics = self.type_mapper.map_generic_params(&e.generics.params);

        let variants = e.variants.iter().map(|v| {
            let variant_name = self.rename_variant(&v.ident.to_string());
            let fields = match &v.fields {
                syn::Fields::Unit          => VariantFields::Unit,
                syn::Fields::Unnamed(un)   => VariantFields::Tuple(
                    un.unnamed.iter().map(|f| self.type_mapper.map_type(&f.ty)).collect()
                ),
                syn::Fields::Named(named)  => VariantFields::Struct(
                    named.named.iter().map(|f| {
                        let field_name = f.ident.as_ref().map(|i| self.rename_field(&i.to_string())).unwrap_or_default();
                        let ty = self.type_mapper.map_type(&f.ty);
                        Field {
                            name: field_name, ty,
                            attributes: Vec::new(),
                            visibility: Visibility::Public,
                            default: None, weak: false, span: S,
                        }
                    }).collect()
                ),
            };
            Variant { name: variant_name, fields, span: S }
        }).collect();

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
        let target_type = self.type_mapper.map_type(&i.self_ty);
        let generics    = self.type_mapper.map_generic_params(&i.generics.params);

        // `impl Trait for Type` → note the trait, still emit the methods
        let trait_name = i.trait_.as_ref().map(|(_, path, _)| {
            path.segments.last().map(|s| s.ident.to_string()).unwrap_or_default()
        });
        if let Some(ref t) = trait_name {
            self.note(format!("impl {} (trait impl — methods emitted inline)", t));
        }

        let mut methods = Vec::new();
        for item in &i.items {
            if let syn::ImplItem::Fn(method) = item {
                let name = self.rename_value(&method.sig.ident.to_string());
                self.current_function = Some(name.clone());

                let method_generics = self.type_mapper.map_generic_params(&method.sig.generics.params);
                self.generics_in_scope = method_generics.clone();

                let params = self.transform_sig_inputs(&method.sig.inputs)?;
                let return_type = match &method.sig.output {
                    syn::ReturnType::Default     => None,
                    syn::ReturnType::Type(_, ty) => Some(self.type_mapper.map_type(ty)),
                };
                let mut effects = Vec::new();
                if method.sig.unsafety.is_some() { effects.push(Effect::Unsafe); }
                if method.sig.asyncness.is_some() { effects.push(Effect::Async); }

                self.push_scope();
                for p in &params { self.define(&p.name, p.ty.clone()); }
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
                    attributes: Vec::new(),
                    visibility: visibility(&method.vis),
                    span: S,
                });
            }
        }

        Ok(Impl {
            generics,
            target_type,
            trait_name,
            methods,
            span: S,
        })
    }

    // ─────────────────────────────────────────────────────────────────────
    // ── Const / Static ────────────────────────────────────────────────────
    // ─────────────────────────────────────────────────────────────────────

    fn transform_const(&mut self, c: &syn::ItemConst) -> Result<Const> {
        let name  = self.rename_value(&c.ident.to_string());
        let ty    = self.type_mapper.map_type(&c.ty);
        let value = self.transform_expr(&c.expr)?;
        Ok(Const { name, ty, value, visibility: visibility(&c.vis), span: S })
    }

    fn transform_static(&mut self, s: &syn::ItemStatic) -> Result<Const> {
        let name  = self.rename_value(&s.ident.to_string());
        let ty    = self.type_mapper.map_type(&s.ty);
        let value = self.transform_expr(&s.expr)?;
        Ok(Const { name, ty, value, visibility: visibility(&s.vis), span: S })
    }

    // ── Type alias ─────────────────────────────────────────────────────

    fn transform_type_alias(&mut self, t: &syn::ItemType) -> Result<TypeAlias> {
        let name     = self.rename_type(&t.ident.to_string());
        let generics = self.type_mapper.map_generic_params(&t.generics.params);
        let target   = self.type_mapper.map_type(&t.ty);
        Ok(TypeAlias { name, generics, target, visibility: visibility(&t.vis), span: S })
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
                let (name, mutable) = self.local_pat_name(&local.pat);
                let ty = local.pat.as_ref().into_iter()
                    .find_map(|_| None::<Type>); // type from annotation if available
                let (ty_ann, value) = if let Some(init) = &local.init {
                    let ty_from_pat = self.type_from_local_pat(&local.pat);
                    let val = self.transform_expr(&init.expr)?;
                    (ty_from_pat, Some(val))
                } else {
                    (None, None)
                };
                let ty_ann = ty.or(ty_ann);
                if let Some(ty_ann) = &ty_ann {
                    self.define(&name, ty_ann.clone());
                }
                Ok(vec![Stmt::Let {
                    pattern: Pattern::Binding { name, mutable, span: S },
                    ty:      ty_ann,
                    value,
                    span:    S,
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
                Ok(items.into_iter().map(Stmt::Item).collect())
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
                let name = path_to_ident(&p.path);
                Ok(Expr::Ident(self.rename_value(&name), S))
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
                    syn::UnOp::Neg(_)   => UnaryOp::Neg,
                    syn::UnOp::Not(_)   => UnaryOp::Not,
                    syn::UnOp::Deref(_) => UnaryOp::Deref,
                    _                   => UnaryOp::Not,
                };
                Ok(Expr::Unary { op, operand: Box::new(operand), span: S })
            }

            // ── Binary ────────────────────────────────────────────────────
            syn::Expr::Binary(b) => {
                let left  = self.transform_expr(&b.left)?;
                let right = self.transform_expr(&b.right)?;
                let op    = binop(&b.op);
                Ok(Expr::Binary { left: Box::new(left), op, right: Box::new(right), span: S })
            }

            // ── Assignment ────────────────────────────────────────────────
            syn::Expr::Assign(a) => {
                let target = self.transform_expr(&a.left)?;
                let value  = self.transform_expr(&a.right)?;
                Ok(Expr::Assign { target: Box::new(target), value: Box::new(value), span: S })
            }

            // Compound assignment: x += y → x = x + y
            syn::Expr::AssignOp(a) => {
                let target = self.transform_expr(&a.left)?;
                let rhs    = self.transform_expr(&a.right)?;
                let op     = compound_binop(&a.op);
                let binary = Expr::Binary {
                    left:  Box::new(target.clone()),
                    op,
                    right: Box::new(rhs),
                    span:  S,
                };
                Ok(Expr::Assign { target: Box::new(target), value: Box::new(binary), span: S })
            }

            // ── Field access ──────────────────────────────────────────────
            syn::Expr::Field(f) => {
                let object = self.transform_expr(&f.base)?;
                let field  = match &f.member {
                    syn::Member::Named(ident)  => self.rename_field(&ident.to_string()),
                    syn::Member::Unnamed(idx)  => format!("field_{}", idx.index),
                };
                Ok(Expr::Field { object: Box::new(object), field, span: S })
            }

            // ── Index ─────────────────────────────────────────────────────
            syn::Expr::Index(i) => {
                let object = self.transform_expr(&i.expr)?;
                let index  = self.transform_expr(&i.index)?;
                Ok(Expr::Index { object: Box::new(object), index: Box::new(index), span: S })
            }

            // ── Function call ─────────────────────────────────────────────
            syn::Expr::Call(c) => {
                let callee = self.transform_expr(&c.func)?;
                let args   = self.transform_call_args(&c.args)?;
                Ok(Expr::Call { callee: Box::new(callee), args, span: S })
            }

            // ── Method call ───────────────────────────────────────────────
            syn::Expr::MethodCall(m) => {
                let receiver = self.transform_expr(&m.receiver)?;
                let method   = self.rename_value(&m.method.to_string());
                let args     = self.transform_call_args(&m.args)?;
                Ok(Expr::MethodCall { receiver: Box::new(receiver), method, args, span: S })
            }

            // ── Struct construction ───────────────────────────────────────
            syn::Expr::Struct(s) => {
                let name   = path_to_ident(&s.path);
                let fields = s.fields.iter().map(|fv| {
                    let field_name = self.rename_field(&fv.member.to_string());
                    let val        = self.transform_expr(&fv.expr)?;
                    Ok((field_name, val))
                }).collect::<Result<Vec<_>>>()?;
                Ok(Expr::Struct { name, fields, span: S })
            }

            // ── Array ─────────────────────────────────────────────────────
            syn::Expr::Array(a) => {
                let items = a.elems.iter()
                    .map(|e| self.transform_expr(e))
                    .collect::<Result<Vec<_>>>()?;
                Ok(Expr::Array(items, S))
            }

            syn::Expr::Repeat(r) => {
                // [expr; N] → fill array
                let elem = self.transform_expr(&r.expr)?;
                Ok(Expr::Array(vec![elem], S)) // simplified — just emit one element
            }

            // ── Tuple ─────────────────────────────────────────────────────
            syn::Expr::Tuple(t) => {
                if t.elems.is_empty() {
                    return Ok(Expr::None(S)); // () → None (unit)
                }
                let items = t.elems.iter()
                    .map(|e| self.transform_expr(e))
                    .collect::<Result<Vec<_>>>()?;
                Ok(Expr::Tuple(items, S))
            }

            // ── Closure / lambda ──────────────────────────────────────────
            syn::Expr::Closure(cl) => {
                let params = cl.inputs.iter().map(|p| {
                    let name = self.pattern_to_name(p);
                    Param { name, ty: Type::Infer(S), mutable: false, default: None, span: S }
                }).collect();
                let return_type = match &cl.output {
                    syn::ReturnType::Default     => None,
                    syn::ReturnType::Type(_, ty) => Some(self.type_mapper.map_type(ty)),
                };
                let body = self.transform_expr(&cl.body)?;
                Ok(Expr::Lambda { params, return_type, body: Box::new(body), span: S })
            }

            // ── If / if let ───────────────────────────────────────────────
            syn::Expr::If(i) => {
                let condition   = self.transform_if_condition(&i.cond)?;
                let then_branch = self.transform_block(&i.then_branch)?;
                let else_branch = if let Some((_, else_expr)) = &i.else_branch {
                    Some(Box::new(self.transform_else(else_expr)?))
                } else {
                    None
                };
                Ok(Expr::If { condition: Box::new(condition), then_branch, else_branch, span: S })
            }

            // ── Match ─────────────────────────────────────────────────────
            syn::Expr::Match(m) => {
                let scrutinee = self.transform_expr(&m.expr)?;
                let arms = m.arms.iter()
                    .map(|arm| self.transform_arm(arm))
                    .collect::<Result<Vec<_>>>()?;
                Ok(Expr::Match { scrutinee: Box::new(scrutinee), arms, span: S })
            }

            // ── Loop / while / for ────────────────────────────────────────
            syn::Expr::Loop(l) => {
                let body = self.transform_block(&l.body)?;
                Ok(Expr::Block(Block {
                    stmts: vec![Stmt::Loop { body, span: S }],
                    span:  S,
                }, S))
            }

            syn::Expr::While(w) => {
                let condition = self.transform_if_condition(&w.cond)?;
                let body      = self.transform_block(&w.body)?;
                Ok(Expr::Block(Block {
                    stmts: vec![Stmt::While { condition, body, span: S }],
                    span:  S,
                }, S))
            }

            syn::Expr::ForLoop(f) => {
                let binding = self.transform_pattern(&f.pat);
                let iter    = self.transform_expr(&f.expr)?;
                let body    = self.transform_block(&f.body)?;
                Ok(Expr::Block(Block {
                    stmts: vec![Stmt::For { binding, iter, body, span: S }],
                    span:  S,
                }, S))
            }

            // ── Return / break / continue ─────────────────────────────────
            syn::Expr::Return(r) => {
                let val = r.expr.as_ref().map(|e| self.transform_expr(e)).transpose()?;
                Ok(Expr::Return(val.map(Box::new), S))
            }

            syn::Expr::Break(b) => {
                let val = b.expr.as_ref().map(|e| self.transform_expr(e)).transpose()?;
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

            // ── Cast: `expr as T` ─────────────────────────────────────────
            syn::Expr::Cast(c) => {
                let value  = self.transform_expr(&c.expr)?;
                let target = self.type_mapper.map_type(&c.ty);
                Ok(Expr::Cast { value: Box::new(value), target, span: S })
            }

            // ── Reference: `&expr` / `&mut expr` ─────────────────────────
            syn::Expr::Reference(r) => {
                let value = self.transform_expr(&r.expr)?;
                Ok(Expr::Ref { mutable: r.mutability.is_some(), value: Box::new(value), span: S })
            }

            // ── Dereference: `*expr` ──────────────────────────────────────
            syn::Expr::Unary(u) if matches!(u.op, syn::UnOp::Deref(_)) => {
                let inner = self.transform_expr(&u.expr)?;
                Ok(Expr::Deref(Box::new(inner), S))
            }

            // ── Paren ─────────────────────────────────────────────────────
            syn::Expr::Paren(p) => {
                let inner = self.transform_expr(&p.expr)?;
                Ok(Expr::Paren(Box::new(inner), S))
            }

            // ── Range ─────────────────────────────────────────────────────
            syn::Expr::Range(r) => {
                let start     = r.start.as_ref().map(|e| self.transform_expr(e)).transpose()?;
                let end       = r.end.as_ref().map(|e| self.transform_expr(e)).transpose()?;
                let inclusive = matches!(r.limits, syn::RangeLimits::Closed(_));
                Ok(Expr::Range {
                    start:     start.map(Box::new),
                    end:       end.map(Box::new),
                    inclusive,
                    span:      S,
                })
            }

            // ── Macro calls (println!, vec!, format!, etc.) ───────────────
            syn::Expr::Macro(m) => {
                let macro_name = path_to_ident(&m.mac.path);
                // Attempt to parse args as comma-separated expressions
                let args = self.parse_macro_args(&m.mac.tokens);
                Ok(Expr::MacroCall { name: macro_name, args, span: S })
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
            _ => {
                self.note("unsupported expression kind".to_string());
                Ok(Expr::None(S))
            }
        }
    }

    // ── Literals ─────────────────────────────────────────────────────────

    fn transform_lit(&mut self, lit: &syn::ExprLit) -> Result<Expr> {
        match &lit.lit {
            syn::Lit::Int(i)    => {
                let val = i.base10_parse::<i64>().unwrap_or(0);
                Ok(Expr::Int(val, S))
            }
            syn::Lit::Float(f)  => {
                let val = f.base10_parse::<f64>().unwrap_or(0.0);
                Ok(Expr::Float(val, S))
            }
            syn::Lit::Bool(b)   => Ok(Expr::Bool(b.value, S)),
            syn::Lit::Str(s)    => Ok(Expr::String(s.value(), S)),
            syn::Lit::Char(c)   => Ok(Expr::String(c.value().to_string(), S)),
            syn::Lit::Byte(b)   => Ok(Expr::Int(b.value() as i64, S)),
            syn::Lit::ByteStr(bs) => {
                // Byte strings → array of ints
                let items = bs.value().iter().map(|&b| Expr::Int(b as i64, S)).collect();
                Ok(Expr::Array(items, S))
            }
            _ => Ok(Expr::None(S)),
        }
    }

    // ── If conditions (handles `if let`) ─────────────────────────────────

    fn transform_if_condition(&mut self, cond: &syn::Expr) -> Result<Expr> {
        // `if let Pattern(x) = expr` → emitted as a simple truthy expression for now
        // Full `if let` desugaring is a future improvement
        match cond {
            syn::Expr::Let(let_expr) => {
                // `if let Some(x) = y` → just transform the scrutinee as the condition
                // (loses the binding — future improvement: desugar into match)
                self.note("if-let condition simplified (binding erased)".to_string());
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
            syn::Expr::If(i) => {
                let condition   = self.transform_if_condition(&i.cond)?;
                let then_branch = self.transform_block(&i.then_branch)?;
                let next = if let Some((_, next_else)) = &i.else_branch {
                    Some(Box::new(self.transform_else(next_else)?))
                } else {
                    None
                };
                Ok(ElseBranch::ElseIf(Box::new(condition), then_branch, next))
            }
            other => {
                let expr  = self.transform_expr(other)?;
                let block = Block { stmts: vec![Stmt::Expr(expr)], span: S };
                Ok(ElseBranch::Else(block))
            }
        }
    }

    // ── Match arms ───────────────────────────────────────────────────────

    fn transform_arm(&mut self, arm: &syn::Arm) -> Result<MatchArm> {
        let pattern = self.transform_pattern(&arm.pat);
        // Guard: `if guard_expr` → simplified (emitted as pattern condition)
        let body = self.transform_expr(&arm.body)?;
        Ok(MatchArm { pattern, body, span: S })
    }

    // ── Patterns ─────────────────────────────────────────────────────────

    fn transform_pattern(&mut self, pat: &syn::Pat) -> Pattern {
        match pat {
            syn::Pat::Ident(pi) => {
                let name    = self.rename_value(&pi.ident.to_string());
                let mutable = pi.mutability.is_some();
                Pattern::Binding { name, mutable, span: S }
            }
            syn::Pat::Wild(_) => Pattern::Wildcard(S),
            syn::Pat::Lit(pl) => {
                if let Ok(expr) = self.transform_lit(&syn::ExprLit { attrs: vec![], lit: pl.lit.clone() }) {
                    Pattern::Literal(expr, S)
                } else {
                    Pattern::Wildcard(S)
                }
            }
            syn::Pat::Tuple(pt) => {
                let pats = pt.elems.iter().map(|p| self.transform_pattern(p)).collect();
                Pattern::Tuple(pats, S)
            }
            syn::Pat::TupleStruct(pts) => {
                let name  = path_to_ident(&pts.path);
                let inner = pts.elems.iter().map(|p| self.transform_pattern(p)).collect();
                Pattern::Constructor { name, fields: VariantPatternFields::Tuple(inner), span: S }
            }
            syn::Pat::Struct(ps) => {
                let name   = path_to_ident(&ps.path);
                let fields = ps.fields.iter().map(|fv| {
                    let field = fv.member.to_string();
                    let pat   = self.transform_pattern(&fv.pat);
                    (field, pat)
                }).collect();
                Pattern::Constructor { name, fields: VariantPatternFields::Struct(fields), span: S }
            }
            syn::Pat::Path(pp) => {
                let name = path_to_ident(&pp.path);
                Pattern::Constructor { name, fields: VariantPatternFields::Unit, span: S }
            }
            syn::Pat::Range(pr) => {
                let start = pr.start.as_ref().and_then(|e| {
                    if let syn::Expr::Lit(l) = e.as_ref() {
                        if let syn::Lit::Int(i) = &l.lit {
                            return i.base10_parse::<i64>().ok();
                        }
                    }
                    None
                });
                let end = pr.end.as_ref().and_then(|e| {
                    if let syn::Expr::Lit(l) = e.as_ref() {
                        if let syn::Lit::Int(i) = &l.lit {
                            return i.base10_parse::<i64>().ok();
                        }
                    }
                    None
                });
                if let (Some(s), Some(e)) = (start, end) {
                    Pattern::Range { start: s, end: e, inclusive: matches!(pr.limits, syn::RangeLimits::Closed(_)), span: S }
                } else {
                    Pattern::Wildcard(S)
                }
            }
            syn::Pat::Or(po) => {
                let cases = po.cases.iter().map(|p| self.transform_pattern(p)).collect();
                Pattern::Or(cases, S)
            }
            syn::Pat::Reference(pr) => {
                // `&x` / `&mut x` pattern — unwrap to inner
                self.transform_pattern(&pr.pat)
            }
            syn::Pat::Slice(ps) => {
                let pats = ps.elems.iter().map(|p| self.transform_pattern(p)).collect();
                Pattern::Array(pats, S)
            }
            _ => Pattern::Wildcard(S),
        }
    }

    fn pattern_to_name(&self, pat: &syn::Pat) -> String {
        match pat {
            syn::Pat::Ident(pi) => {
                let raw = pi.ident.to_string();
                if raw == "self" { "_self".to_string() } else { raw }
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
                let name = if raw == "self" { "_self".to_string() } else { raw };
                (name, pi.mutability.is_some())
            }
            syn::Pat::Wild(_) => ("_".to_string(), false),
            syn::Pat::Type(pt) => self.local_pat_name(&pt.pat),
            _ => ("_".to_string(), false),
        }
    }

    fn type_from_local_pat(&self, pat: &syn::Pat) -> Option<Type> {
        if let syn::Pat::Type(pt) = pat {
            Some(self.type_mapper.map_type(&pt.ty))
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
                Ok(CallArg { name: None, value, span: S })
            })
            .collect()
    }

    // ── Macro arg parsing ─────────────────────────────────────────────────

    fn parse_macro_args(&mut self, tokens: &proc_macro2::TokenStream) -> Vec<Expr> {
        // Best-effort: try parse as comma-separated syn expressions
        struct CommaSep(syn::punctuated::Punctuated<syn::Expr, syn::token::Comma>);
        impl syn::parse::Parse for CommaSep {
            fn parse(input: syn::parse::ParseStream) -> syn::Result<Self> {
                Ok(CommaSep(syn::punctuated::Punctuated::parse_terminated(input)?))
            }
        }
        if let Ok(CommaSep(exprs)) = syn::parse2::<CommaSep>(tokens.clone()) {
            exprs.iter()
                .filter_map(|e| self.transform_expr(e).ok())
                .collect()
        } else {
            // Raw string fallback for format strings etc.
            vec![]
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ── Free helpers ──────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────

const S: Span = Span::ZERO;

fn visibility(vis: &syn::Visibility) -> Visibility {
    match vis {
        syn::Visibility::Public(_) => Visibility::Public,
        _                         => Visibility::Private,
    }
}

fn path_to_ident(path: &syn::Path) -> String {
    path.segments
        .iter()
        .map(|s| s.ident.to_string())
        .collect::<Vec<_>>()
        .join("::")
}

fn binop(op: &syn::BinOp) -> BinaryOp {
    match op {
        syn::BinOp::Add(_)  => BinaryOp::Add,
        syn::BinOp::Sub(_)  => BinaryOp::Sub,
        syn::BinOp::Mul(_)  => BinaryOp::Mul,
        syn::BinOp::Div(_)  => BinaryOp::Div,
        syn::BinOp::Rem(_)  => BinaryOp::Rem,
        syn::BinOp::And(_)  => BinaryOp::And,
        syn::BinOp::Or(_)   => BinaryOp::Or,
        syn::BinOp::BitAnd(_) => BinaryOp::BitAnd,
        syn::BinOp::BitOr(_)  => BinaryOp::BitOr,
        syn::BinOp::BitXor(_) => BinaryOp::BitXor,
        syn::BinOp::Shl(_) => BinaryOp::Shl,
        syn::BinOp::Shr(_) => BinaryOp::Shr,
        syn::BinOp::Eq(_)  => BinaryOp::Eq,
        syn::BinOp::Ne(_)  => BinaryOp::Ne,
        syn::BinOp::Lt(_)  => BinaryOp::Lt,
        syn::BinOp::Le(_)  => BinaryOp::Le,
        syn::BinOp::Gt(_)  => BinaryOp::Gt,
        syn::BinOp::Ge(_)  => BinaryOp::Ge,
        _ => BinaryOp::Add, // fallback
    }
}

fn compound_binop(op: &syn::BinOp) -> BinaryOp {
    // compound ops map to the base operator (x += y → x = x + y)
    match op {
        syn::BinOp::AddAssign(_) => BinaryOp::Add,
        syn::BinOp::SubAssign(_) => BinaryOp::Sub,
        syn::BinOp::MulAssign(_) => BinaryOp::Mul,
        syn::BinOp::DivAssign(_) => BinaryOp::Div,
        syn::BinOp::RemAssign(_) => BinaryOp::Rem,
        syn::BinOp::BitAndAssign(_) => BinaryOp::BitAnd,
        syn::BinOp::BitOrAssign(_)  => BinaryOp::BitOr,
        syn::BinOp::BitXorAssign(_) => BinaryOp::BitXor,
        syn::BinOp::ShlAssign(_) => BinaryOp::Shl,
        syn::BinOp::ShrAssign(_) => BinaryOp::Shr,
        _ => BinaryOp::Add,
    }
}
