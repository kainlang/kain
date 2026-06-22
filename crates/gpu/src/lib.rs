pub mod codegen_hlsl;
pub mod codegen_ptx;
pub mod codegen_spirv;
pub mod codegen_wgsl;
pub mod ptx_module;

pub use codegen_hlsl::generate as generate_hlsl;
pub use codegen_ptx::{
    generate as generate_ptx, generate_variant_modules as generate_ptx_variant_modules,
    generate_with_options as generate_ptx_with_options, GeneratedPtxModule, PtxCodegenOptions,
    PtxVariantSelection,
};
pub use codegen_spirv::generate as generate_spirv;
pub use codegen_wgsl::generate as generate_wgsl;

use kain_core::shader_artifact::bytes_to_hex;
use kain_core::error::KainResult;
use kain_core::types::TypedShader;

/// Compile a single fragment shader to hex-encoded SPIR-V.
/// Returns the hex string suitable for embedding as an LLVM global constant.
pub fn compile_fragment_to_spirv_hex(shader: &TypedShader) -> KainResult<String> {
    let spirv_bytes = codegen_spirv::generate_fragment(shader)?;
    Ok(bytes_to_hex(&spirv_bytes))
}
