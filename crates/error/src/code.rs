//! Runtime-extensible diagnostic code.
//!
//! `DiagnosticCode` stays string-backed so the scratch registry can stay
//! data-driven, but we also preserve the historical associated-constant
//! surface so existing crates keep compiling without churn.
//!
//! ## Auto-slot convention
//!
//! Codes are assigned sequentially within each domain category, sourced from
//! `specs/<domain>.toml` in append order.  To add a new code:
//!   1. Append a `[[diagnostics]]` entry to the owning TOML file.
//!   2. Use the next available number for that prefix (build.rs sorts by code
//!      string, so numbers determine final order).
//!   3. Add the matching `pub const` below so callers have a typed name.
//!
//! Do **not** recycle numbers — retired codes stay in the TOML as tombstones
//! so history is preserved.

use std::fmt;

/// A diagnostic code key. Stored as a `&'static str` for zero-allocation
/// lookup in the registry.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct DiagnosticCode(&'static str);

#[allow(non_upper_case_globals)]
impl DiagnosticCode {
    /// Create a code from a static string.
    pub const fn new(code: &'static str) -> Self {
        Self(code)
    }

    // ── Parse ────────────────────────────────────────────────────────────
    pub const ParseGeneric: Self = Self::new("KAIN-PARSE-0001");
    pub const ParseExpectedToken: Self = Self::new("KAIN-PARSE-0002");
    pub const ParseUnexpectedToken: Self = Self::new("KAIN-PARSE-0003");
    pub const ParseReservedIdentifier: Self = Self::new("KAIN-PARSE-0004");
    pub const ParseMissingDelimiterBeforeNewline: Self = Self::new("KAIN-PARSE-0005");
    pub const ParseInvalidWorldSurfaceKind: Self = Self::new("KAIN-PARSE-0006");
    pub const ParseExpectedContextualKeyword: Self = Self::new("KAIN-PARSE-0007");
    pub const ParseUnclosedDelimiter: Self = Self::new("KAIN-PARSE-0008");
    pub const ParseMismatchedDelimiter: Self = Self::new("KAIN-PARSE-0009");

    // ── Type ─────────────────────────────────────────────────────────────
    pub const TypeGeneric: Self = Self::new("KAIN-TYPE-0001");
    pub const TypeUnknownIdentifier: Self = Self::new("KAIN-TYPE-0002");
    pub const TypeWorldMissingSurface: Self = Self::new("KAIN-TYPE-0003");
    pub const TypeDuplicateSymbol: Self = Self::new("KAIN-TYPE-0004");
    pub const TypeShadowedBuiltin: Self = Self::new("KAIN-TYPE-0005");
    pub const TypeMissingAnnotation: Self = Self::new("KAIN-TYPE-0006");
    pub const TypeTraitNotSatisfied: Self = Self::new("KAIN-TYPE-0007");
    pub const TypeTraitMethodMissing: Self = Self::new("KAIN-TYPE-0008");
    pub const TypeAmbiguousTrait: Self = Self::new("KAIN-TYPE-0009");
    pub const TypeUnresolvedImport: Self = Self::new("KAIN-TYPE-0010");
    pub const TypeCyclicDefinition: Self = Self::new("KAIN-TYPE-0011");
    pub const TypeMutabilityConflict: Self = Self::new("KAIN-TYPE-0012");
    pub const TypePatternInexhaustive: Self = Self::new("KAIN-TYPE-0013");
    pub const TypeRecursiveWithoutIndirection: Self = Self::new("KAIN-TYPE-0014");
    pub const TypeAliascycle: Self = Self::new("KAIN-TYPE-0015");
    pub const TypeImplOnForeignType: Self = Self::new("KAIN-TYPE-0016");
    pub const TypeSelfInStaticContext: Self = Self::new("KAIN-TYPE-0017");
    pub const TypeInvalidParamCount: Self = Self::new("KAIN-TYPE-0018");
    pub const TypeArgKindMismatch: Self = Self::new("KAIN-TYPE-0019");
    pub const TypeReturnTypeMismatch: Self = Self::new("KAIN-TYPE-0020");
    pub const TypeMissingReturnInNonVoid: Self = Self::new("KAIN-TYPE-0021");
    pub const TypeVoidValueInExpression: Self = Self::new("KAIN-TYPE-0022");
    pub const TypeCallableExpected: Self = Self::new("KAIN-TYPE-0023");
    pub const TypeFieldNotFound: Self = Self::new("KAIN-TYPE-0024");
    pub const TypeMismatch: Self = Self::new("KAIN-TYPE-0025");
    pub const TypeIndexNotSupported: Self = Self::new("KAIN-TYPE-0026");

    // ── Validation ───────────────────────────────────────────────────────
    pub const ValidationGeneric: Self = Self::new("KAIN-VALIDATE-0001");

    // ── Codegen ──────────────────────────────────────────────────────────
    pub const CodegenGeneric: Self = Self::new("KAIN-CODEGEN-0001");
    pub const CodegenUnknownVariable: Self = Self::new("KAIN-CODEGEN-0002");
    pub const CodegenLoweringFailed: Self = Self::new("KAIN-CODEGEN-0003");
    pub const CodegenBackendFailed: Self = Self::new("KAIN-CODEGEN-0004");
    pub const CodegenLinkingFailed: Self = Self::new("KAIN-CODEGEN-0005");
    pub const CodegenUnsupportedTarget: Self = Self::new("KAIN-CODEGEN-0006");
    pub const CodegenCapabilityMissing: Self = Self::new("KAIN-CODEGEN-0007");
    pub const CodegenForeignAbiMismatch: Self = Self::new("KAIN-CODEGEN-0008");
    pub const CodegenIntrinsicNotFound: Self = Self::new("KAIN-CODEGEN-0009");
    pub const CodegenOptimizationFailed: Self = Self::new("KAIN-CODEGEN-0010");
    pub const CodegenBudgetExceeded: Self = Self::new("KAIN-CODEGEN-0011");

    // ── Shader ───────────────────────────────────────────────────────────
    pub const ShaderUnsupportedCall: Self = Self::new("KAIN-SHADER-0001");
    pub const ShaderStageMismatch: Self = Self::new("KAIN-SHADER-0002");
    pub const ShaderUniformBindingError: Self = Self::new("KAIN-SHADER-0003");
    pub const ShaderComputeDispatchDimension: Self = Self::new("KAIN-SHADER-0004");
    pub const ShaderResourceNotGpuCompatible: Self = Self::new("KAIN-SHADER-0005");
    pub const ShaderVertexInputLayout: Self = Self::new("KAIN-SHADER-0006");
    pub const ShaderFragmentOutputLayout: Self = Self::new("KAIN-SHADER-0007");
    pub const ShaderCollapseTargetInvalid: Self = Self::new("KAIN-SHADER-0008");
    pub const ShaderFanoutWidthExceeded: Self = Self::new("KAIN-SHADER-0009");
    pub const ShaderCompilationFailed: Self = Self::new("KAIN-SHADER-0010");
    pub const ShaderGpuMemoryBudgetExceeded: Self = Self::new("KAIN-SHADER-0011");
    pub const ShaderSharedMemoryBankConflict: Self = Self::new("KAIN-SHADER-0012");

    // ── Effect ───────────────────────────────────────────────────────────
    pub const EffectViolation: Self = Self::new("KAIN-EFFECT-0001");
    pub const EffectMissingCapability: Self = Self::new("KAIN-EFFECT-0002");
    pub const EffectPolymorphismMismatch: Self = Self::new("KAIN-EFFECT-0003");
    pub const EffectPureSideEffect: Self = Self::new("KAIN-EFFECT-0004");
    pub const EffectCapabilityGateFailed: Self = Self::new("KAIN-EFFECT-0005");
    pub const EffectAsyncInSync: Self = Self::new("KAIN-EFFECT-0006");
    pub const EffectAwaitOutsideAsync: Self = Self::new("KAIN-EFFECT-0007");
    pub const EffectGpuInHost: Self = Self::new("KAIN-EFFECT-0008");
    pub const EffectReactiveCycle: Self = Self::new("KAIN-EFFECT-0009");
    pub const EffectUnsafeDisallowed: Self = Self::new("KAIN-EFFECT-0010");
    pub const EffectLeakageThroughPublicApi: Self = Self::new("KAIN-EFFECT-0011");
    pub const EffectConflictingAnnotations: Self = Self::new("KAIN-EFFECT-0012");

    // ── Borrow / Ownership ───────────────────────────────────────────────
    pub const BorrowGeneric: Self = Self::new("KAIN-BORROW-0001");
    pub const BorrowMultipleMutable: Self = Self::new("KAIN-BORROW-0002");
    pub const BorrowMutationConflict: Self = Self::new("KAIN-BORROW-0003");
    pub const BorrowUseAfterMove: Self = Self::new("KAIN-BORROW-0004");
    pub const BorrowSharedWithoutAnnotation: Self = Self::new("KAIN-BORROW-0005");
    pub const BorrowSingleWriterViolation: Self = Self::new("KAIN-BORROW-0006");
    pub const BorrowWeakUpgradeUnsafe: Self = Self::new("KAIN-BORROW-0007");
    pub const BorrowLifetimeMismatch: Self = Self::new("KAIN-BORROW-0008");
    pub const BorrowSendConstraintViolation: Self = Self::new("KAIN-BORROW-0009");
    pub const BorrowImplicitCloneOnLargeValue: Self = Self::new("KAIN-BORROW-0010");

    // ── Memory ───────────────────────────────────────────────────────────
    pub const MemoryLoweringRequired: Self = Self::new("KAIN-MEM-0001");
    pub const MemoryUnsupportedBackend: Self = Self::new("KAIN-MEM-0002");
    pub const MemoryIllegalBitfieldAddress: Self = Self::new("KAIN-MEM-0003");
    pub const MemoryLayoutOverflow: Self = Self::new("KAIN-MEM-0004");
    pub const MemoryAlignmentNotSatisfied: Self = Self::new("KAIN-MEM-0005");
    pub const MemoryNullDeref: Self = Self::new("KAIN-MEM-0006");
    pub const MemoryOutOfBounds: Self = Self::new("KAIN-MEM-0007");
    pub const MemoryAddressSpaceMismatch: Self = Self::new("KAIN-MEM-0008");

    // ── World ────────────────────────────────────────────────────────────
    pub const WorldGeneric: Self = Self::new("KAIN-WORLD-0001");
    pub const WorldDuplicateSurface: Self = Self::new("KAIN-WORLD-0002");
    pub const WorldSurfaceComponentTypeError: Self = Self::new("KAIN-WORLD-0003");
    pub const WorldOrphan: Self = Self::new("KAIN-WORLD-0004");
    pub const WorldEntanglementInvalid: Self = Self::new("KAIN-WORLD-0005");
    pub const WorldTeleportInvalid: Self = Self::new("KAIN-WORLD-0006");
    pub const WorldCrossReferenceCycle: Self = Self::new("KAIN-WORLD-0007");
    pub const WorldSurfacePlatformMismatch: Self = Self::new("KAIN-WORLD-0008");

    // ── Actor ────────────────────────────────────────────────────────────
    pub const ActorGeneric: Self = Self::new("KAIN-ACTOR-0001");

    // ── Runtime ──────────────────────────────────────────────────────────
    pub const RuntimeGeneric: Self = Self::new("KAIN-RUNTIME-0001");
    pub const RuntimeActorPanic: Self = Self::new("KAIN-RUNTIME-0002");
    pub const RuntimeMessageDeliveryFailed: Self = Self::new("KAIN-RUNTIME-0003");
    pub const RuntimeResourceExhausted: Self = Self::new("KAIN-RUNTIME-0004");
    pub const RuntimeDeadlockDetected: Self = Self::new("KAIN-RUNTIME-0005");
    pub const RuntimeWorldInitFailed: Self = Self::new("KAIN-RUNTIME-0006");
    pub const RuntimeShaderDispatchFailed: Self = Self::new("KAIN-RUNTIME-0007");
    pub const RuntimeTimeoutExceeded: Self = Self::new("KAIN-RUNTIME-0008");

    // ── Comptime ─────────────────────────────────────────────────────────
    pub const ComptimeGeneric: Self = Self::new("KAIN-COMPTIME-0001");
    pub const ComptimeRecursionLimit: Self = Self::new("KAIN-COMPTIME-0002");
    pub const ComptimeAccessToRuntimeValue: Self = Self::new("KAIN-COMPTIME-0003");
    pub const ComptimeMacroExpansionError: Self = Self::new("KAIN-COMPTIME-0004");
    pub const ComptimePatchTargetNotFound: Self = Self::new("KAIN-COMPTIME-0005");
    pub const ComptimeLawViolation: Self = Self::new("KAIN-COMPTIME-0006");
    pub const ComptimeAxiomContradiction: Self = Self::new("KAIN-COMPTIME-0007");
    pub const ComptimeOrchestrateDependencyCycle: Self = Self::new("KAIN-COMPTIME-0008");
    pub const ComptimeConvergeFailed: Self = Self::new("KAIN-COMPTIME-0009");
    pub const ComptimeShatterPatternIncomplete: Self = Self::new("KAIN-COMPTIME-0010");

    // ── State ────────────────────────────────────────────────────────────
    pub const StateGeneric: Self = Self::new("KAIN-STATE-0001");
    pub const StateInexhaustive: Self = Self::new("KAIN-STATE-0002");
    pub const StateTransitionCycle: Self = Self::new("KAIN-STATE-0003");
    pub const StateInvalidTransition: Self = Self::new("KAIN-STATE-0004");
    pub const StatePulseWithoutState: Self = Self::new("KAIN-STATE-0005");
    pub const StateGuaranteeViolation: Self = Self::new("KAIN-STATE-0006");
    pub const StateEveryClauseUnbounded: Self = Self::new("KAIN-STATE-0007");
    pub const StateFallbackUnreachable: Self = Self::new("KAIN-STATE-0008");

    // ── Converge ─────────────────────────────────────────────────────────
    // Fast-lane dispatch selection, spec/fast contract verification, verifier
    // sampling, lane selection failure, and CPUID-gated capability gaps.
    pub const ConvergeGeneric: Self = Self::new("KAIN-CONVERGE-0001");
    pub const ConvergeMissingSpecLane: Self = Self::new("KAIN-CONVERGE-0002");
    pub const ConvergeFastLaneMismatch: Self = Self::new("KAIN-CONVERGE-0003");
    pub const ConvergeVerifierFailed: Self = Self::new("KAIN-CONVERGE-0004");
    pub const ConvergeCapabilityGapAtTarget: Self = Self::new("KAIN-CONVERGE-0005");
    pub const ConvergeReturnTypeDivergence: Self = Self::new("KAIN-CONVERGE-0006");
    pub const ConvergeEffectSetDivergence: Self = Self::new("KAIN-CONVERGE-0007");
    pub const ConvergeAmbiguousLaneSelection: Self = Self::new("KAIN-CONVERGE-0008");

    // ── Entangle ─────────────────────────────────────────────────────────
    // Bidirectional world-state coupling: cycle detection, single_writer
    // policy, dangling references, cross-world scope.
    pub const EntangleGeneric: Self = Self::new("KAIN-ENTANGLE-0001");
    pub const EntangleCycleDetected: Self = Self::new("KAIN-ENTANGLE-0002");
    pub const EntangleSingleWriterViolation: Self = Self::new("KAIN-ENTANGLE-0003");
    pub const EntangleDanglingReference: Self = Self::new("KAIN-ENTANGLE-0004");
    pub const EntangleCrossWorldScope: Self = Self::new("KAIN-ENTANGLE-0005");
    pub const EntangleTypeMismatch: Self = Self::new("KAIN-ENTANGLE-0006");
    pub const EntangleDirectionConflict: Self = Self::new("KAIN-ENTANGLE-0007");

    // ── Patch / Law ──────────────────────────────────────────────────────
    // Transactional world mutation: patch target validation, law
    // precondition/postcondition failure, patch applied outside world scope.
    pub const PatchGeneric: Self = Self::new("KAIN-PATCH-0001");
    pub const PatchTargetNotWorld: Self = Self::new("KAIN-PATCH-0002");
    pub const PatchLawPreconditionFailed: Self = Self::new("KAIN-PATCH-0003");
    pub const PatchLawPostconditionFailed: Self = Self::new("KAIN-PATCH-0004");
    pub const PatchAppliedOutsideWorldScope: Self = Self::new("KAIN-PATCH-0005");
    pub const PatchConflictingMutation: Self = Self::new("KAIN-PATCH-0006");
    pub const PatchLawReturnTypeMismatch: Self = Self::new("KAIN-PATCH-0007");

    // ── Pulse Budget ─────────────────────────────────────────────────────
    // Real-time safety: budget constraint violations detected at compile
    // time inside `pulse budget(alloc, lock, io)` scopes.
    pub const PulseBudgetAlloc: Self = Self::new("KAIN-PULSE-BUDGET-0001");
    pub const PulseBudgetLock: Self = Self::new("KAIN-PULSE-BUDGET-0002");
    pub const PulseBudgetIO: Self = Self::new("KAIN-PULSE-BUDGET-0003");

    // ── Misc leaf domains ────────────────────────────────────────────────
    pub const IoGeneric: Self = Self::new("KAIN-IO-0001");
    pub const ConfigGeneric: Self = Self::new("KAIN-CONFIG-0001");
    pub const TestGeneric: Self = Self::new("KAIN-TEST-0001");
    pub const InternalGeneric: Self = Self::new("KAIN-INTERNAL-0001");

    // ── Accessors ────────────────────────────────────────────────────────

    /// The canonical string form, e.g. `"KAIN-PARSE-0005"`.
    pub fn as_str(self) -> &'static str {
        self.0
    }

    /// The category prefix, e.g. `"PARSE"`, extracted from the code.
    pub fn category_prefix(self) -> &'static str {
        let s = self.0;
        if let Some(rest) = s.strip_prefix("KAIN-") {
            if let Some(idx) = rest.rfind('-') {
                &rest[..idx]
            } else {
                rest
            }
        } else {
            s
        }
    }

    /// The numeric suffix, e.g. `5` for `KAIN-PARSE-0005`.
    pub fn number(self) -> u32 {
        let s = self.0;
        if let Some(idx) = s.rfind('-') {
            s[idx + 1..].parse().unwrap_or(0)
        } else {
            0
        }
    }
}

impl fmt::Display for DiagnosticCode {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.0)
    }
}

impl From<&'static str> for DiagnosticCode {
    fn from(s: &'static str) -> Self {
        Self::new(s)
    }
}

impl PartialEq<&str> for DiagnosticCode {
    fn eq(&self, other: &&str) -> bool {
        self.0 == *other
    }
}

impl PartialEq<DiagnosticCode> for &str {
    fn eq(&self, other: &DiagnosticCode) -> bool {
        *self == other.0
    }
}
