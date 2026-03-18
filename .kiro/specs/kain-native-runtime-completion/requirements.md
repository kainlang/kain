# Requirements Document: KAIN Native Runtime Completion

## Introduction

KAIN already has a meaningful raw-native lane in `runtime/native`, but it is still a substrate rather than a full implementation of the broader runtime vision exposed by `kain-core`. The current native lane can host Win32 apps, ingest runtime contracts and realtime bundles, render viewport scenes, load glTF assets, and consume compiled UI bundles. It does not yet provide end-to-end guarantees for actor semantics, async execution, reflection-backed runtime services, low-level helper ABI parity, hot reload compatibility, modern shader/material/compute execution, or cross-platform runtime parity.

This specification defines the work required to turn the native C runtime into a complete runtime lane for compiled Kain programs. It is intentionally implementation-heavy and validation-heavy so it can be executed by an autonomous long-haul agent with minimal ambiguity.

## Glossary

- **Native Runtime**: The raw C runtime implemented under `runtime/native`
- **Runtime Contract Bundle**: Compiler-emitted metadata describing runtime capabilities, services, and runtime-significant items
- **Reflection Payload**: Compiler-emitted schema/type metadata consumed by runtimes and hosts
- **Service Table**: Canonical runtime ABI for host services such as allocation, actor runtime, timers, reflection, UI, and graphics
- **Actor Runtime**: Scheduler, mailboxes, lifecycle, supervision, monitoring, registry, and crash semantics for actor-backed execution
- **Async Runtime**: Futures, tasks, timers, wake/poll integration, cancellation, and effect-aware scheduling
- **ABI Parity**: Consistent helper/runtime behavior across interpreter, LLVM, C++, Rust-hosted, and raw-native lanes
- **Compiled UI Bundle**: Compiler/driver-emitted UI projection payload consumed by raw-native or Rust-native UI hosts
- **Realtime Bundle**: Compiler/driver-emitted scene/runtime payload used by raw-native viewport/sculpt hosts
- **Hot Reload Compatibility**: Runtime model for versioning, compatibility classes, migration hooks, and live state transfer

## Requirements

### Requirement 1: Canonical Native Runtime ABI

**User Story:** As a KAIN runtime maintainer, I want one canonical runtime ABI and service model for the native lane, so that compiler output, runtime code, and host integrations do not drift.

#### Acceptance Criteria

1. WHEN the native runtime exports execution services, THEN the System SHALL define them in canonical headers under `runtime/native/include` rather than ad hoc scattered declarations
2. WHEN a compiled Kain program targets the raw native lane, THEN the System SHALL expose a stable service table for allocation, retain/release, actor services, timers, reflection, diagnostics, filesystem, networking, UI, and graphics services
3. WHEN ABI-significant structs are introduced, THEN the System SHALL document layout, version, and ownership rules in headers and runtime docs
4. WHEN LLVM or C++ codegen binds runtime helpers, THEN the System SHALL bind through the same canonical ABI contract used by the C runtime
5. WHEN runtime services evolve, THEN the System SHALL version the ABI and preserve compatibility behavior or fail with explicit startup diagnostics
6. WHEN unsupported services are requested, THEN the System SHALL return structured runtime capability failures rather than undefined behavior

### Requirement 2: Structured Diagnostics, Error Codes, and Runtime Versioning

**User Story:** As a KAIN developer or host integrator, I want the native runtime to fail with structured diagnostics and explicit version information, so that breakage is debuggable and upgrade-safe.

#### Acceptance Criteria

1. WHEN any native runtime subsystem fails, THEN the System SHALL emit subsystem-specific diagnostics with stable error codes
2. WHEN startup contract validation fails, THEN the System SHALL report required services, optional downgrades, runtime version, and contract source in a structured format
3. WHEN low-level helpers fail due to invalid arguments, THEN the System SHALL return explicit diagnostics rather than null-only or print-only failure paths
4. WHEN the runtime binary or static runtime lane is built, THEN the System SHALL embed a runtime version, ABI version, and build identifier
5. WHEN a program bundle targets a different ABI or incompatible runtime version, THEN the System SHALL fail before app startup with a compatibility diagnostic
6. WHEN runtime services are downgraded or unavailable, THEN the System SHALL expose downgrade information programmatically and in logs

### Requirement 3: Low-Level Memory Helper Parity

**User Story:** As a KAIN compiler/backend maintainer, I want the native runtime to provide the canonical low-level helper surface implied by `kain-core`, so that raw-memory features behave consistently across targets.

#### Acceptance Criteria

1. WHEN `kain-core` lowers raw memory operations, THEN the native runtime SHALL provide the corresponding canonical `__kain_*` helper ABI expected by backends
2. WHEN address-of, bind-local, field pointer, index pointer, union, bitfield, or memory load/store operations are emitted, THEN the native runtime SHALL expose validated helper implementations for those operations
3. WHEN memory layout depends on ABI policy, THEN the System SHALL respect target-specific packing, alignment, and bit ordering rules
4. WHEN memory helper behavior differs between backends, THEN the System SHALL add conformance tests and align behavior to the canonical ABI
5. WHEN unsupported low-level behavior is requested in a backend, THEN the System SHALL fail in validation rather than produce silent divergence
6. WHEN pointer and allocation helpers are used, THEN the System SHALL define ownership, aliasing, and reallocation behavior explicitly

### Requirement 4: Reflection and Runtime Contract Materialization

**User Story:** As a KAIN runtime and tooling maintainer, I want reflection payloads and runtime contracts to be fully materialized and consumable by the native runtime, so that runtime services can be metadata-driven instead of hardcoded.

#### Acceptance Criteria

1. WHEN `kain-core` emits a runtime contract bundle, THEN the System SHALL emit reflection payloads instead of placeholder-only reflection summaries
2. WHEN the driver packages a native app, THEN the System SHALL materialize runtime contract, reflection payload, UI bundle, and realtime bundle artifacts together
3. WHEN the native runtime loads a contract bundle, THEN the System SHALL parse and validate schema version, runtime service bindings, reflected types, and compatibility metadata
4. WHEN reflected types are available, THEN the System SHALL expose runtime lookup APIs for schemas, item identities, messages, components, and services
5. WHEN reflection payloads are invalid or incomplete, THEN the System SHALL fail with explicit diagnostics rather than partially initialize silently
6. WHEN runtime services depend on reflected metadata, THEN the System SHALL consume compiler-emitted metadata rather than handwritten assumptions

### Requirement 5: Correct Actor Bootstrap and Execution

**User Story:** As a KAIN developer using actor syntax, I want actor-backed programs to execute correctly on the native lane, so that actor semantics are real rather than conceptual.

#### Acceptance Criteria

1. WHEN LLVM emits actor-backed programs, THEN the System SHALL bootstrap actors through the emitted actor entrypoint rather than the current default wrapper path
2. WHEN actors are spawned, THEN the System SHALL create actor state, mailbox ownership, and lifecycle metadata rather than only launching raw OS threads
3. WHEN actor messages are delivered, THEN the System SHALL preserve message typing metadata and actor identity
4. WHEN actor initialization fails, THEN the System SHALL isolate the failure and report structured diagnostics
5. WHEN actor execution completes or exits, THEN the System SHALL perform mailbox, handle, and resource cleanup deterministically
6. WHEN actor code is compiled for the native lane, THEN the runtime SHALL provide a stable actor bootstrap ABI for codegen to target

### Requirement 6: Full Actor Runtime Semantics

**User Story:** As a KAIN developer relying on the advertised actor model, I want supervision, monitoring, registry, backpressure, and lifecycle semantics, so that actors are production-usable.

#### Acceptance Criteria

1. WHEN actors exchange messages, THEN the System SHALL support mailbox operations with explicit ownership, capacity, and backpressure behavior
2. WHEN a supervisor manages child actors, THEN the System SHALL support restart, shutdown, and escalation policies
3. WHEN actors are linked or monitored, THEN the System SHALL propagate exit reasons according to defined monitor/link semantics
4. WHEN named actors or services are registered, THEN the System SHALL provide a runtime actor registry with lookup and deregistration behavior
5. WHEN actor systems are overloaded, THEN the System SHALL expose bounded queue behavior and diagnostic counters
6. WHEN the runtime schedules actors, THEN it SHALL implement fairness and blocking rules that do not rely on unbounded thread spawning
7. WHEN actor semantics are unsupported on a given target or host mode, THEN the System SHALL fail with a clear capability error

### Requirement 7: Native Async, Futures, and Timer Runtime

**User Story:** As a KAIN developer using async-style language features, I want the native lane to provide a real async executor with timers and cancellation, so that async semantics are not interpreter-only.

#### Acceptance Criteria

1. WHEN async tasks are emitted for the native lane, THEN the System SHALL provide task creation, poll, wake, completion, and cancellation semantics
2. WHEN futures depend on timers or delayed work, THEN the runtime SHALL expose timer registration and wake integration
3. WHEN async work blocks on actor or host services, THEN the runtime SHALL integrate those waits into the scheduler rather than forcing ad hoc sleeps
4. WHEN async tasks fail, THEN the System SHALL surface task diagnostics and cleanup semantics
5. WHEN async values cross runtime boundaries, THEN the System SHALL define canonical native representations and ownership rules
6. WHEN async/runtime parity tests run, THEN native behavior SHALL match documented semantics from compiler/runtime contracts

### Requirement 8: Full Native UI and Component Runtime Convergence

**User Story:** As a KAIN app/tool developer, I want the native runtime to move from compiled-overlay scaffolding to a real runtime-backed UI/component lane, so that native tools can host meaningful interactive applications.

#### Acceptance Criteria

1. WHEN compiled UI bundles are loaded, THEN the System SHALL validate bundle shape, semantic node metadata, and lifecycle compatibility
2. WHEN component state changes, THEN the native runtime SHALL support state propagation, invalidation, and redraw behavior
3. WHEN input events occur, THEN the System SHALL support focus, routing, and event dispatch beyond overlay-only drawing
4. WHEN text-input or editable controls are introduced, THEN the runtime SHALL provide state and event plumbing for editing semantics
5. WHEN Rust-native and raw-native UI consume the same bundle family, THEN the System SHALL validate parity of contract interpretation
6. WHEN UI runtime capabilities are unavailable, THEN startup SHALL fail or downgrade explicitly according to contract rules

### Requirement 9: Modern Native Shader, Material, and Compute Runtime

**User Story:** As a KAIN graphics/runtime developer, I want the native lane to support shader artifacts, materials, and compute execution, so that the raw native runtime can credibly host modern Kain graphics programs.

#### Acceptance Criteria

1. WHEN shader or material items are present in a program, THEN the driver SHALL emit runtime-consumable shader/material metadata and artifacts
2. WHEN the native runtime loads shader artifacts, THEN it SHALL validate artifact format, reflection metadata, and target compatibility
3. WHEN resource bindings are created, THEN the runtime SHALL use reflection-driven binding metadata rather than handwritten assumptions
4. WHEN compute workloads are emitted, THEN the native runtime SHALL expose pipeline creation, dispatch, synchronization, and validation hooks
5. WHEN graphics backends differ by platform, THEN the System SHALL define explicit backend contracts or a backend-neutral abstraction
6. WHEN shader or material hot reload occurs, THEN the runtime SHALL validate compatibility before applying the updated artifact

### Requirement 10: Hot Reload, Compatibility Classes, and State Migration

**User Story:** As a KAIN runtime maintainer, I want the native lane to understand versioning, compatibility classes, and state migration, so that live iteration and portable app bundles remain stable across runtime updates.

#### Acceptance Criteria

1. WHEN a program bundle is installed or updated, THEN the runtime SHALL compare bundle compatibility metadata against the active runtime version and ABI version
2. WHEN a hot reload or live patch is applied, THEN the runtime SHALL validate compatibility class, migration requirements, and service availability before activation
3. WHEN runtime state must be preserved across reload, THEN the System SHALL support state snapshot/transfer hooks for actors, tasks, and UI/app state
4. WHEN a migration is unsupported, THEN the System SHALL reject the update with explicit diagnostics
5. WHEN the runtime changes in a breaking way, THEN compatibility metadata SHALL make that break explicit at startup and update time
6. WHEN native bundles are materialized by the driver, THEN they SHALL include the metadata required for version/install/uninstall/update lifecycle decisions

### Requirement 11: Host Bridge, Plugin Bridge, and Foreign Runtime Parity

**User Story:** As a KAIN platform developer, I want the native runtime to expose a real host/plugin bridge and foreign-service boundary, so that raw-native programs can participate in the same broader runtime ecosystem as other Kain lanes.

#### Acceptance Criteria

1. WHEN host services are registered, THEN the native runtime SHALL expose a capability-aware service registration API
2. WHEN native extensions or plugins are loaded, THEN the System SHALL validate ABI version, service requirements, and ownership rules
3. WHEN Python, Node, Rust-host, or other foreign bridges are used from the native lane, THEN the System SHALL define marshaling, error handling, and lifetime rules explicitly
4. WHEN host services are unavailable, THEN the runtime SHALL report capability failures instead of silently degrading
5. WHEN runtime modules are loaded dynamically, THEN the System SHALL support install/uninstall lifecycle APIs
6. WHEN bridge or plugin behavior differs across hosts, THEN those differences SHALL be surfaced through contract metadata rather than hidden assumptions

### Requirement 12: Cross-Platform Native Runtime Parity

**User Story:** As a KAIN platform maintainer, I want the native runtime architecture to be portable beyond Win32, so that the runtime is not structurally trapped in a Windows-only design.

#### Acceptance Criteria

1. WHEN platform-specific runtime services are implemented, THEN the System SHALL isolate them behind headers and service boundaries rather than spreading Win32 assumptions across all modules
2. WHEN Linux or macOS support is added, THEN the System SHALL provide platform-equivalent app host, input, timing, socket, and graphics service boundaries
3. WHEN a runtime capability is platform-specific, THEN the contract SHALL advertise that explicitly
4. WHEN unsupported platforms build the runtime, THEN the System SHALL fail with clear capability diagnostics rather than incomplete symbol breakage
5. WHEN platform parity tests run, THEN they SHALL verify equivalent behavior for capability discovery, startup validation, and core runtime services

### Requirement 13: Runtime Conformance and Smoke Validation

**User Story:** As a KAIN maintainer, I want backend/runtime conformance tests and native smoke coverage, so that runtime drift is caught immediately instead of after large feature landings.

#### Acceptance Criteria

1. WHEN runtime helper behavior is changed, THEN the System SHALL add or update conformance tests in the owning crate or runtime harness
2. WHEN actor/runtime ABI changes, THEN the System SHALL validate LLVM/native integration with dedicated actor smokes
3. WHEN contract/reflection changes, THEN the System SHALL add golden tests for emitted artifacts and native consumers
4. WHEN native app bundling changes, THEN the System SHALL validate emitted bundle contents and runtime startup compatibility
5. WHEN platform host/runtime changes land, THEN the System SHALL provide native smoke programs that exercise startup, input, bundle loading, and shutdown
6. WHEN CI or manual validation runs, THEN the runtime SHALL prove canonical helper parity, startup validation, actor behavior, and bundle compatibility

### Requirement 14: Non-Breaking, Data-Driven Expansion

**User Story:** As a solo maintainer shipping KAIN rapidly, I want native runtime growth to stay data-driven and expansion-friendly, so that the runtime does not calcify around hardcoded one-off behavior.

#### Acceptance Criteria

1. WHEN new runtime services are added, THEN the System SHALL register them through data-driven capability/service tables rather than scattered string checks
2. WHEN runtime artifacts evolve, THEN schema versioning and compatibility metadata SHALL be updated in one canonical place
3. WHEN new platform or host lanes are added, THEN the System SHALL integrate them through explicit contracts instead of forked ad hoc runtime logic
4. WHEN docs and code disagree, THEN the implementation SHALL update the docs in the same change
5. WHEN an implementation shortcut is taken, THEN the System SHALL document it as a temporary limitation and protect it with diagnostics rather than implying completeness
6. WHEN the native runtime expands, THEN existing working viewport, asset, contract, and UI bundle flows SHALL remain intact unless explicitly superseded by compatible replacements
