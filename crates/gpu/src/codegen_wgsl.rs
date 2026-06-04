//! Direct WGSL generation for KAIN shader bundles.
//!
//! SPIR-V remains the canonical native GPU payload; WGSL is emitted as a
//! derived text artifact for WebGPU/wgpu-style consumers.

use kain_core::error::KainResult;
use kain_core::types::TypedProgram;

pub fn generate(program: &TypedProgram) -> KainResult<String> {
    kain_shader_text::wgsl::generate(program)
}
