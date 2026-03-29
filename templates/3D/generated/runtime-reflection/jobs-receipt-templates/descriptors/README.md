# Jobs Receipt Template Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshot for the jobs
receipt-template catalog.

The files here are generated from the same manifest-driven jobs surface that
powers `generated/runtime-reflection/jobs-receipt-templates/catalog.json`.
They keep the schema, index, queue, dispatch-graph, distribution-channel, and
retry-ledger joins available through a single descriptor document.

Contents:

- `jobs_receipt_template_catalog.json`: receipt-template metadata and descriptor-rooted catalog fields

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
