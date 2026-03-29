# Runtime Compatibility Catalog

This folder contains the committed runtime-compatibility catalog snapshot generated from the runtime app manifest set and the runtime-compatibility source contracts.

The snapshot stays manifest-driven and binds the compatibility matrix rows to the same delivery graph, delivery registry, tensor pipeline, GPU resolve kernel, and runtime contracts that the rest of the template already uses.
It also includes descriptor-scoped snapshots so downstream tools can query matrix, window, launch-readiness, and manifest-derived feature-pack/budget-window tier views without rebuilding compatibility state from runtime-app metadata.

Contents:

- `catalog.json`: query-ready runtime compatibility metadata with indexes for `by_matrix_cell_id`, `by_backend_kind`, `by_target_kind`, `by_runtime_app`, `by_runtime_app_source_id`, `by_output_target`, `by_source_path`, `by_source_id`, `by_feature_pack_tier`, `by_budget_window_tier`, and `by_policy_bundle_id`
- `descriptors/README.md`: descriptor-folder guide for the matrix, window, launch-readiness, and feature-pack tier snapshots
- `descriptors/*.json`: per-descriptor committed snapshots with policy, runtime-link, kernel, and contract metadata for downstream tools that prefer descriptor-scoped documents, including `runtime_feature_pack_windows.json`

Regenerate this snapshot with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
