# Memory

## 2026-03-29 - DCC Fuel Mapping for kain-fabric-dcc-suite

Inspected the 3D template for reusable material/paint/uv/brush/sculpt/deformation fuel that can strengthen `apps/kain-fabric-dcc-suite`.
The highest-signal reusable surfaces are:

- `src-kain/stdlib/three_d_runtime/painting_runtime.kn`
- `src-kain/stdlib/three_d_runtime/material_runtime.kn`
- `src-kain/stdlib/three_d_runtime/material_source_runtime.kn`
- `src-kain/stdlib/three_d_runtime/uv_runtime.kn`
- `src-kain/stdlib/three_d_runtime/brush_runtime.kn`
- `src-kain/stdlib/three_d_runtime/deformation_runtime.kn`
- `src-kain/stdlib/three_d_runtime/mesh_runtime.kn`
- `src-kain/stdlib/three_d_runtime/tensor_runtime.kn`
- `src-kain/stdlib/three_d_runtime/engine_systems.kn`

Kain kernels worth reusing directly:

- `src-kain/kernels/painting/paint_layer_blend_tensor.kn`
- `src-kain/kernels/material/material_layer_stack_tensor.kn`
- `src-kain/kernels/material/material_source_document_resolve.kn`
- `src-kain/kernels/uv/uv_chart_pack_resolve.kn`
- `src-kain/kernels/brush/brush_dab_accumulate_resolve.kn`
- `src-kain/kernels/sculpt/sculpt_brush_tensor.kn`
- `src-kain/kernels/sculpt/dyntopo_voxel_remesh.kn`
- `src-kain/kernels/deformation/deformer_stack_resolve.kn`

The manifest layer already binds these into tensor pipelines and runtime systems, so the suite can stay data-driven instead of hardcoding tool modes.

## 2026-04-16 - Scene composition spine documented

I tightened `apps/3D/ARCHITECTURE.md` to make the core 3D composition spine explicit:
`scene_runtime`, `scene_exchange_runtime`, `scene_semantics_runtime`,
`scene_bundle_runtime`, `viewport_runtime`, `camera_runtime`,
`interaction_runtime`, `mesh_runtime`, and `lighting_runtime` are now called out
as the first stop for reusable 3D behavior.

Why this matters: future 3D work should grow through shared contracts and
manifested lanes first, which keeps the template scalable and prevents a trail
of one-off host glue from becoming the real architecture.

Next recommended step: add a small validation or reflection check that verifies
new 3D features are registered against the shared scene spine before they land
as standalone app-specific behavior.

## 2026-04-16 - Scene spine validator added

Added `apps/3D/tools/validation/validate_scene_spine.py` as a lightweight
manifest guardrail for the shared 3D spine. It checks the scene, exchange,
semantics, bundle, viewport, camera, interaction, mesh, and lighting runtime
systems against `manifests/engine_systems.json` and `manifests/sources.json`.

Why this matters: the template now has a direct, runnable check that catches
scene-spine drift before it becomes app-local glue or a hidden manifest hole.

Next recommended step: wire the validator into the template's reflection or CI
lane so spine drift fails automatically during regeneration.
