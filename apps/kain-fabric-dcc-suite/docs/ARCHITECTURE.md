# Kain Fabric DCC Suite Architecture Notes

This file is a lightweight companion to the app-root `ARCHITECTURE.md`. It exists so future agents can open the `docs/` area and immediately see the scaffold shape without hunting across the repo.

## Core Thesis

`kain-fabric-dcc-suite` is a Kain-authored, Fabric-orchestrated DCC suite scaffold. App meaning should stay in registries, session reducers, intent graphs, and projection writers. Native runtime code is a narrow seam, not the semantic owner.

## Durable Ownership Layers

- `config/*.json` — app registries for workspaces, surfaces, commands, intents, resources, reports, jobs, runtime packs, shaders, and UI manifests.
- `session/*.kn` — canonical session schema, reducers, intent planning, and registry-backed session truth.
- `fabric/intents/*.fabric.toml` — reusable lane graphs for ingest, sculpt, mesh, rig, sim, material, render, compositor, publish, and tensor work.
- `src/*.kn` — Kain-authored projection writers and seam modules.
- `native-app/src/*.rs` — file-backed live bridge that mirrors session state, but does not own the semantics.
- `state/*.json` — durable projection outputs and bridge sidecars.
- `generated/*.kn` — disposable shell materialization.

## Key App Patterns

### Mesh lane

The mesh lane is now contract-driven:

- imported payloads
- authored primitives
- active edit targets
- topology outputs
- topology history
- mesh contract and topology-history reports in the session state

The current limitation is explicit: the bridge still mutates JSON heuristically. The clean extension seam is a typed reducer/driver layer shared by the resource registry, report registry, and runtime bridge.

### Material lane

The material flow covers:

- texture-set authoring
- layer stacks
- SVG masks
- packed texture export
- GPU preview and render-preview seams

### Sculpt / rig / tensor / sim / compositor

These lanes are intentionally honest about their limits:

- sculpt uses a GPU-owned heightfield proof and native-facing reports
- rig has a control/deformation contract with an external solver seam
- tensor and sim are readiness/planning seams until native runtimes land
- compositor is a reporting seam until real frame assembly exists

## Shell And Bridge

The generated shell should be treated as a projection of app registries. The live bridge should surface canonical report/resource vocabulary in session state, but the shell must not invent app semantics.

## Common Errors / Lessons Learned

- Do not let the native bridge become the source of truth.
- Keep canonical ids and URIs identical across config, session registries, projections, and live bridge code.
- Treat the tensor, sim, compositor, rig, and sculpt runtime seams as real extension points, not fake completions.
- If a new lane appears, add the registry entry first, then the session contract, then the projection writer.
