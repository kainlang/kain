pub mod codegen_spirv;
pub mod codegen_hlsl;

pub use codegen_spirv::generate as generate_spirv;
pub use codegen_hlsl::generate as generate_hlsl;
