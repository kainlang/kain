use crate::error::ErrorKind;
use std::fmt;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum DiagnosticCategory {
    Parse,
    Type,
    Validation,
    Codegen,
    Shader,
    Io,
    Config,
    Effect,
    Borrow,
    Runtime,
    Memory,
}

impl fmt::Display for DiagnosticCategory {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let text = match self {
            DiagnosticCategory::Parse => "parse",
            DiagnosticCategory::Type => "type",
            DiagnosticCategory::Validation => "validation",
            DiagnosticCategory::Codegen => "codegen",
            DiagnosticCategory::Shader => "shader",
            DiagnosticCategory::Io => "io",
            DiagnosticCategory::Config => "config",
            DiagnosticCategory::Effect => "effect",
            DiagnosticCategory::Borrow => "borrow",
            DiagnosticCategory::Runtime => "runtime",
            DiagnosticCategory::Memory => "memory",
        };
        write!(f, "{text}")
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum DiagnosticCode {
    ParseGeneric,
    ParseExpectedToken,
    ParseUnexpectedToken,
    ParseReservedIdentifier,
    ParseMissingDelimiterBeforeNewline,
    ParseInvalidWorldSurfaceKind,
    ParseExpectedContextualKeyword,
    TypeGeneric,
    TypeUnknownIdentifier,
    TypeWorldMissingSurface,
    TypeDuplicateSymbol,
    TypeShadowedBuiltin,
    ValidationGeneric,
    CodegenGeneric,
    CodegenUnknownVariable,
    ShaderUnsupportedCall,
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
    pub category: DiagnosticCategory,
    pub docs_key: Option<&'static str>,
    pub default_suggestion: Option<&'static str>,
}

const DIAGNOSTIC_SPECS: &[DiagnosticSpec] = &[
    DiagnosticSpec {
        code: DiagnosticCode::ParseGeneric,
        code_str: "KAIN-PARSE-0001",
        title: "Parse Error",
        category: DiagnosticCategory::Parse,
        docs_key: Some("parser/general"),
        default_suggestion: None,
    },
    DiagnosticSpec {
        code: DiagnosticCode::ParseExpectedToken,
        code_str: "KAIN-PARSE-0002",
        title: "Expected Token",
        category: DiagnosticCategory::Parse,
        docs_key: Some("parser/expected-token"),
        default_suggestion: Some(
            "Finish the current construct before continuing; the missing token is usually required by the grammar immediately before the highlighted point.",
        ),
    },
    DiagnosticSpec {
        code: DiagnosticCode::ParseUnexpectedToken,
        code_str: "KAIN-PARSE-0003",
        title: "Unexpected Token",
        category: DiagnosticCategory::Parse,
        docs_key: Some("parser/unexpected-token"),
        default_suggestion: Some(
            "Remove the stray token or reshape the surrounding syntax so the highlighted token appears in a valid grammar slot.",
        ),
    },
    DiagnosticSpec {
        code: DiagnosticCode::ParseReservedIdentifier,
        code_str: "KAIN-PARSE-0004",
        title: "Reserved Identifier",
        category: DiagnosticCategory::Parse,
        docs_key: Some("parser/reserved-identifiers"),
        default_suggestion: Some(
            "Rename the identifier so it does not collide with Kain, shader, C++, or engine-reserved keywords.",
        ),
    },
    DiagnosticSpec {
        code: DiagnosticCode::ParseMissingDelimiterBeforeNewline,
        code_str: "KAIN-PARSE-0005",
        title: "Missing Delimiter Before Newline",
        category: DiagnosticCategory::Parse,
        docs_key: Some("parser/missing-delimiter-before-newline"),
        default_suggestion: Some(
            "Kain block headers and declarations end with ':'. Insert the missing delimiter before the line break or keep the expression on one logical line.",
        ),
    },
    DiagnosticSpec {
        code: DiagnosticCode::ParseInvalidWorldSurfaceKind,
        code_str: "KAIN-PARSE-0006",
        title: "Invalid World Surface Kind",
        category: DiagnosticCategory::Parse,
        docs_key: Some("world/surface-kind"),
        default_suggestion: Some(
            "Use one of the built-in surface kinds: native_ui, viewport3d, web, or ue5.",
        ),
    },
    DiagnosticSpec {
        code: DiagnosticCode::ParseExpectedContextualKeyword,
        code_str: "KAIN-PARSE-0007",
        title: "Expected Contextual Keyword",
        category: DiagnosticCategory::Parse,
        docs_key: Some("parser/contextual-keywords"),
        default_suggestion: Some(
            "Contextual keywords only become special in specific grammar slots. Check whether a nearby identifier or missing delimiter shifted the parser out of that slot.",
        ),
    },
    DiagnosticSpec {
        code: DiagnosticCode::TypeGeneric,
        code_str: "KAIN-TYPE-0001",
        title: "Type Error",
        category: DiagnosticCategory::Type,
        docs_key: Some("types/general"),
        default_suggestion: None,
    },
    DiagnosticSpec {
        code: DiagnosticCode::TypeUnknownIdentifier,
        code_str: "KAIN-TYPE-0002",
        title: "Unknown Identifier",
        category: DiagnosticCategory::Type,
        docs_key: Some("types/unknown-identifier"),
        default_suggestion: Some(
            "Check for a misspelling, a missing import, or a value that only exists on the host side and has not been bridged into Kain.",
        ),
    },
    DiagnosticSpec {
        code: DiagnosticCode::TypeWorldMissingSurface,
        code_str: "KAIN-TYPE-0003",
        title: "World Requires Surface",
        category: DiagnosticCategory::Type,
        docs_key: Some("world/missing-surface"),
        default_suggestion: Some(
            "Add at least one surface projection such as `surface native_ui => MyPanel` inside the world body.",
        ),
    },
    DiagnosticSpec {
        code: DiagnosticCode::TypeDuplicateSymbol,
        code_str: "KAIN-TYPE-0004",
        title: "Duplicate Symbol",
        category: DiagnosticCategory::Type,
        docs_key: Some("types/duplicate-symbol"),
        default_suggestion: Some(
            "Rename one declaration or explicitly alias imports so each visible symbol has a unique meaning in the namespace.",
        ),
    },
    DiagnosticSpec {
        code: DiagnosticCode::TypeShadowedBuiltin,
        code_str: "KAIN-TYPE-0005",
        title: "Builtin Symbol Shadowed",
        category: DiagnosticCategory::Type,
        docs_key: Some("types/shadowed-builtin"),
        default_suggestion: Some(
            "Choose a distinct local name, or import the builtin symbol under an alias if both names need to coexist.",
        ),
    },
    DiagnosticSpec {
        code: DiagnosticCode::ValidationGeneric,
        code_str: "KAIN-VALIDATE-0001",
        title: "Validation Error",
        category: DiagnosticCategory::Validation,
        docs_key: Some("validation/general"),
        default_suggestion: None,
    },
    DiagnosticSpec {
        code: DiagnosticCode::CodegenGeneric,
        code_str: "KAIN-CODEGEN-0001",
        title: "Codegen Error",
        category: DiagnosticCategory::Codegen,
        docs_key: Some("codegen/general"),
        default_suggestion: None,
    },
    DiagnosticSpec {
        code: DiagnosticCode::CodegenUnknownVariable,
        code_str: "KAIN-CODEGEN-0002",
        title: "Unknown Codegen Variable",
        category: DiagnosticCategory::Codegen,
        docs_key: Some("codegen/unknown-variable"),
        default_suggestion: Some(
            "The lowered backend could not find a value for this symbol. Check whether the frontend should have rejected it earlier or whether the lowering pass lost the binding.",
        ),
    },
    DiagnosticSpec {
        code: DiagnosticCode::ShaderUnsupportedCall,
        code_str: "KAIN-SHADER-0001",
        title: "Unsupported Shader Call",
        category: DiagnosticCategory::Shader,
        docs_key: Some("shader/unsupported-call"),
        default_suggestion: Some(
            "Replace the call with a supported shader intrinsic or move the computation to a host-side or precomputed stage.",
        ),
    },
    DiagnosticSpec {
        code: DiagnosticCode::IoGeneric,
        code_str: "KAIN-IO-0001",
        title: "IO Error",
        category: DiagnosticCategory::Io,
        docs_key: Some("io/general"),
        default_suggestion: None,
    },
    DiagnosticSpec {
        code: DiagnosticCode::ConfigGeneric,
        code_str: "KAIN-CONFIG-0001",
        title: "Config Error",
        category: DiagnosticCategory::Config,
        docs_key: Some("config/general"),
        default_suggestion: None,
    },
    DiagnosticSpec {
        code: DiagnosticCode::EffectViolation,
        code_str: "KAIN-EFFECT-0001",
        title: "Effect Violation",
        category: DiagnosticCategory::Effect,
        docs_key: Some("effects/violation"),
        default_suggestion: Some(
            "Align caller and callee effects, or move side effects behind a compatible boundary.",
        ),
    },
    DiagnosticSpec {
        code: DiagnosticCode::BorrowGeneric,
        code_str: "KAIN-BORROW-0001",
        title: "Borrow Error",
        category: DiagnosticCategory::Borrow,
        docs_key: Some("borrow/general"),
        default_suggestion: None,
    },
    DiagnosticSpec {
        code: DiagnosticCode::RuntimeGeneric,
        code_str: "KAIN-RUNTIME-0001",
        title: "Runtime Error",
        category: DiagnosticCategory::Runtime,
        docs_key: Some("runtime/general"),
        default_suggestion: None,
    },
    DiagnosticSpec {
        code: DiagnosticCode::MemoryLoweringRequired,
        code_str: "KAIN-MEM-0001",
        title: "Memory Lowering Required",
        category: DiagnosticCategory::Memory,
        docs_key: Some("memory/lowering"),
        default_suggestion: Some(
            "Add or select a lowering policy before targeting backends that do not support raw memory semantics directly.",
        ),
    },
    DiagnosticSpec {
        code: DiagnosticCode::MemoryUnsupportedBackend,
        code_str: "KAIN-MEM-0002",
        title: "Memory Semantics Unsupported By Backend",
        category: DiagnosticCategory::Memory,
        docs_key: Some("memory/backend-capabilities"),
        default_suggestion: Some(
            "Choose a backend with low-level memory support or lower pointers and raw storage into a backend-safe form first.",
        ),
    },
    DiagnosticSpec {
        code: DiagnosticCode::MemoryIllegalBitfieldAddress,
        code_str: "KAIN-MEM-0003",
        title: "Illegal Bitfield Address",
        category: DiagnosticCategory::Memory,
        docs_key: Some("memory/bitfields"),
        default_suggestion: Some(
            "Do not take the address of a C bitfield directly; lower it into a load/store/mask operation instead.",
        ),
    },
    DiagnosticSpec {
        code: DiagnosticCode::MemoryLayoutOverflow,
        code_str: "KAIN-MEM-0004",
        title: "Memory Layout Overflow",
        category: DiagnosticCategory::Memory,
        docs_key: Some("memory/layout-overflow"),
        default_suggestion: Some(
            "Reduce the aggregate size or field count, or split the layout so size and offset calculations stay within the target address space.",
        ),
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
        ErrorKind::Effect => DiagnosticCode::EffectViolation,
        ErrorKind::Borrow => DiagnosticCode::BorrowGeneric,
        ErrorKind::Runtime => DiagnosticCode::RuntimeGeneric,
    }
}
