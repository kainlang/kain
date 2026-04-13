# Kain Fabric Design

## 1. Overview

Kain Fabric is a local-first, typed orchestration layer for heterogeneous software pipelines.

The product goal is not "one more backend." The goal is to let one `.kn` program coordinate Python, Rust crate FFI, C ABI bridges, Node helpers, compiler-owned bundles, and native tool surfaces through one manifest, one contract model, and one execution report.

Phase 1 is intentionally narrow:

- local-first execution only
- host-backed `run` and `test` lanes only
- Python, Rust crate FFI, C, and Node steps only
- shared buffer and shared image contracts only
- reportable, reproducible pipeline runs

This spec does not claim remote scheduling, browser orchestration, UE5 orchestration, or universal distributed execution already exist.

## 2. Evidence Inventory

The design is grounded in these current repo realities:

- [`M:\Code\Kain\README.md`](M:\Code\Kain\README.md)
  - documents the layered architecture, active bridge lanes, bundle lanes, and mixed-runtime smoke matrix
- [`M:\Code\Kain\crates\kain-driver\src\lib.rs`](M:\Code\Kain\crates\kain-driver\src\lib.rs)
  - owns frontend orchestration, runtime contract bundle emission, realtime app bundle emission, GPU artifact emission, native app materialization, and bridge registration
- [`M:\Code\Kain\crates\kain-interop\src\lib.rs`](M:\Code\Kain\crates\kain-interop\src\lib.rs)
  - already provides canonical shared buffer and shared image contracts with metadata, ownership, and mutation APIs
- [`M:\Code\Kain\crates\kain-omni\src\lib.rs`](M:\Code\Kain\crates\kain-omni\src\lib.rs)
  - already provides a data-driven manifest for staged imports and multi-target outputs
- [`M:\Code\Kain\crates\kain-host\src\lib.rs`](M:\Code\Kain\crates\kain-host\src\lib.rs)
  - already provides the native Rust host runtime and host-side value conversion surface
- [`M:\Code\Kain\crates\kain-sdk\src\lib.rs`](M:\Code\Kain\crates\kain-sdk\src\lib.rs)
  - already provides the high-level embeddable engine facade
- [`M:\Code\Kain\smoketest\py_cargo_node_c\quad_prism_halo\smoke.kn`](M:\Code\Kain\smoketest\py_cargo_node_c\quad_prism_halo\smoke.kn)
  - proves one `.kn` program can already move one image payload through Python, Kain, C, Rust crate logic, and Node packaging
- [`M:\Code\Kain\docs\KAIN_RUNTIME_UNIFICATION_DOCTRINE.md`](M:\Code\Kain\docs\KAIN_RUNTIME_UNIFICATION_DOCTRINE.md)
  - establishes the "one semantic truth, one bundle truth, one capability truth" doctrine
- [`M:\Code\Kain\docs\KAIN_2026_EXECUTION_PLATFORM_BLUEPRINT.md`](M:\Code\Kain\docs\KAIN_2026_EXECUTION_PLATFORM_BLUEPRINT.md)
  - establishes the bundle-first, interop-first, data-driven-first platform direction
- [`M:\Code\Kain\labs\native_ui_viewport_smoke\README.md`](M:\Code\Kain\labs\native_ui_viewport_smoke\README.md)
  - proves Kain already has a real native tool/runtime lane worth targeting later as a Fabric consumer

## 3. Current Reality

### 3.1 What already exists

- `kain-core` owns language semantics, effects, actors, macros, comptime, and runtime contract emission.
- `kain-driver` already compiles runtime contract bundles, realtime app bundles, GPU bundles, and native app bundles.
- `kain-interop` already has stable host objects for shared buffers and shared images.
- `kain-omni` already has a TOML manifest and staging flow for Kain, Rust, TypeScript, C, and assembly imports.
- `kain-host` and `kain-sdk` already provide host execution and embedding surfaces.
- The CLI already exposes `omni`, `run`, `import-c`, `import-rust`, `import-crate`, `import-ts`, `gpu-artifacts`, and `build native-ui`.

### 3.2 What does not exist yet

- no canonical Fabric manifest
- no Fabric-specific capability registry
- no pipeline executor that schedules typed runtime steps
- no session lock or provenance report for mixed-runtime runs
- no first-class CLI command family for Fabric
- no explicit placement, dependency, or lifecycle model for runtime steps
- no stable event/diagnostic stream for mixed-runtime orchestration

### 3.3 Constraints that must be preserved

- bridge-heavy features are strongest in host-backed `run` and `test` lanes today
- bundle truth must remain compiler-owned, not host-reparsed ad hoc source
- capabilities must stay data-driven, not scattered into stringly runtime checks
- native UI, realtime, and UE5 lanes must remain consumers of shared contracts, not separate truth systems

## 4. Design Goals

1. Make Kain the fastest path to build local heterogeneous pipelines without hand-written glue code.
2. Reuse the current runtime contract, host, bridge, and manifest architecture instead of creating a parallel subsystem.
3. Keep Fabric manifest-driven and capability-driven from day one.
4. Ship one undeniable vertical slice: Python -> Kain -> C -> Rust -> Node image pipeline with repeatable reports.
5. Leave room for later GPU, browser, UE5, and native tool orchestration without promising them in phase 1.

## 5. Architecture

### 5.1 Canonical ownership

| Layer | Owner | Responsibility | Must not own |
|---|---|---|---|
| Fabric semantic declarations | `crates/kain-core` | Future syntax or annotations, if added later | runtime scheduling or host-specific step execution |
| Fabric manifest schema and resolution | `crates/kain-omni` | `KAIN.fabric.toml`, step graph parsing, validation, import resolution reuse | bridge-specific payload logic |
| Fabric contract types | `crates/kain-interop` | shared payload contracts, capability descriptors, session contract metadata | CLI flow or manifest parsing |
| Fabric compilation and bundle emission | `crates/kain-driver` | compile runtime contracts, attach capability requirements, emit session sidecars | scheduling policy |
| Fabric local executor | `crates/kain-host` | instantiate local runtimes, execute steps, enforce dependencies, collect diagnostics | syntax parsing for manifests |
| Fabric CLI | `crates/cli` | `kain fabric init`, `validate`, `run`, report UX | canonical contract types |
| Smoke fixtures and proofs | `smoketest/fabric/*` | cross-runtime proof pipelines and regressions | production runtime logic |

### 5.2 Preferred module layout

This spec prefers extending existing crates instead of adding many new crates.

Recommended additions:

- `crates/kain-omni/src/fabric.rs`
- `crates/kain-interop/src/fabric/`
- `crates/kain-driver/src/fabric.rs`
- `crates/kain-host/src/fabric/`
- `crates/cli/src/fabric.rs`

Avoid creating a brand new god crate for everything.

### 5.3 Core domain entities

- `FabricManifest`
  - canonical TOML root for a pipeline
- `FabricStep`
  - one declared runtime step
- `FabricRuntimeKind`
  - `kain`, `python`, `rust_crate`, `c_abi`, `node`
- `FabricCapabilityRequirement`
  - declared capability keys with version and optionality
- `FabricArtifactRef`
  - reference to a file, generated sidecar, or compiled bundle
- `FabricContractRef`
  - reference to a shared payload contract
- `FabricSession`
  - one concrete resolved execution run
- `FabricEvent`
  - typed lifecycle or diagnostic event
- `FabricReport`
  - final execution report with provenance, durations, outputs, and failures

## 6. Data Flow

1. CLI loads `KAIN.fabric.toml`.
2. `kain-omni` resolves imports, staged sources, and referenced entry files.
3. `kain-driver` compiles Kain-side runtime contracts for any Kain-authored steps.
4. The Fabric resolver computes a local execution graph and capability requirements.
5. `kain-host` creates a `FabricSession` and initializes local adapters for Python, Rust crate FFI, C ABI, Node, and Kain host execution.
6. Steps exchange payloads only through declared value outputs or `kain-interop` contract handles.
7. The executor emits structured `FabricEvent` records as each step starts, completes, fails, or emits artifacts.
8. The session writes a lock file and final report.

## 7. Storage Design

Phase 1 storage is file-based and workspace-local.

### 7.1 Manifest

- `KAIN.fabric.toml`

Purpose:

- declare steps
- declare dependencies
- declare required capabilities
- declare inputs and output bindings
- declare report destination

### 7.2 Session output

- `.kain/fabric/sessions/<session-id>/manifest.lock.json`
- `.kain/fabric/sessions/<session-id>/events.jsonl`
- `.kain/fabric/sessions/<session-id>/report.json`
- `.kain/fabric/cache/` for reusable generated sidecars when safe

### 7.3 Lock file

The lock file must capture:

- resolved manifest version
- resolved file paths
- resolved crate manifests or crate paths
- resolved runtime capability set
- runtime ABI and contract schema versions
- generated sidecar paths

## 8. Interfaces

### 8.1 CLI surface

Phase 1 CLI family:

- `kain fabric init`
- `kain fabric validate`
- `kain fabric run`

Optional later additions:

- `kain fabric inspect`
- `kain fabric doctor`

### 8.2 Manifest shape

The manifest must be schema-first and data-driven.

It should declare:

- workspace root
- search roots
- step ids
- step runtime kind
- step input bindings
- step output bindings
- dependency edges
- capability requirements
- report destinations

Do not encode step resolution through implicit filename conventions alone.

### 8.3 Contract surface

Phase 1 canonical payload contracts:

- shared buffer
- shared image
- plain scalar or struct values that already round-trip through host execution

Do not add a second parallel payload model for Fabric.

## 9. Dependency Direction Rules

- `cli` may depend on `kain-omni`, `kain-driver`, and `kain-host`.
- `kain-host` may depend on `kain-interop` and driver-emitted contracts.
- `kain-driver` may depend on `kain-core`, existing backend crates, and `kain-interop`.
- `kain-interop` must not depend on `cli`.
- `kain-omni` must remain manifest-focused and must not absorb host execution logic.

## 10. Migration Map

| Current path | Fabric role |
|---|---|
| `crates/kain-omni/src/lib.rs` | base manifest parsing, import staging, path resolution |
| `crates/kain-driver/src/lib.rs` | contract and bundle emission |
| `crates/kain-interop/src/lib.rs` | canonical shared payload contracts |
| `crates/kain-host/src/lib.rs` | local session execution and host value conversion |
| `smoketest/py_cargo_node_c/quad_prism_halo/smoke.kn` | first vertical-slice proof source |
| `docs/KAIN_RUNTIME_UNIFICATION_DOCTRINE.md` | anti-fragmentation rules |

## 11. Testing Architecture

### 11.1 Required test layers

- unit tests for manifest parsing and schema validation
- unit tests for capability matching and dependency ordering
- unit tests for event and report serialization
- integration tests for local Fabric runs
- smoke tests for the first end-to-end image pipeline

### 11.2 Validation commands

At minimum, the spec expects:

- `cargo test -p kain-omni`
- `cargo test -p kain-interop`
- `cargo test -p kain-driver`
- `cargo test -p kain-host`
- `cargo test -p cli`
- one end-to-end `kain fabric run` smoke fixture

## 12. Implementation Strategy

### Phase 0

Define the manifest, report, and event schemas. Add fixtures. Do not claim execution support yet.

### Phase 1

Add local-only Fabric CLI and manifest validation. Reuse `kain-omni` resolution rules.

### Phase 2

Add the local executor in `kain-host` with dependency ordering and typed events.

### Phase 3

Wire Python, Rust crate FFI, C ABI, Node, and Kain host steps into the executor.

### Phase 4

Ship the first undeniable vertical slice around shared image pipelines and prove report reproducibility.

### Phase 5

Harden diagnostics, provenance, cache behavior, and extension points for later GPU/native-ui consumption.

## 13. Anti-Goals

- Do not market phase 1 as a distributed cloud runtime.
- Do not invent a second contract model separate from `kain-interop`.
- Do not reparse source in runtime hosts as the normal execution path.
- Do not turn `kain-omni` into a god object that owns compilation, execution, reporting, and bridge internals.
- Do not promise browser, UE5, or remote host orchestration before local-first Fabric proves itself.
- Do not hardcode runtime capability checks in scattered string branches when they can live in one typed registry.

## 14. Success Criteria

- A user can declare a local polyglot pipeline in `KAIN.fabric.toml`.
- `kain fabric validate` reports graph and capability issues before execution.
- `kain fabric run` executes a Python + Kain + C + Rust + Node pipeline locally with structured events.
- Payload transfer uses canonical interop contracts instead of ad hoc glue.
- The run emits a lock file and final report with enough provenance to reproduce the result.
- The first smoke fixture is strong enough to replace "trust me" demos with a durable regression test.
