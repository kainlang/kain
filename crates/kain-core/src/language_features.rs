//! Language capability registry for parser/runtime/importer feature gating.
//!
//! Centralizing these flags keeps behavior data-driven and avoids ad-hoc checks.

use crate::ast::BinaryOp;
use once_cell::sync::Lazy;
use std::collections::BTreeMap;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum LanguageCapability {
    ParserStructLiterals,
    ParserBitwiseAnd,
    ParserBitwiseOr,
    ParserBitwiseXor,
    ParserShiftLeft,
    ParserShiftRight,
    RuntimeBitwiseAnd,
    RuntimeBitwiseOr,
    RuntimeBitwiseXor,
    RuntimeShiftLeft,
    RuntimeShiftRight,
}

#[derive(Debug, Clone)]
pub struct CapabilitySpec {
    pub capability: LanguageCapability,
    pub enabled_by_default: bool,
    pub description: &'static str,
}

pub const LANGUAGE_CAPABILITY_SPECS: &[CapabilitySpec] = &[
    CapabilitySpec {
        capability: LanguageCapability::ParserStructLiterals,
        enabled_by_default: true,
        description: "Parser support for struct literal syntax: Type { field: value }",
    },
    CapabilitySpec {
        capability: LanguageCapability::ParserBitwiseAnd,
        enabled_by_default: true,
        description: "Parser support for bitwise '&' binary operator",
    },
    CapabilitySpec {
        capability: LanguageCapability::ParserBitwiseOr,
        enabled_by_default: true,
        description: "Parser support for bitwise '|' binary operator",
    },
    CapabilitySpec {
        capability: LanguageCapability::ParserBitwiseXor,
        enabled_by_default: true,
        description: "Parser support for bitwise '^' binary operator",
    },
    CapabilitySpec {
        capability: LanguageCapability::ParserShiftLeft,
        enabled_by_default: true,
        description: "Parser support for shift-left '<<' binary operator",
    },
    CapabilitySpec {
        capability: LanguageCapability::ParserShiftRight,
        enabled_by_default: true,
        description: "Parser support for shift-right '>>' binary operator",
    },
    CapabilitySpec {
        capability: LanguageCapability::RuntimeBitwiseAnd,
        enabled_by_default: true,
        description: "Runtime support for bitwise '&' on integer values",
    },
    CapabilitySpec {
        capability: LanguageCapability::RuntimeBitwiseOr,
        enabled_by_default: true,
        description: "Runtime support for bitwise '|' on integer values",
    },
    CapabilitySpec {
        capability: LanguageCapability::RuntimeBitwiseXor,
        enabled_by_default: true,
        description: "Runtime support for bitwise '^' on integer values",
    },
    CapabilitySpec {
        capability: LanguageCapability::RuntimeShiftLeft,
        enabled_by_default: true,
        description: "Runtime support for shift-left '<<' on integer values",
    },
    CapabilitySpec {
        capability: LanguageCapability::RuntimeShiftRight,
        enabled_by_default: true,
        description: "Runtime support for shift-right '>>' on integer values",
    },
];

#[derive(Debug, Clone)]
pub struct LanguageCapabilities {
    flags: BTreeMap<LanguageCapability, bool>,
}

impl Default for LanguageCapabilities {
    fn default() -> Self {
        let mut flags = BTreeMap::new();
        for spec in LANGUAGE_CAPABILITY_SPECS {
            flags.insert(spec.capability, spec.enabled_by_default);
        }
        Self { flags }
    }
}

impl LanguageCapabilities {
    pub fn is_enabled(&self, capability: LanguageCapability) -> bool {
        self.flags.get(&capability).copied().unwrap_or(false)
    }

    pub fn with_override(mut self, capability: LanguageCapability, enabled: bool) -> Self {
        self.flags.insert(capability, enabled);
        self
    }

    pub fn supports_parser_struct_literals(&self) -> bool {
        self.is_enabled(LanguageCapability::ParserStructLiterals)
    }

    pub fn supports_parser_binary_op(&self, op: BinaryOp) -> bool {
        match parser_capability_for_binary_op(op) {
            Some(capability) => self.is_enabled(capability),
            None => true,
        }
    }

    pub fn supports_runtime_binary_op(&self, op: BinaryOp) -> bool {
        match runtime_capability_for_binary_op(op) {
            Some(capability) => self.is_enabled(capability),
            None => true,
        }
    }
}

pub static DEFAULT_LANGUAGE_CAPABILITIES: Lazy<LanguageCapabilities> =
    Lazy::new(LanguageCapabilities::default);

pub fn default_language_capabilities() -> LanguageCapabilities {
    DEFAULT_LANGUAGE_CAPABILITIES.clone()
}

pub fn parser_supports_binary_op(op: BinaryOp) -> bool {
    DEFAULT_LANGUAGE_CAPABILITIES.supports_parser_binary_op(op)
}

pub fn runtime_supports_binary_op(op: BinaryOp) -> bool {
    DEFAULT_LANGUAGE_CAPABILITIES.supports_runtime_binary_op(op)
}

fn parser_capability_for_binary_op(op: BinaryOp) -> Option<LanguageCapability> {
    match op {
        BinaryOp::BitAnd => Some(LanguageCapability::ParserBitwiseAnd),
        BinaryOp::BitOr => Some(LanguageCapability::ParserBitwiseOr),
        BinaryOp::BitXor => Some(LanguageCapability::ParserBitwiseXor),
        BinaryOp::Shl => Some(LanguageCapability::ParserShiftLeft),
        BinaryOp::Shr => Some(LanguageCapability::ParserShiftRight),
        _ => None,
    }
}

fn runtime_capability_for_binary_op(op: BinaryOp) -> Option<LanguageCapability> {
    match op {
        BinaryOp::BitAnd => Some(LanguageCapability::RuntimeBitwiseAnd),
        BinaryOp::BitOr => Some(LanguageCapability::RuntimeBitwiseOr),
        BinaryOp::BitXor => Some(LanguageCapability::RuntimeBitwiseXor),
        BinaryOp::Shl => Some(LanguageCapability::RuntimeShiftLeft),
        BinaryOp::Shr => Some(LanguageCapability::RuntimeShiftRight),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_profile_supports_bitwise_parser_and_runtime_ops() {
        let caps = default_language_capabilities();
        assert!(caps.supports_parser_binary_op(BinaryOp::BitAnd));
        assert!(caps.supports_parser_binary_op(BinaryOp::BitOr));
        assert!(caps.supports_parser_binary_op(BinaryOp::BitXor));
        assert!(caps.supports_parser_binary_op(BinaryOp::Shl));
        assert!(caps.supports_parser_binary_op(BinaryOp::Shr));

        assert!(caps.supports_runtime_binary_op(BinaryOp::BitAnd));
        assert!(caps.supports_runtime_binary_op(BinaryOp::BitOr));
        assert!(caps.supports_runtime_binary_op(BinaryOp::BitXor));
        assert!(caps.supports_runtime_binary_op(BinaryOp::Shl));
        assert!(caps.supports_runtime_binary_op(BinaryOp::Shr));
    }

    #[test]
    fn default_profile_keeps_struct_literals_disabled() {
        let caps = default_language_capabilities();
        assert!(!caps.supports_parser_struct_literals());
    }
}
