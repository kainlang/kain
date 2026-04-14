# Requirements: KSculpt And KPainter Parity

**Spec Type:** full  
**Slug:** `ksculpt-kpainter-parity`  
**Created:** 2026-04-14

## Overview

Kain already has a native desktop dev loop, a Chronos-native proof, and strong
compiler/runtime ownership boundaries, but it does not yet deliver the sculpt
and painter product depth proven by the legacy `.reference/` suite.

This spec defines the program to reach native parity with:

- `.reference/sculpting/*` as the KSculpt baseline
- `.reference/graphos/*` plus the current Kain painter scaffolds in
  `apps/kain-canvas-forge` and `apps/kain-fabric-dcc-suite` as the KPainter
  baseline

The goal is not a literal TypeScript transliteration. The goal is a Kain-native
desktop authoring path where flagship sculpt and painter workflows are authored
through Kain semantics, typed runtime descriptors, and a durable native app
shell with hot reload, restart-safe state restore, and data-driven registries.

## User Roles

- Sculpt Artist - Needs responsive mesh sculpting, brush control, topology
  operations, and viewport feedback without leaving the native Kain runtime.
- Paint And Lookdev Artist - Needs layered texture/material painting, brush and
  filter workflows, and 3D preview/export parity inside the same product shape.
- Technical Artist - Needs predictable hot reload, explicit runtime ownership,
  import/export receipts, and extensible data-driven registries.
- Language And Runtime Engineer - Needs one semantic model for authored Kain,
  importer-lowered compatibility patterns, runtime descriptors, and native app
  materialization.

## Requirements

### REQ-1: Native Desktop Workbench And Dev Loop
**User Story:** As a technical artist, I want a one-command native dev loop and
stateful desktop workbench, so that I can build flagship tools in Kain without
falling back to the old TypeScript shell.

**Acceptance Criteria**
1. WHEN an operator runs `kain native-ui dev <input.kn>` for the flagship DCC
   app THEN Kain SHALL materialize the app, launch the native child, stream
   child logs, and watch the authored app root recursively.
2. WHEN a compatible change lands in Kain source, shader source, manifests, or
   app-local runtime descriptors THEN the system SHALL apply an in-process
   reload without losing the current workspace, dock layout, selected tool,
   active camera, or session document.
3. WHEN an incompatible runtime contract or launcher change lands THEN the
   system SHALL restart the native child automatically and restore baseline
   state from the persisted runtime snapshot and app manifest.
4. THE dev loop SHALL ignore generated project directories, artifact roots,
   `target/`, `.git/`, `node_modules/`, editor temp files, and OS swap files so
   file watching cannot self-trigger.
5. THE native workbench SHALL expose explicit reload outcomes, restart reasons,
   and last-good artifact state for operator debugging.

**Edge Cases**
- Native host launcher failure or crash must surface a clear diagnostic path and
  leave the last-good artifacts intact.
- Multiple file writes in one save burst must debounce into a single rebuild.

### REQ-2: Shared DCC Session, Workbench, And Asset Contract
**User Story:** As a language and runtime engineer, I want one typed contract
for workspace chrome, commands, assets, history, presets, and runtime
capabilities, so that sculpt and painter lanes grow inside the same native shell
instead of diverging into app-specific islands.

**Acceptance Criteria**
1. THE flagship DCC app SHALL own a typed session document for workspace state,
   active resource targets, command history, export receipts, runtime
   capabilities, and persisted UI/workbench layout.
2. THE system SHALL keep workspace modes, tools, brush presets, shader
   catalogs, runtime lanes, export presets, and capability flags in
   data-driven registries rather than host-local hardcoded logic.
3. WHEN commands mutate sculpt, painter, or shared workspace state THEN those
   changes SHALL round-trip through reducers and durable receipts instead of
   transient widget state only.
4. THE system SHALL provide undo/redo, session restore, and explicit import and
   export receipts for sculpt meshes, texture sets, captures, and packed
   outputs.

**Edge Cases**
- Partial runtime availability must produce an explicit capability status rather
  than silently hiding tools or claiming unsupported parity.
- Generated shells and snapshots must remain disposable projections of the typed
  session and registry truth.

### REQ-3: KSculpt Brush And Viewport Parity
**User Story:** As a sculpt artist, I want the brush, cursor, and viewport loop
to match the depth of the KSculpt reference suite, so that Kain feels like a
real sculpt workstation instead of a proof demo.

**Acceptance Criteria**
1. THE sculpt workspace SHALL provide a 3D viewport with projected brush cursor
   feedback on the active mesh plus mirrored cursor feedback when symmetry is
   enabled.
2. THE brush system SHALL support radius, intensity, falloff or alpha
   selection, add/subtract behavior, and reusable brush presets through
   registry-owned definitions.
3. THE sculpt workspace SHALL support symmetry modes covering at least `NONE`
   and `X`, with extension seams for further axes or radial variants.
4. THE sculpt workspace SHALL preserve selected sculpt tool, brush preset,
   cursor posture, and viewport camera state across compatible hot reloads.

**Edge Cases**
- If projected cursor data cannot be produced for the active mesh, the system
  must expose a degraded state rather than painting against stale hit results.
- Tablet and mouse input paths must agree on stroke semantics even if pressure
  support is temporarily unavailable on one backend.

### REQ-4: KSculpt Mesh, Topology, And Export Parity
**User Story:** As a sculpt artist, I want topology-aware editing, material
preview, and durable mesh receipts, so that Kain can own real sculpt sessions
instead of only heightfield proofs.

**Acceptance Criteria**
1. THE sculpt workspace SHALL support active edit targets for imported meshes
   and authored primitives through typed mesh resource ids.
2. THE system SHALL support topology-aware sculpt operations including hit
   queries, rebuild or remesh paths, and detail controls equivalent in intent to
   KSculpt dynamic-topology workflows.
3. THE sculpt workspace SHALL provide sculpt layers or equivalent non-destructive
   authoring strata, material preview modes including clay and PBR-style looks,
   and a matcap library or equivalent preview catalog.
4. THE sculpt workspace SHALL support transform/gizmo posture appropriate to the
   active object or layer target.
5. THE system SHALL support export receipts for visible sculpt results, with
   glTF or equivalent geometry export as the baseline deliverable.

**Edge Cases**
- Large-mesh topology operations must surface progress, queue background work,
  or degrade gracefully instead of freezing the shell without feedback.
- If native helper or GPU topology services disagree on compatibility, the shell
  must present a deterministic fallback or a hard failure with diagnostics.

### REQ-5: KPainter Layered Paint And Material Channel Parity
**User Story:** As a paint and lookdev artist, I want layered painting and
channel-aware material authoring that matches the reference painter surface, so
that I can stay inside Kain for texture and look development.

**Acceptance Criteria**
1. THE painter workspace SHALL provide a layer stack with visibility, opacity,
   ordering, blend or composition mode, and mask-aware authoring primitives.
2. THE brush system SHALL support size, opacity, hardness, spacing, angle,
   jitter, erase behavior, alpha-map selection, and symmetry modes covering at
   least `NONE`, `X`, `Y`, and `RADIAL`.
3. THE painter workspace SHALL support authoring across texture or material
   channels including at least albedo, normal, roughness, metalness, and
   emission.
4. THE system SHALL store brush presets, alpha maps, channel bindings, and
   workspace defaults in data-driven registries so they can be authored and
   extended without rewriting the host.

**Edge Cases**
- Missing channel resources or invalid texture dimensions must produce explicit
  validation errors and a recoverable layer state.
- Symmetry and erase semantics must remain deterministic across 2D surface mode
  and 3D projection mode.

### REQ-6: KPainter Generators, Filters, Simulations, And Lookdev Preview
**User Story:** As a paint and lookdev artist, I want procedural generation,
GPU filters, simulation-style effects, and a 3D preview loop, so that Kain can
replace the more advanced parts of the legacy painter workflow.

**Acceptance Criteria**
1. THE painter lane SHALL support registry-owned generators and filters aligned
   with the Graphos reference families, including noise/pattern generation,
   blur/levels or equivalent tonal filters, and normal/posterize or equivalent
   channel transforms.
2. THE painter lane SHALL support simulation-style passes and effect presets for
   paint data, with explicit capability states for each available effect family.
3. THE system SHALL support a 3D preview mode that binds current texture sets to
   a preview material and updates live as painter changes are committed.
4. THE painter workspace SHALL support export receipts for images, texture sets,
   or packed material outputs, plus restore-safe presets for repeated exports.
5. THE advanced painter lane SHALL support time-based playback or keyframed
   parameter changes where those behaviors are part of the chosen parity
   baseline.

**Edge Cases**
- Unsupported GPU passes must show unavailable capability state instead of
  silently omitting the feature from the shell.
- Preview materials and packed exports must remain in sync with the same
  texture-set contract rather than diverging into separate hidden pipelines.

### REQ-7: Native Language Surface And Runtime Descriptor Parity
**User Story:** As a language and runtime engineer, I want sculpt and painter
apps authored through native Kain semantics and shared lowering, so that parity
does not depend on leaked React or Three.js implementation details.

**Acceptance Criteria**
1. Kain SHALL provide native authoring semantics for component declarations,
   state slots, and effect blocks used by the flagship desktop shell.
2. THE compiler SHALL lower native component/state/effect authoring and
   importer-recognized `useState` and `useEffect` patterns through the same
   semantic model.
3. Viewport, compute, simulation, postprocess, capture, and export behavior
   SHALL be representable through typed Kain runtime descriptors instead of
   unstructured host-side scripting.
4. THE importer SHALL emit structured degradation reports and mark outputs as
   degraded whenever generated `.kn` fails parse, build, or semantic-lowering
   checks.

**Edge Cases**
- Unsupported hook or JSX patterns must fail with an explicit degradation reason
  instead of claiming a clean import.
- Authoring syntax that changes runtime compatibility must participate in the
  same compatibility classification used by `kain native-ui dev`.

## Non-Functional Requirements

### NFR-1: Interaction And Reload Latency
- Compatible UI, registry, or shader edits should hot-apply within a developer
  budget suitable for live iteration, with explicit timing telemetry recorded
  for each reload event.
- Interactive sculpt and painter operations should target artist-credible
  latency at the default quality tier and degrade through explicit quality modes
  before they fall below usable responsiveness.

### NFR-2: Reliability, Recovery, And Observability
- Native host crashes, shader compilation failures, topology-service failures,
  and export failures must emit durable diagnostics and leave the last-good
  artifacts or session state recoverable.
- Runtime capability state, reload outcomes, and fallback paths must be visible
  to operators and test harnesses.

### NFR-3: Parity Validation And Test Coverage
- Every shipped parity claim must map to a reference feature, an owning
  subsystem, and at least one automated or scripted validation scenario.
- Flagship parity work must include unit, integration, and end-to-end checks
  across compiler, runtime, host, and app layers.

### NFR-4: Data-Driven Extensibility
- Tools, brushes, filters, export presets, runtime lanes, capability states, and
  workspace chrome must remain manifest- or schema-driven so future parity work
  does not require repeated host rewrites.

## Out of Scope

- Literal one-file TypeScript parity where Kain simply embeds the old React or
  Three.js code with minimal translation.
- Unrelated DCC domains such as full rigging, animation, compositor, or social
  sharing workflows that are not required to satisfy sculpt or painter parity.
- Collaborative cloud services, remote multi-user sessions, or SaaS backends.
- Feature claims beyond the reference baselines without a separate spec package.

## Open Questions

- Should the long-term native host remain on the current `qmlscene`-backed path,
  or should Kain replace that launcher contract if stability remains the
  dominant blocker?
- Should flagship parity land as the evolution of `apps/kain-fabric-dcc-suite`,
  or should that scaffold fold into a new consolidated studio app name once the
  shared contracts stabilize?
- Which tablet-input backend and OS matrix should be the first-class release
  target for pressure-sensitive sculpt and paint parity?
