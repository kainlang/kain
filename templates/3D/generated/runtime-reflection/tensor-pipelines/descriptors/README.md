# Tensor Pipeline Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshot for the tensor
pipeline catalog.

The files here are generated from the same manifest-driven tensor surface that
powers `generated/runtime-reflection/tensor-pipelines/catalog.json`. They keep
the domain, priority, residency, GPU-kernel, pass-source, and pass metadata
available through a single descriptor document.

Contents:

- `tensor_pipeline_catalog.json`: tensor-pipeline metadata and descriptor-rooted
  catalog fields, including pass source-id/path, stage, and tensor-role joins

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
