pub mod codegen_wasm;
pub mod codegen_js;
pub mod codegen_hybrid;

pub use codegen_wasm::generate as generate_wasm;
pub use codegen_js::generate as generate_js;
pub use codegen_hybrid::generate as generate_hybrid;
