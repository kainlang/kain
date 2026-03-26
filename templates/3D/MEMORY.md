# Memory

## Current Context

- This template is intended to stay Kain-first, manifest-driven, and
  downstream-friendly.
- The preferred maintenance shape is to deepen reusable runtime packs,
  manifests, kernels, and entry apps rather than multiply shallow lanes.
- Runtime app and workspace preset rows now carry an explicit `source_id`
  reference to `manifests/sources.json`, which keeps the shared workbench
  source visible even when many rows project from the same authored entrypoint.
- Missing upstream features should be captured in `limitations.md` so template
  workarounds remain visible.

## This Run

- Confirmed the template is still anchored by one authored workbench source and
  many manifest-derived runtime projections.
- Added a concrete top-level source-of-truth map to `README.md` and expanded
  `ARCHITECTURE.md` with the current manifest and generated-output surfaces.
- Normalized the most important file links to absolute Windows paths so the
  docs are easier to navigate from the Codex workspace UI.
- Lightweight manifest sanity checks found no duplicate runtime-app identities
  or missing required fields.
- Added explicit source IDs to the runtime-app and workspace-preset catalogs,
  then regenerated the reflection snapshots so launch profiles and runtime
  compatibility entries now surface both source IDs and source paths.
- Run time: `2026-03-26T15:23:42.8677160-04:00`

## Next Step

- Keep future maintenance focused on the shared manifests and reusable packs
  rather than adding more projections from the same authored entrypoint unless
  a real packaging or behavior difference appears.
