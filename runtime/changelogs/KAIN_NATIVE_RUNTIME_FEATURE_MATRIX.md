# Kain Native Runtime Feature Matrix

_Last updated: 2026-03-18_

## Purpose

This document maps the advertised `kain-core` feature surface against the current raw native C runtime in `runtime/native`.

It is intentionally blunt. The current native runtime is a useful raw-native substrate, but it is not yet a full implementation of the broader Kain runtime vision. If the goal is "full shabang" support for actors, comptime, async/runtime effects, rich UI/components, shader/material systems, reflection, hot reload, and cross-target parity, this document is the gap map.

## Scope

Primary reference points used for this matrix:

- `runtime/native_runtime.toml`
- `runtime/native/src/core/*`
- `runtime/native/src/ui/*`
- `runtime/native/src/platform/win32/*`
- `crates/kain-core/src/runtime.rs`
- `crates/kain-core/src/comptime.rs`
- `crates/kain-core/src/runtime_contract.rs`
- `crates/kain-core/src/low_level_memory.rs`
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
- `crates/kain-sys-codegen/src/codegen_cpp/mod.rs`
- `docs/KAIN_CORE_RUNTIME_IMPLEMENTATION_MATRIX_2026.md`
- `crates/kain-core/FEATURE_AUDIT_REPORT.md`

## Status Legend

- `Strong`: materially present and meaningfully usable
- `Partial`: implemented in a constrained or incomplete form
- `Scaffold`: structural groundwork exists, but not enough to claim feature support
- `Missing`: absent from the native/C lane
- `Broken`: exists in pieces, but integration is currently wrong or unreliable
- `Upstream gap`: not fully complete even in `kain-core`, independent of the C runtime

## Executive Verdict

The current native runtime materially supports:

- Win32 app hosting
- Win32 input capture
- OpenGL viewport hosting
- GLTF asset loading
- compiled UI bundle ingestion and overlay rendering
- actor bootstrap, mailboxes, monitors, links, registry, and supervision scaffolding
- async task execution, wake/poll, timers, and cancellation
- hot reload compatibility validation, lifecycle hooks, and migration scaffolding
- host bridge registration and module ABI validation
- explicit platform-boundary services with Linux/macOS stub coverage
- a small core utility layer for memory, strings, arrays, maps, sockets, threads, and queues

It does **not** yet provide a full native implementation of:

- Erlang-style actors
- Zig-style comptime
- macro/staged metaprogramming parity
- reflection-driven runtime services
- modern shader/material/compute execution
- full component/UI runtime behavior
- cross-platform native host parity
- end-to-end backend/runtime conformance guarantees

## Feature Matrix

| Area | `kain-core` status | Native/C runtime status | Missing to reach the advertised vision |
| --- | --- | --- | --- |
| Core host runtime substrate | `Strong` | `Partial` | Core allocation, thread spawn, basic collections, file IO, sockets, and queue primitives exist, but there is no full service model, capability negotiation, allocator diagnostics, or hardened lifecycle API. |
| Runtime contract sidecars | `Partial` | `Partial` | Contract bundle generation, native validation/parsing, compatibility metadata, async requirements, and platform availability metadata now exist, but the contract still is not rich enough to drive fully dynamic runtime behavior across every subsystem. |
| Reflection/type schema runtime | `Partial` | `Partial` | Compiler-emitted reflection payloads and native loading/validation now exist, but generalized type registries, richer reflective message/component metadata, and dynamic introspection services remain incomplete. |
| Actor model | `Partial` | `Partial` | Native runtime now has actor spawn, mailbox, monitors, links, supervision, registry, lifecycle tests, and scheduler validation, but it still lacks typed mailboxes, selective receive, crash isolation hardening, and remote/distributed actor support. |
| Actor scheduling/runtime policy | `Scaffold` | `Partial` | Mailbox ownership, bounded queues, and scheduler fairness tests now exist, but deeper policy like crash containment metrics, richer supervisor restart modes, and observability are still missing. |
| Async/await and futures | `Partial` | `Partial` | Native runtime now has task spawn/poll/await/cancel, timers, async sleep, wake handles, and conformance coverage, but it is still a small fixed-capacity executor rather than a broader effect-aware runtime. |
| Effects/capabilities model | `Scaffold` | `Missing` | Need a canonical capability/effect contract that is enforced consistently across compile time, interpretation, LLVM, C++, and raw-native execution. |
| Zig-style comptime | `Partial` | `Missing` | Comptime evaluation exists in frontend/interpreter terms, but native-capable staged execution boundaries, deterministic host services, phase isolation, typed transform contracts, and backend parity are missing. |
| Macro/metaprogramming | `Scaffold` | `Missing` | Need macro hygiene, provenance tracking, typed expansion hooks, diagnostics traceability, and stable runtime/compiler contracts for staged transforms. |
| Low-level memory model | `Partial` | `Partial` | Frontend lowering exists, but canonical helper ABI parity is incomplete. Need unified `__kain_*` semantics across LLVM/C++/native, plus conformance tests for pointer ops, unions, bitfields, packing, alignment, aliasing, and realloc behavior. |
| ABI and calling convention parity | `Scaffold` | `Missing` | Need a documented canonical ABI for values, closures, actors, host objects, async tasks, runtime services, reflection payloads, and low-level memory helpers across all backends. |
| Value model parity | `Partial` | `Missing` | `kain-core` runtime values cover richer cases like futures, actor refs, host objects, VNodes/JSX-like values, and structured control flow. Native runtime does not yet expose a comparable unified value model. |
| Component runtime | `Partial` | `Partial` | Native runtime now has explicit component state records, capability flags, focusable/editable tracking, and dirty-state plumbing, but it is still not a full generalized component reconciler. |
| UI runtime | `Partial` | `Partial` | Native UI overlay and compiled-bundle support now include runtime-side bundle validation, focus routing, editable text plumbing, and UI conformance smokes, but a full widget toolkit, diffing, accessibility, and rich event propagation are still missing. |
| JSX/VNode style rendering model | `Partial` | `Missing` | Some higher-level runtime concepts exist in `kain-core`, but native does not host a general VDOM/render graph or a reconciler. |
| Graph runtime/editor features | `Partial` | `Missing` | Need execution model, serialization contracts, node reflection, scheduling, debug stepping, runtime/editor parity, and native host tooling hooks. |
| State machines | `Partial` | `Missing` | Need a first-class runtime representation, transition execution model, event wiring, debug metadata, and serialization/hot-reload behavior. |
| Gameplay ability/effect/cue/task systems | `Scaffold` | `Missing` | Language/frontend surface exists, but no corresponding raw-native runtime subsystems are present. |
| Editor modules/tool runtime | `Scaffold` | `Missing` | Need editor service APIs, module loading, tool lifecycle contracts, command routing, docking/panel runtime behavior, and host/editor communication channels. |
| Shader system | `Scaffold` | `Partial` | Native runtime now validates shader/material/compute metadata from realtime bundles and has a GL-lane bundle contract check, but it still lacks full shader artifact execution, backend abstraction, and pipeline lifecycle management. |
| Material system | `Scaffold` | `Partial` | Native runtime now recognizes material metadata and primary binding references, but it still lacks full material instances, resource lifetime management, caching, and editor/runtime parity. |
| Compute runtime | `Scaffold` | `Scaffold` | Native runtime can now validate compute metadata and bundle expectations, but it still does not execute compute pipelines end to end. |
| 3D scene/runtime integration | `Partial` | `Partial` | GLTF loading and viewport plumbing exist, but there is no broader scene graph contract, ECS-like world runtime, reflection-driven component attachment, robust camera/controller architecture, or unified render/resource model. |
| Sculpt/runtime-native tools | `Scaffold` | `Partial` | There is specific Win32 sculpt/viewport infrastructure, but not a generalized runtime tool framework with plugin contracts, input/action routing, undo/redo, tool state, or data-driven tool descriptors. |
| Asset pipeline/runtime loading | `Partial` | `Partial` | GLTF is present, but broader asset typing, reflection, streaming, cache invalidation, dependency tracking, schema versioning, and hot reload are still missing. |
| Networking | `Partial` | `Partial` | Low-level sockets exist, but no higher-level HTTP, RPC, distributed runtime messaging, service discovery, or actor transport model is implemented in the native lane. |
| Host extension/plugin bridge | `Partial` | `Partial` | Native runtime now has a host bridge registry, module install/activate/uninstall APIs, ABI validation, service registration/discovery, and conformance tests, but it is still an in-process registry rather than a dynamic loader-backed plugin system. |
| Python/foreign bridge parity | `Partial` | `Partial` | Native runtime now exposes canonical foreign bridge contracts for Rust/Python/Node/C/Zig lanes, but actual marshaling/lifetime integration with those runtimes is still thin. |
| Hot reload | `Scaffold` | `Partial` | Native runtime now has compatibility validation, lifecycle APIs, migration hooks, state snapshot/restore, and hot-reload conformance tests, but full contract integration and live subsystem reload policy are still incomplete. |
| Incremental/live development loop | `Scaffold` | `Missing` | Need artifact invalidation tracking, runtime patch points, live subsystem restart rules, and editor/runtime integration for safe live iteration. |
| Diagnostics and errors | `Partial` | `Partial` | The runtime has basic logging and validation messages, but lacks structured diagnostics, stable error codes, layered reporting, subsystem context, crash dumps, and postmortem hooks. |
| Testing and conformance | `Partial` | `Partial` | Native runtime now has dedicated actor, async, UI, graphics, hot reload, host bridge, and platform boundary runners with timeout-backed harnesses, but parity matrices and CI enforcement are still missing. |
| Security/capability boundaries | `Scaffold` | `Missing` | Need capability-scoped runtime services, sandbox-aware host APIs, policy hooks, and explicit trust boundaries for native extensions and staged execution. |
| Cross-platform native runtime | `Partial` | `Partial` | The runtime is still Win32-first, but there is now an explicit platform boundary with Linux/macOS stubs, capability descriptors, and conformance coverage for unsupported-platform diagnostics. |
| Backend parity with LLVM | `Partial` | `Partial` | LLVM does more than C++, but still does not deliver the full runtime story. Need clear feature guarantees, canonical lowering contracts, and runtime compatibility tests. |
| Backend parity with C++ | `Partial` | `Missing` | C++ backend explicitly skips major areas like actors/components/shaders. It is not a substitute for full native runtime completeness. |
| Language feature completeness before runtime | `Partial` | `Upstream gap` | Some advertised language/parser/runtime features still have drift or open gaps in `kain-core` itself. Native completeness depends on tightening the upstream surface first. |

## Critical Blocking Gaps

These are the highest leverage blockers if the goal is "advertised features actually work end-to-end on the native/C lane."

### 1. Canonical runtime ABI

Without one stable runtime ABI, every backend and subsystem will keep drifting.

Need:

- canonical value representation rules
- actor/task/host object calling conventions
- reflection payload schema
- low-level memory helper ABI
- runtime service registry and versioning
- backend conformance harness

### 2. Real actor system

The actor story is currently the most obvious mismatch between the vision and the native implementation.

Need:

- actor bootstrap ABI
- mailbox ownership/lifetime model
- typed message schemas
- selective receive or explicit non-goal
- supervisor tree semantics
- monitor/link semantics
- crash propagation policy
- registry/discovery
- scheduling fairness and backpressure
- native tests for restart, failure, shutdown, and monitoring

### 3. Staged execution and comptime contract

If Kain is going to claim Zig-like comptime and ambitious macro behavior, staged execution needs to become a first-class platform contract.

Need:

- deterministic staged execution boundary
- host capability model for comptime
- typed AST/IR transform interface
- macro hygiene
- provenance and diagnostics traceability
- parity across interpreter, LLVM, C++, and raw-native artifact generation

### 4. Reflection-first runtime services

Many of the grander features depend on reflection payloads actually existing.

Need:

- emitted type schemas
- runtime type registry
- schema versioning and compatibility
- component/message/graph reflection
- dynamic binding driven by metadata instead of handwritten assumptions

### 5. Modern render/material/compute runtime

The current OpenGL host path is useful, but it is not the final architecture for advertised shader/material/compute features.

Need:

- backend-neutral graphics abstraction or explicit backend contracts
- shader artifact loading pipeline
- reflection-driven resource binding
- material runtime representation
- compute dispatch support
- resource lifetime/cache policy
- hot reload and validation/debug tooling

### 6. Full UI/component runtime

Compiled overlays are not enough to claim full component/runtime support.

Need:

- reactive state propagation
- reconciliation or explicit retained-mode alternative
- event routing/focus/input semantics
- text editing and widget toolkit
- accessibility hooks
- host/runtime lifecycle contracts

## Evidence Notes

The current native runtime manifest is intentionally narrow:

- `runtime/native_runtime.toml` only lists core host, contract, realtime, GLTF, OpenGL host, Win32 host/input/shared, UI bundle/overlay, viewport, and sculpt sources.

The actor/runtime mismatch is especially important:

- `runtime/native/src/core/kain_runtime_core.c` provides `KAIN_spawn`, queues, and a `default_actor_run` wrapper, but not a complete actor runtime.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` emits actor-related code, but spawn wiring still appears to use the default wrapper path rather than a robust actor bootstrap.

The comptime/runtime mismatch is also fundamental:

- `crates/kain-core/src/comptime.rs` is a frontend/interpreter-side staged evaluation mechanism, not a full native staged execution runtime contract.

The runtime contract layer is not reflection-complete:

- `crates/kain-core/src/runtime_contract.rs` still treats reflection payload emission as not yet fully realized.

The low-level memory layer is not yet one unified runtime ABI:

- `crates/kain-core/src/low_level_memory.rs` and `crates/kain-core/LOW_LEVEL_MEMORY_STATUS.md` show active design/implementation work still in progress.

## Recommended Implementation Order

If the goal is to get the native/C lane from "useful substrate" to "advertised Kain feature runtime," I would sequence the work like this:

### Phase 1: Lock the contracts

1. Define the canonical runtime ABI.
2. Finalize reflection/type-schema emission.
3. Freeze low-level memory helper contracts.
4. Add backend/runtime conformance tests.

### Phase 2: Fix the concurrency story

1. Repair LLVM actor spawn/bootstrap integration.
2. Introduce real mailbox and actor lifecycle semantics.
3. Add supervision, monitoring, restart, and shutdown behavior.
4. Add actor/runtime diagnostics and test coverage.

### Phase 3: Make staged execution real

1. Formalize comptime execution boundaries.
2. Introduce typed macro/staged transform APIs.
3. Add hygiene, provenance, and diagnostics.
4. Guarantee parity across targets.

### Phase 4: Modernize runtime services

1. Build the reflection-driven component/runtime layer.
2. Replace handwritten UI/runtime assumptions with explicit contracts.
3. Add async/futures runtime support.
4. Establish host extension/plugin APIs.

### Phase 5: Build the graphics/runtime stack that matches the vision

1. Add shader artifact/runtime execution.
2. Add material graph/runtime support.
3. Add compute support.
4. Add hot reload, compatibility, and migration behavior.

### Phase 6: Widen platform coverage

1. Port app host/input/window/runtime services beyond Win32.
2. Add platform parity tests.
3. Make service availability explicit in contracts rather than implicit in host code.

## Practical Conclusion

Today, the C runtime is best described as:

> a promising raw-native host/runtime substrate for viewport-centric apps, compiled UI bundles, GLTF assets, and native experiments

It is **not yet**:

> a full native realization of the entire Kain feature surface

That is fixable, but it requires moving from ad hoc per-subsystem implementation to a reflection-first, ABI-first, conformance-tested runtime architecture.
