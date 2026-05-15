# Kain Fabric Modeler Pipeline
This app treats Fabric as the authoring and orchestration spine behind the modeler.

The pipeline is now split into two layers:

    - `KAIN.fabric.toml` remains the full bootstrap pipeline.
    - `config/fabric_intents.json` plus `fabric/intents/*.fabric.toml` define reusable intent graphs for interactive session work like preview rebakes, topology refreshes, publish summary refreshes, and future mesh-edit commands.


## Step Flow

1. `python_project_seed`
Seeds viewport, project, and brush defaults.

2. `model_seed`
Kain authors the scene seed, mesh stream, preview buffers, and scene report.

3. `native_brush`
The native C helper mutates the preview image and returns a brush snapshot plus signature.

4. `topology_analyzer`
The Rust helper computes a topology-oriented summary over the seeded mesh stream.

5. `gpu_preview`
The GPU compute step copies the preview stream through the Fabric GPU runtime to prove the compute lane is wired.

6. `node_publisher`
Kain calls the Node helper through the JavaScript bridge to render a publish/export-style summary from the upstream Fabric results.
## Session Core

The modeler now has an explicit app-state middle layer:

    - `session/session_schema.kn` defines the canonical live session document.
    - `config/command_registry.json` defines the legal command surface.
    - `session/reducers.kn` performs immediate semantic state updates.
    - `session/derived_state.kn` materializes operator-facing read models.
    - `session/intent_planner.kn` maps commands and dirty resources to Fabric intents.
    - `state/runtime_snapshot.json` projects the session/resource/report truth for the native runtime.

    ## Quickstart


```powershell
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-modeler/scripts/build-native-library.ps1
cargo run -p cli --bin kain -- fabric validate --manifest apps/kain-fabric-modeler/KAIN.fabric.toml
cargo run -p cli --bin kain -- fabric run --manifest apps/kain-fabric-modeler/KAIN.fabric.toml
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-modeler/scripts/materialize-session-state.ps1
```

The native shell is separate:

```powershell
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-modeler/scripts/materialize-shell.ps1
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-modeler/scripts/build-native-ui.ps1
```
