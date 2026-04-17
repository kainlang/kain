# DCC Native Authoring

## Purpose

These rules apply to flagship DCC work that aims for parity with the
`.reference/` applications while landing as native Kain-owned software instead
of TypeScript transliterations.

## Authoring Ownership

- Keep app meaning in Kain source, typed contracts, reducers, registries, and
  runtime descriptors.
- Treat native hosts, helper crates, and generated launchers as execution seams,
  not semantic owners.
- Treat TypeScript importers as migration aids and inspection tools, not as the
  primary authoring path for parity features.

## Product Shape

- Build flagship parity inside a native desktop shell with `kain native-ui dev`
  as the primary iteration loop.
- Use `apps/` for product-shaped authoring surfaces and `labs/` for risky GPU,
  host, or runtime experiments that must prove themselves before integration.
- Prefer one shared DCC shell and shared session contract across sculpt,
  painter, material, and viewport workflows instead of isolated one-off apps for
  each lane.

## Data-Driven Rule

- Workspace modes, tools, brushes, generators, filters, shaders, runtime lanes,
  export presets, and capability state must be registry-owned.
- Session restoration, runtime snapshots, and compatibility metadata must be
  durable artifacts with explicit schema rather than host-local hidden state.

## Parity Validation

- Use the relevant `.reference/` surfaces as the acceptance oracle:
  `sculpting/*` for KSculpt parity and `graphos/*` plus current Kain painter
  scaffolds for KPainter parity.
- The shared scene spine in `apps/3D/tools/validation/validate_scene_spine.py`
  is mandatory for template-level 3D changes. New scene, viewport, camera,
  interaction, mesh, lighting, or scene-exchange behavior should pass through
  that validator, not only through ad hoc app wiring.
- A feature is not at parity until it has:
  1. a Kain-owned implementation,
  2. a documented owning subsystem,
  3. a scenario or benchmark check,
  4. an explicit capability status in the parity matrix.

## Runtime and GPU Guidance

- Keep renderer, compute, and export behavior in typed runtime descriptors and
  shader/material registries rather than ad-hoc host scripting.
- Prefer GPU-owned evaluation for brush, preview, filter, and simulation work
  when it materially improves latency or scale.
- If a feature must fall back to CPU/native helper code, record the ownership
  boundary and the replacement plan in the active spec package.
