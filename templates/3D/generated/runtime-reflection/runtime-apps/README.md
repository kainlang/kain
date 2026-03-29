# Runtime App Catalog

This folder contains the committed runtime-app reflection snapshot generated
from `manifests/runtime_apps.json`.

The snapshot stays manifest-driven and binds each runtime app row back to the
shared source registry so downstream tools can resolve the host/runtime/output
projection set without rebuilding the full registry join.

Contents:

- `catalog.json`: runtime-app metadata with indexes for `by_runtime_app_id`,
  `by_source_id`, `by_namespace`, `by_host_kind`, `by_runtime_kind`, and
  `by_output_target`
- `descriptors/runtime_app_catalog.json`: committed descriptor document for the
  runtime-app catalog contract and artifact roots

Regenerate this snapshot with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
