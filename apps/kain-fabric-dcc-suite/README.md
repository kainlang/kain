# Kain Fabric DCC Suite

`kain-fabric-dcc-suite` is a flagship Fabric-first scaffold for a full digital content creation workstation inside the Kain repo.

The app is intentionally organized around repo-native ownership boundaries:

- `config/*.json` owns registries for workspaces, surfaces, commands, Fabric intents, runtime packs, resources, reports, and automation jobs.
- `session/*.kn` owns the canonical live session document, reducer layer, derived read models, registries, and intent planning logic.
- `KAIN.fabric.toml` owns the broad runtime execution graph across `python`, `kain`, `c_abi`, `rust_crate`, `gpu_compute`, and node-bridged publishing through Kain.
- `fabric/intents/*.fabric.toml` owns reusable per-lane graphs for ingest, sculpt, rig, sim, material, render, compositor, publish, and tensor-oriented work.
- `generated/main.generated.kn` is the materialized native shell projection, not the semantic source of truth.
- `state/runtime_snapshot.json` is the projected runtime-side snapshot consumed by native shell materialization and operator tooling.

## What The Scaffold Covers

- scene graph and session state ownership
- asset ingest and runtime pack registry
- sculpt, modeling, material, rig, animation, simulation, render, compositor, publish, and automation workspaces
- command routing, reducer invalidation, intent planning, resources, reports, and jobs
- narrow native C and Rust proof seams for sculpt mutation and graph analysis
- GPU material/preview compute scaffolding
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
- `native/dcc_suite_ops.dll` is a local build artifact and should not be treated as authored source.
- The tensor, sim, and compositor lanes are scaffolded as intentional extension seams where future runtime work still needs to land.
