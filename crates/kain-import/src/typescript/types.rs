//! TypeScript → KAIN type mapping
//!
//! This module handles the conversion of TypeScript type annotations to KAIN types.

use kain_core::ast::{Type, TypeKind};
use kain_core::span::Span;
use swc_ecma_ast::{TsType, TsKeywordType, TsKeywordTypeKind, TsTypeRef, TsArrayType, TsUnionOrIntersectionType};
use crate::{ImportError, Result};

pub struct TypeMapper;

impl TypeMapper {
    /// Map a TypeScript type to a KAIN type.
    pub fn map_type(ts_type: &TsType) -> Result<Type> {
        let span = Span::default();
        
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
            TsType::TsTupleType(tuple) => {
                // TODO: Map to KAIN tuple type
                Ok(Type {
                    kind: TypeKind::Infer,
                    span,
                })
            }
            
            // Function types ((a: T) => U)
            TsType::TsFnOrConstructorType(_) => {
                // TODO: Map to KAIN function type
                Ok(Type {
                    kind: TypeKind::Infer,
                    span,
                })
            }
            
            // Literal types (e.g., "hello", 42, true)
            TsType::TsLitType(_) => {
                // TODO: Map to KAIN literal type
                Ok(Type {
                    kind: TypeKind::Infer,
                    span,
                })
            }
            
            // Fallback: use type inference
            _ => Ok(Type {
                kind: TypeKind::Infer,
                span,
            }),
        }
    }

    fn map_keyword_type(kw: &TsKeywordType, span: Span) -> Result<Type> {
        let kind = match kw.kind {
            TsKeywordTypeKind::TsNumberKeyword => TypeKind::Named("Float".to_string()),
            TsKeywordTypeKind::TsStringKeyword => TypeKind::Named("String".to_string()),
            TsKeywordTypeKind::TsBooleanKeyword => TypeKind::Named("Bool".to_string()),
            TsKeywordTypeKind::TsVoidKeyword => TypeKind::Unit,
            TsKeywordTypeKind::TsUndefinedKeyword => TypeKind::Named("None".to_string()),
            TsKeywordTypeKind::TsNullKeyword => TypeKind::Named("None".to_string()),
            TsKeywordTypeKind::TsAnyKeyword => TypeKind::Infer,
            TsKeywordTypeKind::TsUnknownKeyword => TypeKind::Infer,
            TsKeywordTypeKind::TsNeverKeyword => TypeKind::Named("Never".to_string()),
            _ => TypeKind::Infer,
        };
        
        Ok(Type { kind, span })
    }

    fn map_type_ref(type_ref: &TsTypeRef, span: Span) -> Result<Type> {
        // Extract type name
        let type_name = match &type_ref.type_name {
            swc_ecma_ast::TsEntityName::Ident(ident) => ident.sym.to_string(),
            swc_ecma_ast::TsEntityName::TsQualifiedName(_) => {
                // TODO: Handle qualified names (e.g., Namespace.Type)
                return Ok(Type {
                    kind: TypeKind::Infer,
                    span,
                });
            }
        };

        // Handle generic types (e.g., Array<T>, Promise<T>)
        if let Some(type_params) = &type_ref.type_params {
            if type_name == "Array" && type_params.params.len() == 1 {
                let elem_type = Self::map_type(&type_params.params[0])?;
                return Ok(Type {
                    kind: TypeKind::Array(Box::new(elem_type)),
                    span,
                });
            }
            
            if type_name == "Promise" && type_params.params.len() == 1 {
                // Promise<T> → async function returning T
                // For now, just return the inner type
                return Self::map_type(&type_params.params[0]);
            }
        }

        // Default: use the type name as-is
        Ok(Type {
            kind: TypeKind::Named(type_name),
            span,
        })
    }

    fn map_array_type(array: &TsArrayType, span: Span) -> Result<Type> {
        let elem_type = Self::map_type(&array.elem_type)?;
        Ok(Type {
            kind: TypeKind::Array(Box::new(elem_type)),
            span,
        })
    }

    fn map_union_or_intersection(
        union_or_intersection: &TsUnionOrIntersectionType,
        span: Span,
    ) -> Result<Type> {
        match union_or_intersection {
            TsUnionOrIntersectionType::TsUnionType(_union) => {
                // TODO: Map union types to KAIN enum
                // For now, use type inference
                Ok(Type {
                    kind: TypeKind::Infer,
                    span,
                })
            }
            TsUnionOrIntersectionType::TsIntersectionType(_intersection) => {
                // TODO: Map intersection types to KAIN struct
                // For now, use type inference
                Ok(Type {
                    kind: TypeKind::Infer,
                    span,
                })
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // TODO: Add unit tests for type mapping
}
