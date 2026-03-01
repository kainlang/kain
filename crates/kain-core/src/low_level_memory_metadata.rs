use crate::ast::{Attribute, Expr};
use crate::span::Span;

pub const C_UNION_ATTR: &str = "c_union";
pub const C_BITFIELD_ATTR: &str = "c_bitfield";

pub fn marker_attr(name: &str, span: Span) -> Attribute {
    Attribute {
        name: name.to_string(),
        args: Vec::new(),
        span,
    }
}

pub fn usize_attr(name: &str, value: usize, span: Span) -> Attribute {
    Attribute {
        name: name.to_string(),
        args: vec![Expr::Int(value as i64, span)],
        span,
    }
}

pub fn has_attr(attrs: &[Attribute], name: &str) -> bool {
    attrs.iter().any(|attr| attr.name == name)
}

pub fn attr_usize_arg(attrs: &[Attribute], name: &str) -> Option<usize> {
    attrs.iter().find_map(|attr| {
        if attr.name != name {
            return None;
        }
        match attr.args.first() {
            Some(Expr::Int(value, _)) if *value >= 0 => Some(*value as usize),
            _ => None,
        }
    })
}
