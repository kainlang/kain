# Build Graph Descriptor Snapshots

This folder contains the committed descriptor-scoped snapshot for the build
graph catalog.

The files here are generated from the same manifest-driven build-graph surface
that powers `generated/runtime-reflection/build-graphs/catalog.json`. The
catalog is now descriptor-rooted so downstream tools can inspect the queued,
graph-kind, input, output, and distribution-channel joins from a single
descriptor document instead of reopening the full catalog.

Contents:

- `build_graph_catalog.json`: build-graph metadata, index names, and descriptor-rooted catalog fields

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
