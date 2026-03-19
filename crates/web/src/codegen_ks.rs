//! KainScript (.ks) Code Generation
//!
//! KainScript is the unified "best of both worlds" output format:
//!   - Pure JavaScript (ES2022+) that runs natively in Node.js, Deno, Bun, browsers
//!   - Full type information via JSDoc annotations understood by TypeScript LSP
//!   - No compilation step required — `node file.ks` just works
//!   - `// @ts-check` enables VS Code full type checking without tsc
//!   - `tsc --checkJs` validates it without producing output files
//!
//! Think of it as TypeScript that refused to put up a build wall between you and your code.

use kain_core::ast::{
    BinaryOp, Block, ElseBranch, EnumVariantFields, Expr, JSXAttrValue, JSXNode, Pattern, Stmt,
    Type, UnaryOp, VariantFields,
};
use kain_core::error::KainResult;
use kain_core::types::{
    ResolvedType, TypedComponent, TypedEnum, TypedFunction, TypedItem, TypedProgram, TypedStruct,
};
use std::collections::HashSet;

/// Generate KainScript source from a typed program.
pub fn generate(program: &TypedProgram) -> KainResult<String> {
    let mut gen = KsGen::new();
    Ok(gen.gen_program(program))
}

// ── Output buffer ─────────────────────────────────────────────────────────────

#[derive(Default)]
struct Lines(Vec<String>);

impl Lines {
    fn push(&mut self, s: impl Into<String>) {
        self.0.push(s.into());
    }
    fn build(self) -> String {
        self.0.join("\n")
    }
}

// ── Generator ─────────────────────────────────────────────────────────────────

struct KsGen {
    out: Lines,
    indent: usize,
    seen_types: HashSet<String>,
    needs_dom: bool,
}

impl KsGen {
    fn new() -> Self {
        Self {
            out: Lines::default(),
            indent: 0,
            seen_types: HashSet::new(),
            needs_dom: false,
        }
    }

    // ── Indented write helpers ─────────────────────────────────────────────

    fn line(&mut self, s: impl Into<String>) {
        let prefix = "  ".repeat(self.indent);
        self.out.push(format!("{}{}", prefix, s.into()));
    }

    fn blank(&mut self) {
        self.out.push(String::new());
    }

    fn indent(&mut self) {
        self.indent += 1;
    }
    fn dedent(&mut self) {
        if self.indent > 0 {
            self.indent -= 1;
        }
    }

    // ── Type → JSDoc string ────────────────────────────────────────────────

    fn type_jsdoc(&self, ty: &Type) -> String {
        match ty {
            Type::Named { name, generics, .. } => {
                let base = match name.as_str() {
                    "Int" | "i8" | "i16" | "i32" | "i64" | "i128" | "isize" | "u8" | "u16"
                    | "u32" | "u64" | "u128" | "usize" | "f32" | "f64" | "Float" => {
                        "number".to_string()
                    }
                    "Bool" => "boolean".to_string(),
                    "String" | "str" | "Char" => "string".to_string(),
                    "Unit" => "void".to_string(),
                    "Vec2" => "[number, number]".to_string(),
                    "Vec3" => "[number, number, number]".to_string(),
                    "Vec4" => "[number, number, number, number]".to_string(),
                    other => other.to_string(),
                };
                if generics.is_empty() {
                    base
                } else {
                    let gs = generics
                        .iter()
                        .map(|g| self.type_jsdoc(g))
                        .collect::<Vec<_>>()
                        .join(", ");
                    // Map KAIN generics to JSDoc equivalents
                    match base.as_str() {
                        "Array" | "Vec" => format!("{}[]", self.type_jsdoc(&generics[0])),
                        "Map" | "HashMap" if generics.len() == 2 => format!(
                            "Map<{}, {}>",
                            self.type_jsdoc(&generics[0]),
                            self.type_jsdoc(&generics[1])
                        ),
                        "Set" | "HashSet" if !generics.is_empty() => {
                            format!("Set<{}>", self.type_jsdoc(&generics[0]))
                        }
                        _ => format!("{}<{}>", base, gs),
                    }
                }
            }
            Type::Array(inner, _, _) | Type::Slice(inner, _) => {
                format!("{}[]", self.type_jsdoc(inner))
            }
            Type::Tuple(types, _) => {
                let inner = types
                    .iter()
                    .map(|t| self.type_jsdoc(t))
                    .collect::<Vec<_>>()
                    .join(", ");
                format!("[{}]", inner)
            }
            Type::Ref { inner, .. } => self.type_jsdoc(inner),
            Type::Function {
                params,
                return_type,
                ..
            } => {
                let ps = params
                    .iter()
                    .enumerate()
                    .map(|(i, p)| format!("arg{}: {}", i, self.type_jsdoc(p)))
                    .collect::<Vec<_>>()
                    .join(", ");
                format!("function({}): {}", ps, self.type_jsdoc(return_type))
            }
            Type::Option(inner, _) => format!("{} | null | undefined", self.type_jsdoc(inner)),
            Type::Result(ok, err, _) => format!(
                "{{ ok: true, value: {} }} | {{ ok: false, error: {} }}",
                self.type_jsdoc(ok),
                self.type_jsdoc(err)
            ),
            Type::Never(_) => "never".to_string(),
            Type::Unit(_) => "void".to_string(),
            Type::Infer(_) | Type::Impl { .. } => "*".to_string(),
            Type::Ptr { inner, .. } => self.type_jsdoc(inner),
        }
    }

    fn resolved_jsdoc(&self, ty: &ResolvedType) -> String {
        match ty {
            ResolvedType::Unit => "void".to_string(),
            ResolvedType::Bool => "boolean".to_string(),
            ResolvedType::Int(_) | ResolvedType::Float(_) => "number".to_string(),
            ResolvedType::String | ResolvedType::Char => "string".to_string(),
            ResolvedType::Array(inner, _) | ResolvedType::Slice(inner) => {
                format!("{}[]", self.resolved_jsdoc(inner))
            }
            ResolvedType::Tuple(inner) => {
                let s = inner
                    .iter()
                    .map(|t| self.resolved_jsdoc(t))
                    .collect::<Vec<_>>()
                    .join(", ");
                format!("[{}]", s)
            }
            ResolvedType::Option(inner) => {
                format!("{} | null | undefined", self.resolved_jsdoc(inner))
            }
            ResolvedType::Result(ok, err) => format!(
                "{{ ok: true, value: {} }} | {{ ok: false, error: {} }}",
                self.resolved_jsdoc(ok),
                self.resolved_jsdoc(err)
            ),
            ResolvedType::Ref { inner, .. } => self.resolved_jsdoc(inner),
            ResolvedType::Struct(name, _)
            | ResolvedType::Enum(name, _)
            | ResolvedType::Generic(name) => name.clone(),
            ResolvedType::Function { params, ret, .. } => {
                let ps = params
                    .iter()
                    .enumerate()
                    .map(|(i, p)| format!("arg{}: {}", i, self.resolved_jsdoc(p)))
                    .collect::<Vec<_>>()
                    .join(", ");
                format!("function({}): {}", ps, self.resolved_jsdoc(ret))
            }
            ResolvedType::Never => "never".to_string(),
            ResolvedType::Unknown => "*".to_string(),
            ResolvedType::Ptr { inner, .. } => self.resolved_jsdoc(inner),
        }
    }

    // ── Program ────────────────────────────────────────────────────────────

    fn gen_program(&mut self, program: &TypedProgram) -> String {
        // Detect DOM usage
        for item in &program.items {
            if matches!(item, TypedItem::Component(_)) {
                self.needs_dom = true;
                break;
            }
        }

        // File header
        self.out.push("// @ts-check".to_string());
        self.out
            .push("// Generated by KAIN compiler — KainScript target (.ks)".to_string());
        self.out.push(
            "// Runs natively in Node.js, Deno, Bun, and browsers. No compilation needed."
                .to_string(),
        );
        self.out
            .push("// Full type checking: tsc --checkJs --noEmit file.ks".to_string());
        self.out.push("/* eslint-disable */".to_string());
        self.blank();

        // Numeric coercion helpers (documented with JSDoc for type safety)
        self.out.push(
            "// ── Numeric type coercion helpers ─────────────────────────────────────────"
                .to_string(),
        );
        self.out.push(
            "/** @param {number} n @returns {number} */ function u8(n)  { return n & 0xFF; }"
                .to_string(),
        );
        self.out.push(
            "/** @param {number} n @returns {number} */ function u16(n) { return n & 0xFFFF; }"
                .to_string(),
        );
        self.out.push(
            "/** @param {number} n @returns {number} */ function u32(n) { return n >>> 0; }"
                .to_string(),
        );
        self.out.push("/** @param {number} n @returns {number} */ function i8(n)  { return (n << 24) >> 24; }".to_string());
        self.out.push("/** @param {number} n @returns {number} */ function i16(n) { return (n << 16) >> 16; }".to_string());
        self.out.push(
            "/** @param {number} n @returns {number} */ function i32(n) { return n | 0; }"
                .to_string(),
        );
        self.out.push(
            "/** @param {number} n @returns {number} */ function f32(n) { return Math.fround(n); }"
                .to_string(),
        );
        self.blank();
        // KAIN stdlib bridge — maps bare KAIN names to JS globals so .ks runs
        // natively in Node.js, Deno, Bun, and browsers with no polyfills needed.
        self.out.push(
            "// ── KAIN stdlib bridge (auto-generated) ───────────────────────────────────"
                .to_string(),
        );
        self.out.push(
            "function __kain_clamp(v, lo, hi) { return v < lo ? lo : v > hi ? hi : v; }"
                .to_string(),
        );
        self.out
            .push("function println(...a) { console.log(...a); }".to_string());
        self.out.push(
            "function print(...a) { process?.stdout?.write(String(a[0])) ?? console.log(...a); }"
                .to_string(),
        );
        self.out
            .push("function push(arr, v) { arr.push(v); return arr; }".to_string());
        self.out
            .push("function pop(arr) { return arr.pop(); }".to_string());
        self.out
            .push("function len(v) { return v?.length ?? 0; }".to_string());
        self.out
            .push("function is_empty(v) { return (v?.length ?? 0) === 0; }".to_string());
        self.out
            .push("function map(arr, f) { return arr.map(f); }".to_string());
        self.out
            .push("function filter(arr, f) { return arr.filter(f); }".to_string());
        self.out
            .push("function reduce(arr, f, init) { return arr.reduce(f, init); }".to_string());
        self.out
            .push("function to_string(v) { return String(v); }".to_string());
        self.out
            .push("function parse_int(s) { return parseInt(s, 10); }".to_string());
        self.out
            .push("function parse_float(s) { return parseFloat(s); }".to_string());
        self.out
            .push("function http_get(url) { return fetch(url).then(r => r.text()); }".to_string());
        self.out.push("function http_post(url, body) { return fetch(url, { method: 'POST', body: JSON.stringify(body) }).then(r => r.text()); }".to_string());
        self.out
            .push("function json_parse(s) { return JSON.parse(s); }".to_string());
        self.out
            .push("function json_stringify(v) { return JSON.stringify(v); }".to_string());
        self.blank();

        if self.needs_dom {
            self.out
                .push("// DOM type hints for JSX components".to_string());
            self.out
                .push("/** @typedef {Node | DocumentFragment} KainNode */".to_string());
            self.blank();
        }

        // Collect items, emit structs/enums/functions/components
        for item in &program.items {
            match item {
                TypedItem::Struct(s) => self.gen_struct(s),
                TypedItem::Enum(e) => self.gen_enum(e),
                TypedItem::Function(f) => self.gen_function(f),
                TypedItem::Component(c) => {
                    self.needs_dom = true;
                    self.gen_component(c);
                }
                TypedItem::Const(c) => {
                    let ty = self.type_jsdoc(&c.ast.ty);
                    self.out.push(format!("/** @type {{{}}} */", ty));
                    let mut val = String::new();
                    self.expr_to_str(&c.ast.value, &mut val);
                    self.out.push(format!("const {} = {};", c.ast.name, val));
                    self.blank();
                }
                TypedItem::Impl(i) => self.gen_impl(&i.ast),
                _ => {}
            }
        }
        // Auto-call main() if defined — makes `node file.ks` work directly
        let has_main = program
            .items
            .iter()
            .any(|item| matches!(item, TypedItem::Function(f) if f.ast.name == "main"));
        if has_main {
            self.blank();
            self.out.push(
                "// Auto-entry: call main() if defined (node file.ks just works)".to_string(),
            );
            self.out
                .push("if (typeof main === 'function') main();".to_string());
        }

        std::mem::take(&mut self.out).build()
    }

    // ── Struct → @typedef + class ──────────────────────────────────────────

    fn gen_struct(&mut self, s: &TypedStruct) {
        if self.seen_types.contains(&s.ast.name) {
            return;
        }
        self.seen_types.insert(s.ast.name.clone());

        // Opaque C types → simple typedef
        if s.ast.fields.is_empty() {
            self.out.push(format!(
                "/** @typedef {{Record<string, *>}} {} */",
                s.ast.name
            ));
            self.blank();
            return;
        }

        // Full struct → jsdoc typedef + class with defaulted constructor
        let mut typedef = format!("/**\n * @typedef {{{{");
        for (i, f) in s.ast.fields.iter().enumerate() {
            let ty = self.type_jsdoc(&f.ty);
            let sep = if i == 0 { "" } else { "," };
            typedef.push_str(&format!("{} {}: {}", sep, f.name, ty));
        }
        typedef.push_str(&format!("}}}} {}", s.ast.name));
        typedef.push_str("\n */");
        self.out.push(typedef);

        // Emit a class that implements the typedef so `new Point(1,2)` works
        let ctor_params = s
            .ast
            .fields
            .iter()
            .map(|f| {
                let dflt = match self.type_jsdoc(&f.ty).as_str() {
                    "number" => "0".to_string(),
                    "boolean" => "false".to_string(),
                    "string" => r#""""#.to_string(),
                    t if t.ends_with("[]") => "[]".to_string(),
                    _ => "null".to_string(),
                };
                format!("{} = {}", f.name, dflt)
            })
            .collect::<Vec<_>>()
            .join(", ");

        self.out.push(format!("class {} {{", s.ast.name));
        // JSDoc for constructor params
        for f in &s.ast.fields {
            let ty = self.type_jsdoc(&f.ty);
            self.out
                .push(format!("  /** @type {{{}}} */ {};", ty, f.name));
        }
        self.out
            .push(format!("  /** @param {{{}}} _ */", s.ast.name));
        self.out.push(format!("  constructor({}) {{", ctor_params));
        for f in &s.ast.fields {
            self.out.push(format!("    this.{n} = {n};", n = f.name));
        }
        self.out.push("  }".to_string());
        self.out.push("}".to_string());
        self.blank();
    }

    // ── Enum → @typedef union + const object ──────────────────────────────

    fn gen_enum(&mut self, e: &TypedEnum) {
        if self.seen_types.contains(&e.ast.name) {
            return;
        }
        self.seen_types.insert(e.ast.name.clone());

        // Build @typedef union of all variants
        let mut variants_doc = vec![];
        let mut const_parts = vec![];

        for v in &e.ast.variants {
            match &v.fields {
                VariantFields::Unit => {
                    variants_doc.push(format!("{{{{ tag: '{}' }}}}", v.name));
                    const_parts.push(format!(
                        "  /** @type {{{{tag: '{}'}}}} */\n  {}: Object.freeze({{ tag: '{}' }}),",
                        v.name, v.name, v.name
                    ));
                }
                VariantFields::Tuple(types) => {
                    let fields: String = types
                        .iter()
                        .enumerate()
                        .map(|(i, t)| format!("_{i}: {}", self.type_jsdoc(t)))
                        .collect::<Vec<_>>()
                        .join(", ");
                    variants_doc.push(format!("{{{{ tag: '{}', {} }}}}", v.name, fields));
                    let param_list = types
                        .iter()
                        .enumerate()
                        .map(|(i, _)| format!("_{i}"))
                        .collect::<Vec<_>>()
                        .join(", ");
                    let obj_fields = types
                        .iter()
                        .enumerate()
                        .map(|(i, _)| format!("_{i}"))
                        .collect::<Vec<_>>()
                        .join(", ");
                    const_parts.push(format!(
                        "  {}({}) {{ return {{ tag: '{}', {} }}; }},",
                        v.name, param_list, v.name, obj_fields
                    ));
                }
                VariantFields::Struct(fields) => {
                    let fstr: String = fields
                        .iter()
                        .map(|f| format!("{}: {}", f.name, self.type_jsdoc(&f.ty)))
                        .collect::<Vec<_>>()
                        .join(", ");
                    variants_doc.push(format!("{{{{ tag: '{}', {} }}}}", v.name, fstr));
                    let param_list = fields
                        .iter()
                        .map(|f| f.name.as_str())
                        .collect::<Vec<_>>()
                        .join(", ");
                    let obj_fields: String = fields
                        .iter()
                        .map(|f| f.name.as_str())
                        .collect::<Vec<_>>()
                        .join(", ");
                    const_parts.push(format!(
                        "  {}({{{}}}) {{ return {{ tag: '{}', {} }}; }},",
                        v.name, param_list, v.name, obj_fields
                    ));
                }
            }
        }

        self.out.push(format!(
            "/** @typedef {{{}}} {} */",
            variants_doc.join(" | "),
            e.ast.name
        ));
        self.out.push(format!("const {} = {{", e.ast.name));
        for p in const_parts {
            self.out.push(format!("  {}", p));
        }
        self.out.push("};".to_string());
        self.blank();
    }

    // ── Function → JSDoc + plain function ─────────────────────────────────

    fn gen_function(&mut self, f: &TypedFunction) {
        // Build JSDoc block
        let mut doc_lines = vec!["/**".to_string()];

        for param in &f.ast.params {
            let ty = self.type_jsdoc(&param.ty);
            doc_lines.push(format!(" * @param {{{}}} {}", ty, param.name));
        }

        let ret = if let Some(ret_ty) = &f.ast.return_type {
            self.type_jsdoc(ret_ty)
        } else if let ResolvedType::Function { ret, .. } = &f.resolved_type {
            self.resolved_jsdoc(ret)
        } else {
            "void".to_string()
        };

        if !f.ast.effects.is_empty() {
            let effects: String = f
                .ast
                .effects
                .iter()
                .map(|e| format!("{:?}", e))
                .collect::<Vec<_>>()
                .join(", ");
            doc_lines.push(format!(" * @kain-effects {}", effects));
        }

        doc_lines.push(format!(" * @returns {{{}}}", ret));
        doc_lines.push(" */".to_string());

        for dl in doc_lines {
            self.out.push(dl);
        }

        // Function signature
        let params = f
            .ast
            .params
            .iter()
            .map(|p| p.name.as_str())
            .collect::<Vec<_>>()
            .join(", ");
        self.out
            .push(format!("function {}({}) {{", f.ast.name, params));

        self.indent();
        self.gen_block(&f.ast.body);
        self.dedent();
        self.out.push("}".to_string());
        self.blank();
    }

    // ── Component → documented function returning Element ─────────────────

    fn gen_component(&mut self, c: &TypedComponent) {
        let mut doc_lines = vec!["/**".to_string()];
        for prop in &c.ast.props {
            let ty = self.type_jsdoc(&prop.ty);
            doc_lines.push(format!(
                " * @param {{{{{}:{}, children?:KainNode[]}}}} props",
                prop.name, ty
            ));
        }
        if c.ast.props.is_empty() {
            doc_lines.push(" * @param {{children?: KainNode[]}} props".to_string());
        }
        doc_lines.push(" * @returns {KainNode}".to_string());
        doc_lines.push(" */".to_string());
        for dl in doc_lines {
            self.out.push(dl);
        }

        self.out.push(format!("function {}(props) {{", c.ast.name));
        let prop_names: Vec<_> = c.ast.props.iter().map(|p| p.name.as_str()).collect();
        if prop_names.is_empty() {
            self.out
                .push("  const { children = [] } = props;".to_string());
        } else {
            self.out.push(format!(
                "  const {{ {}, children = [] }} = props;",
                prop_names.join(", ")
            ));
        }
        // State
        for s in &c.ast.state {
            let mut val = String::new();
            self.expr_to_str(&s.initial, &mut val);
            self.out.push(format!("  let {} = {};", s.name, val));
        }
        self.out.push("  return ".to_string());
        let mut jsx_out = String::new();
        self.jsx_to_str(&c.ast.body, &mut jsx_out);
        self.out.push(format!("  {};", jsx_out));
        self.out.push("}".to_string());
        self.blank();
    }

    // ── Impl block ────────────────────────────────────────────────────────

    fn gen_impl(&mut self, i: &kain_core::ast::Impl) {
        let type_name = match &i.target_type {
            Type::Named { name, .. } => name.clone(),
            _ => return,
        };
        for method in &i.methods {
            // JSDoc
            self.out.push("/**".to_string());
            for p in &method.params {
                let ty = self.type_jsdoc(&p.ty);
                self.out.push(format!(" * @param {{{}}} {}", ty, p.name));
            }
            let ret = method
                .return_type
                .as_ref()
                .map(|t| self.type_jsdoc(t))
                .unwrap_or("void".to_string());
            self.out.push(format!(" * @returns {{{}}}", ret));
            self.out.push(" */".to_string());

            let params: Vec<_> = method.params.iter().map(|p| p.name.as_str()).collect();
            self.out.push(format!(
                "{}.prototype.{} = function({}) {{",
                type_name,
                method.name,
                params.join(", ")
            ));
            self.indent();
            self.gen_block(&method.body);
            self.dedent();
            self.out.push("};".to_string());
            self.blank();
        }
    }

    // ── Block / Statements ─────────────────────────────────────────────────

    fn gen_block(&mut self, block: &Block) {
        for stmt in &block.stmts {
            self.gen_stmt(stmt);
        }
    }

    fn gen_stmt(&mut self, stmt: &Stmt) {
        match stmt {
            Stmt::Let {
                pattern, ty, value, ..
            } => {
                let name = match pattern {
                    Pattern::Binding { name, mutable, .. } => {
                        let kw = if *mutable { "let" } else { "let" };
                        (kw, name.clone())
                    }
                    Pattern::Wildcard(_) => ("let", "_".to_string()),
                    _ => ("let", "/* complex pattern */".to_string()),
                };
                // Emit JSDoc type annotation inline if available
                if let Some(ty_ann) = ty {
                    self.line(format!("/** @type {{{}}} */", self.type_jsdoc(ty_ann)));
                }
                if let Some(val) = value {
                    let mut vs = String::new();
                    self.expr_to_str(val, &mut vs);
                    self.line(format!("{} {} = {};", name.0, name.1, vs));
                } else {
                    self.line(format!("{} {};", name.0, name.1));
                }
            }
            Stmt::Expr(expr) => {
                // If the expression is an `if/else` expression at statement level,
                // emit it as proper if/else statements instead of an IIFE so that
                // `return` statements inside branches actually return from the function.
                if let Expr::If {
                    condition,
                    then_branch,
                    else_branch,
                    ..
                } = expr
                {
                    let mut cond_s = String::new();
                    self.expr_to_str(condition, &mut cond_s);
                    self.line(format!("if ({}) {{", cond_s));
                    self.indent();
                    self.gen_block(then_branch);
                    self.dedent();
                    // walk else chain
                    let mut cur = else_branch.as_ref();
                    while let Some(b) = cur {
                        match b.as_ref() {
                            ElseBranch::Else(blk) => {
                                self.line("} else {");
                                self.indent();
                                self.gen_block(blk);
                                self.dedent();
                                cur = None;
                            }
                            ElseBranch::ElseIf(cond, blk, next) => {
                                let mut cs = String::new();
                                self.expr_to_str(cond, &mut cs);
                                self.line(format!("}} else if ({}) {{", cs));
                                self.indent();
                                self.gen_block(blk);
                                self.dedent();
                                cur = next.as_ref();
                            }
                        }
                    }
                    self.line("}");
                } else {
                    let mut vs = String::new();
                    self.expr_to_str(expr, &mut vs);
                    self.line(format!("{};", vs));
                }
            }
            Stmt::Return(Some(expr), _) => {
                let mut vs = String::new();
                self.expr_to_str(expr, &mut vs);
                self.line(format!("return {};", vs));
            }
            Stmt::Return(None, _) => self.line("return;"),
            Stmt::Break(Some(expr), _) => {
                let mut vs = String::new();
                self.expr_to_str(expr, &mut vs);
                self.line(format!("/* break {} */", vs));
            }
            Stmt::Break(None, _) => self.line("break;"),
            Stmt::Continue(_) => self.line("continue;"),
            Stmt::For {
                binding,
                iter,
                body,
                ..
            } => {
                let b = match binding {
                    Pattern::Binding { name, .. } => name.clone(),
                    Pattern::Tuple(pats, _) => {
                        let ns: Vec<_> = pats
                            .iter()
                            .filter_map(|p| {
                                if let Pattern::Binding { name, .. } = p {
                                    Some(name.as_str())
                                } else {
                                    None
                                }
                            })
                            .collect();
                        format!("[{}]", ns.join(", "))
                    }
                    _ => "_".to_string(),
                };
                let mut is = String::new();
                self.expr_to_str(iter, &mut is);
                self.line(format!("for (const {} of {}) {{", b, is));
                self.indent();
                self.gen_block(body);
                self.dedent();
                self.line("}");
            }
            Stmt::While {
                condition, body, ..
            } => {
                let mut cs = String::new();
                self.expr_to_str(condition, &mut cs);
                self.line(format!("while ({}) {{", cs));
                self.indent();
                self.gen_block(body);
                self.dedent();
                self.line("}");
            }
            Stmt::Loop { body, .. } => {
                self.line("while (true) {");
                self.indent();
                self.gen_block(body);
                self.dedent();
                self.line("}");
            }
            Stmt::Item(_) => { /* nested items hoisted to module scope by type checker */ }
        }
    }

    // ── Expressions → String ───────────────────────────────────────────────

    fn expr_to_str(&self, expr: &Expr, out: &mut String) {
        match expr {
            Expr::Int(n, _) => out.push_str(&n.to_string()),
            Expr::Float(f, _) => out.push_str(&f.to_string()),
            Expr::Bool(b, _) => out.push_str(&b.to_string()),
            Expr::None(_) => out.push_str("null"),
            Expr::String(s, _) => {
                out.push('"');
                out.push_str(
                    &s.replace('\\', "\\\\")
                        .replace('"', "\\\"")
                        .replace('\n', "\\n")
                        .replace('\r', "\\r")
                        .replace('\t', "\\t"),
                );
                out.push('"');
            }
            Expr::FString(parts, _) => {
                out.push('`');
                for p in parts {
                    match p {
                        Expr::String(s, _) => out.push_str(s),
                        other => {
                            out.push_str("${");
                            self.expr_to_str(other, out);
                            out.push('}');
                        }
                    }
                }
                out.push('`');
            }
            Expr::Ident(n, _) => {
                // Map KAIN stdlib bare names to JS global equivalents
                let mapped = match n.as_str() {
                    "sqrt" => "Math.sqrt",
                    "sin" => "Math.sin",
                    "cos" => "Math.cos",
                    "tan" => "Math.tan",
                    "atan" => "Math.atan",
                    "atan2" => "Math.atan2",
                    "asin" => "Math.asin",
                    "acos" => "Math.acos",
                    "floor" => "Math.floor",
                    "ceil" => "Math.ceil",
                    "round" => "Math.round",
                    "abs" => "Math.abs",
                    "pow" => "Math.pow",
                    "exp" => "Math.exp",
                    "log" => "Math.log",
                    "log2" => "Math.log2",
                    "log10" => "Math.log10",
                    "min" => "Math.min",
                    "max" => "Math.max",
                    "fmin" => "Math.min",
                    "fmax" => "Math.max",
                    "clamp" => "__kain_clamp",
                    other => other,
                };
                out.push_str(mapped);
            }
            Expr::Paren(inner, _) => {
                out.push('(');
                self.expr_to_str(inner, out);
                out.push(')');
            }
            Expr::Unary { op, operand, .. } => {
                let op_str = match op {
                    UnaryOp::Neg => "-",
                    UnaryOp::Not => "!",
                    UnaryOp::BitNot => "~",
                    UnaryOp::Ref | UnaryOp::RefMut | UnaryOp::Deref => "",
                };
                out.push_str(op_str);
                self.expr_to_str(operand, out);
            }
            Expr::Binary {
                left, op, right, ..
            } => {
                self.expr_to_str(left, out);
                out.push(' ');
                out.push_str(self.binop_str(*op));
                out.push(' ');
                self.expr_to_str(right, out);
            }
            Expr::Assign { target, value, .. } => {
                self.expr_to_str(target, out);
                out.push_str(" = ");
                self.expr_to_str(value, out);
            }
            Expr::Field { object, field, .. } => {
                self.expr_to_str(object, out);
                out.push('.');
                out.push_str(field);
            }
            Expr::Index { object, index, .. } => {
                self.expr_to_str(object, out);
                out.push('[');
                self.expr_to_str(index, out);
                out.push(']');
            }
            Expr::Call { callee, args, .. } => {
                self.expr_to_str(callee, out);
                out.push('(');
                for (i, arg) in args.iter().enumerate() {
                    if i > 0 {
                        out.push_str(", ");
                    }
                    self.expr_to_str(&arg.value, out);
                }
                out.push(')');
            }
            Expr::MethodCall {
                receiver,
                method,
                args,
                ..
            } => {
                // Known method translations
                match method.as_str() {
                    "len" | "count" => {
                        self.expr_to_str(receiver, out);
                        out.push_str(".length");
                        return;
                    }
                    "is_empty" => {
                        self.expr_to_str(receiver, out);
                        out.push_str(".length === 0");
                        return;
                    }
                    "push" | "append" => {
                        self.expr_to_str(receiver, out);
                        out.push_str(".push(");
                        for (i, a) in args.iter().enumerate() {
                            if i > 0 {
                                out.push_str(", ");
                            }
                            self.expr_to_str(&a.value, out);
                        }
                        out.push(')');
                        return;
                    }
                    "pop" => {
                        self.expr_to_str(receiver, out);
                        out.push_str(".pop()");
                        return;
                    }
                    "first" => {
                        self.expr_to_str(receiver, out);
                        out.push_str("[0]");
                        return;
                    }
                    "last" => {
                        // (() => { const __r = recv; return __r[__r.length-1]; })()
                        out.push_str("(() => { const __r = ");
                        self.expr_to_str(receiver, out);
                        out.push_str("; return __r[__r.length - 1]; })()");
                        return;
                    }
                    "contains" | "includes" => {
                        self.expr_to_str(receiver, out);
                        out.push_str(".includes(");
                        if let Some(a) = args.first() {
                            self.expr_to_str(&a.value, out);
                        }
                        out.push(')');
                        return;
                    }
                    "map" => {
                        self.method_higher_order(receiver, ".map(", args, out);
                        return;
                    }
                    "filter" => {
                        self.method_higher_order(receiver, ".filter(", args, out);
                        return;
                    }
                    "find" => {
                        self.expr_to_str(receiver, out);
                        out.push_str(".find(");
                        if let Some(a) = args.first() {
                            self.expr_to_str(&a.value, out);
                        }
                        out.push_str(") ?? null");
                        return;
                    }
                    "any" => {
                        self.method_higher_order(receiver, ".some(", args, out);
                        return;
                    }
                    "all" => {
                        self.method_higher_order(receiver, ".every(", args, out);
                        return;
                    }
                    "enumerate" => {
                        self.expr_to_str(receiver, out);
                        out.push_str(".map((__v, __i) => [__i, __v])");
                        return;
                    }
                    "collect" => {
                        out.push_str("Array.from(");
                        self.expr_to_str(receiver, out);
                        out.push(')');
                        return;
                    }
                    "iter" | "into_iter" | "iter_mut" => {
                        self.expr_to_str(receiver, out);
                        return;
                    }
                    "clone" | "to_vec" => {
                        out.push_str("[...");
                        self.expr_to_str(receiver, out);
                        out.push(']');
                        return;
                    }
                    "to_string" | "to_owned" => {
                        out.push_str("String(");
                        self.expr_to_str(receiver, out);
                        out.push(')');
                        return;
                    }
                    "to_uppercase" => {
                        self.expr_to_str(receiver, out);
                        out.push_str(".toUpperCase()");
                        return;
                    }
                    "to_lowercase" => {
                        self.expr_to_str(receiver, out);
                        out.push_str(".toLowerCase()");
                        return;
                    }
                    "trim" => {
                        self.expr_to_str(receiver, out);
                        out.push_str(".trim()");
                        return;
                    }
                    "split" => {
                        self.expr_to_str(receiver, out);
                        out.push_str(".split(");
                        if let Some(a) = args.first() {
                            self.expr_to_str(&a.value, out);
                        }
                        out.push(')');
                        return;
                    }
                    "join" => {
                        self.expr_to_str(receiver, out);
                        out.push_str(".join(");
                        if let Some(a) = args.first() {
                            self.expr_to_str(&a.value, out);
                        }
                        out.push(')');
                        return;
                    }
                    "unwrap" => {
                        out.push_str("(");
                        self.expr_to_str(receiver, out);
                        out.push_str(" ?? (() => { throw new Error('KAIN: unwrap on null'); })())");
                        return;
                    }
                    "unwrap_or" => {
                        out.push_str("(");
                        self.expr_to_str(receiver, out);
                        out.push_str(" ?? ");
                        if let Some(a) = args.first() {
                            self.expr_to_str(&a.value, out);
                        }
                        out.push(')');
                        return;
                    }
                    "is_some" | "is_ok" => {
                        out.push_str("((");
                        self.expr_to_str(receiver, out);
                        out.push_str(") != null)");
                        return;
                    }
                    "is_none" | "is_err" => {
                        out.push_str("((");
                        self.expr_to_str(receiver, out);
                        out.push_str(") == null)");
                        return;
                    }
                    "abs" => {
                        out.push_str("Math.abs(");
                        self.expr_to_str(receiver, out);
                        out.push(')');
                        return;
                    }
                    "sqrt" => {
                        out.push_str("Math.sqrt(");
                        self.expr_to_str(receiver, out);
                        out.push(')');
                        return;
                    }
                    "floor" => {
                        out.push_str("Math.floor(");
                        self.expr_to_str(receiver, out);
                        out.push(')');
                        return;
                    }
                    "ceil" => {
                        out.push_str("Math.ceil(");
                        self.expr_to_str(receiver, out);
                        out.push(')');
                        return;
                    }
                    "round" => {
                        out.push_str("Math.round(");
                        self.expr_to_str(receiver, out);
                        out.push(')');
                        return;
                    }
                    "parse" => {
                        out.push_str("Number(");
                        self.expr_to_str(receiver, out);
                        out.push(')');
                        return;
                    }
                    "sort" => {
                        out.push_str("[...");
                        self.expr_to_str(receiver, out);
                        out.push_str("].sort()");
                        return;
                    }
                    "reverse" => {
                        out.push_str("[...");
                        self.expr_to_str(receiver, out);
                        out.push_str("].reverse()");
                        return;
                    }
                    _ => {}
                }
                // Default
                self.expr_to_str(receiver, out);
                out.push('.');
                out.push_str(method);
                out.push('(');
                for (i, a) in args.iter().enumerate() {
                    if i > 0 {
                        out.push_str(", ");
                    }
                    self.expr_to_str(&a.value, out);
                }
                out.push(')');
            }
            Expr::Struct { name, fields, .. } => {
                // Use constructor
                out.push_str(&format!("new {}(", name));
                for (i, (_, v)) in fields.iter().enumerate() {
                    if i > 0 {
                        out.push_str(", ");
                    }
                    self.expr_to_str(v, out);
                }
                out.push(')');
            }
            Expr::EnumVariant {
                enum_name,
                variant,
                fields,
                ..
            } => match fields {
                EnumVariantFields::Unit => out.push_str(&format!("{}.{}", enum_name, variant)),
                EnumVariantFields::Tuple(exprs) => {
                    out.push_str(&format!("{}.{}(", enum_name, variant));
                    for (i, e) in exprs.iter().enumerate() {
                        if i > 0 {
                            out.push_str(", ");
                        }
                        self.expr_to_str(e, out);
                    }
                    out.push(')');
                }
                EnumVariantFields::Struct(fields) => {
                    out.push_str(&format!("{}.{}({{", enum_name, variant));
                    for (i, (name, val)) in fields.iter().enumerate() {
                        if i > 0 {
                            out.push_str(", ");
                        }
                        out.push_str(name);
                        out.push_str(": ");
                        self.expr_to_str(val, out);
                    }
                    out.push_str("})");
                }
            },
            Expr::Array(exprs, _) => {
                out.push('[');
                for (i, e) in exprs.iter().enumerate() {
                    if i > 0 {
                        out.push_str(", ");
                    }
                    self.expr_to_str(e, out);
                }
                out.push(']');
            }
            Expr::Tuple(exprs, _) => {
                out.push('[');
                for (i, e) in exprs.iter().enumerate() {
                    if i > 0 {
                        out.push_str(", ");
                    }
                    self.expr_to_str(e, out);
                }
                out.push(']');
            }
            Expr::Range {
                start,
                end,
                inclusive,
                ..
            } => {
                out.push_str("((__s, __e) => { const __a = []; for (let __i = __s; __i ");
                out.push_str(if *inclusive { "<= __e" } else { "< __e" });
                out.push_str("; __i++) __a.push(__i); return __a; })(");
                if let Some(s) = start {
                    self.expr_to_str(s, out);
                } else {
                    out.push('0');
                }
                out.push_str(", ");
                if let Some(e) = end {
                    self.expr_to_str(e, out);
                } else {
                    out.push('0');
                }
                out.push(')');
            }
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                out.push_str("(() => { if (");
                self.expr_to_str(condition, out);
                out.push_str(") { return ");
                self.block_to_str(then_branch, out);
                out.push_str("; }");
                let mut cur = else_branch.as_ref();
                while let Some(b) = cur {
                    match b.as_ref() {
                        ElseBranch::Else(blk) => {
                            out.push_str(" else { return ");
                            self.block_to_str(blk, out);
                            out.push_str("; }");
                            cur = None;
                        }
                        ElseBranch::ElseIf(cond, blk, next) => {
                            out.push_str(" else if (");
                            self.expr_to_str(cond, out);
                            out.push_str(") { return ");
                            self.block_to_str(blk, out);
                            out.push_str("; }");
                            cur = next.as_ref();
                        }
                    }
                }
                out.push_str(" return undefined; })()");
            }
            Expr::Match {
                scrutinee, arms, ..
            } => {
                out.push_str("(() => { const __s = ");
                self.expr_to_str(scrutinee, out);
                out.push_str("; ");
                for arm in arms {
                    let cond = self.pattern_cond("__s", &arm.pattern);
                    out.push_str(&format!("if ({}) {{ ", cond));
                    self.pattern_bindings_inline("__s", &arm.pattern, out);
                    out.push_str("return ");
                    self.expr_to_str(&arm.body, out);
                    out.push_str("; } ");
                }
                out.push_str("return undefined; })()");
            }
            Expr::Lambda { params, body, .. } => {
                let ps: Vec<_> = params.iter().map(|p| p.name.as_str()).collect();
                out.push('(');
                out.push_str(&ps.join(", "));
                out.push_str(") => ");
                self.expr_to_str(body, out);
            }
            Expr::Block(block, _) => {
                out.push_str("(() => { ");
                self.block_to_str(block, out);
                out.push_str(" })()");
            }
            Expr::Cast { value, .. } => {
                // JSDoc casts use /** @type {T} */ (value) but inside expression context
                // we just emit the value — type annotation is on the let binding
                out.push('(');
                self.expr_to_str(value, out);
                out.push(')');
            }
            Expr::Try(inner, _) => {
                out.push_str("(() => { const __r = ");
                self.expr_to_str(inner, out);
                out.push_str("; if (__r && __r.ok === false) throw __r.error; return __r?.value ?? __r; })()");
            }
            Expr::Await(inner, _) => {
                out.push_str("(await ");
                self.expr_to_str(inner, out);
                out.push(')');
            }
            Expr::Deref(inner, _) | Expr::Ref { value: inner, .. } => {
                self.expr_to_str(inner, out);
            }
            Expr::Comptime(inner, _) => self.expr_to_str(inner, out),
            Expr::Return(Some(v), _) => {
                out.push_str("(() => { return ");
                self.expr_to_str(v, out);
                out.push_str("; })()");
            }
            Expr::Return(None, _) => out.push_str("(() => {})()"),
            Expr::Break(Some(v), _) => {
                self.expr_to_str(v, out);
            }
            Expr::Break(None, _) | Expr::Continue(_) => out.push_str("undefined"),
            Expr::Spawn { actor, init, .. } => {
                out.push_str(&format!("new {}(", actor));
                for (i, (_, v)) in init.iter().enumerate() {
                    if i > 0 {
                        out.push_str(", ");
                    }
                    self.expr_to_str(v, out);
                }
                out.push(')');
            }
            Expr::SendMsg {
                target,
                message,
                data,
                ..
            } => {
                self.expr_to_str(target, out);
                out.push_str(&format!(".{}(", message));
                for (i, (_, v)) in data.iter().enumerate() {
                    if i > 0 {
                        out.push_str(", ");
                    }
                    self.expr_to_str(v, out);
                }
                out.push(')');
            }
            Expr::MacroCall { name, args, .. } => match name.as_str() {
                "print" | "println" | "eprintln" => {
                    out.push_str("console.log(");
                    for (i, a) in args.iter().enumerate() {
                        if i > 0 {
                            out.push_str(", ");
                        }
                        self.expr_to_str(a, out);
                    }
                    out.push(')');
                }
                "assert" => {
                    out.push_str("(() => { const __ok = ");
                    if let Some(e) = args.first() {
                        self.expr_to_str(e, out);
                    } else {
                        out.push_str("true");
                    }
                    out.push_str("; if (!__ok) throw new Error('KAIN assert failed'); })()");
                }
                "vec" | "array" => {
                    out.push('[');
                    for (i, a) in args.iter().enumerate() {
                        if i > 0 {
                            out.push_str(", ");
                        }
                        self.expr_to_str(a, out);
                    }
                    out.push(']');
                }
                "format" | "concat" => {
                    out.push('`');
                    for a in args {
                        out.push_str("${");
                        self.expr_to_str(a, out);
                        out.push('}');
                    }
                    out.push('`');
                }
                "todo" => {
                    out.push_str("(() => { throw new Error('TODO: not yet implemented'); })()")
                }
                "unreachable" => {
                    out.push_str("(() => { throw new Error('KAIN: unreachable'); })()")
                }
                "panic" => {
                    out.push_str("(() => { throw new Error(");
                    if let Some(e) = args.first() {
                        self.expr_to_str(e, out);
                    } else {
                        out.push_str("'panic'");
                    }
                    out.push_str("); })()");
                }
                _ => {
                    out.push_str(&format!("{}__macro(", name));
                    for (i, a) in args.iter().enumerate() {
                        if i > 0 {
                            out.push_str(", ");
                        }
                        self.expr_to_str(a, out);
                    }
                    out.push(')');
                }
            },
            Expr::JSX(node, _) => self.jsx_to_str(node, out),
            _ => out.push_str("/* unsupported */"),
        }
    }

    // ── Block → inline expression string ──────────────────────────────────

    fn block_to_str(&self, block: &Block, out: &mut String) {
        if let Some(last) = block.stmts.last() {
            match last {
                Stmt::Expr(e) | Stmt::Return(Some(e), _) => {
                    self.expr_to_str(e, out);
                    return;
                }
                _ => {}
            }
        }
        out.push_str("undefined");
    }

    // ── Pattern matching helpers ───────────────────────────────────────────

    fn pattern_cond(&self, scrutinee: &str, pattern: &Pattern) -> String {
        match pattern {
            Pattern::Wildcard(_) | Pattern::Binding { .. } => "true".to_string(),
            Pattern::Literal(e) => {
                let mut s = scrutinee.to_string();
                s.push_str(" === ");
                let mut vs = String::new();
                self.expr_to_str(e, &mut vs);
                s.push_str(&vs);
                s
            }
            Pattern::Variant {
                enum_name, variant, ..
            } => {
                if let Some(en) = enum_name {
                    format!(
                        "{}.type === '{}' && {}.tag === '{}'",
                        scrutinee, en, scrutinee, variant
                    )
                } else {
                    format!("{}.tag === '{}'", scrutinee, variant)
                }
            }
            Pattern::Range {
                start,
                end,
                inclusive,
                ..
            } => {
                let mut s = String::new();
                if let Some(st) = start {
                    self.expr_to_str(st, &mut s);
                    s.push_str(&format!(" <= {}", scrutinee));
                }
                if let Some(en) = end {
                    if !s.is_empty() {
                        s.push_str(" && ");
                    }
                    s.push_str(&format!("{} ", scrutinee));
                    if *inclusive {
                        s.push_str("<=");
                    } else {
                        s.push('<');
                    }
                    let mut es = String::new();
                    self.expr_to_str(en, &mut es);
                    s.push(' ');
                    s.push_str(&es);
                }
                if s.is_empty() {
                    "true".to_string()
                } else {
                    s
                }
            }
            Pattern::Or(patterns, _) => patterns
                .iter()
                .map(|p| self.pattern_cond(scrutinee, p))
                .collect::<Vec<_>>()
                .join(" || "),
            _ => "false".to_string(),
        }
    }

    fn pattern_bindings_inline(&self, scrutinee: &str, pattern: &Pattern, out: &mut String) {
        if let Pattern::Binding { name, .. } = pattern {
            out.push_str(&format!("const {} = {}; ", name, scrutinee));
        }
    }

    // ── JSX → string ──────────────────────────────────────────────────────

    fn jsx_to_str(&self, node: &JSXNode, out: &mut String) {
        match node {
            JSXNode::Text(s, _) => {
                out.push_str(&format!(
                    "document.createTextNode('{}')",
                    s.replace('\'', "\\'").replace('\\', "\\\\")
                ));
            }
            JSXNode::Element {
                tag,
                attributes,
                children,
                ..
            } => {
                out.push_str(&format!(
                    "(() => {{ const __el = document.createElement('{}'); ",
                    tag
                ));
                for attr in attributes {
                    let val = match &attr.value {
                        JSXAttrValue::String(s) => format!("'{}'", s),
                        JSXAttrValue::Bool(b) => b.to_string(),
                        JSXAttrValue::Expr(e) => {
                            let mut vs = String::new();
                            self.expr_to_str(e, &mut vs);
                            vs
                        }
                    };
                    if attr.name.starts_with("on") {
                        out.push_str(&format!(
                            "__el.addEventListener('{}', {}); ",
                            &attr.name[2..].to_lowercase(),
                            val
                        ));
                    } else {
                        out.push_str(&format!("__el.setAttribute('{}', {}); ", attr.name, val));
                    }
                }
                for child in children {
                    out.push_str("__el.appendChild(");
                    self.jsx_to_str(child, out);
                    out.push_str("); ");
                }
                out.push_str("return __el; })()");
            }
            JSXNode::Expression(e) => self.expr_to_str(e, out),
            JSXNode::Fragment(children, _) => {
                out.push_str("(() => { const __f = document.createDocumentFragment(); ");
                for c in children {
                    out.push_str("__f.appendChild(");
                    self.jsx_to_str(c, out);
                    out.push_str("); ");
                }
                out.push_str("return __f; })()");
            }
            JSXNode::ComponentCall {
                name,
                props,
                children,
                ..
            } => {
                out.push_str(&format!("{}({{", name));
                for (i, p) in props.iter().enumerate() {
                    if i > 0 {
                        out.push_str(", ");
                    }
                    let val = match &p.value {
                        JSXAttrValue::String(s) => format!("'{}'", s),
                        JSXAttrValue::Bool(b) => b.to_string(),
                        JSXAttrValue::Expr(e) => {
                            let mut vs = String::new();
                            self.expr_to_str(e, &mut vs);
                            vs
                        }
                    };
                    out.push_str(&format!("{}: {}", p.name, val));
                }
                if !children.is_empty() {
                    if !props.is_empty() {
                        out.push_str(", ");
                    }
                    out.push_str("children: [");
                    for (i, c) in children.iter().enumerate() {
                        if i > 0 {
                            out.push_str(", ");
                        }
                        self.jsx_to_str(c, out);
                    }
                    out.push(']');
                }
                out.push_str("})");
            }
            JSXNode::For {
                binding,
                iter,
                body,
                ..
            } => {
                out.push_str(&format!(
                    "(() => {{ const __f = document.createDocumentFragment(); for (const {} of ",
                    binding
                ));
                self.expr_to_str(iter, out);
                out.push_str(") { __f.appendChild(");
                self.jsx_to_str(body, out);
                out.push_str("); } return __f; })()");
            }
            JSXNode::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                out.push_str("(() => { if (");
                self.expr_to_str(condition, out);
                out.push_str(") { return ");
                self.jsx_to_str(then_branch, out);
                out.push_str("; }");
                if let Some(eb) = else_branch {
                    out.push_str(" return ");
                    self.jsx_to_str(eb, out);
                    out.push(';');
                }
                out.push_str(" return document.createTextNode(''); })()");
            }
        }
    }

    // ── Binop → JS string ─────────────────────────────────────────────────

    fn binop_str(&self, op: BinaryOp) -> &'static str {
        match op {
            BinaryOp::Add => "+",
            BinaryOp::Sub => "-",
            BinaryOp::Mul => "*",
            BinaryOp::Div => "/",
            BinaryOp::Mod => "%",
            BinaryOp::Pow => "**",
            BinaryOp::Eq => "===",
            BinaryOp::Ne => "!==",
            BinaryOp::Lt => "<",
            BinaryOp::Le => "<=",
            BinaryOp::Gt => ">",
            BinaryOp::Ge => ">=",
            BinaryOp::And => "&&",
            BinaryOp::Or => "||",
            BinaryOp::BitAnd => "&",
            BinaryOp::BitOr => "|",
            BinaryOp::BitXor => "^",
            BinaryOp::Shl => "<<",
            BinaryOp::Shr => ">>",
            BinaryOp::Assign => "=",
            BinaryOp::AddAssign => "+=",
            BinaryOp::SubAssign => "-=",
            BinaryOp::MulAssign => "*=",
            BinaryOp::DivAssign => "/=",
            BinaryOp::Range | BinaryOp::RangeInclusive => "/* .. */",
        }
    }

    // ── Higher-order method helper ─────────────────────────────────────────

    fn method_higher_order(
        &self,
        receiver: &Expr,
        method_js: &str,
        args: &[kain_core::ast::CallArg],
        out: &mut String,
    ) {
        self.expr_to_str(receiver, out);
        out.push_str(method_js);
        if let Some(a) = args.first() {
            self.expr_to_str(&a.value, out);
        }
        out.push(')');
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::types::TypedProgram;

    #[test]
    fn empty_program_has_ts_check_header() {
        let prog = TypedProgram { items: vec![] };
        let out = generate(&prog).unwrap();
        assert!(out.contains("// @ts-check"), "should start with @ts-check");
        assert!(
            out.contains("KainScript"),
            "should mention KainScript target"
        );
        assert!(
            !out.contains("KainNode"),
            "should not emit KainNode without components"
        );
    }

    #[test]
    fn numeric_helpers_present() {
        let prog = TypedProgram { items: vec![] };
        let out = generate(&prog).unwrap();
        assert!(out.contains("function u8(n)"));
        assert!(out.contains("function u32(n)"));
        assert!(out.contains("function f32(n)"));
    }
}
