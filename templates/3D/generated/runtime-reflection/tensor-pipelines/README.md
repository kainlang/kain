# Tensor Pipeline Catalog

This folder contains the committed tensor-pipeline reflection snapshot generated
from `manifests/tensor_pipelines.json`.

The catalog stays manifest-driven and joins each tensor pipeline to its
authored passes plus resolved GPU kernel metadata where available from
`generated/runtime-reflection/gpu/catalog.json`. It keeps pipeline domain,
priority, residency, pass, GPU stage/tensor-role, and pass-source metadata
queryable without reopening the manifest.

Contents:

- `catalog.json`: tensor-pipeline metadata with domain, priority, residency,
  pass, tensor-role, stage, pass-source-id, and pass-source-path indexes
- `descriptors/tensor_pipeline_catalog.json`: committed descriptor document for
  the tensor-pipeline catalog contract and runtime links
- `descriptors/README.md`: folder guide for the descriptor snapshot

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
