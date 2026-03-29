# Jobs Retry Ledger Catalog

This folder contains the committed jobs retry-ledger reflection snapshot
generated from the jobs dispatch and retry manifests.

The catalog stays manifest-driven and now emits a descriptor-rooted companion
under `generated/runtime-reflection/jobs-retry-ledgers/descriptors` so
downstream tools can inspect the retry and worker-requeue contract without
reopening only the full catalog.

Contents:

- `catalog.json`: retry-ledger metadata with queue, dispatch-graph,
  delivery-registry, state, and resume-policy indexes
- `descriptors/jobs_retry_ledger_catalog.json`: committed descriptor document
  for the retry-ledger catalog contract and runtime links
- `descriptors/README.md`: folder guide for the descriptor snapshot

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
