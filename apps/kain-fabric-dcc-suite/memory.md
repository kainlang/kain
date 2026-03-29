# Kain Fabric DCC Suite Memory

## 2026-03-29 - Asset Pipeline Manifest Surfaced In The Shell

- Added a dedicated `asset_pipeline_manifest` inspector surface in `config/surfaces.json` and surfaced it in the scene and publish workbenches through `config/ui_shell.json` plus `session/ui_workbench_registry.kn`.
- The new surface makes the source-id-first intake policy, lineage chain, transcode profile set, and routed runtime matrix visible alongside the existing asset registry instead of leaving the asset lane as an implicit contract.
- Reran `scripts/materialize-shell.ps1` and `scripts/materialize-session-state.ps1` so `generated/main.generated.kn`, `state/runtime_snapshot.json`, and `state/session_document.json` stayed aligned with the authored registry after the shell update.
- Clean next seam: wire richer residency/manifest telemetry from a real importer or interchange runtime when that lane is ready.

## 2026-03-28 - Runtime Lane Docs Now Match The Registry

- `README.md` and `ARCHITECTURE.md` now describe `config/runtime_lanes.json` as the explicit source of truth for the Kain / Fabric / Python / GPU / native C / Rust / Node ownership matrix.
- This cleans up stale prose that still implied the runtime-lane registry was only a future seam, which keeps the scaffold easier to audit and less likely to drift from the authored config.
- The next clean seam is still live consumption: keep threading the registry into live chrome and bridge consumers so operators see the same matrix outside the docs.

## 2026-03-28 - Derived State Now Accepts Authored Runtime Lane Data

- `session/derived_state.kn` no longer hardcodes the lane count and lane summary; it now accepts the authored runtime-lane values as inputs so the read model can stay aligned with `config/runtime_lanes.json` instead of repeating registry truth in code.
- This keeps the app's semantic read model honest and makes the runtime-lane contract easier to extend without editing the derived-state logic every time the registry changes.
- The next clean seam is to wire the same registry-backed values through any remaining shell or bridge projections that still infer lane ownership from static prose.

- 2026-03-28 - Added a new high-fuel render lane to `kain-fabric-dcc-suite`: `render.pathtrace_preview`.
- Wired a new SPIR-V-flavored compute shader at `shaders/pathtrace_preview_lighting.kn`, a Fabric intent graph at `fabric/intents/render_pathtrace.fabric.toml`, and a Kain projection at `src/pathtrace_preview_projection.kn`.
- Updated `src/main.kn` so the seed contract now returns `pathtrace_dst`, and updated the render dispatcher so `render.preview` schedules `render.pathtrace_preview`.
- Validated the lane end-to-end: queueing `render.preview` now runs both `render.preview` and `render.pathtrace_preview` successfully through Fabric.
- The app now has a legitimate path-traced preview spine, not just a standard render preview.

## 2026-03-29 - Shell Now Surfaces The Runtime-Lane Map

- `scripts/materialize-session-state.ps1` now projects `runtime_lane_summary` from `config/runtime_lanes.json` into `state/runtime_snapshot.json` so the live bridge snapshot can carry the authored lane map instead of only the lane count.
- `scripts/materialize-shell.ps1` and `config/ui_shell.json` now expose that summary as a first-class chrome metric, which makes the Kain / Fabric / Python / GPU / C ABI / Rust / Node ownership split visible in the shell instead of only in docs.
- The runtime-lane summary currently reads `kain | fabric | python | gpu_compute | c_abi | rust_crate | node_bridge`; if that registry ever changes, the shell should stay driven by the registry instead of hand-edited prose.

## 2026-03-29 - DCC Shell Fuel Pulled From `apps/3D`

- Reviewed `M:/Code/Kain/apps/3D` for reusable shell/workbench material and found the strongest carry-over candidates in `manifests/ui_surfaces.json`, `manifests/workspace_presets.json`, `manifests/runtime_apps.json`, `manifests/sources.json`, and the `src-kain/stdlib/three_d_runtime/*.kn` catalog.
- The most useful shell patterns were the explicit workspace navigator, command spotlight, status strip, report browser, and jobs monitor framing, plus the DCC-style notion that the shell should keep the active lane obvious and maintain a clear return path.
- Made a small shell reinforcement in `config/ui_shell.json` so the system rack and operator notes now call out lane visibility, report visibility, and workbench return-path clarity more directly.

## 2026-03-29 - Pathtrace Preview Lane Now Has Its Own Projection

- Added `fabric/intents/render_pathtrace.fabric.toml`, `shaders/pathtrace_preview_lighting.kn`, and `src/pathtrace_preview_projection.kn` so the render preview lane now has a dedicated path-traced branch instead of borrowing generic preview semantics.
- The pathtrace projection writes a concrete `state/pathtrace_preview_report.json` receipt, which keeps bounce-budget and preview-buffer details app-owned and inspectable.
- `scripts/materialize-session-state.ps1` and `scripts/materialize-shell.ps1` were rerun after the lane landed so the generated shell and session snapshot stay in sync with the authored render seam.
- Next step: keep building render fuel outward from this spine, especially accumulation and denoise passes if they can be added without collapsing the current scaffold honesty.

## 2026-03-29 - Render Accumulation Report Added As The Next Progressive-Preview Seam

- Added `src/render_accumulation_projection.kn` plus a matching `render_accumulation_projection` step in `fabric/intents/render_pathtrace.fabric.toml` so the pathtrace lane now emits a second, more temporal report instead of stopping at a single frame summary.
- Registered `render_accumulation_report` in the pipeline and report registries and threaded it through the `render.pathtrace_preview` intent, which makes the progressive-preview spine visible to the scaffold without inventing a new shell surface yet.
- This is still an authored reporting seam, not a true history-buffer or denoise runtime; the clean extension path is to back the report with an actual accumulation buffer once the runtime lane exists.

## 2026-03-29 - Render Denoise Readiness Report Added As The Next Progressive-Preview Seam

- Added `src/render_denoise_projection.kn` plus a matching `render_denoise_projection` step in `fabric/intents/render_pathtrace.fabric.toml` so the pathtrace lane now exposes a third report stage that explicitly frames denoise as a readiness seam instead of pretending the runtime exists.
- Registered `render_denoise_report` in the pipeline, report registry, and render intent outputs so the progressive-preview spine now reads pathtrace -> accumulation -> denoise in the authored registry graph.
- This is still an authored reporting seam, not a real denoise kernel or history-buffer runtime; the clean extension path is to back it with an actual accumulation/temporal filter lane when the native/GPU seam is ready.

## 2026-03-29 - Render Progression Made Visible In The Shell

- `config/ui_shell.json` now calls out the progressive render chain explicitly in operator notes and the render control-room hero copy, so pathtrace -> accumulation -> denoise reads as a first-class preview spine instead of hidden implied plumbing.
- `scripts/materialize-shell.ps1` and `scripts/materialize-session-state.ps1` were rerun after the shell tweak so the generated shell and live snapshot stay aligned with the authored UI registry.
- This is still a shell-level honesty pass, not a new render runtime; the next clean step is to keep turning the render chain into real history-buffer or denoise execution once the runtime seam is ready.

## 2026-03-28 - Fuel-First Direction Lock-In

- Direction update: stop treating the app like a registry demo and keep pushing product heat into the flagship lane.
- Priorities now: render/pathtrace/accumulation/denoise, asset import pipelines, UI/shell wiring, and native host/FFI seams.
- Docs only matter when they directly unblock shipping or explain a real runtime limitation.

## 2026-03-29 - Mesh Pipeline Now Carries Primitive, Subdivision, and UV Pack Seams

- Added `fabric/intents/mesh_session.fabric.toml` steps for primitive generation, topology projection, Catmull-Clark-style subdivision, UV packing, and the existing mesh session projection so the mesh lane now has a clearer authored pipeline instead of only a coarse contract report.
- Added Kain projection receipts for subdivision and UV packing, plus session/bridge command handling for `mesh.subdivide` and `mesh.pack_uv`, so the active edit target can stay explicit while heavy geometry work remains an external/native seam.
- Wired `src/mesh_edit_session_projection.kn` to emit a new native mesh runtime signature alongside the existing contract/topology signatures, backed by a C helper in `native/dcc_suite_ops.{h,c}`. That gives the mesh lane a concrete Catmull-Clark/UV helper seam instead of only orchestration text.
- Extended the mesh command registry and intent registry so the new mesh actions are discoverable from the shell and can be routed through the same reducer/plan path as the older mesh commands.
- The next clean step is to back the new receipts with a real native mesh solver and UV atlas seam instead of just durable orchestration reports.

## 2026-03-29 - Render-Room Lanes Wired In

- Added the first dedicated render-room lanes to `kain-fabric-dcc-suite`: `render.delegate_preview` and `lighting.review_preview`.
- New Fabric graphs live at `fabric/intents/render_delegate.fabric.toml` and `fabric/intents/lighting_review.fabric.toml`.
- Added Kain projection files for the new lanes: `src/render_delegation_projection.kn` and `src/lighting_review_projection.kn`.
- Extended the suite registries so these lanes are first-class in `runtime_packs`, `report_kinds`, `command_registry`, `fabric_intents`, `report_registry`, `command_handlers`, `reducers`, `intent_planner`, and the dispatcher hot-path list.
- Validation succeeded: queueing both new commands now runs Fabric and materializes `render.delegate_preview:succeeded` and `lighting.review_preview:succeeded`.
- The suite is now moving from preview-only render plumbing into a more believable render room with delegated preview and lighting review contracts.

## 2026-03-29 - Render Room Got AOV, Capture, And Visibility Fuel

- Added `render.review_capture` plus a new `fabric/intents/render_review.fabric.toml` lane so the render room now has a dedicated AOV packing and review-capture branch instead of keeping that work implicit.
- Added authored projection receipts for `render_aov_pack`, `render_review_capture`, and `render_visibility` so the room can speak in capture/evidence/culling telemetry rather than only preview and pathtrace summaries.
- Wired the new lane into the app registries, the command/intent plumbing, the native bridge, and the shell-facing docs so review capture now follows the same app-owned contract path as the earlier render-room steps.
- The staged `shaders/render_aov_pack.kn` shader is now consumed as a render-room AOV packing seam, which makes the render stack feel closer to an AAA review bench without pretending the final compositor runtime already exists.

## 2026-03-29 - Shell Workbench Tightened Toward Slate-Like Density

- `config/ui_shell.json` and `config/surfaces.json` were tightened so the shell reads more like a real workbench frame: shorter labels, denser chrome language, clearer lane return paths, and less toy-like page copy.
- The workbench now leans harder on compact navigator / command / property / status terminology, which should make the generated shell feel more deliberate once materialized.
- Keep future shell edits similarly terse and registry-driven; avoid expanding the shell back into decorative prose cards.

## 2026-03-29 - Render Room Progression Now Includes Frame Scheduling

- Added a dedicated `render_frame_schedule_projection` seam plus `render_frame_schedule_report` so the render-room path now has an explicit frame-budget and capture-scheduling receipt instead of only preview, accumulation, and denoise summaries.
- Threaded the new schedule report into render delegation and lighting review so the room can cite a concrete frame queue while still keeping the actual GPU/runtime seams honest.
- The progressive render spine is now easier to read as a room contract: delegated preview -> pathtrace -> accumulation -> denoise -> frame scheduling.
- Next clean step is to back these report receipts with a real temporal-history or render queue runtime if the platform seam opens up.

## 2026-03-29 - Asset Import Pipeline Now Has Real Lane Structure

- Pushed the asset import uplift in `kain-fabric-dcc-suite` so ingest now speaks in source-id-first manifests, interchange transcode, scene exchange, asset lineage, and media ingest terms instead of a single generic import step.
- Added app-owned projection writers for `asset_source_manifest`, `interchange_transcode`, `scene_exchange`, `asset_lineage`, and `media_ingest` receipts, and routed `asset.ingest_package` through them in `fabric/intents/asset_ingest.fabric.toml`.
- Extended the session registries and command planner so asset import can fan out into explicit routing intents instead of pretending one reducer owns the whole import story.
- The clean next seam is to back these receipts with a native importer or typed interchange runtime, because the current Kain-side projections are still orchestration-grade contracts.

## 2026-03-29 - Render Review Capture Surface Brought Into The Shell

- `config/ui_shell.json` now includes `render.review_capture` in the command spotlight and render workbench quick actions so the AOV / review-capture lane is visible from the main operator frame instead of being buried in registries only.
- `session/ui_workbench_registry.kn` now mirrors that same render lane set, keeping the authored workbench descriptor aligned with the shell projection.
- `generated/main.generated.kn`, `state/runtime_snapshot.json`, and `state/session_document.json` were regenerated after the shell change so the live bridge artifacts stayed in sync.
- The clean extension seam is still the same: keep the review-capture lane app-authored until a real native/GPU compositor bridge is ready.

## 2026-03-29 - Render Control Room Registry Kept In Sync

- Mirrored the render control-room progression in `session/ui_workbench_registry.kn` so the authored workbench now names delegated preview, pathtrace, accumulation, denoise, and frame scheduling as part of the same control-room story.
- Tightened `config/ui_shell.json` lane caption text so the live shell uses the explicit registry lane names: `kain | fabric | python | gpu_compute | c_abi | rust_crate | node_bridge`.
- Regenerated `generated/main.generated.kn`, `state/runtime_snapshot.json`, and `state/session_document.json` after the registry update so the bridge artifacts stayed aligned with the authored shell/session data.
- The next clean seam is still a real temporal-history or denoise runtime, but the shell and authored workbench now tell the same render story.
## 2026-03-29 - Asset Routing Lanes Surfaced In The Shell

- Added an `asset_registry` inspector surface to `config/surfaces.json` so the command registry's source-manifest, scene-exchange, and lineage routing commands now have a durable shell home instead of pointing at an undefined surface.
- Surfaced `asset.route_source_manifest`, `asset.route_scene_exchange`, and `asset.route_asset_lineage` in the command spotlight and scene workbench quick actions inside `config/ui_shell.json`, which makes the source-id-first import chain visible from the operator frame instead of only in registries.
- Regenerated `generated/main.generated.kn`, `state/runtime_snapshot.json`, and `state/session_document.json` after the shell update so the materialized bridge artifacts stayed in sync with the authored shell registry.
- Clean next seam: wire the asset registry surface into the live native chrome once there is a real importer/runtime that can publish richer residency and lineage receipts.

## 2026-03-29 - Asset Registry Joined The Scene Workbench

- Wired `asset_registry` into the `scene_assembly` page's right-side surface set in `config/ui_shell.json`, so the scene workbench can land directly on source manifests, scene exchange, lineage, and residency receipts instead of only exposing them as a standalone surface.
- Reran `scripts/materialize-shell.ps1` and `scripts/materialize-session-state.ps1` so `generated/main.generated.kn`, `state/runtime_snapshot.json`, and `state/session_document.json` stayed aligned with the authored workbench frame.
- The next clean seam is still a richer importer/runtime-backed residency model; this pass just made the authored intake path easier to reach from the main scene page.

## 2026-03-29 - Material Lookdev Now Exposes The New Runtime Seams

- Added dedicated shell surfaces for `material_paint_runtime_surface`, `material_uv_policy_surface`, and `material_deformation_surface`, then surfaced them in both `config/ui_shell.json` and `session/ui_workbench_registry.kn` so the material bench now shows the newer paint/runtime/UV/deformation contracts instead of hiding them behind the generic material graph.
- Added discoverable commands for `material.inspect_paint_runtime`, `material.inspect_uv_policy`, and `material.inspect_deformation_surface` so the shell can navigate directly into the new material seams without inventing host-local logic.
- This is still an authored registry/shell pass, not a real painter runtime; the clean extension seam remains to back these reports with a native or GPU paint engine once the runtime lane is ready.

## 2026-03-29 - Sculpt Brush Seam Now States The Runtime Limitation Cleanly

- Tightened `src/sculpt_brush_step.kn` so the sculpt brush seam names the real limitation up front: Kain still lacks the native high-performance spatial index and mesh-edit kernel needed for interactive displacement on large meshes.
- The step now frames the extension seam more cleanly around the actual multi-runtime lane choices: native C ABI, GPU compute, or Rust worker pool, with the active edit target still owned by the session/resource contract.
- This is still a documentation-only seam, not a new sculpt runtime; the next clean extension path is to back the data-driven brush contract with a real native or GPU execution lane.

## 2026-03-29 - Material Paint Runtime Shader Is Now Wired Into The Fabric Graph

- Added `gpu_material_paint_runtime` to `fabric/intents/material_bake.fabric.toml` so the authored `shaders/material_paint_runtime_preview.kn` seam is no longer just a staged file; it now participates in the material bake graph.
- The new GPU step consumes the same app-owned authoring inputs as the rest of the material lane and keeps the paint/runtime seam visible as a first-class multi-runtime lane instead of an orphaned shader.
- The clean next seam is to back `paint_runtime_dst` with a richer native or GPU paint-runtime contract if the lane needs more than preview math.

## 2026-03-29 - Material Registry Surfaces Materialized To The Live Shell

- Materialized the new material paint runtime, UV policy, and deformation surfaces through `scripts/materialize-shell.ps1` and `scripts/materialize-session-state.ps1` so `generated/main.generated.kn`, `state/runtime_snapshot.json`, and `state/session_document.json` stay aligned with the authored registry after the shell/workbench expansion.
- This keeps the new `material.inspect_*` commands and `material_*_surface` inspectors on the live bridge path instead of leaving them as config-only scaffolding.
- The paint-runtime seam is still preview math, not a true tiled painter engine; the clean next seam is a richer native or GPU paint/deformation runtime once that lane is ready.

## 2026-03-29 - Render Control Room Reaffirmed The Temporal Spine

- Tightened the render control-room shell copy in `config/ui_shell.json` and `session/ui_workbench_registry.kn` so the progressive render spine reads as pathtrace -> accumulation -> denoise without implying that the temporal-history runtime already exists.
- The highest-value safe improvement here is visibility, not fake execution: the authored report seams stay obvious while the real native/GPU extension lane is still pending.
- Clean next seam: back the accumulation and denoise reports with a real temporal buffer or history-filter runtime when that host seam is ready.

## 2026-03-29 - Material Registry And Bridge Outputs Re-Synced

- Reran `scripts/materialize-shell.ps1` and `scripts/materialize-session-state.ps1` after the latest material runtime seam work so `generated/main.generated.kn`, `state/runtime_snapshot.json`, and `state/session_document.json` stay aligned with the authored registry.
- The app now has the new material paint-runtime / UV-policy / deformation surfaces and their command/report seams reflected in the durable projections; the clean next step is still a real native or GPU-backed painter runtime if those seams need execution beyond preview/report contracts.

## 2026-03-29 - Render Lounge And Report Browser Now Surface The Temporal Spine

- Added explicit properties to `config/surfaces.json` for `render_lounge` and `report_browser` so the render workbench now shows the pathtrace -> accumulation -> denoise chain and the frame-schedule receipt directly in shell chrome.
- Materialized the updated surface registry through `scripts/materialize-shell.ps1` and `scripts/materialize-session-state.ps1`, which refreshed `generated/main.generated.kn`, `state/runtime_snapshot.json`, and `state/session_document.json`.
- This is still a safe authored-contract pass, not a real temporal-history runtime; the clean extension seam remains a native or GPU history buffer / temporal filter lane.

## 2026-03-29 - Material Surface Projections Re-Synced After Scaffold Inspection

- Inspected the scaffold and confirmed the new material seam files are wired through the shell/workbench registry: `material_paint_runtime_surface`, `material_uv_policy_surface`, and `material_deformation_surface` now show up in the lookdev frame and command spotlight.
- Reran `scripts/materialize-shell.ps1` and `scripts/materialize-session-state.ps1` so `generated/main.generated.kn`, `state/runtime_snapshot.json`, and `state/session_document.json` stayed aligned with the authored registry after the inspection pass.
- The current clean extension seam is still a real native or GPU paint/deformation runtime; the app-side work is intentionally limited to durable projections and bridge honesty until that lane exists.

## 2026-03-29 - Material Materializer Re-Run Kept The Live Bridge Honest

- Re-ran `scripts/materialize-shell.ps1` and `scripts/materialize-session-state.ps1` after the latest scaffold inspection so the generated shell and live session snapshot stayed in lockstep with the authored material surfaces and their command/report seams.
- This pass did not change the runtime model; it just kept the new paint-runtime / UV-policy / deformation registry surfaces projected into `generated/main.generated.kn`, `state/runtime_snapshot.json`, and `state/session_document.json`.
- Clean next seam: back `paint_runtime_dst` and the material deformation surface with a real native or GPU execution lane when preview math is no longer enough.

## 2026-03-29 - Runtime Lane Map Surface Made Explicit

- Added a dedicated `runtime_lane_map` inspector surface in `config/surfaces.json` and surfaced it in the scene assembly, render control room, and publish automation pages inside `config/ui_shell.json` plus the mirrored `session/ui_workbench_registry.kn`.
- The new surface keeps the authored Kain / Fabric / Python / GPU / C ABI / Rust / Node ownership matrix visible in the same live shell frame operators already use for scene, render, and delivery work.
- Re-ran `scripts/materialize-shell.ps1` and `scripts/materialize-session-state.ps1` so `generated/main.generated.kn`, `state/runtime_snapshot.json`, and `state/session_document.json` stayed aligned with the authored shell/session registry after the change.
- Clean next seam: if the lane matrix ever needs richer live telemetry, wire this surface into actual bridge/runtime health instead of leaving it as a static registry view.

## 2026-03-29 - Runtime Lane Map Reached Every Workbench

- Expanded `config/ui_shell.json` and the mirrored `session/ui_workbench_registry.kn` so `runtime_lane_map` is now visible across all major workbenches instead of only scene/render/publish.
- This keeps the Kain / Fabric / Python / GPU / C ABI / Rust / Node ownership matrix in the operator frame everywhere the suite already expects navigational and inspection context.
- Reran `scripts/materialize-shell.ps1` and `scripts/materialize-session-state.ps1` so `generated/main.generated.kn`, `state/runtime_snapshot.json`, and `state/session_document.json` stayed aligned with the authored registry after the shell/workbench update.
- The next clean seam is still richer live lane telemetry rather than another static registry view.

## 2026-03-29 - Material Paint Runtime Is Now Wired Into The Graph

- Added the staged `shaders/material_paint_runtime_preview.kn` seam to `fabric/intents/material_bake.fabric.toml` as `gpu_material_paint_runtime`, so the paint/runtime lane now participates in the authored Fabric graph instead of sitting as a dormant shader.
- Added the supporting Kain projection receipts for `material_paint_runtime`, `material_uv_policy`, and `material_deformation_surface`, which keeps the lookdev bench multi-runtime-shaped while still honest about execution limits.
- Reran `scripts/materialize-shell.ps1` and `scripts/materialize-session-state.ps1` so the generated shell and live session snapshot stayed aligned with the new material lane projections.
- Clean extension seam: the paint/deformation runtime is still preview math, so a real native C ABI or GPU-backed painter engine is the next durable execution lane.

## 2026-03-29 - Runtime Lane Map Got A Direct Focus Command

- Added a new `ui.focus_runtime_lane_map` command to `config/command_registry.json` and surfaced it in the shell spotlight plus the scene/render workbench quick actions.
- Kept `config/ui_shell.json` and `session/ui_workbench_registry.kn` aligned so the runtime ownership matrix is one gesture away from the operator frame instead of only being visible as a passive inspector.
- Reran the shell and session materializers so the generated shell and live bridge projections stayed in sync with the authored registry.
- Clean next seam: if bridge health gets richer telemetry, let the runtime lane map surface show live health instead of only static ownership.
## 2026-03-29 - Publish Deck Now Surfaces Asset Lineage

- In `M:\Code\Kain\apps\kain-fabric-dcc-suite`, added `asset_registry` to the publish workbench's center surfaces and tightened the publish hero copy so delivery now keeps lineage visible alongside packages, jobs, and receipts.
- Mirrored the same publish-workbench change in `session/ui_workbench_registry.kn`, then reran `scripts/materialize-shell.ps1` and `scripts/materialize-session-state.ps1` so `generated/main.generated.kn`, `state/runtime_snapshot.json`, and `state/session_document.json` stayed aligned.
- This is still an authored shell/workbench visibility pass, not a native publish runtime; the clean seam remains richer delivery validation and residency telemetry when a host-backed lane is ready.
