# Fabric Notes

`apps/kain-fabric-dcc-suite/KAIN.fabric.toml` is the broad scaffold pipeline for the suite.

Current top-level step ownership:

- `python_suite_bootstrap`: seeds app and session defaults plus lane counts
- `dcc_suite_seed`: authors scene, asset, preview, tensor, and session bootstrap artifacts
- `native_sculpt_kernel`: proves a native sculpt and mutation seam
- `rig_graph_analysis`: proves a Rust-owned graph and topology seam
- `tensor_train_stage`: emits tensor training readiness and plan summaries
- `tensor_infer_stage`: emits tensor inference readiness and plan summaries
- `gpu_material_preview`: proves GPU compute preview and material bake flow
- `publish_suite_report`: emits publish and report output through the Kain-to-Node bridge

The intent manifests under `fabric/intents/` are the lane-local reusable graphs the session planner should schedule for interactive work.
