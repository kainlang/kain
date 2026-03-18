# Implementation Plan: KAIN Native Runtime Completion

## Overview

This implementation plan completes the native C runtime as a real runtime lane rather than a thin host substrate. It is requirements-first, contract-heavy, and validation-heavy on purpose. The intended executor is an autonomous long-haul agent, so each phase names ownership, code touchpoints, and concrete validation work.

Execution rules for the agent:

- do not skip validation tasks
- do not replace working runtime seams with rewrites unless the replacement preserves current capabilities
- keep compiler, driver, and runtime artifacts in lockstep
- prefer canonical headers, tables, manifests, and schemas over scattered helper additions
- update docs and tests in the same phase that changes runtime behavior

## Tasks

- [ ] 0. Phase 0: Baseline Runtime Audit, Harnesses, and Guardrails
  - [x] 0.1 Create a runtime completion tracker doc
    - Add a progress doc under `runtime/` that mirrors this spec's phases and records implementation status, open issues, and validation status
    - Record the current runtime ABI version, native runtime manifest contents, and known blocking gaps
    - _Requirements: 2.4, 13.1, 14.4_

  - [x] 0.2 Establish native runtime validation commands
    - Document the canonical commands used to validate `kain-core`, `kain-driver`, `kain-sys-codegen`, and native runtime compilation
    - Include at minimum `cargo test` coverage for affected crates plus direct native runtime compilation checks
    - Add a script or doc section for compiling `runtime/kain_runtime.c` with the current manifest and include paths
    - _Requirements: 13.1, 13.5_

  - [x] 0.3 Create native runtime smoke fixtures
    - Add minimal smoke programs/artifacts covering contract startup, realtime bundle startup, UI bundle startup, and native viewport startup
    - Organize them so later phases can reuse the same fixtures instead of inventing new ones
    - _Requirements: 4.2, 8.1, 13.4, 13.5_

  - [x] 0.4 Add a conformance test directory for native runtime behavior
    - Create a stable location for runtime-specific harnesses and ABI parity tests
    - Include README/instructions so future phases extend one harness family instead of scattering ad hoc checks
    - _Requirements: 13.1, 13.6, 14.4_

  - [x] 0.5 Checkpoint - Baseline can be reproduced
    - Prove current runtime compilation still works
    - Prove existing contract/realtime/UI bundle startup paths still work before deeper refactors begin
    - Record failures instead of silently working around them

- [ ] 1. Phase 1: Canonical ABI, Service Tables, and Version Metadata
  - [x] 1.1 Define native runtime ABI versioning
    - Add canonical ABI version constants and runtime version metadata in `runtime/native/include`
    - Expose runtime version/build information programmatically
    - Thread ABI version into startup validation and diagnostics
    - _Requirements: 1.3, 1.5, 2.4, 10.1_

  - [x] 1.2 Introduce canonical runtime service table headers
    - Add headers for diagnostics, service registry, actor ABI, async ABI, reflection ABI, and compatibility APIs
    - Keep declarations centralized under `runtime/native/include`
    - Ensure current core/app/input/viewport/UI services map cleanly into the new model
    - _Requirements: 1.1, 1.2, 14.1_

  - [-] 1.3 Implement a runtime capability/service registry
    - Add table-driven capability and service descriptors in native core sources
    - Replace narrow hardcoded service checks with canonical registry-driven resolution where practical
    - Preserve current `native.app-host`, `native.input`, `native.viewport`, `native.asset.gltf`, and `native.ui.compiled-bundle` service handling
    - _Requirements: 1.2, 1.5, 2.6, 14.1_

  - [~] 1.4 Extend `native_runtime.toml` and related runtime metadata
    - Add explicit runtime ABI/version/service metadata to the manifest or companion metadata
    - Make runtime source/service declaration more transparent and machine-checkable
    - _Requirements: 1.3, 2.4, 14.2_

  - [~] 1.5 Teach CLI/driver startup flows about runtime version metadata
    - Update `crates/cli` and `crates/kain-driver` paths that materialize or resolve native runtime artifacts so runtime version metadata is preserved
    - Ensure bundle output includes the metadata required for startup validation
    - _Requirements: 2.4, 10.1, 10.6_

  - [~] 1.6 Add ABI and startup validation tests
    - Add tests covering runtime version exposure, service registry resolution, and startup mismatch failures
    - Validate required vs optional service reporting
    - _Requirements: 1.5, 2.2, 2.5, 13.1_

- [ ] 2. Phase 2: Structured Diagnostics and Failure Model Hardening
  - [~] 2.1 Add native runtime diagnostic record types
    - Create diagnostic structs/enums for subsystem, code, severity, summary, detail, and source path
    - Expose APIs for collecting and reporting diagnostics during startup and runtime operations
    - _Requirements: 2.1, 2.2, 2.6_

  - [~] 2.2 Replace primitive error paths in native core helpers
    - Audit `runtime/native/src/core/kain_runtime_core.c` and related startup paths for print-only/null-only failures
    - Convert these to explicit diagnostics while preserving call-site compatibility where necessary
    - _Requirements: 2.1, 2.3, 14.6_

  - [~] 2.3 Harden startup validation reports
    - Extend runtime contract validation results to include runtime version, ABI version, subsystem codes, and downgrade information
    - Surface these through viewport/sculpt/app-host startup
    - _Requirements: 2.2, 2.5, 8.6_

  - [~] 2.4 Define stable native runtime error codes
    - Create a documented error code family for contract, reflection, actor, async, UI, graphics, platform, and compatibility failures
    - Add docs and tests so codes stay stable
    - _Requirements: 2.1, 2.3, 14.4_

  - [~] 2.5 Add diagnostics conformance tests
    - Test contract mismatch, missing optional service downgrade, invalid bundle path, invalid JSON/schema, and startup failure diagnostics
    - _Requirements: 2.1, 2.2, 2.5, 13.1_

- [ ] 3. Phase 3: Reflection Payload Emission and Native Runtime Consumption
  - [~] 3.1 Extend `kain-core` runtime contract emission
    - Upgrade `crates/kain-core/src/runtime_contract.rs` so reflection payloads are emitted instead of placeholder-only summaries
    - Add stable schema/version fields and item identity metadata for reflected runtime items
    - _Requirements: 4.1, 4.4, 10.5_

  - [~] 3.2 Introduce compiler-owned reflection artifact structures
    - Add or extend reflection emission modules in `crates/kain-core` and `crates/kain-reflect` as needed
    - Ensure actors, components, messages, services, and host imports can be reflected
    - _Requirements: 4.1, 4.4_

  - [~] 3.3 Extend `kain-driver` native bundle output
    - Update `crates/kain-driver/src/native_app.rs` to write reflection artifacts alongside runtime contract, realtime, and UI bundle outputs
    - Ensure naming and layout are deterministic and documented
    - _Requirements: 4.2, 10.6_

  - [~] 3.4 Add native reflection loader and registry
    - Implement native runtime code that loads reflection payloads, validates schema version, and exposes lookup APIs
    - Keep the parser/loader in core runtime modules, not platform-specific files
    - _Requirements: 4.3, 4.4, 4.5_

  - [~] 3.5 Thread reflection metadata into startup validation
    - Make startup validation aware of reflection presence, schema compatibility, and reflection-driven service requirements
    - _Requirements: 4.3, 4.5, 8.6_

  - [~] 3.6 Add contract/reflection golden tests
    - Add `kain-core` golden tests for emitted JSON
    - Add native runtime tests for loading valid and invalid reflection payloads
    - _Requirements: 4.1, 4.3, 13.3_

- [ ] 4. Phase 4: Low-Level Memory Helper ABI Parity
  - [~] 4.1 Inventory canonical low-level helper requirements
    - Derive the actual helper surface from `crates/kain-core/src/low_level_memory.rs` and `LOW_LEVEL_MEMORY_STATUS.md`
    - Produce an implementation checklist mapping compiler expectations to native helper exports
    - _Requirements: 3.1, 3.4, 14.4_

  - [~] 4.2 Add canonical helper declarations to native headers
    - Define the native helper ABI in headers under `runtime/native/include`
    - Cover address-of, bind-local, load/store, field/index pointer, union, bitfield, and related memory operations
    - _Requirements: 3.1, 3.2, 3.6_

  - [~] 4.3 Implement missing native helper functions
    - Add the helper implementations in native core modules
    - Respect ABI-aware packing, alignment, bit ordering, and ownership behavior
    - _Requirements: 3.2, 3.3, 3.6_

  - [~] 4.4 Align LLVM/runtime helper binding
    - Update `crates/kain-sys-codegen/src/codegen_llvm/mod.rs` so emitted calls target the canonical helper surface
    - Add capability failures for unsupported cases instead of silent divergence
    - _Requirements: 1.4, 3.4, 3.5_

  - [~] 4.5 Improve C++ backend clarity or parity path
    - At minimum, make unsupported helper/runtime areas fail explicitly and document that status
    - Where practical, begin aligning helper names/contracts with the canonical ABI even if full parity remains later
    - _Requirements: 1.4, 3.5, 14.5_

  - [~] 4.6 Add ABI parity and conformance tests
    - Add tests for pointer ops, layout-sensitive operations, unions, bitfields, and load/store helpers
    - Verify emitted LLVM calls match native exports and behavior
    - _Requirements: 3.3, 3.4, 13.1, 13.6_

- [ ] 5. Phase 5: Actor Bootstrap Repair and Minimal Real Actor Runtime
  - [~] 5.1 Replace the `default_actor_run` bootstrap path
    - Audit the current actor emission path in `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
    - Replace the fallback/default wrapper integration with a real actor bootstrap ABI and runtime entrypoint
    - _Requirements: 5.1, 5.6, 13.2_

  - [~] 5.2 Define actor runtime structs and headers
    - Add actor ID, actor state, mailbox, exit reason, supervisor ref, monitor ref, and scheduler queue declarations
    - Document ownership and lifetime rules
    - _Requirements: 5.2, 6.1, 6.3_

  - [~] 5.3 Implement mailbox-backed actor spawn and shutdown
    - Add actor creation, mailbox init, lifecycle transitions, and deterministic cleanup
    - Keep old raw thread helpers available only where still needed as low-level substrate, not as actor semantics
    - _Requirements: 5.2, 5.5, 6.1_

  - [~] 5.4 Add actor identity and typed message metadata plumbing
    - Thread message type tags and actor IDs through send/receive paths
    - Ensure actor bootstrap receives enough metadata to tie runtime behavior back to reflected or compiled identity
    - _Requirements: 5.3, 6.4_

  - [~] 5.5 Surface actor diagnostics and cleanup behavior
    - Emit explicit diagnostics on actor spawn failure, mailbox overflow, invalid message delivery, and actor shutdown
    - _Requirements: 5.4, 5.5, 6.5_

  - [~] 5.6 Add actor bootstrap smoke tests
    - Add native/LLVM actor smokes proving emitted actors run through the correct bootstrap path
    - Add tests for actor exit and mailbox cleanup
    - _Requirements: 5.1, 5.6, 13.2_

- [ ] 6. Phase 6: Full Actor Runtime Semantics
  - [~] 6.1 Add bounded mailbox policy and backpressure
    - Implement capacity-aware mailboxes and explicit push failure/blocking behavior
    - Record overload diagnostics and counters
    - _Requirements: 6.1, 6.5_

  - [~] 6.2 Implement actor registry
    - Add register/lookup/unregister APIs for named actors/services
    - Ensure registry lifetime and cleanup rules are explicit
    - _Requirements: 6.4_

  - [~] 6.3 Implement monitors and links
    - Add monitor/link registration and exit propagation semantics
    - Define exit reason structures and crash-containment behavior
    - _Requirements: 6.3_

  - [~] 6.4 Implement supervision policies
    - Add restart, shutdown, and escalation policies for supervisors and children
    - Ensure restarts are bounded and observable
    - _Requirements: 6.2, 6.3_

  - [~] 6.5 Introduce scheduler policy beyond raw thread-per-actor spawn
    - Add a scheduler queue and fairness rules that can host actor work without unbounded thread explosion
    - Integrate blocking waits and sleeps through scheduler-aware primitives where possible
    - _Requirements: 6.5, 6.6, 7.3_

  - [~] 6.6 Add supervision and monitor tests
    - Test child failure, restart, shutdown, monitored exits, bounded mailbox behavior, and registry cleanup
    - _Requirements: 6.2, 6.3, 6.4, 6.5, 13.2_

- [ ] 7. Phase 7: Native Async, Futures, and Timers
  - [~] 7.1 Define async/task ABI and runtime data structures
    - Add task/future handles, state enums, wake records, and timer records in native headers
    - _Requirements: 7.1, 7.5_

  - [~] 7.2 Implement native task executor and wake/poll machinery
    - Add task spawn, poll, wake, completion, and cancellation APIs
    - Integrate them with the scheduler substrate instead of isolated sleeps/threads
    - _Requirements: 7.1, 7.3, 7.4_

  - [~] 7.3 Add timer services
    - Implement runtime timer registration, cancellation, and wake delivery
    - Ensure timers work cleanly with actor and task scheduling
    - _Requirements: 7.2, 7.3_

  - [~] 7.4 Define async/runtime value ownership rules
    - Document and implement native ownership/lifetime rules for task handles, future results, and cross-boundary values
    - _Requirements: 7.4, 7.5_

  - [~] 7.5 Extend compiler/runtime contracts for async requirements
    - Ensure compiler-emitted capabilities and runtime service bindings can express async/timer requirements
    - _Requirements: 1.2, 7.1, 7.6_

  - [~] 7.6 Add async/timer conformance tests
    - Test wake, cancellation, timer delay, actor/task interop, and completion diagnostics
    - _Requirements: 7.1, 7.2, 7.4, 13.1_

- [ ] 8. Phase 8: UI Runtime and Component Convergence
  - [~] 8.1 Harden compiled bundle validation
    - Expand bundle validation in `runtime/native/src/ui/kain_ui_compiled_bundle.c`
    - Validate node shape, semantic fields, lifecycle metadata, and compatibility versioning
    - _Requirements: 8.1, 8.6_

  - [~] 8.2 Introduce component/runtime state records
    - Add component instance/state/invalidation data structures in native UI runtime
    - Preserve current overlay consumption while enabling richer runtime behavior
    - _Requirements: 8.2_

  - [~] 8.3 Implement focus and event routing
    - Extend app/input/UI integration so input events can route to focused or targeted UI elements
    - _Requirements: 8.3, 8.4_

  - [~] 8.4 Add editable control groundwork
    - Implement text input state and event plumbing required for real controls
    - If full controls are too large for one phase, still land the canonical runtime plumbing and capability checks
    - _Requirements: 8.4, 14.5_

  - [~] 8.5 Validate raw-native vs Rust-native bundle parity
    - Compare interpretation of shared UI bundle contracts across native lanes
    - Add explicit tests where they diverge
    - _Requirements: 8.5_

  - [~] 8.6 Add UI/runtime smoke tests
    - Prove bundle validation, focus routing, redraw/invalidation, and startup capability checks
    - _Requirements: 8.1, 8.2, 8.3, 13.4, 13.5_

- [ ] 9. Phase 9: Shader, Material, and Compute Runtime
  - [~] 9.1 Define runtime-consumable shader/material/compute artifacts
    - Extend compiler/driver artifact schemas so the native runtime can consume modern graphics metadata instead of only narrow realtime summaries
    - _Requirements: 9.1, 9.2_

  - [~] 9.2 Add native artifact loaders and validators
    - Implement native loaders for shader/material/compute artifacts and reflection-driven binding metadata
    - _Requirements: 9.2, 9.3_

  - [~] 9.3 Create a backend contract for graphics execution
    - Either formalize a backend-neutral runtime interface or define explicit backend contracts starting with the current GL lane
    - Remove as much handwritten one-off binding logic as possible
    - _Requirements: 9.3, 9.5_

  - [~] 9.4 Implement material/runtime resource binding
    - Add runtime structures for material instances, parameters, caches, and resource lifetime
    - _Requirements: 9.3, 9.6_

  - [~] 9.5 Implement compute runtime support
    - Add compute pipeline creation, dispatch, synchronization, and diagnostics
    - _Requirements: 9.4_

  - [~] 9.6 Add graphics/runtime smokes
    - Validate artifact loading, binding validation, material parameter wiring, compute dispatch, and compatibility failures
    - _Requirements: 9.2, 9.3, 9.4, 13.5_

- [ ] 10. Phase 10: Hot Reload, Compatibility, and Lifecycle APIs
  - [~] 10.1 Add compatibility metadata emission in compiler/driver lanes
    - Extend runtime contract or companion artifacts with compatibility classes, migration hints, and install/update metadata
    - _Requirements: 10.1, 10.5, 10.6_

  - [~] 10.2 Implement native compatibility validator
    - Add native runtime code to compare bundle/runtime ABI version, compatibility class, and service deltas
    - _Requirements: 10.1, 10.4, 10.5_

  - [~] 10.3 Add install/update/uninstall lifecycle APIs
    - Implement lifecycle operations for native bundles and runtime modules
    - _Requirements: 10.2, 10.6, 11.5_

  - [~] 10.4 Add state transfer and migration hooks
    - Implement migration boundary APIs for actors, tasks, UI/app state, and runtime-owned services
    - _Requirements: 10.3, 10.4_

  - [~] 10.5 Integrate live update rejection rules
    - Reject incompatible hot reloads early with explicit diagnostics
    - _Requirements: 10.2, 10.4_

  - [~] 10.6 Add compatibility and migration tests
    - Test compatible update, incompatible update, missing migration, and startup version mismatch
    - _Requirements: 10.1, 10.2, 10.3, 13.4_

- [ ] 11. Phase 11: Host Bridge, Plugin Bridge, and Foreign Runtime Services
  - [~] 11.1 Define native host service registration ABI
    - Add service registration/discovery APIs for host-provided capabilities
    - Keep the API capability-aware and versioned
    - _Requirements: 11.1, 11.4_

  - [~] 11.2 Add plugin/module ABI validation
    - Validate module ABI version, required services, and lifecycle hooks before activation
    - _Requirements: 11.2, 11.5_

  - [~] 11.3 Define foreign bridge contracts
    - Add canonical contracts for Python/Node/Rust-host or other foreign-service lanes where native runtime interop is intended
    - Focus on marshaling, ownership, failure handling, and capability checks
    - _Requirements: 11.3, 11.4, 11.6_

  - [~] 11.4 Add install/uninstall behavior for runtime modules
    - Reuse the lifecycle APIs from Phase 10 so extensions behave like first-class runtime modules
    - _Requirements: 11.5_

  - [~] 11.5 Add host/plugin bridge tests
    - Validate service registration, missing capability failure, module ABI mismatch, and module removal
    - _Requirements: 11.1, 11.2, 11.4, 11.5_

- [ ] 12. Phase 12: Cross-Platform Runtime Boundaries
  - [~] 12.1 Audit and isolate Win32 assumptions
    - Move platform-neutral logic into core modules and headers
    - Leave Win32-only implementations behind explicit platform service boundaries
    - _Requirements: 12.1, 12.4_

  - [~] 12.2 Define Linux and macOS platform service stubs
    - Add platform directories, headers, capability declarations, and diagnostic stubs
    - Ensure unsupported features fail cleanly rather than through missing symbols
    - _Requirements: 12.2, 12.3, 12.4_

  - [~] 12.3 Make platform availability contract-visible
    - Extend runtime capability metadata so platform-specific service availability is explicit
    - _Requirements: 12.3_

  - [~] 12.4 Add platform boundary tests
    - Validate build-time or startup-time unsupported-platform diagnostics and capability discovery behavior
    - _Requirements: 12.3, 12.4, 12.5_

- [ ] 13. Phase 13: End-to-End Conformance and Repo Hardening
  - [~] 13.1 Add end-to-end native bundle tests
    - Cover native app bundle emission, runtime contract/reflection loading, realtime/UI bundle loading, and startup validation
    - _Requirements: 4.2, 10.6, 13.4_

  - [~] 13.2 Add backend/runtime parity matrix checks
    - Compare `kain-core`, `kain-driver`, `kain-sys-codegen`, and `runtime/native` behavior against the canonical runtime ABI and feature matrix
    - _Requirements: 1.4, 3.4, 13.1, 13.6_

  - [~] 13.3 Update runtime docs and matrices
    - Update `runtime/KAIN_NATIVE_RUNTIME_FEATURE_MATRIX.md`, `docs/KAIN_CORE_RUNTIME_IMPLEMENTATION_MATRIX_2026.md`, `docs/KAIN_CORE_RUNTIME_ROADMAP_2026.md`, and any related docs to reflect the new reality
    - _Requirements: 14.4, 14.5_

  - [~] 13.4 Final checkpoint - native runtime reaches "full lane" status
    - Prove the runtime now has:
      - canonical ABI and service tables
      - structured diagnostics and versioning
      - reflection payload consumption
      - low-level helper parity
      - real actor runtime
      - async/timer runtime
      - richer UI runtime
      - shader/material/compute runtime scaffolding or implementation
      - hot reload/version lifecycle
      - host/plugin bridge
      - explicit cross-platform boundaries
    - Record any remaining non-goals explicitly instead of leaving them ambiguous

- [ ] 14. Ongoing Discipline Tasks
  - [~] 14.1 Update tests alongside every runtime-facing change
    - No runtime ABI, codegen binding, or contract change lands without test updates
    - _Requirements: 13.1, 14.4_

  - [~] 14.2 Keep service/capability additions table-driven
    - New runtime services must be added through canonical registries and manifests
    - _Requirements: 14.1, 14.2, 14.3_

  - [~] 14.3 Preserve existing working flows during refactors
    - Contract loading, realtime loading, UI bundle loading, viewport startup, and glTF support must remain functional throughout the implementation
    - _Requirements: 14.6_
