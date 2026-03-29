# Jobs Receipt Schema Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshot for the jobs
receipt-schema catalog.

The files here are generated from the same manifest-driven jobs surface that
powers `generated/runtime-reflection/jobs-receipt-schemas/catalog.json`.
They keep the schema, queue, dispatch-graph, distribution-channel, and
retry-ledger joins available through a single descriptor document.

Contents:

- `jobs_receipt_schema_catalog.json`: receipt-schema metadata and descriptor-rooted catalog fields

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
