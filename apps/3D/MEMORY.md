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

The manifest layer already binds these into tensor pipelines and runtime systems, so the suite can stay data-driven instead of hardcoding tool modes. No code edits were needed in this pass.
