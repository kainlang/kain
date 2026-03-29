# Runtime Compatibility Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshots for the
runtime-compatibility catalog.

The files here are generated from the same manifest-driven compatibility
surface that powers `generated/runtime-compatibility/catalog.json`. They are
kept alongside the catalog so downstream tools can open a single descriptor
document when they only need one compatibility view instead of the full
matrix.

Contents:

- `runtime_compatibility_matrix.json`: matrix-scoped compatibility metadata
- `runtime_compatibility_window.json`: backend/target window metadata
- `runtime_launch_readiness.json`: launch-readiness and gate metadata
- `runtime_feature_pack_windows.json`: manifest-derived feature-pack and
  budget-window tier views

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
