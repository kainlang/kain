# 3D Template Architecture

This template is a Kain-first starting point for downstream 3D projects.
The goal is to keep the system manifest-driven, stdlib-first, and usable
without requiring Rust or Cargo on the consumer machine.

## Current Shape

- One authored app drives the template: `src-kain/apps/universal_3d_workbench/main.kn`.
- Runtime behavior fans out through manifest data in `manifests/runtime_apps.json`,
  `manifests/sources.json`, `manifests/workspace_presets.json`,
  `manifests/build_graphs.json`, `manifests/gpu_kernels.json`,
  `manifests/tensor_pipelines.json`, `manifests/ui_surfaces.json`, and
  `manifests/distribution_channels.json`.
- `runtime_apps.json` and `workspace_presets.json` both carry explicit
  `source_id` references back to `manifests/sources.json`; the repeated
  `source_path` value is resolved from the source registry during reflection
  generation instead of being duplicated across every row.
- `engine_systems.json` now follows the same source-id-first pattern; the
  reflection generator resolves each engine-system `source_path` from the shared
  source registry instead of requiring the authored manifest to repeat it.
- `gpu_kernels.json` now follows the same `source_id`-first pattern; the
  reflection generator resolves each kernel `source_path` from the shared source
  registry instead of requiring the authored manifest to repeat the path.
- Committed reflection snapshots live under `generated/runtime-reflection`,
  `generated/runtime-reflection/runtime-apps`,
  `generated/runtime-reflection/launch-profiles`,
  `generated/runtime-reflection/engine-systems`,
  `generated/runtime-reflection/gpu`,
  `generated/runtime-reflection/source-registry`,
  `generated/runtime-reflection/build-graphs`,
  `generated/runtime-reflection/distribution`,
  `generated/runtime-reflection/jobs-receipt-schemas`,
  `generated/runtime-reflection/jobs-receipt-templates`,
  `generated/runtime-reflection/jobs-retry-ledgers`,
  `generated/resource-reflection`, and `generated/runtime-compatibility`.
- The runtime-app snapshot now also emits a descriptor document under
  `generated/runtime-reflection/runtime-apps/descriptors` so downstream
  consumers can inspect the app catalog contract without reopening only the
  monolithic catalog snapshot.
- The launch-profile snapshot now emits a descriptor document under
  `generated/runtime-reflection/launch-profiles/descriptors` so downstream
  consumers can inspect the workspace-preset/runtime binding contract without
  reopening only the monolithic catalog snapshot.
- The scene-composition spine is intentionally explicit: `scene_runtime`,
  `scene_exchange_runtime`, `scene_semantics_runtime`, `scene_bundle_runtime`,
  `viewport_runtime`, `camera_runtime`, `interaction_runtime`, `mesh_runtime`,
  and `lighting_runtime` are the core contracts that downstream 3D tools should
  compose first before introducing new host glue.
- `tools/validation/validate_scene_spine.py` is the lightweight guardrail for
  that spine. It checks that the shared scene, exchange, semantics, bundle,
  viewport, camera, interaction, mesh, and lighting runtime systems stay
  registered against the shared source registry instead of drifting into
  app-local behavior.
- `tools/validation/run_template_checks.py` is the template-level 3D check
  runner. It wraps the scene-spine validator and the primary workbench launch
  bindings so CI and regen flows have one stable entrypoint instead of ad hoc
  shell glue.
- Renderer, viewport, and scene tooling should prefer manifest additions or
  stdlib surface growth when a new 3D behavior is reusable across workbench
  presets, rather than hardcoding a one-off app path.
- The jobs receipt schema, receipt template, and retry-ledger snapshots now
  each emit descriptor documents under
  `generated/runtime-reflection/jobs-receipt-schemas/descriptors`,
  `generated/runtime-reflection/jobs-receipt-templates/descriptors`, and
  `generated/runtime-reflection/jobs-retry-ledgers/descriptors` so the jobs
  dispatch, receipt, and retry surfaces stay navigable through the reflection
  tree.
- The workspace-preset snapshot now also has folder documentation under
  `generated/runtime-reflection/workspace-presets/README.md` so the catalog is
  discoverable directly from the reflection tree.
- The engine-system snapshot now emits a descriptor document under
  `generated/runtime-reflection/engine-systems/descriptors` so downstream
  consumers can inspect the lane registry contract without reopening only the
  raw `engine_systems.json` manifest.
- The reflection runtime pack explicitly anchors the source-registry catalog
  alongside the schema, GPU, runtime-contract, resource-reflection,
  workspace-preset, jobs, launch-profile, build-graph, and distribution
  catalogs so downstream consumers can treat the registry as a first-class
  reflection surface.
- The build-graph and distribution catalogs now also carry descriptor-rooted
  docs under `generated/runtime-reflection/build-graphs/descriptors` and
  `generated/runtime-reflection/distribution/descriptors`, while the jobs
  catalog family carries the same pattern under
  `generated/runtime-reflection/jobs-receipt-schemas/descriptors`,
  `generated/runtime-reflection/jobs-receipt-templates/descriptors`, and
  `generated/runtime-reflection/jobs-retry-ledgers/descriptors`; together they
  keep the queue, delivery, receipt, and retry surfaces queryable without
  reopening only the top-level catalogs.
- The GPU kernel reflection subtree now has its own folder README under
  `generated/runtime-reflection/gpu/README.md` and a descriptor-rooted companion
  under `generated/runtime-reflection/gpu/descriptors`; the catalog now also
  projects `source_id` joins alongside `source_path` so downstream consumers can
  resolve kernels through the shared source registry, while the authored
  `gpu_kernels.json` manifest stays source-id-first.
- The build-graph and distribution reflection subtrees now also carry folder
  READMEs and descriptor-rooted companions under
  `generated/runtime-reflection/build-graphs/descriptors` and
  `generated/runtime-reflection/distribution/descriptors`, keeping the queue
  and delivery surfaces discoverable from the same reflection tree.
- The tensor-pipeline reflection snapshot now lives under
  `generated/runtime-reflection/tensor-pipelines` with a descriptor-rooted
  companion under `generated/runtime-reflection/tensor-pipelines/descriptors`;
  pass metadata resolves GPU kernel source ids and paths through the shared
  source registry and GPU reflection catalog so downstream tooling can query
  pipelines, domains, priority, residency, stages, tensor roles, and pass
  source-id/path indexes without reopening the manifest.
- The source-registry snapshot now also emits a descriptor document under
  `generated/runtime-reflection/source-registry/descriptors` so downstream
  consumers can read the shared registry contract without joining only the
  monolithic catalog snapshot. Its workspace-preset indexes cover the full
  `workspace_presets.json` manifest, while the launch/receipt catalogs stay
  example-scoped.
- The launch-profile snapshot now also emits a descriptor document under
  `generated/runtime-reflection/launch-profiles/descriptors` so downstream
  consumers can inspect the launch binding contract without reopening only the
  catalog snapshot.
- Runtime-compatibility snapshots also emit descriptor-rooted documents under
  `generated/runtime-compatibility/descriptors` so downstream consumers can
  inspect launch-readiness, matrix, and feature-pack views without rebuilding
  joins locally. The descriptor folder now also carries its own README, and the
  reflection generator emits that file so the compatibility subtree is
  navigable without opening only the parent catalog file.
- Runtime-app reflection is materialized under
  `generated/runtime-reflection/runtime-apps` with descriptors under
  `generated/runtime-reflection/runtime-apps/descriptors`, exposing host, runtime,
  namespace, source, and output-target indexes without rehydrating the full
  source-registry catalog.
- Workspace-preset reflection snapshots now also emit descriptor-rooted
  documents under `generated/runtime-reflection/workspace-preset-*/descriptors`
  so downstream consumers can read a single descriptor file instead of joining
  only the top-level catalog snapshots.
- Jobs reflection snapshots under `generated/runtime-reflection/jobs-*` are
  backed by canonical queued/running/failed/completed receipts plus a retry
  ledger and lifecycle/transition indexes generated from committed samples.
- `tools/reflection/generate_runtime_reflection_catalogs.ps1` is the current
  regeneration entrypoint for the committed catalog snapshots, including the
  workspace-preset reflection catalogs and descriptor documents under
  `generated/runtime-reflection/workspace-preset-*`.
- The runtime app list is intentionally broader than the authored source list:
  most entries are downstream projections of the same workbench source, not
  separate app trees.

## Scene Composition Spine

The highest-leverage 3D reuse path in this template is the shared scene spine.
When adding capability, start by checking whether the behavior belongs in one of
these contracts instead of a new local tool implementation:

- `scene_runtime` for authored scenes, layers, cameras, and world metadata
- `scene_exchange_runtime` for USD-style stage composition and handoff
- `scene_semantics_runtime` for collections, query, and semantic views
- `scene_bundle_runtime` for packaged scene composition and launch presets
- `viewport_runtime` for camera/view presentation and review-safe framing
- `camera_runtime` for capture routes, lens metadata, and tracked sync
- `interaction_runtime` for gizmo and input routing
- `mesh_runtime` for topology, remesh, LOD, and UV policy
- `lighting_runtime` for probe bake, shadow, and exposure policy

If a new 3D feature cannot be expressed through that spine, document the gap in
`limitations.md` before adding ad hoc host code.

## Design Goals

- Prefer reusable manifests, runtime packs, kernels, and entry apps over
  one-off lanes.
- Keep the template downstream-friendly so project users can build from the
  packaged template rather than from source toolchain prerequisites.
- Treat `manifests/sources.json` as the shared source registry, and have
  runtime app, workspace preset, and GPU kernel rows point back to it with explicit
  `source_id` references instead of relying only on repeated `source_path`
  strings. Projected `source_path` values should come from `manifests/sources.json`
  rather than being repeated inline in projection-heavy manifests.
- Treat `manifests/engine_systems.json` the same way so engine-system lanes stay
  source-id-first and the reflected path is projected rather than duplicated.
- Use SPIR-V and tensor pipelines when they materially improve the 3D path.
- Favor Kain UI and Kain-native runtime wiring where it reduces duplication or
  unlocks clearer product behavior.
- Avoid duplicating runtime apps that point at the same source unless there is a
  real packaging or behavior difference.

## Maintenance Rules

- Keep template behavior data-driven rather than hardcoded.
- Prefer small, composable reusable assets over broad shallow expansion.
- Record upstream gaps in `limitations.md` instead of hiding them behind local
  workarounds.
- Update `MEMORY.md` after changes that materially affect template behavior,
  layout, or known constraints.

## Common Errors

- Letting a temporary workaround become the template contract.
- Adding duplicate runtime entrypoints when a manifest or pack-level switch
  would keep the surface smaller.
- Pulling in upstream language/runtime changes into the template when a local
  manifest or pack adjustment is sufficient.
- Treating runtime-app projections as separate authored sources when they are
  manifest-driven lanes over the same workbench entrypoint.
