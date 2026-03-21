# Kain Fabric Tasks

## Overview

This task plan is written for an implementation-focused agent. Follow the phases in order. Do not skip validation. Do not mark a phase complete because a type or command stub exists.

Execution rules:

- reuse existing crates before adding new ones
- keep manifest logic in `kain-omni`
- keep payload contract logic in `kain-interop`
- keep Kain-side compilation and contract emission in `kain-driver`
- keep local session execution in `kain-host`
- keep CLI UX in `crates/cli`
- ship one vertical slice before expanding scope

## Phase 0: Baseline and Schemas

- [ ] 0.1 Create the Fabric tracker doc under `docs/kainplan/kain-fabric/`
  - Record current scope truth: local-only, run/test-heavy, shared buffer and image contracts only
  - _Requirements: KF-3, KF-12_

- [ ] 0.2 Define manifest, lock, event, and report structs
  - Add Fabric schema types under `crates/kain-omni` and `crates/kain-interop` as appropriate
  - Keep file and runtime contract shapes separate but linkable
  - _Requirements: KF-1, KF-2, KF-7_

- [ ] 0.3 Add serialization tests for the new schema types
  - Cover valid manifest parsing, invalid manifests, and deterministic serialization
  - _Requirements: KF-1, KF-7, KF-11_

## Phase 1: Manifest Resolution and CLI Scaffolding

- [ ] 1.1 Add `crates/kain-omni/src/fabric.rs`
  - Parse `KAIN.fabric.toml`
  - Resolve search roots and workspace-relative paths
  - Detect duplicate ids and dependency cycles
  - _Requirements: KF-1, KF-8_

- [ ] 1.2 Add Fabric subcommands to `crates/cli`
  - Implement `kain fabric init`
  - Implement `kain fabric validate`
  - Stub `kain fabric run` with explicit "executor not wired" status if phase 2 is not yet finished
  - _Requirements: KF-10, KF-12_

- [ ] 1.3 Generate starter manifest templates
  - Include a simple local Kain-only template
  - Include a multi-runtime template that names Python, Rust crate, C ABI, and Node steps
  - _Requirements: KF-1, KF-10_

- [ ] 1.4 Add manifest validation tests in `crates/cli` and `crates/kain-omni`
  - Cover init output, validate success, and validate failure
  - _Requirements: KF-8, KF-10, KF-11_

## Phase 2: Capability Registry and Session Planning

- [ ] 2.1 Define Fabric capability keys and versions
  - Add typed capability descriptors in `crates/kain-interop/src/fabric/`
  - Cover local runtimes and payload contract support
  - _Requirements: KF-2, KF-4, KF-5_

- [ ] 2.2 Build a session planner
  - Resolve runtime steps into an execution order
  - Resolve required versus optional capabilities
  - Produce a resolved session plan before execution begins
  - _Requirements: KF-2, KF-3, KF-7, KF-8_

- [ ] 2.3 Emit session lock files
  - Write resolved paths, versions, capabilities, and sidecar references into `.kain/fabric/sessions/<session-id>/manifest.lock.json`
  - _Requirements: KF-7_

- [ ] 2.4 Add planner tests
  - Cover dependency ordering, missing capabilities, optional capability downgrade, and cycle rejection
  - _Requirements: KF-2, KF-8, KF-11_

## Phase 3: Local Executor Core

- [ ] 3.1 Add `crates/kain-host/src/fabric/`
  - Define `FabricSession`, `FabricStepRunner`, and `FabricEvent`
  - Keep execution orchestration local-first
  - _Requirements: KF-3, KF-7, KF-8_

- [ ] 3.2 Implement event emission
  - Emit started, completed, failed, and artifact-written events
  - Serialize events to JSONL during execution
  - _Requirements: KF-7, KF-8_

- [ ] 3.3 Implement the Kain step runner
  - Reuse existing host execution surfaces
  - Reuse `kain-driver` contract emission where needed
  - _Requirements: KF-4, KF-6_

- [ ] 3.4 Add executor-only integration tests
  - Cover a Kain-only manifest with multiple dependent steps
  - _Requirements: KF-3, KF-8, KF-11_

## Phase 4: Runtime Adapters

- [ ] 4.1 Add Python adapter
  - Reuse current Python bridge behavior
  - Report missing module or bridge failures clearly
  - _Requirements: KF-4, KF-8_

- [ ] 4.2 Add Rust crate FFI adapter
  - Reuse current crate FFI resolution and reports where possible
  - Make host-lane-only status explicit
  - _Requirements: KF-4, KF-6, KF-12_

- [ ] 4.3 Add C ABI adapter
  - Reuse current C ABI bridge path
  - Validate required shared library and symbol availability before execution
  - _Requirements: KF-4, KF-8_

- [ ] 4.4 Add Node adapter
  - Reuse current JavaScript bridge path
  - Validate helper module resolution before execution
  - _Requirements: KF-4, KF-8_

- [ ] 4.5 Add adapter coverage tests
  - Cover missing dependency failures and happy-path local runs for each adapter
  - _Requirements: KF-4, KF-8, KF-11_

## Phase 5: Shared Contract Enforcement

- [ ] 5.1 Add Fabric payload binding rules
  - Define how step outputs reference shared buffers and shared images
  - Reject incompatible bindings at validation time
  - _Requirements: KF-5, KF-8_

- [ ] 5.2 Add contract-aware report fields
  - Record payload contract kind, producer step, and consumer step in reports
  - _Requirements: KF-5, KF-7_

- [ ] 5.3 Add negative tests for invalid contract handoff
  - Reject unknown contract kinds and unsupported step bindings
  - _Requirements: KF-5, KF-8, KF-11_

## Phase 6: First Vertical Slice

- [ ] 6.1 Create `smoketest/fabric/quad_prism_fabric/`
  - Base it on the existing `quad_prism_halo` proof
  - Replace ad hoc orchestration with a Fabric manifest and session run
  - _Requirements: KF-9_

- [ ] 6.2 Add generated session artifact assertions
  - Validate report fields, event sequence, and output artifacts
  - _Requirements: KF-7, KF-9, KF-11_

- [ ] 6.3 Add CLI smoke command coverage
  - Prove `kain fabric validate` and `kain fabric run` work on the first Fabric smoke
  - _Requirements: KF-9, KF-10, KF-11_

## Phase 7: Hardening and Product Readiness

- [ ] 7.1 Add human-readable report rendering
  - Keep JSON authoritative, but add a readable CLI summary
  - _Requirements: KF-7, KF-10_

- [ ] 7.2 Add cache safety rules
  - Reuse generated sidecars only when manifest and capability inputs match
  - _Requirements: KF-7_

- [ ] 7.3 Tighten docs and help text
  - Explicitly state that Fabric is local-first and host-lane-first in phase 1
  - _Requirements: KF-3, KF-12_

- [ ] 7.4 Add crate-level validation commands to CI docs
  - Document the exact `cargo test` and smoke commands
  - _Requirements: KF-11_

## Phase 8: Extension Hooks, Not Full New Scope

- [ ] 8.1 Add extension points for future runtime kinds
  - Make the adapter registry extensible without promising remote execution yet
  - _Requirements: KF-2, KF-12_

- [ ] 8.2 Add placeholders for future GPU/native-ui/UE5 consumers
  - Keep them as typed capability and adapter registration slots only
  - Do not claim working orchestration until real vertical slices exist
  - _Requirements: KF-12_

## Completion Standard

Do not mark Kain Fabric complete when:

- the CLI subcommand exists but does not execute real sessions
- the manifest types exist but validation is missing
- adapters compile but do not emit events or reports
- one runtime works but shared contract handoff is still ad hoc

Kain Fabric phase 1 is complete only when the Fabric smoke fixture proves:

- manifest validation
- local session planning
- multi-runtime step execution
- shared contract handoff
- lock file output
- final report output
