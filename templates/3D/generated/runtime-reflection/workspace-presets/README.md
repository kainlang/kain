# Workspace Preset Catalog

This folder contains the committed workspace-preset reflection snapshot generated
from `manifests/workspace_presets.json`.

The catalog stays manifest-driven and ties each preset back to the shared
runtime-app and source-registry projections, along with the launch-manifest and
delivery-receipt examples that materialize the workspace lanes.

Contents:

- `catalog.json`: workspace-preset metadata with indexes for
  `by_preset_id`, `by_preset_kind`, `by_focus_lane`, `by_runtime_app`,
  `by_runtime_app_source_id`, `by_runtime_kind`, `by_host_kind`,
  `by_launch_manifest_id`, and `by_receipt_id`
- `descriptors/workspace_preset_catalog.json`: committed descriptor document
  for the workspace-preset catalog contract and artifact roots

Regenerate this snapshot with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
