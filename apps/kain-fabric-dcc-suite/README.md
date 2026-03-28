# Kain Fabric DCC Suite

`kain-fabric-dcc-suite` is a flagship Fabric-first scaffold for a full digital content creation workstation inside the Kain repo.

The app is intentionally organized around repo-native ownership boundaries:

- `config/*.json` owns registries for workspaces, surfaces, commands, Fabric intents, runtime packs, resources, reports, automation jobs, and the universal UI theme plus workbench manifests.
- `session/*.kn` owns the canonical live session document, reducer layer, derived read models, registries, and intent planning logic.
- `KAIN.fabric.toml` owns the broad runtime execution graph across `python`, `kain`, `c_abi`, `rust_crate`, `gpu_compute`, and node-bridged publishing through Kain.
- `fabric/intents/*.fabric.toml` owns reusable per-lane graphs for ingest, sculpt, rig, sim, material, render, compositor, publish, and tensor-oriented work.
- `src/mesh_import_projection.kn`, `src/primitive_mesh_authoring.kn`, `src/material_authoring_projection.kn`, `src/svg_material_mask_projection.kn`, and `src/material_texture_export_projection.kn` own the mesh and painter-style receipts for imported payloads, authored primitives, texture sets, SVG masks, and packed exports.
- `config/shader_catalog.json` owns the suite-level shader map so material, sculpt, render, compositor, and viewport GPU work can grow without rediscovering ownership every pass.
- `config/runtime_lanes.json` now names the runtime-lane ownership matrix explicitly so shell and session tooling can see which semantics sit with Kain, Fabric, Python, GPU, native C, Rust, or a Node bridge at a glance.
- `config/mesh_resource_contract.json` now owns the first app-level mesh resource contract for imported payloads, authored primitives, the active edit target, topology outputs, and topology history, with a matching `mesh_resource_contract_document` registry entry.
- `generated/main.generated.kn` is the materialized native shell projection, not the semantic source of truth.
- `state/runtime_snapshot.json` is the projected runtime-side snapshot consumed by native shell materialization and operator tooling.

## Universal UI System

The suite now carries a manifest-driven studio shell intended to feel closer to a real editor framework than a static dashboard.

- `config/ui_theme.json` defines theme scopes, variants, text variants, and widget defaults for the shell.
- `config/ui_shell.json` defines workspace pages, authored chrome blocks, quick-command decks, and per-page surface placement.
- `config/surfaces.json` and `config/command_registry.json` keep the navigator, command palette, property grid, status strip, and report browser registry-owned instead of host-invented.
- The generated shell now also surfaces `report_count` in the top telemetry band so report inventory stays visible alongside commands, pipelines, jobs, and seam health.
- `session/ui_workbench_registry.kn` mirrors the workbench contract in Kain so future native consumers can bind to typed UI semantics instead of reverse-engineering generated shell output.
- `scripts/materialize-shell.ps1` turns those manifests plus `state/runtime_snapshot.json` into a multi-page Kain UI shell with workspace tabs, a top bar, workstation rails, property grids, telemetry strips, and report browser surfaces that keep mesh/topology lineage visible.

## What The Scaffold Covers

- scene graph and session state ownership
- asset ingest and runtime pack registry
- sculpt, modeling, material, rig, animation, simulation, render, compositor, publish, and automation workspaces
- painter-style PBR texture sets, layer stacks, SVG masks, smart materials, and packed export presets
- command routing, reducer invalidation, intent planning, resources, reports, and jobs
- native sculpt reporting plus a Rust graph-analysis seam over GPU sculpt output
- GPU shader coverage for sculpt heightfields, material preview, export packing, render preview lighting, compositor tone mapping, and staged viewport/material support passes
- tensor training and inference planning steps that stay honest about current runtime limits

## Suggested Commands

```powershell
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-dcc-suite/scripts/build-native-library.ps1
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-dcc-suite/scripts/materialize-shell.ps1
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-dcc-suite/scripts/materialize-session-state.ps1
cargo run -p cli --bin kain -- fabric validate --manifest apps/kain-fabric-dcc-suite/KAIN.fabric.toml
cargo run -p cli --bin kain -- fabric run --manifest apps/kain-fabric-dcc-suite/KAIN.fabric.toml
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-dcc-suite/scripts/build-native-ui.ps1
```

## Output Hygiene

- `generated/` and `state/runtime_snapshot.json` are materialized artifacts and can be regenerated.
- `state/material_authoring_report.json`, `state/svg_mask_report.json`, and `state/material_texture_export_report.json` are also materialized receipts for the material lane.
- `native/dcc_suite_ops.dll` is a local build artifact and should not be treated as authored source.
- The tensor, sim, compositor, and material-export lanes are scaffolded as intentional extension seams where future runtime work still needs to land.
- The scaffold also still wants an explicit runtime-lane registry so operators can see, at a glance, which ownership sits with Kain, Fabric, GPU, C ABI, Rust, Python, or external Node bridges.
- The sculpt lane is now a real GPU-owned heightfield proof, but it is still not a production BVH, voxel, or mesh-surface sculpt runtime.
