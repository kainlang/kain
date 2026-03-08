//! KAIN System Code Generators
//! 
//! Generates LLVM IR, Rust, and C++ code from KAIN source.

pub mod codegen_llvm;
pub mod codegen_rust;
pub mod codegen_cpp;

// Re-export for convenience
pub use codegen_llvm::generate as generate_llvm;
pub use codegen_rust::generate as generate_rust;
pub use codegen_rust::generate_gpu_host_wrappers as generate_rust_gpu_host_wrappers;
pub use codegen_rust::{
    collect_gpu_artifacts,
    collect_gpu_artifacts_json,
    generate_rust_artifact_bundle,
    RustArtifactBundle,
    RustArtifactKind,
    RustGpuArtifactOutput,
    RustGpuBindingArtifact,
    RustGpuBindingKind,
    RustGpuInputArtifact,
    RustGpuShaderArtifact,
    RustGpuShaderStage,
    RustTextArtifact,
};
pub use codegen_cpp::generate as generate_cpp;
