use crate::ast::Type;
use crate::diagnostic_registry::DiagnosticCode;
use crate::error::{DiagnosticBuilder, ErrorKind, KainResult};
use crate::types::{TypedItem, TypedProgram};
use crate::CompileTarget;

#[derive(Debug, Clone, Copy)]
pub struct BackendMemoryCapabilities {
    pub raw_pointers: bool,
}

const TS_MEMORY_CAPS: BackendMemoryCapabilities = BackendMemoryCapabilities {
    raw_pointers: false,
};
const UE5_MEMORY_CAPS: BackendMemoryCapabilities = BackendMemoryCapabilities {
    raw_pointers: false,
};
const DEFAULT_MEMORY_CAPS: BackendMemoryCapabilities = BackendMemoryCapabilities {
    raw_pointers: true,
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
    if caps.raw_pointers {
        return Ok(());
    }

    if let Some(context) = first_pointer_context(&program.items) {
        return Err(
            DiagnosticBuilder::new(
                ErrorKind::Validation,
                DiagnosticCode::MemoryUnsupportedBackend,
                format!(
                    "Target '{:?}' does not currently support raw pointer types in normalized codegen.",
                    target
                ),
            )
            .context(context)
            .build(),
        );
    }

    Ok(())
}

fn first_pointer_context(items: &[TypedItem]) -> Option<String> {
    for item in items {
        match item {
            TypedItem::Function(f) => {
                if let Some(context) = first_pointer_context_in_function(f) {
                    return Some(context);
                }
            }
            TypedItem::Struct(s) => {
                for field in &s.ast.fields {
                    if field.ty.contains_raw_ptr() {
                        return Some(format!(
                            "Struct '{}' field '{}' uses a raw pointer type",
                            s.ast.name, field.name
                        ));
                    }
                }
            }
            TypedItem::Component(c) => {
                for prop in &c.ast.props {
                    if prop.ty.contains_raw_ptr() {
                        return Some(format!(
                            "Component '{}' prop '{}' uses a raw pointer type",
                            c.ast.name, prop.name
                        ));
                    }
                }
            }
            TypedItem::Actor(a) => {
                for state in &a.ast.state {
                    if state.ty.contains_raw_ptr() {
                        return Some(format!(
                            "Actor '{}' state '{}' uses a raw pointer type",
                            a.ast.name, state.name
                        ));
                    }
                }
            }
            TypedItem::Const(c) => {
                if c.ast.ty.contains_raw_ptr() {
                    return Some(format!(
                        "Const '{}' uses a raw pointer type",
                        c.ast.name
                    ));
                }
            }
            TypedItem::TypeAlias(alias) => {
                if alias.ast.target.contains_raw_ptr() {
                    return Some(format!(
                        "Type alias '{}' uses a raw pointer type",
                        alias.ast.name
                    ));
                }
            }
            TypedItem::Impl(imp) => {
                for method in &imp.ast.methods {
                    if let Some(context) = first_pointer_context_in_ast_function(method) {
                        return Some(context);
                    }
                }
            }
            _ => {}
        }
    }

    None
}

fn first_pointer_context_in_function(function: &crate::types::TypedFunction) -> Option<String> {
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
    None
}

fn first_pointer_context_in_ast_function(function: &crate::ast::Function) -> Option<String> {
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
    None
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
