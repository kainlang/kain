use crate::CompileTarget;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CAbiKind {
    GenericLp64,
    GenericLlp64,
}

#[derive(Debug, Clone, Copy)]
pub struct CAbiPolicy {
    pub kind: CAbiKind,
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

pub const GENERIC_LP64_C_ABI: CAbiPolicy = CAbiPolicy {
    kind: CAbiKind::GenericLp64,
    char_bits: 8,
    bool_bits: 8,
    short_bits: 16,
    int_bits: 32,
    long_bits: 64,
    long_long_bits: 64,
    float_bits: 32,
    double_bits: 64,
    pointer_bits: 64,
    integer_promotion_bits: 32,
    bitfield_lsb_first: true,
    packed_struct_align: 1,
};

pub const GENERIC_LLP64_C_ABI: CAbiPolicy = CAbiPolicy {
    kind: CAbiKind::GenericLlp64,
    char_bits: 8,
    bool_bits: 8,
    short_bits: 16,
    int_bits: 32,
    long_bits: 32,
    long_long_bits: 64,
    float_bits: 32,
    double_bits: 64,
    pointer_bits: 64,
    integer_promotion_bits: 32,
    bitfield_lsb_first: true,
    packed_struct_align: 1,
};

pub fn default_c_abi_policy() -> &'static CAbiPolicy {
    &GENERIC_LP64_C_ABI
}

pub fn c_abi_policy_for_target(target: CompileTarget) -> &'static CAbiPolicy {
    match target {
        CompileTarget::Ue5 | CompileTarget::Ue5Editor => &GENERIC_LLP64_C_ABI,
        _ => &GENERIC_LP64_C_ABI,
    }
}

pub fn promoted_integer_bits(width: usize, _signed: bool, abi: &CAbiPolicy) -> usize {
    width.max(abi.integer_promotion_bits)
}
