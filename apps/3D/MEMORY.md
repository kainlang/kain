# Memory

## 2026-04-15 - 3D Manifest Projection Validator Links Workspace Presets to Runtime Apps

Expanded `scripts/python/validate_3d_template_manifests.py` so the 3D template
validator now also checks that each `workspace_presets.json` row resolves to a
real `runtime_apps.json` row, in addition to the shared source registry, engine
systems, runtime apps, GPU kernels, and tensor pipelines.

Why this mattered:

- workspace presets are the 3D operator routing layer, so broken preset-to-app
  links now fail fast instead of surfacing later in materialization
- the template stays more explicitly data-driven as the render and scene
  composition graph grows
- the validator now covers the full projection chain from source registry to
  app lane to workspace preset to tensor pipeline

Validation:

- ran the validator directly after the change
- updated the template README and memory notes to reflect the broader coverage

## 2026-04-15 - Balanced scene framing and diagnostics in kain-3D

Adjusted `crates/kain-3D/src/scene.rs` so `SceneBounds::dominant_axis_label()` now recognizes near-isotropic scenes as `balanced` instead of forcing them into wide/tall/deep buckets. The framing helper now uses a balanced camera direction for those scenes, which makes auto-framing and scene-composition summaries read more honestly for cube-like layouts.

Why this mattered:

- balanced scenes were being described with a misleading shape label
- the auto-framing offset now matches the scene's actual symmetry better
- tooling gets a cleaner composition signal for logs and smoke output

Validation:

- `rustfmt crates/kain-3D/src/scene.rs`
- `cargo test -p kain-3d scene::tests::balanced_scene_reports_balanced_shape_and_camera_direction -- --nocapture` was blocked by the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`)

## 2026-04-15 - 3D Manifest Projection Validator Expansion

Expanded `scripts/python/validate_3d_template_manifests.py` so the 3D template
validator now covers `engine_systems.json` in addition to the shared source
registry, runtime apps, and workspace presets.

Why this mattered:

- engine systems are a first-class projection surface in the 3D template, not
  just documentation
- the validator now catches broken `source_id` links before they leak into the
  reflection/catalog layer
- this keeps the scene/composition/tooling lattice manifest-driven as it grows

Validation:

- ran the validator directly after the change
- updated the template README to advertise the broader coverage

## 2026-04-14 - 3D Template Manifest Projection Validator

Added `scripts/python/validate_3d_template_manifests.py` to keep the 3D
workspace template manifest-driven. The validator checks:

- `manifests/sources.json` ids are unique and well-formed
- `runtime_apps.json` rows resolve their `source_id` back to the shared source
  registry
- `workspace_presets.json` rows resolve their `source_id` back to the shared
  source registry
- runtime app output targets stay unique per app row

This is a lightweight but high-leverage guardrail for the 3D lane because the
template is projection-heavy and the source registry is the real owner of the
shared authored entrypoint.

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

The manifest layer already binds these into tensor pipelines and runtime systems, so the suite can stay data-driven instead of hardcoding tool modes. No code edits were needed in this pass.
