# Implementation Plan: KSculpt And KPainter Parity

**Spec Type:** full  
**Slug:** `ksculpt-kpainter-parity`  
**Created:** 2026-04-14

## Overview

Execute the work as a hybrid risk-first program:

- stabilize and harden the shared native authoring/runtime foundation first,
- converge sculpt and painter onto one shared DCC shell and session contract,
- land feature-deep parity through vertical slices inside the flagship native
  app,
- prove every parity claim through a reference-backed capability matrix and
  scenario harness.

## Sequencing Strategy

- Primary strategy: `hybrid`
- Major dependencies:
  `kain native-ui dev` stability, session-schema convergence, GPU/runtime
  descriptor coverage, sculpt topology services, painter texture-set contract.
- Parallel work opportunities:
  shared runtime foundation vs. app registry work, sculpt lane vs. painter lane,
  parity harness vs. importer honesty and docs.

## Task Checklist

- [ ] 1. Reference Baseline And Capability Inventory
- [ ] 1.1 Build the explicit sculpt and painter parity matrix
  - Objective: Capture every required KSculpt and KPainter capability with a
    durable feature id, reference source, owning subsystem, and acceptance
    status.
  - Files: `.reference/sculpting/*`, `.reference/graphos/*`,
    `docs/reference/feature-matrix.md`, new parity matrix docs or manifests
    under `docs/reference/` and `apps/kain-fabric-dcc-suite/config/`.
  - Validation: Matrix review; missing-owner and missing-test checks fail.
  - _Requirements: REQ-2, REQ-3, REQ-4, REQ-5, REQ-6, NFR-3, NFR-4_
- [ ] 1.2 Lock the flagship app and labs ownership model
  - Objective: Confirm `apps/kain-fabric-dcc-suite` as the parity destination
    and document how `apps/kain-canvas-forge` and `labs/*` feed it without
    becoming long-term semantic forks.
  - Files: `apps/kain-fabric-dcc-suite/ARCHITECTURE.md`,
    `apps/kain-canvas-forge/ARCHITECTURE.md`, root `ARCHITECTURE.md`,
    parity-spec docs.
  - Validation: Architecture docs reflect one canonical parity destination and
    explicit lab integration rules.
  - _Requirements: REQ-1, REQ-2, REQ-6, REQ-7_

- [ ] 2. Native Authoring And Dev Loop Foundation
- [ ] 2.1 Complete native component, state, and effect semantics
  - Objective: Extend `kain-core` and lowering paths so flagship native shells
    no longer depend on React-like leftovers.
  - Files: `crates/kain-core/src/*`, `crates/kain-import/src/typescript/*`,
    language docs under `docs/syntax-and-semantics/`.
  - Validation: Parser/lowering tests for native declarations and importer hook
    lowering; degraded imports fail honestly.
  - _Requirements: REQ-7, NFR-3_
- [ ] 2.2 Expand typed runtime descriptors for viewport, compute, preview, and export
  - Objective: Give sculpt and painter lanes typed descriptors for viewports,
    compute passes, postprocess, captures, and exports.
  - Files: `crates/kain-core/src/runtime_contract*`,
    `crates/kain-core/src/realtime_app_bundle*`, `crates/kain-driver/src/*`,
    app descriptor registries.
  - Validation: Descriptor serialization and compatibility tests; bundle
    materialization smoketests.
  - _Requirements: REQ-1, REQ-4, REQ-6, REQ-7, NFR-1, NFR-4_
- [ ] 2.3 Harden `kain native-ui dev` and the native host bridge
  - Objective: Finish reload reporting, launcher diagnostics, compatibility
    classification, and restart-safe session restoration across app, shader, and
    manifest edits.
  - Files: `crates/cli/src/*`, `crates/kain-driver/src/*`,
    `crates/kain-ui/src/*`, `crates/kain-ui-native/src/*`.
  - Validation: CLI integration tests for ignore rules, debounce, hot reload vs
    restart, launcher-failure reporting, and last-good artifact recovery.
  - _Requirements: REQ-1, REQ-2, REQ-7, NFR-1, NFR-2, NFR-3_

- [ ] 3. Shared DCC Session, Workbench, And Resource Layer
- [ ] 3.1 Unify workbench, tool, brush, and capability registries
  - Objective: Keep all sculpt and painter chrome, presets, and capability state
    in manifest- or schema-driven registries consumed by one native shell.
  - Files: `apps/kain-fabric-dcc-suite/config/*.json`,
    `apps/kain-fabric-dcc-suite/session/*.kn`, shell materializer scripts, root
    docs for registry ownership.
  - Validation: Registry schema checks; generated shell and session state stay in
    sync with manifest edits.
  - _Requirements: REQ-2, REQ-3, REQ-5, REQ-6, NFR-4_
- [ ] 3.2 Add durable undo/redo, history, and restore-safe receipts
  - Objective: Make sculpt and painter actions round-trip through the shared
    session document and durable receipts rather than ephemeral widget state.
  - Files: `apps/kain-fabric-dcc-suite/session/*.kn`,
    `apps/kain-fabric-dcc-suite/state/*`, runtime snapshot and bridge modules.
  - Validation: Scenario tests for undo/redo, hot reload state preservation, and
    restart recovery.
  - _Requirements: REQ-1, REQ-2, NFR-2, NFR-3_
- [ ] 3.3 Consolidate asset import, export, and receipt vocabulary
  - Objective: Define one asset and export contract across sculpt meshes,
    texture sets, captures, and packed outputs.
  - Files: `apps/kain-fabric-dcc-suite/src/*projection.kn`,
    `apps/kain-fabric-dcc-suite/config/*contract*.json`, export-related
    descriptors and docs.
  - Validation: Import/export receipt schema tests and scenario exports.
  - _Requirements: REQ-2, REQ-4, REQ-6, NFR-2, NFR-4_

- [ ] 4. KSculpt Parity Vertical Slice
- [ ] 4.1 Ship the sculpt interaction stack
  - Objective: Deliver projected cursor feedback, symmetry, brush presets,
    add/subtract semantics, and viewport interaction that match the KSculpt
    baseline.
  - Files: sculpt workspace registries, viewport descriptors, shared 3D runtime,
    sculpt input modules, native host event bridge.
  - Validation: Scenario tests for cursor projection, symmetry, brush switching,
    and hot-reload state retention.
  - _Requirements: REQ-3, NFR-1, NFR-3_
- [ ] 4.2 Ship topology-aware sculpt runtime services
  - Objective: Add hit-query, remesh or rebuild, detail-control, and helper/GPU
    coordination needed for real mesh sculpt sessions.
  - Files: sculpt runtime descriptors, GPU shaders, native helper seams,
    topology reports, mesh resource contracts.
  - Validation: Integration tests for topology-service compatibility and large
    mesh operation reporting; benchmark receipts.
  - _Requirements: REQ-4, NFR-1, NFR-2, NFR-3_
- [ ] 4.3 Ship sculpt layers, preview materials, gizmos, and export
  - Objective: Add layer-aware authoring strata, clay/PBR preview modes, matcap
    catalogs, transform posture, and geometry export receipts.
  - Files: workbench manifests, material preview registries, export modules,
    sculpt state schema, preview shaders.
  - Validation: End-to-end sculpt session with import, layer edits, preview
    switching, and export.
  - _Requirements: REQ-4, NFR-3, NFR-4_

- [ ] 5. KPainter Parity Vertical Slice
- [ ] 5.1 Ship the layered painter core
  - Objective: Deliver layer stacks, channel-aware painting, symmetry, alpha
    maps, and registry-driven brush presets inside the flagship native shell.
  - Files: painter workspace registries, layer-stack contracts, texture-set
    state, brush catalogs, painter runtime descriptors.
  - Validation: Scenario tests for layer ordering, masking, channel switching,
    and preview-safe paint commits.
  - _Requirements: REQ-5, NFR-1, NFR-3, NFR-4_
- [ ] 5.2 Ship live lookdev preview and packed export
  - Objective: Bind painter outputs to a 3D preview material and keep export
    receipts synchronized with the same texture-set contract.
  - Files: preview-material bindings, texture export projections, runtime
    descriptors, workbench manifests, shader catalog.
  - Validation: End-to-end preview and export scenario; receipt consistency
    checks.
  - _Requirements: REQ-5, REQ-6, NFR-2, NFR-3_
- [ ] 5.3 Ship generators, filters, simulations, and time-based painter behaviors
  - Objective: Add the advanced Graphos-style compute effects, previews, and
    timeline-aware behaviors required by the parity baseline.
  - Files: GPU descriptors, generator/filter registries, simulation presets,
    painter session schema, capability manifests.
  - Validation: GPU effect smoketests, unavailable-capability reporting tests,
    playback or keyframe scenario checks.
  - _Requirements: REQ-6, NFR-1, NFR-2, NFR-3, NFR-4_

- [ ] 6. Migration Tooling, Docs, And Parity Harness
- [ ] 6.1 Finish importer degradation reporting and migration receipts
  - Objective: Make importer output honest, inspectable, and usable as a
    bootstrap aid for moving reference concepts into native Kain surfaces.
  - Files: `crates/kain-import/src/typescript/*`, `crates/cli/src/import_*`,
    importer docs under `docs/cli/`.
  - Validation: Strict-mode importer tests and degraded-output reporting checks.
  - _Requirements: REQ-7, NFR-2, NFR-3_
- [ ] 6.2 Build the parity harness and benchmark suite
  - Objective: Automate feature coverage, scenario execution, and latency
    tracking against the parity matrix.
  - Files: new validation scripts or commands under `scripts/`, `docs/reference/`
    parity docs, app-level benchmark receipts.
  - Validation: CI- or operator-runnable parity report with failing checks for
    missing coverage or broken latency budgets.
  - _Requirements: REQ-1, REQ-3, REQ-4, REQ-5, REQ-6, NFR-1, NFR-2, NFR-3_
- [ ] 6.3 Update durable operator and architecture docs
  - Objective: Keep the repo docs current as parity work lands so future agents
    can extend the system without rediscovering ownership rules.
  - Files: root `ARCHITECTURE.md`, `MEMORY.md`, app architecture docs,
    `docs/reference/*`, `docs/cli/*`.
  - Validation: Docs review against the shipped architecture and command surface.
  - _Requirements: REQ-1, REQ-2, REQ-7, NFR-4_

- [ ] 7. Flagship Acceptance And Packaging Hardening
- [ ] 7.1 Promote the sculpt and painter lanes to flagship acceptance
  - Objective: Prove that the flagship native app can execute the agreed sculpt
    and painter scenarios without falling back to legacy TS shells.
  - Files: `apps/kain-fabric-dcc-suite/**/*`, parity harness outputs,
    acceptance docs.
  - Validation: Full scenario run covering sculpt, painter, reload, restore, and
    export.
  - _Requirements: REQ-1, REQ-3, REQ-4, REQ-5, REQ-6, REQ-7, NFR-1, NFR-2, NFR-3_
- [ ] 7.2 Harden packaging, launcher behavior, and operating playbooks
  - Objective: Make the native shipping path and operator recovery path
    reliable enough for daily internal use.
  - Files: `crates/cli/src/*`, `crates/kain-driver/src/*`,
    `crates/kain-ui-native/src/*`, app-native scripts and packaging docs.
  - Validation: Package build, launcher restart, and recovery drills across the
    supported host matrix.
  - _Requirements: REQ-1, REQ-2, NFR-2, NFR-3_

## Validation Gates

- [x] All requirement IDs are covered
- [x] Dependencies are respected
- [x] Tests are included in the relevant tasks
- [x] Rollout or migration work is captured when needed

## Notes and Risks

- Risk: Native host instability, especially around the current launcher path,
  could block the entire parity program even if app semantics are correct.
  Mitigation: Treat host hardening as a first-wave foundation task and keep
  last-good artifact recovery plus lab-isolated host experiments available.
- Risk: Painter parity has no single dedicated `.reference/paint/` folder.
  Mitigation: Lock the parity baseline explicitly to `.reference/graphos/*` plus
  the current Kain painter scaffolds and keep the capability matrix source field
  mandatory.
- Risk: Sculpt and painter could drift into separate runtime models.
  Mitigation: Force both lanes through one shared DCC session contract and one
  typed descriptor model before feature-deep vertical slices land.
