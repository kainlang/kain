# Kain Fabric DCC Suite Memory

This file preserves the durable design intent for `apps/kain-fabric-dcc-suite`.

## 2026-03-27 (Later) - Painter-Style PBR And SVG Material Pipeline Added

- Expanded the material lane from a single preview-bake scaffold into a painter-style authored pipeline with first-class texture sets, layer stacks, SVG masks, smart materials, and packed export presets.
- Added new config-owned material surfaces, tools, commands, runtime packs, resources, reports, and automation jobs so the shell and operator model understand the lane without hardcoded host logic.
- Extended `session/session_schema.kn`, reducers, planner, handler catalog, and registries so material authoring, SVG mask edits, and texture export requests are first-class session truth instead of UI-only strings.
- Upgraded `src/main.kn` to seed material and SVG contract documents and added `src/material_authoring_projection.kn`, `src/svg_material_mask_projection.kn`, and `src/material_texture_export_projection.kn` to materialize durable receipts into `state/`.
- Rewired `KAIN.fabric.toml`, `fabric/intents/material_bake.fabric.toml`, `fabric/intents/render_preview.fabric.toml`, and `fabric/intents/publish_package.fabric.toml` so the material lane now runs as authoring projection -> SVG projection -> GPU preview -> export projection.
- Regenerated `generated/main.generated.kn` and `state/runtime_snapshot.json` from the updated registries.

Important design decision:

- No compiler or language-core changes were made in this pass because the broader Kain repo already has PBR/material-graph concepts. The gap in this app was ownership and orchestration, so the new work lives in app-level Kain authoring and Fabric graphs.

Current risk:

- The material lane is now structurally much closer to a Substance Painter-style workflow, but the bake/export execution is still orchestration-grade. There is not yet a native tiled brush engine, sparse texture runtime, or true GPU baker behind the new receipts.

Next recommended step:

- Replace the current material export and preview seams with a real native painter runtime or Rust/WGPU baking service that consumes the authored texture-set and SVG receipts as execution truth.

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

## 2026-03-27 (Later) - First End-To-End Fabric Pass Succeeded

- The suite now completes a full `kain fabric run --manifest apps/kain-fabric-dcc-suite/KAIN.fabric.toml` session across python, kain, gpu_compute, c_abi, rust_crate, and publish stages.
- Added Windows symbol exports in `native/dcc_suite_ops.h` so the Fabric `c_abi` bridge can resolve `dcc_suite_apply_sculpt_stamp` and `dcc_suite_signature` from `native/dcc_suite_ops.dll`.
- Fixed `shaders/material_bake_preview.kn` to use the trailing-comma `comptime` tuple style expected by the current Fabric GPU parser.
- Added a local `[workspace]` table to `local_crate/Cargo.toml` so the Fabric rust-crate loader can resolve `fabric_dcc_suite_runtime` without modifying the top-level monorepo workspace.
- Re-ran the shell and runtime materializers so `generated/main.generated.kn` and `state/runtime_snapshot.json` reflect a successful Fabric session rather than stale scaffold output.
- Materialized a native UI bundle under `apps/kain-fabric-dcc-suite/native-app` with `kain build native-ui ... --bundle-only`.

Next recommended step:

- Replace the current mock execution seams in sim, tensor bridge dispatch, and compositor with real runtime work now that Fabric convergence is proven.

## 2026-03-27 (Later) - Universal Gizmo System Added

- Added `config/gizmo_registry.json` as the app-owned source of truth for universal viewport gizmo policy, including hotkeys, drag trigger, snap increments, and per-viewport binding.
- Extended `config/surfaces.json`, `config/tool_catalog.json`, and `config/command_registry.json` so viewport surfaces, tools, and commands can describe gizmo participation without pushing that meaning into the native host.
- Extended `session/session_schema.kn`, `session/reducers.kn`, and `session/command_handlers.kn` with durable gizmo concepts so future session-to-host bridges have explicit ownership seams.
- Updated the shell and runtime snapshot materializers so generated UI and `state/runtime_snapshot.json` expose the same gizmo contract the host consumes.

Current durable state:

- The native viewport can now consume bundle-authored gizmo defaults instead of relying on one hardcoded ctrl-drag path.
- Tool metadata now declares whether a lane participates in the universal gizmo and which default mode or space it prefers.
- The registry currently drives viewport defaults and runtime metadata, but live tool-activation to viewport-policy sync still needs a dedicated session bridge.

Next recommended step:

- Add a session-to-host command bridge so `tool.activate`, `gizmo.set_mode`, `gizmo.set_space`, and `gizmo.toggle_snap` can update the live native viewport without relying only on authored defaults.

## 2026-03-27 (Later) - Universal Studio UI System Added

- Added `config/ui_theme.json` and `config/ui_shell.json` as manifest-owned inputs for a page-based universal studio shell with theme scopes, variants, and workspace-specific workbenches.
- Added `session/ui_workbench_registry.kn` so the workbench contract also exists as typed Kain-owned semantics instead of only inside generated UI output.
- Replaced the shell materializer with a richer projection that emits workspace pages, viewport-first stage decks, inspector rails, telemetry trays, and manifest-driven operator copy.
- Expanded `state/runtime_snapshot.json` materialization so future shell or host consumers can read surfaces, runtime packs, pipeline steps, intents, and UI manifest metadata from one projected document.

What future work should preserve:

- keep the generated shell projection disposable while preserving UI truth in the manifests and Kain workbench registry
- keep workspace pages aligned with the session and intent system instead of letting the native host invent separate navigation truth
- keep the shell explicit about extension seams so simulation, compositor, and tensor pages do not imply more runtime completeness than the suite currently has

Next recommended step:

- connect the universal shell pages to real interactive command dispatch, layout persistence recovery, and runtime snapshot deltas so the current manifest-rich projection becomes a live editor shell rather than a static generated studio frame

## 2026-03-27 (Later) - Sim, Compositor, and Tensor Receipts Materialized as Real Lane Artifacts

- Replaced the cwd-relative receipt writes in the sim, compositor, and tensor bridge steps with explicit app-rooted paths under `apps/kain-fabric-dcc-suite/state/`.
- Added dedicated Kain receipt emitters for simulation planning and compositor planning so those lanes now leave durable JSON artifacts instead of only returning mock summary strings.
- Upgraded the Python tensor stages and Kain tensor bridges so the suite now materializes `tensor_train_dispatch.json`, `tensor_train_checkpoint.json`, `tensor_infer_dispatch.json`, and `tensor_infer_result.json` as first-class bridge artifacts.
- Normalized the lane manifests under `fabric/intents/` to resolve from the app root and to declare the full seeded output set expected by `dcc_suite_seed`.
- Verified the focused lane manifests for `sim_tick`, `compositor_rebuild`, `tensor_train_step`, and `tensor_infer_step`, then re-ran the full `KAIN.fabric.toml` suite successfully.
- Refreshed `state/runtime_snapshot.json` after the successful lane pass so the shell sees the latest Fabric success state alongside the new state receipts.

Current durable state:

- Sim now writes `sim_tick_plan.json` and `sim_tick_report.json`.
- Compositor now writes `compositor_rebuild_plan.json` and `compositor_rebuild_report.json`.
- Tensor now writes dispatch, checkpoint, and inference result receipts with explicit app-local paths shared across Python and Kain.
- These are still orchestration-grade runtime seams, not full solver/compositor/tensor engines.

Next recommended step:

- Replace the synthetic metrics inside the new sim/compositor/tensor receipts with outputs from real external runtimes or typed artifact contracts so the receipts become execution truth rather than planned scaffolding.

