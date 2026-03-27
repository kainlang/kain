# Kain Fabric Modeler Memory

This file captures durable design context for `M:/Code/Kain/apps/kain-fabric-modeler`.
## 2026-03-27 - Session Core And Intent Graph Foundation Landed

What changed:

    - Added `session/session_schema.kn` as the canonical live session/project state document for the modeler app.
    - Added `session/reducers.kn` to turn app commands into immediate semantic state updates and resource invalidation.
    - Added `session/intent_planner.kn` to map commands plus dirty resources into reusable Fabric intents.
    - Added `config/command_registry.json` as the app-owned command surface.
    - Added `config/fabric_intents.json` plus `fabric/intents/*.fabric.toml` so interactive work can target subgraphs instead of only the monolithic bootstrap pipeline.
    - Registered the new session/intent manifests through `KAIN.toml` and `config/app_manifest.json`.
    - Updated project architecture docs so future agents understand that the shell, session core, and Fabric intent layer now form one continuous app substrate.

    Why this matters:

        - The app now has a real "missing middle" between the generated shell and Fabric execution.
        - Interactive authoring can be modeled as commands and planned intents instead of ad hoc script coupling.
        - Future work like undo/redo, persistent session restore, and live viewport/inspector refresh now has a clear home.

        Design decisions to preserve:

            - `KAIN.fabric.toml` stays the bootstrap/full-pipeline truth, while `fabric/intents/*.fabric.toml` owns reusable interactive subgraphs.
            - `session/session_schema.kn` is the canonical live truth for app state; Fabric reports and native shell widgets are projections over that state.
            - Reducers should stay lightweight and semantic, while heavy mutation/analysis/publish work remains Fabric-owned.
            - Command and intent registries stay data-driven in `config/` so the app can expose/automate behavior without hardcoding everything into the shell.

            Current risks:

                - The new Kain session files are foundational scaffolding and have not been validated against the live compiler yet.
                - Intent manifests currently mirror the existing deterministic seed pipeline; true mesh mutation and persistence workers still need deeper runtime support.
                - Selection payload parsing and reducer helpers assume string-based command payload projection for now.

                Recommended next step:

                    - Wire a real controller/host loop that emits commands from shell interactions, runs reducers/planners, executes intents, then projects resulting session/resource/report state back into the native UI.

                    ## 2026-03-26 - Initial Fabric-Native Modeling Workbench Landed


The app now exists as a real `/apps` project instead of an idea or a loose smoke.

What changed:

- Added a dedicated app at `apps/kain-fabric-modeler`.
- Chose Fabric as the orchestration spine rather than a validation-only accessory.
- Added a desktop-native shell path through `generated/main.generated.kn` and `scripts/build-native-ui.ps1`.
- Added all currently proven local Fabric runtime kinds in one app, then stabilized the publish lane as a Kain step that calls the Node helper through the JavaScript bridge.
- Added data-driven shell registries for surfaces, workspace modes, tool rails, runtime pack imports, and Fabric step presentation.

Why this matters:

- It gives Kain a clear flagship app shape for “Fabric + native-ui + native runtime” convergence.
- It keeps semantic ownership honest: Kain authors the modeler meaning, while Fabric coordinates multi-runtime execution.
- It creates a better expansion path for future modeling features like real mesh mutation receipts, undo/redo graphs, runtime scene bindings, and packaged tool libraries.

Design decisions to preserve:

- The shell is config-driven and materialized, not hand-expanded forever.
- Fabric is the app’s orchestration backbone, not an optional export lane.
- The C and Rust helpers stay intentionally narrow and replaceable.
- The imported runtime pack list should stay broad and data-driven so the app continues to read like a platform-grade modeler.
- The publish summary helper currently runs through `src/node_publisher.kn`; keep the direct Node Fabric runtime as a future optimization, not a requirement.

Current risks:

- The shell is still a strong authored scaffold, not a fully interactive controller-backed product.
- The Fabric workers currently use deterministic seed data rather than true scene asset ingestion or runtime scene mutation receipts.
- The C helper depends on a locally built DLL.
- The publish helper path is currently resolved from the repo-root build working directory.

Recommended next step:

- Add a controller or Kain-hosted state materializer that converts Fabric report outputs and app config into live workspace/session state so the shell and pipeline become one continuous editing loop.
