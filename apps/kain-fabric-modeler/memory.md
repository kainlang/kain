# Kain Fabric Modeler Memory

This file captures durable design context for `M:/Code/Kain/apps/kain-fabric-modeler`.

## 2026-03-26 - Initial Fabric-Native Modeling Workbench Landed

The app now exists as a real `/apps` project instead of an idea or a loose smoke.

What changed:

- Added a dedicated app at `apps/kain-fabric-modeler`.
- Chose Fabric as the orchestration spine rather than a validation-only accessory.
- Added a desktop-native shell path through `generated/main.generated.kn` and `scripts/build-native-ui.ps1`.
- Added all currently proven local Fabric runtime kinds in one app: Python, Kain, C ABI, Rust crate, GPU compute, and Node.
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

Current risks:

- The shell is still a strong authored scaffold, not a fully interactive controller-backed product.
- The Fabric workers currently use deterministic seed data rather than true scene asset ingestion or runtime scene mutation receipts.
- The C helper depends on a locally built DLL.

Recommended next step:

- Add a controller or Kain-hosted state materializer that converts Fabric report outputs and app config into live workspace/session state so the shell and pipeline become one continuous editing loop.
