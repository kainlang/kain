pub use crate::code::DiagnosticCode;
use crate::report::ErrorKind;
use once_cell::sync::Lazy;
use std::collections::HashMap;
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
    World,
    Actor,
    Comptime,
    State,
    Test,
    Internal,
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
            DiagnosticCategory::World => "world",
            DiagnosticCategory::Actor => "actor",
            DiagnosticCategory::Comptime => "comptime",
            DiagnosticCategory::State => "state",
            DiagnosticCategory::Test => "test",
            DiagnosticCategory::Internal => "internal",
        };
        write!(f, "{text}")
    }
}

#[derive(Debug, Clone, Copy)]
pub struct DiagnosticSpec {
    pub code: DiagnosticCode,
    pub code_str: &'static str,
    pub title: &'static str,
    pub category: DiagnosticCategory,
    pub docs_key: Option<&'static str>,
    pub default_suggestion: Option<&'static str>,
}

static COMPAT_SPECS: Lazy<HashMap<DiagnosticCode, DiagnosticSpec>> = Lazy::new(build_compat_specs);

pub fn spec_for_code(code: DiagnosticCode) -> &'static DiagnosticSpec {
    COMPAT_SPECS
        .get(&code)
        .unwrap_or_else(|| panic!("diagnostic code {} missing compat registry entry", code))
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
        ErrorKind::World => DiagnosticCode::WorldGeneric,
        ErrorKind::Shader => DiagnosticCode::ShaderUnsupportedCall,
        ErrorKind::Component => DiagnosticCode::ActorGeneric,
        ErrorKind::Comptime => DiagnosticCode::ComptimeGeneric,
        ErrorKind::State => DiagnosticCode::StateGeneric,
        ErrorKind::Test => DiagnosticCode::TestGeneric,
        ErrorKind::Memory => DiagnosticCode::MemoryLoweringRequired,
        ErrorKind::Internal => DiagnosticCode::InternalGeneric,
    }
}

fn build_compat_specs() -> HashMap<DiagnosticCode, DiagnosticSpec> {
    let mut specs = HashMap::new();

    for rich in crate::registry::registry().all_specs() {
        specs.insert(
            rich.code,
            DiagnosticSpec {
                code: rich.code,
                code_str: rich.code.as_str(),
                title: leak_text(rich.title.clone()),
                category: category_from_code(rich.code),
                docs_key: optional_leak(rich.docs_key.clone()),
                default_suggestion: rich
                    .fixit
                    .clone()
                    .and_then(optional_leak)
                    .or_else(|| derive_default_suggestion(&rich.help)),
            },
        );
    }

    // Legacy Kain core truth that must survive the swap.
    specs.insert(
        DiagnosticCode::TypeWorldMissingSurface,
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
    );

    specs.insert(
        DiagnosticCode::ValidationGeneric,
        DiagnosticSpec {
            code: DiagnosticCode::ValidationGeneric,
            code_str: "KAIN-VALIDATE-0001",
            title: "Validation Error",
            category: DiagnosticCategory::Validation,
            docs_key: Some("validation/general"),
            default_suggestion: None,
        },
    );

    specs.insert(
        DiagnosticCode::InternalGeneric,
        DiagnosticSpec {
            code: DiagnosticCode::InternalGeneric,
            code_str: "KAIN-INTERNAL-0001",
            title: "Internal Error",
            category: DiagnosticCategory::Internal,
            docs_key: Some("internal/general"),
            default_suggestion: Some(
                "This usually indicates a compiler bug or an unexpected unsupported path. Capture the surrounding repro and inspect the originating pass.",
            ),
        },
    );

    specs
}

fn category_from_code(code: DiagnosticCode) -> DiagnosticCategory {
    match code.category_prefix() {
        "PARSE" => DiagnosticCategory::Parse,
        "TYPE" => DiagnosticCategory::Type,
        "VALIDATE" => DiagnosticCategory::Validation,
        "CODEGEN" => DiagnosticCategory::Codegen,
        "SHADER" => DiagnosticCategory::Shader,
        "IO" => DiagnosticCategory::Io,
        "CONFIG" => DiagnosticCategory::Config,
        "EFFECT" => DiagnosticCategory::Effect,
        "BORROW" => DiagnosticCategory::Borrow,
        "RUNTIME" => DiagnosticCategory::Runtime,
        "MEM" => DiagnosticCategory::Memory,
        "WORLD" => DiagnosticCategory::World,
        "ACTOR" => DiagnosticCategory::Actor,
        "COMPTIME" => DiagnosticCategory::Comptime,
        "STATE" => DiagnosticCategory::State,
        "TEST" => DiagnosticCategory::Test,
        "INTERNAL" => DiagnosticCategory::Internal,
        _ => DiagnosticCategory::Internal,
    }
}

fn derive_default_suggestion(help: &str) -> Option<&'static str> {
    for line in help.lines() {
        let trimmed = line.trim();
        if let Some(rest) = trimmed.strip_prefix("Fix:") {
            let suggestion = rest.trim();
            if !suggestion.is_empty() {
                return Some(leak_text(suggestion.to_string()));
            }
        }
    }
    None
}

fn optional_leak(value: String) -> Option<&'static str> {
    let trimmed = value.trim();
    if trimmed.is_empty() {
        None
    } else {
        Some(leak_text(trimmed.to_string()))
    }
}

fn leak_text(value: String) -> &'static str {
    Box::leak(value.into_boxed_str())
}
