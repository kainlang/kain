# KAIN Core and Native Runtime Roadmap 2026

> **Date:** March 14, 2026  
> **Purpose:** Define the concrete roadmap for turning Kain's declared language features into robust runtime-backed platform capabilities.  
> **Scope:** `crates/kain-core`, `runtime/native`, `crates/kain-host`, `crates/kain-reflect`, and the contracts between them.

---

## 1. Why This Document Exists

Kain already exposes a large semantic surface in `kain-core`:

- actor syntax and interpreter support
- compile-time evaluation
- macros
- monomorphization
- async lowering
- low-level memory semantics
- multiple backend targets

At the same time, the native C runtime is still comparatively thin:

- refcounted allocations
- arrays and maps
- basic thread spawning
- sockets and file I/O
- a simple message queue
- viewport and native UI support layered above that base

That means Kain currently has a mismatch:

**the language can describe more than the portable runtime can guarantee**

This document exists to close that gap in a disciplined way.

## March 18, 2026 Update

The gap described above is still real, but it is narrower than it was when this roadmap was drafted.

- actor bootstrap, mailbox/runtime semantics, registry, monitors, links, and timeout-safe actor conformance are now in the native lane
- async task execution, timers, wake/poll, and async contract metadata are now in the native lane
- hot reload compatibility, lifecycle APIs, migration/state transfer hooks, and host bridge contracts are now present
- platform boundaries are explicit, with Linux/macOS stub services and contract-visible availability
- UI runtime and graphics runtime moved from \"thin host only\" into materially useful partial implementations

The remaining honest gap is not \"nothing exists.\" It is that several advanced lanes are still partial rather than fully system-complete:

- richer supervision policy and scheduler depth
- Rust-native versus raw-native UI bundle parity
- full material/resource lifetime management
- true compute execution support
- deeper reflection/diagnostics conformance coverage

---

## 2. Current State

### 2.1 What `kain-core` already has

`kain-core` is already the semantic heart of the language:

- parser, AST, type checker, effects, diagnostics, stdlib loading
- tree-walking interpreter/runtime for `kain run` and `kain test`
- comptime execution
- monomorphization
- low-level memory lowering and ABI-aware layouts
- actor syntax and interpreter-level actor execution

Important reality:

- much of the feature surface is real at the parser and type level
- some of it is real in the interpreter
- much less of it is real as a stable cross-target runtime contract

### 2.2 What the native runtime already has

The native runtime under `runtime/native` is becoming a platform substrate, but it is not yet a full execution runtime in the Erlang-style sense.

It currently provides:

- RC allocation primitives
- string, array, map, file, socket helpers
- OS thread spawn helpers
- a mutex-backed queue
- Win32 viewport and native UI support
- glTF and graphics support on the native lane

It does **not** yet provide:

- a scheduler-based actor runtime
- supervision trees
- mailbox policies and backpressure
- hot-reload state transfer
- reflection-backed object metadata
- stable runtime capability negotiation
- robust cross-thread ownership guarantees

### 2.3 What is already in the right crate

Two important pieces already live outside `kain-core`, which is correct:

- `kain-host` owns embedding and Rust host interop
- `kain-reflect` owns reflection schemas and type registries

This separation should be preserved.

---

## 3. Core Thesis

The roadmap is **not** "move every cool feature into `kain-core`."

The roadmap is:

1. Put semantic truth, contracts, metadata, and lowering rules in `kain-core`.
2. Put portable low-level execution machinery in `runtime/native`.
3. Put host embedding and reflected host types in `kain-host` and `kain-reflect`.
4. Make backends and runtimes consume the same compiler-emitted runtime contract.

If we fail this separation, Kain becomes a compiler crate that is trying to be:

- a parser
- an interpreter
- a hot-reload engine
- a host SDK
- a UI runtime
- a native actor VM
- an editor platform

That would be structurally wrong.

---

## 4. Ownership Model

### 4.1 `kain-core`

`kain-core` should own:

- syntax
- AST and typed IR
- effect and capability rules
- actor semantics
- message typing rules
- comptime semantics
- macro and transform semantics
- monomorphization
- metamorphization passes
- runtime capability requirements
- reflection metadata emission
- hot-reload compatibility analysis
- migration contract generation

`kain-core` should **not** own:

- OS threads
- file watchers
- DLL loading
- mailbox scheduling implementation
- native UI host loops
- raw host embedding APIs

### 4.2 `runtime/native`

`runtime/native` should own:

- memory and object lifetime runtime
- mailbox implementation
- scheduler and task execution
- timers and event loop primitives
- actor process state and supervision runtime
- dynamic code loading hooks
- code version registration
- state migration application
- native host services used by compiled programs

### 4.3 `kain-host`

`kain-host` should own:

- Rust embedding APIs
- native function registration
- reflected type registration
- engine/module export helpers
- bridging between Kain runtime values and host values

### 4.4 `kain-reflect`

`kain-reflect` should own:

- stable reflection schema types
- type registries
- schema rendering and serialization support
- type identity rules for host-visible types

---

## 5. Architectural End State

The desired execution stack is:

```text
Kain source
  -> parse / type / effects / comptime / monomorphize / metamorphize
  -> runtime contract bundle
  -> target backend output + metadata
  -> runtime/native and host runtimes consume same contract
```

The contract bundle must describe:

- symbols and stable item IDs
- reflected types
- actor definitions and message schemas
- runtime capability requirements
- migration compatibility metadata
- hot-reload patch boundaries
- host imports and capability bindings

This is the bridge between "language feature" and "runtime guarantee."

---

## 6. The Runtime Contract Layer

This is the highest-value missing layer.

Today, actor syntax in `kain-core` and mailbox helpers in the C runtime are adjacent but not unified by a formal ABI.

Kain needs a dedicated runtime contract layer with:

### 6.1 Stable symbol identity

Every runtime-significant item needs a stable identity:

- functions
- structs
- enums
- actors
- messages
- components
- services
- host imports

This identity cannot be "current textual name only." It needs a stable item ID plus versioning metadata.

### 6.2 Runtime capability requirements

`kain-core` should emit required capabilities for a program, such as:

- `actor_runtime`
- `supervision`
- `hot_reload`
- `state_migration`
- `reflection_emit`
- `host_bridge`
- `native_ui`
- `gpu_runtime`
- `timer_service`

Targets and runtimes then advertise what they support.

Compilation or launch should fail early if the runtime cannot satisfy the required contract.

### 6.3 Runtime service table

Compiled programs should bind to a declared service table rather than scattered helper names.

Examples:

- memory allocator
- object retain/release
- mailbox push/pop/wait
- scheduler enqueue
- timer registration
- actor spawn/link/monitor
- reflection lookup
- code version install
- state migration apply

This is where the C runtime becomes a real execution runtime instead of a helper bag.

---

## 7. Actor Concurrency Roadmap

### 7.1 Current reality

Kain already has:

- actor syntax in `kain-core`
- interpreter-level actor refs and message sending
- native thread spawn helper
- native mutex-backed queue

That is enough to demonstrate concurrency semantics, but not enough for production actor runtime behavior.

### 7.2 Required target semantics

The actor model should converge on:

- isolated actor state
- typed messages
- per-actor mailbox
- mailbox ordering guarantees
- bounded mailbox options
- backpressure policies
- links and monitors
- supervision trees
- restart strategies
- actor registry and discovery
- timers and scheduled messages
- selective receive or an explicit alternative
- structured failure propagation

### 7.3 Required compiler work in `kain-core`

Add a dedicated typed actor IR:

- actor definition metadata
- state layout metadata
- message schemas
- handler signatures
- spawn configuration
- supervision declarations
- restart policy metadata

Add compile-time validation for:

- actor state isolation
- message payload type safety
- cross-actor access restrictions
- mailbox capability requirements
- forbidden shared mutable state patterns

### 7.4 Required native runtime work

Implement real runtime subsystems for:

- mailbox lifecycle
- scheduler
- blocking mailbox wait
- timer wheel or timer queue
- actor process table
- link and monitor tables
- supervision restart orchestration
- structured shutdown

### 7.5 Critical blocker

Current RC lifetime handling is not safe enough for this model. Cross-thread ownership must be made correct before actor concurrency is treated as a flagship feature.

That means:

- atomic RC or another concurrency-safe ownership model
- well-defined move versus share semantics
- runtime tests for races and teardown safety

---

## 8. Metaprogramming Roadmap

### 8.1 Current reality

Kain already has:

- comptime execution
- macro syntax
- interpreter-backed evaluation

The current model is useful but still narrow.

### 8.2 Required end state

Metaprogramming in Kain should have three explicit tiers:

1. **Comptime evaluation**
2. **Macro expansion**
3. **Typed transform passes**

These should not be blurred together.

### 8.3 Comptime

Comptime should become:

- deterministic by default
- capability-gated
- explicit about host access
- serializable into compiler diagnostics and artifacts

It should be able to evaluate:

- constants
- schema generation
- reflected metadata derivation
- code/config generation
- safe compile-time transforms

### 8.4 Macros

Macros should evolve from simple built-ins into structured macro expansion over AST nodes.

Requirements:

- hygiene rules
- typed expansion boundaries
- source-span preservation
- debuggable expansion traces
- expansion diagnostics

### 8.5 Typed transforms

Some transformations should happen after typing, not at parse-time.

Examples:

- actor contract normalization
- runtime service injection
- capability specialization
- host bridge expansion
- hot-reload patch slicing

These transforms belong in `kain-core` as explicit passes.

---

## 9. Hot Reload Roadmap

### 9.1 Current reality

Kain has strong ingredients for hot reload, but not the system:

- compilation pipeline
- host runtime
- reflection registries
- native UI direction
- modular native runtime

What is missing is compatibility accounting and runtime state migration.

### 9.2 Required end state

Hot reload must support:

- code version installation
- stable symbol identity
- compatibility checking
- state migration
- actor/component replacement
- invalidation of incompatible reloads
- reload event hooks for tooling

### 9.3 Compiler responsibilities

`kain-core` should emit:

- reload manifests
- item IDs and version hashes
- layout compatibility summaries
- message schema compatibility summaries
- migration requirements
- patchable symbol boundaries

### 9.4 Runtime responsibilities

`runtime/native` should implement:

- code version table
- module loading / registration
- symbol rebinding
- actor handoff points
- component rebind points
- state migration application
- rollback on failed reload

### 9.5 Safety rule

Hot reload must be treated as a compatibility problem, not a convenience toggle.

If a reload changes:

- struct layout
- enum shape
- actor message schema
- component state contract

then the compiler/runtime pair must either:

- auto-migrate safely
- require an explicit migration function
- reject the reload

---

## 10. Reflection Roadmap

### 10.1 Current reality

Reflection infrastructure already exists in the crate graph. That is good. The missing piece is making compiler-emitted reflection metadata a first-class part of the program bundle.

### 10.2 Required end state

Kain should emit reflection metadata for:

- types
- fields
- enum variants
- functions
- actors
- components
- messages
- services
- capabilities

### 10.3 Compiler role

`kain-core` should produce a reflection graph as one of its standard outputs, not just as an optional side artifact.

### 10.4 Runtime and host role

- `kain-reflect` stays the schema carrier
- `kain-host` uses it for host embedding and registry generation
- `runtime/native` uses it for inspection, migration, tooling, and live reload support

This also enables:

- editor inspectors
- property grids
- dynamic command surfaces
- graph node generation
- schema-aware state migration

---

## 11. Metamorphization Roadmap

### 11.1 Definition

Monomorphization already exists and should stay focused on generic instantiation.

Metamorphization should be treated as a separate transformation family that reshapes a typed program for runtime context while preserving semantics.

Examples:

- specializing for runtime capability sets
- converting actor declarations into runtime actor bundles
- decomposing UI into host/runtime service boundaries
- generating hot-reload patch sets
- specializing for native, host, editor, or automation lanes

### 11.2 Why it matters

Without a formal metamorphization stage, Kain risks pushing too much semantic restructuring into:

- ad hoc backend codegen
- hand-written host glue
- target-specific hacks

That would make the language look capable while the platform remains fragmented.

### 11.3 Implementation model

Add metamorphization as explicit typed passes after typecheck and before final backend codegen:

1. monomorphization
2. actor/runtime normalization
3. capability specialization
4. hot-reload partitioning
5. host/service bridge expansion
6. target-specific final lowering

This should be data-driven and pass-oriented, not encoded as scattered special cases.

---

## 12. Native Runtime Hardening Requirements

Before the bigger features become credible, `runtime/native` needs a hardening phase.

### 12.1 Memory and ownership

- thread-safe ownership model
- allocator failure handling
- destructor ordering guarantees
- weak/strong semantics that are race-safe
- runtime leak instrumentation

### 12.2 Concurrency substrate

- mailbox wait primitives
- scheduler queues
- wakeup model
- timer primitives
- cancellation and shutdown signals

### 12.3 Runtime safety

- deterministic teardown
- panic/failure isolation boundaries
- structured diagnostics from runtime services
- runtime invariant checks in debug mode

### 12.4 Validation

- native unit tests
- actor stress tests
- mailbox ordering tests
- leak/race regression tests
- hot-reload compatibility tests

Without this phase, every advanced runtime feature will remain fragile.

---

## 13. Phased Delivery Plan

## Phase 1: Contract Foundation

Goal:

- establish the runtime contract layer

Deliverables:

- stable item IDs
- runtime capability registry
- reflection graph emission from `kain-core`
- initial actor/message typed IR
- documented runtime ABI surface

Primary repos:

- `crates/kain-core`
- `crates/kain-reflect`

## Phase 2: Runtime Hardening

Goal:

- make the native runtime trustworthy enough to host actor and reload semantics

Deliverables:

- safe ownership model
- scheduler primitives
- mailbox wait/backpressure
- timer service
- native runtime tests

Primary repos:

- `runtime/native`

## Phase 3: Actor Runtime

Goal:

- turn actor syntax into a real execution feature

Deliverables:

- typed actor lowering contract
- actor process table
- links and monitors
- supervision trees
- restart policies
- actor conformance tests

Primary repos:

- `crates/kain-core`
- `runtime/native`

## Phase 4: Hot Reload Core

Goal:

- support safe live code updates

Deliverables:

- reload manifests
- version tables
- compatibility analysis
- explicit migration hooks
- runtime rollback on failed reload

Primary repos:

- `crates/kain-core`
- `runtime/native`
- `crates/kain-host`

## Phase 5: Metaprogramming Expansion

Goal:

- make compile-time and transform-time programming a first-class platform strength

Deliverables:

- deterministic comptime capability model
- hygienic macro expansion tracing
- typed transform passes
- reflection-aware code generation hooks

Primary repos:

- `crates/kain-core`

## Phase 6: Metamorphization

Goal:

- unify target/context shaping in one formal pass family

Deliverables:

- metamorphization pass framework
- capability specialization
- actor/runtime packaging transforms
- host bridge specialization
- hot-reload patch slicing

Primary repos:

- `crates/kain-core`
- downstream backend crates

---

## 14. Recommended Immediate Work Order

If the goal is maximum leverage with minimum structural regret, the next work should happen in this order:

1. Define the runtime ABI and capability contract.
2. Harden the native runtime memory and concurrency substrate.
3. Add typed actor/message IR in `kain-core`.
4. Emit reflection metadata from `kain-core` as a standard artifact.
5. Add hot-reload item IDs and compatibility manifests.
6. Add migration hooks and runtime version installation.
7. Expand comptime and macros into a proper staged metaprogramming system.
8. Introduce metamorphization as an explicit pass family.

This order matters because:

- hot reload without stable IDs is fake
- actors without safe ownership are unsafe
- reflection without compiler ownership is incomplete
- metamorphization before contracts will become backend sprawl

---

## 15. Repo-Level Implications

### `crates/kain-core`

Needs new modules or major expansion around:

- runtime capability IR
- reflection emission
- actor/message typed IR
- reload compatibility analysis
- migration metadata
- metamorphization passes

### `runtime/native`

Needs new subsystems for:

- scheduler
- actor runtime
- timers
- reload/version manager
- migration runtime
- conformance and stress tests

### `crates/kain-host`

Needs tighter integration with:

- emitted reflection metadata
- reload manifests
- version install APIs
- host-visible runtime services

### `crates/kain-reflect`

Needs to remain stable and boring.

That is a feature, not a limitation.

It should become the durable schema layer that both compiler and host runtimes trust.

---

## 16. Main Risks

### 16.1 Letting the interpreter define the platform

The interpreter is valuable, but it cannot remain the de facto source of truth for runtime semantics.

### 16.2 Letting backends invent their own runtime assumptions

If actor, reload, and reflection behavior drift by backend, Kain stops being one platform.

### 16.3 Treating hot reload as a tooling trick

Hot reload is a compiler-runtime contract problem with migration and compatibility requirements.

### 16.4 Overloading `kain-core`

If `kain-core` starts owning host runtime implementation details, it will become harder to evolve and harder to test.

---

## 17. Final Position

The path forward is not to stuff everything into `kain-core` as implementation.

The correct path is:

- put semantics, contracts, metadata, and transforms in `kain-core`
- put robust execution machinery in `runtime/native`
- keep host embedding in `kain-host`
- keep schemas in `kain-reflect`

That is how Kain gets:

- real actor concurrency
- real hot reload
- real metaprogramming
- real metamorphization
- real runtime robustness

without turning into a pile of disconnected features.
