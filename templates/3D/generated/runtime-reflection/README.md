# Runtime Reflection Catalogs

These snapshots mirror the runtime reflection descriptors in `src-kain/stdlib/three_d_runtime/reflection_runtime.kn` and the workspace-preset materializer bridge in `graph_materialization_runtime.kn`.
The same reflection profile now also registers the sibling `generated/resource-reflection/catalog.json` snapshot for resource residency, inspection, and compatibility queries.
The same generation pass also emits `generated/runtime-compatibility/catalog.json` as the committed matrix snapshot for target/backend window and launch-readiness queries.

They provide committed examples for downstream generators without requiring a live `kain build` run. Catalogs stay manifest-driven and currently include:

- workspace-preset registry metadata (`workspace-presets/catalog.json`)
- workspace-preset launch-schema metadata (`workspace-preset-launch-schemas/catalog.json`)
- workspace-preset launch-template metadata (`workspace-preset-launch-templates/catalog.json`)
- workspace-preset receipt-schema metadata (`workspace-preset-receipt-schemas/catalog.json`)
- workspace-preset receipt-template metadata (`workspace-preset-receipt-templates/catalog.json`)
- workspace-preset launch/receipt contract bindings (`workspace-preset-launch-receipt-bindings/catalog.json`)
- workspace-preset receipt metadata (`workspace-preset-receipts/catalog.json`)
- launch-profile metadata (`launch-profiles/catalog.json`) with `runtime_app_source_id`
  and `by_runtime_app_source_id` so shared workbench projections stay anchored
  to `sources.json`
- build-graph metadata (`build-graphs/catalog.json`)
- distribution receipt metadata (`distribution/catalog.json`)
- GPU kernel reflection metadata (`gpu/catalog.json`)
- runtime contract metadata (`contracts/catalog.json`)
- schema reflection metadata (`schema/catalog.json`)
- jobs receipt-schema metadata (`jobs-receipt-schemas/catalog.json`)
- jobs receipt-template metadata (`jobs-receipt-templates/catalog.json`)
- jobs retry-ledger metadata (`jobs-retry-ledgers/catalog.json`)
- resource reflection metadata (`../resource-reflection/catalog.json`)
- runtime compatibility matrix metadata (`../runtime-compatibility/catalog.json`)
  with `runtime_app_source_ids`, `by_runtime_app_source_id`, and `by_source_id`
  alongside the existing source-path view

The launch-profile, build-graph, and distribution catalogs now also expose query-ready indexes and cross-links:

- `launch-profiles/catalog.json` includes lane/app/host/delivery indexes plus launch-to-receipt bindings per preset
- `launch-profiles/catalog.json` also includes `by_runtime_app_source_id` for source-registry lookups
- `build-graphs/catalog.json` includes queue/graph-kind/input/output/distribution indexes and linked distribution channels per graph
- `distribution/catalog.json` includes channel-kind/approval/artifact-root/build-graph indexes and linked build graphs per channel

The jobs receipt-schema, jobs receipt-template, and jobs retry-ledger catalogs now also expose query-ready indexes and contract joins:

- `jobs-receipt-schemas/catalog.json` includes queue/dispatch/distribution/retry/job-state/promotion-state/template/tensor/kernel indexes plus linked jobs pipeline and kernel metadata
- `jobs-receipt-templates/catalog.json` includes template/schema/dispatch/queue/distribution/retry/kernel indexes and links to jobs graph/channel contracts
- `jobs-retry-ledgers/catalog.json` includes retry-ledger/dispatch/queue/delivery/state/resume-policy/receipt/kernel indexes plus linked jobs tensor pipeline metadata

These jobs catalogs are backed by three canonical receipt examples covering `completed`, `running`, and `failed` states plus a multi-entry retry ledger, so downstream consumers can query both the happy path and the retry path without adding host-side fixtures.

The resource reflection catalog now also exposes deeper cross-links and indexes:

- `../resource-reflection/catalog.json` now carries per-descriptor queue/input/output/channel/pipeline/kernels metadata plus kernel consumes/produces fields
- it now includes top-level linked runtime-contract and GPU-catalog entry snapshots so downstream tooling can query one document instead of joining catalogs manually
- indexes now include `by_artifact_root`, `by_build_graph_queue`, `by_distribution_channel_kind`, `by_tensor_pipeline_pass`, `by_kernel_stage`, `by_kernel_tensor_role`, and `by_contract_path`

The runtime compatibility catalog now also exposes a matrix-shaped snapshot:

- `../runtime-compatibility/catalog.json` carries backend/target matrix rows derived from `runtime_apps.json`
- it includes the compatibility matrix, compatibility window, launch-readiness descriptors, and manifest-derived feature-pack/budget-window tier views from `runtime_compatibility_runtime.kn`
- indexes now include `by_matrix_cell_id`, `by_backend_kind`, `by_target_kind`, `by_runtime_app`, `by_output_target`, `by_source_path`, `by_feature_pack_tier`, `by_budget_window_tier`, and `by_policy_bundle_id`

Regenerate these snapshots with:

- `powershell -ExecutionPolicy Bypass -File tools/reflection/generate_runtime_reflection_catalogs.ps1`
