//! PTX donor surface for the next backend splice.
//!
//! This module now sits beside the live emitter as the shared PTX planning and
//! capability surface. It captures the next PTX backend slice as data and plan
//! objects so the emitter can grow without rediscovering the shape of atomics,
//! shared memory, warp collectives, tensor primitives, cluster launch
//! plumbing, or kernel ABI metadata.

use std::collections::HashSet;
use std::fmt::{self, Write};

// ============================================================================
// Core PTX Model
// ============================================================================

pub const DEFAULT_PTX_VERSION: &str = "7.8";
pub const DEFAULT_PTX_ARCH: PtxArch = PtxArch::Sm50;

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum PtxArch {
    Sm30,
    Sm35,
    Sm50,
    Sm52,
    Sm60,
    Sm61,
    Sm70,
    Sm72,
    Sm75,
    Sm80,
    Sm86,
    Sm89,
    Sm90,
    Sm100,
    Sm120,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PtxArchSpec {
    pub arch: PtxArch,
    pub sm: &'static str,
    pub rank: u16,
    pub compute_capability: &'static str,
    pub aliases: &'static [&'static str],
}

impl PtxArchSpec {
    pub const fn new(
        arch: PtxArch,
        sm: &'static str,
        rank: u16,
        compute_capability: &'static str,
        aliases: &'static [&'static str],
    ) -> Self {
        Self {
            arch,
            sm,
            rank,
            compute_capability,
            aliases,
        }
    }
}

pub const PTX_ARCH_SPECS: &[PtxArchSpec] = &[
    PtxArchSpec::new(
        PtxArch::Sm30,
        "sm_30",
        30,
        "3.0",
        &[
            "sm_30",
            "sm30",
            "30",
            "3.0",
            "compute_30",
            "compute30",
            "compute_3_0",
        ],
    ),
    PtxArchSpec::new(
        PtxArch::Sm35,
        "sm_35",
        35,
        "3.5",
        &[
            "sm_35",
            "sm35",
            "35",
            "3.5",
            "compute_35",
            "compute35",
            "compute_3_5",
        ],
    ),
    PtxArchSpec::new(
        PtxArch::Sm50,
        "sm_50",
        50,
        "5.0",
        &[
            "sm_50",
            "sm50",
            "50",
            "5.0",
            "compute_50",
            "compute50",
            "compute_5_0",
        ],
    ),
    PtxArchSpec::new(
        PtxArch::Sm52,
        "sm_52",
        52,
        "5.2",
        &[
            "sm_52",
            "sm52",
            "52",
            "5.2",
            "compute_52",
            "compute52",
            "compute_5_2",
        ],
    ),
    PtxArchSpec::new(
        PtxArch::Sm60,
        "sm_60",
        60,
        "6.0",
        &[
            "sm_60",
            "sm60",
            "60",
            "6.0",
            "compute_60",
            "compute60",
            "compute_6_0",
        ],
    ),
    PtxArchSpec::new(
        PtxArch::Sm61,
        "sm_61",
        61,
        "6.1",
        &[
            "sm_61",
            "sm61",
            "61",
            "6.1",
            "compute_61",
            "compute61",
            "compute_6_1",
        ],
    ),
    PtxArchSpec::new(
        PtxArch::Sm70,
        "sm_70",
        70,
        "7.0",
        &[
            "sm_70",
            "sm70",
            "70",
            "7.0",
            "compute_70",
            "compute70",
            "compute_7_0",
        ],
    ),
    PtxArchSpec::new(
        PtxArch::Sm72,
        "sm_72",
        72,
        "7.2",
        &[
            "sm_72",
            "sm72",
            "72",
            "7.2",
            "compute_72",
            "compute72",
            "compute_7_2",
        ],
    ),
    PtxArchSpec::new(
        PtxArch::Sm75,
        "sm_75",
        75,
        "7.5",
        &[
            "sm_75",
            "sm75",
            "75",
            "7.5",
            "compute_75",
            "compute75",
            "compute_7_5",
        ],
    ),
    PtxArchSpec::new(
        PtxArch::Sm80,
        "sm_80",
        80,
        "8.0",
        &[
            "sm_80",
            "sm80",
            "80",
            "8.0",
            "compute_80",
            "compute80",
            "compute_8_0",
        ],
    ),
    PtxArchSpec::new(
        PtxArch::Sm86,
        "sm_86",
        86,
        "8.6",
        &[
            "sm_86",
            "sm86",
            "86",
            "8.6",
            "compute_86",
            "compute86",
            "compute_8_6",
        ],
    ),
    PtxArchSpec::new(
        PtxArch::Sm89,
        "sm_89",
        89,
        "8.9",
        &[
            "sm_89",
            "sm89",
            "89",
            "8.9",
            "compute_89",
            "compute89",
            "compute_8_9",
        ],
    ),
    PtxArchSpec::new(
        PtxArch::Sm90,
        "sm_90",
        90,
        "9.0",
        &[
            "sm_90",
            "sm90",
            "90",
            "9.0",
            "compute_90",
            "compute90",
            "compute_9_0",
        ],
    ),
    PtxArchSpec::new(
        PtxArch::Sm100,
        "sm_100",
        100,
        "10.0",
        &[
            "sm_100",
            "sm100",
            "100",
            "10.0",
            "compute_100",
            "compute100",
            "compute_10_0",
        ],
    ),
    PtxArchSpec::new(
        PtxArch::Sm120,
        "sm_120",
        120,
        "12.0",
        &[
            "sm_120",
            "sm120",
            "120",
            "12.0",
            "compute_120",
            "compute120",
            "compute_12_0",
        ],
    ),
];

impl PtxArch {
    pub fn spec(self) -> &'static PtxArchSpec {
        PTX_ARCH_SPECS
            .iter()
            .find(|spec| spec.arch == self)
            .expect("PTX arch spec must exist for every enum variant")
    }

    pub fn as_sm(self) -> &'static str {
        self.spec().sm
    }

    pub fn rank(self) -> u16 {
        self.spec().rank
    }

    pub fn compute_capability(self) -> &'static str {
        self.spec().compute_capability
    }

    pub fn parse(raw: &str) -> Option<Self> {
        let normalized = raw.trim().to_ascii_lowercase();
        PTX_ARCH_SPECS.iter().find_map(|spec| {
            spec.aliases
                .iter()
                .any(|alias| *alias == normalized)
                .then_some(spec.arch)
        })
    }

    pub fn from_compute_capability(major: u32, minor: u32) -> Option<Self> {
        let capability = format!("{major}.{minor}");
        PTX_ARCH_SPECS
            .iter()
            .find(|spec| spec.compute_capability == capability)
            .map(|spec| spec.arch)
    }

    pub fn supported_target_examples() -> String {
        PTX_ARCH_SPECS
            .iter()
            .map(|spec| spec.sm)
            .collect::<Vec<_>>()
            .join(", ")
    }

    pub fn supports(self, feature: PtxFeature) -> bool {
        self.rank() >= feature.min_arch().rank()
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum PtxFeature {
    Atomics,
    SharedMemory,
    WarpCollectives,
    MBarrier,
    CpAsync,
    ClusterLaunch,
    TensorCores,
    Wgmma,
    Tma,
    Tcgen05,
}

impl PtxFeature {
    pub const fn min_arch(self) -> PtxArch {
        match self {
            Self::Atomics => PtxArch::Sm50,
            Self::SharedMemory => PtxArch::Sm50,
            Self::WarpCollectives => PtxArch::Sm50,
            Self::MBarrier => PtxArch::Sm90,
            Self::CpAsync => PtxArch::Sm80,
            Self::ClusterLaunch => PtxArch::Sm90,
            Self::TensorCores => PtxArch::Sm75,
            Self::Wgmma => PtxArch::Sm90,
            Self::Tma => PtxArch::Sm90,
            Self::Tcgen05 => PtxArch::Sm100,
        }
    }

    pub const fn label(self) -> &'static str {
        match self {
            Self::Atomics => "atomics",
            Self::SharedMemory => "shared memory",
            Self::WarpCollectives => "warp collectives",
            Self::MBarrier => "mbarrier",
            Self::CpAsync => "cp.async",
            Self::ClusterLaunch => "cluster launch",
            Self::TensorCores => "tensor cores",
            Self::Wgmma => "wgmma",
            Self::Tma => "TMA",
            Self::Tcgen05 => "tcgen05",
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum PtxScope {
    Cta,
    Cluster,
    Gpu,
    System,
}

impl PtxScope {
    pub const fn suffix(self) -> &'static str {
        match self {
            Self::Cta => "cta",
            Self::Cluster => "cluster",
            Self::Gpu => "gpu",
            Self::System => "sys",
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum PtxOrdering {
    Relaxed,
    Acquire,
    Release,
    AcqRel,
    SeqCst,
}

impl PtxOrdering {
    pub const fn fence_suffix(self) -> Option<&'static str> {
        match self {
            Self::Relaxed => None,
            Self::Acquire => Some("acquire"),
            Self::Release => Some("release"),
            Self::AcqRel => Some("acq_rel"),
            Self::SeqCst => Some("sc"),
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum PtxAddressSpace {
    Global,
    Shared,
    SharedCluster,
    Local,
    Param,
    Constant,
}

impl PtxAddressSpace {
    pub const fn qualifier(self) -> &'static str {
        match self {
            Self::Global => "global",
            Self::Shared => "shared",
            Self::SharedCluster => "shared::cluster",
            Self::Local => "local",
            Self::Param => "param",
            Self::Constant => "const",
        }
    }

    pub const fn is_shared_like(self) -> bool {
        matches!(self, Self::Shared | Self::SharedCluster)
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum PtxScalarKind {
    Pred,
    U32,
    S32,
    U64,
    S64,
    F16,
    BF16,
    F32,
    F64,
}

impl PtxScalarKind {
    pub const fn width_bits(self) -> u16 {
        match self {
            Self::Pred => 1,
            Self::F16 | Self::BF16 => 16,
            Self::U32 | Self::S32 | Self::F32 => 32,
            Self::U64 | Self::S64 | Self::F64 => 64,
        }
    }

    pub const fn abi_width_bytes(self) -> u32 {
        match self {
            Self::Pred | Self::U32 | Self::S32 | Self::F32 => 4,
            Self::F16 | Self::BF16 => 2,
            Self::U64 | Self::S64 | Self::F64 => 8,
        }
    }

    pub const fn is_float(self) -> bool {
        matches!(self, Self::F16 | Self::BF16 | Self::F32 | Self::F64)
    }

    pub const fn is_integer(self) -> bool {
        matches!(
            self,
            Self::Pred | Self::U32 | Self::S32 | Self::U64 | Self::S64
        )
    }

    pub const fn ptx_suffix(self) -> &'static str {
        match self {
            Self::Pred => "pred",
            Self::U32 => "u32",
            Self::S32 => "s32",
            Self::U64 => "u64",
            Self::S64 => "s64",
            Self::F16 => "f16",
            Self::BF16 => "b16",
            Self::F32 => "f32",
            Self::F64 => "f64",
        }
    }

    pub const fn width_suffix(self) -> &'static str {
        match self {
            Self::Pred => "b1",
            Self::F16 | Self::BF16 => "b16",
            Self::U32 | Self::S32 | Self::F32 => "b32",
            Self::U64 | Self::S64 | Self::F64 => "b64",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum PtxSurfaceError {
    UnsupportedFeature {
        feature: PtxFeature,
        arch: PtxArch,
    },
    UnsupportedAtomic {
        kind: PtxAtomicRmwKind,
        value: PtxScalarKind,
        arch: PtxArch,
    },
    UnsupportedSharedOp {
        kind: PtxSharedOpKind,
        arch: PtxArch,
    },
    UnsupportedWarpOp {
        kind: PtxWarpOpKind,
        arch: PtxArch,
    },
    UnsupportedTensorOp {
        op: String,
        arch: PtxArch,
    },
    InvalidLaunchConfig {
        reason: &'static str,
    },
    InvalidSharedSegment {
        name: String,
        reason: &'static str,
    },
    InvalidKernelParam {
        name: String,
        reason: &'static str,
    },
    DuplicateKernelName(String),
    DuplicateParamName {
        kernel: String,
        name: String,
    },
}

impl fmt::Display for PtxSurfaceError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::UnsupportedFeature { feature, arch } => write!(
                f,
                "{} requires at least {}, but the target is {}",
                feature.label(),
                feature.min_arch().as_sm(),
                arch.as_sm()
            ),
            Self::UnsupportedAtomic { kind, value, arch } => write!(
                f,
                "atomic {} on {} is not supported for {}",
                kind.mnemonic(),
                value.ptx_suffix(),
                arch.as_sm()
            ),
            Self::UnsupportedSharedOp { kind, arch } => write!(
                f,
                "shared-memory op {} is not supported for {}",
                kind.mnemonic(),
                arch.as_sm()
            ),
            Self::UnsupportedWarpOp { kind, arch } => write!(
                f,
                "warp op {} is not supported for {}",
                kind.mnemonic(),
                arch.as_sm()
            ),
            Self::UnsupportedTensorOp { op, arch } => {
                write!(f, "tensor op {} is not supported for {}", op, arch.as_sm())
            }
            Self::InvalidLaunchConfig { reason } => {
                write!(f, "invalid launch config: {}", reason)
            }
            Self::InvalidSharedSegment { name, reason } => {
                write!(f, "invalid shared segment {}: {}", name, reason)
            }
            Self::InvalidKernelParam { name, reason } => {
                write!(f, "invalid kernel param {}: {}", name, reason)
            }
            Self::DuplicateKernelName(name) => write!(f, "duplicate kernel name: {}", name),
            Self::DuplicateParamName { kernel, name } => {
                write!(f, "duplicate param name {} in kernel {}", name, kernel)
            }
        }
    }
}

impl std::error::Error for PtxSurfaceError {}

pub type PtxResult<T> = Result<T, PtxSurfaceError>;

fn align_up(value: u32, alignment: u32) -> PtxResult<u32> {
    if alignment == 0 {
        return Err(PtxSurfaceError::InvalidLaunchConfig {
            reason: "alignment cannot be zero",
        });
    }
    if !alignment.is_power_of_two() {
        return Err(PtxSurfaceError::InvalidLaunchConfig {
            reason: "alignment must be a power of two",
        });
    }
    let mask = alignment - 1;
    value
        .checked_add(mask)
        .map(|next| next & !mask)
        .ok_or(PtxSurfaceError::InvalidLaunchConfig {
            reason: "alignment calculation overflowed",
        })
}

// ============================================================================
// Atomics
// ============================================================================

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum PtxAtomicRmwKind {
    Add,
    Sub,
    And,
    Or,
    Xor,
    Min,
    Max,
    Exch,
    Cas,
}

impl PtxAtomicRmwKind {
    pub const fn mnemonic(self) -> &'static str {
        match self {
            Self::Add => "add",
            Self::Sub => "sub",
            Self::And => "and",
            Self::Or => "or",
            Self::Xor => "xor",
            Self::Min => "min",
            Self::Max => "max",
            Self::Exch => "exch",
            Self::Cas => "cas",
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PtxAtomicOpSpec {
    pub kind: PtxAtomicRmwKind,
    pub value: PtxScalarKind,
    pub arch_min: PtxArch,
}

impl PtxAtomicOpSpec {
    pub const fn new(kind: PtxAtomicRmwKind, value: PtxScalarKind, arch_min: PtxArch) -> Self {
        Self {
            kind,
            value,
            arch_min,
        }
    }

    pub fn render_core(self) -> String {
        format!("atom.{}.{}", self.kind.mnemonic(), self.value.ptx_suffix())
    }
}

pub static ATOMIC_OPS: &[PtxAtomicOpSpec] = &[
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Add, PtxScalarKind::U32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Add, PtxScalarKind::S32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Add, PtxScalarKind::U64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Add, PtxScalarKind::S64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Add, PtxScalarKind::F32, PtxArch::Sm60),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Add, PtxScalarKind::F64, PtxArch::Sm60),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Sub, PtxScalarKind::U32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Sub, PtxScalarKind::S32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Sub, PtxScalarKind::U64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Sub, PtxScalarKind::S64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::And, PtxScalarKind::U32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::And, PtxScalarKind::S32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::And, PtxScalarKind::U64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::And, PtxScalarKind::S64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Or, PtxScalarKind::U32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Or, PtxScalarKind::S32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Or, PtxScalarKind::U64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Or, PtxScalarKind::S64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Xor, PtxScalarKind::U32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Xor, PtxScalarKind::S32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Xor, PtxScalarKind::U64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Xor, PtxScalarKind::S64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Min, PtxScalarKind::U32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Min, PtxScalarKind::S32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Min, PtxScalarKind::U64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Min, PtxScalarKind::S64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Max, PtxScalarKind::U32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Max, PtxScalarKind::S32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Max, PtxScalarKind::U64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Max, PtxScalarKind::S64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Exch, PtxScalarKind::U32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Exch, PtxScalarKind::S32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Exch, PtxScalarKind::U64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Exch, PtxScalarKind::S64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Exch, PtxScalarKind::F32, PtxArch::Sm60),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Exch, PtxScalarKind::F64, PtxArch::Sm60),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Cas, PtxScalarKind::U32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Cas, PtxScalarKind::S32, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Cas, PtxScalarKind::U64, PtxArch::Sm50),
    PtxAtomicOpSpec::new(PtxAtomicRmwKind::Cas, PtxScalarKind::S64, PtxArch::Sm50),
];

pub fn lookup_atomic_op(
    kind: PtxAtomicRmwKind,
    value: PtxScalarKind,
) -> Option<&'static PtxAtomicOpSpec> {
    ATOMIC_OPS
        .iter()
        .find(|candidate| candidate.kind == kind && candidate.value == value)
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PtxFenceSpec {
    pub ordering: PtxOrdering,
    pub scope: PtxScope,
}

impl PtxFenceSpec {
    pub const fn new(ordering: PtxOrdering, scope: PtxScope) -> Self {
        Self { ordering, scope }
    }

    pub fn render(self) -> Option<String> {
        self.ordering.fence_suffix().map(|suffix| {
            let mut out = String::new();
            write!(&mut out, "fence.{}.{}", suffix, self.scope.suffix()).unwrap();
            out
        })
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PtxAtomicSequence {
    pub op: &'static PtxAtomicOpSpec,
    pub scope: PtxScope,
    pub ordering: PtxOrdering,
}

impl PtxAtomicSequence {
    pub const fn new(op: &'static PtxAtomicOpSpec, scope: PtxScope, ordering: PtxOrdering) -> Self {
        Self {
            op,
            scope,
            ordering,
        }
    }

    pub fn minimum_arch(self) -> PtxArch {
        if self.scope == PtxScope::Cluster {
            std::cmp::max(self.op.arch_min, PtxFeature::ClusterLaunch.min_arch())
        } else {
            self.op.arch_min
        }
    }

    pub fn validate(self, arch: PtxArch) -> PtxResult<()> {
        if arch < self.op.arch_min {
            return Err(PtxSurfaceError::UnsupportedAtomic {
                kind: self.op.kind,
                value: self.op.value,
                arch,
            });
        }
        if self.scope == PtxScope::Cluster && !arch.supports(PtxFeature::ClusterLaunch) {
            return Err(PtxSurfaceError::UnsupportedFeature {
                feature: PtxFeature::ClusterLaunch,
                arch,
            });
        }
        Ok(())
    }

    pub fn fences(self) -> (Option<PtxFenceSpec>, Option<PtxFenceSpec>) {
        match self.ordering {
            PtxOrdering::Relaxed => (None, None),
            PtxOrdering::Acquire => (
                None,
                Some(PtxFenceSpec::new(PtxOrdering::Acquire, self.scope)),
            ),
            PtxOrdering::Release => (
                Some(PtxFenceSpec::new(PtxOrdering::Release, self.scope)),
                None,
            ),
            PtxOrdering::AcqRel => (
                Some(PtxFenceSpec::new(PtxOrdering::AcqRel, self.scope)),
                Some(PtxFenceSpec::new(PtxOrdering::AcqRel, self.scope)),
            ),
            PtxOrdering::SeqCst => (
                Some(PtxFenceSpec::new(PtxOrdering::SeqCst, self.scope)),
                Some(PtxFenceSpec::new(PtxOrdering::SeqCst, self.scope)),
            ),
        }
    }

    pub fn render_lines(self) -> Vec<String> {
        let (pre, post) = self.fences();
        let mut lines = Vec::new();
        if let Some(line) = pre.and_then(PtxFenceSpec::render) {
            lines.push(line);
        }
        lines.push(self.op.render_core());
        if let Some(line) = post.and_then(PtxFenceSpec::render) {
            lines.push(line);
        }
        lines
    }
}

// ============================================================================
// Shared Memory, Async Copies, and Barriers
// ============================================================================

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum PtxSharedOpKind {
    LdShared,
    StShared,
    CpAsync,
    CpAsyncCommitGroup,
    CpAsyncWaitGroup,
    FenceProxyAsyncSharedCta,
    BarSync,
    MBarrierInit,
    MBarrierArrive,
    MBarrierTestWait,
    MBarrierInval,
    MBarrierArriveExpectTx,
}

impl PtxSharedOpKind {
    pub const fn mnemonic(self) -> &'static str {
        match self {
            Self::LdShared => "ld.shared",
            Self::StShared => "st.shared",
            Self::CpAsync => "cp.async.shared.global",
            Self::CpAsyncCommitGroup => "cp.async.commit_group",
            Self::CpAsyncWaitGroup => "cp.async.wait_group",
            Self::FenceProxyAsyncSharedCta => "fence.proxy.async.shared::cta",
            Self::BarSync => "bar.sync",
            Self::MBarrierInit => "mbarrier.init.shared.b64",
            Self::MBarrierArrive => "mbarrier.arrive.shared.b64",
            Self::MBarrierTestWait => "mbarrier.test_wait.shared.b64",
            Self::MBarrierInval => "mbarrier.inval.shared.b64",
            Self::MBarrierArriveExpectTx => "mbarrier.arrive.expect_tx.shared.b64",
        }
    }

    pub const fn arch_min(self) -> PtxArch {
        match self {
            Self::LdShared | Self::StShared | Self::BarSync => PtxArch::Sm30,
            Self::CpAsync | Self::CpAsyncCommitGroup | Self::CpAsyncWaitGroup => PtxArch::Sm80,
            Self::FenceProxyAsyncSharedCta
            | Self::MBarrierInit
            | Self::MBarrierArrive
            | Self::MBarrierTestWait
            | Self::MBarrierInval
            | Self::MBarrierArriveExpectTx => PtxArch::Sm90,
        }
    }

    pub const fn feature(self) -> PtxFeature {
        match self {
            Self::LdShared | Self::StShared | Self::BarSync => PtxFeature::SharedMemory,
            Self::CpAsync | Self::CpAsyncCommitGroup | Self::CpAsyncWaitGroup => {
                PtxFeature::CpAsync
            }
            Self::FenceProxyAsyncSharedCta
            | Self::MBarrierInit
            | Self::MBarrierArrive
            | Self::MBarrierTestWait
            | Self::MBarrierInval
            | Self::MBarrierArriveExpectTx => PtxFeature::MBarrier,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PtxSharedOpSpec {
    pub kind: PtxSharedOpKind,
    pub mnemonic: &'static str,
    pub arch_min: PtxArch,
}

impl PtxSharedOpSpec {
    pub const fn new(kind: PtxSharedOpKind, mnemonic: &'static str, arch_min: PtxArch) -> Self {
        Self {
            kind,
            mnemonic,
            arch_min,
        }
    }
}

pub static SHARED_OPS: &[PtxSharedOpSpec] = &[
    PtxSharedOpSpec::new(PtxSharedOpKind::LdShared, "ld.shared", PtxArch::Sm50),
    PtxSharedOpSpec::new(PtxSharedOpKind::StShared, "st.shared", PtxArch::Sm50),
    PtxSharedOpSpec::new(
        PtxSharedOpKind::CpAsync,
        "cp.async.shared.global",
        PtxArch::Sm80,
    ),
    PtxSharedOpSpec::new(
        PtxSharedOpKind::CpAsyncCommitGroup,
        "cp.async.commit_group",
        PtxArch::Sm80,
    ),
    PtxSharedOpSpec::new(
        PtxSharedOpKind::CpAsyncWaitGroup,
        "cp.async.wait_group",
        PtxArch::Sm80,
    ),
    PtxSharedOpSpec::new(
        PtxSharedOpKind::FenceProxyAsyncSharedCta,
        "fence.proxy.async.shared::cta",
        PtxArch::Sm90,
    ),
    PtxSharedOpSpec::new(PtxSharedOpKind::BarSync, "bar.sync", PtxArch::Sm50),
    PtxSharedOpSpec::new(
        PtxSharedOpKind::MBarrierInit,
        "mbarrier.init.shared.b64",
        PtxArch::Sm90,
    ),
    PtxSharedOpSpec::new(
        PtxSharedOpKind::MBarrierArrive,
        "mbarrier.arrive.shared.b64",
        PtxArch::Sm90,
    ),
    PtxSharedOpSpec::new(
        PtxSharedOpKind::MBarrierTestWait,
        "mbarrier.test_wait.shared.b64",
        PtxArch::Sm90,
    ),
    PtxSharedOpSpec::new(
        PtxSharedOpKind::MBarrierInval,
        "mbarrier.inval.shared.b64",
        PtxArch::Sm90,
    ),
    PtxSharedOpSpec::new(
        PtxSharedOpKind::MBarrierArriveExpectTx,
        "mbarrier.arrive.expect_tx.shared.b64",
        PtxArch::Sm90,
    ),
];

pub fn lookup_shared_op(kind: PtxSharedOpKind) -> Option<&'static PtxSharedOpSpec> {
    SHARED_OPS.iter().find(|candidate| candidate.kind == kind)
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PtxSharedSegment {
    pub name: String,
    pub bytes: u32,
    pub alignment: u32,
    pub space: PtxAddressSpace,
}

impl PtxSharedSegment {
    pub fn new(
        name: impl Into<String>,
        bytes: u32,
        alignment: u32,
        space: PtxAddressSpace,
    ) -> Self {
        Self {
            name: name.into(),
            bytes,
            alignment,
            space,
        }
    }

    pub fn validate(&self) -> PtxResult<()> {
        if self.name.is_empty() {
            return Err(PtxSurfaceError::InvalidSharedSegment {
                name: self.name.clone(),
                reason: "segment name cannot be empty",
            });
        }
        if self.bytes == 0 {
            return Err(PtxSurfaceError::InvalidSharedSegment {
                name: self.name.clone(),
                reason: "segment size cannot be zero",
            });
        }
        if self.alignment == 0 || !self.alignment.is_power_of_two() {
            return Err(PtxSurfaceError::InvalidSharedSegment {
                name: self.name.clone(),
                reason: "alignment must be a non-zero power of two",
            });
        }
        if !self.space.is_shared_like() {
            return Err(PtxSurfaceError::InvalidSharedSegment {
                name: self.name.clone(),
                reason: "shared segment must live in shared or shared::cluster space",
            });
        }
        Ok(())
    }
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct PtxSharedMemoryPlan {
    pub static_segments: Vec<PtxSharedSegment>,
    pub dynamic_bytes: u32,
    pub barrier_slots: u32,
}

impl PtxSharedMemoryPlan {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn add_segment(&mut self, segment: PtxSharedSegment) {
        self.static_segments.push(segment);
    }

    pub fn required_bytes(&self) -> PtxResult<u32> {
        let mut cursor = 0u32;
        for segment in &self.static_segments {
            cursor = align_up(cursor, segment.alignment)?;
            cursor =
                cursor
                    .checked_add(segment.bytes)
                    .ok_or(PtxSurfaceError::InvalidLaunchConfig {
                        reason: "shared-memory segment accounting overflowed",
                    })?;
        }

        cursor = align_up(cursor, 8)?;
        let barrier_bytes =
            self.barrier_slots
                .checked_mul(8)
                .ok_or(PtxSurfaceError::InvalidLaunchConfig {
                    reason: "mbarrier slot accounting overflowed",
                })?;
        cursor = cursor
            .checked_add(barrier_bytes)
            .ok_or(PtxSurfaceError::InvalidLaunchConfig {
                reason: "mbarrier reservation overflowed",
            })?;
        cursor =
            cursor
                .checked_add(self.dynamic_bytes)
                .ok_or(PtxSurfaceError::InvalidLaunchConfig {
                    reason: "dynamic shared-memory reservation overflowed",
                })?;
        Ok(cursor)
    }

    pub fn validate(&self) -> PtxResult<()> {
        let mut names = HashSet::new();
        for segment in &self.static_segments {
            segment.validate()?;
            if !names.insert(segment.name.clone()) {
                return Err(PtxSurfaceError::InvalidSharedSegment {
                    name: segment.name.clone(),
                    reason: "duplicate shared-memory segment name",
                });
            }
        }
        Ok(())
    }

    pub fn minimum_arch(&self) -> PtxArch {
        let mut arch = PtxArch::Sm30;
        if self.barrier_slots > 0 {
            arch = std::cmp::max(arch, PtxFeature::MBarrier.min_arch());
        }
        if self
            .static_segments
            .iter()
            .any(|segment| segment.space == PtxAddressSpace::SharedCluster)
        {
            arch = std::cmp::max(arch, PtxFeature::ClusterLaunch.min_arch());
        }
        arch
    }
}

// ============================================================================
// Warp Collectives
// ============================================================================

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum PtxWarpOpKind {
    Activemask,
    BallotSync,
    AnySync,
    AllSync,
    ShflIdxSync,
    ShflUpSync,
    ShflDownSync,
    ShflXorSync,
    MatchAnySync,
    MatchAllSync,
    BarWarpSync,
}

impl PtxWarpOpKind {
    pub const fn mnemonic(self) -> &'static str {
        match self {
            Self::Activemask => "activemask.b32",
            Self::BallotSync => "vote.sync.ballot.b32",
            Self::AnySync => "vote.sync.any.pred",
            Self::AllSync => "vote.sync.all.pred",
            Self::ShflIdxSync => "shfl.sync.idx.b32",
            Self::ShflUpSync => "shfl.sync.up.b32",
            Self::ShflDownSync => "shfl.sync.down.b32",
            Self::ShflXorSync => "shfl.sync.bfly.b32",
            Self::MatchAnySync => "match.any.sync.b32",
            Self::MatchAllSync => "match.all.sync.b32",
            Self::BarWarpSync => "bar.warp.sync",
        }
    }

    pub const fn arch_min(self) -> PtxArch {
        let _ = self;
        PtxArch::Sm50
    }

    pub const fn feature(self) -> PtxFeature {
        let _ = self;
        PtxFeature::WarpCollectives
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PtxWarpOpSpec {
    pub kind: PtxWarpOpKind,
    pub mnemonic: &'static str,
    pub result: PtxScalarKind,
    pub arch_min: PtxArch,
}

impl PtxWarpOpSpec {
    pub const fn new(
        kind: PtxWarpOpKind,
        mnemonic: &'static str,
        result: PtxScalarKind,
        arch_min: PtxArch,
    ) -> Self {
        Self {
            kind,
            mnemonic,
            result,
            arch_min,
        }
    }

    pub fn render(self) -> &'static str {
        self.mnemonic
    }
}

pub static WARP_OPS: &[PtxWarpOpSpec] = &[
    PtxWarpOpSpec::new(
        PtxWarpOpKind::Activemask,
        "activemask.b32",
        PtxScalarKind::U32,
        PtxArch::Sm50,
    ),
    PtxWarpOpSpec::new(
        PtxWarpOpKind::BallotSync,
        "vote.sync.ballot.b32",
        PtxScalarKind::U32,
        PtxArch::Sm50,
    ),
    PtxWarpOpSpec::new(
        PtxWarpOpKind::AnySync,
        "vote.sync.any.pred",
        PtxScalarKind::Pred,
        PtxArch::Sm50,
    ),
    PtxWarpOpSpec::new(
        PtxWarpOpKind::AllSync,
        "vote.sync.all.pred",
        PtxScalarKind::Pred,
        PtxArch::Sm50,
    ),
    PtxWarpOpSpec::new(
        PtxWarpOpKind::ShflIdxSync,
        "shfl.sync.idx.b32",
        PtxScalarKind::U32,
        PtxArch::Sm50,
    ),
    PtxWarpOpSpec::new(
        PtxWarpOpKind::ShflUpSync,
        "shfl.sync.up.b32",
        PtxScalarKind::U32,
        PtxArch::Sm50,
    ),
    PtxWarpOpSpec::new(
        PtxWarpOpKind::ShflDownSync,
        "shfl.sync.down.b32",
        PtxScalarKind::U32,
        PtxArch::Sm50,
    ),
    PtxWarpOpSpec::new(
        PtxWarpOpKind::ShflXorSync,
        "shfl.sync.bfly.b32",
        PtxScalarKind::U32,
        PtxArch::Sm50,
    ),
    PtxWarpOpSpec::new(
        PtxWarpOpKind::MatchAnySync,
        "match.any.sync.b32",
        PtxScalarKind::U32,
        PtxArch::Sm50,
    ),
    PtxWarpOpSpec::new(
        PtxWarpOpKind::MatchAllSync,
        "match.all.sync.b32",
        PtxScalarKind::U32,
        PtxArch::Sm50,
    ),
    PtxWarpOpSpec::new(
        PtxWarpOpKind::BarWarpSync,
        "bar.warp.sync",
        PtxScalarKind::Pred,
        PtxArch::Sm50,
    ),
];

pub fn lookup_warp_op(kind: PtxWarpOpKind) -> Option<&'static PtxWarpOpSpec> {
    WARP_OPS.iter().find(|candidate| candidate.kind == kind)
}

// ============================================================================
// Tensor / WGMMA / TMA Surface
// ============================================================================

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum PtxTensorFixedKind {
    WgmmaFence,
    WgmmaCommitGroup,
    CpAsyncBulkTensor2d,
    CpAsyncBulkTensor3d,
    MmaSync,
    Tcgen05Mma,
}

impl PtxTensorFixedKind {
    pub const fn mnemonic(self) -> &'static str {
        match self {
            Self::WgmmaFence => "wgmma.fence.sync.aligned",
            Self::WgmmaCommitGroup => "wgmma.commit_group.sync.aligned",
            Self::CpAsyncBulkTensor2d => "cp.async.bulk.tensor.2d.shared::cluster.global",
            Self::CpAsyncBulkTensor3d => "cp.async.bulk.tensor.3d.shared::cluster.global",
            Self::MmaSync => "mma.sync.aligned.m16n8k16",
            Self::Tcgen05Mma => "tcgen05.mma",
        }
    }

    pub const fn arch_min(self) -> PtxArch {
        match self {
            Self::MmaSync => PtxArch::Sm75,
            Self::WgmmaFence
            | Self::WgmmaCommitGroup
            | Self::CpAsyncBulkTensor2d
            | Self::CpAsyncBulkTensor3d => PtxArch::Sm90,
            Self::Tcgen05Mma => PtxArch::Sm100,
        }
    }

    pub const fn feature(self) -> PtxFeature {
        match self {
            Self::MmaSync => PtxFeature::TensorCores,
            Self::WgmmaFence | Self::WgmmaCommitGroup => PtxFeature::Wgmma,
            Self::CpAsyncBulkTensor2d | Self::CpAsyncBulkTensor3d => PtxFeature::Tma,
            Self::Tcgen05Mma => PtxFeature::Tcgen05,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PtxTensorOpSpec {
    pub kind: PtxTensorFixedKind,
    pub mnemonic: &'static str,
    pub arch_min: PtxArch,
    pub warpgroup_threads: u32,
    pub requires_shared_memory: bool,
    pub requires_cluster_launch: bool,
    pub requires_mbarrier: bool,
}

impl PtxTensorOpSpec {
    pub const fn new(
        kind: PtxTensorFixedKind,
        mnemonic: &'static str,
        arch_min: PtxArch,
        warpgroup_threads: u32,
        requires_shared_memory: bool,
        requires_cluster_launch: bool,
        requires_mbarrier: bool,
    ) -> Self {
        Self {
            kind,
            mnemonic,
            arch_min,
            warpgroup_threads,
            requires_shared_memory,
            requires_cluster_launch,
            requires_mbarrier,
        }
    }
}

pub static TENSOR_OPS: &[PtxTensorOpSpec] = &[
    PtxTensorOpSpec::new(
        PtxTensorFixedKind::WgmmaFence,
        "wgmma.fence.sync.aligned",
        PtxArch::Sm90,
        128,
        true,
        false,
        false,
    ),
    PtxTensorOpSpec::new(
        PtxTensorFixedKind::WgmmaCommitGroup,
        "wgmma.commit_group.sync.aligned",
        PtxArch::Sm90,
        128,
        true,
        false,
        false,
    ),
    PtxTensorOpSpec::new(
        PtxTensorFixedKind::CpAsyncBulkTensor2d,
        "cp.async.bulk.tensor.2d.shared::cluster.global",
        PtxArch::Sm90,
        128,
        true,
        true,
        true,
    ),
    PtxTensorOpSpec::new(
        PtxTensorFixedKind::CpAsyncBulkTensor3d,
        "cp.async.bulk.tensor.3d.shared::cluster.global",
        PtxArch::Sm90,
        128,
        true,
        true,
        true,
    ),
    PtxTensorOpSpec::new(
        PtxTensorFixedKind::MmaSync,
        "mma.sync.aligned.m16n8k16",
        PtxArch::Sm75,
        32,
        true,
        false,
        false,
    ),
    PtxTensorOpSpec::new(
        PtxTensorFixedKind::Tcgen05Mma,
        "tcgen05.mma",
        PtxArch::Sm100,
        128,
        true,
        true,
        true,
    ),
];

pub fn lookup_tensor_op(kind: PtxTensorFixedKind) -> Option<&'static PtxTensorOpSpec> {
    TENSOR_OPS.iter().find(|candidate| candidate.kind == kind)
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum PtxTensorOpRequest {
    Fixed(PtxTensorFixedKind),
    WgmmaWaitGroup(u32),
}

impl PtxTensorOpRequest {
    pub fn label(self) -> String {
        match self {
            Self::Fixed(kind) => kind.mnemonic().to_string(),
            Self::WgmmaWaitGroup(group) => format!("wgmma.wait_group.sync.aligned {}", group),
        }
    }

    pub const fn arch_min(self) -> PtxArch {
        match self {
            Self::Fixed(kind) => kind.arch_min(),
            Self::WgmmaWaitGroup(_) => PtxArch::Sm90,
        }
    }

    pub const fn feature(self) -> PtxFeature {
        match self {
            Self::Fixed(kind) => kind.feature(),
            Self::WgmmaWaitGroup(_) => PtxFeature::Wgmma,
        }
    }

    pub fn render(self) -> PtxResult<String> {
        match self {
            Self::Fixed(kind) => {
                let spec =
                    lookup_tensor_op(kind).ok_or_else(|| PtxSurfaceError::UnsupportedTensorOp {
                        op: kind.mnemonic().to_string(),
                        arch: kind.arch_min(),
                    })?;
                Ok(spec.mnemonic.to_string())
            }
            Self::WgmmaWaitGroup(group) => {
                let mut out = String::new();
                write!(&mut out, "wgmma.wait_group.sync.aligned {}", group).unwrap();
                Ok(out)
            }
        }
    }

    pub fn validate(self, arch: PtxArch) -> PtxResult<()> {
        let required = self.arch_min();
        if arch < required {
            return Err(PtxSurfaceError::UnsupportedTensorOp {
                op: self.label(),
                arch,
            });
        }
        Ok(())
    }
}

// ============================================================================
// Launch / Runtime / Host-Device Planning
// ============================================================================

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct PtxClusterLaunchConfig {
    pub dim: [u32; 3],
}

impl PtxClusterLaunchConfig {
    pub const fn new(dim: [u32; 3]) -> Self {
        Self { dim }
    }

    pub fn validate(self, arch: PtxArch) -> PtxResult<()> {
        if !arch.supports(PtxFeature::ClusterLaunch) {
            return Err(PtxSurfaceError::UnsupportedFeature {
                feature: PtxFeature::ClusterLaunch,
                arch,
            });
        }
        for (axis, value) in self.dim.iter().copied().enumerate() {
            if value == 0 {
                return Err(PtxSurfaceError::InvalidLaunchConfig {
                    reason: match axis {
                        0 => "cluster_dim.x cannot be zero",
                        1 => "cluster_dim.y cannot be zero",
                        _ => "cluster_dim.z cannot be zero",
                    },
                });
            }
        }
        Ok(())
    }
}

impl Default for PtxClusterLaunchConfig {
    fn default() -> Self {
        Self { dim: [1, 1, 1] }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct PtxLaunchConfig {
    pub grid_dim: [u32; 3],
    pub block_dim: [u32; 3],
    pub shared_mem_bytes: u32,
    pub cluster: Option<PtxClusterLaunchConfig>,
}

impl Default for PtxLaunchConfig {
    fn default() -> Self {
        Self {
            grid_dim: [1, 1, 1],
            block_dim: [1, 1, 1],
            shared_mem_bytes: 0,
            cluster: None,
        }
    }
}

impl PtxLaunchConfig {
    pub const fn new(grid_dim: [u32; 3], block_dim: [u32; 3], shared_mem_bytes: u32) -> Self {
        Self {
            grid_dim,
            block_dim,
            shared_mem_bytes,
            cluster: None,
        }
    }

    pub fn with_cluster(mut self, cluster: PtxClusterLaunchConfig) -> Self {
        self.cluster = Some(cluster);
        self
    }

    pub fn minimum_arch(self) -> PtxArch {
        if self.cluster.is_some() {
            PtxFeature::ClusterLaunch.min_arch()
        } else {
            PtxArch::Sm30
        }
    }

    pub fn validate(self, arch: PtxArch) -> PtxResult<()> {
        for (axis, value) in self.grid_dim.iter().copied().enumerate() {
            if value == 0 {
                return Err(PtxSurfaceError::InvalidLaunchConfig {
                    reason: match axis {
                        0 => "grid_dim.x cannot be zero",
                        1 => "grid_dim.y cannot be zero",
                        _ => "grid_dim.z cannot be zero",
                    },
                });
            }
        }
        for (axis, value) in self.block_dim.iter().copied().enumerate() {
            if value == 0 {
                return Err(PtxSurfaceError::InvalidLaunchConfig {
                    reason: match axis {
                        0 => "block_dim.x cannot be zero",
                        1 => "block_dim.y cannot be zero",
                        _ => "block_dim.z cannot be zero",
                    },
                });
            }
        }
        if let Some(cluster) = self.cluster {
            cluster.validate(arch)?;
        }
        Ok(())
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum PtxKernelParamEncoding {
    Scalar(PtxScalarKind),
    Pointer {
        space: PtxAddressSpace,
        pointee: Option<PtxScalarKind>,
    },
    Descriptor64,
}

impl PtxKernelParamEncoding {
    pub const fn abi_width_bytes(self) -> u32 {
        match self {
            Self::Scalar(kind) => kind.abi_width_bytes(),
            Self::Pointer { .. } | Self::Descriptor64 => 8,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PtxKernelParam {
    pub name: String,
    pub encoding: PtxKernelParamEncoding,
    pub align: u32,
}

impl PtxKernelParam {
    pub fn new(name: impl Into<String>, encoding: PtxKernelParamEncoding) -> Self {
        let align = encoding.abi_width_bytes();
        Self {
            name: name.into(),
            encoding,
            align,
        }
    }

    pub fn scalar(name: impl Into<String>, kind: PtxScalarKind) -> Self {
        Self::new(name, PtxKernelParamEncoding::Scalar(kind))
    }

    pub fn pointer(name: impl Into<String>, space: PtxAddressSpace) -> Self {
        Self::new(
            name,
            PtxKernelParamEncoding::Pointer {
                space,
                pointee: None,
            },
        )
    }

    pub fn descriptor64(name: impl Into<String>) -> Self {
        Self::new(name, PtxKernelParamEncoding::Descriptor64)
    }

    pub fn validate(&self) -> PtxResult<()> {
        if self.name.is_empty() {
            return Err(PtxSurfaceError::InvalidKernelParam {
                name: self.name.clone(),
                reason: "parameter name cannot be empty",
            });
        }
        if self.align == 0 || !self.align.is_power_of_two() {
            return Err(PtxSurfaceError::InvalidKernelParam {
                name: self.name.clone(),
                reason: "alignment must be a non-zero power of two",
            });
        }
        Ok(())
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PtxKernelPlan {
    pub host_name: String,
    pub ptx_entry: String,
    pub launch: PtxLaunchConfig,
    pub shared_memory: PtxSharedMemoryPlan,
    pub params: Vec<PtxKernelParam>,
    pub atomics: Vec<PtxAtomicSequence>,
    pub shared_ops: Vec<PtxSharedOpKind>,
    pub warp_ops: Vec<PtxWarpOpKind>,
    pub tensor_ops: Vec<PtxTensorOpRequest>,
}

impl PtxKernelPlan {
    pub fn new(
        host_name: impl Into<String>,
        ptx_entry: impl Into<String>,
        launch: PtxLaunchConfig,
    ) -> Self {
        Self {
            host_name: host_name.into(),
            ptx_entry: ptx_entry.into(),
            launch,
            shared_memory: PtxSharedMemoryPlan::default(),
            params: Vec::new(),
            atomics: Vec::new(),
            shared_ops: Vec::new(),
            warp_ops: Vec::new(),
            tensor_ops: Vec::new(),
        }
    }

    pub fn add_param(&mut self, param: PtxKernelParam) {
        self.params.push(param);
    }

    pub fn add_atomic(&mut self, atomic: PtxAtomicSequence) {
        self.atomics.push(atomic);
    }

    pub fn add_shared_op(&mut self, op: PtxSharedOpKind) {
        self.shared_ops.push(op);
    }

    pub fn add_warp_op(&mut self, op: PtxWarpOpKind) {
        self.warp_ops.push(op);
    }

    pub fn add_tensor_op(&mut self, op: PtxTensorOpRequest) {
        self.tensor_ops.push(op);
    }

    pub fn minimum_arch(&self) -> PtxArch {
        let mut arch = std::cmp::max(
            self.launch.minimum_arch(),
            self.shared_memory.minimum_arch(),
        );
        for atomic in &self.atomics {
            arch = std::cmp::max(arch, atomic.minimum_arch());
        }
        for op in &self.shared_ops {
            arch = std::cmp::max(arch, op.arch_min());
        }
        for op in &self.warp_ops {
            arch = std::cmp::max(arch, op.arch_min());
        }
        for op in &self.tensor_ops {
            arch = std::cmp::max(arch, op.arch_min());
        }
        arch
    }

    pub fn validate(&self, arch: PtxArch) -> PtxResult<()> {
        if self.host_name.is_empty() {
            return Err(PtxSurfaceError::InvalidLaunchConfig {
                reason: "host kernel name cannot be empty",
            });
        }
        if self.ptx_entry.is_empty() {
            return Err(PtxSurfaceError::InvalidLaunchConfig {
                reason: "PTX entry name cannot be empty",
            });
        }

        self.launch.validate(arch)?;
        self.shared_memory.validate()?;

        let required_shared = self.shared_memory.required_bytes()?;
        if self.launch.shared_mem_bytes < required_shared {
            return Err(PtxSurfaceError::InvalidLaunchConfig {
                reason: "launch shared memory reservation is smaller than the plan requires",
            });
        }

        let mut param_names = HashSet::new();
        for param in &self.params {
            param.validate()?;
            if !param_names.insert(param.name.clone()) {
                return Err(PtxSurfaceError::DuplicateParamName {
                    kernel: self.host_name.clone(),
                    name: param.name.clone(),
                });
            }
        }

        for atomic in &self.atomics {
            atomic.validate(arch)?;
        }

        for op in &self.shared_ops {
            let spec = lookup_shared_op(*op)
                .ok_or(PtxSurfaceError::UnsupportedSharedOp { kind: *op, arch })?;
            if arch < spec.arch_min {
                return Err(PtxSurfaceError::UnsupportedSharedOp { kind: *op, arch });
            }
            if !arch.supports(spec.kind.feature()) {
                return Err(PtxSurfaceError::UnsupportedFeature {
                    feature: spec.kind.feature(),
                    arch,
                });
            }
        }

        for op in &self.warp_ops {
            let spec = lookup_warp_op(*op)
                .ok_or(PtxSurfaceError::UnsupportedWarpOp { kind: *op, arch })?;
            if arch < spec.arch_min {
                return Err(PtxSurfaceError::UnsupportedWarpOp { kind: *op, arch });
            }
        }

        for op in &self.tensor_ops {
            op.validate(arch)?;
        }

        Ok(())
    }
}

// ============================================================================
// Surface Catalog
// ============================================================================

pub struct PtxSurfaceCatalog {
    pub atomics: &'static [PtxAtomicOpSpec],
    pub shared_ops: &'static [PtxSharedOpSpec],
    pub warp_ops: &'static [PtxWarpOpSpec],
    pub tensor_ops: &'static [PtxTensorOpSpec],
}

impl PtxSurfaceCatalog {
    pub const fn new(
        atomics: &'static [PtxAtomicOpSpec],
        shared_ops: &'static [PtxSharedOpSpec],
        warp_ops: &'static [PtxWarpOpSpec],
        tensor_ops: &'static [PtxTensorOpSpec],
    ) -> Self {
        Self {
            atomics,
            shared_ops,
            warp_ops,
            tensor_ops,
        }
    }

    pub fn atomic(
        &self,
        kind: PtxAtomicRmwKind,
        value: PtxScalarKind,
    ) -> Option<&'static PtxAtomicOpSpec> {
        self.atomics
            .iter()
            .find(|candidate| candidate.kind == kind && candidate.value == value)
    }

    pub fn shared(&self, kind: PtxSharedOpKind) -> Option<&'static PtxSharedOpSpec> {
        self.shared_ops
            .iter()
            .find(|candidate| candidate.kind == kind)
    }

    pub fn warp(&self, kind: PtxWarpOpKind) -> Option<&'static PtxWarpOpSpec> {
        self.warp_ops
            .iter()
            .find(|candidate| candidate.kind == kind)
    }

    pub fn tensor(&self, kind: PtxTensorFixedKind) -> Option<&'static PtxTensorOpSpec> {
        self.tensor_ops
            .iter()
            .find(|candidate| candidate.kind == kind)
    }

    pub fn render_atomic_sequence(&self, sequence: PtxAtomicSequence) -> Vec<String> {
        sequence.render_lines()
    }

    pub fn render_shared_request(&self, kind: PtxSharedOpKind) -> PtxResult<&'static str> {
        self.shared(kind)
            .map(|spec| spec.mnemonic)
            .ok_or(PtxSurfaceError::UnsupportedSharedOp {
                kind,
                arch: kind.arch_min(),
            })
    }

    pub fn render_warp_request(&self, kind: PtxWarpOpKind) -> PtxResult<&'static str> {
        self.warp(kind)
            .map(|spec| spec.mnemonic)
            .ok_or(PtxSurfaceError::UnsupportedWarpOp {
                kind,
                arch: kind.arch_min(),
            })
    }

    pub fn render_tensor_request(&self, request: PtxTensorOpRequest) -> PtxResult<String> {
        request.render()
    }
}

pub const PTX_MODULE_CATALOG: PtxSurfaceCatalog =
    PtxSurfaceCatalog::new(ATOMIC_OPS, SHARED_OPS, WARP_OPS, TENSOR_OPS);

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn atomic_sequences_render_fence_wrappers() {
        let op = PTX_MODULE_CATALOG
            .atomic(PtxAtomicRmwKind::Add, PtxScalarKind::U32)
            .expect("add/u32 atomic should exist");
        let seq = PtxAtomicSequence::new(op, PtxScope::Gpu, PtxOrdering::AcqRel);
        assert_eq!(
            seq.render_lines(),
            vec![
                "fence.acq_rel.gpu".to_string(),
                "atom.add.u32".to_string(),
                "fence.acq_rel.gpu".to_string(),
            ]
        );
    }

    #[test]
    fn ptx_arch_parses_common_compute_capability_aliases() {
        assert_eq!(PtxArch::parse("sm_30"), Some(PtxArch::Sm30));
        assert_eq!(PtxArch::parse("3.5"), Some(PtxArch::Sm35));
        assert_eq!(PtxArch::parse("sm_75"), Some(PtxArch::Sm75));
        assert_eq!(PtxArch::parse("7.5"), Some(PtxArch::Sm75));
        assert_eq!(PtxArch::parse("compute_8_6"), Some(PtxArch::Sm86));
        assert_eq!(PtxArch::from_compute_capability(3, 0), Some(PtxArch::Sm30));
        assert_eq!(PtxArch::from_compute_capability(8, 9), Some(PtxArch::Sm89));
        assert_eq!(PtxArch::parse("sm_77"), None);
    }

    #[test]
    fn cluster_launch_requires_sm90() {
        let cfg = PtxLaunchConfig::new([1, 1, 1], [8, 8, 1], 0)
            .with_cluster(PtxClusterLaunchConfig::new([2, 1, 1]));
        assert!(matches!(
            cfg.validate(PtxArch::Sm80),
            Err(PtxSurfaceError::UnsupportedFeature {
                feature: PtxFeature::ClusterLaunch,
                arch: PtxArch::Sm80
            })
        ));
        assert!(cfg.validate(PtxArch::Sm90).is_ok());
    }

    #[test]
    fn plain_launch_and_shared_memory_can_target_kepler_floor() {
        let launch = PtxLaunchConfig::new([1, 1, 1], [8, 1, 1], 0);
        let shared = PtxSharedMemoryPlan::new();

        assert_eq!(launch.minimum_arch(), PtxArch::Sm30);
        assert_eq!(shared.minimum_arch(), PtxArch::Sm30);
        assert_eq!(PtxSharedOpKind::LdShared.arch_min(), PtxArch::Sm30);
        assert_eq!(PtxSharedOpKind::BarSync.arch_min(), PtxArch::Sm30);
    }

    #[test]
    fn shared_memory_plan_tracks_layout_and_barriers() {
        let mut plan = PtxSharedMemoryPlan::new();
        plan.add_segment(PtxSharedSegment::new(
            "tile_a",
            128,
            16,
            PtxAddressSpace::Shared,
        ));
        plan.add_segment(PtxSharedSegment::new(
            "tile_b",
            256,
            16,
            PtxAddressSpace::SharedCluster,
        ));
        plan.barrier_slots = 2;
        plan.dynamic_bytes = 64;
        assert_eq!(plan.required_bytes().unwrap(), 464);
        assert!(plan.validate().is_ok());
    }

    #[test]
    fn module_plan_rejects_duplicate_names() {
        let mut kernel = PtxKernelPlan::new(
            "vecadd",
            "vecadd",
            PtxLaunchConfig::new([1, 1, 1], [8, 8, 1], 0),
        );
        kernel.shared_memory = PtxSharedMemoryPlan::new();
        kernel.add_param(PtxKernelParam::scalar("a", PtxScalarKind::U32));
        kernel.add_param(PtxKernelParam::scalar("a", PtxScalarKind::U32));
        assert!(matches!(
            kernel.validate(PtxArch::Sm50),
            Err(PtxSurfaceError::DuplicateParamName { .. })
        ));
    }

    #[test]
    fn tensor_wait_group_renders_with_immediate() {
        let request = PtxTensorOpRequest::WgmmaWaitGroup(0);
        assert_eq!(request.render().unwrap(), "wgmma.wait_group.sync.aligned 0");
        assert!(request.validate(PtxArch::Sm90).is_ok());
        assert!(matches!(
            request.validate(PtxArch::Sm80),
            Err(PtxSurfaceError::UnsupportedTensorOp { .. })
        ));
    }

    #[test]
    fn mma_sync_tracks_turing_min_arch() {
        let request = PtxTensorOpRequest::Fixed(PtxTensorFixedKind::MmaSync);
        assert!(request.validate(PtxArch::Sm75).is_ok());
        assert!(matches!(
            request.validate(PtxArch::Sm70),
            Err(PtxSurfaceError::UnsupportedTensorOp { .. })
        ));
    }

    #[test]
    fn kernel_plan_minimum_arch_tracks_tensor_and_cluster_requirements() {
        let mut kernel = PtxKernelPlan::new(
            "mma_kernel",
            "mma_kernel",
            PtxLaunchConfig::new([1, 1, 1], [8, 8, 1], 0),
        );
        kernel.add_tensor_op(PtxTensorOpRequest::Fixed(PtxTensorFixedKind::MmaSync));
        assert_eq!(kernel.minimum_arch(), PtxArch::Sm75);

        kernel.launch = kernel
            .launch
            .with_cluster(PtxClusterLaunchConfig::new([2, 1, 1]));
        assert_eq!(kernel.minimum_arch(), PtxArch::Sm90);
    }
}
