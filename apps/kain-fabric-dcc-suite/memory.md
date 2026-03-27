# Kain Fabric DCC Suite Memory

This file preserves the durable design intent for `apps/kain-fabric-dcc-suite`.

## 2026-03-27 (Later) - Live Command Session Host Bridge Landed

- Replaced the old minimal runtime snapshot materializer with a host-compatible snapshot plus live `session_document.json` and `command_queue.jsonl` seed flow in `scripts/materialize-session-state.ps1`.
- Extended `crates/kain-ui-native` so runtime snapshot commands are now actionable: the desktop topbar and runtime inspector can emit command requests into a JSONL bridge sink, and the topbar now reflects DCC session state such as active mode, tool, gizmo state, frame, dirty-count, and processed-command status.
- Added `native-app/src/runtime_bridge.rs` and updated `native-app/src/main.rs` so the native launcher now spawns a background bridge loop before booting the host UI. The bridge consumes queued commands, mutates the live session document, rewrites the runtime snapshot, mirrors sidecars when both app and native-app copies exist, and relies on `kain-ui-native` hot-reload watchers to refresh the shell.
- The first bridge slice is intentionally deterministic and data-driven rather than fully semantic-complete: commands such as `workspace.switch_mode`, `tool.activate`, `gizmo.*`, `sim.tick`, `material.*`, `render.preview`, and `publish.package` now drive visible session transitions and dirty-state changes without pushing lane ownership into the native host.

Important design decision:

- The native app still does not own session truth. The bridge mutates a file-backed session document that mirrors the Kain-owned session schema, and the host UI only emits commands plus reloads projected state.

Current risk:

- The bridge currently applies deterministic command heuristics over JSON documents rather than invoking the true `session/*.kn` reducer/runtime path. It is the right bootstrap layer for a live shell, but the next durability step is to route those same commands through a typed reducer or driver contract so the bridge stops duplicating session-transition logic.

Next recommended step:

- Replace the JSON-heuristic mutations inside `native-app/src/runtime_bridge.rs` with a reducer-backed bridge contract shared with the Kain session layer, then let material/lookdev and viewport commands dispatch into real runtime services behind the same contract.

## 2026-03-27 (Later) - Native GPU Sculpt Pipeline Replaced The Placeholder Sculpt Seam

- Replaced the old preview-image C stamp seam with a real GPU-owned sculpt proof made of seeded heightfield buffers, seeded brush parameter buffers, and a dedicated `gpu_sculpt_displacement` Fabric compute step.
- Added `config/sculpt_pipeline.json` so sculpt grid size, brush center, radius, strength, falloff, invert mode, and height range are data-driven instead of being hardcoded in Kain or shader code.
- Upgraded `src/main.kn` to seed `sculpt_heightfield_src`, `sculpt_brush_params`, and a zeroed `sculpt_delta` buffer so Fabric can infer the GPU output binding shape by name.
- Reworked `src/native_sculpt_step.kn` and `native/dcc_suite_ops.*` so the native seam now summarizes GPU output into `sculpt_signature` and `sculpt_report` rather than pretending C owns the sculpt mutation itself.
- Rewired the Rust topology analysis, publish bridge, Fabric pipeline manifest, sculpt/topology/publish intent manifests, and the resource/report registries to treat `gpu_sculpt_displacement.sculpt_delta` as the canonical sculpt artifact.

Important design decision:

- This pass intentionally stops at a heightfield sculpt proof instead of pretending the app now has a full mesh-surface sculpt engine. That keeps the implementation honest while still making GPU ownership real and testable inside the current Fabric architecture.
- The sculpt compute kernel is intentionally branchless because the current Kain HLSL backend does not yet lower general `if` blocks inside compute shaders. Brush inversion and radius masking are expressed through scalar math instead of control flow.

Current risk:

- The sculpt lane is now structurally real in Fabric, but it still works over a synthetic seeded heightfield rather than actual mesh topology, tablet input, multiresolution subdivision, or sparse voxel data.

Next recommended step:

- Replace the seeded heightfield source with a real mesh or sculpt-tile artifact contract and add a native or Rust bridge that can project brush strokes into GPU-friendly tiles or patches without giving semantic ownership back to the host.

## 2026-03-27 (Later) - Shader Library Expanded Beyond The Single Material Preview Pass

- Added `config/shader_catalog.json` so the suite now has a manifest-owned record of which shader families exist, which lane each belongs to, and which Fabric graphs already wire them.
- Preserved the existing `shaders/sculpt_heightfield_apply.kn` seam and expanded `shaders/` with broader material, render, compositor, and viewport coverage instead of leaving the suite centered on one narrow preview shader.
- Rewired `fabric/intents/render_preview.fabric.toml` to use a dedicated render-preview lighting shader and added `src/render_preview_projection.kn` so render reports now summarize a render-specific GPU pass instead of a generic session string.
- Rewired `fabric/intents/publish_package.fabric.toml` so publish-time material export uses a dedicated channel-pack shader instead of reusing the material preview shader.
- Rewired `fabric/intents/compositor_rebuild.fabric.toml` to run a compositor tone-map shader before report emission, and updated `src/compositor_rebuild_step.kn` to report GPU buffer metadata from that pass.

Important design decision:

- The shader catalog is intentionally broader than the currently scheduled Fabric graphs. That keeps the suite honest about what it still needs while still landing real shader coverage in the highest-value wired lanes first.

Current risk:

- The new shader files are structurally aligned with the current Fabric GPU contract, but most of the newly added library shaders are staged assets rather than fully orchestrated multi-pass pipelines. They still need richer buffer/image contracts before they can behave like a production painter, renderer, or compositor.

Next recommended step:

- Introduce richer shared image and intermediate buffer contracts for the material, render, and comp graphs so the staged shader library can be wired as true multi-pass GPU flows instead of single-buffer transforms.

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

## 2026-03-27 (Later) - Fabric Root Alignment And Full Suite Validation

- Reviewed the root `README.md` and `FABRIC.md` doctrine and aligned the app-local Fabric notes with the repo-wide model: explicit manifest DAG ownership, `value` outputs for contracts and receipts, and `shared_*` outputs only for hot payload lanes.
- Corrected the lane manifests under `fabric/intents/` so the affected graphs resolve `[workspace].root = "../.."` from the app root rather than from `fabric/intents/`. This prevents interactive intent graphs from silently resolving scripts, sources, shaders, or state receipts against the wrong cwd.
- Re-ran `cargo run -p cli --bin kain -- fabric validate --manifest apps/kain-fabric-dcc-suite/KAIN.fabric.toml` successfully from `M:/Code/Kain`.
- Re-ran `cargo run -p cli --bin kain -- fabric run --manifest apps/kain-fabric-dcc-suite/KAIN.fabric.toml` successfully from `M:/Code/Kain`, including the painter material chain `material_authoring_projection -> svg_material_mask_projection -> gpu_material_preview -> material_texture_export_projection`.
- Re-materialized `state/runtime_snapshot.json` and `state/session_document.json` so the shell-facing state reflects the verified Fabric session.

Current durable state:

- The broad suite and the painter-style material lane both execute end-to-end under the real Fabric runner, not just under static manifest inspection.
- The app still uses orchestration-grade material receipts rather than a true native brush, sparse texture, or GPU baking runtime.

Next recommended step:

- Build the next execution layer as a real painter service that consumes the current material and SVG receipts directly, instead of leaving preview and export as the terminal implementation.

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

## 2026-03-27 (Later) - First Live Command Queue Bridge Landed

- Added `scripts/queue-command.ps1` and `scripts/process-command-queue.ps1` as the first real interactive control-loop bridge for the suite.
- The new bridge reads `state/command_queue.jsonl`, applies command effects into `state/session_document.json`, derives an intent queue, updates `state/runtime_snapshot.json`, and mirrors both documents into `native-app/state/`.
- The first validated vertical slice is lookdev/material authoring: queueing `material.author_texture_set` now switches the app into `material_lookdev`, updates the active texture set and paint resolution, marks material/render/compositor dirty, and queues `material.bake_preview` plus `render.preview`.
- Updated `scripts/build-native-ui.ps1` so bundle generation now runs state materialization, command processing, and shell regeneration in sequence instead of treating the runtime snapshot as static.

Important design decision:

- This pass intentionally keeps the live bridge outside the native host for now. The command queue and runtime snapshot are the first executable seam between shell interaction and Fabric work, which lets the app prove the control loop before pushing more logic into runtime-specific code.

Current risk:

- `scripts/materialize-session-state.ps1` still regenerates a baseline session document, so the bridge currently acts as an incremental command pass layered over a freshly materialized state. A deeper future pass should preserve and evolve session truth continuously instead of re-seeding it before every build flow.

Next recommended step:

- Replace the file-based queue with a true host/session dispatcher that can consume commands continuously, debounce intent scheduling, and hand the queued intents directly to Fabric execution without a full rematerialize cycle.
