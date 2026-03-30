# Kain Fabric DCC Suite Architecture

This file is the durable project overview for `M:/Code/Kain/apps/kain-fabric-dcc-suite`.

## What It Is

`kain-fabric-dcc-suite` is a flagship multi-lane DCC suite scaffold that keeps app meaning in Kain plus Fabric rather than in the eventual native host.

The material lane now includes a painter-style PBR authoring contract with texture sets, layer stacks, SVG masks, smart materials, and packed texture export receipts. The sculpt lane now also includes a native GPU-owned heightfield proof where Kain seeds brush and surface buffers, the GPU step owns deformation, and the C seam emits native-facing signatures and reports rather than pretending to own the sculpt itself. The mesh lane now carries a topology history contract so replacement lineage and authored topology decisions have a durable home instead of living only in transient planner state. The rig lane now has a first-class control/deformation contract and an explicit solver bridge seam so rig sync can stay data-driven even while the native IK runtime remains external.

The scaffold is split into seven durable ownership layers:

1. App registries
`config/*.json` defines workspace modes, surfaces, tools, universal gizmo profiles, commands, Fabric pipeline summaries, Fabric intent registry, resource kinds, report kinds, runtime packs, automation jobs, the shader catalog, and the universal UI theme plus workbench manifests.

2. Session core
`session/*.kn` defines the canonical session document, reducers, derived read models, command handler catalog, intent planner, and resource/report/job/workspace registries.

3. Fabric pipeline
`KAIN.fabric.toml` is the broad bootstrap and reporting graph that converges `python`, `kain`, `c_abi`, `rust_crate`, `gpu_compute`, and node-bridged publishing through Kain.

4. Intent graphs
`fabric/intents/*.fabric.toml` splits interactive or lane-specific work into reusable graphs for bootstrap, ingest, sculpt, topology, rig, sim, material, render, compositor, publish, and tensor work. The material graph now runs as authored contract projection -> SVG mask projection -> GPU preview -> packed export projection, the render pathtrace graph now emits both a pathtrace report and a temporal accumulation report so the lane can grow toward a real progressive-preview spine, and the render-room branch now carries explicit frame-scheduling plus AOV/review-capture/visibility telemetry reports for delegation and lighting review.

5. Runtime seam modules
`src/*.kn`, `native/*`, `local_crate/*`, `shaders/*`, and `scripts/*` provide the narrow per-runtime seams behind the Fabric manifests.

6. Native live bridge
`native-app/src/main.rs` and `native-app/src/runtime_bridge.rs` now provide a file-backed command/session/host bridge. `kain-ui-native` emits command requests into `state/command_queue.jsonl`; the bridge mutates `state/session_document.json`, rewrites `state/runtime_snapshot.json`, and relies on `kain-ui-native` file watchers to hot-reload the shell state.

7. Materialized outputs
`generated/main.generated.kn`, `state/runtime_snapshot.json`, `state/session_document.json`, `state/command_queue.jsonl`, and the lane receipt files in `state/*.json` are projected artifacts produced from the registries, the latest Fabric reports, and the live bridge loop. The current shell materializer now emits a dock-rooted panel at the top level instead of a generic slot wrapper so the generated UI stays aligned with the native dock systems.

## Ownership Boundaries

- Kain owns authored shell meaning, session state contracts, scene/asset/tensor projection glue, and node publishing bridge entrypoints.
- Fabric owns runtime selection, step ordering, dependency edges, report emission, and reusable intent graph composition.
- The native C helper owns only a narrow sculpt signature and report seam over GPU output.
- The local Rust crate owns graph, topology, and rig health analysis only.
- The GPU shader layer owns sculpt heightfield evaluation, material preview bake, export channel packing, render preview lighting, compositor tone mapping, and staged shader-library expansion for viewport and smart-material work.
- The app owns the `dcc_suite_scene` viewport intent and startup-session semantics, while the shared `crates/kain-3D` scene catalog currently owns the temporary procedural realization of the startup mesh, floor, backdrop, and studio light rig until a first-class mesh asset contract lands.
- The native shell may emit command events and host the bridge loop, but it still must not become the semantic owner of workspace lanes, command routing, or session truth. Session truth remains the document projected from app-owned schema plus reducers.
- The session document now carries a small `reports` block for `mesh_contract_report` and `topology_history_report` so the live bridge can expose report vocabulary directly in state instead of forcing tools to infer it from generated files alone.

## Mesh Contract

- Meshes are now being treated as app-owned resources addressed by stable mesh resource ids. Sculpt and topology steps should read the active edit target from the session/resource contract, mutate or replace that resource, and mark the resulting resource dirty for downstream consumers.
- The first mesh command surface is now wired end-to-end through `session/command_handlers.kn`, `session/reducers.kn`, `session/intent_planner.kn`, and the file-backed `native-app/src/runtime_bridge.rs`. Commands for opening mesh documents, rebinding edit targets, switching authoring policy, creating primitives, importing assets, and driving topology edits now all mutate the same app-owned session mesh state.
- The current sculpt and topology seams are deliberately narrower than a full mesh asset system. They can operate on the active edit target, but they do not yet own persistent serialization, provenance tracking, undo/redo topology history, or import-time asset normalization.
- The clean extension seam for the next increment is a typed reducer/driver bridge shared by `session/resource_registry.kn`, `session/report_registry.kn`, and `native-app/src/runtime_bridge.rs`, so canonical ids, URIs, and lineage receipts can round-trip through one registry-backed contract instead of only through heuristic JSON mutation.
- Shared viewport startup geometry is still only a bootstrap realization. It is acceptable for `crates/kain-3D` to materialize a temporary cube or support mesh for the opening viewport, but that runtime geometry should not be mistaken for the durable mesh ownership boundary.
- The next durable contract should explicitly cover imported assets, authored primitives, topology history, and topology edits as first-class mesh resources so the viewport, sculpt lane, and topology lane all speak the same id-based language.


## Evaluation / Cook / Cache Seam

- The app now treats graph evaluation and cache materialization as explicit session-owned contracts, not just implied downstream work.
- `session/session_schema.kn` carries dedicated `evaluation` and `cache` blocks for dirty propagation, cook outputs, and cache materialization receipts.
- `session/intent_planner.kn` and `session/reducers.kn` now route `evaluation.recompute_graph`, `evaluation.cook_graph`, and `cache.materialize` as first-class intents so operator actions can fan out through dependency flow more like a Houdini-style cook chain.
- `session/resource_registry.kn`, `session/report_registry.kn`, and `session/job_registry.kn` name the cook/materialization outputs directly so downstream runtime seams can consume stable ids instead of ad-hoc strings.

## Main Files

- `KAIN.toml`: app package and build contract.
- `KAIN.fabric.toml`: canonical cross-runtime scaffold pipeline.
- `config/app_manifest.json`: app identity, manifest map, and runtime capability contract.
- `config/workspace_modes.json`: workspace and lane presets for the full DCC suite, including painter-style material and lookdev flow.
- `config/surfaces.json`: docked shell surface registry, including the report browser surface that keeps mesh and topology lineage visible in the shell.
- `config/tool_catalog.json`: tool and operator rail, including smart material, SVG mask, channel-pack export tools, and per-tool gizmo defaults.
- `config/gizmo_registry.json`: universal gizmo profile and per-viewport binding registry.
- `config/ui_theme.json`: semantic tokens, scopes, variants, and widget defaults for the universal studio shell, including authored workspace rails, status strips, property grids, and command surfaces.
- `config/ui_shell.json`: workspace-page layout manifest with per-mode workbench composition and authored chrome blocks. The authored shell now leans harder into DCC language (`DCC Shell`, `Outliner Rail`, `Attributes Rail`, `Status Bar`, `Command Launcher`) so the native UI frame reads like a mounted workstation instead of a general app dashboard. The authored shell telemetry still includes `report_count` so report inventory stays visible at a glance.
- `session/derived_state.kn`: workspace and pipeline read models, now including a registry-backed runtime-lane count and compact lane summary so the shell can reflect lane ownership without hand-written prose.
- `config/command_registry.json`: canonical command surface for operators, routing, automation, painter-style material authoring, export, shell navigation, and property-grid state changes.
- `config/fabric_pipeline.json`: shell-facing summary of the broad pipeline.
- `config/fabric_intents.json`: reusable intent registry with per-lane graph ownership.
- `config/resource_kinds.json`: resource registry schema for scene, asset, preview, tensor, sculpt, and publish artifacts.
- `config/mesh_resource_contract.json`: first-class mesh document contract for imported payloads, authored primitives, active edit targets, topology outputs, and topology history.
- `session/resource_registry.kn`: canonical mesh resource registry entries, including the contract document itself and the active edit-target seam.
- `config/report_kinds.json`: report registry schema for bootstrap, ingest, mesh contract, topology lineage, rig, tensor, publish, and automation artifacts.
- `config/rig_resource_contract.json`: app-owned rig control, deformation, and solver-bridge document contract for the rig sync seam.
- `config/sculpt_pipeline.json`: data-driven sculpt grid, brush, and height-range defaults for the GPU sculpt lane.
- `config/runtime_packs.json`: data-driven runtime pack catalog inspired by K_OS registry patterns.
- `config/runtime_lanes.json`: explicit runtime-lane ownership matrix for Kain, Fabric, Python, GPU, native C, Rust, and Node bridge semantics.
- `config/automation_jobs.json`: recurring job catalog for caches, previews, publish, and tensor upkeep.
- `config/shader_catalog.json`: manifest-owned registry of shader families, lane ownership, compute keys, and current wiring status.
- `session/*.kn`: session truth, reducer logic, read models, and typed registries.
- `session/ui_workbench_registry.kn`: durable Kain-side workbench contract mirroring the generated shell pages.
- `fabric/intents/*.fabric.toml`: reusable lane graphs.
- `src/*.kn`: Kain-authored runtime bridge modules.
- `src/material_authoring_projection.kn`: projects the active texture-set, layer-stack, and export-preset report.
- `src/svg_material_mask_projection.kn`: projects the active SVG mask stack and vector decal report.
- `src/material_texture_export_projection.kn`: projects packed texture export receipts for downstream runtimes.
- `src/render_preview_projection.kn`: render preview report writer that summarizes the dedicated render GPU pass rather than reusing a generic session string.
- `src/topology_history_projection.kn`: durable mesh lineage projection that retains topology rebuild history as an app-owned report and now cites the canonical mesh contract seams instead of the generic scene bootstrap document.
- `shaders/material_bake_preview.kn`: baseline GPU compute preview and material bake seam for the material lane.
- `shaders/sculpt_heightfield_apply.kn`: GPU sculpt stroke seam for future heightfield-style brush evaluation.
- `shaders/material_channel_pack.kn`: export-oriented GPU channel packing seam used by the publish graph.
- `shaders/render_preview_lighting.kn`: render-preview GPU seam used by the render lounge graph.
- `shaders/compositor_tone_map.kn`: compositor GPU seam used by the rebuild graph.
- `shaders/material_layer_blend_preview.kn`, `shaders/svg_mask_raster.kn`, `shaders/smart_material_resolve.kn`, `shaders/viewport_lighting_preview.kn`, `shaders/render_aov_pack.kn`, and `shaders/compositor_id_matte.kn`: staged shader library coverage for the suite’s likely next GPU responsibilities.
- `scripts/materialize-shell.ps1`: data-driven shell materializer.
- `scripts/materialize-session-state.ps1`: runtime snapshot plus session-document materializer from config and latest Fabric report. It also seeds the bridge command queue.
- `native-app/src/main.rs`: native launcher that resolves the live bridge sidecars and exports bridge env vars for `kain-ui-native`.
- `native-app/src/runtime_bridge.rs`: background bridge loop that consumes JSONL commands, mutates the session document, rewrites the runtime snapshot, and mirrors state sidecars when both app and native-app copies exist.
- `state/session_document.json`: mutable live session document consumed and rewritten by the bridge loop.
- `state/command_queue.jsonl`: append-only command request sink emitted by the host UI.
- `state/*.json`: durable lane receipts for sim planning, compositor planning, tensor dispatch, tensor checkpoints, and tensor inference results.
- `state/material_authoring_report.json`, `state/svg_mask_report.json`, and `state/material_texture_export_report.json`: durable painter-style material receipts consumed by shell inspectors and publish/reporting flow.

## Primary Data Flow

`config/ui_*.json + config/*.json + state/runtime_snapshot.json -> scripts/materialize-shell.ps1 -> generated/main.generated.kn -> kain build native-ui`

`config/*.json + latest Fabric report -> scripts/materialize-session-state.ps1 -> state/runtime_snapshot.json -> native shell`

`kain-ui-native topbar or inspector command button -> KAIN_UI_NATIVE_COMMAND_BRIDGE -> state/command_queue.jsonl -> native-app/src/runtime_bridge.rs -> state/session_document.json + state/runtime_snapshot.json -> kain-ui-native file watcher hot reload`

`config/gizmo_registry.json + viewport surface metadata -> bundle-authored <viewport3d ... gizmo.*> props -> realtime bundle + native viewport policy`

`operator command -> session/reducers.kn -> session/intent_planner.kn -> fabric/intents/*.fabric.toml -> runtime workers -> resources/reports/jobs -> shell projections`

`python suite bootstrap -> kain scene/session seed -> gpu sculpt displacement -> native sculpt reporting -> rust graph analysis -> python tensor planning -> gpu preview bake -> Kain node publish bridge`

`material authoring command -> session material state -> dcc_suite_seed material/svg documents -> material_authoring_projection -> svg_material_mask_projection -> gpu_material_preview -> material_texture_export_projection -> render/publish consumers`

`render preview command -> material receipts -> gpu_render_preview -> render_preview_projection -> render-facing shell reports`

`compositor rebuild command -> compositor_projection -> gpu_compositor_tone_map -> compositor_rebuild_step -> compositor-facing shell reports`

`sim/compositor/tensor intent graphs -> explicit app-rooted receipt writes in state/*.json -> shell inspection, automation jobs, and future native runtime consumers`

## Extension Seams That Are Intentional

- The asset import lane now speaks in source-id-first manifests, interchange transcode, scene exchange, asset lineage, and media ingest receipts. The current contracts are app-owned projections and routing state, not a native interchange runtime or serializer ownership.
- The tensor lane now emits explicit dispatch, checkpoint, and inference-result receipts in `state/*.json`. A first-class typed tensor artifact contract across Python, Kain, and GPU runtime lanes is still future work.
- The sim lane now emits durable plan and report receipts in `state/*.json` rather than a mock string return, but it is still not a real solver runtime. That keeps the current repo honest until a durable sim contract exists.
- The compositor lane now emits durable rebuild-plan and rebuild-report receipts in `state/*.json`, but real graph execution and frame assembly should still arrive through a broader runtime extension rather than by overloading shell presentation code.
- The mesh lane now has real Kain-authored projection writers for imported payloads, authored primitives, Catmull-Clark-style subdivision, and UV packing receipts. It also now emits a native mesh runtime signature through the C helper seam, giving the lane a concrete extension point for geometry ownership without pretending the app itself solves remesh math.
- The material lane now emits durable authoring, SVG mask, export, paint-runtime, UV policy, and deformation receipts in `state/*.json`, and the session contract now also carries explicit smart-mask and scan-ingest profiles so the lookdev bench can read more like a layered paint + sampler hybrid. It is still not a native painter engine with tiled brush evaluation, GPU bakers, or live sparse texture streaming. Those remain explicit extension seams.
- The sculpt lane now emits a real GPU-owned heightfield delta buffer and native-facing sculpt receipts, but it is still not a production mesh sculpt engine with BVH queries, voxel remeshing, multiresolution data, or tablet-pressure sampling. Those remain explicit extension seams.
- The sculpt and topology seam modules should be read as resource-contract adapters, not as mesh owners. They are expected to operate on active edit targets identified by resource id and hand the mutated or rebuilt mesh back through the app-owned resource contract.
- The shader catalog is intentionally broader than the currently scheduled Fabric steps. Some shader files are staged for near-term lane growth rather than being scheduled in every graph immediately.
- The runtime pack registry is broad on purpose, but it is still manifest-owned metadata until downstream pack loaders and launchers consume it directly.
- The explicit runtime-lane matrix now lives in `config/runtime_lanes.json`, so the app can declare which semantic lanes are owned by Kain, Fabric, GPU, C ABI, Rust, Python, or external Node bridges without leaving that mapping implicit in prose.
- The next clean extension seam is to keep that registry flowing into live chrome, shell materialization, and bridge consumers as more runtime surfaces grow.

## Common Commands

From `M:/Code/Kain`:

```powershell
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-dcc-suite/scripts/build-native-library.ps1
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-dcc-suite/scripts/materialize-shell.ps1
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-dcc-suite/scripts/materialize-session-state.ps1
cargo run -p cli --bin kain -- fabric validate --manifest apps/kain-fabric-dcc-suite/KAIN.fabric.toml
cargo run -p cli --bin kain -- fabric run --manifest apps/kain-fabric-dcc-suite/KAIN.fabric.toml
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-dcc-suite/scripts/build-native-ui.ps1
```

## Architectural Guardrails

- Keep semantic ownership in Kain, session modules, and config plus Fabric graphs, not in the native host.
- Keep registry truth in `config/*.json`; generated shell output is a projection.
- Keep page, theme, chrome, and workbench truth in `config/ui_*.json` plus `session/ui_workbench_registry.kn`; the generated shell remains disposable output.
- Keep navigator, command palette, property grid, and status strip meaning in the app registries instead of inventing it inside the native host.
- Keep universal gizmo defaults, hotkeys, drag triggers, and snap policy in `config/gizmo_registry.json` and viewport-authored props rather than re-hardcoding them inside `kain-ui-native`.
- Keep session truth in `session/*.kn`; reports and runtime snapshots are derivative artifacts.
- Keep the C and Rust helpers narrow and replaceable. If a concept becomes true DCC semantics, move it back into Kain or registry data.
- Keep the bridge file-backed and data-driven until a stronger typed runtime contract lands. Do not hardcode lane truth inside egui widgets or the launcher.
- Keep tensor, sim, and compositor work explicit about current limits instead of implying runtime completeness that does not exist yet.

## Common Errors

- `native/dcc_suite_ops.dll` must exist before `c_abi` sculpt steps can execute.
- On Windows, keep `__declspec(dllexport)` on the declarations in `native/dcc_suite_ops.h` or the Fabric `c_abi` bridge will fail to resolve `dcc_suite_sculpt_signature` and `dcc_suite_sculpt_report`.
- `local_crate/Cargo.toml` needs a local `[workspace]` table so the Fabric rust-crate loader can resolve the helper crate without adding it to the monorepo workspace members list.
- `gpu_sculpt_displacement` relies on name-based GPU binding resolution. `dcc_suite_seed` must keep emitting a zeroed `sculpt_delta` buffer so Fabric can infer the output shape for the GPU step.
- The current Kain HLSL backend only lowers very simple `if` expressions. Branchy compute shaders like `shaders/sculpt_heightfield_apply.kn` should prefer branchless math with `step`, `max`, `min`, and `lerp` or they can fail with `Complex if expressions not yet supported in HLSL backend`.
- `shaders/material_bake_preview.kn` follows the stricter compute-shader tuple syntax used by the Fabric GPU smoketests. Missing trailing commas inside the `comptime` tuple can make `gpu_material_preview` fail with a parser error.
- The new staged shader files under `shaders/` follow the same tuple syntax and shared-buffer contract as the Fabric GPU smoketests. When adding more wired GPU steps, keep the `src` and `dst` buffer bindings aligned with Fabric output contracts unless the graph is also updated.
- The material receipt writers in `src/material_authoring_projection.kn`, `src/svg_material_mask_projection.kn`, and `src/material_texture_export_projection.kn` use escaped JSON string assembly just like the existing lane receipts. Unescaped quote characters will break Kain parsing.
- The lane manifests under `fabric/intents/*.fabric.toml` must set `[workspace].root = "../.."` so scripts, source files, and local crate paths resolve from the app root rather than `fabric/intents/`.
- For Kain steps launched through lane manifests, do not rely on cwd-relative receipt paths like `state/foo.json`. Use explicit app-rooted paths such as `apps/kain-fabric-dcc-suite/state/foo.json` or the receipts may not materialize where the shell expects them.
- `generated/main.generated.kn` is materialized output. If config and shell drift, rerun `scripts/materialize-shell.ps1`.
- `state/runtime_snapshot.json`, `state/session_document.json`, and `state/command_queue.jsonl` are part of the live bridge contract now. If the host stops reflecting changes, verify that `KAIN_UI_NATIVE_APP_SNAPSHOT` and `KAIN_UI_NATIVE_COMMAND_BRIDGE` point at the same state root the native bridge thread is rewriting.
- `state/runtime_snapshot.json` must satisfy `crates/kain-ui-native`'s `NativeAppRuntimeSnapshot` schema. If the snapshot shape drifts, the host will silently fail to hot-reload it.
- `scripts/materialize-session-state.ps1` is now responsible for seeding both app-root and `native-app/state` sidecars. If one copy is missing, the bridge loop will still run, but only the existing sidecar roots will stay synchronized.
- `config/gizmo_registry.json` is now part of the viewport contract. If gizmo defaults or hotkeys change, rerun `scripts/materialize-shell.ps1` and rebuild the native UI bundle so the realtime viewport sees the new metadata.
- The DCC suite now expects `dcc_suite_scene` to resolve through the shared `crates/kain-3D` scene catalog. If that scene id changes on the app side, update the shared scene catalog or the viewport will silently fall back to another catalog scene.
- Treat `target_mesh_id` and related mesh inputs as active edit target ids, not ownership claims. The sculpt and topology seams should resolve the resource, operate on it, and hand back a dirty or replacement resource rather than inventing ad-hoc mesh lifetime rules inside the step.
- The tensor manifests intentionally report readiness and plan state even when `torch` is unavailable. That is not a bug in the scaffold; it is the current extension seam.
