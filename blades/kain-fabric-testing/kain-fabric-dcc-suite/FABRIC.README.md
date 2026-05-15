# Fabric Notes

`apps/kain-fabric-dcc-suite/KAIN.fabric.toml` is the suite's canonical cross-runtime Fabric graph.

The graph follows the root Kain Fabric model:

- use `value` outputs for authored contracts, reports, and routing receipts
- use `shared_buffer` and `shared_image` only for hot preview and tensor payloads
- keep semantic ownership in Kain/session/config and let Fabric express the explicit DAG

Current top-level step ownership:

- `python_suite_bootstrap`: seeds app defaults, preview sizing, runtime-pack counts, and session-local hints
- `dcc_suite_seed`: authors the scene graph, asset catalog, material authoring document, SVG mask document, preview payloads, tensor features, sculpt buffers, and session bootstrap report
- `material_authoring_projection`: projects painter-style texture-set, layer-stack, and export-preset receipts from session truth
- `svg_material_mask_projection`: projects SVG mask stack and vector decal receipts for the material lane
- `gpu_sculpt_displacement`: proves a GPU-owned sculpt stroke over seeded heightfield and brush buffers
- `native_sculpt_kernel`: proves the native reporting and signature seam over GPU sculpt output
- `rig_graph_analysis`: proves a Rust-owned graph and topology seam
- `tensor_train_stage`: emits tensor training readiness and plan summaries
- `tensor_infer_stage`: emits tensor inference readiness and plan summaries
- `gpu_material_preview`: runs the GPU preview after the material and SVG projections have materialized
- `material_texture_export_projection`: emits packed PBR texture export receipts for downstream runtimes and publishing
- `publish_suite_report`: emits publish and report output through the Kain-to-Node bridge

The suite now also carries a manifest-owned shader catalog in `config/shader_catalog.json` plus intent-local shader wiring for render preview, publish channel packing, and compositor tone mapping.

The intent manifests under `fabric/intents/` are the lane-local reusable graphs the session planner should schedule for interactive work. Those manifests should resolve `[workspace].root = "../.."` so scripts, source, shaders, and state receipts stay anchored to the app root instead of the `fabric/intents/` folder.
