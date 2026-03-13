pub mod codegen_hlsl;
pub mod codegen_spirv;

pub use codegen_hlsl::generate as generate_hlsl;
pub use codegen_spirv::generate as generate_spirv;
