# Source Registry Catalog

This folder contains the committed source-registry snapshot generated from `manifests/sources.json`, `manifests/runtime_apps.json`, and `manifests/workspace_presets.json`.

The catalog keeps the shared source registry manifest-driven and lets downstream tools resolve authored sources, runtime app projections, and workspace-preset projections from one committed payload. The workspace-preset side now indexes the full manifest, while the launch/receipt catalogs remain example-scoped.

Contents:

- `catalog.json`: source registry metadata with indexes for `by_source_id`, `by_source_path`, `by_domain`, `by_target`, `by_runtime_app_id`, `by_focus_lane`, `by_host_kind`, `by_runtime_kind`, `by_preset_id`, `by_workspace_preset_runtime_app_id`, `by_workspace_preset_launch_manifest_id`, and `by_workspace_preset_receipt_id`
- `descriptors/source_registry_catalog.json`: committed descriptor document for the registry contract and artifact roots

Regenerate this snapshot with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
