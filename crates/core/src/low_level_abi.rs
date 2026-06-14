use crate::ast::{BinaryOp, Type};
use crate::CompileTarget;

const KAIN_C_ABI_FLAVOR_ENV: &str = "KAIN_C_ABI_FLAVOR";

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CAbiKind {
    GenericLp64,
    GenericLlp64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CCompilerFlavor {
    Generic,
    Gcc,
    Clang,
    Msvc,
}

impl CCompilerFlavor {
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Generic => "generic",
            Self::Gcc => "gcc",
            Self::Clang => "clang",
            Self::Msvc => "msvc",
        }
    }
}

#[derive(Debug, Clone, Copy)]
pub struct CAbiPolicy {
    pub key: &'static str,
    pub kind: CAbiKind,
    pub flavor: CCompilerFlavor,
    pub char_bits: usize,
    pub bool_bits: usize,
    pub short_bits: usize,
    pub int_bits: usize,
    pub long_bits: usize,
    pub long_long_bits: usize,
    pub float_bits: usize,
    pub double_bits: usize,
    pub pointer_bits: usize,
    pub integer_promotion_bits: usize,
    pub bitfield_lsb_first: bool,
    pub packed_struct_align: usize,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ScalarLayout {
    pub size_bytes: usize,
    pub align_bytes: usize,
}

#[derive(Debug, Clone, Copy)]
struct CAbiPolicyEntry {
    kind: CAbiKind,
    flavor: CCompilerFlavor,
    policy: CAbiPolicy,
}

const fn policy_entry(
    key: &'static str,
    kind: CAbiKind,
    flavor: CCompilerFlavor,
    long_bits: usize,
) -> CAbiPolicyEntry {
    CAbiPolicyEntry {
        kind,
        flavor,
        policy: CAbiPolicy {
            key,
            kind,
            flavor,
            char_bits: 8,
            bool_bits: 8,
            short_bits: 16,
            int_bits: 32,
            long_bits,
            long_long_bits: 64,
            float_bits: 32,
            double_bits: 64,
            pointer_bits: 64,
            integer_promotion_bits: 32,
            bitfield_lsb_first: true,
            packed_struct_align: 1,
        },
    }
}

const C_ABI_POLICY_TABLE: &[CAbiPolicyEntry] = &[
    policy_entry(
        "generic-lp64",
        CAbiKind::GenericLp64,
        CCompilerFlavor::Generic,
        64,
    ),
    policy_entry(
        "generic-llp64",
        CAbiKind::GenericLlp64,
        CCompilerFlavor::Generic,
        32,
    ),
    policy_entry("gcc-lp64", CAbiKind::GenericLp64, CCompilerFlavor::Gcc, 64),
    policy_entry(
        "gcc-llp64",
        CAbiKind::GenericLlp64,
        CCompilerFlavor::Gcc,
        32,
    ),
    policy_entry(
        "clang-lp64",
        CAbiKind::GenericLp64,
        CCompilerFlavor::Clang,
        64,
    ),
    policy_entry(
        "clang-llp64",
        CAbiKind::GenericLlp64,
        CCompilerFlavor::Clang,
        32,
    ),
    policy_entry(
        "msvc-lp64",
        CAbiKind::GenericLp64,
        CCompilerFlavor::Msvc,
        64,
    ),
    policy_entry(
        "msvc-llp64",
        CAbiKind::GenericLlp64,
        CCompilerFlavor::Msvc,
        32,
    ),
];

pub fn c_compiler_flavor_from_str(value: &str) -> Option<CCompilerFlavor> {
    match value.trim().to_ascii_lowercase().as_str() {
        "" => None,
        "generic" | "default" => Some(CCompilerFlavor::Generic),
        "gcc" | "gnu" => Some(CCompilerFlavor::Gcc),
        "clang" | "llvm" => Some(CCompilerFlavor::Clang),
        "msvc" | "cl" | "visualc" => Some(CCompilerFlavor::Msvc),
        _ => None,
    }
}

pub fn selected_c_compiler_flavor() -> CCompilerFlavor {
    std::env::var(KAIN_C_ABI_FLAVOR_ENV)
        .ok()
        .as_deref()
        .and_then(c_compiler_flavor_from_str)
        .unwrap_or(CCompilerFlavor::Generic)
}

pub fn c_abi_kind_for_target(target: CompileTarget) -> CAbiKind {
    match target {
        CompileTarget::Ue5 | CompileTarget::Ue5Editor => CAbiKind::GenericLlp64,
        CompileTarget::BareMetal => CAbiKind::GenericLp64,
        _ => CAbiKind::GenericLp64,
    }
}

pub fn default_c_abi_policy() -> &'static CAbiPolicy {
    c_abi_policy_for_kind_and_flavor(CAbiKind::GenericLp64, selected_c_compiler_flavor())
}

pub fn c_abi_policy_for_kind_and_flavor(
    kind: CAbiKind,
    flavor: CCompilerFlavor,
) -> &'static CAbiPolicy {
    C_ABI_POLICY_TABLE
        .iter()
        .find(|entry| entry.kind == kind && entry.flavor == flavor)
        .map(|entry| &entry.policy)
        .unwrap_or_else(|| {
            C_ABI_POLICY_TABLE
                .iter()
                .find(|entry| entry.kind == kind && entry.flavor == CCompilerFlavor::Generic)
                .map(|entry| &entry.policy)
                .expect("missing generic C ABI policy")
        })
}

pub fn c_abi_policy_for_target_and_flavor(
    target: CompileTarget,
    flavor: CCompilerFlavor,
) -> &'static CAbiPolicy {
    c_abi_policy_for_kind_and_flavor(c_abi_kind_for_target(target), flavor)
}

pub fn c_abi_policy_for_target(target: CompileTarget) -> &'static CAbiPolicy {
    c_abi_policy_for_target_and_flavor(target, selected_c_compiler_flavor())
}

pub fn promoted_integer_bits(width: usize, _signed: bool, abi: &CAbiPolicy) -> usize {
    width.max(abi.integer_promotion_bits)
}

fn scalar_layout_from_bits(bits: usize) -> ScalarLayout {
    let width = bits.div_ceil(8).max(1);
    ScalarLayout {
        size_bytes: width,
        align_bytes: width,
    }
}

pub fn named_scalar_layout(name: &str, abi: &CAbiPolicy) -> Option<ScalarLayout> {
    match name {
        "Bool" | "bool" => Some(scalar_layout_from_bits(abi.bool_bits)),
        "Byte" | "byte" | "I8" | "i8" | "U8" | "u8" => Some(scalar_layout_from_bits(8)),
        "Char" | "char" => Some(scalar_layout_from_bits(abi.char_bits)),
        "I16" | "i16" | "U16" | "u16" => Some(scalar_layout_from_bits(16)),
        "I32" | "i32" | "U32" | "u32" => Some(scalar_layout_from_bits(32)),
        "Int" | "ISize" | "isize" | "UInt" | "USize" | "usize" => {
            Some(scalar_layout_from_bits(abi.long_bits))
        }
        "I64" | "i64" | "U64" | "u64" => Some(scalar_layout_from_bits(64)),
        "I128" | "i128" | "U128" | "u128" => Some(scalar_layout_from_bits(128)),
        "Float" | "f64" | "F64" | "double" => Some(scalar_layout_from_bits(abi.double_bits)),
        "f32" | "F32" => Some(scalar_layout_from_bits(abi.float_bits)),
        _ => None,
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ArithmeticDomain {
    Integer { bits: usize, signed: bool },
    Float { bits: usize },
}

pub fn arithmetic_domain_for_type(ty: &Type, abi: &CAbiPolicy) -> Option<ArithmeticDomain> {
    match ty {
        Type::Named { name, .. } => match name.as_str() {
            "Bool" => Some(ArithmeticDomain::Integer {
                bits: abi.bool_bits,
                signed: false,
            }),
            "Char" => Some(ArithmeticDomain::Integer {
                bits: abi.char_bits,
                signed: true,
            }),
            "Int" | "isize" => Some(ArithmeticDomain::Integer {
                bits: abi.long_bits,
                signed: true,
            }),
            "UInt" | "usize" => Some(ArithmeticDomain::Integer {
                bits: abi.long_bits,
                signed: false,
            }),
            "Float" => Some(ArithmeticDomain::Float {
                bits: abi.double_bits,
            }),
            _ => None,
        },
        _ => None,
    }
}

pub fn promoted_type_for_arithmetic(ty: &Type, abi: &CAbiPolicy) -> Option<Type> {
    let domain = arithmetic_domain_for_type(ty, abi)?;
    Some(match domain {
        ArithmeticDomain::Float { .. } => ty.clone(),
        ArithmeticDomain::Integer { bits, signed } => {
            let promoted_bits = promoted_integer_bits(bits, signed, abi);
            if promoted_bits <= abi.integer_promotion_bits {
                if signed {
                    named_type("Int")
                } else {
                    named_type("UInt")
                }
            } else if signed {
                named_type("Int")
            } else {
                named_type("UInt")
            }
        }
    })
}

pub fn usual_arithmetic_conversion_type(
    left: &Type,
    right: &Type,
    abi: &CAbiPolicy,
) -> Option<Type> {
    let lhs = arithmetic_domain_for_type(left, abi)?;
    let rhs = arithmetic_domain_for_type(right, abi)?;

    match (lhs, rhs) {
        (
            ArithmeticDomain::Float { bits: lhs_bits },
            ArithmeticDomain::Float { bits: rhs_bits },
        ) => Some(if lhs_bits >= rhs_bits {
            left.clone()
        } else {
            right.clone()
        }),
        (ArithmeticDomain::Float { .. }, _) => Some(left.clone()),
        (_, ArithmeticDomain::Float { .. }) => Some(right.clone()),
        (
            ArithmeticDomain::Integer {
                bits: lhs_bits,
                signed: lhs_signed,
            },
            ArithmeticDomain::Integer {
                bits: rhs_bits,
                signed: rhs_signed,
            },
        ) => {
            let lhs_promoted = promoted_integer_bits(lhs_bits, lhs_signed, abi);
            let rhs_promoted = promoted_integer_bits(rhs_bits, rhs_signed, abi);
            if lhs_signed == rhs_signed {
                Some(if lhs_signed {
                    named_type("Int")
                } else {
                    named_type("UInt")
                })
            } else if lhs_promoted > rhs_promoted {
                Some(if lhs_signed {
                    named_type("Int")
                } else {
                    named_type("UInt")
                })
            } else if rhs_promoted > lhs_promoted {
                Some(if rhs_signed {
                    named_type("Int")
                } else {
                    named_type("UInt")
                })
            } else if !lhs_signed || !rhs_signed {
                Some(named_type("UInt"))
            } else {
                Some(named_type("Int"))
            }
        }
    }
}

pub fn should_apply_usual_arithmetic_conversions(op: BinaryOp) -> bool {
    matches!(
        op,
        BinaryOp::Add
            | BinaryOp::Sub
            | BinaryOp::Mul
            | BinaryOp::Div
            | BinaryOp::Mod
            | BinaryOp::Eq
            | BinaryOp::Ne
            | BinaryOp::Lt
            | BinaryOp::Gt
            | BinaryOp::Le
            | BinaryOp::Ge
            | BinaryOp::BitAnd
            | BinaryOp::BitOr
            | BinaryOp::BitXor
    )
}

pub fn named_type(name: &str) -> Type {
    Type::Named {
        name: name.to_string(),
        generics: Vec::new(),
        span: crate::span::Span::default(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn abi_policy_table_resolves_flavored_entries() {
        let msvc = c_abi_policy_for_kind_and_flavor(CAbiKind::GenericLlp64, CCompilerFlavor::Msvc);
        assert_eq!(msvc.key, "msvc-llp64");
        assert_eq!(msvc.flavor, CCompilerFlavor::Msvc);
        assert_eq!(msvc.long_bits, 32);

        let clang = c_abi_policy_for_kind_and_flavor(CAbiKind::GenericLp64, CCompilerFlavor::Clang);
        assert_eq!(clang.key, "clang-lp64");
        assert_eq!(clang.flavor, CCompilerFlavor::Clang);
        assert_eq!(clang.long_bits, 64);
    }

    #[test]
    fn compiler_flavor_parser_accepts_aliases() {
        assert_eq!(
            c_compiler_flavor_from_str("gnu"),
            Some(CCompilerFlavor::Gcc)
        );
        assert_eq!(
            c_compiler_flavor_from_str("llvm"),
            Some(CCompilerFlavor::Clang)
        );
        assert_eq!(
            c_compiler_flavor_from_str("cl"),
            Some(CCompilerFlavor::Msvc)
        );
        assert_eq!(c_compiler_flavor_from_str("unknown"), None);
    }
}
