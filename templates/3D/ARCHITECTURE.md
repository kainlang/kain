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
  `source_id` references back to `manifests/sources.json`, so the shared
  authored workbench source stays visible even when many manifest rows project
  from it.
- Committed reflection snapshots live under `generated/runtime-reflection`,
  `generated/resource-reflection`, and `generated/runtime-compatibility`.
- `tools/reflection/generate_runtime_reflection_catalogs.ps1` is the current
  regeneration entrypoint for the committed catalog snapshots.
- The runtime app list is intentionally broader than the authored source list:
  most entries are downstream projections of the same workbench source, not
  separate app trees.

## Design Goals

- Prefer reusable manifests, runtime packs, kernels, and entry apps over
  one-off lanes.
- Keep the template downstream-friendly so project users can build from the
  packaged template rather than from source toolchain prerequisites.
- Treat `manifests/sources.json` as the shared source registry, and have
  runtime app and workspace preset rows point back to it with explicit
  `source_id` references instead of relying only on repeated `source_path`
  strings.
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
