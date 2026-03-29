# GPU Kernel Catalog

This folder contains the committed GPU kernel reflection snapshot generated
from `manifests/gpu_kernels.json`.

The catalog stays manifest-driven and now emits a descriptor-rooted companion
under `generated/runtime-reflection/gpu/descriptors` alongside `source_id`
projections resolved from `manifests/sources.json`.
The authored `manifests/gpu_kernels.json` file is source-id-first, so the
generator reconstructs `source_path` from the shared registry instead of
repeating it in the kernel manifest.

Contents:

- `catalog.json`: GPU kernel metadata with `source_id`, `source_path`,
  dispatch, tensor role, stage, artifact-root, and index metadata
- `descriptors/gpu_reflection_catalog.json`: committed descriptor document for
  the GPU kernel catalog contract and artifact roots
- `descriptors/README.md`: folder guide for the descriptor snapshot

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
