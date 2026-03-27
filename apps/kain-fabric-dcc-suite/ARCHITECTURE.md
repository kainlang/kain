# Kain Fabric DCC Suite Architecture

This file is the durable project overview for `M:/Code/Kain/apps/kain-fabric-dcc-suite`.

## What It Is

`kain-fabric-dcc-suite` is a flagship multi-lane DCC suite scaffold that keeps app meaning in Kain plus Fabric rather than in the eventual native host.

The material lane now includes a painter-style PBR authoring contract with texture sets, layer stacks, SVG masks, smart materials, and packed texture export receipts. This pass deliberately keeps those semantics in app-level Kain and Fabric manifests because the wider repo already has PBR and material-graph language concepts; the missing piece here was pipeline ownership, not a new compiler dialect.

The scaffold is split into six durable ownership layers:

1. App registries
`config/*.json` defines workspace modes, surfaces, tools, universal gizmo profiles, commands, Fabric pipeline summaries, Fabric intent registry, resource kinds, report kinds, runtime packs, and automation jobs.

2. Session core
`session/*.kn` defines the canonical session document, reducers, derived read models, command handler catalog, intent planner, and resource/report/job/workspace registries.

3. Fabric pipeline
`KAIN.fabric.toml` is the broad bootstrap and reporting graph that converges `python`, `kain`, `c_abi`, `rust_crate`, `gpu_compute`, and node-bridged publishing through Kain.

4. Intent graphs
`fabric/intents/*.fabric.toml` splits interactive or lane-specific work into reusable graphs for bootstrap, ingest, sculpt, topology, rig, sim, material, render, compositor, publish, and tensor work. The material graph now runs as authored contract projection -> SVG mask projection -> GPU preview -> packed export projection.

5. Runtime seam modules
`src/*.kn`, `native/*`, `local_crate/*`, `shaders/*`, and `scripts/*` provide the narrow per-runtime seams behind the Fabric manifests.

6. Materialized outputs
`generated/main.generated.kn`, `state/runtime_snapshot.json`, and the lane receipt files in `state/*.json` are projected artifacts produced from the registries and the latest Fabric reports.

## Ownership Boundaries

- Kain owns authored shell meaning, session state contracts, scene/asset/tensor projection glue, and node publishing bridge entrypoints.
- Fabric owns runtime selection, step ordering, dependency edges, report emission, and reusable intent graph composition.
- The native C helper owns only a narrow sculpt and mesh-signature mutation seam.
- The local Rust crate owns graph, topology, and rig health analysis only.
- The GPU shader owns preview and material-bake compute shape only.
- The future native shell must consume generated bundles and snapshots. It must not become the semantic owner of workspace lanes, command routing, or session truth.

## Main Files

- `KAIN.toml`: app package and build contract.
- `KAIN.fabric.toml`: canonical cross-runtime scaffold pipeline.
- `config/app_manifest.json`: app identity, manifest map, and runtime capability contract.
- `config/workspace_modes.json`: workspace and lane presets for the full DCC suite, including painter-style material and lookdev flow.
- `config/surfaces.json`: docked shell surface registry.
- `config/tool_catalog.json`: tool and operator rail, including smart material, SVG mask, channel-pack export tools, and per-tool gizmo defaults.
- `config/gizmo_registry.json`: universal gizmo profile and per-viewport binding registry.
- `config/command_registry.json`: canonical command surface for operators, routing, automation, painter-style material authoring, export, and gizmo state changes.
- `config/fabric_pipeline.json`: shell-facing summary of the broad pipeline.
- `config/fabric_intents.json`: reusable intent registry with per-lane graph ownership.
- `config/resource_kinds.json`: resource registry schema for scene, asset, preview, tensor, sculpt, and publish artifacts.
- `config/report_kinds.json`: report registry schema for bootstrap, ingest, topology, rig, tensor, publish, and automation artifacts.
- `config/runtime_packs.json`: data-driven runtime pack catalog inspired by K_OS registry patterns.
- `config/automation_jobs.json`: recurring job catalog for caches, previews, publish, and tensor upkeep.
- `session/*.kn`: session truth, reducer logic, read models, and typed registries.
- `fabric/intents/*.fabric.toml`: reusable lane graphs.
- `src/*.kn`: Kain-authored runtime bridge modules.
- `src/material_authoring_projection.kn`: projects the active texture-set, layer-stack, and export-preset report.
- `src/svg_material_mask_projection.kn`: projects the active SVG mask stack and vector decal report.
- `src/material_texture_export_projection.kn`: projects packed texture export receipts for downstream runtimes.
- `shaders/material_bake_preview.kn`: GPU compute preview and material bake seam.
- `scripts/materialize-shell.ps1`: data-driven shell materializer.
- `scripts/materialize-session-state.ps1`: runtime snapshot materializer from config and latest Fabric report.
- `state/*.json`: durable lane receipts for sim planning, compositor planning, tensor dispatch, tensor checkpoints, and tensor inference results.
- `state/material_authoring_report.json`, `state/svg_mask_report.json`, and `state/material_texture_export_report.json`: durable painter-style material receipts consumed by shell inspectors and publish/reporting flow.

## Primary Data Flow

`config/*.json -> scripts/materialize-shell.ps1 -> generated/main.generated.kn -> kain build native-ui`

`config/*.json + latest Fabric report -> scripts/materialize-session-state.ps1 -> state/runtime_snapshot.json -> native shell`

`config/gizmo_registry.json + viewport surface metadata -> bundle-authored <viewport3d ... gizmo.*> props -> realtime bundle + native viewport policy`

`operator command -> session/reducers.kn -> session/intent_planner.kn -> fabric/intents/*.fabric.toml -> runtime workers -> resources/reports/jobs -> shell projections`

`python suite bootstrap -> kain scene/session seed -> native sculpt seam -> rust graph analysis -> python tensor planning -> gpu preview bake -> Kain node publish bridge`

`material authoring command -> session material state -> dcc_suite_seed material/svg documents -> material_authoring_projection -> svg_material_mask_projection -> gpu_material_preview -> material_texture_export_projection -> render/publish consumers`

`sim/compositor/tensor intent graphs -> explicit app-rooted receipt writes in state/*.json -> shell inspection, automation jobs, and future native runtime consumers`

## Extension Seams That Are Intentional

- The tensor lane now emits explicit dispatch, checkpoint, and inference-result receipts in `state/*.json`. A first-class typed tensor artifact contract across Python, Kain, and GPU runtime lanes is still future work.
- The sim lane now emits durable plan and report receipts in `state/*.json` rather than a mock string return, but it is still not a real solver runtime. That keeps the current repo honest until a durable sim contract exists.
- The compositor lane now emits durable rebuild-plan and rebuild-report receipts in `state/*.json`, but real graph execution and frame assembly should still arrive through a broader runtime extension rather than by overloading shell presentation code.
- The material lane now emits durable authoring, SVG mask, and export receipts in `state/*.json`, but it is still not a native painter engine with tiled brush evaluation, GPU bakers, or live sparse texture streaming. Those remain explicit extension seams.
- The runtime pack registry is broad on purpose, but it is still manifest-owned metadata until downstream pack loaders and launchers consume it directly.

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
- Keep universal gizmo defaults, hotkeys, drag triggers, and snap policy in `config/gizmo_registry.json` and viewport-authored props rather than re-hardcoding them inside `kain-ui-native`.
- Keep session truth in `session/*.kn`; reports and runtime snapshots are derivative artifacts.
- Keep the C and Rust helpers narrow and replaceable. If a concept becomes true DCC semantics, move it back into Kain or registry data.
- Keep tensor, sim, and compositor work explicit about current limits instead of implying runtime completeness that does not exist yet.

## Common Errors

- `native/dcc_suite_ops.dll` must exist before `c_abi` sculpt steps can execute.
- On Windows, keep `__declspec(dllexport)` on the declarations in `native/dcc_suite_ops.h` or the Fabric `c_abi` bridge will fail to resolve `dcc_suite_apply_sculpt_stamp` and `dcc_suite_signature`.
- `local_crate/Cargo.toml` needs a local `[workspace]` table so the Fabric rust-crate loader can resolve the helper crate without adding it to the monorepo workspace members list.
- `shaders/material_bake_preview.kn` follows the stricter compute-shader tuple syntax used by the Fabric GPU smoketests. Missing trailing commas inside the `comptime` tuple can make `gpu_material_preview` fail with a parser error.
- The material receipt writers in `src/material_authoring_projection.kn`, `src/svg_material_mask_projection.kn`, and `src/material_texture_export_projection.kn` use escaped JSON string assembly just like the existing lane receipts. Unescaped quote characters will break Kain parsing.
- The lane manifests under `fabric/intents/*.fabric.toml` must set `[workspace].root = "../.."` so scripts, source files, and local crate paths resolve from the app root rather than `fabric/intents/`.
- For Kain steps launched through lane manifests, do not rely on cwd-relative receipt paths like `state/foo.json`. Use explicit app-rooted paths such as `apps/kain-fabric-dcc-suite/state/foo.json` or the receipts may not materialize where the shell expects them.
- `generated/main.generated.kn` is materialized output. If config and shell drift, rerun `scripts/materialize-shell.ps1`.
- `state/runtime_snapshot.json` and the material lane receipts are also materialized output. If reports change, rerun `scripts/materialize-session-state.ps1`.
- `config/gizmo_registry.json` is now part of the viewport contract. If gizmo defaults or hotkeys change, rerun `scripts/materialize-shell.ps1` and rebuild the native UI bundle so the realtime viewport sees the new metadata.
- The tensor manifests intentionally report readiness and plan state even when `torch` is unavailable. That is not a bug in the scaffold; it is the current extension seam.
