use crate::ast::*;
use crate::diagnostic_registry::DiagnosticCode;
use crate::error::{DiagnosticBuilder, ErrorKind, KainResult};
use crate::low_level_abi::{
    c_abi_policy_for_target, named_scalar_layout, promoted_integer_bits,
    promoted_type_for_arithmetic, should_apply_usual_arithmetic_conversions,
    usual_arithmetic_conversion_type, CAbiPolicy,
};
use crate::low_level_memory_metadata::{
    attr_usize_arg, attr_usize_bool_args, has_attr, C_BITFIELD_ATTR, C_PACKED_ATTR,
    C_PACK_ALIGN_ATTR, C_STORAGE_ALIGN_ATTR, C_STORAGE_BITS_ATTR, C_TYPE_ALIGN_ATTR, C_UNION_ATTR,
    PUBLIC_ALIGNED_ATTR, PUBLIC_PACKED_ATTR,
};
use crate::monomorphize::MonomorphizedProgram;
use crate::span::Span;
use crate::types::{
    IntSize, ResolvedType, TypedActor, TypedComponent, TypedConst, TypedEnum, TypedFunction,
    TypedImpl, TypedItem, TypedMod, TypedProgram, TypedStruct, TypedTypeAlias,
};
use crate::CompileTarget;
use std::collections::{HashMap, HashSet};
use std::convert::TryFrom;

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
    fn build_with_abi(items: &[TypedItem], abi: &'static CAbiPolicy) -> KainResult<Self> {
        let mut registry = Self {
            abi,
            structs: HashMap::new(),
        };
        collect_struct_layouts(items, &mut registry)?;
        Ok(registry)
    }
}

fn format_type_label(ty: &Type) -> String {
    format!("{ty:?}")
}

fn layout_overflow_error(operation: &str, detail: impl Into<String>) -> crate::error::KainError {
    DiagnosticBuilder::new(
        ErrorKind::Validation,
        DiagnosticCode::MemoryLayoutOverflow,
        format!("low-level memory layout {operation} overflowed target usize arithmetic"),
    )
    .context(detail)
    .suggestion(
        "Reduce the affected array length, tuple width, field storage width, or aggregate size so the layout remains representable.",
    )
    .build()
}

fn checked_layout_add(lhs: usize, rhs: usize, detail: impl Into<String>) -> KainResult<usize> {
    lhs.checked_add(rhs)
        .ok_or_else(|| layout_overflow_error("addition", detail))
}

fn checked_layout_mul(lhs: usize, rhs: usize, detail: impl Into<String>) -> KainResult<usize> {
    lhs.checked_mul(rhs)
        .ok_or_else(|| layout_overflow_error("multiplication", detail))
}

fn checked_align_up(value: usize, align: usize, detail: impl Into<String>) -> KainResult<usize> {
    let align = align.max(1);
    let remainder = value % align;
    if remainder == 0 {
        Ok(value)
    } else {
        checked_layout_add(
            value,
            align - remainder,
            format!("{} (align_up with align={align})", detail.into()),
        )
    }
}

fn size_literal_i64(value: usize, detail: impl Into<String>) -> KainResult<i64> {
    i64::try_from(value).map_err(|_| layout_overflow_error("signed literal conversion", detail))
}

fn checked_layout_add_or_panic(lhs: usize, rhs: usize, detail: impl Into<String>) -> usize {
    checked_layout_add(lhs, rhs, detail).unwrap_or_else(|err| panic!("{err}"))
}

fn checked_layout_mul_or_panic(lhs: usize, rhs: usize, detail: impl Into<String>) -> usize {
    checked_layout_mul(lhs, rhs, detail).unwrap_or_else(|err| panic!("{err}"))
}

fn size_literal_i64_or_panic(value: usize, detail: impl Into<String>) -> i64 {
    size_literal_i64(value, detail).unwrap_or_else(|err| panic!("{err}"))
}

fn collect_struct_layouts(items: &[TypedItem], registry: &mut LayoutRegistry) -> KainResult<()> {
    for item in items {
        match item {
            TypedItem::Struct(st) => {
                let struct_name = st.ast.name.clone();
                let is_union = has_attr(&st.ast.attributes, C_UNION_ATTR);
                let is_packed = has_attr(&st.ast.attributes, C_PACKED_ATTR)
                    || has_attr(&st.ast.attributes, PUBLIC_PACKED_ATTR);
                let pack_align = match attr_usize_arg(&st.ast.attributes, C_PACK_ALIGN_ATTR) {
                    Some(bits) => {
                        let align = bits.div_ceil(8);
                        if align > 0 {
                            Some(align)
                        } else {
                            None
                        }
                    }
                    None => None,
                };
                let explicit_type_align =
                    match attr_usize_arg(&st.ast.attributes, C_TYPE_ALIGN_ATTR) {
                        Some(bits) => {
                            let align = bits.div_ceil(8);
                            if align > 0 {
                                Some(align)
                            } else {
                                None
                            }
                        }
                        None => None,
                    };
                let public_type_align = attr_usize_arg(&st.ast.attributes, PUBLIC_ALIGNED_ATTR)
                    .filter(|align| *align > 0);
                let explicit_type_align = match (explicit_type_align, public_type_align) {
                    (Some(internal), Some(public)) => Some(internal.max(public)),
                    (Some(internal), None) => Some(internal),
                    (None, Some(public)) => Some(public),
                    (None, None) => None,
                };
                let mut offset = 0usize;
                let mut max_field_size = 0usize;
                let mut max_align = 1usize;
                let mut fields = HashMap::new();
                let mut field_order = Vec::new();
                let mut bit_pack: Option<BitfieldPack> = None;
                for field in &st.ast.fields {
                    let field_context = format!(
                        "collecting layout for struct '{struct_name}' field '{}'",
                        field.name
                    );
                    let size = match attr_usize_arg(&field.attributes, C_STORAGE_BITS_ATTR) {
                        Some(bits) => bits.div_ceil(8),
                        None => registry.type_size_fallback(&field.ty)?,
                    };
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
                    let (bit_width, bit_signed) =
                        attr_usize_bool_args(&field.attributes, C_BITFIELD_ATTR)
                            .map(|(width, signed)| (Some(width), signed))
                            .unwrap_or_else(|| {
                                (attr_usize_arg(&field.attributes, C_BITFIELD_ATTR), true)
                            });
                    max_field_size = max_field_size.max(size.max(1));
                    max_align = max_align.max(align);
                    let field_name = field.name.clone();
                    let (field_offset, field_bit_offset) = if is_union {
                        (0usize, bit_width.map(|_| 0usize))
                    } else if let Some(width) = bit_width {
                        let width = width.max(1);
                        let unit_size = size.max(1);
                        let unit_bits = checked_layout_mul(
                            unit_size,
                            8,
                            format!("{field_context}: computing bitfield storage width"),
                        )?
                        .max(1);

                        if width >= unit_bits {
                            if let Some(pack) = bit_pack.take() {
                                let pack_end = checked_layout_add(
                                    pack.unit_offset,
                                    pack.unit_size,
                                    format!(
                                        "{field_context}: flushing full-width bitfield storage pack"
                                    ),
                                )?;
                                offset = offset.max(pack_end);
                            }
                            offset = checked_align_up(
                                offset,
                                align,
                                format!("{field_context}: aligning full-width bitfield storage"),
                            )?;
                            let field_offset = offset;
                            offset = checked_layout_add(
                                offset,
                                unit_size,
                                format!("{field_context}: advancing full-width bitfield storage"),
                            )?;
                            (field_offset, Some(0usize))
                        } else {
                            let needs_new_pack = match bit_pack.as_ref() {
                                Some(pack) => {
                                    let used_bits_after_insert = checked_layout_add(
                                        pack.used_bits,
                                        width,
                                        format!("{field_context}: checking bitfield pack capacity"),
                                    )?;
                                    pack.unit_size != unit_size
                                        || pack.align != align
                                        || used_bits_after_insert > unit_bits
                                }
                                None => true,
                            };

                            if needs_new_pack {
                                if let Some(pack) = bit_pack.take() {
                                    let pack_end = checked_layout_add(
                                        pack.unit_offset,
                                        pack.unit_size,
                                        format!("{field_context}: flushing prior bitfield pack"),
                                    )?;
                                    offset = offset.max(pack_end);
                                }
                                offset = checked_align_up(
                                    offset,
                                    align,
                                    format!("{field_context}: aligning new bitfield storage pack"),
                                )?;
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
                            let used_bits_after_insert = checked_layout_add(
                                pack.used_bits,
                                width,
                                format!("{field_context}: recording bitfield usage"),
                            )?;
                            let field_bit_offset = if registry.abi.bitfield_lsb_first {
                                pack.used_bits
                            } else {
                                unit_bits - used_bits_after_insert
                            };
                            pack.used_bits = used_bits_after_insert;
                            if pack.used_bits >= unit_bits {
                                let flushed = bit_pack.take().expect("bitfield pack should flush");
                                let flushed_end = checked_layout_add(
                                    flushed.unit_offset,
                                    flushed.unit_size,
                                    format!("{field_context}: flushing completed bitfield pack"),
                                )?;
                                offset = offset.max(flushed_end);
                            }
                            (field_offset, Some(field_bit_offset))
                        }
                    } else {
                        if let Some(pack) = bit_pack.take() {
                            let pack_end = checked_layout_add(
                                pack.unit_offset,
                                pack.unit_size,
                                format!("{field_context}: flushing trailing bitfield pack"),
                            )?;
                            offset = offset.max(pack_end);
                        }
                        offset = checked_align_up(
                            offset,
                            align,
                            format!("{field_context}: aligning field storage"),
                        )?;
                        let field_offset = offset;
                        offset = checked_layout_add(
                            offset,
                            size.max(1),
                            format!("{field_context}: advancing field storage"),
                        )?;
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
                    let pack_end = checked_layout_add(
                        pack.unit_offset,
                        pack.unit_size,
                        format!("collecting layout for struct '{struct_name}': flushing trailing bitfield storage"),
                    )?;
                    offset = offset.max(pack_end);
                }
                let raw_size = if is_union { max_field_size } else { offset };
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
                let size = checked_align_up(
                    raw_size.max(1),
                    final_align,
                    format!("collecting layout for struct '{struct_name}': final struct size"),
                )?;
                registry.structs.insert(
                    struct_name,
                    StructLayoutInfo {
                        size,
                        align: final_align,
                        is_union,
                        field_order,
                        fields,
                    },
                );
            }
            TypedItem::Mod(module) => collect_struct_layouts(&module.items, registry)?,
            _ => {}
        }
    }
    Ok(())
}

impl LayoutRegistry {
    fn field(&self, struct_name: &str, field: &str) -> Option<&LayoutField> {
        self.structs.get(struct_name)?.fields.get(field)
    }

    fn type_size_fallback(&self, ty: &Type) -> KainResult<usize> {
        match ty {
            Type::Named { name, .. } => Ok(match named_scalar_layout(name, self.abi) {
                Some(layout) => layout.size_bytes,
                None => self.structs.get(name).map(|info| info.size).unwrap_or(8),
            }),
            Type::Array(inner, size, _) => checked_layout_mul(
                self.type_size_fallback(inner)?,
                *size,
                format!(
                    "computing fallback array size for {}",
                    format_type_label(ty)
                ),
            ),
            Type::Slice(_, _) => Ok(16),
            Type::Tuple(types, _) => {
                let mut total = 0usize;
                for item_ty in types {
                    total = checked_layout_add(
                        total,
                        self.type_size_fallback(item_ty)?,
                        format!(
                            "computing fallback tuple size for {}",
                            format_type_label(ty)
                        ),
                    )?;
                }
                Ok(total)
            }
            Type::Ref { .. } | Type::Ptr { .. } => Ok(8),
            Type::Option(inner, _) => self.type_size_fallback(inner),
            Type::Result(ok, err, _) => Ok(self
                .type_size_fallback(ok)?
                .max(self.type_size_fallback(err)?)),
            Type::Unit(_) | Type::Never(_) => Ok(0),
            _ => Ok(8),
        }
    }

    fn type_align_fallback(&self, ty: &Type) -> usize {
        match ty {
            Type::Named { name, .. } => match named_scalar_layout(name, self.abi) {
                Some(layout) => layout.align_bytes,
                None => self
                    .structs
                    .get(name)
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
            Type::Result(ok, err, _) => self
                .type_align_fallback(ok)
                .max(self.type_align_fallback(err)),
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

#[derive(Debug, Clone)]
struct FunctionMemoryCtx<'a> {
    target: CompileTarget,
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
    let supported = match target {
        CompileTarget::Ts
        | CompileTarget::Js
        | CompileTarget::Wasm
        | CompileTarget::C
        | CompileTarget::Llvm
        | CompileTarget::Cpp
        | CompileTarget::Rust
        | CompileTarget::Ue5
        | CompileTarget::Ue5Editor => true,
        _ => false,
    };
    if !supported {
        return Ok(program.clone());
    }
    let layouts = LayoutRegistry::build_with_abi(&program.items, c_abi_policy_for_target(target))?;

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
            actor_contract: actor.actor_contract.clone(),
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
        TypedItem::Mod(module) => TypedItem::Mod(TypedMod {
            ast: module.ast.clone(),
            items: module
                .items
                .iter()
                .map(|child| lower_typed_item_memory(child, target, layouts))
                .collect(),
        }),
        TypedItem::Enum(enum_item) => TypedItem::Enum(TypedEnum {
            ast: lower_enum_memory(&enum_item.ast, target, layouts),
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

fn lower_enum_memory(enum_ast: &Enum, target: CompileTarget, layouts: &LayoutRegistry) -> Enum {
    let mut variants = Vec::new();
    for variant in &enum_ast.variants {
        let fields = match &variant.fields {
            VariantFields::Unit => VariantFields::Unit,
            VariantFields::Tuple(types) => {
                VariantFields::Tuple(types.iter().map(lower_type_memory).collect())
            }
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
        };
        variants.push(Variant {
            fields,
            ..variant.clone()
        });
    }
    Enum {
        variants,
        ..enum_ast.clone()
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
        target,
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
            let mut merged = prefix;
            for stmt in body.stmts {
                merged.push(stmt);
            }
            body.stmts = merged;
        }
    }

    Function {
        params,
        return_type: match &function.return_type {
            Some(return_type) => Some(lower_type_memory(return_type)),
            None => None,
        },
        body,
        ..function.clone()
    }
}

fn lower_free_expr_memory(expr: &Expr, target: CompileTarget, layouts: &LayoutRegistry) -> Expr {
    let mut ctx = FunctionMemoryCtx {
        target,
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
        target,
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
        for lowered_stmt in lower_stmt_memory_with_ctx(stmt, ctx) {
            stmts.push(lowered_stmt);
        }
    }
    Block {
        stmts,
        ..block.clone()
    }
}

fn lower_stmt_memory_with_ctx(stmt: &Stmt, ctx: &mut FunctionMemoryCtx<'_>) -> Vec<Stmt> {
    let mut out = Vec::new();
    match stmt {
        Stmt::Expr(expr) => {
            out.push(Stmt::Expr(lower_expr_memory_with_ctx(expr, ctx)));
        }
        Stmt::Defer { expr, span } => {
            out.push(Stmt::Defer {
                expr: lower_expr_memory_with_ctx(expr, ctx),
                span: *span,
            });
        }
        Stmt::Dispatch {
            compute_key,
            dispatch_size,
            span,
        } => {
            out.push(Stmt::Dispatch {
                compute_key: compute_key.clone(),
                dispatch_size: [
                    lower_expr_memory_with_ctx(&dispatch_size[0], ctx),
                    lower_expr_memory_with_ctx(&dispatch_size[1], ctx),
                    lower_expr_memory_with_ctx(&dispatch_size[2], ctx),
                ],
                span: *span,
            });
        }
        Stmt::Let {
            pattern,
            ty,
            value,
            span,
        } => {
            let lowered_value = match value.as_ref() {
                Some(expr) => Some(lower_expr_memory_with_ctx(expr, ctx)),
                None => None,
            };
            out.push(Stmt::Let {
                pattern: pattern.clone(),
                ty: match ty {
                    Some(ty) => Some(lower_type_memory(ty)),
                    None => None,
                },
                value: lowered_value,
                span: *span,
            });
            if let Pattern::Binding { name, .. } = pattern {
                if let Some(ty) = ty {
                    ctx.local_types.insert(name.clone(), ty.clone());
                }
                if ctx.address_taken.contains(name) {
                    out.push(make_pointer_binding_stmt(name, *span));
                    return out;
                }
            }
        }
        Stmt::Return(value, span) => {
            out.push(Stmt::Return(
                match value.as_ref() {
                    Some(expr) => Some(lower_expr_memory_with_ctx(expr, ctx)),
                    None => None,
                },
                *span,
            ));
        }
        Stmt::Break(value, span) => {
            out.push(Stmt::Break(
                match value.as_ref() {
                    Some(expr) => Some(lower_expr_memory_with_ctx(expr, ctx)),
                    None => None,
                },
                *span,
            ));
        }
        Stmt::Continue(span) => {
            out.push(Stmt::Continue(*span));
        }
        Stmt::For {
            binding,
            iter,
            body,
            span,
        } => {
            out.push(Stmt::For {
                binding: binding.clone(),
                iter: lower_expr_memory_with_ctx(iter, ctx),
                body: lower_block_memory_with_ctx(body, ctx),
                span: *span,
            });
        }
        Stmt::Fanout {
            binding,
            iter,
            body,
            span,
        } => {
            out.push(Stmt::Fanout {
                binding: binding.clone(),
                iter: lower_expr_memory_with_ctx(iter, ctx),
                body: lower_block_memory_with_ctx(body, ctx),
                span: *span,
            });
        }
        Stmt::While {
            condition,
            body,
            span,
        } => {
            out.push(Stmt::While {
                condition: lower_expr_memory_with_ctx(condition, ctx),
                body: lower_block_memory_with_ctx(body, ctx),
                span: *span,
            });
        }
        Stmt::Loop { body, span } => {
            out.push(Stmt::Loop {
                body: lower_block_memory_with_ctx(body, ctx),
                span: *span,
            });
        }
        Stmt::Item(item) => {
            out.push(Stmt::Item(item.clone()));
        }
    }
    out
}

fn lower_expr_memory_with_ctx(expr: &Expr, ctx: &mut FunctionMemoryCtx<'_>) -> Expr {
    let span = expr.span();
    match expr {
        Expr::Ident(name, ident_span) if should_load_from_pointer(name, ctx) => {
            load_from_pointer_binding(name, ctx.local_types.get(name), *ident_span)
        }
        Expr::AddrOf {
            value, pointee_ty, ..
        } => lower_addr_of_memory(value, pointee_ty.as_ref(), ctx, span),
        Expr::PtrOffset {
            pointer,
            offset,
            element_ty,
            ..
        } => {
            let stride = memory_stride_for_type(element_ty.as_ref(), ctx.layouts).unwrap_or(1);
            helper_call(
                "__kain_ptr_offset",
                vec![
                    lower_expr_memory_with_ctx(pointer, ctx),
                    lower_expr_memory_with_ctx(offset, ctx),
                    Expr::Int(stride, span),
                ],
                span,
            )
        }
        Expr::MemLoad {
            pointer, load_ty, ..
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
        Expr::MemStore { pointer, value, .. } => helper_call(
            "__kain_mem_store",
            vec![
                lower_expr_memory_with_ctx(pointer, ctx),
                lower_expr_memory_with_ctx(value, ctx),
            ],
            span,
        ),
        Expr::VolatileLoad {
            pointer, load_ty, ..
        } => {
            let call = helper_call(
                "__kain_volatile_load",
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
        Expr::VolatileStore { pointer, value, .. } => helper_call(
            "__kain_volatile_store",
            vec![
                lower_expr_memory_with_ctx(pointer, ctx),
                lower_expr_memory_with_ctx(value, ctx),
            ],
            span,
        ),
        Expr::AtomicLoad {
            pointer,
            load_ty,
            ordering,
            ..
        } => {
            let call = helper_call(
                "__kain_atomic_load_ordered",
                vec![
                    lower_expr_memory_with_ctx(pointer, ctx),
                    atomic_ordering_literal(*ordering, span),
                ],
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
        Expr::AtomicStore {
            pointer,
            value,
            ordering,
            ..
        } => helper_call(
            "__kain_atomic_store_ordered",
            vec![
                lower_expr_memory_with_ctx(pointer, ctx),
                lower_expr_memory_with_ctx(value, ctx),
                atomic_ordering_literal(*ordering, span),
            ],
            span,
        ),
        Expr::AtomicAdd {
            pointer,
            value,
            ordering,
            ..
        } => helper_call(
            "__kain_atomic_add_ordered",
            vec![
                lower_expr_memory_with_ctx(pointer, ctx),
                lower_expr_memory_with_ctx(value, ctx),
                atomic_ordering_literal(*ordering, span),
            ],
            span,
        ),
        Expr::AtomicSub {
            pointer,
            value,
            ordering,
            ..
        } => helper_call(
            "__kain_atomic_sub_ordered",
            vec![
                lower_expr_memory_with_ctx(pointer, ctx),
                lower_expr_memory_with_ctx(value, ctx),
                atomic_ordering_literal(*ordering, span),
            ],
            span,
        ),
        Expr::AtomicAnd {
            pointer,
            value,
            ordering,
            ..
        } => helper_call(
            "__kain_atomic_and_ordered",
            vec![
                lower_expr_memory_with_ctx(pointer, ctx),
                lower_expr_memory_with_ctx(value, ctx),
                atomic_ordering_literal(*ordering, span),
            ],
            span,
        ),
        Expr::AtomicOr {
            pointer,
            value,
            ordering,
            ..
        } => helper_call(
            "__kain_atomic_or_ordered",
            vec![
                lower_expr_memory_with_ctx(pointer, ctx),
                lower_expr_memory_with_ctx(value, ctx),
                atomic_ordering_literal(*ordering, span),
            ],
            span,
        ),
        Expr::AtomicXor {
            pointer,
            value,
            ordering,
            ..
        } => helper_call(
            "__kain_atomic_xor_ordered",
            vec![
                lower_expr_memory_with_ctx(pointer, ctx),
                lower_expr_memory_with_ctx(value, ctx),
                atomic_ordering_literal(*ordering, span),
            ],
            span,
        ),
        Expr::AtomicExchange {
            pointer,
            value,
            ordering,
            ..
        } => helper_call(
            "__kain_atomic_exchange_ordered",
            vec![
                lower_expr_memory_with_ctx(pointer, ctx),
                lower_expr_memory_with_ctx(value, ctx),
                atomic_ordering_literal(*ordering, span),
            ],
            span,
        ),
        Expr::AtomicCompareExchange {
            pointer,
            expected,
            desired,
            success_ordering,
            failure_ordering,
            ..
        } => helper_call(
            "__kain_atomic_compare_exchange_ordered",
            vec![
                lower_expr_memory_with_ctx(pointer, ctx),
                lower_expr_memory_with_ctx(expected, ctx),
                lower_expr_memory_with_ctx(desired, ctx),
                atomic_ordering_literal(*success_ordering, span),
                atomic_ordering_literal(*failure_ordering, span),
            ],
            span,
        ),
        Expr::AtomicFence { ordering, .. } => helper_call(
            "__kain_atomic_fence",
            vec![atomic_ordering_literal(*ordering, span)],
            span,
        ),
        Expr::CpuFence { kind, .. } => Expr::CpuFence { kind: *kind, span },
        Expr::CpuCacheFlush { pointer, .. } => Expr::CpuCacheFlush {
            pointer: Box::new(lower_expr_memory_with_ctx(pointer, ctx)),
            span,
        },
        Expr::InlineAsm {
            template,
            operands,
            options,
            ..
        } => Expr::InlineAsm {
            template: template.clone(),
            operands: operands
                .iter()
                .map(|operand| lower_expr_memory_with_ctx(operand, ctx))
                .collect(),
            options: options.clone(),
            span,
        },
        Expr::SizeOfType { target, .. } => Expr::Int(
            size_literal_i64_or_panic(
                estimate_type_size(target, ctx.layouts),
                format!("lowering sizeof_type for {}", format_type_label(target)),
            ),
            span,
        ),
        Expr::AlignOfType { target, .. } => Expr::Int(
            size_literal_i64_or_panic(
                estimate_type_align(target, ctx.layouts),
                format!("lowering alignof_type for {}", format_type_label(target)),
            ),
            span,
        ),
        Expr::Alloca { ty, .. } => lower_storage_expr(ty, ctx.layouts, span, false),
        Expr::Uninit { ty, .. } => lower_storage_expr(ty, ctx.layouts, span, false),
        Expr::Alloc {
            size, ty, zeroed, ..
        } => lower_heap_alloc_expr(
            &lower_expr_memory_with_ctx(size, ctx),
            ty.as_ref(),
            *zeroed,
            ctx.target,
            ctx.layouts,
            span,
        ),
        Expr::Realloc {
            pointer,
            size,
            ty,
            zeroed_new,
            ..
        } => lower_heap_realloc_expr(
            &lower_expr_memory_with_ctx(pointer, ctx),
            &lower_expr_memory_with_ctx(size, ctx),
            ty.as_ref(),
            *zeroed_new,
            ctx.target,
            ctx.layouts,
            span,
        ),
        Expr::Observe { target, body, span } => Expr::Observe {
            target: Box::new(lower_expr_memory_with_ctx(target, ctx)),
            body: Box::new(lower_expr_memory_with_ctx(body, ctx)),
            span: *span,
        },
        Expr::Collapse { target, body, span } => Expr::Collapse {
            target: Box::new(lower_expr_memory_with_ctx(target, ctx)),
            body: Box::new(lower_expr_memory_with_ctx(body, ctx)),
            span: *span,
        },
        Expr::Decay { target, span } => Expr::Decay {
            target: Box::new(lower_expr_memory_with_ctx(target, ctx)),
            span: *span,
        },
        Expr::Share { target, body, span } => Expr::Share {
            target: Box::new(lower_expr_memory_with_ctx(target, ctx)),
            body: Box::new(lower_expr_memory_with_ctx(body, ctx)),
            span: *span,
        },
        Expr::Teleport {
            value,
            source_world,
            target_world,
            channel,
            span,
        } => Expr::Teleport {
            value: Box::new(lower_expr_memory_with_ctx(value, ctx)),
            source_world: source_world.clone(),
            target_world: target_world.clone(),
            channel: channel.clone(),
            span: *span,
        },
        Expr::AggregateInit {
            ty,
            fields,
            zero_fill_rest,
            ..
        } => {
            let mut lowered_fields: Vec<(String, Expr)> = Vec::new();
            for (name, value) in fields.iter() {
                lowered_fields.push((name.clone(), lower_expr_memory_with_ctx(value, ctx)));
            }
            lower_aggregate_init_expr(ty, &lowered_fields, *zero_fill_rest, ctx.layouts, span)
        }
        Expr::Assign {
            target,
            value,
            span,
        } => {
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
                                Expr::Int(
                                    size_literal_i64_or_panic(
                                        field_offset,
                                        format!("lowering bitfield field offset for '{field}'"),
                                    ),
                                    *span,
                                ),
                                Expr::Int(
                                    size_literal_i64_or_panic(
                                        field_bit_offset,
                                        format!("lowering bitfield bit offset for '{field}'"),
                                    ),
                                    *span,
                                ),
                                Expr::Int(
                                    size_literal_i64_or_panic(
                                        bit_width,
                                        format!("lowering bitfield width for '{field}'"),
                                    ),
                                    *span,
                                ),
                                Expr::Bool(field_bit_signed, *span),
                                Expr::Int(
                                    size_literal_i64_or_panic(
                                        promoted_bits,
                                        format!("lowering promoted bitfield width for '{field}'"),
                                    ),
                                    *span,
                                ),
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
                    let (field_type_name, field_stride) = match &field_ty {
                        Some(ty) => (
                            type_key(ty),
                            memory_stride_for_type(Some(ty), ctx.layouts).unwrap_or(1),
                        ),
                        None => ("unknown".to_string(), 1),
                    };
                    return helper_call(
                        "__kain_union_set",
                        vec![
                            lower_expr_memory_with_ctx(object, ctx),
                            Expr::String(field.clone(), *span),
                            Expr::String(field_type_name, *span),
                            Expr::Int(field_stride, *span),
                            Expr::Int(
                                size_literal_i64_or_panic(
                                    layout_size,
                                    format!("lowering union layout size for field '{field}'"),
                                ),
                                *span,
                            ),
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
        Expr::Deref(inner, inner_span) => Expr::Deref(
            Box::new(lower_expr_memory_with_ctx(inner, ctx)),
            *inner_span,
        ),
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
                let common_ty = match (left_ty.as_ref(), right_ty.as_ref()) {
                    (Some(lhs), Some(rhs)) => {
                        usual_arithmetic_conversion_type(lhs, rhs, ctx.layouts.abi)
                    }
                    _ => None,
                };
                return Expr::Binary {
                    left: Box::new(cast_if_needed(lowered_left, common_ty.clone(), *span)),
                    op: *op,
                    right: Box::new(cast_if_needed(lowered_right, common_ty, *span)),
                    span: *span,
                };
            }

            let normalized_left = match op {
                BinaryOp::Shl | BinaryOp::Shr => cast_if_needed(
                    lowered_left,
                    match left_ty.as_ref() {
                        Some(ty) => promoted_type_for_arithmetic(ty, ctx.layouts.abi),
                        None => None,
                    },
                    *span,
                ),
                _ => lowered_left,
            };
            let normalized_right = match op {
                BinaryOp::Shl | BinaryOp::Shr => {
                    cast_if_needed(lowered_right, Some(named_int_type(*span)), *span)
                }
                _ => lowered_right,
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
                    match operand_ty.as_ref() {
                        Some(ty) => promoted_type_for_arithmetic(ty, ctx.layouts.abi),
                        None => None,
                    },
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
        Expr::StageCall {
            runtime,
            function,
            args,
            selector,
            metadata,
            span,
        } => Expr::StageCall {
            runtime: *runtime,
            function: function.clone(),
            args: args
                .iter()
                .map(|arg| CallArg {
                    value: lower_expr_memory_with_ctx(&arg.value, ctx),
                    ..arg.clone()
                })
                .collect(),
            selector: selector.clone(),
            metadata: metadata.clone(),
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
                            Expr::Int(
                                size_literal_i64_or_panic(
                                    field_offset,
                                    format!("lowering bitfield field offset for '{field}'"),
                                ),
                                *span,
                            ),
                            Expr::Int(
                                size_literal_i64_or_panic(
                                    field_bit_offset,
                                    format!("lowering bitfield bit offset for '{field}'"),
                                ),
                                *span,
                            ),
                            Expr::Int(
                                size_literal_i64_or_panic(
                                    bit_width,
                                    format!("lowering bitfield width for '{field}'"),
                                ),
                                *span,
                            ),
                            Expr::Bool(field_bit_signed, *span),
                            Expr::Int(
                                size_literal_i64_or_panic(
                                    promoted_bits,
                                    format!("lowering promoted bitfield width for '{field}'"),
                                ),
                                *span,
                            ),
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
                let (field_type_name, field_stride) = match &field_ty {
                    Some(ty) => (
                        type_key(ty),
                        memory_stride_for_type(Some(ty), ctx.layouts).unwrap_or(1),
                    ),
                    None => ("unknown".to_string(), 1),
                };
                return helper_call(
                    "__kain_union_get",
                    vec![
                        lower_expr_memory_with_ctx(object, ctx),
                        Expr::String(field.clone(), *span),
                        Expr::String(field_type_name, *span),
                        Expr::Int(field_stride, *span),
                        Expr::Int(
                            size_literal_i64_or_panic(
                                layout_size,
                                format!("lowering union layout size for field '{field}'"),
                            ),
                            *span,
                        ),
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
            items
                .iter()
                .map(|item| lower_expr_memory_with_ctx(item, ctx))
                .collect(),
            *items_span,
        ),
        Expr::Tuple(items, items_span) => Expr::Tuple(
            items
                .iter()
                .map(|item| lower_expr_memory_with_ctx(item, ctx))
                .collect(),
            *items_span,
        ),
        Expr::Struct {
            name,
            fields,
            rest,
            span,
        } => {
            let mut lowered_fields: Vec<(String, Expr)> = Vec::new();
            for (field, value) in fields.iter() {
                lowered_fields.push((field.clone(), lower_expr_memory_with_ctx(value, ctx)));
            }
            Expr::Struct {
                name: name.clone(),
                fields: lowered_fields,
                rest: rest
                    .as_ref()
                    .map(|value| Box::new(lower_expr_memory_with_ctx(value, ctx))),
                span: *span,
            }
        }
        Expr::EnumVariant {
            enum_name,
            variant,
            fields,
            span,
        } => {
            let lowered_fields = match fields {
                EnumVariantFields::Unit => EnumVariantFields::Unit,
                EnumVariantFields::Tuple(items) => {
                    let mut lowered_items: Vec<Expr> = Vec::new();
                    for item in items.iter() {
                        lowered_items.push(lower_expr_memory_with_ctx(item, ctx));
                    }
                    EnumVariantFields::Tuple(lowered_items)
                }
                EnumVariantFields::Struct(items) => {
                    let mut lowered_items: Vec<(String, Expr)> = Vec::new();
                    for (field, value) in items.iter() {
                        lowered_items.push((field.clone(), lower_expr_memory_with_ctx(value, ctx)));
                    }
                    EnumVariantFields::Struct(lowered_items)
                }
            };
            Expr::EnumVariant {
                enum_name: enum_name.clone(),
                variant: variant.clone(),
                fields: lowered_fields,
                span: *span,
            }
        }
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
            return_type: match return_type {
                Some(return_type) => Some(lower_type_memory(return_type)),
                None => None,
            },
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
        Expr::Bitcast {
            value,
            target,
            span,
        } => Expr::Bitcast {
            value: Box::new(lower_expr_memory_with_ctx(value, ctx)),
            target: lower_type_memory(target),
            span: *span,
        },
        Expr::Try(inner, inner_span) => Expr::Try(
            Box::new(lower_expr_memory_with_ctx(inner, ctx)),
            *inner_span,
        ),
        Expr::Await(inner, inner_span) => Expr::Await(
            Box::new(lower_expr_memory_with_ctx(inner, ctx)),
            *inner_span,
        ),
        Expr::AsyncBlock(inner, inner_span) => Expr::AsyncBlock(
            Box::new(lower_expr_memory_with_ctx(inner, ctx)),
            *inner_span,
        ),
        Expr::Block(block, block_span) => {
            Expr::Block(lower_block_memory_with_ctx(block, ctx), *block_span)
        }
        Expr::Paren(inner, inner_span) => Expr::Paren(
            Box::new(lower_expr_memory_with_ctx(inner, ctx)),
            *inner_span,
        ),
        Expr::Return(Some(inner), inner_span) => Expr::Return(
            Some(Box::new(lower_expr_memory_with_ctx(inner, ctx))),
            *inner_span,
        ),
        Expr::Break(Some(inner), inner_span) => Expr::Break(
            Some(Box::new(lower_expr_memory_with_ctx(inner, ctx))),
            *inner_span,
        ),
        Expr::Spawn { actor, init, span } => {
            let mut lowered_init: Vec<(String, Expr)> = Vec::new();
            for (name, value) in init.iter() {
                lowered_init.push((name.clone(), lower_expr_memory_with_ctx(value, ctx)));
            }
            Expr::Spawn {
                actor: actor.clone(),
                init: lowered_init,
                span: *span,
            }
        }
        Expr::SendMsg {
            target,
            message,
            data,
            span,
        } => {
            let mut lowered_data: Vec<(String, Expr)> = Vec::new();
            for (name, value) in data.iter() {
                lowered_data.push((name.clone(), lower_expr_memory_with_ctx(value, ctx)));
            }
            Expr::SendMsg {
                target: Box::new(lower_expr_memory_with_ctx(target, ctx)),
                message: message.clone(),
                data: lowered_data,
                span: *span,
            }
        }
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
            items
                .iter()
                .map(|item| lower_expr_memory_with_ctx(item, ctx))
                .collect(),
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
            args.push(Expr::Int(
                memory_stride_for_type(Some(ty), ctx.layouts).unwrap_or(1),
                span,
            ));
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
                    Expr::Int(
                        size_literal_i64_or_panic(
                            offset,
                            format!("lowering field pointer offset for '{field}'"),
                        ),
                        *span,
                    ),
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
            let element_ty = infer_element_type(object, ctx);
            let stride = match &element_ty {
                Some(ty) => memory_stride_for_type(Some(ty), ctx.layouts).unwrap_or(1),
                None => 1,
            };
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
            Stmt::For { body, .. } | Stmt::While { body, .. } | Stmt::Loop { body, .. } => {
                collect_local_types_from_block_into(body, locals)
            }
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
            Stmt::Expr(expr) | Stmt::Return(Some(expr), _) | Stmt::Break(Some(expr), _) => {
                collect_address_taken_from_expr(expr, roots)
            }
            Stmt::Let {
                value: Some(expr), ..
            } => collect_address_taken_from_expr(expr, roots),
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
                roots.insert(root);
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
        Expr::MethodCall { receiver, args, .. } => {
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
        | Expr::AsyncBlock(inner, _)
        | Expr::Comptime(inner, _) => collect_address_taken_from_expr(inner, roots),
        _ => {}
    }
}

fn root_ident_of_addressable(expr: &Expr) -> Option<String> {
    match expr {
        Expr::Ident(name, _) => Some(name.clone()),
        Expr::Field { object, .. } | Expr::Index { object, .. } => {
            root_ident_of_addressable(object)
        }
        _ => None,
    }
}

fn infer_expr_type(expr: &Expr, ctx: &FunctionMemoryCtx<'_>) -> Option<Type> {
    match expr {
        Expr::Int(_, span) => Option::Some(Type::Named {
            name: "Int".to_string(),
            generics: Vec::new(),
            span: *span,
        }),
        Expr::Float(_, span) => Option::Some(Type::Named {
            name: "Float".to_string(),
            generics: Vec::new(),
            span: *span,
        }),
        Expr::Bool(_, span) => Option::Some(Type::Named {
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
        Expr::Cast { target, .. } | Expr::Bitcast { target, .. } => Option::Some(target.clone()),
        _ => Option::None,
    }
}

fn infer_element_type(expr: &Expr, ctx: &FunctionMemoryCtx<'_>) -> Option<Type> {
    match infer_expr_type(expr, ctx)? {
        Type::Array(inner, _, _)
        | Type::Slice(inner, _)
        | Type::Ref { inner, .. }
        | Type::Ptr { inner, .. }
        | Type::Option(inner, _) => Option::Some(*inner),
        other => Option::Some(other),
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
        Type::Named { name, generics, .. } => {
            format!("{}<{}>", name, join_type_key_list(generics, ","))
        }
        Type::Array(inner, size, _) => format!("[{};{}]", type_key(inner), size),
        Type::Slice(inner, _) => format!("[{}]", type_key(inner)),
        Type::Ref { inner, .. } => format!("&{}", type_key(inner)),
        Type::Ptr { inner, .. } => format!("ptr<{}>", type_key(inner)),
        Type::Tuple(types, _) => format!("({})", join_type_key_list(types, ",")),
        Type::Unit(_) => "()".to_string(),
        Type::Never(_) => "!".to_string(),
        _ => "unknown".to_string(),
    }
}

fn join_type_key_list(types: &[Type], separator: &str) -> String {
    let mut out = String::new();
    let mut first = true;
    for ty in types {
        if !first {
            out.push_str(separator);
        }
        first = false;
        out.push_str(&type_key(ty));
    }
    out
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
        Type::Array(inner, size, span) => {
            Type::Array(Box::new(lower_type_memory(inner)), *size, *span)
        }
        Type::Slice(inner, span) => Type::Slice(Box::new(lower_type_memory(inner)), *span),
        Type::Tuple(types, span) => {
            Type::Tuple(types.iter().map(lower_type_memory).collect(), *span)
        }
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
        Type::Named {
            name,
            generics,
            span,
        } => Type::Named {
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
        ResolvedType::Ptr { .. } => ResolvedType::Int(IntSize::I64),
        ResolvedType::Array(inner, size) => {
            ResolvedType::Array(Box::new(lower_resolved_type_memory(inner)), *size)
        }
        ResolvedType::Slice(inner) => {
            ResolvedType::Slice(Box::new(lower_resolved_type_memory(inner)))
        }
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
        ResolvedType::Function {
            params,
            ret,
            effects,
        } => ResolvedType::Function {
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
        ResolvedType::Enum(name, variants) => {
            ResolvedType::Enum(name.clone(), lower_resolved_enum_variants(variants))
        }
        _ => ty.clone(),
    }
}

fn lower_resolved_enum_variants(
    variants: &[(String, ResolvedType)],
) -> Vec<(String, ResolvedType)> {
    let mut lowered_variants = Vec::new();
    for (variant, ty) in variants {
        lowered_variants.push((variant.clone(), lower_resolved_type_memory(ty)));
    }
    lowered_variants
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

fn atomic_ordering_literal(ordering: AtomicOrdering, span: Span) -> Expr {
    Expr::Int(ordering.abi_code(), span)
}

fn memory_stride_for_type(ty: Option<&Type>, layouts: &LayoutRegistry) -> Option<i64> {
    ty.map(|ty| {
        size_literal_i64_or_panic(
            estimate_type_size(ty, layouts),
            format!("lowering memory stride for {}", format_type_label(ty)),
        )
    })
}

fn estimate_type_size(ty: &Type, layouts: &LayoutRegistry) -> usize {
    match ty {
        Type::Named { name, .. } => match named_scalar_layout(name, layouts.abi) {
            Some(layout) => layout.size_bytes,
            None => layouts
                .type_size_fallback(ty)
                .unwrap_or_else(|err| panic!("{err}")),
        },
        Type::Array(inner, size, _) => checked_layout_mul_or_panic(
            estimate_type_size(inner, layouts),
            *size,
            format!(
                "estimating lowered array size for {}",
                format_type_label(ty)
            ),
        ),
        Type::Slice(_, _) => 16,
        Type::Tuple(types, _) => {
            let mut total = 0usize;
            for item_ty in types {
                total = checked_layout_add_or_panic(
                    total,
                    estimate_type_size(item_ty, layouts),
                    format!(
                        "estimating lowered tuple size for {}",
                        format_type_label(ty)
                    ),
                );
            }
            total
        }
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
        Type::Named { name, .. } => match named_scalar_layout(name, layouts.abi) {
            Some(layout) => layout.align_bytes,
            None => match layouts.structs.get(name) {
                Some(info) => info.align.max(1),
                None => layouts.type_align_fallback(ty),
            },
        },
        Type::Array(inner, _, _) => estimate_type_align(inner, layouts),
        Type::Tuple(types, _) => types
            .iter()
            .map(|ty| estimate_type_align(ty, layouts))
            .max()
            .unwrap_or(1),
        Type::Option(inner, _) => estimate_type_align(inner, layouts),
        Type::Result(ok, err, _) => {
            estimate_type_align(ok, layouts).max(estimate_type_align(err, layouts))
        }
        _ => layouts.type_align_fallback(ty),
    }
}

fn lower_storage_expr(ty: &Type, layouts: &LayoutRegistry, span: Span, zeroed: bool) -> Expr {
    match ty {
        Type::Array(inner, count, _) => Expr::Array(
            lower_storage_array_items(inner, layouts, span, zeroed, *count),
            span,
        ),
        Type::Tuple(items, _) => Expr::Tuple(
            items
                .iter()
                .map(|item| lower_storage_expr(item, layouts, span, zeroed))
                .collect(),
            span,
        ),
        Type::Named { name, .. } => match name.as_str() {
            "Int" | "UInt" | "isize" | "usize" => {
                if zeroed {
                    Expr::Int(0, span)
                } else {
                    Expr::None(span)
                }
            }
            "Float" => {
                if zeroed {
                    Expr::Float(0.0, span)
                } else {
                    Expr::None(span)
                }
            }
            "Bool" => {
                if zeroed {
                    Expr::Bool(false, span)
                } else {
                    Expr::None(span)
                }
            }
            "Char" => {
                if zeroed {
                    Expr::String("\0".to_string(), span)
                } else {
                    Expr::None(span)
                }
            }
            _ => match layouts.structs.get(name) {
                Some(info) => {
                    let mut fields = Vec::new();
                    for field_name in info.field_order.iter() {
                        if let Some(field) = info.fields.get(field_name) {
                            fields.push((
                                field_name.clone(),
                                lower_storage_expr(&field.ty, layouts, span, zeroed),
                            ));
                        }
                    }
                    Expr::Struct {
                        name: name.clone(),
                        fields,
                        rest: None,
                        span,
                    }
                }
                None => Expr::None(span),
            },
        },
        Type::Unit(_) => Expr::Tuple(Vec::new(), span),
        Type::Ref { .. } | Type::Ptr { .. } => Expr::None(span),
        _ => Expr::None(span),
    }
}

fn lower_storage_array_items(
    inner: &Type,
    layouts: &LayoutRegistry,
    span: Span,
    zeroed: bool,
    count: usize,
) -> Vec<Expr> {
    let mut items = Vec::new();
    let mut remaining = count;
    while remaining > 0 {
        items.push(lower_storage_expr(inner, layouts, span, zeroed));
        remaining -= 1;
    }
    items
}

fn storage_seed_expr(
    ty: Option<&Type>,
    layouts: &LayoutRegistry,
    span: Span,
    zeroed: bool,
) -> Expr {
    ty.map(|ty| lower_storage_expr(ty, layouts, span, zeroed))
        .unwrap_or_else(|| {
            if zeroed {
                Expr::Int(0, span)
            } else {
                Expr::None(span)
            }
        })
}

fn lower_heap_alloc_expr(
    size: &Expr,
    ty: Option<&Type>,
    zeroed: bool,
    target: CompileTarget,
    layouts: &LayoutRegistry,
    span: Span,
) -> Expr {
    let mut args = vec![
        size.clone(),
        Expr::Int(memory_stride_for_type(ty, layouts).unwrap_or(1), span),
        Expr::Bool(zeroed, span),
    ];
    if uses_seeded_heap_helper_abi(target) {
        args.push(storage_seed_expr(ty, layouts, span, zeroed));
    }
    helper_call("__kain_alloc", args, span)
}

fn lower_heap_realloc_expr(
    pointer: &Expr,
    size: &Expr,
    ty: Option<&Type>,
    zeroed_new: bool,
    target: CompileTarget,
    layouts: &LayoutRegistry,
    span: Span,
) -> Expr {
    let mut args = vec![
        pointer.clone(),
        size.clone(),
        Expr::Int(memory_stride_for_type(ty, layouts).unwrap_or(1), span),
        Expr::Bool(zeroed_new, span),
    ];
    if uses_seeded_heap_helper_abi(target) {
        args[3] = storage_seed_expr(ty, layouts, span, zeroed_new);
    }
    helper_call("__kain_realloc", args, span)
}

fn uses_seeded_heap_helper_abi(target: CompileTarget) -> bool {
    match target {
        CompileTarget::Ts | CompileTarget::Js | CompileTarget::Wasm | CompileTarget::Hybrid => true,
        _ => false,
    }
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
            let mut provided: HashMap<String, Expr> = HashMap::new();
            for (field_name, value) in fields.iter() {
                provided.insert(field_name.clone(), value.clone());
            }

            match layouts.structs.get(name) {
                Some(info) => {
                    if info.is_union {
                        let (field_values, active_field, active_value) =
                            lower_union_aggregate_fields(
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
                            rest: None,
                            span,
                        };
                        if let Some(active_field) = active_field {
                            let active_ty = match info.fields.get(&active_field) {
                                Some(field) => Some(field.ty.clone()),
                                None => None,
                            };
                            let (active_type_name, active_stride) = match active_ty {
                                Some(ty) => (
                                    type_key(&ty),
                                    memory_stride_for_type(Some(&ty), layouts).unwrap_or(1),
                                ),
                                None => ("unknown".to_string(), 1),
                            };
                            helper_call(
                                "__kain_union_wrap",
                                vec![
                                    base_struct,
                                    Expr::String(active_field, span),
                                    Expr::String(active_type_name, span),
                                    Expr::Int(active_stride, span),
                                    Expr::Int(
                                        size_literal_i64_or_panic(
                                            info.size,
                                            format!("lowering aggregate union size for '{}'", name),
                                        ),
                                        span,
                                    ),
                                    active_value,
                                ],
                                span,
                            )
                        } else {
                            base_struct
                        }
                    } else {
                        let mut field_values = Vec::new();
                        for field_name in info.field_order.iter() {
                            if let Some(field) = info.fields.get(field_name) {
                                let value = match provided.get(field_name).cloned() {
                                    Some(value) => value,
                                    None => {
                                        if zero_fill_rest {
                                            lower_storage_expr(&field.ty, layouts, span, true)
                                        } else {
                                            Expr::None(span)
                                        }
                                    }
                                };
                                field_values.push((field_name.clone(), value));
                            }
                        }
                        Expr::Struct {
                            name: name.clone(),
                            fields: field_values,
                            rest: None,
                            span,
                        }
                    }
                }
                None => {
                    let mut copied_fields = Vec::new();
                    for (field_name, value) in fields.iter() {
                        copied_fields.push((field_name.clone(), value.clone()));
                    }
                    Expr::Struct {
                        name: name.clone(),
                        fields: copied_fields,
                        rest: None,
                        span,
                    }
                }
            }
        }
        _ => {
            let mut tuple_values = Vec::new();
            for (_, value) in fields.iter() {
                tuple_values.push(value.clone());
            }
            Expr::Tuple(tuple_values, span)
        }
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
    let mut active_name: Option<String> = None;
    let mut index = original_fields.len();
    while index > 0 {
        index -= 1;
        let (field_name, _) = &original_fields[index];
        if info.fields.contains_key(field_name) {
            active_name = Some(field_name.clone());
            break;
        }
    }
    if active_name.is_none() {
        active_name = info.field_order.first().cloned();
    }

    let Some(active_name) = active_name else {
        return (Vec::new(), None, Expr::None(span));
    };

    let active_value = match info.fields.get(&active_name) {
        Some(field) => match provided.get(&active_name).cloned() {
            Some(value) => value,
            None => {
                if zero_fill_rest {
                    lower_storage_expr(&field.ty, layouts, span, true)
                } else {
                    Expr::None(span)
                }
            }
        },
        None => Expr::None(span),
    };

    let mut field_values = Vec::new();
    for field_name in info.field_order.iter() {
        if let Some(field) = info.fields.get(field_name) {
            let value = if field_name == &active_name {
                match provided.get(field_name).cloned() {
                    Some(value) => value,
                    None => {
                        if zero_fill_rest {
                            lower_storage_expr(&field.ty, layouts, span, true)
                        } else {
                            Expr::None(span)
                        }
                    }
                }
            } else if zero_fill_rest {
                lower_storage_expr(&field.ty, layouts, span, true)
            } else {
                Expr::None(span)
            };
            field_values.push((field_name.clone(), value));
        }
    }

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
                        return Some(format!("Impl method '{}': {}", method.name, context));
                    }
                }
            }
            TypedItem::Mod(module) => {
                if let Some(context) = first_unsupported_memory_context(&module.items, caps) {
                    return Some(context);
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
        Stmt::Expr(expr) => {
            first_memory_expr_context(expr, format!("{owner} contains a raw memory operation"))
        }
        Stmt::Let { value, .. } => match value {
            Some(value) => {
                first_memory_expr_context(value, format!("{owner} contains a raw memory operation"))
            }
            None => None,
        },
        Stmt::Return(Some(expr), _) => first_memory_expr_context(
            expr,
            format!("{owner} return contains a raw memory operation"),
        ),
        Stmt::Defer { expr, .. } => first_memory_expr_context(
            expr,
            format!("{owner} defer contains a raw memory operation"),
        ),
        Stmt::Dispatch { dispatch_size, .. } => dispatch_size.iter().find_map(|expr| {
            first_memory_expr_context(
                expr,
                format!("{owner} dispatch dimension contains a raw memory operation"),
            )
        }),
        Stmt::For { iter, body, .. } | Stmt::Fanout { iter, body, .. } => {
            if let Some(context) = first_memory_expr_context(
                iter,
                format!("{owner} loop iterator contains a raw memory operation"),
            ) {
                Some(context)
            } else {
                first_memory_block_context(body, owner.to_string())
            }
        }
        Stmt::While {
            condition, body, ..
        } => {
            if let Some(context) = first_memory_expr_context(
                condition,
                format!("{owner} loop condition contains a raw memory operation"),
            ) {
                Some(context)
            } else {
                first_memory_block_context(body, owner.to_string())
            }
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
        Expr::VolatileLoad { .. } => Some(format!("{base}: volatile memory load expression")),
        Expr::VolatileStore { .. } => Some(format!("{base}: volatile memory store expression")),
        Expr::AtomicLoad { .. } => Some(format!("{base}: atomic load expression")),
        Expr::AtomicStore { .. } => Some(format!("{base}: atomic store expression")),
        Expr::AtomicAdd { .. } => Some(format!("{base}: atomic add expression")),
        Expr::AtomicSub { .. } => Some(format!("{base}: atomic sub expression")),
        Expr::AtomicAnd { .. } => Some(format!("{base}: atomic and expression")),
        Expr::AtomicOr { .. } => Some(format!("{base}: atomic or expression")),
        Expr::AtomicXor { .. } => Some(format!("{base}: atomic xor expression")),
        Expr::AtomicExchange { .. } => Some(format!("{base}: atomic exchange expression")),
        Expr::AtomicCompareExchange { .. } => {
            Some(format!("{base}: atomic compare_exchange expression"))
        }
        Expr::AtomicFence { .. } => Some(format!("{base}: atomic fence expression")),
        Expr::CpuFence { kind, .. } => Some(format!(
            "{base}: {} instruction expression",
            kind.intrinsic_name()
        )),
        Expr::CpuCacheFlush { .. } => Some(format!("{base}: cache-line flush expression")),
        Expr::InlineAsm { .. } => Some(format!("{base}: inline asm expression")),
        Expr::SizeOfType { .. }
        | Expr::AlignOfType { .. }
        | Expr::Alloca { .. }
        | Expr::Uninit { .. } => None,
        Expr::Alloc { size, .. } => first_memory_expr_context(size, base),
        Expr::Realloc { pointer, size, .. } => {
            if let Some(context) = first_memory_expr_context(pointer, base.clone()) {
                Some(context)
            } else {
                first_memory_expr_context(size, base)
            }
        }
        Expr::Observe { .. } => Some(format!("{base}: ownership observe expression")),
        Expr::Collapse { .. } => Some(format!("{base}: ownership collapse expression")),
        Expr::Decay { .. } => Some(format!("{base}: ownership decay expression")),
        Expr::Share { .. } => Some(format!("{base}: ownership share expression")),
        Expr::Teleport { value, .. } => first_memory_expr_context(value, base),
        Expr::AggregateInit { fields, .. } => first_memory_expr_context_from_pairs(fields, base),
        Expr::Binary { left, right, .. } => {
            if let Some(context) = first_memory_expr_context(left, base.clone()) {
                Some(context)
            } else {
                first_memory_expr_context(right, base)
            }
        }
        Expr::Unary { operand, .. }
        | Expr::Ref { value: operand, .. }
        | Expr::Deref(operand, _)
        | Expr::Try(operand, _)
        | Expr::Await(operand, _)
        | Expr::AsyncBlock(operand, _)
        | Expr::Comptime(operand, _)
        | Expr::Paren(operand, _) => first_memory_expr_context(operand, base),
        Expr::Cast { value, .. } | Expr::Bitcast { value, .. } => {
            first_memory_expr_context(value, base)
        }
        Expr::Call { callee, args, .. } => {
            if let Some(context) = first_memory_expr_context(callee, base.clone()) {
                Some(context)
            } else {
                first_memory_expr_context_from_call_args(args, base)
            }
        }
        Expr::StageCall { args, .. } => first_memory_expr_context_from_call_args(args, base),
        Expr::MethodCall { receiver, args, .. } => {
            if let Some(context) = first_memory_expr_context(receiver, base.clone()) {
                Some(context)
            } else {
                first_memory_expr_context_from_call_args(args, base)
            }
        }
        Expr::Field { object, .. } => first_memory_expr_context(object, base),
        Expr::Index { object, index, .. } => {
            if let Some(context) = first_memory_expr_context(object, base.clone()) {
                Some(context)
            } else {
                first_memory_expr_context(index, base)
            }
        }
        Expr::Assign { target, value, .. } => {
            if let Some(context) = first_memory_expr_context(target, base.clone()) {
                Some(context)
            } else {
                first_memory_expr_context(value, base)
            }
        }
        Expr::Struct { fields, rest, .. } => {
            if let Some(context) = first_memory_expr_context_from_pairs(fields, base.clone()) {
                Some(context)
            } else {
                match rest {
                    Some(value) => first_memory_expr_context(value, base),
                    None => None,
                }
            }
        }
        Expr::EnumVariant { fields, .. } => first_memory_enum_variant_fields_context(fields, base),
        Expr::Array(items, _) | Expr::Tuple(items, _) | Expr::FString(items, _) => {
            first_memory_expr_context_from_exprs(items, base)
        }
        Expr::Range { start, end, .. } => match start {
            Some(expr) => {
                if let Some(context) = first_memory_expr_context(expr, base.clone()) {
                    Some(context)
                } else {
                    match end {
                        Some(expr) => first_memory_expr_context(expr, base),
                        None => None,
                    }
                }
            }
            None => match end {
                Some(expr) => first_memory_expr_context(expr, base),
                None => None,
            },
        },
        Expr::If {
            condition,
            then_branch,
            else_branch,
            ..
        } => {
            if let Some(context) = first_memory_expr_context(condition, base.clone()) {
                Some(context)
            } else if let Some(context) = first_memory_block_context(then_branch, base.clone()) {
                Some(context)
            } else {
                match else_branch {
                    Some(branch) => first_memory_else_branch_context(branch, base),
                    None => None,
                }
            }
        }
        Expr::Match {
            scrutinee, arms, ..
        } => {
            if let Some(context) = first_memory_expr_context(scrutinee, base.clone()) {
                Some(context)
            } else {
                for arm in arms {
                    if let Some(context) = first_memory_expr_context(&arm.body, base.clone()) {
                        return Some(context);
                    }
                }
                None
            }
        }
        Expr::Lambda { body, .. } => first_memory_expr_context(body, base),
        Expr::Spawn { init, .. } | Expr::SendMsg { data: init, .. } => {
            first_memory_expr_context_from_pairs(init, base)
        }
        Expr::Block(block, _) => first_memory_block_context(block, base),
        Expr::MacroCall { args, .. } => first_memory_expr_context_from_exprs(args, base),
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

fn first_memory_expr_context_from_pairs(items: &[(String, Expr)], base: String) -> Option<String> {
    for (_, value) in items {
        if let Some(context) = first_memory_expr_context(value, base.clone()) {
            return Some(context);
        }
    }
    None
}

fn first_memory_expr_context_from_exprs(items: &[Expr], base: String) -> Option<String> {
    for value in items {
        if let Some(context) = first_memory_expr_context(value, base.clone()) {
            return Some(context);
        }
    }
    None
}

fn first_memory_expr_context_from_call_args(
    args: &[crate::ast::CallArg],
    base: String,
) -> Option<String> {
    for arg in args {
        if let Some(context) = first_memory_expr_context(&arg.value, base.clone()) {
            return Some(context);
        }
    }
    None
}

fn first_memory_else_branch_context(
    else_branch: &crate::ast::ElseBranch,
    base: String,
) -> Option<String> {
    match else_branch {
        crate::ast::ElseBranch::Else(else_block) => first_memory_block_context(else_block, base),
        crate::ast::ElseBranch::ElseIf(cond, else_block, next_branch) => {
            if let Some(context) = first_memory_expr_context(cond, base.clone()) {
                Some(context)
            } else if let Some(context) = first_memory_block_context(else_block, base.clone()) {
                Some(context)
            } else {
                match next_branch {
                    Some(next_branch) => first_memory_else_branch_context(next_branch, base),
                    None => None,
                }
            }
        }
    }
}

fn first_memory_enum_variant_fields_context(
    fields: &crate::ast::EnumVariantFields,
    base: String,
) -> Option<String> {
    match fields {
        crate::ast::EnumVariantFields::Unit => None,
        crate::ast::EnumVariantFields::Tuple(tuple_items) => {
            first_memory_expr_context_from_exprs(tuple_items, base)
        }
        crate::ast::EnumVariantFields::Struct(named_fields) => {
            first_memory_expr_context_from_pairs(named_fields, base)
        }
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
                format!("{}<{}>", name, join_formatted_type_list(generics, ", "))
            }
        }
        Type::Tuple(items, _) => format!("({})", join_formatted_type_list(items, ", ")),
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
        Type::Function {
            params,
            return_type,
            ..
        } => format!(
            "fn({}) -> {}",
            join_formatted_type_list(params, ", "),
            format_type(return_type)
        ),
        Type::Option(inner, _) => format!("{}?", format_type(inner)),
        Type::Result(ok, err, _) => format!("{}!{}", format_type(ok), format_type(err)),
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
                format!(
                    "impl {}<{}>",
                    trait_name,
                    join_formatted_type_list(generics, ", ")
                )
            }
        }
    }
}

fn join_formatted_type_list(types: &[Type], separator: &str) -> String {
    let mut out = String::new();
    let mut first = true;
    for ty in types {
        if !first {
            out.push_str(separator);
        }
        first = false;
        out.push_str(&format_type(ty));
    }
    out
}
