# Kain Fabric Modeler

This app is a Fabric-first 3D modeling workbench for the Kain native runtime lane.

It is intentionally split into three ownership layers:

- `KAIN.fabric.toml` is the orchestration spine for Python project seeding, Kain scene seeding, native brush work, Rust topology analysis, GPU preview baking, and Node publishing summaries.
- `generated/main.generated.kn` is the native-ui shell that presents the modeler as a desktop authoring tool with a 3D viewport, authoring rails, graph lanes, and runtime telemetry.
- `config/*.json` is the data-driven source of truth for shell surfaces, workspace modes, imported runtime packs, tool rails, command routing, and Fabric intent presentation.
- `session/*.kn` is the live state core that defines the canonical session document, reducer layer, and intent planner between shell interactions and Fabric execution.


## Key Folders
- `config/` contains the app manifest, shell registries, command registry, and Fabric intent registry.
- `session/` contains the canonical app state schema, reducers, and intent planner.
- `fabric/intents/` contains reusable Fabric subgraphs for interactive app work.
- `src/` contains the Kain-authored Fabric glue.

- `shaders/` contains the Fabric GPU compute preview step.
- `native/` contains the C ABI brush helper.
- `local_crate/` contains the Rust topology helper crate.
- `scripts/` contains shell materialization and build helpers.

## Suggested Commands

```powershell
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-modeler/scripts/build-native-library.ps1
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-modeler/scripts/materialize-shell.ps1
cargo run -p cli --bin kain -- fabric validate --manifest apps/kain-fabric-modeler/KAIN.fabric.toml
cargo run -p cli --bin kain -- fabric run --manifest apps/kain-fabric-modeler/KAIN.fabric.toml
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-modeler/scripts/build-native-ui.ps1
```

## Current Shape

- The native shell is already authored as a desktop-grade docked workbench.
- The Fabric lane already spans all local runtime kinds currently proven in the repo: `python`, `kain`, `c_abi`, `rust_crate`, `gpu_compute`, and `node`.
- The imported runtime pack catalog is deliberately broad so the app reads like a flagship modeler platform rather than a single-panel smoke.

## Output Hygiene

- `generated/` is materialized output and can be regenerated.
- `native/modeler_ops.dll` is a local build artifact and should not be committed if rebuilt.
