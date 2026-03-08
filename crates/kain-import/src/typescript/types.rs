//! TypeScript -> KAIN type mapping.
//!
//! The mapping is intentionally table-driven for the stable primitive/container
//! cases so the importer can stay aligned with the existing web TypeScript
//! backend without scattering string checks throughout the transformer.

use kain_core::ast::Type;
use kain_core::effects::Effect;
use kain_core::span::Span;
use swc_ecma_ast as ts;

use crate::Result;

const PRIMITIVE_TYPE_MAP: &[(ts::TsKeywordTypeKind, &str)] = &[
    (ts::TsKeywordTypeKind::TsNumberKeyword, "Float"),
    (ts::TsKeywordTypeKind::TsStringKeyword, "String"),
    (ts::TsKeywordTypeKind::TsBooleanKeyword, "Bool"),
    (ts::TsKeywordTypeKind::TsBigIntKeyword, "Int"),
];

const NULLISH_KINDS: &[ts::TsKeywordTypeKind] = &[
    ts::TsKeywordTypeKind::TsNullKeyword,
    ts::TsKeywordTypeKind::TsUndefinedKeyword,
];

pub struct TypeMapper;

impl TypeMapper {
    pub fn new() -> Self {
        Self
    }

    pub fn map_type(&self, ts_type: &ts::TsType, span: Span) -> Result<Type> {
        let mapped = match ts_type {
            ts::TsType::TsKeywordType(keyword) => self.map_keyword_type(keyword.kind, span),
            ts::TsType::TsTypeRef(type_ref) => self.map_type_ref(type_ref, span)?,
            ts::TsType::TsArrayType(array) => {
                Type::Slice(Box::new(self.map_type(&array.elem_type, span)?), span)
            }
            ts::TsType::TsTupleType(tuple) => Type::Tuple(
                tuple
                    .elem_types
                    .iter()
                    .map(|elem| self.map_type(&elem.ty, span))
                    .collect::<Result<Vec<_>>>()?,
                span,
            ),
            ts::TsType::TsUnionOrIntersectionType(union_or_intersection) => {
                self.map_union_or_intersection(union_or_intersection, span)?
            }
            ts::TsType::TsOptionalType(optional) => {
                Type::Option(Box::new(self.map_type(&optional.type_ann, span)?), span)
            }
            ts::TsType::TsFnOrConstructorType(function) => self.map_function_type(function, span)?,
            ts::TsType::TsParenthesizedType(paren) => self.map_type(&paren.type_ann, span)?,
            ts::TsType::TsLitType(lit) => match &lit.lit {
                ts::TsLit::Number(_) => Type::Named {
                    name: "Float".to_string(),
                    generics: Vec::new(),
                    span,
                },
                ts::TsLit::Str(_) => Type::Named {
                    name: "String".to_string(),
                    generics: Vec::new(),
                    span,
                },
                ts::TsLit::Bool(_) => Type::Named {
                    name: "Bool".to_string(),
                    generics: Vec::new(),
                    span,
                },
                ts::TsLit::BigInt(_) => Type::Named {
                    name: "Int".to_string(),
                    generics: Vec::new(),
                    span,
                },
                ts::TsLit::Tpl(_) => Type::Named {
                    name: "String".to_string(),
                    generics: Vec::new(),
                    span,
                },
            },
            ts::TsType::TsThisType(_) => Type::Named {
                name: "Self".to_string(),
                generics: Vec::new(),
                span,
            },
            _ => Type::Infer(span),
        };

        Ok(mapped)
    }

    fn map_keyword_type(&self, kind: ts::TsKeywordTypeKind, span: Span) -> Type {
        if let Some((_, name)) = PRIMITIVE_TYPE_MAP.iter().find(|(key, _)| *key == kind) {
            return Type::Named {
                name: (*name).to_string(),
                generics: Vec::new(),
                span,
            };
        }

        match kind {
            ts::TsKeywordTypeKind::TsVoidKeyword => Type::Unit(span),
            ts::TsKeywordTypeKind::TsNeverKeyword => Type::Never(span),
            ts::TsKeywordTypeKind::TsNullKeyword | ts::TsKeywordTypeKind::TsUndefinedKeyword => {
                Type::Option(Box::new(Type::Infer(span)), span)
            }
            _ => Type::Infer(span),
        }
    }

    fn map_type_ref(&self, type_ref: &ts::TsTypeRef, span: Span) -> Result<Type> {
        let name = entity_name_to_string(&type_ref.type_name);
        let generics = type_ref
            .type_params
            .as_ref()
            .map(|params| {
                params
                    .params
                    .iter()
                    .map(|param| self.map_type(param, span))
                    .collect::<Result<Vec<_>>>()
            })
            .transpose()?
            .unwrap_or_default();

        match name.as_str() {
            "Array" if generics.len() == 1 => Ok(Type::Slice(Box::new(generics[0].clone()), span)),
            "Promise" if generics.len() == 1 => Ok(Type::Impl {
                trait_name: "Async".to_string(),
                generics,
                span,
            }),
            "ReadonlyArray" if generics.len() == 1 => {
                Ok(Type::Slice(Box::new(generics[0].clone()), span))
            }
            _ => Ok(Type::Named { name, generics, span }),
        }
    }

    fn map_union_or_intersection(
        &self,
        ty: &ts::TsUnionOrIntersectionType,
        span: Span,
    ) -> Result<Type> {
        match ty {
            ts::TsUnionOrIntersectionType::TsUnionType(union) => {
                let mut mapped = Vec::new();
                let mut non_nullish = Vec::new();

                for member in &union.types {
                    mapped.push(self.map_type(member, span)?);
                    if !is_nullish_type(member) {
                        non_nullish.push(member);
                    }
                }

                if non_nullish.len() == 1 && union.types.len() > 1 {
                    return Ok(Type::Option(
                        Box::new(self.map_type(non_nullish[0], span)?),
                        span,
                    ));
                }

                if mapped.iter().all(|ty| ty == &mapped[0]) {
                    return Ok(mapped[0].clone());
                }

                Ok(Type::Infer(span))
            }
            ts::TsUnionOrIntersectionType::TsIntersectionType(intersection) => {
                if intersection.types.len() == 1 {
                    self.map_type(&intersection.types[0], span)
                } else {
                    Ok(Type::Infer(span))
                }
            }
        }
    }

    fn map_function_type(&self, function: &ts::TsFnOrConstructorType, span: Span) -> Result<Type> {
        match function {
            ts::TsFnOrConstructorType::TsFnType(fn_type) => Ok(Type::Function {
                params: fn_type
                    .params
                    .iter()
                    .map(|param| self.map_ts_fn_param(param, span))
                    .collect::<Result<Vec<_>>>()?,
                return_type: Box::new(self.map_type(&fn_type.type_ann.type_ann, span)?),
                effects: Vec::new(),
                span,
            }),
            ts::TsFnOrConstructorType::TsConstructorType(ctor) => Ok(Type::Function {
                params: ctor
                    .params
                    .iter()
                    .map(|param| self.map_ts_fn_param(param, span))
                    .collect::<Result<Vec<_>>>()?,
                return_type: Box::new(self.map_type(&ctor.type_ann.type_ann, span)?),
                effects: vec![Effect::Unsafe],
                span,
            }),
        }
    }

    fn map_ts_fn_param(&self, param: &ts::TsFnParam, span: Span) -> Result<Type> {
        let ty = match param {
            ts::TsFnParam::Ident(ident) => ident
                .type_ann
                .as_ref()
                .map(|ann| self.map_type(&ann.type_ann, span))
                .transpose()?
                .unwrap_or(Type::Infer(span)),
            ts::TsFnParam::Array(array) => array
                .type_ann
                .as_ref()
                .map(|ann| self.map_type(&ann.type_ann, span))
                .transpose()?
                .unwrap_or(Type::Infer(span)),
            ts::TsFnParam::Object(object) => object
                .type_ann
                .as_ref()
                .map(|ann| self.map_type(&ann.type_ann, span))
                .transpose()?
                .unwrap_or(Type::Infer(span)),
            ts::TsFnParam::Rest(rest) => rest
                .type_ann
                .as_ref()
                .map(|ann| self.map_type(&ann.type_ann, span))
                .transpose()?
                .unwrap_or(Type::Infer(span)),
        };

        Ok(ty)
    }
}

fn entity_name_to_string(entity: &ts::TsEntityName) -> String {
    match entity {
        ts::TsEntityName::Ident(ident) => ident.sym.to_string(),
        ts::TsEntityName::TsQualifiedName(name) => {
            format!("{}.{}", entity_name_to_string(&name.left), name.right.sym)
        }
    }
}

fn is_nullish_type(ty: &ts::TsType) -> bool {
    match ty {
        ts::TsType::TsKeywordType(keyword) => NULLISH_KINDS.contains(&keyword.kind),
        _ => false,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn maps_optional_union_to_option() {
        let mapper = TypeMapper::new();
        let span = Span::default();
        let ty = ts::TsType::TsUnionOrIntersectionType(ts::TsUnionOrIntersectionType::TsUnionType(
            ts::TsUnionType {
                span: swc_common::DUMMY_SP,
                types: vec![
                    Box::new(ts::TsType::TsKeywordType(ts::TsKeywordType {
                        span: swc_common::DUMMY_SP,
                        kind: ts::TsKeywordTypeKind::TsStringKeyword,
                    })),
                    Box::new(ts::TsType::TsKeywordType(ts::TsKeywordType {
                        span: swc_common::DUMMY_SP,
                        kind: ts::TsKeywordTypeKind::TsNullKeyword,
                    })),
                ],
            },
        ));

        let mapped = mapper.map_type(&ty, span).unwrap();
        assert!(matches!(mapped, Type::Option(_, _)));
    }
}
