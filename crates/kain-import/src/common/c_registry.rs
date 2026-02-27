//! Data-driven C language mapping registry.
//!
//! This module centralizes C -> KAIN mapping tables so C importer logic can
//! consume shared data rather than duplicating match arms.

use kain_core::ast::{BinaryOp, Type};
use kain_core::span::Span;
use lang_c::ast as c_ast;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum KainTypeDescriptor {
    Unit,
    Named(&'static str),
}

const C_TYPE_NAME_ALIASES: &[(&str, KainTypeDescriptor)] = &[
    // Integer families
    ("int", KainTypeDescriptor::Named("Int")),
    ("long", KainTypeDescriptor::Named("Int")),
    ("short", KainTypeDescriptor::Named("Int")),
    ("signed", KainTypeDescriptor::Named("Int")),
    ("unsigned", KainTypeDescriptor::Named("Int")),
    ("int8_t", KainTypeDescriptor::Named("Int")),
    ("int16_t", KainTypeDescriptor::Named("Int")),
    ("int32_t", KainTypeDescriptor::Named("Int")),
    ("int64_t", KainTypeDescriptor::Named("Int")),
    ("uint8_t", KainTypeDescriptor::Named("Int")),
    ("uint16_t", KainTypeDescriptor::Named("Int")),
    ("uint32_t", KainTypeDescriptor::Named("Int")),
    ("uint64_t", KainTypeDescriptor::Named("Int")),
    ("size_t", KainTypeDescriptor::Named("Int")),
    ("ptrdiff_t", KainTypeDescriptor::Named("Int")),
    // Floating point
    ("float", KainTypeDescriptor::Named("Float")),
    ("double", KainTypeDescriptor::Named("Float")),
    // Other builtins
    ("char", KainTypeDescriptor::Named("Char")),
    ("bool", KainTypeDescriptor::Named("Bool")),
    ("_Bool", KainTypeDescriptor::Named("Bool")),
    ("void", KainTypeDescriptor::Unit),
];

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum CBuiltinTypeSpecifier {
    Void,
    Char,
    Short,
    Int,
    Long,
    Signed,
    Unsigned,
    Float,
    Double,
    Bool,
}

const C_BUILTIN_TYPE_SPECIFIERS: &[(CBuiltinTypeSpecifier, KainTypeDescriptor)] = &[
    (CBuiltinTypeSpecifier::Void, KainTypeDescriptor::Unit),
    (CBuiltinTypeSpecifier::Char, KainTypeDescriptor::Named("Char")),
    (CBuiltinTypeSpecifier::Short, KainTypeDescriptor::Named("Int")),
    (CBuiltinTypeSpecifier::Int, KainTypeDescriptor::Named("Int")),
    (CBuiltinTypeSpecifier::Long, KainTypeDescriptor::Named("Int")),
    (CBuiltinTypeSpecifier::Signed, KainTypeDescriptor::Named("Int")),
    (CBuiltinTypeSpecifier::Unsigned, KainTypeDescriptor::Named("Int")),
    (CBuiltinTypeSpecifier::Float, KainTypeDescriptor::Named("Float")),
    (CBuiltinTypeSpecifier::Double, KainTypeDescriptor::Named("Float")),
    (CBuiltinTypeSpecifier::Bool, KainTypeDescriptor::Named("Bool")),
];

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CBinaryOperatorResolution {
    Supported(BinaryOp),
    UnsupportedAssignment,
    Unsupported,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum CBinaryOperatorKey {
    Plus,
    Minus,
    Multiply,
    Divide,
    Modulo,
    Equals,
    NotEquals,
    Less,
    Greater,
    LessOrEqual,
    GreaterOrEqual,
    LogicalAnd,
    LogicalOr,
    BitwiseAnd,
    BitwiseOr,
    BitwiseXor,
    ShiftLeft,
    ShiftRight,
    Assign,
    AssignPlus,
    AssignMinus,
    AssignMultiply,
    AssignDivide,
    AssignModulo,
    AssignShiftLeft,
    AssignShiftRight,
    AssignBitwiseAnd,
    AssignBitwiseOr,
    AssignBitwiseXor,
}

const C_BINARY_OPERATOR_MAPPINGS: &[(CBinaryOperatorKey, BinaryOp)] = &[
    (CBinaryOperatorKey::Plus, BinaryOp::Add),
    (CBinaryOperatorKey::Minus, BinaryOp::Sub),
    (CBinaryOperatorKey::Multiply, BinaryOp::Mul),
    (CBinaryOperatorKey::Divide, BinaryOp::Div),
    (CBinaryOperatorKey::Modulo, BinaryOp::Mod),
    (CBinaryOperatorKey::Equals, BinaryOp::Eq),
    (CBinaryOperatorKey::NotEquals, BinaryOp::Ne),
    (CBinaryOperatorKey::Less, BinaryOp::Lt),
    (CBinaryOperatorKey::Greater, BinaryOp::Gt),
    (CBinaryOperatorKey::LessOrEqual, BinaryOp::Le),
    (CBinaryOperatorKey::GreaterOrEqual, BinaryOp::Ge),
    (CBinaryOperatorKey::LogicalAnd, BinaryOp::And),
    (CBinaryOperatorKey::LogicalOr, BinaryOp::Or),
    (CBinaryOperatorKey::BitwiseAnd, BinaryOp::BitAnd),
    (CBinaryOperatorKey::BitwiseOr, BinaryOp::BitOr),
    (CBinaryOperatorKey::BitwiseXor, BinaryOp::BitXor),
    (CBinaryOperatorKey::ShiftLeft, BinaryOp::Shl),
    (CBinaryOperatorKey::ShiftRight, BinaryOp::Shr),
    (CBinaryOperatorKey::Assign, BinaryOp::Assign),
    (CBinaryOperatorKey::AssignPlus, BinaryOp::AddAssign),
    (CBinaryOperatorKey::AssignMinus, BinaryOp::SubAssign),
    (CBinaryOperatorKey::AssignMultiply, BinaryOp::MulAssign),
    (CBinaryOperatorKey::AssignDivide, BinaryOp::DivAssign),
];

const C_UNSUPPORTED_ASSIGNMENT_OPERATORS: &[CBinaryOperatorKey] = &[
    CBinaryOperatorKey::AssignModulo,
    CBinaryOperatorKey::AssignShiftLeft,
    CBinaryOperatorKey::AssignShiftRight,
    CBinaryOperatorKey::AssignBitwiseAnd,
    CBinaryOperatorKey::AssignBitwiseOr,
    CBinaryOperatorKey::AssignBitwiseXor,
];

const C_COMPOUND_ASSIGNMENT_BINARY_MAPPINGS: &[(CBinaryOperatorKey, BinaryOp)] = &[
    (CBinaryOperatorKey::AssignPlus, BinaryOp::Add),
    (CBinaryOperatorKey::AssignMinus, BinaryOp::Sub),
    (CBinaryOperatorKey::AssignMultiply, BinaryOp::Mul),
    (CBinaryOperatorKey::AssignDivide, BinaryOp::Div),
    (CBinaryOperatorKey::AssignModulo, BinaryOp::Mod),
    (CBinaryOperatorKey::AssignShiftLeft, BinaryOp::Shl),
    (CBinaryOperatorKey::AssignShiftRight, BinaryOp::Shr),
    (CBinaryOperatorKey::AssignBitwiseAnd, BinaryOp::BitAnd),
    (CBinaryOperatorKey::AssignBitwiseOr, BinaryOp::BitOr),
    (CBinaryOperatorKey::AssignBitwiseXor, BinaryOp::BitXor),
];

pub fn c_type_name_aliases() -> &'static [(&'static str, KainTypeDescriptor)] {
    C_TYPE_NAME_ALIASES
}

pub fn materialize_type_descriptor(descriptor: KainTypeDescriptor) -> Type {
    match descriptor {
        KainTypeDescriptor::Unit => Type::Unit(Span::default()),
        KainTypeDescriptor::Named(name) => named_type(name),
    }
}

pub fn named_type(name: &str) -> Type {
    Type::Named {
        name: name.to_string(),
        generics: Vec::new(),
        span: Span::default(),
    }
}

pub fn map_c_builtin_type_specifier(spec: &c_ast::TypeSpecifier) -> Option<Type> {
    let key = match spec {
        c_ast::TypeSpecifier::Void => CBuiltinTypeSpecifier::Void,
        c_ast::TypeSpecifier::Char => CBuiltinTypeSpecifier::Char,
        c_ast::TypeSpecifier::Short => CBuiltinTypeSpecifier::Short,
        c_ast::TypeSpecifier::Int => CBuiltinTypeSpecifier::Int,
        c_ast::TypeSpecifier::Long => CBuiltinTypeSpecifier::Long,
        c_ast::TypeSpecifier::Signed => CBuiltinTypeSpecifier::Signed,
        c_ast::TypeSpecifier::Unsigned => CBuiltinTypeSpecifier::Unsigned,
        c_ast::TypeSpecifier::Float => CBuiltinTypeSpecifier::Float,
        c_ast::TypeSpecifier::Double => CBuiltinTypeSpecifier::Double,
        c_ast::TypeSpecifier::Bool => CBuiltinTypeSpecifier::Bool,
        _ => return None,
    };

    C_BUILTIN_TYPE_SPECIFIERS
        .iter()
        .find_map(|(candidate, descriptor)| {
            if *candidate == key {
                Some(materialize_type_descriptor(*descriptor))
            } else {
                None
            }
        })
}

pub fn resolve_c_binary_operator(op: &c_ast::BinaryOperator) -> CBinaryOperatorResolution {
    let Some(key) = c_binary_operator_key(op) else {
        return CBinaryOperatorResolution::Unsupported;
    };

    if C_UNSUPPORTED_ASSIGNMENT_OPERATORS.contains(&key) {
        return CBinaryOperatorResolution::UnsupportedAssignment;
    }

    match C_BINARY_OPERATOR_MAPPINGS
        .iter()
        .find_map(|(candidate, mapped)| if *candidate == key { Some(*mapped) } else { None })
    {
        Some(mapped) => CBinaryOperatorResolution::Supported(mapped),
        None => CBinaryOperatorResolution::Unsupported,
    }
}

pub fn resolve_c_compound_assignment_binary_operator(op: &c_ast::BinaryOperator) -> Option<BinaryOp> {
    let key = c_binary_operator_key(op)?;
    C_COMPOUND_ASSIGNMENT_BINARY_MAPPINGS
        .iter()
        .find_map(|(candidate, mapped)| if *candidate == key { Some(*mapped) } else { None })
}

fn c_binary_operator_key(op: &c_ast::BinaryOperator) -> Option<CBinaryOperatorKey> {
    let key = match op {
        c_ast::BinaryOperator::Plus => CBinaryOperatorKey::Plus,
        c_ast::BinaryOperator::Minus => CBinaryOperatorKey::Minus,
        c_ast::BinaryOperator::Multiply => CBinaryOperatorKey::Multiply,
        c_ast::BinaryOperator::Divide => CBinaryOperatorKey::Divide,
        c_ast::BinaryOperator::Modulo => CBinaryOperatorKey::Modulo,
        c_ast::BinaryOperator::Equals => CBinaryOperatorKey::Equals,
        c_ast::BinaryOperator::NotEquals => CBinaryOperatorKey::NotEquals,
        c_ast::BinaryOperator::Less => CBinaryOperatorKey::Less,
        c_ast::BinaryOperator::Greater => CBinaryOperatorKey::Greater,
        c_ast::BinaryOperator::LessOrEqual => CBinaryOperatorKey::LessOrEqual,
        c_ast::BinaryOperator::GreaterOrEqual => CBinaryOperatorKey::GreaterOrEqual,
        c_ast::BinaryOperator::LogicalAnd => CBinaryOperatorKey::LogicalAnd,
        c_ast::BinaryOperator::LogicalOr => CBinaryOperatorKey::LogicalOr,
        c_ast::BinaryOperator::BitwiseAnd => CBinaryOperatorKey::BitwiseAnd,
        c_ast::BinaryOperator::BitwiseOr => CBinaryOperatorKey::BitwiseOr,
        c_ast::BinaryOperator::BitwiseXor => CBinaryOperatorKey::BitwiseXor,
        c_ast::BinaryOperator::ShiftLeft => CBinaryOperatorKey::ShiftLeft,
        c_ast::BinaryOperator::ShiftRight => CBinaryOperatorKey::ShiftRight,
        c_ast::BinaryOperator::Assign => CBinaryOperatorKey::Assign,
        c_ast::BinaryOperator::AssignPlus => CBinaryOperatorKey::AssignPlus,
        c_ast::BinaryOperator::AssignMinus => CBinaryOperatorKey::AssignMinus,
        c_ast::BinaryOperator::AssignMultiply => CBinaryOperatorKey::AssignMultiply,
        c_ast::BinaryOperator::AssignDivide => CBinaryOperatorKey::AssignDivide,
        c_ast::BinaryOperator::AssignModulo => CBinaryOperatorKey::AssignModulo,
        c_ast::BinaryOperator::AssignShiftLeft => CBinaryOperatorKey::AssignShiftLeft,
        c_ast::BinaryOperator::AssignShiftRight => CBinaryOperatorKey::AssignShiftRight,
        c_ast::BinaryOperator::AssignBitwiseAnd => CBinaryOperatorKey::AssignBitwiseAnd,
        c_ast::BinaryOperator::AssignBitwiseOr => CBinaryOperatorKey::AssignBitwiseOr,
        c_ast::BinaryOperator::AssignBitwiseXor => CBinaryOperatorKey::AssignBitwiseXor,
        c_ast::BinaryOperator::Index => return None,
    };

    Some(key)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn maps_builtin_c_type_specifier() {
        assert_eq!(
            map_c_builtin_type_specifier(&c_ast::TypeSpecifier::Int),
            Some(named_type("Int"))
        );
    }

    #[test]
    fn resolves_supported_binary_operator() {
        assert_eq!(
            resolve_c_binary_operator(&c_ast::BinaryOperator::AssignPlus),
            CBinaryOperatorResolution::Supported(BinaryOp::AddAssign)
        );
    }

    #[test]
    fn identifies_unsupported_assignment_operator() {
        assert_eq!(
            resolve_c_binary_operator(&c_ast::BinaryOperator::AssignBitwiseOr),
            CBinaryOperatorResolution::UnsupportedAssignment
        );
    }

    #[test]
    fn resolves_compound_assignment_lowering_operator() {
        assert_eq!(
            resolve_c_compound_assignment_binary_operator(&c_ast::BinaryOperator::AssignShiftLeft),
            Some(BinaryOp::Shl)
        );
        assert_eq!(
            resolve_c_compound_assignment_binary_operator(&c_ast::BinaryOperator::AssignModulo),
            Some(BinaryOp::Mod)
        );
    }
}
