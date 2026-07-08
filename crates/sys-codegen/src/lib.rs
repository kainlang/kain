//! KAIN System Code Generators
//!
//! Generates LLVM IR, Rust, C, and C++ code from KAIN source.

pub mod codegen_c;
pub mod codegen_cpp;
pub mod codegen_llvm;
pub mod codegen_rust;

// Re-export for convenience
pub use codegen_c::generate as generate_c;
pub use codegen_cpp::generate as generate_cpp;
pub use codegen_llvm::generate as generate_llvm;
pub use codegen_llvm::generate_with_target as generate_llvm_with_target;
pub use codegen_llvm::generate_llvm_for_target;
pub use codegen_llvm::resolve_llvm_target_for_compile_target;
pub use codegen_llvm::generate_with_debug as generate_with_debug;
pub use codegen_llvm::generate_with_debug_for_target;
pub use codegen_llvm::generate_for_shared_library;
pub use codegen_llvm::generate_with_target_for_shared_library;
pub use codegen_llvm::generate_with_debug_for_shared_library;
pub use codegen_rust::generate as generate_rust;
pub use codegen_rust::generate_gpu_host_wrappers as generate_rust_gpu_host_wrappers;
pub use codegen_rust::{
    collect_gpu_artifacts, collect_gpu_artifacts_json, generate_rust_artifact_bundle,
    RustArtifactBundle, RustArtifactKind, RustGpuArtifactOutput, RustGpuBindingArtifact,
    RustGpuBindingKind, RustGpuInputArtifact, RustGpuShaderArtifact, RustGpuShaderStage,
    RustTextArtifact,
};
