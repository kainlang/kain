# Kain Labs Folder Guide

`labs/` holds focused runtime validation and experiment harnesses.

Current lanes:

- `brainfuck/` for the Kain-native Brainfuck interpreter and Turing-completeness proof
- `kain_native_taste_lab/` for a compact native-ui dev taste test covering world state, dock tabs, viewport props, shader canvases, and packaged desktop materialization
- `llvm_world_dogfood_lab/` for the canonical LLVM dogfood app covering world, patch, converge, orchestrate, actor mailbox traffic, and native UI + viewport rendering
- `llvmzone/` for a five-app LLVM utility lane that compiles separate native executables and stress-tests enums, loops, actors, world/patch/orchestrate, floats, and native UI/viewport surfaces
- `raw_native_world_lab/` for raw-native world and UI bundle validation
- `raw_native_magma_forge_lab/` for magma forge runtime proofing
- `native_ui_viewport_smoke/` for native UI + viewport smoke validation
- `threejs_node_ffi_space_lab/` for the Node FFI + Three.js localhost sculpt-suite proof with a universal viewport and Rust-built WASM brush core

Keep sources, configs, and summary reports. Treat compiled outputs (`.exe`, `.pdb`, `.ilk`, `.obj`, `.o`) plus build caches (`target/`, `.kain-runtime`) as disposable.
