use crate::ast::{Block, Expr, Stmt, Type};
use crate::diagnostic_registry::DiagnosticCode;
use crate::error::{DiagnosticBuilder, ErrorKind, KainResult};
use crate::types::{TypedItem, TypedProgram};
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
