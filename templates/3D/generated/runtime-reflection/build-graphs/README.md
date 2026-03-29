# Build Graph Catalog

This folder contains the committed build-graph reflection snapshot generated
from `manifests/build_graphs.json`.

The catalog stays manifest-driven and now emits a descriptor-rooted companion
under `generated/runtime-reflection/build-graphs/descriptors` so downstream
tools can inspect the queue/output/distribution surface without rebuilding the
full manifest joins.

Contents:

- `catalog.json`: build-graph metadata with queue, graph-kind, input, output,
  and linked distribution-channel indexes
- `descriptors/build_graph_catalog.json`: committed descriptor document for the
  build-graph catalog contract and runtime links
- `descriptors/README.md`: folder guide for the descriptor snapshot

Regenerate the parent catalog with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
