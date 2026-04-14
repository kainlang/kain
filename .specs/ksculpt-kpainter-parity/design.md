# Design: KSculpt And KPainter Parity

**Spec Type:** full  
**Slug:** `ksculpt-kpainter-parity`  
**Created:** 2026-04-14

## Overview

This initiative uses a hybrid risk-first architecture:

- keep flagship parity inside one native Kain-owned DCC shell,
- push risky kernels and host experiments into `labs/`,
- land shared authoring/runtime contracts before feature-deep sculpt and paint
  vertical slices,
- treat TypeScript import as migration support rather than the core product
  path.

The primary flagship surface is the native DCC shell already taking shape in
`apps/kain-fabric-dcc-suite`. `apps/kain-canvas-forge` remains valuable as a
Node-first painter prototype and comparison surface, but it is not the long-term
parity destination. Chronos and other `labs/*` proofs remain the place to prove
compute-heavy or runtime-risky techniques before integration.

## Requirements Traceability

- REQ-1 -> Native Desktop Workbench And Reload Loop, Reload Coordinator, Shared
  Session Snapshot Flow
- REQ-2 -> Shared DCC Contract Layer, Registry Model, Session And Receipt Model
- REQ-3 -> Sculpt Interaction Stack, Viewport Hit And Cursor Services
- REQ-4 -> Mesh And Topology Runtime Lane, Sculpt Resource Contracts, Export
  Receipts
- REQ-5 -> Painter Layer Stack, Brush Registry, Material Channel Contracts
- REQ-6 -> Painter Compute Effects, Preview Material Binding, Export Pipeline
- REQ-7 -> Native Language Surface, Shared Lowering Pipeline, Runtime
  Descriptor Model
- NFR-1 -> Reload Coordinator Telemetry, Runtime Quality Tiers, GPU/CPU Fallback
  Contracts
- NFR-2 -> Host Diagnostics, Capability Matrix, Last-Good Artifact Recovery
- NFR-3 -> Parity Harness, Benchmarks, Scenario Acceptance Suite
- NFR-4 -> Registry Model, Manifest Ownership Rules, Session Schema

## Architecture

### System Overview

The parity program has six durable layers:

1. Native authoring surface
   `crates/kain-core` grows the `component`, state, and effect surface plus the
   typed descriptor vocabulary needed by native sculpt and painter apps.

2. Native desktop packaging and dev loop
   `crates/cli`, `crates/kain-driver`, `crates/kain-ui`, and
   `crates/kain-ui-native` own project materialization, runtime compatibility
   classification, hot reload, restart fallback, and host diagnostics.

3. Shared DCC shell and session contract
   `apps/kain-fabric-dcc-suite/config/*.json`, `session/*.kn`, and generated
   native shell outputs own workbench modes, tools, commands, presets, session
   history, runtime capabilities, and asset receipts.

4. Sculpt runtime lane
   Shared viewport code, Kain runtime descriptors, GPU shaders, and narrow
   native helper seams own hit queries, brush evaluation, topology workflows,
   preview materials, layers, and mesh export.

5. Painter runtime lane
   Shared 2D or 3D paint descriptors, channel-aware texture contracts, GPU
   generators and filters, preview bindings, and export receipts own the painter
   product surface.

6. Parity harness and migration tooling
   Reference matrices, importer degradation reports, and scenario suites prove
   the parity claims and keep the migration story honest.

### Component Boundaries
- `crates/kain-core`
  Responsibility: native language surface, AST or IR support, shared lowering
  model, runtime descriptor schemas.
- `crates/kain-import`
  Responsibility: lower compatible React patterns into the same semantic model
  and emit explicit degradation reports for unsupported imports.
- `crates/cli`
  Responsibility: `kain native-ui dev`, parity-oriented validation commands, and
  top-level operator reports.
- `crates/kain-driver`
  Responsibility: bundle materialization, compatibility metadata, artifact role
  classification, and restart-vs-hot-reload decisions.
- `crates/kain-ui` and `crates/kain-ui-native`
  Responsibility: runtime bundle consumption, native workbench hosting, session
  snapshot reload, and file- or service-backed host bridge behavior.
- `apps/kain-fabric-dcc-suite`
  Responsibility: flagship DCC product shell, shared session contracts,
  workbench manifests, sculpt lane, painter lane, and export flows.
- `apps/kain-canvas-forge`
  Responsibility: comparison surface for painter ergonomics and a place to
  harvest proven manifest patterns while parity shifts to the native shell.
- `labs/*`
  Responsibility: risky kernels and proofs that must validate performance or
  host semantics before integration into the flagship app.

### Data Flow

1. Authored Kain files plus data registries feed the compiler and the native-ui
   materializer.
2. The driver emits runtime bundles, compatibility metadata, manifests,
   descriptors, and sidecar receipts.
3. `kain native-ui dev` decides whether to hot-apply artifacts or restart the
   child host based on artifact role changes and compatibility state.
4. The native shell consumes the current session document, runtime snapshot,
   capability map, and workbench manifests to render sculpt and painter chrome.
5. User commands flow back through reducers and planner-owned receipts instead
   of becoming host-local state.
6. Sculpt and painter runtime lanes evaluate GPU, native, or helper work and
   emit deterministic receipts back into the session and export state.
7. The parity harness reads the same registries, receipts, and scenario outputs
   to validate feature coverage against the reference matrix.

## Components and Interfaces

### Native Desktop Workbench And Reload Loop
- Purpose: Own native app materialization, launch, watch, compatibility
  classification, and operator-visible reload reporting.
- Inputs: Authored `.kn` files, app registries, shader sources, runtime
  descriptors, filesystem events, compatibility metadata.
- Outputs: Materialized project tree, artifact sidecars, child process state,
  reload reports, restart diagnostics.
- Dependencies: `crates/cli`, `crates/kain-driver`, `crates/kain-ui`,
  `crates/kain-ui-native`.

### Shared DCC Contract Layer
- Purpose: Provide one typed session and registry vocabulary for sculpt,
  painter, workbench, resources, presets, and capability state.
- Inputs: `config/*.json`, `session/*.kn`, command events, import/export
  receipts, runtime reports.
- Outputs: Session document, runtime snapshot, reducer transitions, registry
  projections, durable receipt files.
- Dependencies: `apps/kain-fabric-dcc-suite`, generated shell materializers,
  native host bridge.

### Sculpt Runtime Lane
- Purpose: Own hit testing, brush evaluation, topology services, sculpt layers,
  preview materials, and geometry export.
- Inputs: Active mesh resource id, sculpt tool preset, viewport state, brush
  stroke events, shader descriptors, helper-service capabilities.
- Outputs: Updated sculpt state, mesh or delta receipts, topology reports,
  preview updates, export artifacts.
- Dependencies: shared DCC session contract, GPU artifact lane, narrow native or
  Rust helpers where Kain does not yet own a kernel directly.

### Painter Runtime Lane
- Purpose: Own layer-stack painting, material channel authoring, generators,
  filters, simulations, preview binding, and packed export.
- Inputs: Layer stack document, brush preset, texture set contract, simulation
  parameters, preview scene state, export preset.
- Outputs: Updated layer data, texture-set receipts, preview updates, packed
  textures, generator/filter receipts.
- Dependencies: shared DCC session contract, GPU artifact lane, preview
  material/runtime descriptors.

### Native Language Surface And Lowering
- Purpose: Ensure Kain-native authoring and importer-recognized compatibility
  patterns target one semantic runtime model.
- Inputs: Kain component/state/effect syntax, compatible TypeScript hook
  patterns, descriptor declarations.
- Outputs: Shared IR, runtime descriptors, degradation reports, compatibility
  signatures.
- Dependencies: `crates/kain-core`, `crates/kain-import`, compiler validation
  and CLI report surfaces.

### Parity Harness
- Purpose: Make parity claims measurable and durable.
- Inputs: Reference feature matrix, capability registries, scenario scripts,
  performance receipts, importer degradation reports.
- Outputs: Parity dashboards, acceptance reports, benchmark results, missing
  capability inventory.
- Dependencies: docs/reference, `.reference/*`, app receipts, CLI test or
  validation commands.

## Data Models

### DccCapabilityMatrix
- Fields: `feature_id`, `domain`, `reference_source`, `owning_subsystem`,
  `status`, `test_id`, `notes`.
- Validation: Every parity-facing feature must have a unique id, a reference
  source, and an owning subsystem before it can be claimed as complete.
- Relationships: Drives parity harness reporting and acceptance dashboards.

### DccSessionDocument
- Fields: `workspace`, `workbench`, `active_resources`, `command_history`,
  `runtime_capabilities`, `sculpt`, `painter`, `exports`, `reports`.
- Validation: Generated snapshots must conform to one durable schema shared by
  the native shell and runtime bridges.
- Relationships: Primary durable state contract for the flagship app.

### SculptResourceContract
- Fields: `mesh_resource_id`, `layer_stack`, `tool_id`, `symmetry_mode`,
  `detail_policy`, `preview_material_id`, `topology_state`, `export_receipts`.
- Validation: Active targets and topology receipts must resolve through stable
  resource ids.
- Relationships: Linked from session state, export pipeline, and preview
  viewport descriptors.

### PainterTextureSetContract
- Fields: `texture_set_id`, `layers`, `channels`, `preview_binding`,
  `generator_stack`, `simulation_stack`, `export_preset_id`.
- Validation: Channel dimensions, layer references, and preview bindings must
  stay consistent across paint, preview, and export.
- Relationships: Linked from painter workspace state, preview material, and
  packed export outputs.

### NativeReloadReport
- Fields: `timestamp`, `changed_paths`, `artifact_roles`, `decision`,
  `elapsed_ms`, `compatibility_state`, `restart_reason`, `last_good_artifact`.
- Validation: Every dev-loop rebuild must produce a machine-readable report.
- Relationships: Consumed by operators, logs, and test harnesses.

## Error Handling

- Validation failures:
  parser, lowering, session-schema, and registry-validation failures stop the
  current publish path, preserve last-good artifacts, and emit structured
  diagnostics.
- Dependency failures:
  shader compilation failures, missing helper libraries, broken host launchers,
  or unsupported GPU/runtime capabilities surface as explicit capability or
  launcher errors rather than silent feature disappearance.
- Recovery:
  compatible changes hot-reload in place; incompatible changes restart the host
  from the last persisted session snapshot; hard runtime failures preserve the
  last-good bundle set for manual relaunch.
- User-visible errors:
  the native shell and CLI both expose last reload outcome, active fallback
  state, and guidance to the failing artifact or subsystem.

## Testing Strategy

- Unit:
  language lowering, capability matrix validation, reducer behavior, brush
  preset parsing, texture-set contract validation, and compatibility
  classification.
- Integration:
  native-ui dev loop, materialization/restart flow, shader and GPU artifact
  generation, import/export receipts, and host bridge state reload.
- Scenario or end-to-end:
  KSculpt brush and topology scenarios, painter layer/channel scenarios, preview
  binding, export, and restart-safe session restore.
- Performance, security, or reliability:
  stroke latency, preview frame time, reload latency, crash recovery, and
  deterministic receipt generation under failure conditions.

## Rollout and Operations

- Configuration:
  keep feature and capability ownership in app registries such as
  `config/runtime_lanes.json`, `config/shader_catalog.json`, tool and brush
  catalogs, export presets, and parity capability manifests.
- Observability:
  record reload reports, benchmark receipts, capability inventory, host restart
  reasons, GPU compile failures, and export receipts in durable machine-readable
  artifacts.
- Rollout:
  1. shared native authoring and reload foundation,
  2. shared DCC session and registry contract,
  3. sculpt parity vertical slice,
  4. painter parity vertical slice,
  5. parity harness and acceptance hardening.
- Rollback:
  preserve last-good artifacts, keep labs proofs isolated until hardened, and
  gate unfinished parity surfaces behind capability flags instead of half-enabled
  shell entries.
