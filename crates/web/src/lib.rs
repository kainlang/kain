pub mod codegen_hybrid;

use kain_core::error::KainResult;
use kain_core::types::TypedProgram;

pub use codegen_hybrid::{generate as generate_hybrid, HybridOutput, WasmExport};

pub fn generate_js(program: &TypedProgram) -> KainResult<String> {
    kain_script::generate_js(program)
}

pub fn generate_ks(program: &TypedProgram) -> KainResult<String> {
    kain_script::generate_ks(program)
}

pub fn generate_ts(program: &TypedProgram) -> KainResult<String> {
    kain_script::generate_ts(program)
}

pub fn generate_wasm(program: &TypedProgram) -> KainResult<Vec<u8>> {
    kain_wasm::generate_wasm(program)
}
