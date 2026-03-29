# Runtime Reflection Catalogs

These snapshots mirror the runtime reflection descriptors in `src-kain/stdlib/three_d_runtime/reflection_runtime.kn` and the workspace-preset materializer bridge in `graph_materialization_runtime.kn`.
The same reflection profile now also registers the sibling `generated/resource-reflection/catalog.json` snapshot for resource residency, inspection, and compatibility queries.
The same generation pass also emits `generated/runtime-compatibility/catalog.json` as the committed matrix snapshot for target/backend window and launch-readiness queries.
The same generation pass also emits `generated/runtime-reflection/engine-systems/catalog.json` so the core engine-system lane registry stays queryable from one committed snapshot instead of only through `manifests/engine_systems.json`.

They provide committed examples for downstream generators without requiring a live `kain build` run. Catalogs stay manifest-driven and currently include:

- workspace-preset registry metadata (`workspace-presets/catalog.json`)
- workspace-preset launch-schema metadata (`workspace-preset-launch-schemas/catalog.json`)
- workspace-preset launch-template metadata (`workspace-preset-launch-templates/catalog.json`)
- workspace-preset receipt-schema metadata (`workspace-preset-receipt-schemas/catalog.json`)
- workspace-preset receipt-template metadata (`workspace-preset-receipt-templates/catalog.json`)
- workspace-preset launch/receipt contract bindings (`workspace-preset-launch-receipt-bindings/catalog.json`)
- workspace-preset receipt metadata (`workspace-preset-receipts/catalog.json`)
- workspace-preset descriptor documents under
  `workspace-presets/descriptors` and `workspace-preset-*/descriptors` so
  downstream tools can consume descriptor-rooted payloads without joining only
  top-level catalog files
- runtime-app metadata (`runtime-apps/catalog.json`) with host/runtime/output
  indexes plus a descriptor document under
  `runtime-apps/descriptors/runtime_app_catalog.json` for manifest-driven app
  queries without reopening the full manifest
- runtime-app folder documentation under `runtime-apps/README.md` so the
  committed runtime-app catalog is discoverable from the reflection tree itself
- launch-profile metadata (`launch-profiles/catalog.json`) with focus-lane,
  runtime-app, host, delivery-registry, and source-aware indexes plus a
  descriptor document under
  `launch-profiles/descriptors/launch_profile_catalog.json` for manifest-driven
  workspace-preset/runtime binding queries
- launch-profile folder documentation under `launch-profiles/README.md` so the
  committed launch-profile catalog is discoverable from the reflection tree
  itself
- workspace-preset folder documentation under `workspace-presets/README.md` so
  the committed workspace-preset catalog is discoverable from the reflection
  tree itself
- engine-system metadata (`engine-systems/catalog.json`) with lane/source/
  runtime-app/workspace-preset indexes plus a descriptor document under
  `engine-systems/descriptors/engine_system_catalog.json` for lane registration
  queries without reopening only the manifest
- source-registry metadata (`source-registry/catalog.json`) with
  `source_id`/`source_path`, runtime-app projections, and workspace-preset
  projections grouped by authored source
- source-registry descriptor metadata (`source-registry/descriptors/source_registry_catalog.json`)
  so the shared registry contract also has a committed descriptor-rooted payload
- launch-profile metadata (`launch-profiles/catalog.json`) with
  `runtime_app_source_id`, `by_runtime_app_source_id`, and a descriptor-rooted
  companion under `launch-profiles/descriptors` so shared workbench projections
  stay anchored to `sources.json`
- build-graph metadata (`build-graphs/catalog.json`) with queue, graph-kind,
  input, output, and distribution-channel indexes plus a descriptor document
  under `build-graphs/descriptors/build_graph_catalog.json`
- distribution receipt metadata (`distribution/catalog.json`) with
  channel-kind, approval-policy, artifact-root, and build-graph indexes plus a
  descriptor document under `distribution/descriptors/distribution_receipt_catalog.json`
- tensor-pipeline metadata (`tensor-pipelines/catalog.json`) with domain,
  priority, residency, pass, GPU-kernel stage/tensor-role, pass-source, and
  pass-id indexes plus a descriptor document under
  `tensor-pipelines/descriptors/tensor_pipeline_catalog.json`
- GPU kernel reflection metadata (`gpu/catalog.json`) with `source_id`
  projections, a folder README at `gpu/README.md`, and a descriptor snapshot
  under `gpu/descriptors/gpu_reflection_catalog.json`; the authored
  `manifests/gpu_kernels.json` surface is source-id-first and the generator
  resolves `source_path` from `manifests/sources.json`
- tensor-pipeline metadata (`tensor-pipelines/catalog.json`) with domain,
  priority, residency, pass, tensor-role, and stage indexes plus a descriptor
  document under `tensor-pipelines/descriptors/tensor_pipeline_catalog.json`;
  pass metadata resolves GPU kernel source ids and paths from the shared
  source registry and GPU reflection catalog
- runtime contract metadata (`contracts/catalog.json`)
- schema reflection metadata (`schema/catalog.json`)
- jobs receipt-schema metadata (`jobs-receipt-schemas/catalog.json`)
- jobs receipt-template metadata (`jobs-receipt-templates/catalog.json`)
- jobs retry-ledger metadata (`jobs-retry-ledgers/catalog.json`)
- jobs receipt-schema, receipt-template, and retry-ledger descriptor documents
  under `jobs-receipt-schemas/descriptors`,
  `jobs-receipt-templates/descriptors`, and `jobs-retry-ledgers/descriptors`
- resource reflection metadata (`../resource-reflection/catalog.json`)
- runtime compatibility matrix metadata (`../runtime-compatibility/catalog.json`)
  with `runtime_app_source_ids`, `by_runtime_app_source_id`, and `by_source_id`
  alongside the existing source-path view

The launch-profile, build-graph, and distribution catalogs now also expose query-ready indexes and cross-links:

- `launch-profiles/catalog.json` includes lane/app/host/delivery indexes plus launch-to-receipt bindings per preset
- `launch-profiles/catalog.json` also includes `by_runtime_app_source_id` for source-registry lookups and a descriptor-rooted companion under `launch-profiles/descriptors`
- `source-registry/catalog.json` groups the authored source registry with runtime-app and workspace-preset projections per source id
- `source-registry/descriptors/source_registry_catalog.json` captures the registry contract, manifest roots, and index surface in one descriptor document
- workspace-preset catalogs (`workspace-presets`, `workspace-preset-launch-*`, `workspace-preset-receipt-*`, and `workspace-preset-launch-receipt-bindings`) are emitted by the same generator pass and now carry source-id, preset-id, schema/template join indexes, and committed descriptor roots under `workspace-preset-*/descriptors`
- `build-graphs/catalog.json` includes queue/graph-kind/input/output/distribution indexes and linked distribution channels per graph, with a descriptor-rooted companion under `build-graphs/descriptors`
- `distribution/catalog.json` includes channel-kind/approval/artifact-root/build-graph indexes and linked build graphs per channel, with a descriptor-rooted companion under `distribution/descriptors`
- `tensor-pipelines/catalog.json` includes domain/priority/residency/pass
  indexes plus GPU-kernel stage, tensor-role, source-id, and source-path
  joins for each pass, with a descriptor-rooted companion under
  `tensor-pipelines/descriptors`

The jobs receipt-schema, jobs receipt-template, and jobs retry-ledger catalogs now also expose query-ready indexes and contract joins:

- `jobs-receipt-schemas/catalog.json` includes queue/dispatch/distribution/retry/job-state/promotion-state/lifecycle/artifact-kind/template/tensor/kernel indexes plus linked jobs pipeline and kernel metadata
- `jobs-receipt-templates/catalog.json` includes template/schema/dispatch/queue/distribution/retry/kernel indexes and links to jobs graph/channel contracts
- `jobs-retry-ledgers/catalog.json` includes retry-ledger/dispatch/queue/delivery/state/latest-state/state-transition/resume-policy/receipt/kernel indexes plus linked jobs tensor pipeline metadata

These jobs catalogs are backed by four canonical receipt examples covering `queued`, `running`, `failed`, and `completed` states plus a multi-entry retry ledger, so downstream consumers can query in-flight and terminal paths without adding host-side fixtures.

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
