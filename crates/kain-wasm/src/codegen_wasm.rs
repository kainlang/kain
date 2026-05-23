//! WASM Code Generation using walrus
//!
//! This module converts the Typed AST into WebAssembly.

use crate::c_runtime_shims::{
    wasm_c_runtime_constant, wasm_c_runtime_shim, wasm_import_signature_types, WasmCRuntimeShimKind,
};
use kain_core::ast::{BinaryOp, Block, CallArg, ConvergeSelector, Expr, Stmt};
use kain_core::error::{KainError, KainResult};
use kain_core::types::{
    ResolvedType, TypedActor, TypedConverge, TypedFunction, TypedItem, TypedProgram,
};
use kain_core::{lower_typed_program_memory_for_target, CompileTarget};
use std::collections::HashMap;
use walrus::{FunctionBuilder, InstrSeqBuilder, LocalId, Module, ModuleConfig, ValType};

pub fn generate(program: &TypedProgram) -> KainResult<Vec<u8>> {
    let lowered = lower_typed_program_memory_for_target(program, CompileTarget::Wasm)?;
    let mut compiler = WasmCompiler::new();
    compiler.compile_program(&lowered)?;
    Ok(compiler.module.emit_wasm())
}

#[cfg(test)]
mod tests {
    use super::generate;
    use kain_core::diagnostics::SpanMapper;
    use kain_core::lexer::Lexer;
    use kain_core::low_level_memory_metadata::{
        marker_attr, usize_bool_attr, C_BITFIELD_ATTR, C_UNION_ATTR,
    };
    use kain_core::parser::Parser;
    use kain_core::ast::{
        Expr, Field, Function, Item, Param, Program, Stmt, Struct, Type, Visibility,
    };
    use kain_core::types::check;

    fn parse_and_typecheck(source: &str, filename: &str) -> kain_core::types::TypedProgram {
        let tokens = Lexer::new(source).tokenize().expect("tokenize");
        let mapper = SpanMapper::new(source);
        let program = Parser::new(&tokens, &mapper, filename)
            .parse()
            .expect("parse");
        check(&program, &mapper, filename).expect("typecheck")
    }

    #[test]
    fn wasm_generate_handles_lowered_union_and_bitfield_memory_helpers() {
        let span = kain_core::span::Span::default();
        let int_ty = Type::Named {
            name: "Int".to_string(),
            generics: Vec::new(),
            span,
        };
        let float_ty = Type::Named {
            name: "Float".to_string(),
            generics: Vec::new(),
            span,
        };
        let union_ty = Type::Named {
            name: "Number".to_string(),
            generics: Vec::new(),
            span,
        };
        let flags_ty = Type::Named {
            name: "Flags".to_string(),
            generics: Vec::new(),
            span,
        };

        let program = Program {
            items: vec![
                Item::Struct(Struct {
                    name: "Number".to_string(),
                    generics: Vec::new(),
                    fields: vec![
                        Field {
                            name: "as_int".to_string(),
                            ty: int_ty.clone(),
                            attributes: Vec::new(),
                            visibility: Visibility::Public,
                            default: None,
                            weak: false,
                            span,
                        },
                        Field {
                            name: "as_float".to_string(),
                            ty: float_ty,
                            attributes: Vec::new(),
                            visibility: Visibility::Public,
                            default: None,
                            weak: false,
                            span,
                        },
                    ],
                    methods: Vec::new(),
                    attributes: vec![marker_attr(C_UNION_ATTR, span)],
                    visibility: Visibility::Public,
                    span,
                }),
                Item::Struct(Struct {
                    name: "Flags".to_string(),
                    generics: Vec::new(),
                    fields: vec![Field {
                        name: "ready".to_string(),
                        ty: int_ty.clone(),
                        attributes: vec![usize_bool_attr(C_BITFIELD_ATTR, 1, true, span)],
                        visibility: Visibility::Public,
                        default: None,
                        weak: false,
                        span,
                    }],
                    methods: Vec::new(),
                    attributes: Vec::new(),
                    visibility: Visibility::Public,
                    span,
                }),
                Item::Function(Function {
                    name: "probe".to_string(),
                    generics: Vec::new(),
                    params: vec![Param {
                        name: "flags".to_string(),
                        ty: flags_ty,
                        mutable: true,
                        default: None,
                        span,
                    }],
                    return_type: Some(union_ty.clone()),
                    effects: Vec::new(),
                    body: kain_core::ast::Block {
                        stmts: vec![
                            Stmt::Expr(Expr::Assign {
                                target: Box::new(Expr::Field {
                                    object: Box::new(Expr::Ident("flags".to_string(), span)),
                                    field: "ready".to_string(),
                                    span,
                                }),
                                value: Box::new(Expr::Int(1, span)),
                                span,
                            }),
                            Stmt::Return(
                                Some(Expr::AggregateInit {
                                    ty: union_ty,
                                    fields: vec![
                                        ("as_int".to_string(), Expr::Int(7, span)),
                                        ("as_float".to_string(), Expr::Float(3.0, span)),
                                    ],
                                    zero_fill_rest: true,
                                    span,
                                }),
                                span,
                            ),
                        ],
                        span,
                    },
                    visibility: Visibility::Public,
                    attributes: Vec::new(),
                    span,
                }),
            ],
            span,
        };

        let mapper = SpanMapper::new("");
        let typed = check(&program, &mapper, "wasm_low_level.kn").expect("typecheck");
        let wasm = generate(&typed).expect("wasm generation");
        assert!(!wasm.is_empty());
    }

    #[test]
    fn wasm_generate_resolves_struct_fields_through_indexed_arrays() {
        let source = r#"
struct Packet:
    bias: Int
    phase: Int

fn main() -> Int:
    let packets = [
        Packet { bias: 3, phase: 5 },
        Packet { bias: 7, phase: 11 }
    ]
    let slot = 1
    return packets[slot].bias + packets[slot].phase
"#;

        let typed = parse_and_typecheck(source, "wasm_indexed_packets.kn");
        let wasm = generate(&typed).expect("wasm generation");
        assert!(!wasm.is_empty());
    }

    #[test]
    fn wasm_generate_allows_ask_through_actor_typed_helper_params() {
        let source = r#"
actor Relay:
    state bias: Int = 7

    on Fold(reply_to: P, request: Int):
        send reply_to.Reply(value = request + self.bias)

fn ask_helper(relay: Relay, request: Int) -> Int:
    return ask(relay, "Fold", request)

fn main() -> Int:
    let relay = spawn Relay(bias = 5)
    return ask_helper(relay, 9)
"#;

        let typed = parse_and_typecheck(source, "wasm_actor_helper.kn");
        let wasm = generate(&typed).expect("wasm generation");
        assert!(!wasm.is_empty());
    }

    #[test]
    fn wasm_generate_preserves_layouts_for_function_returned_vectors() {
        let source = r#"
struct Axis:
    x: Float
    y: Float
    z: Float

struct Other:
    x: Float
    t: Float

fn axis() -> Axis:
    return Axis { x: 1.0, y: 2.0, z: 3.0 }

fn main() -> Float:
    let value = axis()
    return value.x + value.y + value.z
"#;

        let typed = parse_and_typecheck(source, "wasm_function_vector_layout.kn");
        let wasm = generate(&typed).expect("wasm generation");
        assert!(!wasm.is_empty());
    }

    #[test]
    fn wasm_generate_preserves_layouts_for_if_expression_results() {
        let source = r#"
struct Axis:
    x: Float
    y: Float

struct Other:
    x: Float
    t: Float

fn choose(flag: Bool) -> Axis:
    if flag:
        return Axis { x: 1.0, y: 2.0 }
    return Axis { x: 3.0, y: 4.0 }

fn main() -> Float:
    let chosen = if true:
        choose(true)
    else:
        choose(false)
    return chosen.x + chosen.y
"#;

        let typed = parse_and_typecheck(source, "wasm_if_vector_layout.kn");
        let wasm = generate(&typed).expect("wasm generation");
        assert!(!wasm.is_empty());
    }

    #[test]
    fn wasm_generate_preserves_layouts_for_if_expr_with_param_branch() {
        let source = r#"
struct Axis:
    x: Float
    y: Float

struct Other:
    x: Float
    t: Float

fn blend(a: Axis, b: Axis, flag: Bool) -> Float:
    let chosen = if flag:
        Axis { x: a.x, y: a.y }
    else:
        b
    return chosen.x + chosen.y

fn main() -> Float:
    return blend(Axis { x: 1.0, y: 2.0 }, Axis { x: 3.0, y: 4.0 }, true)
"#;

        let typed = parse_and_typecheck(source, "wasm_if_param_branch_layout.kn");
        let wasm = generate(&typed).expect("wasm generation");
        assert!(!wasm.is_empty());
    }
}

struct WasmCompiler {
    module: Module,
    /// Map function names to their WASM function IDs for call resolution
    functions: HashMap<String, walrus::FunctionId>,
    /// Return layouts for callable names so field access on call results can stay typed.
    callable_layout_names: HashMap<String, String>,
    /// Top-level constants folded into wasm immediates.
    constants: HashMap<String, WasmConstValue>,
    /// Memory ID for linear memory
    memory_id: Option<walrus::MemoryId>,
    heap_ptr_global: walrus::GlobalId,
    /// Current offset in data segment for string allocation
    data_offset: u32,
    /// Map string literals to their memory offset (for deduplication)
    string_table: HashMap<String, u32>,
    /// Struct layouts: struct_name -> (field_name -> offset, total_size)
    struct_layouts: HashMap<String, (HashMap<String, u32>, u32)>,
    /// Struct field value types: struct_name -> (field_name -> wasm value type)
    struct_field_types: HashMap<String, HashMap<String, ValType>>,
    /// Nested layout names for struct fields that themselves point at another layout.
    struct_field_layout_names: HashMap<String, HashMap<String, String>>,
    /// Enum layouts: enum_name -> (variant_name -> tag, max_payload_size, variant_name -> (field_name -> offset))
    enum_layouts: HashMap<
        String,
        (
            HashMap<String, u32>,
            u32,
            HashMap<String, HashMap<String, u32>>,
        ),
    >,
    /// Heap pointer (for runtime allocation) - starts after data segment
    // heap_ptr: u32, // Unused
    /// Funcref table for indirect calls (closures)
    funcref_table: Option<walrus::TableId>,
    /// Counter for generating unique lambda names
    lambda_counter: u32,
    /// Map lambda ID -> (table_index, func_id) for indirect calls
    lambda_table: HashMap<u32, (u32, walrus::FunctionId)>,
    /// Lowered world singleton pointers in linear memory.
    world_globals: HashMap<String, u32>,
    /// Typed actor definitions available to the wasm lane.
    actors: HashMap<String, TypedActor>,
    /// Compiled synchronous actor handler entry points for wasm ask/send lowering.
    actor_handlers: HashMap<String, WasmActorHandler>,
}

#[derive(Debug, Clone, Copy, PartialEq)]
enum WasmConstValue {
    I64(i64),
    I32(i32),
    F64(f64),
}

#[derive(Debug, Clone)]
struct WasmActorHandler {
    actor_name: String,
    message_name: String,
    body: Block,
    params: Vec<(String, ValType)>,
    reply_ports: HashMap<String, ValType>,
    result_type: Option<ValType>,
    func_id: walrus::FunctionId,
    span: kain_core::span::Span,
}

impl WasmConstValue {
    fn val_type(self) -> ValType {
        match self {
            WasmConstValue::I64(_) => ValType::I64,
            WasmConstValue::I32(_) => ValType::I32,
            WasmConstValue::F64(_) => ValType::F64,
        }
    }

    fn truthy(self) -> bool {
        match self {
            WasmConstValue::I64(value) => value != 0,
            WasmConstValue::I32(value) => value != 0,
            WasmConstValue::F64(value) => value != 0.0,
        }
    }

    fn as_i64(self) -> Option<i64> {
        match self {
            WasmConstValue::I64(value) => Some(value),
            WasmConstValue::I32(value) => Some(value as i64),
            WasmConstValue::F64(_) => None,
        }
    }

    fn as_f64(self) -> f64 {
        match self {
            WasmConstValue::I64(value) => value as f64,
            WasmConstValue::I32(value) => value as f64,
            WasmConstValue::F64(value) => value,
        }
    }
}

// Separate Context from Builder to avoid self-borrow issues
// Locals are pre-allocated, so we don't need mutable access during emission
struct CompilationContext<'a> {
    locals: HashMap<String, LocalId>,
    actor_locals: HashMap<String, String>,
    layout_locals: HashMap<String, String>,
    reply_ports: HashMap<String, ValType>,
    functions: &'a HashMap<String, walrus::FunctionId>,
    constants: &'a HashMap<String, WasmConstValue>,
    string_table: &'a HashMap<String, u32>,
    struct_layouts: &'a HashMap<String, (HashMap<String, u32>, u32)>,
    struct_field_types: &'a HashMap<String, HashMap<String, ValType>>,
    struct_field_layout_names: &'a HashMap<String, HashMap<String, String>>,
    enum_layouts: &'a HashMap<
        String,
        (
            HashMap<String, u32>,
            u32,
            HashMap<String, HashMap<String, u32>>,
        ),
    >,
    memory_id: walrus::MemoryId,
    heap_ptr_global: walrus::GlobalId,
    world_globals: &'a HashMap<String, u32>,
    tmp_i32: LocalId,
    tmp_i32_2: LocalId,
    tmp_i64: LocalId,
    tmp_f64: LocalId,
    #[allow(dead_code)]
    funcref_table: Option<walrus::TableId>,
    lambda_table: &'a HashMap<u32, (u32, walrus::FunctionId)>,
}

impl WasmCompiler {
    fn compile_print_like_args(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        args: &[CallArg],
    ) -> KainResult<()> {
        for arg in args {
            match &arg.value {
                Expr::Int(_, _) => {
                    self.compile_expr(ctx, builder, &arg.value)?;
                    if let Some(func_id) = ctx.functions.get("print_i64") {
                        builder.call(*func_id);
                    }
                }
                Expr::Float(_, _) => {
                    self.compile_expr(ctx, builder, &arg.value)?;
                    if let Some(func_id) = ctx.functions.get("print_f64") {
                        builder.call(*func_id);
                    }
                }
                Expr::Bool(_, _) => {
                    self.compile_expr(ctx, builder, &arg.value)?;
                    if let Some(func_id) = ctx.functions.get("print_bool") {
                        builder.call(*func_id);
                    }
                }
                Expr::String(s, _) => {
                    if let Some(&offset) = ctx.string_table.get(s) {
                        builder.i32_const((offset + 4) as i32);
                        builder.i32_const(s.len() as i32);
                        if let Some(func_id) = ctx.functions.get("print_str") {
                            builder.call(*func_id);
                        }
                    }
                }
                _ => {
                    let is_string = self.is_string_expr(&arg.value);
                    let is_i32_var = match &arg.value {
                        Expr::Ident(name, _) => self.is_i32_local(name, &ctx.locals),
                        _ => self.is_i32_expr(&arg.value),
                    };

                    self.compile_expr(ctx, builder, &arg.value)?;

                    if is_string {
                        builder.local_set(ctx.tmp_i32);
                        builder.local_get(ctx.tmp_i32);
                        builder.local_get(ctx.tmp_i32);
                        builder.i32_const(4);
                        builder.binop(walrus::ir::BinaryOp::I32Sub);
                        builder.load(
                            ctx.memory_id,
                            walrus::ir::LoadKind::I32 { atomic: false },
                            walrus::ir::MemArg {
                                align: 4,
                                offset: 0,
                            },
                        );
                        if let Some(func_id) = ctx.functions.get("print_str") {
                            builder.call(*func_id);
                        }
                    } else if is_i32_var {
                        builder.drop();
                    } else if let Some(func_id) = ctx.functions.get("print_i64") {
                        builder.call(*func_id);
                    }
                }
            }
        }
        Ok(())
    }

    fn compile_expr_as_i32(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        expr: &Expr,
    ) -> KainResult<()> {
        let source_ty = self.infer_expr_wasm_type_in_context(ctx, expr);
        self.compile_expr(ctx, builder, expr)?;
        self.coerce_stack_to_val_type(builder, source_ty, ValType::I32);
        Ok(())
    }

    fn compile_expr_as_i64(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        expr: &Expr,
    ) -> KainResult<()> {
        let source_ty = self.infer_expr_wasm_type_in_context(ctx, expr);
        self.compile_expr(ctx, builder, expr)?;
        self.coerce_stack_to_val_type(builder, source_ty, ValType::I64);
        Ok(())
    }

    fn builtin_math_call_result_type(
        &self,
        func_name: &str,
        arg_types: &[ValType],
    ) -> Option<ValType> {
        match func_name {
            "abs" => Some(if arg_types.first().copied() == Some(ValType::F64) {
                ValType::F64
            } else {
                ValType::I64
            }),
            "sqrt" | "floor" | "ceil" | "round" | "sin" | "cos" | "tan" | "pow" => {
                Some(ValType::F64)
            }
            _ => None,
        }
    }

    fn primitive_call_result_type(&self, func_name: &str) -> Option<ValType> {
        match func_name {
            "Bool" | "bool" | "Char" => Some(ValType::I32),
            "Float32" | "F32" => Some(ValType::F32),
            "Float" | "Float64" | "F64" => Some(ValType::F64),
            "Int" | "I64" | "U64" | "Isize" | "Usize" | "I32" | "U32" | "I16" | "U16"
            | "I8" | "U8" => Some(ValType::I64),
            _ => None,
        }
    }

    fn compile_primitive_cast_call(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        func_name: &str,
        args: &[CallArg],
    ) -> KainResult<bool> {
        let Some(target_ty) = self.primitive_call_result_type(func_name) else {
            return Ok(false);
        };
        if args.len() != 1 {
            return Ok(false);
        }

        let value = &args[0].value;
        let source_ty = self.infer_expr_wasm_type_in_context(ctx, value);
        self.compile_expr(ctx, builder, value)?;

        if func_name.eq_ignore_ascii_case("bool") {
            match source_ty {
                ValType::I64 => {
                    builder.i64_const(0);
                    builder.binop(walrus::ir::BinaryOp::I64Ne);
                }
                ValType::F64 => {
                    builder.f64_const(0.0);
                    builder.binop(walrus::ir::BinaryOp::F64Ne);
                }
                ValType::F32 => {
                    builder.f32_const(0.0);
                    builder.binop(walrus::ir::BinaryOp::F32Ne);
                }
                _ => {}
            }
        } else {
            self.coerce_stack_to_val_type(builder, source_ty, target_ty);
        }

        Ok(true)
    }

    fn compile_builtin_math_call(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        func_name: &str,
        args: &[CallArg],
    ) -> KainResult<bool> {
        match func_name {
            "abs" if args.len() == 1 => {
                let arg_ty = self.infer_expr_wasm_type_in_context(ctx, &args[0].value);
                if arg_ty == ValType::F64 {
                    self.compile_expr(ctx, builder, &args[0].value)?;
                    self.coerce_stack_to_val_type(builder, arg_ty, ValType::F64);
                    builder.unop(walrus::ir::UnaryOp::F64Abs);
                } else {
                    self.compile_expr_as_i64(ctx, builder, &args[0].value)?;
                    builder.local_set(ctx.tmp_i64);
                    builder.local_get(ctx.tmp_i64);
                    builder.i64_const(0);
                    builder.binop(walrus::ir::BinaryOp::I64LtS);
                    builder.if_else(
                        ValType::I64,
                        |then_builder| {
                            then_builder.i64_const(0);
                            then_builder.local_get(ctx.tmp_i64);
                            then_builder.binop(walrus::ir::BinaryOp::I64Sub);
                        },
                        |else_builder| {
                            else_builder.local_get(ctx.tmp_i64);
                        },
                    );
                }
                Ok(true)
            }
            "sqrt" if args.len() == 1 => {
                let arg_ty = self.infer_expr_wasm_type_in_context(ctx, &args[0].value);
                self.compile_expr(ctx, builder, &args[0].value)?;
                self.coerce_stack_to_val_type(builder, arg_ty, ValType::F64);
                builder.unop(walrus::ir::UnaryOp::F64Sqrt);
                Ok(true)
            }
            "floor" if args.len() == 1 => {
                let arg_ty = self.infer_expr_wasm_type_in_context(ctx, &args[0].value);
                self.compile_expr(ctx, builder, &args[0].value)?;
                self.coerce_stack_to_val_type(builder, arg_ty, ValType::F64);
                builder.unop(walrus::ir::UnaryOp::F64Floor);
                Ok(true)
            }
            "ceil" if args.len() == 1 => {
                let arg_ty = self.infer_expr_wasm_type_in_context(ctx, &args[0].value);
                self.compile_expr(ctx, builder, &args[0].value)?;
                self.coerce_stack_to_val_type(builder, arg_ty, ValType::F64);
                builder.unop(walrus::ir::UnaryOp::F64Ceil);
                Ok(true)
            }
            _ => Ok(false),
        }
    }

    fn install_builtin_vector_layouts(&mut self) {
        for (layout_name, fields) in [
            ("Vec2", &["x", "y"][..]),
            ("Vec3", &["x", "y", "z"][..]),
            ("Vec4", &["x", "y", "z", "w"][..]),
        ] {
            let mut offsets = HashMap::new();
            let mut types = HashMap::new();
            for (index, field_name) in fields.iter().enumerate() {
                offsets.insert((*field_name).to_string(), (index as u32) * 8);
                types.insert((*field_name).to_string(), ValType::F64);
            }
            self.struct_layouts
                .insert(layout_name.to_string(), (offsets, (fields.len() as u32) * 8));
            self.struct_field_types
                .insert(layout_name.to_string(), types);
        }
    }

    fn compile_c_runtime_call(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        func_name: &str,
        args: &[CallArg],
    ) -> KainResult<bool> {
        let Some(shim) = wasm_c_runtime_shim(func_name) else {
            return Ok(false);
        };

        match shim.kind {
            WasmCRuntimeShimKind::Printf => {
                let print_args: Vec<CallArg> = args.iter().skip(1).cloned().collect();
                self.compile_print_like_args(ctx, builder, &print_args)?;
                builder.i64_const(0);
                Ok(true)
            }
            WasmCRuntimeShimKind::Fprintf => {
                let print_args: Vec<CallArg> = args.iter().skip(2).cloned().collect();
                self.compile_print_like_args(ctx, builder, &print_args)?;
                builder.i64_const(0);
                Ok(true)
            }
            WasmCRuntimeShimKind::Puts => {
                let print_args: Vec<CallArg> = args.iter().take(1).cloned().collect();
                self.compile_print_like_args(ctx, builder, &print_args)?;
                if let Some(&offset) = ctx.string_table.get("\n") {
                    builder.i32_const((offset + 4) as i32);
                    builder.i32_const(1);
                    if let Some(func_id) = ctx.functions.get("print_str") {
                        builder.call(*func_id);
                    }
                }
                builder.i64_const(0);
                Ok(true)
            }
            WasmCRuntimeShimKind::Atoll => {
                if let Some(arg) = args.first() {
                    self.compile_expr_as_i32(ctx, builder, &arg.value)?;
                } else {
                    builder.i32_const(0);
                }
                if let Some(func_id) = ctx.functions.get(func_name) {
                    builder.call(*func_id);
                }
                Ok(true)
            }
            WasmCRuntimeShimKind::Sprintf => {
                if let Some(dst) = args.first() {
                    self.compile_expr_as_i32(ctx, builder, &dst.value)?;
                } else {
                    builder.i32_const(0);
                }
                if let Some(fmt) = args.get(1) {
                    self.compile_expr_as_i32(ctx, builder, &fmt.value)?;
                } else {
                    builder.i32_const(0);
                }
                if let Some(value) = args.get(2) {
                    self.compile_expr_as_i64(ctx, builder, &value.value)?;
                } else {
                    builder.i64_const(0);
                }
                if let Some(func_id) = ctx.functions.get(func_name) {
                    builder.call(*func_id);
                } else {
                    builder.i32_const(0);
                }
                builder.unop(walrus::ir::UnaryOp::I64ExtendSI32);
                Ok(true)
            }
            WasmCRuntimeShimKind::Exit => {
                if let Some(arg) = args.first() {
                    self.compile_expr(ctx, builder, &arg.value)?;
                    builder.unop(walrus::ir::UnaryOp::I32WrapI64);
                } else {
                    builder.i32_const(0);
                }
                if let Some(func_id) = ctx.functions.get(func_name) {
                    builder.call(*func_id);
                }
                builder.i64_const(0);
                Ok(true)
            }
            WasmCRuntimeShimKind::Free => {
                if let Some(arg) = args.first() {
                    self.compile_expr(ctx, builder, &arg.value)?;
                } else {
                    builder.i32_const(0);
                }
                if let Some(func_id) = ctx.functions.get(func_name) {
                    builder.call(*func_id);
                }
                builder.i64_const(0);
                Ok(true)
            }
            WasmCRuntimeShimKind::Fopen => {
                if let Some(path) = args.first() {
                    self.compile_expr_as_i32(ctx, builder, &path.value)?;
                } else {
                    builder.i32_const(0);
                }
                if let Some(mode) = args.get(1) {
                    self.compile_expr_as_i32(ctx, builder, &mode.value)?;
                } else {
                    builder.i32_const(0);
                }
                if let Some(func_id) = ctx.functions.get(func_name) {
                    builder.call(*func_id);
                } else {
                    builder.i64_const(0);
                }
                Ok(true)
            }
            WasmCRuntimeShimKind::Fseek => {
                for idx in 0..3 {
                    if let Some(arg) = args.get(idx) {
                        self.compile_expr_as_i64(ctx, builder, &arg.value)?;
                    } else {
                        builder.i64_const(0);
                    }
                }
                if let Some(func_id) = ctx.functions.get(func_name) {
                    builder.call(*func_id);
                } else {
                    builder.i64_const(0);
                }
                Ok(true)
            }
            WasmCRuntimeShimKind::Ftell | WasmCRuntimeShimKind::Fclose => {
                if let Some(arg) = args.first() {
                    self.compile_expr_as_i64(ctx, builder, &arg.value)?;
                } else {
                    builder.i64_const(0);
                }
                if let Some(func_id) = ctx.functions.get(func_name) {
                    builder.call(*func_id);
                } else {
                    builder.i64_const(0);
                }
                Ok(true)
            }
            WasmCRuntimeShimKind::Fread | WasmCRuntimeShimKind::Fwrite => {
                for idx in 0..4 {
                    if let Some(arg) = args.get(idx) {
                        self.compile_expr_as_i64(ctx, builder, &arg.value)?;
                    } else {
                        builder.i64_const(0);
                    }
                }
                if let Some(func_id) = ctx.functions.get(func_name) {
                    builder.call(*func_id);
                } else {
                    builder.i64_const(0);
                }
                Ok(true)
            }
            WasmCRuntimeShimKind::Strncpy => {
                for idx in 0..3 {
                    if let Some(arg) = args.get(idx) {
                        self.compile_expr_as_i64(ctx, builder, &arg.value)?;
                    } else {
                        builder.i64_const(0);
                    }
                }
                if let Some(func_id) = ctx.functions.get(func_name) {
                    builder.call(*func_id);
                } else {
                    builder.i64_const(0);
                }
                Ok(true)
            }
            WasmCRuntimeShimKind::Strdup => {
                if let Some(arg) = args.first() {
                    self.compile_expr_as_i32(ctx, builder, &arg.value)?;
                } else {
                    builder.i32_const(0);
                }
                if let Some(func_id) = ctx.functions.get(func_name) {
                    builder.call(*func_id);
                } else {
                    builder.i64_const(0);
                }
                Ok(true)
            }
            WasmCRuntimeShimKind::Strlen
            | WasmCRuntimeShimKind::Strcmp
            | WasmCRuntimeShimKind::Strcpy
            | WasmCRuntimeShimKind::Strcat => Ok(false),
        }
    }

    fn new() -> Self {
        let config = ModuleConfig::new();
        let mut module = Module::with_config(config);

        // Create linear memory (1 page = 64KB)
        // add_local(shared, memory64, initial, maximum, page_size_log2)
        let memory_id = module.memories.add_local(false, false, 1, None, None);
        module.exports.add("memory", memory_id);

        let heap_ptr = 4096u32;
        let heap_ptr_global = module.globals.add_local(
            ValType::I32,
            true,
            false, // shared
            walrus::ConstExpr::Value(walrus::ir::Value::I32(heap_ptr as i32)),
        );

        // --- WASM Host Imports for I/O ---
        let mut functions = HashMap::new();

        // print_i64(value: i64) -> void
        let print_i64_type = module.types.add(&[ValType::I64], &[]);
        let (print_i64_func, _) = module.add_import_func("host", "print_i64", print_i64_type);
        functions.insert("print_i64".to_string(), print_i64_func);

        // print_f64(value: f64) -> void
        let print_f64_type = module.types.add(&[ValType::F64], &[]);
        let (print_f64_func, _) = module.add_import_func("host", "print_f64", print_f64_type);
        functions.insert("print_f64".to_string(), print_f64_func);

        // print_str(ptr: i32, len: i32) -> void
        let print_str_type = module.types.add(&[ValType::I32, ValType::I32], &[]);
        let (print_str_func, _) = module.add_import_func("host", "print_str", print_str_type);
        functions.insert("print_str".to_string(), print_str_func);

        // print_bool(value: i32) -> void
        let print_bool_type = module.types.add(&[ValType::I32], &[]);
        let (print_bool_func, _) = module.add_import_func("host", "print_bool", print_bool_type);
        functions.insert("print_bool".to_string(), print_bool_func);

        // read_i64() -> i64
        let read_i64_type = module.types.add(&[], &[ValType::I64]);
        let (read_i64_func, _) = module.add_import_func("host", "read_i64", read_i64_type);
        functions.insert("read_i64".to_string(), read_i64_func);

        // int_to_str(val: i64) -> ptr: i32
        let int_to_str_type = module.types.add(&[ValType::I64], &[ValType::I32]);
        let (int_to_str_func, _) = module.add_import_func("host", "int_to_str", int_to_str_type);
        functions.insert("int_to_str".to_string(), int_to_str_func);

        // str_concat(ptr1: i32, len1: i32, ptr2: i32, len2: i32) -> ptr: i32
        // Note: For simplicity, we'll assume strings are just pointers in this specific hack,
        // but robustly we need lengths.
        // If our runtime strings are (ptr, len), we can't easily pass them as single values.
        // Let's assume the host handles "String Objects" via pointers for concatenation.
        // BUT `print_str` takes (ptr, len).
        // Let's change strategy: strings are pointers to [len: i32, data...].
        // So we just pass pointers.
        let str_concat_type = module
            .types
            .add(&[ValType::I32, ValType::I32], &[ValType::I32]);
        let (str_concat_func, _) = module.add_import_func("host", "str_concat", str_concat_type);
        functions.insert("str_concat".to_string(), str_concat_func);

        // str_eq(ptr1: i32, ptr2: i32) -> bool: i32
        let str_eq_type = module
            .types
            .add(&[ValType::I32, ValType::I32], &[ValType::I32]);
        let (str_eq_func, _) = module.add_import_func("host", "str_eq", str_eq_type);
        functions.insert("str_eq".to_string(), str_eq_func);

        // char_at(ptr: i32, index: i64) -> ptr: i32
        let char_at_type = module
            .types
            .add(&[ValType::I32, ValType::I64], &[ValType::I32]);
        let (char_at_func, _) = module.add_import_func("host", "char_at", char_at_type);
        functions.insert("char_at".to_string(), char_at_func);

        // time_now() -> i64
        let time_now_type = module.types.add(&[], &[ValType::I64]);
        let (time_now_func, _) = module.add_import_func("host", "time_now", time_now_type);
        functions.insert("time_now".to_string(), time_now_func);

        let round_type = module.types.add(&[ValType::F64], &[ValType::F64]);
        let (round_func, _) = module.add_import_func("host", "round", round_type);
        functions.insert("round".to_string(), round_func);

        let sin_type = module.types.add(&[ValType::F64], &[ValType::F64]);
        let (sin_func, _) = module.add_import_func("host", "sin", sin_type);
        functions.insert("sin".to_string(), sin_func);

        let cos_type = module.types.add(&[ValType::F64], &[ValType::F64]);
        let (cos_func, _) = module.add_import_func("host", "cos", cos_type);
        functions.insert("cos".to_string(), cos_func);

        let tan_type = module.types.add(&[ValType::F64], &[ValType::F64]);
        let (tan_func, _) = module.add_import_func("host", "tan", tan_type);
        functions.insert("tan".to_string(), tan_func);

        let pow_type = module.types.add(&[ValType::F64, ValType::F64], &[ValType::F64]);
        let (pow_func, _) = module.add_import_func("host", "pow", pow_type);
        functions.insert("pow".to_string(), pow_func);

        let fmod_type = module.types.add(&[ValType::F64, ValType::F64], &[ValType::F64]);
        let (fmod_func, _) = module.add_import_func("host", "fmod", fmod_type);
        functions.insert("fmod".to_string(), fmod_func);

        for shim in crate::c_runtime_shims::WASM_C_RUNTIME_SHIMS {
            let Some(host_symbol) = shim.host_symbol else {
                continue;
            };
            let Some(signature) = shim.signature else {
                continue;
            };
            let (params, results) = wasm_import_signature_types(signature);
            let import_type = module.types.add(params, results);
            let (func_id, _) = module.add_import_func("host", host_symbol, import_type);
            functions.insert(shim.c_symbol.to_string(), func_id);
        }

        // --- DOM Imports ---
        // dom_create(tag_ptr: i32, tag_len: i32) -> node_id: i32
        let dom_create_type = module
            .types
            .add(&[ValType::I32, ValType::I32], &[ValType::I32]);
        let (dom_create_func, _) = module.add_import_func("host", "dom_create", dom_create_type);
        functions.insert("dom_create".to_string(), dom_create_func);

        // dom_append(parent_id: i32, child_id: i32) -> void
        let dom_append_type = module.types.add(&[ValType::I32, ValType::I32], &[]);
        let (dom_append_func, _) = module.add_import_func("host", "dom_append", dom_append_type);
        functions.insert("dom_append".to_string(), dom_append_func);

        // dom_attr(node_id: i32, key_ptr: i32, key_len: i32, val_ptr: i32, val_len: i32) -> void
        let dom_attr_type = module.types.add(
            &[
                ValType::I32,
                ValType::I32,
                ValType::I32,
                ValType::I32,
                ValType::I32,
            ],
            &[],
        );
        let (dom_attr_func, _) = module.add_import_func("host", "dom_attr", dom_attr_type);
        functions.insert("dom_attr".to_string(), dom_attr_func);

        // dom_text(text_ptr: i32, text_len: i32) -> node_id: i32
        let dom_text_type = module
            .types
            .add(&[ValType::I32, ValType::I32], &[ValType::I32]);
        let (dom_text_func, _) = module.add_import_func("host", "dom_text", dom_text_type);
        functions.insert("dom_text".to_string(), dom_text_func);

        // Create funcref table for closures/lambdas
        // Starts with 16 slots, can grow as needed
        let funcref_table = module
            .tables
            .add_local(false, 16, Some(256), walrus::RefType::Funcref);

        let mut compiler = Self {
            module,
            functions,
            callable_layout_names: HashMap::new(),
            constants: HashMap::new(),
            memory_id: Some(memory_id),
            heap_ptr_global,
            data_offset: 0,
            string_table: HashMap::new(),
            struct_layouts: HashMap::new(),
            struct_field_types: HashMap::new(),
            struct_field_layout_names: HashMap::new(),
            enum_layouts: HashMap::new(),
            // heap_ptr, // Unused
            funcref_table: Some(funcref_table),
            lambda_counter: 0,
            lambda_table: HashMap::new(),
            world_globals: HashMap::new(),
            actors: HashMap::new(),
            actor_handlers: HashMap::new(),
        };
        compiler.install_builtin_vector_layouts();
        compiler
    }

    fn compile_program(&mut self, program: &TypedProgram) -> KainResult<()> {
        // First pass: fold top-level constants that can be represented as wasm immediates.
        // LLVM already treats these as compile-time values; wasm must do the same to keep
        // ordinary benchmark and proof blades target-portable.
        for item in &program.items {
            if let TypedItem::Const(constant) = item {
                if let Some(value) = self.try_eval_const_expr(&constant.ast.value) {
                    self.constants.insert(constant.ast.name.clone(), value);
                }
            }
        }

        for item in &program.items {
            match item {
                TypedItem::Function(f) => {
                    self.register_resolved_callable_layout(&f.ast.name, &f.resolved_type);
                }
                TypedItem::Patch(p) => {
                    self.register_resolved_callable_layout(&p.ast.name, &p.resolved_type);
                }
                TypedItem::Law(l) => {
                    self.register_resolved_callable_layout(&l.ast.name, &l.resolved_type);
                }
                TypedItem::Converge(c) => {
                    self.register_resolved_callable_layout(&c.ast.name, &c.resolved_type);
                }
                TypedItem::Orchestrate(o) => {
                    self.register_resolved_callable_layout(&o.ast.name, &o.resolved_type);
                }
                TypedItem::Impl(i) => {
                    for method in &i.ast.methods {
                        let qualified = self.impl_method_name(&i.ast.target_type, &method.name);
                        self.register_authored_callable_layout(
                            &qualified,
                            method.return_type.as_ref(),
                        );
                    }
                }
                TypedItem::Actor(actor) => {
                    for method in &actor.ast.methods {
                        let qualified = format!("{}.{}", actor.ast.name, method.name);
                        self.register_authored_callable_layout(
                            &qualified,
                            method.return_type.as_ref(),
                        );
                    }
                }
                _ => {}
            }
        }

        // First pass: collect struct layouts
        for item in &program.items {
            if let TypedItem::Struct(s) = item {
                self.compute_struct_layout(s);
            }
            if let TypedItem::Component(c) = item {
                self.compute_component_layout(c);
            }
            if let TypedItem::World(w) = item {
                self.compute_world_layout(w);
            }
            if let TypedItem::Actor(actor) = item {
                self.actors.insert(actor.ast.name.clone(), actor.clone());
                self.compute_actor_layout(actor);
            }
        }

        // Second pass: collect enum layouts
        for item in &program.items {
            if let TypedItem::Enum(e) = item {
                self.compute_enum_layout(e);
            }
        }

        // Third pass: collect all string literals
        for item in &program.items {
            match item {
                TypedItem::Function(f) => self.collect_strings_in_block(&f.ast.body),
                TypedItem::Patch(p) => self.collect_strings_in_block(&p.ast.body),
                TypedItem::Law(l) => self.collect_strings_in_block(&l.ast.body),
                TypedItem::Converge(c) => {
                    self.collect_strings_in_block(&c.ast.spec_lane.body);
                    for lane in &c.ast.fast_lanes {
                        self.collect_strings_in_block(&lane.body);
                    }
                }
                TypedItem::Orchestrate(o) => self.collect_strings_in_block(&o.ast.body),
                TypedItem::World(w) => {
                    for state in &w.ast.states {
                        self.collect_strings_in_expr(&state.initial);
                    }
                }
                TypedItem::Impl(i) => {
                    for method in &i.ast.methods {
                        self.collect_strings_in_block(&method.body);
                    }
                }
                TypedItem::Actor(actor) => {
                    for handler in &actor.ast.handlers {
                        self.collect_strings_in_block(&handler.body);
                    }
                    for method in &actor.ast.methods {
                        self.collect_strings_in_block(&method.body);
                    }
                }
                _ => {}
            }
        }

        for item in &program.items {
            if let TypedItem::World(w) = item {
                self.allocate_world(w)?;
            }
        }

        // Fourth pass: collect and compile all lambdas
        let mut all_lambdas = Vec::new();
        for item in &program.items {
            match item {
                TypedItem::Function(f) => {
                    self.collect_lambdas_in_block(&f.ast.body, &mut all_lambdas)
                }
                TypedItem::Patch(p) => self.collect_lambdas_in_block(&p.ast.body, &mut all_lambdas),
                TypedItem::Law(l) => self.collect_lambdas_in_block(&l.ast.body, &mut all_lambdas),
                TypedItem::Converge(c) => {
                    self.collect_lambdas_in_block(&c.ast.spec_lane.body, &mut all_lambdas);
                    for lane in &c.ast.fast_lanes {
                        self.collect_lambdas_in_block(&lane.body, &mut all_lambdas);
                    }
                }
                TypedItem::Orchestrate(o) => {
                    self.collect_lambdas_in_block(&o.ast.body, &mut all_lambdas)
                }
                TypedItem::Impl(i) => {
                    for method in &i.ast.methods {
                        self.collect_lambdas_in_block(&method.body, &mut all_lambdas);
                    }
                }
                TypedItem::Actor(actor) => {
                    for handler in &actor.ast.handlers {
                        self.collect_lambdas_in_block(&handler.body, &mut all_lambdas);
                    }
                    for method in &actor.ast.methods {
                        self.collect_lambdas_in_block(&method.body, &mut all_lambdas);
                    }
                }
                _ => {}
            }
        }
        // Compile each lambda to a WASM function
        for (id, params, body) in &all_lambdas {
            self.compile_lambda(*id, &params, &body)?;
        }

        // Fifth pass: declare functions (recursion support)
        for item in &program.items {
            match item {
                TypedItem::Function(f) => self.declare_function(f)?,
                TypedItem::Patch(p) => self.declare_patch(p)?,
                TypedItem::Law(l) => self.declare_law(l)?,
                TypedItem::Converge(c) => self.declare_converge(c)?,
                TypedItem::Orchestrate(o) => self.declare_orchestrate(o)?,
                TypedItem::Impl(i) => self.declare_impl_methods(i)?,
                TypedItem::Actor(actor) => self.declare_actor_handlers(actor)?,
                _ => {}
            }
        }

        // Fifth pass: compile function bodies
        for item in &program.items {
            match item {
                TypedItem::Function(f) => {
                    self.compile_function_body(f)?;
                }
                TypedItem::Patch(p) => {
                    self.compile_patch_body(p)?;
                }
                TypedItem::Law(l) => {
                    self.compile_law_body(l)?;
                }
                TypedItem::Converge(c) => {
                    self.compile_converge_body(c)?;
                }
                TypedItem::Orchestrate(o) => {
                    self.compile_orchestrate_body(o)?;
                }
                TypedItem::Impl(i) => {
                    self.compile_impl_methods(i)?;
                }
                TypedItem::Actor(actor) => {
                    self.compile_actor_handlers(actor)?;
                }
                _ => {}
            }
        }

        // Sixth pass: compile components
        for item in &program.items {
            if let TypedItem::Component(c) = item {
                self.compile_component(c)?;
            }
        }

        Ok(())
    }

    fn compute_struct_layout(&mut self, s: &kain_core::types::TypedStruct) {
        let mut offset = 0u32;
        let mut field_offsets = HashMap::new();
        let mut field_types = HashMap::new();
        let mut field_layout_names = HashMap::new();

        for field in &s.ast.fields {
            // Align to 4 bytes
            offset = (offset + 3) & !3;
            field_offsets.insert(field.name.clone(), offset);

            // Calculate field size based on type
            let resolved_type = s
                .field_types
                .get(&field.name)
                .cloned()
                .unwrap_or(ResolvedType::Int(kain_core::types::IntSize::I64));
            field_types.insert(
                field.name.clone(),
                self.map_heap_value_type_from_resolved_type(&resolved_type),
            );
            if let Some(layout_name) = self.layout_name_from_resolved_type(&resolved_type) {
                field_layout_names.insert(field.name.clone(), layout_name);
            }
            let field_size = self.type_size_of(&resolved_type);
            offset += field_size;
        }

        // Align total size to 4 bytes
        let total_size = (offset + 3) & !3;
        self.struct_layouts
            .insert(s.ast.name.clone(), (field_offsets, total_size));
        self.struct_field_types
            .insert(s.ast.name.clone(), field_types);
        if !field_layout_names.is_empty() {
            self.struct_field_layout_names
                .insert(s.ast.name.clone(), field_layout_names);
        }
    }

    fn compute_component_layout(&mut self, c: &kain_core::types::TypedComponent) {
        let mut offset = 0u32;
        let mut field_offsets = HashMap::new();
        let mut field_types = HashMap::new();
        let mut field_layout_names = HashMap::new();

        for state in &c.ast.state {
            // Align to 4 bytes
            offset = (offset + 3) & !3;
            field_offsets.insert(state.name.clone(), offset);

            let field_type = self.map_heap_value_type_from_authored_type(&state.ty);
            field_types.insert(state.name.clone(), field_type);
            if let Some(layout_name) = self.layout_name_from_authored_type(&state.ty) {
                field_layout_names.insert(state.name.clone(), layout_name);
            }
            offset += self.ast_type_size_of(&state.ty);
        }

        let total_size = (offset + 3) & !3;
        self.struct_layouts
            .insert(c.ast.name.clone(), (field_offsets, total_size));
        self.struct_field_types
            .insert(c.ast.name.clone(), field_types);
        if !field_layout_names.is_empty() {
            self.struct_field_layout_names
                .insert(c.ast.name.clone(), field_layout_names);
        }
    }

    fn compute_world_layout(&mut self, world: &kain_core::types::TypedWorld) {
        let mut offset = 0u32;
        let mut field_offsets = HashMap::new();
        let mut field_types = HashMap::new();
        let mut field_layout_names = HashMap::new();

        for state in &world.ast.states {
            offset = (offset + 3) & !3;
            field_offsets.insert(state.name.clone(), offset);
            field_types.insert(
                state.name.clone(),
                self.map_heap_value_type_from_authored_type(&state.ty),
            );
            if let Some(layout_name) = self.layout_name_from_authored_type(&state.ty) {
                field_layout_names.insert(state.name.clone(), layout_name);
            }
            offset += self.ast_type_size_of(&state.ty);
        }

        let total_size = (offset + 3) & !3;
        self.struct_layouts
            .insert(world.ast.name.clone(), (field_offsets, total_size));
        self.struct_field_types
            .insert(world.ast.name.clone(), field_types);
        if !field_layout_names.is_empty() {
            self.struct_field_layout_names
                .insert(world.ast.name.clone(), field_layout_names);
        }
    }

    fn compute_actor_layout(&mut self, actor: &TypedActor) {
        let mut offset = 0u32;
        let mut field_offsets = HashMap::new();
        let mut field_types = HashMap::new();
        let mut field_layout_names = HashMap::new();

        for state in &actor.ast.state {
            offset = (offset + 3) & !3;
            field_offsets.insert(state.name.clone(), offset);
            let resolved_type = actor
                .state_types
                .get(&state.name)
                .cloned()
                .unwrap_or_else(|| ResolvedType::Unknown);
            let field_type = if resolved_type == ResolvedType::Unknown {
                self.map_heap_value_type_from_authored_type(&state.ty)
            } else {
                self.map_heap_value_type_from_resolved_type(&resolved_type)
            };
            field_types.insert(state.name.clone(), field_type);
            if let Some(layout_name) = self
                .layout_name_from_resolved_type(&resolved_type)
                .or_else(|| self.layout_name_from_authored_type(&state.ty))
            {
                field_layout_names.insert(state.name.clone(), layout_name);
            }
            let field_size = actor
                .state_types
                .get(&state.name)
                .map(|ty| self.type_size_of(ty))
                .unwrap_or_else(|| self.ast_type_size_of(&state.ty));
            offset += field_size;
        }

        let total_size = (offset + 3) & !3;
        self.struct_layouts
            .insert(actor.ast.name.clone(), (field_offsets, total_size));
        self.struct_field_types
            .insert(actor.ast.name.clone(), field_types);
        if !field_layout_names.is_empty() {
            self.struct_field_layout_names
                .insert(actor.ast.name.clone(), field_layout_names);
        }
    }

    fn map_heap_value_type_from_resolved_type(&self, ty: &ResolvedType) -> ValType {
        match ty {
            ResolvedType::Bool | ResolvedType::Char => ValType::I32,
            ResolvedType::Int(_) => ValType::I64,
            ResolvedType::Float(kain_core::types::FloatSize::F32) => ValType::F32,
            ResolvedType::Float(kain_core::types::FloatSize::F64) => ValType::F64,
            ResolvedType::String
            | ResolvedType::Array(_, _)
            | ResolvedType::Slice(_)
            | ResolvedType::Tuple(_)
            | ResolvedType::Option(_)
            | ResolvedType::Result(_, _)
            | ResolvedType::Ref { .. }
            | ResolvedType::Ptr { .. }
            | ResolvedType::Function { .. }
            | ResolvedType::Struct(_, _)
            | ResolvedType::Enum(_, _) => ValType::I32,
            _ => self.map_type(ty),
        }
    }

    fn map_heap_value_type_from_authored_type(&self, ty: &kain_core::ast::Type) -> ValType {
        match ty {
            kain_core::ast::Type::Named { name, .. } => match name.as_str() {
                "Bool" | "Char" => ValType::I32,
                "Float32" | "F32" => ValType::F32,
                "Float" | "Float64" | "F64" => ValType::F64,
                "String" => ValType::I32,
                "Int" | "I64" | "U64" | "Isize" | "Usize" | "I32" | "U32" | "I16" | "U16"
                | "I8" | "U8" => ValType::I64,
                _ => ValType::I32,
            },
            kain_core::ast::Type::Array(_, _, _)
            | kain_core::ast::Type::Tuple(_, _)
            | kain_core::ast::Type::Ref { .. }
            | kain_core::ast::Type::Ptr { .. }
            | kain_core::ast::Type::Function { .. }
            | kain_core::ast::Type::Option(_, _)
            | kain_core::ast::Type::Result(_, _, _)
            | kain_core::ast::Type::Impl { .. } => ValType::I32,
            _ => ValType::I64,
        }
    }

    fn infer_global_field_val_type(&self, field: &str) -> Option<ValType> {
        let mut matches = self
            .struct_field_types
            .values()
            .filter_map(|fields| fields.get(field).copied());
        let first = matches.next()?;
        if matches.all(|next| next == first) {
            Some(first)
        } else {
            None
        }
    }

    fn layout_name_from_resolved_type(&self, ty: &ResolvedType) -> Option<String> {
        match ty {
            ResolvedType::Struct(name, _) => Some(name.clone()),
            ResolvedType::Tuple(items) => match items.len() {
                2 => Some("Vec2".to_string()),
                3 => Some("Vec3".to_string()),
                4 => Some("Vec4".to_string()),
                _ => None,
            },
            ResolvedType::Ref { inner, .. } | ResolvedType::Ptr { inner, .. } => {
                self.layout_name_from_resolved_type(inner)
            }
            _ => None,
        }
    }

    fn layout_name_from_authored_type(&self, ty: &kain_core::ast::Type) -> Option<String> {
        match ty {
            kain_core::ast::Type::Named { name, .. } => match name.as_str() {
                "Bool" | "Char" | "Float32" | "F32" | "Float" | "Float64" | "F64" | "String"
                | "Int" | "I64" | "U64" | "Isize" | "Usize" | "I32" | "U32" | "I16" | "U16"
                | "I8" | "U8" => None,
                _ => Some(name.clone()),
            },
            kain_core::ast::Type::Ref { inner, .. } | kain_core::ast::Type::Ptr { inner, .. } => {
                self.layout_name_from_authored_type(inner)
            }
            _ => None,
        }
    }

    fn layout_name_from_callable_resolved_type(&self, resolved_type: &ResolvedType) -> Option<String> {
        match resolved_type {
            ResolvedType::Function { ret, .. } => self.layout_name_from_resolved_type(ret),
            _ => None,
        }
    }

    fn register_resolved_callable_layout(&mut self, name: &str, resolved_type: &ResolvedType) {
        if let Some(layout_name) = self.layout_name_from_callable_resolved_type(resolved_type) {
            self.callable_layout_names
                .insert(name.to_string(), layout_name);
        }
    }

    fn register_authored_callable_layout(
        &mut self,
        name: &str,
        return_type: Option<&kain_core::ast::Type>,
    ) {
        if let Some(layout_name) =
            return_type.and_then(|ty| self.layout_name_from_authored_type(ty))
        {
            self.callable_layout_names
                .insert(name.to_string(), layout_name);
        }
    }

    fn resolve_method_layout_name(
        &self,
        receiver_layout: Option<String>,
        method: &str,
    ) -> Option<String> {
        if let Some(receiver_layout) = receiver_layout {
            let qualified = format!("{receiver_layout}.{method}");
            if let Some(layout_name) = self.callable_layout_names.get(&qualified) {
                return Some(layout_name.clone());
            }
        }
        self.callable_layout_names.get(method).cloned()
    }

    fn resolve_block_layout_name_in_layout_scope(
        &self,
        layout_locals: &HashMap<String, String>,
        block: &Block,
    ) -> Option<String> {
        match block.stmts.last()? {
            Stmt::Expr(expr) => self.resolve_layout_name_in_layout_scope(layout_locals, expr),
            Stmt::Return(Some(expr), _) | Stmt::Break(Some(expr), _) => {
                self.resolve_layout_name_in_layout_scope(layout_locals, expr)
            }
            _ => None,
        }
    }

    fn resolve_else_layout_name_in_layout_scope(
        &self,
        layout_locals: &HashMap<String, String>,
        branch: &kain_core::ast::ElseBranch,
    ) -> Option<String> {
        match branch {
            kain_core::ast::ElseBranch::Else(block) => {
                self.resolve_block_layout_name_in_layout_scope(layout_locals, block)
            }
            kain_core::ast::ElseBranch::ElseIf(_, then_branch, next) => self
                .merge_layout_candidates([
                    self.resolve_block_layout_name_in_layout_scope(layout_locals, then_branch),
                    next.as_ref().and_then(|next| {
                        self.resolve_else_layout_name_in_layout_scope(layout_locals, next)
                    }),
                ]),
        }
    }

    fn resolve_block_layout_name_in_context(
        &self,
        ctx: &CompilationContext,
        block: &Block,
    ) -> Option<String> {
        match block.stmts.last()? {
            Stmt::Expr(expr) => self.resolve_layout_name_in_context(ctx, expr),
            Stmt::Return(Some(expr), _) | Stmt::Break(Some(expr), _) => {
                self.resolve_layout_name_in_context(ctx, expr)
            }
            _ => None,
        }
    }

    fn resolve_else_layout_name_in_context(
        &self,
        ctx: &CompilationContext,
        branch: &kain_core::ast::ElseBranch,
    ) -> Option<String> {
        match branch {
            kain_core::ast::ElseBranch::Else(block) => {
                self.resolve_block_layout_name_in_context(ctx, block)
            }
            kain_core::ast::ElseBranch::ElseIf(_, then_branch, next) => self
                .merge_layout_candidates([
                    self.resolve_block_layout_name_in_context(ctx, then_branch),
                    next.as_ref()
                        .and_then(|next| self.resolve_else_layout_name_in_context(ctx, next)),
                ]),
        }
    }

    fn actor_handler_key(actor_name: &str, message_name: &str) -> String {
        format!("{actor_name}::{message_name}")
    }

    fn is_reply_port_type(&self, ty: &kain_core::ast::Type) -> bool {
        matches!(
            ty,
            kain_core::ast::Type::Named { name, .. }
                if matches!(name.as_str(), "P" | "ReplyPort" | "KainReplyPort")
        )
    }

    fn extract_reply_send_value_expr<'a>(
        &self,
        stmt: &'a Stmt,
        reply_port_name: &str,
    ) -> Option<&'a Expr> {
        match stmt {
            Stmt::Expr(Expr::SendMsg {
                target,
                message,
                data,
                ..
            }) => {
                if message != "Reply" {
                    return None;
                }
                match target.as_ref() {
                    Expr::Ident(name, _) if name == reply_port_name => data
                        .iter()
                        .find(|(field, _)| field == "value")
                        .map(|(_, value)| value)
                        .or_else(|| data.first().map(|(_, value)| value)),
                    _ => None,
                }
            }
            _ => None,
        }
    }

    fn infer_actor_expr_wasm_type(
        &self,
        actor: &TypedActor,
        locals: &HashMap<String, ValType>,
        expr: &Expr,
    ) -> ValType {
        match expr {
            Expr::Int(_, _) => ValType::I64,
            Expr::None(_) => ValType::I32,
            Expr::Float(_, _) => ValType::F64,
            Expr::Bool(_, _) => ValType::I32,
            Expr::String(_, _) => ValType::I32,
            Expr::Paren(inner, _) => self.infer_actor_expr_wasm_type(actor, locals, inner),
            Expr::Observe { body, .. } | Expr::Collapse { body, .. } => {
                self.infer_actor_expr_wasm_type(actor, locals, body)
            }
            Expr::Teleport { value, .. }
            | Expr::Comptime(value, _)
            | Expr::Await(value, _)
            | Expr::AsyncBlock(value, _)
            | Expr::Try(value, _) => self.infer_actor_expr_wasm_type(actor, locals, value),
            Expr::Cast { target, .. } | Expr::Bitcast { target, .. } => self.map_authored_type(target),
            Expr::Ident(name, _) => locals
                .get(name)
                .copied()
                .or_else(|| self.constants.get(name).map(|value| value.val_type()))
                .unwrap_or(ValType::I64),
            Expr::Field { object, field, .. } => match object.as_ref() {
                Expr::Ident(name, _) if name == "self" => actor
                    .state_types
                    .get(field)
                    .map(|ty| self.map_heap_value_type_from_resolved_type(ty))
                    .unwrap_or_else(|| {
                        self.infer_global_field_val_type(field)
                            .unwrap_or(ValType::I64)
                    }),
                _ => self
                    .infer_global_field_val_type(field)
                    .unwrap_or(ValType::I64),
            },
            Expr::Array(_, _)
            | Expr::Tuple(_, _)
            | Expr::Struct { .. }
            | Expr::AggregateInit { .. }
            | Expr::EnumVariant { .. }
            | Expr::Spawn { .. }
            | Expr::JSX(_, _) => ValType::I32,
            Expr::Binary {
                op, left, right, ..
            } => match op {
                BinaryOp::Eq
                | BinaryOp::Ne
                | BinaryOp::Lt
                | BinaryOp::Gt
                | BinaryOp::Le
                | BinaryOp::Ge
                | BinaryOp::And
                | BinaryOp::Or => ValType::I32,
                BinaryOp::Add => {
                    let left_ty = self.infer_actor_expr_wasm_type(actor, locals, left);
                    let right_ty = self.infer_actor_expr_wasm_type(actor, locals, right);
                    if left_ty == ValType::I32 && right_ty == ValType::I32 {
                        ValType::I32
                    } else if left_ty == ValType::F64 || right_ty == ValType::F64 {
                        ValType::F64
                    } else {
                        ValType::I64
                    }
                }
                _ => {
                    let left_ty = self.infer_actor_expr_wasm_type(actor, locals, left);
                    let right_ty = self.infer_actor_expr_wasm_type(actor, locals, right);
                    if left_ty == ValType::F64 || right_ty == ValType::F64 {
                        ValType::F64
                    } else {
                        ValType::I64
                    }
                }
            },
            Expr::Unary { op, operand, .. } => match op {
                kain_core::ast::UnaryOp::Not => ValType::I32,
                _ => self.infer_actor_expr_wasm_type(actor, locals, operand),
            },
            Expr::Call { callee, args, .. } => {
                if let Expr::Ident(name, _) = callee.as_ref() {
                    if let Some(cast_ty) = self.primitive_call_result_type(name) {
                        return cast_ty;
                    }
                    let arg_types: Vec<_> = args
                        .iter()
                        .map(|arg| self.infer_actor_expr_wasm_type(actor, locals, &arg.value))
                        .collect();
                    if let Some(builtin_ty) = self.builtin_math_call_result_type(name, &arg_types) {
                        return builtin_ty;
                    }
                    if matches!(
                        name.as_str(),
                        "to_string" | "str_concat" | "char_at" | "str_eq"
                    ) {
                        return ValType::I32;
                    }
                    if matches!(
                        name.as_str(),
                        "__kain_ptr_offset"
                            | "__kain_alloc"
                            | "__kain_realloc"
                            | "__kain_union_wrap"
                    ) {
                        return ValType::I32;
                    }
                }
                ValType::I64
            }
            Expr::MethodCall { method, args, .. } => match method.as_str() {
                "is_ok" | "is_err" | "is_some" | "is_none" => ValType::I32,
                "unwrap_or" => args
                    .first()
                    .map(|arg| self.infer_actor_expr_wasm_type(actor, locals, &arg.value))
                    .unwrap_or(ValType::I64),
                _ => ValType::I64,
            },
            _ => ValType::I64,
        }
    }

    fn infer_actor_handler_reply_type(
        &self,
        actor: &TypedActor,
        handler: &kain_core::ast::MessageHandler,
    ) -> Option<ValType> {
        let reply_port_name = handler
            .params
            .iter()
            .find(|param| self.is_reply_port_type(&param.ty))
            .map(|param| param.name.clone())?;

        let mut locals = HashMap::new();
        for param in &handler.params {
            locals.insert(param.name.clone(), self.map_authored_type(&param.ty));
        }

        handler
            .body
            .stmts
            .iter()
            .rev()
            .find_map(|stmt| self.extract_reply_send_value_expr(stmt, &reply_port_name))
            .map(|value| self.infer_actor_expr_wasm_type(actor, &locals, value))
    }

    fn collect_actor_locals_in_block(
        &self,
        block: &Block,
        actor_locals: &mut HashMap<String, String>,
    ) {
        for stmt in &block.stmts {
            self.collect_actor_locals_in_stmt(stmt, actor_locals);
        }
    }

    fn collect_actor_locals_in_stmt(
        &self,
        stmt: &Stmt,
        actor_locals: &mut HashMap<String, String>,
    ) {
        match stmt {
            Stmt::Let { pattern, value, .. } => {
                if let Some(value) = value {
                    if let (
                        kain_core::ast::Pattern::Binding { name, .. },
                        Expr::Spawn { actor, .. },
                    ) = (pattern, value)
                    {
                        actor_locals.insert(name.clone(), actor.clone());
                    } else {
                        self.collect_actor_locals_in_expr(value, actor_locals);
                    }
                }
            }
            Stmt::Expr(expr) => self.collect_actor_locals_in_expr(expr, actor_locals),
            Stmt::Return(Some(expr), _) | Stmt::Break(Some(expr), _) => {
                self.collect_actor_locals_in_expr(expr, actor_locals);
            }
            Stmt::For { iter, body, .. } | Stmt::Fanout { iter, body, .. } => {
                self.collect_actor_locals_in_expr(iter, actor_locals);
                self.collect_actor_locals_in_block(body, actor_locals);
            }
            Stmt::While {
                condition, body, ..
            } => {
                self.collect_actor_locals_in_expr(condition, actor_locals);
                self.collect_actor_locals_in_block(body, actor_locals);
            }
            Stmt::Loop { body, .. } => self.collect_actor_locals_in_block(body, actor_locals),
            _ => {}
        }
    }

    fn collect_actor_locals_in_else(
        &self,
        branch: &kain_core::ast::ElseBranch,
        actor_locals: &mut HashMap<String, String>,
    ) {
        match branch {
            kain_core::ast::ElseBranch::Else(block) => {
                self.collect_actor_locals_in_block(block, actor_locals);
            }
            kain_core::ast::ElseBranch::ElseIf(condition, then_branch, next) => {
                self.collect_actor_locals_in_expr(condition, actor_locals);
                self.collect_actor_locals_in_block(then_branch, actor_locals);
                if let Some(next) = next {
                    self.collect_actor_locals_in_else(next, actor_locals);
                }
            }
        }
    }

    fn collect_actor_locals_in_expr(
        &self,
        expr: &Expr,
        actor_locals: &mut HashMap<String, String>,
    ) {
        match expr {
            Expr::Spawn { .. }
            | Expr::Int(_, _)
            | Expr::Float(_, _)
            | Expr::String(_, _)
            | Expr::Bool(_, _)
            | Expr::None(_)
            | Expr::Ident(_, _)
            | Expr::SizeOfType { .. }
            | Expr::AlignOfType { .. }
            | Expr::Alloca { .. }
            | Expr::Uninit { .. } => {}
            Expr::Paren(inner, _)
            | Expr::Deref(inner, _)
            | Expr::Ref { value: inner, .. }
            | Expr::AddrOf { value: inner, .. }
            | Expr::Cast { value: inner, .. }
            | Expr::Bitcast { value: inner, .. }
            | Expr::Comptime(inner, _)
            | Expr::Await(inner, _)
            | Expr::AsyncBlock(inner, _)
            | Expr::Try(inner, _) => self.collect_actor_locals_in_expr(inner, actor_locals),
            Expr::Teleport { value, .. } => self.collect_actor_locals_in_expr(value, actor_locals),
            Expr::Binary { left, right, .. } => {
                self.collect_actor_locals_in_expr(left, actor_locals);
                self.collect_actor_locals_in_expr(right, actor_locals);
            }
            Expr::Unary { operand, .. } => self.collect_actor_locals_in_expr(operand, actor_locals),
            Expr::Assign { target, value, .. } => {
                if let (Expr::Ident(name, _), Expr::Spawn { actor, .. }) =
                    (target.as_ref(), value.as_ref())
                {
                    actor_locals.insert(name.clone(), actor.clone());
                }
                self.collect_actor_locals_in_expr(target, actor_locals);
                self.collect_actor_locals_in_expr(value, actor_locals);
            }
            Expr::Call { callee, args, .. } => {
                self.collect_actor_locals_in_expr(callee, actor_locals);
                for arg in args {
                    self.collect_actor_locals_in_expr(&arg.value, actor_locals);
                }
            }
            Expr::StageCall { args, .. } => {
                for arg in args {
                    self.collect_actor_locals_in_expr(&arg.value, actor_locals);
                }
            }
            Expr::MethodCall { receiver, args, .. } => {
                self.collect_actor_locals_in_expr(receiver, actor_locals);
                for arg in args {
                    self.collect_actor_locals_in_expr(&arg.value, actor_locals);
                }
            }
            Expr::Field { object, .. } => self.collect_actor_locals_in_expr(object, actor_locals),
            Expr::Index { object, index, .. } => {
                self.collect_actor_locals_in_expr(object, actor_locals);
                self.collect_actor_locals_in_expr(index, actor_locals);
            }
            Expr::Struct { fields, rest, .. } => {
                for (_, value) in fields {
                    self.collect_actor_locals_in_expr(value, actor_locals);
                }
                if let Some(rest) = rest {
                    self.collect_actor_locals_in_expr(rest, actor_locals);
                }
            }
            Expr::AggregateInit { fields, .. } => {
                for (_, value) in fields {
                    self.collect_actor_locals_in_expr(value, actor_locals);
                }
            }
            Expr::EnumVariant { fields, .. } => match fields {
                kain_core::ast::EnumVariantFields::Unit => {}
                kain_core::ast::EnumVariantFields::Tuple(values) => {
                    for value in values {
                        self.collect_actor_locals_in_expr(value, actor_locals);
                    }
                }
                kain_core::ast::EnumVariantFields::Struct(values) => {
                    for (_, value) in values {
                        self.collect_actor_locals_in_expr(value, actor_locals);
                    }
                }
            },
            Expr::Array(values, _) | Expr::Tuple(values, _) | Expr::FString(values, _) => {
                for value in values {
                    self.collect_actor_locals_in_expr(value, actor_locals);
                }
            }
            Expr::Range { start, end, .. } => {
                if let Some(start) = start {
                    self.collect_actor_locals_in_expr(start, actor_locals);
                }
                if let Some(end) = end {
                    self.collect_actor_locals_in_expr(end, actor_locals);
                }
            }
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                self.collect_actor_locals_in_expr(condition, actor_locals);
                self.collect_actor_locals_in_block(then_branch, actor_locals);
                if let Some(else_branch) = else_branch {
                    self.collect_actor_locals_in_else(else_branch, actor_locals);
                }
            }
            Expr::Match {
                scrutinee, arms, ..
            } => {
                self.collect_actor_locals_in_expr(scrutinee, actor_locals);
                for arm in arms {
                    if let Some(guard) = &arm.guard {
                        self.collect_actor_locals_in_expr(guard, actor_locals);
                    }
                    self.collect_actor_locals_in_expr(&arm.body, actor_locals);
                }
            }
            Expr::Observe { target, body, .. }
            | Expr::Collapse { target, body, .. }
            | Expr::Share { target, body, .. } => {
                self.collect_actor_locals_in_expr(target, actor_locals);
                self.collect_actor_locals_in_expr(body, actor_locals);
            }
            Expr::PtrOffset {
                pointer, offset, ..
            } => {
                self.collect_actor_locals_in_expr(pointer, actor_locals);
                self.collect_actor_locals_in_expr(offset, actor_locals);
            }
            Expr::MemLoad { pointer, .. }
            | Expr::VolatileLoad { pointer, .. }
            | Expr::AtomicLoad { pointer, .. }
            | Expr::CpuCacheFlush { pointer, .. }
            | Expr::Decay {
                target: pointer, ..
            }
            | Expr::Alloc { size: pointer, .. } => {
                self.collect_actor_locals_in_expr(pointer, actor_locals);
            }
            Expr::MemStore { pointer, value, .. }
            | Expr::VolatileStore { pointer, value, .. }
            | Expr::AtomicStore { pointer, value, .. }
            | Expr::AtomicAdd { pointer, value, .. }
            | Expr::AtomicSub { pointer, value, .. }
            | Expr::AtomicAnd { pointer, value, .. }
            | Expr::AtomicOr { pointer, value, .. }
            | Expr::AtomicXor { pointer, value, .. }
            | Expr::AtomicExchange { pointer, value, .. } => {
                self.collect_actor_locals_in_expr(pointer, actor_locals);
                self.collect_actor_locals_in_expr(value, actor_locals);
            }
            Expr::AtomicCompareExchange {
                pointer,
                expected,
                desired,
                ..
            } => {
                self.collect_actor_locals_in_expr(pointer, actor_locals);
                self.collect_actor_locals_in_expr(expected, actor_locals);
                self.collect_actor_locals_in_expr(desired, actor_locals);
            }
            Expr::AtomicFence { .. } | Expr::CpuFence { .. } => {}
            Expr::InlineAsm { operands, .. } => {
                for operand in operands {
                    self.collect_actor_locals_in_expr(operand, actor_locals);
                }
            }
            Expr::Realloc { pointer, size, .. } => {
                self.collect_actor_locals_in_expr(pointer, actor_locals);
                self.collect_actor_locals_in_expr(size, actor_locals);
            }
            Expr::SendMsg { target, data, .. } => {
                self.collect_actor_locals_in_expr(target, actor_locals);
                for (_, value) in data {
                    self.collect_actor_locals_in_expr(value, actor_locals);
                }
            }
            Expr::MacroCall { args, .. } => {
                for arg in args {
                    self.collect_actor_locals_in_expr(arg, actor_locals);
                }
            }
            Expr::Block(block, _) => self.collect_actor_locals_in_block(block, actor_locals),
            Expr::Return(Some(value), _) | Expr::Break(Some(value), _) => {
                self.collect_actor_locals_in_expr(value, actor_locals);
            }
            Expr::Return(None, _) | Expr::Break(None, _) | Expr::Continue(_) => {}
            Expr::Lambda { body, .. } => self.collect_actor_locals_in_expr(body, actor_locals),
            Expr::JSX(_, _) => {}
        }
    }

    fn collect_layout_locals_in_block(
        &self,
        block: &Block,
        layout_locals: &mut HashMap<String, String>,
    ) {
        for stmt in &block.stmts {
            self.collect_layout_locals_in_stmt(stmt, layout_locals);
        }
    }

    fn collect_layout_locals_in_stmt(
        &self,
        stmt: &Stmt,
        layout_locals: &mut HashMap<String, String>,
    ) {
        match stmt {
            Stmt::Let {
                pattern, ty, value, ..
            } => {
                if let kain_core::ast::Pattern::Binding { name, .. } = pattern {
                    let resolved_layout = value
                        .as_ref()
                        .and_then(|expr| {
                            self.resolve_layout_name_in_layout_scope(layout_locals, expr)
                        })
                        .or_else(|| {
                            ty.as_ref().and_then(|authored_ty| {
                                self.layout_name_from_authored_type(authored_ty)
                            })
                        });
                    if let Some(layout_name) = resolved_layout {
                        layout_locals.insert(name.clone(), layout_name);
                    } else {
                        layout_locals.remove(name);
                    }
                }
                if let Some(value) = value {
                    self.collect_layout_locals_in_expr(value, layout_locals);
                }
            }
            Stmt::Expr(expr) => self.collect_layout_locals_in_expr(expr, layout_locals),
            Stmt::Return(Some(expr), _) | Stmt::Break(Some(expr), _) => {
                self.collect_layout_locals_in_expr(expr, layout_locals);
            }
            Stmt::For { iter, body, .. } | Stmt::Fanout { iter, body, .. } => {
                self.collect_layout_locals_in_expr(iter, layout_locals);
                self.collect_layout_locals_in_block(body, layout_locals);
            }
            Stmt::While {
                condition, body, ..
            } => {
                self.collect_layout_locals_in_expr(condition, layout_locals);
                self.collect_layout_locals_in_block(body, layout_locals);
            }
            Stmt::Loop { body, .. } => self.collect_layout_locals_in_block(body, layout_locals),
            _ => {}
        }
    }

    fn collect_layout_locals_in_else(
        &self,
        branch: &kain_core::ast::ElseBranch,
        layout_locals: &mut HashMap<String, String>,
    ) {
        match branch {
            kain_core::ast::ElseBranch::Else(block) => {
                self.collect_layout_locals_in_block(block, layout_locals);
            }
            kain_core::ast::ElseBranch::ElseIf(condition, then_branch, next) => {
                self.collect_layout_locals_in_expr(condition, layout_locals);
                self.collect_layout_locals_in_block(then_branch, layout_locals);
                if let Some(next) = next {
                    self.collect_layout_locals_in_else(next, layout_locals);
                }
            }
        }
    }

    fn collect_layout_locals_in_expr(
        &self,
        expr: &Expr,
        layout_locals: &mut HashMap<String, String>,
    ) {
        match expr {
            Expr::Paren(inner, _)
            | Expr::Deref(inner, _)
            | Expr::Ref { value: inner, .. }
            | Expr::AddrOf { value: inner, .. }
            | Expr::Cast { value: inner, .. }
            | Expr::Bitcast { value: inner, .. }
            | Expr::Comptime(inner, _)
            | Expr::Await(inner, _)
            | Expr::AsyncBlock(inner, _)
            | Expr::Try(inner, _) => self.collect_layout_locals_in_expr(inner, layout_locals),
            Expr::Teleport { value, .. } => {
                self.collect_layout_locals_in_expr(value, layout_locals)
            }
            Expr::Binary { left, right, .. } => {
                self.collect_layout_locals_in_expr(left, layout_locals);
                self.collect_layout_locals_in_expr(right, layout_locals);
            }
            Expr::Unary { operand, .. } => {
                self.collect_layout_locals_in_expr(operand, layout_locals)
            }
            Expr::Assign { target, value, .. } => {
                if let Expr::Ident(name, _) = target.as_ref() {
                    if let Some(layout_name) =
                        self.resolve_layout_name_in_layout_scope(layout_locals, value)
                    {
                        layout_locals.insert(name.clone(), layout_name);
                    } else {
                        layout_locals.remove(name);
                    }
                }
                self.collect_layout_locals_in_expr(target, layout_locals);
                self.collect_layout_locals_in_expr(value, layout_locals);
            }
            Expr::Call { callee, args, .. } => {
                self.collect_layout_locals_in_expr(callee, layout_locals);
                for arg in args {
                    self.collect_layout_locals_in_expr(&arg.value, layout_locals);
                }
            }
            Expr::StageCall { args, .. } => {
                for arg in args {
                    self.collect_layout_locals_in_expr(&arg.value, layout_locals);
                }
            }
            Expr::MethodCall { receiver, args, .. } => {
                self.collect_layout_locals_in_expr(receiver, layout_locals);
                for arg in args {
                    self.collect_layout_locals_in_expr(&arg.value, layout_locals);
                }
            }
            Expr::Field { object, .. } => self.collect_layout_locals_in_expr(object, layout_locals),
            Expr::Index { object, index, .. } => {
                self.collect_layout_locals_in_expr(object, layout_locals);
                self.collect_layout_locals_in_expr(index, layout_locals);
            }
            Expr::Struct { fields, rest, .. } => {
                for (_, value) in fields {
                    self.collect_layout_locals_in_expr(value, layout_locals);
                }
                if let Some(rest) = rest {
                    self.collect_layout_locals_in_expr(rest, layout_locals);
                }
            }
            Expr::AggregateInit { fields, .. } => {
                for (_, value) in fields {
                    self.collect_layout_locals_in_expr(value, layout_locals);
                }
            }
            Expr::EnumVariant { fields, .. } => match fields {
                kain_core::ast::EnumVariantFields::Unit => {}
                kain_core::ast::EnumVariantFields::Tuple(items) => {
                    for item in items {
                        self.collect_layout_locals_in_expr(item, layout_locals);
                    }
                }
                kain_core::ast::EnumVariantFields::Struct(fields) => {
                    for (_, value) in fields {
                        self.collect_layout_locals_in_expr(value, layout_locals);
                    }
                }
            },
            Expr::Array(items, _) | Expr::Tuple(items, _) | Expr::FString(items, _) => {
                for item in items {
                    self.collect_layout_locals_in_expr(item, layout_locals);
                }
            }
            Expr::Range { start, end, .. } => {
                if let Some(start) = start {
                    self.collect_layout_locals_in_expr(start, layout_locals);
                }
                if let Some(end) = end {
                    self.collect_layout_locals_in_expr(end, layout_locals);
                }
            }
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                self.collect_layout_locals_in_expr(condition, layout_locals);
                self.collect_layout_locals_in_block(then_branch, layout_locals);
                if let Some(else_branch) = else_branch {
                    self.collect_layout_locals_in_else(else_branch, layout_locals);
                }
            }
            Expr::Match {
                scrutinee, arms, ..
            } => {
                self.collect_layout_locals_in_expr(scrutinee, layout_locals);
                for arm in arms {
                    if let Some(guard) = &arm.guard {
                        self.collect_layout_locals_in_expr(guard, layout_locals);
                    }
                    self.collect_layout_locals_in_expr(&arm.body, layout_locals);
                }
            }
            Expr::Observe { target, body, .. }
            | Expr::Collapse { target, body, .. }
            | Expr::Share { target, body, .. } => {
                self.collect_layout_locals_in_expr(target, layout_locals);
                self.collect_layout_locals_in_expr(body, layout_locals);
            }
            Expr::Decay { target, .. } => self.collect_layout_locals_in_expr(target, layout_locals),
            Expr::PtrOffset {
                pointer, offset, ..
            } => {
                self.collect_layout_locals_in_expr(pointer, layout_locals);
                self.collect_layout_locals_in_expr(offset, layout_locals);
            }
            Expr::MemLoad { pointer, .. }
            | Expr::VolatileLoad { pointer, .. }
            | Expr::AtomicLoad { pointer, .. } => {
                self.collect_layout_locals_in_expr(pointer, layout_locals);
            }
            Expr::CpuCacheFlush { pointer, .. } => {
                self.collect_layout_locals_in_expr(pointer, layout_locals);
            }
            Expr::MemStore { pointer, value, .. }
            | Expr::VolatileStore { pointer, value, .. }
            | Expr::AtomicStore { pointer, value, .. }
            | Expr::AtomicAdd { pointer, value, .. }
            | Expr::AtomicSub { pointer, value, .. }
            | Expr::AtomicAnd { pointer, value, .. }
            | Expr::AtomicOr { pointer, value, .. }
            | Expr::AtomicXor { pointer, value, .. }
            | Expr::AtomicExchange { pointer, value, .. } => {
                self.collect_layout_locals_in_expr(pointer, layout_locals);
                self.collect_layout_locals_in_expr(value, layout_locals);
            }
            Expr::AtomicCompareExchange {
                pointer,
                expected,
                desired,
                ..
            } => {
                self.collect_layout_locals_in_expr(pointer, layout_locals);
                self.collect_layout_locals_in_expr(expected, layout_locals);
                self.collect_layout_locals_in_expr(desired, layout_locals);
            }
            Expr::AtomicFence { .. } | Expr::CpuFence { .. } => {}
            Expr::InlineAsm { operands, .. } => {
                for operand in operands {
                    self.collect_layout_locals_in_expr(operand, layout_locals);
                }
            }
            Expr::Alloc { size, .. } => self.collect_layout_locals_in_expr(size, layout_locals),
            Expr::Realloc { pointer, size, .. } => {
                self.collect_layout_locals_in_expr(pointer, layout_locals);
                self.collect_layout_locals_in_expr(size, layout_locals);
            }
            Expr::Spawn { init, .. } => {
                for (_, value) in init {
                    self.collect_layout_locals_in_expr(value, layout_locals);
                }
            }
            Expr::SendMsg { target, data, .. } => {
                self.collect_layout_locals_in_expr(target, layout_locals);
                for (_, value) in data {
                    self.collect_layout_locals_in_expr(value, layout_locals);
                }
            }
            Expr::MacroCall { args, .. } => {
                for arg in args {
                    self.collect_layout_locals_in_expr(arg, layout_locals);
                }
            }
            Expr::Block(block, _) => self.collect_layout_locals_in_block(block, layout_locals),
            Expr::Return(Some(value), _) | Expr::Break(Some(value), _) => {
                self.collect_layout_locals_in_expr(value, layout_locals);
            }
            Expr::Return(None, _) | Expr::Break(None, _) | Expr::Continue(_) => {}
            Expr::JSX(_, _)
            | Expr::Lambda { .. }
            | Expr::Int(_, _)
            | Expr::Float(_, _)
            | Expr::String(_, _)
            | Expr::Bool(_, _)
            | Expr::None(_)
            | Expr::Ident(_, _)
            | Expr::SizeOfType { .. }
            | Expr::AlignOfType { .. }
            | Expr::Alloca { .. }
            | Expr::Uninit { .. } => {}
        }
    }

    fn resolve_layout_name_in_layout_scope(
        &self,
        layout_locals: &HashMap<String, String>,
        expr: &Expr,
    ) -> Option<String> {
        match expr {
            Expr::Struct { name, .. } => Some(name.clone()),
            Expr::AggregateInit { ty, .. } => self.layout_name_from_authored_type(ty),
            Expr::Spawn { actor, .. } => Some(actor.clone()),
            Expr::Ident(name, _) => layout_locals
                .get(name)
                .cloned()
                .or_else(|| self.world_globals.contains_key(name).then(|| name.clone())),
            Expr::Array(items, _) | Expr::Tuple(items, _) => self.merge_layout_candidates(
                items.iter()
                    .map(|item| self.resolve_layout_name_in_layout_scope(layout_locals, item)),
            ),
            Expr::Index { object, .. } => {
                self.resolve_layout_name_in_layout_scope(layout_locals, object)
            }
            Expr::Call { callee, .. } => match callee.as_ref() {
                Expr::Ident(name, _) => self.callable_layout_names.get(name).cloned(),
                _ => None,
            },
            Expr::StageCall { function, .. } => self.callable_layout_names.get(function).cloned(),
            Expr::MethodCall {
                receiver, method, ..
            } => self.resolve_method_layout_name(
                self.resolve_layout_name_in_layout_scope(layout_locals, receiver),
                method,
            ),
            Expr::If {
                then_branch,
                else_branch,
                ..
            } => self.merge_layout_candidates([
                self.resolve_block_layout_name_in_layout_scope(layout_locals, then_branch),
                else_branch.as_ref().and_then(|branch| {
                    self.resolve_else_layout_name_in_layout_scope(layout_locals, branch)
                }),
            ]),
            Expr::Match { arms, .. } => self.merge_layout_candidates(
                arms.iter()
                    .map(|arm| self.resolve_layout_name_in_layout_scope(layout_locals, &arm.body)),
            ),
            Expr::Block(block, _) => {
                self.resolve_block_layout_name_in_layout_scope(layout_locals, block)
            }
            Expr::Paren(inner, _)
            | Expr::Deref(inner, _)
            | Expr::Ref { value: inner, .. }
            | Expr::AddrOf { value: inner, .. }
            | Expr::Cast { value: inner, .. }
            | Expr::Bitcast { value: inner, .. }
            | Expr::Comptime(inner, _)
            | Expr::Await(inner, _)
            | Expr::AsyncBlock(inner, _)
            | Expr::Try(inner, _) => self.resolve_layout_name_in_layout_scope(layout_locals, inner),
            Expr::Teleport { value, .. } => {
                self.resolve_layout_name_in_layout_scope(layout_locals, value)
            }
            Expr::Observe { body, .. } | Expr::Collapse { body, .. } => {
                self.resolve_layout_name_in_layout_scope(layout_locals, body)
            }
            Expr::Field { object, field, .. } => {
                let owner_layout =
                    self.resolve_layout_name_in_layout_scope(layout_locals, object)?;
                self.struct_field_layout_names
                    .get(&owner_layout)
                    .and_then(|fields| fields.get(field))
                    .cloned()
            }
            _ => None,
        }
    }

    fn allocate_world(&mut self, world: &kain_core::types::TypedWorld) -> KainResult<()> {
        let Some((field_offsets, total_size)) = self.struct_layouts.get(&world.ast.name).cloned()
        else {
            return Err(KainError::codegen(
                format!("World '{}' layout not found", world.ast.name),
                world.ast.span,
            ));
        };

        let base = self.data_offset;
        let mut data = vec![0u8; total_size as usize];

        for state in &world.ast.states {
            let Some(&offset) = field_offsets.get(&state.name) else {
                continue;
            };
            if let Expr::String(value, _) = &state.initial {
                if let Some(string_offset) = self.string_table.get(value) {
                    let ptr = (*string_offset + 4) as i32;
                    let start = offset as usize;
                    if start + 4 <= data.len() {
                        data[start..start + 4].copy_from_slice(&ptr.to_le_bytes());
                    }
                }
                continue;
            }
            if let Some(value) = self.try_eval_const_expr(&state.initial) {
                self.write_const_bytes(&mut data, offset as usize, &value);
            }
        }

        if let Some(memory_id) = self.memory_id {
            self.module.data.add(
                walrus::DataKind::Active {
                    memory: memory_id,
                    offset: walrus::ConstExpr::Value(walrus::ir::Value::I32(base as i32)),
                },
                data,
            );
        }

        self.world_globals.insert(world.ast.name.clone(), base);
        self.data_offset += total_size;
        self.data_offset = (self.data_offset + 7) & !7;
        Ok(())
    }

    fn compile_component(&mut self, c: &kain_core::types::TypedComponent) -> KainResult<()> {
        let render_name = format!("{}_render", c.ast.name);

        // Params: self (i32)
        // Ret: VNode (i32)
        let wasm_params = vec![ValType::I32];
        let wasm_results = vec![ValType::I32];

        let mut builder = FunctionBuilder::new(&mut self.module.types, &wasm_params, &wasm_results);
        let self_local = self.module.locals.add(ValType::I32);

        // Locals
        let tmp_i32 = self.module.locals.add(ValType::I32);
        let tmp_i32_2 = self.module.locals.add(ValType::I32);
        let tmp_i64 = self.module.locals.add(ValType::I64);
        let tmp_f64 = self.module.locals.add(ValType::F64);

        let mut locals_map = HashMap::new();
        locals_map.insert("self".to_string(), self_local);
        let mut layout_locals = HashMap::new();
        layout_locals.insert("self".to_string(), c.ast.name.clone());

        let ctx = CompilationContext {
            locals: locals_map,
            actor_locals: HashMap::new(),
            layout_locals,
            reply_ports: HashMap::new(),
            functions: &self.functions,
            constants: &self.constants,
            string_table: &self.string_table,
            struct_layouts: &self.struct_layouts,
            struct_field_types: &self.struct_field_types,
            struct_field_layout_names: &self.struct_field_layout_names,
            enum_layouts: &self.enum_layouts,
            memory_id: self.memory_id.unwrap(),
            heap_ptr_global: self.heap_ptr_global,
            world_globals: &self.world_globals,
            tmp_i32,
            tmp_i32_2,
            tmp_i64,
            tmp_f64,
            funcref_table: self.funcref_table,
            lambda_table: &self.lambda_table,
        };

        let mut func_body = builder.func_body();
        self.compile_jsx_node(&ctx, &mut func_body, &c.ast.body)?;

        let func_id = builder.finish(vec![self_local], &mut self.module.funcs);
        self.functions.insert(render_name.clone(), func_id);
        self.module.exports.add(&render_name, func_id);

        Ok(())
    }

    fn compute_enum_layout(&mut self, e: &kain_core::types::TypedEnum) {
        let mut variant_tags = HashMap::new();
        let mut max_payload_size = 0u32;
        let mut variant_field_offsets = HashMap::new();

        for (idx, variant) in e.ast.variants.iter().enumerate() {
            variant_tags.insert(variant.name.clone(), idx as u32);
            let mut field_offsets = HashMap::new();
            let mut payload_size = 0u32;

            if let Some(payload_types) = e.variant_payload_types.get(&variant.name) {
                let mut current_offset = 0u32;

                // Determine offsets based on variant type
                match &variant.fields {
                    kain_core::ast::VariantFields::Struct(fields) => {
                        for (i, field) in fields.iter().enumerate() {
                            if let Some(ty) = payload_types.get(i) {
                                // Align to 4 bytes for simplicity (WASM is 32-bit mostly)
                                current_offset = (current_offset + 3) & !3;
                                field_offsets.insert(field.name.clone(), current_offset);

                                let size = self.type_size_of(ty);
                                current_offset += size;
                            }
                        }
                    }
                    kain_core::ast::VariantFields::Tuple(_) => {
                        for (i, ty) in payload_types.iter().enumerate() {
                            current_offset = (current_offset + 3) & !3;
                            field_offsets.insert(i.to_string(), current_offset);
                            current_offset += self.type_size_of(ty);
                        }
                    }
                    kain_core::ast::VariantFields::Unit => {}
                }

                // Align final size
                payload_size = (current_offset + 3) & !3;
            }

            variant_field_offsets.insert(variant.name.clone(), field_offsets);
            max_payload_size = max_payload_size.max(payload_size);
        }

        self.enum_layouts.insert(
            e.ast.name.clone(),
            (variant_tags, max_payload_size, variant_field_offsets),
        );
    }

    fn type_size_of(&self, ty: &ResolvedType) -> u32 {
        match ty {
            ResolvedType::Unit => 0,
            ResolvedType::Bool => 4,
            ResolvedType::Int(kain_core::types::IntSize::I8)
            | ResolvedType::Int(kain_core::types::IntSize::U8) => 1,
            ResolvedType::Int(kain_core::types::IntSize::I16)
            | ResolvedType::Int(kain_core::types::IntSize::U16) => 2,
            ResolvedType::Int(kain_core::types::IntSize::I32)
            | ResolvedType::Int(kain_core::types::IntSize::U32) => 4,
            ResolvedType::Int(kain_core::types::IntSize::I64)
            | ResolvedType::Int(kain_core::types::IntSize::U64)
            | ResolvedType::Int(kain_core::types::IntSize::Isize)
            | ResolvedType::Int(kain_core::types::IntSize::Usize) => 8,
            ResolvedType::Float(kain_core::types::FloatSize::F32) => 4,
            ResolvedType::Float(kain_core::types::FloatSize::F64) => 8,
            ResolvedType::String => 4, // pointer
            ResolvedType::Char => 4,
            ResolvedType::Array(_, len) => 4 + (*len as u32 * 8), // pointer + inline storage
            ResolvedType::Tuple(_) => 4,
            ResolvedType::Struct(_, _) => 4,                      // pointer
            _ => 8,                                               // default to 8 bytes
        }
    }

    fn ast_type_size_of(&self, ty: &kain_core::ast::Type) -> u32 {
        match ty {
            kain_core::ast::Type::Named { name, .. } => match name.as_str() {
                "Bool" => 4,
                "Int" | "I64" | "U64" | "Isize" | "Usize" => 8,
                "I32" | "U32" | "Float32" | "F32" => 4,
                "I16" | "U16" => 2,
                "I8" | "U8" | "Char" => 1,
                "Float" | "Float64" | "F64" => 8,
                "String" => 4,
                _ => 4,
            },
            kain_core::ast::Type::Array(_, len, _) => 4 + (*len as u32 * 8),
            kain_core::ast::Type::Tuple(types, _) => (types.len() as u32) * 8,
            kain_core::ast::Type::Ref { .. }
            | kain_core::ast::Type::Ptr { .. }
            | kain_core::ast::Type::Function { .. }
            | kain_core::ast::Type::Option(_, _)
            | kain_core::ast::Type::Result(_, _, _)
            | kain_core::ast::Type::Impl { .. } => 4,
            kain_core::ast::Type::Slice(_, _) => 8,
            kain_core::ast::Type::Infer(_)
            | kain_core::ast::Type::Never(_)
            | kain_core::ast::Type::Unit(_) => 0,
        }
    }

    /// Emit bump allocator: allocates `size` bytes, returns pointer to start
    /// Stack effect: [] -> [i32 pointer]
    ///
    /// Algorithm:
    ///   old_ptr = heap_ptr
    ///   heap_ptr = (heap_ptr + size + 7) & ~7  // 8-byte aligned
    ///   return old_ptr
    fn emit_alloc(&self, ctx: &CompilationContext, builder: &mut InstrSeqBuilder, size: u32) {
        // Get current heap pointer (this will be our return value)
        builder.global_get(ctx.heap_ptr_global);

        // Compute new heap pointer: (heap_ptr + size + 7) & ~7
        builder.global_get(ctx.heap_ptr_global);
        builder.i32_const(size as i32);
        builder.binop(walrus::ir::BinaryOp::I32Add);
        builder.i32_const(7);
        builder.binop(walrus::ir::BinaryOp::I32Add);
        builder.i32_const(-8); // ~7 in two's complement
        builder.binop(walrus::ir::BinaryOp::I32And);

        // Store new heap pointer
        builder.global_set(ctx.heap_ptr_global);

        // Stack now has: [old_ptr] - which is our allocated address
    }

    fn emit_const_value(&self, builder: &mut InstrSeqBuilder, value: WasmConstValue) {
        match value {
            WasmConstValue::I64(value) => builder.i64_const(value),
            WasmConstValue::I32(value) => builder.i32_const(value),
            WasmConstValue::F64(value) => builder.f64_const(value),
        };
    }

    fn write_const_bytes(&self, data: &mut [u8], offset: usize, value: &WasmConstValue) {
        match value {
            WasmConstValue::I64(value) => {
                if offset + 8 <= data.len() {
                    data[offset..offset + 8].copy_from_slice(&value.to_le_bytes());
                }
            }
            WasmConstValue::I32(value) => {
                if offset + 4 <= data.len() {
                    data[offset..offset + 4].copy_from_slice(&value.to_le_bytes());
                }
            }
            WasmConstValue::F64(value) => {
                if offset + 8 <= data.len() {
                    data[offset..offset + 8].copy_from_slice(&value.to_le_bytes());
                }
            }
        }
    }

    fn try_eval_const_expr(&self, expr: &Expr) -> Option<WasmConstValue> {
        match expr {
            Expr::Int(value, _) => Some(WasmConstValue::I64(*value)),
            Expr::Float(value, _) => Some(WasmConstValue::F64(*value)),
            Expr::Bool(value, _) => Some(WasmConstValue::I32(if *value { 1 } else { 0 })),
            Expr::Ident(name, _) => self
                .constants
                .get(name)
                .copied()
                .or_else(|| wasm_c_runtime_constant(name).map(WasmConstValue::I64)),
            Expr::Paren(inner, _) => self.try_eval_const_expr(inner),
            Expr::Unary { op, operand, .. } => {
                let value = self.try_eval_const_expr(operand)?;
                match op {
                    kain_core::ast::UnaryOp::Neg => match value {
                        WasmConstValue::I64(value) => {
                            Some(WasmConstValue::I64(value.wrapping_neg()))
                        }
                        WasmConstValue::I32(value) => {
                            Some(WasmConstValue::I64((value as i64).wrapping_neg()))
                        }
                        WasmConstValue::F64(value) => Some(WasmConstValue::F64(-value)),
                    },
                    kain_core::ast::UnaryOp::Not => {
                        Some(WasmConstValue::I32(if value.truthy() { 0 } else { 1 }))
                    }
                    kain_core::ast::UnaryOp::BitNot => Some(WasmConstValue::I64(!value.as_i64()?)),
                    _ => None,
                }
            }
            Expr::Binary {
                left, op, right, ..
            } => {
                let left = self.try_eval_const_expr(left)?;
                let right = self.try_eval_const_expr(right)?;
                self.eval_const_binary(left, *op, right)
            }
            _ => None,
        }
    }

    fn eval_const_binary(
        &self,
        left: WasmConstValue,
        op: BinaryOp,
        right: WasmConstValue,
    ) -> Option<WasmConstValue> {
        if matches!(left, WasmConstValue::F64(_)) || matches!(right, WasmConstValue::F64(_)) {
            let l = left.as_f64();
            let r = right.as_f64();
            return match op {
                BinaryOp::Add => Some(WasmConstValue::F64(l + r)),
                BinaryOp::Sub => Some(WasmConstValue::F64(l - r)),
                BinaryOp::Mul => Some(WasmConstValue::F64(l * r)),
                BinaryOp::Div => Some(WasmConstValue::F64(l / r)),
                BinaryOp::Eq => Some(WasmConstValue::I32(if l == r { 1 } else { 0 })),
                BinaryOp::Ne => Some(WasmConstValue::I32(if l != r { 1 } else { 0 })),
                BinaryOp::Lt => Some(WasmConstValue::I32(if l < r { 1 } else { 0 })),
                BinaryOp::Gt => Some(WasmConstValue::I32(if l > r { 1 } else { 0 })),
                BinaryOp::Le => Some(WasmConstValue::I32(if l <= r { 1 } else { 0 })),
                BinaryOp::Ge => Some(WasmConstValue::I32(if l >= r { 1 } else { 0 })),
                BinaryOp::And => Some(WasmConstValue::I32(if left.truthy() && right.truthy() {
                    1
                } else {
                    0
                })),
                BinaryOp::Or => Some(WasmConstValue::I32(if left.truthy() || right.truthy() {
                    1
                } else {
                    0
                })),
                _ => None,
            };
        }

        let l = left.as_i64()?;
        let r = right.as_i64()?;
        match op {
            BinaryOp::Add => Some(WasmConstValue::I64(l.wrapping_add(r))),
            BinaryOp::Sub => Some(WasmConstValue::I64(l.wrapping_sub(r))),
            BinaryOp::Mul => Some(WasmConstValue::I64(l.wrapping_mul(r))),
            BinaryOp::Div if r != 0 => Some(WasmConstValue::I64(l.wrapping_div(r))),
            BinaryOp::Mod if r != 0 => Some(WasmConstValue::I64(l.wrapping_rem(r))),
            BinaryOp::Eq => Some(WasmConstValue::I32(if l == r { 1 } else { 0 })),
            BinaryOp::Ne => Some(WasmConstValue::I32(if l != r { 1 } else { 0 })),
            BinaryOp::Lt => Some(WasmConstValue::I32(if l < r { 1 } else { 0 })),
            BinaryOp::Gt => Some(WasmConstValue::I32(if l > r { 1 } else { 0 })),
            BinaryOp::Le => Some(WasmConstValue::I32(if l <= r { 1 } else { 0 })),
            BinaryOp::Ge => Some(WasmConstValue::I32(if l >= r { 1 } else { 0 })),
            BinaryOp::And => Some(WasmConstValue::I32(if left.truthy() && right.truthy() {
                1
            } else {
                0
            })),
            BinaryOp::Or => Some(WasmConstValue::I32(if left.truthy() || right.truthy() {
                1
            } else {
                0
            })),
            BinaryOp::BitAnd => Some(WasmConstValue::I64(l & r)),
            BinaryOp::BitOr => Some(WasmConstValue::I64(l | r)),
            BinaryOp::BitXor => Some(WasmConstValue::I64(l ^ r)),
            BinaryOp::Shl => Some(WasmConstValue::I64(l.wrapping_shl((r & 63) as u32))),
            BinaryOp::Shr => Some(WasmConstValue::I64(l.wrapping_shr((r & 63) as u32))),
            _ => None,
        }
    }

    fn collect_strings_in_block(&mut self, block: &Block) {
        for stmt in &block.stmts {
            self.collect_strings_in_stmt(stmt);
        }
    }

    fn collect_strings_in_stmt(&mut self, stmt: &Stmt) {
        match stmt {
            Stmt::Expr(expr) => self.collect_strings_in_expr(expr),
            Stmt::Let {
                value: Some(expr), ..
            } => self.collect_strings_in_expr(expr),
            Stmt::Return(Some(expr), _) | Stmt::Break(Some(expr), _) => {
                self.collect_strings_in_expr(expr)
            }
            Stmt::For { iter, body, .. } | Stmt::Fanout { iter, body, .. } => {
                self.collect_strings_in_expr(iter);
                self.collect_strings_in_block(body);
            }
            Stmt::While {
                condition, body, ..
            } => {
                self.collect_strings_in_expr(condition);
                self.collect_strings_in_block(body);
            }
            Stmt::Loop { body, .. } => self.collect_strings_in_block(body),
            _ => {}
        }
    }

    fn collect_strings_in_call_args(&mut self, args: &[kain_core::ast::CallArg]) {
        for arg in args {
            self.collect_strings_in_expr(&arg.value);
        }
    }

    fn collect_strings_in_enum_variant_fields(
        &mut self,
        fields: &kain_core::ast::EnumVariantFields,
    ) {
        match fields {
            kain_core::ast::EnumVariantFields::Unit => {}
            kain_core::ast::EnumVariantFields::Tuple(values) => {
                for value in values {
                    self.collect_strings_in_expr(value);
                }
            }
            kain_core::ast::EnumVariantFields::Struct(fields) => {
                for (_, value) in fields {
                    self.collect_strings_in_expr(value);
                }
            }
        }
    }

    fn collect_strings_in_match_arm(&mut self, arm: &kain_core::ast::MatchArm) {
        if let Some(guard) = &arm.guard {
            self.collect_strings_in_expr(guard);
        }
        self.collect_strings_in_expr(&arm.body);
    }

    fn collect_strings_in_expr(&mut self, expr: &Expr) {
        match expr {
            Expr::String(s, _) => {
                self.allocate_string(s);
            }
            Expr::FString(parts, _) | Expr::Array(parts, _) | Expr::Tuple(parts, _) => {
                for part in parts {
                    self.collect_strings_in_expr(part);
                }
            }
            Expr::MacroCall { args, .. } => {
                for arg in args {
                    self.collect_strings_in_expr(arg);
                }
            }
            Expr::Binary { left, right, .. } => {
                self.collect_strings_in_expr(left);
                self.collect_strings_in_expr(right);
            }
            Expr::Unary { operand, .. }
            | Expr::Ref { value: operand, .. }
            | Expr::AddrOf { value: operand, .. }
            | Expr::Deref(operand, _) => self.collect_strings_in_expr(operand),
            Expr::Assign { target, value, .. } => {
                self.collect_strings_in_expr(target);
                self.collect_strings_in_expr(value);
            }
            Expr::Paren(inner, _) => self.collect_strings_in_expr(inner),
            Expr::Call { callee, args, .. } => {
                self.collect_strings_in_expr(callee);
                self.collect_strings_in_call_args(args);
            }
            Expr::StageCall { args, .. } => {
                self.collect_strings_in_call_args(args);
            }
            Expr::MethodCall { receiver, args, .. } => {
                self.collect_strings_in_expr(receiver);
                self.collect_strings_in_call_args(args);
            }
            Expr::Field { object, .. } => self.collect_strings_in_expr(object),
            Expr::Index { object, index, .. } => {
                self.collect_strings_in_expr(object);
                self.collect_strings_in_expr(index);
            }
            Expr::Struct { fields, rest, .. } => {
                for (_, value) in fields {
                    self.collect_strings_in_expr(value);
                }
                if let Some(rest) = rest {
                    self.collect_strings_in_expr(rest);
                }
            }
            Expr::AggregateInit { fields, .. } => {
                for (_, value) in fields {
                    self.collect_strings_in_expr(value);
                }
            }
            Expr::EnumVariant { fields, .. } => {
                self.collect_strings_in_enum_variant_fields(fields);
            }
            Expr::Range { start, end, .. } => {
                if let Some(start) = start {
                    self.collect_strings_in_expr(start);
                }
                if let Some(end) = end {
                    self.collect_strings_in_expr(end);
                }
            }
            Expr::Observe { target, body, .. }
            | Expr::Collapse { target, body, .. }
            | Expr::Share { target, body, .. } => {
                self.collect_strings_in_expr(target);
                self.collect_strings_in_expr(body);
            }
            Expr::PtrOffset {
                pointer, offset, ..
            }
            | Expr::MemStore {
                pointer,
                value: offset,
                ..
            }
            | Expr::VolatileStore {
                pointer,
                value: offset,
                ..
            }
            | Expr::AtomicStore {
                pointer,
                value: offset,
                ..
            }
            | Expr::AtomicAdd {
                pointer,
                value: offset,
                ..
            }
            | Expr::AtomicSub {
                pointer,
                value: offset,
                ..
            }
            | Expr::AtomicAnd {
                pointer,
                value: offset,
                ..
            }
            | Expr::AtomicOr {
                pointer,
                value: offset,
                ..
            }
            | Expr::AtomicXor {
                pointer,
                value: offset,
                ..
            }
            | Expr::AtomicExchange {
                pointer,
                value: offset,
                ..
            } => {
                self.collect_strings_in_expr(pointer);
                self.collect_strings_in_expr(offset);
            }
            Expr::MemLoad { pointer, .. }
            | Expr::VolatileLoad { pointer, .. }
            | Expr::AtomicLoad { pointer, .. }
            | Expr::CpuCacheFlush { pointer, .. }
            | Expr::Decay {
                target: pointer, ..
            }
            | Expr::Alloc { size: pointer, .. } => self.collect_strings_in_expr(pointer),
            Expr::AtomicCompareExchange {
                pointer,
                expected,
                desired,
                ..
            } => {
                self.collect_strings_in_expr(pointer);
                self.collect_strings_in_expr(expected);
                self.collect_strings_in_expr(desired);
            }
            Expr::AtomicFence { .. } | Expr::CpuFence { .. } => {}
            Expr::InlineAsm { operands, .. } => {
                for operand in operands {
                    self.collect_strings_in_expr(operand);
                }
            }
            Expr::Realloc { pointer, size, .. } => {
                self.collect_strings_in_expr(pointer);
                self.collect_strings_in_expr(size);
            }
            Expr::Spawn { init, .. } => {
                for (_, value) in init {
                    self.collect_strings_in_expr(value);
                }
            }
            Expr::SendMsg { target, data, .. } => {
                self.collect_strings_in_expr(target);
                for (_, value) in data {
                    self.collect_strings_in_expr(value);
                }
            }
            Expr::Teleport { value, .. }
            | Expr::Cast { value, .. }
            | Expr::Bitcast { value, .. }
            | Expr::Comptime(value, _)
            | Expr::Await(value, _)
            | Expr::AsyncBlock(value, _)
            | Expr::Try(value, _) => self.collect_strings_in_expr(value),
            Expr::Block(block, _) => self.collect_strings_in_block(block),
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                self.collect_strings_in_expr(condition);
                self.collect_strings_in_block(then_branch);
                if let Some(else_br) = else_branch {
                    self.collect_strings_in_else(else_br);
                }
            }
            Expr::Match {
                scrutinee, arms, ..
            } => {
                self.collect_strings_in_expr(scrutinee);
                for arm in arms {
                    self.collect_strings_in_match_arm(arm);
                }
            }
            Expr::Lambda { body, .. } => self.collect_strings_in_expr(body),
            Expr::Return(Some(value), _) | Expr::Break(Some(value), _) => {
                self.collect_strings_in_expr(value)
            }
            Expr::JSX(node, _) => {
                self.collect_strings_in_jsx(node);
            }
            _ => {}
        }
    }

    fn collect_strings_in_jsx(&mut self, node: &kain_core::ast::JSXNode) {
        match node {
            kain_core::ast::JSXNode::Element {
                tag,
                attributes,
                children,
                ..
            } => {
                self.allocate_string(tag);
                for attr in attributes {
                    self.allocate_string(&attr.name);
                    match &attr.value {
                        kain_core::ast::JSXAttrValue::String(s) => {
                            self.allocate_string(s);
                        }
                        kain_core::ast::JSXAttrValue::Expr(e) => self.collect_strings_in_expr(e),
                        _ => {}
                    }
                }
                for child in children {
                    self.collect_strings_in_jsx(child);
                }
            }
            kain_core::ast::JSXNode::Text(s, _) => {
                self.allocate_string(s);
            }
            kain_core::ast::JSXNode::Expression(e) => {
                self.collect_strings_in_expr(e);
            }
            kain_core::ast::JSXNode::ComponentCall {
                name,
                props,
                children,
                ..
            } => {
                // Name might not be a string literal in runtime, but let's alloc it anyway
                self.allocate_string(name);
                for attr in props {
                    self.allocate_string(&attr.name);
                    match &attr.value {
                        kain_core::ast::JSXAttrValue::String(s) => {
                            self.allocate_string(s);
                        }
                        kain_core::ast::JSXAttrValue::Expr(e) => self.collect_strings_in_expr(e),
                        _ => {}
                    }
                }
                for child in children {
                    self.collect_strings_in_jsx(child);
                }
            }
            kain_core::ast::JSXNode::Fragment(children, _) => {
                for child in children {
                    self.collect_strings_in_jsx(child);
                }
            }
            kain_core::ast::JSXNode::For { iter, body, .. } => {
                self.collect_strings_in_expr(iter);
                self.collect_strings_in_jsx(body);
            }
            kain_core::ast::JSXNode::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                self.collect_strings_in_expr(condition);
                self.collect_strings_in_jsx(then_branch);
                if let Some(else_br) = else_branch {
                    self.collect_strings_in_jsx(else_br);
                }
            }
        }
    }

    fn collect_strings_in_else(&mut self, branch: &kain_core::ast::ElseBranch) {
        match branch {
            kain_core::ast::ElseBranch::Else(block) => self.collect_strings_in_block(block),
            kain_core::ast::ElseBranch::ElseIf(cond, then, next) => {
                self.collect_strings_in_expr(cond);
                self.collect_strings_in_block(then);
                if let Some(next_br) = next {
                    self.collect_strings_in_else(next_br);
                }
            }
        }
    }

    // === LAMBDA COLLECTION AND COMPILATION ===

    fn collect_lambdas_in_block(
        &mut self,
        block: &Block,
        lambdas: &mut Vec<(u32, Vec<kain_core::ast::Param>, Expr)>,
    ) {
        for stmt in &block.stmts {
            self.collect_lambdas_in_stmt(stmt, lambdas);
        }
    }

    fn collect_lambdas_in_stmt(
        &mut self,
        stmt: &Stmt,
        lambdas: &mut Vec<(u32, Vec<kain_core::ast::Param>, Expr)>,
    ) {
        match stmt {
            Stmt::Expr(expr) => self.collect_lambdas_in_expr(expr, lambdas),
            Stmt::Let {
                value: Some(expr), ..
            } => self.collect_lambdas_in_expr(expr, lambdas),
            Stmt::Return(Some(expr), _) => self.collect_lambdas_in_expr(expr, lambdas),
            Stmt::While {
                condition, body, ..
            } => {
                self.collect_lambdas_in_expr(condition, lambdas);
                self.collect_lambdas_in_block(body, lambdas);
            }
            Stmt::For { iter, body, .. } | Stmt::Fanout { iter, body, .. } => {
                self.collect_lambdas_in_expr(iter, lambdas);
                self.collect_lambdas_in_block(body, lambdas);
            }
            Stmt::Loop { body, .. } => {
                self.collect_lambdas_in_block(body, lambdas);
            }
            _ => {}
        }
    }

    fn collect_lambdas_in_expr(
        &mut self,
        expr: &Expr,
        lambdas: &mut Vec<(u32, Vec<kain_core::ast::Param>, Expr)>,
    ) {
        match expr {
            Expr::Lambda { params, body, .. } => {
                let id = self.lambda_counter;
                self.lambda_counter += 1;
                lambdas.push((id, params.clone(), (**body).clone()));
                // Also collect nested lambdas in body
                self.collect_lambdas_in_expr(body, lambdas);
            }
            Expr::Binary { left, right, .. } => {
                self.collect_lambdas_in_expr(left, lambdas);
                self.collect_lambdas_in_expr(right, lambdas);
            }
            Expr::Unary { operand, .. } => {
                self.collect_lambdas_in_expr(operand, lambdas);
            }
            Expr::Assign { target, value, .. } => {
                self.collect_lambdas_in_expr(target, lambdas);
                self.collect_lambdas_in_expr(value, lambdas);
            }
            Expr::Paren(inner, _) => self.collect_lambdas_in_expr(inner, lambdas),
            Expr::Call { callee, args, .. } => {
                self.collect_lambdas_in_expr(callee, lambdas);
                for arg in args {
                    self.collect_lambdas_in_expr(&arg.value, lambdas);
                }
            }
            Expr::StageCall { args, .. } => {
                for arg in args {
                    self.collect_lambdas_in_expr(&arg.value, lambdas);
                }
            }
            Expr::MethodCall { receiver, args, .. } => {
                self.collect_lambdas_in_expr(receiver, lambdas);
                for arg in args {
                    self.collect_lambdas_in_expr(&arg.value, lambdas);
                }
            }
            Expr::Observe { target, body, .. }
            | Expr::Collapse { target, body, .. }
            | Expr::Share { target, body, .. } => {
                self.collect_lambdas_in_expr(target, lambdas);
                self.collect_lambdas_in_expr(body, lambdas);
            }
            Expr::Teleport { value, .. }
            | Expr::Cast { value, .. }
            | Expr::Bitcast { value, .. }
            | Expr::Comptime(value, _)
            | Expr::Await(value, _)
            | Expr::AsyncBlock(value, _)
            | Expr::Try(value, _) => self.collect_lambdas_in_expr(value, lambdas),
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                self.collect_lambdas_in_expr(condition, lambdas);
                self.collect_lambdas_in_block(then_branch, lambdas);
                if let Some(else_br) = else_branch {
                    self.collect_lambdas_in_else_branch(else_br, lambdas);
                }
            }
            Expr::Match {
                scrutinee, arms, ..
            } => {
                self.collect_lambdas_in_expr(scrutinee, lambdas);
                for arm in arms {
                    self.collect_lambdas_in_expr(&arm.body, lambdas);
                }
            }
            Expr::Array(elements, _) => {
                for e in elements {
                    self.collect_lambdas_in_expr(e, lambdas);
                }
            }
            Expr::Tuple(elements, _) => {
                for e in elements {
                    self.collect_lambdas_in_expr(e, lambdas);
                }
            }
            Expr::VolatileLoad { pointer, .. } | Expr::AtomicLoad { pointer, .. } => {
                self.collect_lambdas_in_expr(pointer, lambdas);
            }
            Expr::CpuCacheFlush { pointer, .. } => {
                self.collect_lambdas_in_expr(pointer, lambdas);
            }
            Expr::VolatileStore { pointer, value, .. }
            | Expr::AtomicStore { pointer, value, .. }
            | Expr::AtomicAdd { pointer, value, .. }
            | Expr::AtomicSub { pointer, value, .. }
            | Expr::AtomicAnd { pointer, value, .. }
            | Expr::AtomicOr { pointer, value, .. }
            | Expr::AtomicXor { pointer, value, .. }
            | Expr::AtomicExchange { pointer, value, .. } => {
                self.collect_lambdas_in_expr(pointer, lambdas);
                self.collect_lambdas_in_expr(value, lambdas);
            }
            Expr::AtomicCompareExchange {
                pointer,
                expected,
                desired,
                ..
            } => {
                self.collect_lambdas_in_expr(pointer, lambdas);
                self.collect_lambdas_in_expr(expected, lambdas);
                self.collect_lambdas_in_expr(desired, lambdas);
            }
            Expr::AtomicFence { .. } | Expr::CpuFence { .. } => {}
            Expr::InlineAsm { operands, .. } => {
                for operand in operands {
                    self.collect_lambdas_in_expr(operand, lambdas);
                }
            }
            Expr::Block(block, _) => {
                self.collect_lambdas_in_block(block, lambdas);
            }
            _ => {}
        }
    }

    fn collect_lambdas_in_else_branch(
        &mut self,
        branch: &kain_core::ast::ElseBranch,
        lambdas: &mut Vec<(u32, Vec<kain_core::ast::Param>, Expr)>,
    ) {
        match branch {
            kain_core::ast::ElseBranch::Else(block) => {
                self.collect_lambdas_in_block(block, lambdas)
            }
            kain_core::ast::ElseBranch::ElseIf(cond, then, next) => {
                self.collect_lambdas_in_expr(cond, lambdas);
                self.collect_lambdas_in_block(then, lambdas);
                if let Some(next_br) = next {
                    self.collect_lambdas_in_else_branch(next_br, lambdas);
                }
            }
        }
    }

    /// Compile a collected lambda into a WASM function and add to funcref table
    fn compile_lambda(
        &mut self,
        id: u32,
        params: &[kain_core::ast::Param],
        body: &Expr,
    ) -> KainResult<()> {
        // Create function type: all params i64, returns i64
        let wasm_params: Vec<ValType> = params.iter().map(|_| ValType::I64).collect();
        let wasm_results = vec![ValType::I64];

        let mut builder = FunctionBuilder::new(&mut self.module.types, &wasm_params, &wasm_results);

        // Create parameter locals
        let mut locals = HashMap::new();
        let mut param_local_ids = Vec::new();
        for (i, param) in params.iter().enumerate() {
            let local_id = self.module.locals.add(wasm_params[i]);
            locals.insert(param.name.clone(), local_id);
            param_local_ids.push(local_id);
        }
        let mut layout_locals = HashMap::new();
        for param in params {
            if let Some(layout_name) = self.layout_name_from_authored_type(&param.ty) {
                layout_locals.insert(param.name.clone(), layout_name);
            }
        }
        self.collect_layout_locals_in_expr(body, &mut layout_locals);

        let tmp_i32 = self.module.locals.add(ValType::I32);
        let tmp_i32_2 = self.module.locals.add(ValType::I32);
        let tmp_i64 = self.module.locals.add(ValType::I64);
        let tmp_f64 = self.module.locals.add(ValType::F64);

        let ctx = CompilationContext {
            locals,
            actor_locals: HashMap::new(),
            layout_locals,
            reply_ports: HashMap::new(),
            functions: &self.functions,
            constants: &self.constants,
            string_table: &self.string_table,
            struct_layouts: &self.struct_layouts,
            struct_field_types: &self.struct_field_types,
            struct_field_layout_names: &self.struct_field_layout_names,
            enum_layouts: &self.enum_layouts,
            memory_id: self.memory_id.unwrap(),
            heap_ptr_global: self.heap_ptr_global,
            world_globals: &self.world_globals,
            tmp_i32,
            tmp_i32_2,
            tmp_i64,
            tmp_f64,
            funcref_table: self.funcref_table,
            lambda_table: &self.lambda_table,
        };

        // Compile lambda body
        let mut func_body = builder.func_body();
        self.compile_expr(&ctx, &mut func_body, body)?;

        // Finish function
        let func_id = builder.finish(param_local_ids, &mut self.module.funcs);

        // Add to function table via elem segment
        let table_index = id; // Use lambda ID as table index
        if let Some(table_id) = self.funcref_table {
            // Add function to table via elem segment
            self.module.elements.add(
                walrus::ElementKind::Active {
                    table: table_id,
                    offset: walrus::ConstExpr::Value(walrus::ir::Value::I32(table_index as i32)),
                },
                walrus::ElementItems::Functions(vec![func_id]),
            );
        }

        // Store in lambda_table for lookup during compilation
        self.lambda_table.insert(id, (table_index, func_id));

        // Also add to functions map with generated name
        let lambda_name = format!("__lambda_{}", id);
        self.functions.insert(lambda_name, func_id);

        Ok(())
    }

    fn coerce_stack_to_val_type(&self, builder: &mut InstrSeqBuilder, source: ValType, target: ValType) {
        if source == target {
            return;
        }

        match (source, target) {
            (ValType::I64, ValType::I32) => {
                builder.unop(walrus::ir::UnaryOp::I32WrapI64);
            }
            (ValType::I32, ValType::I64) => {
                builder.unop(walrus::ir::UnaryOp::I64ExtendSI32);
            }
            (ValType::I64, ValType::F64) => {
                builder.unop(walrus::ir::UnaryOp::F64ConvertSI64);
            }
            (ValType::I32, ValType::F64) => {
                builder.unop(walrus::ir::UnaryOp::F64ConvertSI32);
            }
            (ValType::F64, ValType::I64) => {
                builder.unop(walrus::ir::UnaryOp::I64TruncSF64);
            }
            (ValType::F64, ValType::I32) => {
                builder.unop(walrus::ir::UnaryOp::I32TruncSF64);
            }
            (ValType::F32, ValType::F64) => {
                builder.unop(walrus::ir::UnaryOp::F64PromoteF32);
            }
            (ValType::F64, ValType::F32) => {
                builder.unop(walrus::ir::UnaryOp::F32DemoteF64);
            }
            _ => {}
        }
    }

    fn coerce_expr_stack_to_val_type(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        expr: &Expr,
        target: ValType,
    ) {
        self.coerce_stack_to_val_type(builder, self.infer_expr_wasm_type_in_context(ctx, expr), target);
    }

    fn resolve_actor_name_in_context<'a>(
        &self,
        ctx: &'a CompilationContext,
        expr: &'a Expr,
    ) -> Option<&'a str> {
        match expr {
            Expr::Spawn { actor, .. } => Some(actor.as_str()),
            Expr::Ident(name, _) => ctx
                .actor_locals
                .get(name)
                .map(|actor| actor.as_str())
                .or_else(|| {
                    ctx.layout_locals.get(name).and_then(|layout_name| {
                        self.actors
                            .contains_key(layout_name)
                            .then_some(layout_name.as_str())
                    })
                }),
            Expr::Paren(inner, _) => self.resolve_actor_name_in_context(ctx, inner),
            _ => None,
        }
    }

    fn merge_layout_candidates<I>(&self, layouts: I) -> Option<String>
    where
        I: IntoIterator<Item = Option<String>>,
    {
        let mut resolved: Option<String> = None;
        for layout in layouts {
            let Some(layout) = layout else {
                continue;
            };
            match &resolved {
                None => resolved = Some(layout),
                Some(existing) if existing == &layout => {}
                Some(_) => return None,
            }
        }
        resolved
    }

    fn actor_handler_result_type_for_call(
        &self,
        ctx: &CompilationContext,
        args: &[CallArg],
    ) -> Option<ValType> {
        let actor_name = self.resolve_actor_name_in_context(ctx, &args.first()?.value)?;
        let message_name = match &args.get(1)?.value {
            Expr::String(value, _) => value.as_str(),
            _ => return None,
        };
        self.actor_handlers
            .get(&Self::actor_handler_key(actor_name, message_name))
            .and_then(|handler| handler.result_type)
    }

    fn resolve_layout_name_in_context(
        &self,
        ctx: &CompilationContext,
        expr: &Expr,
    ) -> Option<String> {
        match expr {
            Expr::Struct { name, .. } => Some(name.clone()),
            Expr::AggregateInit { ty, .. } => self.layout_name_from_authored_type(ty),
            Expr::Spawn { actor, .. } => Some(actor.clone()),
            Expr::Ident(name, _) => ctx
                .layout_locals
                .get(name)
                .cloned()
                .or_else(|| ctx.actor_locals.get(name).cloned())
                .or_else(|| ctx.world_globals.contains_key(name).then(|| name.clone())),
            Expr::Array(items, _) | Expr::Tuple(items, _) => self.merge_layout_candidates(
                items.iter()
                    .map(|item| self.resolve_layout_name_in_context(ctx, item)),
            ),
            Expr::Index { object, .. } => self.resolve_layout_name_in_context(ctx, object),
            Expr::Call { callee, .. } => match callee.as_ref() {
                Expr::Ident(name, _) => self.callable_layout_names.get(name).cloned(),
                _ => None,
            },
            Expr::StageCall { function, .. } => self.callable_layout_names.get(function).cloned(),
            Expr::MethodCall {
                receiver, method, ..
            } => self.resolve_method_layout_name(
                self.resolve_layout_name_in_context(ctx, receiver),
                method,
            ),
            Expr::If {
                then_branch,
                else_branch,
                ..
            } => self.merge_layout_candidates([
                self.resolve_block_layout_name_in_context(ctx, then_branch),
                else_branch
                    .as_ref()
                    .and_then(|branch| self.resolve_else_layout_name_in_context(ctx, branch)),
            ]),
            Expr::Match { arms, .. } => self.merge_layout_candidates(
                arms.iter()
                    .map(|arm| self.resolve_layout_name_in_context(ctx, &arm.body)),
            ),
            Expr::Block(block, _) => self.resolve_block_layout_name_in_context(ctx, block),
            Expr::Paren(inner, _)
            | Expr::Deref(inner, _)
            | Expr::Ref { value: inner, .. }
            | Expr::AddrOf { value: inner, .. }
            | Expr::Cast { value: inner, .. }
            | Expr::Bitcast { value: inner, .. }
            | Expr::Comptime(inner, _)
            | Expr::Await(inner, _)
            | Expr::AsyncBlock(inner, _)
            | Expr::Try(inner, _) => self.resolve_layout_name_in_context(ctx, inner),
            Expr::Teleport { value, .. } => self.resolve_layout_name_in_context(ctx, value),
            Expr::Observe { body, .. } | Expr::Collapse { body, .. } => {
                self.resolve_layout_name_in_context(ctx, body)
            }
            Expr::Field { object, field, .. } => {
                let owner_layout = self.resolve_layout_name_in_context(ctx, object)?;
                ctx.struct_field_layout_names
                    .get(&owner_layout)
                    .and_then(|fields| fields.get(field))
                    .cloned()
            }
            _ => None,
        }
    }

    fn resolve_field_offset_in_context(
        &self,
        ctx: &CompilationContext,
        object: &Expr,
        field: &str,
    ) -> Result<u32, &'static str> {
        if let Some(layout_name) = self.resolve_layout_name_in_context(ctx, object) {
            return ctx
                .struct_layouts
                .get(&layout_name)
                .and_then(|(offsets, _)| offsets.get(field).copied())
                .ok_or("missing");
        }

        let mut matches = ctx
            .struct_layouts
            .values()
            .filter_map(|(offsets, _)| offsets.get(field).copied());
        let Some(offset) = matches.next() else {
            return Err("missing");
        };
        if matches.next().is_some() {
            return Err("ambiguous");
        }
        Ok(offset)
    }

    fn infer_field_val_type_in_context(
        &self,
        ctx: &CompilationContext,
        object: &Expr,
        field: &str,
    ) -> Option<ValType> {
        if let Some(layout_name) = self.resolve_layout_name_in_context(ctx, object) {
            return ctx
                .struct_field_types
                .get(&layout_name)
                .and_then(|fields| fields.get(field))
                .copied();
        }

        let mut matches = ctx
            .struct_field_types
            .values()
            .filter_map(|fields| fields.get(field).copied());
        let first = matches.next()?;
        if matches.next().is_some() {
            return None;
        }
        Some(first)
    }

    fn function_result_type(&self, func_id: walrus::FunctionId) -> Option<ValType> {
        let func = self.module.funcs.get(func_id);
        let results = self.module.types.results(func.ty());
        match results {
            [] => None,
            [result] => Some(*result),
            _ => None,
        }
    }

    fn infer_expr_wasm_type_in_context(&self, ctx: &CompilationContext, expr: &Expr) -> ValType {
        match expr {
            Expr::Field { object, field, .. } => self
                .infer_field_val_type_in_context(ctx, object, field)
                .unwrap_or_else(|| self.infer_wasm_type_with_locals(&ctx.locals, expr)),
            Expr::Call { callee, args, .. } => {
                if let Expr::Ident(name, _) = callee.as_ref() {
                    if let Some(cast_ty) = self.primitive_call_result_type(name) {
                        return cast_ty;
                    }
                    let arg_types: Vec<_> = args
                        .iter()
                        .map(|arg| self.infer_expr_wasm_type_in_context(ctx, &arg.value))
                        .collect();
                    if let Some(builtin_ty) = self.builtin_math_call_result_type(name, &arg_types) {
                        return builtin_ty;
                    }
                    if let Some(func_id) = ctx.functions.get(name) {
                        if let Some(result_ty) = self.function_result_type(*func_id) {
                            return result_ty;
                        }
                    }
                }
                self.infer_wasm_type_with_locals(&ctx.locals, expr)
            }
            Expr::StageCall { function, .. } => ctx
                .functions
                .get(function)
                .and_then(|func_id| self.function_result_type(*func_id))
                .unwrap_or_else(|| self.infer_wasm_type_with_locals(&ctx.locals, expr)),
            Expr::MethodCall { method, .. } => ctx
                .functions
                .get(method)
                .and_then(|func_id| self.function_result_type(*func_id))
                .unwrap_or_else(|| self.infer_wasm_type_with_locals(&ctx.locals, expr)),
            _ => self.infer_wasm_type_with_locals(&ctx.locals, expr),
        }
    }

    fn compile_call_args_for_function(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        func_id: walrus::FunctionId,
        args: &[CallArg],
    ) -> KainResult<()> {
        let func = self.module.funcs.get(func_id);
        let param_types = self.module.types.params(func.ty()).to_vec();
        if args.len() != param_types.len() {
            return Err(KainError::codegen(
                format!(
                    "Function arity mismatch in WASM codegen: expected {}, got {}",
                    param_types.len(),
                    args.len()
                ),
                args.first().map(|arg| arg.value.span()).unwrap_or_default(),
            ));
        }

        for (arg, param_ty) in args.iter().zip(param_types.iter().copied()) {
            self.compile_expr(ctx, builder, &arg.value)?;
            self.coerce_expr_stack_to_val_type(ctx, builder, &arg.value, param_ty);
        }
        Ok(())
    }

    fn declare_actor_handlers(&mut self, actor: &TypedActor) -> KainResult<()> {
        for handler in &actor.ast.handlers {
            let mut wasm_params = vec![ValType::I32];
            let mut params = Vec::new();
            let mut reply_ports = HashMap::new();
            for param in &handler.params {
                let param_ty = self.map_authored_type(&param.ty);
                wasm_params.push(param_ty);
                params.push((param.name.clone(), param_ty));
                if self.is_reply_port_type(&param.ty) {
                    if let Some(reply_ty) = self.infer_actor_handler_reply_type(actor, handler) {
                        reply_ports.insert(param.name.clone(), reply_ty);
                    }
                }
            }
            let result_type = self.infer_actor_handler_reply_type(actor, handler);
            let wasm_results = result_type.map(|ty| vec![ty]).unwrap_or_default();
            let builder = FunctionBuilder::new(&mut self.module.types, &wasm_params, &wasm_results);
            let mut param_local_ids = Vec::new();
            for &param_type in &wasm_params {
                param_local_ids.push(self.module.locals.add(param_type));
            }
            let func_id = builder.finish(param_local_ids, &mut self.module.funcs);
            let key = Self::actor_handler_key(&actor.ast.name, &handler.message_type);
            self.actor_handlers.insert(
                key,
                WasmActorHandler {
                    actor_name: actor.ast.name.clone(),
                    message_name: handler.message_type.clone(),
                    body: handler.body.clone(),
                    params,
                    reply_ports,
                    result_type,
                    func_id,
                    span: handler.span,
                },
            );
        }
        Ok(())
    }

    fn compile_actor_handlers(&mut self, actor: &TypedActor) -> KainResult<()> {
        for handler in &actor.ast.handlers {
            let key = Self::actor_handler_key(&actor.ast.name, &handler.message_type);
            let Some(handler_info) = self.actor_handlers.get(&key).cloned() else {
                return Err(KainError::codegen(
                    format!(
                        "Missing declared wasm actor handler for '{}.{}'",
                        actor.ast.name, handler.message_type
                    ),
                    handler.span,
                ));
            };
            self.compile_actor_handler_body(actor, &handler_info)?;
        }
        Ok(())
    }

    fn compile_actor_handler_body(
        &mut self,
        actor: &TypedActor,
        handler_info: &WasmActorHandler,
    ) -> KainResult<()> {
        let mut wasm_params = vec![ValType::I32];
        wasm_params.extend(handler_info.params.iter().map(|(_, ty)| *ty));
        let wasm_results = handler_info
            .result_type
            .map(|ty| vec![ty])
            .unwrap_or_default();

        let mut builder = FunctionBuilder::new(&mut self.module.types, &wasm_params, &wasm_results);
        let mut locals = HashMap::new();
        let mut param_local_ids = Vec::new();

        let self_local = self.module.locals.add(ValType::I32);
        locals.insert("self".to_string(), self_local);
        param_local_ids.push(self_local);

        for (index, (name, ty)) in handler_info.params.iter().enumerate() {
            let local_id = self.module.locals.add(wasm_params[index + 1]);
            locals.insert(name.clone(), local_id);
            debug_assert_eq!(*ty, wasm_params[index + 1]);
            param_local_ids.push(local_id);
        }

        self.preallocate_locals(&handler_info.body, &mut locals);
        let mut actor_locals = HashMap::new();
        actor_locals.insert("self".to_string(), actor.ast.name.clone());
        self.collect_actor_locals_in_block(&handler_info.body, &mut actor_locals);
        let mut layout_locals = HashMap::new();
        layout_locals.insert("self".to_string(), actor.ast.name.clone());
        self.collect_layout_locals_in_block(&handler_info.body, &mut layout_locals);

        let tmp_i32 = self.module.locals.add(ValType::I32);
        let tmp_i32_2 = self.module.locals.add(ValType::I32);
        let tmp_i64 = self.module.locals.add(ValType::I64);
        let tmp_f64 = self.module.locals.add(ValType::F64);

        let ctx = CompilationContext {
            locals,
            actor_locals,
            layout_locals,
            reply_ports: handler_info.reply_ports.clone(),
            functions: &self.functions,
            constants: &self.constants,
            string_table: &self.string_table,
            struct_layouts: &self.struct_layouts,
            struct_field_types: &self.struct_field_types,
            struct_field_layout_names: &self.struct_field_layout_names,
            enum_layouts: &self.enum_layouts,
            memory_id: self.memory_id.unwrap(),
            heap_ptr_global: self.heap_ptr_global,
            world_globals: &self.world_globals,
            tmp_i32,
            tmp_i32_2,
            tmp_i64,
            tmp_f64,
            funcref_table: self.funcref_table,
            lambda_table: &self.lambda_table,
        };

        let mut func_body = builder.func_body();
        if let Some(result_type) = handler_info.result_type {
            if handler_info.body.stmts.is_empty() {
                self.emit_zero_for_val_type(&mut func_body, result_type);
                func_body.return_();
            } else {
                for (index, stmt) in handler_info.body.stmts.iter().enumerate() {
                    let is_last = index + 1 == handler_info.body.stmts.len();
                    if !is_last {
                        self.compile_stmt(&ctx, &mut func_body, stmt)?;
                        continue;
                    }
                    match stmt {
                        Stmt::Expr(expr) => {
                            self.compile_expr(&ctx, &mut func_body, expr)?;
                            self.coerce_expr_stack_to_val_type(
                                &ctx,
                                &mut func_body,
                                expr,
                                result_type,
                            );
                            func_body.return_();
                        }
                        Stmt::Return(Some(expr), _) => {
                            self.compile_expr(&ctx, &mut func_body, expr)?;
                            self.coerce_expr_stack_to_val_type(
                                &ctx,
                                &mut func_body,
                                expr,
                                result_type,
                            );
                            func_body.return_();
                        }
                        Stmt::Return(None, _) => {
                            self.emit_zero_for_val_type(&mut func_body, result_type);
                            func_body.return_();
                        }
                        _ => {
                            self.compile_stmt(&ctx, &mut func_body, stmt)?;
                            self.emit_zero_for_val_type(&mut func_body, result_type);
                            func_body.return_();
                        }
                    }
                }
            }
        } else {
            self.compile_block(&ctx, &mut func_body, &handler_info.body)?;
        }

        let temp_func_id = builder.finish(param_local_ids, &mut self.module.funcs);
        self.replace_reserved_function_body(handler_info.func_id, temp_func_id);
        Ok(())
    }

    fn declare_function(&mut self, func: &TypedFunction) -> KainResult<()> {
        self.declare_resolved_callable(
            &func.ast.name,
            &func.resolved_type,
            matches!(func.ast.visibility, kain_core::ast::Visibility::Public),
            func.ast.name == "main",
            func.ast.span,
        )
    }

    fn declare_patch(&mut self, patch: &kain_core::types::TypedPatch) -> KainResult<()> {
        self.declare_resolved_callable(
            &patch.ast.name,
            &patch.resolved_type,
            matches!(patch.ast.visibility, kain_core::ast::Visibility::Public),
            patch.ast.name == "main",
            patch.ast.span,
        )
    }

    fn declare_law(&mut self, law: &kain_core::types::TypedLaw) -> KainResult<()> {
        self.declare_resolved_callable(
            &law.ast.name,
            &law.resolved_type,
            matches!(law.ast.visibility, kain_core::ast::Visibility::Public),
            law.ast.name == "main",
            law.ast.span,
        )
    }

    fn declare_orchestrate(
        &mut self,
        orchestrate: &kain_core::types::TypedOrchestrate,
    ) -> KainResult<()> {
        self.declare_resolved_callable(
            &orchestrate.ast.name,
            &orchestrate.resolved_type,
            matches!(
                orchestrate.ast.visibility,
                kain_core::ast::Visibility::Public
            ),
            orchestrate.ast.name == "main",
            orchestrate.ast.span,
        )
    }

    fn declare_resolved_callable(
        &mut self,
        name: &str,
        resolved_type: &ResolvedType,
        is_public: bool,
        is_main: bool,
        span: kain_core::span::Span,
    ) -> KainResult<()> {
        let (param_types, ret_type) =
            if let ResolvedType::Function { params, ret, .. } = resolved_type {
                (params, ret)
            } else {
                return Err(KainError::codegen("Expected function type", span));
            };

        let wasm_params: Vec<ValType> = param_types.iter().map(|t| self.map_type(t)).collect();
        let wasm_results = if **ret_type == ResolvedType::Unit {
            vec![]
        } else {
            vec![self.map_type(ret_type)]
        };

        let builder = FunctionBuilder::new(&mut self.module.types, &wasm_params, &wasm_results);
        let mut param_local_ids = Vec::new();
        for &param_type in &wasm_params {
            param_local_ids.push(self.module.locals.add(param_type));
        }

        let func_id = builder.finish(param_local_ids, &mut self.module.funcs);
        self.functions.insert(name.to_string(), func_id);
        if is_public || is_main {
            self.module.exports.add(name, func_id);
        }
        Ok(())
    }

    fn declare_authored_callable(
        &mut self,
        name: &str,
        params: &[kain_core::ast::Param],
        return_type: Option<&kain_core::ast::Type>,
        is_public: bool,
        is_main: bool,
    ) -> KainResult<()> {
        let wasm_params: Vec<ValType> = params
            .iter()
            .map(|param| self.map_authored_type(&param.ty))
            .collect();
        let wasm_results = match return_type {
            Some(ty) if !matches!(ty, kain_core::ast::Type::Unit(_)) => {
                vec![self.map_authored_type(ty)]
            }
            _ => vec![],
        };

        let builder = FunctionBuilder::new(&mut self.module.types, &wasm_params, &wasm_results);
        let mut param_local_ids = Vec::new();
        for &param_type in &wasm_params {
            param_local_ids.push(self.module.locals.add(param_type));
        }
        let func_id = builder.finish(param_local_ids, &mut self.module.funcs);
        self.functions.insert(name.to_string(), func_id);
        if is_public || is_main {
            self.module.exports.add(name, func_id);
        }
        Ok(())
    }

    fn impl_method_name(&self, target_type: &kain_core::ast::Type, method_name: &str) -> String {
        match target_type {
            kain_core::ast::Type::Named { name, .. } => format!("{name}.{method_name}"),
            _ => method_name.to_string(),
        }
    }

    fn declare_impl_methods(&mut self, impl_block: &kain_core::types::TypedImpl) -> KainResult<()> {
        for method in &impl_block.ast.methods {
            let qualified = self.impl_method_name(&impl_block.ast.target_type, &method.name);
            self.declare_authored_callable(
                &qualified,
                &method.params,
                method.return_type.as_ref(),
                matches!(method.visibility, kain_core::ast::Visibility::Public),
                false,
            )?;
        }
        Ok(())
    }

    fn declare_converge(&mut self, converge: &TypedConverge) -> KainResult<()> {
        let (param_types, ret_type) =
            if let ResolvedType::Function { params, ret, .. } = &converge.resolved_type {
                (params, ret)
            } else {
                return Err(KainError::codegen(
                    "Expected converge function type",
                    converge.ast.span,
                ));
            };

        let wasm_params: Vec<ValType> = param_types.iter().map(|t| self.map_type(t)).collect();
        let wasm_results = if **ret_type == ResolvedType::Unit {
            vec![]
        } else {
            vec![self.map_type(ret_type)]
        };

        let builder = FunctionBuilder::new(&mut self.module.types, &wasm_params, &wasm_results);
        let mut param_local_ids = Vec::new();
        for &param_type in &wasm_params {
            param_local_ids.push(self.module.locals.add(param_type));
        }

        let func_id = builder.finish(param_local_ids, &mut self.module.funcs);
        self.functions.insert(converge.ast.name.clone(), func_id);

        if matches!(converge.ast.visibility, kain_core::ast::Visibility::Public)
            || converge.ast.name == "main"
        {
            self.module.exports.add(&converge.ast.name, func_id);
        }

        Ok(())
    }

    fn converge_target_matches_wasm(target: &str) -> bool {
        matches!(
            target.to_ascii_lowercase().as_str(),
            "wasm" | "webassembly" | "web" | "browser" | "kain.wasm" | "wasm32"
        )
    }

    fn selected_converge_body<'a>(&self, converge: &'a TypedConverge) -> &'a Block {
        converge
            .ast
            .fast_lanes
            .iter()
            .find(|lane| {
                matches!(
                    lane.selector.as_ref(),
                    Some(ConvergeSelector::Target(target))
                        if Self::converge_target_matches_wasm(target)
                )
            })
            .map(|lane| &lane.body)
            .unwrap_or(&converge.ast.spec_lane.body)
    }

    fn compile_converge_body(&mut self, converge: &TypedConverge) -> KainResult<()> {
        let func_id = *self.functions.get(&converge.ast.name).unwrap();
        let body = self.selected_converge_body(converge);

        let (param_types, ret_type) =
            if let ResolvedType::Function { params, ret, .. } = &converge.resolved_type {
                (params, ret)
            } else {
                return Ok(());
            };

        let wasm_params: Vec<ValType> = param_types.iter().map(|t| self.map_type(t)).collect();
        let wasm_results = if **ret_type == ResolvedType::Unit {
            vec![]
        } else {
            vec![self.map_type(ret_type)]
        };

        let mut builder = FunctionBuilder::new(&mut self.module.types, &wasm_params, &wasm_results);

        let mut text_locals_map = HashMap::new();
        let mut param_local_ids = Vec::new();

        for (i, param) in converge.ast.params.iter().enumerate() {
            let local_id = self.module.locals.add(wasm_params[i]);
            text_locals_map.insert(param.name.clone(), local_id);
            param_local_ids.push(local_id);
        }

        self.preallocate_locals(body, &mut text_locals_map);
        let mut actor_locals = HashMap::new();
        self.collect_actor_locals_in_block(body, &mut actor_locals);
        let mut layout_locals = HashMap::new();
        for (param, param_type) in converge.ast.params.iter().zip(param_types.iter()) {
            if let Some(layout_name) = self
                .layout_name_from_resolved_type(param_type)
                .or_else(|| self.layout_name_from_authored_type(&param.ty))
            {
                layout_locals.insert(param.name.clone(), layout_name);
            }
        }
        self.collect_layout_locals_in_block(body, &mut layout_locals);

        let tmp_i32 = self.module.locals.add(ValType::I32);
        let tmp_i32_2 = self.module.locals.add(ValType::I32);
        let tmp_i64 = self.module.locals.add(ValType::I64);
        let tmp_f64 = self.module.locals.add(ValType::F64);

        let ctx = CompilationContext {
            locals: text_locals_map,
            actor_locals,
            layout_locals,
            reply_ports: HashMap::new(),
            functions: &self.functions,
            constants: &self.constants,
            string_table: &self.string_table,
            struct_layouts: &self.struct_layouts,
            struct_field_types: &self.struct_field_types,
            struct_field_layout_names: &self.struct_field_layout_names,
            enum_layouts: &self.enum_layouts,
            memory_id: self.memory_id.unwrap(),
            heap_ptr_global: self.heap_ptr_global,
            world_globals: &self.world_globals,
            tmp_i32,
            tmp_i32_2,
            tmp_i64,
            tmp_f64,
            funcref_table: self.funcref_table,
            lambda_table: &self.lambda_table,
        };

        let mut func_body = builder.func_body();
        self.compile_block(&ctx, &mut func_body, body)?;

        if body.stmts.is_empty() && !wasm_results.is_empty() {
            match wasm_results[0] {
                ValType::I64 => func_body.i64_const(0),
                ValType::I32 => func_body.i32_const(0),
                ValType::F64 => func_body.f64_const(0.0),
                ValType::F32 => func_body.f32_const(0.0),
                _ => func_body.i64_const(0),
            };
        }

        let temp_func_id = builder.finish(param_local_ids, &mut self.module.funcs);
        self.replace_reserved_function_body(func_id, temp_func_id);

        Ok(())
    }

    fn replace_reserved_function_body(
        &mut self,
        reserved_func_id: walrus::FunctionId,
        temp_func_id: walrus::FunctionId,
    ) {
        let dummy_type = self.module.types.add(&[], &[]);
        let (_dummy_global_id, dummy_import_id) =
            self.module
                .add_import_global("KAIN_internal", "dummy", ValType::I32, false, false);

        let dummy_kind = walrus::FunctionKind::Import(walrus::ImportedFunction {
            import: dummy_import_id,
            ty: dummy_type,
        });

        let new_func = self.module.funcs.get_mut(temp_func_id);
        let new_kind = std::mem::replace(&mut new_func.kind, dummy_kind);

        let old_func = self.module.funcs.get_mut(reserved_func_id);
        let _old_kind = std::mem::replace(&mut old_func.kind, new_kind);

        self.module.funcs.delete(temp_func_id);
        self.module.imports.delete(dummy_import_id);
    }

    fn compile_function_body(&mut self, func: &TypedFunction) -> KainResult<()> {
        self.compile_resolved_callable_body(
            &func.ast.name,
            &func.ast.params,
            &func.ast.body,
            &func.resolved_type,
        )
    }

    fn compile_patch_body(&mut self, patch: &kain_core::types::TypedPatch) -> KainResult<()> {
        self.compile_resolved_callable_body(
            &patch.ast.name,
            &patch.ast.params,
            &patch.ast.body,
            &patch.resolved_type,
        )
    }

    fn compile_law_body(&mut self, law: &kain_core::types::TypedLaw) -> KainResult<()> {
        self.compile_resolved_callable_body(
            &law.ast.name,
            &law.ast.params,
            &law.ast.body,
            &law.resolved_type,
        )
    }

    fn compile_orchestrate_body(
        &mut self,
        orchestrate: &kain_core::types::TypedOrchestrate,
    ) -> KainResult<()> {
        self.compile_resolved_callable_body(
            &orchestrate.ast.name,
            &orchestrate.ast.params,
            &orchestrate.ast.body,
            &orchestrate.resolved_type,
        )
    }

    fn compile_resolved_callable_body(
        &mut self,
        name: &str,
        params: &[kain_core::ast::Param],
        body: &Block,
        resolved_type: &ResolvedType,
    ) -> KainResult<()> {
        let func_id = *self.functions.get(name).unwrap();

        let (param_types, ret_type) =
            if let ResolvedType::Function { params, ret, .. } = resolved_type {
                (params, ret)
            } else {
                return Ok(());
            };

        let wasm_params: Vec<ValType> = param_types.iter().map(|t| self.map_type(t)).collect();
        let wasm_results = if **ret_type == ResolvedType::Unit {
            vec![]
        } else {
            vec![self.map_type(ret_type)]
        };

        let mut builder = FunctionBuilder::new(&mut self.module.types, &wasm_params, &wasm_results);
        let mut text_locals_map = HashMap::new();
        let mut param_local_ids = Vec::new();
        for (i, param) in params.iter().enumerate() {
            let local_id = self.module.locals.add(wasm_params[i]);
            text_locals_map.insert(param.name.clone(), local_id);
            param_local_ids.push(local_id);
        }

        self.preallocate_locals(body, &mut text_locals_map);
        let mut actor_locals = HashMap::new();
        self.collect_actor_locals_in_block(body, &mut actor_locals);
        let mut layout_locals = HashMap::new();
        for (param, param_type) in params.iter().zip(param_types.iter()) {
            if let Some(layout_name) = self
                .layout_name_from_resolved_type(param_type)
                .or_else(|| self.layout_name_from_authored_type(&param.ty))
            {
                layout_locals.insert(param.name.clone(), layout_name);
            }
        }
        self.collect_layout_locals_in_block(body, &mut layout_locals);

        let tmp_i32 = self.module.locals.add(ValType::I32);
        let tmp_i32_2 = self.module.locals.add(ValType::I32);
        let tmp_i64 = self.module.locals.add(ValType::I64);
        let tmp_f64 = self.module.locals.add(ValType::F64);

        let ctx = CompilationContext {
            locals: text_locals_map,
            actor_locals,
            layout_locals,
            reply_ports: HashMap::new(),
            functions: &self.functions,
            constants: &self.constants,
            string_table: &self.string_table,
            struct_layouts: &self.struct_layouts,
            struct_field_types: &self.struct_field_types,
            struct_field_layout_names: &self.struct_field_layout_names,
            enum_layouts: &self.enum_layouts,
            memory_id: self.memory_id.unwrap(),
            heap_ptr_global: self.heap_ptr_global,
            world_globals: &self.world_globals,
            tmp_i32,
            tmp_i32_2,
            tmp_i64,
            tmp_f64,
            funcref_table: self.funcref_table,
            lambda_table: &self.lambda_table,
        };

        let mut func_body = builder.func_body();
        self.compile_block(&ctx, &mut func_body, body)?;
        if body.stmts.is_empty() && !wasm_results.is_empty() {
            match wasm_results[0] {
                ValType::I64 => func_body.i64_const(0),
                ValType::I32 => func_body.i32_const(0),
                ValType::F64 => func_body.f64_const(0.0),
                ValType::F32 => func_body.f32_const(0.0),
                _ => func_body.i64_const(0),
            };
        }

        let temp_func_id = builder.finish(param_local_ids, &mut self.module.funcs);
        self.replace_reserved_function_body(func_id, temp_func_id);
        Ok(())
    }

    fn compile_impl_methods(&mut self, impl_block: &kain_core::types::TypedImpl) -> KainResult<()> {
        for method in &impl_block.ast.methods {
            let qualified = self.impl_method_name(&impl_block.ast.target_type, &method.name);
            self.compile_authored_callable_body(
                &qualified,
                &method.params,
                method.return_type.as_ref(),
                &method.body,
            )?;
        }
        Ok(())
    }

    fn compile_authored_callable_body(
        &mut self,
        name: &str,
        params: &[kain_core::ast::Param],
        return_type: Option<&kain_core::ast::Type>,
        body: &Block,
    ) -> KainResult<()> {
        let func_id = *self.functions.get(name).unwrap();
        let wasm_params: Vec<ValType> = params
            .iter()
            .map(|param| self.map_authored_type(&param.ty))
            .collect();
        let wasm_results = match return_type {
            Some(ty) if !matches!(ty, kain_core::ast::Type::Unit(_)) => {
                vec![self.map_authored_type(ty)]
            }
            _ => vec![],
        };

        let mut builder = FunctionBuilder::new(&mut self.module.types, &wasm_params, &wasm_results);
        let mut locals = HashMap::new();
        let mut param_local_ids = Vec::new();
        for (i, param) in params.iter().enumerate() {
            let local_id = self.module.locals.add(wasm_params[i]);
            locals.insert(param.name.clone(), local_id);
            param_local_ids.push(local_id);
        }
        self.preallocate_locals(body, &mut locals);
        let mut actor_locals = HashMap::new();
        self.collect_actor_locals_in_block(body, &mut actor_locals);
        let mut layout_locals = HashMap::new();
        for param in params {
            if let Some(layout_name) = self.layout_name_from_authored_type(&param.ty) {
                layout_locals.insert(param.name.clone(), layout_name);
            }
        }
        self.collect_layout_locals_in_block(body, &mut layout_locals);

        let tmp_i32 = self.module.locals.add(ValType::I32);
        let tmp_i32_2 = self.module.locals.add(ValType::I32);
        let tmp_i64 = self.module.locals.add(ValType::I64);
        let tmp_f64 = self.module.locals.add(ValType::F64);
        let ctx = CompilationContext {
            locals,
            actor_locals,
            layout_locals,
            reply_ports: HashMap::new(),
            functions: &self.functions,
            constants: &self.constants,
            string_table: &self.string_table,
            struct_layouts: &self.struct_layouts,
            struct_field_types: &self.struct_field_types,
            struct_field_layout_names: &self.struct_field_layout_names,
            enum_layouts: &self.enum_layouts,
            memory_id: self.memory_id.unwrap(),
            heap_ptr_global: self.heap_ptr_global,
            world_globals: &self.world_globals,
            tmp_i32,
            tmp_i32_2,
            tmp_i64,
            tmp_f64,
            funcref_table: self.funcref_table,
            lambda_table: &self.lambda_table,
        };

        let mut func_body = builder.func_body();
        self.compile_block(&ctx, &mut func_body, body)?;
        if body.stmts.is_empty() && !wasm_results.is_empty() {
            match wasm_results[0] {
                ValType::I64 => func_body.i64_const(0),
                ValType::I32 => func_body.i32_const(0),
                ValType::F64 => func_body.f64_const(0.0),
                ValType::F32 => func_body.f32_const(0.0),
                _ => func_body.i64_const(0),
            };
        }

        let temp_func_id = builder.finish(param_local_ids, &mut self.module.funcs);
        self.replace_reserved_function_body(func_id, temp_func_id);
        Ok(())
    }

    fn preallocate_locals(&mut self, block: &Block, locals: &mut HashMap<String, LocalId>) {
        for stmt in &block.stmts {
            match stmt {
                Stmt::Let {
                    pattern, ty, value, ..
                } => {
                    let val_type = if let Some(authored_ty) = ty {
                        self.map_authored_type(authored_ty)
                    } else if let Some(expr) = value {
                        self.infer_wasm_type_with_locals(locals, expr)
                    } else {
                        ValType::I64
                    };
                    self.preallocate_pattern_locals(pattern, locals, val_type);
                    if let Some(expr) = value {
                        self.preallocate_locals_in_expr(expr, locals);
                    }
                }
                Stmt::Expr(expr) => self.preallocate_locals_in_expr(expr, locals),
                Stmt::Return(Some(expr), _) | Stmt::Break(Some(expr), _) => {
                    self.preallocate_locals_in_expr(expr, locals);
                }
                Stmt::While { body, .. } => {
                    self.preallocate_locals(body, locals);
                }
                Stmt::For {
                    binding,
                    iter,
                    body,
                    ..
                }
                | Stmt::Fanout {
                    binding,
                    iter,
                    body,
                    ..
                } => {
                    self.preallocate_pattern_locals(binding, locals, ValType::I64);
                    self.preallocate_locals_in_expr(iter, locals);
                    self.preallocate_locals(body, locals);
                }
                Stmt::Loop { body, .. } => {
                    self.preallocate_locals(body, locals);
                }
                _ => {}
            }
        }
    }

    fn infer_block_wasm_type_with_locals(
        &self,
        locals: &HashMap<String, LocalId>,
        block: &Block,
    ) -> ValType {
        match block.stmts.last() {
            Some(Stmt::Expr(expr)) => self.infer_wasm_type_with_locals(locals, expr),
            Some(Stmt::Return(Some(expr), _)) | Some(Stmt::Break(Some(expr), _)) => {
                self.infer_wasm_type_with_locals(locals, expr)
            }
            _ => ValType::I64,
        }
    }

    fn infer_else_branch_wasm_type_with_locals(
        &self,
        locals: &HashMap<String, LocalId>,
        branch: &kain_core::ast::ElseBranch,
    ) -> ValType {
        match branch {
            kain_core::ast::ElseBranch::Else(block) => {
                self.infer_block_wasm_type_with_locals(locals, block)
            }
            kain_core::ast::ElseBranch::ElseIf(_, then_block, next) => {
                let then_ty = self.infer_block_wasm_type_with_locals(locals, then_block);
                let else_ty = next
                    .as_deref()
                    .map(|next_branch| {
                        self.infer_else_branch_wasm_type_with_locals(locals, next_branch)
                    })
                    .unwrap_or(ValType::I64);
                if then_ty == else_ty {
                    then_ty
                } else if then_ty == ValType::F64 || else_ty == ValType::F64 {
                    ValType::F64
                } else {
                    ValType::I64
                }
            }
        }
    }

    fn infer_wasm_type_with_locals(
        &self,
        locals: &HashMap<String, LocalId>,
        expr: &Expr,
    ) -> ValType {
        match expr {
            Expr::Int(_, _) => ValType::I64,
            Expr::None(_) => ValType::I32,
            Expr::Float(_, _) => ValType::F64,
            Expr::Bool(_, _) => ValType::I32,
            Expr::String(_, _) => ValType::I32,
            Expr::Ident(name, _) if name == "None" || self.world_globals.contains_key(name) => {
                ValType::I32
            }
            Expr::Ident(name, _) => locals
                .get(name)
                .map(|local| self.module.locals.get(*local).ty())
                .or_else(|| self.constants.get(name).map(|value| value.val_type()))
                .unwrap_or(ValType::I64),
            Expr::Field { field, .. } => self
                .infer_global_field_val_type(field)
                .unwrap_or(ValType::I64),
            Expr::Paren(inner, _) => self.infer_wasm_type_with_locals(locals, inner),
            Expr::Observe { body, .. } | Expr::Collapse { body, .. } => {
                self.infer_wasm_type_with_locals(locals, body)
            }
            Expr::Teleport { value, .. }
            | Expr::Comptime(value, _)
            | Expr::Await(value, _)
            | Expr::AsyncBlock(value, _)
            | Expr::Try(value, _) => self.infer_wasm_type_with_locals(locals, value),
            Expr::Cast { target, .. } | Expr::Bitcast { target, .. } => self.map_authored_type(target),
            Expr::JSX(_, _)
            | Expr::Array(_, _)
            | Expr::Tuple(_, _)
            | Expr::Struct { .. }
            | Expr::AggregateInit { .. }
            | Expr::EnumVariant { .. }
            | Expr::Spawn { .. }
            | Expr::Alloc { .. }
            | Expr::Realloc { .. }
            | Expr::Alloca { .. }
            | Expr::Ref { .. }
            | Expr::AddrOf { .. }
            | Expr::PtrOffset { .. } => ValType::I32,
            Expr::Block(block, _) => self.infer_block_wasm_type_with_locals(locals, block),
            Expr::If {
                then_branch,
                else_branch,
                ..
            } => {
                let then_ty = self.infer_block_wasm_type_with_locals(locals, then_branch);
                let else_ty = else_branch
                    .as_ref()
                    .map(|branch| self.infer_else_branch_wasm_type_with_locals(locals, branch))
                    .unwrap_or(ValType::I64);
                if then_ty == else_ty {
                    then_ty
                } else if then_ty == ValType::F64 || else_ty == ValType::F64 {
                    ValType::F64
                } else {
                    ValType::I64
                }
            }
            Expr::Call { callee, args, .. } => {
                if let Expr::Ident(name, _) = callee.as_ref() {
                    if let Some(cast_ty) = self.primitive_call_result_type(name) {
                        return cast_ty;
                    }
                    let arg_types: Vec<_> = args
                        .iter()
                        .map(|arg| self.infer_wasm_type_with_locals(locals, &arg.value))
                        .collect();
                    if let Some(builtin_ty) = self.builtin_math_call_result_type(name, &arg_types) {
                        return builtin_ty;
                    }
                    if name
                        .chars()
                        .next()
                        .map(|c| c.is_uppercase())
                        .unwrap_or(false)
                    {
                        return ValType::I32;
                    }
                    if matches!(
                        name.as_str(),
                        "to_string" | "str_concat" | "char_at" | "str_eq"
                    ) || name.starts_with("dom_")
                    {
                        return ValType::I32;
                    }
                    if matches!(
                        name.as_str(),
                        "__kain_ptr_offset"
                            | "__kain_alloc"
                            | "__kain_realloc"
                            | "__kain_union_wrap"
                    ) {
                        return ValType::I32;
                    }
                }
                ValType::I64
            }
            Expr::MethodCall { method, args, .. } => match method.as_str() {
                "is_ok" | "is_err" | "is_some" | "is_none" => ValType::I32,
                "unwrap_or" => args
                    .first()
                    .map(|arg| self.infer_wasm_type_with_locals(locals, &arg.value))
                    .unwrap_or(ValType::I64),
                _ => ValType::I64,
            },
            Expr::Binary {
                op, left, right, ..
            } => match op {
                BinaryOp::Eq
                | BinaryOp::Ne
                | BinaryOp::Lt
                | BinaryOp::Gt
                | BinaryOp::Le
                | BinaryOp::Ge
                | BinaryOp::And
                | BinaryOp::Or => ValType::I32,
                BinaryOp::Add => {
                    let left_ty = self.infer_wasm_type_with_locals(locals, left);
                    let right_ty = self.infer_wasm_type_with_locals(locals, right);
                    if left_ty == ValType::I32 && right_ty == ValType::I32 {
                        ValType::I32
                    } else if left_ty == ValType::F64 || right_ty == ValType::F64 {
                        ValType::F64
                    } else {
                        ValType::I64
                    }
                }
                _ => {
                    let left_ty = self.infer_wasm_type_with_locals(locals, left);
                    let right_ty = self.infer_wasm_type_with_locals(locals, right);
                    if left_ty == ValType::F64 || right_ty == ValType::F64 {
                        ValType::F64
                    } else {
                        ValType::I64
                    }
                }
            },
            Expr::Unary { op, operand, .. } => match op {
                kain_core::ast::UnaryOp::Not => ValType::I32,
                _ => self.infer_wasm_type_with_locals(locals, operand),
            },
            _ => self.infer_wasm_type(expr),
        }
    }

    fn preallocate_pattern_locals(
        &mut self,
        pattern: &kain_core::ast::Pattern,
        locals: &mut HashMap<String, LocalId>,
        val_type: ValType,
    ) {
        match pattern {
            kain_core::ast::Pattern::Binding { name, .. } => {
                if !locals.contains_key(name) {
                    let local = self.module.locals.add(val_type);
                    locals.insert(name.clone(), local);
                }
            }
            kain_core::ast::Pattern::Tuple(items, _) | kain_core::ast::Pattern::Or(items, _) => {
                for item in items {
                    self.preallocate_pattern_locals(item, locals, val_type);
                }
            }
            kain_core::ast::Pattern::Struct { fields, .. } => {
                for (_, field_pattern) in fields {
                    self.preallocate_pattern_locals(field_pattern, locals, val_type);
                }
            }
            kain_core::ast::Pattern::Variant { fields, .. } => match fields {
                kain_core::ast::VariantPatternFields::Unit => {}
                kain_core::ast::VariantPatternFields::Tuple(items) => {
                    for item in items {
                        self.preallocate_pattern_locals(item, locals, val_type);
                    }
                }
                kain_core::ast::VariantPatternFields::Struct(fields) => {
                    for (_, field_pattern) in fields {
                        self.preallocate_pattern_locals(field_pattern, locals, val_type);
                    }
                }
            },
            kain_core::ast::Pattern::Slice { patterns, rest, .. } => {
                for item in patterns {
                    self.preallocate_pattern_locals(item, locals, val_type);
                }
                if let Some(rest_name) = rest {
                    if !locals.contains_key(rest_name) {
                        let local = self.module.locals.add(ValType::I32);
                        locals.insert(rest_name.clone(), local);
                    }
                }
            }
            _ => {}
        }
    }

    fn preallocate_locals_in_else(
        &mut self,
        branch: &kain_core::ast::ElseBranch,
        locals: &mut HashMap<String, LocalId>,
    ) {
        match branch {
            kain_core::ast::ElseBranch::Else(block) => self.preallocate_locals(block, locals),
            kain_core::ast::ElseBranch::ElseIf(condition, then_block, next) => {
                self.preallocate_locals_in_expr(condition, locals);
                self.preallocate_locals(then_block, locals);
                if let Some(next) = next {
                    self.preallocate_locals_in_else(next, locals);
                }
            }
        }
    }

    fn preallocate_locals_in_expr(&mut self, expr: &Expr, locals: &mut HashMap<String, LocalId>) {
        match expr {
            Expr::Paren(inner, _)
            | Expr::Deref(inner, _)
            | Expr::Comptime(inner, _)
            | Expr::Await(inner, _)
            | Expr::AsyncBlock(inner, _)
            | Expr::Try(inner, _) => self.preallocate_locals_in_expr(inner, locals),
            Expr::Unary { operand, .. }
            | Expr::Ref { value: operand, .. }
            | Expr::AddrOf { value: operand, .. }
            | Expr::Cast { value: operand, .. }
            | Expr::Bitcast { value: operand, .. }
            | Expr::Teleport { value: operand, .. } => {
                self.preallocate_locals_in_expr(operand, locals)
            }
            Expr::Binary { left, right, .. } => {
                self.preallocate_locals_in_expr(left, locals);
                self.preallocate_locals_in_expr(right, locals);
            }
            Expr::Assign { target, value, .. } => {
                self.preallocate_locals_in_expr(target, locals);
                self.preallocate_locals_in_expr(value, locals);
            }
            Expr::Call { callee, args, .. } => {
                self.preallocate_locals_in_expr(callee, locals);
                for arg in args {
                    self.preallocate_locals_in_expr(&arg.value, locals);
                }
            }
            Expr::StageCall { args, .. } => {
                for arg in args {
                    self.preallocate_locals_in_expr(&arg.value, locals);
                }
            }
            Expr::MethodCall { receiver, args, .. } => {
                self.preallocate_locals_in_expr(receiver, locals);
                for arg in args {
                    self.preallocate_locals_in_expr(&arg.value, locals);
                }
            }
            Expr::Field { object, .. } => self.preallocate_locals_in_expr(object, locals),
            Expr::Index { object, index, .. } => {
                self.preallocate_locals_in_expr(object, locals);
                self.preallocate_locals_in_expr(index, locals);
            }
            Expr::Struct { fields, rest, .. } => {
                for (_, value) in fields {
                    self.preallocate_locals_in_expr(value, locals);
                }
                if let Some(rest) = rest {
                    self.preallocate_locals_in_expr(rest, locals);
                }
            }
            Expr::AggregateInit { fields, .. } => {
                for (_, value) in fields {
                    self.preallocate_locals_in_expr(value, locals);
                }
            }
            Expr::EnumVariant { fields, .. } => match fields {
                kain_core::ast::EnumVariantFields::Unit => {}
                kain_core::ast::EnumVariantFields::Tuple(items) => {
                    for item in items {
                        self.preallocate_locals_in_expr(item, locals);
                    }
                }
                kain_core::ast::EnumVariantFields::Struct(fields) => {
                    for (_, value) in fields {
                        self.preallocate_locals_in_expr(value, locals);
                    }
                }
            },
            Expr::Array(items, _) | Expr::Tuple(items, _) | Expr::FString(items, _) => {
                for item in items {
                    self.preallocate_locals_in_expr(item, locals);
                }
            }
            Expr::Range { start, end, .. } => {
                if let Some(start) = start {
                    self.preallocate_locals_in_expr(start, locals);
                }
                if let Some(end) = end {
                    self.preallocate_locals_in_expr(end, locals);
                }
            }
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                self.preallocate_locals_in_expr(condition, locals);
                self.preallocate_locals(then_branch, locals);
                if let Some(else_branch) = else_branch {
                    self.preallocate_locals_in_else(else_branch, locals);
                }
            }
            Expr::Match {
                scrutinee, arms, ..
            } => {
                self.preallocate_locals_in_expr(scrutinee, locals);
                for arm in arms {
                    self.preallocate_pattern_locals(&arm.pattern, locals, ValType::I64);
                    if let Some(guard) = &arm.guard {
                        self.preallocate_locals_in_expr(guard, locals);
                    }
                    self.preallocate_locals_in_expr(&arm.body, locals);
                }
            }
            Expr::Observe { target, body, .. }
            | Expr::Collapse { target, body, .. }
            | Expr::Share { target, body, .. } => {
                self.preallocate_locals_in_expr(target, locals);
                self.preallocate_locals_in_expr(body, locals);
            }
            Expr::Decay { target, .. } => self.preallocate_locals_in_expr(target, locals),
            Expr::PtrOffset {
                pointer, offset, ..
            } => {
                self.preallocate_locals_in_expr(pointer, locals);
                self.preallocate_locals_in_expr(offset, locals);
            }
            Expr::MemLoad { pointer, .. }
            | Expr::VolatileLoad { pointer, .. }
            | Expr::AtomicLoad { pointer, .. }
            | Expr::CpuCacheFlush { pointer, .. } => {
                self.preallocate_locals_in_expr(pointer, locals)
            }
            Expr::MemStore { pointer, value, .. }
            | Expr::VolatileStore { pointer, value, .. }
            | Expr::AtomicStore { pointer, value, .. }
            | Expr::AtomicAdd { pointer, value, .. }
            | Expr::AtomicSub { pointer, value, .. }
            | Expr::AtomicAnd { pointer, value, .. }
            | Expr::AtomicOr { pointer, value, .. }
            | Expr::AtomicXor { pointer, value, .. }
            | Expr::AtomicExchange { pointer, value, .. } => {
                self.preallocate_locals_in_expr(pointer, locals);
                self.preallocate_locals_in_expr(value, locals);
            }
            Expr::AtomicCompareExchange {
                pointer,
                expected,
                desired,
                ..
            } => {
                self.preallocate_locals_in_expr(pointer, locals);
                self.preallocate_locals_in_expr(expected, locals);
                self.preallocate_locals_in_expr(desired, locals);
            }
            Expr::AtomicFence { .. } | Expr::CpuFence { .. } => {}
            Expr::InlineAsm { operands, .. } => {
                for operand in operands {
                    self.preallocate_locals_in_expr(operand, locals);
                }
            }
            Expr::Alloc { size, .. } => self.preallocate_locals_in_expr(size, locals),
            Expr::Realloc { pointer, size, .. } => {
                self.preallocate_locals_in_expr(pointer, locals);
                self.preallocate_locals_in_expr(size, locals);
            }
            Expr::Spawn { init, .. } => {
                for (_, value) in init {
                    self.preallocate_locals_in_expr(value, locals);
                }
            }
            Expr::SendMsg { target, data, .. } => {
                self.preallocate_locals_in_expr(target, locals);
                for (_, value) in data {
                    self.preallocate_locals_in_expr(value, locals);
                }
            }
            Expr::MacroCall { args, .. } => {
                for arg in args {
                    self.preallocate_locals_in_expr(arg, locals);
                }
            }
            Expr::Block(block, _) => self.preallocate_locals(block, locals),
            Expr::Return(Some(value), _) | Expr::Break(Some(value), _) => {
                self.preallocate_locals_in_expr(value, locals);
            }
            Expr::Return(None, _) | Expr::Break(None, _) => {}
            Expr::JSX(node, _) => self.preallocate_locals_in_jsx(node, locals),
            Expr::Lambda { .. }
            | Expr::Int(_, _)
            | Expr::Float(_, _)
            | Expr::String(_, _)
            | Expr::Bool(_, _)
            | Expr::None(_)
            | Expr::Ident(_, _)
            | Expr::SizeOfType { .. }
            | Expr::AlignOfType { .. }
            | Expr::Alloca { .. }
            | Expr::Uninit { .. }
            | Expr::Continue(_) => {}
        }
    }

    fn preallocate_locals_in_jsx(
        &mut self,
        node: &kain_core::ast::JSXNode,
        locals: &mut HashMap<String, LocalId>,
    ) {
        match node {
            kain_core::ast::JSXNode::Element {
                attributes,
                children,
                ..
            } => {
                for attr in attributes {
                    if let kain_core::ast::JSXAttrValue::Expr(expr) = &attr.value {
                        self.preallocate_locals_in_expr(expr, locals);
                    }
                }
                for child in children {
                    self.preallocate_locals_in_jsx(child, locals);
                }
            }
            kain_core::ast::JSXNode::ComponentCall {
                props, children, ..
            } => {
                for attr in props {
                    if let kain_core::ast::JSXAttrValue::Expr(expr) = &attr.value {
                        self.preallocate_locals_in_expr(expr, locals);
                    }
                }
                for child in children {
                    self.preallocate_locals_in_jsx(child, locals);
                }
            }
            kain_core::ast::JSXNode::Fragment(children, _) => {
                for child in children {
                    self.preallocate_locals_in_jsx(child, locals);
                }
            }
            kain_core::ast::JSXNode::Expression(expr) => {
                self.preallocate_locals_in_expr(expr, locals)
            }
            kain_core::ast::JSXNode::For { iter, body, .. } => {
                self.preallocate_locals_in_expr(iter, locals);
                self.preallocate_locals_in_jsx(body, locals);
            }
            kain_core::ast::JSXNode::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                self.preallocate_locals_in_expr(condition, locals);
                self.preallocate_locals_in_jsx(then_branch, locals);
                if let Some(else_branch) = else_branch {
                    self.preallocate_locals_in_jsx(else_branch, locals);
                }
            }
            kain_core::ast::JSXNode::Text(_, _) => {}
        }
    }

    fn map_type(&self, ty: &ResolvedType) -> ValType {
        match ty {
            ResolvedType::Int(_) => ValType::I64,
            ResolvedType::Float(_) => ValType::F64,
            ResolvedType::Bool => ValType::I32,
            ResolvedType::String => ValType::I32, // Strings are pointers (i32 offset)
            ResolvedType::Array(_, _)
            | ResolvedType::Slice(_)
            | ResolvedType::Tuple(_)
            | ResolvedType::Option(_)
            | ResolvedType::Result(_, _)
            | ResolvedType::Ref { .. }
            | ResolvedType::Ptr { .. }
            | ResolvedType::Function { .. }
            | ResolvedType::Struct(_, _)
            | ResolvedType::Enum(_, _) => ValType::I32,
            _ => ValType::I64,
        }
    }

    fn map_authored_type(&self, ty: &kain_core::ast::Type) -> ValType {
        match ty {
            kain_core::ast::Type::Named { name, .. } => match name.as_str() {
                "Bool" => ValType::I32,
                "Float" | "Float64" | "F64" => ValType::F64,
                "String" => ValType::I32,
                "Int" | "I64" | "U64" | "I32" | "U32" | "I16" | "U16" | "I8" | "U8" => ValType::I64,
                _ => ValType::I32,
            },
            kain_core::ast::Type::Ref { .. }
            | kain_core::ast::Type::Ptr { .. }
            | kain_core::ast::Type::Array(_, _, _)
            | kain_core::ast::Type::Slice(_, _)
            | kain_core::ast::Type::Option(_, _)
            | kain_core::ast::Type::Result(_, _, _)
            | kain_core::ast::Type::Impl { .. } => ValType::I32,
            kain_core::ast::Type::Tuple(_, _)
            | kain_core::ast::Type::Function { .. }
            | kain_core::ast::Type::Infer(_)
            | kain_core::ast::Type::Never(_)
            | kain_core::ast::Type::Unit(_) => ValType::I64,
        }
    }

    // Infer WASM ValType from an expression (for local allocation)
    fn infer_wasm_type(&self, expr: &Expr) -> ValType {
        match expr {
            Expr::Int(_, _) => ValType::I64,
            Expr::None(_) => ValType::I32,
            Expr::Float(_, _) => ValType::F64,
            Expr::Bool(_, _) => ValType::I32,
            Expr::String(_, _) => ValType::I32,
            Expr::Ident(name, _) if name == "None" || self.world_globals.contains_key(name) => {
                ValType::I32
            }
            Expr::Field { field, .. } => self
                .infer_global_field_val_type(field)
                .unwrap_or(ValType::I64),
            Expr::JSX(_, _) => ValType::I32, // JSX nodes are DOM element IDs (i32)
            Expr::Array(_, _)
            | Expr::Tuple(_, _)
            | Expr::Struct { .. }
            | Expr::AggregateInit { .. }
            | Expr::EnumVariant { .. }
            | Expr::Spawn { .. } => ValType::I32,
            Expr::Paren(inner, _) => self.infer_wasm_type(inner),
            Expr::Observe { body, .. } | Expr::Collapse { body, .. } => self.infer_wasm_type(body),
            Expr::Teleport { value, .. }
            | Expr::Comptime(value, _)
            | Expr::Await(value, _)
            | Expr::AsyncBlock(value, _)
            | Expr::Try(value, _) => self.infer_wasm_type(value),
            Expr::Cast { target, .. } | Expr::Bitcast { target, .. } => self.map_authored_type(target),
            Expr::Call { callee, args, .. } => {
                if let Expr::Ident(name, _) = callee.as_ref() {
                    if let Some(cast_ty) = self.primitive_call_result_type(name) {
                        return cast_ty;
                    }
                    let arg_types: Vec<_> =
                        args.iter().map(|arg| self.infer_wasm_type(&arg.value)).collect();
                    if let Some(builtin_ty) = self.builtin_math_call_result_type(name, &arg_types) {
                        return builtin_ty;
                    }
                    // Component calls return i32 (DOM node IDs)
                    if name
                        .chars()
                        .next()
                        .map(|c| c.is_uppercase())
                        .unwrap_or(false)
                    {
                        return ValType::I32;
                    }
                    // String functions return i32 (pointers)
                    if name == "to_string" || name == "str_concat" || name == "char_at" {
                        return ValType::I32;
                    }
                    if name == "str_eq" {
                        return ValType::I32;
                    }
                    // DOM functions return i32
                    if name.starts_with("dom_") {
                        return ValType::I32;
                    }
                    if matches!(
                        name.as_str(),
                        "__kain_ptr_offset"
                            | "__kain_alloc"
                            | "__kain_realloc"
                            | "__kain_union_wrap"
                    ) {
                        return ValType::I32;
                    }
                }
                ValType::I64 // Default for other functions
            }
            Expr::MethodCall { method, args, .. } => match method.as_str() {
                "is_ok" | "is_err" | "is_some" | "is_none" => ValType::I32,
                "unwrap_or" => args
                    .first()
                    .map(|arg| self.infer_wasm_type(&arg.value))
                    .unwrap_or(ValType::I64),
                _ => ValType::I64,
            },
            Expr::Binary { op, .. } => {
                // Most binary ops return same type as operands
                // Comparisons return bool (i32)
                match op {
                    BinaryOp::Eq
                    | BinaryOp::Ne
                    | BinaryOp::Lt
                    | BinaryOp::Gt
                    | BinaryOp::Le
                    | BinaryOp::Ge
                    | BinaryOp::And
                    | BinaryOp::Or => ValType::I32,
                    _ => ValType::I64,
                }
            }
            Expr::Unary { op, .. } => match op {
                kain_core::ast::UnaryOp::Not => ValType::I32,
                _ => ValType::I64,
            },
            _ => ValType::I64, // Default fallback
        }
    }

    /// Allocate a string literal in the data segment
    /// Returns the memory offset where the string starts
    /// Format: [length: 4 bytes][utf8 data]
    fn allocate_string(&mut self, s: &str) -> u32 {
        // Check if string already allocated (deduplication)
        if let Some(&offset) = self.string_table.get(s) {
            return offset;
        }

        let offset = self.data_offset;
        let bytes = s.as_bytes();
        let len = bytes.len() as u32;

        // Build data: length (4 bytes, little-endian) + string bytes
        let mut data = Vec::with_capacity(4 + bytes.len());
        data.extend_from_slice(&len.to_le_bytes());
        data.extend_from_slice(bytes);

        // Add to data segment
        if let Some(memory_id) = self.memory_id {
            self.module.data.add(
                walrus::DataKind::Active {
                    memory: memory_id,
                    offset: walrus::ConstExpr::Value(walrus::ir::Value::I32(offset as i32)),
                },
                data,
            );
        }

        // Update offset for next allocation
        self.data_offset += 4 + len;
        // Align to 4 bytes
        self.data_offset = (self.data_offset + 3) & !3;

        // Cache for deduplication
        self.string_table.insert(s.to_string(), offset);

        offset
    }

    // --- Compilation Logic (Stateless regarding Module, uses passed Builder) ---

    fn compile_block(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        block: &Block,
    ) -> KainResult<()> {
        for stmt in &block.stmts {
            self.compile_stmt(ctx, builder, stmt)?;
        }
        Ok(())
    }

    fn temp_local_for_expr(&self, ctx: &CompilationContext, expr: &Expr) -> LocalId {
        match self.infer_expr_wasm_type_in_context(ctx, expr) {
            ValType::I32 => ctx.tmp_i32,
            ValType::F64 => ctx.tmp_f64,
            ValType::F32 => ctx.tmp_f64,
            _ => ctx.tmp_i64,
        }
    }

    fn compile_field_address(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        object: &Expr,
        field: &str,
        span: kain_core::span::Span,
    ) -> KainResult<()> {
        self.compile_expr_as_i32(ctx, builder, object)?;
        let field_offset = self
            .resolve_field_offset_in_context(ctx, object, field)
            .map_err(|reason| match reason {
                "ambiguous" => KainError::codegen(
                    format!(
                        "Field '{}' layout is ambiguous in WASM codegen for object {:?} with local layout {:?}",
                        field,
                        object,
                        match object {
                            Expr::Ident(name, _) => ctx.layout_locals.get(name).cloned(),
                            _ => self.resolve_layout_name_in_context(ctx, object),
                        }
                    ),
                    span,
                ),
                _ => KainError::codegen(format!("Field '{}' layout not found", field), span),
            })?;
        if field_offset > 0 {
            builder.i32_const(field_offset as i32);
            builder.binop(walrus::ir::BinaryOp::I32Add);
        }
        Ok(())
    }

    fn emit_store_for_val_type(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        val_type: ValType,
        offset: u32,
    ) {
        match val_type {
            ValType::I32 => builder.store(
                ctx.memory_id,
                walrus::ir::StoreKind::I32 { atomic: false },
                walrus::ir::MemArg { align: 4, offset },
            ),
            ValType::F32 => builder.store(
                ctx.memory_id,
                walrus::ir::StoreKind::F32,
                walrus::ir::MemArg { align: 4, offset },
            ),
            ValType::F64 => builder.store(
                ctx.memory_id,
                walrus::ir::StoreKind::F64,
                walrus::ir::MemArg { align: 8, offset },
            ),
            _ => builder.store(
                ctx.memory_id,
                walrus::ir::StoreKind::I64 { atomic: false },
                walrus::ir::MemArg { align: 8, offset },
            ),
        };
    }

    fn emit_load_for_val_type(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        val_type: ValType,
        offset: u32,
    ) {
        match val_type {
            ValType::I32 => builder.load(
                ctx.memory_id,
                walrus::ir::LoadKind::I32 { atomic: false },
                walrus::ir::MemArg { align: 4, offset },
            ),
            ValType::F32 => builder.load(
                ctx.memory_id,
                walrus::ir::LoadKind::F32,
                walrus::ir::MemArg { align: 4, offset },
            ),
            ValType::F64 => builder.load(
                ctx.memory_id,
                walrus::ir::LoadKind::F64,
                walrus::ir::MemArg { align: 8, offset },
            ),
            _ => builder.load(
                ctx.memory_id,
                walrus::ir::LoadKind::I64 { atomic: false },
                walrus::ir::MemArg { align: 8, offset },
            ),
        };
    }

    fn emit_zero_for_val_type(&self, builder: &mut InstrSeqBuilder, val_type: ValType) {
        match val_type {
            ValType::I32 => builder.i32_const(0),
            ValType::F32 => builder.f32_const(0.0),
            ValType::F64 => builder.f64_const(0.0),
            _ => builder.i64_const(0),
        };
    }

    fn compile_builtin_tagged_value(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        tag: u32,
        payload: &Expr,
    ) -> KainResult<()> {
        let total_size = 16u32;
        self.emit_alloc(ctx, builder, total_size);
        builder.drop();

        builder.global_get(ctx.heap_ptr_global);
        builder.i32_const(total_size as i32);
        builder.binop(walrus::ir::BinaryOp::I32Sub);
        builder.i32_const(tag as i32);
        builder.store(
            ctx.memory_id,
            walrus::ir::StoreKind::I32 { atomic: false },
            walrus::ir::MemArg {
                align: 4,
                offset: 0,
            },
        );

        builder.global_get(ctx.heap_ptr_global);
        builder.i32_const(total_size as i32);
        builder.binop(walrus::ir::BinaryOp::I32Sub);
        builder.i32_const(8);
        builder.binop(walrus::ir::BinaryOp::I32Add);
        self.compile_expr(ctx, builder, payload)?;
        self.emit_store_for_expr(ctx, builder, payload, 0);

        builder.global_get(ctx.heap_ptr_global);
        builder.i32_const(total_size as i32);
        builder.binop(walrus::ir::BinaryOp::I32Sub);
        Ok(())
    }

    fn compile_builtin_tagged_constructor_call(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        func_name: &str,
        args: &[CallArg],
        span: kain_core::span::Span,
    ) -> KainResult<bool> {
        let tag = match func_name {
            "Some" | "Ok" => 0u32,
            "Err" => 1u32,
            _ => return Ok(false),
        };

        if args.len() != 1 {
            return Err(KainError::codegen(
                format!(
                    "Builtin constructor '{}' expects exactly one argument",
                    func_name
                ),
                span,
            ));
        }

        self.compile_builtin_tagged_value(ctx, builder, tag, &args[0].value)?;
        Ok(true)
    }

    fn compile_builtin_tagged_method_call(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        receiver: &Expr,
        method: &str,
        args: &[CallArg],
        span: kain_core::span::Span,
    ) -> KainResult<bool> {
        match method {
            "is_ok" | "is_some" | "is_err" | "is_none" => {
                if !args.is_empty() {
                    return Err(KainError::codegen(
                        format!("Builtin method '{}' expects no arguments", method),
                        span,
                    ));
                }

                self.compile_expr_as_i32(ctx, builder, receiver)?;
                builder.local_set(ctx.tmp_i32_2);

                let branch_error = std::cell::RefCell::new(None);
                builder.local_get(ctx.tmp_i32_2);
                builder.unop(walrus::ir::UnaryOp::I32Eqz);
                builder.if_else(
                    None,
                    |then_builder| {
                        match method {
                            "is_none" => then_builder.i32_const(1),
                            _ => then_builder.i32_const(0),
                        };
                    },
                    |else_builder| {
                        else_builder.local_get(ctx.tmp_i32_2);
                        self.emit_load_for_val_type(ctx, else_builder, ValType::I32, 0);
                        else_builder.unop(walrus::ir::UnaryOp::I32Eqz);
                        match method {
                            "is_ok" | "is_some" => {}
                            "is_err" | "is_none" => {
                                else_builder.unop(walrus::ir::UnaryOp::I32Eqz);
                            }
                            _ => {
                                *branch_error.borrow_mut() = Some(KainError::codegen(
                                    format!("Unsupported builtin tagged method '{}'", method),
                                    span,
                                ));
                            }
                        }
                    },
                );

                if let Some(err) = branch_error.into_inner() {
                    return Err(err);
                }

                Ok(true)
            }
            "unwrap_or" => {
                if args.len() != 1 {
                    return Err(KainError::codegen(
                        "Builtin unwrap_or expects exactly one argument",
                        span,
                    ));
                }

                let result_type = self.infer_wasm_type(&args[0].value);
                let result_local = match result_type {
                    ValType::I32 => ctx.tmp_i32,
                    ValType::F64 => ctx.tmp_f64,
                    _ => ctx.tmp_i64,
                };

                self.compile_expr_as_i32(ctx, builder, receiver)?;
                builder.local_set(ctx.tmp_i32_2);

                let branch_error = std::cell::RefCell::new(None);
                builder.local_get(ctx.tmp_i32_2);
                builder.unop(walrus::ir::UnaryOp::I32Eqz);
                builder.if_else(
                    None,
                    |then_builder| {
                        if let Err(err) = self.compile_expr(ctx, then_builder, &args[0].value) {
                            *branch_error.borrow_mut() = Some(err);
                            return;
                        }
                        then_builder.local_set(result_local);
                    },
                    |else_builder| {
                        else_builder.local_get(ctx.tmp_i32_2);
                        self.emit_load_for_val_type(ctx, else_builder, ValType::I32, 0);
                        else_builder.unop(walrus::ir::UnaryOp::I32Eqz);
                        else_builder.if_else(
                            None,
                            |ok_builder| {
                                ok_builder.local_get(ctx.tmp_i32_2);
                                self.emit_load_for_val_type(ctx, ok_builder, result_type, 8);
                                ok_builder.local_set(result_local);
                            },
                            |fallback_builder| {
                                if let Err(err) =
                                    self.compile_expr(ctx, fallback_builder, &args[0].value)
                                {
                                    *branch_error.borrow_mut() = Some(err);
                                    return;
                                }
                                fallback_builder.local_set(result_local);
                            },
                        );
                    },
                );

                if let Some(err) = branch_error.into_inner() {
                    return Err(err);
                }

                builder.local_get(result_local);
                Ok(true)
            }
            "unwrap" => {
                if !args.is_empty() {
                    return Err(KainError::codegen(
                        "Builtin unwrap expects no arguments",
                        span,
                    ));
                }

                let result_local = ctx.tmp_i64;

                self.compile_expr_as_i32(ctx, builder, receiver)?;
                builder.local_set(ctx.tmp_i32_2);

                builder.local_get(ctx.tmp_i32_2);
                builder.unop(walrus::ir::UnaryOp::I32Eqz);

                builder.if_else(
                    None,
                    |then_builder| {
                        self.emit_zero_for_val_type(then_builder, ValType::I64);
                        then_builder.local_set(result_local);
                    },
                    |else_builder| {
                        else_builder.local_get(ctx.tmp_i32_2);
                        self.emit_load_for_val_type(ctx, else_builder, ValType::I32, 0);
                        else_builder.unop(walrus::ir::UnaryOp::I32Eqz);
                        else_builder.if_else(
                            None,
                            |ok_builder| {
                                ok_builder.local_get(ctx.tmp_i32_2);
                                self.emit_load_for_val_type(ctx, ok_builder, ValType::I64, 8);
                                ok_builder.local_set(result_local);
                            },
                            |err_builder| {
                                self.emit_zero_for_val_type(err_builder, ValType::I64);
                                err_builder.local_set(result_local);
                            },
                        );
                    },
                );

                builder.local_get(result_local);
                Ok(true)
            }
            _ => Ok(false),
        }
    }

    fn compile_variant_constructor_call(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        variant_name: &str,
        args: &[CallArg],
        span: kain_core::span::Span,
    ) -> KainResult<bool> {
        let mut matched_variant: Option<(String, u32, u32, HashMap<String, u32>)> = None;

        for (enum_name, (tags, max_payload, field_offsets_map)) in ctx.enum_layouts.iter() {
            if let Some(&tag) = tags.get(variant_name) {
                if matched_variant.is_some() {
                    return Err(KainError::codegen(
                        format!(
                            "Enum variant constructor '{}' is ambiguous in WASM codegen",
                            variant_name
                        ),
                        span,
                    ));
                }

                matched_variant = Some((
                    enum_name.clone(),
                    tag,
                    *max_payload,
                    field_offsets_map
                        .get(variant_name)
                        .cloned()
                        .unwrap_or_default(),
                ));
            }
        }

        let Some((enum_name, tag, max_payload, variant_offsets)) = matched_variant else {
            return Ok(false);
        };

        if !variant_offsets.is_empty() && args.len() != variant_offsets.len() {
            return Err(KainError::codegen(
                format!(
                    "Enum variant constructor '{}::{}' expected {} payload values but got {}",
                    enum_name,
                    variant_name,
                    variant_offsets.len(),
                    args.len()
                ),
                span,
            ));
        }

        let total_size = 4 + max_payload;
        self.emit_alloc(ctx, builder, total_size);
        builder.drop();

        let aligned_size = (total_size + 7) & !7;

        builder.global_get(ctx.heap_ptr_global);
        builder.i32_const(aligned_size as i32);
        builder.binop(walrus::ir::BinaryOp::I32Sub);
        builder.i32_const(tag as i32);
        builder.store(
            ctx.memory_id,
            walrus::ir::StoreKind::I32 { atomic: false },
            walrus::ir::MemArg {
                align: 4,
                offset: 0,
            },
        );

        for (i, arg) in args.iter().enumerate() {
            let key = i.to_string();
            let Some(&offset) = variant_offsets.get(&key) else {
                return Err(KainError::codegen(
                    format!(
                        "Enum variant constructor '{}::{}' is missing tuple offset {}",
                        enum_name, variant_name, i
                    ),
                    span,
                ));
            };

            builder.global_get(ctx.heap_ptr_global);
            builder.i32_const(aligned_size as i32);
            builder.binop(walrus::ir::BinaryOp::I32Sub);
            builder.i32_const((4 + offset) as i32);
            builder.binop(walrus::ir::BinaryOp::I32Add);

            self.compile_expr(ctx, builder, &arg.value)?;
            self.emit_store_for_expr(ctx, builder, &arg.value, 0);
        }

        builder.global_get(ctx.heap_ptr_global);
        builder.i32_const(aligned_size as i32);
        builder.binop(walrus::ir::BinaryOp::I32Sub);

        Ok(true)
    }

    fn compile_direct_actor_send(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        target: &Expr,
        message: &str,
        data: &[(String, Expr)],
        span: kain_core::span::Span,
    ) -> KainResult<bool> {
        if let Expr::Ident(name, _) = target {
            if let Some(&reply_type) = ctx.reply_ports.get(name) {
                if message == "Reply" {
                    if let Some((_, value_expr)) = data
                        .iter()
                        .find(|(field, _)| field == "value")
                        .or_else(|| data.first())
                    {
                        self.compile_expr(ctx, builder, value_expr)?;
                        self.coerce_expr_stack_to_val_type(ctx, builder, value_expr, reply_type);
                    } else {
                        self.emit_zero_for_val_type(builder, reply_type);
                    }
                    return Ok(true);
                }
            }
        }

        let Some(actor_name) = self.resolve_actor_name_in_context(ctx, target) else {
            return Ok(false);
        };
        let key = Self::actor_handler_key(actor_name, message);
        let Some(handler) = self.actor_handlers.get(&key) else {
            return Ok(false);
        };

        let data_map: HashMap<&str, &Expr> = data
            .iter()
            .map(|(field, value)| (field.as_str(), value))
            .collect();

        self.compile_expr_as_i32(ctx, builder, target)?;
        for (param_name, param_ty) in &handler.params {
            if handler.reply_ports.contains_key(param_name) {
                if let Some(value_expr) = data_map.get(param_name.as_str()).copied() {
                    self.compile_expr(ctx, builder, value_expr)?;
                    self.coerce_expr_stack_to_val_type(ctx, builder, value_expr, *param_ty);
                } else {
                    builder.i32_const(0);
                }
                continue;
            }

            let Some(value_expr) = data_map.get(param_name.as_str()).copied() else {
                return Err(KainError::codegen(
                    format!(
                        "WASM actor send '{}.{}' is missing payload field '{}'",
                        actor_name, message, param_name
                    ),
                    span,
                ));
            };
            self.compile_expr(ctx, builder, value_expr)?;
            self.coerce_expr_stack_to_val_type(ctx, builder, value_expr, *param_ty);
        }

        builder.call(handler.func_id);
        if handler.result_type.is_some() {
            builder.drop();
        }
        builder.i64_const(0);
        Ok(true)
    }

    fn compile_actor_ask_call(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        args: &[CallArg],
        span: kain_core::span::Span,
    ) -> KainResult<()> {
        if args.len() != 3 && args.len() != 4 {
            return Err(KainError::codegen(
                "ask/ask_timeout expects (actor, message, request[, timeout_ms])",
                span,
            ));
        }

        let Some(actor_name) = self.resolve_actor_name_in_context(ctx, &args[0].value) else {
            return Err(KainError::codegen(
                "WASM ask currently requires a directly resolved actor handle local or spawn expression",
                span,
            ));
        };
        let message_name = match &args[1].value {
            Expr::String(value, _) => value.as_str(),
            _ => {
                return Err(KainError::codegen(
                    "WASM ask currently requires a literal actor message name",
                    span,
                ))
            }
        };
        let key = Self::actor_handler_key(actor_name, message_name);
        let Some(handler) = self.actor_handlers.get(&key) else {
            return Err(KainError::codegen(
                format!(
                    "WASM actor handler '{}.{}' was not declared",
                    actor_name, message_name
                ),
                span,
            ));
        };
        if handler.result_type.is_none() {
            return Err(KainError::codegen(
                format!(
                    "WASM ask requires '{}.{}' to reply through a reply port send",
                    actor_name, message_name
                ),
                span,
            ));
        }

        let payload_params: Vec<_> = handler
            .params
            .iter()
            .filter(|(name, _)| !handler.reply_ports.contains_key(name))
            .collect();
        if payload_params.len() != 1 {
            return Err(KainError::codegen(
                format!(
                    "WASM ask currently supports one request payload parameter for '{}.{}'",
                    actor_name, message_name
                ),
                span,
            ));
        }

        self.compile_expr_as_i32(ctx, builder, &args[0].value)?;
        for (param_name, param_ty) in &handler.params {
            if handler.reply_ports.contains_key(param_name) {
                builder.i32_const(0);
            } else {
                self.compile_expr(ctx, builder, &args[2].value)?;
                self.coerce_expr_stack_to_val_type(ctx, builder, &args[2].value, *param_ty);
            }
        }
        builder.call(handler.func_id);
        Ok(())
    }

    fn stmt_span(stmt: &Stmt) -> kain_core::span::Span {
        match stmt {
            Stmt::Let { span, .. }
            | Stmt::Return(_, span)
            | Stmt::Break(_, span)
            | Stmt::Continue(span)
            | Stmt::For { span, .. }
            | Stmt::While { span, .. }
            | Stmt::Loop { span, .. }
            | Stmt::Fanout { span, .. } => *span,
            Stmt::Expr(expr) => expr.span(),
            Stmt::Item(_) => kain_core::span::Span::default(),
        }
    }

    fn compile_stmt(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        stmt: &Stmt,
    ) -> KainResult<()> {
        match stmt {
            Stmt::Expr(expr) => {
                self.compile_expr(ctx, builder, expr)?;
                // Control-flow-only if statements do not leave a stack value.
                if !matches!(expr, Expr::If { .. }) {
                    builder.drop();
                }
            }
            Stmt::Let { value, pattern, .. } => {
                if let Some(val_expr) = value {
                    if let kain_core::ast::Pattern::Binding { name, .. } = pattern {
                        if let Some(local_id) = ctx.locals.get(name) {
                            let local_ty = self.module.locals.get(*local_id).ty();
                            let value_is_i32 = self.expr_is_i32_in_context(ctx, val_expr);
                            self.compile_expr(ctx, builder, val_expr)?;
                            match local_ty {
                                ValType::I32 if !value_is_i32 => {
                                    builder.unop(walrus::ir::UnaryOp::I32WrapI64);
                                }
                                ValType::I64 if value_is_i32 => {
                                    builder.unop(walrus::ir::UnaryOp::I64ExtendSI32);
                                }
                                _ => {}
                            }
                            builder.local_set(*local_id);
                        } else {
                            self.compile_expr(ctx, builder, val_expr)?;
                            builder.drop();
                        }
                    } else {
                        self.compile_expr(ctx, builder, val_expr)?;
                        builder.drop();
                    }
                }
            }
            Stmt::Return(opt_expr, _) => {
                if let Some(expr) = opt_expr {
                    self.compile_expr(ctx, builder, expr)?;
                }
                builder.return_();
            }
            Stmt::While {
                condition, body, ..
            } => {
                let loop_error = std::cell::RefCell::new(None);
                builder.block(None, |block_builder| {
                    let block_id = block_builder.id();

                    block_builder.loop_(None, |loop_builder| {
                        let loop_id = loop_builder.id();

                        if let Err(err) = self.compile_expr(ctx, loop_builder, condition) {
                            *loop_error.borrow_mut() = Some(err);
                            return;
                        }

                        loop_builder.unop(walrus::ir::UnaryOp::I32Eqz);
                        loop_builder.br_if(block_id);

                        if let Err(err) = self.compile_block(ctx, loop_builder, body) {
                            *loop_error.borrow_mut() = Some(err);
                            return;
                        }

                        loop_builder.br(loop_id);
                    });
                });
                if let Some(err) = loop_error.into_inner() {
                    return Err(err);
                }
            }
            // For loop: `for i in start..end: body`
            // Desugars to: let i = start; while i < end: body; i = i + 1
            Stmt::For {
                binding,
                iter,
                body,
                span: _,
            } => {
                // Get the loop variable name
                let loop_var = match binding {
                    kain_core::ast::Pattern::Binding { name, .. } => name.clone(),
                    _ => "".to_string(),
                };

                let (start_expr, end_expr, inclusive) = match iter {
                    Expr::Range {
                        start,
                        end,
                        inclusive,
                        ..
                    } => (start.as_deref(), end.as_deref(), *inclusive),
                    Expr::Call { callee, args, .. } if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "range") => {
                        match args.len() {
                            1 => (None, Some(&args[0].value), false),
                            2 => (Some(&args[0].value), Some(&args[1].value), false),
                            _ => {
                                return Err(KainError::codegen(
                                    "WASM range(...) expects one or two arguments",
                                    iter.span(),
                                ));
                            }
                        }
                    }
                    _ => {
                        return Err(KainError::codegen(
                            "Only range iterators are supported in WASM codegen",
                            iter.span(),
                        ));
                    }
                };

                // Initialize loop variable with start value
                if let Some(start_e) = start_expr {
                    self.compile_expr(ctx, builder, start_e)?;
                } else {
                    builder.i64_const(0);
                }

                if let Some(local_id) = ctx.locals.get(&loop_var) {
                    builder.local_set(*local_id);
                }

                // block { loop { if i >= end: break; body; i++; br loop } }
                let loop_error = std::cell::RefCell::new(None);
                builder.block(None, |block_builder| {
                    let block_id = block_builder.id();

                    block_builder.loop_(None, |loop_builder| {
                        let loop_id = loop_builder.id();

                        // Check condition: i < end (or i <= end if inclusive)
                        if let Some(local_id) = ctx.locals.get(&loop_var) {
                            loop_builder.local_get(*local_id);
                        }

                        if let Some(end_e) = end_expr {
                            if let Err(err) = self.compile_expr(ctx, loop_builder, end_e) {
                                *loop_error.borrow_mut() = Some(err);
                                return;
                            }
                        } else {
                            loop_builder.i64_const(i64::MAX);
                        }

                        // Compare: if i >= end (or i > end if inclusive), break
                        if inclusive {
                            loop_builder.binop(walrus::ir::BinaryOp::I64GtS);
                        } else {
                            loop_builder.binop(walrus::ir::BinaryOp::I64GeS);
                        }
                        loop_builder.br_if(block_id);

                        // Execute body
                        if let Err(err) = self.compile_block(ctx, loop_builder, body) {
                            *loop_error.borrow_mut() = Some(err);
                            return;
                        }

                        // Increment loop variable: i = i + 1
                        if let Some(local_id) = ctx.locals.get(&loop_var) {
                            loop_builder.local_get(*local_id);
                            loop_builder.i64_const(1);
                            loop_builder.binop(walrus::ir::BinaryOp::I64Add);
                            loop_builder.local_set(*local_id);
                        }

                        loop_builder.br(loop_id);
                    });
                });
                if let Some(err) = loop_error.into_inner() {
                    return Err(err);
                }
            }
            // Infinite loop: `loop: body` - can be exited with break
            Stmt::Loop { body, span: _ } => {
                let loop_error = std::cell::RefCell::new(None);
                builder.block(None, |block_builder| {
                    let _block_id = block_builder.id();

                    block_builder.loop_(None, |loop_builder| {
                        let loop_id = loop_builder.id();

                        // Execute body
                        if let Err(err) = self.compile_block(ctx, loop_builder, body) {
                            *loop_error.borrow_mut() = Some(err);
                            return;
                        }

                        // Continue loop
                        loop_builder.br(loop_id);
                    });
                });
                if let Some(err) = loop_error.into_inner() {
                    return Err(err);
                }
            }
            Stmt::Fanout { span, .. } => {
                return Err(KainError::codegen(
                    "WASM backend does not support shared fanout lowering",
                    *span,
                ));
            }
            // Break statement
            Stmt::Break(_, _) => {
                // Break out of innermost block
                // Note: This is simplified - would need proper block tracking for nested loops
                builder.unreachable(); // Placeholder - real impl needs block ID tracking
            }
            // Continue statement
            Stmt::Continue(_) => {
                // Jump to loop header
                builder.unreachable(); // Placeholder - real impl needs loop ID tracking
            }
            _ => {
                return Err(KainError::codegen(
                    "Unsupported statement in WASM codegen",
                    Self::stmt_span(stmt),
                ));
            }
        }
        Ok(())
    }

    fn is_string_expr(&self, expr: &Expr) -> bool {
        match expr {
            Expr::String(_, _) => true,
            Expr::Paren(inner, _) => self.is_string_expr(inner),
            Expr::Call { callee, .. } => {
                if let Expr::Ident(name, _) = callee.as_ref() {
                    name == "to_string" || name == "str_concat" || name == "char_at"
                } else {
                    false
                }
            }
            Expr::Binary {
                op, left, right, ..
            } => match op {
                BinaryOp::Add => self.is_string_expr(left) || self.is_string_expr(right),
                _ => false,
            },
            _ => false,
        }
    }

    // Check if expression produces an i32 (JSX node IDs, bools, etc)
    fn is_i32_expr(&self, expr: &Expr) -> bool {
        match expr {
            Expr::None(_) => true,
            Expr::JSX(_, _) => true,
            Expr::Bool(_, _) => true,
            Expr::String(_, _) => true, // Strings are i32 pointers
            Expr::Array(_, _)
            | Expr::Tuple(_, _)
            | Expr::Struct { .. }
            | Expr::AggregateInit { .. }
            | Expr::EnumVariant { .. }
            | Expr::Spawn { .. } => true,
            Expr::Paren(inner, _) => self.is_i32_expr(inner),
            Expr::Observe { body, .. } | Expr::Collapse { body, .. } => self.is_i32_expr(body),
            Expr::Teleport { value, .. }
            | Expr::Cast { value, .. }
            | Expr::Bitcast { value, .. }
            | Expr::Comptime(value, _)
            | Expr::Await(value, _)
            | Expr::AsyncBlock(value, _)
            | Expr::Try(value, _) => self.is_i32_expr(value),
            Expr::Call { callee, .. } => {
                if let Expr::Ident(name, _) = callee.as_ref() {
                    if let Some(result_ty) = self.primitive_call_result_type(name) {
                        return result_ty == ValType::I32;
                    }
                    // Component calls and DOM functions return i32
                    name.chars()
                        .next()
                        .map(|c| c.is_uppercase())
                        .unwrap_or(false)
                        || name.starts_with("dom_")
                        || name == "to_string"
                        || name == "str_concat"
                        || name == "char_at"
                        || name == "str_eq"
                } else {
                    false
                }
            }
            Expr::MethodCall { method, args, .. } => match method.as_str() {
                "is_ok" | "is_err" | "is_some" | "is_none" => true,
                "unwrap_or" => args
                    .first()
                    .map(|arg| self.is_i32_expr(&arg.value))
                    .unwrap_or(false),
                _ => false,
            },
            // For identifiers, we can't know without context - return false and handle separately
            Expr::Ident(name, _) => name == "None", // Other identifiers are checked via context
            _ => false,
        }
    }

    // Check if an identifier refers to an i32 local (needs module access)
    fn is_i32_local(&self, name: &str, locals: &HashMap<String, LocalId>) -> bool {
        if let Some(local_id) = locals.get(name) {
            // Check the local's type in the module
            let local = self.module.locals.get(*local_id);
            return local.ty() == ValType::I32;
        }
        false
    }

    fn expr_is_i32_in_context(&self, ctx: &CompilationContext, expr: &Expr) -> bool {
        self.infer_expr_wasm_type_in_context(ctx, expr) == ValType::I32
            || matches!(
                expr,
                Expr::Call { callee, args, .. }
                    if matches!(callee.as_ref(), Expr::Ident(name, _) if name == "ask" || name == "ask_timeout")
                        && self.actor_handler_result_type_for_call(ctx, args) == Some(ValType::I32)
            )
    }

    fn expr_string_literal<'a>(&self, expr: &'a Expr) -> Option<&'a str> {
        match expr {
            Expr::String(value, _) => Some(value.as_str()),
            _ => None,
        }
    }

    fn expr_int_literal(&self, expr: &Expr) -> Option<i64> {
        match expr {
            Expr::Int(value, _) => Some(*value),
            _ => None,
        }
    }

    fn expr_bool_literal(&self, expr: &Expr) -> Option<bool> {
        match expr {
            Expr::Bool(value, _) => Some(*value),
            _ => None,
        }
    }

    fn emit_wasm_load_for_type_key(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        type_key: &str,
        byte_size: i64,
    ) {
        match type_key {
            "Float" if byte_size <= 4 => {
                builder.load(
                    ctx.memory_id,
                    walrus::ir::LoadKind::F32,
                    walrus::ir::MemArg {
                        align: 4,
                        offset: 0,
                    },
                );
                builder.unop(walrus::ir::UnaryOp::F64PromoteF32);
            }
            "Float" => {
                builder.load(
                    ctx.memory_id,
                    walrus::ir::LoadKind::F64,
                    walrus::ir::MemArg {
                        align: 8,
                        offset: 0,
                    },
                );
            }
            "Bool" | "Char" => {
                builder.load(
                    ctx.memory_id,
                    walrus::ir::LoadKind::I32_8 {
                        kind: walrus::ir::ExtendedLoad::ZeroExtend,
                    },
                    walrus::ir::MemArg {
                        align: 1,
                        offset: 0,
                    },
                );
                builder.unop(walrus::ir::UnaryOp::I64ExtendUI32);
            }
            _ => {
                builder.load(
                    ctx.memory_id,
                    walrus::ir::LoadKind::I64 { atomic: false },
                    walrus::ir::MemArg {
                        align: 8,
                        offset: 0,
                    },
                );
            }
        }
    }

    fn emit_wasm_store_for_type_key(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        type_key: &str,
        byte_size: i64,
    ) {
        match type_key {
            "Float" if byte_size <= 4 => {
                builder.unop(walrus::ir::UnaryOp::F32DemoteF64);
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::F32,
                    walrus::ir::MemArg {
                        align: 4,
                        offset: 0,
                    },
                );
            }
            "Float" => {
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::F64,
                    walrus::ir::MemArg {
                        align: 8,
                        offset: 0,
                    },
                );
            }
            "Bool" | "Char" => {
                builder.unop(walrus::ir::UnaryOp::I32WrapI64);
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::I32_8 { atomic: false },
                    walrus::ir::MemArg {
                        align: 1,
                        offset: 0,
                    },
                );
            }
            _ => {
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::I64 { atomic: false },
                    walrus::ir::MemArg {
                        align: 8,
                        offset: 0,
                    },
                );
            }
        }
    }

    fn compile_low_level_helper_call(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        func_name: &str,
        args: &[kain_core::ast::CallArg],
    ) -> KainResult<bool> {
        match func_name {
            "__kain_ptr_offset" if args.len() >= 3 => {
                self.compile_expr_as_i32(ctx, builder, &args[0].value)?;
                self.compile_expr_as_i32(ctx, builder, &args[1].value)?;
                self.compile_expr_as_i32(ctx, builder, &args[2].value)?;
                builder.binop(walrus::ir::BinaryOp::I32Mul);
                builder.binop(walrus::ir::BinaryOp::I32Add);
                Ok(true)
            }
            "__kain_mem_load" if !args.is_empty() => {
                self.compile_expr_as_i32(ctx, builder, &args[0].value)?;
                builder.load(
                    ctx.memory_id,
                    walrus::ir::LoadKind::I64 { atomic: false },
                    walrus::ir::MemArg {
                        align: 8,
                        offset: 0,
                    },
                );
                Ok(true)
            }
            "__kain_mem_store" if args.len() >= 2 => {
                self.compile_expr_as_i32(ctx, builder, &args[0].value)?;
                builder.local_set(ctx.tmp_i32);
                self.compile_expr_as_i64(ctx, builder, &args[1].value)?;
                builder.local_set(ctx.tmp_i64);
                builder.local_get(ctx.tmp_i32);
                builder.local_get(ctx.tmp_i64);
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::I64 { atomic: false },
                    walrus::ir::MemArg {
                        align: 8,
                        offset: 0,
                    },
                );
                builder.local_get(ctx.tmp_i64);
                Ok(true)
            }
            "__kain_alloc" if args.len() >= 4 => {
                builder.global_get(ctx.heap_ptr_global);
                builder.local_set(ctx.tmp_i32);
                self.compile_expr_as_i32(ctx, builder, &args[0].value)?;
                builder.local_set(ctx.tmp_i32_2);
                builder.global_get(ctx.heap_ptr_global);
                builder.local_get(ctx.tmp_i32_2);
                builder.binop(walrus::ir::BinaryOp::I32Add);
                builder.global_set(ctx.heap_ptr_global);
                builder.local_get(ctx.tmp_i32);
                Ok(true)
            }
            "__kain_realloc" if args.len() >= 4 => {
                self.compile_expr_as_i32(ctx, builder, &args[0].value)?;
                Ok(true)
            }
            "__kain_union_wrap" if args.len() >= 6 => {
                let type_key = self.expr_string_literal(&args[2].value).unwrap_or("Int");
                let byte_size = self.expr_int_literal(&args[3].value).unwrap_or(8);
                self.compile_expr(ctx, builder, &args[0].value)?;
                builder.local_set(ctx.tmp_i32);
                builder.local_get(ctx.tmp_i32);
                self.compile_expr(ctx, builder, &args[5].value)?;
                self.emit_wasm_store_for_type_key(ctx, builder, type_key, byte_size);
                builder.local_get(ctx.tmp_i32);
                Ok(true)
            }
            "__kain_union_get" if args.len() >= 6 => {
                let type_key = self.expr_string_literal(&args[2].value).unwrap_or("Int");
                let byte_size = self.expr_int_literal(&args[3].value).unwrap_or(8);
                self.compile_expr(ctx, builder, &args[0].value)?;
                self.emit_wasm_load_for_type_key(ctx, builder, type_key, byte_size);
                Ok(true)
            }
            "__kain_union_set" if args.len() >= 6 => {
                let type_key = self.expr_string_literal(&args[2].value).unwrap_or("Int");
                let byte_size = self.expr_int_literal(&args[3].value).unwrap_or(8);
                self.compile_expr(ctx, builder, &args[0].value)?;
                builder.local_set(ctx.tmp_i32);
                builder.local_get(ctx.tmp_i32);
                self.compile_expr(ctx, builder, &args[5].value)?;
                self.emit_wasm_store_for_type_key(ctx, builder, type_key, byte_size);
                builder.local_get(ctx.tmp_i32);
                self.emit_wasm_load_for_type_key(ctx, builder, type_key, byte_size);
                Ok(true)
            }
            "__kain_bitfield_get" if args.len() >= 7 => {
                let unit_offset = self.expr_int_literal(&args[2].value).unwrap_or(0) as i32;
                let bit_offset = self.expr_int_literal(&args[3].value).unwrap_or(0);
                let width = self.expr_int_literal(&args[4].value).unwrap_or(0);
                let is_signed = self.expr_bool_literal(&args[5].value).unwrap_or(false);
                let mask = if width <= 0 {
                    0
                } else if width >= 63 {
                    i64::MAX
                } else {
                    (1i64 << width) - 1i64
                };
                let sign_bit = if width > 0 && width < 63 {
                    1i64 << (width - 1)
                } else {
                    0
                };
                self.compile_expr(ctx, builder, &args[0].value)?;
                if unit_offset != 0 {
                    builder.i32_const(unit_offset);
                    builder.binop(walrus::ir::BinaryOp::I32Add);
                }
                builder.load(
                    ctx.memory_id,
                    walrus::ir::LoadKind::I64 { atomic: false },
                    walrus::ir::MemArg {
                        align: 8,
                        offset: 0,
                    },
                );
                if bit_offset > 0 {
                    builder.i64_const(bit_offset);
                    builder.binop(walrus::ir::BinaryOp::I64ShrU);
                }
                builder.i64_const(mask);
                builder.binop(walrus::ir::BinaryOp::I64And);
                if is_signed && width > 0 && width < 63 {
                    builder.local_set(ctx.tmp_i64);
                    builder.local_get(ctx.tmp_i64);
                    builder.i64_const(sign_bit);
                    builder.binop(walrus::ir::BinaryOp::I64Xor);
                    builder.i64_const(sign_bit);
                    builder.binop(walrus::ir::BinaryOp::I64Sub);
                }
                Ok(true)
            }
            "__kain_bitfield_set" if args.len() >= 8 => {
                let unit_offset = self.expr_int_literal(&args[2].value).unwrap_or(0) as i32;
                let bit_offset = self.expr_int_literal(&args[3].value).unwrap_or(0);
                let width = self.expr_int_literal(&args[4].value).unwrap_or(0);
                let is_signed = self.expr_bool_literal(&args[5].value).unwrap_or(false);
                let mask = if width <= 0 {
                    0
                } else if width >= 63 {
                    i64::MAX
                } else {
                    (1i64 << width) - 1i64
                };
                let shifted_mask = if bit_offset > 0 {
                    mask.checked_shl(bit_offset as u32).unwrap_or(0)
                } else {
                    mask
                };
                self.compile_expr(ctx, builder, &args[0].value)?;
                if unit_offset != 0 {
                    builder.i32_const(unit_offset);
                    builder.binop(walrus::ir::BinaryOp::I32Add);
                }
                builder.local_set(ctx.tmp_i32);
                builder.local_get(ctx.tmp_i32);
                builder.load(
                    ctx.memory_id,
                    walrus::ir::LoadKind::I64 { atomic: false },
                    walrus::ir::MemArg {
                        align: 8,
                        offset: 0,
                    },
                );
                builder.local_set(ctx.tmp_i64);
                builder.local_get(ctx.tmp_i64);
                builder.i64_const(!shifted_mask);
                builder.binop(walrus::ir::BinaryOp::I64And);
                self.compile_expr(ctx, builder, &args[7].value)?;
                builder.i64_const(mask);
                builder.binop(walrus::ir::BinaryOp::I64And);
                if bit_offset > 0 {
                    builder.i64_const(bit_offset);
                    builder.binop(walrus::ir::BinaryOp::I64Shl);
                }
                builder.binop(walrus::ir::BinaryOp::I64Or);
                builder.local_set(ctx.tmp_i64);
                builder.local_get(ctx.tmp_i32);
                builder.local_get(ctx.tmp_i64);
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::I64 { atomic: false },
                    walrus::ir::MemArg {
                        align: 8,
                        offset: 0,
                    },
                );
                // Return normalized value via the same get semantics.
                builder.local_get(ctx.tmp_i32);
                builder.load(
                    ctx.memory_id,
                    walrus::ir::LoadKind::I64 { atomic: false },
                    walrus::ir::MemArg {
                        align: 8,
                        offset: 0,
                    },
                );
                if bit_offset > 0 {
                    builder.i64_const(bit_offset);
                    builder.binop(walrus::ir::BinaryOp::I64ShrU);
                }
                builder.i64_const(mask);
                builder.binop(walrus::ir::BinaryOp::I64And);
                if is_signed && width > 0 && width < 63 {
                    let sign_bit = 1i64 << (width - 1);
                    builder.local_set(ctx.tmp_i64);
                    builder.local_get(ctx.tmp_i64);
                    builder.i64_const(sign_bit);
                    builder.binop(walrus::ir::BinaryOp::I64Xor);
                    builder.i64_const(sign_bit);
                    builder.binop(walrus::ir::BinaryOp::I64Sub);
                }
                Ok(true)
            }
            _ => Ok(false),
        }
    }

    fn compile_expr(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        expr: &Expr,
    ) -> KainResult<()> {
        match expr {
            Expr::Int(n, _) => {
                builder.i64_const(*n);
            }
            Expr::None(_) => {
                builder.i32_const(0);
            }
            Expr::Float(f, _) => {
                builder.f64_const(*f);
            }
            Expr::Bool(b, _) => {
                builder.i32_const(if *b { 1 } else { 0 });
            }
            Expr::String(s, span) => {
                // String literals are stored in data segment during pre-pass
                // Here we just emit the memory offset as an i32
                if let Some(&offset) = ctx.string_table.get(s) {
                    builder.i32_const((offset + 4) as i32); // Return pointer to data (skip length)
                } else {
                    return Err(KainError::codegen("String not found in table", *span));
                }
            }
            Expr::Paren(inner, _) => {
                self.compile_expr(ctx, builder, inner)?;
            }
            Expr::Cast { value, target, .. } => {
                let source_ty = self.infer_expr_wasm_type_in_context(ctx, value);
                self.compile_expr(ctx, builder, value)?;
                match target {
                    kain_core::ast::Type::Named { name, .. }
                        if matches!(name.as_str(), "Bool" | "bool") =>
                    {
                        match source_ty {
                            ValType::I64 => {
                                builder.i64_const(0);
                                builder.binop(walrus::ir::BinaryOp::I64Ne);
                            }
                            ValType::F64 => {
                                builder.f64_const(0.0);
                                builder.binop(walrus::ir::BinaryOp::F64Ne);
                            }
                            _ => {}
                        }
                    }
                    _ => {
                        self.coerce_stack_to_val_type(builder, source_ty, self.map_authored_type(target));
                    }
                }
            }
            Expr::Bitcast { value, .. }
            | Expr::Comptime(value, _)
            | Expr::Await(value, _)
            | Expr::AsyncBlock(value, _)
            | Expr::Try(value, _) => {
                self.compile_expr(ctx, builder, value)?;
            }
            Expr::Observe { target, body, .. } | Expr::Collapse { target, body, .. } => {
                self.compile_expr(ctx, builder, target)?;
                builder.drop();
                self.compile_expr(ctx, builder, body)?;
            }
            Expr::Teleport { value, .. } => {
                self.compile_expr(ctx, builder, value)?;
            }
            Expr::Ref { value, .. } | Expr::AddrOf { value, .. } => {
                self.compile_expr(ctx, builder, value)?;
            }
            Expr::Deref(value, _) => {
                self.compile_expr_as_i32(ctx, builder, value)?;
                builder.load(
                    ctx.memory_id,
                    walrus::ir::LoadKind::I64 { atomic: false },
                    walrus::ir::MemArg {
                        align: 8,
                        offset: 0,
                    },
                );
            }
            Expr::Binary {
                left, op, right, ..
            } => {
                let string_binary = self.is_string_expr(left) || self.is_string_expr(right);
                if string_binary && matches!(op, BinaryOp::Eq | BinaryOp::Ne) {
                    self.compile_expr_as_i32(ctx, builder, left)?;
                    self.compile_expr_as_i32(ctx, builder, right)?;
                    if let Some(func_id) = ctx.functions.get("str_eq") {
                        builder.call(*func_id);
                    } else {
                        builder.i32_const(0);
                    }
                    if matches!(op, BinaryOp::Ne) {
                        builder.unop(walrus::ir::UnaryOp::I32Eqz);
                    }
                    return Ok(());
                }

                let left_ty = self.infer_expr_wasm_type_in_context(ctx, left);
                let right_ty = self.infer_expr_wasm_type_in_context(ctx, right);
                let uses_i32 = left_ty == ValType::I32 && right_ty == ValType::I32;
                let uses_f64 = left_ty == ValType::F64 || right_ty == ValType::F64;
                let operand_ty = if uses_f64 {
                    ValType::F64
                } else if uses_i32 {
                    ValType::I32
                } else {
                    ValType::I64
                };

                self.compile_expr(ctx, builder, left)?;
                self.coerce_stack_to_val_type(builder, left_ty, operand_ty);
                self.compile_expr(ctx, builder, right)?;
                self.coerce_stack_to_val_type(builder, right_ty, operand_ty);
                match op {
                    // Arithmetic
                    BinaryOp::Add => {
                        if self.is_string_expr(left) || self.is_string_expr(right) {
                            if let Some(func_id) = ctx.functions.get("str_concat") {
                                builder.call(*func_id);
                            }
                        } else if uses_f64 {
                            builder.binop(walrus::ir::BinaryOp::F64Add);
                        } else if uses_i32 {
                            builder.binop(walrus::ir::BinaryOp::I32Add);
                        } else {
                            builder.binop(walrus::ir::BinaryOp::I64Add);
                        }
                    }
                    BinaryOp::Sub => {
                        if uses_f64 {
                            builder.binop(walrus::ir::BinaryOp::F64Sub);
                        } else if uses_i32 {
                            builder.binop(walrus::ir::BinaryOp::I32Sub);
                        } else {
                            builder.binop(walrus::ir::BinaryOp::I64Sub);
                        }
                    }
                    BinaryOp::Mul => {
                        if uses_f64 {
                            builder.binop(walrus::ir::BinaryOp::F64Mul);
                        } else if uses_i32 {
                            builder.binop(walrus::ir::BinaryOp::I32Mul);
                        } else {
                            builder.binop(walrus::ir::BinaryOp::I64Mul);
                        }
                    }
                    BinaryOp::Div => {
                        if uses_f64 {
                            builder.binop(walrus::ir::BinaryOp::F64Div);
                        } else if uses_i32 {
                            builder.binop(walrus::ir::BinaryOp::I32DivS);
                        } else {
                            builder.binop(walrus::ir::BinaryOp::I64DivS);
                        }
                    }
                    BinaryOp::Mod => {
                        if uses_f64 {
                            if let Some(func_id) = ctx.functions.get("fmod") {
                                builder.call(*func_id);
                            } else {
                                return Err(KainError::codegen(
                                    "WASM host import 'fmod' not found",
                                    expr.span(),
                                ));
                            }
                        } else if uses_i32 {
                            builder.binop(walrus::ir::BinaryOp::I32RemS);
                        } else {
                            builder.binop(walrus::ir::BinaryOp::I64RemS);
                        }
                    }
                    // Comparison
                    BinaryOp::Eq => {
                        if uses_f64 {
                            builder.binop(walrus::ir::BinaryOp::F64Eq);
                        } else if uses_i32 {
                            builder.binop(walrus::ir::BinaryOp::I32Eq);
                        } else {
                            builder.binop(walrus::ir::BinaryOp::I64Eq);
                        }
                    }
                    BinaryOp::Ne => {
                        if uses_f64 {
                            builder.binop(walrus::ir::BinaryOp::F64Ne);
                        } else if uses_i32 {
                            builder.binop(walrus::ir::BinaryOp::I32Ne);
                        } else {
                            builder.binop(walrus::ir::BinaryOp::I64Ne);
                        }
                    }
                    BinaryOp::Lt => {
                        if uses_f64 {
                            builder.binop(walrus::ir::BinaryOp::F64Lt);
                        } else if uses_i32 {
                            builder.binop(walrus::ir::BinaryOp::I32LtS);
                        } else {
                            builder.binop(walrus::ir::BinaryOp::I64LtS);
                        }
                    }
                    BinaryOp::Gt => {
                        if uses_f64 {
                            builder.binop(walrus::ir::BinaryOp::F64Gt);
                        } else if uses_i32 {
                            builder.binop(walrus::ir::BinaryOp::I32GtS);
                        } else {
                            builder.binop(walrus::ir::BinaryOp::I64GtS);
                        }
                    }
                    BinaryOp::Le => {
                        if uses_f64 {
                            builder.binop(walrus::ir::BinaryOp::F64Le);
                        } else if uses_i32 {
                            builder.binop(walrus::ir::BinaryOp::I32LeS);
                        } else {
                            builder.binop(walrus::ir::BinaryOp::I64LeS);
                        }
                    }
                    BinaryOp::Ge => {
                        if uses_f64 {
                            builder.binop(walrus::ir::BinaryOp::F64Ge);
                        } else if uses_i32 {
                            builder.binop(walrus::ir::BinaryOp::I32GeS);
                        } else {
                            builder.binop(walrus::ir::BinaryOp::I64GeS);
                        }
                    }
                    // Logical (short-circuit would need control flow, treat as bitwise for now)
                    BinaryOp::And => {
                        builder.binop(walrus::ir::BinaryOp::I32And);
                    }
                    BinaryOp::Or => {
                        builder.binop(walrus::ir::BinaryOp::I32Or);
                    }
                    // Bitwise
                    BinaryOp::BitAnd => {
                        if uses_i32 {
                            builder.binop(walrus::ir::BinaryOp::I32And);
                        } else {
                            builder.binop(walrus::ir::BinaryOp::I64And);
                        }
                    }
                    BinaryOp::BitOr => {
                        if uses_i32 {
                            builder.binop(walrus::ir::BinaryOp::I32Or);
                        } else {
                            builder.binop(walrus::ir::BinaryOp::I64Or);
                        }
                    }
                    BinaryOp::BitXor => {
                        if uses_i32 {
                            builder.binop(walrus::ir::BinaryOp::I32Xor);
                        } else {
                            builder.binop(walrus::ir::BinaryOp::I64Xor);
                        }
                    }
                    BinaryOp::Shl => {
                        if uses_i32 {
                            builder.binop(walrus::ir::BinaryOp::I32Shl);
                        } else {
                            builder.binop(walrus::ir::BinaryOp::I64Shl);
                        }
                    }
                    BinaryOp::Shr => {
                        if uses_i32 {
                            builder.binop(walrus::ir::BinaryOp::I32ShrS);
                        } else {
                            builder.binop(walrus::ir::BinaryOp::I64ShrS);
                        }
                    }
                    _ => {}
                }
            }
            Expr::Unary { op, operand, .. } => {
                use kain_core::ast::UnaryOp;
                let operand_ty = self.infer_expr_wasm_type_in_context(ctx, operand);
                match op {
                    UnaryOp::Neg => {
                        match operand_ty {
                            ValType::F64 => {
                                builder.f64_const(0.0);
                                self.compile_expr(ctx, builder, operand)?;
                                self.coerce_stack_to_val_type(builder, operand_ty, ValType::F64);
                                builder.binop(walrus::ir::BinaryOp::F64Sub);
                            }
                            ValType::I32 => {
                                builder.i32_const(0);
                                self.compile_expr(ctx, builder, operand)?;
                                self.coerce_stack_to_val_type(builder, operand_ty, ValType::I32);
                                builder.binop(walrus::ir::BinaryOp::I32Sub);
                            }
                            _ => {
                                builder.i64_const(0);
                                self.compile_expr(ctx, builder, operand)?;
                                self.coerce_stack_to_val_type(builder, operand_ty, ValType::I64);
                                builder.binop(walrus::ir::BinaryOp::I64Sub);
                            }
                        }
                    }
                    UnaryOp::Not => {
                        // !x = x == 0 (logical not)
                        self.compile_expr(ctx, builder, operand)?;
                        if operand_ty == ValType::I32 {
                            builder.unop(walrus::ir::UnaryOp::I32Eqz);
                        } else {
                            builder.unop(walrus::ir::UnaryOp::I64Eqz);
                        }
                    }
                    UnaryOp::BitNot => {
                        // ~x = x xor -1
                        self.compile_expr(ctx, builder, operand)?;
                        if operand_ty == ValType::I32 {
                            builder.i32_const(-1);
                            builder.binop(walrus::ir::BinaryOp::I32Xor);
                        } else {
                            builder.i64_const(-1);
                            builder.binop(walrus::ir::BinaryOp::I64Xor);
                        }
                    }
                    _ => {
                        // Ref, Deref - just compile operand for now
                        self.compile_expr(ctx, builder, operand)?;
                    }
                }
            }
            Expr::Ident(name, span) => {
                if name == "None" {
                    builder.i32_const(0);
                } else if let Some(local_id) = ctx.locals.get(name) {
                    builder.local_get(*local_id);
                } else if let Some(offset) = ctx.world_globals.get(name) {
                    builder.i32_const(*offset as i32);
                } else if let Some(value) = ctx.constants.get(name).copied() {
                    self.emit_const_value(builder, value);
                } else if let Some(value) = wasm_c_runtime_constant(name) {
                    builder.i64_const(value);
                } else {
                    return Err(KainError::codegen(
                        format!("Variable '{}' not found in locals", name),
                        *span,
                    ));
                }
            }
            Expr::Assign {
                target,
                value,
                span,
            } => match target.as_ref() {
                Expr::Ident(name, _) => {
                    let Some(local_id) = ctx.locals.get(name).copied() else {
                        return Err(KainError::codegen(
                            format!("Assignment target '{}' not found in locals", name),
                            *span,
                        ));
                    };
                    let local_ty = self.module.locals.get(local_id).ty();
                    self.compile_expr(ctx, builder, value)?;
                    match local_ty {
                        ValType::I32 if !self.expr_is_i32_in_context(ctx, value) => {
                            builder.unop(walrus::ir::UnaryOp::I32WrapI64);
                        }
                        ValType::I64 if self.expr_is_i32_in_context(ctx, value) => {
                            builder.unop(walrus::ir::UnaryOp::I64ExtendSI32);
                        }
                        _ => {}
                    }
                    builder.local_set(local_id);
                    builder.local_get(local_id);
                }
                Expr::Field {
                    object,
                    field,
                    span: field_span,
                } => {
                    let temp_local = self.temp_local_for_expr(ctx, value);
                    let temp_ty = self.module.locals.get(temp_local).ty();
                    self.compile_expr(ctx, builder, value)?;
                    builder.local_set(temp_local);
                    self.compile_field_address(ctx, builder, object, field, *field_span)?;
                    builder.local_get(temp_local);
                    self.emit_store_for_val_type(ctx, builder, temp_ty, 0);
                    builder.local_get(temp_local);
                }
                _ => {
                    return Err(KainError::codegen(
                        "Only local and field assignment targets are supported in WASM codegen",
                        *span,
                    ));
                }
            },
            Expr::Spawn { actor, init, span } => {
                let typed_actor = self.actors.get(actor).ok_or_else(|| {
                    KainError::codegen(format!("Unknown actor '{}'", actor), *span)
                })?;
                let (field_offsets, total_size) =
                    ctx.struct_layouts.get(actor).cloned().ok_or_else(|| {
                        KainError::codegen(format!("Actor '{}' layout not found", actor), *span)
                    })?;

                self.emit_alloc(ctx, builder, total_size);
                builder.local_set(ctx.tmp_i32);

                let provided: HashMap<&str, &Expr> = init
                    .iter()
                    .map(|(field_name, value)| (field_name.as_str(), value))
                    .collect();

                for state in &typed_actor.ast.state {
                    let Some(&field_offset) = field_offsets.get(&state.name) else {
                        continue;
                    };
                    let value_expr = provided
                        .get(state.name.as_str())
                        .copied()
                        .unwrap_or(&state.initial);
                    let value_ty = typed_actor
                        .state_types
                        .get(&state.name)
                        .map(|ty| self.map_heap_value_type_from_resolved_type(ty))
                        .unwrap_or_else(|| self.map_heap_value_type_from_authored_type(&state.ty));

                    builder.local_get(ctx.tmp_i32);
                    if field_offset != 0 {
                        builder.i32_const(field_offset as i32);
                        builder.binop(walrus::ir::BinaryOp::I32Add);
                    }
                    self.compile_expr(ctx, builder, value_expr)?;
                    self.coerce_expr_stack_to_val_type(ctx, builder, value_expr, value_ty);
                    self.emit_store_for_val_type(ctx, builder, value_ty, 0);
                }

                builder.local_get(ctx.tmp_i32);
            }
            Expr::SendMsg {
                target,
                message,
                data,
                span,
            } => {
                if self.compile_direct_actor_send(ctx, builder, target, message, data, *span)? {
                    return Ok(());
                }
                return Err(KainError::codegen(
                    format!("Unsupported actor send in WASM codegen: {}", message),
                    *span,
                ));
            }
            Expr::If {
                condition,
                then_branch,
                else_branch,
                ..
            } => {
                self.compile_expr(ctx, builder, condition)?;

                let branch_error = std::cell::RefCell::new(None);
                builder.if_else(
                    None,
                    |then_builder| {
                        if let Err(err) = self.compile_block(ctx, then_builder, then_branch) {
                            *branch_error.borrow_mut() = Some(err);
                        }
                    },
                    |else_builder| {
                        if let Some(else_br) = else_branch {
                            if let Err(err) = self.compile_else_branch(ctx, else_builder, else_br) {
                                *branch_error.borrow_mut() = Some(err);
                            }
                        }
                    },
                );
                if let Some(err) = branch_error.into_inner() {
                    return Err(err);
                }
            }
            Expr::JSX(node, _) => {
                self.compile_jsx_node(ctx, builder, node)?;
            }
            Expr::Call { callee, args, span } => {
                // Get function name from callee
                if let Expr::Ident(func_name, _) = callee.as_ref() {
                    // Built-in len(value): arrays store their element count at base pointer.
                    // String literals point past their length prefix, so adjust those back.
                    if func_name == "len" && args.len() == 1 {
                        let arg = &args[0].value;
                        if self.is_string_expr(arg) {
                            self.compile_expr_as_i32(ctx, builder, arg)?;
                            builder.i32_const(4);
                            builder.binop(walrus::ir::BinaryOp::I32Sub);
                        } else {
                            self.compile_expr_as_i32(ctx, builder, arg)?;
                        }
                        builder.load(
                            ctx.memory_id,
                            walrus::ir::LoadKind::I32 { atomic: false },
                            walrus::ir::MemArg {
                                align: 4,
                                offset: 0,
                            },
                        );
                        builder.unop(walrus::ir::UnaryOp::I64ExtendUI32);
                        return Ok(());
                    }

                    if self.compile_low_level_helper_call(ctx, builder, func_name, args)? {
                        return Ok(());
                    }
                    if self.compile_c_runtime_call(ctx, builder, func_name, args)? {
                        return Ok(());
                    }
                    if self.compile_builtin_math_call(ctx, builder, func_name, args)? {
                        return Ok(());
                    }
                    if self.compile_primitive_cast_call(ctx, builder, func_name, args)? {
                        return Ok(());
                    }
                    // Special intrinsic: print
                    if func_name == "print" {
                        self.compile_print_like_args(ctx, builder, args)?;
                        builder.i64_const(0); // Return Unit/0
                        return Ok(());
                    }

                    // Special intrinsic: to_string
                    if func_name == "to_string" {
                        if let Some(arg) = args.first() {
                            self.compile_expr(ctx, builder, &arg.value)?;
                            if let Some(func_id) = ctx.functions.get("int_to_str") {
                                builder.call(*func_id);
                            }
                        } else {
                            builder.i32_const(0);
                        }
                        return Ok(());
                    }

                    // Special intrinsic: now
                    if func_name == "now" {
                        if let Some(func_id) = ctx.functions.get("time_now") {
                            builder.call(*func_id);
                        }
                        return Ok(());
                    }

                    if func_name == "ask" || func_name == "ask_timeout" {
                        self.compile_actor_ask_call(ctx, builder, args, *span)?;
                        return Ok(());
                    }

                    if self.compile_builtin_tagged_constructor_call(
                        ctx, builder, func_name, args, *span,
                    )? {
                        return Ok(());
                    }

                    if self
                        .compile_variant_constructor_call(ctx, builder, func_name, args, *span)?
                    {
                        return Ok(());
                    }

                    // Look up function ID
                    if let Some(func_id) = ctx.functions.get(func_name) {
                        self.compile_call_args_for_function(ctx, builder, *func_id, args)?;
                        builder.call(*func_id);
                    } else {
                        return Err(KainError::codegen(
                            format!("Function '{}' not found", func_name),
                            *span,
                        ));
                    }
                } else {
                    // For now, only support direct function calls by name
                    return Err(KainError::codegen(
                        "Only direct function calls supported in WASM",
                        *span,
                    ));
                }
            }
            Expr::StageCall {
                function,
                args,
                span,
                ..
            } => {
                if let Some(func_id) = ctx.functions.get(function) {
                    self.compile_call_args_for_function(ctx, builder, *func_id, args)?;
                    builder.call(*func_id);
                } else {
                    return Err(KainError::codegen(
                        format!("Stage function '{}' not found", function),
                        *span,
                    ));
                }
            }
            // Struct literal: allocate memory and initialize fields
            Expr::EnumVariant {
                enum_name,
                variant,
                fields,
                span,
            } => {
                if enum_name == "Option" || enum_name == "Result" {
                    match fields {
                        kain_core::ast::EnumVariantFields::Unit if variant == "None" => {
                            builder.i32_const(0);
                            return Ok(());
                        }
                        kain_core::ast::EnumVariantFields::Tuple(exprs)
                            if exprs.len() == 1
                                && matches!(variant.as_str(), "Some" | "Ok" | "Err") =>
                        {
                            let tag = if variant == "Err" { 1 } else { 0 };
                            self.compile_builtin_tagged_value(ctx, builder, tag, &exprs[0])?;
                            return Ok(());
                        }
                        _ => {
                            return Err(KainError::codegen(
                                format!(
                                    "Builtin {}::{} shape is not yet supported in WASM codegen",
                                    enum_name, variant
                                ),
                                *span,
                            ));
                        }
                    }
                }

                if let Some((tags, max_payload, field_offsets_map)) =
                    ctx.enum_layouts.get(enum_name)
                {
                    let tag = *tags
                        .get(variant)
                        .ok_or_else(|| KainError::codegen("Variant tag not found", *span))?;

                    // 4 bytes tag + payload
                    let total_size = 4 + max_payload;
                    self.emit_alloc(ctx, builder, total_size);
                    // Stack: [base_ptr]

                    // Drop base_ptr to recompute for stores
                    builder.drop();

                    // Store tag at offset 0
                    let aligned_size = (total_size + 7) & !7;

                    builder.global_get(ctx.heap_ptr_global);
                    builder.i32_const(aligned_size as i32);
                    builder.binop(walrus::ir::BinaryOp::I32Sub);

                    builder.i32_const(tag as i32);
                    builder.store(
                        ctx.memory_id,
                        walrus::ir::StoreKind::I32 { atomic: false },
                        walrus::ir::MemArg {
                            align: 4,
                            offset: 0,
                        },
                    );

                    match fields {
                        kain_core::ast::EnumVariantFields::Unit => {}
                        kain_core::ast::EnumVariantFields::Tuple(exprs) => {
                            let variant_offsets = field_offsets_map
                                .get(variant)
                                .expect("Variant offsets missing");
                            for (i, expr) in exprs.iter().enumerate() {
                                if let Some(&offset) = variant_offsets.get(&i.to_string()) {
                                    builder.global_get(ctx.heap_ptr_global);
                                    builder.i32_const(aligned_size as i32);
                                    builder.binop(walrus::ir::BinaryOp::I32Sub);
                                    builder.i32_const((4 + offset) as i32);
                                    builder.binop(walrus::ir::BinaryOp::I32Add);

                                    self.compile_expr(ctx, builder, expr)?;
                                    self.emit_store_for_expr(ctx, builder, expr, 0);
                                }
                            }
                        }
                        kain_core::ast::EnumVariantFields::Struct(named_fields) => {
                            let variant_offsets = field_offsets_map
                                .get(variant)
                                .expect("Variant offsets missing");
                            for (name, expr) in named_fields {
                                if let Some(&offset) = variant_offsets.get(name) {
                                    builder.global_get(ctx.heap_ptr_global);
                                    builder.i32_const(aligned_size as i32);
                                    builder.binop(walrus::ir::BinaryOp::I32Sub);
                                    builder.i32_const((4 + offset) as i32);
                                    builder.binop(walrus::ir::BinaryOp::I32Add);

                                    self.compile_expr(ctx, builder, expr)?;
                                    self.emit_store_for_expr(ctx, builder, expr, 0);
                                }
                            }
                        }
                    }

                    // Return base pointer
                    builder.global_get(ctx.heap_ptr_global);
                    builder.i32_const(aligned_size as i32);
                    builder.binop(walrus::ir::BinaryOp::I32Sub);
                } else {
                    return Err(KainError::codegen(
                        format!("Enum layout not found for {}", enum_name),
                        *span,
                    ));
                }
            }
            Expr::Struct {
                name,
                fields,
                rest,
                span,
            } => {
                if rest.is_some() {
                    return Err(KainError::codegen(
                        "Struct update syntax is not yet supported by WASM codegen",
                        *span,
                    ));
                }
                if let Some((field_offsets, total_size)) = ctx.struct_layouts.get(name).cloned() {
                    // Allocate memory for struct using bump allocator
                    self.emit_alloc(ctx, builder, total_size);
                    // Stack: [base_ptr]

                    // We need to keep base_ptr for field stores AND return it
                    // Strategy: for each field, dup the ptr, add offset, store
                    // But walrus doesn't have dup... so we emit base_ptr before each store

                    // Store fields: emit [addr, value] then store
                    for (field_name, field_expr) in fields {
                        if let Some(&field_offset) = field_offsets.get(field_name) {
                            // Emit base_ptr + offset for store address
                            builder.global_get(ctx.heap_ptr_global);
                            // Need to subtract total_size to get back to our base
                            // Actually, heap_ptr now points PAST our allocation
                            // Our base = heap_ptr - aligned_size
                            // Simpler: re-emit the base calculation

                            // Get the base we just allocated (heap_ptr - aligned_total_size)
                            let aligned_size = (total_size + 7) & !7;
                            builder.i32_const(aligned_size as i32);
                            builder.binop(walrus::ir::BinaryOp::I32Sub);
                            builder.i32_const(field_offset as i32);
                            builder.binop(walrus::ir::BinaryOp::I32Add);
                            // Stack: [field_addr]

                            // Compile the field value
                            self.compile_expr(ctx, builder, field_expr)?;
                            // Stack: [field_addr, value]

                            self.emit_store_for_expr(ctx, builder, field_expr, 0);
                        }
                    }

                    // Leave struct pointer on stack (base address)
                    let aligned_size = (total_size + 7) & !7;
                    builder.global_get(ctx.heap_ptr_global);
                    builder.i32_const(aligned_size as i32);
                    builder.binop(walrus::ir::BinaryOp::I32Sub);
                } else {
                    return Err(KainError::codegen(
                        format!("Struct '{}' layout not found", name),
                        *span,
                    ));
                }
            }
            // Field access: load from struct pointer + offset
            Expr::Field {
                object,
                field,
                span,
            } => {
                let field_type = self
                    .infer_field_val_type_in_context(ctx, object, field)
                    .unwrap_or(ValType::I64);
                self.compile_field_address(ctx, builder, object, field, *span)?;
                self.emit_load_for_val_type(ctx, builder, field_type, 0);
            }
            // Method call: obj.method(args) desugars to Type.method(obj, args)
            Expr::MethodCall {
                receiver,
                method,
                args,
                span,
            } => {
                if self.compile_builtin_tagged_method_call(
                    ctx, builder, receiver, method, args, *span,
                )? {
                    return Ok(());
                }

                // Compile the receiver (self)
                self.compile_expr(ctx, builder, receiver)?;

                // Compile arguments
                for arg in args {
                    self.compile_expr(ctx, builder, &arg.value)?;
                }

                let resolved_method = ctx.functions.get(method).copied().or_else(|| {
                    let suffix = format!(".{method}");
                    let mut matches = ctx
                        .functions
                        .iter()
                        .filter_map(|(name, func_id)| name.ends_with(&suffix).then_some(*func_id));
                    let first = matches.next()?;
                    if matches.next().is_some() {
                        None
                    } else {
                        Some(first)
                    }
                });

                if let Some(func_id) = resolved_method {
                    builder.call(func_id);
                } else {
                    return Err(KainError::codegen(
                        format!("Method '{}' not found", method),
                        *span,
                    ));
                }
            }
            // Array literal: allocate memory and store length + elements
            Expr::Array(elements, _span) => {
                let len = elements.len() as u32;
                let element_size = 8u32; // i64 elements
                let total_size = 4 + (len * element_size); // 4 bytes for length + elements
                let aligned_size = (total_size + 7) & !7;

                // Allocate using bump allocator
                self.emit_alloc(ctx, builder, total_size);
                // Stack: [base_ptr] - but emit_alloc leaves OLD ptr, heap_ptr is now past us
                // Actually emit_alloc returns old heap_ptr which IS our base. Perfect!

                // Drop the base_ptr from stack for now, we'll recompute for stores
                builder.drop();

                // Compute base address: heap_ptr - aligned_size
                let get_base = |b: &mut InstrSeqBuilder, hp: walrus::GlobalId, sz: u32| {
                    b.global_get(hp);
                    b.i32_const(sz as i32);
                    b.binop(walrus::ir::BinaryOp::I32Sub);
                };

                // Store length at base
                get_base(builder, ctx.heap_ptr_global, aligned_size);
                builder.i32_const(len as i32);
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::I32 { atomic: false },
                    walrus::ir::MemArg {
                        align: 4,
                        offset: 0,
                    },
                );

                // Store each element
                for (i, elem) in elements.iter().enumerate() {
                    // Address = base + 4 + (i * 8)
                    get_base(builder, ctx.heap_ptr_global, aligned_size);
                    builder.i32_const((4 + i as u32 * element_size) as i32);
                    builder.binop(walrus::ir::BinaryOp::I32Add);

                    self.compile_expr(ctx, builder, elem)?;
                    self.emit_store_for_expr(ctx, builder, elem, 0);
                }

                // Leave array pointer on stack
                get_base(builder, ctx.heap_ptr_global, aligned_size);
            }
            // Index access: arr[i] - load from array pointer + 4 + (i * 8)
            Expr::Index {
                object,
                index,
                span: _,
            } => {
                // Compile array pointer
                self.compile_expr(ctx, builder, object)?;
                // Save to compute address: base + 4 + (index * 8)
                // Stack: [base_ptr]

                builder.i32_const(4); // Skip length field
                builder.binop(walrus::ir::BinaryOp::I32Add);
                // Stack: [base_ptr + 4]

                // Compile index
                self.compile_expr(ctx, builder, index)?;
                // Convert i64 index to i32 for address calculation
                builder.unop(walrus::ir::UnaryOp::I32WrapI64);
                builder.i32_const(8); // element size
                builder.binop(walrus::ir::BinaryOp::I32Mul);
                // Stack: [base_ptr + 4, index * 8]

                builder.binop(walrus::ir::BinaryOp::I32Add);
                // Stack: [base_ptr + 4 + index * 8]

                // Load i64 element
                builder.load(
                    ctx.memory_id,
                    walrus::ir::LoadKind::I64 { atomic: false },
                    walrus::ir::MemArg {
                        align: 8,
                        offset: 0,
                    },
                );
            }
            // Tuple literal: allocate memory and store elements (like struct with indexed fields)
            Expr::Tuple(elements, _span) => {
                let len = elements.len() as u32;
                let element_size = 8u32; // All elements i64 for now
                let total_size = len * element_size;
                let aligned_size = (total_size + 7) & !7;

                // Allocate
                self.emit_alloc(ctx, builder, total_size);
                builder.drop(); // We'll recompute base for each store

                // Store each element
                for (i, elem) in elements.iter().enumerate() {
                    // Address = heap_ptr - aligned_size + (i * 8)
                    builder.global_get(ctx.heap_ptr_global);
                    builder.i32_const(aligned_size as i32);
                    builder.binop(walrus::ir::BinaryOp::I32Sub);
                    builder.i32_const((i as u32 * element_size) as i32);
                    builder.binop(walrus::ir::BinaryOp::I32Add);

                    self.compile_expr(ctx, builder, elem)?;
                    self.emit_store_for_expr(ctx, builder, elem, 0);
                }

                // Leave tuple pointer on stack
                builder.global_get(ctx.heap_ptr_global);
                builder.i32_const(aligned_size as i32);
                builder.binop(walrus::ir::BinaryOp::I32Sub);
            }
            // Match expression: compile as chained if-else
            Expr::Match {
                scrutinee,
                arms,
                span: _,
            } => {
                // Compile scrutinee and store in temp local
                self.compile_expr(ctx, builder, scrutinee)?;
                builder.local_set(ctx.tmp_i32);

                // Build nested if-else chain for arms
                // Each arm: check pattern, if matches execute body
                // We'll use a simple approach: each arm is an if/else

                for (i, arm) in arms.iter().enumerate() {
                    let is_last = i == arms.len() - 1;

                    match &arm.pattern {
                        kain_core::ast::Pattern::Wildcard(_) => {
                            // Wildcard always matches - just emit the body
                            self.compile_expr(ctx, builder, &arm.body)?;
                        }
                        kain_core::ast::Pattern::Literal(lit_expr) => {
                            // Compare scrutinee with literal
                            builder.local_get(ctx.tmp_i32);
                            self.compile_expr(ctx, builder, lit_expr)?;
                            // Wrap i64 to i32 for comparison if needed
                            builder.unop(walrus::ir::UnaryOp::I32WrapI64);
                            builder.binop(walrus::ir::BinaryOp::I32Eq);

                            if is_last {
                                // Last arm: just emit body conditionally
                                builder.if_else(
                                    None,
                                    |then_b| {
                                        let _ = self.compile_expr(ctx, then_b, &arm.body);
                                    },
                                    |_else_b| {},
                                );
                            } else {
                                builder.if_else(
                                    None,
                                    |then_b| {
                                        let _ = self.compile_expr(ctx, then_b, &arm.body);
                                    },
                                    |_else_b| {
                                        // Continue to next arm - but we can't recurse easily here
                                        // For now, just leave empty - full impl needs restructuring
                                    },
                                );
                            }
                        }
                        kain_core::ast::Pattern::Binding { name, .. } => {
                            // Binding: bind scrutinee to local and execute body
                            if let Some(local_id) = ctx.locals.get(name) {
                                builder.local_get(ctx.tmp_i32);
                                builder.unop(walrus::ir::UnaryOp::I64ExtendSI32); // Convert back to i64
                                builder.local_set(*local_id);
                            }
                            self.compile_expr(ctx, builder, &arm.body)?;
                        }
                        kain_core::ast::Pattern::Variant { variant, .. } => {
                            // For enum patterns: load tag, compare with variant tag
                            // Load tag from scrutinee pointer
                            builder.local_get(ctx.tmp_i32);
                            builder.load(
                                ctx.memory_id,
                                walrus::ir::LoadKind::I32 { atomic: false },
                                walrus::ir::MemArg {
                                    align: 4,
                                    offset: 0,
                                },
                            );

                            // TODO: look up variant tag from enum_layouts
                            // For now just use the variant name hash as placeholder
                            let tag = variant.len() as i32 % 256; // Placeholder
                            builder.i32_const(tag);
                            builder.binop(walrus::ir::BinaryOp::I32Eq);

                            builder.if_else(
                                None,
                                |then_b| {
                                    let _ = self.compile_expr(ctx, then_b, &arm.body);
                                },
                                |_else_b| {},
                            );
                        }
                        _ => {
                            // Other patterns: just emit body (fallback)
                            self.compile_expr(ctx, builder, &arm.body)?;
                        }
                    }
                }
            }
            // MacroCall: handle println!, print!, dbg!
            Expr::MacroCall {
                name,
                args,
                span: _,
            } => {
                match name.as_str() {
                    "println" | "print" => {
                        // For each argument, determine type and call appropriate print function
                        for arg in args {
                            match arg {
                                Expr::Int(_, _) => {
                                    self.compile_expr(ctx, builder, arg)?;
                                    if let Some(func_id) = ctx.functions.get("print_i64") {
                                        builder.call(*func_id);
                                    }
                                }
                                Expr::Float(_, _) => {
                                    self.compile_expr(ctx, builder, arg)?;
                                    if let Some(func_id) = ctx.functions.get("print_f64") {
                                        builder.call(*func_id);
                                    }
                                }
                                Expr::Bool(_, _) => {
                                    self.compile_expr(ctx, builder, arg)?;
                                    if let Some(func_id) = ctx.functions.get("print_bool") {
                                        builder.call(*func_id);
                                    }
                                }
                                Expr::String(s, _) => {
                                    // For strings, we need ptr and len
                                    if let Some(&offset) = ctx.string_table.get(s) {
                                        // Push pointer (offset + 4 to skip length prefix)
                                        builder.i32_const((offset + 4) as i32);
                                        // Push length
                                        builder.i32_const(s.len() as i32);
                                        if let Some(func_id) = ctx.functions.get("print_str") {
                                            builder.call(*func_id);
                                        }
                                    }
                                }
                                Expr::Ident(_, _) => {
                                    // For variables, compile and assume i64 for now
                                    self.compile_expr(ctx, builder, arg)?;
                                    if let Some(func_id) = ctx.functions.get("print_i64") {
                                        builder.call(*func_id);
                                    }
                                }
                                _ => {
                                    // Default: compile and print as i64
                                    self.compile_expr(ctx, builder, arg)?;
                                    if let Some(func_id) = ctx.functions.get("print_i64") {
                                        builder.call(*func_id);
                                    }
                                }
                            }
                        }
                        // Push a dummy value since expressions need to produce something
                        builder.i64_const(0);
                    }
                    "dbg" => {
                        // Debug: print and return the value
                        if let Some(arg) = args.first() {
                            self.compile_expr(ctx, builder, arg)?;
                            // Duplicate for print and return
                            // Actually can't dup easily, so just print
                            if let Some(func_id) = ctx.functions.get("print_i64") {
                                builder.call(*func_id);
                            }
                        }
                        builder.i64_const(0);
                    }
                    _ => {
                        // Unknown macro - just push 0
                        builder.i64_const(0);
                    }
                }
            }
            // Range expression: for now just push start value since ranges are handled inline in for loops
            Expr::Range {
                start,
                end: _,
                inclusive: _,
                span: _,
            } => {
                // Ranges are typically used inline in for loops
                // If used standalone, just return the start value
                if let Some(start_expr) = start {
                    self.compile_expr(ctx, builder, start_expr)?;
                } else {
                    builder.i64_const(0);
                }
            }
            // Lambda expression: return table index for the pre-compiled lambda function
            Expr::Lambda {
                params,
                return_type: _,
                body: _,
                span: _,
            } => {
                // Lambdas are compiled in pre-pass and stored in lambda_table
                // Find the lambda by matching parameter count (simplified - proper impl would use unique IDs)
                // For now, we need to track which lambda this is
                //
                // Since lambdas are assigned IDs in order during collection,
                // we need to find which ID this lambda has
                // This is a limitation - proper impl would tag each lambda AST with an ID
                //
                // For now, push the table index based on param count heuristic
                // This works if lambdas are unique by param count
                let _param_count = params.len() as u32;

                // Search lambda_table for a lambda with matching param count
                let mut found_index = 0i32;
                for (_id, (table_idx, _func_id)) in ctx.lambda_table.iter() {
                    // Simple heuristic: use first lambda if param counts can't be matched
                    found_index = *table_idx as i32;
                    break; // TODO: proper ID tracking
                }

                // Push table index as i32 (for call_indirect)
                builder.i32_const(found_index);
            }
            // Block expression: compile all statements, return last expression value
            Expr::Block(block, _span) => {
                // Compile all statements except the last
                for (i, stmt) in block.stmts.iter().enumerate() {
                    if i < block.stmts.len() - 1 {
                        self.compile_stmt(ctx, builder, stmt)?;
                    } else {
                        // Last statement - if it's an expression, keep its value
                        if let Stmt::Expr(expr) = stmt {
                            self.compile_expr(ctx, builder, expr)?;
                        } else {
                            self.compile_stmt(ctx, builder, stmt)?;
                            builder.i64_const(0); // Block returns unit
                        }
                    }
                }
                if block.stmts.is_empty() {
                    builder.i64_const(0);
                }
            }
            Expr::Return(value, _) => {
                if let Some(value) = value {
                    self.compile_expr(ctx, builder, value)?;
                }
                builder.return_();
            }
            Expr::Decay { target, .. } => {
                self.compile_expr(ctx, builder, target)?;
                builder.drop();
                builder.i64_const(0);
            }
            Expr::Break(_, _) | Expr::Continue(_) => {
                return Err(KainError::codegen(
                    "break/continue expressions are not yet supported in WASM codegen",
                    expr.span(),
                ));
            }
            _ => {
                return Err(KainError::codegen(
                    format!("Unsupported expression in WASM codegen: {:?}", expr),
                    expr.span(),
                ));
            }
        }
        Ok(())
    }

    fn compile_else_branch(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        branch: &kain_core::ast::ElseBranch,
    ) -> KainResult<()> {
        match branch {
            kain_core::ast::ElseBranch::Else(block) => {
                self.compile_block(ctx, builder, block)?;
            }
            kain_core::ast::ElseBranch::ElseIf(cond, then, next_else) => {
                self.compile_expr(ctx, builder, cond)?;

                let branch_error = std::cell::RefCell::new(None);
                builder.if_else(
                    None,
                    |then_builder| {
                        if let Err(err) = self.compile_block(ctx, then_builder, then) {
                            *branch_error.borrow_mut() = Some(err);
                        }
                    },
                    |else_builder| {
                        if let Some(next) = next_else {
                            if let Err(err) = self.compile_else_branch(ctx, else_builder, next) {
                                *branch_error.borrow_mut() = Some(err);
                            }
                        }
                    },
                );
                if let Some(err) = branch_error.into_inner() {
                    return Err(err);
                }
            }
        }
        Ok(())
    }

    fn compile_jsx_node(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        node: &kain_core::ast::JSXNode,
    ) -> KainResult<()> {
        match node {
            kain_core::ast::JSXNode::Element {
                tag,
                attributes,
                children,
                ..
            } => {
                // 1. Compile Children
                for child in children {
                    self.compile_jsx_node(ctx, builder, child)?;
                }

                // 2. Allocate Children Array
                let child_count = children.len() as u32;
                let children_size = 4 + (child_count * 4);
                self.emit_alloc(ctx, builder, children_size);
                builder.local_set(ctx.tmp_i32); // Save array ptr

                // Store children (Reverse order because they are on stack)
                for i in (0..child_count).rev() {
                    // Stack: [.., child_val]
                    builder.local_set(ctx.tmp_i32_2); // Pop child val

                    // Addr = base + 4 + i*4
                    builder.local_get(ctx.tmp_i32);
                    builder.i32_const((4 + i * 4) as i32);
                    builder.binop(walrus::ir::BinaryOp::I32Add);

                    builder.local_get(ctx.tmp_i32_2); // Val

                    builder.store(
                        ctx.memory_id,
                        walrus::ir::StoreKind::I32 { atomic: false },
                        walrus::ir::MemArg {
                            align: 4,
                            offset: 0,
                        },
                    );
                }

                // Store length
                builder.local_get(ctx.tmp_i32);
                builder.i32_const(child_count as i32);
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::I32 { atomic: false },
                    walrus::ir::MemArg {
                        align: 4,
                        offset: 0,
                    },
                );

                // Keep Children Array Ptr on stack (Wait, we stored it in tmp_i32, but we need to push it back)
                // BUT we have Props to compile. If Props use tmp_i32, we lose it.
                // We MUST push it to stack now.
                builder.local_get(ctx.tmp_i32);
                // Stack: [children_ptr]

                // 3. Compile Props
                let props_count = attributes.len() as u32;
                for attr in attributes {
                    // Key
                    if let Some(&offset) = ctx.string_table.get(&attr.name) {
                        builder.i32_const((offset + 4) as i32);
                    } else {
                        builder.i32_const(0);
                    }

                    // Value
                    match &attr.value {
                        kain_core::ast::JSXAttrValue::String(s) => {
                            if let Some(&offset) = ctx.string_table.get(s) {
                                builder.i32_const((offset + 4) as i32);
                            } else {
                                builder.i32_const(0);
                            }
                            builder.unop(walrus::ir::UnaryOp::I64ExtendUI32);
                        }
                        kain_core::ast::JSXAttrValue::Expr(e) => {
                            self.compile_expr(ctx, builder, e)?;
                        }
                        kain_core::ast::JSXAttrValue::Bool(b) => {
                            builder.i64_const(if *b { 1 } else { 0 });
                        }
                    }
                }

                // Allocate Props Array
                let props_item_size = 12;
                let props_size = 4 + (props_count * props_item_size);
                self.emit_alloc(ctx, builder, props_size);
                builder.local_set(ctx.tmp_i32); // Save props array ptr

                // Store Props (Reverse)
                for i in (0..props_count).rev() {
                    builder.local_set(ctx.tmp_i64); // Pop val (i64)
                    builder.local_set(ctx.tmp_i32_2); // Pop key (i32)

                    // Store Key
                    builder.local_get(ctx.tmp_i32);
                    builder.i32_const((4 + i * props_item_size) as i32);
                    builder.binop(walrus::ir::BinaryOp::I32Add);
                    builder.local_get(ctx.tmp_i32_2);
                    builder.store(
                        ctx.memory_id,
                        walrus::ir::StoreKind::I32 { atomic: false },
                        walrus::ir::MemArg {
                            align: 4,
                            offset: 0,
                        },
                    );

                    // Store Val
                    builder.local_get(ctx.tmp_i32);
                    builder.i32_const((4 + i * props_item_size + 4) as i32);
                    builder.binop(walrus::ir::BinaryOp::I32Add);
                    builder.local_get(ctx.tmp_i64);
                    builder.store(
                        ctx.memory_id,
                        walrus::ir::StoreKind::I64 { atomic: false },
                        walrus::ir::MemArg {
                            align: 8,
                            offset: 0,
                        },
                    );
                }

                // Store Props Length
                builder.local_get(ctx.tmp_i32);
                builder.i32_const(props_count as i32);
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::I32 { atomic: false },
                    walrus::ir::MemArg {
                        align: 4,
                        offset: 0,
                    },
                );

                // Push Props Ptr
                builder.local_get(ctx.tmp_i32);

                // Stack: [children_ptr, props_ptr]

                // 4. Allocate VNode (16 bytes)
                self.emit_alloc(ctx, builder, 16);
                builder.local_set(ctx.tmp_i32); // VNode Ptr

                // Store Props Ptr (offset 8)
                // Stack: [children_ptr, props_ptr]
                builder.local_set(ctx.tmp_i32_2); // props_ptr

                builder.local_get(ctx.tmp_i32);
                builder.i32_const(8);
                builder.binop(walrus::ir::BinaryOp::I32Add);
                builder.local_get(ctx.tmp_i32_2);
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::I32 { atomic: false },
                    walrus::ir::MemArg {
                        align: 4,
                        offset: 0,
                    },
                );

                // Store Children Ptr (offset 12)
                // Stack: [children_ptr]
                builder.local_set(ctx.tmp_i32_2); // children_ptr

                builder.local_get(ctx.tmp_i32);
                builder.i32_const(12);
                builder.binop(walrus::ir::BinaryOp::I32Add);
                builder.local_get(ctx.tmp_i32_2);
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::I32 { atomic: false },
                    walrus::ir::MemArg {
                        align: 4,
                        offset: 0,
                    },
                );

                // Store Type = 1 (Element) (offset 0)
                builder.local_get(ctx.tmp_i32);
                builder.i32_const(1);
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::I32 { atomic: false },
                    walrus::ir::MemArg {
                        align: 4,
                        offset: 0,
                    },
                );

                // Store Tag (offset 4)
                let tag_ptr = if let Some(&offset) = ctx.string_table.get(tag) {
                    offset + 4
                } else {
                    0
                };
                builder.local_get(ctx.tmp_i32);
                builder.i32_const(tag_ptr as i32);
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::I32 { atomic: false },
                    walrus::ir::MemArg {
                        align: 4,
                        offset: 4,
                    },
                );

                // Return VNode Ptr
                builder.local_get(ctx.tmp_i32);
            }
            kain_core::ast::JSXNode::Text(s, _) => {
                self.emit_alloc(ctx, builder, 16);
                builder.local_set(ctx.tmp_i32);

                builder.local_get(ctx.tmp_i32);
                builder.i32_const(0); // Type = 0 (Text)
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::I32 { atomic: false },
                    walrus::ir::MemArg {
                        align: 4,
                        offset: 0,
                    },
                );

                let text_ptr = if let Some(&offset) = ctx.string_table.get(s) {
                    offset + 4
                } else {
                    0
                };
                builder.local_get(ctx.tmp_i32);
                builder.i32_const(text_ptr as i32);
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::I32 { atomic: false },
                    walrus::ir::MemArg {
                        align: 4,
                        offset: 12,
                    },
                ); // Store in text field (offset 12)

                builder.local_get(ctx.tmp_i32);
            }
            kain_core::ast::JSXNode::Expression(e) => {
                self.compile_expr(ctx, builder, e)?;
                builder.unop(walrus::ir::UnaryOp::I32WrapI64);
            }
            _ => {
                builder.i32_const(0);
            }
        }
        Ok(())
    }

    fn emit_store_for_expr(
        &self,
        ctx: &CompilationContext,
        builder: &mut InstrSeqBuilder,
        expr: &Expr,
        offset: u32,
    ) {
        let val_type = self.infer_expr_wasm_type_in_context(ctx, expr);
        match val_type {
            ValType::I64 => {
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::I64 { atomic: false },
                    walrus::ir::MemArg { align: 8, offset },
                );
            }
            ValType::F64 => {
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::F64,
                    walrus::ir::MemArg { align: 8, offset },
                );
            }
            ValType::F32 => {
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::F32,
                    walrus::ir::MemArg { align: 4, offset },
                );
            }
            ValType::I32 => {
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::I32 { atomic: false },
                    walrus::ir::MemArg { align: 4, offset },
                );
            }
            ValType::V128 | ValType::Ref(_) => {
                builder.store(
                    ctx.memory_id,
                    walrus::ir::StoreKind::I64 { atomic: false },
                    walrus::ir::MemArg { align: 8, offset },
                );
            }
        }
    }
}
