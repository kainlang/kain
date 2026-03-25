# KAIN Tech Stack

## Build System

- **Primary**: Cargo workspace with 34+ crates
- **Language**: Rust (stable toolchain)
- **Workspace resolver**: Version 2

## Core Dependencies

### Compiler Frontend
- `logos` (0.14) - lexer generation
- `chumsky` (0.9) - parser combinators
- `ariadne` (0.4) - error reporting
- `petgraph` (0.6) - graph algorithms for type system
- `winnow` (0.6) - parsing utilities

### Runtime & Async
- `tokio` (1.x) - async runtime with multi-thread, macros, sync, time, io-std, fs
- `flume` (0.11) - MPSC channels
- `once_cell` (1.x) - lazy statics

### Serialization
- `serde` (1.x) with derive - serialization framework
- `serde_json` (1.x) - JSON support
- `toml` (0.8) - TOML config parsing
- `jsonschema` (0.18) - schema validation

### Backend-Specific
- `inkwell` (0.7.1) - LLVM bindings (llvm21-1, target-x86)
- `walrus` (0.21) - WebAssembly manipulation
- `rspirv` (0.12) - SPIR-V generation
- `pyo3` (0.20) - Python embedding (auto-initialize)
- `wgpu` (24.0.5) - GPU abstraction for 3D viewport

### CLI & LSP
- `clap` (4.x) with derive - CLI argument parsing
- `tower-lsp` (0.20) - LSP server framework
- `notify` (6.x) - file system watching

### Utilities
- `reqwest` (0.12.28) - HTTP client (blocking, json)
- `heck` (0.5.0) - case conversion
- `minijinja` (2.15.1) - template engine
- `chrono` (0.4.43) - date/time handling
- `bytemuck` (1.25.0) - safe transmutation

## Common Commands

### Inspection
```bash
# Check toolchain status and supported targets
kain doctor

# View CLI help
kain --help
kain build --help
kain import-crate --help
```

### Building
```bash
# Build Rust workspace
cargo build
cargo build --release

# Compile .kn to various targets
kain build src/main.kn --target wasm
kain build src/main.kn --target rust
kain build src/main.kn --target ts
kain build src/shader.kn --target spirv
kain build --ue5

# Build native UI application
kain build native-ui src/main.kn
kain build native-ui src/main.kn --bundle-only
kain build native-ui src/main.kn --release

# Generate GPU artifacts (SPIR-V + Rust wrapper + reflection JSON)
kain gpu-artifacts src/shader.kn --output dist
```

### Testing
```bash
# Run Rust tests
cargo test
cargo test --package kain-core
cargo test --workspace

# Run .kn runtime tests
kain run smoketest/python/pygame_poster/smoke.kn
kain run smoketest/node/orbit_portal/smoke.kn
```

### Source Import
```bash
# Import foreign source into .kn
kain import-c src/main.c --output main.kn
kain import-rust src/lib.rs --output lib.kn
kain import-ts src/app.ts --output app.kn
kain import-asm firmware.asm --format gameboy --out game.kn

# Generate Rust crate FFI bindings
kain import-crate <crate_name> --crate-path ./path --mode both -o ./out
```

### Mixed-Language Orchestration
```bash
# Initialize and build omni manifest
kain omni init
kain omni build
```

### Selfhost Workflows
```bash
kain selfhost phase1
kain selfhost phase2
```

### UE5 Integration
```bash
# Inject .kn into existing UE plugin
kain inject src/new_actor.kn --ue5
```

## Project Structure Conventions

- Crates follow `kain-*` naming for core pipeline components
- `ue5-*` prefix for Unreal Engine integration crates
- Backend crates: `web`, `gpu`, `kain-sys-codegen`
- Vendored dependencies in `crates/unreal/` for asset manipulation
- Smoketests organized by runtime bridge type (single vs mixed)
- Generated artifacts go to `generated/` (gitignored)
- Toolchain binaries in `toolchain/` (LLVM, Clang)
