# Design Document: KAIN Native Runtime Completion

## Overview

This design turns `runtime/native` from a useful raw-native substrate into a complete runtime lane for compiled Kain programs. The work is intentionally staged around the repo's current active platform spine instead of pretending the runtime starts from zero.

The current native lane already has real infrastructure:

- core allocation, RC, arrays, maps, strings, file I/O, sockets, thread spawn, and FIFO queue primitives in `runtime/native/src/core/kain_runtime_core.c`
- runtime contract validation in `runtime/native/src/core/kain_runtime_contract.c`
- realtime bundle ingestion in `runtime/native/src/core/kain_runtime_realtime.c`
- glTF asset ingestion in `runtime/native/src/asset/kain_asset_gltf.c`
- Win32 app host and input capture in `runtime/native/src/platform/win32`
- compiled UI bundle loading and overlay rendering in `runtime/native/src/ui`
- compiler-side runtime contract emission in `crates/kain-core/src/runtime_contract.rs`
- driver-side native app bundle materialization in `crates/kain-driver/src/native_app.rs`

The design goal is not to discard that work. The goal is to harden it, make it canonical, and layer the deeper runtime features on top of it in a way that matches Kain's broader language/runtime story.

## Design Principles

### 1. Active Platform Spine First

The runtime plan must respect the seams already present in the repo:

- `kain-core` owns semantics, contracts, lowering rules, and metadata emission
- `kain-driver` owns bundle materialization and operational app packaging
- `runtime/native` owns portable low-level execution machinery and native host services
- `kain-ui` and `kain-ui-native` remain part of the UI/runtime convergence lane
- `kain-reflect` and `kain-host` remain the natural homes for schema and host-bound reflection concerns

This means the native runtime completion work cannot be "just write more C". It must also tighten compiler-emitted contracts, driver packaging, and backend/runtime parity.

### 2. Canonical Contract Before Clever Features

Actor runtime, async, hot reload, and compute support will keep drifting unless the repo first has:

- one canonical runtime ABI
- one explicit runtime service table model
- one runtime/reflection artifact family
- explicit versioning and compatibility rules
- conformance tests proving codegen/runtime agreement

### 3. Data-Driven Runtime Surface

Capability declarations, service registries, compatibility classes, schema versions, and backend support tables must be data-driven or table-driven. The runtime should not grow through scattered string literals and isolated conditionals.

### 4. Hardened Failure Model

The native lane currently uses a mix of null returns, prints, and narrow startup checks. The finished runtime must instead expose:

- subsystem-tagged diagnostics
- stable error codes
- startup validation results
- capability downgrade reporting
- version mismatch reporting
- actor/task crash information

### 5. Preserve Existing Wins

Current working flows must remain functional while the deeper runtime lands:

- runtime contract loading
- realtime bundle loading
- Win32 viewport/sculpt host startup
- compiled UI bundle loading
- glTF support

The completion work should extend these seams, not bulldoze them.

## Current State

### Native Runtime Layout

Current sources are declared by `runtime/native_runtime.toml` and aggregated by `runtime/kain_runtime.c`:

- `runtime/native/src/core/kain_runtime_core.c`
- `runtime/native/src/core/kain_runtime_contract.c`
- `runtime/native/src/core/kain_runtime_realtime.c`
- `runtime/native/src/asset/kain_asset_gltf.c`
- `runtime/native/src/gfx/opengl/kain_gl_win32_host.c`
- `runtime/native/src/platform/win32/kain_win32_app_host.c`
- `runtime/native/src/platform/win32/kain_win32_input_host.c`
- `runtime/native/src/platform/win32/kain_runtime_win32_shared.c`
- `runtime/native/src/platform/win32/kain_runtime_viewport_win32.c`
- `runtime/native/src/platform/win32/kain_runtime_sculpt_win32.c`
- `runtime/native/src/ui/kain_ui_compiled_bundle.c`
- `runtime/native/src/ui/kain_ui_compiled_overlay.c`
- `runtime/native/src/ui/kain_ui_overlay.c`

### Compiler and Driver State

Relevant current compiler/driver sources:

- `crates/kain-core/src/runtime_contract.rs`
- `crates/kain-core/src/comptime.rs`
- `crates/kain-core/src/low_level_memory.rs`
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
- `crates/kain-sys-codegen/src/codegen_cpp/mod.rs`
- `crates/kain-driver/src/native_app.rs`
- `crates/cli/src/main.rs`

### Known Gaps That Drive This Design

- actor spawn on the LLVM/native lane still appears to route through `default_actor_run` instead of a real actor bootstrap
- reflection payloads are not emitted yet even though runtime contract scaffolding exists
- low-level memory is much more mature on the compiler/ABI side than on backend/runtime helper parity
- runtime services are still shallow and narrow compared to the advertised language/runtime feature surface
- the current native lane is strongly Win32-centric

## Architecture

## 1. Runtime Layering

The completed native runtime is organized into nine layers.

### Layer A: ABI and Base Runtime

Files:

- `runtime/native/include/kain_runtime_base.h`
- new headers for diagnostics, actor ABI, async ABI, reflection ABI, service registry, compatibility/versioning
- `runtime/native/src/core/kain_runtime_core.c`

Responsibilities:

- canonical runtime ABI version
- service table declarations
- base handle/value/layout declarations
- allocation/lifetime helpers
- diagnostic record types
- runtime version metadata

### Layer B: Runtime Services and Capability Registry

Files:

- `runtime/native/src/core/kain_runtime_contract.c`
- new service registry and capability source files under `runtime/native/src/core`

Responsibilities:

- service registry and lookup
- required vs optional capability resolution
- startup validation
- downgrade reporting
- compatibility/version validation

### Layer C: Reflection and Metadata Runtime

Files:

- `crates/kain-core/src/runtime_contract.rs`
- new or expanded reflection emission modules in `crates/kain-core`
- `crates/kain-driver/src/native_app.rs`
- new native reflection consumer files under `runtime/native/src/core`

Responsibilities:

- emit runtime contract + reflection payloads
- package contract/reflection/UI/realtime bundles together
- load schema/type/item metadata on the native lane
- expose reflected lookup APIs

### Layer D: Actor Runtime

Files:

- `runtime/native/src/core/kain_runtime_core.c`
- new native actor runtime files under `runtime/native/src/core`
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
- potentially `crates/kain-sys-codegen/src/codegen_cpp/mod.rs` for capability failure clarity or future parity

Responsibilities:

- actor bootstrap ABI
- actor process state
- mailbox storage and policies
- scheduler integration
- monitors/links/supervision/registry
- crash containment and restart semantics

### Layer E: Async and Timer Runtime

Files:

- new native async/timer files under `runtime/native/src/core`
- new headers under `runtime/native/include`
- backend/runtime contract touches in `kain-core` and `kain-sys-codegen`

Responsibilities:

- task/future representation
- wake/poll mechanics
- cancellation
- timers
- scheduler integration
- diagnostics and ownership rules

### Layer F: UI and Component Runtime

Files:

- `runtime/native/src/ui/*`
- `runtime/native/src/platform/win32/*`
- `crates/kain-ui`
- `crates/kain-ui-native`
- driver/runtime contract emitters

Responsibilities:

- bundle validation
- component lifecycle/state invalidation
- focus/input routing
- event propagation
- parity with Rust-native bundle consumption

### Layer G: Graphics, Materials, and Compute Runtime

Files:

- `runtime/native/src/gfx/*`
- `runtime/native/src/platform/win32/*`
- driver/runtime artifact emitters
- relevant `kain-core` runtime contract additions

Responsibilities:

- shader artifact loading
- reflection-driven binding
- material runtime representation
- compute pipeline/dispatch contracts
- backend abstraction or explicit backend service contracts

### Layer H: Hot Reload and Compatibility

Files:

- new compatibility/versioning modules in `runtime/native/src/core`
- `crates/kain-core`
- `crates/kain-driver`

Responsibilities:

- runtime ABI versioning
- bundle compatibility classes
- migration metadata
- install/update/uninstall lifecycle APIs
- state transfer boundaries

### Layer I: Platform Adapters

Files:

- `runtime/native/src/platform/win32/*`
- future `runtime/native/src/platform/linux/*`
- future `runtime/native/src/platform/macos/*`

Responsibilities:

- platform-specific app host/input/timing/window/graphics plumbing
- explicit service implementation boundaries
- capability advertisement per platform

## 2. Canonical Service Table Model

The finished runtime should expose a canonical service table instead of requiring backends to bind random helper names.

Each service family gets:

- ABI version
- service key
- provider lane
- required/optional status
- exported function table
- diagnostics behavior on absence or incompatibility

Representative service families:

- base memory/lifetime
- diagnostics/logging
- runtime contract/reflection
- actor runtime
- async/timers
- filesystem
- networking
- app host
- input
- viewport/graphics
- UI runtime
- shader/material/compute
- hot reload/version management
- host/plugin bridge

This can still be implemented in C, but the declarations must be centralized and versioned.

## 3. Runtime Diagnostics Model

Every subsystem should write into one structured diagnostics model:

- `subsystem`: `contract`, `reflection`, `actor`, `async`, `ui`, `gfx`, `platform`, `host_bridge`
- `code`: stable machine-readable code such as `KAIN-RT-ACTOR-0004`
- `severity`: info, warning, error, fatal
- `message`: human-readable summary
- `detail`: optional extended detail
- `source_path`: bundle or runtime source path if relevant
- `runtime_version`
- `abi_version`

Startup should aggregate these into a validation report rather than only printing one-line failures.

## 4. Reflection and Contract Architecture

`kain-core` currently emits:

- required capabilities
- service bindings
- runtime-significant items
- placeholder reflection summary

The design extends that into:

- full reflected type schemas
- stable item identity metadata
- actor/message/component metadata
- compatibility metadata
- hot reload boundaries

`kain-driver` becomes the packager of record for:

- compiled output
- runtime contract bundle
- reflection payload
- realtime bundle
- compiled UI bundle
- version/compatibility metadata

The native runtime loads these together and validates them at startup.

## 5. Actor Runtime Design

The actor runtime should stop pretending that "spawn a thread plus queue" is enough.

Required structures:

- actor ID
- actor state record
- mailbox
- supervisor relationship
- monitor relationship
- registry entries
- exit reason
- scheduler queue

Required runtime APIs:

- actor spawn/bootstrap
- mailbox send/receive
- bounded mailbox config
- monitor/link registration
- actor registry register/lookup/unregister
- actor shutdown
- supervisor restart/escalation
- scheduler tick/yield
- actor diagnostics inspection

LLVM integration change:

- replace the current fallback-to-`default_actor_run` behavior with a real actor bootstrap call path tied to emitted actor entrypoints

## 6. Async Runtime Design

Async support must share the scheduler substrate rather than creating a disconnected mini-runtime.

Required pieces:

- task/future handle
- pending/ready/completed/cancelled states
- wake queue
- timer wheel or timer queue
- host wait integration
- actor/task interop

Required behaviors:

- spawn async task
- await on task
- poll once / wake later
- sleep/delay without blocking a worker thread unnecessarily
- cancellation propagation
- deterministic cleanup

## 7. UI Runtime Convergence Design

The compiled bundle/UI overlay path is worth preserving, but it needs to grow into a real UI runtime.

Near-term convergence:

- validate bundle structure more strictly
- carry more semantic metadata
- centralize input and focus routing
- formalize lifecycle hooks

Fuller runtime:

- stateful component instances
- invalidation/re-render model
- routed input events
- editable control support
- parity checks against Rust-native consumers

## 8. Shader, Material, and Compute Design

The current OpenGL host path is a platform foothold, not the final architecture.

This design expects:

- driver-emitted shader/material/compute artifact manifests
- reflection metadata for bindings and resource layout
- runtime loader APIs
- backend contract that is either backend-neutral or explicitly multi-backend
- artifact compatibility and cache policy

The first completed version can still use the current GL lane where appropriate, but must stop encoding everything as one-off host logic.

## 9. Hot Reload and Compatibility Design

Versioning must become part of the runtime's startup and update model.

Compatibility metadata should cover:

- runtime ABI version
- runtime feature set
- bundle compatibility class
- migration hook presence
- state transfer capability
- required/optional service changes

Lifecycle APIs:

- install bundle
- validate bundle
- activate bundle
- deactivate bundle
- update bundle
- uninstall bundle

## 10. Cross-Platform Design

Win32 remains the first concrete platform, but the runtime should stop assuming Win32 is the architecture.

This means:

- keep platform-independent declarations in `runtime/native/include`
- keep platform-independent logic in `runtime/native/src/core`
- move platform-specific behavior behind explicit service implementations
- fail unsupported builds with clear diagnostics rather than partial symbol exposure

## Validation Strategy

Validation is not a cleanup step. It is part of the design.

The finished work should include:

- `kain-core` golden tests for runtime contract and reflection emission
- `kain-driver` bundle artifact tests
- `kain-sys-codegen` tests for actor bootstrap and helper ABI emission
- C runtime unit or harness tests for base helpers, actor runtime, async/timers, diagnostics, and compatibility checks
- native smoke tests that compile and launch representative apps
- bundle compatibility and startup validation tests
- parity tests between Rust-native and raw-native bundle consumers where applicable

## Rollout Order

The correct order is:

1. contract/version/service-table hardening
2. reflection payload emission and runtime consumption
3. low-level helper ABI parity
4. actor bootstrap fix and actor runtime
5. async/timer runtime
6. UI/component convergence
7. shader/material/compute runtime
8. hot reload/version lifecycle
9. cross-platform adapters and parity validation

This keeps the implementation aligned with the current repo architecture and prevents the deep runtime features from outrunning the compiler/driver/runtime contracts they depend on.
