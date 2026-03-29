# Kain Fabric DCC Suite Memory

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

## 2026-03-29 - Render-Room Lanes Wired In

- Added the first dedicated render-room lanes to `kain-fabric-dcc-suite`: `render.delegate_preview` and `lighting.review_preview`.
- New Fabric graphs live at `fabric/intents/render_delegate.fabric.toml` and `fabric/intents/lighting_review.fabric.toml`.
- Added Kain projection files for the new lanes: `src/render_delegation_projection.kn` and `src/lighting_review_projection.kn`.
- Extended the suite registries so these lanes are first-class in `runtime_packs`, `report_kinds`, `command_registry`, `fabric_intents`, `report_registry`, `command_handlers`, `reducers`, `intent_planner`, and the dispatcher hot-path list.
- Validation succeeded: queueing both new commands now runs Fabric and materializes `render.delegate_preview:succeeded` and `lighting.review_preview:succeeded`.
- The suite is now moving from preview-only render plumbing into a more believable render room with delegated preview and lighting review contracts.
