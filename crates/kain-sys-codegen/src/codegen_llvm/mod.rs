//! LLVM IR Generator
//!
//! Generates textual LLVM IR (Intermediate Representation) which can be compiled
//! by `clang` or `llc`. This approach is chosen for maximum portability and
//! reliability without requiring local LLVM library linking during the build.

use kain_core::ast::{
    BinaryOp, Block, ElseBranch, Expr, JSXAttrValue, JSXNode, Pattern, Stmt, UnaryOp,
    VariantPatternFields,
};
use kain_core::error::{KainError, KainResult};
use kain_core::types::{ResolvedType, TypedComponent, TypedFunction, TypedItem, TypedProgram};
use kain_core::{
    lower_typed_program_memory_for_target, validate_typed_program_memory_support, CompileTarget,
};
use std::collections::HashMap;

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
enum LlvmTargetId {
    WindowsX64Msvc,
    LinuxX64Gnu,
    MacOsArm64,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct LlvmTargetDescriptor {
    id: LlvmTargetId,
    triple: &'static str,
    datalayout: &'static str,
}

const LLVM_TARGET_WINDOWS_X64_MSVC: LlvmTargetDescriptor = LlvmTargetDescriptor {
    id: LlvmTargetId::WindowsX64Msvc,
    triple: "x86_64-pc-windows-msvc",
    datalayout: "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128",
};

const LLVM_TARGET_LINUX_X64_GNU: LlvmTargetDescriptor = LlvmTargetDescriptor {
    id: LlvmTargetId::LinuxX64Gnu,
    triple: "x86_64-unknown-linux-gnu",
    datalayout: "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128",
};

const LLVM_TARGET_MACOS_ARM64: LlvmTargetDescriptor = LlvmTargetDescriptor {
    id: LlvmTargetId::MacOsArm64,
    triple: "arm64-apple-darwin",
    datalayout: "e-m:o-i64:64-i128:128-n32:64-S128",
};

const LLVM_TARGET_DESCRIPTOR_REGISTRY: &[LlvmTargetDescriptor] = &[
    LLVM_TARGET_WINDOWS_X64_MSVC,
    LLVM_TARGET_LINUX_X64_GNU,
    LLVM_TARGET_MACOS_ARM64,
];

fn runtime_symbol_for_stdlib_function(name: &str) -> &str {
    match name {
        "floor" => "kain_floor_i64",
        "ceil" => "kain_ceil_i64",
        "round" => "kain_round_i64",
        _ => name,
    }
}

fn resolve_host_llvm_target_descriptor() -> &'static LlvmTargetDescriptor {
    let target_id = if cfg!(all(target_os = "windows", target_arch = "x86_64")) {
        LlvmTargetId::WindowsX64Msvc
    } else if cfg!(all(target_os = "linux", target_arch = "x86_64")) {
        LlvmTargetId::LinuxX64Gnu
    } else if cfg!(all(target_os = "macos", target_arch = "aarch64")) {
        LlvmTargetId::MacOsArm64
    } else {
        LlvmTargetId::WindowsX64Msvc
    };

    LLVM_TARGET_DESCRIPTOR_REGISTRY
        .iter()
        .find(|descriptor| descriptor.id == target_id)
        .unwrap_or(&LLVM_TARGET_WINDOWS_X64_MSVC)
}

pub fn generate(program: &TypedProgram) -> KainResult<Vec<u8>> {
    let lowered = lower_typed_program_memory_for_target(program, CompileTarget::Llvm)?;
    validate_typed_program_memory_support(&lowered, CompileTarget::Llvm)?;
    let mut gen = LlvmGenerator::new();
    gen.compile_module(&lowered)?;
    Ok(gen.output.into_bytes())
}

struct LlvmGenerator {
    output: String,
    reg_count: usize,
    label_count: usize,
    /// Maps variable names to (stack_ptr, type)
    locals: HashMap<String, (String, String)>,
    /// Maps function names to return type
    functions: HashMap<String, String>,
    /// Maps string content to global variable name
    strings: HashMap<String, String>,
    string_counter: usize,
    /// Stack of (continue_label, break_label) for loops
    loop_stack: Vec<(String, String)>,
    /// Stack of scopes, each containing list of variable names declared in that scope
    scopes: Vec<Vec<String>>,
    /// Struct definitions: Name -> Vec<(FieldName, Type)>
    struct_defs: HashMap<String, Vec<(String, String)>>,
    component_defs: HashMap<String, Vec<(String, String)>>,
    /// Current basic block label (for Phi nodes)
    current_block: String,
    current_return_type: Option<String>,
    target: &'static LlvmTargetDescriptor,
}

impl LlvmGenerator {
    fn new() -> Self {
        Self {
            output: String::new(),
            reg_count: 0,
            label_count: 0,
            locals: HashMap::new(),
            functions: HashMap::new(),
            strings: HashMap::new(),
            string_counter: 0,
            loop_stack: Vec::new(),
            scopes: Vec::new(),
            struct_defs: HashMap::new(),
            component_defs: HashMap::new(),
            current_block: "entry".to_string(),
            current_return_type: None,
            target: resolve_host_llvm_target_descriptor(),
        }
    }

    fn emit(&mut self, s: &str) {
        self.output.push_str(s);
        self.output.push('\n');
    }

    fn emit_label(&mut self, label: &str) {
        self.emit(&format!("{}:", label));
        self.current_block = label.to_string();
    }

    fn next_reg(&mut self) -> String {
        let r = format!("%{}", self.reg_count);
        self.reg_count += 1;
        r
    }

    fn next_label(&mut self) -> String {
        let l = format!("L{}", self.label_count);
        self.label_count += 1;
        l
    }

    fn sanitize_type_fragment(fragment: &str) -> String {
        fragment
            .chars()
            .map(|ch| if ch.is_ascii_alphanumeric() { ch } else { '_' })
            .collect()
    }

    fn tuple_struct_name_from_types(field_tys: &[String]) -> String {
        let mut name = String::from("__kain_tuple");
        for field_ty in field_tys {
            name.push('_');
            name.push_str(&Self::sanitize_type_fragment(field_ty));
        }
        name
    }

    fn tuple_struct_ptr_type_from_types(field_tys: &[String]) -> String {
        format!("%{}*", Self::tuple_struct_name_from_types(field_tys))
    }

    fn register_tuple_struct(&mut self, field_tys: Vec<String>) -> String {
        let name = Self::tuple_struct_name_from_types(&field_tys);
        if !self.struct_defs.contains_key(&name) {
            let fields = field_tys
                .iter()
                .enumerate()
                .map(|(index, ty)| (format!("_{}", index), ty.clone()))
                .collect::<Vec<_>>();
            self.struct_defs.insert(name.clone(), fields);
            self.emit(&format!("%{} = type {{ {} }}", name, field_tys.join(", ")));
        }
        name
    }

    fn collect_tuple_types_from_ast(&mut self, ty: &kain_core::ast::Type) {
        match ty {
            kain_core::ast::Type::Tuple(items, _) => {
                for item in items {
                    self.collect_tuple_types_from_ast(item);
                }
                let field_tys = items
                    .iter()
                    .map(|item| self.map_type_from_ast(item))
                    .collect::<Vec<_>>();
                self.register_tuple_struct(field_tys);
            }
            kain_core::ast::Type::Array(inner, _, _)
            | kain_core::ast::Type::Slice(inner, _)
            | kain_core::ast::Type::Option(inner, _)
            | kain_core::ast::Type::Result(inner, _, _)
            | kain_core::ast::Type::Ref { inner, .. }
            | kain_core::ast::Type::Ptr { inner, .. } => self.collect_tuple_types_from_ast(inner),
            _ => {}
        }
    }

    fn collect_tuple_types_from_resolved(&mut self, ty: &ResolvedType) {
        match ty {
            ResolvedType::Tuple(items) => {
                for item in items {
                    self.collect_tuple_types_from_resolved(item);
                }
                let field_tys = items
                    .iter()
                    .map(|item| self.map_type(item))
                    .collect::<Vec<_>>();
                self.register_tuple_struct(field_tys);
            }
            ResolvedType::Array(inner, _)
            | ResolvedType::Slice(inner)
            | ResolvedType::Option(inner)
            | ResolvedType::Ref { inner, .. }
            | ResolvedType::Ptr { inner, .. } => self.collect_tuple_types_from_resolved(inner),
            ResolvedType::Result(ok, err) => {
                self.collect_tuple_types_from_resolved(ok);
                self.collect_tuple_types_from_resolved(err);
            }
            ResolvedType::Function { params, ret, .. } => {
                for param in params {
                    self.collect_tuple_types_from_resolved(param);
                }
                self.collect_tuple_types_from_resolved(ret);
            }
            _ => {}
        }
    }

    fn collect_program_tuple_types(&mut self, program: &TypedProgram) {
        for item in &program.items {
            match item {
                TypedItem::Function(func) => {
                    self.collect_tuple_types_from_resolved(&func.resolved_type);
                    for param in &func.ast.params {
                        self.collect_tuple_types_from_ast(&param.ty);
                    }
                    if let Some(ret) = &func.ast.return_type {
                        self.collect_tuple_types_from_ast(ret);
                    }
                }
                TypedItem::Struct(s) => {
                    for ty in s.field_types.values() {
                        self.collect_tuple_types_from_resolved(ty);
                    }
                }
                TypedItem::Component(component) => {
                    for ty in component.prop_types.values() {
                        self.collect_tuple_types_from_resolved(ty);
                    }
                    for prop in &component.ast.props {
                        self.collect_tuple_types_from_ast(&prop.ty);
                    }
                }
                TypedItem::Actor(actor) => {
                    for ty in actor.state_types.values() {
                        self.collect_tuple_types_from_resolved(ty);
                    }
                    for state in &actor.ast.state {
                        self.collect_tuple_types_from_ast(&state.ty);
                    }
                    for handler in &actor.ast.handlers {
                        for param in &handler.params {
                            self.collect_tuple_types_from_ast(&param.ty);
                        }
                    }
                }
                TypedItem::Enum(en) => {
                    for payload_types in en.variant_payload_types.values() {
                        for ty in payload_types {
                            self.collect_tuple_types_from_resolved(ty);
                        }
                    }
                }
                TypedItem::Impl(imp) => {
                    self.collect_tuple_types_from_ast(&imp.ast.target_type);
                    for method in &imp.ast.methods {
                        for param in &method.params {
                            self.collect_tuple_types_from_ast(&param.ty);
                        }
                        if let Some(ret) = &method.return_type {
                            self.collect_tuple_types_from_ast(ret);
                        }
                    }
                }
                _ => {}
            }
        }
    }

    fn map_type_from_ast(&self, ty: &kain_core::ast::Type) -> String {
        match ty {
            kain_core::ast::Type::Named { name, .. } => self.map_type_from_str(name),
            kain_core::ast::Type::Tuple(items, _) => {
                let field_tys = items
                    .iter()
                    .map(|item| self.map_type_from_ast(item))
                    .collect::<Vec<_>>();
                Self::tuple_struct_ptr_type_from_types(&field_tys)
            }
            kain_core::ast::Type::Array(_, _, _) => "i8*".into(),
            kain_core::ast::Type::Slice(_, _) => "i8*".into(),
            kain_core::ast::Type::Ref { inner, .. } => {
                format!("{}*", self.map_type_from_ast(inner))
            }
            kain_core::ast::Type::Ptr { inner, .. } => {
                format!("{}*", self.map_type_from_ast(inner))
            }
            kain_core::ast::Type::Option(inner, _) => self.map_type_from_ast(inner),
            kain_core::ast::Type::Result(ok, _, _) => self.map_type_from_ast(ok),
            kain_core::ast::Type::Unit(_) => "void".into(),
            kain_core::ast::Type::Never(_) => "void".into(),
            _ => "i64".into(),
        }
    }

    fn map_type_from_str(&self, name: &str) -> String {
        match name {
            "Int" | "i64" => "i64".into(),
            "Float" | "f64" | "double" => "double".into(),
            "Bool" | "bool" => "i1".into(),
            "String" | "str" => "i8*".into(),
            "Unit" | "()" | "void" => "void".into(),
            _ => {
                // Check if it's a known struct/enum
                if self.struct_defs.contains_key(name) {
                    format!("%{}*", name)
                } else {
                    "i64".into()
                }
            }
        }
    }

    fn map_type(&self, ty: &kain_core::types::ResolvedType) -> String {
        use kain_core::types::ResolvedType;
        match ty {
            ResolvedType::Int(_) => "i64".into(),
            ResolvedType::Float(_) => "double".into(),
            ResolvedType::Bool => "i1".into(),
            ResolvedType::String => "i8*".into(),
            ResolvedType::Unit => "void".into(),
            ResolvedType::Char => "i8".into(),
            ResolvedType::Struct(name, _) => {
                if self.struct_defs.contains_key(name) {
                    format!("%{}*", name)
                } else {
                    self.map_type_from_str(name)
                }
            }
            ResolvedType::Enum(name, _) => format!("%{}*", name),
            ResolvedType::Array(_, _) => "i64".into(), // Arrays are opaque pointers for now
            ResolvedType::Slice(_) => "i64".into(),
            ResolvedType::Option(inner) => self.map_type(inner),
            ResolvedType::Result(ok, _) => self.map_type(ok),
            ResolvedType::Function { .. } => "i64".into(), // Function pointers
            ResolvedType::Generic(name) => self.map_type_from_str(name),
            ResolvedType::Tuple(items) => {
                let field_tys = items
                    .iter()
                    .map(|item| self.map_type(item))
                    .collect::<Vec<_>>();
                Self::tuple_struct_ptr_type_from_types(&field_tys)
            }
            ResolvedType::Ref { inner, .. } => self.map_type(inner),
            ResolvedType::Ptr { inner, .. } => format!("{}*", self.map_type(inner)),
            ResolvedType::Never => "void".into(),
            ResolvedType::Unknown => "i64".into(),
        }
    }

    fn compile_string_literal(&mut self, s: &str) -> (String, String) {
        let global_name = if let Some(name) = self.strings.get(s) {
            name.clone()
        } else {
            let name = format!("@.str.{}", self.string_counter);
            self.string_counter += 1;
            self.strings.insert(s.to_string(), name.clone());
            name
        };

        let reg_static = self.next_reg();
        let len = s.len() + 1;
        self.emit(&format!(
            "  {} = getelementptr inbounds [{} x i8], [{} x i8]* {}, i64 0, i64 0",
            reg_static, len, len, global_name
        ));

        let reg_rc = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @string_new(i8* {})",
            reg_rc, reg_static
        ));
        (reg_rc, "i8*".to_string())
    }

    fn concat_strings(&mut self, lhs: &str, rhs: &str) -> String {
        let res = self.next_reg();
        self.emit(&format!(
            "  {} = call i8* @str_concat(i8* {}, i8* {})",
            res, lhs, rhs
        ));
        res
    }

    fn stringify_value(&mut self, val: &str, ty: &str) -> KainResult<(String, String)> {
        match ty {
            "i8*" => Ok((val.to_string(), "i8*".to_string())),
            "i64" | "i8" => {
                let widened = if ty == "i64" {
                    val.to_string()
                } else {
                    let reg = self.next_reg();
                    self.emit(&format!("  {} = sext i8 {} to i64", reg, val));
                    reg
                };
                let res = self.next_reg();
                self.emit(&format!("  {} = call i8* @to_string(i64 {})", res, widened));
                Ok((res, "i8*".to_string()))
            }
            "i1" => {
                let widened = self.next_reg();
                self.emit(&format!("  {} = zext i1 {} to i64", widened, val));
                let res = self.next_reg();
                self.emit(&format!("  {} = call i8* @to_string(i64 {})", res, widened));
                Ok((res, "i8*".to_string()))
            }
            "double" => {
                let narrowed = self.next_reg();
                self.emit(&format!("  {} = fptosi double {} to i64", narrowed, val));
                let res = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @to_string(i64 {})",
                    res, narrowed
                ));
                Ok((res, "i8*".to_string()))
            }
            _ if ty.starts_with('%') => Ok(self.compile_string_literal("<value>")),
            _ => Ok(self.compile_string_literal("<value>")),
        }
    }

    fn zero_value_for_ty(&self, ty: &str) -> String {
        match ty {
            "double" => "0.0".into(),
            "i1" | "i8" | "i64" => "0".into(),
            "void" => "0".into(),
            _ if ty.ends_with('*') => "null".into(),
            _ => "0".into(),
        }
    }

    fn compile_expr_for_target_type(
        &mut self,
        expr: &Expr,
        target_ty: &str,
    ) -> KainResult<(String, String)> {
        if matches!(expr, Expr::None(_)) {
            Ok((self.zero_value_for_ty(target_ty), target_ty.to_string()))
        } else {
            self.compile_expr(expr)
        }
    }

    fn coerce_to_i64_storage(&mut self, val: &str, ty: &str) -> String {
        match ty {
            "i64" => val.to_string(),
            "i1" => {
                let reg = self.next_reg();
                self.emit(&format!("  {} = zext i1 {} to i64", reg, val));
                reg
            }
            "i8" => {
                let reg = self.next_reg();
                self.emit(&format!("  {} = sext i8 {} to i64", reg, val));
                reg
            }
            "double" => {
                let reg = self.next_reg();
                self.emit(&format!("  {} = fptosi double {} to i64", reg, val));
                reg
            }
            _ if ty.ends_with('*') => {
                let reg = self.next_reg();
                self.emit(&format!("  {} = ptrtoint {} {} to i64", reg, ty, val));
                reg
            }
            _ => val.to_string(),
        }
    }

    fn cast_numeric_value(
        &mut self,
        val: String,
        src_ty: &str,
        dst_ty: &str,
    ) -> KainResult<String> {
        if src_ty == dst_ty {
            return Ok(val);
        }

        let reg = self.next_reg();
        match (src_ty, dst_ty) {
            ("i64", "double") => {
                self.emit(&format!("  {} = sitofp i64 {} to double", reg, val));
                Ok(reg)
            }
            ("i1", "double") => {
                self.emit(&format!("  {} = uitofp i1 {} to double", reg, val));
                Ok(reg)
            }
            ("i8", "double") => {
                self.emit(&format!("  {} = sitofp i8 {} to double", reg, val));
                Ok(reg)
            }
            ("i1", "i64") => {
                self.emit(&format!("  {} = zext i1 {} to i64", reg, val));
                Ok(reg)
            }
            ("i8", "i64") => {
                self.emit(&format!("  {} = sext i8 {} to i64", reg, val));
                Ok(reg)
            }
            ("double", "i64") => {
                self.emit(&format!("  {} = fptosi double {} to i64", reg, val));
                Ok(reg)
            }
            _ => Err(KainError::codegen(
                format!("Unsupported numeric cast from {} to {}", src_ty, dst_ty),
                kain_core::Span::default(),
            )),
        }
    }

    fn coerce_binary_operands(
        &mut self,
        lhs: String,
        lhs_ty: String,
        rhs: String,
        rhs_ty: String,
    ) -> KainResult<(String, String, String, String)> {
        if lhs_ty == rhs_ty {
            return Ok((lhs, lhs_ty, rhs, rhs_ty));
        }

        if lhs_ty == "double" {
            let rhs_cast = self.cast_numeric_value(rhs, &rhs_ty, "double")?;
            return Ok((lhs, lhs_ty, rhs_cast, "double".to_string()));
        }

        if rhs_ty == "double" {
            let lhs_cast = self.cast_numeric_value(lhs, &lhs_ty, "double")?;
            return Ok((lhs_cast, "double".to_string(), rhs, rhs_ty));
        }

        if lhs_ty == "i64" {
            let rhs_cast = self.cast_numeric_value(rhs, &rhs_ty, "i64")?;
            return Ok((lhs, lhs_ty, rhs_cast, "i64".to_string()));
        }

        if rhs_ty == "i64" {
            let lhs_cast = self.cast_numeric_value(lhs, &lhs_ty, "i64")?;
            return Ok((lhs_cast, "i64".to_string(), rhs, rhs_ty));
        }

        Ok((lhs, lhs_ty, rhs, rhs_ty))
    }

    fn compile_value_eq(
        &mut self,
        lhs: &str,
        lhs_ty: &str,
        rhs: &str,
        rhs_ty: &str,
        span: kain_core::Span,
    ) -> KainResult<String> {
        let (lhs, lhs_ty, rhs, rhs_ty) = self.coerce_binary_operands(
            lhs.to_string(),
            lhs_ty.to_string(),
            rhs.to_string(),
            rhs_ty.to_string(),
        )?;

        let res = self.next_reg();
        if lhs_ty == "i8*" || rhs_ty == "i8*" {
            self.emit(&format!(
                "  {} = call i1 @deep_eq(i8* {}, i8* {})",
                res, lhs, rhs
            ));
            return Ok(res);
        }

        match lhs_ty.as_str() {
            "double" => self.emit(&format!("  {} = fcmp oeq double {}, {}", res, lhs, rhs)),
            "i1" | "i8" | "i64" => {
                self.emit(&format!("  {} = icmp eq {} {}, {}", res, lhs_ty, lhs, rhs))
            }
            _ if lhs_ty.ends_with('*') => {
                self.emit(&format!("  {} = icmp eq {} {}, {}", res, lhs_ty, lhs, rhs))
            }
            _ => {
                return Err(KainError::codegen(
                    format!(
                        "Unsupported equality comparison between {} and {}",
                        lhs_ty, rhs_ty
                    ),
                    span,
                ));
            }
        }

        Ok(res)
    }

    fn compile_range_check(
        &mut self,
        val: &str,
        val_ty: &str,
        start: &Option<Box<Expr>>,
        end: &Option<Box<Expr>>,
        inclusive: bool,
        span: kain_core::Span,
    ) -> KainResult<String> {
        let mut checks = Vec::new();

        if let Some(lo) = start {
            let (lo_val, lo_ty) = self.compile_expr(lo)?;
            let (lhs, lhs_ty, rhs, _) =
                self.coerce_binary_operands(val.to_string(), val_ty.to_string(), lo_val, lo_ty)?;
            let cmp = self.next_reg();
            match lhs_ty.as_str() {
                "double" => self.emit(&format!("  {} = fcmp oge double {}, {}", cmp, lhs, rhs)),
                "i1" | "i8" | "i64" => {
                    self.emit(&format!("  {} = icmp sge {} {}, {}", cmp, lhs_ty, lhs, rhs))
                }
                _ => {
                    return Err(KainError::codegen(
                        format!("Unsupported range lower bound type {}", lhs_ty),
                        span,
                    ))
                }
            }
            checks.push(cmp);
        }

        if let Some(hi) = end {
            let (hi_val, hi_ty) = self.compile_expr(hi)?;
            let (lhs, lhs_ty, rhs, _) =
                self.coerce_binary_operands(val.to_string(), val_ty.to_string(), hi_val, hi_ty)?;
            let cmp = self.next_reg();
            match lhs_ty.as_str() {
                "double" => {
                    let op = if inclusive { "fcmp ole" } else { "fcmp olt" };
                    self.emit(&format!("  {} = {} double {}, {}", cmp, op, lhs, rhs));
                }
                "i1" | "i8" | "i64" => {
                    let op = if inclusive { "icmp sle" } else { "icmp slt" };
                    self.emit(&format!("  {} = {} {} {}, {}", cmp, op, lhs_ty, lhs, rhs));
                }
                _ => {
                    return Err(KainError::codegen(
                        format!("Unsupported range upper bound type {}", lhs_ty),
                        span,
                    ))
                }
            }
            checks.push(cmp);
        }

        if checks.is_empty() {
            return Ok("1".to_string());
        }

        let mut current = checks[0].clone();
        for check in checks.iter().skip(1) {
            let combined = self.next_reg();
            self.emit(&format!("  {} = and i1 {}, {}", combined, current, check));
            current = combined;
        }

        Ok(current)
    }

    fn compile_pattern_condition(
        &mut self,
        pattern: &Pattern,
        scrutinee_val: &str,
        scrutinee_ty: &str,
        enum_name: Option<&str>,
        span: kain_core::Span,
    ) -> KainResult<String> {
        match pattern {
            Pattern::Wildcard(_) | Pattern::Binding { .. } => Ok("1".to_string()),
            Pattern::Literal(expr) => {
                let (rhs, rhs_ty) = self.compile_expr(expr)?;
                self.compile_value_eq(scrutinee_val, scrutinee_ty, &rhs, &rhs_ty, span)
            }
            Pattern::Range {
                start,
                end,
                inclusive,
                ..
            } => {
                self.compile_range_check(scrutinee_val, scrutinee_ty, start, end, *inclusive, span)
            }
            Pattern::Or(items, _) => {
                let mut regs = Vec::new();
                for item in items {
                    regs.push(self.compile_pattern_condition(
                        item,
                        scrutinee_val,
                        scrutinee_ty,
                        enum_name,
                        span,
                    )?);
                }
                if regs.is_empty() {
                    return Ok("0".to_string());
                }
                let mut current = regs[0].clone();
                for reg in regs.iter().skip(1) {
                    let merged = self.next_reg();
                    self.emit(&format!("  {} = or i1 {}, {}", merged, current, reg));
                    current = merged;
                }
                Ok(current)
            }
            Pattern::Variant { variant, .. } => {
                let enum_name = enum_name.ok_or_else(|| {
                    KainError::codegen("Variant pattern requires an enum scrutinee", span)
                })?;
                let tag_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds %{}, {} {}, i32 0, i32 0",
                    tag_ptr, enum_name, scrutinee_ty, scrutinee_val
                ));
                let tag = self.next_reg();
                self.emit(&format!("  {} = load i64, i64* {}", tag, tag_ptr));
                let cmp = self.next_reg();
                self.emit(&format!(
                    "  {} = icmp eq i64 {}, {}",
                    cmp,
                    tag,
                    self.hash_message_tag(enum_name, variant)
                ));
                Ok(cmp)
            }
            other => Err(KainError::codegen(
                format!("Unsupported LLVM pattern condition: {:?}", other),
                span,
            )),
        }
    }

    fn bind_local_pattern_value(
        &mut self,
        pattern: &Pattern,
        val: String,
        ty: String,
    ) -> KainResult<()> {
        match pattern {
            Pattern::Wildcard(_) => Ok(()),
            Pattern::Binding { name, .. } => {
                let addr_reg = format!("%{}.addr_{}", name, self.reg_count);
                self.reg_count += 1;
                self.emit(&format!("  {} = alloca {}", addr_reg, ty));
                self.emit(&format!("  store {} {}, {}* {}", ty, val, ty, addr_reg));

                self.locals.insert(name.clone(), (addr_reg, ty));
                if let Some(scope) = self.scopes.last_mut() {
                    scope.push(name.clone());
                }
                Ok(())
            }
            Pattern::Tuple(patterns, span) => {
                let struct_name = ty
                    .strip_prefix('%')
                    .and_then(|name| name.strip_suffix('*'))
                    .ok_or_else(|| {
                        KainError::codegen(
                            format!("Tuple pattern requires tuple pointer value, got {}", ty),
                            *span,
                        )
                    })?
                    .to_string();

                let field_defs = self.struct_defs.get(&struct_name).cloned().ok_or_else(|| {
                    KainError::codegen(
                        format!("Unknown tuple storage type for pattern: {}", struct_name),
                        *span,
                    )
                })?;

                for (index, sub_pattern) in patterns.iter().enumerate() {
                    let field_ty = field_defs
                        .get(index)
                        .map(|(_, ty)| ty.clone())
                        .unwrap_or_else(|| "i64".to_string());
                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds %{}, {} {}, i32 0, i32 {}",
                        field_ptr, struct_name, ty, val, index
                    ));
                    let field_val = self.next_reg();
                    self.emit(&format!("  {} = load {}, {}* {}", field_val, field_ty, field_ty, field_ptr));
                    if field_ty == "i8*" {
                        self.emit(&format!("  call void @rc_retain(i8* {})", field_val));
                    }
                    self.bind_local_pattern_value(sub_pattern, field_val, field_ty)?;
                }
                Ok(())
            }
            Pattern::Struct { name, fields, span, .. } => {
                let struct_name = ty
                    .strip_prefix('%')
                    .and_then(|name| name.strip_suffix('*'))
                    .ok_or_else(|| {
                        KainError::codegen(
                            format!("Struct pattern requires struct pointer value, got {}", ty),
                            *span,
                        )
                    })?
                    .to_string();

                if &struct_name != name {
                    return Err(KainError::codegen(
                        format!("Struct pattern expected {}, got {}", name, struct_name),
                        *span,
                    ));
                }

                let field_defs = self.struct_defs.get(&struct_name).cloned().ok_or_else(|| {
                    KainError::codegen(
                        format!("Unknown struct storage type for pattern: {}", struct_name),
                        *span,
                    )
                })?;

                for (field_name, sub_pattern) in fields {
                    let (index, field_ty) = field_defs
                        .iter()
                        .enumerate()
                        .find(|(_, (name, _))| name == field_name)
                        .map(|(index, (_, ty))| (index, ty.clone()))
                        .ok_or_else(|| {
                            KainError::codegen(
                                format!("Unknown struct field '{}' on {}", field_name, struct_name),
                                *span,
                            )
                        })?;

                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds %{}, {} {}, i32 0, i32 {}",
                        field_ptr, struct_name, ty, val, index
                    ));
                    let field_val = self.next_reg();
                    self.emit(&format!("  {} = load {}, {}* {}", field_val, field_ty, field_ty, field_ptr));
                    if field_ty == "i8*" {
                        self.emit(&format!("  call void @rc_retain(i8* {})", field_val));
                    }
                    self.bind_local_pattern_value(sub_pattern, field_val, field_ty)?;
                }
                Ok(())
            }
            _ => Err(KainError::codegen(
                "Local pattern binding currently supports wildcard, binding, tuple, and struct patterns",
                kain_core::Span::default(),
            )),
        }
    }

    fn bind_variant_pattern_fields(
        &mut self,
        enum_name: &str,
        variant: &str,
        fields: &VariantPatternFields,
        scrutinee_val: &str,
        scrutinee_ty: &str,
        span: kain_core::Span,
    ) -> KainResult<()> {
        let payload_struct_name = format!("{}_{}", enum_name, variant);
        if !self.struct_defs.contains_key(&payload_struct_name) {
            return Ok(());
        }

        let payload_ty = format!("%{}", payload_struct_name);
        let payload_ptr_ty = format!("{}*", payload_ty);
        let payload_ptr_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds %{}, {} {}, i32 0, i32 1",
            payload_ptr_ptr, enum_name, scrutinee_ty, scrutinee_val
        ));
        let payload_void = self.next_reg();
        self.emit(&format!(
            "  {} = load i8*, i8** {}",
            payload_void, payload_ptr_ptr
        ));
        let payload_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast i8* {} to {}",
            payload_ptr, payload_void, payload_ptr_ty
        ));

        match fields {
            VariantPatternFields::Unit => Ok(()),
            VariantPatternFields::Tuple(patterns) => {
                let field_defs = self
                    .struct_defs
                    .get(&payload_struct_name)
                    .cloned()
                    .unwrap_or_default();
                for (index, pattern) in patterns.iter().enumerate() {
                    let field_ty = field_defs
                        .get(index)
                        .map(|(_, ty)| ty.clone())
                        .unwrap_or_else(|| "i64".to_string());
                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 {}",
                        field_ptr, payload_ty, payload_ptr_ty, payload_ptr, index
                    ));
                    let field_val = self.next_reg();
                    self.emit(&format!(
                        "  {} = load {}, {}* {}",
                        field_val, field_ty, field_ty, field_ptr
                    ));
                    self.bind_local_pattern_value(pattern, field_val, field_ty)?;
                }
                Ok(())
            }
            VariantPatternFields::Struct(named_patterns) => {
                let field_defs = self
                    .struct_defs
                    .get(&payload_struct_name)
                    .cloned()
                    .unwrap_or_default();
                for (field_name, pattern) in named_patterns {
                    let (index, field_ty) = field_defs
                        .iter()
                        .enumerate()
                        .find(|(_, (name, _))| name == field_name)
                        .map(|(index, (_, ty))| (index, ty.clone()))
                        .ok_or_else(|| {
                            KainError::codegen(
                                format!(
                                    "Unknown payload field '{}' for {}::{}",
                                    field_name, enum_name, variant
                                ),
                                span,
                            )
                        })?;
                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 {}",
                        field_ptr, payload_ty, payload_ptr_ty, payload_ptr, index
                    ));
                    let field_val = self.next_reg();
                    self.emit(&format!(
                        "  {} = load {}, {}* {}",
                        field_val, field_ty, field_ty, field_ptr
                    ));
                    self.bind_local_pattern_value(pattern, field_val, field_ty)?;
                }
                Ok(())
            }
        }
    }

    fn bind_match_pattern(
        &mut self,
        pattern: &Pattern,
        scrutinee_val: &str,
        scrutinee_ty: &str,
        enum_name: Option<&str>,
        span: kain_core::Span,
    ) -> KainResult<()> {
        match pattern {
            Pattern::Wildcard(_) => Ok(()),
            Pattern::Binding { .. } | Pattern::Tuple(_, _) | Pattern::Struct { .. } => self
                .bind_local_pattern_value(
                    pattern,
                    scrutinee_val.to_string(),
                    scrutinee_ty.to_string(),
                ),
            Pattern::Variant {
                variant, fields, ..
            } => {
                let enum_name = enum_name.ok_or_else(|| {
                    KainError::codegen("Variant pattern requires an enum scrutinee", span)
                })?;
                self.bind_variant_pattern_fields(
                    enum_name,
                    variant,
                    fields,
                    scrutinee_val,
                    scrutinee_ty,
                    span,
                )
            }
            Pattern::Or(_, _) | Pattern::Literal(_) | Pattern::Range { .. } => Ok(()),
            other => Err(KainError::codegen(
                format!("Unsupported LLVM match binding pattern: {:?}", other),
                span,
            )),
        }
    }

    fn ptr_struct_name<'a>(&self, ty: &'a str) -> Option<&'a str> {
        if ty.starts_with('%') && ty.ends_with('*') {
            Some(&ty[1..ty.len() - 1])
        } else {
            None
        }
    }

    fn field_index(&self, struct_name: &str, field: &str) -> Option<usize> {
        self.struct_defs
            .get(struct_name)?
            .iter()
            .position(|(name, _)| name == field)
    }

    fn compile_temporary_address(&mut self, expr: &Expr) -> KainResult<(String, String)> {
        let (val, ty) = self.compile_expr(expr)?;
        let addr = format!("%tmp.addr.{}", self.reg_count);
        self.reg_count += 1;
        self.emit(&format!("  {} = alloca {}", addr, ty));
        self.emit(&format!("  store {} {}, {}* {}", ty, val, ty, addr));
        Ok((addr, ty))
    }

    fn compile_index_address_from_compiled(
        &mut self,
        obj_val: &str,
        obj_ty: &str,
        idx_val: &str,
        span: kain_core::Span,
    ) -> KainResult<(String, String)> {
        if let Some(pointee_ty) = obj_ty.strip_suffix('*') {
            let field_ptr = self.next_reg();
            self.emit(&format!(
                "  {} = getelementptr inbounds {}, {} {}, i64 {}",
                field_ptr, pointee_ty, obj_ty, obj_val, idx_val
            ));
            Ok((field_ptr, pointee_ty.to_string()))
        } else if obj_ty == "i64" {
            let base_ptr = self.next_reg();
            self.emit(&format!(
                "  {} = inttoptr i64 {} to i64*",
                base_ptr, obj_val
            ));
            let field_ptr = self.next_reg();
            self.emit(&format!(
                "  {} = getelementptr inbounds i64, i64* {}, i64 {}",
                field_ptr, base_ptr, idx_val
            ));
            Ok((field_ptr, "i64".to_string()))
        } else {
            Err(KainError::codegen(
                format!("Indexing is not supported for LLVM type {}", obj_ty),
                span,
            ))
        }
    }

    fn compile_addressable_ptr(&mut self, expr: &Expr) -> KainResult<(String, String)> {
        match expr {
            Expr::Ident(name, span) => self
                .locals
                .get(name)
                .cloned()
                .map(|(addr, ty)| (addr, ty))
                .ok_or_else(|| KainError::codegen(format!("Undefined variable: {}", name), *span)),
            Expr::Field {
                object,
                field,
                span,
            } => {
                let (obj_val, obj_ty) = self.compile_expr(object)?;
                if let Some(struct_name) =
                    self.ptr_struct_name(&obj_ty).map(|name| name.to_string())
                {
                    if let Some(index) = self.field_index(&struct_name, field) {
                        let field_ty = self
                            .struct_defs
                            .get(&struct_name)
                            .and_then(|fields| fields.get(index))
                            .map(|(_, ty)| ty.clone())
                            .unwrap_or_else(|| "i64".to_string());
                        let field_ptr = self.next_reg();
                        self.emit(&format!(
                            "  {} = getelementptr inbounds %{}, {} {}, i32 0, i32 {}",
                            field_ptr, struct_name, obj_ty, obj_val, index
                        ));
                        Ok((field_ptr, field_ty))
                    } else {
                        Err(KainError::codegen(
                            format!("Unknown field '{}' on {}", field, struct_name),
                            *span,
                        ))
                    }
                } else {
                    Err(KainError::codegen(
                        "Field address requires a struct pointer",
                        *span,
                    ))
                }
            }
            Expr::Index {
                object,
                index,
                span,
            } => {
                let (obj_val, obj_ty) = self.compile_expr(object)?;
                let (idx_val, _) = self.compile_expr(index)?;
                if obj_ty == "i8*" {
                    Err(KainError::codegen(
                        "Runtime array indexing is not addressable in LLVM",
                        *span,
                    ))
                } else {
                    self.compile_index_address_from_compiled(&obj_val, &obj_ty, &idx_val, *span)
                }
            }
            _ => self.compile_temporary_address(expr),
        }
    }

    fn compile_lowered_helper_call(
        &mut self,
        func_name: &str,
        args: &[kain_core::ast::CallArg],
        span: kain_core::Span,
    ) -> Option<KainResult<(String, String)>> {
        match func_name {
            "__kain_bind_local" => {
                // Canonical ABI: i8* __kain_bind_local(i8* ptr)
                // Requirements: 1.4, 3.2
                if args.len() != 1 {
                    return Some(Err(KainError::codegen(
                        "__kain_bind_local expects 1 argument",
                        span,
                    )));
                }
                let (addr, ty) = match &args[0].value {
                    Expr::Ident(name, arg_span) => match self.locals.get(name).cloned() {
                        Some(pair) => pair,
                        None => {
                            return Some(Err(KainError::codegen(
                                format!("Undefined variable: {}", name),
                                *arg_span,
                            )))
                        }
                    },
                    other => match self.compile_temporary_address(other) {
                        Ok(pair) => pair,
                        Err(err) => return Some(Err(err)),
                    },
                };
                // Cast typed pointer to i8*
                let ptr_i8 = self.next_reg();
                self.emit(&format!("  {} = bitcast {}* {} to i8*", ptr_i8, ty, addr));
                // Call canonical helper
                let result = self.next_reg();
                self.emit(&format!("  {} = call i8* @__kain_bind_local(i8* {})", result, ptr_i8));
                // Convert back to i64 for compatibility with existing codegen
                let res = self.next_reg();
                self.emit(&format!("  {} = ptrtoint i8* {} to i64", res, result));
                Some(Ok((res, "i64".to_string())))
            }
            "__kain_addr_of" => {
                // Canonical ABI: i8* __kain_addr_of(i8* ptr, i64 size)
                // Requirements: 1.4, 3.2
                if args.len() < 1 {
                    return Some(Err(KainError::codegen(
                        "__kain_addr_of expects at least 1 argument",
                        span,
                    )));
                }
                let (addr, ty) = match self.compile_addressable_ptr(&args[0].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                // Cast typed pointer to i8*
                let ptr_i8 = self.next_reg();
                self.emit(&format!("  {} = bitcast {}* {} to i8*", ptr_i8, ty, addr));
                // Get size (if provided, otherwise use 8 as default)
                let size = if args.len() > 1 {
                    match self.compile_expr(&args[1].value) {
                        Ok((val, _)) => val,
                        Err(err) => return Some(Err(err)),
                    }
                } else {
                    "8".to_string()
                };
                // Call canonical helper
                let result = self.next_reg();
                self.emit(&format!("  {} = call i8* @__kain_addr_of(i8* {}, i64 {})", result, ptr_i8, size));
                // Convert to i64
                let res = self.next_reg();
                self.emit(&format!("  {} = ptrtoint i8* {} to i64", res, result));
                Some(Ok((res, "i64".to_string())))
            }
            "__kain_mem_load" => {
                // Canonical ABI: void __kain_mem_load(i8* ptr, i8* out, i64 size)
                // Requirements: 1.4, 3.2
                // Current implementation uses simplified inline load for i64
                // TODO: Align with canonical ABI when native runtime implements __kain_mem_load
                if args.len() != 1 {
                    return Some(Err(KainError::codegen(
                        "__kain_mem_load expects 1 argument",
                        span,
                    )));
                }
                let compiled = match self.compile_expr(&args[0].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let ptr_i64 = self.coerce_to_i64_storage(&compiled.0, &compiled.1);
                let typed_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = inttoptr i64 {} to i64*",
                    typed_ptr, ptr_i64
                ));
                let loaded = self.next_reg();
                self.emit(&format!("  {} = load i64, i64* {}", loaded, typed_ptr));
                Some(Ok((loaded, "i64".to_string())))
            }
            "__kain_mem_store" => {
                // Canonical ABI: void __kain_mem_store(i8* ptr, i8* value, i64 size)
                // Requirements: 1.4, 3.2
                // Current implementation uses simplified inline store
                // TODO: Align with canonical ABI when native runtime implements __kain_mem_store
                if args.len() != 2 {
                    return Some(Err(KainError::codegen(
                        "__kain_mem_store expects 2 arguments",
                        span,
                    )));
                }
                let compiled_ptr = match self.compile_expr(&args[0].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let ptr_i64 = self.coerce_to_i64_storage(&compiled_ptr.0, &compiled_ptr.1);
                let (val, val_ty) = match self.compile_expr(&args[1].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let typed_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = inttoptr i64 {} to {}*",
                    typed_ptr, ptr_i64, val_ty
                ));
                self.emit(&format!(
                    "  store {} {}, {}* {}",
                    val_ty, val, val_ty, typed_ptr
                ));
                Some(Ok((val, val_ty)))
            }
            "__kain_field_ptr" => {
                // Canonical ABI: i8* __kain_field_ptr(i8* ptr, const char* field, size_t offset)
                // Requirements: 1.4, 3.2
                if args.len() != 3 {
                    return Some(Err(KainError::codegen(
                        "__kain_field_ptr expects 3 arguments (ptr, field_name, offset)",
                        span,
                    )));
                }
                let compiled_base = match self.compile_expr(&args[0].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let base_i64 = self.coerce_to_i64_storage(&compiled_base.0, &compiled_base.1);
                
                // Get field name (for diagnostics, not used in calculation)
                let field_name = match &args[1].value {
                    Expr::String(s, _) => s.clone(),
                    _ => "unknown".to_string(),
                };
                let (field_str, _) = self.compile_string_literal(&field_name);
                
                let (offset, _) = match self.compile_expr(&args[2].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                
                // Cast base to i8*
                let base_ptr = self.next_reg();
                self.emit(&format!("  {} = inttoptr i64 {} to i8*", base_ptr, base_i64));
                
                // Call canonical helper
                let result = self.next_reg();
                self.emit(&format!("  {} = call i8* @__kain_field_ptr(i8* {}, i8* {}, i64 {})", 
                    result, base_ptr, field_str, offset));
                
                // Convert to i64
                let res = self.next_reg();
                self.emit(&format!("  {} = ptrtoint i8* {} to i64", res, result));
                Some(Ok((res, "i64".to_string())))
            }
            "__kain_index_ptr" => {
                // Canonical ABI: i8* __kain_index_ptr(i8* ptr, i64 index, i64 stride)
                // Requirements: 1.4, 3.2
                if args.len() != 3 {
                    return Some(Err(KainError::codegen(
                        "__kain_index_ptr expects 3 arguments (ptr, index, stride)",
                        span,
                    )));
                }
                let compiled_base = match self.compile_expr(&args[0].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let base_i64 = self.coerce_to_i64_storage(&compiled_base.0, &compiled_base.1);
                let (index, _) = match self.compile_expr(&args[1].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let (stride, _) = match self.compile_expr(&args[2].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                
                // Cast base to i8*
                let base_ptr = self.next_reg();
                self.emit(&format!("  {} = inttoptr i64 {} to i8*", base_ptr, base_i64));
                
                // Call canonical helper
                let result = self.next_reg();
                self.emit(&format!("  {} = call i8* @__kain_index_ptr(i8* {}, i64 {}, i64 {})", 
                    result, base_ptr, index, stride));
                
                // Convert to i64
                let res = self.next_reg();
                self.emit(&format!("  {} = ptrtoint i8* {} to i64", res, result));
                Some(Ok((res, "i64".to_string())))
            }
            "__kain_ptr_offset" => {
                // Canonical ABI: i8* __kain_ptr_offset(i8* ptr, i64 offset, i64 stride)
                // Requirements: 1.4, 3.2
                if args.len() != 3 {
                    return Some(Err(KainError::codegen(
                        "__kain_ptr_offset expects 3 arguments (ptr, offset, stride)",
                        span,
                    )));
                }
                let compiled_base = match self.compile_expr(&args[0].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let base_i64 = self.coerce_to_i64_storage(&compiled_base.0, &compiled_base.1);
                let (offset, _) = match self.compile_expr(&args[1].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let (stride, _) = match self.compile_expr(&args[2].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                
                // Cast base to i8*
                let base_ptr = self.next_reg();
                self.emit(&format!("  {} = inttoptr i64 {} to i8*", base_ptr, base_i64));
                
                // Call canonical helper
                let result = self.next_reg();
                self.emit(&format!("  {} = call i8* @__kain_ptr_offset(i8* {}, i64 {}, i64 {})", 
                    result, base_ptr, offset, stride));
                
                // Convert to i64
                let res = self.next_reg();
                self.emit(&format!("  {} = ptrtoint i8* {} to i64", res, result));
                Some(Ok((res, "i64".to_string())))
            }
            "__kain_alloc" => {
                // Canonical ABI: i8* __kain_alloc(i64 size, i64 stride, i32 zeroed)
                // Requirements: 1.4, 3.6
                if args.len() < 3 {
                    return Some(Err(KainError::codegen(
                        "__kain_alloc expects 3 arguments (size, stride, zeroed)",
                        span,
                    )));
                }
                let (size, _) = match self.compile_expr(&args[0].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let (stride, _) = match self.compile_expr(&args[1].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let (zeroed, _) = match self.compile_expr(&args[2].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                
                // Call canonical helper
                let result = self.next_reg();
                self.emit(&format!("  {} = call i8* @__kain_alloc(i64 {}, i64 {}, i32 {})", 
                    result, size, stride, zeroed));
                
                // Convert to i64
                let res = self.next_reg();
                self.emit(&format!("  {} = ptrtoint i8* {} to i64", res, result));
                Some(Ok((res, "i64".to_string())))
            }
            "__kain_realloc" => {
                // Canonical ABI: i8* __kain_realloc(i8* ptr, i64 size, i64 stride, i32 zeroed_new)
                // Requirements: 1.4, 3.6
                if args.len() < 4 {
                    return Some(Err(KainError::codegen(
                        "__kain_realloc expects 4 arguments (ptr, size, stride, zeroed_new)",
                        span,
                    )));
                }
                let (ptr, ptr_ty) = match self.compile_expr(&args[0].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let ptr_i64 = self.coerce_to_i64_storage(&ptr, &ptr_ty);
                let (size, _) = match self.compile_expr(&args[1].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let (stride, _) = match self.compile_expr(&args[2].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                let (zeroed_new, _) = match self.compile_expr(&args[3].value) {
                    Ok(pair) => pair,
                    Err(err) => return Some(Err(err)),
                };
                
                // Cast to i8*
                let ptr_i8 = self.next_reg();
                self.emit(&format!("  {} = inttoptr i64 {} to i8*", ptr_i8, ptr_i64));
                
                // Call canonical helper
                let result = self.next_reg();
                self.emit(&format!("  {} = call i8* @__kain_realloc(i8* {}, i64 {}, i64 {}, i32 {})", 
                    result, ptr_i8, size, stride, zeroed_new));
                
                // Convert to i64
                let res = self.next_reg();
                self.emit(&format!("  {} = ptrtoint i8* {} to i64", res, result));
                Some(Ok((res, "i64".to_string())))
            }
            _ => None,
        }
    }

    fn jsx_span(&self, node: &JSXNode) -> kain_core::Span {
        match node {
            JSXNode::Element { span, .. }
            | JSXNode::Text(_, span)
            | JSXNode::ComponentCall { span, .. }
            | JSXNode::For { span, .. }
            | JSXNode::If { span, .. }
            | JSXNode::Fragment(_, span) => *span,
            JSXNode::Expression(expr) => expr.span(),
        }
    }

    fn compile_jsx(&mut self, node: &JSXNode) -> KainResult<(String, String)> {
        match node {
            JSXNode::Text(text, _) => Ok(self.compile_string_literal(text)),
            JSXNode::Expression(expr) => {
                let (val, ty) = self.compile_expr(expr)?;
                self.stringify_value(&val, &ty)
            }
            JSXNode::Fragment(children, _) => {
                let (mut acc, _) = self.compile_string_literal("");
                for child in children {
                    let (child_val, _) = self.compile_jsx(child)?;
                    acc = self.concat_strings(&acc, &child_val);
                }
                Ok((acc, "i8*".to_string()))
            }
            JSXNode::Element {
                tag,
                attributes,
                children,
                ..
            } => {
                let (mut acc, _) = self.compile_string_literal(&format!("<{}", tag));
                for attr in attributes {
                    match &attr.value {
                        JSXAttrValue::String(value) => {
                            let (piece, _) = self
                                .compile_string_literal(&format!(" {}=\"{}\"", attr.name, value));
                            acc = self.concat_strings(&acc, &piece);
                        }
                        JSXAttrValue::Bool(true) => {
                            let (piece, _) =
                                self.compile_string_literal(&format!(" {}", attr.name));
                            acc = self.concat_strings(&acc, &piece);
                        }
                        JSXAttrValue::Bool(false) => {}
                        JSXAttrValue::Expr(expr) => {
                            let (prefix, _) =
                                self.compile_string_literal(&format!(" {}=\"", attr.name));
                            acc = self.concat_strings(&acc, &prefix);
                            let (value, ty) = self.compile_expr(expr)?;
                            let (value_str, _) = self.stringify_value(&value, &ty)?;
                            acc = self.concat_strings(&acc, &value_str);
                            let (suffix, _) = self.compile_string_literal("\"");
                            acc = self.concat_strings(&acc, &suffix);
                        }
                    }
                }
                let (open_end, _) = self.compile_string_literal(">");
                acc = self.concat_strings(&acc, &open_end);
                for child in children {
                    let (child_val, _) = self.compile_jsx(child)?;
                    acc = self.concat_strings(&acc, &child_val);
                }
                let (close, _) = self.compile_string_literal(&format!("</{}>", tag));
                acc = self.concat_strings(&acc, &close);
                Ok((acc, "i8*".to_string()))
            }
            JSXNode::ComponentCall {
                name,
                props,
                children,
                span,
            } => {
                let defs = self.component_defs.get(name).cloned().unwrap_or_default();
                let mut compiled_args = Vec::new();
                let mut arg_types = Vec::new();
                for (prop_name, prop_ty) in defs {
                    if let Some(prop) = props.iter().find(|prop| prop.name == prop_name) {
                        match &prop.value {
                            JSXAttrValue::String(value) => {
                                let (val, ty) = self.compile_string_literal(value);
                                compiled_args.push(val);
                                arg_types.push(if prop_ty == "i8*" {
                                    ty
                                } else {
                                    prop_ty.clone()
                                });
                            }
                            JSXAttrValue::Bool(value) => {
                                compiled_args.push(if *value { "1".into() } else { "0".into() });
                                arg_types.push(prop_ty.clone());
                            }
                            JSXAttrValue::Expr(expr) => {
                                let (val, ty) = self.compile_expr(expr)?;
                                compiled_args.push(val);
                                arg_types.push(ty);
                            }
                        }
                    } else {
                        compiled_args.push(self.zero_value_for_ty(&prop_ty));
                        arg_types.push(prop_ty.clone());
                    }
                }
                let (children_val, children_ty) =
                    self.compile_jsx(&JSXNode::Fragment(children.clone(), *span))?;
                compiled_args.push(children_val);
                arg_types.push(children_ty);
                let res = self.next_reg();
                let arg_str = compiled_args
                    .iter()
                    .zip(arg_types.iter())
                    .map(|(val, ty)| format!("{} {}", ty, val))
                    .collect::<Vec<_>>()
                    .join(", ");
                self.emit(&format!("  {} = call i8* @{}({})", res, name, arg_str));
                Ok((res, "i8*".to_string()))
            }
            JSXNode::For {
                binding,
                iter,
                body,
                ..
            } => {
                let (iter_val, iter_ty) = self.compile_expr(iter)?;
                let _ = binding;
                let _ = body;
                self.stringify_value(&iter_val, &iter_ty)
            }
            JSXNode::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                let then_span = self.jsx_span(then_branch);
                let expr = Expr::If {
                    condition: condition.clone(),
                    then_branch: Block {
                        stmts: vec![Stmt::Expr(Expr::JSX((**then_branch).clone(), then_span))],
                        span: then_span,
                    },
                    else_branch: else_branch.as_ref().map(|branch| {
                        let branch_span = self.jsx_span(branch);
                        Box::new(ElseBranch::Else(Block {
                            stmts: vec![Stmt::Expr(Expr::JSX((**branch).clone(), branch_span))],
                            span: branch_span,
                        }))
                    }),
                    span: then_span,
                };
                self.compile_expr(&expr)
            }
        }
    }

    fn hash_message_tag(&self, actor: &str, msg: &str) -> i64 {
        let s = format!("{}_{}", actor, msg);
        let mut hash: i64 = 5381;
        for c in s.bytes() {
            hash = ((hash << 5).wrapping_add(hash)) ^ (c as i64);
        }
        hash
    }

    fn compile_module(&mut self, program: &TypedProgram) -> KainResult<()> {
        // 1. Emit Header
        self.emit("; ModuleID = 'KAIN'");
        self.emit("source_filename = \"KAIN\"");
        self.emit(&format!(
            "target datalayout = \"{}\"",
            self.target.datalayout
        ));
        self.emit(&format!("target triple = \"{}\"", self.target.triple));
        self.emit("");

        self.collect_program_tuple_types(program);

        // 2a. Pre-scan Structs to register and emit definitions
        for item in &program.items {
            if let TypedItem::Struct(s) = item {
                let mut fields = Vec::new();
                for field in &s.ast.fields {
                    // We need to resolve type from field_types map
                    if let Some(res_ty) = s.field_types.get(&field.name) {
                        fields.push((field.name.clone(), self.map_type(res_ty)));
                    } else {
                        // Should not happen if typed correctly
                        fields.push((field.name.clone(), "i64".into()));
                    }
                }
                self.struct_defs.insert(s.ast.name.clone(), fields.clone());

                // Emit type definition
                let field_types: Vec<String> = fields.iter().map(|(_, t)| t.clone()).collect();
                self.emit(&format!(
                    "%{} = type {{ {} }}",
                    s.ast.name,
                    field_types.join(", ")
                ));
            } else if let TypedItem::Component(component) = item {
                let mut props = Vec::new();
                for prop in &component.ast.props {
                    if let Some(res_ty) = component.prop_types.get(&prop.name) {
                        props.push((prop.name.clone(), self.map_type(res_ty)));
                    } else {
                        props.push((prop.name.clone(), self.map_type_from_ast(&prop.ty)));
                    }
                }
                props.push(("children".to_string(), "i8*".to_string()));
                self.component_defs
                    .insert(component.ast.name.clone(), props.clone());
                self.functions
                    .insert(component.ast.name.clone(), "i8*".to_string());
            } else if let TypedItem::Actor(a) = item {
                let mut fields = Vec::new();
                // Mailbox is always field 0 (MessageQueue*)
                fields.push(("__mailbox".to_string(), "i8*".into()));

                for state in &a.ast.state {
                    if let Some(res_ty) = a.state_types.get(&state.name) {
                        fields.push((state.name.clone(), self.map_type(res_ty)));
                    } else {
                        fields.push((state.name.clone(), "i64".into()));
                    }
                }
                self.struct_defs.insert(a.ast.name.clone(), fields.clone());

                let field_types: Vec<String> = fields.iter().map(|(_, t)| t.clone()).collect();
                self.emit(&format!(
                    "%{} = type {{ {} }}",
                    a.ast.name,
                    field_types.join(", ")
                ));

                // Emit Message Payload Structs
                for handler in &a.ast.handlers {
                    let mut payload_fields = Vec::new();
                    let mut field_defs = Vec::new();
                    for param in &handler.params {
                        let p_ty = self.map_type_from_ast(&param.ty);
                        payload_fields.push(p_ty.clone());
                        field_defs.push((param.name.clone(), p_ty));
                    }
                    let msg_struct_name = format!("{}_{}", a.ast.name, handler.message_type);
                    self.struct_defs.insert(msg_struct_name.clone(), field_defs);
                    self.emit(&format!(
                        "%{} = type {{ {} }}",
                        msg_struct_name,
                        payload_fields.join(", ")
                    ));
                }
            } else if let TypedItem::Enum(e) = item {
                // Emit Enum definition: { tag, payload* }
                self.emit(&format!("%{} = type {{ i64, i8* }}", e.ast.name));

                // Emit Variant Payload Structs
                for (variant_name, payload_types) in &e.variant_payload_types {
                    if !payload_types.is_empty() {
                        let field_types: Vec<String> =
                            payload_types.iter().map(|t| self.map_type(t)).collect();
                        let struct_name = format!("{}_{}", e.ast.name, variant_name);
                        self.emit(&format!(
                            "%{} = type {{ {} }}",
                            struct_name,
                            field_types.join(", ")
                        ));

                        // Register payload struct fields for later lookup
                        let mut fields = Vec::new();
                        for (i, ty) in field_types.iter().enumerate() {
                            fields.push((format!("_{}", i), ty.clone()));
                        }
                        self.struct_defs.insert(struct_name, fields);
                    }
                }
            }
        }

        // 2b. Pre-scan functions to register return types
        for item in &program.items {
            if let TypedItem::Function(func) = item {
                if let ResolvedType::Function { ret, .. } = &func.resolved_type {
                    let mut ret_ty = self.map_type(ret);
                    // Heuristic: If void and not main, assume i64 (missing inference)
                    if ret_ty == "void" && func.ast.name != "main" {
                        ret_ty = "i64".into();
                    }
                    self.functions.insert(func.ast.name.clone(), ret_ty);
                }
            } else if let TypedItem::Impl(imp) = item {
                if let kain_core::ast::Type::Named { name, .. } = &imp.ast.target_type {
                    for method in &imp.ast.methods {
                        let mut ret_ty = method
                            .return_type
                            .as_ref()
                            .map(|ty| self.map_type_from_ast(ty))
                            .unwrap_or_else(|| "void".to_string());
                        if ret_ty == "void" {
                            ret_ty = "i64".to_string();
                        }
                        self.functions
                            .insert(format!("{}_{}", name, method.name), ret_ty);
                    }
                }
            }
        }

        // 2c. Register StdLib functions
        let stdlib = kain_core::stdlib::StdLib::new();
        for (name, func) in stdlib.functions {
            let ret_ty = self.map_type_from_str(func.return_type);
            self.functions.insert(name, ret_ty);
        }

        // 3. Emit External Declarations (stdlib)
        self.emit_externs();
        self.emit_runtime();

        // 4. Compile Items
        for item in &program.items {
            match item {
                TypedItem::Function(func) => self.compile_function(func)?,
                TypedItem::Component(component) => self.compile_component(component)?,
                TypedItem::Impl(imp) => self.compile_impl(imp)?,
                TypedItem::Actor(actor) => self.compile_actor(actor)?,
                // TODO: Handle Structs, Enums, Consts
                _ => {}
            }
        }

        // 5. Emit String Constants
        // Clone strings to avoid borrow issues
        let strings: Vec<(String, String)> = self
            .strings
            .iter()
            .map(|(k, v)| (k.clone(), v.clone()))
            .collect();
        for (content, name) in strings {
            let len = content.len() + 1;
            // Escape string content for LLVM (simplified)
            // LLVM expects \xx for hex bytes.
            let mut escaped = String::new();
            for b in content.bytes() {
                if b >= 32 && b < 127 && b != b'"' && b != b'\\' {
                    escaped.push(b as char);
                } else {
                    escaped.push_str(&format!("\\{:02X}", b));
                }
            }
            escaped.push_str("\\00"); // Null terminator

            self.emit(&format!(
                "{} = private unnamed_addr constant [{} x i8] c\"{}\", align 1",
                name, len, escaped
            ));
        }

        // 6. Emit Struct Destructors
        self.emit_struct_destructors();

        Ok(())
    }

    fn compile_actor(&mut self, actor: &kain_core::types::TypedActor) -> KainResult<()> {
        let name = &actor.ast.name;
        let struct_ty = format!("%{}", name);

        // Generate Run Loop Function
        self.emit(&format!("define void @{}_run(i8* %arg) {{", name));
        self.emit_label("entry");

        // Cast arg to Actor*
        let self_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = bitcast i8* %arg to {}*",
            self_ptr, struct_ty
        ));

        // Get Mailbox
        let mailbox_ptr = self.next_reg();
        self.emit(&format!(
            "  {} = getelementptr inbounds {}, {}* {}, i32 0, i32 0",
            mailbox_ptr, struct_ty, struct_ty, self_ptr
        ));
        let mailbox = self.next_reg();
        self.emit(&format!("  {} = load i8*, i8** {}", mailbox, mailbox_ptr));

        // Loop
        let label_loop = self.next_label();
        let label_process = self.next_label();
        let label_sleep = self.next_label();

        self.emit(&format!("  br label %{}", label_loop));
        self.emit_label(&label_loop);

        // Prepare mq_pop args
        let tag_ptr = self.next_reg();
        self.emit(&format!("  {} = alloca i64", tag_ptr));
        let data_ptr = self.next_reg();
        self.emit(&format!("  {} = alloca i8*", data_ptr));

        let pop_res = self.next_reg();
        self.emit(&format!(
            "  {} = call i32 @mq_pop(i8* {}, i64* {}, i8** {})",
            pop_res, mailbox, tag_ptr, data_ptr
        ));

        let cond = self.next_reg();
        self.emit(&format!("  {} = icmp ne i32 {}, 0", cond, pop_res));
        self.emit(&format!(
            "  br i1 {}, label %{}, label %{}",
            cond, label_process, label_sleep
        ));

        // Sleep
        self.emit_label(&label_sleep);
        self.emit("  call void @KAIN_sleep(double 0.001)");
        self.emit(&format!("  br label %{}", label_loop));

        // Process
        self.emit_label(&label_process);
        let tag_val = self.next_reg();
        self.emit(&format!("  {} = load i64, i64* {}", tag_val, tag_ptr));

        // Switch
        let mut handler_labels = Vec::new();
        for _ in &actor.ast.handlers {
            handler_labels.push(self.next_label());
        }

        let mut switch_cases = String::new();
        for (i, handler) in actor.ast.handlers.iter().enumerate() {
            let tag = self.hash_message_tag(name, &handler.message_type);
            switch_cases.push_str(&format!("i64 {}, label %{} ", tag, handler_labels[i]));
        }
        self.emit(&format!(
            "  switch i64 {}, label %{} [ {} ]",
            tag_val, label_loop, switch_cases
        ));

        // Generate Handler Bodies
        for (i, handler) in actor.ast.handlers.iter().enumerate() {
            self.emit_label(&handler_labels[i]);

            // Extract Payload
            let payload_void = self.next_reg();
            self.emit(&format!("  {} = load i8*, i8** {}", payload_void, data_ptr));

            let msg_struct_name = format!("{}_{}", name, handler.message_type);
            let msg_struct_ty = format!("%{}", msg_struct_name);
            let payload = self.next_reg();
            self.emit(&format!(
                "  {} = bitcast i8* {} to {}*",
                payload, payload_void, msg_struct_ty
            ));

            // Setup Scope
            self.scopes.push(Vec::new());
            self.locals.clear();
            self.locals
                .insert("self".to_string(), (self_ptr.clone(), struct_ty.clone()));

            // Map params
            for (j, param) in handler.params.iter().enumerate() {
                let p_ty = self.map_type_from_ast(&param.ty);
                let field_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds {}, {}* {}, i32 0, i32 {}",
                    field_ptr, msg_struct_ty, msg_struct_ty, payload, j
                ));
                let val = self.next_reg();
                self.emit(&format!(
                    "  {} = load {}, {}* {}",
                    val, p_ty, p_ty, field_ptr
                ));

                let addr_reg = format!("%{}.addr", param.name);
                self.emit(&format!("  {} = alloca {}", addr_reg, p_ty));
                self.emit(&format!("  store {} {}, {}* {}", p_ty, val, p_ty, addr_reg));
                self.locals.insert(param.name.clone(), (addr_reg, p_ty));
                if let Some(scope) = self.scopes.last_mut() {
                    scope.push(param.name.clone());
                }
            }

            // Compile Body
            self.compile_block(&handler.body)?;

            // Free payload
            self.emit(&format!("  call void @rc_release(i8* {})", payload_void));

            self.emit(&format!("  br label %{}", label_loop));
        }

        self.emit("}");
        self.emit("");
        self.current_return_type = None;
        Ok(())
    }

    fn emit_runtime(&mut self) {
        // Runtime implemented in C (KAIN_runtime.c)
    }

    fn emit_externs(&mut self) {
        // Core Runtime
        self.emit("declare void @print_i64(i64)");
        self.emit("declare void @print_f64(double)");
        self.emit("declare void @print_bool(i1)");
        self.emit("declare void @print_str(i8*, i64)");
        self.emit("declare i8* @to_string(i64)");
        self.emit("declare i8* @str_concat(i8*, i8*)");
        self.emit("declare i64 @clock_wrapper()");
        self.emit("declare i8* @KAIN_alloc(i64)");
        self.emit("declare void @rc_retain(i8*)");
        self.emit("declare void @rc_release(i8*)");
        self.emit("declare i8* @string_new(i8*)");
        self.emit("declare i8* @array_new(i64)");
        self.emit("declare void @array_push(i8*, i64)");
        self.emit("declare i64 @array_get(i8*, i64)");
        self.emit("declare void @array_set(i8*, i64, i64)");
        self.emit("declare i64 @array_len(i8*)");

        // Message Queue & Concurrency
        self.emit("declare i8* @mq_new()");
        self.emit("declare void @mq_push(i8*, i64, i8*)");
        self.emit("declare i32 @mq_pop(i8*, i64*, i8**)");
        self.emit("declare void @KAIN_spawn(i8*, i8*)");
        self.emit("declare void @KAIN_set_destructor(i8*, void(i8*)*)");
        self.emit("declare void @KAIN_sleep(double)");
        self.emit("declare i1 @deep_eq(i8*, i8*)");

        // Low-Level Memory Helpers (Canonical ABI)
        // Source: runtime/native/include/kain_runtime_memory.h
        // Requirements: 1.4, 3.1, 3.4, 3.5
        self.emit("");
        self.emit("; Low-Level Memory Helper Surface");
        self.emit("; Category 1: Pointer and Address Operations");
        self.emit("declare i8* @__kain_bind_local(i8*)");
        self.emit("declare i8* @__kain_addr_of(i8*, i64)");
        self.emit("declare i8* @__kain_ptr_offset(i8*, i64, i64)");
        self.emit("declare i8* @__kain_field_ptr(i8*, i8*, i64)");
        self.emit("declare i8* @__kain_index_ptr(i8*, i64, i64)");
        self.emit("");
        self.emit("; Category 2: Memory Load/Store Operations");
        self.emit("declare void @__kain_mem_load(i8*, i8*, i64)");
        self.emit("declare void @__kain_mem_store(i8*, i8*, i64)");
        self.emit("");
        self.emit("; Category 3: Allocation Operations");
        self.emit("declare i8* @__kain_alloc(i64, i64, i32)");
        self.emit("declare i8* @__kain_realloc(i8*, i64, i64, i32)");

        // StdLib
        self.emit_stdlib_externs();
    }

    fn emit_stdlib_externs(&mut self) {
        let stdlib = kain_core::stdlib::StdLib::new();
        // Skip functions that conflict with manual runtime declarations or are handled specially
        let skip_list = ["print", "println", "to_string"];

        for (name, func) in stdlib.functions {
            if skip_list.contains(&name.as_str()) {
                continue;
            }

            let ret_ty = self.map_type_from_str(func.return_type);
            let mut param_tys = Vec::new();
            for (_, p_ty) in func.params {
                param_tys.push(self.map_type_from_str(p_ty));
            }
            let runtime_symbol = runtime_symbol_for_stdlib_function(&name);
            self.emit(&format!(
                "declare {} @{}({})",
                ret_ty,
                runtime_symbol,
                param_tys.join(", ")
            ));
        }
    }

    fn emit_struct_destructors(&mut self) {
        let structs: Vec<(String, Vec<(String, String)>)> = self
            .struct_defs
            .iter()
            .map(|(k, v)| (k.clone(), v.clone()))
            .collect();

        for (name, fields) in structs {
            // Only generate if there are RC fields
            let has_rc_fields = fields
                .iter()
                .any(|(_, ty)| ty == "i8*" || ty.starts_with("%"));
            if !has_rc_fields {
                continue;
            }

            let struct_ty = format!("%{}", name);
            let dtor_name = format!("dtor_{}", name);

            self.emit(&format!("define void @{}(i8* %ptr_void) {{", dtor_name));
            self.emit_label("entry");

            let ptr_typed = self.next_reg();
            self.emit(&format!(
                "  {} = bitcast i8* %ptr_void to {}*",
                ptr_typed, struct_ty
            ));

            for (i, (_, field_ty)) in fields.iter().enumerate() {
                if field_ty == "i8*" || field_ty.starts_with("%") {
                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds {}, {}* {}, i32 0, i32 {}",
                        field_ptr, struct_ty, struct_ty, ptr_typed, i
                    ));
                    let loaded = self.next_reg();
                    self.emit(&format!(
                        "  {} = load {}, {}* {}",
                        loaded, field_ty, field_ty, field_ptr
                    ));

                    self.emit_release(&loaded, field_ty);
                }
            }

            self.emit("  ret void");
            self.emit("}");
        }
    }

    fn compile_component(&mut self, component: &TypedComponent) -> KainResult<()> {
        self.reg_count = 0;
        self.locals.clear();
        self.scopes.clear();
        self.scopes.push(Vec::new());

        let name = &component.ast.name;
        let defs = self.component_defs.get(name).cloned().unwrap_or_else(|| {
            let mut props = component
                .ast
                .props
                .iter()
                .map(|prop| {
                    let ty = component
                        .prop_types
                        .get(&prop.name)
                        .map(|ty| self.map_type(ty))
                        .unwrap_or_else(|| self.map_type_from_ast(&prop.ty));
                    (prop.name.clone(), ty)
                })
                .collect::<Vec<_>>();
            props.push(("children".to_string(), "i8*".to_string()));
            props
        });

        let param_str = defs
            .iter()
            .enumerate()
            .map(|(i, (_, ty))| format!("{} %arg{}", ty, i))
            .collect::<Vec<_>>()
            .join(", ");

        self.emit(&format!("define i8* @{}({}) {{", name, param_str));
        self.emit_label("entry");

        for (i, (param_name, param_ty)) in defs.iter().enumerate() {
            let addr_reg = format!("%{}.addr", param_name);
            self.emit(&format!("  {} = alloca {}", addr_reg, param_ty));
            self.emit(&format!(
                "  store {} %arg{}, {}* {}",
                param_ty, i, param_ty, addr_reg
            ));
            self.locals
                .insert(param_name.clone(), (addr_reg, param_ty.clone()));
            if let Some(scope) = self.scopes.last_mut() {
                scope.push(param_name.clone());
            }
        }

        for method in &component.ast.methods {
            let _ = method;
        }

        let (result, _) = self.compile_jsx(&component.ast.body)?;
        self.emit(&format!("  ret i8* {}", result));
        self.emit("}");
        self.emit("");
        self.current_return_type = None;
        Ok(())
    }

    fn compile_impl(&mut self, imp: &kain_core::types::TypedImpl) -> KainResult<()> {
        let target_name = match &imp.ast.target_type {
            kain_core::ast::Type::Named { name, .. } => name.as_str(),
            _ => return Ok(()),
        };

        for method in &imp.ast.methods {
            self.compile_impl_method(target_name, method)?;
        }

        Ok(())
    }

    fn compile_impl_method(
        &mut self,
        target_name: &str,
        method: &kain_core::ast::Function,
    ) -> KainResult<()> {
        self.reg_count = 0;
        self.locals.clear();
        self.scopes.clear();
        self.scopes.push(Vec::new());

        let self_ty = format!("%{}*", target_name);
        let mut ret_type = method
            .return_type
            .as_ref()
            .map(|ty| self.map_type_from_ast(ty))
            .unwrap_or_else(|| "void".to_string());
        if ret_type == "void" {
            ret_type = "i64".to_string();
        }
        self.current_return_type = Some(ret_type.clone());

        let mut params = Vec::new();
        params.push(format!("{} %arg0", self_ty));
        for (i, param) in method.params.iter().enumerate() {
            params.push(format!(
                "{} %arg{}",
                self.map_type_from_ast(&param.ty),
                i + 1
            ));
        }

        self.emit(&format!(
            "define {} @{}_{}({}) {{",
            ret_type,
            target_name,
            method.name,
            params.join(", ")
        ));
        self.emit_label("entry");

        let self_addr = "%self.addr".to_string();
        self.emit(&format!("  {} = alloca {}", self_addr, self_ty));
        self.emit(&format!(
            "  store {} %arg0, {}* {}",
            self_ty, self_ty, self_addr
        ));
        self.locals
            .insert("self".to_string(), (self_addr, self_ty.clone()));
        if let Some(scope) = self.scopes.last_mut() {
            scope.push("self".to_string());
        }

        for (i, param) in method.params.iter().enumerate() {
            let p_ty = self.map_type_from_ast(&param.ty);
            let addr_reg = format!("%{}.addr", param.name);
            self.emit(&format!("  {} = alloca {}", addr_reg, p_ty));
            self.emit(&format!(
                "  store {} %arg{}, {}* {}",
                p_ty,
                i + 1,
                p_ty,
                addr_reg
            ));
            self.locals.insert(param.name.clone(), (addr_reg, p_ty));
            if let Some(scope) = self.scopes.last_mut() {
                scope.push(param.name.clone());
            }
        }

        self.compile_block(&method.body)?;
        self.emit_scope_exit();

        if ret_type == "void" {
            self.emit("  ret void");
        } else {
            self.emit("  unreachable");
        }

        self.emit("}");
        self.emit("");
        self.current_return_type = None;
        Ok(())
    }

    fn compile_function(&mut self, func: &TypedFunction) -> KainResult<()> {
        self.reg_count = 0;
        self.locals.clear();
        self.scopes.clear();
        self.scopes.push(Vec::new()); // Top level scope for params

        let name = &func.ast.name;

        // Get param types and return type from resolved_type
        let (param_types, ret_type_resolved) =
            if let ResolvedType::Function { params, ret, .. } = &func.resolved_type {
                (params, ret)
            } else {
                return Err(KainError::codegen(
                    "Function has non-function type",
                    func.ast.span,
                ));
            };

        let mut ret_type = self.map_type(ret_type_resolved);
        // Heuristic: If void and not main, assume i64
        if ret_type == "void" && func.ast.name != "main" {
            ret_type = "i64".into();
        }
        self.current_return_type = Some(ret_type.clone());

        // Special case for main
        let (llvm_name, is_main) = if name == "main" {
            if ret_type == "void" {
                ret_type = "i64".into();
            }
            ("main", true)
        } else {
            (name.as_str(), false)
        };

        // Params
        let mut param_str = String::new();
        for (i, _) in func.ast.params.iter().enumerate() {
            if i > 0 {
                param_str.push_str(", ");
            }
            let p_ty = self.map_type(&param_types[i]);
            param_str.push_str(&format!("{} %arg{}", p_ty, i));
        }

        self.emit(&format!(
            "define {} @{}({}) {{",
            ret_type, llvm_name, param_str
        ));
        self.emit_label("entry");

        // Alloc parameters to stack (standard "alloca" pattern for debuggable IR)
        for (i, param) in func.ast.params.iter().enumerate() {
            let p_ty = self.map_type(&param_types[i]);
            // %param.addr = alloca type
            let addr_reg = format!("%{}.addr", param.name);
            self.emit(&format!("  {} = alloca {}", addr_reg, p_ty));
            self.emit(&format!(
                "  store {} %arg{}, {}* {}",
                p_ty, i, p_ty, addr_reg
            ));
            self.locals.insert(param.name.clone(), (addr_reg, p_ty));
            if let Some(scope) = self.scopes.last_mut() {
                scope.push(param.name.clone());
            }
        }

        // Compile Body
        self.compile_block(&func.ast.body)?;

        // Implicit return cleanup
        self.emit_scope_exit(); // Clean up params

        // Implicit return for void/Unit functions
        if ret_type == "void" {
            self.emit("  ret void");
        } else if is_main {
            // Main always returns 0 if not specified
            self.emit("  ret i64 0");
        } else {
            // Add a dummy return to ensure validity if flow analysis failed
            self.emit("  unreachable");
        }

        self.emit("}");
        self.emit("");
        Ok(())
    }

    fn compile_block(&mut self, block: &Block) -> KainResult<()> {
        self.scopes.push(Vec::new());
        for stmt in &block.stmts {
            self.compile_stmt(stmt)?;
        }
        self.emit_scope_exit();
        Ok(())
    }

    fn compile_block_with_result(&mut self, block: &Block) -> KainResult<Option<(String, String)>> {
        self.scopes.push(Vec::new());
        let mut last_res = None;
        let mut last_is_new = false;

        for (i, stmt) in block.stmts.iter().enumerate() {
            if i == block.stmts.len() - 1 {
                if let Stmt::Expr(expr) = stmt {
                    let (val, ty) = self.compile_expr(expr)?;
                    last_res = Some((val, ty));
                    last_is_new = self.is_new_object(expr);
                } else {
                    self.compile_stmt(stmt)?;
                }
            } else {
                self.compile_stmt(stmt)?;
            }
        }

        // If we are returning a value from the block, we must retain it before scope exit
        // destroys the local variables it might depend on.
        // Optimization: If the value is already a "new object" (owned with RC=1), we don't need to retain it
        // because no local variable owns it yet, so scope exit won't destroy it.
        if let Some((val, ty)) = &last_res {
            if ty == "i8*" && !last_is_new {
                self.emit(&format!("  call void @rc_retain(i8* {})", val));
            }
        }

        self.emit_scope_exit();
        Ok(last_res)
    }

    fn emit_release(&mut self, val: &str, ty: &str) {
        if ty == "i8*" {
            self.emit(&format!("  call void @rc_release(i8* {})", val));
        } else if ty.starts_with("%") {
            let struct_name = &ty[1..];
            // Clone fields to avoid borrowing self while emitting
            if let Some(fields) = self.struct_defs.get(struct_name).cloned() {
                for (i, (_, field_ty)) in fields.iter().enumerate() {
                    if field_ty == "i8*" || field_ty.starts_with("%") {
                        let field_val = self.next_reg();
                        self.emit(&format!(
                            "  {} = extractvalue {} {}, {}",
                            field_val, ty, val, i
                        ));
                        self.emit_release(&field_val, field_ty);
                    }
                }
            }
        }
    }

    fn emit_scope_exit(&mut self) {
        if let Some(vars) = self.scopes.pop() {
            for var_name in vars.iter().rev() {
                if let Some((addr, ty)) = self.locals.get(var_name).cloned() {
                    // Release if it's a pointer or struct
                    if ty == "i8*" || ty.starts_with("%") {
                        let tmp = self.next_reg();
                        self.emit(&format!("  {} = load {}, {}* {}", tmp, ty, ty, addr));
                        self.emit_release(&tmp, &ty);
                    }
                }
            }
        }
    }

    fn emit_all_scopes_cleanup(&mut self) {
        let mut vars_to_release = Vec::new();
        for scope in self.scopes.iter().rev() {
            for var in scope.iter().rev() {
                vars_to_release.push(var.clone());
            }
        }

        for var_name in vars_to_release {
            if let Some((addr, ty)) = self.locals.get(&var_name).cloned() {
                if ty == "i8*" || ty.starts_with("%") {
                    let tmp = self.next_reg();
                    self.emit(&format!("  {} = load {}, {}* {}", tmp, ty, ty, addr));
                    self.emit_release(&tmp, &ty);
                }
            }
        }
    }

    fn is_new_object(&self, expr: &Expr) -> bool {
        match expr {
            Expr::String(..) => true,
            Expr::Array(..) => true,
            Expr::Tuple(..) => true,
            Expr::Struct { .. } => true,
            Expr::Call { .. } => true, // Function calls return owned values
            Expr::Binary { op, .. } => *op == BinaryOp::Add, // String concat
            Expr::If { .. } => true,   // If expressions return new objects (Phi result)
            _ => false,
        }
    }

    fn compile_stmt(&mut self, stmt: &Stmt) -> KainResult<()> {
        match stmt {
            Stmt::Let {
                pattern, value, ty, ..
            } => {
                if let Some(val_expr) = value {
                    // Allocate and Store
                    if let kain_core::ast::Pattern::Binding { name, .. } = pattern {
                        let target_ty =
                            ty.as_ref().map(|declared| self.map_type_from_ast(declared));
                        let (val_reg, val_ty) = if let Some(target_ty) = target_ty.as_deref() {
                            self.compile_expr_for_target_type(val_expr, target_ty)?
                        } else {
                            self.compile_expr(val_expr)?
                        };
                        let addr_reg = format!("%{}.addr_{}", name, self.reg_count);
                        self.reg_count += 1;

                        self.emit(&format!("  {} = alloca {}", addr_reg, val_ty));
                        self.emit(&format!(
                            "  store {} {}, {}* {}",
                            val_ty, val_reg, val_ty, addr_reg
                        ));

                        // Retain if RC type AND it's not a new object (which already has RC=1)
                        if val_ty == "i8*" {
                            if !self.is_new_object(val_expr) {
                                self.emit(&format!("  call void @rc_retain(i8* {})", val_reg));
                            }
                        }

                        self.locals.insert(name.clone(), (addr_reg, val_ty));
                        if let Some(scope) = self.scopes.last_mut() {
                            scope.push(name.clone());
                        }
                    } else {
                        let (val_reg, val_ty) = self.compile_expr(val_expr)?;
                        self.bind_local_pattern_value(pattern, val_reg, val_ty)?;
                    }
                }
            }
            Stmt::Expr(expr) => {
                let (val, ty) = self.compile_expr(expr)?;
                // If it is a new object, and we are ignoring the result, release it.
                if (ty == "i8*" || ty.starts_with("%")) && self.is_new_object(expr) {
                    self.emit_release(&val, &ty);
                }
            }
            Stmt::Return(expr, _) => {
                if let Some(e) = expr {
                    let (val, ty) = if let Some(target_ty) = self.current_return_type.clone() {
                        self.compile_expr_for_target_type(e, &target_ty)?
                    } else {
                        self.compile_expr(e)?
                    };

                    if ty == "i8*" {
                        self.emit(&format!("  call void @rc_retain(i8* {})", val));
                    }

                    self.emit_all_scopes_cleanup();

                    self.emit(&format!("  ret {} {}", ty, val));
                } else {
                    self.emit_all_scopes_cleanup();
                    self.emit("  ret void");
                }
                // Terminate block to keep LLVM happy if there's dead code
                let dead_label = self.next_label();
                self.emit_label(&dead_label);
            }
            Stmt::Break(_, _) => {
                if let Some((_, break_label)) = self.loop_stack.last() {
                    self.emit(&format!("  br label %{}", break_label));
                    let dead_label = self.next_label();
                    self.emit_label(&dead_label);
                }
            }
            Stmt::Continue(_) => {
                if let Some((continue_label, _)) = self.loop_stack.last() {
                    self.emit(&format!("  br label %{}", continue_label));
                    let dead_label = self.next_label();
                    self.emit_label(&dead_label);
                }
            }
            Stmt::While {
                condition, body, ..
            } => {
                let label_cond = self.next_label();
                let label_body = self.next_label();
                let label_end = self.next_label();

                self.emit(&format!("  br label %{}", label_cond));
                self.emit_label(&label_cond);

                let (cond_val, _) = self.compile_expr(condition)?;
                self.emit(&format!(
                    "  br i1 {}, label %{}, label %{}",
                    cond_val, label_body, label_end
                ));

                self.emit_label(&label_body);

                self.loop_stack
                    .push((label_cond.clone(), label_end.clone()));
                self.compile_block(body)?;
                self.loop_stack.pop();

                self.emit(&format!("  br label %{}", label_cond));

                self.emit_label(&label_end);
            }
            Stmt::Loop { body, .. } => {
                let label_body = self.next_label();
                let label_end = self.next_label();

                self.emit(&format!("  br label %{}", label_body));
                self.emit_label(&label_body);

                self.loop_stack
                    .push((label_body.clone(), label_end.clone()));
                self.compile_block(body)?;
                self.loop_stack.pop();

                self.emit(&format!("  br label %{}", label_body));
                self.emit_label(&label_end);
            }
            Stmt::For {
                binding,
                iter,
                body,
                span,
            } => {
                // Determine start, end
                let (start_val, end_val) = match iter {
                    Expr::Call { callee, args, .. } => {
                        if let Expr::Ident(name, _) = callee.as_ref() {
                            if name == "range" && args.len() == 2 {
                                let (s, _) = self.compile_expr(&args[0].value)?;
                                let (e, _) = self.compile_expr(&args[1].value)?;
                                (s, e)
                            } else {
                                return Err(KainError::codegen(
                                    "Unsupported call in for loop",
                                    *span,
                                ));
                            }
                        } else {
                            return Err(KainError::codegen("Unsupported call in for loop", *span));
                        }
                    }
                    Expr::Range {
                        start,
                        end,
                        inclusive,
                        ..
                    } => {
                        let s = if let Some(e) = start {
                            self.compile_expr(e)?.0
                        } else {
                            "0".into()
                        };
                        let mut e = if let Some(e) = end {
                            self.compile_expr(e)?.0
                        } else {
                            "9223372036854775807".into()
                        };
                        if *inclusive {
                            let tmp = self.next_reg();
                            self.emit(&format!("  {} = add i64 {}, 1", tmp, e));
                            e = tmp;
                        }
                        (s, e)
                    }
                    _ => {
                        return Err(KainError::codegen(
                            "Unsupported iterator in for loop",
                            *span,
                        ))
                    }
                };

                // Allocate loop variable
                let loop_var = if let kain_core::ast::Pattern::Binding { name, .. } = binding {
                    name
                } else {
                    "it"
                };
                let var_addr = format!("%{}.addr_{}", loop_var, self.reg_count);
                self.reg_count += 1;
                self.emit(&format!("  {} = alloca i64", var_addr));
                self.emit(&format!("  store i64 {}, i64* {}", start_val, var_addr));
                self.locals
                    .insert(loop_var.to_string(), (var_addr.clone(), "i64".into()));

                let label_cond = self.next_label();
                let label_body = self.next_label();
                let label_step = self.next_label();
                let label_end = self.next_label();

                self.emit(&format!("  br label %{}", label_cond));
                self.emit_label(&label_cond);

                // Check condition: var < end
                let curr_val = self.next_reg();
                self.emit(&format!("  {} = load i64, i64* {}", curr_val, var_addr));
                let cond_res = self.next_reg();
                self.emit(&format!(
                    "  {} = icmp slt i64 {}, {}",
                    cond_res, curr_val, end_val
                ));
                self.emit(&format!(
                    "  br i1 {}, label %{}, label %{}",
                    cond_res, label_body, label_end
                ));

                self.emit_label(&label_body);

                self.loop_stack
                    .push((label_step.clone(), label_end.clone()));
                self.compile_block(body)?;
                self.loop_stack.pop();

                self.emit(&format!("  br label %{}", label_step));
                self.emit_label(&label_step);

                // Increment
                let val_before_inc = self.next_reg();
                self.emit(&format!(
                    "  {} = load i64, i64* {}",
                    val_before_inc, var_addr
                ));
                let val_after_inc = self.next_reg();
                self.emit(&format!(
                    "  {} = add i64 {}, 1",
                    val_after_inc, val_before_inc
                ));
                self.emit(&format!("  store i64 {}, i64* {}", val_after_inc, var_addr));

                self.emit(&format!("  br label %{}", label_cond));
                self.emit_label(&label_end);
            }
            _ => {}
        }
        Ok(())
    }

    fn compile_expr(&mut self, expr: &Expr) -> KainResult<(String, String)> {
        match expr {
            Expr::Int(n, _) => Ok((format!("{}", n), "i64".to_string())),
            Expr::Float(f, _) => Ok((format!("{:.6}", f), "double".to_string())),
            Expr::Bool(b, _) => Ok((if *b { "1".into() } else { "0".into() }, "i1".to_string())),
            Expr::String(s, _) => Ok(self.compile_string_literal(s)),
            Expr::FString(parts, _) => {
                let (mut acc, _) = self.compile_string_literal("");
                for part in parts {
                    let (val, ty) = self.compile_expr(part)?;
                    let (text, _) = self.stringify_value(&val, &ty)?;
                    acc = self.concat_strings(&acc, &text);
                }
                Ok((acc, "i8*".to_string()))
            }
            Expr::None(_) => Ok(("0".into(), "i64".to_string())),
            Expr::JSX(node, _) => self.compile_jsx(node),
            Expr::Paren(inner, _) => self.compile_expr(inner),
            Expr::Block(block, _) => self
                .compile_block_with_result(block)
                .map(|res| res.unwrap_or(("0".into(), "i64".into()))),
            Expr::Cast { value, target, .. } => {
                let (val, src_ty) = self.compile_expr(value)?;
                let dst_ty = self.map_type_from_ast(target);
                if src_ty == dst_ty {
                    Ok((val, dst_ty))
                } else if src_ty == "i64" && dst_ty == "double" {
                    let res = self.next_reg();
                    self.emit(&format!("  {} = sitofp i64 {} to double", res, val));
                    Ok((res, dst_ty))
                } else if src_ty == "double" && dst_ty == "i64" {
                    let res = self.next_reg();
                    self.emit(&format!("  {} = fptosi double {} to i64", res, val));
                    Ok((res, dst_ty))
                } else if src_ty == "i1" && dst_ty == "i64" {
                    let res = self.next_reg();
                    self.emit(&format!("  {} = zext i1 {} to i64", res, val));
                    Ok((res, dst_ty))
                } else if src_ty == "i64" && dst_ty == "i1" {
                    let res = self.next_reg();
                    self.emit(&format!("  {} = icmp ne i64 {}, 0", res, val));
                    Ok((res, dst_ty))
                } else if src_ty == "i64" && dst_ty.ends_with('*') {
                    let res = self.next_reg();
                    self.emit(&format!("  {} = inttoptr i64 {} to {}", res, val, dst_ty));
                    Ok((res, dst_ty))
                } else if src_ty.ends_with('*') && dst_ty == "i64" {
                    let res = self.next_reg();
                    self.emit(&format!("  {} = ptrtoint {} {} to i64", res, src_ty, val));
                    Ok((res, dst_ty))
                } else {
                    Ok((val, dst_ty))
                }
            }
            Expr::Ref { value, .. } => {
                let (addr, ty) = self.compile_addressable_ptr(value)?;
                Ok((addr, format!("{}*", ty)))
            }
            Expr::AddrOf { value, .. } => {
                let (addr, ty) = self.compile_addressable_ptr(value)?;
                Ok((addr, format!("{}*", ty)))
            }
            Expr::Deref(inner, span) => {
                let (val, ty) = self.compile_expr(inner)?;
                if let Some(pointee_ty) = ty.strip_suffix('*') {
                    let res = self.next_reg();
                    self.emit(&format!("  {} = load {}, {} {}", res, pointee_ty, ty, val));
                    Ok((res, pointee_ty.to_string()))
                } else {
                    Err(KainError::codegen(
                        "Cannot dereference non-pointer value",
                        *span,
                    ))
                }
            }
            Expr::PtrOffset {
                pointer,
                offset,
                element_ty,
                ..
            } => {
                let (base, base_ty) = self.compile_expr(pointer)?;
                let (off, _) = self.compile_expr(offset)?;
                let stride = element_ty
                    .as_ref()
                    .map(|ty| self.map_type_from_ast(ty))
                    .map(|ty| {
                        if ty == "double" {
                            8
                        } else if ty == "i8" {
                            1
                        } else if ty == "i1" {
                            1
                        } else {
                            8
                        }
                    })
                    .unwrap_or(8);
                let base_i64 = self.coerce_to_i64_storage(&base, &base_ty);
                let scaled = self.next_reg();
                self.emit(&format!("  {} = mul i64 {}, {}", scaled, off, stride));
                let res = self.next_reg();
                self.emit(&format!("  {} = add i64 {}, {}", res, base_i64, scaled));
                Ok((res, "i64".into()))
            }
            Expr::MemLoad { pointer, .. } => {
                let call = Expr::Call {
                    callee: Box::new(Expr::Ident("__kain_mem_load".to_string(), pointer.span())),
                    args: vec![kain_core::ast::CallArg {
                        name: None,
                        value: (**pointer).clone(),
                        span: pointer.span(),
                    }],
                    span: pointer.span(),
                };
                self.compile_expr(&call)
            }
            Expr::MemStore { pointer, value, .. } => {
                let call = Expr::Call {
                    callee: Box::new(Expr::Ident("__kain_mem_store".to_string(), value.span())),
                    args: vec![
                        kain_core::ast::CallArg {
                            name: None,
                            value: (**pointer).clone(),
                            span: pointer.span(),
                        },
                        kain_core::ast::CallArg {
                            name: None,
                            value: (**value).clone(),
                            span: value.span(),
                        },
                    ],
                    span: value.span(),
                };
                self.compile_expr(&call)
            }
            Expr::SizeOfType { target, .. } => {
                let mapped = self.map_type_from_ast(target);
                let size = if mapped == "double" {
                    8
                } else if mapped == "i8" {
                    1
                } else if mapped == "i1" {
                    1
                } else {
                    8
                };
                Ok((size.to_string(), "i64".into()))
            }
            Expr::AlignOfType { target, .. } => {
                let mapped = self.map_type_from_ast(target);
                let align = if mapped == "double" {
                    8
                } else if mapped == "i8" {
                    1
                } else if mapped == "i1" {
                    1
                } else {
                    8
                };
                Ok((align.to_string(), "i64".into()))
            }
            Expr::Alloca { ty, .. } => {
                let ty_str = self.map_type_from_ast(ty);
                let addr = self.next_reg();
                self.emit(&format!("  {} = alloca {}", addr, ty_str));
                Ok((addr, format!("{}*", ty_str)))
            }
            Expr::Uninit { ty, .. } => Ok((
                self.zero_value_for_ty(&self.map_type_from_ast(ty)),
                self.map_type_from_ast(ty),
            )),
            Expr::Alloc { size, .. } => {
                let call = Expr::Call {
                    callee: Box::new(Expr::Ident("__kain_alloc".to_string(), size.span())),
                    args: vec![kain_core::ast::CallArg {
                        name: None,
                        value: (**size).clone(),
                        span: size.span(),
                    }],
                    span: size.span(),
                };
                self.compile_expr(&call)
            }
            Expr::Realloc { pointer, size, .. } => {
                let call = Expr::Call {
                    callee: Box::new(Expr::Ident("__kain_realloc".to_string(), size.span())),
                    args: vec![
                        kain_core::ast::CallArg {
                            name: None,
                            value: (**pointer).clone(),
                            span: pointer.span(),
                        },
                        kain_core::ast::CallArg {
                            name: None,
                            value: (**size).clone(),
                            span: size.span(),
                        },
                    ],
                    span: size.span(),
                };
                self.compile_expr(&call)
            }
            Expr::Unary { op, operand, span } => {
                let (val, ty) = self.compile_expr(operand)?;
                match op {
                    UnaryOp::Neg => {
                        let res = self.next_reg();
                        if ty == "double" {
                            self.emit(&format!("  {} = fneg double {}", res, val));
                        } else {
                            self.emit(&format!("  {} = sub {} 0, {}", res, ty, val));
                        }
                        Ok((res, ty))
                    }
                    UnaryOp::Not => {
                        let res = self.next_reg();
                        self.emit(&format!("  {} = xor i1 {}, 1", res, val));
                        Ok((res, "i1".into()))
                    }
                    UnaryOp::BitNot => {
                        let res = self.next_reg();
                        self.emit(&format!("  {} = xor {} {}, -1", res, ty, val));
                        Ok((res, ty))
                    }
                    UnaryOp::Deref => {
                        if let Some(pointee_ty) = ty.strip_suffix('*') {
                            let res = self.next_reg();
                            self.emit(&format!("  {} = load {}, {} {}", res, pointee_ty, ty, val));
                            Ok((res, pointee_ty.to_string()))
                        } else {
                            Err(KainError::codegen(
                                "Cannot dereference non-pointer value",
                                *span,
                            ))
                        }
                    }
                    UnaryOp::Ref | UnaryOp::RefMut => Ok((val, format!("{}*", ty))),
                }
            }
            Expr::Field {
                object,
                field,
                span,
            } => {
                let (obj_val, obj_ty) = self.compile_expr(object)?;
                if let Some(struct_name) =
                    self.ptr_struct_name(&obj_ty).map(|name| name.to_string())
                {
                    if let Some(index) = self.field_index(&struct_name, field) {
                        let field_ty = self
                            .struct_defs
                            .get(&struct_name)
                            .and_then(|fields| fields.get(index))
                            .map(|(_, ty)| ty.clone())
                            .unwrap_or_else(|| "i64".to_string());
                        let field_ptr = self.next_reg();
                        self.emit(&format!(
                            "  {} = getelementptr inbounds %{}, {} {}, i32 0, i32 {}",
                            field_ptr, struct_name, obj_ty, obj_val, index
                        ));
                        let loaded = self.next_reg();
                        self.emit(&format!(
                            "  {} = load {}, {}* {}",
                            loaded, field_ty, field_ty, field_ptr
                        ));
                        Ok((loaded, field_ty))
                    } else {
                        Err(KainError::codegen(
                            format!("Unknown field '{}' on {}", field, struct_name),
                            *span,
                        ))
                    }
                } else {
                    Err(KainError::codegen(
                        "Field access requires a struct pointer",
                        *span,
                    ))
                }
            }
            Expr::Assign {
                target,
                value,
                span,
            } => match target.as_ref() {
                Expr::Ident(name, _) => {
                    if let Some((addr, ty)) = self.locals.get(name).cloned() {
                        let (rhs, rhs_ty) = self.compile_expr_for_target_type(value, &ty)?;
                        self.emit(&format!("  store {} {}, {}* {}", rhs_ty, rhs, ty, addr));
                        Ok((rhs, rhs_ty))
                    } else {
                        Err(KainError::codegen(
                            format!("Undefined assignment target: {}", name),
                            *span,
                        ))
                    }
                }
                Expr::Field { object, field, .. } => {
                    let (obj_val, obj_ty) = self.compile_expr(object)?;
                    if let Some(struct_name) =
                        self.ptr_struct_name(&obj_ty).map(|name| name.to_string())
                    {
                        if let Some(index) = self.field_index(&struct_name, field) {
                            let field_ty = self
                                .struct_defs
                                .get(&struct_name)
                                .and_then(|fields| fields.get(index))
                                .map(|(_, ty)| ty.clone())
                                .unwrap_or_else(|| "i64".to_string());
                            let (rhs, rhs_ty) =
                                self.compile_expr_for_target_type(value, &field_ty)?;
                            let field_ptr = self.next_reg();
                            self.emit(&format!(
                                "  {} = getelementptr inbounds %{}, {} {}, i32 0, i32 {}",
                                field_ptr, struct_name, obj_ty, obj_val, index
                            ));
                            self.emit(&format!(
                                "  store {} {}, {}* {}",
                                rhs_ty, rhs, field_ty, field_ptr
                            ));
                            Ok((rhs, rhs_ty))
                        } else {
                            Err(KainError::codegen(
                                format!("Unknown field '{}' on {}", field, struct_name),
                                *span,
                            ))
                        }
                    } else {
                        Err(KainError::codegen(
                            "Field assignment requires a struct pointer",
                            *span,
                        ))
                    }
                }
                Expr::Index { object, index, .. } => {
                    let (obj_val, obj_ty) = self.compile_expr(object)?;
                    let (idx_val, _) = self.compile_expr(index)?;
                    if obj_ty == "i8*" {
                        let (rhs, rhs_ty) = self.compile_expr(value)?;
                        let stored = self.coerce_to_i64_storage(&rhs, &rhs_ty);
                        self.emit(&format!(
                            "  call void @array_set(i8* {}, i64 {}, i64 {})",
                            obj_val, idx_val, stored
                        ));
                        Ok((rhs, rhs_ty))
                    } else {
                        let (field_ptr, field_ty) = self.compile_index_address_from_compiled(
                            &obj_val, &obj_ty, &idx_val, *span,
                        )?;
                        let (rhs, rhs_ty) = self.compile_expr_for_target_type(value, &field_ty)?;
                        self.emit(&format!(
                            "  store {} {}, {}* {}",
                            rhs_ty, rhs, field_ty, field_ptr
                        ));
                        Ok((rhs, rhs_ty))
                    }
                }
                _ => Err(KainError::codegen("Unsupported assignment target", *span)),
            },
            Expr::Struct {
                name,
                fields,
                rest,
                span,
            } => {
                if rest.is_some() {
                    return Err(KainError::codegen(
                        "Struct update syntax is not yet supported by LLVM codegen",
                        *span,
                    ));
                }
                let def = self.struct_defs.get(name).cloned().ok_or_else(|| {
                    KainError::codegen(format!("Unknown struct: {}", name), *span)
                })?;
                let struct_ty = format!("%{}", name);
                let ptr_ty = format!("{}*", struct_ty);
                let null_ptr = format!("{} null", ptr_ty);
                let size_ptr_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr {}, {}, i32 1",
                    size_ptr_reg, struct_ty, null_ptr
                ));
                let size_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = ptrtoint {} {} to i64",
                    size_reg, ptr_ty, size_ptr_reg
                ));
                let mem_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @KAIN_alloc(i64 {})",
                    mem_reg, size_reg
                ));
                let struct_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = bitcast i8* {} to {}",
                    struct_ptr, mem_reg, ptr_ty
                ));
                let mut provided: HashMap<String, Expr> = fields.iter().cloned().collect();
                for (i, (field_name, field_ty)) in def.iter().enumerate() {
                    let (val, val_ty) = if let Some(expr) = provided.remove(field_name) {
                        self.compile_expr(&expr)?
                    } else {
                        (self.zero_value_for_ty(field_ty), field_ty.clone())
                    };
                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 {}",
                        field_ptr, struct_ty, ptr_ty, struct_ptr, i
                    ));
                    self.emit(&format!(
                        "  store {} {}, {}* {}",
                        val_ty, val, val_ty, field_ptr
                    ));
                }
                Ok((struct_ptr, ptr_ty))
            }
            Expr::AggregateInit {
                ty, fields, span, ..
            } => match ty {
                kain_core::ast::Type::Named { name, .. } => self.compile_expr(&Expr::Struct {
                    name: name.clone(),
                    fields: fields.clone(),
                    rest: None,
                    span: *span,
                }),
                kain_core::ast::Type::Tuple(_, _) => self.compile_expr(&Expr::Tuple(
                    fields.iter().map(|(_, value)| value.clone()).collect(),
                    *span,
                )),
                _ => Err(KainError::codegen(
                    format!("Unsupported LLVM aggregate init type: {:?}", ty),
                    *span,
                )),
            },
            Expr::Array(items, _) => {
                let arr = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @array_new(i64 {})",
                    arr,
                    items.len().max(4)
                ));
                for item in items {
                    let (val, ty) = self.compile_expr(item)?;
                    let stored = self.coerce_to_i64_storage(&val, &ty);
                    self.emit(&format!(
                        "  call void @array_push(i8* {}, i64 {})",
                        arr, stored
                    ));
                }
                Ok((arr, "i8*".into()))
            }
            Expr::Tuple(items, span) => {
                let mut compiled_fields = Vec::new();
                let mut field_tys = Vec::new();
                for item in items {
                    let (val, ty) = self.compile_expr(item)?;
                    compiled_fields.push((val, ty.clone()));
                    field_tys.push(ty);
                }

                let tuple_name = Self::tuple_struct_name_from_types(&field_tys);
                let tuple_ptr_ty = format!("%{}*", tuple_name);
                if !self.struct_defs.contains_key(&tuple_name) {
                    return Err(KainError::codegen(
                        format!(
                            "Tuple LLVM type '{}' was not registered before codegen",
                            tuple_name
                        ),
                        *span,
                    ));
                }

                let null_ptr = format!("{} null", tuple_ptr_ty);
                let size_ptr_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr %{}, {}, i32 1",
                    size_ptr_reg, tuple_name, null_ptr
                ));
                let size_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = ptrtoint {} {} to i64",
                    size_reg, tuple_ptr_ty, size_ptr_reg
                ));
                let mem_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @KAIN_alloc(i64 {})",
                    mem_reg, size_reg
                ));
                let tuple_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = bitcast i8* {} to {}",
                    tuple_ptr, mem_reg, tuple_ptr_ty
                ));

                for (index, (field_val, field_ty)) in compiled_fields.iter().enumerate() {
                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds %{}, {} {}, i32 0, i32 {}",
                        field_ptr, tuple_name, tuple_ptr_ty, tuple_ptr, index
                    ));
                    self.emit(&format!(
                        "  store {} {}, {}* {}",
                        field_ty, field_val, field_ty, field_ptr
                    ));
                }

                Ok((tuple_ptr, tuple_ptr_ty))
            }
            Expr::Index {
                object,
                index,
                span: _,
            } => {
                let (obj_val, obj_ty) = self.compile_expr(object)?;
                let (idx_val, _) = self.compile_expr(index)?;
                if obj_ty == "i8*" {
                    let res = self.next_reg();
                    self.emit(&format!(
                        "  {} = call i64 @array_get(i8* {}, i64 {})",
                        res, obj_val, idx_val
                    ));
                    Ok((res, "i64".into()))
                } else {
                    let (field_ptr, field_ty) = self.compile_index_address_from_compiled(
                        &obj_val,
                        &obj_ty,
                        &idx_val,
                        index.span(),
                    )?;
                    let loaded = self.next_reg();
                    self.emit(&format!(
                        "  {} = load {}, {}* {}",
                        loaded, field_ty, field_ty, field_ptr
                    ));
                    Ok((loaded, field_ty))
                }
            }
            Expr::Spawn { actor, init, span } => {
                let def = self
                    .struct_defs
                    .get(actor)
                    .cloned()
                    .ok_or(KainError::codegen(
                        format!("Unknown actor: {}", actor),
                        *span,
                    ))?;

                let struct_ty = format!("%{}", actor);

                // Allocate on Heap
                let null_ptr = format!("{}* null", struct_ty);
                let size_ptr_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr {}, {}, i32 1",
                    size_ptr_reg, struct_ty, null_ptr
                ));
                let size_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = ptrtoint {}* {} to i64",
                    size_reg, struct_ty, size_ptr_reg
                ));

                let mem_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @KAIN_alloc(i64 {})",
                    mem_reg, size_reg
                ));

                // Cast to struct ptr
                let struct_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = bitcast i8* {} to {}*",
                    struct_ptr, mem_reg, struct_ty
                ));

                // Initialize fields
                let mut provided: HashMap<String, Expr> = init.iter().cloned().collect();
                for (i, (field_name, field_ty)) in def.iter().enumerate() {
                    let (val, val_ty) = if let Some(expr) = provided.remove(field_name) {
                        self.compile_expr(&expr)?
                    } else {
                        // Default zero init
                        let zero_val = if field_ty == "double" {
                            "0.0"
                        } else if field_ty == "i8*" {
                            "null"
                        } else {
                            "0"
                        };
                        (zero_val.into(), field_ty.clone())
                    };

                    let field_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds {}, {}* {}, i32 0, i32 {}",
                        field_ptr, struct_ty, struct_ty, struct_ptr, i
                    ));
                    self.emit(&format!(
                        "  store {} {}, {}* {}",
                        val_ty, val, val_ty, field_ptr
                    ));
                }

                // Spawn using proper actor bootstrap ABI
                // Use the actor-specific run function instead of default_actor_run
                let actor_run_fn = format!("{}_run", actor);
                let func_ptr = format!("bitcast (void (i8*)* @{} to i8*)", actor_run_fn);

                // Register Destructor if struct has RC fields
                let has_rc_fields = def.iter().any(|(_, ty)| ty == "i8*" || ty.starts_with("%"));
                if has_rc_fields {
                    let dtor_name = format!("dtor_{}", actor);
                    self.emit(&format!(
                        "  call void @KAIN_set_destructor(i8* {}, void (i8*)* @{})",
                        mem_reg, dtor_name
                    ));
                }

                // Call KAIN_spawn with the actor-specific entrypoint
                self.emit(&format!(
                    "  call void @KAIN_spawn(i8* {}, i8* {})",
                    func_ptr, mem_reg
                ));

                Ok((struct_ptr, format!("%{}*", actor)))
            }
            Expr::SendMsg {
                target,
                message,
                data,
                span,
            } => {
                let (target_val, target_ty) = self.compile_expr(target)?;
                let actor_name = if target_ty.starts_with('%') && target_ty.ends_with('*') {
                    target_ty
                        .trim_start_matches('%')
                        .trim_end_matches('*')
                        .to_string()
                } else {
                    return Err(KainError::codegen(
                        format!(
                            "Cannot send message '{}' to non-actor type {}",
                            message, target_ty
                        ),
                        *span,
                    ));
                };

                let mailbox_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds %{}, {} {}, i32 0, i32 0",
                    mailbox_ptr, actor_name, target_ty, target_val
                ));
                let mailbox = self.next_reg();
                self.emit(&format!("  {} = load i8*, i8** {}", mailbox, mailbox_ptr));

                let payload_struct_name = format!("{}_{}", actor_name, message);
                let payload_mem =
                    if let Some(field_defs) = self.struct_defs.get(&payload_struct_name).cloned() {
                        let payload_ty = format!("%{}", payload_struct_name);
                        let payload_ptr_ty = format!("{}*", payload_ty);
                        let null_ptr = format!("{} null", payload_ptr_ty);
                        let size_ptr_reg = self.next_reg();
                        self.emit(&format!(
                            "  {} = getelementptr {}, {}, i32 1",
                            size_ptr_reg, payload_ty, null_ptr
                        ));
                        let size_reg = self.next_reg();
                        self.emit(&format!(
                            "  {} = ptrtoint {} {} to i64",
                            size_reg, payload_ptr_ty, size_ptr_reg
                        ));
                        let payload_mem = self.next_reg();
                        self.emit(&format!(
                            "  {} = call i8* @KAIN_alloc(i64 {})",
                            payload_mem, size_reg
                        ));
                        let payload_ptr = self.next_reg();
                        self.emit(&format!(
                            "  {} = bitcast i8* {} to {}",
                            payload_ptr, payload_mem, payload_ptr_ty
                        ));

                        let named_args: std::collections::HashMap<String, Expr> =
                            data.iter().cloned().collect();
                        for (i, (field_name, field_ty)) in field_defs.iter().enumerate() {
                            let expr = named_args.get(field_name).ok_or_else(|| {
                                KainError::codegen(
                                    format!(
                                        "Missing field '{}' for actor message '{}.{}'",
                                        field_name, actor_name, message
                                    ),
                                    *span,
                                )
                            })?;
                            let (val, val_ty) = self.compile_expr(expr)?;
                            let field_ptr = self.next_reg();
                            self.emit(&format!(
                                "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 {}",
                                field_ptr, payload_ty, payload_ptr_ty, payload_ptr, i
                            ));
                            self.emit(&format!(
                                "  store {} {}, {}* {}",
                                val_ty, val, field_ty, field_ptr
                            ));
                        }

                        payload_mem
                    } else {
                        "null".to_string()
                    };

                let tag = self.hash_message_tag(&actor_name, message);
                self.emit(&format!(
                    "  call void @mq_push(i8* {}, i64 {}, i8* {})",
                    mailbox, tag, payload_mem
                ));
                Ok(("0".into(), "i64".into()))
            }
            Expr::If {
                condition,
                then_branch,
                else_branch,
                span,
            } => {
                let start_block = self.current_block.clone();
                let (cond_val, _) = self.compile_expr(condition)?;

                let label_then = self.next_label();
                let label_else = self.next_label();
                let label_merge = self.next_label();

                let has_else = else_branch.is_some();
                let target_else = if has_else { &label_else } else { &label_merge };

                self.emit(&format!(
                    "  br i1 {}, label %{}, label %{}",
                    cond_val, label_then, target_else
                ));

                let mut incoming = Vec::new();

                // Then Block
                self.emit_label(&label_then);
                let then_res = self.compile_block_with_result(then_branch)?;
                let then_end_block = self.current_block.clone();
                self.emit(&format!("  br label %{}", label_merge));

                if let Some((val, ty)) = then_res {
                    incoming.push((val, ty, then_end_block));
                } else {
                    incoming.push(("0".into(), "i64".into(), then_end_block));
                }

                // Else Block
                if let Some(else_branch) = else_branch {
                    self.emit_label(&label_else);
                    let else_res = match else_branch.as_ref() {
                        kain_core::ast::ElseBranch::Else(b) => self.compile_block_with_result(b)?,
                        kain_core::ast::ElseBranch::ElseIf(cond, then, el) => {
                            let nested = Expr::If {
                                condition: cond.clone(),
                                then_branch: then.clone(),
                                else_branch: el.clone(),
                                span: *span,
                            };
                            Some(self.compile_expr(&nested)?)
                        }
                    };

                    let else_end_block = self.current_block.clone();
                    self.emit(&format!("  br label %{}", label_merge));

                    if let Some((val, ty)) = else_res {
                        incoming.push((val, ty, else_end_block));
                    } else {
                        incoming.push(("0".into(), "i64".into(), else_end_block));
                    }
                } else {
                    // No else branch: path comes from start_block with value 0
                    incoming.push(("0".into(), "i64".into(), start_block));
                }

                self.emit_label(&label_merge);

                // Generate Phi
                let res_ty = incoming[0].1.clone();
                let res_reg = self.next_reg();

                // Check consistency (simple check)
                let consistent = incoming.iter().all(|(_, ty, _)| *ty == res_ty);

                if consistent {
                    let phi_args = incoming
                        .iter()
                        .map(|(val, _, block)| format!("[ {}, %{} ]", val, block))
                        .collect::<Vec<_>>()
                        .join(", ");

                    self.emit(&format!("  {} = phi {} {}", res_reg, res_ty, phi_args));
                    Ok((res_reg, res_ty))
                } else {
                    Err(KainError::codegen(
                        "LLVM if-expression branches produced inconsistent result types",
                        *span,
                    ))
                }
            }
            Expr::Ident(name, span) => {
                if let Some((ptr, ty)) = self.locals.get(name).cloned() {
                    let reg = self.next_reg();
                    self.emit(&format!("  {} = load {}, {}* {}", reg, ty, ty, ptr));
                    Ok((reg, ty))
                } else {
                    Err(KainError::codegen(
                        format!("Undefined variable: {}", name),
                        *span,
                    ))
                }
            }
            Expr::Binary {
                left, op, right, ..
            } => {
                let (lhs, lhs_ty) = self.compile_expr(left)?;
                let (rhs, rhs_ty) = self.compile_expr(right)?;
                let (lhs, ty, rhs, rhs_ty) =
                    self.coerce_binary_operands(lhs, lhs_ty, rhs, rhs_ty)?;

                if *op == BinaryOp::Add && (ty == "i8*" || rhs_ty == "i8*") {
                    let res = self.next_reg();
                    self.emit(&format!(
                        "  {} = call i8* @str_concat(i8* {}, i8* {})",
                        res, lhs, rhs
                    ));
                    return Ok((res, "i8*".into()));
                }

                if (*op == BinaryOp::Eq || *op == BinaryOp::Ne) && (ty == "i8*" || rhs_ty == "i8*")
                {
                    let res = self.next_reg();
                    self.emit(&format!(
                        "  {} = call i1 @deep_eq(i8* {}, i8* {})",
                        res, lhs, rhs
                    ));

                    if *op == BinaryOp::Ne {
                        let inv = self.next_reg();
                        self.emit(&format!("  {} = xor i1 {}, 1", inv, res));
                        return Ok((inv, "i1".into()));
                    }
                    return Ok((res, "i1".into()));
                }

                let is_float = ty == "double" && rhs_ty == "double";
                let res = self.next_reg();

                match op {
                    BinaryOp::Add => {
                        if is_float {
                            self.emit(&format!("  {} = fadd double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = add {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, ty))
                    }
                    BinaryOp::Sub => {
                        if is_float {
                            self.emit(&format!("  {} = fsub double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = sub {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, ty))
                    }
                    BinaryOp::Mul => {
                        if is_float {
                            self.emit(&format!("  {} = fmul double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = mul {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, ty))
                    }
                    BinaryOp::Div => {
                        if is_float {
                            self.emit(&format!("  {} = fdiv double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = sdiv {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, ty))
                    }
                    BinaryOp::Mod => {
                        if is_float {
                            self.emit(&format!("  {} = frem double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = srem {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, ty))
                    }
                    BinaryOp::Pow => {
                        if is_float {
                            self.emit(&format!(
                                "  {} = call double @pow(double {}, double {})",
                                res, lhs, rhs
                            ));
                            Ok((res, "double".to_string()))
                        } else {
                            let lhs_cast = self.cast_numeric_value(lhs, &ty, "double")?;
                            let rhs_cast = self.cast_numeric_value(rhs, &rhs_ty, "double")?;
                            let pow_res = self.next_reg();
                            self.emit(&format!(
                                "  {} = call double @pow(double {}, double {})",
                                pow_res, lhs_cast, rhs_cast
                            ));
                            let int_res = self.next_reg();
                            self.emit(&format!("  {} = fptosi double {} to i64", int_res, pow_res));
                            Ok((int_res, "i64".to_string()))
                        }
                    }
                    BinaryOp::Eq => {
                        if is_float {
                            self.emit(&format!("  {} = fcmp oeq double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = icmp eq {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, "i1".to_string()))
                    }
                    BinaryOp::Ne => {
                        if is_float {
                            self.emit(&format!("  {} = fcmp one double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = icmp ne {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, "i1".to_string()))
                    }
                    BinaryOp::Lt => {
                        if is_float {
                            self.emit(&format!("  {} = fcmp olt double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = icmp slt {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, "i1".to_string()))
                    }
                    BinaryOp::Gt => {
                        if is_float {
                            self.emit(&format!("  {} = fcmp ogt double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = icmp sgt {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, "i1".to_string()))
                    }
                    BinaryOp::Le => {
                        if is_float {
                            self.emit(&format!("  {} = fcmp ole double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = icmp sle {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, "i1".to_string()))
                    }
                    BinaryOp::Ge => {
                        if is_float {
                            self.emit(&format!("  {} = fcmp oge double {}, {}", res, lhs, rhs));
                        } else {
                            self.emit(&format!("  {} = icmp sge {} {}, {}", res, ty, lhs, rhs));
                        }
                        Ok((res, "i1".to_string()))
                    }
                    BinaryOp::And => {
                        self.emit(&format!("  {} = and {} {}, {}", res, ty, lhs, rhs));
                        Ok((res, ty))
                    }
                    BinaryOp::Or => {
                        self.emit(&format!("  {} = or {} {}, {}", res, ty, lhs, rhs));
                        Ok((res, ty))
                    }
                    BinaryOp::BitAnd => {
                        self.emit(&format!("  {} = and {} {}, {}", res, ty, lhs, rhs));
                        Ok((res, ty))
                    }
                    BinaryOp::BitOr => {
                        self.emit(&format!("  {} = or {} {}, {}", res, ty, lhs, rhs));
                        Ok((res, ty))
                    }
                    BinaryOp::BitXor => {
                        self.emit(&format!("  {} = xor {} {}, {}", res, ty, lhs, rhs));
                        Ok((res, ty))
                    }
                    BinaryOp::Shl => {
                        self.emit(&format!("  {} = shl {} {}, {}", res, ty, lhs, rhs));
                        Ok((res, ty))
                    }
                    BinaryOp::Shr => {
                        self.emit(&format!("  {} = ashr {} {}, {}", res, ty, lhs, rhs));
                        Ok((res, ty))
                    }
                    _ => Err(KainError::codegen(
                        format!("Unsupported LLVM binary operator: {:?}", op),
                        expr.span(),
                    )),
                }
            }
            Expr::MethodCall {
                receiver,
                method,
                args,
                span,
            } => {
                // LLVM doesn't have native method dispatch.
                // We resolve methods by checking the type of the receiver.

                let (obj_val, obj_ty) = self.compile_expr(receiver)?;

                // 1. Struct Methods: Call Struct_method(obj, args...)
                if obj_ty.starts_with("%") && obj_ty.ends_with("*") {
                    let struct_name = &obj_ty[1..obj_ty.len() - 1]; // Remove % and *
                    let func_name = format!("{}_{}", struct_name, method);

                    if self.functions.contains_key(&func_name) {
                        let mut compiled_args = Vec::new();
                        let mut arg_types = Vec::new();

                        // Pass 'self' as first argument
                        compiled_args.push(obj_val);
                        arg_types.push(obj_ty);

                        for arg in args {
                            let (val, ty) = self.compile_expr(&arg.value)?;
                            compiled_args.push(val);
                            arg_types.push(ty);
                        }

                        let ret_ty = self.functions.get(&func_name).unwrap().clone();
                        let res = self.next_reg();

                        let arg_str = compiled_args
                            .iter()
                            .zip(arg_types.iter())
                            .map(|(val, ty)| format!("{} {}", ty, val))
                            .collect::<Vec<_>>()
                            .join(", ");

                        if ret_ty == "void" {
                            self.emit(&format!("  call void @{}({})", func_name, arg_str));
                            return Ok(("0".into(), "i64".into()));
                        }

                        self.emit(&format!(
                            "  {} = call {} @{}({})",
                            res, ret_ty, func_name, arg_str
                        ));
                        return Ok((res, ret_ty));
                    }
                }

                return Err(KainError::codegen(
                    format!("Method {} not found on type {}", method, obj_ty),
                    *span,
                ));
            }
            Expr::Call { callee, args, span } => {
                if let Expr::Ident(name, _) = callee.as_ref() {
                    if let Some(result) = self.compile_lowered_helper_call(name, args, *span) {
                        return result;
                    }
                }

                // Handle print intrinsic
                if let Expr::Ident(name, _) = callee.as_ref() {
                    if name == "to_string" && args.len() == 1 {
                        let (val, ty) = self.compile_expr(&args[0].value)?;
                        if ty == "i64" {
                            let res = self.next_reg();
                            self.emit(&format!("  {} = call i8* @to_string(i64 {})", res, val));
                            return Ok((res, "i8*".into()));
                        }
                    }

                    if name == "now" {
                        let res = self.next_reg();
                        self.emit(&format!("  {} = call i64 @clock_wrapper()", res));
                        return Ok((res, "i64".into()));
                    }

                    if name == "print" || name == "println" {
                        // Just print the first arg for now
                        if let Some(arg) = args.first() {
                            let (val, ty) = self.compile_expr(&arg.value)?;
                            if ty == "i64" {
                                self.emit(&format!("  call void @print_i64(i64 {})", val));
                            } else if ty == "double" {
                                self.emit(&format!("  call void @print_f64(double {})", val));
                            } else if ty == "i1" {
                                self.emit(&format!("  call void @print_bool(i1 {})", val));
                            } else {
                                // Assume string or unknown
                                self.emit(&format!("  call void @print_str(i8* {}, i64 0)", val));
                            }

                            // Release if temporary
                            if (ty == "i8*" || ty.starts_with("%"))
                                && self.is_new_object(&arg.value)
                            {
                                self.emit_release(&val, &ty);
                            }
                        }
                        return Ok(("0".into(), "i64".into()));
                    }
                }

                // Normal call - extract function name
                let func_name = match callee.as_ref() {
                    Expr::Ident(name, _) => name.clone(),
                    _ => {
                        return Err(KainError::codegen(
                            "Only direct function calls supported",
                            *span,
                        ))
                    }
                };

                let mut compiled_args = Vec::new();
                let mut arg_types = Vec::new();

                for (i, arg) in args.iter().enumerate() {
                    let (val, ty) = self.compile_expr(&arg.value)?;

                    // --- HOTFIX: Intrinsic Pointer Casting ---
                    // Check if we are passing a pointer (i8* or %Struct*) to a function
                    // that expects a generic i64 storage value (Array/Map storage).
                    let needs_cast_to_i64 = (ty == "i8*" || ty.starts_with("%"))
                        && (
                            (func_name == "push" && i == 1) ||        // push(arr, VAL)
                        (func_name == "array_push" && i == 1) ||  // array_push(arr, VAL)
                        (func_name == "array_set" && i == 2) ||   // array_set(arr, idx, VAL)
                        (func_name == "map_set" && i == 2)
                            // map_set(map, key, VAL)
                        );

                    if needs_cast_to_i64 {
                        let int_val = self.next_reg();
                        // Explicitly cast pointer to integer for the runtime
                        self.emit(&format!("  {} = ptrtoint {} {} to i64", int_val, ty, val));
                        compiled_args.push(int_val);
                        arg_types.push("i64".to_string());
                        continue;
                    }
                    // --- END HOTFIX ---

                    compiled_args.push(val);
                    arg_types.push(ty);
                }

                let ret_ty = if let Some(ty) = self.functions.get(&func_name) {
                    ty.clone()
                } else {
                    "i64".into() // Default
                };

                let res = self.next_reg();
                let arg_str = compiled_args
                    .iter()
                    .zip(arg_types.iter())
                    .map(|(val, ty)| format!("{} {}", ty, val))
                    .collect::<Vec<_>>()
                    .join(", ");
                let runtime_symbol = runtime_symbol_for_stdlib_function(&func_name);

                if ret_ty == "void" {
                    self.emit(&format!("  call void @{}({})", runtime_symbol, arg_str));
                    Ok(("0".into(), "i64".into()))
                } else {
                    self.emit(&format!(
                        "  {} = call {} @{}({})",
                        res, ret_ty, runtime_symbol, arg_str
                    ));
                    Ok((res, ret_ty))
                }
            }
            Expr::EnumVariant {
                enum_name,
                variant,
                fields,
                ..
            } => {
                let struct_ty = format!("%{}", enum_name);
                let ptr_ty = format!("{}*", struct_ty);

                // Allocate Enum struct
                let null_ptr = format!("{} null", ptr_ty);
                let size_ptr_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr {}, {}, i32 1",
                    size_ptr_reg, struct_ty, null_ptr
                ));
                let size_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = ptrtoint {} {} to i64",
                    size_reg, ptr_ty, size_ptr_reg
                ));

                let mem_reg = self.next_reg();
                self.emit(&format!(
                    "  {} = call i8* @KAIN_alloc(i64 {})",
                    mem_reg, size_reg
                ));

                let enum_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = bitcast i8* {} to {}",
                    enum_ptr, mem_reg, ptr_ty
                ));

                // Store Tag
                let tag = self.hash_message_tag(enum_name, variant);
                let tag_ptr = self.next_reg();
                self.emit(&format!(
                    "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 0",
                    tag_ptr, struct_ty, ptr_ty, enum_ptr
                ));
                self.emit(&format!("  store i64 {}, i64* {}", tag, tag_ptr));

                // Handle Payload
                let payload_struct_name = format!("{}_{}", enum_name, variant);
                let payload_ty = format!("%{}", payload_struct_name);
                let payload_ptr_ty = format!("{}*", payload_ty);

                // Check if payload struct exists (implies non-empty payload)
                if self.struct_defs.contains_key(&payload_struct_name) {
                    // Allocate Payload
                    let p_null_ptr = format!("{} null", payload_ptr_ty);
                    let p_size_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr {}, {}, i32 1",
                        p_size_ptr, payload_ty, p_null_ptr
                    ));
                    let p_size = self.next_reg();
                    self.emit(&format!(
                        "  {} = ptrtoint {} {} to i64",
                        p_size, payload_ptr_ty, p_size_ptr
                    ));

                    let p_mem = self.next_reg();
                    self.emit(&format!(
                        "  {} = call i8* @KAIN_alloc(i64 {})",
                        p_mem, p_size
                    ));

                    let p_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = bitcast i8* {} to {}",
                        p_ptr, p_mem, payload_ptr_ty
                    ));

                    // Store Fields
                    match fields {
                        kain_core::ast::EnumVariantFields::Tuple(exprs) => {
                            for (i, expr) in exprs.iter().enumerate() {
                                let (val, val_ty) = self.compile_expr(expr)?;
                                let field_ptr = self.next_reg();
                                self.emit(&format!(
                                    "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 {}",
                                    field_ptr, payload_ty, payload_ptr_ty, p_ptr, i
                                ));
                                self.emit(&format!(
                                    "  store {} {}, {}* {}",
                                    val_ty, val, val_ty, field_ptr
                                ));
                            }
                        }
                        kain_core::ast::EnumVariantFields::Struct(named_fields) => {
                            for (i, (_, expr)) in named_fields.iter().enumerate() {
                                let (val, val_ty) = self.compile_expr(expr)?;
                                let field_ptr = self.next_reg();
                                self.emit(&format!(
                                    "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 {}",
                                    field_ptr, payload_ty, payload_ptr_ty, p_ptr, i
                                ));
                                self.emit(&format!(
                                    "  store {} {}, {}* {}",
                                    val_ty, val, val_ty, field_ptr
                                ));
                            }
                        }
                        _ => {}
                    }

                    // Store Payload Pointer in Enum
                    let payload_ptr_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 1",
                        payload_ptr_ptr, struct_ty, ptr_ty, enum_ptr
                    ));
                    self.emit(&format!("  store i8* {}, i8** {}", p_mem, payload_ptr_ptr));
                } else {
                    // Store Null
                    let payload_ptr_ptr = self.next_reg();
                    self.emit(&format!(
                        "  {} = getelementptr inbounds {}, {} {}, i32 0, i32 1",
                        payload_ptr_ptr, struct_ty, ptr_ty, enum_ptr
                    ));
                    self.emit(&format!("  store i8* null, i8** {}", payload_ptr_ptr));
                }

                Ok((enum_ptr, ptr_ty))
            }
            Expr::Match {
                scrutinee,
                arms,
                span,
            } => {
                let (val, val_ty) = self.compile_expr(scrutinee)?;
                let enum_name = if val_ty.starts_with('%') && val_ty.ends_with('*') {
                    Some(
                        val_ty
                            .trim_start_matches('%')
                            .trim_end_matches('*')
                            .to_string(),
                    )
                } else {
                    None
                };

                let label_end = self.next_label();
                let mut arm_labels = Vec::new();
                let mut next_labels = Vec::new();
                for i in 0..arms.len() {
                    arm_labels.push(self.next_label());
                    next_labels.push(if i + 1 < arms.len() {
                        self.next_label()
                    } else {
                        label_end.clone()
                    });
                }

                if arms.is_empty() {
                    self.emit(&format!("  br label %{}", label_end));
                } else {
                    self.emit(&format!("  br label %{}", next_labels[0]));
                }

                let mut incoming = Vec::new();

                for (i, arm) in arms.iter().enumerate() {
                    self.emit_label(&next_labels[i]);
                    let cond = self.compile_pattern_condition(
                        &arm.pattern,
                        &val,
                        &val_ty,
                        enum_name.as_deref(),
                        arm.span,
                    )?;

                    let branch_true = arm_labels[i].clone();
                    let branch_false = if i + 1 < arms.len() {
                        next_labels[i + 1].clone()
                    } else {
                        label_end.clone()
                    };
                    self.emit(&format!(
                        "  br i1 {}, label %{}, label %{}",
                        cond, branch_true, branch_false
                    ));

                    self.emit_label(&arm_labels[i]);
                    self.scopes.push(Vec::new());

                    self.bind_match_pattern(
                        &arm.pattern,
                        &val,
                        &val_ty,
                        enum_name.as_deref(),
                        arm.span,
                    )?;

                    if let Some(guard) = &arm.guard {
                        let (guard_val, guard_ty) = self.compile_expr(guard)?;
                        if guard_ty != "i1" {
                            return Err(KainError::codegen(
                                format!("Match guard must compile to bool/i1, got {}", guard_ty),
                                arm.span,
                            ));
                        }
                        let guard_pass = self.next_label();
                        let guard_fail = if i + 1 < arms.len() {
                            next_labels[i + 1].clone()
                        } else {
                            label_end.clone()
                        };
                        self.emit(&format!(
                            "  br i1 {}, label %{}, label %{}",
                            guard_val, guard_pass, guard_fail
                        ));
                        self.emit_label(&guard_pass);
                    }

                    let (res_val, res_ty) = self.compile_expr(&arm.body)?;
                    let arm_end_block = self.current_block.clone();

                    self.emit_scope_exit();
                    self.emit(&format!("  br label %{}", label_end));
                    incoming.push((res_val, res_ty, arm_end_block));
                }

                self.emit_label(&label_end);

                // Phi
                if incoming.is_empty() {
                    Ok(("0".into(), "i64".into()))
                } else {
                    let res_ty = incoming[0].1.clone();
                    let res_reg = self.next_reg();

                    let phi_args = incoming
                        .iter()
                        .map(|(val, _, block)| format!("[ {}, %{} ]", val, block))
                        .collect::<Vec<_>>()
                        .join(", ");

                    self.emit(&format!("  {} = phi {} {}", res_reg, res_ty, phi_args));
                    Ok((res_reg, res_ty))
                }
            }
            // Catch-all for unsupported expressions
            other => Err(KainError::codegen(
                format!("Unsupported LLVM expression: {:?}", other),
                other.span(),
            )),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::runtime_symbol_for_stdlib_function;

    #[test]
    fn remaps_rounding_builtins_to_runtime_wrappers() {
        assert_eq!(
            runtime_symbol_for_stdlib_function("floor"),
            "kain_floor_i64"
        );
        assert_eq!(runtime_symbol_for_stdlib_function("ceil"), "kain_ceil_i64");
        assert_eq!(
            runtime_symbol_for_stdlib_function("round"),
            "kain_round_i64"
        );
        assert_eq!(runtime_symbol_for_stdlib_function("sqrt"), "sqrt");
    }
}
