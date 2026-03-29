# Memory

## 2026-03-27 - Tensor Pipeline Reflection Source-Aware Indexes

The tensor-pipeline reflection catalog under
`generated/runtime-reflection/tensor-pipelines` now carries aggregated pass
source-id/path fields alongside stage and tensor-role metadata, with new
indexes for pass source ids and paths. The descriptor companion under
`generated/runtime-reflection/tensor-pipelines/descriptors` was refreshed to
capture the source-aware fields, and the catalog/descriptor READMEs now call
out the pass-source indexes. Regenerated the reflection snapshots after the
generator change. Run time: `2026-03-27T22:24:18.0644760-04:00`

## 2026-03-27 - Launch Profile Descriptor Root Materialization

The launch-profile reflection catalog now ships a descriptor-rooted companion
under `generated/runtime-reflection/launch-profiles/descriptors`, with folder
READMEs for both the parent catalog and the descriptor subtree. The reflection
generator now emits the launch-profile descriptor document alongside the
runtime-app, GPU, build-graph, distribution, and jobs descriptor surfaces, and
the template docs now call out launch-profiles as a first-class reflection
surface. Run time: `2026-03-27T05:19:33.4560897-04:00`

## 2026-03-27 - Jobs Reflection Descriptor Materialization

The jobs receipt-schema, receipt-template, and retry-ledger catalogs now each
ship descriptor-rooted snapshots under
`generated/runtime-reflection/{jobs-receipt-schemas,jobs-receipt-templates,jobs-retry-ledgers}/descriptors`
with committed READMEs for the parent folders and descriptor folders. The
reflection generator now emits the new descriptor documents, and the template
docs now call out the jobs reflection tree alongside the other committed
catalog surfaces. Run time: `2026-03-27T08:19:43.0579746Z`

## 2026-03-27 - Engine System Source-ID-First Normalization

Normalized `manifests/engine_systems.json` so the authored rows now carry
`source_id` instead of repeating `source_path`. The reflection generator now
projects each engine-system path from `manifests/sources.json`, keeping the
committed `generated/runtime-reflection/engine-systems/catalog.json` contract
aligned with the other source-id-first manifests. I also updated the template
docs to call out the shared source-registry projection pattern and regenerated
the reflection catalogs after the generator change. Run time:
`2026-03-27T04:18:07.8338254-04:00`

## 2026-03-27 - Build Graph and Distribution Descriptor Root Materialization

The build-graph and distribution reflection catalogs now both emit
descriptor-rooted companions under `generated/runtime-reflection/build-graphs`
and `generated/runtime-reflection/distribution`. The generator projects
descriptor-rooted catalog fields, publishes folder READMEs for both subtrees,
and keeps the queue/channel surfaces queryable without reopening only the raw
catalog snapshots. During regeneration I also hardened a couple of strict-mode
optional `source_path` reads in the generator so the new snapshot pass could
complete cleanly. Run time: `2026-03-27T07:21:49.3756757Z`

## 2026-03-27 - GPU Reflection Descriptor Root Materialization

The standalone GPU kernel reflection catalog is now descriptor-rooted under
`generated/runtime-reflection/gpu/descriptors` and projects `source_id` joins
from `manifests/sources.json` alongside `source_path`. The generator now emits
`generated/runtime-reflection/gpu/catalog.json` as the source of truth for the
GPU lane, and resource reflection now carries the same source-aware kernel
metadata through its linked GPU catalog entries. `limitations.md` still records
that this surface is template-generated rather than owned directly by the
upstream GPU emitter.

## 2026-03-27 - Launch Profile Descriptor Root Materialization

The launch-profile catalog now ships a descriptor-rooted companion under
`generated/runtime-reflection/launch-profiles/descriptors`. The generator emits
`launch_profile_catalog.json` with explicit runtime links to the source
registry, runtime-app, workspace-preset, workspace-preset receipt, and
distribution receipt catalogs so the launch binding surface stays queryable
without reopening only the manifest join. The generated runtime-reflection and
template docs now call out the new descriptor subtree. Run time:
`2026-03-27T09:20:02.3613823Z`

## Current Context

- This template is intentionally Kain-first, manifest-driven, and downstream-friendly.
- The preferred maintenance shape is to deepen reusable runtime packs, manifests,
  kernels, and entry apps rather than multiply shallow lanes.
- Runtime app and workspace preset rows carry explicit `source_id` references back
  to `manifests/sources.json`, while repeated `source_path` values are resolved
  from the source registry during reflection generation instead of being repeated
  in the projection manifests.
- The GPU kernel manifest now follows the same source-id-first pattern so the
  generator projects `source_path` from the shared source registry instead of
  requiring the authored kernel manifest to duplicate it.
- The source-registry reflection catalog keeps those projections queryable from
  one committed snapshot.
- The reflection runtime profile now also exposes an `engine_system_catalog`
  snapshot rooted in `manifests/engine_systems.json` so the core lane registry
  is queryable alongside runtime-app and source-registry reflections.
- The reflection runtime profile now names `source_registry_catalog` and
  `runtime_app_catalog` explicitly so both contracts are first-class reflection
  surfaces rather than side effects of the generator.
- The launch-profile reflection catalog now also emits a descriptor-rooted
  companion under `generated/runtime-reflection/launch-profiles/descriptors`
  so the workspace-preset/runtime binding surface is queryable as a committed
  subtree.
- The source-registry workspace-preset indexes now cover the full
  `workspace_presets.json` manifest rather than only the launch-example subset.
- The workspace-preset reflection snapshot now has a committed folder README
  under `generated/runtime-reflection/workspace-presets` so the catalog is
  discoverable from the reflection tree alongside the runtime-app, source
  registry, engine-system, and runtime-compatibility surfaces.
- Missing upstream features should be captured in `limitations.md` so template
  workarounds remain visible.
- Descriptor-rooted compatibility snapshots now have a dedicated README under
  `generated/runtime-compatibility/descriptors`, and the reflection generator
  emits it so the matrix, window, launch-readiness, and feature-pack views are
  discoverable as a subtree on every regeneration.
- The GPU kernel reflection snapshot is now descriptor-rooted under
  `generated/runtime-reflection/gpu/descriptors` and projects `source_id`
  joins alongside `source_path`, while the remaining upstream gap is that the
  Kain emitter still does not own that committed snapshot directly.
- The authored GPU kernel manifest now uses `source_id` as the canonical
  repeated key and lets the reflection generator recover `source_path` from
  `manifests/sources.json`.
- The build-graph and distribution snapshots are now also descriptor-rooted so
  the queue and delivery surfaces can be inspected through committed
  subtrees, not only through top-level catalog files.
- The jobs receipt schema, receipt template, and retry-ledger snapshots now
  also carry descriptor-rooted companions under
  `generated/runtime-reflection/{jobs-receipt-schemas,jobs-receipt-templates,jobs-retry-ledgers}/descriptors`,
  keeping the dispatch, receipt, and retry surfaces discoverable from the
  committed reflection tree.

## This Run

- Added a committed launch-profiles README under
  `generated/runtime-reflection/launch-profiles` and a descriptor-folder README
  under `generated/runtime-reflection/launch-profiles/descriptors` so the
  workspace-preset/runtime binding catalog is discoverable from the reflection
  tree.
- Added a committed launch-profile descriptor document under
  `generated/runtime-reflection/launch-profiles/descriptors/launch_profile_catalog.json`.
- Expanded the launch-profile descriptor runtime links to include the source
  registry, runtime-app, workspace-preset, workspace-preset receipt, and
  distribution receipt catalogs.
- Updated the template and generated reflection docs to call out the new
  descriptor-rooted launch-profile subtree.
- Regenerated the committed runtime-reflection snapshots after the generator
  change.
- No broad testing or validation was run.
- Run time: `2026-03-27T09:20:02.3613823Z`

## Next Step

- Keep the GPU, launch-profile, build-graph, distribution, and runtime-app
  reflection catalogs aligned with manifest changes and continue treating them
  as first-class reflection surfaces alongside the source registry,
  engine-system registry, workspace-preset catalog, and runtime compatibility
  matrix.

