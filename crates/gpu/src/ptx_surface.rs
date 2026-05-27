use kain_core::ast::BinaryOp;

use super::PtxScalarKind;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[allow(dead_code)]
pub enum SuffixMode {
    Append,
    Fixed,
    None,
    Rounding,
    Width,
}

#[derive(Debug, Clone, Copy)]
pub struct PtxInstDef {
    pub mnemonic: &'static str,
    pub suffix: SuffixMode,
}

impl PtxInstDef {
    pub const fn new(mnemonic: &'static str, suffix: SuffixMode) -> Self {
        Self { mnemonic, suffix }
    }

    #[allow(dead_code)]
    pub fn render(&self, kind: PtxScalarKind) -> String {
        match self.suffix {
            SuffixMode::Append => format!("{}.{}", self.mnemonic, kind.op_suffix()),
            SuffixMode::Fixed | SuffixMode::None => self.mnemonic.to_string(),
            SuffixMode::Rounding => format!("{}.{}", self.mnemonic, kind.op_suffix()),
            SuffixMode::Width => format!("{}.b{}", self.mnemonic, kind.width_bits()),
        }
    }
}

static BINARY_OP_TABLE: &[(BinaryOp, PtxScalarKind, PtxInstDef)] = &[
    // Arithmetic
    (
        BinaryOp::Add,
        PtxScalarKind::U32,
        PtxInstDef::new("add", SuffixMode::Append),
    ),
    (
        BinaryOp::Add,
        PtxScalarKind::S32,
        PtxInstDef::new("add", SuffixMode::Append),
    ),
    (
        BinaryOp::Add,
        PtxScalarKind::F32,
        PtxInstDef::new("add", SuffixMode::Append),
    ),
    (
        BinaryOp::Add,
        PtxScalarKind::U64,
        PtxInstDef::new("add", SuffixMode::Append),
    ),
    (
        BinaryOp::Sub,
        PtxScalarKind::U32,
        PtxInstDef::new("sub", SuffixMode::Append),
    ),
    (
        BinaryOp::Sub,
        PtxScalarKind::S32,
        PtxInstDef::new("sub", SuffixMode::Append),
    ),
    (
        BinaryOp::Sub,
        PtxScalarKind::F32,
        PtxInstDef::new("sub", SuffixMode::Append),
    ),
    (
        BinaryOp::Sub,
        PtxScalarKind::U64,
        PtxInstDef::new("sub", SuffixMode::Append),
    ),
    (
        BinaryOp::Mul,
        PtxScalarKind::U32,
        PtxInstDef::new("mul.lo", SuffixMode::Append),
    ),
    (
        BinaryOp::Mul,
        PtxScalarKind::S32,
        PtxInstDef::new("mul.lo", SuffixMode::Append),
    ),
    (
        BinaryOp::Mul,
        PtxScalarKind::U64,
        PtxInstDef::new("mul.lo", SuffixMode::Append),
    ),
    (
        BinaryOp::Mul,
        PtxScalarKind::F32,
        PtxInstDef::new("mul.rn", SuffixMode::Rounding),
    ),
    (
        BinaryOp::Div,
        PtxScalarKind::U32,
        PtxInstDef::new("div", SuffixMode::Append),
    ),
    (
        BinaryOp::Div,
        PtxScalarKind::S32,
        PtxInstDef::new("div", SuffixMode::Append),
    ),
    (
        BinaryOp::Div,
        PtxScalarKind::U64,
        PtxInstDef::new("div", SuffixMode::Append),
    ),
    (
        BinaryOp::Div,
        PtxScalarKind::F32,
        PtxInstDef::new("div.rn", SuffixMode::Rounding),
    ),
    (
        BinaryOp::Mod,
        PtxScalarKind::U32,
        PtxInstDef::new("rem", SuffixMode::Append),
    ),
    (
        BinaryOp::Mod,
        PtxScalarKind::S32,
        PtxInstDef::new("rem", SuffixMode::Append),
    ),
    (
        BinaryOp::Mod,
        PtxScalarKind::U64,
        PtxInstDef::new("rem", SuffixMode::Append),
    ),
    // Bitwise
    (
        BinaryOp::BitAnd,
        PtxScalarKind::U32,
        PtxInstDef::new("and", SuffixMode::Width),
    ),
    (
        BinaryOp::BitAnd,
        PtxScalarKind::S32,
        PtxInstDef::new("and", SuffixMode::Width),
    ),
    (
        BinaryOp::BitAnd,
        PtxScalarKind::U64,
        PtxInstDef::new("and", SuffixMode::Width),
    ),
    (
        BinaryOp::BitOr,
        PtxScalarKind::U32,
        PtxInstDef::new("or", SuffixMode::Width),
    ),
    (
        BinaryOp::BitOr,
        PtxScalarKind::S32,
        PtxInstDef::new("or", SuffixMode::Width),
    ),
    (
        BinaryOp::BitOr,
        PtxScalarKind::U64,
        PtxInstDef::new("or", SuffixMode::Width),
    ),
    (
        BinaryOp::BitXor,
        PtxScalarKind::U32,
        PtxInstDef::new("xor", SuffixMode::Width),
    ),
    (
        BinaryOp::BitXor,
        PtxScalarKind::S32,
        PtxInstDef::new("xor", SuffixMode::Width),
    ),
    (
        BinaryOp::BitXor,
        PtxScalarKind::U64,
        PtxInstDef::new("xor", SuffixMode::Width),
    ),
    (
        BinaryOp::Shl,
        PtxScalarKind::U32,
        PtxInstDef::new("shl", SuffixMode::Width),
    ),
    (
        BinaryOp::Shl,
        PtxScalarKind::S32,
        PtxInstDef::new("shl", SuffixMode::Width),
    ),
    (
        BinaryOp::Shl,
        PtxScalarKind::U64,
        PtxInstDef::new("shl", SuffixMode::Width),
    ),
    (
        BinaryOp::Shr,
        PtxScalarKind::S32,
        PtxInstDef::new("shr.s32", SuffixMode::Fixed),
    ),
    (
        BinaryOp::Shr,
        PtxScalarKind::U32,
        PtxInstDef::new("shr.u32", SuffixMode::Fixed),
    ),
    (
        BinaryOp::Shr,
        PtxScalarKind::U64,
        PtxInstDef::new("shr.u64", SuffixMode::Fixed),
    ),
];

static CMP_TABLE: &[(BinaryOp, &'static str)] = &[
    (BinaryOp::Eq, "setp.eq"),
    (BinaryOp::Ne, "setp.ne"),
    (BinaryOp::Lt, "setp.lt"),
    (BinaryOp::Le, "setp.le"),
    (BinaryOp::Gt, "setp.gt"),
    (BinaryOp::Ge, "setp.ge"),
];

static CVT_TABLE: &[((PtxScalarKind, PtxScalarKind), &'static str)] = &[
    ((PtxScalarKind::S32, PtxScalarKind::U32), "cvt.u32.s32"),
    ((PtxScalarKind::U32, PtxScalarKind::S32), "cvt.s32.u32"),
    ((PtxScalarKind::S32, PtxScalarKind::F32), "cvt.rn.f32.s32"),
    ((PtxScalarKind::U32, PtxScalarKind::F32), "cvt.rn.f32.u32"),
    ((PtxScalarKind::F32, PtxScalarKind::S32), "cvt.rzi.s32.f32"),
    ((PtxScalarKind::F32, PtxScalarKind::U32), "cvt.rzi.u32.f32"),
    ((PtxScalarKind::U32, PtxScalarKind::U64), "cvt.u64.u32"),
    ((PtxScalarKind::U64, PtxScalarKind::U32), "cvt.u32.u64"),
    ((PtxScalarKind::S32, PtxScalarKind::U64), "cvt.u64.s32"),
    ((PtxScalarKind::U64, PtxScalarKind::S32), "cvt.s32.u64"),
    ((PtxScalarKind::U64, PtxScalarKind::F32), "cvt.rn.f32.u64"),
    ((PtxScalarKind::F32, PtxScalarKind::U64), "cvt.rzi.u64.f32"),
    ((PtxScalarKind::Pred, PtxScalarKind::U32), "selp"),
];

pub fn binary_op_inst(op: BinaryOp, kind: PtxScalarKind) -> Option<&'static PtxInstDef> {
    BINARY_OP_TABLE
        .iter()
        .find(|(candidate_op, candidate_kind, _)| *candidate_op == op && *candidate_kind == kind)
        .map(|(_, _, def)| def)
}

pub fn cmp_inst(op: BinaryOp) -> Option<&'static str> {
    CMP_TABLE
        .iter()
        .find(|(candidate_op, _)| *candidate_op == op)
        .map(|(_, mnemonic)| *mnemonic)
}

pub fn cvt_inst(from: PtxScalarKind, to: PtxScalarKind) -> Option<&'static str> {
    CVT_TABLE
        .iter()
        .find(|((candidate_from, candidate_to), _)| *candidate_from == from && *candidate_to == to)
        .map(|(_, mnemonic)| *mnemonic)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn binary_table_covers_basic_arithmetic() {
        let def = binary_op_inst(BinaryOp::Mul, PtxScalarKind::U32).unwrap();
        assert_eq!(def.render(PtxScalarKind::U32), "mul.lo.u32");

        let def = binary_op_inst(BinaryOp::Add, PtxScalarKind::F32).unwrap();
        assert_eq!(def.render(PtxScalarKind::F32), "add.f32");

        let def = binary_op_inst(BinaryOp::Mul, PtxScalarKind::F32).unwrap();
        assert_eq!(def.render(PtxScalarKind::F32), "mul.rn.f32");

        let def = binary_op_inst(BinaryOp::Shr, PtxScalarKind::U64).unwrap();
        assert_eq!(def.render(PtxScalarKind::U64), "shr.u64");
    }

    #[test]
    fn compare_table_covers_all_relations() {
        for op in [
            BinaryOp::Eq,
            BinaryOp::Ne,
            BinaryOp::Lt,
            BinaryOp::Le,
            BinaryOp::Gt,
            BinaryOp::Ge,
        ] {
            assert!(cmp_inst(op).is_some());
        }
    }

    #[test]
    fn conversion_table_covers_core_crossings() {
        assert_eq!(
            cvt_inst(PtxScalarKind::S32, PtxScalarKind::U32),
            Some("cvt.u32.s32")
        );
        assert_eq!(
            cvt_inst(PtxScalarKind::F32, PtxScalarKind::U32),
            Some("cvt.rzi.u32.f32")
        );
        assert_eq!(
            cvt_inst(PtxScalarKind::U32, PtxScalarKind::U64),
            Some("cvt.u64.u32")
        );
        assert_eq!(
            cvt_inst(PtxScalarKind::Pred, PtxScalarKind::U32),
            Some("selp")
        );
    }
}
