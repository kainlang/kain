# KAIN Runtime Expansion Roadmap - 2026-04-11

## Intent

This roadmap defines the next runtime direction for Kain as of 2026-04-11.

The target is not a modest native runtime. The target is a self-hosting, AI-developed, private language runtime that can credibly own:

- self-hosted compilation and execution
- web servers, web clients, and networked systems
- MCP servers, MCP clients, tool orchestration, and agent-heavy workflows
- high-end graphics, realtime rendering, scene systems, and large 3D engines
- audio, media, DAW-grade execution, and realtime scheduling
- aggressive comptime, reflection, and code generation
- cross-language runtime integration with Rust, Zig, Go, C, C++, Python, Node, WASM, and future lanes

Because the language is private and the project is AI-developed end-to-end, the runtime should optimize for long-range power and coherence rather than compatibility theater. Breaking changes are acceptable until a deliberate runtime ABI v1 is declared.

## Baseline In Repo Today

Kain already has a meaningful runtime substrate:

- manifest-driven native runtime selection in [native_runtime.toml](./native_runtime.toml)
- a canonical service registry in [native/include/kain_runtime_services.h](./native/include/kain_runtime_services.h)
- startup, ABI, and runtime-contract validation in [native/src/core/kain_runtime_contract.c](./native/src/core/kain_runtime_contract.c)
- actor, async, timer, reflection, UI bundle, compatibility, and host-bridge seams
- a real Linux raw-native compile lane and a stronger Win32 desktop-host lane

Kain does not yet have the deeper runtime core needed for the final ambition:

- no unified runtime value/object model
- no engine-grade scheduler/kernel
- no production-grade dynamic module ABI
- no deep web/runtime service family
- no real audio/DAW subsystem
- no fully realized graphics/scene/resource execution engine
- no hardened self-hosting compiler/runtime boundary

## Working Assumptions

1. The runtime is allowed to evolve aggressively until an explicit ABI freeze is declared.
2. The compiler, runtime, driver, bridge SDKs, and generated schemas should be treated as one co-designed system.
3. Every major runtime family should be manifest-driven, service-table-driven, and conformance-tested.
4. Runtime truths should be machine-readable first: manifests, schemas, capability tables, generated bindings, compatibility metadata, and validation matrices.
5. Human ergonomics still matter, but the codebase should optimize for strong-agent continuity and large autonomous refactors.

## North Star Runtime Shape

The runtime should evolve toward a capability-driven kernel with explicit service families:

- `core.kernel`
- `core.scheduler`
- `core.values`
- `core.memory`
- `core.tracing`
- `platform.*`
- `net.*`
- `web.*`
- `mcp.*`
- `scene.*`
- `gfx.*`
- `audio.*`
- `ui.*`
- `asset.*`
- `hostbridge.*`
- `comptime.*`
- `tooling.*`

Each family should eventually have:

- canonical service keys
- versioned binary contracts
- generated host bindings
- structured diagnostics
- startup and compatibility validation
- conformance tests
- hot-reload and migration policy

## Roadmap

## Phase 0 - Break Window And Runtime Constitution

Goal: define the rules before the runtime grows into ten incompatible partial systems.

Deliverables:

- declare a pre-ABI-v1 break window for runtime internals, manifests, contracts, and bridge surfaces
- define the canonical runtime family map and service-key namespace
- define a runtime schema strategy for values, messages, resources, graphs, and module metadata
- define a versioning policy for runtime core, bridge SDKs, plugins, and generated artifacts
- define what the runtime kernel owns vs what app/template code owns

Exit criteria:

- one written runtime constitution document
- one canonical namespace for service keys and capabilities
- one declared ABI stabilization policy

## Phase 1 - Runtime Kernel And Scheduler

Goal: replace the current substrate mindset with an actual runtime kernel.

Deliverables:

- unified scheduler for actors, async tasks, timers, IO events, job queues, and frame work
- priority classes for UI, render, background, network, tool, and audio lanes
- budget-aware execution for frame and interactive workloads
- scheduler metrics, tracing hooks, queue-depth telemetry, and crash snapshots
- backpressure and admission control at every mailbox, task queue, and service boundary
- cooperative yield plus controlled escape hatches for blocking foreign calls

Exit criteria:

- all runtime work classes run through one kernel-owned scheduling model
- no major subsystem owns a secret scheduler
- tracing can explain where runtime time is going

## Phase 2 - Unified Runtime Values, Memory, And Reflection

Goal: create one value model that all language and foreign lanes can speak.

Deliverables:

- stable runtime value ABI for scalars, strings, blobs, arrays, maps, structs, enums, unions, errors, handles, and opaque foreign objects
- ownership and lifetime rules across C, Rust, Zig, Go, Python, Node, and WASM
- runtime reflection queries over values, services, modules, resources, and live state
- canonical binary serialization format for in-process and IPC transport
- zero-copy buffer and shared-memory primitives
- generated FFI bindings from the canonical schema family

Exit criteria:

- new services stop inventing ad hoc structs for every boundary
- cross-language calls can use one binding strategy
- runtime inspection is value-aware, not just service-aware

## Phase 3 - Platform, IO, And Web Runtime

Goal: make Kain a serious host and server runtime, not just a native executable lane.

Deliverables:

- platform-neutral service contracts for windows, input, display, files, processes, watchers, sockets, timers, clipboard, and device discovery
- real Linux and macOS host providers, not just capability stubs
- integrated async IO model under the kernel scheduler
- HTTP client/server runtime services
- WebSocket and streaming transport services
- TLS boundary ownership and diagnostics
- browser and WASM hosting strategy for web-facing execution

Exit criteria:

- portable host services exist across the major desktop targets
- web functionality is a runtime family, not an external bolt-on

## Phase 4 - MCP And Agent Runtime

Goal: make MCP a first-class runtime capability because Kain is being developed for AI-heavy operation.

Deliverables:

- `mcp.server` runtime service family
- `mcp.client` runtime service family
- stdio, pipe, websocket, and process transport adapters
- canonical runtime contracts for tools, resources, prompts, schemas, and auth/session policy
- runtime-side permission model for tool invocation and file/network/process access
- generated bindings so Kain-authored MCP servers and clients can materialize without bespoke glue
- structured tracing for MCP tool calls, resource fetches, and failures

Exit criteria:

- Kain can host MCP servers natively
- Kain can consume MCP servers as runtime services
- runtime policy can gate tool access safely

## Phase 5 - Graphics, Scene, Compute, And Asset Engine

Goal: graduate from validation-heavy graphics scaffolding into an engine-grade execution lane.

Deliverables:

- render graph contracts with explicit pass dependencies and scheduling
- resource graph and residency management for buffers, images, streaming uploads, transient pools, and memory budgets
- scene runtime with stable handles, query APIs, mutation transactions, undo/redo receipts, and snapshot support
- backend-neutral graphics interface with concrete Vulkan/Metal/DX/OpenGL strategy
- material system, shader reflection, pipeline caches, and bindless-friendly resource binding
- compute runtime with dispatch, synchronization, barriers, staging, and diagnostics
- asset ingestion pipeline with one canonical route for compiler-emitted and host-imported data

Exit criteria:

- realtime lane owns actual execution policy
- scene and graphics state are runtime-owned, queryable, and serializable

## Phase 6 - Audio, Media, And DAW Runtime

Goal: add a true realtime-safe audio lane rather than forcing audio through the general async runtime.

Deliverables:

- dedicated audio device/runtime family
- lock-free or bounded realtime-safe queues
- sample-accurate transport, timeline, and automation primitives
- audio graph execution with plugin/module hooks
- media decode/encode and stream timing support
- session persistence, offline render, and deterministic bounce/export contracts
- diagnostics for underruns, drift, latency, and device hotplug

Exit criteria:

- Kain can host DAW-grade execution without violating realtime safety rules
- audio does not depend on allocator-heavy or blocking code paths

## Phase 7 - UI, Workspace, And Editor Runtime

Goal: make Kain capable of hosting large native tools, not just launching narrow bundles.

Deliverables:

- dockable workspace model with multi-window and multi-viewport layout state
- inspector, outliner, tree, table, property-sheet, timeline, command-palette, and overlay primitives
- runtime-bound UI component model that sits above the current compiled-bundle layer
- scene-aware UI bindings and editor-grade focus/input routing
- persistent workspace/session state with crash recovery
- native app packaging and launch-profile materialization across major hosts

Exit criteria:

- Kain can host its own large editor surfaces
- runtime-owned workspace state survives reloads and crashes

## Phase 8 - Foreign Runtime Mesh

Goal: make foreign languages a designed runtime mesh, not random ad hoc bridges.

Deliverables:

- one generated ABI/binding system for Rust, Zig, Go, C, Python, Node, and WASM
- explicit categories for in-process library, out-of-process service, sandboxed module, and generated tool/plugin
- foreign runtime capability negotiation and lifecycle management
- panic/exception/failure normalization at ABI boundaries
- shared tracing, metrics, and diagnostic routing across foreign lanes
- bridge policy for zero-copy payloads vs serialized payloads

Exit criteria:

- adding a new language lane means generating bindings and contracts, not hand-writing a new runtime theory

## Phase 9 - Self-Hosting, Comptime, And Runtime-Owned Toolchain

Goal: make the language capable of owning more of its own compiler, runtime, and generated toolchain behavior.

Deliverables:

- explicit comptime capability model
- deterministic artifact cache and build graph
- runtime-aware incremental compilation
- generated bridge SDKs from compiler-owned schemas
- backend/runtime parity tests across interpreter, LLVM, native runtime, Rust-native, and foreign bridge lanes
- staged path toward the compiler owning more of its own runtime emission and validation logic

Exit criteria:

- the compiler and runtime share one contract system
- self-hosting can grow without duplicating truth across backends

## Phase 10 - Hardening, Isolation, And Production Discipline

Goal: turn raw power into something survivable.

Deliverables:

- crash isolation for modules and foreign lanes where possible
- permission and capability sandboxing
- replayable trace captures
- deterministic conformance and stress suites
- performance dashboards and regression gates
- ABI compatibility matrix and migration tooling
- memory, leak, and ownership verification layers

Exit criteria:

- runtime failures are diagnosable
- high-risk subsystems can be isolated or restarted

## Cross-Language Viability Study

This section evaluates how other languages should participate in the Kain runtime.

### Decision Summary

- Rust: highest-priority foreign language for in-process runtime services
- Zig: highest-priority systems coprocessor and native-module language alongside Rust
- Go: useful, but best treated as sidecar/server/WASI/service language more often than as deep in-process kernel code
- C/C++: baseline ABI layer and low-level escape hatch, but not the only long-term extension story
- Python/Node: keep for tooling, scripting, adapters, and bridge surfaces, not for the deepest runtime core
- WASM: strategically important as a sandboxed module format and future plugin lane

### Rust

Viability: very high

Best roles:

- kernel-adjacent services
- network and protocol stacks
- storage layers
- runtime modules with strong correctness requirements
- graphics helpers, asset pipelines, and editor infrastructure

Why:

- official Rust linkage supports `staticlib` and `cdylib`; `cdylib` is explicitly for dynamic libraries loaded from another language, and `staticlib` is recommended for linking Rust into existing non-Rust applications
- Rust gives a strong story for ownership, rich type modeling, and generated bindings
- Rust is already present in the repo and is the natural closest partner to the existing compiler and driver lanes

Constraints:

- unwinding across FFI boundaries must be designed carefully
- if Rust panics cross the wrong ABI boundary, behavior can abort or become undefined depending on the case
- symbol visibility and static-link duplication must be managed deliberately

Recommendation:

- treat Rust as a Tier 1 in-process runtime language
- build canonical `repr(C)` bridge types, `extern "C"` entrypoints, and panic-containment wrappers
- prefer `staticlib` for tightly integrated runtime cores and `cdylib` for hot-swappable module families

### Zig

Viability: very high

Best roles:

- low-level runtime helpers
- allocators and memory subsystems
- graphics, DSP, and media kernels
- platform adapters
- WASM-targeted modules

Why:

- official Zig docs position exporting a C ABI library as a primary use case
- Zig can emit static libraries, shared libraries, and object files cleanly
- Zig is strong for low-level systems work, cross-target builds, and compile-time specialization

Constraints:

- use a stable Zig release for production lanes rather than depending on `master` semantics
- do not let Zig-specific build assumptions bypass Kain’s canonical manifests and schemas

Recommendation:

- treat Zig as a Tier 1 systems coprocessor language beside Rust
- use it for high-performance modules, platform work, DSP, graphics kernels, and WASM-friendly exports
- keep ABI boundaries C-shaped and generated

### Go

Viability: medium for in-process use, high for sidecar/service use

Best roles:

- MCP servers and clients
- web services
- distributed tooling
- background orchestration
- sidecar agents
- WASI or subprocess-hosted modules when isolation matters more than raw in-process integration

Why:

- official Go build modes support `c-archive` and `c-shared`
- Go can expose C-callable entrypoints with cgo-exported functions
- Go also supports plugin builds, but the official plugin docs list major portability and runtime-consistency drawbacks

Constraints:

- cgo requires a C toolchain and is disabled by default for some cross-compilation cases
- Go `plugin` is not portable enough for a core runtime extension model
- official docs warn that plugin users can hit runtime crashes unless everything is built with the exact same toolchain, flags, environment, and source versions
- Go has its own scheduler and runtime assumptions, so deep kernel embedding is less attractive than with Rust or Zig

Recommendation:

- do not make Go the primary in-process runtime extension model
- use Go heavily for MCP, orchestration, web, and sidecar/server lanes
- if in-process Go is required, prefer narrow `c-shared` or `c-archive` surfaces with strict payload contracts
- avoid designing the Kain plugin ecosystem around Go `plugin`

### C And C++

Viability: high, with higher correctness risk

Best roles:

- baseline ABI
- platform and graphics interop
- legacy library reuse
- hardware/vendor SDK integration

Recommendation:

- preserve C ABI compatibility as the universal floor
- use C++ selectively for vendor-heavy integrations, not as the main extensibility model

### Python And Node

Viability: high for tooling and service lanes, low for kernel ownership

Best roles:

- content tooling
- automation
- scripting
- editor adapters
- MCP and web-edge integrations

Recommendation:

- keep them as high-leverage outer lanes
- do not let them become the correctness-critical runtime core

### WASM

Viability: strategically high

Best roles:

- sandboxed plugins
- portable compute modules
- edge/runtime extensions
- future community-safe extension points

Recommendation:

- add a first-class WASM service family after the runtime value ABI is stable enough
- use WASM to complement, not replace, native Rust/Zig/C extensions

## Foreign Runtime Policy

The runtime should classify non-Kain execution into four modes:

1. In-process native module  
   Rust, Zig, C, and some Go via C ABI. Highest performance. Highest correctness burden.

2. Out-of-process service  
   Go, Python, Node, Rust, or mixed. Best for MCP, web, orchestration, and isolation.

3. Sandboxed WASM module  
   Best for portable capability-limited execution.

4. Generated bridge SDK  
   Best for language-specific authoring while preserving one canonical runtime contract.

No language should get a bespoke contract family. Every language lane should be generated from the same schemas and service definitions.

## Immediate Next Steps

1. Formalize the runtime constitution and pre-ABI-v1 break window.
2. Design the unified runtime value ABI and cross-language ownership model.
3. Upgrade the scheduler into a real kernel for actors, async, IO, and frame work.
4. Split platform/web/MCP into first-class service families.
5. Define the foreign runtime mesh around Rust and Zig first, with Go positioned primarily as a sidecar/service language.
6. Build the graphics/scene/resource architecture and the audio/DAW architecture as separate kernel-aware families, not as generic helper piles.
7. Tighten backend/runtime parity until the compiler, runtime, and bridges all consume one truth model.

## Research Basis

Official references used for the foreign-language viability section:

- Go `cmd/go` build modes: https://pkg.go.dev/cmd/go
- Go `cmd/cgo`: https://pkg.go.dev/cmd/cgo
- Go `plugin` package warnings: https://pkg.go.dev/plugin
- Rust linkage reference: https://doc.rust-lang.org/stable/reference/linkage.html
- Rust Nomicon FFI guidance: https://doc.rust-lang.org/nomicon/ffi.html
- Zig language reference: https://ziglang.org/documentation/master/
