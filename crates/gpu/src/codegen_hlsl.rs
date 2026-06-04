//! Derived HLSL generation for KAIN shader bundles.
//!
//! SPIR-V remains the canonical native GPU payload; HLSL is emitted as a
//! backend materialization through the shared text-shader lowering crate.

use kain_core::error::KainResult;
use kain_core::types::TypedProgram;

pub fn generate(program: &TypedProgram) -> KainResult<String> {
    kain_shader_text::hlsl::generate(program)
}
