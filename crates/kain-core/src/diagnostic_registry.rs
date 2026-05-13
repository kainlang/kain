use crate::error::ErrorKind;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum DiagnosticCode {
    ParseGeneric,
    TypeGeneric,
    ValidationGeneric,
    CodegenGeneric,
    IoGeneric,
    ConfigGeneric,
    EffectViolation,
    BorrowGeneric,
    RuntimeGeneric,
    MemoryLoweringRequired,
    MemoryUnsupportedBackend,
    MemoryIllegalBitfieldAddress,
    MemoryLayoutOverflow,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct DiagnosticSpec {
    pub code: DiagnosticCode,
    pub code_str: &'static str,
    pub title: &'static str,
    pub docs_key: Option<&'static str>,
    pub default_suggestion: Option<&'static str>,
}

const DIAGNOSTIC_SPECS: &[DiagnosticSpec] = &[
    DiagnosticSpec {
        code: DiagnosticCode::ParseGeneric,
        code_str: "KAIN-PARSE-0001",
        title: "Parse Error",
        docs_key: Some("parser/general"),
        default_suggestion: None,
    },
    DiagnosticSpec {
        code: DiagnosticCode::TypeGeneric,
        code_str: "KAIN-TYPE-0001",
        title: "Type Error",
        docs_key: Some("types/general"),
        default_suggestion: None,
    },
    DiagnosticSpec {
        code: DiagnosticCode::ValidationGeneric,
        code_str: "KAIN-VALIDATE-0001",
        title: "Validation Error",
        docs_key: Some("validation/general"),
        default_suggestion: None,
    },
    DiagnosticSpec {
        code: DiagnosticCode::CodegenGeneric,
        code_str: "KAIN-CODEGEN-0001",
        title: "Codegen Error",
        docs_key: Some("codegen/general"),
        default_suggestion: None,
    },
    DiagnosticSpec {
        code: DiagnosticCode::IoGeneric,
        code_str: "KAIN-IO-0001",
        title: "IO Error",
        docs_key: Some("io/general"),
        default_suggestion: None,
    },
    DiagnosticSpec {
        code: DiagnosticCode::ConfigGeneric,
        code_str: "KAIN-CONFIG-0001",
        title: "Config Error",
        docs_key: Some("config/general"),
        default_suggestion: None,
    },
    DiagnosticSpec {
        code: DiagnosticCode::EffectViolation,
        code_str: "KAIN-EFFECT-0001",
        title: "Effect Violation",
        docs_key: Some("effects/violation"),
        default_suggestion: Some("Align caller and callee effects, or move side effects behind a compatible boundary."),
    },
    DiagnosticSpec {
        code: DiagnosticCode::BorrowGeneric,
        code_str: "KAIN-BORROW-0001",
        title: "Borrow Error",
        docs_key: Some("borrow/general"),
        default_suggestion: None,
    },
    DiagnosticSpec {
        code: DiagnosticCode::RuntimeGeneric,
        code_str: "KAIN-RUNTIME-0001",
        title: "Runtime Error",
        docs_key: Some("runtime/general"),
        default_suggestion: None,
    },
    DiagnosticSpec {
        code: DiagnosticCode::MemoryLoweringRequired,
        code_str: "KAIN-MEM-0001",
        title: "Memory Lowering Required",
        docs_key: Some("memory/lowering"),
        default_suggestion: Some("Add or select a lowering policy before targeting backends that do not support raw memory semantics directly."),
    },
    DiagnosticSpec {
        code: DiagnosticCode::MemoryUnsupportedBackend,
        code_str: "KAIN-MEM-0002",
        title: "Memory Semantics Unsupported By Backend",
        docs_key: Some("memory/backend-capabilities"),
        default_suggestion: Some("Choose a backend with low-level memory support or lower pointers and raw storage into a backend-safe form first."),
    },
    DiagnosticSpec {
        code: DiagnosticCode::MemoryIllegalBitfieldAddress,
        code_str: "KAIN-MEM-0003",
        title: "Illegal Bitfield Address",
        docs_key: Some("memory/bitfields"),
        default_suggestion: Some("Do not take the address of a C bitfield directly; lower it into a load/store/mask operation instead."),
    },
    DiagnosticSpec {
        code: DiagnosticCode::MemoryLayoutOverflow,
        code_str: "KAIN-MEM-0004",
        title: "Memory Layout Overflow",
        docs_key: Some("memory/layout-overflow"),
        default_suggestion: Some("Reduce the aggregate size or field count, or split the layout so size and offset calculations stay within the target address space."),
    },
];

pub fn spec_for_code(code: DiagnosticCode) -> &'static DiagnosticSpec {
    DIAGNOSTIC_SPECS
        .iter()
        .find(|spec| spec.code == code)
        .expect("diagnostic code must have a registry entry")
}

pub fn default_code_for_kind(kind: ErrorKind) -> DiagnosticCode {
    match kind {
        ErrorKind::Parse => DiagnosticCode::ParseGeneric,
        ErrorKind::Type => DiagnosticCode::TypeGeneric,
        ErrorKind::Validation => DiagnosticCode::ValidationGeneric,
        ErrorKind::Codegen => DiagnosticCode::CodegenGeneric,
        ErrorKind::Io => DiagnosticCode::IoGeneric,
        ErrorKind::Config => DiagnosticCode::ConfigGeneric,
    }
}
