# Engine System Catalog

This folder contains the committed engine-system reflection snapshot generated
from `manifests/engine_systems.json`.

The catalog keeps the core engine lane registry manifest-driven and links each
row back to shared source, runtime-app, and workspace-preset projections so
downstream tools can query lane registration without rebuilding joins locally.

Contents:

- `catalog.json`: engine-system metadata with indexes for
  `by_engine_system_id`, `by_lane`, `by_source_id`, `by_required`,
  `by_runtime_app_id`, and `by_workspace_preset_id`
- `descriptors/engine_system_catalog.json`: committed descriptor document for
  the engine-system catalog contract and artifact roots

Regenerate this snapshot with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
