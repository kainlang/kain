# Launch Profile Catalog

This folder contains the committed launch-profile reflection snapshot generated
from the workspace preset and runtime app manifests.

The snapshot stays manifest-driven and binds each launch profile back to the
shared source registry through `runtime_app_source_id`, so downstream tools can
query preset/runtime bindings without rebuilding the join locally.

Contents:

- `catalog.json`: launch-profile metadata with focus-lane, runtime-app, host,
  delivery-registry, and source-aware indexes
- `descriptors/launch_profile_catalog.json`: committed descriptor document for
  the launch-profile catalog contract and artifact roots
- `descriptors/README.md`: folder guide for the descriptor snapshot

Regenerate this snapshot with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
