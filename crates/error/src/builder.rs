//! Typed diagnostic builders — one per error category.
//!
//! Each builder starts from the default generic code for its category
//! and provides ergonomic constructors that encourage specific error codes.
//! The builder pattern returns a `DiagnosticReport`.

use crate::code::DiagnosticCode;
use crate::report::{DiagnosticReport, ErrorKind};

// ── Parse ─────────────────────────────────────────────────────────────

pub struct ParseDiagnostic;

impl ParseDiagnostic {
    pub fn generic(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new_default(ErrorKind::Parse, message)
    }

    pub fn expected_token(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Parse, code("KAIN-PARSE-0002"), message)
    }

    pub fn unexpected_token(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Parse, code("KAIN-PARSE-0003"), message)
    }

    pub fn reserved_identifier(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Parse, code("KAIN-PARSE-0004"), message)
    }

    pub fn missing_delimiter(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Parse, code("KAIN-PARSE-0005"), message)
    }

    pub fn invalid_surface_kind(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Parse, code("KAIN-PARSE-0006"), message)
    }

    pub fn expected_contextual(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Parse, code("KAIN-PARSE-0007"), message)
    }

    pub fn unclosed_delimiter(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Parse, code("KAIN-PARSE-0008"), message)
    }

    pub fn mismatched_delimiter(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Parse, code("KAIN-PARSE-0009"), message)
    }
}

// ── Type ──────────────────────────────────────────────────────────────

pub struct TypeDiagnostic;

impl TypeDiagnostic {
    pub fn generic(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new_default(ErrorKind::Type, message)
    }

    pub fn unknown_identifier(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Type, code("KAIN-TYPE-0002"), message)
    }

    pub fn type_mismatch(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Type, code("KAIN-TYPE-0025"), message)
    }

    pub fn duplicate_symbol(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Type, code("KAIN-TYPE-0004"), message)
    }

    pub fn trait_not_satisfied(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Type, code("KAIN-TYPE-0007"), message)
    }

    pub fn trait_method_missing(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Type, code("KAIN-TYPE-0008"), message)
    }

    pub fn field_not_found(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Type, code("KAIN-TYPE-0024"), message)
    }
}

// ── Effect ────────────────────────────────────────────────────────────

pub struct EffectDiagnostic;

impl EffectDiagnostic {
    pub fn generic(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new_default(ErrorKind::Effect, message)
    }

    pub fn pure_side_effect(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Effect, code("KAIN-EFFECT-0004"), message)
    }

    pub fn async_in_sync(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Effect, code("KAIN-EFFECT-0006"), message)
    }

    pub fn gpu_in_host(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Effect, code("KAIN-EFFECT-0008"), message)
    }
}

// ── Borrow ────────────────────────────────────────────────────────────

pub struct BorrowDiagnostic;

impl BorrowDiagnostic {
    pub fn generic(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new_default(ErrorKind::Borrow, message)
    }

    pub fn multiple_mutable(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Borrow, code("KAIN-BORROW-0002"), message)
    }

    pub fn use_after_move(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Borrow, code("KAIN-BORROW-0004"), message)
    }

    pub fn lifetime_mismatch(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Borrow, code("KAIN-BORROW-0008"), message)
    }
}

// ── World ─────────────────────────────────────────────────────────────

pub struct WorldDiagnostic;

impl WorldDiagnostic {
    pub fn generic(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new_default(ErrorKind::World, message)
    }

    pub fn missing_surface(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::World, code("KAIN-WORLD-0001"), message)
    }

    pub fn entanglement_invalid(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::World, code("KAIN-WORLD-0005"), message)
    }

    pub fn teleport_invalid(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::World, code("KAIN-WORLD-0006"), message)
    }
}

// ── Shader ────────────────────────────────────────────────────────────

pub struct ShaderDiagnostic;

impl ShaderDiagnostic {
    pub fn generic(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new_default(ErrorKind::Shader, message)
    }

    pub fn unsupported_call(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Shader, code("KAIN-SHADER-0001"), message)
    }

    pub fn stage_mismatch(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Shader, code("KAIN-SHADER-0002"), message)
    }

    pub fn compilation_failed(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Shader, code("KAIN-SHADER-0010"), message)
    }

    pub fn uniform_binding_error(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Shader, code("KAIN-SHADER-0003"), message)
    }

    pub fn compute_dispatch_dimension(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Shader, code("KAIN-SHADER-0004"), message)
    }

    pub fn resource_not_compatible(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Shader, code("KAIN-SHADER-0005"), message)
    }

    pub fn vertex_input_layout(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Shader, code("KAIN-SHADER-0006"), message)
    }

    pub fn fragment_output_layout(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Shader, code("KAIN-SHADER-0007"), message)
    }

    pub fn collapse_target_invalid(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Shader, code("KAIN-SHADER-0008"), message)
    }

    pub fn fanout_width_exceeded(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Shader, code("KAIN-SHADER-0009"), message)
    }

    pub fn gpu_memory_budget_exceeded(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Shader, code("KAIN-SHADER-0011"), message)
    }

    pub fn shared_memory_bank_conflict(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Shader, code("KAIN-SHADER-0012"), message)
    }
}

// ── Memory ────────────────────────────────────────────────────────────

pub struct MemoryDiagnostic;

impl MemoryDiagnostic {
    pub fn generic(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new_default(ErrorKind::Memory, message)
    }

    pub fn lowering_required(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Memory, code("KAIN-MEM-0001"), message)
    }

    pub fn null_deref(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Memory, code("KAIN-MEM-0006"), message)
    }

    pub fn out_of_bounds(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Memory, code("KAIN-MEM-0007"), message)
    }
}

// ── Codegen ───────────────────────────────────────────────────────────

pub struct CodegenDiagnostic;

impl CodegenDiagnostic {
    pub fn generic(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new_default(ErrorKind::Codegen, message)
    }

    pub fn unknown_variable(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Codegen, code("KAIN-CODEGEN-0002"), message)
    }

    pub fn lowering_failed(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Codegen, code("KAIN-CODEGEN-0003"), message)
    }

    pub fn backend_failed(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Codegen, code("KAIN-CODEGEN-0004"), message)
    }

    pub fn linking_failed(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Codegen, code("KAIN-CODEGEN-0005"), message)
    }

    pub fn unsupported_target(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Codegen, code("KAIN-CODEGEN-0006"), message)
    }

    pub fn capability_missing(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Codegen, code("KAIN-CODEGEN-0007"), message)
    }

    pub fn foreign_abi_mismatch(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Codegen, code("KAIN-CODEGEN-0008"), message)
    }

    pub fn intrinsic_not_found(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Codegen, code("KAIN-CODEGEN-0009"), message)
    }

    pub fn optimization_failed(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Codegen, code("KAIN-CODEGEN-0010"), message)
    }

    pub fn budget_exceeded(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Codegen, code("KAIN-CODEGEN-0011"), message)
    }
}

// ── Runtime ───────────────────────────────────────────────────────────

pub struct RuntimeDiagnostic;

impl RuntimeDiagnostic {
    pub fn generic(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new_default(ErrorKind::Runtime, message)
    }

    pub fn resource_exhausted(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Runtime, code("KAIN-RUNTIME-0004"), message)
    }

    pub fn shader_dispatch_failed(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Runtime, code("KAIN-RUNTIME-0007"), message)
    }

    pub fn timeout_exceeded(message: impl Into<String>) -> DiagnosticReport {
        DiagnosticReport::new(ErrorKind::Runtime, code("KAIN-RUNTIME-0008"), message)
    }
}

// ── Helpers ──────────────────────────────────────────────────────────

fn code(s: &'static str) -> DiagnosticCode {
    DiagnosticCode::new(s)
}
