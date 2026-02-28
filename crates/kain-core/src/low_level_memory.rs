use crate::ast::*;
use crate::diagnostic_registry::DiagnosticCode;
use crate::error::{DiagnosticBuilder, ErrorKind, KainResult};
use crate::monomorphize::MonomorphizedProgram;
use crate::span::Span;
use crate::types::{
    ResolvedType, TypedActor, TypedComponent, TypedConst, TypedEnum, TypedFunction, TypedImpl,
    TypedItem, TypedProgram, TypedStruct, TypedTypeAlias,
};
use crate::CompileTarget;

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
    if !matches!(target, CompileTarget::Ts | CompileTarget::Ue5 | CompileTarget::Ue5Editor) {
        return Ok(program.clone());
    }

    Ok(TypedProgram {
        items: program
            .items
            .iter()
            .map(|item| lower_typed_item_memory(item, target))
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

fn lower_typed_item_memory(item: &TypedItem, target: CompileTarget) -> TypedItem {
    match item {
        TypedItem::Function(function) => TypedItem::Function(TypedFunction {
            ast: lower_function_memory(&function.ast, target),
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
                        default: field.default.as_ref().map(|expr| lower_expr_memory(expr, target)),
                        ..field.clone()
                    })
                    .collect(),
                methods: struct_item
                    .ast
                    .methods
                    .iter()
                    .map(|method| lower_function_memory(method, target))
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
                        initial: lower_expr_memory(&state.initial, target),
                        ..state.clone()
                    })
                    .collect(),
                handlers: actor
                    .ast
                    .handlers
                    .iter()
                    .map(|handler| MessageHandler {
                        params: handler
                            .params
                            .iter()
                            .map(|param| Param {
                                ty: lower_type_memory(&param.ty),
                                ..param.clone()
                            })
                            .collect(),
                        body: lower_block_memory(&handler.body, target),
                        ..handler.clone()
                    })
                    .collect(),
                methods: actor
                    .ast
                    .methods
                    .iter()
                    .map(|method| lower_function_memory(method, target))
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
                value: lower_expr_memory(&constant.ast.value, target),
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
                    .map(|method| lower_function_memory(method, target))
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
                                            .map(|expr| lower_expr_memory(expr, target)),
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

fn lower_function_memory(function: &Function, target: CompileTarget) -> Function {
    Function {
        params: function
            .params
            .iter()
            .map(|param| Param {
                ty: lower_type_memory(&param.ty),
                ..param.clone()
            })
            .collect(),
        return_type: function.return_type.as_ref().map(lower_type_memory),
        body: lower_block_memory(&function.body, target),
        ..function.clone()
    }
}

fn lower_block_memory(block: &Block, target: CompileTarget) -> Block {
    Block {
        stmts: block
            .stmts
            .iter()
            .map(|stmt| lower_stmt_memory(stmt, target))
            .collect(),
        ..block.clone()
    }
}

fn lower_stmt_memory(stmt: &Stmt, target: CompileTarget) -> Stmt {
    match stmt {
        Stmt::Expr(expr) => Stmt::Expr(lower_expr_memory(expr, target)),
        Stmt::Let {
            pattern,
            ty,
            value,
            span,
        } => Stmt::Let {
            pattern: pattern.clone(),
            ty: ty.as_ref().map(lower_type_memory),
            value: value.as_ref().map(|expr| lower_expr_memory(expr, target)),
            span: *span,
        },
        Stmt::Return(value, span) => {
            Stmt::Return(value.as_ref().map(|expr| lower_expr_memory(expr, target)), *span)
        }
        Stmt::For {
            binding,
            iter,
            body,
            span,
        } => Stmt::For {
            binding: binding.clone(),
            iter: lower_expr_memory(iter, target),
            body: lower_block_memory(body, target),
            span: *span,
        },
        Stmt::While {
            condition,
            body,
            span,
        } => Stmt::While {
            condition: lower_expr_memory(condition, target),
            body: lower_block_memory(body, target),
            span: *span,
        },
        Stmt::Loop { body, span } => Stmt::Loop {
            body: lower_block_memory(body, target),
            span: *span,
        },
        _ => stmt.clone(),
    }
}

fn lower_expr_memory(expr: &Expr, target: CompileTarget) -> Expr {
    let span = expr.span();
    match expr {
        Expr::AddrOf { value, .. } => helper_call("__kain_addr_of", vec![lower_expr_memory(value, target)], span),
        Expr::PtrOffset {
            pointer,
            offset,
            element_ty,
            ..
        } => helper_call(
            "__kain_ptr_offset",
            vec![
                lower_expr_memory(pointer, target),
                lower_expr_memory(offset, target),
                Expr::Int(memory_stride_for_type(element_ty.as_ref()).unwrap_or(1), span),
            ],
            span,
        ),
        Expr::MemLoad {
            pointer,
            load_ty,
            ..
        } => {
            let call = helper_call("__kain_mem_load", vec![lower_expr_memory(pointer, target)], span);
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
            vec![lower_expr_memory(pointer, target), lower_expr_memory(value, target)],
            span,
        ),
        Expr::Ref { mutable, value, span } => Expr::Ref {
            mutable: *mutable,
            value: Box::new(lower_expr_memory(value, target)),
            span: *span,
        },
        Expr::Deref(inner, span) => Expr::Deref(Box::new(lower_expr_memory(inner, target)), *span),
        Expr::Binary {
            left,
            op,
            right,
            span,
        } => Expr::Binary {
            left: Box::new(lower_expr_memory(left, target)),
            op: op.clone(),
            right: Box::new(lower_expr_memory(right, target)),
            span: *span,
        },
        Expr::Unary { op, operand, span } => Expr::Unary {
            op: op.clone(),
            operand: Box::new(lower_expr_memory(operand, target)),
            span: *span,
        },
        Expr::Call { callee, args, span } => Expr::Call {
            callee: Box::new(lower_expr_memory(callee, target)),
            args: args
                .iter()
                .map(|arg| CallArg {
                    value: lower_expr_memory(&arg.value, target),
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
            receiver: Box::new(lower_expr_memory(receiver, target)),
            method: method.clone(),
            args: args
                .iter()
                .map(|arg| CallArg {
                    value: lower_expr_memory(&arg.value, target),
                    ..arg.clone()
                })
                .collect(),
            span: *span,
        },
        Expr::Field { object, field, span } => Expr::Field {
            object: Box::new(lower_expr_memory(object, target)),
            field: field.clone(),
            span: *span,
        },
        Expr::Index { object, index, span } => Expr::Index {
            object: Box::new(lower_expr_memory(object, target)),
            index: Box::new(lower_expr_memory(index, target)),
            span: *span,
        },
        Expr::Assign {
            target: assign_target,
            value,
            span,
        } => Expr::Assign {
            target: Box::new(lower_expr_memory(assign_target, target)),
            value: Box::new(lower_expr_memory(value, target)),
            span: *span,
        },
        Expr::Array(items, span) => Expr::Array(
            items.iter().map(|item| lower_expr_memory(item, target)).collect(),
            *span,
        ),
        Expr::Tuple(items, span) => Expr::Tuple(
            items.iter().map(|item| lower_expr_memory(item, target)).collect(),
            *span,
        ),
        Expr::Struct {
            name,
            fields,
            span,
        } => Expr::Struct {
            name: name.clone(),
            fields: fields
                .iter()
                .map(|(field, value)| (field.clone(), lower_expr_memory(value, target)))
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
                    items.iter().map(|item| lower_expr_memory(item, target)).collect(),
                ),
                EnumVariantFields::Struct(items) => EnumVariantFields::Struct(
                    items
                        .iter()
                        .map(|(field, value)| (field.clone(), lower_expr_memory(value, target)))
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
            condition: Box::new(lower_expr_memory(condition, target)),
            then_branch: lower_block_memory(then_branch, target),
            else_branch: else_branch
                .as_ref()
                .map(|branch| Box::new(lower_else_branch_memory(branch, target))),
            span: *span,
        },
        Expr::Match {
            scrutinee,
            arms,
            span,
        } => Expr::Match {
            scrutinee: Box::new(lower_expr_memory(scrutinee, target)),
            arms: arms
                .iter()
                .map(|arm| MatchArm {
                    body: lower_expr_memory(&arm.body, target),
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
            body: Box::new(lower_expr_memory(body, target)),
            span: *span,
        },
        Expr::Cast { value, target: ty, span } => Expr::Cast {
            value: Box::new(lower_expr_memory(value, target)),
            target: lower_type_memory(ty),
            span: *span,
        },
        Expr::Try(inner, span) => Expr::Try(Box::new(lower_expr_memory(inner, target)), *span),
        Expr::Await(inner, span) => Expr::Await(Box::new(lower_expr_memory(inner, target)), *span),
        Expr::Block(block, span) => Expr::Block(lower_block_memory(block, target), *span),
        Expr::Paren(inner, span) => Expr::Paren(Box::new(lower_expr_memory(inner, target)), *span),
        Expr::Return(Some(inner), span) => {
            Expr::Return(Some(Box::new(lower_expr_memory(inner, target))), *span)
        }
        Expr::Break(Some(inner), span) => {
            Expr::Break(Some(Box::new(lower_expr_memory(inner, target))), *span)
        }
        Expr::Spawn { actor, init, span } => Expr::Spawn {
            actor: actor.clone(),
            init: init
                .iter()
                .map(|(name, value)| (name.clone(), lower_expr_memory(value, target)))
                .collect(),
            span: *span,
        },
        Expr::SendMsg {
            target: msg_target,
            message,
            data,
            span,
        } => Expr::SendMsg {
            target: Box::new(lower_expr_memory(msg_target, target)),
            message: message.clone(),
            data: data
                .iter()
                .map(|(name, value)| (name.clone(), lower_expr_memory(value, target)))
                .collect(),
            span: *span,
        },
        Expr::Comptime(inner, span) => {
            Expr::Comptime(Box::new(lower_expr_memory(inner, target)), *span)
        }
        Expr::MacroCall { name, args, span } => Expr::MacroCall {
            name: name.clone(),
            args: args.iter().map(|arg| lower_expr_memory(arg, target)).collect(),
            span: *span,
        },
        Expr::Range {
            start,
            end,
            inclusive,
            span,
        } => Expr::Range {
            start: start.as_ref().map(|expr| Box::new(lower_expr_memory(expr, target))),
            end: end.as_ref().map(|expr| Box::new(lower_expr_memory(expr, target))),
            inclusive: *inclusive,
            span: *span,
        },
        Expr::FString(items, span) => Expr::FString(
            items.iter().map(|item| lower_expr_memory(item, target)).collect(),
            *span,
        ),
        _ => expr.clone(),
    }
}

fn lower_else_branch_memory(branch: &ElseBranch, target: CompileTarget) -> ElseBranch {
    match branch {
        ElseBranch::Else(block) => ElseBranch::Else(lower_block_memory(block, target)),
        ElseBranch::ElseIf(condition, block, next) => ElseBranch::ElseIf(
            Box::new(lower_expr_memory(condition, target)),
            lower_block_memory(block, target),
            next.as_ref()
                .map(|branch| Box::new(lower_else_branch_memory(branch, target))),
        ),
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

fn memory_stride_for_type(ty: Option<&Type>) -> Option<i64> {
    ty.map(estimate_type_size).and_then(|size| i64::try_from(size).ok())
}

fn estimate_type_size(ty: &Type) -> usize {
    match ty {
        Type::Named { name, .. } => match name.as_str() {
            "Bool" | "Char" => 1,
            "Int" | "isize" | "usize" => 8,
            "Float" => 8,
            _ => 8,
        },
        Type::Array(inner, size, _) => estimate_type_size(inner) * size,
        Type::Slice(_, _) => 16,
        Type::Tuple(types, _) => types.iter().map(estimate_type_size).sum(),
        Type::Ref { .. } | Type::Ptr { .. } => 8,
        Type::Option(inner, _) => estimate_type_size(inner),
        Type::Result(ok, err, _) => estimate_type_size(ok).max(estimate_type_size(err)),
        Type::Unit(_) | Type::Never(_) => 0,
        _ => 8,
    }
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
