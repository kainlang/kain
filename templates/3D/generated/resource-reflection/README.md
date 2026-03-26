# Resource Reflection Catalogs

This folder contains the committed resource-reflection catalog snapshot generated from `src-kain/stdlib/three_d_runtime/resource_reflection_runtime.kn`.

The snapshot stays manifest-driven and links the resource reflection runtime to the same delivery graph, delivery registry, tensor pipeline, GPU resolve kernel, and runtime contracts that the rest of the template already uses.
It now also includes query-ready queue/input/output/channel metadata, kernel consumes/produces metadata, and linked GPU/runtime-contract catalog entries so downstream tools can resolve resource-reflection joins from one committed payload.

Contents:

- `catalog.json`: query-ready resource reflection metadata for the reflection catalog, inspection runtime, and compatibility runtime descriptors, including indexes for `by_artifact_root`, `by_build_graph_queue`, `by_distribution_channel_kind`, `by_tensor_pipeline_pass`, `by_kernel_stage`, `by_kernel_tensor_role`, `by_descriptor_path`, and `by_contract_path`
- `descriptors/*.json`: per-descriptor committed snapshots with policy, runtime-link, kernel, and contract metadata for downstream tools that prefer descriptor-scoped documents

Regenerate this snapshot with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
