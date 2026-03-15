# KAIN Core Runtime Track Implementation Matrix 2026

> **Date:** March 14, 2026  
> **Purpose:** Define the runtime-deep implementation track that sits under the broader execution-platform blueprint.  
> **Companion Docs:** `KAIN_2026_EXECUTION_PLATFORM_BLUEPRINT.md`, `KAIN_CORE_RUNTIME_ROADMAP_2026.md`

---

## 1. Positioning

This document is **not** the primary execution-platform plan.

The primary north star remains `KAIN_2026_EXECUTION_PLATFORM_BLUEPRINT.md`.

This matrix is a focused execution track for the runtime/compiler contract side of that bigger platform plan. Its job is to answer:

- how `kain-core` becomes the semantic contract center
- how `runtime/native` becomes a durable execution substrate
- how the existing active platform spine converges instead of drifting

That means this document must stay grounded in the lanes that are already real in the repo:

- `kain-driver` bundle and runtime boundary
- `kain-ui` semantic UI runtime model
- `kain-ui-native` native host convergence
- `kain-sys-codegen` backend/runtime ABI parity
- `runtime/native` core, viewport, input, app-host, asset, and UI modules

---

## 2. Current Active Platform Spine

These are not hypothetical tracks. They are already visible in the codebase and must be first-class in planning.

### 2.1 Compiler and Driver Spine

- [lib.rs](/M:/Code/Kain/crates/kain-core/src/lib.rs#L1)
- [lib.rs](/M:/Code/Kain/crates/kain-driver/src/lib.rs#L1)
- [native_app.rs](/M:/Code/Kain/crates/kain-driver/src/native_app.rs#L1)
- [lib.rs](/M:/Code/Kain/crates/kain-sys-codegen/src/lib.rs#L1)

### 2.2 Semantic UI and Native UI Spine

- [lib.rs](/M:/Code/Kain/crates/kain-ui/src/lib.rs#L1)
- [lib.rs](/M:/Code/Kain/crates/kain-ui-native/src/lib.rs#L1)
- [kain_ui_compiled_bundle.c](/M:/Code/Kain/runtime/native/src/ui/kain_ui_compiled_bundle.c#L1)
- [kain_ui_compiled_overlay.c](/M:/Code/Kain/runtime/native/src/ui/kain_ui_compiled_overlay.c#L1)
- [kain_ui_overlay.c](/M:/Code/Kain/runtime/native/src/ui/kain_ui_overlay.c#L1)

### 2.3 Native Runtime Services Spine

- [kain_runtime_core.c](/M:/Code/Kain/runtime/native/src/core/kain_runtime_core.c#L1)
- [kain_asset_gltf.c](/M:/Code/Kain/runtime/native/src/asset/kain_asset_gltf.c#L1)
- [kain_gl_win32_host.c](/M:/Code/Kain/runtime/native/src/gfx/opengl/kain_gl_win32_host.c#L1)
- [kain_runtime_viewport_win32.c](/M:/Code/Kain/runtime/native/src/platform/win32/kain_runtime_viewport_win32.c#L1)
- [kain_win32_app_host.c](/M:/Code/Kain/runtime/native/src/platform/win32/kain_win32_app_host.c#L1)
- [kain_win32_input_host.c](/M:/Code/Kain/runtime/native/src/platform/win32/kain_win32_input_host.c#L1)

### 2.4 Reflection and Host Spine

- [lib.rs](/M:/Code/Kain/crates/kain-host/src/lib.rs#L1)
- [lib.rs](/M:/Code/Kain/crates/kain-reflect/src/lib.rs#L1)

---

## 3. Planning Rules

### 3.1 Runtime Track, Not Runtime Tunnel Vision

The runtime contract work must not crowd out native UI convergence, viewport hosting, input, asset I/O, or driver/bundle work. Those are already platform-critical.

### 3.2 Contract Depth Must Follow Real Lanes

If a capability is already real in a runtime lane, the contract matrix must account for it. That includes:

- native UI bundle/runtime shape
- viewport embedding
- input delivery
- asset/runtime services
- app-host lifecycle
- backend ABI parity

### 3.3 No Speculative Scoring as Source of Truth

This document should describe:

- priorities
- dependencies
- risks
- likely code touchpoints
- validation requirements

It should not use speculative probability or multiplier scoring as a planning primitive.

### 3.4 ABI Parity Is First-Class

Backend/runtime math and helper parity is part of the core contract problem, not a side note. Recent low-level fixes like the raw-lane `floor` / `ceil` / `round` handling are exactly the kind of compiler/runtime mismatch this matrix must explicitly capture.

---

## 4. Execution Tracks

This matrix is organized into six connected tracks.

1. Compiler and contract center
2. Driver and bundle boundary
3. Native UI convergence
4. Native runtime services
5. Backend/runtime ABI parity
6. Deep runtime features

The first five are active platform work. The sixth is where actor concurrency, hot reload, metaprogramming, and metamorphization mature.

---

## 5. Track A: Compiler and Contract Center

### Goal

Make `kain-core` the authoritative source for semantic contracts without forcing it to become the full runtime implementation.

### Primary crates

- `crates/kain-core`
- `crates/kain-reflect`

### Likely files

- [lib.rs](/M:/Code/Kain/crates/kain-core/src/lib.rs#L1)
- [ast.rs](/M:/Code/Kain/crates/kain-core/src/ast.rs#L1)
- [types.rs](/M:/Code/Kain/crates/kain-core/src/types.rs#L1)
- [effects.rs](/M:/Code/Kain/crates/kain-core/src/effects.rs#L1)
- [language_features.rs](/M:/Code/Kain/crates/kain-core/src/language_features.rs#L1)
- [monomorphize.rs](/M:/Code/Kain/crates/kain-core/src/monomorphize.rs#L1)
- [comptime.rs](/M:/Code/Kain/crates/kain-core/src/comptime.rs#L1)

### New modules likely needed

- `runtime_contract.rs`
- `runtime_capabilities.rs`
- `symbol_identity.rs`
- `reflection_emit.rs`
- `compatibility.rs`

### Core tasks

- define stable item identity for runtime-significant symbols
- define runtime capability requirements emitted by the compiler
- define contract schemas for actors, messages, services, UI bundles, and host imports
- emit reflection metadata as a standard compiler artifact
- define compatibility metadata for future reload and migration work

### Validation

- golden tests for runtime contract emission
- item identity stability tests
- reflection bundle golden tests
- capability compatibility tests

### Dependency notes

- this is a prerequisite for deep runtime features
- it must also feed the driver, native UI, and sys-codegen lanes

---

## 6. Track B: Driver and Bundle Boundary

### Goal

Make `kain-driver` the operational boundary between compiler outputs and runtime/app materialization.

### Primary crates

- `crates/kain-driver`
- `crates/kain-core`
- `crates/kain-sys-codegen`

### Likely files

- [lib.rs](/M:/Code/Kain/crates/kain-driver/src/lib.rs#L1)
- [native_app.rs](/M:/Code/Kain/crates/kain-driver/src/native_app.rs#L1)
- [lib.rs](/M:/Code/Kain/crates/kain-sys-codegen/src/lib.rs#L1)

### Core tasks

- formalize the bundle/runtime boundary in the driver layer
- carry runtime contract bundles through driver output, not just codegen text
- materialize native runtime dependencies explicitly
- let driver outputs expose reflection, UI bundle, GPU, and runtime metadata together
- make native app bundling use the same contract shape that runtime/native expects

### Validation

- native app bundle smoke tests
- artifact manifest tests
- driver bundle round-trip tests
- bundle dependency resolution tests

### Dependency notes

- depends on Track A contracts
- feeds native UI convergence and runtime service materialization

---

## 7. Track C: Native UI Convergence

### Goal

Converge `kain-ui`, `kain-ui-native`, and `runtime/native` UI modules into one runtime-backed native UI lane.

### Primary crates and modules

- `crates/kain-ui`
- `crates/kain-ui-native`
- `runtime/native/src/ui`
- `runtime/native/src/platform/win32`

### Likely files

- [lib.rs](/M:/Code/Kain/crates/kain-ui/src/lib.rs#L1)
- [lib.rs](/M:/Code/Kain/crates/kain-ui-native/src/lib.rs#L1)
- [kain_ui_compiled_bundle.c](/M:/Code/Kain/runtime/native/src/ui/kain_ui_compiled_bundle.c#L1)
- [kain_ui_compiled_overlay.c](/M:/Code/Kain/runtime/native/src/ui/kain_ui_compiled_overlay.c#L1)
- [kain_ui_overlay.c](/M:/Code/Kain/runtime/native/src/ui/kain_ui_overlay.c#L1)
- [kain_runtime_viewport_win32.c](/M:/Code/Kain/runtime/native/src/platform/win32/kain_runtime_viewport_win32.c#L1)
- [kain_win32_app_host.c](/M:/Code/Kain/runtime/native/src/platform/win32/kain_win32_app_host.c#L1)

### Core tasks

- define one UI runtime bundle contract that both Rust-native and raw-native lanes can consume
- align semantic widget/runtime metadata with compiled UI bundle loading
- formalize viewport embedding, overlays, panel regions, and event routing
- make native UI host lifecycle and runtime bundle lifecycle explicit
- unify runtime validation for semantic UI bundle shape across lanes

### Validation

- UI runtime bundle validation tests
- panel/viewport embedding tests
- runtime bundle load/render smoke tests
- parity tests between `kain-ui-native` and raw-native UI bundle consumption

### Dependency notes

- depends on Track A contract work
- tightly coupled with Track D native runtime services

### Why this track matters now

This is one of the most real platform seams in the repo today. It must be centered, not treated like a later refinement.

---

## 8. Track D: Native Runtime Services

### Goal

Strengthen `runtime/native` as the execution substrate for app hosting, input, graphics, assets, and future deep runtime features.

### Primary modules

- `runtime/native/src/core`
- `runtime/native/src/asset`
- `runtime/native/src/gfx`
- `runtime/native/src/platform/win32`

### Likely files

- [kain_runtime_core.c](/M:/Code/Kain/runtime/native/src/core/kain_runtime_core.c#L1)
- [kain_asset_gltf.c](/M:/Code/Kain/runtime/native/src/asset/kain_asset_gltf.c#L1)
- [kain_gl_win32_host.c](/M:/Code/Kain/runtime/native/src/gfx/opengl/kain_gl_win32_host.c#L1)
- [kain_runtime_viewport_win32.c](/M:/Code/Kain/runtime/native/src/platform/win32/kain_runtime_viewport_win32.c#L1)
- [kain_runtime_sculpt_win32.c](/M:/Code/Kain/runtime/native/src/platform/win32/kain_runtime_sculpt_win32.c#L1)
- [kain_runtime_win32_shared.c](/M:/Code/Kain/runtime/native/src/platform/win32/kain_runtime_win32_shared.c#L1)
- [kain_win32_app_host.c](/M:/Code/Kain/runtime/native/src/platform/win32/kain_win32_app_host.c#L1)
- [kain_win32_input_host.c](/M:/Code/Kain/runtime/native/src/platform/win32/kain_win32_input_host.c#L1)

### Core tasks

- harden memory and lifetime rules in the core runtime
- formalize runtime services for input, app host lifecycle, viewport surfaces, and asset I/O
- expose graphics and viewport services as contract-visible runtime capabilities
- define native runtime diagnostics and service versioning
- make platform modules align around one service table instead of ad hoc helpers

### Validation

- native runtime smoke tests
- input delivery tests
- viewport host tests
- asset loading tests
- app host lifecycle tests

### Dependency notes

- this is the main execution substrate track
- deep runtime features should be layered onto this, not designed in isolation from it

---

## 9. Track E: Backend and Runtime ABI Parity

### Goal

Treat backend/runtime helper parity as a first-class execution concern.

### Primary crates and modules

- `crates/kain-sys-codegen`
- `crates/kain-driver`
- `crates/kain-core`
- `runtime/native`

### Likely files

- [lib.rs](/M:/Code/Kain/crates/kain-sys-codegen/src/lib.rs#L1)
- backend modules under `crates/kain-sys-codegen/src/codegen_*`
- [lib.rs](/M:/Code/Kain/crates/kain-driver/src/lib.rs#L1)
- [kain_runtime_core.c](/M:/Code/Kain/runtime/native/src/core/kain_runtime_core.c#L1)

### Core tasks

- define canonical mappings for runtime math helpers and low-level service calls
- make helper parity explicit between emitted code and native runtime exports
- add contract tests for builtin math/service parity
- keep raw-lane runtime helper behavior and compiler lowering in lockstep
- extend parity checks beyond math into allocation, UI bundle loading, viewport/runtime calls, and future actor/runtime services

### Validation

- ABI parity tests
- helper mapping golden tests
- cross-backend conformance tests for runtime-visible builtins
- raw-lane regression tests for emitted helper names and signatures

### Dependency notes

- should proceed in parallel with Tracks A and D
- directly reduces future compiler/runtime contract drift

---

## 10. Track F: Deep Runtime Features

This is where the original runtime-heavy document remains useful, but it should be treated as a deep track under the broader platform work above.

### 10.1 Actor Concurrency

#### Goal

Turn actor syntax into a runtime-backed feature with typed messages, mailboxes, and supervision.

#### Primary crates

- `crates/kain-core`
- `runtime/native`

#### Likely files

- [ast.rs](/M:/Code/Kain/crates/kain-core/src/ast.rs#L1)
- [types.rs](/M:/Code/Kain/crates/kain-core/src/types.rs#L1)
- [runtime.rs](/M:/Code/Kain/crates/kain-core/src/runtime.rs#L1)
- [kain_runtime_core.c](/M:/Code/Kain/runtime/native/src/core/kain_runtime_core.c#L1)

#### Core tasks

- add typed actor/message IR
- define mailbox/runtime contracts
- add supervision and monitor metadata
- implement actor runtime services on top of the hardened runtime substrate

### 10.2 Hot Reload Compatibility

#### Goal

Support safe live updates with compatibility analysis and migration hooks.

#### Primary crates

- `crates/kain-core`
- `runtime/native`
- `crates/kain-host`

#### Core tasks

- emit reload manifests from compiler contracts
- add version/install APIs in runtime and host layers
- define compatibility classes for symbols, layouts, messages, and UI bundles
- add migration hook contracts

### 10.3 Metaprogramming

#### Goal

Expand comptime and macro systems into deterministic staged metaprogramming.

#### Primary crate

- `crates/kain-core`

#### Core tasks

- formalize staged execution boundaries
- add macro hygiene and traceability
- make typed transform hooks explicit

### 10.4 Metamorphization

#### Goal

Add a pass family for context-aware typed reshaping after monomorphization.

#### Primary crate

- `crates/kain-core`

#### Core tasks

- distinguish monomorphization from runtime/context specialization
- add pass infrastructure for capability specialization, runtime partitioning, and patch partitioning

---

## 11. Recommended Order of Work

This is the practical sequencing that fits the current codebase better.

### Stage 1: Contract and Boundary Work

- Track A: compiler and contract center
- Track B: driver and bundle boundary
- Track E: backend/runtime ABI parity

### Stage 2: Active Platform Convergence

- Track C: native UI convergence
- Track D: native runtime services

### Stage 3: Deep Runtime Growth

- Track F.1: actor concurrency
- Track F.2: hot reload compatibility
- Track F.3: metaprogramming
- Track F.4: metamorphization

This sequence better matches the repo because it starts from active platform seams instead of jumping straight to runtime theory.

---

## 12. Immediate Checklist

If we wanted the next concrete implementation sprint to align with this matrix, the highest-value immediate checklist would be:

1. Add compiler-owned runtime contract scaffolding in `kain-core`.
2. Thread that contract through `kain-driver` bundle output.
3. Add explicit ABI parity tests between sys codegen and runtime/native helpers.
4. Define a shared UI runtime bundle contract path across `kain-ui`, `kain-ui-native`, and raw-native UI modules.
5. Harden `runtime/native` service boundaries around app host, input, viewport, and asset services.

Only after those are underway should actor concurrency and reload compatibility become the main planning center.

---

## 13. Final Position

The runtime matrix is worth keeping, but only in the right place:

- as a deep runtime track
- under the broader execution-platform blueprint
- tied to the real platform seams already active in the repo

If we do that, the document becomes useful planning infrastructure.

If we do not, it risks overfitting planning around future runtime theory while underrepresenting the platform work already underway.

