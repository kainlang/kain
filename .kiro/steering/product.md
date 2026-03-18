# KAIN Product Overview

KAIN is a compiled multi-target language toolchain with an embeddable runtime/host stack. A single `.kn` source file can compile to multiple targets (web, native, GPU, UE5) and orchestrate host-backed runtime bridges for C, Rust crates, Python, Node, and mixed-runtime pipelines.

## Core Capabilities

- **Multi-target compilation**: Compile `.kn` to web (WASM, JS, TS), system (LLVM, Rust, C++), GPU (SPIR-V, HLSL, USF), and UE5 (runtime, editor, shaders, materials, blueprints)
- **Source import**: Transform C, Rust, TypeScript, and assembly into Kain program form
- **Host-backed runtime bridges**: Live interop with C ABI, Rust crates, Python, and Node.js at runtime
- **Embeddable SDK**: Rust-native embedding via `kain-driver`, `kain-host`, and `kain-sdk` crates
- **Native UI/3D applications**: Semantic UI compiler and native desktop runtime with 3D viewport support
- **Mixed-language orchestration**: Data-driven build manifests for multi-language projects via `kain omni`

## Key Differentiators

- Single source, multiple targets with consistent semantics
- Runtime bridges enable live foreign function calls without offline codegen
- Neutral shared interop contracts for payload exchange across runtimes
- Native tool/application materialization, not just text compilation
- Deep UE5 integration including runtime, editor, shaders, materials, and graph generation
