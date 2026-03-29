# Distribution Receipt Catalog

This folder contains the committed distribution-receipt snapshot generated
from `manifests/distribution_channels.json`.

The catalog stays manifest-driven and now emits a descriptor-rooted companion
under `generated/runtime-reflection/distribution/descriptors` so downstream
tools can inspect the delivery surface without rebuilding the full manifest
joins.

Contents:

- `catalog.json`: distribution-channel metadata with channel-kind,
  approval-policy, artifact-root, and linked build-graph indexes
- `descriptors/distribution_receipt_catalog.json`: committed descriptor
  document for the distribution catalog contract and runtime links
- `descriptors/README.md`: folder guide for the descriptor snapshot

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
