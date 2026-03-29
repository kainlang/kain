# Distribution Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshot for the
distribution receipt catalog.

The files here are generated from the same manifest-driven distribution surface
that powers `generated/runtime-reflection/distribution/catalog.json`. The
catalog is now descriptor-rooted so downstream tools can inspect the channel,
approval, artifact-root, and build-graph joins from a single descriptor
document instead of reopening the full catalog.

Contents:

- `distribution_receipt_catalog.json`: distribution metadata, index names, and descriptor-rooted catalog fields

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
