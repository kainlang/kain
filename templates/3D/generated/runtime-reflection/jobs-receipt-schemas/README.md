# Jobs Receipt Schema Catalog

This folder contains the committed jobs receipt-schema reflection snapshot
generated from the jobs dispatch and retry manifests.

The catalog stays manifest-driven and now emits a descriptor-rooted companion
under `generated/runtime-reflection/jobs-receipt-schemas/descriptors` so
downstream tools can inspect the job receipt schema contract without reopening
only the full catalog.

Contents:

- `catalog.json`: receipt-schema metadata with queue, dispatch-graph,
  distribution-channel, retry-ledger, and tensor-pipeline indexes
- `descriptors/jobs_receipt_schema_catalog.json`: committed descriptor
  document for the receipt-schema catalog contract and runtime links
- `descriptors/README.md`: folder guide for the descriptor snapshot

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
