use crate::ast::*;
use crate::diagnostic_registry::DiagnosticCode;
use crate::error::{DiagnosticBuilder, ErrorKind, KainResult};
use crate::low_level_abi::{
    c_abi_policy_for_target, promoted_integer_bits, promoted_type_for_arithmetic,
    should_apply_usual_arithmetic_conversions, usual_arithmetic_conversion_type, CAbiPolicy,
};
use crate::low_level_memory_metadata::{
    attr_usize_arg, attr_usize_bool_args, has_attr, C_BITFIELD_ATTR, C_PACK_ALIGN_ATTR,
    C_PACKED_ATTR, C_STORAGE_ALIGN_ATTR, C_STORAGE_BITS_ATTR, C_TYPE_ALIGN_ATTR, C_UNION_ATTR,
};
use crate::monomorphize::MonomorphizedProgram;
use crate::span::Span;
use crate::types::{
    ResolvedType, TypedActor, TypedComponent, TypedConst, TypedEnum, TypedFunction, TypedImpl,
    TypedItem, TypedProgram, TypedStruct, TypedTypeAlias,
};
use crate::CompileTarget;
use std::collections::{HashMap, HashSet};

#[derive(Debug, Clone)]
struct LayoutField {
    offset: usize,
    ty: Type,
    bit_width: Option<usize>,
    bit_offset: Option<usize>,
    bit_signed: bool,
}

#[derive(Debug, Clone, Default)]
struct StructLayoutInfo {
    size: usize,
    align: usize,
    is_union: bool,
    field_order: Vec<String>,
    fields: HashMap<String, LayoutField>,
}

#[derive(Debug, Clone)]
struct LayoutRegistry {
    abi: &'static CAbiPolicy,
    structs: HashMap<String, StructLayoutInfo>,
}

impl LayoutRegistry {
    fn build_with_abi(items: &[TypedItem], abi: &'static CAbiPolicy) -> Self {
        let mut registry = Self {
            abi,
            structs: HashMap::new(),
        };
        for item in items {
            if let TypedItem::Struct(st) = item {
                let is_union = has_attr(&st.ast.attributes, C_UNION_ATTR);
                let is_packed = has_attr(&st.ast.attributes, C_PACKED_ATTR);
                let pack_align = attr_usize_arg(&st.ast.attributes, C_PACK_ALIGN_ATTR)
                    .map(|bits| bits.div_ceil(8))
                    .filter(|align| *align > 0);
                let explicit_type_align = attr_usize_arg(&st.ast.attributes, C_TYPE_ALIGN_ATTR)
                    .map(|bits| bits.div_ceil(8))
                    .filter(|align| *align > 0);
                let mut offset = 0usize;
                let mut max_field_size = 0usize;
                let mut max_align = 1usize;
                let mut fields = HashMap::new();
                let mut field_order = Vec::new();
                let mut bit_pack: Option<BitfieldPack> = None;
                for field in &st.ast.fields {
                    let size = attr_usize_arg(&field.attributes, C_STORAGE_BITS_ATTR)
                        .map(|bits| bits.div_ceil(8))
                        .unwrap_or_else(|| registry.type_size_fallback(&field.ty));
                    let natural_align = attr_usize_arg(&field.attributes, C_STORAGE_ALIGN_ATTR)
                        .map(|bits| bits.div_ceil(8))
                        .unwrap_or_else(|| registry.type_align_fallback(&field.ty))
                        .max(1);
                    let align = if let Some(pack_align) = pack_align {
                        natural_align.min(pack_align).max(1)
                    } else if is_packed {
                        registry.abi.packed_struct_align.max(1)
                    } else {
                        natural_align
                    };
                    let (bit_width, bit_signed) = attr_usize_bool_args(&field.attributes, C_BITFIELD_ATTR)
                        .map(|(width, signed)| (Some(width), signed))
                        .unwrap_or_else(|| (attr_usize_arg(&field.attributes, C_BITFIELD_ATTR), true));
                    max_field_size = max_field_size.max(size.max(1));
                    max_align = max_align.max(align);
                    let field_name = field.name.clone();
                    let (field_offset, field_bit_offset) = if is_union {
                        (0usize, bit_width.map(|_| 0usize))
                    } else if let Some(width) = bit_width {
                        let width = width.max(1);
                        let unit_size = size.max(1);
                        let unit_bits = unit_size.saturating_mul(8).max(1);

                        if width >= unit_bits {
                            if let Some(pack) = bit_pack.take() {
                                offset = offset.max(pack.unit_offset + pack.unit_size);
                            }
                            offset = align_up(offset, align);
                            let field_offset = offset;
                            offset += unit_size;
                            (field_offset, Some(0usize))
                        } else {
                            let needs_new_pack = match bit_pack.as_ref() {
                                Some(pack) => {
                                    pack.unit_size != unit_size
                                        || pack.align != align
                                        || pack.used_bits + width > unit_bits
                                }
                                None => true,
                            };

                            if needs_new_pack {
                                if let Some(pack) = bit_pack.take() {
                                    offset = offset.max(pack.unit_offset + pack.unit_size);
                                }
                                offset = align_up(offset, align);
                                bit_pack = Some(BitfieldPack {
                                    unit_offset: offset,
                                    unit_size,
                                    align,
                                    used_bits: 0,
                                });
                            }

                            let pack = bit_pack
                                .as_mut()
                                .expect("bitfield pack should exist before recording field");
                            let field_offset = pack.unit_offset;
                            let field_bit_offset = if registry.abi.bitfield_lsb_first {
                                pack.used_bits
                            } else {
                                unit_bits.saturating_sub(pack.used_bits + width)
                            };
                            pack.used_bits += width;
                            if pack.used_bits >= unit_bits {
                                let flushed = bit_pack.take().expect("bitfield pack should flush");
                                offset = offset.max(flushed.unit_offset + flushed.unit_size);
                            }
                            (field_offset, Some(field_bit_offset))
                        }
                    } else {
                        if let Some(pack) = bit_pack.take() {
                            offset = offset.max(pack.unit_offset + pack.unit_size);
                        }
                        offset = align_up(offset, align);
                        let field_offset = offset;
                        offset += size.max(1);
                        (field_offset, None)
                    };
                    fields.insert(
                        field_name.clone(),
                        LayoutField {
                            offset: field_offset,
                            ty: field.ty.clone(),
                            bit_width,
                            bit_offset: field_bit_offset,
                            bit_signed,
                        },
                    );
                    field_order.push(field_name);
                }
                if let Some(pack) = bit_pack.take() {
                    offset = offset.max(pack.unit_offset + pack.unit_size);
                }
                let raw_size = if is_union {
                    max_field_size
                } else {
                    offset
                };
                let base_final_align = if let Some(pack_align) = pack_align {
                    max_align.min(pack_align).max(1)
                } else if is_packed {
                    registry.abi.packed_struct_align.max(1)
                } else {
                    max_align
                };
                let final_align = explicit_type_align
                    .map(|align| align.max(base_final_align))
                    .unwrap_or(base_final_align);
                let size = align_up(raw_size.max(1), final_align);
                registry.structs.insert(
                    st.ast.name.clone(),
                    StructLayoutInfo {
                        size,
                        align: final_align,
                        is_union,
                        field_order,
                        fields,
                    },
                );
            }
        }
        registry
    }

    fn field(&self, struct_name: &str, field: &str) -> Option<&LayoutField> {
        self.structs.get(struct_name)?.fields.get(field)
    }

    fn type_size_fallback(&self, ty: &Type) -> usize {
        match ty {
            Type::Named { name, .. } => match name.as_str() {
                "Bool" => self.abi.bool_bits.div_ceil(8),
                "Char" => self.abi.char_bits.div_ceil(8),
                "Int" | "isize" | "usize" => self.abi.long_bits.div_ceil(8),
                "Float" => self.abi.double_bits.div_ceil(8),
                other => self.structs.get(other).map(|info| info.size).unwrap_or(8),
            },
            Type::Array(inner, size, _) => self.type_size_fallback(inner) * size,
            Type::Slice(_, _) => 16,
            Type::Tuple(types, _) => types.iter().map(|ty| self.type_size_fallback(ty)).sum(),
            Type::Ref { .. } | Type::Ptr { .. } => 8,
            Type::Option(inner, _) => self.type_size_fallback(inner),
            Type::Result(ok, err, _) => {
                self.type_size_fallback(ok).max(self.type_size_fallback(err))
            }
            Type::Unit(_) | Type::Never(_) => 0,
            _ => 8,
        }
    }

    fn type_align_fallback(&self, ty: &Type) -> usize {
        match ty {
            Type::Named { name, .. } => match name.as_str() {
                "Bool" => self.abi.bool_bits.div_ceil(8),
                "Char" => self.abi.char_bits.div_ceil(8),
                "Int" | "isize" | "usize" => self.abi.long_bits.div_ceil(8),
                "Float" => self.abi.double_bits.div_ceil(8),
                other => self
                    .structs
                    .get(other)
                    .map(|info| info.align.max(1))
                    .unwrap_or(8),
            },
            Type::Array(inner, _, _) => self.type_align_fallback(inner),
            Type::Slice(_, _) => 8,
            Type::Tuple(types, _) => types
                .iter()
                .map(|ty| self.type_align_fallback(ty))
                .max()
                .unwrap_or(1),
            Type::Ref { .. } | Type::Ptr { .. } => 8,
            Type::Option(inner, _) => self.type_align_fallback(inner),
            Type::Result(ok, err, _) => {
                self.type_align_fallback(ok).max(self.type_align_fallback(err))
            }
            Type::Unit(_) | Type::Never(_) => 1,
            _ => 8,
        }
    }
}

#[derive(Debug, Clone, Copy)]
struct BitfieldPack {
    unit_offset: usize,
    unit_size: usize,
    align: usize,
    used_bits: usize,
}

fn align_up(value: usize, align: usize) -> usize {
    let align = align.max(1);
    let remainder = value % align;
    if remainder == 0 {
        value
    } else {
        value + (align - remainder)
    }
}

#[derive(Debug, Clone)]
struct FunctionMemoryCtx<'a> {
    _target: CompileTarget,
    layouts: &'a LayoutRegistry,
    address_taken: HashSet<String>,
    local_types: HashMap<String, Type>,
}

#[derive(Debug, Clone, Copy)]
pub struct BackendMemoryCapabilities {
    pub raw_pointers: bool,
    pub raw_memory_ops: bool,
}

const TS_MEMORY_CAPS: BackendMemoryCapabilities = BackendMemoryCapabilities {
    raw_pointers: false,
    raw_memory_ops: false,
};
const UE5_MEMORY_CAPS: BackendMemoryCapabilities = BackendMemoryCapabilities {
    raw_pointers: false,
    raw_memory_ops: false,
};
const DEFAULT_MEMORY_CAPS: BackendMemoryCapabilities = BackendMemoryCapabilities {
    raw_pointers: true,
    raw_memory_ops: true,
};

pub fn backend_memory_capabilities(target: CompileTarget) -> BackendMemoryCapabilities {
    match target {
        CompileTarget::Ts | CompileTarget::Js | CompileTarget::Wasm | CompileTarget::Hybrid => {
            TS_MEMORY_CAPS
        }
        CompileTarget::Ue5 | CompileTarget::Ue5Editor => UE5_MEMORY_CAPS,
        _ => DEFAULT_MEMORY_CAPS,
    }
}

pub fn validate_typed_program_memory_support(
    program: &TypedProgram,
    target: CompileTarget,
) -> KainResult<()> {
    let caps = backend_memory_capabilities(target);
    if caps.raw_pointers && caps.raw_memory_ops {
        return Ok(());
    }

    if let Some(context) = first_unsupported_memory_context(&program.items, caps) {
        return Err(
            DiagnosticBuilder::new(
                ErrorKind::Validation,
                DiagnosticCode::MemoryUnsupportedBackend,
                format!(
                    "Target '{:?}' does not currently support raw low-level memory semantics in normalized codegen.",
                    target
                ),
            )
            .context(context)
            .build(),
        );
    }

    Ok(())
}

pub fn lower_typed_program_memory_for_target(
    program: &TypedProgram,
    target: CompileTarget,
) -> KainResult<TypedProgram> {
    if !matches!(
        target,
        CompileTarget::Ts
            | CompileTarget::Js
            | CompileTarget::Wasm
            | CompileTarget::Cpp
            | CompileTarget::Rust
            | CompileTarget::Ue5
            | CompileTarget::Ue5Editor
    ) {
        return Ok(program.clone());
    }
    let layouts = LayoutRegistry::build_with_abi(&program.items, c_abi_policy_for_target(target));

    Ok(TypedProgram {
        items: program
            .items
            .iter()
            .map(|item| lower_typed_item_memory(item, target, &layouts))
            .collect(),
    })
}

pub fn lower_monomorphized_program_memory_for_target(
    program: &MonomorphizedProgram,
    target: CompileTarget,
) -> KainResult<MonomorphizedProgram> {
    Ok(MonomorphizedProgram {
        items: lower_typed_program_memory_for_target(
            &TypedProgram {
                items: program.items.clone(),
            },
            target,
        )?
        .items,
    })
}

fn lower_typed_item_memory(
    item: &TypedItem,
    target: CompileTarget,
    layouts: &LayoutRegistry,
) -> TypedItem {
    match item {
        TypedItem::Function(function) => TypedItem::Function(TypedFunction {
            ast: lower_function_memory(&function.ast, target, layouts),
            resolved_type: lower_resolved_type_memory(&function.resolved_type),
            effects: function.effects.clone(),
        }),
        TypedItem::Struct(struct_item) => TypedItem::Struct(TypedStruct {
            ast: Struct {
                fields: struct_item
                    .ast
                    .fields
                    .iter()
                    .map(|field| Field {
                        ty: lower_type_memory(&field.ty),
                        default: field
                            .default
                            .as_ref()
                            .map(|expr| lower_free_expr_memory(expr, target, layouts)),
                        ..field.clone()
                    })
                    .collect(),
                methods: struct_item
                    .ast
                    .methods
                    .iter()
                    .map(|method| lower_function_memory(method, target, layouts))
                    .collect(),
                ..struct_item.ast.clone()
            },
            field_types: struct_item
                .field_types
                .iter()
                .map(|(name, ty)| (name.clone(), lower_resolved_type_memory(ty)))
                .collect(),
        }),
        TypedItem::Component(component) => TypedItem::Component(TypedComponent {
            ast: Component {
                props: component
                    .ast
                    .props
                    .iter()
                    .map(|prop| Param {
                        ty: lower_type_memory(&prop.ty),
                        ..prop.clone()
                    })
                    .collect(),
                body: component.ast.body.clone(),
                ..component.ast.clone()
            },
            prop_types: component
                .prop_types
                .iter()
                .map(|(name, ty)| (name.clone(), lower_resolved_type_memory(ty)))
                .collect(),
        }),
        TypedItem::Actor(actor) => TypedItem::Actor(TypedActor {
            ast: Actor {
                state: actor
                    .ast
                    .state
                    .iter()
                    .map(|state| StateDecl {
                        ty: lower_type_memory(&state.ty),
                        initial: lower_free_expr_memory(&state.initial, target, layouts),
                        ..state.clone()
                    })
                    .collect(),
                handlers: actor
                    .ast
                    .handlers
                    .iter()
                    .map(|handler| lower_actor_handler_memory(handler, target, layouts))
                    .collect(),
                methods: actor
                    .ast
                    .methods
                    .iter()
                    .map(|method| lower_function_memory(method, target, layouts))
                    .collect(),
                ..actor.ast.clone()
            },
            state_types: actor
                .state_types
                .iter()
                .map(|(name, ty)| (name.clone(), lower_resolved_type_memory(ty)))
                .collect(),
        }),
        TypedItem::Const(constant) => TypedItem::Const(TypedConst {
            ast: Const {
                ty: lower_type_memory(&constant.ast.ty),
                value: lower_free_expr_memory(&constant.ast.value, target, layouts),
                ..constant.ast.clone()
            },
            ty: lower_resolved_type_memory(&constant.ty),
        }),
        TypedItem::Impl(imp) => TypedItem::Impl(TypedImpl {
            ast: Impl {
                target_type: lower_type_memory(&imp.ast.target_type),
                methods: imp
                    .ast
                    .methods
                    .iter()
                    .map(|method| lower_function_memory(method, target, layouts))
                    .collect(),
                ..imp.ast.clone()
            },
        }),
        TypedItem::TypeAlias(alias) => TypedItem::TypeAlias(TypedTypeAlias {
            ast: TypeAlias {
                target: lower_type_memory(&alias.ast.target),
                ..alias.ast.clone()
            },
        }),
        TypedItem::Enum(enum_item) => TypedItem::Enum(TypedEnum {
            ast: Enum {
                variants: enum_item
                    .ast
                    .variants
                    .iter()
                    .map(|variant| Variant {
                        fields: match &variant.fields {
                            VariantFields::Unit => VariantFields::Unit,
                            VariantFields::Tuple(types) => VariantFields::Tuple(
                                types.iter().map(lower_type_memory).collect(),
                            ),
                            VariantFields::Struct(fields) => VariantFields::Struct(
                                fields
                                    .iter()
                                    .map(|field| Field {
                                        ty: lower_type_memory(&field.ty),
                                        default: field
                                            .default
                                            .as_ref()
                                            .map(|expr| lower_free_expr_memory(expr, target, layouts)),
                                        ..field.clone()
                                    })
                                    .collect(),
                            ),
                        },
                        ..variant.clone()
                    })
                    .collect(),
                ..enum_item.ast.clone()
            },
            variant_payload_types: enum_item
                .variant_payload_types
                .iter()
                .map(|(name, tys)| {
                    (
                        name.clone(),
                        tys.iter().map(lower_resolved_type_memory).collect(),
                    )
                })
                .collect(),
        }),
        _ => item.clone(),
    }
}

fn lower_function_memory(
    function: &Function,
    target: CompileTarget,
    layouts: &LayoutRegistry,
) -> Function {
    let params = function
        .params
        .iter()
        .map(|param| Param {
            ty: lower_type_memory(&param.ty),
            ..param.clone()
        })
        .collect::<Vec<_>>();

    let mut local_types = collect_local_types_from_block(&function.body);
    for param in &function.params {
        local_types.insert(param.name.clone(), param.ty.clone());
    }

    let mut ctx = FunctionMemoryCtx {
        _target: target,
        layouts,
        address_taken: collect_address_taken_roots(&function.body),
        local_types,
    };
    let mut body = lower_block_memory_with_ctx(&function.body, &mut ctx);

    if !ctx.address_taken.is_empty() {
        let mut prefix = Vec::new();
        for param in &function.params {
            if ctx.address_taken.contains(&param.name) {
                prefix.push(make_pointer_binding_stmt(&param.name, param.span));
            }
        }
        if !prefix.is_empty() {
            prefix.extend(body.stmts);
            body.stmts = prefix;
        }
    }

    Function {
        params,
        return_type: function.return_type.as_ref().map(lower_type_memory),
        body,
        ..function.clone()
    }
}

fn lower_free_expr_memory(expr: &Expr, target: CompileTarget, layouts: &LayoutRegistry) -> Expr {
    let mut ctx = FunctionMemoryCtx {
        _target: target,
        layouts,
        address_taken: HashSet::new(),
        local_types: HashMap::new(),
    };
    lower_expr_memory_with_ctx(expr, &mut ctx)
}

fn lower_actor_handler_memory(
    handler: &MessageHandler,
    target: CompileTarget,
    layouts: &LayoutRegistry,
) -> MessageHandler {
    let mut local_types = collect_local_types_from_block(&handler.body);
    for param in &handler.params {
        local_types.insert(param.name.clone(), param.ty.clone());
    }

    let mut ctx = FunctionMemoryCtx {
        _target: target,
        layouts,
        address_taken: collect_address_taken_roots(&handler.body),
        local_types,
    };

    MessageHandler {
        params: handler
            .params
            .iter()
            .map(|param| Param {
                ty: lower_type_memory(&param.ty),
                ..param.clone()
            })
            .collect(),
        body: lower_block_memory_with_ctx(&handler.body, &mut ctx),
        ..handler.clone()
    }
}

fn lower_block_memory_with_ctx(block: &Block, ctx: &mut FunctionMemoryCtx<'_>) -> Block {
    let mut stmts = Vec::new();
    for stmt in &block.stmts {
        stmts.extend(lower_stmt_memory_with_ctx(stmt, ctx));
    }
    Block {
        stmts,
        ..block.clone()
    }
}

fn lower_stmt_memory_with_ctx(stmt: &Stmt, ctx: &mut FunctionMemoryCtx<'_>) -> Vec<Stmt> {
    match stmt {
        Stmt::Expr(expr) => vec![Stmt::Expr(lower_expr_memory_with_ctx(expr, ctx))],
        Stmt::Let {
            pattern,
            ty,
            value,
            span,
        } => {
            let lowered = Stmt::Let {
                pattern: pattern.clone(),
                ty: ty.as_ref().map(lower_type_memory),
                value: value.as_ref().map(|expr| lower_expr_memory_with_ctx(expr, ctx)),
                span: *span,
            };
            if let Pattern::Binding { name, .. } = pattern {
                if let Some(ty) = ty {
                    ctx.local_types.insert(name.clone(), ty.clone());
                }
                if ctx.address_taken.contains(name) {
                    return vec![lowered, make_pointer_binding_stmt(name, *span)];
                }
            }
            vec![lowered]
        }
        Stmt::Return(value, span) => vec![Stmt::Return(
            value.as_ref().map(|expr| lower_expr_memory_with_ctx(expr, ctx)),
            *span,
        )],
        Stmt::Break(value, span) => vec![Stmt::Break(
            value.as_ref().map(|expr| lower_expr_memory_with_ctx(expr, ctx)),
            *span,
        )],
        Stmt::Continue(span) => vec![Stmt::Continue(*span)],
        Stmt::For {
            binding,
            iter,
            body,
            span,
        } => vec![Stmt::For {
            binding: binding.clone(),
            iter: lower_expr_memory_with_ctx(iter, ctx),
            body: lower_block_memory_with_ctx(body, ctx),
            span: *span,
        }],
        Stmt::While {
            condition,
            body,
            span,
        } => vec![Stmt::While {
            condition: lower_expr_memory_with_ctx(condition, ctx),
            body: lower_block_memory_with_ctx(body, ctx),
            span: *span,
        }],
        Stmt::Loop { body, span } => vec![Stmt::Loop {
            body: lower_block_memory_with_ctx(body, ctx),
            span: *span,
        }],
        Stmt::Item(item) => vec![Stmt::Item(item.clone())],
    }
}

fn lower_expr_memory_with_ctx(expr: &Expr, ctx: &mut FunctionMemoryCtx<'_>) -> Expr {
    let span = expr.span();
    match expr {
        Expr::Ident(name, ident_span) if should_load_from_pointer(name, ctx) => {
            load_from_pointer_binding(name, ctx.local_types.get(name), *ident_span)
        }
        Expr::AddrOf {
            value,
            pointee_ty,
            ..
        } => lower_addr_of_memory(value, pointee_ty.as_ref(), ctx, span),
        Expr::PtrOffset {
            pointer,
            offset,
            element_ty,
            ..
        } => helper_call(
            "__kain_ptr_offset",
            vec![
                lower_expr_memory_with_ctx(pointer, ctx),
                lower_expr_memory_with_ctx(offset, ctx),
                Expr::Int(
                    memory_stride_for_type(element_ty.as_ref(), ctx.layouts).unwrap_or(1),
                    span,
                ),
            ],
            span,
        ),
        Expr::MemLoad {
            pointer,
            load_ty,
            ..
        } => {
            let call = helper_call(
                "__kain_mem_load",
                vec![lower_expr_memory_with_ctx(pointer, ctx)],
                span,
            );
            if let Some(ty) = load_ty.as_ref() {
                Expr::Cast {
                    value: Box::new(call),
                    target: lower_type_memory(ty),
                    span,
                }
            } else {
                call
            }
        }
        Expr::MemStore {
            pointer,
            value,
            ..
        } => helper_call(
            "__kain_mem_store",
            vec![
                lower_expr_memory_with_ctx(pointer, ctx),
                lower_expr_memory_with_ctx(value, ctx),
            ],
            span,
        ),
        Expr::SizeOfType { target, .. } => Expr::Int(estimate_type_size(target, ctx.layouts) as i64, span),
        Expr::AlignOfType { target, .. } => Expr::Int(estimate_type_align(target, ctx.layouts) as i64, span),
        Expr::Alloca { ty, .. } => lower_storage_expr(ty, ctx.layouts, span, false),
        Expr::Uninit { ty, .. } => lower_storage_expr(ty, ctx.layouts, span, false),
        Expr::Alloc {
            size,
            ty,
            zeroed,
            ..
        } => lower_heap_alloc_expr(
            &lower_expr_memory_with_ctx(size, ctx),
            ty.as_ref(),
            *zeroed,
            ctx.layouts,
            span,
        ),
        Expr::Realloc {
            pointer,
            size,
            ty,
            zeroed_new,
            ..
        } => helper_call(
            "__kain_realloc",
            vec![
                lower_expr_memory_with_ctx(pointer, ctx),
                lower_expr_memory_with_ctx(size, ctx),
                Expr::Int(memory_stride_for_type(ty.as_ref(), ctx.layouts).unwrap_or(1), span),
                storage_seed_expr(ty.as_ref(), ctx.layouts, span, *zeroed_new),
            ],
            span,
        ),
        Expr::AggregateInit {
            ty,
            fields,
            zero_fill_rest,
            ..
        } => {
            let lowered_fields = fields
                .iter()
                .map(|(name, value)| (name.clone(), lower_expr_memory_with_ctx(value, ctx)))
                .collect::<Vec<_>>();
            lower_aggregate_init_expr(ty, &lowered_fields, *zero_fill_rest, ctx.layouts, span)
        }
        Expr::Assign { target, value, span } => {
            if let Expr::Ident(name, _) = target.as_ref() {
                if should_load_from_pointer(name, ctx) {
                    return helper_call(
                        "__kain_mem_store",
                        vec![
                            Expr::Ident(pointer_binding_name(name), *span),
                            lower_expr_memory_with_ctx(value, ctx),
                        ],
                        *span,
                    );
                }
            }
            if let Expr::Field { object, field, .. } = target.as_ref() {
                if let Some(field_layout) = field_layout_from_object(object, field, ctx) {
                    let field_offset = field_layout.offset;
                    let field_bit_width = field_layout.bit_width;
                    let field_bit_offset = field_layout.bit_offset.unwrap_or(0);
                    let field_bit_signed = field_layout.bit_signed;
                    let lowered_object = lower_expr_memory_with_ctx(object, ctx);
                    let lowered_value = lower_expr_memory_with_ctx(value, ctx);
                    if let Some(bit_width) = field_bit_width {
                        let promoted_bits =
                            promoted_integer_bits(bit_width, field_bit_signed, ctx.layouts.abi);
                        return helper_call(
                            "__kain_bitfield_set",
                            vec![
                                lowered_object,
                                Expr::String(field.clone(), *span),
                                Expr::Int(field_offset as i64, *span),
                                Expr::Int(field_bit_offset as i64, *span),
                                Expr::Int(bit_width as i64, *span),
                                Expr::Bool(field_bit_signed, *span),
                                Expr::Int(promoted_bits as i64, *span),
                                lowered_value,
                            ],
                            *span,
                        );
                    }
                }
                if let Some(layout_size) = struct_layout_from_object(object, ctx)
                    .filter(|layout| layout.is_union)
                    .map(|layout| layout.size)
                {
                    let field_ty = field_type_from_object(
                        &infer_expr_type(object, ctx).unwrap_or(Type::Unit(*span)),
                        field,
                        ctx,
                    );
                    return helper_call(
                        "__kain_union_set",
                        vec![
                            lower_expr_memory_with_ctx(object, ctx),
                            Expr::String(field.clone(), *span),
                            Expr::String(
                                field_ty.as_ref().map(type_key).unwrap_or_else(|| "unknown".to_string()),
                                *span,
                            ),
                            Expr::Int(field_ty.as_ref().and_then(|ty| memory_stride_for_type(Some(ty), ctx.layouts)).unwrap_or(1), *span),
                            Expr::Int(layout_size as i64, *span),
                            lower_expr_memory_with_ctx(value, ctx),
                        ],
                        *span,
                    );
                }
            }
            Expr::Assign {
                target: Box::new(lower_expr_memory_with_ctx(target, ctx)),
                value: Box::new(lower_expr_memory_with_ctx(value, ctx)),
                span: *span,
            }
        }
        Expr::Ref {
            mutable,
            value,
            span,
        } => Expr::Ref {
            mutable: *mutable,
            value: Box::new(lower_expr_memory_with_ctx(value, ctx)),
            span: *span,
        },
        Expr::Deref(inner, inner_span) => {
            Expr::Deref(Box::new(lower_expr_memory_with_ctx(inner, ctx)), *inner_span)
        }
        Expr::Binary {
            left,
            op,
            right,
            span,
        } => {
            let left_ty = infer_expr_type(left, ctx);
            let right_ty = infer_expr_type(right, ctx);
            let lowered_left = lower_expr_memory_with_ctx(left, ctx);
            let lowered_right = lower_expr_memory_with_ctx(right, ctx);

            if should_apply_usual_arithmetic_conversions(*op) {
                let common_ty = left_ty
                    .as_ref()
                    .zip(right_ty.as_ref())
                    .and_then(|(lhs, rhs)| usual_arithmetic_conversion_type(lhs, rhs, ctx.layouts.abi));
                return Expr::Binary {
                    left: Box::new(cast_if_needed(lowered_left, common_ty.clone(), *span)),
                    op: *op,
                    right: Box::new(cast_if_needed(lowered_right, common_ty, *span)),
                    span: *span,
                };
            }

            let normalized_left = if matches!(op, BinaryOp::Shl | BinaryOp::Shr) {
                cast_if_needed(
                    lowered_left,
                    left_ty
                        .as_ref()
                        .and_then(|ty| promoted_type_for_arithmetic(ty, ctx.layouts.abi)),
                    *span,
                )
            } else {
                lowered_left
            };
            let normalized_right = if matches!(op, BinaryOp::Shl | BinaryOp::Shr) {
                cast_if_needed(lowered_right, Some(named_int_type(*span)), *span)
            } else {
                lowered_right
            };

            Expr::Binary {
                left: Box::new(normalized_left),
                op: *op,
                right: Box::new(normalized_right),
                span: *span,
            }
        }
        Expr::Unary { op, operand, span } => {
            let operand_ty = infer_expr_type(operand, ctx);
            let lowered_operand = lower_expr_memory_with_ctx(operand, ctx);
            let normalized_operand = match op {
                UnaryOp::Neg | UnaryOp::BitNot => cast_if_needed(
                    lowered_operand,
                    operand_ty
                        .as_ref()
                        .and_then(|ty| promoted_type_for_arithmetic(ty, ctx.layouts.abi)),
                    *span,
                ),
                _ => lowered_operand,
            };
            Expr::Unary {
                op: *op,
                operand: Box::new(normalized_operand),
                span: *span,
            }
        }
        Expr::Call { callee, args, span } => Expr::Call {
            callee: Box::new(lower_expr_memory_with_ctx(callee, ctx)),
            args: args
                .iter()
                .map(|arg| CallArg {
                    value: lower_expr_memory_with_ctx(&arg.value, ctx),
                    ..arg.clone()
                })
                .collect(),
            span: *span,
        },
        Expr::MethodCall {
            receiver,
            method,
            args,
            span,
        } => Expr::MethodCall {
            receiver: Box::new(lower_expr_memory_with_ctx(receiver, ctx)),
            method: method.clone(),
            args: args
                .iter()
                .map(|arg| CallArg {
                    value: lower_expr_memory_with_ctx(&arg.value, ctx),
                    ..arg.clone()
                })
                .collect(),
            span: *span,
        },
        Expr::Field {
            object,
            field,
            span,
        } => {
            if let Some(field_layout) = field_layout_from_object(object, field, ctx) {
                let field_offset = field_layout.offset;
                let field_bit_width = field_layout.bit_width;
                let field_bit_offset = field_layout.bit_offset.unwrap_or(0);
                let field_bit_signed = field_layout.bit_signed;
                if let Some(bit_width) = field_bit_width {
                    let promoted_bits =
                        promoted_integer_bits(bit_width, field_bit_signed, ctx.layouts.abi);
                    return helper_call(
                        "__kain_bitfield_get",
                        vec![
                            lower_expr_memory_with_ctx(object, ctx),
                            Expr::String(field.clone(), *span),
                            Expr::Int(field_offset as i64, *span),
                            Expr::Int(field_bit_offset as i64, *span),
                            Expr::Int(bit_width as i64, *span),
                            Expr::Bool(field_bit_signed, *span),
                            Expr::Int(promoted_bits as i64, *span),
                        ],
                        *span,
                    );
                }
            }
            if let Some(layout_size) = struct_layout_from_object(object, ctx)
                .filter(|layout| layout.is_union)
                .map(|layout| layout.size)
            {
                let field_ty = infer_expr_type(object, ctx)
                    .and_then(|object_ty| field_type_from_object(&object_ty, field, ctx));
                let fallback = infer_expr_type(object, ctx)
                    .and_then(|object_ty| field_type_from_object(&object_ty, field, ctx))
                    .map(|ty| lower_storage_expr(&ty, ctx.layouts, *span, true))
                    .unwrap_or_else(|| Expr::None(*span));
                return helper_call(
                    "__kain_union_get",
                    vec![
                        lower_expr_memory_with_ctx(object, ctx),
                        Expr::String(field.clone(), *span),
                        Expr::String(
                            field_ty.as_ref().map(type_key).unwrap_or_else(|| "unknown".to_string()),
                            *span,
                        ),
                        Expr::Int(field_ty.as_ref().and_then(|ty| memory_stride_for_type(Some(ty), ctx.layouts)).unwrap_or(1), *span),
                        Expr::Int(layout_size as i64, *span),
                        fallback,
                    ],
                    *span,
                );
            }
            Expr::Field {
                object: Box::new(lower_expr_memory_with_ctx(object, ctx)),
                field: field.clone(),
                span: *span,
            }
        }
        Expr::Index {
            object,
            index,
            span,
        } => Expr::Index {
            object: Box::new(lower_expr_memory_with_ctx(object, ctx)),
            index: Box::new(lower_expr_memory_with_ctx(index, ctx)),
            span: *span,
        },
        Expr::Array(items, items_span) => Expr::Array(
            items.iter().map(|item| lower_expr_memory_with_ctx(item, ctx)).collect(),
            *items_span,
        ),
        Expr::Tuple(items, items_span) => Expr::Tuple(
            items.iter().map(|item| lower_expr_memory_with_ctx(item, ctx)).collect(),
            *items_span,
        ),
        Expr::Struct {
            name,
            fields,
            span,
        } => Expr::Struct {
            name: name.clone(),
            fields: fields
                .iter()
                .map(|(field, value)| (field.clone(), lower_expr_memory_with_ctx(value, ctx)))
                .collect(),
            span: *span,
        },
        Expr::EnumVariant {
            enum_name,
            variant,
            fields,
            span,
        } => Expr::EnumVariant {
            enum_name: enum_name.clone(),
            variant: variant.clone(),
            fields: match fields {
                EnumVariantFields::Unit => EnumVariantFields::Unit,
                EnumVariantFields::Tuple(items) => EnumVariantFields::Tuple(
                    items.iter().map(|item| lower_expr_memory_with_ctx(item, ctx)).collect(),
                ),
                EnumVariantFields::Struct(items) => EnumVariantFields::Struct(
                    items
                        .iter()
                        .map(|(field, value)| {
                            (field.clone(), lower_expr_memory_with_ctx(value, ctx))
                        })
                        .collect(),
                ),
            },
            span: *span,
        },
        Expr::If {
            condition,
            then_branch,
            else_branch,
            span,
        } => Expr::If {
            condition: Box::new(lower_expr_memory_with_ctx(condition, ctx)),
            then_branch: lower_block_memory_with_ctx(then_branch, ctx),
            else_branch: else_branch
                .as_ref()
                .map(|branch| Box::new(lower_else_branch_memory_with_ctx(branch, ctx))),
            span: *span,
        },
        Expr::Match {
            scrutinee,
            arms,
            span,
        } => Expr::Match {
            scrutinee: Box::new(lower_expr_memory_with_ctx(scrutinee, ctx)),
            arms: arms
                .iter()
                .map(|arm| MatchArm {
                    body: lower_expr_memory_with_ctx(&arm.body, ctx),
                    ..arm.clone()
                })
                .collect(),
            span: *span,
        },
        Expr::Lambda {
            params,
            return_type,
            body,
            span,
        } => Expr::Lambda {
            params: params
                .iter()
                .map(|param| Param {
                    ty: lower_type_memory(&param.ty),
                    ..param.clone()
                })
                .collect(),
            return_type: return_type.as_ref().map(lower_type_memory),
            body: Box::new(lower_expr_memory_with_ctx(body, ctx)),
            span: *span,
        },
        Expr::Cast {
            value,
            target,
            span,
        } => Expr::Cast {
            value: Box::new(lower_expr_memory_with_ctx(value, ctx)),
            target: lower_type_memory(target),
            span: *span,
        },
        Expr::Try(inner, inner_span) => {
            Expr::Try(Box::new(lower_expr_memory_with_ctx(inner, ctx)), *inner_span)
        }
        Expr::Await(inner, inner_span) => {
            Expr::Await(Box::new(lower_expr_memory_with_ctx(inner, ctx)), *inner_span)
        }
        Expr::Block(block, block_span) => {
            Expr::Block(lower_block_memory_with_ctx(block, ctx), *block_span)
        }
        Expr::Paren(inner, inner_span) => {
            Expr::Paren(Box::new(lower_expr_memory_with_ctx(inner, ctx)), *inner_span)
        }
        Expr::Return(Some(inner), inner_span) => Expr::Return(
            Some(Box::new(lower_expr_memory_with_ctx(inner, ctx))),
            *inner_span,
        ),
        Expr::Break(Some(inner), inner_span) => Expr::Break(
            Some(Box::new(lower_expr_memory_with_ctx(inner, ctx))),
            *inner_span,
        ),
        Expr::Spawn { actor, init, span } => Expr::Spawn {
            actor: actor.clone(),
            init: init
                .iter()
                .map(|(name, value)| (name.clone(), lower_expr_memory_with_ctx(value, ctx)))
                .collect(),
            span: *span,
        },
        Expr::SendMsg {
            target,
            message,
            data,
            span,
        } => Expr::SendMsg {
            target: Box::new(lower_expr_memory_with_ctx(target, ctx)),
            message: message.clone(),
            data: data
                .iter()
                .map(|(name, value)| (name.clone(), lower_expr_memory_with_ctx(value, ctx)))
                .collect(),
            span: *span,
        },
        Expr::Comptime(inner, inner_span) => Expr::Comptime(
            Box::new(lower_expr_memory_with_ctx(inner, ctx)),
            *inner_span,
        ),
        Expr::MacroCall { name, args, span } => Expr::MacroCall {
            name: name.clone(),
            args: args
                .iter()
                .map(|arg| lower_expr_memory_with_ctx(arg, ctx))
                .collect(),
            span: *span,
        },
        Expr::Range {
            start,
            end,
            inclusive,
            span,
        } => Expr::Range {
            start: start
                .as_ref()
                .map(|expr| Box::new(lower_expr_memory_with_ctx(expr, ctx))),
            end: end
                .as_ref()
                .map(|expr| Box::new(lower_expr_memory_with_ctx(expr, ctx))),
            inclusive: *inclusive,
            span: *span,
        },
        Expr::FString(items, items_span) => Expr::FString(
            items.iter().map(|item| lower_expr_memory_with_ctx(item, ctx)).collect(),
            *items_span,
        ),
        _ => expr.clone(),
    }
}

fn lower_else_branch_memory_with_ctx(
    branch: &ElseBranch,
    ctx: &mut FunctionMemoryCtx<'_>,
) -> ElseBranch {
    match branch {
        ElseBranch::Else(block) => ElseBranch::Else(lower_block_memory_with_ctx(block, ctx)),
        ElseBranch::ElseIf(condition, block, next) => ElseBranch::ElseIf(
            Box::new(lower_expr_memory_with_ctx(condition, ctx)),
            lower_block_memory_with_ctx(block, ctx),
            next.as_ref()
                .map(|branch| Box::new(lower_else_branch_memory_with_ctx(branch, ctx))),
        ),
    }
}

fn should_load_from_pointer(name: &str, ctx: &FunctionMemoryCtx<'_>) -> bool {
    ctx.address_taken.contains(name) && ctx.local_types.contains_key(name)
}

fn load_from_pointer_binding(name: &str, ty: Option<&Type>, span: Span) -> Expr {
    let call = helper_call(
        "__kain_mem_load",
        vec![Expr::Ident(pointer_binding_name(name), span)],
        span,
    );
    if let Some(ty) = ty {
        Expr::Cast {
            value: Box::new(call),
            target: lower_type_memory(ty),
            span,
        }
    } else {
        call
    }
}

fn pointer_binding_name(name: &str) -> String {
    format!("__kain_ptr_{}", name)
}

fn make_pointer_binding_stmt(name: &str, span: Span) -> Stmt {
    Stmt::Let {
        pattern: Pattern::Binding {
            name: pointer_binding_name(name),
            mutable: true,
            span,
        },
        ty: Some(Type::Named {
            name: "Int".to_string(),
            generics: vec![],
            span,
        }),
        value: Some(helper_call(
            "__kain_bind_local",
            vec![Expr::Ident(name.to_string(), span)],
            span,
        )),
        span,
    }
}

fn lower_addr_of_memory(
    value: &Expr,
    pointee_ty: Option<&Type>,
    ctx: &mut FunctionMemoryCtx<'_>,
    span: Span,
) -> Expr {
    if let Some(ptr) = pointer_for_addressable(value, ctx) {
        ptr
    } else {
        let mut args = vec![lower_expr_memory_with_ctx(value, ctx)];
        if let Some(ty) = pointee_ty {
            args.push(Expr::String(type_key(ty), span));
        }
        helper_call("__kain_addr_of", args, span)
    }
}

fn pointer_for_addressable(value: &Expr, ctx: &mut FunctionMemoryCtx<'_>) -> Option<Expr> {
    match value {
        Expr::Ident(name, span) if should_load_from_pointer(name, ctx) => {
            Some(Expr::Ident(pointer_binding_name(name), *span))
        }
        Expr::Field {
            object,
            field,
            span,
        } => {
            let base_ptr = pointer_for_addressable(object, ctx)?;
            if let Some(layout) = field_layout_from_object(object, field, ctx) {
                if let Some(bit_width) = layout.bit_width {
                    let _bit_offset = layout.bit_offset.unwrap_or(0);
                    let _bit_width = bit_width;
                    return None;
                }
            }
            let offset = infer_field_offset(object, field, ctx).unwrap_or(0);
            Some(helper_call(
                "__kain_field_ptr",
                vec![
                    base_ptr,
                    Expr::String(field.clone(), *span),
                    Expr::Int(offset as i64, *span),
                ],
                *span,
            ))
        }
        Expr::Index {
            object,
            index,
            span,
        } => {
            let base_ptr = pointer_for_addressable(object, ctx)?;
            let stride = infer_element_type(object, ctx)
                .as_ref()
                .and_then(|ty| memory_stride_for_type(Some(ty), ctx.layouts))
                .unwrap_or(1);
            Some(helper_call(
                "__kain_index_ptr",
                vec![
                    base_ptr,
                    lower_expr_memory_with_ctx(index, ctx),
                    Expr::Int(stride, *span),
                ],
                *span,
            ))
        }
        _ => None,
    }
}

fn collect_local_types_from_block(block: &Block) -> HashMap<String, Type> {
    let mut locals = HashMap::new();
    collect_local_types_from_block_into(block, &mut locals);
    locals
}

fn collect_local_types_from_block_into(block: &Block, locals: &mut HashMap<String, Type>) {
    for stmt in &block.stmts {
        match stmt {
            Stmt::Let {
                pattern: Pattern::Binding { name, .. },
                ty: Some(ty),
                ..
            } => {
                locals.insert(name.clone(), ty.clone());
            }
            Stmt::For { body, .. }
            | Stmt::While { body, .. }
            | Stmt::Loop { body, .. } => collect_local_types_from_block_into(body, locals),
            Stmt::Expr(Expr::Block(block, _)) => collect_local_types_from_block_into(block, locals),
            _ => {}
        }
    }
}

fn collect_address_taken_roots(block: &Block) -> HashSet<String> {
    let mut roots = HashSet::new();
    collect_address_taken_from_block(block, &mut roots);
    roots
}

fn collect_address_taken_from_block(block: &Block, roots: &mut HashSet<String>) {
    for stmt in &block.stmts {
        match stmt {
            Stmt::Expr(expr)
            | Stmt::Return(Some(expr), _)
            | Stmt::Break(Some(expr), _) => collect_address_taken_from_expr(expr, roots),
            Stmt::Let { value: Some(expr), .. } => collect_address_taken_from_expr(expr, roots),
            Stmt::For { iter, body, .. } => {
                collect_address_taken_from_expr(iter, roots);
                collect_address_taken_from_block(body, roots);
            }
            Stmt::While {
                condition, body, ..
            } => {
                collect_address_taken_from_expr(condition, roots);
                collect_address_taken_from_block(body, roots);
            }
            Stmt::Loop { body, .. } => collect_address_taken_from_block(body, roots),
            _ => {}
        }
    }
}

fn collect_address_taken_from_expr(expr: &Expr, roots: &mut HashSet<String>) {
    match expr {
        Expr::AddrOf { value, .. } => {
            if let Some(root) = root_ident_of_addressable(value) {
                roots.insert(root.to_string());
            }
            collect_address_taken_from_expr(value, roots);
        }
        Expr::Binary { left, right, .. } => {
            collect_address_taken_from_expr(left, roots);
            collect_address_taken_from_expr(right, roots);
        }
        Expr::Unary { operand, .. } => collect_address_taken_from_expr(operand, roots),
        Expr::Call { callee, args, .. } => {
            collect_address_taken_from_expr(callee, roots);
            for arg in args {
                collect_address_taken_from_expr(&arg.value, roots);
            }
        }
        Expr::MethodCall {
            receiver, args, ..
        } => {
            collect_address_taken_from_expr(receiver, roots);
            for arg in args {
                collect_address_taken_from_expr(&arg.value, roots);
            }
        }
        Expr::Field { object, .. } => collect_address_taken_from_expr(object, roots),
        Expr::Index { object, index, .. } => {
            collect_address_taken_from_expr(object, roots);
            collect_address_taken_from_expr(index, roots);
        }
        Expr::Assign { target, value, .. } => {
            collect_address_taken_from_expr(target, roots);
            collect_address_taken_from_expr(value, roots);
        }
        Expr::Block(block, _) => collect_address_taken_from_block(block, roots),
        Expr::Paren(inner, _)
        | Expr::Try(inner, _)
        | Expr::Await(inner, _)
        | Expr::Comptime(inner, _) => collect_address_taken_from_expr(inner, roots),
        _ => {}
    }
}

fn root_ident_of_addressable(expr: &Expr) -> Option<&str> {
    match expr {
        Expr::Ident(name, _) => Some(name),
        Expr::Field { object, .. } | Expr::Index { object, .. } => root_ident_of_addressable(object),
        _ => None,
    }
}

fn infer_expr_type(expr: &Expr, ctx: &FunctionMemoryCtx<'_>) -> Option<Type> {
    match expr {
        Expr::Int(_, span) => Some(Type::Named {
            name: "Int".to_string(),
            generics: Vec::new(),
            span: *span,
        }),
        Expr::Float(_, span) => Some(Type::Named {
            name: "Float".to_string(),
            generics: Vec::new(),
            span: *span,
        }),
        Expr::Bool(_, span) => Some(Type::Named {
            name: "Bool".to_string(),
            generics: Vec::new(),
            span: *span,
        }),
        Expr::Ident(name, _) => ctx.local_types.get(name).cloned(),
        Expr::Field { object, field, .. } => {
            let object_ty = infer_expr_type(object, ctx)?;
            field_type_from_object(&object_ty, field, ctx)
        }
        Expr::Index { object, .. } => infer_element_type(object, ctx),
        Expr::Cast { target, .. } => Some(target.clone()),
        _ => None,
    }
}

fn infer_element_type(expr: &Expr, ctx: &FunctionMemoryCtx<'_>) -> Option<Type> {
    match infer_expr_type(expr, ctx)? {
        Type::Array(inner, _, _)
        | Type::Slice(inner, _)
        | Type::Ref { inner, .. }
        | Type::Ptr { inner, .. }
        | Type::Option(inner, _) => Some(*inner),
        other => Some(other),
    }
}

fn field_type_from_object(
    object_ty: &Type,
    field: &str,
    ctx: &FunctionMemoryCtx<'_>,
) -> Option<Type> {
    match object_ty {
        Type::Named { name, .. } => ctx.layouts.field(name, field).map(|field| field.ty.clone()),
        Type::Ref { inner, .. } | Type::Ptr { inner, .. } => {
            field_type_from_object(inner, field, ctx)
        }
        _ => None,
    }
}

fn field_layout_from_object_ty<'a>(
    object_ty: &Type,
    field: &str,
    ctx: &'a FunctionMemoryCtx<'_>,
) -> Option<&'a LayoutField> {
    match object_ty {
        Type::Named { name, .. } => ctx.layouts.field(name, field),
        Type::Ref { inner, .. } | Type::Ptr { inner, .. } => {
            field_layout_from_object_ty(inner, field, ctx)
        }
        _ => None,
    }
}

fn field_layout_from_object<'a>(
    object: &Expr,
    field: &str,
    ctx: &'a FunctionMemoryCtx<'_>,
) -> Option<&'a LayoutField> {
    let object_ty = infer_expr_type(object, ctx)?;
    field_layout_from_object_ty(&object_ty, field, ctx)
}

fn struct_layout_from_type<'a>(
    ty: &Type,
    ctx: &'a FunctionMemoryCtx<'_>,
) -> Option<&'a StructLayoutInfo> {
    match ty {
        Type::Named { name, .. } => ctx.layouts.structs.get(name),
        Type::Ref { inner, .. } | Type::Ptr { inner, .. } => struct_layout_from_type(inner, ctx),
        _ => None,
    }
}

fn struct_layout_from_object<'a>(
    object: &Expr,
    ctx: &'a FunctionMemoryCtx<'_>,
) -> Option<&'a StructLayoutInfo> {
    let object_ty = infer_expr_type(object, ctx)?;
    struct_layout_from_type(&object_ty, ctx)
}

fn infer_field_offset(object: &Expr, field: &str, ctx: &FunctionMemoryCtx<'_>) -> Option<usize> {
    field_layout_from_object(object, field, ctx).map(|field| field.offset)
}

fn type_key(ty: &Type) -> String {
    match ty {
        Type::Named { name, generics, .. } if generics.is_empty() => name.clone(),
        Type::Named { name, generics, .. } => format!(
            "{}<{}>",
            name,
            generics.iter().map(type_key).collect::<Vec<_>>().join(",")
        ),
        Type::Array(inner, size, _) => format!("[{};{}]", type_key(inner), size),
        Type::Slice(inner, _) => format!("[{}]", type_key(inner)),
        Type::Ref { inner, .. } => format!("&{}", type_key(inner)),
        Type::Ptr { inner, .. } => format!("ptr<{}>", type_key(inner)),
        Type::Tuple(types, _) => format!(
            "({})",
            types.iter().map(type_key).collect::<Vec<_>>().join(",")
        ),
        Type::Unit(_) => "()".to_string(),
        Type::Never(_) => "!".to_string(),
        _ => "unknown".to_string(),
    }
}

fn cast_if_needed(expr: Expr, target: Option<Type>, span: Span) -> Expr {
    let Some(target) = target else {
        return expr;
    };
    Expr::Cast {
        value: Box::new(expr),
        target,
        span,
    }
}

fn named_int_type(span: Span) -> Type {
    Type::Named {
        name: "Int".to_string(),
        generics: Vec::new(),
        span,
    }
}

fn lower_type_memory(ty: &Type) -> Type {
    match ty {
        Type::Ptr { span, .. } => Type::Named {
            name: "Int".to_string(),
            generics: vec![],
            span: *span,
        },
        Type::Array(inner, size, span) => Type::Array(Box::new(lower_type_memory(inner)), *size, *span),
        Type::Slice(inner, span) => Type::Slice(Box::new(lower_type_memory(inner)), *span),
        Type::Tuple(types, span) => Type::Tuple(types.iter().map(lower_type_memory).collect(), *span),
        Type::Ref {
            mutable,
            inner,
            lifetime,
            span,
            ..
        } => Type::Ref {
            mutable: *mutable,
            inner: Box::new(lower_type_memory(inner)),
            lifetime: lifetime.clone(),
            span: *span,
        },
        Type::Function {
            params,
            return_type,
            effects,
            span,
        } => Type::Function {
            params: params.iter().map(lower_type_memory).collect(),
            return_type: Box::new(lower_type_memory(return_type)),
            effects: effects.clone(),
            span: *span,
        },
        Type::Option(inner, span) => Type::Option(Box::new(lower_type_memory(inner)), *span),
        Type::Result(ok, err, span) => Type::Result(
            Box::new(lower_type_memory(ok)),
            Box::new(lower_type_memory(err)),
            *span,
        ),
        Type::Named { name, generics, span } => Type::Named {
            name: name.clone(),
            generics: generics.iter().map(lower_type_memory).collect(),
            span: *span,
        },
        Type::Impl {
            trait_name,
            generics,
            span,
        } => Type::Impl {
            trait_name: trait_name.clone(),
            generics: generics.iter().map(lower_type_memory).collect(),
            span: *span,
        },
        _ => ty.clone(),
    }
}

fn lower_resolved_type_memory(ty: &ResolvedType) -> ResolvedType {
    match ty {
        ResolvedType::Ptr { .. } => ResolvedType::Int(crate::types::IntSize::I64),
        ResolvedType::Array(inner, size) => {
            ResolvedType::Array(Box::new(lower_resolved_type_memory(inner)), *size)
        }
        ResolvedType::Slice(inner) => ResolvedType::Slice(Box::new(lower_resolved_type_memory(inner))),
        ResolvedType::Tuple(types) => {
            ResolvedType::Tuple(types.iter().map(lower_resolved_type_memory).collect())
        }
        ResolvedType::Option(inner) => {
            ResolvedType::Option(Box::new(lower_resolved_type_memory(inner)))
        }
        ResolvedType::Result(ok, err) => ResolvedType::Result(
            Box::new(lower_resolved_type_memory(ok)),
            Box::new(lower_resolved_type_memory(err)),
        ),
        ResolvedType::Ref { mutable, inner } => ResolvedType::Ref {
            mutable: *mutable,
            inner: Box::new(lower_resolved_type_memory(inner)),
        },
        ResolvedType::Function { params, ret, effects } => ResolvedType::Function {
            params: params.iter().map(lower_resolved_type_memory).collect(),
            ret: Box::new(lower_resolved_type_memory(ret)),
            effects: effects.clone(),
        },
        ResolvedType::Struct(name, fields) => ResolvedType::Struct(
            name.clone(),
            fields
                .iter()
                .map(|(field, ty)| (field.clone(), lower_resolved_type_memory(ty)))
                .collect(),
        ),
        ResolvedType::Enum(name, variants) => ResolvedType::Enum(
            name.clone(),
            variants
                .iter()
                .map(|(variant, ty)| (variant.clone(), lower_resolved_type_memory(ty)))
                .collect(),
        ),
        _ => ty.clone(),
    }
}

fn helper_call(name: &str, args: Vec<Expr>, span: Span) -> Expr {
    Expr::Call {
        callee: Box::new(Expr::Ident(name.to_string(), span)),
        args: args
            .into_iter()
            .map(|value| CallArg {
                name: None,
                value,
                span,
            })
            .collect(),
        span,
    }
}

fn memory_stride_for_type(ty: Option<&Type>, layouts: &LayoutRegistry) -> Option<i64> {
    ty.map(|ty| estimate_type_size(ty, layouts))
        .and_then(|size| i64::try_from(size).ok())
}

fn estimate_type_size(ty: &Type, layouts: &LayoutRegistry) -> usize {
    match ty {
        Type::Named { name, .. } => match name.as_str() {
            "Bool" | "Char" => 1,
            "Int" | "isize" | "usize" => 8,
            "Float" => 8,
            _ => layouts.type_size_fallback(ty),
        },
        Type::Array(inner, size, _) => estimate_type_size(inner, layouts) * size,
        Type::Slice(_, _) => 16,
        Type::Tuple(types, _) => types.iter().map(|ty| estimate_type_size(ty, layouts)).sum(),
        Type::Ref { .. } | Type::Ptr { .. } => 8,
        Type::Option(inner, _) => estimate_type_size(inner, layouts),
        Type::Result(ok, err, _) => {
            estimate_type_size(ok, layouts).max(estimate_type_size(err, layouts))
        }
        Type::Unit(_) | Type::Never(_) => 0,
        _ => 8,
    }
}

fn estimate_type_align(ty: &Type, layouts: &LayoutRegistry) -> usize {
    match ty {
        Type::Named { name, .. } => match layouts.structs.get(name) {
            Some(info) => info.align.max(1),
            None => layouts.type_align_fallback(ty),
        },
        Type::Array(inner, _, _) => estimate_type_align(inner, layouts),
        Type::Tuple(types, _) => types
            .iter()
            .map(|ty| estimate_type_align(ty, layouts))
            .max()
            .unwrap_or(1),
        Type::Option(inner, _) => estimate_type_align(inner, layouts),
        Type::Result(ok, err, _) => estimate_type_align(ok, layouts).max(estimate_type_align(err, layouts)),
        _ => layouts.type_align_fallback(ty),
    }
}

fn lower_storage_expr(ty: &Type, layouts: &LayoutRegistry, span: Span, zeroed: bool) -> Expr {
    match ty {
        Type::Array(inner, count, _) => Expr::Array(
            (0..*count)
                .map(|_| lower_storage_expr(inner, layouts, span, zeroed))
                .collect(),
            span,
        ),
        Type::Tuple(items, _) => Expr::Tuple(
            items
                .iter()
                .map(|item| lower_storage_expr(item, layouts, span, zeroed))
                .collect(),
            span,
        ),
        Type::Named { name, .. } if name == "Int" || name == "UInt" || name == "isize" || name == "usize" => {
            if zeroed { Expr::Int(0, span) } else { Expr::None(span) }
        }
        Type::Named { name, .. } if name == "Float" => {
            if zeroed { Expr::Float(0.0, span) } else { Expr::None(span) }
        }
        Type::Named { name, .. } if name == "Bool" => {
            if zeroed { Expr::Bool(false, span) } else { Expr::None(span) }
        }
        Type::Named { name, .. } if name == "Char" => {
            if zeroed { Expr::String("\0".to_string(), span) } else { Expr::None(span) }
        }
        Type::Unit(_) => Expr::Tuple(Vec::new(), span),
        Type::Named { name, .. } if layouts.structs.contains_key(name) => {
            let fields = layouts
                .structs
                .get(name)
                .map(|info| {
                    info.field_order
                        .iter()
                        .filter_map(|field_name| {
                            info.fields.get(field_name).map(|field| {
                                (
                                    field_name.clone(),
                                    lower_storage_expr(&field.ty, layouts, span, zeroed),
                                )
                            })
                        })
                        .collect::<Vec<_>>()
                })
                .unwrap_or_default();
            Expr::Struct {
                name: name.clone(),
                fields,
                span,
            }
        }
        Type::Ref { .. } | Type::Ptr { .. } => Expr::None(span),
        _ => Expr::None(span),
    }
}

fn storage_seed_expr(ty: Option<&Type>, layouts: &LayoutRegistry, span: Span, zeroed: bool) -> Expr {
    ty.map(|ty| lower_storage_expr(ty, layouts, span, zeroed))
        .unwrap_or_else(|| if zeroed { Expr::Int(0, span) } else { Expr::None(span) })
}

fn lower_heap_alloc_expr(
    size: &Expr,
    ty: Option<&Type>,
    zeroed: bool,
    layouts: &LayoutRegistry,
    span: Span,
) -> Expr {
    helper_call(
        "__kain_alloc",
        vec![
            size.clone(),
            Expr::Int(memory_stride_for_type(ty, layouts).unwrap_or(1), span),
            Expr::Bool(zeroed, span),
            storage_seed_expr(ty, layouts, span, zeroed),
        ],
        span,
    )
}

fn lower_aggregate_init_expr(
    ty: &Type,
    fields: &[(String, Expr)],
    zero_fill_rest: bool,
    layouts: &LayoutRegistry,
    span: Span,
) -> Expr {
    match ty {
        Type::Named { name, .. } if layouts.structs.contains_key(name) => {
            let provided: HashMap<String, Expr> = fields.iter().cloned().collect();
            let field_values = layouts
                .structs
                .get(name)
                .map(|info| {
                    if info.is_union {
                        let (field_values, active_field, active_value) = lower_union_aggregate_fields(
                            info,
                            fields,
                            &provided,
                            layouts,
                            span,
                            zero_fill_rest,
                        );
                        let base_struct = Expr::Struct {
                            name: name.clone(),
                            fields: field_values,
                            span,
                        };
                        return if let Some(active_field) = active_field {
                            let active_ty = info
                                .fields
                                .get(&active_field)
                                .map(|field| field.ty.clone());
                            helper_call(
                                "__kain_union_wrap",
                                vec![
                                    base_struct,
                                    Expr::String(active_field, span),
                                    Expr::String(
                                        active_ty.as_ref().map(type_key).unwrap_or_else(|| "unknown".to_string()),
                                        span,
                                    ),
                                    Expr::Int(active_ty.as_ref().and_then(|ty| memory_stride_for_type(Some(ty), layouts)).unwrap_or(1), span),
                                    Expr::Int(info.size as i64, span),
                                    active_value,
                                ],
                                span,
                            )
                        } else {
                            base_struct
                        };
                    } else {
                        let field_values = info.field_order
                            .iter()
                            .filter_map(|field_name| {
                                info.fields.get(field_name).map(|field| {
                                    let value = provided.get(field_name).cloned().unwrap_or_else(|| {
                                        if zero_fill_rest {
                                            lower_storage_expr(&field.ty, layouts, span, true)
                                        } else {
                                            Expr::None(span)
                                        }
                                    });
                                    (field_name.clone(), value)
                                })
                            })
                            .collect::<Vec<_>>();
                        return Expr::Struct {
                            name: name.clone(),
                            fields: field_values,
                            span,
                        };
                    }
                })
                .unwrap_or_else(|| Expr::Struct {
                    name: name.clone(),
                    fields: fields.to_vec(),
                    span,
                });
            field_values
        }
        _ => Expr::Tuple(
            fields.iter().map(|(_, value)| value.clone()).collect(),
            span,
        ),
    }
}

fn lower_union_aggregate_fields(
    info: &StructLayoutInfo,
    original_fields: &[(String, Expr)],
    provided: &HashMap<String, Expr>,
    layouts: &LayoutRegistry,
    span: Span,
    zero_fill_rest: bool,
) -> (Vec<(String, Expr)>, Option<String>, Expr) {
    let active_name = original_fields
        .iter()
        .rev()
        .find_map(|(field_name, _)| info.fields.contains_key(field_name).then(|| field_name.clone()))
        .or_else(|| info.field_order.first().cloned());

    let Some(active_name) = active_name else {
        return (Vec::new(), None, Expr::None(span));
    };

    let active_value = info
        .fields
        .get(&active_name)
        .map(|field| {
            provided.get(&active_name).cloned().unwrap_or_else(|| {
                if zero_fill_rest {
                    lower_storage_expr(&field.ty, layouts, span, true)
                } else {
                    Expr::None(span)
                }
            })
        })
        .unwrap_or_else(|| Expr::None(span));

    let field_values = info
        .field_order
        .iter()
        .filter_map(|field_name| {
            info.fields.get(field_name).map(|field| {
                let value = if field_name == &active_name {
                    provided.get(field_name).cloned().unwrap_or_else(|| {
                        if zero_fill_rest {
                            lower_storage_expr(&field.ty, layouts, span, true)
                        } else {
                            Expr::None(span)
                        }
                    })
                } else if zero_fill_rest {
                    lower_storage_expr(&field.ty, layouts, span, true)
                } else {
                    Expr::None(span)
                };
                (field_name.clone(), value)
            })
        })
        .collect::<Vec<_>>();

    (field_values, Some(active_name), active_value)
}

fn first_unsupported_memory_context(
    items: &[TypedItem],
    caps: BackendMemoryCapabilities,
) -> Option<String> {
    for item in items {
        match item {
            TypedItem::Function(f) => {
                if let Some(context) = first_unsupported_memory_context_in_function(f, caps) {
                    return Some(context);
                }
            }
            TypedItem::Struct(s) => {
                if !caps.raw_pointers {
                    for field in &s.ast.fields {
                        if field.ty.contains_raw_ptr() {
                            return Some(format!(
                                "Struct '{}' field '{}' uses a raw pointer type",
                                s.ast.name, field.name
                            ));
                        }
                    }
                }
            }
            TypedItem::Component(c) => {
                if !caps.raw_pointers {
                    for prop in &c.ast.props {
                        if prop.ty.contains_raw_ptr() {
                            return Some(format!(
                                "Component '{}' prop '{}' uses a raw pointer type",
                                c.ast.name, prop.name
                            ));
                        }
                    }
                }
            }
            TypedItem::Actor(a) => {
                if !caps.raw_pointers {
                    for state in &a.ast.state {
                        if state.ty.contains_raw_ptr() {
                            return Some(format!(
                                "Actor '{}' state '{}' uses a raw pointer type",
                                a.ast.name, state.name
                            ));
                        }
                    }
                }
            }
            TypedItem::Const(c) => {
                if !caps.raw_pointers && c.ast.ty.contains_raw_ptr() {
                    return Some(format!("Const '{}' uses a raw pointer type", c.ast.name));
                }
                if !caps.raw_memory_ops {
                    if let Some(context) = first_memory_expr_context(
                        &c.ast.value,
                        format!("Const '{}' contains a raw memory operation", c.ast.name),
                    ) {
                        return Some(context);
                    }
                }
            }
            TypedItem::TypeAlias(alias) => {
                if !caps.raw_pointers && alias.ast.target.contains_raw_ptr() {
                    return Some(format!(
                        "Type alias '{}' uses a raw pointer type",
                        alias.ast.name
                    ));
                }
            }
            TypedItem::Impl(imp) => {
                for method in &imp.ast.methods {
                    if let Some(context) =
                        first_unsupported_memory_context_in_ast_function(method, caps)
                    {
                        return Some(format!(
                            "Impl method '{}': {}",
                            method.name, context
                        ));
                    }
                }
            }
            _ => {}
        }
    }

    None
}

fn first_unsupported_memory_context_in_function(
    function: &crate::types::TypedFunction,
    caps: BackendMemoryCapabilities,
) -> Option<String> {
    if !caps.raw_pointers {
        for param in &function.ast.params {
            if param.ty.contains_raw_ptr() {
                return Some(format!(
                    "Function '{}' parameter '{}' uses a raw pointer type",
                    function.ast.name, param.name
                ));
            }
        }
        if let Some(ret) = &function.ast.return_type {
            if ret.contains_raw_ptr() {
                return Some(format!(
                    "Function '{}' return type uses a raw pointer type",
                    function.ast.name
                ));
            }
        }
    }

    if !caps.raw_memory_ops {
        if let Some(context) = first_memory_block_context(
            &function.ast.body,
            format!("Function '{}'", function.ast.name),
        ) {
            return Some(context);
        }
    }

    None
}

fn first_unsupported_memory_context_in_ast_function(
    function: &crate::ast::Function,
    caps: BackendMemoryCapabilities,
) -> Option<String> {
    if !caps.raw_pointers {
        for param in &function.params {
            if param.ty.contains_raw_ptr() {
                return Some(format!(
                    "Function '{}' parameter '{}' uses a raw pointer type",
                    function.name, param.name
                ));
            }
        }
        if let Some(ret) = &function.return_type {
            if ret.contains_raw_ptr() {
                return Some(format!(
                    "Function '{}' return type uses a raw pointer type",
                    function.name
                ));
            }
        }
    }

    if !caps.raw_memory_ops {
        if let Some(context) =
            first_memory_block_context(&function.body, format!("Function '{}'", function.name))
        {
            return Some(context);
        }
    }

    None
}

fn first_memory_block_context(block: &Block, owner: String) -> Option<String> {
    for stmt in &block.stmts {
        if let Some(context) = first_memory_stmt_context(stmt, &owner) {
            return Some(context);
        }
    }
    None
}

fn first_memory_stmt_context(stmt: &Stmt, owner: &str) -> Option<String> {
    match stmt {
        Stmt::Expr(expr) => first_memory_expr_context(expr, format!("{owner} contains a raw memory operation")),
        Stmt::Let { value, .. } => value
            .as_ref()
            .and_then(|value| first_memory_expr_context(value, format!("{owner} contains a raw memory operation"))),
        Stmt::Return(Some(expr), _) => {
            first_memory_expr_context(expr, format!("{owner} return contains a raw memory operation"))
        }
        Stmt::For { iter, body, .. } => {
            first_memory_expr_context(iter, format!("{owner} loop iterator contains a raw memory operation"))
                .or_else(|| first_memory_block_context(body, owner.to_string()))
        }
        Stmt::While { condition, body, .. } => {
            first_memory_expr_context(condition, format!("{owner} loop condition contains a raw memory operation"))
                .or_else(|| first_memory_block_context(body, owner.to_string()))
        }
        Stmt::Loop { body, .. } => first_memory_block_context(body, owner.to_string()),
        Stmt::Item(_) | Stmt::Return(None, _) | Stmt::Break(_, _) | Stmt::Continue(_) => None,
    }
}

fn first_memory_expr_context(expr: &Expr, base: String) -> Option<String> {
    match expr {
        Expr::AddrOf { .. } => Some(format!("{base}: address-of expression")),
        Expr::PtrOffset { .. } => Some(format!("{base}: pointer offset expression")),
        Expr::MemLoad { .. } => Some(format!("{base}: raw memory load expression")),
        Expr::MemStore { .. } => Some(format!("{base}: raw memory store expression")),
        Expr::SizeOfType { .. }
        | Expr::AlignOfType { .. }
        | Expr::Alloca { .. }
        | Expr::Uninit { .. } => None,
        Expr::Alloc { size, .. } => first_memory_expr_context(size, base),
        Expr::Realloc { pointer, size, .. } => first_memory_expr_context(pointer, base.clone())
            .or_else(|| first_memory_expr_context(size, base)),
        Expr::AggregateInit { fields, .. } => fields
            .iter()
            .find_map(|(_, value)| first_memory_expr_context(value, base.clone())),
        Expr::Binary { left, right, .. } => {
            first_memory_expr_context(left, base.clone()).or_else(|| first_memory_expr_context(right, base))
        }
        Expr::Unary { operand, .. }
        | Expr::Ref { value: operand, .. }
        | Expr::Deref(operand, _)
        | Expr::Try(operand, _)
        | Expr::Await(operand, _)
        | Expr::Comptime(operand, _)
        | Expr::Paren(operand, _) => first_memory_expr_context(operand, base),
        Expr::Cast { value, .. } => first_memory_expr_context(value, base),
        Expr::Call { callee, args, .. } => {
            first_memory_expr_context(callee, base.clone()).or_else(|| {
                args.iter().find_map(|arg| first_memory_expr_context(&arg.value, base.clone()))
            })
        }
        Expr::MethodCall { receiver, args, .. } => {
            first_memory_expr_context(receiver, base.clone()).or_else(|| {
                args.iter().find_map(|arg| first_memory_expr_context(&arg.value, base.clone()))
            })
        }
        Expr::Field { object, .. } => first_memory_expr_context(object, base),
        Expr::Index { object, index, .. } => {
            first_memory_expr_context(object, base.clone()).or_else(|| first_memory_expr_context(index, base))
        }
        Expr::Assign { target, value, .. } => {
            first_memory_expr_context(target, base.clone()).or_else(|| first_memory_expr_context(value, base))
        }
        Expr::Struct { fields, .. } => fields
            .iter()
            .find_map(|(_, value)| first_memory_expr_context(value, base.clone())),
        Expr::EnumVariant { fields, .. } => match fields {
            crate::ast::EnumVariantFields::Unit => None,
            crate::ast::EnumVariantFields::Tuple(items) => items
                .iter()
                .find_map(|value| first_memory_expr_context(value, base.clone())),
            crate::ast::EnumVariantFields::Struct(items) => items
                .iter()
                .find_map(|(_, value)| first_memory_expr_context(value, base.clone())),
        },
        Expr::Array(items, _) | Expr::Tuple(items, _) | Expr::FString(items, _) => items
            .iter()
            .find_map(|value| first_memory_expr_context(value, base.clone())),
        Expr::Range { start, end, .. } => start
            .as_deref()
            .and_then(|expr| first_memory_expr_context(expr, base.clone()))
            .or_else(|| end.as_deref().and_then(|expr| first_memory_expr_context(expr, base))),
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => first_memory_expr_context(condition, base.clone())
            .or_else(|| first_memory_block_context(then_branch, base.clone()))
            .or_else(|| match else_branch.as_deref() {
                Some(crate::ast::ElseBranch::Else(block)) => first_memory_block_context(block, base),
                Some(crate::ast::ElseBranch::ElseIf(cond, block, next)) => {
                    first_memory_expr_context(cond, base.clone())
                        .or_else(|| first_memory_block_context(block, base.clone()))
                        .or_else(|| next.as_deref().and_then(|else_branch| match else_branch {
                            crate::ast::ElseBranch::Else(block) => first_memory_block_context(block, base.clone()),
                            crate::ast::ElseBranch::ElseIf(cond, block, _) => {
                                first_memory_expr_context(cond, base.clone())
                                    .or_else(|| first_memory_block_context(block, base.clone()))
                            }
                        }))
                }
                None => None,
            }),
        Expr::Match { scrutinee, arms, .. } => first_memory_expr_context(scrutinee, base.clone())
            .or_else(|| arms.iter().find_map(|arm| first_memory_expr_context(&arm.body, base.clone()))),
        Expr::Lambda { body, .. } => first_memory_expr_context(body, base),
        Expr::Spawn { init, .. } | Expr::SendMsg { data: init, .. } => init
            .iter()
            .find_map(|(_, value)| first_memory_expr_context(value, base.clone())),
        Expr::Block(block, _) => first_memory_block_context(block, base),
        Expr::MacroCall { args, .. } => args
            .iter()
            .find_map(|arg| first_memory_expr_context(arg, base.clone())),
        Expr::Return(Some(expr), _) | Expr::Break(Some(expr), _) => {
            first_memory_expr_context(expr, base)
        }
        Expr::Int(_, _)
        | Expr::Float(_, _)
        | Expr::String(_, _)
        | Expr::Bool(_, _)
        | Expr::None(_)
        | Expr::Ident(_, _)
        | Expr::Return(None, _)
        | Expr::Break(None, _)
        | Expr::Continue(_)
        | Expr::JSX(_, _) => None,
    }
}

pub fn format_ptr_type(ty: &Type, mutable: bool) -> String {
    let inner = match ty {
        Type::Ptr { inner, .. } => inner.as_ref(),
        _ => ty,
    };
    if mutable {
        format!("ptr_mut<{}>", format_type(inner))
    } else {
        format!("ptr<{}>", format_type(inner))
    }
}

fn format_type(ty: &Type) -> String {
    match ty {
        Type::Named { name, generics, .. } => {
            if generics.is_empty() {
                name.clone()
            } else {
                format!(
                    "{}<{}>",
                    name,
                    generics.iter().map(format_type).collect::<Vec<_>>().join(", ")
                )
            }
        }
        Type::Tuple(items, _) => format!("({})", items.iter().map(format_type).collect::<Vec<_>>().join(", ")),
        Type::Array(inner, size, _) => format!("[{}; {}]", format_type(inner), size),
        Type::Slice(inner, _) => format!("[{}]", format_type(inner)),
        Type::Ref { mutable, inner, .. } => {
            if *mutable {
                format!("&mut {}", format_type(inner))
            } else {
                format!("&{}", format_type(inner))
            }
        }
        Type::Ptr { mutable, inner, .. } => {
            if *mutable {
                format!("ptr_mut<{}>", format_type(inner))
            } else {
                format!("ptr<{}>", format_type(inner))
            }
        }
        Type::Function { params, return_type, .. } => format!(
            "fn({}) -> {}",
            params.iter().map(format_type).collect::<Vec<_>>().join(", "),
            format_type(return_type)
        ),
        Type::Option(inner, _) => format!("{}?", format_type(inner)),
        Type::Result(ok, err, _) => format!("{}!{}", format_type(ok), format_type(err)),
        Type::Infer(_) => "_".to_string(),
        Type::Never(_) => "!".to_string(),
        Type::Unit(_) => "()".to_string(),
        Type::Impl { trait_name, generics, .. } => {
            if generics.is_empty() {
                format!("impl {}", trait_name)
            } else {
                format!(
                    "impl {}<{}>",
                    trait_name,
                    generics.iter().map(format_type).collect::<Vec<_>>().join(", ")
                )
            }
        }
    }
}
