//! KAIN Browser Compiler
//! 
//! Provides compile functions for in-browser use via WebAssembly.

use wasm_bindgen::prelude::*;
use kain_core::{compile, CompileTarget};

/// Compile KAIN source code to JavaScript
#[wasm_bindgen]
pub fn compile_to_js(source: &str) -> Result<String, JsValue> {
    match compile(source, CompileTarget::Js) {
        Ok(bytes) => Ok(String::from_utf8_lossy(&bytes).to_string()),
        Err(e) => Err(JsValue::from_str(&format!("{}", e))),
    }
}

/// Compile KAIN source code to TypeScript
#[wasm_bindgen]
pub fn compile_to_ts(source: &str) -> Result<String, JsValue> {
    match compile(source, CompileTarget::Ts) {
        Ok(bytes) => Ok(String::from_utf8_lossy(&bytes).to_string()),
        Err(e) => Err(JsValue::from_str(&format!("{}", e))),
    }
}

/// Compile KAIN source code to WebAssembly (returns bytes)
#[wasm_bindgen]
pub fn compile_to_wasm(source: &str) -> Result<Vec<u8>, JsValue> {
    match compile(source, CompileTarget::Wasm) {
        Ok(bytes) => Ok(bytes),
        Err(e) => Err(JsValue::from_str(&format!("{}", e))),
    }
}

/// Compile KAIN source code to Rust
#[wasm_bindgen]
pub fn compile_to_rust(source: &str) -> Result<String, JsValue> {
    match compile(source, CompileTarget::Rust) {
        Ok(bytes) => Ok(String::from_utf8_lossy(&bytes).to_string()),
        Err(e) => Err(JsValue::from_str(&format!("{}", e))),
    }
}

/// Compile KAIN source code to HLSL
#[wasm_bindgen]
pub fn compile_to_hlsl(source: &str) -> Result<String, JsValue> {
    match compile(source, CompileTarget::Hlsl) {
        Ok(bytes) => Ok(String::from_utf8_lossy(&bytes).to_string()),
        Err(e) => Err(JsValue::from_str(&format!("{}", e))),
    }
}

/// Compile KAIN source code to USF (Unreal Engine)
#[wasm_bindgen]
pub fn compile_to_usf(source: &str) -> Result<String, JsValue> {
    match compile(source, CompileTarget::Usf) {
        Ok(bytes) => Ok(String::from_utf8_lossy(&bytes).to_string()),
        Err(e) => Err(JsValue::from_str(&format!("{}", e))),
    }
}

/// Get compiler version
#[wasm_bindgen]
pub fn get_version() -> String {
    kain::VERSION.to_string()
}
