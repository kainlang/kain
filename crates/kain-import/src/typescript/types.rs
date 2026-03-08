//! TypeScript → KAIN type mapping
//!
//! This module handles the conversion of TypeScript type annotations to KAIN types.

use kain_core::ast::Type;
use kain_core::span::Span;
use swc_ecma_ast::{TsType, TsKeywordType, TsKeywordTypeKind, TsTypeRef, TsArrayType, TsUnionOrIntersectionType};
use crate::{ImportError, Result};

pub struct TypeMapper;

impl TypeMapper {
    /// Map a TypeScript type to a KAIN type.
    pub fn map_type(ts_type: &TsType, span: Span) -> Result<Type> {
        match ts_type {
            // Keyword types (primitives)
            TsType::TsKeywordType(kw) => Self::map_keyword_type(kw, span),
            
            // Type references (e.g., Array<T>, custom types)
            TsType::TsTypeRef(type_ref) => Self::map_type_ref(type_ref, span),
            
            // Array types (T[])
            TsType::TsArrayType(array) => Self::map_array_type(array, span),
            
            // Union types (T | U)
            TsType::TsUnionOrIntersectionType(union_or_intersection) => {
                Self::map_union_or_intersection(union_or_intersection, span)
            }
            
            // Tuple types ([T, U, V])
            TsType::TsTupleType(_tuple) => {
                // TODO: Map to KAIN tuple type
                Ok(Type::Infer(span))
            }
            
            // Function types ((a: T) => U)
            TsType::TsFnOrConstructorType(_) => {
                // TODO: Map to KAIN function type
                Ok(Type::Infer(span))
            }
            
            // Literal types (e.g., "hello", 42, true)
            TsType::TsLitType(_) => {
                // TODO: Map to KAIN literal type
                Ok(Type::Infer(span))
            }
            
            // Fallback: use type inference
            _ => Ok(Type::Infer(span)),
        }
    }

    fn map_keyword_type(kw: &TsKeywordType, span: Span) -> Result<Type> {
        match kw.kind {
            TsKeywordTypeKind::TsNumberKeyword => Ok(Type::Named {
                name: "Float".to_string(),
                generics: vec![],
                span,
            }),
            TsKeywordTypeKind::TsStringKeyword => Ok(Type::Named {
                name: "String".to_string(),
                generics: vec![],
                span,
            }),
            TsKeywordTypeKind::TsBooleanKeyword => Ok(Type::Named {
                name: "Bool".to_string(),
                generics: vec![],
                span,
            }),
            TsKeywordTypeKind::TsVoidKeyword => Ok(Type::Unit(span)),
            TsKeywordTypeKind::TsUndefinedKeyword => Ok(Type::Named {
                name: "None".to_string(),
                generics: vec![],
                span,
            }),
            TsKeywordTypeKind::TsNullKeyword => Ok(Type::Named {
                name: "None".to_string(),
                generics: vec![],
                span,
            }),
            TsKeywordTypeKind::TsAnyKeyword => Ok(Type::Infer(span)),
            TsKeywordTypeKind::TsUnknownKeyword => Ok(Type::Infer(span)),
            TsKeywordTypeKind::TsNeverKeyword => Ok(Type::Never(span)),
            _ => Ok(Type::Infer(span)),
        }
    }

    fn map_type_ref(type_ref: &TsTypeRef, span: Span) -> Result<Type> {
        // Extract type name
        let type_name = match &type_ref.type_name {
            swc_ecma_ast::TsEntityName::Ident(ident) => ident.sym.to_string(),
            swc_ecma_ast::TsEntityName::TsQualifiedName(_) => {
                // TODO: Handle qualified names (e.g., Namespace.Type)
                return Ok(Type::Infer(span));
            }
        };

        // Handle generic types (e.g., Array<T>, Promise<T>)
        if let Some(type_params) = &type_ref.type_params {
            if type_name == "Array" && type_params.params.len() == 1 {
                let elem_type = Self::map_type(&type_params.params[0], span)?;
                // KAIN uses Slice for dynamic arrays
                return Ok(Type::Slice(Box::new(elem_type), span));
            }
            
            if type_name == "Promise" && type_params.params.len() == 1 {
                // Promise<T> → async function returning T
                // For now, just return the inner type
                return Self::map_type(&type_params.params[0], span);
            }
            
            // Generic type with parameters
            let generic_types = type_params.params.iter()
                .map(|p| Self::map_type(p, span))
                .collect::<Result<Vec<_>>>()?;
            
            return Ok(Type::Named {
                name: type_name,
                generics: generic_types,
                span,
            });
        }

        // Default: use the type name as-is
        Ok(Type::Named {
            name: type_name,
            generics: vec![],
            span,
        })
    }

    fn map_array_type(array: &TsArrayType, span: Span) -> Result<Type> {
        let elem_type = Self::map_type(&array.elem_type, span)?;
        // KAIN uses Slice for dynamic arrays
        Ok(Type::Slice(Box::new(elem_type), span))
    }

    fn map_union_or_intersection(
        union_or_intersection: &TsUnionOrIntersectionType,
        span: Span,
    ) -> Result<Type> {
        match union_or_intersection {
            TsUnionOrIntersectionType::TsUnionType(_union) => {
                // TODO: Map union types to KAIN enum
                // For now, use type inference
                Ok(Type::Infer(span))
            }
            TsUnionOrIntersectionType::TsIntersectionType(_intersection) => {
                // TODO: Map intersection types to KAIN struct
                // For now, use type inference
                Ok(Type::Infer(span))
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // TODO: Add unit tests for type mapping
}
