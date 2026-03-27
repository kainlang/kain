# Kain Fabric DCC Suite Memory

This file preserves the durable design intent for `apps/kain-fabric-dcc-suite`.

## 2026-03-27 - Flagship Fabric DCC Suite Scaffold Added

The repo now has a flagship DCC suite scaffold under `apps/kain-fabric-dcc-suite`.

What changed:

- Added a registry-driven app package with explicit manifests for workspaces, surfaces, commands, Fabric pipeline summary, Fabric intents, resources, reports, runtime packs, and automation jobs.
- Added a focused Kain session core split across schema, reducers, derived state, command handlers, intent planner, and typed registries.
- Added a broad Fabric pipeline plus lane-specific intent manifests that cover bootstrap, ingest, sculpt, topology, rig, sim, material, render, compositor, publish, and tensor-oriented work.
- Added narrow native C and Rust proof seams plus a GPU compute shader and a Kain-to-Node publish bridge.
- Added local docs, generated shell scaffolding, and runtime snapshot scaffolding that explicitly call out extension seams instead of pretending the runtime is already complete.

What future work should preserve:

- keep workspace, runtime-pack, command, report, and automation truth data-driven through `config/*.json`
- keep live operator state and dirty-state planning in `session/*.kn`
- keep Kain plus Fabric as the semantic owners of the suite and avoid letting native host code absorb lane meaning
- keep tensor, sim, and compositor lanes honest about current runtime gaps until first-class contracts land

## 2026-03-27 (Later) - Added Explicit Extension Seams for Sim and Compositor
- Created `src/sim_solver_step.kn` and `src/compositor_rebuild_step.kn` to act as explicit execution steps in their respective Fabric intent graphs.
- Documented the current Kain runtime limitations (lack of physics solvers, image processing nodes) within these steps, providing a clean landing zone for future external integrations.

## 2026-03-27 (Later) - Added Explicit Extension Seam for Material Baking
- Created src/material_bake_step.kn to handle the material_bake Fabric intent.
- Documented the current Kain runtime limitations (lack of native GPU texture baking) and provided a clean extension seam for a future Rust/WGPU or C++/Vulkan texture baking pipeline.
- Linked the step to the material_bake_preview.kn shader for visual inspection.

## 2026-03-27 (Later) - Explicit Extension Seams for Tensor Operations
- Converted \src/tensor_train_bridge.kn\ and \src/tensor_infer_bridge.kn\ into explicit execution steps for their respective Fabric intent graphs.
- Documented Kain runtime limitations (lack of native autograd engine and GPU tensor types) and created clean IPC/ABI bridges to external PyTorch/ONNX processes.

## 2026-03-27 (Later) - Explicit Extension Seam for Rig Solving
- Created `src/rig_solve_step.kn` to act as an explicit execution step for the `rig_solve` Fabric intent.
- Documented the current Kain runtime limitations (lack of a native high-performance IK solver and bone evaluation engine) and provided a clean FFI extension seam for a future Rust/C++ animation engine.

## 2026-03-27 (Later) - Explicit Extension Seam for Asset Ingest
- Expanded `src/asset_ingest_step.kn` to handle the `asset_ingest` Fabric intent properly.
- Documented the current Kain runtime limitations (lack of native, high-performance USD/Alembic parsing) and provided a clean FFI extension seam delegating the heavy lifting of binary-to-memory geometry parsing to a native Rust crate `kain_asset_ingest_rs` via C ABI boundary.
## 2026-03-27 (Later) - Explicit Extension Seam for Sculpting
- Created \src/sculpt_brush_step.kn\ to act as an explicit execution step for the \sculpt_brush_stroke\ Fabric intent.
- Documented the current Kain runtime limitations (lack of a native high-performance BVH and spatial hash) and provided a clean FFI extension seam delegating the brush stroke to a C++/Rust compute kernel over shared memory.


## 2026-03-27 (Later) - Explicit Extension Seam for Topology Rebuild
- Created \src/topology_rebuild_step.kn\ to act as an explicit execution step for the \	opology_rebuild\ Fabric intent.
- Documented the current Kain runtime limitations (lack of native, high-performance half-edge data structures and algorithms) and provided a clean FFI extension seam delegating mesh retopology to a native C++/Rust library.

