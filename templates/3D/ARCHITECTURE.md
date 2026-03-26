# Universal Kain 3D Template Architecture

## Purpose

`/mnt/m/Templates/3D` is a Kain-first universal template for building 3D runtime applications, DCC suites, game engines, sculpt tools, XR shells, review tools, and delivery pipelines without requiring downstream users to install `rustc` or `cargo`.

The template is intentionally platform-like, with explicit substrate for scene objects, native controls, runtime/import-export schemas, physics, shader permutation management, resource residency, source-level materials, editor widget suites, scene mutation receipts, render delegation, resource reflection, and runtime compatibility instead of leaving those engine-grade systems implicit:


- authored behavior lives in Kain source under `src-kain`
- system registration is data-driven through JSON manifests under `manifests`
- GPU work is modeled through authored SPIR-V kernel seeds and tensor pipelines
- the default shell is a Kain UI native workbench
- generated artifacts are outputs of manifests and build graphs, not hand-edited source
- engine-grade mesh, baking, scripting, AI, and modding lanes are modeled as first-class stdlib/manifests/kernels instead of host-local glue
- missing language/runtime capability is recorded in [`limitations.md`](/mnt/m/Templates/3D/limitations.md) instead of hidden in host-specific code

## Main Folders

- [`/mnt/m/Templates/3D/src-kain/apps/universal_3d_workbench/main.kn`](/mnt/m/Templates/3D/src-kain/apps/universal_3d_workbench/main.kn)
  Primary native-ui authoring shell exposing the registered platform lanes.
- [`/mnt/m/Templates/3D/src-kain/stdlib/three_d_runtime`](/mnt/m/Templates/3D/src-kain/stdlib/three_d_runtime)
  Reusable runtime/system contracts. This is the main place to add new platform capability instead of hand-writing app-local logic.
- [`/mnt/m/Templates/3D/src-kain/kernels`](/mnt/m/Templates/3D/src-kain/kernels)
  Authored SPIR-V/tensor seeds grouped by domain.
- [`/mnt/m/Templates/3D/manifests`](/mnt/m/Templates/3D/manifests)
  Source of truth for registered systems, kernels, tensor pipelines, canonical runtime apps, workspace presets, UI surfaces, build graphs, distribution channels, and source assets.
- [`/mnt/m/Templates/3D/generated`](/mnt/m/Templates/3D/generated)
  Disposable build output roots. Do not hand-edit generated artifacts.
- [`/mnt/m/Templates/3D/limitations.md`](/mnt/m/Templates/3D/limitations.md)
  Upstream Kain/runtime gaps that should be added to the language/runtime.
- [`/mnt/m/Templates/3D/MEMORY.md`](/mnt/m/Templates/3D/MEMORY.md)
  Durable local task memory for future agents extending the template.

## Data Flow

1. Add or expand reusable contracts under `src-kain/stdlib/three_d_runtime`.
2. Add matching authored kernels under `src-kain/kernels` only when the capability truly belongs in the SPIR-V/tensor lane.
3. Register everything in the manifests: `engine_systems.json`, `gpu_kernels.json`, `tensor_pipelines.json`, `runtime_apps.json`, `workspace_presets.json`, `ui_surfaces.json`, `build_graphs.json`, `distribution_channels.json`, and `sources.json`.
4. Expose the new lane in the universal workbench shell and docs.
5. If Kain/runtime lacks a required primitive, record it in `limitations.md` rather than hiding it in template-local engine code.

## Architectural Rules

- Keep the template Kain-owned. FFI is allowed, but only as a contract-driven extension surface.
- Prefer stdlib/runtime packs over app-local one-off logic.
- Prefer manifest registration over hardcoded routing or special cases.
- Treat `runtime_apps.json` as the canonical packaging/runtime-target catalog and `workspace_presets.json` as the lane-selection catalog. Do not model every workspace lane as a separate app when the source and packaging are the same.
- Keep workbench preset UX manifest-backed. The shell can expose a small shortcut layer, but the exhaustive preset catalog belongs in `workspace_presets.json` and preset-sensitive materializers should consume that manifest directly.
- Prefer explicit workspace-preset export receipts and registries over burying preset launch routing inside generic bundle or UI logic.
- When a graph-materialization lane consumes workspace presets, declare the exact manifest inputs, output roots, and delivery/bundle consumers inside the reusable runtime pack instead of leaving the bridge as naming-only metadata.
- Prefer deepening reusable capture/review/delivery packs such as virtual production, lookdev, runtime reflection, and runtime bundles before adding more lane labels or duplicate runtime apps.
- Prefer adding source/runtime primitives to the template over writing manual 3D host code.
- Treat SPIR-V/tensor lanes as first-class systems, not demo artifacts.
- Treat engines, DCC tools, review pipelines, packaging, streaming, and delivery as one coherent runtime platform.

## Common Commands

Heavy validation was intentionally avoided in recent runs, but the relevant focused commands are:

```bash
cd /mnt/m/Templates/3D
# Use when validation is explicitly desired later:
# kain build
# kain check
```

Upstream capability references from the Kain/ZenDCC stack:

```bash
cd /mnt/m/Code/Kain
# cargo run -q -p cli -- doctor
# cargo run -q -p cli -- build native-ui --help
# cargo run -q -p cli -- gpu-artifacts --help
```

## Common Errors

- Do not add Rust host projects to this template. Downstream consumers should remain Rust-toolchain-free.
- Do not hide missing runtime surfaces in bespoke host code. Add them to `limitations.md`.
- Do not treat `generated/` as authored source.
- Do not add narrow app-local 3D logic when the right home is a reusable stdlib/runtime pack.
- When adding a new lane, keep manifests, docs, workbench UI, and memory in sync or future automation runs will drift.
- Prefer scene-object, schema, physics, shader, resource, material-source, scene-mutation, and compatibility capability to land as reusable runtime packs before inventing app-local engine code.

## Recent Expansion Notes

The template now also carries first-class identity, cloud, marketplace, dataops, fleet, rigging, deformation, painting, UV, and brush substrate. Workspace-preset routing is explicitly modeled as a reusable manifest-backed runtime contract rather than a growing UI-local table, and preset exports now have a dedicated materialization graph plus delivery registry. The current graph-materialization pack also declares the concrete preset-export consumer path, including manifest inputs, launch/receipt/reflection roots, and runtime-bundle delivery handoff, so future generators can extend one shared contract instead of inventing parallel export glue. Future buildouts should continue treating these as reusable Kain-owned runtime packs and manifest lanes rather than external service glue or app-local editor code.
