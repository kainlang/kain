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

## 2026-04-16 - 3D lane wiring guarded end-to-end

Extended the same validator to also verify the primary `universal_3d_workbench`
runtime app and workspace preset stay wired to `universal_3d_workbench_app`
with `native_ui` hosting. That makes the spine check cover both the shared
runtime contracts and the flagship 3D launch lane.

Why this matters: it closes the gap between "the 3D contracts exist" and "the
actual workbench route still points at them," which is the easiest place for
3D template drift to hide.

Next recommended step: keep the validator treated as a mandatory template-level
3D gate in the steering docs and wire it into regen or CI so drift fails by
default.

## 2026-04-17 - Scene spine validator promoted to an explicit steering gate

Updated `.specs/steering/dcc-native-authoring.md` so the shared scene spine
validator is now called out as a mandatory check for template-level 3D changes.
This makes the reusable scene/viewport/camera/interaction/mesh/lighting spine a
formal contract instead of only a helper script.

Why this matters: 3D growth stays centered on shared contracts and manifest
wiring, which keeps reusable behavior from fragmenting into app-local glue.

Next recommended step: wire the validator into the 3D regen or CI path so the
spec rule becomes automatic enforcement.

## 2026-04-17 - Template 3D check runner added

Added `apps/3D/tools/validation/run_template_checks.py` as a single entrypoint
for template-level 3D validation. It now runs the shared scene-spine validator
and checks the primary workbench launch bindings in `KAIN.toml`, which makes it
easier to wire the 3D lane into CI or regeneration without duplicating shell
glue.

Why this matters: the 3D template now has one durable command-shaped check that
captures both the reusable scene spine and the flagship launch binding.

Next recommended step: add this runner to the template regen or CI path so the
scene spine and workbench binding checks fail automatically on drift.

## 2026-04-16 - Scene composition staging surfaced in renderer diagnostics

Added an explicit `composition_stage` signal to the 3D renderer diagnostics,
derived from scene bounds. The new labels are `staged-line`, `staged-plane`,
`staged-stack`, and `staged-volume`, and they now flow through both the
software and WGPU render paths.

Why this matters: viewport tooling can now distinguish scene composition shape
without inferring it from the longer summary string, which makes framing,
layout, and composition debug overlays easier to consume in 3D shells.

Next recommended step: expose `composition_stage` in any 3D UI/debug HUD that
already consumes `FrameDiagnostics`, then add a scene fixture that exercises all
four stage labels.
