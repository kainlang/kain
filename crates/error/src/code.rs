//! Runtime-extensible diagnostic code.
//!
//! `DiagnosticCode` stays string-backed so the scratch registry can stay
//! data-driven, but we also preserve the historical associated-constant
//! surface (`DiagnosticCode::ParseGeneric`, etc.) so existing crates keep
//! compiling without churn.

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

    pub const ParseGeneric: Self = Self::new("KAIN-PARSE-0001");
    pub const ParseExpectedToken: Self = Self::new("KAIN-PARSE-0002");
    pub const ParseUnexpectedToken: Self = Self::new("KAIN-PARSE-0003");
    pub const ParseReservedIdentifier: Self = Self::new("KAIN-PARSE-0004");
    pub const ParseMissingDelimiterBeforeNewline: Self = Self::new("KAIN-PARSE-0005");
    pub const ParseInvalidWorldSurfaceKind: Self = Self::new("KAIN-PARSE-0006");
    pub const ParseExpectedContextualKeyword: Self = Self::new("KAIN-PARSE-0007");

    pub const TypeGeneric: Self = Self::new("KAIN-TYPE-0001");
    pub const TypeUnknownIdentifier: Self = Self::new("KAIN-TYPE-0002");
    pub const TypeWorldMissingSurface: Self = Self::new("KAIN-TYPE-0003");
    pub const TypeDuplicateSymbol: Self = Self::new("KAIN-TYPE-0004");
    pub const TypeShadowedBuiltin: Self = Self::new("KAIN-TYPE-0005");

    pub const ValidationGeneric: Self = Self::new("KAIN-VALIDATE-0001");

    pub const CodegenGeneric: Self = Self::new("KAIN-CODEGEN-0001");
    pub const CodegenUnknownVariable: Self = Self::new("KAIN-CODEGEN-0002");

    pub const ShaderUnsupportedCall: Self = Self::new("KAIN-SHADER-0001");

    pub const IoGeneric: Self = Self::new("KAIN-IO-0001");
    pub const ConfigGeneric: Self = Self::new("KAIN-CONFIG-0001");
    pub const EffectViolation: Self = Self::new("KAIN-EFFECT-0001");
    pub const BorrowGeneric: Self = Self::new("KAIN-BORROW-0001");
    pub const RuntimeGeneric: Self = Self::new("KAIN-RUNTIME-0001");

    pub const MemoryLoweringRequired: Self = Self::new("KAIN-MEM-0001");
    pub const MemoryUnsupportedBackend: Self = Self::new("KAIN-MEM-0002");
    pub const MemoryIllegalBitfieldAddress: Self = Self::new("KAIN-MEM-0003");
    pub const MemoryLayoutOverflow: Self = Self::new("KAIN-MEM-0004");

    pub const WorldGeneric: Self = Self::new("KAIN-WORLD-0001");
    pub const ActorGeneric: Self = Self::new("KAIN-ACTOR-0001");
    pub const ComptimeGeneric: Self = Self::new("KAIN-COMPTIME-0001");
    pub const StateGeneric: Self = Self::new("KAIN-STATE-0001");
    pub const TestGeneric: Self = Self::new("KAIN-TEST-0001");
    pub const InternalGeneric: Self = Self::new("KAIN-INTERNAL-0001");

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
