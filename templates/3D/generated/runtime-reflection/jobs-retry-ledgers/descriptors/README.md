# Jobs Retry Ledger Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshot for the jobs
retry-ledger catalog.

The files here are generated from the same manifest-driven jobs surface that
powers `generated/runtime-reflection/jobs-retry-ledgers/catalog.json`. They
keep the dispatch-graph, queue, delivery-registry, receipt, and resume-policy
joins available through a single descriptor document.

Contents:

- `jobs_retry_ledger_catalog.json`: retry-ledger metadata and descriptor-rooted catalog fields

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
