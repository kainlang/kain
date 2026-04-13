# Kain Fabric Requirements

## KF-1 Manifest Schema

**User story**

As a Kain user building a mixed-runtime pipeline, I want one canonical manifest so I can declare steps, dependencies, capabilities, and outputs without writing custom glue code.

**Acceptance criteria**

- THE system SHALL define a canonical `KAIN.fabric.toml` schema.
- THE manifest SHALL support step identifiers, runtime kinds, dependency edges, input bindings, output bindings, and report destinations.
- THE manifest SHALL support explicit search roots and workspace-relative paths.
- THE manifest SHALL reject duplicate step identifiers and dependency cycles.
- THE manifest SHALL NOT rely on implicit filename conventions as the only source of truth.

## KF-2 Capability Registry

**User story**

As a Fabric executor, I want capabilities represented as typed data so runtime compatibility is checked before execution starts.

**Acceptance criteria**

- THE system SHALL represent Fabric capabilities as structured data with stable keys and versions.
- WHEN a manifest requires a capability that is unavailable locally, `kain fabric validate` SHALL fail before execution.
- THE system SHALL distinguish required capabilities from optional capabilities.
- THE system SHALL NOT scatter capability checks across ad hoc string comparisons in unrelated modules.

## KF-3 Local-First Execution Model

**User story**

As a developer, I want a reliable local-first Fabric runtime before any remote or distributed claims are made.

**Acceptance criteria**

- THE first implementation SHALL support local execution only.
- THE first implementation SHALL target host-backed `run` and `test` execution lanes only.
- THE system SHALL NOT require remote transport, remote workers, or network scheduling in phase 1.
- THE system SHALL make this scope explicit in docs and CLI help.

## KF-4 Step Runtime Adapters

**User story**

As a pipeline author, I want Fabric to run Kain, Python, Rust crate FFI, C ABI, and Node steps through one session model.

**Acceptance criteria**

- THE system SHALL define runtime adapters for `kain`, `python`, `rust_crate`, `c_abi`, and `node`.
- EACH adapter SHALL declare the capability keys it satisfies.
- EACH adapter SHALL emit structured step lifecycle events.
- THE system SHALL fail clearly when a manifest references a runtime kind with no registered adapter.

## KF-5 Shared Contract Reuse

**User story**

As the platform owner, I want Fabric to reuse Kain's existing interop contracts so payload movement stays canonical.

**Acceptance criteria**

- THE system SHALL use `kain-interop` contract families as the canonical payload exchange surface.
- PHASE 1 SHALL support shared buffer and shared image contracts.
- WHEN a step emits a shared payload, downstream steps SHALL consume a contract reference rather than a runtime-specific private object.
- THE system SHALL NOT introduce a second incompatible payload schema for Fabric.

## KF-6 Driver and Bundle Alignment

**User story**

As a compiler/runtime maintainer, I want Fabric to stay aligned with compiler-owned bundles and runtime contracts.

**Acceptance criteria**

- THE system SHALL reuse `kain-driver` for Kain-side compilation and contract emission.
- Kain-authored steps SHALL preserve runtime contract metadata in Fabric session outputs.
- THE system SHALL distinguish current runtime-hosted bridge behavior from offline codegen behavior.
- THE runtime SHALL NOT treat source reparsing as the normal execution truth when compiler-owned bundles or contracts already exist.

## KF-7 Session Lock and Provenance

**User story**

As a developer debugging a pipeline, I want a reproducible session lock and report so I can see exactly what ran and why.

**Acceptance criteria**

- EACH Fabric run SHALL emit a session lock file.
- THE lock file SHALL record resolved paths, runtime kinds, capability set, schema versions, and generated sidecars.
- EACH Fabric run SHALL emit a final report with step durations, outputs, and failures.
- THE system SHALL support machine-readable event output during execution.

## KF-8 Diagnostics and Failure Model

**User story**

As a user, I want clear failures when a pipeline graph, capability set, or runtime step is invalid.

**Acceptance criteria**

- `kain fabric validate` SHALL report manifest, graph, and capability errors without executing steps.
- `kain fabric run` SHALL report step failures with step id, runtime kind, and failure summary.
- THE system SHALL preserve the difference between validation failures and runtime failures.
- THE system SHALL NOT silently skip failed required steps.

## KF-9 First Vertical Slice

**User story**

As the repo owner, I want one undeniable Fabric proof so the architecture is real, not just scaffolded.

**Acceptance criteria**

- THE implementation SHALL ship one end-to-end smoke fixture under `smoketest/fabric/`.
- THE first smoke SHALL cover Python, Kain, C ABI, Rust crate FFI, and Node in one declared session.
- THE first smoke SHALL exercise shared image or shared buffer transfer across multiple step kinds.
- THE first smoke SHALL produce a stable report artifact and validate expected invariants.

## KF-10 CLI Surface

**User story**

As a user, I want a simple command family so Fabric feels first-class instead of hidden behind internal scripts.

**Acceptance criteria**

- THE CLI SHALL expose `kain fabric init`, `kain fabric validate`, and `kain fabric run`.
- `kain fabric init` SHALL generate a starter manifest and example step layout.
- `kain fabric validate` SHALL succeed without executing runtime steps.
- `kain fabric run` SHALL execute the resolved session and write session artifacts.

## KF-11 Testing and CI

**User story**

As a maintainer, I want Fabric to be guarded by real validation commands so regressions are visible.

**Acceptance criteria**

- THE implementation SHALL add unit tests for manifest parsing and graph validation.
- THE implementation SHALL add unit tests for capability matching and session report serialization.
- THE implementation SHALL add integration coverage for local Fabric execution.
- CI SHALL include `cargo test` for all touched crates plus at least one Fabric smoke run.

## KF-12 Explicit Non-Requirements

**User story**

As the architect, I want the first implementation to avoid overclaiming.

**Acceptance criteria**

- PHASE 1 SHALL NOT claim distributed scheduling.
- PHASE 1 SHALL NOT claim browser orchestration.
- PHASE 1 SHALL NOT claim UE5 orchestration.
- PHASE 1 SHALL NOT claim arbitrary offline codegen parity for bridge-hosted features.
- Docs and task status SHALL describe scaffolding as scaffolding, not as completed platform capability.
