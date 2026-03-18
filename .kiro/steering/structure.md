# KAIN Project Structure

## Top-Level Organization

```
M:\Code\Kain
├── crates/              # Rust workspace crates (34+ crates)
├── smoketest/           # Runtime bridge and mixed-language proof suites
├── labs/                # Focused validation labs (e.g., native viewport)
├── stdlib/              # Standard library data and target/runtime support
├── toolchain/           # LLVM and related toolchain binaries
├── generated/           # Generated artifacts (gitignored)
├── bootstrap/           # Bootstrap and self-hosting experiments
├── Research/            # Assembly research (Furby, Game Boy, Z80)
├── 3D/                  # 3D assets and resources
└── Cargo.toml           # Workspace manifest
```

## Crate Architecture

### Five-Layer Mental Model

1. **Frontend** (`kain-core`) - lexer, parser, comptime, typechecking, runtime/test execution
2. **Importers** (`kain-import`, `kain-asm`) - convert C, Rust, TypeScript, assembly into Kain
3. **Codegen backends** (`web`, `gpu`, `kain-sys-codegen`, `ue5*`) - multi-target compilation
4. **Host-backed bridges** (`kain-c-ffi`, `kain-crate-ffi`, `kain-python`, `kain-node`, `kain-interop`) - runtime interop
5. **Embeddable stack** (`kain-driver`, `kain-host`, `kain-sdk`, `kain-ui`, `kain-ui-native`, `kain-3D`) - embedding and application materialization

### Core Pipeline Crates

- `kain-core` - Language frontend (parse, typecheck, comptime, interpreter)
- `kain-driver` - Embeddable compiler orchestration layer
- `kain-interop` - Neutral shared buffer/image contracts across runtimes
- `cli` - CLI binary (thin wrapper over kain-driver)

### Runtime Bridge Crates

- `kain-c-ffi` - C ABI host FFI (live native library loading)
- `kain-crate-ffi` - Rust crate FFI generation and bridge loading
- `kain-python` - Embedded Python bridge + DCC payload wrappers
- `kain-node` - JavaScript/Node.js runtime bridge
- `kain-host` - Native Rust host runtime for embedding Kain
- `kain-host-derive` - Derive macros for host boundary ergonomics
- `kain-reflect` - Reflection/type schema layer

### Backend Crates

- `web` - Web targets (WASM, JS, TS, KS, hybrid)
- `gpu` - GPU targets (SPIR-V, HLSL, USF) + artifact bundling
- `kain-sys-codegen` - System backends (LLVM, Rust, C++)

### UE5 Integration Crates

- `ue5` - Runtime plugin generation
- `ue5-editor` - Editor generation
- `ue5-shaders` - Shader generation (USF)
- `ue5-materials` - Material graph generation
- `ue5-graphs` - Graph editor/runtime support
- `ue5-blueprints` - Blueprint-oriented support
- `ue5-gas` - Gameplay Ability System support
- `ue5-config` - Config/build helpers
- `ue5-asset-utils` - Asset utility support
- `unreal/*` - Vendored Unreal asset read/write crates (unrealmodding)

### Application/UI Crates

- `kain-ui` - Semantic UI compiler/runtime model
- `kain-ui-native` - Native desktop UI runtime (eframe/egui)
- `kain-3D` - 3D authoring/scene/renderer/interaction/runtime (WGPU)
- `kain-sdk` - High-level embedder facade

### Orchestration Crates

- `kain-omni` - Mixed-language manifest orchestration
- `kain-selfhost` - Self-hosting bootstrap workflows
- `kain-build` - Engine/module build helpers

### Import Crates

- `kain-import` - C, Rust, TypeScript source importers
- `kain-asm` - Assembly importers (Game Boy LR35902, 6502/Furby, Z80)

## Smoketest Organization

Smoketests serve as capability proofs and are organized by runtime bridge type:

### Single-Runtime Suites
- `smoketest/c_ffi/` - C ABI interop (beacon_math, cgltf_scene_probe, miniaudio_tone_lab, shared_image_contract)
- `smoketest/cargo/` - Rust crate FFI (local_crate_synth)
- `smoketest/python/` - Python bridge (pygame_poster, trimesh_glb_forge, numpy_supernova)
- `smoketest/node/` - Node/JS bridge (orbit_portal, typescript_signal_forge)
- `smoketest/UI/` - Semantic UI (theme_authoring_shell, dock_layout_workbench, surface_modes_gallery)
- `smoketest/3D/` - 3D viewport and rendering

### Mixed-Runtime Suites
- `smoketest/py_node/` - Python + Node
- `smoketest/cargo_node/` - Cargo FFI + Node (signal_workbench)
- `smoketest/py_cargo/` - Python + Cargo (triple_stack_canvas)
- `smoketest/py_cargo_node/` - Python + Cargo + Node (trinity_web_lattice)
- `smoketest/py_cargo_node_c/` - Python + Cargo + Node + C (quad_prism_halo)

## Key Directories

- `stdlib/` - Standard library implementations per target/runtime
- `toolchain/` - LLVM, Clang, and related binaries (gitignored bin/)
- `generated/` - Compiler outputs, test artifacts (gitignored)
- `labs/` - Focused validation (e.g., `native_ui_viewport_smoke`)
- `bootstrap/` - Self-hosting experiments and bootstrap workflows
- `Research/` - Assembly dialect research and canonical examples

## File Naming Conventions

- Crate names: `kain-*` for core, `ue5-*` for Unreal, backend names (`web`, `gpu`)
- Smoketest entry: `smoke.kn` in each smoketest directory
- Config files: `KAIN.toml` (project), `KAIN.omni.toml` (omni manifest)
- Generated bindings: typically in `.kain/cache/` subdirectories (gitignored)

## Important Constraints

- **Host-backed bridges** (C FFI, Rust crate FFI, Python, Node) target `run`/`test` lanes, not arbitrary offline codegen
- **Source import** (`import-c`, `import-rust`, `import-ts`, `import-asm`) is distinct from runtime bridges
- **Smoketests are truth** - they validate current capability and serve as integration tests
- **Generated artifacts** should never be committed (use `generated/` or `.kain/cache/`)
