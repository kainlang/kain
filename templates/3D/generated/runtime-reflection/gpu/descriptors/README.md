# GPU Reflection Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshot for the GPU
kernel reflection catalog.

The files here are generated from the same manifest-driven GPU surface that
powers `generated/runtime-reflection/gpu/catalog.json`. The catalog is now
descriptor-rooted and projects `source_id` values from `manifests/sources.json`
so downstream tools can join against the shared source registry without using
`source_path` as the only lookup key.
The authored `manifests/gpu_kernels.json` file is source-id-first, so the
generator reconstructs `source_path` from the shared registry when it writes
the committed reflection snapshot.

Contents:

- `gpu_reflection_catalog.json`: GPU kernel metadata, `source_id` projections,
  and descriptor-rooted index names

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
