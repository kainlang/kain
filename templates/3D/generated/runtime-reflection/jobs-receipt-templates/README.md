# Jobs Receipt Template Catalog

This folder contains the committed jobs receipt-template reflection snapshot
generated from the jobs dispatch and retry manifests.

The catalog stays manifest-driven and now emits a descriptor-rooted companion
under `generated/runtime-reflection/jobs-receipt-templates/descriptors` so
downstream tools can inspect the job receipt template contract without
reopening only the full catalog.

Contents:

- `catalog.json`: receipt-template metadata with schema, index, queue,
  dispatch-graph, distribution-channel, and retry-ledger joins
- `descriptors/jobs_receipt_template_catalog.json`: committed descriptor
  document for the receipt-template catalog contract and runtime links
- `descriptors/README.md`: folder guide for the descriptor snapshot

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
