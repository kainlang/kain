# Kain Fabric Modeler Architecture

This file is the durable project overview for `M:/Code/Kain/apps/kain-fabric-modeler`.

## What It Is

`kain-fabric-modeler` is a flagship app scaffold that converges the newly implemented Fabric executor with the native-ui and native runtime lanes for a desktop-grade 3D modeling tool.

The app is organized so future agents can expand it as a real product surface:

1. Fabric orchestration
`KAIN.fabric.toml` declares the step graph and local runtime ownership.

2. Native shell
`generated/main.generated.kn` is the current materialized shell consumed by `kain build native-ui`.

3. Data-driven shell registries
`config/*.json` defines the workspace modes, shell surfaces, imported runtime packs, tools, and Fabric step presentation.

4. Runtime-specific workers
`src/*.kn`, `native/*`, `local_crate/*`, `shaders/*`, and `scripts/*` implement the per-runtime responsibilities behind the Fabric manifest.

## Ownership Boundaries

- Kain owns modeling semantics, shared contract values, and the authored shell meaning.
- Fabric owns step ordering, runtime selection, dependency edges, and session reporting.
- The C helper owns the native brush mutation proof only.
- The Rust crate owns topology and checksum-style reporting only.
- The GPU shader owns preview buffer transformation only.
- Node owns summary/export formatting only.
- The native runtime and native-ui lane consume the compiled app bundle; they must not become the semantic source of truth for the modeler.

## Main Files

- `KAIN.fabric.toml`: canonical local pipeline.
- `KAIN.toml`: package/build contract for the app shell.
- `config/app_manifest.json`: runtime identity and capability declaration.
- `config/surfaces.json`: docked surface registry.
- `config/workspace_modes.json`: operator-facing workspace presets.
- `config/library_catalog.json`: broad imported runtime pack catalog.
- `config/tool_catalog.json`: modeling tool rail.
- `config/fabric_pipeline.json`: shell-facing view of the Fabric step graph.
- `scripts/materialize-shell.ps1`: regenerates `generated/main.generated.kn` from config manifests.
- `scripts/build-native-ui.ps1`: materializes the shell, validates/runs Fabric, and packages the native-ui app.
- `scripts/build-native-library.ps1`: builds the local C ABI helper.
- `src/main.kn`: Kain Fabric orchestration glue.
- `src/native_step.kn`: C ABI bridge glue.
- `src/rust_step.kn`: Rust crate bridge glue.
- `shaders/preview_bake.kn`: GPU compute preview step.

## Primary Data Flow

`config/*.json -> scripts/materialize-shell.ps1 -> generated/main.generated.kn -> kain build native-ui`

`python settings -> kain scene seed -> c brush mutation -> rust topology report -> gpu preview bake -> node summary`

The native shell and Fabric lane are related but separate on purpose: the shell is the operator surface, while Fabric is the authoring/orchestration spine behind project bootstrapping and derived outputs.

## Common Commands

From `M:/Code/Kain`:

```powershell
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-modeler/scripts/build-native-library.ps1
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-modeler/scripts/materialize-shell.ps1
cargo run -p cli --bin kain -- fabric validate --manifest apps/kain-fabric-modeler/KAIN.fabric.toml
cargo run -p cli --bin kain -- fabric run --manifest apps/kain-fabric-modeler/KAIN.fabric.toml
powershell -ExecutionPolicy Bypass -File apps/kain-fabric-modeler/scripts/build-native-ui.ps1
```

## Architectural Guardrails

- Keep Fabric manifest truth in `KAIN.fabric.toml`; do not spread execution semantics across multiple ad hoc scripts.
- Keep shell layout data-driven through `config/*.json`.
- Keep the native helper and Rust helper narrow. If a concept is really modeling semantics, move it back into Kain or manifest data.
- Preserve the split between shell presentation and Fabric output generation.
- Prefer adding new runtime packs to `config/library_catalog.json` and shell registries before hardcoding more lanes into `generated/main.generated.kn`.

## Common Errors

- `native/modeler_ops.dll` must exist before the `c_abi` Fabric step can succeed.
- `generated/main.generated.kn` is materialized output. If the shell and config drift, rerun `scripts/materialize-shell.ps1`.
- If the GPU step works through direct shader compilation but not through Fabric, inspect the Fabric manifest inputs and the shared-buffer contracts first.
