# Universal Kain 3D Template

This workspace is a Kain-first template for building full 3D runtime applications without requiring `rustc` or `cargo` on the user machine.

The template is intentionally platform-like rather than sample-like:

- authored source lives in Kain
- system registration is manifest-driven
- GPU work flows through SPIR-V and tensor-oriented compute pipelines
- tool shells are built with Kain UI and the native UI materialization lane
- canonical runtime targets stay slim in `runtime_apps.json`, while lane/workspace selection lives in `workspace_presets.json`; repeated rows are keyed by `source_id` and project `source_path` from `sources.json` instead of duplicating it inline
- engine-system rows are keyed by `source_id` and project `source_path` from `sources.json` instead of duplicating that path inline
- authored GPU kernels stay source-id-first in `gpu_kernels.json`, with the reflection generator projecting `source_path` from `sources.json` instead of keeping the path in the authored kernel manifest
- artifact generation, review delivery, machine export, XR/runtime deployment, and publishing are expressed as data-driven orchestration surfaces
- core engine-grade topology, bake, scripting, AI, and modding systems are expressed as reusable Kain runtime packs instead of app-local glue
- rigging, deformation, painting, UV, and brush systems are expressed as reusable Kain runtime packs instead of bespoke editor subsystems
- color management, media pipelines, narrative state, haptics routing, and update channels are expressed as reusable Kain runtime packs instead of host-specific glue
- optional FFI exists only as a contract-driven extension of Kain-owned runtime surfaces
- any missing language/runtime capability is recorded in [`limitations.md`](M:/Templates/3D/limitations.md) instead of being hidden behind bespoke engine code

## Template Surface

Use these files as the fastest route to the current template contract:

- authored entry app: [`src-kain/apps/universal_3d_workbench/main.kn`](M:/Templates/3D/src-kain/apps/universal_3d_workbench/main.kn)
- runtime and source registration: [`manifests/runtime_apps.json`](M:/Templates/3D/manifests/runtime_apps.json) and [`manifests/sources.json`](M:/Templates/3D/manifests/sources.json)
- workspace, graph, kernel, tensor, UI, and distribution manifests: [`manifests/workspace_presets.json`](M:/Templates/3D/manifests/workspace_presets.json), [`manifests/build_graphs.json`](M:/Templates/3D/manifests/build_graphs.json), [`manifests/gpu_kernels.json`](M:/Templates/3D/manifests/gpu_kernels.json), [`manifests/tensor_pipelines.json`](M:/Templates/3D/manifests/tensor_pipelines.json), [`manifests/ui_surfaces.json`](M:/Templates/3D/manifests/ui_surfaces.json), and [`manifests/distribution_channels.json`](M:/Templates/3D/manifests/distribution_channels.json)
- main package contract: [`KAIN.toml`](M:/Templates/3D/KAIN.toml)
- committed generated reflection outputs: [`generated/runtime-reflection`](M:/Templates/3D/generated/runtime-reflection), [`generated/runtime-reflection/runtime-apps`](M:/Templates/3D/generated/runtime-reflection/runtime-apps), [`generated/runtime-reflection/launch-profiles`](M:/Templates/3D/generated/runtime-reflection/launch-profiles), [`generated/runtime-reflection/engine-systems`](M:/Templates/3D/generated/runtime-reflection/engine-systems), [`generated/runtime-reflection/gpu`](M:/Templates/3D/generated/runtime-reflection/gpu), [`generated/runtime-reflection/tensor-pipelines`](M:/Templates/3D/generated/runtime-reflection/tensor-pipelines), [`generated/runtime-reflection/source-registry`](M:/Templates/3D/generated/runtime-reflection/source-registry), [`generated/runtime-reflection/build-graphs`](M:/Templates/3D/generated/runtime-reflection/build-graphs), [`generated/runtime-reflection/distribution`](M:/Templates/3D/generated/runtime-reflection/distribution), [`generated/runtime-reflection/jobs-receipt-schemas`](M:/Templates/3D/generated/runtime-reflection/jobs-receipt-schemas), [`generated/runtime-reflection/jobs-receipt-templates`](M:/Templates/3D/generated/runtime-reflection/jobs-receipt-templates), [`generated/runtime-reflection/jobs-retry-ledgers`](M:/Templates/3D/generated/runtime-reflection/jobs-retry-ledgers), [`generated/resource-reflection`](M:/Templates/3D/generated/resource-reflection), and [`generated/runtime-compatibility`](M:/Templates/3D/generated/runtime-compatibility) with descriptor snapshots under [`generated/runtime-reflection/runtime-apps/descriptors`](M:/Templates/3D/generated/runtime-reflection/runtime-apps/descriptors), [`generated/runtime-reflection/launch-profiles/descriptors`](M:/Templates/3D/generated/runtime-reflection/launch-profiles/descriptors), [`generated/runtime-reflection/engine-systems/descriptors`](M:/Templates/3D/generated/runtime-reflection/engine-systems/descriptors), [`generated/runtime-reflection/gpu/descriptors`](M:/Templates/3D/generated/runtime-reflection/gpu/descriptors), [`generated/runtime-reflection/tensor-pipelines/descriptors`](M:/Templates/3D/generated/runtime-reflection/tensor-pipelines/descriptors), [`generated/runtime-reflection/source-registry/descriptors`](M:/Templates/3D/generated/runtime-reflection/source-registry/descriptors), [`generated/runtime-reflection/build-graphs/descriptors`](M:/Templates/3D/generated/runtime-reflection/build-graphs/descriptors), [`generated/runtime-reflection/distribution/descriptors`](M:/Templates/3D/generated/runtime-reflection/distribution/descriptors), [`generated/runtime-reflection/jobs-receipt-schemas/descriptors`](M:/Templates/3D/generated/runtime-reflection/jobs-receipt-schemas/descriptors), [`generated/runtime-reflection/jobs-receipt-templates/descriptors`](M:/Templates/3D/generated/runtime-reflection/jobs-receipt-templates/descriptors), [`generated/runtime-reflection/jobs-retry-ledgers/descriptors`](M:/Templates/3D/generated/runtime-reflection/jobs-retry-ledgers/descriptors), and [`generated/runtime-compatibility/descriptors`](M:/Templates/3D/generated/runtime-compatibility/descriptors); the runtime-app catalog exposes host/runtime/output indexes for the 31-entry projection set, the launch-profile catalog now carries a descriptor-rooted companion for the workspace preset/runtime binding surface, the engine-system catalog now projects lane, source, runtime-app, and workspace-preset lookups from `manifests/engine_systems.json`, the GPU catalog now projects `source_id` alongside `source_path` with indexable stage/tensor-role views, the tensor-pipeline catalog now indexes domain/priority/residency/pass ids plus GPU stage/tensor-role and pass source-id/path joins, the build-graph, distribution, and jobs catalogs now expose descriptor-rooted snapshots alongside queue/channel/retry indexes, the reflection runtime profile now names `source_registry_catalog` directly, and the source-registry catalog also carries direct workspace-preset lookup indexes for runtime-app, launch-manifest, and receipt IDs
- regeneration entrypoint: [`tools/reflection/generate_runtime_reflection_catalogs.ps1`](M:/Templates/3D/tools/reflection/generate_runtime_reflection_catalogs.ps1)

The template currently ships one authored workbench source and many manifest-derived runtime projections. Treat the projections as downstream surfaces, not separate source trees.

## What This Template Covers

- DCC suites
- game engines
- sculpting applications
- material and shader graph editors
- world-building and terrain biome tools
- animation, rigging, mocap, and constraint workbenches
- simulation and destruction authoring stacks
- procedural modeling systems
- grooming and strand tools
- volumetric and FX authoring
- photogrammetry and scan reconstruction
- collaboration, review, and annotation stages
- artifact materialization and deployment orchestration
- plugin/package builders and release publishers
- renderer graph authoring and frame scheduling studios
- behavior graph and runtime state authoring lanes
- CAD/NURBS surface, assembly, and drafting workbenches
- robotics, digital twin, and toolpath authoring stacks
- live asset sync, hot reload, and multi-workspace mirror control
- compositor, editorial, and finishing suites
- USD-style scene exchange and pipeline handoff hubs
- metrology, inspection, and tolerance validation labs
- fabrication scheduling and machine delivery towers
- persistent review databases and searchable evidence consoles
- scene semantics, collection policies, and semantic view composition
- query, picking, and selection resolution laboratories
- standalone runtime bundle assembly and deployment registries
- artifact materialization receipts and generated-output control centers
- asset registry, lineage, dependency, and residency consoles
- character, facial, and performer-authoring systems
- virtual production stages with camera and LED wall coordination
- geospatial, survey, and digital twin streaming stacks
- XR authoring and immersive runtime shells
- networked runtimes, multiplayer/editor session replication, and authority control
- device discovery, hardware control, tracker/sensor routing, and machine IO coordination
- runtime reflection, schema catalogs, contract metadata, and GPU reflection consoles
- runtime compatibility hubs, launch-readiness matrices, and backend/window validation receipts
- lookdev, lighting review, wedge management, and reference-match approval stages
- advanced solver authoring, coupled simulation graphing, and checkpoint branch labs
- delivery materializer towers for resumable, manifest-driven artifact promotion
- scene bundle authoring and launch-preset studios
- native widget designers for docked desktop tools and viewport overlays
- interchange hubs for broad import, export, transcode, and archive handoff
- automation recipe towers for scheduled and event-driven production workflows
- project knowledge and operations consoles for searchable runtime context
- sequencing, shot assembly, editorial conform, and cinematic timeline workbenches
- navigation, pathfinding, navmesh, and crowd-flow authoring systems
- vehicle dynamics, suspension, drivetrain, and handling labs
- lighting pipeline, probe bake, shadow atlas, and exposure-control stages
- broadcast, livestream, rundown, and program-control rooms
- ZBrush-style native desktop applications
- evaluation-graph and dependency-scheduling consoles
- native viewport-stack, overlay, and camera-control systems
- interaction, gizmo, shortcut, and command-context systems
- autosave, persistence, branching, and crash-recovery layers
- render farm, bake queue, and background job-control towers
- telemetry, profiling, diagnostics, and budget-observability consoles
- capability policy, sandbox, trust-zone, and audit governance surfaces
- entity and archetype authoring stacks for reusable scene/runtime composition
- prefab assembly, variant patching, and launch-preset authoring labs
- command, macro, transaction, and tool-action orchestration consoles
- state-schema migration, compatibility, and project-upgrade control rooms
- multi-window workspace-layout, dock-graph, and overlay-routing designers
- rigging, control-rig, skin binding, and retarget authoring studios
- deformation-stack, corrective-shape, wrap, and lattice authoring labs
- texture, vertex, projection, and layer-stack painting systems
- UV charting, seam policy, UDIM layout, and texel-density workbenches
- brush-library, alpha preset, stylus, and stroke-engine foundries
- OCIO/LUT/HDR color-management control rooms
- media clip, plate, proxy, and transcode pipeline stages
- narrative, dialogue, branching-state, and quest authoring studios
- haptics, stylus feedback, XR force, and tactile routing labs
- update-channel, patch, rollback, and install registries

- authored scene-object graphs, object classes, component mounts, and mutation-safe scene composition
- native control families for outliners, inspectors, tables, timelines, and command bars
- runtime/import/export schema catalogs, compatibility windows, and exchange validation
- rigid-body, collision, joint, and deterministic physics authoring/runtime systems
- shader families, permutation scheduling, program layouts, and reflection-safe bindings
- resource residency, streaming, virtual-memory budgets, and delivery-aware governance
- source-level material documents, layer stacks, preview compilation, and shader-hook authoring
- higher-order editor widget suites, pane families, state persistence, and modal desktop tooling
- transactional scene mutation, delta replay, and audit-visible scene change receipts
- render backend delegation, frame capture, review/debug routing, and multi-host presentation control
- resource reflection catalogs, budget inspection, and compatibility-aware residency introspection
- runtime compatibility matrices, launch readiness, backend/device windows, and promotion-safe validation
- color transforms, display profiles, review-safe looks, and paint-render-compositor parity
- media clips, image sequences, backplates, proxies, and delivery-safe transcode routes
- branching narrative graphs, dialogue bundles, and objective-state progression
- stylus/controller/XR haptic devices, force profiles, and tool-context-safe feedback routes
- patch channels, rollback bundles, compatibility installs, and resumable update promotion

## Included System Packs

- project/runtime packaging contracts in [`project_profile.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/project_profile.kn)
- core lane registration in [`engine_systems.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/engine_systems.kn)
- render/runtime contracts in [`render_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/render_runtime.kn)
- scene/selection/asset graph contracts in [`scene_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/scene_runtime.kn)
- viewport contracts now carry explicit composition and framing policies in [`viewport_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/viewport_runtime.kn), so launchable viewports stay bound to scene summaries, bounds-driven fit, and aspect-aware camera policy instead of relying on renderer-local guesses
- tensor/pipeline scheduling contracts in [`tensor_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/tensor_runtime.kn)
- asset ingest/bake/export contracts in [`asset_pipeline.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/asset_pipeline.kn)
- material/runtime graph contracts in [`material_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/material_runtime.kn)
- animation/rig/mocap contracts in [`animation_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/animation_runtime.kn)
- simulation contracts in [`simulation_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/simulation_runtime.kn)
- world streaming/terrain/scatter contracts in [`world_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/world_runtime.kn)
- editor shell/transactions/workspace contracts in [`editor_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/editor_runtime.kn)
- workbench UI defaults in [`ui_workbench.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/ui_workbench.kn)
- workspace preset registry and shortcut contracts in [`workspace_preset_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/workspace_preset_runtime.kn)
  with manifest export descriptors and materialization-receipt routing
- procedural modeling contracts in [`procedural_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/procedural_runtime.kn)
- grooming contracts in [`grooming_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/grooming_runtime.kn)
- volumetric contracts in [`volumetric_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/volumetric_runtime.kn)
- photogrammetry contracts in [`photogrammetry_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/photogrammetry_runtime.kn)
- audio graph/runtime contracts in [`audio_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/audio_runtime.kn)
- collaboration contracts in [`collaboration_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/collaboration_runtime.kn)
- plugin packaging contracts in [`plugin_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/plugin_runtime.kn)
- orchestration/materialization contracts in [`orchestration_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/orchestration_runtime.kn)
- constraint graph contracts in [`constraint_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/constraint_runtime.kn)
- biome authoring contracts in [`biome_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/biome_runtime.kn)
- destruction authoring contracts in [`destruction_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/destruction_runtime.kn)
- review annotation contracts in [`review_annotation_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/review_annotation_runtime.kn)
- publishing contracts in [`publishing_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/publishing_runtime.kn)
- renderer graph contracts in [`renderer_graph_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/renderer_graph_runtime.kn)
- behavior graph contracts in [`behavior_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/behavior_runtime.kn)
- CAD/NURBS contracts in [`cad_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/cad_runtime.kn)
- robotics/toolpath contracts in [`robotics_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/robotics_runtime.kn)
- live sync and hot reload contracts in [`live_sync_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/live_sync_runtime.kn)
- compositor and finishing contracts in [`compositor_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/compositor_runtime.kn)
- scene exchange contracts in [`scene_exchange_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/scene_exchange_runtime.kn)
- metrology and inspection contracts in [`metrology_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/metrology_runtime.kn)
- fabrication scheduling contracts in [`fabrication_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/fabrication_runtime.kn)
- review database contracts in [`review_database_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/review_database_runtime.kn)
- scene semantics contracts in [`scene_semantics_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/scene_semantics_runtime.kn)
- query and selection contracts in [`query_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/query_runtime.kn)
- runtime bundle assembly contracts in [`runtime_bundle_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/runtime_bundle_runtime.kn)
  with workspace-preset routing, launch-manifest exports, materialization receipts, and compatibility gates
- output materialization contracts in [`materialization_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/materialization_runtime.kn)
- asset registry and lineage contracts in [`asset_registry_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/asset_registry_runtime.kn)
- character/facial contracts in [`character_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/character_runtime.kn)
- virtual production contracts in [`virtual_production_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/virtual_production_runtime.kn)
  with tracked-lens calibration, playback sync, and stage-export roots
- geospatial/digital twin contracts in [`geospatial_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/geospatial_runtime.kn)
- XR runtime contracts in [`xr_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/xr_runtime.kn)
- networking/replication contracts in [`networking_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/networking_runtime.kn)
- device/hardware orchestration contracts in [`device_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/device_runtime.kn)
- runtime/schema reflection contracts in [`reflection_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/reflection_runtime.kn)
  with workspace-preset metadata, launch/receipt binding metadata, schema template/index, preset-receipt, build-graph, and distribution-receipt catalogs
  with committed workspace-preset reflection snapshots under `generated/runtime-reflection/workspace-preset-*` so `runtime_reflection_tensor_pipeline` consumers stay manifest-driven
  with committed launch-profile, source-registry, build-graph, distribution-receipt, GPU, and runtime-contract reflection snapshots under `generated/runtime-reflection/{launch-profiles,source-registry,build-graphs,distribution,gpu,contracts}` for bundle and compatibility queries
  with committed descriptor-rooted documents under `generated/runtime-reflection/{launch-profiles,source-registry,gpu,build-graphs,distribution}/descriptors`, plus source-registry indexes that now cover the full workspace-preset manifest instead of only the three launch-example presets
  with query-ready indexes and graph/channel cross-links in launch-profile, build-graph, and distribution catalogs generated by `tools/reflection/generate_runtime_reflection_catalogs.ps1`
  with query-ready indexes and explicit jobs graph/channel/pipeline/kernel joins in `generated/runtime-reflection/{jobs-receipt-schemas,jobs-receipt-templates,jobs-retry-ledgers}` generated by `tools/reflection/generate_runtime_reflection_catalogs.ps1`
- lookdev/lighting review contracts in [`lookdev_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/lookdev_runtime.kn)
  with wedge-set scoring, perceptual diff policy, and approval routing
- advanced solver authoring contracts in [`simulation_authoring_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/simulation_authoring_runtime.kn)
- delivery materializer contracts in [`delivery_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/delivery_runtime.kn)
  with a dedicated workspace-preset export batch and receipt-resume graph
- graph materialization contracts in [`graph_materialization_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/graph_materialization_runtime.kn)
  with explicit workspace-preset manifest inputs, the concrete `workspace_preset_launch_receipt_resolve` materializer kernel id, launch/export/schema-template roots, and bundle/delivery consumer contracts
- scene bundle contracts in [`scene_bundle_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/scene_bundle_runtime.kn)
- native widget contracts in [`widget_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/widget_runtime.kn)
- interchange contracts in [`interchange_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/interchange_runtime.kn)
- automation recipe contracts in [`automation_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/automation_runtime.kn)
- knowledge operations contracts in [`knowledge_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/knowledge_runtime.kn)
- sequencing and editorial contracts in [`sequencing_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/sequencing_runtime.kn)
- navigation and crowd contracts in [`navigation_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/navigation_runtime.kn)
- vehicle dynamics contracts in [`vehicle_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/vehicle_runtime.kn)
- lighting pipeline contracts in [`lighting_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/lighting_runtime.kn)
- broadcast and live-program contracts in [`broadcast_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/broadcast_runtime.kn)
- evaluation graph contracts in [`evaluation_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/evaluation_runtime.kn)
- viewport stack contracts in [`viewport_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/viewport_runtime.kn)
- interaction and gizmo contracts in [`interaction_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/interaction_runtime.kn)
- persistence and recovery contracts in [`persistence_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/persistence_runtime.kn)
- jobs and farm orchestration contracts in [`jobs_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/jobs_runtime.kn)
  with explicit dispatch-graph, worker-capability, output-root, receipt-schema, template/index, retry-ledger, and delivery-registry descriptors
- telemetry and diagnostics contracts in [`telemetry_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/telemetry_runtime.kn)
- capability-policy contracts in [`security_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/security_runtime.kn)
- entity/archetype mutation contracts in [`entity_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/entity_runtime.kn)
- prefab assembly and variant contracts in [`prefab_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/prefab_runtime.kn)
- command/macro transaction contracts in [`command_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/command_runtime.kn)
- state migration and compatibility contracts in [`state_migration_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/state_migration_runtime.kn)
- workspace layout and dock graph contracts in [`workspace_layout_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/workspace_layout_runtime.kn)
- workspace preset registry and launch export contracts in [`workspace_preset_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/workspace_preset_runtime.kn)
- scene-object authoring contracts in [`scene_object_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/scene_object_runtime.kn)
- native control-family contracts in [`native_controls_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/native_controls_runtime.kn)
- runtime/import/export schema contracts in [`schema_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/schema_runtime.kn)
- rigid-body and collision contracts in [`physics_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/physics_runtime.kn)
- shader family and permutation contracts in [`shader_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/shader_runtime.kn)
- resource residency and streaming contracts in [`resource_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/resource_runtime.kn)
- material source authoring contracts in [`material_source_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/material_source_runtime.kn)
- editor widget suite contracts in [`editor_widget_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/editor_widget_runtime.kn)
- scene mutation contracts in [`scene_mutation_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/scene_mutation_runtime.kn)
- render delegation contracts in [`render_delegation_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/render_delegation_runtime.kn)
- resource reflection contracts in [`resource_reflection_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/resource_reflection_runtime.kn)
- committed resource-reflection catalog snapshot in [`generated/resource-reflection/catalog.json`](M:/Templates/3D/generated/resource-reflection/catalog.json)
- committed resource-reflection descriptor snapshots in [`generated/resource-reflection/descriptors`](M:/Templates/3D/generated/resource-reflection/descriptors) for descriptor-scoped policy/runtime-link/kernel/contract lookups
- runtime compatibility contracts in [`runtime_compatibility_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/runtime_compatibility_runtime.kn)
- committed runtime-compatibility matrix snapshot in [`generated/runtime-compatibility/catalog.json`](M:/Templates/3D/generated/runtime-compatibility/catalog.json)
  with backend/target matrix rows, compatibility descriptors, launch-readiness metadata, and manifest-derived feature-pack/budget-window tier views
- FFI bridge policy in [`ffi_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/ffi_runtime.kn)
- rigging contracts in [`rigging_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/rigging_runtime.kn)
- deformation stack contracts in [`deformation_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/deformation_runtime.kn)
- painting contracts in [`painting_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/painting_runtime.kn)
- UV layout contracts in [`uv_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/uv_runtime.kn)
- brush engine contracts in [`brush_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/brush_runtime.kn)

- scene-object authoring, object-class composition, and mutation-receipt workbenches
- native control-family design surfaces for inspector/outliner/timeline/table widgets
- schema contract consoles for runtime/import/export compatibility and validation gates
- rigid-body, collision, and joint authoring labs for engine/runtime physics systems
- shader permutation studios for render/material program variants and binding layouts
- resource residency consoles for streaming tiers, eviction policy, and budget governance

## Runtime Applications And Workspace Presets

The template now keeps canonical packaging/runtime targets in [`runtime_apps.json`](M:/Templates/3D/manifests/runtime_apps.json) and lane/workspace selection in [`workspace_presets.json`](M:/Templates/3D/manifests/workspace_presets.json). Both manifests use `source_id` as the repeated key, while `source_path` is resolved from [`sources.json`](M:/Templates/3D/manifests/sources.json) through the reflection generator.

Current catalog shape:

- 31 canonical runtime targets with real host/runtime-kind differences
- 109 workspace presets that bind users into focused lanes without multiplying identical apps
- one authored source path projected through the source registry rather than repeated across every runtime-app and workspace-preset row

Canonical runtime targets include:

- `universal_3d_workbench` for the primary native editor shell
- `asset_pipeline_studio` for ingest, conversion, validation, and baking flows
- `headless_build_orchestrator` for script-driven generation and batch workflows
- `game_runtime_shell` for in-engine runtime authoring and play-mode surfaces
- `collaboration_review_stage` for synced review sessions
- `plugin_package_builder` for extension packaging
- `artifact_orchestration_studio` for build graph and materialization control
- `release_publisher` for channel-aware runtime and package publishing
- `live_sync_control_tower` for workspace mirrors, runtime hot reload, and authority control
- `scene_exchange_hub` for USD-style stage composition and pipeline handoff
- `runtime_bundle_manager` for standalone bundle assembly, launch profiles, and delivery manifests
- bundle delivery remains manifest-driven through workspace presets and bundle receipts, rather than app-local launch logic
- launch-manifest exports now materialize from the preset graph and feed runtime-bundle inputs instead of hiding in generic bundle metadata
- preset launch exports now also have a dedicated registered runtime pack, tensor pipeline, and workbench surface instead of only piggybacking on adjacent bundle and delivery packs
- `delivery_materializer_tower` for resumable delivery batches and promotion-aware graph execution
- `automation_recipe_tower` for scheduled jobs, event-driven command graphs, and execution receipt control
- `xr_experience_lab` for immersive authoring, stereo runtime presentation, and spatial interaction
- `device_control_center` for GPU/display/sensor/machine routing and authority control

Workspace presets carry the broader authoring surface, including:

- `render_lab`, `simulation_console`, `sculpt_studio`, and `procedural_world_lab`
- `scene_object_studio`, `native_control_designer`, `schema_contract_console`, and `physics_authoring_lab`
- `rigging_authoring_studio`, `deformation_stack_lab`, `texture_paint_studio`, `uv_layout_workbench`, and `brush_system_foundry`
- `color_management_control_room`, `media_pipeline_stage`, `narrative_state_studio`, `haptics_routing_lab`, and `update_channel_registry`

## GPU And Tensor Coverage

The manifest set now treats GPU and tensor orchestration as first-class systems:

- authored kernel catalog in [`gpu_kernels.json`](M:/Templates/3D/manifests/gpu_kernels.json) with source-id-first entries projected through the shared source registry
- source registration in [`sources.json`](M:/Templates/3D/manifests/sources.json)
- tensor scheduling lanes in [`tensor_pipelines.json`](M:/Templates/3D/manifests/tensor_pipelines.json)
- workspace-preset catalog in [`workspace_presets.json`](M:/Templates/3D/manifests/workspace_presets.json)
- workspace preset runtime contracts in [`workspace_preset_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/workspace_preset_runtime.kn)
  with launch-manifest exports, runtime-binding registries, schema-emitter ids, and receipt-aware manifest routing
- artifact build graphs in [`build_graphs.json`](M:/Templates/3D/manifests/build_graphs.json)
- release routing in [`distribution_channels.json`](M:/Templates/3D/manifests/distribution_channels.json)

Coverage includes:

- clustered visibility and viewport compositing
- sculpt deformation and dyntopo remeshing
- rig skinning, retargeting, and constraint solving
- cloth, particles, fluids, destruction, and debris workflows
- terrain clipmap baking, biome distribution, and world partition visibility
- procedural scatter and field evaluation
- groom strand interpolation
- volumetric integration
- scan reconstruction fusion
- audio-reactive analysis
- collaboration presence resolution
- artifact graph materialization and review overlay capture
- renderer graph scheduling
- behavior state evaluation
- NURBS surface tessellation
- toolpath kinematics resolution
- live asset delta merge
- compositor frame composition
- scene exchange stage resolution
- metrology deviation analysis
- fabrication schedule resolution
- review evidence indexing
- scene semantic indexing
- selection query resolution
- runtime bundle assembly
- materialization receipt indexing
- asset lineage resolution
- character facial pose solving and deformation feeds
- virtual production camera-to-wall stage sync
- geospatial tile and survey alignment resolution
- XR stereo frame composition and immersive UI timing
- network replication, authority, and rollback state resolution
- device discovery, telemetry, and frame-sync orchestration
- runtime schema, contract metadata, and canonical template/index emission metadata indexing
- lookdev lighting review, wedge scoring, and reference matching
- advanced solver graph evaluation for coupled simulation authoring
- delivery batch materialization across graph nodes, receipts, and approval channels
- scene bundle composition and launch-preset resolution
- native widget layout, overlay, and inspector schema composition
- interchange transcode and delivery package normalization
- automation recipe scheduling and queue shaping
- knowledge graph indexing across production, asset, and operational records
- sequencing timeline resolve, shot conform, and cinematic preview control
- navigation corridor solve, navmesh/runtime pathing, and crowd-flow staging
- vehicle dynamics integration, drivetrain state, and wheel-contact feedback
- lighting probe-grid resolve, bake orchestration, and shadow atlas continuity
- broadcast program mixing, rundown-state control, and live overlay routing
- dependency scheduling, cache invalidation, checkpoint replay, and delivery-graph evaluation
- viewport frame packets, camera rigs, overlay composition, and native renderer presentation
- workspace preset launch-manifest and delivery-receipt materialization
- workspace preset reflection catalogs for launch schemas, launch templates, launch/receipt bindings, receipt schemas, receipt templates, and receipts at `generated/runtime-reflection/workspace-preset-*`
- committed launch-profile, build-graph, distribution-receipt, GPU, and runtime-contract reflection catalogs at `generated/runtime-reflection/{launch-profiles,build-graphs,distribution,gpu,contracts}` with query indexes/cross-links for preset->receipt and graph<->channel resolution, plus descriptor-rooted launch-profile, build-graph, and distribution snapshots under `generated/runtime-reflection/{launch-profiles,build-graphs,distribution}/descriptors`
- input routing, gizmo transforms, tool contexts, shortcuts, and command dispatch
- snapshot deltas, autosave, recovery checkpoints, and session-state materialization
- farm queue dispatch, worker capability matching, retries, and promotion-aware job receipts
- frame telemetry, budget rollups, diagnostics traces, and release-level observability
- capability grants, trust-zone policy resolution, audit emission, and secured FFI/device/network routing

## Kain UI Workbench Coverage

The workbench app in [`main.kn`](M:/Templates/3D/src-kain/apps/universal_3d_workbench/main.kn) is structured to host:

- docked viewport and telemetry surfaces
- outliner, inspector, and property rails
- graph, timeline, orchestration, biome, and constraint views
- material, procedural, groom, volume, scan, destruction, review, and publishing tooling surfaces
- renderer graph, behavior graph, CAD, robotics, live sync, compositor, exchange, metrology, fabrication, and review database control surfaces
- scene semantics, selection query, runtime bundle, materialization, and asset registry control surfaces
- character, virtual production, geospatial, XR, and networking control surfaces
- evaluation graph, viewport stack, interaction lab, persistence console, jobs tower, jobs receipt ledger, telemetry console, and security policy surfaces
- device control, runtime reflection, lookdev review, solver authoring, delivery materializer, scene bundle, native widget, interchange, automation, and knowledge surfaces
- stage calibration, runtime bundle receipts, and lookdev approval surfaces
- workspace preset catalog and launch-export surfaces
- sequencing, navigation, vehicle, lighting, and broadcast control surfaces
- scene-object, native-control, schema, physics, shader, and resource governance surfaces
- asset ingest, deployment, and package routing panels

## Current Data Snapshot

- engine systems: `119`
- GPU kernels: `116`
- tensor pipelines: `110`
- runtime apps: `31`
- workspace presets: `109`
- UI surfaces: `118`
- registered sources: `235`
- build graphs: `87`
- distribution channels: `87`

## Working Rule

If a needed 3D feature starts pushing toward hand-written host code, first add the contract, manifest entry, kernel shape, runtime app lane, or stdlib surface here.
Only put something in [`limitations.md`](M:/Templates/3D/limitations.md) when the language/runtime genuinely needs a new capability.


## Latest Expansion

This run deepened the runtime-app reflection surface by adding a committed runtime-app catalog snapshot and a folder README under `generated/runtime-reflection/runtime-apps`, keeping the host/runtime/output projection set discoverable without reopening `runtime_apps.json`.

What was updated:

- added a committed runtime-app catalog surface under [`generated/runtime-reflection/runtime-apps`](M:/Templates/3D/generated/runtime-reflection/runtime-apps) with a descriptor document and folder README for the 31-entry projection set
- kept the reflection runtime profile and root docs aligned so both `source_registry_catalog` and `runtime_app_catalog` are first-class reflection surfaces
- bumped [`KAIN.toml`](M:/Templates/3D/KAIN.toml) to `2.3.18`

Updated snapshot:

- engine systems: `119`
- GPU kernels: `116`
- tensor pipelines: `110`
- runtime apps: `31`
- workspace presets: `109`
- UI surfaces: `118`
- registered sources: `235`
- build graphs: `87`
- distribution channels: `87`

## Identity Cloud Marketplace DataOps And Fleet

This expansion adds platform-scale operational lanes that serious 3D applications need once they move beyond a single workstation authoring shell:

- identity and entitlement governance in [`identity_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/identity_runtime.kn)
- cloud session bursting, remote storage, and elastic compute in [`cloud_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/cloud_runtime.kn)
- marketplace catalogs, licensing, and package promotion in [`marketplace_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/marketplace_runtime.kn)
- dataset lineage, corpus governance, and data-product orchestration in [`dataops_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/dataops_runtime.kn)
- fleet capacity, leases, and deadline-aware dispatch in [`fleet_runtime.kn`](M:/Templates/3D/src-kain/stdlib/three_d_runtime/fleet_runtime.kn)

Representative app lanes now also include:

- `identity_access_console` for operator identity, role/entitlement resolution, and authority handoff
- `cloud_session_control_tower` for remote sessions, storage tiers, and burst compute governance
- `marketplace_registry_hub` for asset/plugin catalogs, licensing, and package promotion
- `dataops_dataset_foundry` for dataset registries, lineage, and training-corpus shaping
- `fleet_orchestration_tower` for worker/device pools, leasing, and deadline-aware dispatch


