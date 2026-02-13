# KAIN Workspace Refactor: 3-Agent 10x Performance Plan

**Goal:** Reduce compile times by 80% and clean up the "God Module" directory structure.
**Strategy:** Orchestrate 3 AI Agents (Alpha, Beta, Charlie) to parallelize the migration.

## Monorepo Structure
The `kain/` root will become a Cargo Workspace. The existing `src/` folder will be **DELETED** after migration to `crates/`.

```
kain/
├── Cargo.toml (Workspace Root)
└── crates/
    ├── kain-core/           # Frontend + Runtime + Interpreter
    ├── kain-codegen-ue5/    # UE5 Backend 
    ├── kain-codegen-ue5-shaders/ # USF Backend
    ├── kain-codegen-gpu/    # SPIR-V, HLSL
    ├── kain-codegen-web/    # WASM, JS, Hybrid
    ├── kain-codegen-sys/    # LLVM, Rust, C++
    ├── kain-cli/            # binary, LSP, Packager
    └── kain-stdlib/         # Built-in KAIN source libraries
```

---

## 🛡️ Risk Mitigation & Breaking Points
This refactor touches the heart of the compiler. We must verify these points before completion:

1.  **Macro/Include Paths:** Any `include_str!` or `include_bytes!` that uses relative paths from the current file will need updating.
2.  **Stdlib Discovery:** The `kain-cli` searches for the `stdlib/` folder. We must ensure the binary knows its relative position to `../../stdlib/` or similar.
3.  **Cross-Crate Visibility:** Many functions currently `pub(crate)` will need to become `pub` to be visible across the new crate boundaries.
4.  **Shader Paths:** The UE5 backend uses virtual paths for shaders. These must remain consistent regardless of where the generator lives.
5.  **Build Scripts:** Root scripts like `cb.ps1` and `build.bat` must be updated to use `cargo run -p kain-cli`.

---

## The "Surgical" Migration Strategy
Instead of a bulk move, we follow this sequence:
1.  **Stage 1: Shadow Structure:** Create the `crates/` subdirectories and `Cargo.toml` files *without* deleting `src/`.
2.  **Stage 2: Dependency Linkage:** Point the workspace to the NEW crates but keep the old `src` for reference.
3.  **Stage 3: Incremental Move:** Agent Alpha moves core files -> Verify. Agent Beta moves backends -> Verify.
4.  **Stage 4: Path Patching:** Update all `pub` visibilities and path references.
5.  **Stage 5: Final Verification:** Run the full integration test suite.
6.  **Stage 6: The Clean:** Only after Stage 5 is 100% green do we **DELETE** the root `src/`.

---

## Agent Coordination

### 🤖 Agent Alpha: The Architect (Frontend & Core)
**Focus:** `crates/kain-core`
- **Task 1:** Initialize `crates/kain-core`.
- **Task 2: Migration Manifest (EVERY FILE ACCOUNTED FOR):**
    - `src/ast.rs`
    - `src/lexer.rs`
    - `src/parser.rs`
    - `src/types.rs`
    - `src/error.rs`
    - `src/span.rs`
    - `src/diagnostics.rs`
    - `src/effects.rs`
    - `src/monomorphize.rs` (Core AST Transformation)
    - `src/stdlib.rs` (Core Symbol Defs)
    - `src/shader_analysis.rs` (Generic Analysis)
    - `src/comptime.rs` (CTFE Logic)
    - `src/runtime.rs` (Interpreter/VM - required for CTFE)
- **Task 3:** `lib.rs` Extraction: Extract `TypedProgram` and core re-exports.


### 🤖 Agent Beta: The Blacksmith (Codegen Specialist)
**Focus:** All `kain-codegen-*` crates.
- **Task 1: The Big Three**
    - `kain-codegen-ue5`: Move `src/ue5/` and `src/codegen/ue5.rs`.
    - `kain-codegen-ue5-shaders`: Move `src/codegen/usf.rs`.
    - `kain-codegen-gpu`: Move `src/codegen/spirv.rs`, `src/codegen/hlsl.rs`.
- **Task 2: The Web Backends (kain-codegen-web)**
    - Move `src/codegen/wasm.rs`, `src/codegen/js.rs`, `src/codegen/hybrid.rs`.
- **Task 3: The System Backends (kain-codegen-sys)**
    - Move `src/codegen/llvm.rs`, `src/codegen/rust.rs`, `src/codegen/cpp.rs`.
- **Task 4: Integration**
    - Ensure all codegen crates depend on `kain-core`.

### 🤖 Agent Charlie: The Pilot (CLI, LSP & Integration)
**Focus:** `crates/kain-cli` & Workspace Integration
- **Task 1: Bootstrap Workspace:** Convert root `Cargo.toml` to `[workspace]`.
- **Task 2: Preserve the Name:** Ensure `crates/kain-cli/Cargo.toml` defines `[[bin]] name = "kain-pro"`.
- **Task 3: Migration:** Move `main.rs`, `lsp.rs`, `packager.rs`, `editor/`, and `bootstrap/`.
- **Task 4: Existing Crates:** Integrate `crates/kain-browser` into the workspace members.
- **Task 5: The Feature Trick:** Add optional `ue5`, `gpu`, `web`, and `sys` features to `kain-cli`.
- **Task 6: Local Path Patching:** Update internal paths for stdlib discovery and template loading.

---

## 📦 The Dependency Strategy (279+ Crates)
With 279+ direct and transitive dependencies (from `tokio`, `pyo3`, `inkwell`, etc.), we must avoid "Dependency Hell" across the workspace.

1.  **Workspace Inheritance:** We will move common heavy hitters (`tokio`, `serde`, `clap`) into the **Root `Cargo.toml`** under `[workspace.dependencies]`.
2.  **Crate-Level Selection:** Individual crates will only pull what they need using `name = { workspace = true }`.
    - `kain-core`: `chumsky`, `logos`, `ariadne`.
    - `kain-codegen-ue5`: `heck`, `minijinja`.
    - `kain-codegen-sys`: `inkwell`.
    - `kain-cli`: `clap`, `tower-lsp`.
3.  **Local Dev Path Preservation:** Every crate will point to siblings via `{ path = "../kain-core" }` to ensure `cargo build` at the root keeps the graph in sync.
4.  **No Duplicate Compilation:** The workspace sharing ensures that `tokio` or `serde` is only compiled ONCE for the entire project, even if multiple crates use it.

---

## 🚀 Binary Preservation
The `kain-pro` executable name MUST remain consistent. The CLI crate will define:
```toml
[[bin]]
name = "kain-pro"
path = "src/main.rs"
```

---

## The "Wait for Signal" Protocol
1. **Research Phase:** Agents examine all `include_str!` and file path references.
2. **Path Audit:** Agent Charlie audits `kain/Cargo.toml` and root scripts.
3. **Execution Blocked:** **DO NOT START** until User provides the "START REFACTOR" signal.

## Progress Checklist
- [ ] Workspace Root `Cargo.toml` defined
- [ ] `kain-core` migrated and passing `cargo check`
- [ ] `kain-codegen-ue5` migrated and passing `cargo check`
- [ ] `kain-codegen-ue5-shaders` migrated and passing `cargo check`
- [ ] `kain-codegen-gpu` migrated and passing `cargo check`
- [ ] `kain-cli` reassembled with feature flags
- [ ] **ROOT `src/` DELETED**
- [ ] Full plugin build verified
