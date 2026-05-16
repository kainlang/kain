//! Rust Code Generation - Transpiles KAIN AST to Rust source code
//!
//! This module generates valid Rust code from a typed KAIN program.
//! The generated Rust can be compiled with `rustc` directly or integrated
//! into a Cargo project.

pub mod artifact_bundle;
pub mod gpu_artifacts;
pub mod gpu_host;

use kain_core::ast::{
    BinaryOp, Block, CallArg, Component, ElseBranch, Enum, EnumVariantFields, Expr, Function,
    Generic, Impl, Item, JSXAttrValue, JSXNode, Mod, Param, Pattern, Stmt, Struct, Trait,
    TraitMethod, Type, UnaryOp, Use, VariantFields, VariantPatternFields,
};
use kain_core::effects::Effect;
use kain_core::error::KainResult;
use kain_core::parser::RESERVED_KEYWORDS;
use kain_core::types::{TypedItem, TypedMod, TypedProgram};
use kain_core::{lower_typed_program_memory_for_target, CompileTarget};
use std::collections::{BTreeSet, HashMap, HashSet};

pub use artifact_bundle::{
    generate_rust_artifact_bundle, RustArtifactBundle, RustArtifactKind, RustTextArtifact,
};
pub use gpu_artifacts::{
    collect_gpu_artifacts, collect_gpu_artifacts_json, RustGpuArtifactOutput,
    RustGpuBindingArtifact, RustGpuBindingKind, RustGpuInputArtifact, RustGpuShaderArtifact,
    RustGpuShaderStage,
};
pub use gpu_host::generate_gpu_host;

/// Generate Rust source code from a typed program
pub fn generate(program: &TypedProgram) -> KainResult<String> {
    let lowered = lower_typed_program_memory_for_target(program, CompileTarget::Rust)?;
    let has_modules = typed_program_has_modules(&lowered);
    let mut gen = RustGen::new(has_modules);
    let rendered = gen.gen_program(&lowered);
    if has_modules {
        Ok(rendered)
    } else {
        Ok(postprocess_flattened_selfhost_output(&rendered))
    }
}

pub fn generate_gpu_host_wrappers(program: &TypedProgram) -> KainResult<String> {
    let lowered = lower_typed_program_memory_for_target(program, CompileTarget::Rust)?;
    Ok(gpu_host::generate_gpu_host(&lowered))
}

// StringBuilder helper for accumulated output
struct StringBuilder {
    lines: Vec<String>,
}

impl StringBuilder {
    fn new() -> Self {
        Self { lines: Vec::new() }
    }

    fn push_line(&mut self, text: &str) {
        self.lines.push(format!("{}\n", text));
    }

    fn build(&self) -> String {
        self.lines.join("")
    }
}

// Main Rust code generator
struct RustGen {
    output: StringBuilder,
    indent: usize,
    has_modules: bool,
    emitted_symbols: HashSet<String>,
    known_type_names: HashSet<String>,
    struct_fields: HashMap<String, Vec<String>>,
    binding_type_stack: Vec<HashMap<String, String>>,
    current_function_stack: Vec<String>,
    current_return_struct_stack: Vec<Option<String>>,
    self_type_stack: Vec<String>,
    impl_target_stack: Vec<String>,
    impl_trait_stack: Vec<Option<String>>,
    generic_lifetime_stack: Vec<HashSet<String>>,
    display_helpers: BTreeSet<String>,
    free_self_alias_stack: Vec<Option<String>>,
}

fn postprocess_flattened_selfhost_output(source: &str) -> String {
    const LOCAL_MODULE_PREFIXES: &[&str] = &[
        "crate::runtime::",
        "crate::ui::",
        "crate::ast::",
        "crate::error::",
        "crate::effects::",
        "crate::types::",
        "crate::lexer::",
        "crate::diagnostic_registry::",
        "crate::diagnostics::",
        "crate::parser::",
        "crate::span::",
        "crate::language_features::",
        "crate::low_level_abi::",
        "crate::low_level_memory::",
        "crate::low_level_memory_metadata::",
        "crate::monomorphize::",
        "crate::stdlib::",
        "crate::comptime::",
        "crate::asm_ir::",
        "runtime::",
        "ui::",
        "ast::",
        "error::",
        "effects::",
        "types::",
        "lexer::",
        "diagnostic_registry::",
        "diagnostics::",
        "parser::",
        "span::",
        "language_features::",
        "low_level_abi::",
        "low_level_memory::",
        "low_level_memory_metadata::",
        "monomorphize::",
        "stdlib::",
        "comptime::",
        "asm_ir::",
    ];

    let mut output = source.to_string();
    for prefix in LOCAL_MODULE_PREFIXES {
        output = replace_prefix_at_token_boundaries(&output, prefix, "");
    }
    output
}

fn replace_prefix_at_token_boundaries(source: &str, prefix: &str, replacement: &str) -> String {
    let mut output = String::with_capacity(source.len());
    let mut cursor = 0;
    while cursor < source.len() {
        if source[cursor..].starts_with(prefix) && is_token_boundary_before(source, cursor) {
            output.push_str(replacement);
            cursor += prefix.len();
            continue;
        }
        let ch = source[cursor..].chars().next().unwrap();
        output.push(ch);
        cursor += ch.len_utf8();
    }
    output
}

fn is_token_boundary_before(source: &str, index: usize) -> bool {
    if index == 0 {
        return true;
    }
    !matches!(
        source[..index].chars().next_back(),
        Some(ch) if ch.is_ascii_alphanumeric() || ch == '_'
    )
}

fn typed_program_has_modules(program: &TypedProgram) -> bool {
    program
        .items
        .iter()
        .any(|item| matches!(item, TypedItem::Mod(_)))
}

const EMITTED_IDENTIFIER_CONTEXTUAL_KEYWORDS: &[&str] = &["state", "weak", "compute", "shader"];
const RUST_RESERVED_KEYWORDS: &[&str] = &[
    "Self", "as", "async", "await", "break", "const", "continue", "crate", "dyn", "else", "enum",
    "extern", "false", "fn", "for", "if", "impl", "in", "let", "loop", "match", "mod", "move",
    "mut", "pub", "ref", "return", "self", "static", "struct", "super", "trait", "true", "type",
    "unsafe", "use", "where", "while",
];
const RUST_PATH_ROOT_SEGMENTS: &[&str] = &["Self", "crate", "self", "super"];

fn sanitize_rust_identifier(name: &str) -> String {
    let mut sanitized = name
        .chars()
        .map(|ch| {
            if ch.is_ascii_alphanumeric() || ch == '_' {
                ch
            } else {
                '_'
            }
        })
        .collect::<String>();
    if sanitized.is_empty() {
        sanitized.push('_');
    }
    if sanitized
        .chars()
        .next()
        .is_some_and(|ch| ch.is_ascii_digit())
    {
        sanitized.insert(0, '_');
    }
    if RUST_RESERVED_KEYWORDS.contains(&sanitized.as_str()) || sanitized == "_" {
        sanitized.push('_');
    }
    sanitized
}

fn desanitize_kain_identifier(name: &str) -> String {
    let candidate = name.trim();
    let Some(stripped) = candidate.strip_suffix('_') else {
        return candidate.to_string();
    };
    if stripped.is_empty() {
        return candidate.to_string();
    }
    if (RESERVED_KEYWORDS.contains(&stripped)
        || EMITTED_IDENTIFIER_CONTEXTUAL_KEYWORDS.contains(&stripped))
        && !RUST_RESERVED_KEYWORDS.contains(&stripped)
    {
        return stripped.to_string();
    }
    candidate.to_string()
}

fn sanitize_emitted_identifier(name: &str) -> String {
    sanitize_rust_identifier(&desanitize_kain_identifier(name))
}

fn sanitize_emitted_path_segment(segment: &str, is_root: bool) -> String {
    let normalized = desanitize_kain_identifier(segment);
    if is_root && RUST_PATH_ROOT_SEGMENTS.contains(&normalized.as_str()) {
        normalized
    } else {
        sanitize_rust_identifier(&normalized)
    }
}

fn sanitize_emitted_path(path: &str) -> String {
    path.split("::")
        .filter(|segment| !segment.is_empty())
        .enumerate()
        .map(|(index, segment)| sanitize_emitted_path_segment(segment, index == 0))
        .collect::<Vec<_>>()
        .join("::")
}

fn tuple_field_marker_index(field: &str) -> Option<&str> {
    let index = field.strip_prefix("__kain_tuple_")?;
    (!index.is_empty() && index.chars().all(|ch| ch.is_ascii_digit())).then_some(index)
}

fn strip_rendered_type_generics(rendered: &str) -> String {
    let mut depth = 0usize;
    let mut output = String::with_capacity(rendered.len());
    for ch in rendered.chars() {
        match ch {
            '<' => depth += 1,
            '>' => depth = depth.saturating_sub(1),
            _ if depth == 0 => output.push(ch),
            _ => {}
        }
    }
    output.trim().to_string()
}

fn split_sanitized_dunder_path(path: &str) -> Vec<String> {
    let mut segments = Vec::new();
    let mut current = String::new();
    let chars: Vec<char> = path.chars().collect();
    let mut index = 0;
    while index < chars.len() {
        if chars[index] == '_' {
            let run_start = index;
            while index < chars.len() && chars[index] == '_' {
                index += 1;
            }
            let run_len = index - run_start;
            if run_len >= 2 {
                current.push_str(&"_".repeat(run_len.saturating_sub(2)));
                if !current.is_empty() {
                    segments.push(std::mem::take(&mut current));
                }
                continue;
            }
            current.push('_');
            continue;
        }
        current.push(chars[index]);
        index += 1;
    }
    if !current.is_empty() {
        segments.push(current);
    }
    segments
}

impl RustGen {
    fn new(has_modules: bool) -> Self {
        Self {
            output: StringBuilder::new(),
            indent: 0,
            has_modules,
            emitted_symbols: HashSet::new(),
            known_type_names: HashSet::new(),
            struct_fields: HashMap::new(),
            binding_type_stack: Vec::new(),
            current_function_stack: Vec::new(),
            current_return_struct_stack: Vec::new(),
            self_type_stack: Vec::new(),
            impl_target_stack: Vec::new(),
            impl_trait_stack: Vec::new(),
            generic_lifetime_stack: vec![HashSet::new()],
            display_helpers: BTreeSet::new(),
            free_self_alias_stack: Vec::new(),
        }
    }

    fn push_indent(&mut self) {
        self.indent += 1;
    }

    fn pop_indent(&mut self) {
        if self.indent > 0 {
            self.indent -= 1;
        }
    }

    fn indent_str(&self) -> String {
        "    ".repeat(self.indent)
    }

    fn write_line(&mut self, line: &str) {
        let indented = format!("{}{}", self.indent_str(), line);
        self.output.push_line(&indented);
    }

    fn write_blank(&mut self) {
        self.output.push_line("");
    }

    fn push_generic_lifetimes(&mut self, lifetimes: HashSet<String>) {
        let mut merged = self
            .generic_lifetime_stack
            .last()
            .cloned()
            .unwrap_or_default();
        merged.extend(lifetimes);
        self.generic_lifetime_stack.push(merged);
    }

    fn pop_generic_lifetimes(&mut self) {
        if self.generic_lifetime_stack.len() > 1 {
            self.generic_lifetime_stack.pop();
        }
    }

    fn current_generic_lifetimes(&self) -> &HashSet<String> {
        self.generic_lifetime_stack
            .last()
            .expect("generic lifetime stack must be initialized")
    }

    // Generate Rust code for an entire program
    fn gen_program(&mut self, program: &TypedProgram) -> String {
        self.collect_known_type_names(program);

        // Header
        self.write_line("// Generated by KAIN Compiler (Project Ouroboros)");
        self.write_line("// Do not edit - regenerate from .kn source");
        self.write_blank();
        self.write_line("#![allow(unused_variables)]");
        self.write_line("#![allow(unused_mut)]");
        self.write_line("#![allow(dead_code)]");
        self.write_line("#![allow(unused_parens)]");
        self.write_blank();

        // Standard imports
        self.write_line("use std::collections::{HashMap, HashSet};");
        self.write_line("use std::rc::Rc;");
        self.write_line("use std::cell::{Cell, RefCell};");
        self.write_line("use std::sync::{Arc, Mutex, RwLock};");
        self.write_line("use std::borrow::Cow;");
        self.write_line("use std::mem::{size_of, ManuallyDrop, MaybeUninit};");
        self.write_line("use std::pin::Pin;");
        self.write_line("use std::cmp::min;");
        self.write_line("use std::ffi::c_void;");
        self.write_line("use std::fmt::Formatter;");
        self.write_blank();
        self.emit_runtime_helpers();
        self.write_blank();
        self.write_low_level_memory_helpers();
        self.write_blank();

        // Generate each item
        for item in &program.items {
            self.gen_item(item);
            self.write_blank();
        }

        self.emit_synthetic_display_impls();

        self.output.build()
    }

    fn collect_known_type_names(&mut self, program: &TypedProgram) {
        self.known_type_names.clear();
        self.struct_fields.clear();
        for item in &program.items {
            self.collect_known_type_names_from_item(item);
        }
    }

    fn collect_known_type_names_from_item(&mut self, item: &TypedItem) {
        match item {
            TypedItem::Struct(st) => {
                self.known_type_names.insert(st.ast.name.clone());
                self.struct_fields.insert(
                    st.ast.name.clone(),
                    st.ast
                        .fields
                        .iter()
                        .map(|field| field.name.clone())
                        .collect(),
                );
            }
            TypedItem::Enum(en) => {
                self.known_type_names.insert(en.ast.name.clone());
            }
            TypedItem::TypeAlias(alias) => {
                self.known_type_names.insert(alias.ast.name.clone());
            }
            TypedItem::Component(component) => {
                self.known_type_names.insert(component.ast.name.clone());
                self.known_type_names
                    .insert(self.component_props_name(&component.ast.name));
                let props_name = self.component_props_name(&component.ast.name);
                let mut fields: Vec<String> = component
                    .ast
                    .props
                    .iter()
                    .map(|prop| prop.name.clone())
                    .collect();
                fields.push("children".to_string());
                self.struct_fields.insert(props_name, fields);
            }
            TypedItem::Mod(module) => {
                for child in &module.items {
                    self.collect_known_type_names_from_item(child);
                }
            }
            _ => {}
        }
    }

    fn emit_runtime_helpers(&mut self) {
        self.write_line("unsafe extern \"C\" {");
        self.push_indent();
        self.write_line("fn malloc(size: usize) -> *mut c_void;");
        self.write_line("fn calloc(count: usize, size: usize) -> *mut c_void;");
        self.write_line("fn realloc(ptr: *mut c_void, size: usize) -> *mut c_void;");
        self.pop_indent();
        self.write_line("}");
        self.write_line("fn __kain_pow(left: f64, right: f64) -> f64 {");
        self.push_indent();
        self.write_line("left.powf(right)");
        self.pop_indent();
        self.write_line("}");
        self.write_line("fn __kain_alloc_bytes(size: usize, zeroed: bool) -> *mut u8 {");
        self.push_indent();
        self.write_line("unsafe {");
        self.push_indent();
        self.write_line(
            "if zeroed { calloc(1, size) as *mut u8 } else { malloc(size) as *mut u8 }",
        );
        self.pop_indent();
        self.write_line("}");
        self.pop_indent();
        self.write_line("}");
        self.write_line(
            "fn __kain_realloc_bytes(ptr: *mut u8, size: usize, _zeroed_new: bool) -> *mut u8 {",
        );
        self.push_indent();
        self.write_line("unsafe { realloc(ptr.cast::<c_void>(), size) as *mut u8 }");
        self.pop_indent();
        self.write_line("}");
    }

    fn emit_synthetic_display_impls(&mut self) {
        let helpers: Vec<String> = self.display_helpers.iter().cloned().collect();
        for target in helpers {
            self.write_line(&format!("impl std::fmt::Display for {} {{", target));
            self.push_indent();
            self.write_line("fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {");
            self.push_indent();
            self.write_line("self.fmt_impl(f).map_err(|_| std::fmt::Error)");
            self.pop_indent();
            self.write_line("}");
            self.pop_indent();
            self.write_line("}");
            self.write_blank();
        }
    }

    fn write_low_level_memory_helpers(&mut self) {
        self.write_line("fn __kain_union_wrap<TObject: Copy, TValue: Copy>(mut value: TObject, _active: &str, _type_key: &str, byte_size: i64, union_size: i64, active_value: TValue) -> TObject {");
        self.push_indent();
        self.write_line("let zero_span = min(union_size.max(0) as usize, size_of::<TObject>());");
        self.write_line("let copy_span = min(min(byte_size.max(0) as usize, union_size.max(0) as usize), min(size_of::<TObject>(), size_of::<TValue>()));");
        self.write_line("unsafe {");
        self.push_indent();
        self.write_line(
            "std::ptr::write_bytes((&mut value as *mut TObject).cast::<u8>(), 0, zero_span);",
        );
        self.write_line("if copy_span > 0 { std::ptr::copy_nonoverlapping((&active_value as *const TValue).cast::<u8>(), (&mut value as *mut TObject).cast::<u8>(), copy_span); }");
        self.pop_indent();
        self.write_line("}");
        self.write_line("value");
        self.pop_indent();
        self.write_line("}");
        self.write_line("fn __kain_union_get<TObject: Copy, TValue: Copy>(value: TObject, _field: &str, _type_key: &str, byte_size: i64, union_size: i64, fallback: TValue) -> TValue {");
        self.push_indent();
        self.write_line("let mut result = fallback;");
        self.write_line("let copy_span = min(min(byte_size.max(0) as usize, union_size.max(0) as usize), min(size_of::<TObject>(), size_of::<TValue>()));");
        self.write_line("unsafe { if copy_span > 0 { std::ptr::copy_nonoverlapping((&value as *const TObject).cast::<u8>(), (&mut result as *mut TValue).cast::<u8>(), copy_span); } }");
        self.write_line("result");
        self.pop_indent();
        self.write_line("}");
        self.write_line("fn __kain_union_set<TObject: Copy, TValue: Copy>(mut value: TObject, _field: &str, _type_key: &str, byte_size: i64, union_size: i64, next: TValue) -> TValue {");
        self.push_indent();
        self.write_line("let zero_span = min(union_size.max(0) as usize, size_of::<TObject>());");
        self.write_line("let copy_span = min(min(byte_size.max(0) as usize, union_size.max(0) as usize), min(size_of::<TObject>(), size_of::<TValue>()));");
        self.write_line("unsafe {");
        self.push_indent();
        self.write_line(
            "std::ptr::write_bytes((&mut value as *mut TObject).cast::<u8>(), 0, zero_span);",
        );
        self.write_line("if copy_span > 0 { std::ptr::copy_nonoverlapping((&next as *const TValue).cast::<u8>(), (&mut value as *mut TObject).cast::<u8>(), copy_span); }");
        self.pop_indent();
        self.write_line("}");
        self.write_line("next");
        self.pop_indent();
        self.write_line("}");
        self.write_line(
            "fn __kain_load_bitfield_unit<T: Copy>(value: &T, unit_offset: i64) -> u64 {",
        );
        self.push_indent();
        self.write_line(
            "if unit_offset < 0 || unit_offset as usize >= size_of::<T>() { return 0; }",
        );
        self.write_line("let mut unit = 0u64;");
        self.write_line("let available = min(8usize, size_of::<T>() - unit_offset as usize);");
        self.write_line("unsafe { std::ptr::copy_nonoverlapping((value as *const T).cast::<u8>().add(unit_offset as usize), (&mut unit as *mut u64).cast::<u8>(), available); }");
        self.write_line("unit");
        self.pop_indent();
        self.write_line("}");
        self.write_line(
            "fn __kain_store_bitfield_unit<T: Copy>(value: &mut T, unit_offset: i64, unit: u64) {",
        );
        self.push_indent();
        self.write_line("if unit_offset < 0 || unit_offset as usize >= size_of::<T>() { return; }");
        self.write_line("let available = min(8usize, size_of::<T>() - unit_offset as usize);");
        self.write_line("unsafe { std::ptr::copy_nonoverlapping((&unit as *const u64).cast::<u8>(), (value as *mut T).cast::<u8>().add(unit_offset as usize), available); }");
        self.pop_indent();
        self.write_line("}");
        self.write_line("fn __kain_bitfield_mask(width: i64) -> u64 { if width <= 0 { 0 } else if width >= 64 { u64::MAX } else { (1u64 << width) - 1 } }");
        self.write_line("fn __kain_sign_extend(value: u64, width: i64) -> i64 { if width <= 0 { 0 } else if width >= 64 { value as i64 } else { let sign_bit = 1u64 << (width - 1); if (value & sign_bit) == 0 { value as i64 } else { (value | !__kain_bitfield_mask(width)) as i64 } } }");
        self.write_line("fn __kain_bitfield_get<T: Copy>(value: T, _field: &str, unit_offset: i64, bit_offset: i64, width: i64, is_signed: bool, _promoted_bits: i64) -> i64 {");
        self.push_indent();
        self.write_line("let mask = __kain_bitfield_mask(width);");
        self.write_line("let unit = __kain_load_bitfield_unit(&value, unit_offset);");
        self.write_line("let shifted = if bit_offset <= 0 { unit } else { unit >> bit_offset };");
        self.write_line("let encoded = shifted & mask;");
        self.write_line(
            "if is_signed { __kain_sign_extend(encoded, width) } else { encoded as i64 }",
        );
        self.pop_indent();
        self.write_line("}");
        self.write_line("fn __kain_bitfield_set<T: Copy, TValue: Copy + Into<i64>>(mut value: T, _field: &str, unit_offset: i64, bit_offset: i64, width: i64, is_signed: bool, promoted_bits: i64, next: TValue) -> TValue {");
        self.push_indent();
        self.write_line("let mask = __kain_bitfield_mask(width);");
        self.write_line("let mut unit = __kain_load_bitfield_unit(&value, unit_offset);");
        self.write_line("let encoded = (next.into() as u64) & mask;");
        self.write_line(
            "let shifted_mask = if bit_offset <= 0 { mask } else { mask << bit_offset };",
        );
        self.write_line("unit = (unit & !shifted_mask) | if bit_offset <= 0 { encoded } else { encoded << bit_offset };");
        self.write_line("__kain_store_bitfield_unit(&mut value, unit_offset, unit);");
        self.write_line("let _ = __kain_bitfield_get(value, \"\", unit_offset, bit_offset, width, is_signed, promoted_bits);");
        self.write_line("next");
        self.pop_indent();
        self.write_line("}");
    }

    fn gen_item(&mut self, item: &TypedItem) {
        match item {
            TypedItem::Function(fn_typed) => {
                let emitted_name = self.rust_function_name(&fn_typed.ast.name);
                if self.should_emit_function(&fn_typed.ast) {
                    if let Some(target) = self.synthetic_impl_target(&fn_typed.ast) {
                        if self.register_symbol("method", &format!("{target}::{emitted_name}")) {
                            self.gen_synthetic_impl_function(&target, &fn_typed.ast);
                        }
                    } else if self.register_symbol("fn", &emitted_name) {
                        self.gen_function(&fn_typed.ast);
                    }
                }
            }
            TypedItem::Component(component) => self.gen_component(&component.ast),
            TypedItem::Struct(st) => {
                if self.register_symbol("type", &st.ast.name) {
                    self.gen_struct(&st.ast);
                }
            }
            TypedItem::Enum(en) => {
                if self.register_symbol("type", &en.ast.name) {
                    self.gen_enum(&en.ast);
                }
            }
            TypedItem::Trait(tr) => {
                if self.register_symbol("trait", &tr.ast.name) {
                    self.gen_trait(&tr.ast);
                }
            }
            TypedItem::Impl(im) => self.gen_impl(&im.ast),
            TypedItem::Const(c) => {
                if self.register_symbol("const", &c.ast.name) {
                    self.gen_const(&c.ast.name, &c.ast.ty, &c.ast.value, c.ast.visibility);
                }
            }
            TypedItem::TypeAlias(alias) => {
                if self.register_symbol("type", &alias.ast.name) {
                    let lifetime_names = self
                        .type_alias_lifetime_generic_names(&alias.ast.generics, &alias.ast.target);
                    self.push_generic_lifetimes(lifetime_names.clone());
                    let vis = self.visibility_prefix(alias.ast.visibility);
                    let generics = self.render_generic_params(&alias.ast.generics, &lifetime_names);
                    self.write_line(&format!(
                        "{vis}type {}{} = {};",
                        sanitize_emitted_identifier(&alias.ast.name),
                        generics,
                        self.map_type(&alias.ast.target)
                    ));
                    self.pop_generic_lifetimes();
                }
            }
            TypedItem::Use(u) => self.gen_use(&u.ast),
            TypedItem::Mod(module) => self.gen_typed_mod(module),
            _ => {}
        }
    }

    fn gen_const(
        &mut self,
        name: &str,
        ty: &Type,
        value: &Expr,
        visibility: kain_core::ast::Visibility,
    ) {
        let vis = self.visibility_prefix(visibility);
        if let Type::Named {
            name: type_name, ..
        } = ty
        {
            if type_name == "Lazy" || type_name.ends_with("::Lazy") {
                self.write_line(&format!(
                    "{vis}static {name}: {} = {};",
                    self.map_storage_type(ty, None),
                    self.gen_const_value_expr(value)
                ));
                return;
            }
        }
        if matches!(value, Expr::String(_, _)) {
            self.write_line(&format!(
                "{vis}static {name}: &str = {};",
                self.gen_const_value_expr(value)
            ));
            return;
        }
        if self.is_simple_const(ty, value) {
            self.write_line(&format!(
                "{vis}static {name}: {} = {};",
                self.map_storage_type(ty, None),
                self.gen_const_value_expr(value)
            ));
            return;
        }
        self.write_line(&format!(
            "{vis}static {name}: once_cell::sync::Lazy<{}> = once_cell::sync::Lazy::new(|| {});",
            self.map_const_type(ty),
            self.gen_const_value_expr(value)
        ));
    }

    fn gen_use(&mut self, use_item: &Use) {
        self.write_line(&self.render_use(use_item));
    }

    fn gen_nested_item(&mut self, item: &Item) {
        match item {
            Item::Function(function) => self.gen_function(function),
            Item::Struct(struct_def) => self.gen_struct(struct_def),
            Item::Enum(enum_def) => self.gen_enum(enum_def),
            Item::Trait(trait_def) => self.gen_trait(trait_def),
            Item::Impl(impl_def) => self.gen_impl(impl_def),
            Item::Const(const_def) => {
                self.gen_const(
                    &const_def.name,
                    &const_def.ty,
                    &const_def.value,
                    const_def.visibility,
                );
            }
            Item::Use(use_item) => self.gen_use(use_item),
            Item::Mod(module) => self.gen_ast_mod(module),
            Item::TypeAlias(alias) => {
                let lifetime_names =
                    self.type_alias_lifetime_generic_names(&alias.generics, &alias.target);
                self.push_generic_lifetimes(lifetime_names.clone());
                let vis = self.visibility_prefix(alias.visibility);
                let generics = self.render_generic_params(&alias.generics, &lifetime_names);
                self.write_line(&format!(
                    "{vis}type {}{} = {};",
                    sanitize_emitted_identifier(&alias.name),
                    generics,
                    self.map_type(&alias.target)
                ));
                self.pop_generic_lifetimes();
            }
            _ => {}
        }
    }

    fn gen_typed_mod(&mut self, module: &TypedMod) {
        let vis = self.visibility_prefix(module.ast.visibility);
        self.write_line(&format!(
            "{vis}mod {} {{",
            sanitize_emitted_identifier(&module.ast.name)
        ));
        self.push_indent();
        self.with_symbol_scope(|gen| {
            for child in &module.items {
                gen.gen_item(child);
                gen.write_blank();
            }
        });
        self.pop_indent();
        self.write_line("}");
    }

    fn gen_ast_mod(&mut self, module: &Mod) {
        let vis = self.visibility_prefix(module.visibility);
        self.write_line(&format!(
            "{vis}mod {} {{",
            sanitize_emitted_identifier(&module.name)
        ));
        self.push_indent();
        self.with_symbol_scope(|gen| {
            if let Some(children) = &module.inline {
                for child in children {
                    gen.gen_nested_item(child);
                    gen.write_blank();
                }
            }
        });
        self.pop_indent();
        self.write_line("}");
    }

    fn gen_trait(&mut self, trait_def: &Trait) {
        let lifetime_names = self.trait_lifetime_generic_names(trait_def);
        self.push_generic_lifetimes(lifetime_names.clone());
        let vis = self.visibility_prefix(trait_def.visibility);
        let generics = self.render_generic_params(&trait_def.generics, &lifetime_names);
        self.write_line(&format!(
            "{vis}trait {}{} {{",
            sanitize_emitted_identifier(&trait_def.name),
            generics
        ));
        self.push_indent();
        for method in &trait_def.methods {
            self.gen_trait_method(method);
            self.write_blank();
        }
        self.pop_indent();
        self.write_line("}");
        self.pop_generic_lifetimes();
    }

    fn gen_trait_method(&mut self, method: &TraitMethod) {
        let params = self.render_trait_method_params(&method.params);
        let ret = method
            .return_type
            .as_ref()
            .map(|ty| format!(" -> {}", self.map_return_type(ty, None)))
            .unwrap_or_default();
        let name = self.normalize_runtime_method(&self.rust_function_name(&method.name));
        if let Some(default_impl) = &method.default_impl {
            self.write_line(&format!("fn {name}({params}){ret} {{"));
            self.push_indent();
            let has_implicit_return = method
                .return_type
                .as_ref()
                .is_some_and(|ty| !matches!(ty, Type::Unit(_)));
            self.gen_block_with_implicit_return(default_impl, has_implicit_return);
            self.pop_indent();
            self.write_line("}");
        } else {
            self.write_line(&format!("fn {name}({params}){ret};"));
        }
    }

    fn render_trait_method_params(&self, params: &[Param]) -> String {
        params
            .iter()
            .enumerate()
            .map(|(index, param)| {
                if index == 0 && matches!(param.name.as_str(), "self" | "_self") {
                    if param.mutable {
                        "mut self".to_string()
                    } else {
                        "self".to_string()
                    }
                } else {
                    let ty = self.map_type(&param.ty);
                    if param.mutable {
                        format!("mut {}: {}", sanitize_emitted_identifier(&param.name), ty)
                    } else {
                        format!("{}: {}", sanitize_emitted_identifier(&param.name), ty)
                    }
                }
            })
            .collect::<Vec<_>>()
            .join(", ")
    }

    fn with_symbol_scope<T>(&mut self, f: impl FnOnce(&mut Self) -> T) -> T {
        let saved = std::mem::take(&mut self.emitted_symbols);
        let result = f(self);
        self.emitted_symbols = saved;
        result
    }

    fn render_use(&self, use_item: &Use) -> String {
        let mut path = self.normalize_runtime_path(&use_item.path.join("::"));
        if use_item.glob {
            path.push_str("::*");
        }
        match &use_item.alias {
            Some(alias) => format!("use {path} as {};", sanitize_emitted_identifier(alias)),
            None => format!("use {path};"),
        }
    }

    fn is_simple_const(&self, ty: &Type, value: &Expr) -> bool {
        matches!(
            value,
            Expr::Int(_, _) | Expr::Float(_, _) | Expr::Bool(_, _)
        ) && matches!(ty, Type::Named { name, .. } if {
            let mapped = self.map_named_type(name);
            matches!(
                mapped.as_str(),
                "i8" | "i16" | "i32" | "i64" | "u8" | "u16" | "u32" | "u64"
                    | "isize" | "usize" | "f32" | "f64" | "bool"
            )
        })
    }

    fn map_const_type(&self, ty: &Type) -> String {
        match ty {
            Type::Array(inner, _, _) | Type::Slice(inner, _) => {
                format!("Vec<{}>", self.map_const_type(inner))
            }
            Type::Ref { inner, .. } => self.map_const_type(inner),
            Type::Tuple(items, _) => {
                let parts: Vec<String> =
                    items.iter().map(|item| self.map_const_type(item)).collect();
                if parts.len() == 1 {
                    format!("({},)", parts[0])
                } else {
                    format!("({})", parts.join(", "))
                }
            }
            _ => self.map_storage_type(ty, None),
        }
    }

    fn gen_const_value_expr(&self, expr: &Expr) -> String {
        match expr {
            Expr::String(value, _) => format!("{value:?}"),
            Expr::Ref { value, .. } | Expr::AddrOf { value, .. } => {
                self.gen_const_value_expr(value)
            }
            Expr::Array(items, _) => {
                let parts: Vec<String> = items
                    .iter()
                    .map(|item| self.gen_const_value_expr(item))
                    .collect();
                format!("vec![{}]", parts.join(", "))
            }
            Expr::Tuple(items, _) => {
                let parts: Vec<String> = items
                    .iter()
                    .map(|item| self.gen_const_value_expr(item))
                    .collect();
                if parts.len() == 1 {
                    format!("({},)", parts[0])
                } else {
                    format!("({})", parts.join(", "))
                }
            }
            Expr::Struct {
                name, fields, rest, ..
            } => {
                let mut field_strs: Vec<String> = fields
                    .iter()
                    .map(|(field, value)| {
                        format!(
                            "{}: {}",
                            sanitize_emitted_identifier(field),
                            self.gen_const_value_expr(value)
                        )
                    })
                    .collect();
                if let Some(rest) = rest {
                    field_strs.push(format!("..{}", self.gen_const_value_expr(rest)));
                }
                if field_strs.is_empty() {
                    format!("{} {{}}", self.normalize_runtime_path(name))
                } else {
                    format!(
                        "{} {{ {} }}",
                        self.normalize_runtime_path(name),
                        field_strs.join(", ")
                    )
                }
            }
            Expr::AggregateInit {
                ty,
                fields,
                zero_fill_rest,
                ..
            } => {
                if let Type::Named { name, .. } = ty {
                    if let Some((enum_name, variant)) = name.rsplit_once("__") {
                        let head = self.normalize_variant_head(
                            Some(&enum_name.to_string()),
                            variant,
                            None,
                        );
                        let field_strs: Vec<String> = fields
                            .iter()
                            .map(|(field, value)| {
                                format!(
                                    "{}: {}",
                                    sanitize_emitted_identifier(field),
                                    self.gen_const_value_expr(value)
                                )
                            })
                            .collect();
                        return format!("{head} {{ {} }}", field_strs.join(", "));
                    }
                }
                let ty_name = self.map_type(ty);
                let field_strs: Vec<String> = fields
                    .iter()
                    .map(|(field, value)| {
                        format!(
                            "{}: {}",
                            sanitize_emitted_identifier(field),
                            self.gen_const_value_expr(value)
                        )
                    })
                    .collect();
                if *zero_fill_rest {
                    if field_strs.is_empty() {
                        format!("{ty_name} {{ ..Default::default() }}")
                    } else {
                        format!(
                            "{ty_name} {{ {}, ..Default::default() }}",
                            field_strs.join(", ")
                        )
                    }
                } else {
                    format!("{ty_name} {{ {} }}", field_strs.join(", "))
                }
            }
            Expr::EnumVariant {
                enum_name,
                variant,
                fields,
                ..
            } => match fields {
                EnumVariantFields::Unit => {
                    self.normalize_variant_head(Some(enum_name), variant, None)
                }
                EnumVariantFields::Tuple(values) => {
                    let parts: Vec<String> = values
                        .iter()
                        .map(|value| self.gen_const_value_expr(value))
                        .collect();
                    format!(
                        "{}({})",
                        self.normalize_variant_head(Some(enum_name), variant, None),
                        parts.join(", ")
                    )
                }
                EnumVariantFields::Struct(values) => {
                    let parts: Vec<String> = values
                        .iter()
                        .map(|(field, value)| {
                            format!("{field}: {}", self.gen_const_value_expr(value))
                        })
                        .collect();
                    format!(
                        "{} {{ {} }}",
                        self.normalize_variant_head(Some(enum_name), variant, None),
                        parts.join(", ")
                    )
                }
            },
            _ => self.gen_expr(expr),
        }
    }

    // Generate a function definition
    fn gen_function(&mut self, func: &Function) {
        if let Some(target) = func.name.strip_suffix("_fmt") {
            if !target.is_empty() {
                self.display_helpers.insert(target.to_string());
            }
        }
        let self_ctx = self.current_impl_target();
        let self_ctx_ref = self_ctx.as_deref();
        let free_self_alias = if self_ctx_ref.is_none()
            && func
                .params
                .first()
                .is_some_and(|param| matches!(param.name.as_str(), "self" | "_self"))
        {
            Some("value".to_string())
        } else {
            None
        };
        let mutated_params = self.collect_mutated_bindings_in_block(&func.body);
        let binding_types = self.collect_binding_types(func);
        self.current_function_stack.push(func.name.clone());
        let lifetime_names = self.function_lifetime_generic_names(func);
        self.push_generic_lifetimes(lifetime_names.clone());
        let vis = self.visibility_prefix(func.visibility);
        let modifiers = self.function_modifiers(&func.effects);
        let params = self.gen_params_with_context(
            &func.params,
            self_ctx_ref,
            Some(&mutated_params),
            free_self_alias.as_deref(),
        );
        let generic_params = self.collect_function_generic_params(func);
        let generic_suffix = self.render_generic_names(&generic_params, &lifetime_names);
        let ret = if let Some(trait_name) = self.current_impl_trait() {
            if (self.trait_name_matches(trait_name, "Display")
                || self.trait_name_matches(trait_name, "Debug"))
                && self.rust_method_name(&func.name) == "fmt"
            {
                " -> std::fmt::Result".to_string()
            } else if let Some(ty) = &func.return_type {
                format!(" -> {}", self.map_return_type(ty, self_ctx_ref))
            } else {
                String::new()
            }
        } else if let Some(ty) = &func.return_type {
            format!(" -> {}", self.map_return_type(ty, self_ctx_ref))
        } else {
            String::new()
        };
        let return_struct = func.return_type.as_ref().and_then(|ty| match ty {
            Type::Named { name, .. } => {
                let normalized = self.normalize_type_name(name, self_ctx_ref);
                self.struct_fields
                    .contains_key(&normalized)
                    .then_some(normalized)
            }
            _ => None,
        });
        let has_implicit_return = func
            .return_type
            .as_ref()
            .is_some_and(|ty| !matches!(ty, Type::Unit(_)));

        self.emit_rust_attributes(&func.attributes);
        self.write_line(&format!(
            "{}{}fn {}{}({}){} {{",
            vis,
            modifiers,
            self.rust_method_name(&func.name),
            generic_suffix,
            params,
            ret
        ));
        self.push_indent();
        if let Some(self_ctx) = &self_ctx {
            self.self_type_stack
                .push(strip_rendered_type_generics(self_ctx));
        }
        self.free_self_alias_stack.push(free_self_alias);
        self.current_return_struct_stack.push(return_struct);
        self.binding_type_stack.push(binding_types);
        self.gen_block_with_implicit_return(&func.body, has_implicit_return);
        self.binding_type_stack.pop();
        self.current_return_struct_stack.pop();
        self.free_self_alias_stack.pop();
        if self_ctx.is_some() {
            self.self_type_stack.pop();
        }
        self.pop_generic_lifetimes();
        self.current_function_stack.pop();
        self.pop_indent();
        self.write_line("}");
    }

    fn synthetic_impl_target(&self, func: &Function) -> Option<String> {
        if self.current_impl_target().is_some() {
            return None;
        }

        let target = self.inferred_self_type(&func.name)?;
        if self.known_type_names.contains(target) {
            Some(target.to_string())
        } else {
            None
        }
    }

    fn gen_synthetic_impl_function(&mut self, target: &str, func: &Function) {
        self.write_line(&format!("impl {} {{", self.normalize_runtime_path(target)));
        self.push_indent();
        self.impl_target_stack.push(target.to_string());
        self.impl_trait_stack.push(None);
        self.gen_function(func);
        self.impl_trait_stack.pop();
        self.impl_target_stack.pop();
        self.pop_indent();
        self.write_line("}");
    }

    fn gen_params(&self, params: &[Param]) -> String {
        self.gen_params_with_context(params, None, None, None)
    }

    fn gen_params_with_context(
        &self,
        params: &[Param],
        self_ctx: Option<&str>,
        mutated_params: Option<&HashSet<String>>,
        free_self_alias: Option<&str>,
    ) -> String {
        let free_self_target = if self_ctx.is_none() && free_self_alias.is_some() {
            self.current_function_name()
                .and_then(|name| self.inferred_self_type(name))
        } else {
            None
        };
        let parts: Vec<String> = params
            .iter()
            .enumerate()
            .map(|(index, p)| {
                let needs_mut = mutated_params.is_some_and(|names| names.contains(&p.name));
                if index == 0 && self_ctx.is_some() && matches!(p.name.as_str(), "self" | "_self") {
                    match &p.ty {
                        Type::Ref { mutable, inner, .. }
                            if self.extract_named_type_name(inner).as_deref().is_some_and(
                                |name| name == "Self_" || name == "Self" || Some(name) == self_ctx,
                            ) =>
                        {
                            return if *mutable || p.mutable || needs_mut {
                                "&mut self".to_string()
                            } else {
                                "&self".to_string()
                            };
                        }
                        Type::Named { name, .. }
                            if name == "Self_"
                                || name == "Self"
                                || Some(name.as_str()) == self_ctx =>
                        {
                            return if p.mutable || needs_mut {
                                "mut self".to_string()
                            } else {
                                "self".to_string()
                            };
                        }
                        _ => {}
                    }
                }
                let ty_str = self.map_type_in_context(&p.ty, self_ctx.or(free_self_target), false);
                if index == 0 && matches!(p.name.as_str(), "self" | "_self") {
                    if let Some(alias) = free_self_alias {
                        return format!("{}: {ty_str}", sanitize_emitted_identifier(alias));
                    }
                }
                if p.mutable || needs_mut {
                    format!("mut {}: {}", sanitize_emitted_identifier(&p.name), ty_str)
                } else {
                    format!("{}: {}", sanitize_emitted_identifier(&p.name), ty_str)
                }
            })
            .collect();
        parts.join(", ")
    }

    fn collect_function_generic_params(&self, func: &Function) -> Vec<String> {
        let mut names: Vec<String> = func
            .generics
            .iter()
            .map(|generic| generic.name.clone())
            .collect();
        if self.current_impl_target().is_none() && self.current_impl_trait().is_none() {
            let mut inferred = BTreeSet::new();
            for param in &func.params {
                self.collect_type_generic_params(&param.ty, &mut inferred);
            }
            if let Some(return_type) = &func.return_type {
                self.collect_type_generic_params(return_type, &mut inferred);
            }
            for inferred_name in inferred {
                if !names.iter().any(|name| name == &inferred_name) {
                    names.push(inferred_name);
                }
            }
        }
        names
    }

    fn collect_type_generic_params(&self, ty: &Type, out: &mut BTreeSet<String>) {
        match ty {
            Type::Named { name, generics, .. } => {
                let leaf = name.rsplit("::").next().unwrap_or(name).trim();
                for generic in generics {
                    self.collect_type_generic_params(generic, out);
                }
                if generics.is_empty()
                    && leaf.chars().all(|ch| ch.is_ascii_uppercase())
                    && !self.known_type_names.contains(leaf)
                {
                    out.insert(leaf.to_string());
                }
            }
            Type::Tuple(types, _) => {
                for item in types {
                    self.collect_type_generic_params(item, out);
                }
            }
            Type::Array(inner, _, _)
            | Type::Slice(inner, _)
            | Type::Ref { inner, .. }
            | Type::Ptr { inner, .. }
            | Type::Option(inner, _) => {
                self.collect_type_generic_params(inner, out);
            }
            Type::Result(ok, err, _) => {
                self.collect_type_generic_params(ok, out);
                self.collect_type_generic_params(err, out);
            }
            Type::Function {
                params,
                return_type,
                ..
            } => {
                for param in params {
                    self.collect_type_generic_params(param, out);
                }
                self.collect_type_generic_params(return_type, out);
            }
            Type::Impl { generics, .. } => {
                for generic in generics {
                    self.collect_type_generic_params(generic, out);
                }
            }
            Type::Infer(_) | Type::Never(_) | Type::Unit(_) => {}
        }
    }

    fn looks_like_lifetime_name(&self, name: &str) -> bool {
        let leaf = name.rsplit("::").next().unwrap_or(name);
        if leaf.is_empty() {
            return false;
        }
        if leaf == "static" || leaf == "_" {
            return true;
        }
        let mut chars = leaf.chars();
        matches!(chars.next(), Some(ch) if ch.is_ascii_lowercase() || ch == '_')
            && chars.all(|ch| ch.is_ascii_alphanumeric() || ch == '_')
    }

    fn collect_declared_lifetime_generic_names_from_type(
        &self,
        ty: &Type,
        declared_names: &HashSet<String>,
        out: &mut HashSet<String>,
    ) {
        match ty {
            Type::Named { name, generics, .. } => {
                let leaf = name.rsplit("::").next().unwrap_or(name);
                if generics.is_empty()
                    && (declared_names.contains(name) || declared_names.contains(leaf))
                    && self.looks_like_lifetime_name(leaf)
                {
                    out.insert(leaf.to_string());
                }
                for generic in generics {
                    self.collect_declared_lifetime_generic_names_from_type(
                        generic,
                        declared_names,
                        out,
                    );
                }
            }
            Type::Tuple(types, _) => {
                for item in types {
                    self.collect_declared_lifetime_generic_names_from_type(
                        item,
                        declared_names,
                        out,
                    );
                }
            }
            Type::Array(inner, _, _)
            | Type::Slice(inner, _)
            | Type::Ptr { inner, .. }
            | Type::Option(inner, _) => {
                self.collect_declared_lifetime_generic_names_from_type(inner, declared_names, out);
            }
            Type::Ref {
                inner, lifetime, ..
            } => {
                if let Some(name) = lifetime {
                    let leaf = name.rsplit("::").next().unwrap_or(name);
                    if (declared_names.contains(name) || declared_names.contains(leaf))
                        && self.looks_like_lifetime_name(leaf)
                    {
                        out.insert(leaf.to_string());
                    }
                }
                self.collect_declared_lifetime_generic_names_from_type(inner, declared_names, out);
            }
            Type::Result(ok, err, _) => {
                self.collect_declared_lifetime_generic_names_from_type(ok, declared_names, out);
                self.collect_declared_lifetime_generic_names_from_type(err, declared_names, out);
            }
            Type::Function {
                params,
                return_type,
                ..
            } => {
                for param in params {
                    self.collect_declared_lifetime_generic_names_from_type(
                        param,
                        declared_names,
                        out,
                    );
                }
                self.collect_declared_lifetime_generic_names_from_type(
                    return_type,
                    declared_names,
                    out,
                );
            }
            Type::Impl { generics, .. } => {
                for generic in generics {
                    self.collect_declared_lifetime_generic_names_from_type(
                        generic,
                        declared_names,
                        out,
                    );
                }
            }
            Type::Infer(_) | Type::Never(_) | Type::Unit(_) => {}
        }
    }

    fn function_lifetime_generic_names(&self, func: &Function) -> HashSet<String> {
        let declared_names: HashSet<String> = func
            .generics
            .iter()
            .map(|generic| generic.name.clone())
            .collect();
        let mut out = HashSet::new();
        for param in &func.params {
            self.collect_declared_lifetime_generic_names_from_type(
                &param.ty,
                &declared_names,
                &mut out,
            );
        }
        if let Some(return_type) = &func.return_type {
            self.collect_declared_lifetime_generic_names_from_type(
                return_type,
                &declared_names,
                &mut out,
            );
        }
        out
    }

    fn struct_lifetime_generic_names(&self, struct_def: &Struct) -> HashSet<String> {
        let declared_names: HashSet<String> = struct_def
            .generics
            .iter()
            .map(|generic| generic.name.clone())
            .collect();
        let mut out = HashSet::new();
        for field in &struct_def.fields {
            self.collect_declared_lifetime_generic_names_from_type(
                &field.ty,
                &declared_names,
                &mut out,
            );
        }
        out
    }

    fn enum_lifetime_generic_names(&self, enum_def: &Enum) -> HashSet<String> {
        let declared_names: HashSet<String> = enum_def
            .generics
            .iter()
            .map(|generic| generic.name.clone())
            .collect();
        let mut out = HashSet::new();
        for variant in &enum_def.variants {
            match &variant.fields {
                VariantFields::Unit => {}
                VariantFields::Tuple(types) => {
                    for ty in types {
                        self.collect_declared_lifetime_generic_names_from_type(
                            ty,
                            &declared_names,
                            &mut out,
                        );
                    }
                }
                VariantFields::Struct(fields) => {
                    for field in fields {
                        self.collect_declared_lifetime_generic_names_from_type(
                            &field.ty,
                            &declared_names,
                            &mut out,
                        );
                    }
                }
            }
        }
        out
    }

    fn trait_lifetime_generic_names(&self, trait_def: &Trait) -> HashSet<String> {
        let declared_names: HashSet<String> = trait_def
            .generics
            .iter()
            .map(|generic| generic.name.clone())
            .collect();
        let mut out = HashSet::new();
        for method in &trait_def.methods {
            for param in &method.params {
                self.collect_declared_lifetime_generic_names_from_type(
                    &param.ty,
                    &declared_names,
                    &mut out,
                );
            }
            if let Some(return_type) = &method.return_type {
                self.collect_declared_lifetime_generic_names_from_type(
                    return_type,
                    &declared_names,
                    &mut out,
                );
            }
        }
        out
    }

    fn impl_lifetime_generic_names(&self, impl_def: &Impl) -> HashSet<String> {
        let declared_names: HashSet<String> = impl_def
            .generics
            .iter()
            .map(|generic| generic.name.clone())
            .collect();
        let mut out = HashSet::new();
        self.collect_declared_lifetime_generic_names_from_type(
            &impl_def.target_type,
            &declared_names,
            &mut out,
        );
        for generic in &impl_def.trait_generics {
            self.collect_declared_lifetime_generic_names_from_type(
                generic,
                &declared_names,
                &mut out,
            );
        }
        for method in &impl_def.methods {
            for param in &method.params {
                self.collect_declared_lifetime_generic_names_from_type(
                    &param.ty,
                    &declared_names,
                    &mut out,
                );
            }
            if let Some(return_type) = &method.return_type {
                self.collect_declared_lifetime_generic_names_from_type(
                    return_type,
                    &declared_names,
                    &mut out,
                );
            }
        }
        out
    }

    fn type_alias_lifetime_generic_names(
        &self,
        generics: &[Generic],
        target: &Type,
    ) -> HashSet<String> {
        let declared_names: HashSet<String> = generics
            .iter()
            .map(|generic| generic.name.clone())
            .collect();
        let mut out = HashSet::new();
        self.collect_declared_lifetime_generic_names_from_type(target, &declared_names, &mut out);
        out
    }

    fn render_generic_name(&self, name: &str, lifetime_names: &HashSet<String>) -> String {
        let leaf = name.rsplit("::").next().unwrap_or(name);
        if (lifetime_names.contains(name) || lifetime_names.contains(leaf))
            && self.looks_like_lifetime_name(leaf)
        {
            format!("'{}", self.sanitize_lifetime_name(leaf))
        } else {
            sanitize_emitted_identifier(name)
        }
    }

    fn render_generic_names(&self, names: &[String], lifetime_names: &HashSet<String>) -> String {
        if names.is_empty() {
            String::new()
        } else {
            format!(
                "<{}>",
                names
                    .iter()
                    .map(|name| self.render_generic_name(name, lifetime_names))
                    .collect::<Vec<_>>()
                    .join(", ")
            )
        }
    }

    fn render_generic_params(
        &self,
        generics: &[Generic],
        lifetime_names: &HashSet<String>,
    ) -> String {
        if generics.is_empty() {
            String::new()
        } else {
            let names = generics
                .iter()
                .map(|generic| generic.name.clone())
                .collect::<Vec<_>>();
            self.render_generic_names(&names, lifetime_names)
        }
    }

    fn emit_rust_attributes(&mut self, attrs: &[kain_core::ast::Attribute]) {
        for attr in attrs {
            if let Some(rendered) = self.render_rust_attribute(attr) {
                self.write_line(&rendered);
            }
        }
    }

    fn has_rust_attribute(&self, attrs: &[kain_core::ast::Attribute], target: &str) -> bool {
        attrs
            .iter()
            .any(|attr| self.normalize_runtime_path(&attr.name) == target)
    }

    fn render_rust_attribute(&self, attr: &kain_core::ast::Attribute) -> Option<String> {
        let name = self.normalize_runtime_path(&attr.name);
        if name == "doc" {
            if attr.args.len() == 1 {
                if let Some(value) = self.render_rust_doc_attr_value(&attr.args[0]) {
                    return Some(format!("#[doc = {value}]"));
                }
            }
        }
        if attr.args.is_empty() {
            Some(format!("#[{name}]"))
        } else {
            Some(format!(
                "#[{name}({})]",
                attr.args
                    .iter()
                    .map(|arg| self.render_rust_attr_expr(arg))
                    .collect::<Vec<_>>()
                    .join(", ")
            ))
        }
    }

    fn render_rust_doc_attr_value(&self, expr: &Expr) -> Option<String> {
        match expr {
            Expr::Tuple(parts, _) if parts.len() == 2 => {
                if let Expr::Ident(name, _) = &parts[0] {
                    if self.normalize_runtime_path(name) == "doc" {
                        return Some(self.render_rust_attr_expr(&parts[1]));
                    }
                }
                None
            }
            Expr::Assign { target, value, .. } => {
                if matches!(target.as_ref(), Expr::Ident(name, _) if self.normalize_runtime_path(name) == "doc")
                {
                    Some(self.render_rust_attr_expr(value))
                } else {
                    None
                }
            }
            Expr::String(_, _) => Some(self.render_rust_attr_expr(expr)),
            _ => None,
        }
    }

    fn render_rust_attr_expr(&self, expr: &Expr) -> String {
        match expr {
            Expr::Ident(name, _) => self.normalize_runtime_path(name),
            Expr::String(value, _) => format!("{value:?}"),
            Expr::Int(value, _) => value.to_string(),
            Expr::Float(value, _) => value.to_string(),
            Expr::Bool(value, _) => value.to_string(),
            Expr::Tuple(parts, _) if parts.len() == 2 => {
                if let Expr::Ident(name, _) = &parts[0] {
                    return format!(
                        "{} = {}",
                        self.normalize_runtime_path(name),
                        self.render_rust_attr_expr(&parts[1])
                    );
                }
                self.gen_expr(expr)
            }
            Expr::Assign { target, value, .. } => format!(
                "{} = {}",
                self.render_rust_attr_expr(target),
                self.render_rust_attr_expr(value)
            ),
            Expr::Call { callee, args, .. } => {
                let callee = self.render_rust_attr_expr(callee);
                let args = args
                    .iter()
                    .map(|arg| match &arg.name {
                        Some(name) => format!(
                            "{} = {}",
                            self.normalize_runtime_path(name),
                            self.render_rust_attr_expr(&arg.value)
                        ),
                        None => self.render_rust_attr_expr(&arg.value),
                    })
                    .collect::<Vec<_>>()
                    .join(", ");
                format!("{callee}({args})")
            }
            Expr::Field { object, field, .. } => {
                format!(
                    "{}.{}",
                    self.render_rust_attr_expr(object),
                    self.render_field_accessor(field)
                )
            }
            _ => self.gen_expr(expr),
        }
    }

    fn gen_struct(&mut self, struct_def: &Struct) {
        let lifetime_names = self.struct_lifetime_generic_names(struct_def);
        self.push_generic_lifetimes(lifetime_names.clone());
        let vis = self.visibility_prefix(struct_def.visibility);
        let generics = self.render_generic_params(&struct_def.generics, &lifetime_names);

        self.emit_rust_attributes(&struct_def.attributes);
        if !self.has_rust_attribute(&struct_def.attributes, "derive") {
            self.write_line("#[derive(Debug, Clone, PartialEq)]");
        }
        self.write_line(&format!(
            "{}struct {}{} {{",
            vis,
            sanitize_emitted_identifier(&struct_def.name),
            generics
        ));
        self.push_indent();

        for field in &struct_def.fields {
            self.emit_rust_attributes(&field.attributes);
            self.write_line(&format!(
                "{}{}: {},",
                self.visibility_prefix(field.visibility),
                sanitize_emitted_identifier(&field.name),
                self.map_storage_type(&field.ty, Some(&struct_def.name))
            ));
        }

        self.pop_indent();
        self.write_line("}");
        self.pop_generic_lifetimes();
    }

    fn gen_impl(&mut self, impl_def: &Impl) {
        let lifetime_names = self.impl_lifetime_generic_names(impl_def);
        self.push_generic_lifetimes(lifetime_names.clone());
        let impl_generics = self.render_generic_params(&impl_def.generics, &lifetime_names);
        let target = self.map_type(&impl_def.target_type);
        let trait_name = impl_def.trait_name.as_ref().map(|name| {
            let name = self.map_trait_name(name);
            if impl_def.trait_generics.is_empty() {
                name
            } else {
                format!(
                    "{}<{}>",
                    name,
                    impl_def
                        .trait_generics
                        .iter()
                        .map(|ty| {
                            let current_self = self.current_impl_target();
                            self.map_generic_arg_in_context(ty, current_self.as_deref(), false)
                        })
                        .collect::<Vec<_>>()
                        .join(", ")
                )
            }
        });

        if let Some(trait_name) = &trait_name {
            self.write_line(&format!(
                "impl{} {} for {} {{",
                impl_generics, trait_name, target
            ));
        } else {
            self.write_line(&format!("impl{} {} {{", impl_generics, target));
        }

        self.push_indent();
        self.impl_target_stack.push(target.clone());
        self.impl_trait_stack.push(impl_def.trait_name.clone());
        for method in &impl_def.methods {
            self.gen_function(method);
            self.write_blank();
        }
        self.impl_trait_stack.pop();
        self.impl_target_stack.pop();
        self.pop_indent();
        self.write_line("}");
        self.pop_generic_lifetimes();
    }

    fn gen_enum(&mut self, enum_def: &Enum) {
        let lifetime_names = self.enum_lifetime_generic_names(enum_def);
        self.push_generic_lifetimes(lifetime_names.clone());
        let generics = self.render_generic_params(&enum_def.generics, &lifetime_names);
        self.write_line("#[derive(Debug, Clone, PartialEq)]");
        self.write_line(&format!(
            "{}enum {}{} {{",
            self.visibility_prefix(enum_def.visibility),
            sanitize_emitted_identifier(&enum_def.name),
            generics
        ));
        self.push_indent();

        for variant in &enum_def.variants {
            match &variant.fields {
                VariantFields::Unit => {
                    self.write_line(&format!("{},", sanitize_emitted_identifier(&variant.name)));
                }
                VariantFields::Tuple(types) => {
                    let fields: Vec<String> = types
                        .iter()
                        .map(|t| self.map_storage_type(t, Some(&enum_def.name)))
                        .collect();
                    self.write_line(&format!(
                        "{}({}),",
                        sanitize_emitted_identifier(&variant.name),
                        fields.join(", ")
                    ));
                }
                VariantFields::Struct(fields) => {
                    self.write_line(&format!(
                        "{} {{",
                        sanitize_emitted_identifier(&variant.name)
                    ));
                    self.push_indent();
                    for f in fields {
                        self.write_line(&format!(
                            "{}: {},",
                            sanitize_emitted_identifier(&f.name),
                            self.map_storage_type(&f.ty, Some(&enum_def.name))
                        ));
                    }
                    self.pop_indent();
                    self.write_line("},");
                }
            }
        }

        self.pop_indent();
        self.write_line("}");
        self.pop_generic_lifetimes();
    }

    fn component_props_name(&self, name: &str) -> String {
        format!("{}Props", name)
    }

    fn gen_component_props_struct(&mut self, comp: &Component) {
        let props_name = self.component_props_name(&comp.name);
        let vis = self.visibility_prefix(comp.visibility);

        self.write_line("#[derive(Debug, Clone, PartialEq)]");
        self.write_line(&format!("{}struct {} {{", vis, props_name));
        self.push_indent();
        for prop in &comp.props {
            self.write_line(&format!("pub {}: {},", prop.name, self.map_type(&prop.ty)));
        }
        self.write_line("pub children: String,");
        self.pop_indent();
        self.write_line("}");
        self.write_blank();
    }

    fn gen_component(&mut self, comp: &Component) {
        let vis = self.visibility_prefix(comp.visibility);
        let modifiers = self.function_modifiers(&comp.effects);
        let props_name = self.component_props_name(&comp.name);
        let mut prop_bindings: Vec<String> =
            comp.props.iter().map(|prop| prop.name.clone()).collect();
        prop_bindings.push("children".to_string());

        self.gen_component_props_struct(comp);
        self.write_line(&format!(
            "{}{}fn {}(props: {}) -> String {{",
            vis,
            modifiers,
            self.rust_function_name(&comp.name),
            props_name
        ));
        self.push_indent();
        self.write_line(&format!(
            "let {} {{ {} }} = props;",
            props_name,
            prop_bindings.join(", ")
        ));

        if !comp.state.is_empty() || !comp.methods.is_empty() {
            self.write_blank();
        }

        for state in &comp.state {
            self.write_line(&format!(
                "let mut {}: {} = {};",
                state.name,
                self.map_type(&state.ty),
                self.gen_expr(&state.initial)
            ));
        }

        if !comp.state.is_empty() && !comp.methods.is_empty() {
            self.write_blank();
        }

        for (index, method) in comp.methods.iter().enumerate() {
            self.gen_component_method_binding(method);
            if index + 1 != comp.methods.len() {
                self.write_blank();
            }
        }

        if !comp.methods.is_empty() {
            self.write_blank();
        }

        self.write_line(&self.gen_jsx(&comp.body));
        self.pop_indent();
        self.write_line("}");
    }

    fn gen_component_method_binding(&mut self, method: &Function) {
        let params = self.gen_params(&method.params);
        let ret = if let Some(ty) = &method.return_type {
            format!(" -> {}", self.map_return_type(ty, None))
        } else {
            String::new()
        };
        let has_implicit_return = method
            .return_type
            .as_ref()
            .is_some_and(|ty| !matches!(ty, Type::Unit(_)));

        self.write_line(&format!("let mut {} = |{}|{} {{", method.name, params, ret));
        self.push_indent();
        self.gen_block_with_implicit_return(&method.body, has_implicit_return);
        self.pop_indent();
        self.write_line("};");
    }

    fn gen_block(&mut self, block: &Block) {
        self.gen_block_with_implicit_return(block, false);
    }

    fn gen_block_with_implicit_return(&mut self, block: &Block, implicit_return: bool) {
        let len = block.stmts.len();
        for (i, stmt) in block.stmts.iter().enumerate() {
            let is_last = i + 1 == len;
            if implicit_return && is_last {
                if let Stmt::Expr(expr) = stmt {
                    if !matches!(
                        expr,
                        Expr::Return(_, _) | Expr::Break(_, _) | Expr::Continue(_)
                    ) {
                        self.write_line(&self.gen_expr(expr));
                        continue;
                    }
                }
            }
            self.gen_stmt(stmt);
        }
    }

    fn gen_stmt(&mut self, stmt: &Stmt) {
        match stmt {
            Stmt::Let {
                pattern, ty, value, ..
            } => {
                let pat_str = self.gen_pattern(pattern);
                let ty_str = ty
                    .as_ref()
                    .map(|t| format!(": {}", self.map_type(t)))
                    .unwrap_or_default();
                if let Some(val) = value {
                    self.write_line(&format!(
                        "let {}{} = {};",
                        pat_str,
                        ty_str,
                        self.gen_expr(val)
                    ));
                } else {
                    self.write_line(&format!("let {}{};", pat_str, ty_str));
                }
            }
            Stmt::Return(maybe_expr, _) => {
                if let Some(expr) = maybe_expr {
                    self.write_line(&format!("return {};", self.gen_expr(expr)));
                } else {
                    self.write_line("return;");
                }
            }
            Stmt::Break(maybe_expr, _) => {
                if let Some(expr) = maybe_expr {
                    self.write_line(&format!("break {};", self.gen_expr(expr)));
                } else {
                    self.write_line("break;");
                }
            }
            Stmt::Continue(_) => {
                self.write_line("continue;");
            }
            Stmt::For {
                binding,
                iter,
                body,
                ..
            } => {
                let pat = self.gen_pattern(binding);
                self.write_line(&format!("for {} in {} {{", pat, self.gen_expr(iter)));
                self.push_indent();
                self.gen_block(body);
                self.pop_indent();
                self.write_line("}");
            }
            Stmt::While {
                condition, body, ..
            } => {
                self.write_line(&format!("while {} {{", self.gen_expr(condition)));
                self.push_indent();
                self.gen_block(body);
                self.pop_indent();
                self.write_line("}");
            }
            Stmt::Loop { body, .. } => {
                self.write_line("loop {");
                self.push_indent();
                self.gen_block(body);
                self.pop_indent();
                self.write_line("}");
            }
            Stmt::Expr(expr) => {
                if let Expr::Assign { target, value, .. } = expr {
                    self.write_line(&format!(
                        "{} = {};",
                        self.gen_expr(target),
                        self.gen_expr(value)
                    ));
                } else if let Expr::Match {
                    scrutinee, arms, ..
                } = expr
                {
                    self.write_line(&self.gen_match_expr(scrutinee, arms, true));
                } else {
                    self.write_line(&format!("{};", self.gen_expr(expr)));
                }
            }
            Stmt::Item(item) => self.gen_nested_item(item),
        }
    }

    fn gen_block_expr(&self, block: &Block) -> String {
        if self.is_selfhost_empty_block(block) {
            return "{ () }".to_string();
        }
        let len = block.stmts.len();
        let mut stmt_strs = Vec::new();
        for (index, stmt) in block.stmts.iter().enumerate() {
            stmt_strs.push(self.gen_stmt_inline(stmt, index + 1 == len));
        }
        if stmt_strs.is_empty() {
            "{ () }".to_string()
        } else {
            format!("{{ {} }}", stmt_strs.join(" "))
        }
    }

    fn gen_stmt_inline(&self, stmt: &Stmt, is_last: bool) -> String {
        match stmt {
            Stmt::Let {
                pattern, ty, value, ..
            } => {
                let pat_str = self.gen_pattern(pattern);
                let ty_str = ty
                    .as_ref()
                    .map(|t| format!(": {}", self.map_type(t)))
                    .unwrap_or_default();
                if let Some(val) = value {
                    format!("let {}{} = {};", pat_str, ty_str, self.gen_expr(val))
                } else {
                    format!("let {}{};", pat_str, ty_str)
                }
            }
            Stmt::Expr(expr) => {
                let expr_str = if let Expr::Match {
                    scrutinee, arms, ..
                } = expr
                {
                    self.gen_match_expr(scrutinee, arms, true)
                } else {
                    self.gen_expr(expr)
                };
                if is_last {
                    expr_str
                } else {
                    format!("{};", expr_str)
                }
            }
            Stmt::Return(maybe_expr, _) => {
                if let Some(expr) = maybe_expr {
                    format!("return {};", self.gen_expr(expr))
                } else {
                    "return;".to_string()
                }
            }
            Stmt::Break(maybe_expr, _) => {
                if let Some(expr) = maybe_expr {
                    format!("break {};", self.gen_expr(expr))
                } else {
                    "break;".to_string()
                }
            }
            Stmt::Continue(_) => "continue;".to_string(),
            Stmt::For {
                binding,
                iter,
                body,
                ..
            } => {
                format!(
                    "for {} in {} {}",
                    self.gen_pattern(binding),
                    self.gen_expr(iter),
                    self.gen_block_expr(body)
                )
            }
            Stmt::While {
                condition, body, ..
            } => {
                format!(
                    "while {} {}",
                    self.gen_expr(condition),
                    self.gen_block_expr(body)
                )
            }
            Stmt::Loop { body, .. } => format!("loop {}", self.gen_block_expr(body)),
            Stmt::Item(_) => "()".to_string(),
        }
    }

    fn gen_else_branch_expr(&self, else_branch: &ElseBranch) -> String {
        match else_branch {
            ElseBranch::Else(block) => self.gen_block_expr(block),
            ElseBranch::ElseIf(condition, block, tail) => {
                let else_tail = tail
                    .as_ref()
                    .map(|next| format!(" else {}", self.gen_else_branch_expr(next)))
                    .unwrap_or_default();
                format!(
                    "if {} {}{}",
                    self.gen_expr(condition),
                    self.gen_block_expr(block),
                    else_tail
                )
            }
        }
    }

    fn string_literal_expr(expr: &Expr) -> Option<String> {
        match expr {
            Expr::String(value, _) => Some(format!("{value:?}")),
            _ => None,
        }
    }

    fn render_format_head_macro(&self, name: &str, args: &[&Expr]) -> String {
        let Some((first, rest)) = args.split_first() else {
            return format!("{name}!()");
        };

        if rest.is_empty() {
            return format!("{name}!(\"{{}}\", {})", self.gen_expr(first));
        }

        if let Some(literal) = Self::string_literal_expr(first) {
            let rendered_rest: Vec<String> = rest.iter().map(|arg| self.gen_expr(arg)).collect();
            format!("{name}!({literal}, {})", rendered_rest.join(", "))
        } else {
            let rendered_args: Vec<String> = args.iter().map(|arg| self.gen_expr(arg)).collect();
            let placeholders = vec!["{}"; rendered_args.len()].join(" ");
            format!("{name}!(\"{placeholders}\", {})", rendered_args.join(", "))
        }
    }

    fn render_builtin_macro_exprs(&self, name: &str, args: &[&Expr]) -> Option<String> {
        let rendered = match name {
            "format" => self.render_format_head_macro("format", args),
            "panic" | "unreachable" | "todo" => self.render_format_head_macro(name, args),
            "__kain_write_fmt" => {
                let arg_strs: Vec<String> = args.iter().map(|arg| self.gen_expr(arg)).collect();
                if arg_strs.len() == 2 {
                    format!("write!({}, \"{{}}\", {})", arg_strs[0], arg_strs[1])
                } else {
                    format!("write!({})", arg_strs.join(", "))
                }
            }
            "__kain_writeln_fmt" => {
                let arg_strs: Vec<String> = args.iter().map(|arg| self.gen_expr(arg)).collect();
                if arg_strs.len() == 2 {
                    format!("writeln!({}, \"{{}}\", {})", arg_strs[0], arg_strs[1])
                } else {
                    format!("writeln!({})", arg_strs.join(", "))
                }
            }
            "println" | "print" | "eprintln" | "eprint" => {
                self.render_format_head_macro(name, args)
            }
            _ => return None,
        };
        Some(rendered)
    }

    fn render_bootstrap_intrinsic_call(&self, name: &str, args: &[CallArg]) -> Option<String> {
        match name {
            "__kain_bootstrap_lex_tokens" if args.len() == 1 => {
                let source = self.gen_expr(&args[0].value);
                Some(format!(
                    "{{ let __kain_source_ref = {source}; let __kain_source = __kain_source_ref.as_str(); let mut __kain_lex = crate::lexer::TokenKind::lexer(__kain_source); let mut __kain_raw_tokens = Vec::new(); while let Some(__kain_result) = __kain_lex.next() {{ let __kain_span = crate::span::Span::new(__kain_lex.span().start, __kain_lex.span().end); match __kain_result {{ Ok(__kain_kind) => {{ if matches!(__kain_kind, crate::lexer::TokenKind::Comment | crate::lexer::TokenKind::HashComment) {{ continue; }} __kain_raw_tokens.push(crate::lexer::Token::new(__kain_kind, __kain_span)); }} Err(_) => {{ return Err(crate::error::KainError::lexer(format!(\"Unexpected character: '{{}}'\", &__kain_source[__kain_span.start..__kain_span.end]), __kain_span)); }} }} }} let mut __kain_result_tokens = Vec::new(); let mut __kain_indent_stack: Vec<usize> = vec![0]; let mut __kain_iter = __kain_raw_tokens.into_iter().peekable(); while let Some(__kain_token) = __kain_iter.next() {{ match &__kain_token.kind {{ crate::lexer::TokenKind::Newline(__kain_ws) => {{ if let Some(__kain_next) = __kain_iter.peek() {{ if matches!(__kain_next.kind, crate::lexer::TokenKind::Newline(_)) {{ continue; }} }} let __kain_indent: usize = __kain_ws[1..].chars().map(|__kain_ch| if __kain_ch == '\\t' {{ 4 }} else {{ 1 }}).sum(); let __kain_current = *__kain_indent_stack.last().unwrap(); if __kain_indent > __kain_current {{ __kain_indent_stack.push(__kain_indent); __kain_result_tokens.push(crate::lexer::Token::new(crate::lexer::TokenKind::Newline(__kain_ws.clone()), __kain_token.span)); __kain_result_tokens.push(crate::lexer::Token::new(crate::lexer::TokenKind::Indent, __kain_token.span)); }} else if __kain_indent < __kain_current {{ __kain_result_tokens.push(crate::lexer::Token::new(crate::lexer::TokenKind::Newline(__kain_ws.clone()), __kain_token.span)); while __kain_indent_stack.len() > 1 && *__kain_indent_stack.last().unwrap() > __kain_indent {{ __kain_indent_stack.pop(); __kain_result_tokens.push(crate::lexer::Token::new(crate::lexer::TokenKind::Dedent, __kain_token.span)); }} }} else {{ __kain_result_tokens.push(crate::lexer::Token::new(crate::lexer::TokenKind::Newline(__kain_ws.clone()), __kain_token.span)); }} }} _ => __kain_result_tokens.push(__kain_token), }} }} let __kain_final_span = __kain_result_tokens.last().map(|__kain_token| __kain_token.span).unwrap_or(crate::span::Span::new(0, 0)); while __kain_indent_stack.len() > 1 {{ __kain_indent_stack.pop(); __kain_result_tokens.push(crate::lexer::Token::new(crate::lexer::TokenKind::Dedent, __kain_final_span)); }} __kain_result_tokens.push(crate::lexer::Token::new(crate::lexer::TokenKind::Eof, __kain_final_span)); Ok(__kain_result_tokens) }}"
                ))
            }
            "__kain_bootstrap_parse_source" if args.len() == 2 => {
                let tokens = self.gen_expr(&args[0].value);
                let file_name = self.gen_expr(&args[1].value);
                Some(format!(
                    "{{ let __kain_tokens = {tokens}; let __kain_file_name = {file_name}; let __kain_span_mapper = crate::diagnostics::SpanMapper::new(\"\"); let mut __kain_parser = crate::parser::Parser::new(&__kain_tokens, &__kain_span_mapper, &__kain_file_name); __kain_parser.parse().unwrap() }}"
                ))
            }
            "__kain_bootstrap_run_program" if args.len() == 1 => {
                let _program = self.gen_expr(&args[0].value);
                Some("crate::runtime::Value::Null".to_string())
            }
            "__kain_bootstrap_generate_llvm_ir" if args.len() == 1 => {
                let _program = self.gen_expr(&args[0].value);
                Some("String::new()".to_string())
            }
            _ => None,
        }
    }

    fn gen_expr(&self, expr: &Expr) -> String {
        match expr {
            Expr::Int(n, _) => n.to_string(),
            Expr::Float(f, _) => format!("{:.1}", f),
            Expr::String(s, _) => format!("\"{}\".to_string()", self.escape_string(s)),
            Expr::FString(parts, _) => self.render_interpolated_string(parts),
            Expr::Bool(b, _) => b.to_string(),
            Expr::None(_) => "None".to_string(),
            Expr::Ident(name, _) => self.normalize_runtime_path(name),
            Expr::MacroCall { name, args, .. } => {
                let arg_refs: Vec<&Expr> = args.iter().collect();
                if let Some(rendered) = self.render_builtin_macro_exprs(name, &arg_refs) {
                    rendered
                } else {
                    let arg_strs: Vec<String> = args.iter().map(|arg| self.gen_expr(arg)).collect();
                    format!("{}!({})", name, arg_strs.join(", "))
                }
            }
            Expr::Binary { left, op, right, .. } => {
                let l = self.gen_expr(left);
                let r = self.gen_expr(right);
                if matches!(op, BinaryOp::Pow) {
                    format!("__kain_pow(({}) as f64, ({}) as f64)", l, r)
                } else {
                    format!("({} {} {})", l, self.map_binop(op), r)
                }
            }
            Expr::Unary { op, operand, .. } => format!("({}{})", self.map_unaryop(op), self.gen_expr(operand)),
            Expr::Call { callee, args, .. } => {
                let fn_name = self.gen_expr(callee);
                if fn_name == "Some" && args.len() == 1 && matches!(args[0].value, Expr::None(_)) {
                    return "None".to_string();
                }
                let call_arg_refs: Vec<&Expr> = args.iter().map(|a| &a.value).collect();
                if let Some(rendered) = self.render_builtin_macro_exprs(&fn_name, &call_arg_refs) {
                    return rendered;
                }
                if let Some(rendered) = self.render_bootstrap_intrinsic_call(&fn_name, args) {
                    return rendered;
                }
                if fn_name == "range" {
                    let arg_strs: Vec<String> = args.iter().map(|a| self.gen_expr(&a.value)).collect();
                    return match arg_strs.as_slice() {
                        [start, end] => format!("{start}..{end}"),
                        [start, end, inclusive] if inclusive == "true" => format!("{start}..={end}"),
                        [start, end, _] => format!("{start}..{end}"),
                        _ => format!("range({})", arg_strs.join(", ")),
                    };
                }
                if fn_name == "slice" {
                    let arg_strs: Vec<String> = args.iter().map(|a| self.gen_expr(&a.value)).collect();
                    return match arg_strs.as_slice() {
                        [object, start, end] => {
                            let start = if start == "None" || start == "none" { "" } else { start };
                            let end = if end == "None" || end == "none" { "" } else { end };
                            format!("{object}[{start}..{end}]")
                        }
                        _ => format!("slice({})", arg_strs.join(", ")),
                    };
                }
                let arg_strs: Vec<String> = args.iter().map(|a| self.gen_expr(&a.value)).collect();
                format!("{}({})", fn_name, arg_strs.join(", "))
            }
            Expr::StageCall { function, args, .. } => {
                let arg_strs: Vec<String> = args.iter().map(|a| self.gen_expr(&a.value)).collect();
                format!(
                    "{}({})",
                    self.normalize_runtime_path(function),
                    arg_strs.join(", ")
                )
            }
            Expr::MethodCall { receiver, method, args, .. } => {
                let recv = self.gen_expr(receiver);
                let normalized_method = self.normalize_runtime_method(method);
                if args.len() == 1 && matches!(args[0].value, Expr::None(_)) {
                    return match normalized_method.as_str() {
                        "map" => format!("{}.map(|value| value)", recv),
                        "filter" | "find" | "all" => format!("{}.{}(|_| true)", recv, normalized_method),
                        "find_map" | "and_then" => format!("{}.{}(|_| None)", recv, normalized_method),
                        "or_else" => format!("{}.or_else(|| None)", recv),
                        "unwrap_or_else" => format!("{}.unwrap_or_default()", recv),
                        _ => format!("{}.{}(None)", recv, normalized_method),
                    };
                }
                if normalized_method == "strip_prefix" && args.len() == 1 {
                    if let Expr::String(value, _) = &args[0].value {
                        return format!("{}.strip_prefix(\"{}\")", recv, self.escape_string(value));
                    }
                }
                let arg_strs: Vec<String> = args.iter().map(|a| self.gen_expr(&a.value)).collect();
                format!("{}.{}({})", recv, normalized_method, arg_strs.join(", "))
            }
            Expr::Field { object, field, .. } => format!(
                "{}.{}",
                self.gen_expr(object),
                self.render_field_accessor(field)
            ),
            Expr::Index { object, index, .. } => format!("{}[{}]", self.gen_expr(object), self.gen_expr(index)),
            Expr::Assign { target, value, .. } => format!("({} = {})", self.gen_expr(target), self.gen_expr(value)),
            Expr::Struct {
                name, fields, rest, ..
            } => {
                let mut field_strs: Vec<String> = fields
                    .iter()
                    .map(|(name, value)| {
                        format!(
                            "{}: {}",
                            sanitize_emitted_identifier(name),
                            self.gen_expr(value)
                        )
                    })
                    .collect();
                if let Some(rest) = rest {
                    field_strs.push(format!("..{}", self.gen_expr(rest)));
                }
                if field_strs.is_empty() {
                    format!("{} {{}}", self.normalize_runtime_path(name))
                } else {
                    format!(
                        "{} {{ {} }}",
                        self.normalize_runtime_path(name),
                        field_strs.join(", ")
                    )
                }
            }
            Expr::AggregateInit { ty, fields, zero_fill_rest, .. } => {
                if let Type::Named { name, .. } = ty {
                    if let Some((enum_name, variant)) = name.rsplit_once("__") {
                        let head = self.normalize_variant_head(
                            Some(&enum_name.to_string()),
                            variant,
                            None,
                        );
                        let field_strs: Vec<String> = fields
                            .iter()
                            .map(|(name, value)| {
                                format!(
                                    "{}: {}",
                                    sanitize_emitted_identifier(name),
                                    self.gen_expr(value)
                                )
                            })
                            .collect();
                        return format!("{} {{ {} }}", head, field_strs.join(", "));
                    }
                }
                let ty_name = self.map_type(ty);
                let field_strs: Vec<String> = fields
                    .iter()
                    .map(|(name, value)| {
                        format!(
                            "{}: {}",
                            sanitize_emitted_identifier(name),
                            self.gen_expr(value)
                        )
                    })
                    .collect();
                if *zero_fill_rest {
                    if field_strs.is_empty() {
                        format!("{} {{ ..Default::default() }}", ty_name)
                    } else {
                        format!("{} {{ {}, ..Default::default() }}", ty_name, field_strs.join(", "))
                    }
                } else {
                    format!("{} {{ {} }}", ty_name, field_strs.join(", "))
                }
            }
            Expr::EnumVariant { enum_name, variant, fields, .. } => {
                match fields {
                    EnumVariantFields::Unit => self.normalize_variant_head(Some(enum_name), variant, None),
                    EnumVariantFields::Tuple(values) => {
                        let parts: Vec<String> = values.iter().map(|value| self.gen_expr(value)).collect();
                        format!("{}({})", self.normalize_variant_head(Some(enum_name), variant, None), parts.join(", "))
                    }
                    EnumVariantFields::Struct(values) => {
                        let parts: Vec<String> = values
                            .iter()
                            .map(|(name, value)| {
                                format!(
                                    "{}: {}",
                                    sanitize_emitted_identifier(name),
                                    self.gen_expr(value)
                                )
                            })
                            .collect();
                        format!("{} {{ {} }}", self.normalize_variant_head(Some(enum_name), variant, None), parts.join(", "))
                    }
                }
            }
            Expr::Array(items, _) => {
                let parts: Vec<String> = items.iter().map(|item| self.gen_expr(item)).collect();
                format!("vec![{}]", parts.join(", "))
            }
            Expr::Tuple(items, _) => {
                let parts: Vec<String> = items.iter().map(|item| self.gen_expr(item)).collect();
                if let Some(struct_name) = self.current_return_struct() {
                    if let Some(field_names) = self.struct_fields.get(struct_name) {
                        if field_names.len() == parts.len() {
                            let fields = field_names
                                .iter()
                                .zip(parts.iter())
                                .map(|(field, value)| format!("{field}: {value}"))
                                .collect::<Vec<_>>()
                                .join(", ");
                            return format!("{struct_name} {{ {fields} }}");
                        }
                    }
                }
                if parts.len() == 1 {
                    format!("({},)", parts[0])
                } else {
                    format!("({})", parts.join(", "))
                }
            }
            Expr::Range { start, end, inclusive, .. } => {
                let start = start.as_ref().map(|e| self.gen_expr(e)).unwrap_or_default();
                let end = end.as_ref().map(|e| self.gen_expr(e)).unwrap_or_default();
                if *inclusive { format!("{}..={}", start, end) } else { format!("{}..{}", start, end) }
            }
            Expr::If { condition, then_branch, else_branch, .. } => {
                let mut then_expr = self.gen_block_expr(then_branch);
                if let Some(else_branch) = else_branch {
                    let mut else_expr = self.gen_else_branch_expr(else_branch);
                    self.coerce_placeholder_branches(&mut then_expr, &mut else_expr);
                    format!("if {} {} else {}", self.gen_expr(condition), then_expr, else_expr)
                } else {
                    format!("if {} {}", self.gen_expr(condition), then_expr)
                }
            }
            Expr::Match { scrutinee, arms, .. } => {
                self.gen_match_expr(scrutinee, arms, false)
            }
            Expr::Lambda { params, return_type, body, .. } => {
                let params = self.gen_params(params);
                let ret = return_type.as_ref().map(|ty| format!(" -> {}", self.map_type(ty))).unwrap_or_default();
                format!("|{}|{} {}", params, ret, self.gen_expr(body))
            }
            Expr::Ref { mutable, value, .. } => {
                if *mutable { format!("&mut {}", self.gen_expr(value)) } else { format!("&{}", self.gen_expr(value)) }
            }
            Expr::AddrOf { value, pointee_ty, .. } => {
                if let Some(ty) = pointee_ty {
                    format!("((&{}) as *const {})", self.gen_expr(value), self.map_type(ty))
                } else {
                    format!("std::ptr::addr_of!({})", self.gen_expr(value))
                }
            }
            Expr::Deref(value, _) => format!("(*{})", self.gen_expr(value)),
            Expr::PtrOffset { pointer, offset, element_ty, .. } => {
                let ty = element_ty.as_ref().map(|ty| self.map_type(ty)).unwrap_or_else(|| "u8".to_string());
                format!("(({}) as *mut {}).wrapping_offset(({}) as isize)", self.gen_expr(pointer), ty, self.gen_expr(offset))
            }
            Expr::MemLoad { pointer, load_ty, .. } => {
                let ty = load_ty.as_ref().map(|ty| self.map_type(ty)).unwrap_or_else(|| "u8".to_string());
                format!("unsafe {{ std::ptr::read(({}) as *const {}) }}", self.gen_expr(pointer), ty)
            }
            Expr::MemStore { pointer, value, store_ty, .. } => {
                let ty = store_ty.as_ref().map(|ty| self.map_type(ty)).unwrap_or_else(|| "u8".to_string());
                format!("unsafe {{ std::ptr::write(({}) as *mut {}, {}) }}", self.gen_expr(pointer), ty, self.gen_expr(value))
            }
            Expr::SizeOfType { target, .. } => format!("size_of::<{}>() as i64", self.map_type(target)),
            Expr::AlignOfType { target, .. } => format!("std::mem::align_of::<{}>() as i64", self.map_type(target)),
            Expr::Alloca { ty, .. } => format!("Box::into_raw(Box::new(unsafe {{ std::mem::MaybeUninit::<{}>::zeroed().assume_init() }}))", self.map_type(ty)),
            Expr::Uninit { ty, .. } => format!("unsafe {{ std::mem::MaybeUninit::<{}>::uninit().assume_init() }}", self.map_type(ty)),
            Expr::Alloc { size, ty, zeroed, .. } => {
                let pointee = ty.as_ref().map(|ty| self.map_type(ty)).unwrap_or_else(|| "u8".to_string());
                format!("__kain_alloc_bytes(({}) as usize, {}) as *mut {}", self.gen_expr(size), zeroed, pointee)
            }
            Expr::Realloc { pointer, size, ty, zeroed_new, .. } => {
                let pointee = ty.as_ref().map(|ty| self.map_type(ty)).unwrap_or_else(|| "u8".to_string());
                format!("__kain_realloc_bytes(({}) as *mut u8, ({}) as usize, {}) as *mut {}", self.gen_expr(pointer), self.gen_expr(size), zeroed_new, pointee)
            }
            Expr::Observe { target, body, .. } | Expr::Collapse { target, body, .. } => {
                format!("{{ let _kain_ownership_target = {}; {} }}", self.gen_expr(target), self.gen_expr(body))
            }
            Expr::Decay { target, .. } => {
                format!("{{ let _kain_ownership_target = {}; () }}", self.gen_expr(target))
            }
            Expr::Teleport { value, .. } => self.gen_expr(value),
            Expr::Cast { value, target, .. } => format!("(({}) as {})", self.gen_expr(value), self.map_type(target)),
            Expr::Try(value, _) => format!("({}?)", self.gen_expr(value)),
            Expr::Await(value, _) => format!("({}.await)", self.gen_expr(value)),
            Expr::AsyncBlock(value, _) => match value.as_ref() {
                Expr::Block(block, _) => format!("async {}", self.gen_block_expr(block)),
                _ => format!("async {{ {} }}", self.gen_expr(value)),
            },
            Expr::Spawn { actor, init, .. } => {
                let init_fields: Vec<String> = init.iter().map(|(name, value)| format!("{}: {}", name, self.gen_expr(value))).collect();
                format!("{} {{ {} }}", actor, init_fields.join(", "))
            }
            Expr::SendMsg { target, message, data, .. } => {
                let data_expr = if data.is_empty() {
                    "()".to_string()
                } else {
                    let fields: Vec<String> = data.iter().map(|(name, value)| format!("{}: {}", name, self.gen_expr(value))).collect();
                    format!("{{ {} }}", fields.join(", "))
                };
                format!("{{ let _target = {}; let _message = \"{}\"; let _data = {}; () }}", self.gen_expr(target), message, data_expr)
            }
            Expr::Comptime(expr, _) => self.gen_expr(expr),
            Expr::Block(block, _) => self.gen_block_expr(block),
            Expr::JSX(node, _) => self.gen_jsx(node),
            Expr::Paren(expr, _) => format!("({})", self.gen_expr(expr)),
            Expr::Return(expr, _) => expr.as_ref().map(|expr| format!("return {}", self.gen_expr(expr))).unwrap_or_else(|| "return".to_string()),
            Expr::Break(expr, _) => expr.as_ref().map(|expr| format!("break {}", self.gen_expr(expr))).unwrap_or_else(|| "break".to_string()),
            Expr::Continue(_) => "continue".to_string(),
        }
    }

    fn gen_jsx_attr_value_expr(&self, value: &JSXAttrValue) -> String {
        match value {
            JSXAttrValue::String(value) => format!("\"{}\".to_string()", self.escape_string(value)),
            JSXAttrValue::Expr(expr) => self.gen_expr(expr),
            JSXAttrValue::Bool(value) => value.to_string(),
        }
    }

    fn gen_jsx_children_expr(&self, children: &[JSXNode]) -> String {
        if children.is_empty() {
            "String::new()".to_string()
        } else {
            let child_strs: Vec<String> =
                children.iter().map(|child| self.gen_jsx(child)).collect();
            format!("vec![{}].join(\"\")", child_strs.join(", "))
        }
    }

    fn gen_jsx(&self, node: &JSXNode) -> String {
        match node {
            JSXNode::Text(text, _) => format!("\"{}\".to_string()", self.escape_string(text)),
            JSXNode::Expression(expr) => format!("format!(\"{{}}\", {})", self.gen_expr(expr)),
            JSXNode::Fragment(children, _) => self.gen_jsx_children_expr(children),
            JSXNode::Element {
                tag,
                attributes,
                children,
                ..
            } => {
                let attr_strs: Vec<String> = attributes
                    .iter()
                    .map(|attr| match &attr.value {
                        JSXAttrValue::String(value) => format!(
                            "format!(\" {}=\\\"{{}}\\\"\", \"{}\")",
                            attr.name,
                            self.escape_string(value)
                        ),
                        JSXAttrValue::Expr(expr) => format!(
                            "format!(\" {}=\\\"{{}}\\\"\", {})",
                            attr.name,
                            self.gen_expr(expr)
                        ),
                        JSXAttrValue::Bool(value) => {
                            if *value {
                                format!("\" {}\".to_string()", attr.name)
                            } else {
                                "String::new()".to_string()
                            }
                        }
                    })
                    .collect();
                let attrs_expr = if attr_strs.is_empty() {
                    "String::new()".to_string()
                } else {
                    format!("vec![{}].join(\"\")", attr_strs.join(", "))
                };
                let children_expr = self.gen_jsx_children_expr(children);
                format!(
                    "format!(\"<{}{{}}{{}}</{}>\", {}, {})",
                    tag, tag, attrs_expr, children_expr
                )
            }
            JSXNode::ComponentCall {
                name,
                props,
                children,
                ..
            } => {
                let props_name = self.component_props_name(name);
                let mut field_strs: Vec<String> = props
                    .iter()
                    .map(|prop| {
                        format!(
                            "{}: {}",
                            prop.name,
                            self.gen_jsx_attr_value_expr(&prop.value)
                        )
                    })
                    .collect();
                field_strs.push(format!(
                    "children: {}",
                    self.gen_jsx_children_expr(children)
                ));
                format!("{}({} {{ {} }})", name, props_name, field_strs.join(", "))
            }
            JSXNode::For {
                binding,
                iter,
                body,
                ..
            } => {
                format!(
                    "({}).into_iter().map(|{}| {}).collect::<Vec<String>>().join(\"\")",
                    self.gen_expr(iter),
                    binding,
                    self.gen_jsx(body)
                )
            }
            JSXNode::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                let else_expr = else_branch
                    .as_ref()
                    .map(|branch| self.gen_jsx(branch))
                    .unwrap_or_else(|| "String::new()".to_string());
                format!(
                    "if {} {{ {} }} else {{ {} }}",
                    self.gen_expr(condition),
                    self.gen_jsx(then_branch),
                    else_expr
                )
            }
        }
    }

    fn gen_match_expr(
        &self,
        scrutinee: &Expr,
        arms: &[kain_core::ast::MatchArm],
        statement_context: bool,
    ) -> String {
        let enum_hint = self.enum_hint_for_expr(scrutinee);
        let mut arm_bodies: Vec<String> = arms.iter().map(|arm| self.gen_expr(&arm.body)).collect();
        let prefers_string = arm_bodies.iter().any(|body| body.contains(".to_string()"));
        if prefers_string {
            for body in &mut arm_bodies {
                if body.ends_with(".as_str()") {
                    *body = format!("{}.to_string()", body);
                }
            }
        }
        if statement_context {
            for body in &mut arm_bodies {
                *body = self.coerce_statement_placeholder(body);
            }
        }
        let arm_strs: Vec<String> = arms
            .iter()
            .zip(arm_bodies.iter())
            .map(|(arm, body)| {
                let guard = arm
                    .guard
                    .as_ref()
                    .map(|guard| format!(" if {}", self.gen_expr(guard)))
                    .unwrap_or_default();
                format!(
                    "{}{} => {}",
                    self.gen_pattern_with_hint(&arm.pattern, enum_hint.as_deref()),
                    guard,
                    body
                )
            })
            .collect();
        format!(
            "match {} {{ {} }}",
            self.gen_expr(scrutinee),
            arm_strs.join(", ")
        )
    }

    fn gen_pattern(&self, pattern: &Pattern) -> String {
        self.gen_pattern_with_hint(pattern, None)
    }

    fn gen_pattern_with_hint(&self, pattern: &Pattern, enum_hint: Option<&str>) -> String {
        match pattern {
            Pattern::Wildcard(_) => "_".to_string(),
            Pattern::Literal(expr) => self.gen_pattern_literal(expr),
            Pattern::Binding { name, mutable, .. } => {
                if *mutable {
                    format!("mut {}", sanitize_emitted_identifier(name))
                } else {
                    sanitize_emitted_identifier(name)
                }
            }
            Pattern::Struct {
                name, fields, rest, ..
            } => {
                let mut field_strs: Vec<String> = fields
                    .iter()
                    .map(|(field, pat)| {
                        format!(
                            "{}: {}",
                            sanitize_emitted_identifier(field),
                            self.gen_pattern_with_hint(pat, None)
                        )
                    })
                    .collect();
                if *rest || !fields.is_empty() {
                    field_strs.push("..".to_string());
                }
                format!(
                    "{} {{ {} }}",
                    self.normalize_runtime_path(name),
                    field_strs.join(", ")
                )
            }
            Pattern::Tuple(patterns, _) => {
                let parts: Vec<String> = patterns
                    .iter()
                    .map(|pat| self.gen_pattern_with_hint(pat, None))
                    .collect();
                if parts.len() == 1 {
                    format!("({},)", parts[0])
                } else {
                    format!("({})", parts.join(", "))
                }
            }
            Pattern::Variant {
                enum_name,
                variant,
                fields,
                ..
            } => {
                let head = self.normalize_variant_head(enum_name.as_ref(), variant, enum_hint);
                match fields {
                    VariantPatternFields::Unit => head,
                    VariantPatternFields::Tuple(patterns) => {
                        let parts: Vec<String> = patterns
                            .iter()
                            .map(|pat| self.gen_pattern_with_hint(pat, None))
                            .collect();
                        format!("{head}({})", parts.join(", "))
                    }
                    VariantPatternFields::Struct(fields) => {
                        let mut parts: Vec<String> = fields
                            .iter()
                            .map(|(field, pat)| {
                                format!("{field}: {}", self.gen_pattern_with_hint(pat, None))
                            })
                            .collect();
                        parts.push("..".to_string());
                        format!("{head} {{ {} }}", parts.join(", "))
                    }
                }
            }
            Pattern::Slice { patterns, rest, .. } => {
                let mut parts: Vec<String> = patterns
                    .iter()
                    .map(|pat| self.gen_pattern_with_hint(pat, None))
                    .collect();
                if let Some(rest_name) = rest {
                    parts.push(format!("{rest_name} @ .."));
                }
                format!("[{}]", parts.join(", "))
            }
            Pattern::Or(patterns, _) => patterns
                .iter()
                .map(|pat| self.gen_pattern_with_hint(pat, enum_hint))
                .collect::<Vec<_>>()
                .join(" | "),
            Pattern::Range {
                start,
                end,
                inclusive,
                ..
            } => {
                let start = start
                    .as_ref()
                    .map(|value| self.gen_pattern_literal(value))
                    .unwrap_or_default();
                let end = end
                    .as_ref()
                    .map(|value| self.gen_pattern_literal(value))
                    .unwrap_or_default();
                if *inclusive {
                    format!("{start}..={end}")
                } else {
                    format!("{start}..{end}")
                }
            }
        }
    }

    fn collect_binding_types(&self, func: &Function) -> HashMap<String, String> {
        func.params
            .iter()
            .filter_map(|param| {
                self.extract_named_type_name(&param.ty)
                    .map(|name| (param.name.clone(), name))
            })
            .collect()
    }

    fn current_self_type(&self) -> Option<&str> {
        self.self_type_stack.last().map(String::as_str)
    }

    fn current_impl_target(&self) -> Option<String> {
        self.impl_target_stack.last().cloned()
    }

    fn current_function_name(&self) -> Option<&str> {
        self.current_function_stack.last().map(String::as_str)
    }

    fn current_impl_trait(&self) -> Option<&str> {
        self.impl_trait_stack
            .last()
            .and_then(|value| value.as_deref())
    }

    fn is_selfhost_empty_block(&self, block: &Block) -> bool {
        matches!(
            block.stmts.as_slice(),
            [Stmt::Let {
                pattern: Pattern::Binding { name, .. },
                value: Some(Expr::None(_)),
                ..
            }] if name == "__selfhost_empty"
        )
    }

    fn extract_named_type_name(&self, ty: &Type) -> Option<String> {
        match ty {
            Type::Named { name, .. } => Some(name.clone()),
            Type::Ref { inner, .. } => self.extract_named_type_name(inner),
            _ => None,
        }
    }

    fn enum_hint_for_expr(&self, expr: &Expr) -> Option<String> {
        let Expr::Ident(name, _) = expr else {
            return None;
        };

        self.binding_type_stack
            .iter()
            .rev()
            .find_map(|scope| scope.get(name).cloned())
    }

    fn collect_mutated_bindings_in_block(&self, block: &Block) -> HashSet<String> {
        let mut names = HashSet::new();
        self.collect_mutated_bindings_in_stmts(&block.stmts, &mut names);
        names
    }

    fn collect_mutated_bindings_in_stmts(&self, stmts: &[Stmt], names: &mut HashSet<String>) {
        for stmt in stmts {
            match stmt {
                Stmt::Let { value, .. } => {
                    if let Some(value) = value {
                        self.collect_mutated_bindings_in_expr(value, names);
                    }
                }
                Stmt::Expr(expr) => self.collect_mutated_bindings_in_expr(expr, names),
                Stmt::Return(value, _) | Stmt::Break(value, _) => {
                    if let Some(value) = value {
                        self.collect_mutated_bindings_in_expr(value, names);
                    }
                }
                Stmt::For { iter, body, .. } => {
                    self.collect_mutated_bindings_in_expr(iter, names);
                    self.collect_mutated_bindings_in_stmts(&body.stmts, names);
                }
                Stmt::While {
                    condition, body, ..
                } => {
                    self.collect_mutated_bindings_in_expr(condition, names);
                    self.collect_mutated_bindings_in_stmts(&body.stmts, names);
                }
                Stmt::Loop { body, .. } => {
                    self.collect_mutated_bindings_in_stmts(&body.stmts, names)
                }
                Stmt::Continue(_) | Stmt::Item(_) => {}
            }
        }
    }

    fn collect_mutated_bindings_in_expr(&self, expr: &Expr, names: &mut HashSet<String>) {
        match expr {
            Expr::Assign { target, value, .. } => {
                self.record_assignment_target(target, names);
                self.collect_mutated_bindings_in_expr(value, names);
            }
            Expr::Binary { left, right, .. } => {
                self.collect_mutated_bindings_in_expr(left, names);
                self.collect_mutated_bindings_in_expr(right, names);
            }
            Expr::Unary { operand, .. }
            | Expr::Ref { value: operand, .. }
            | Expr::AddrOf { value: operand, .. }
            | Expr::Deref(operand, _)
            | Expr::Try(operand, _)
            | Expr::Await(operand, _)
            | Expr::AsyncBlock(operand, _)
            | Expr::Comptime(operand, _)
            | Expr::Paren(operand, _)
            | Expr::Return(Some(operand), _)
            | Expr::Break(Some(operand), _) => {
                self.collect_mutated_bindings_in_expr(operand, names)
            }
            Expr::Call { callee, args, .. } => {
                self.collect_mutated_bindings_in_expr(callee, names);
                for arg in args {
                    self.collect_mutated_bindings_in_expr(&arg.value, names);
                }
            }
            Expr::StageCall { args, .. } => {
                for arg in args {
                    self.collect_mutated_bindings_in_expr(&arg.value, names);
                }
            }
            Expr::MethodCall { receiver, args, .. } => {
                self.collect_mutated_bindings_in_expr(receiver, names);
                for arg in args {
                    self.collect_mutated_bindings_in_expr(&arg.value, names);
                }
            }
            Expr::Field { object, .. } => self.collect_mutated_bindings_in_expr(object, names),
            Expr::Index { object, index, .. } => {
                self.collect_mutated_bindings_in_expr(object, names);
                self.collect_mutated_bindings_in_expr(index, names);
            }
            Expr::Struct { fields, rest, .. } => {
                for (_, value) in fields {
                    self.collect_mutated_bindings_in_expr(value, names);
                }
                if let Some(rest) = rest {
                    self.collect_mutated_bindings_in_expr(rest, names);
                }
            }
            Expr::AggregateInit { fields, .. } => {
                for (_, value) in fields {
                    self.collect_mutated_bindings_in_expr(value, names);
                }
            }
            Expr::EnumVariant { fields, .. } => match fields {
                EnumVariantFields::Unit => {}
                EnumVariantFields::Tuple(values) => {
                    for value in values {
                        self.collect_mutated_bindings_in_expr(value, names);
                    }
                }
                EnumVariantFields::Struct(values) => {
                    for (_, value) in values {
                        self.collect_mutated_bindings_in_expr(value, names);
                    }
                }
            },
            Expr::Array(values, _) | Expr::Tuple(values, _) | Expr::FString(values, _) => {
                for value in values {
                    self.collect_mutated_bindings_in_expr(value, names);
                }
            }
            Expr::Range { start, end, .. } => {
                if let Some(start) = start {
                    self.collect_mutated_bindings_in_expr(start, names);
                }
                if let Some(end) = end {
                    self.collect_mutated_bindings_in_expr(end, names);
                }
            }
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                self.collect_mutated_bindings_in_expr(condition, names);
                self.collect_mutated_bindings_in_stmts(&then_branch.stmts, names);
                if let Some(else_branch) = else_branch {
                    self.collect_mutated_bindings_in_else_branch(else_branch, names);
                }
            }
            Expr::Match {
                scrutinee, arms, ..
            } => {
                self.collect_mutated_bindings_in_expr(scrutinee, names);
                for arm in arms {
                    if let Some(guard) = &arm.guard {
                        self.collect_mutated_bindings_in_expr(guard, names);
                    }
                    self.collect_mutated_bindings_in_expr(&arm.body, names);
                }
            }
            Expr::Lambda { body, .. } => self.collect_mutated_bindings_in_expr(body, names),
            Expr::PtrOffset {
                pointer, offset, ..
            } => {
                self.collect_mutated_bindings_in_expr(pointer, names);
                self.collect_mutated_bindings_in_expr(offset, names);
            }
            Expr::MemLoad { pointer, .. } => self.collect_mutated_bindings_in_expr(pointer, names),
            Expr::MemStore { pointer, value, .. } => {
                self.collect_mutated_bindings_in_expr(pointer, names);
                self.collect_mutated_bindings_in_expr(value, names);
            }
            Expr::Alloc { size, .. } => self.collect_mutated_bindings_in_expr(size, names),
            Expr::Realloc { pointer, size, .. } => {
                self.collect_mutated_bindings_in_expr(pointer, names);
                self.collect_mutated_bindings_in_expr(size, names);
            }
            Expr::Observe { target, body, .. } | Expr::Collapse { target, body, .. } => {
                self.collect_mutated_bindings_in_expr(target, names);
                self.collect_mutated_bindings_in_expr(body, names);
            }
            Expr::Decay { target, .. } => self.collect_mutated_bindings_in_expr(target, names),
            Expr::Teleport { value, .. } => self.collect_mutated_bindings_in_expr(value, names),
            Expr::Cast { value, .. } => self.collect_mutated_bindings_in_expr(value, names),
            Expr::Spawn { init, .. } | Expr::SendMsg { data: init, .. } => {
                for (_, value) in init {
                    self.collect_mutated_bindings_in_expr(value, names);
                }
            }
            Expr::Block(block, _) => self.collect_mutated_bindings_in_stmts(&block.stmts, names),
            Expr::JSX(..)
            | Expr::Int(_, _)
            | Expr::Float(_, _)
            | Expr::String(_, _)
            | Expr::Bool(_, _)
            | Expr::None(_)
            | Expr::Ident(_, _)
            | Expr::MacroCall { .. }
            | Expr::SizeOfType { .. }
            | Expr::AlignOfType { .. }
            | Expr::Alloca { .. }
            | Expr::Uninit { .. }
            | Expr::Continue(_)
            | Expr::Return(None, _)
            | Expr::Break(None, _) => {}
        }
    }

    fn collect_mutated_bindings_in_else_branch(
        &self,
        branch: &ElseBranch,
        names: &mut HashSet<String>,
    ) {
        match branch {
            ElseBranch::Else(block) => self.collect_mutated_bindings_in_stmts(&block.stmts, names),
            ElseBranch::ElseIf(condition, block, next) => {
                self.collect_mutated_bindings_in_expr(condition, names);
                self.collect_mutated_bindings_in_stmts(&block.stmts, names);
                if let Some(next) = next {
                    self.collect_mutated_bindings_in_else_branch(next, names);
                }
            }
        }
    }

    fn record_assignment_target(&self, expr: &Expr, names: &mut HashSet<String>) {
        match expr {
            Expr::Ident(name, _) => {
                names.insert(name.clone());
            }
            Expr::Field { object, .. } => self.record_assignment_target(object, names),
            Expr::Index { object, .. } => self.record_assignment_target(object, names),
            Expr::Paren(inner, _) | Expr::Deref(inner, _) => {
                self.record_assignment_target(inner, names)
            }
            _ => {}
        }
    }

    fn gen_pattern_literal(&self, expr: &Expr) -> String {
        match expr {
            Expr::String(value, _) => format!("\"{}\"", self.escape_string(value)),
            Expr::Int(value, _) => value.to_string(),
            Expr::Float(value, _) => format!("{value:?}"),
            Expr::Bool(value, _) => value.to_string(),
            Expr::None(_) => "None".to_string(),
            Expr::Ident(name, _) => self.normalize_runtime_path(name),
            Expr::EnumVariant {
                enum_name,
                variant,
                fields,
                ..
            } => match fields {
                EnumVariantFields::Unit => {
                    self.normalize_variant_head(Some(enum_name), variant, None)
                }
                EnumVariantFields::Tuple(values) => {
                    let parts: Vec<String> = values
                        .iter()
                        .map(|value| self.gen_pattern_literal(value))
                        .collect();
                    format!(
                        "{}({})",
                        self.normalize_variant_head(Some(enum_name), variant, None),
                        parts.join(", ")
                    )
                }
                EnumVariantFields::Struct(values) => {
                    let parts: Vec<String> = values
                        .iter()
                        .map(|(name, value)| {
                            format!(
                                "{}: {}",
                                sanitize_emitted_identifier(name),
                                self.gen_pattern_literal(value)
                            )
                        })
                        .collect();
                    format!(
                        "{} {{ {} }}",
                        self.normalize_variant_head(Some(enum_name), variant, None),
                        parts.join(", ")
                    )
                }
            },
            Expr::Paren(inner, _) => format!("({})", self.gen_pattern_literal(inner)),
            _ => self.gen_expr(expr),
        }
    }

    fn rust_function_name(&self, name: &str) -> String {
        name.split("::")
            .filter(|segment| !segment.is_empty())
            .map(sanitize_emitted_identifier)
            .collect::<Vec<_>>()
            .join("__")
    }

    fn rust_method_name(&self, name: &str) -> String {
        let mut emitted = self.rust_function_name(name);
        if let Some(target) = self.current_impl_target() {
            for prefix in [format!("{target}__"), format!("{target}_")] {
                if let Some(stripped) = emitted.strip_prefix(&prefix) {
                    emitted = stripped.to_string();
                    break;
                }
            }
            if emitted.ends_with("_fmt") {
                return "fmt_impl".to_string();
            }
        }
        if let Some(trait_name) = self.current_impl_trait() {
            match trait_name {
                _ if self.trait_name_matches(trait_name, "Display")
                    && emitted.ends_with("_fmt") =>
                {
                    return "fmt".to_string()
                }
                _ if self.trait_name_matches(trait_name, "Default")
                    && (emitted.ends_with("_default")
                        || emitted == "default"
                        || emitted == "default_") =>
                {
                    return "default".to_string()
                }
                _ => {}
            }
        }
        if self.current_impl_target().is_some() || self.current_impl_trait().is_some() {
            self.normalize_runtime_method(&emitted)
        } else {
            emitted
        }
    }

    fn map_trait_name(&self, name: &str) -> String {
        match name {
            "Display" => "std::fmt::Display".to_string(),
            "Debug" => "std::fmt::Debug".to_string(),
            _ => self.normalize_runtime_path(name),
        }
    }

    fn trait_name_matches(&self, trait_name: &str, target: &str) -> bool {
        trait_name == target || trait_name.ends_with(&format!("::{target}"))
    }

    fn normalize_runtime_head_segment(&self, segment: &str) -> String {
        let normalized = desanitize_kain_identifier(segment);
        if RUST_PATH_ROOT_SEGMENTS.contains(&normalized.as_str()) {
            normalized
        } else {
            self.map_named_type(&normalized)
        }
    }

    fn normalize_runtime_path(&self, path: &str) -> String {
        const STRIP_COLON_PREFIXES: &[&str] = &[
            "crate::runtime::",
            "crate::ui::",
            "crate::ast::",
            "crate::error::",
            "crate::effects::",
            "crate::types::",
            "crate::lexer::",
            "crate::diagnostic_registry::",
            "crate::diagnostics::",
            "crate::parser::",
            "crate::span::",
            "crate::language_features::",
            "crate::low_level_abi::",
            "crate::low_level_memory::",
            "crate::low_level_memory_metadata::",
            "crate::monomorphize::",
            "crate::stdlib::",
            "crate::comptime::",
            "crate::asm_ir::",
            "crate::tokio::runtime::",
            "runtime::",
            "ui::",
            "ast::",
            "error::",
            "effects::",
            "types::",
            "lexer::",
            "diagnostic_registry::",
            "diagnostics::",
            "parser::",
            "span::",
            "language_features::",
            "low_level_abi::",
            "low_level_memory::",
            "low_level_memory_metadata::",
            "monomorphize::",
            "stdlib::",
            "comptime::",
            "asm_ir::",
            "tokio::runtime::",
        ];
        const STRIP_DUNDER_PREFIXES: &[&str] = &[
            "crate__runtime__",
            "crate__ui__",
            "crate__ast__",
            "crate__error__",
            "crate__effects__",
            "crate__types__",
            "crate__lexer__",
            "crate__diagnostic_registry__",
            "crate__diagnostics__",
            "crate__parser__",
            "crate__span__",
            "crate__language_features__",
            "crate__low_level_abi__",
            "crate__low_level_memory__",
            "crate__low_level_memory_metadata__",
            "crate__monomorphize__",
            "crate__stdlib__",
            "crate__comptime__",
            "crate__asm_ir__",
            "crate__tokio__runtime__",
            "runtime__",
            "ui__",
            "ast__",
            "error__",
            "effects__",
            "types__",
            "lexer__",
            "diagnostic_registry__",
            "diagnostics__",
            "parser__",
            "span__",
            "language_features__",
            "low_level_abi__",
            "low_level_memory__",
            "low_level_memory_metadata__",
            "monomorphize__",
            "stdlib__",
            "comptime__",
            "asm_ir__",
            "tokio__runtime__",
        ];
        if path == "_self" || path == "self" {
            if self.current_self_type().is_none() {
                if let Some(Some(alias)) = self.free_self_alias_stack.last() {
                    return alias.clone();
                }
            }
            return "self".to_string();
        }
        if let Some(self_ty) = self.current_self_type() {
            if path == "Self_" || path == "Self" {
                return self_ty.to_string();
            }
            if let Some(rest) = path.strip_prefix("Self::") {
                return format!("{}::{}", self_ty, rest);
            }
            if let Some(rest) = path.strip_prefix("Self_::") {
                return format!("{}::{}", self_ty, rest);
            }
            if let Some(rest) = path.strip_prefix("Self___") {
                return format!(
                    "{}::{}",
                    self_ty,
                    self.normalize_runtime_segment(rest, true)
                );
            }
        }
        let mut normalized = path;
        if normalized.contains("__") && normalized.contains("::") {
            let canonical = normalized.replace("__", "::");
            if canonical != normalized {
                return self.normalize_runtime_path(&canonical);
            }
        }
        if !self.has_modules {
            for prefix in STRIP_COLON_PREFIXES {
                if let Some(rest) = normalized.strip_prefix(prefix) {
                    normalized = rest;
                    break;
                }
            }
            for prefix in STRIP_DUNDER_PREFIXES {
                if let Some(rest) = normalized.strip_prefix(prefix) {
                    normalized = rest;
                    break;
                }
            }
        }
        if normalized == "Self_" {
            return "Self".to_string();
        }
        if !self.has_modules && normalized.starts_with("crate::") {
            normalized = normalized.trim_start_matches("crate::");
        }
        if normalized.starts_with("__kain_") {
            return normalized.to_string();
        }
        if normalized.contains("::") {
            let segments = normalized
                .split("::")
                .filter(|segment| !segment.is_empty())
                .collect::<Vec<_>>();
            if let Some((first, rest)) = segments.split_first() {
                let mapped = self.normalize_runtime_head_segment(first);
                let rest = rest
                    .iter()
                    .map(|segment| self.normalize_runtime_segment(segment, true))
                    .collect::<Vec<_>>();
                return if rest.is_empty() {
                    mapped
                } else {
                    format!("{mapped}::{}", rest.join("::"))
                };
            }
        }
        if !normalized.contains("__") {
            return self.normalize_runtime_head_segment(normalized);
        }

        let segments = split_sanitized_dunder_path(normalized)
            .into_iter()
            .filter(|segment| !segment.is_empty())
            .collect::<Vec<_>>();
        if let Some((first, rest)) = segments.split_first() {
            let mapped = self.normalize_runtime_head_segment(first);
            let rest = rest
                .iter()
                .map(|segment| self.normalize_runtime_segment(segment, true))
                .collect::<Vec<_>>();
            return if rest.is_empty() {
                mapped
            } else {
                format!("{mapped}::{}", rest.join("::"))
            };
        }
        String::new()
    }

    fn normalize_runtime_segment(&self, segment: &str, terminal: bool) -> String {
        let normalized = if terminal {
            if segment.starts_with('_')
                && segment
                    .chars()
                    .nth(1)
                    .is_some_and(|ch| ch.is_ascii_uppercase())
            {
                segment.trim_start_matches('_').to_string()
            } else {
                match segment {
                    "new_" => "new".to_string(),
                    "default_" => "default".to_string(),
                    "var_" => "var".to_string(),
                    "spawn_" => "spawn".to_string(),
                    _ => segment.to_string(),
                }
            }
        } else {
            segment.to_string()
        };
        sanitize_emitted_identifier(&normalized)
    }

    fn normalize_runtime_method(&self, method: &str) -> String {
        if method.starts_with('_')
            && method
                .chars()
                .nth(1)
                .is_some_and(|ch| ch.is_ascii_uppercase())
        {
            return method.trim_start_matches('_').to_string();
        }
        let normalized = match method {
            "send_" => "send".to_string(),
            "spawn_" => "spawn".to_string(),
            "or_" => "or".to_string(),
            "with_" => "with".to_string(),
            "new_" => "new".to_string(),
            "default_" => "default".to_string(),
            "var_" => "var".to_string(),
            _ => desanitize_kain_identifier(method),
        };
        sanitize_rust_identifier(&normalized)
    }

    fn render_interpolated_string(&self, parts: &[Expr]) -> String {
        if parts.is_empty() {
            return "String::new()".to_string();
        }
        let mut lines = Vec::with_capacity(parts.len() + 4);
        lines.push("{".to_string());
        lines.push("let mut __kain_fstr = String::new();".to_string());
        for part in parts {
            match part {
                Expr::String(value, _) => {
                    lines.push(format!("__kain_fstr.push_str({value:?});"));
                }
                expr => {
                    lines.push(format!(
                        "__kain_fstr.push_str(&format!(\"{{}}\", {}));",
                        self.gen_expr(expr)
                    ));
                }
            }
        }
        lines.push("__kain_fstr".to_string());
        lines.push("}".to_string());
        lines.join(" ")
    }

    fn current_return_struct(&self) -> Option<&str> {
        self.current_return_struct_stack
            .last()
            .and_then(|value| value.as_deref())
    }

    fn normalize_variant_head(
        &self,
        enum_name: Option<&String>,
        variant: &str,
        enum_hint: Option<&str>,
    ) -> String {
        if let Some(enum_name) = enum_name {
            if enum_name == "Self_" || enum_name == "Self" {
                if let Some(enum_hint) = enum_hint {
                    return format!(
                        "{}::{}",
                        self.normalize_runtime_path(enum_hint),
                        self.normalize_runtime_method(variant)
                    );
                }
                if let Some(self_ty) = self.current_self_type() {
                    return format!(
                        "{}::{}",
                        self.normalize_runtime_path(self_ty),
                        self.normalize_runtime_method(variant)
                    );
                }
            }
            return format!(
                "{}::{}",
                self.normalize_runtime_path(enum_name),
                self.normalize_runtime_method(variant)
            );
        }

        if variant.contains("__") {
            let mut parts = variant
                .split("__")
                .filter(|segment| !segment.is_empty())
                .collect::<Vec<_>>();
            if let Some(last) = parts.pop() {
                let mut head = parts.join("::");
                if (head == "Self" || head == "Self_") && enum_hint.is_some() {
                    head = self.normalize_runtime_path(enum_hint.unwrap());
                } else if head == "Self" || head == "Self_" {
                    if let Some(self_ty) = self.current_self_type() {
                        head = self.normalize_runtime_path(self_ty);
                    }
                }
                if !head.is_empty() {
                    return format!("{}::{}", head, self.normalize_runtime_method(last));
                }
            }
        }

        if let Some(enum_hint) = enum_hint {
            return format!(
                "{}::{}",
                self.normalize_runtime_path(enum_hint),
                self.normalize_runtime_method(variant)
            );
        }

        self.normalize_runtime_method(variant)
    }

    fn coerce_statement_placeholder(&self, body: &str) -> String {
        if body == "None" {
            return "()".to_string();
        }
        if body.trim() == "{ None }" {
            return "{ () }".to_string();
        }
        body.replace(" else { None }", " else { () }")
    }

    fn coerce_placeholder_branches(&self, then_expr: &mut String, else_expr: &mut String) {
        let then_snapshot = then_expr.clone();
        let else_snapshot = else_expr.clone();
        if let Some(preserved) = self.replace_placeholder_expr(then_expr, &else_snapshot) {
            *then_expr = preserved;
        }
        if let Some(preserved) = self.replace_placeholder_expr(else_expr, &then_snapshot) {
            *else_expr = preserved;
        }
    }

    fn replace_placeholder_expr(&self, candidate: &str, other: &str) -> Option<String> {
        let trimmed = candidate.trim();
        if trimmed == "None" {
            return Some(self.placeholder_for_peer_branch(other));
        }
        if trimmed == "{ None }" {
            return Some(format!("{{ {} }}", self.placeholder_for_peer_branch(other)));
        }
        None
    }

    fn placeholder_for_peer_branch(&self, other: &str) -> String {
        let other = other.trim();
        if other.contains("VNode::") {
            if self.current_function_name() == Some("reconcile") {
                return "next.clone()".to_string();
            }
            return "VNode::Fragment(Vec::new())".to_string();
        }
        if other == "()" || other == "{ () }" || other.contains("return ") {
            return "()".to_string();
        }
        if other.starts_with("Some(") || other == "None" {
            return "None".to_string();
        }
        "None".to_string()
    }

    fn map_type(&self, ty: &Type) -> String {
        self.map_type_in_context(ty, None, false)
    }

    fn sanitize_lifetime_name(&self, lifetime: &str) -> String {
        let normalized = lifetime.trim_start_matches('\'');
        if normalized == "static" || normalized == "_" {
            normalized.to_string()
        } else {
            sanitize_rust_identifier(normalized)
        }
    }

    fn render_field_accessor(&self, field: &str) -> String {
        if let Some(index) = tuple_field_marker_index(field) {
            index.to_string()
        } else {
            sanitize_emitted_identifier(field)
        }
    }

    fn render_ref_lifetime(&self, lifetime: Option<&str>) -> String {
        lifetime
            .map(|name| format!("'{} ", self.sanitize_lifetime_name(name)))
            .unwrap_or_default()
    }

    fn lifetime_generic_arg_name(&self, ty: &Type) -> Option<String> {
        let Type::Named { name, generics, .. } = ty else {
            return None;
        };
        if !generics.is_empty() {
            return None;
        }
        let leaf = name.rsplit("::").next().unwrap_or(name);
        ((leaf == "_" || leaf == "static")
            || ((self.current_generic_lifetimes().contains(name)
                || self.current_generic_lifetimes().contains(leaf))
                && self.looks_like_lifetime_name(leaf)))
        .then(|| leaf.to_string())
    }

    fn map_generic_arg_in_context(
        &self,
        ty: &Type,
        current_self: Option<&str>,
        recursive_slot: bool,
    ) -> String {
        if let Some(lifetime_name) = self.lifetime_generic_arg_name(ty) {
            format!("'{}", self.sanitize_lifetime_name(&lifetime_name))
        } else {
            self.map_type_in_context(ty, current_self, recursive_slot)
        }
    }

    fn map_storage_type(&self, ty: &Type, current_self: Option<&str>) -> String {
        self.map_owned_type_in_context(ty, current_self, true)
    }

    fn map_return_type(&self, ty: &Type, current_self: Option<&str>) -> String {
        self.map_type_in_context(ty, current_self, false)
    }

    fn map_owned_type_in_context(
        &self,
        ty: &Type,
        current_self: Option<&str>,
        recursive_slot: bool,
    ) -> String {
        match ty {
            Type::Named { name, generics, .. } => {
                let rust_name = self.normalize_type_name(name, current_self);

                if recursive_slot && current_self.is_some_and(|self_name| rust_name == self_name) {
                    return format!("Box<{}>", rust_name);
                }

                if generics.is_empty() {
                    rust_name
                } else {
                    let gen_strs: Vec<String> = generics
                        .iter()
                        .map(|g| self.map_generic_arg_in_context(g, current_self, false))
                        .collect();
                    format!("{}<{}>", rust_name, gen_strs.join(", "))
                }
            }
            Type::Ref { inner, .. } => self.map_owned_type_in_context(inner, current_self, false),
            Type::Tuple(types, _) => {
                let type_strs: Vec<String> = types
                    .iter()
                    .map(|t| self.map_owned_type_in_context(t, current_self, true))
                    .collect();
                if type_strs.len() == 1 {
                    format!("({},)", type_strs[0])
                } else {
                    format!("({})", type_strs.join(", "))
                }
            }
            Type::Array(inner, size, _) => {
                format!(
                    "[{}; {}]",
                    self.map_owned_type_in_context(inner, current_self, true),
                    size
                )
            }
            Type::Slice(inner, _) => {
                format!(
                    "Vec<{}>",
                    self.map_owned_type_in_context(inner, current_self, true)
                )
            }
            Type::Function {
                params,
                return_type,
                ..
            } => {
                let param_strs: Vec<String> = params
                    .iter()
                    .map(|p| self.map_owned_type_in_context(p, current_self, false))
                    .collect();
                format!(
                    "fn({}) -> {}",
                    param_strs.join(", "),
                    self.map_owned_type_in_context(return_type, current_self, false)
                )
            }
            Type::Option(inner, _) => {
                format!(
                    "Option<{}>",
                    self.map_owned_type_in_context(inner, current_self, true)
                )
            }
            Type::Result(ok, err, _) => {
                format!(
                    "Result<{}, {}>",
                    self.map_owned_type_in_context(ok, current_self, true),
                    self.map_owned_type_in_context(err, current_self, true)
                )
            }
            _ => self.map_type_in_context(ty, current_self, recursive_slot),
        }
    }

    fn map_type_in_context(
        &self,
        ty: &Type,
        current_self: Option<&str>,
        recursive_slot: bool,
    ) -> String {
        match ty {
            Type::Named { name, generics, .. } => {
                let rust_name = self.normalize_type_name(name, current_self);

                if recursive_slot && current_self.is_some_and(|self_name| rust_name == self_name) {
                    return format!("Box<{}>", rust_name);
                }

                if generics.is_empty() {
                    rust_name
                } else {
                    let gen_strs: Vec<String> = generics
                        .iter()
                        .map(|g| self.map_generic_arg_in_context(g, current_self, false))
                        .collect();
                    format!("{}<{}>", rust_name, gen_strs.join(", "))
                }
            }
            Type::Tuple(types, _) => {
                let type_strs: Vec<String> = types
                    .iter()
                    .map(|t| self.map_type_in_context(t, current_self, true))
                    .collect();
                if type_strs.len() == 1 {
                    format!("({},)", type_strs[0])
                } else {
                    format!("({})", type_strs.join(", "))
                }
            }
            Type::Array(inner, size, _) => {
                format!(
                    "[{}; {}]",
                    self.map_type_in_context(inner, current_self, true),
                    size
                )
            }
            Type::Slice(inner, _) => {
                format!("[{}]", self.map_type_in_context(inner, current_self, true))
            }
            Type::Ref {
                mutable,
                inner,
                lifetime,
                ..
            } => {
                let lifetime = self.render_ref_lifetime(lifetime.as_deref());
                if *mutable {
                    format!(
                        "&{}mut {}",
                        lifetime,
                        self.map_type_in_context(inner, current_self, false)
                    )
                } else {
                    format!(
                        "&{}{}",
                        lifetime,
                        self.map_type_in_context(inner, current_self, false)
                    )
                }
            }
            Type::Ptr { mutable, inner, .. } => {
                if *mutable {
                    format!(
                        "*mut {}",
                        self.map_type_in_context(inner, current_self, false)
                    )
                } else {
                    format!(
                        "*const {}",
                        self.map_type_in_context(inner, current_self, false)
                    )
                }
            }
            Type::Function {
                params,
                return_type,
                ..
            } => {
                let param_strs: Vec<String> = params
                    .iter()
                    .map(|p| self.map_type_in_context(p, current_self, false))
                    .collect();
                format!(
                    "fn({}) -> {}",
                    param_strs.join(", "),
                    self.map_type_in_context(return_type, current_self, false)
                )
            }
            Type::Option(inner, _) => {
                format!(
                    "Option<{}>",
                    self.map_type_in_context(inner, current_self, true)
                )
            }
            Type::Result(ok, err, _) => {
                format!(
                    "Result<{}, {}>",
                    self.map_type_in_context(ok, current_self, true),
                    self.map_type_in_context(err, current_self, true)
                )
            }
            Type::Infer(_) => "_".to_string(),
            Type::Never(_) => "!".to_string(),
            Type::Unit(_) => "()".to_string(),
            Type::Impl {
                trait_name,
                generics,
                ..
            } => {
                if generics.is_empty() {
                    format!("impl {}", trait_name)
                } else {
                    let gen_strs: Vec<String> = generics
                        .iter()
                        .map(|g| self.map_generic_arg_in_context(g, current_self, false))
                        .collect();
                    format!("impl {}<{}>", trait_name, gen_strs.join(", "))
                }
            }
        }
    }

    fn visibility_prefix(&self, visibility: kain_core::ast::Visibility) -> &'static str {
        match visibility {
            kain_core::ast::Visibility::Public => "pub ",
            kain_core::ast::Visibility::Private => "",
            kain_core::ast::Visibility::Crate => "pub(crate) ",
            kain_core::ast::Visibility::Super => "pub(super) ",
        }
    }

    fn function_modifiers(&self, effects: &[Effect]) -> String {
        let mut parts = Vec::new();
        if effects.contains(&Effect::Async) {
            parts.push("async");
        }

        if effects.contains(&Effect::Unsafe) {
            parts.push("unsafe");
        }
        if parts.is_empty() {
            String::new()
        } else {
            format!("{} ", parts.join(" "))
        }
    }

    fn map_named_type(&self, name: &str) -> String {
        let mapped = match name {
            "Int" => "i64".to_string(),
            "UInt" => "u64".to_string(),
            "Int8" => "i8".to_string(),
            "Int16" => "i16".to_string(),
            "Int32" => "i32".to_string(),
            "Int64" => "i64".to_string(),
            "UInt8" => "u8".to_string(),
            "UInt16" => "u16".to_string(),
            "UInt32" => "u32".to_string(),
            "UInt64" => "u64".to_string(),
            "Float" => "f64".to_string(),
            "Float32" => "f32".to_string(),
            "Float64" => "f64".to_string(),
            "Bool" => "bool".to_string(),
            "String" => "String".to_string(),
            "Char" => "char".to_string(),
            "Unit" => "()".to_string(),
            "Array" => "Vec".to_string(),
            "Map" => "std::collections::HashMap".to_string(),
            "Set" => "std::collections::HashSet".to_string(),
            "Range" => "std::ops::Range".to_string(),
            "Error" => "crate::error::KainError".to_string(),
            "PathBuf" => "std::path::PathBuf".to_string(),
            "Path" => "std::path::Path".to_string(),
            "Formatter" => "std::fmt::Formatter".to_string(),
            "Sender" => "flume::Sender".to_string(),
            "Receiver" => "flume::Receiver".to_string(),
            "Lazy" => "once_cell::sync::Lazy".to_string(),
            "PyAny" => "pyo3::PyAny".to_string(),
            "PyResult" => "pyo3::PyResult".to_string(),
            "PyObject" => "pyo3::PyObject".to_string(),
            "Python" => "pyo3::Python".to_string(),
            _ => name.to_string(),
        };
        sanitize_emitted_path(&mapped)
    }

    fn normalize_type_name(&self, name: &str, current_self: Option<&str>) -> String {
        let raw = name.trim();
        let mut leaf = raw.rsplit("::").next().unwrap_or(raw).trim();
        if let Some(stripped) = leaf.strip_prefix("&mut ") {
            leaf = stripped.trim();
        } else if let Some(stripped) = leaf.strip_prefix('&') {
            leaf = stripped.trim();
        }
        if leaf == "Self_" || leaf == "Self" {
            current_self
                .map(str::to_string)
                .unwrap_or_else(|| "Self".to_string())
        } else if raw.contains("::") {
            sanitize_emitted_path(raw)
        } else {
            self.map_named_type(leaf)
        }
    }

    fn inferred_self_type<'a>(&self, fn_name: &'a str) -> Option<&'a str> {
        let (prefix, _) = fn_name.split_once('_')?;
        if prefix
            .chars()
            .next()
            .is_some_and(|ch| ch.is_ascii_uppercase())
        {
            Some(prefix)
        } else {
            None
        }
    }

    fn register_symbol(&mut self, kind: &str, name: &str) -> bool {
        self.emitted_symbols.insert(format!("{}:{}", kind, name))
    }

    fn should_emit_function(&self, func: &Function) -> bool {
        let lower = func.name.to_ascii_lowercase();
        if lower.starts_with("test_")
            || lower.ends_with("_test")
            || lower.contains("_test_")
            || lower.starts_with("create_test_")
        {
            return false;
        }

        !func
            .attributes
            .iter()
            .any(|attr| attr.name.eq_ignore_ascii_case("test"))
    }

    fn map_binop(&self, op: &BinaryOp) -> &'static str {
        match op {
            BinaryOp::Add => "+",
            BinaryOp::Sub => "-",
            BinaryOp::Mul => "*",
            BinaryOp::Div => "/",
            BinaryOp::Mod => "%",
            BinaryOp::Pow => ".pow",
            BinaryOp::And => "&&",
            BinaryOp::Or => "||",
            BinaryOp::Eq => "==",
            BinaryOp::Ne => "!=",
            BinaryOp::Lt => "<",
            BinaryOp::Le => "<=",
            BinaryOp::Gt => ">",
            BinaryOp::Ge => ">=",
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
            BinaryOp::Range => "..",
            BinaryOp::RangeInclusive => "..=",
        }
    }

    fn map_unaryop(&self, op: &UnaryOp) -> &'static str {
        match op {
            UnaryOp::Not => "!",
            UnaryOp::Neg => "-",
            UnaryOp::Ref => "&",
            UnaryOp::RefMut => "&mut ",
            UnaryOp::BitNot => "!",
            UnaryOp::Deref => "*",
        }
    }

    fn escape_string(&self, s: &str) -> String {
        let mut result = String::new();
        for c in s.chars() {
            match c {
                '\n' => result.push_str("\\n"),
                '\t' => result.push_str("\\t"),
                '\\' => result.push_str("\\\\"),
                '"' => result.push_str("\\\""),
                _ => result.push(c),
            }
        }
        result
    }
}

/// Generate a Cargo.toml for the transpiled Rust project
pub fn gen_cargo_toml(name: &str, deps: &[&str]) -> String {
    let mut sb = StringBuilder::new();

    sb.push_line("[package]");
    sb.push_line(&format!("name = \"{}\"", name));
    sb.push_line("version = \"0.1.0\"");
    sb.push_line("edition = \"2021\"");
    sb.push_line("");
    sb.push_line("# Generated by KAIN Compiler");
    sb.push_line("");
    sb.push_line("[dependencies]");

    for dep in deps {
        sb.push_line(&format!("{} = \"*\"", dep));
    }

    sb.build()
}

#[cfg(test)]
mod tests {
    use super::*;
    use kain_core::ast::{Field, Visibility};
    use kain_core::effects::EffectSet;
    use kain_core::span::Span;
    use kain_core::types::{
        ResolvedType, TypedComponent, TypedFunction, TypedItem, TypedProgram, TypedStruct,
    };
    use std::collections::HashMap;

    #[test]
    fn test_type_mapping() {
        let gen = RustGen::new(false);
        let int_ty = Type::Named {
            name: "Int".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(gen.map_type(&int_ty), "i64");

        let uint_ty = Type::Named {
            name: "UInt".to_string(),
            generics: vec![],
            span: Span::default(),
        };
        assert_eq!(gen.map_type(&uint_ty), "u64");

        let tuple_ty = Type::Tuple(vec![int_ty.clone()], Span::default());
        assert_eq!(gen.map_type(&tuple_ty), "(i64,)");
    }

    #[test]
    fn test_function_modifiers_and_implicit_return() {
        let mut gen = RustGen::new(false);
        let func = Function {
            name: "compute".to_string(),
            generics: vec![],
            params: vec![],
            return_type: Some(Type::Named {
                name: "Int".to_string(),
                generics: vec![],
                span: Span::default(),
            }),
            effects: vec![Effect::Async, Effect::Unsafe],
            body: Block {
                stmts: vec![Stmt::Expr(Expr::Int(42, Span::default()))],
                span: Span::default(),
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span: Span::default(),
        };

        gen.gen_function(&func);
        let output = gen.output.build();
        assert!(output.contains("pub async unsafe fn compute() -> i64 {"));
        assert!(output.contains("    42\n"));
        assert!(!output.contains("return 42;"));
    }

    #[test]
    fn test_low_level_memory_expr_lowering() {
        let gen = RustGen::new(false);
        let span = Span::default();
        let int_ty = Type::Named {
            name: "Int".to_string(),
            generics: vec![],
            span,
        };

        let mem_store = Expr::MemStore {
            pointer: Box::new(Expr::Ident("ptr".to_string(), span)),
            value: Box::new(Expr::Ident("value".to_string(), span)),
            store_ty: Some(int_ty.clone()),
            span,
        };
        assert!(gen.gen_expr(&mem_store).contains("std::ptr::write"));

        let mem_load = Expr::MemLoad {
            pointer: Box::new(Expr::Ident("ptr".to_string(), span)),
            load_ty: Some(int_ty.clone()),
            span,
        };
        assert!(gen.gen_expr(&mem_load).contains("std::ptr::read"));

        let ptr_offset = Expr::PtrOffset {
            pointer: Box::new(Expr::Ident("ptr".to_string(), span)),
            offset: Box::new(Expr::Int(4, span)),
            element_ty: Some(int_ty.clone()),
            span,
        };
        assert!(gen.gen_expr(&ptr_offset).contains("wrapping_offset"));

        let size_of = Expr::SizeOfType {
            target: int_ty.clone(),
            span,
        };
        assert_eq!(gen.gen_expr(&size_of), "size_of::<i64>() as i64");

        let alloca = Expr::Alloca {
            ty: int_ty.clone(),
            span,
        };
        assert!(gen.gen_expr(&alloca).contains("MaybeUninit::<i64>::zeroed"));
    }

    #[test]
    fn test_component_and_system_expr_codegen_fixture() {
        let span = Span::default();
        let int_ty = Type::Named {
            name: "Int".to_string(),
            generics: vec![],
            span,
        };
        let string_ty = Type::Named {
            name: "String".to_string(),
            generics: vec![],
            span,
        };
        let ptr_int_ty = Type::Ptr {
            mutable: true,
            inner: Box::new(int_ty.clone()),
            provenance: kain_core::ast::PointerProvenance::Raw,
            span,
        };

        let view_model = TypedItem::Struct(TypedStruct {
            ast: Struct {
                name: "ViewModel".to_string(),
                generics: vec![],
                fields: vec![
                    Field {
                        name: "value".to_string(),
                        ty: int_ty.clone(),
                        attributes: vec![],
                        visibility: Visibility::Public,
                        default: None,
                        weak: false,
                        span,
                    },
                    Field {
                        name: "label".to_string(),
                        ty: string_ty.clone(),
                        attributes: vec![],
                        visibility: Visibility::Public,
                        default: None,
                        weak: false,
                        span,
                    },
                ],
                methods: vec![],
                attributes: vec![],
                visibility: Visibility::Public,
                span,
            },
            field_types: HashMap::new(),
        });

        let raw_roundtrip = TypedItem::Function(TypedFunction {
            ast: Function {
                name: "raw_roundtrip".to_string(),
                generics: vec![],
                params: vec![
                    Param {
                        name: "ptr".to_string(),
                        ty: ptr_int_ty.clone(),
                        mutable: false,
                        default: None,
                        span,
                    },
                    Param {
                        name: "value".to_string(),
                        ty: int_ty.clone(),
                        mutable: false,
                        default: None,
                        span,
                    },
                ],
                return_type: Some(int_ty.clone()),
                effects: vec![Effect::Unsafe],
                body: Block {
                    stmts: vec![
                        Stmt::Expr(Expr::MemStore {
                            pointer: Box::new(Expr::Ident("ptr".to_string(), span)),
                            value: Box::new(Expr::Ident("value".to_string(), span)),
                            store_ty: Some(int_ty.clone()),
                            span,
                        }),
                        Stmt::Expr(Expr::MemLoad {
                            pointer: Box::new(Expr::Ident("ptr".to_string(), span)),
                            load_ty: Some(int_ty.clone()),
                            span,
                        }),
                    ],
                    span,
                },
                visibility: Visibility::Private,
                attributes: vec![],
                span,
            },
            resolved_type: ResolvedType::Int(kain_core::types::IntSize::I64),
            effects: EffectSet::default(),
        });

        let build_model = TypedItem::Function(TypedFunction {
            ast: Function {
                name: "build_model".to_string(),
                generics: vec![],
                params: vec![Param {
                    name: "value".to_string(),
                    ty: int_ty.clone(),
                    mutable: false,
                    default: None,
                    span,
                }],
                return_type: Some(Type::Named {
                    name: "ViewModel".to_string(),
                    generics: vec![],
                    span,
                }),
                effects: vec![],
                body: Block {
                    stmts: vec![Stmt::Expr(Expr::AggregateInit {
                        ty: Type::Named {
                            name: "ViewModel".to_string(),
                            generics: vec![],
                            span,
                        },
                        fields: vec![("value".to_string(), Expr::Ident("value".to_string(), span))],
                        zero_fill_rest: true,
                        span,
                    })],
                    span,
                },
                visibility: Visibility::Private,
                attributes: vec![],
                span,
            },
            resolved_type: ResolvedType::Struct("ViewModel".to_string(), HashMap::new()),
            effects: EffectSet::default(),
        });

        let hud_panel = TypedItem::Component(TypedComponent {
            ast: Component {
                name: "HudPanel".to_string(),
                props: vec![Param {
                    name: "title".to_string(),
                    ty: string_ty.clone(),
                    mutable: false,
                    default: None,
                    span,
                }],
                state: vec![kain_core::ast::StateDecl {
                    name: "count".to_string(),
                    ty: int_ty.clone(),
                    initial: Expr::Int(0, span),
                    weak: false,
                    attributes: vec![],
                    span,
                }],
                methods: vec![Function {
                    name: "increment".to_string(),
                    generics: vec![],
                    params: vec![Param {
                        name: "delta".to_string(),
                        ty: int_ty.clone(),
                        mutable: false,
                        default: None,
                        span,
                    }],
                    return_type: Some(int_ty.clone()),
                    effects: vec![],
                    body: Block {
                        stmts: vec![
                            Stmt::Expr(Expr::Assign {
                                target: Box::new(Expr::Ident("count".to_string(), span)),
                                value: Box::new(Expr::Binary {
                                    left: Box::new(Expr::Ident("count".to_string(), span)),
                                    op: BinaryOp::Add,
                                    right: Box::new(Expr::Ident("delta".to_string(), span)),
                                    span,
                                }),
                                span,
                            }),
                            Stmt::Expr(Expr::Ident("count".to_string(), span)),
                        ],
                        span,
                    },
                    visibility: Visibility::Private,
                    attributes: vec![],
                    span,
                }],
                effects: vec![],
                body: JSXNode::Element {
                    tag: "div".to_string(),
                    attributes: vec![kain_core::ast::JSXAttribute {
                        name: "class".to_string(),
                        value: JSXAttrValue::String("hud".to_string()),
                        span,
                    }],
                    children: vec![
                        JSXNode::Text("Title: ".to_string(), span),
                        JSXNode::Expression(Box::new(Expr::Ident("title".to_string(), span))),
                        JSXNode::Text(" Count: ".to_string(), span),
                        JSXNode::Expression(Box::new(Expr::Ident("count".to_string(), span))),
                        JSXNode::Text(" ".to_string(), span),
                        JSXNode::Expression(Box::new(Expr::Ident("children".to_string(), span))),
                    ],
                    span,
                },
                visibility: Visibility::Public,
                attributes: vec![],
                span,
            },
            prop_types: HashMap::new(),
        });

        let app_shell = TypedItem::Component(TypedComponent {
            ast: Component {
                name: "AppShell".to_string(),
                props: vec![],
                state: vec![],
                methods: vec![],
                effects: vec![],
                body: JSXNode::ComponentCall {
                    name: "HudPanel".to_string(),
                    props: vec![kain_core::ast::JSXAttribute {
                        name: "title".to_string(),
                        value: JSXAttrValue::String("Status".to_string()),
                        span,
                    }],
                    children: vec![JSXNode::Text("Inner body".to_string(), span)],
                    span,
                },
                visibility: Visibility::Public,
                attributes: vec![],
                span,
            },
            prop_types: HashMap::new(),
        });

        let program = TypedProgram {
            items: vec![view_model, raw_roundtrip, build_model, hud_panel, app_shell],
        };

        let output = generate(&program).expect("rust generation should succeed");
        assert!(output.contains("pub struct ViewModel {"));
        assert!(output.contains("raw_roundtrip("));
        assert!(output.contains("value: i64"));
        assert!(output.contains("fn build_model(value: i64) -> ViewModel {"));
        assert!(output.contains("ViewModel {"));
        assert!(output.contains("value: value"));
        assert!(output.contains("pub struct HudPanelProps {"));
        assert!(output.contains("pub children: String,"));
        assert!(output.contains("pub fn HudPanel(props: HudPanelProps) -> String {"));
        assert!(output.contains("let HudPanelProps { title, children } = props;"));
        assert!(output.contains("let mut count: i64 = 0;"));
        assert!(output.contains("let mut increment = |delta: i64| -> i64 {"));
        assert!(output.contains("count = (count + delta)"));
        assert!(output.contains("class="));
        assert!(output.contains("hud"));
        assert!(output.contains("Title: "));
        assert!(output.contains("Count: "));
        assert!(output.contains("format!(\"{}\", children)"));
        assert!(output.contains("pub struct AppShellProps {"));
        assert!(output.contains("pub fn AppShell(props: AppShellProps) -> String {"));
        assert!(output.contains("let AppShellProps { children } = props;"));
        assert!(output.contains("HudPanel(HudPanelProps { title: \"Status\".to_string(), children: vec![\"Inner body\".to_string()].join(\"\") })"));
    }

    #[test]
    fn test_async_block_and_panic_codegen() {
        let span = Span::default();
        let gen = RustGen::new(false);

        let async_expr = Expr::AsyncBlock(
            Box::new(Expr::Block(
                Block {
                    stmts: vec![Stmt::Expr(Expr::Await(
                        Box::new(Expr::Call {
                            callee: Box::new(Expr::Ident("work".to_string(), span)),
                            args: vec![],
                            span,
                        }),
                        span,
                    ))],
                    span,
                },
                span,
            )),
            span,
        );
        let async_rendered = gen.gen_expr(&async_expr);
        assert!(async_rendered.contains("async"));
        assert!(async_rendered.contains(".await"));

        let panic_expr = Expr::Call {
            callee: Box::new(Expr::Ident("panic".to_string(), span)),
            args: vec![kain_core::ast::CallArg {
                name: None,
                value: Expr::MethodCall {
                    receiver: Box::new(Expr::String("boom".to_string(), span)),
                    method: "to_string".to_string(),
                    args: vec![],
                    span,
                },
                span,
            }],
            span,
        };
        let panic_rendered = gen.gen_expr(&panic_expr);
        assert!(panic_rendered.starts_with("panic!(\"{}\", "));
        assert!(!panic_rendered.contains("panic!(\"boom\".to_string())"));
    }

    #[test]
    fn test_tuple_field_marker_renders_as_tuple_accessor() {
        let span = Span::default();
        let gen = RustGen::new(false);
        let expr = Expr::Field {
            object: Box::new(Expr::Ident("pair".to_string(), span)),
            field: "__kain_tuple_1".to_string(),
            span,
        };

        assert_eq!(gen.gen_expr(&expr), "pair.1");
    }

    #[test]
    fn test_bootstrap_lexer_intrinsic_renders_logos_backed_block() {
        let span = Span::default();
        let gen = RustGen::new(false);
        let expr = Expr::Call {
            callee: Box::new(Expr::Ident("__kain_bootstrap_lex_tokens".to_string(), span)),
            args: vec![CallArg {
                name: None,
                value: Expr::Ref {
                    mutable: false,
                    value: Box::new(Expr::Field {
                        object: Box::new(Expr::Ident("_self".to_string(), span)),
                        field: "source".to_string(),
                        span,
                    }),
                    span,
                },
                span,
            }],
            span,
        };

        let rendered = gen.gen_expr(&expr);
        assert!(rendered.contains("crate::lexer::TokenKind::lexer"));
        assert!(rendered.contains("crate::lexer::TokenKind::Indent"));
        assert!(rendered.contains("crate::lexer::TokenKind::Eof"));
    }

    #[test]
    fn test_bootstrap_parser_intrinsic_renders_parser_block() {
        let span = Span::default();
        let gen = RustGen::new(false);
        let expr = Expr::Call {
            callee: Box::new(Expr::Ident(
                "__kain_bootstrap_parse_source".to_string(),
                span,
            )),
            args: vec![
                CallArg {
                    name: None,
                    value: Expr::Ident("tokens".to_string(), span),
                    span,
                },
                CallArg {
                    name: None,
                    value: Expr::Ident("file_name".to_string(), span),
                    span,
                },
            ],
            span,
        };

        let rendered = gen.gen_expr(&expr);
        assert!(rendered.contains("crate::parser::Parser::new"));
        assert!(rendered.contains("unwrap()"));
    }

    #[test]
    fn test_bootstrap_runtime_intrinsic_renders_runtime_block() {
        let span = Span::default();
        let gen = RustGen::new(false);
        let expr = Expr::Call {
            callee: Box::new(Expr::Ident(
                "__kain_bootstrap_run_program".to_string(),
                span,
            )),
            args: vec![CallArg {
                name: None,
                value: Expr::Ident("program".to_string(), span),
                span,
            }],
            span,
        };

        let rendered = gen.gen_expr(&expr);
        assert!(rendered.contains("crate::runtime::Value::Null"));
    }

    #[test]
    fn test_bootstrap_llvm_intrinsic_renders_string_block() {
        let span = Span::default();
        let gen = RustGen::new(false);
        let expr = Expr::Call {
            callee: Box::new(Expr::Ident(
                "__kain_bootstrap_generate_llvm_ir".to_string(),
                span,
            )),
            args: vec![CallArg {
                name: None,
                value: Expr::Ident("program".to_string(), span),
                span,
            }],
            span,
        };

        let rendered = gen.gen_expr(&expr);
        assert!(rendered.contains("String::new()"));
    }

    #[test]
    fn test_lifetime_generics_render_in_structs_and_functions() {
        let span = Span::default();
        let mut gen = RustGen::new(false);
        let lifetime_generic = Generic {
            name: "a".to_string(),
            bounds: vec![],
            span,
        };
        let string_ref = Type::Ref {
            mutable: false,
            inner: Box::new(Type::Named {
                name: "String".to_string(),
                generics: vec![],
                span,
            }),
            lifetime: Some("a".to_string()),
            span,
        };
        let source_location = Struct {
            name: "SourceLocation".to_string(),
            generics: vec![lifetime_generic.clone()],
            fields: vec![Field {
                name: "file".to_string(),
                ty: string_ref.clone(),
                attributes: vec![],
                visibility: Visibility::Private,
                default: None,
                weak: false,
                span,
            }],
            methods: vec![],
            attributes: vec![],
            visibility: Visibility::Public,
            span,
        };
        let function = Function {
            name: "span_to_location".to_string(),
            generics: vec![lifetime_generic],
            params: vec![Param {
                name: "file".to_string(),
                ty: string_ref,
                mutable: false,
                default: None,
                span,
            }],
            return_type: Some(Type::Named {
                name: "SourceLocation".to_string(),
                generics: vec![Type::Named {
                    name: "a".to_string(),
                    generics: vec![],
                    span,
                }],
                span,
            }),
            effects: vec![],
            body: Block {
                stmts: vec![Stmt::Expr(Expr::Struct {
                    name: "SourceLocation".to_string(),
                    fields: vec![("file".to_string(), Expr::Ident("file".to_string(), span))],
                    rest: None,
                    span,
                })],
                span,
            },
            visibility: Visibility::Public,
            attributes: vec![],
            span,
        };

        gen.gen_struct(&source_location);
        gen.write_blank();
        gen.gen_function(&function);
        let output = gen.output.build();

        assert!(output.contains("pub struct SourceLocation<'a> {"));
        assert!(output.contains("file: &'a String"));
        assert!(output
            .contains("pub fn span_to_location<'a>(file: &'a String) -> SourceLocation<'a> {"));
    }

    #[test]
    fn test_self_in_generic_impl_uses_type_in_signature_and_base_path_in_body() {
        let span = Span::default();
        let mut gen = RustGen::new(false);
        let source_location = Struct {
            name: "SourceLocation".to_string(),
            generics: vec![Generic {
                name: "a".to_string(),
                bounds: vec![],
                span,
            }],
            fields: vec![Field {
                name: "file".to_string(),
                ty: Type::Ref {
                    mutable: false,
                    inner: Box::new(Type::Named {
                        name: "String".to_string(),
                        generics: vec![],
                        span,
                    }),
                    lifetime: Some("a".to_string()),
                    span,
                },
                attributes: vec![],
                visibility: Visibility::Private,
                default: None,
                weak: false,
                span,
            }],
            methods: vec![],
            attributes: vec![],
            visibility: Visibility::Public,
            span,
        };
        let impl_def = Impl {
            generics: vec![Generic {
                name: "a".to_string(),
                bounds: vec![],
                span,
            }],
            trait_name: None,
            trait_generics: vec![],
            target_type: Type::Named {
                name: "SourceLocation".to_string(),
                generics: vec![Type::Named {
                    name: "a".to_string(),
                    generics: vec![],
                    span,
                }],
                span,
            },
            methods: vec![Function {
                name: "new".to_string(),
                generics: vec![],
                params: vec![Param {
                    name: "file".to_string(),
                    ty: Type::Ref {
                        mutable: false,
                        inner: Box::new(Type::Named {
                            name: "String".to_string(),
                            generics: vec![],
                            span,
                        }),
                        lifetime: Some("a".to_string()),
                        span,
                    },
                    mutable: false,
                    default: None,
                    span,
                }],
                return_type: Some(Type::Named {
                    name: "Self_".to_string(),
                    generics: vec![],
                    span,
                }),
                effects: vec![],
                body: Block {
                    stmts: vec![Stmt::Expr(Expr::Struct {
                        name: "Self_".to_string(),
                        fields: vec![("file".to_string(), Expr::Ident("file".to_string(), span))],
                        rest: None,
                        span,
                    })],
                    span,
                },
                visibility: Visibility::Public,
                attributes: vec![],
                span,
            }],
            span,
        };

        gen.gen_struct(&source_location);
        gen.write_blank();
        gen.gen_impl(&impl_def);
        let output = gen.output.build();

        assert!(output.contains("impl<'a> SourceLocation<'a> {"));
        assert!(output.contains("pub fn new(file: &'a String) -> SourceLocation<'a> {"));
        assert!(output.contains("SourceLocation { file: file }"));
    }

    #[test]
    fn test_placeholder_lifetime_generic_arg_renders_verbatim() {
        let gen = RustGen::new(false);
        let span = Span::default();
        let ty = Type::Named {
            name: "std::fmt::Formatter".to_string(),
            generics: vec![Type::Named {
                name: "_".to_string(),
                generics: vec![],
                span,
            }],
            span,
        };

        assert_eq!(gen.map_type(&ty), "std::fmt::Formatter<'_>");
    }

    #[test]
    fn test_qualified_type_paths_remain_qualified() {
        let gen = RustGen::new(false);
        let span = Span::default();

        let io_error = Type::Named {
            name: "std::io::Error".to_string(),
            generics: vec![],
            span,
        };
        let crate_span = Type::Named {
            name: "crate::span::Span".to_string(),
            generics: vec![],
            span,
        };

        assert_eq!(gen.map_type(&io_error), "std::io::Error");
        assert_eq!(gen.map_type(&crate_span), "crate::span::Span");
    }
}
