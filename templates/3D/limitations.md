# Limitations To Add Into Kain

These are not template bugs. They are capability gaps that should be added to the language/runtime so this template can stay Kain-first.

## Required Additions

1. Native runtime scene graph mutation APIs with first-class mesh, skeleton, material, instancer, field, procedural generator, groom, volume, and scan handles.
2. A canonical tensor resource type for image, volume, point cloud, voxel, sparse field, simulation grid, strand, audio-frame, and collaboration-state payloads.
3. First-class render graph declarations so complex frame pipelines do not have to be implied through separate kernels and host orchestration.
4. Native asset import/export contracts for glTF, USD, FBX, Alembic, OpenEXR, MaterialX, texture transcoding, scan ingestion, and plugin package publishing workflows.
5. Stable viewport input, gizmo, camera navigation, picking, and selection event contracts directly accessible from Kain UI/native runtime surfaces.
6. A multi-window docking workspace persistence system exposed at the Kain source level.
7. Realtime scene bundle authoring syntax that can declare cameras, lights, materials, graph nodes, simulation fields, volumetric fields, audio emitters, tool caps, and runtime requirements in source instead of sidecar JSON only.
8. Native timeline, dope sheet, curve editor, graph editor, node editor, and annotation widgets with authored data binding in Kain UI.
9. Compute graph scheduling primitives for chained tensor passes, async execution, residency, streaming, synchronization barriers, and transient memory pools.
10. First-class FFI schema generation for DCC/plugin hosts so external integrations can stay data-driven instead of bespoke.
11. GPU/CPU shared sparse volume, sparse voxel, virtual texturing, strand, and clipmap abstractions.
12. Canonical undo/redo transaction and command history traits across UI, scene, asset, procedural, and simulation lanes.
13. Native scene query APIs for BVH, raycast, signed distance fields, strand queries, and selection masks.
14. First-class audio graph, lip sync, spectrum analysis, and mocap sensor device contracts.
15. Declarative packaging surfaces for plugins, standalone executables, content projects, and embeddable runtime modules.
16. Built-in schema reflection for material graphs, behavior graphs, procedural node systems, and review annotations.
17. Native streaming asset residency callbacks for terrain, world partitions, megascans, groom caches, scan caches, and simulation checkpoints.
18. Canonical collaborative session/runtime authority contracts for multi-user editing, review, annotations, and conflict resolution.
19. Native photogrammetry and reconstruction contracts for calibration, bundle adjustment, fusion, reprojection, and scan cleanup.
20. First-class volumetric authoring/runtime contracts for fog, pyro, signed-distance editing, sparse field IO, and raymarch presentation.
21. Stable extension/plugin ABI packaging so Kain-authored tools can publish reusable feature packs without downstream host glue code.
22. Native constraint graph primitives for rig, assembly, mechanical, and physics relationship authoring with solver reflection and timeline binding.
23. Canonical destruction/fracture contracts for clustered fracture, debris emission, cache baking, and soft-body handoff without bespoke host orchestration.
24. First-class biome and ecosystem authoring resource types for terrain climate masks, species distribution layers, spline-influenced scatter, and material biome blending.
25. Review output and approval contracts for frame drawovers, scene-anchored notes, media captures, and signoff routing directly from Kain-authored surfaces.
26. Declarative distribution and publishing registries for internal catalogs, review drops, and public release channels with manifest-driven approvals.
27. First-class renderer graph source syntax with dependency validation, transient attachment lifetimes, pass scheduling introspection, and frame capture hooks.
28. Canonical behavior graph/state machine contracts for editor tools, gameplay logic, automation graphs, and replicated runtime state with schema reflection.
29. Exact CAD/NURBS/B-rep authoring primitives with trims, booleans, fillets, continuity solving, and STEP/IGES/native drafting exchange.
30. Robotics and machine runtime contracts for articulated kinematics, collision-safe toolpaths, controller profiles, digital twins, and device IO without bespoke host code.
31. Live asset sync, hot reload, and workspace mirroring contracts with deterministic conflict resolution, authority transfer, and partial-scene delta application.
32. First-class compositor and sequence authoring primitives with clip stacks, color pipelines, frame-accurate editorial metadata, and delivery renders exposed directly in Kain UI/native runtime.
33. Canonical USD-style scene exchange contracts with composition arcs, layer muting, variants, payloads, asset resolution, and non-destructive overrides as native Kain runtime surfaces.
34. Native metrology and inspection primitives for deviation fields, feature probes, tolerance schemas, and report bundles that stay Kain-authored instead of host-specific.
35. Declarative fabrication scheduling and nesting contracts for CNC, additive, robotic, and hybrid machine queues with machine capability matching and operator approvals.
36. Persistent review database contracts for indexed captures, annotations, issue threads, approvals, retention policies, and searchable evidence bundles across large productions.
37. First-class scene semantics primitives for collections, variants, semantic tags, authored view layers, scene classes, and cross-domain context binding.
38. Native picking, selection masking, query batching, and scene probe surfaces with GPU/CPU parity and viewport binding contracts.
39. Declarative standalone runtime bundle assembly surfaces for desktop apps, game runtimes, plugin suites, portable workspaces, and embedded runtime kits.
40. Canonical artifact materialization receipt and provenance contracts so generated outputs can be traced, diffed, resumed, and promoted without bespoke pipeline code.
41. Asset registry and lineage primitives for dependency graphs, residency audits, package provenance, semantic classification, and cache invalidation authored directly in Kain.
42. Character and facial authoring primitives for performer rigs, corrective solve layers, wrinkle masks, groom binding, and digital-double variation as first-class Kain runtime surfaces.
43. Virtual production stage contracts for tracked cameras, lens distortion, LED wall sync, stage color pipelines, take recording, and slate-aware metadata binding.
44. Geospatial and survey primitives for coordinate reference systems, large-world precision, terrain/imagery/vector tile streaming, BIM alignment, and survey-control authored scene anchors.
45. XR runtime contracts for stereo display families, spatial input, hand/controller tracking, passthrough composition, comfort policy, and immersive Kain UI surfaces without bespoke host glue.
46. Native networking and replication primitives for entity/state replication, authority transfer, rollback/replay, prediction, session topology, and editor/runtime shared-presence contracts.
47. Native device and hardware reflection contracts for GPUs, displays, cameras, trackers, haptics, fabrication controllers, sensor rigs, and hotplug-safe authority transitions authored directly in Kain.
48. First-class runtime reflection query surfaces for stdlib schema metadata, SPIR-V/kernel dispatch reflection, UI binding manifests, runtime contracts, and generated-output compatibility checks.
49. Canonical lookdev and lighting-review primitives for shot contexts, light rigs, wedge sets, reference matching, perceptual diff scoring, and approval presets exposed directly in Kain UI/native runtime.
50. Advanced simulation authoring primitives for coupled solver graphs, cache branches, checkpoint diffing, field coupling, and multi-domain dependency scheduling without bespoke host orchestration.
51. Manifest-native delivery materializer surfaces that can consume build graphs and distribution channels to emit resumable artifact batches, approval-aware promotions, and cross-channel delivery receipts.

52. Native scene-bundle authoring primitives for reusable editor/runtime scene packages, bundle presets, layer composition, and launch profile binding without sidecar-only orchestration.
53. First-class native widget authoring and reflection contracts for dock layouts, inspectors, overlays, command bars, property sheets, and tool-context routing directly in Kain UI.
54. Canonical interchange contracts for broad import/export/transcode workflows across scene, asset, cache, review, runtime, and archive package families with schema validation and delivery receipts.
55. Declarative automation recipe and command-graph primitives for scheduled, event-driven, operator-gated, and farm-dispatched 3D workflows without bespoke host queue code.
56. Project knowledge and operational-memory runtime surfaces for searchable production context, asset usage, automation history, review evidence, and delivery-aware summaries.
57. First-class sequencing and editorial primitives for shots, takes, clip conform, EDL-style assembly, retime metadata, and timeline-linked review without host-side timeline glue.
58. Native navigation and crowd runtime surfaces for navmesh generation, path queries, avoidance fields, locomotion costs, and large-scene agent orchestration authored directly in Kain.
59. Canonical vehicle dynamics, suspension, drivetrain, contact patch, and control-rig contracts for cars, tracked vehicles, aircraft, watercraft, and machine motion systems.
60. Declarative lighting pipeline primitives for probe grids, shadow atlases, reflection capture, exposure policy, stage continuity, and bake/runtime parity across editor and delivery lanes.
61. Broadcast and live-program control contracts for feed routing, rundown state, tally, overlays, stream outputs, operator handoff, and stage/broadcast synchronization as Kain runtime surfaces.
62. Native evaluation-graph authoring primitives for dependency DAGs, invalidation scopes, scheduler partitions, cache policies, and checkpoint replay without sidecar-only orchestration.
63. First-class viewport runtime surfaces for renderer-owned multi-viewport presentation, camera rigs, overlays, capture paths, and input focus authored directly in Kain UI/native runtime.
64. Canonical interaction contracts for device routing, shortcut layers, gizmos, manipulators, command contexts, and transactional tool state across editor, runtime, XR, and stage surfaces.
65. Declarative persistence primitives for autosave, branching, crash recovery, workspace sessions, audit-visible snapshot deltas, and resumable project-state materialization.
66. Native farm and job-orchestration surfaces for queue definitions, worker capability matching, deadline and approval policy, retries, and execution receipts without bespoke host schedulers.
67. First-class telemetry and observability primitives for frame metrics, budget policies, trace streams, diagnostics capture, and delivery-aware runtime health rollups.
68. Canonical capability-policy and trust-zone contracts for FFI, network, file, plugin, device, and machine operations with audit emission and operator-gated approval flows.
69. First-class entity and archetype primitives for authored scene objects, component schemas, spawn policies, residency hints, and replication-safe mutation authored directly in Kain.
70. Declarative prefab and reusable-assembly contracts with layered variants, patch sets, launch presets, and scene/runtime composition rules without sidecar-only orchestration.
71. Canonical command, macro, and tool-action primitives for undo/redo transactions, command receipts, operator gating, automation handoff, and audit-visible replay across editor and runtime lanes.
72. Native state-schema migration and compatibility surfaces for save/load evolution, project upgrades, dry-run diffs, fallback mounts, and migration receipts authored directly in Kain.
73. First-class workspace layout and multi-window dock-graph primitives with authored window policies, overlay-safe viewport routing, focus control, and per-role persistence in Kain UI/native runtime.
74. Canonical gameplay framework primitives for world rules, state modes, gameplay tasks, event routing, and runtime-safe game-authoring surfaces without slipping into host-engine code.
75. First-class presentation and accessibility runtime primitives for HUDs, menus, diegetic overlays, subtitles, prompts, and in-game/native UI parity under Kain ownership.
76. Declarative replay and deterministic capture/runtime primitives for session recording, checkpoint branching, state scrub, cinematic review, and issue repro bundles.
77. Native localization/runtime culture surfaces for text bundles, subtitle timing, region routing, localized asset variants, and audio-language policy authored directly in Kain.
78. Canonical remote-streaming and thin-client runtime contracts for pixel streaming, operator authority, quality adaptation, cloud-hosted sessions, and editor/runtime handoff without bespoke host services.
79. First-class point-cloud and lidar primitives for ingest, indexing, splat rendering, semantic segmentation, decimation, and scan-to-scene authoring as Kain runtime surfaces.

80. Source-level scene document primitives for declaring scene classes, mounts, launch profiles, and runtime requirements directly in Kain instead of sidecar-only orchestration.
81. First-class widget-primitive source syntax for dock atoms, overlay chrome, modal layers, property sheets, toolbars, and runtime-bound native UI composition.
82. Direct graph-materialization primitives that can consume source catalogs, manifests, kernels, build graphs, and distribution channels into resumable outputs with provenance and promotion policy.
83. Canonical input/action-map/runtime focus contracts for mouse, keyboard, pen, touch, controller, XR, stage devices, and accessibility-aware shortcut layers.
84. Native camera/lens/capture source contracts for cinematic rigs, tracked cameras, viewport routing, replay capture, broadcast handoff, and review-safe delivery bundles.
85. First-class cache, checkpoint, lineage, and invalidation primitives for scene, simulation, render, scan, bundle, and delivery workloads authored directly in Kain.

86. First-class scene-object source syntax for authored objects, class inheritance, relationship binding, and mutation-safe component mounts without sidecar-only scene schemas.
87. Native control-family primitives for inspectors, outliners, trees, tables, timelines, property grids, and command bars with dock-aware state binding in Kain UI.
88. Canonical runtime/import/export schema primitives for versioned contract catalogs, compatibility windows, validation gates, and reflection-safe exchange receipts.
89. First-class physics runtime contracts for rigid bodies, collision layers, joints, character controllers, determinism, and replay-safe solver checkpoints authored directly in Kain.
90. Native shader family and permutation source primitives with reflection-safe binding layouts, variant constraints, and material/runtime compatibility validation.
91. Canonical resource residency and streaming primitives for GPU/CPU/virtual resources, budget governance, eviction policy, telemetry, and delivery-aware memory receipts.
92. First-class mesh/topology primitives for remeshing, retopology, subdivision, decimation, UV policy, attribute transfer, and scene-safe geometry mutation authored directly in Kain.
93. Native bake pipeline contracts for texture, lightmap, probe, curvature, AO, and validation bakes with atlas scheduling, preview/farm parity, and delivery-visible receipts.
94. Canonical KainScript/runtime scripting host surfaces for editor tools, gameplay, automation, modding, and UI commands with capability gates, reflection-bound bindings, and audit visibility.
95. First-class AI planner and agent primitives for utility scoring, goal/task planning, crowd behaviors, tool assistants, and scene-semantic world-state reasoning authored directly in Kain.
96. Declarative modding and user-extension contracts for package schemas, sandbox-safe mounts, trust-zone approvals, dependency resolution, and workspace/public registry promotion without bespoke host logic.
97. First-class material source document primitives for layer stacks, shader-hook declarations, preview compilation, and reflection-safe material authoring without sidecar-only graph glue.
98. Native editor widget suite primitives for pane families, spreadsheet/table surfaces, command palettes, browser stacks, timeline variants, and modal desktop-tool composition under Kain UI ownership.
99. Canonical scene-mutation primitives for transactional deltas, branch-safe replay, operator approvals, replication-aware edits, and audit-visible mutation receipts authored directly in Kain.
100. First-class render-delegation contracts for backend routing, frame-debug capture, review/broadcast presentation handoff, and multi-host render ownership without bespoke engine glue.
101. Native resource-reflection query surfaces for residency catalogs, budget inspection, device/backend compatibility windows, and delivery-visible resource diagnostics authored directly in Kain.
102. Canonical runtime-compatibility primitives for target matrices, launch-readiness gates, fallback routes, feature-pack windows, and promotion-safe validation receipts under Kain ownership.

103. First-class identity, role, entitlement, session, and delegated-authority primitives for operators, teams, service accounts, licenses, and trust-zone-aware tool access directly in Kain UI/native/runtime surfaces.
104. Native cloud session, remote workspace, object storage, cache mirror, burst-compute, and cost-governance contracts for hybrid local/cloud 3D applications without bespoke host services.
105. Canonical marketplace and commerce primitives for asset/plugin catalogs, license windows, entitlements, package promotion, partner/public channels, and receipt-safe installations under Kain ownership.
106. First-class dataops primitives for dataset registries, lineage graphs, corpus shaping, training/evaluation governance, privacy/license policy, and model-ops-ready 3D data products authored directly in Kain.
107. Native fleet orchestration surfaces for render/bake/sim/AI/streaming workers, GPU/device leases, failover routes, capacity planning, and deadline-aware dispatch without bespoke schedulers.

108. Canonical rigging primitives for skeleton hierarchies, control rigs, skin clusters, corrective bindings, retarget maps, and delivery-safe rig receipts authored directly in Kain.
109. First-class deformation-stack surfaces for lattices, wraps, cages, pose-space correctives, morph layers, and checkpoint-safe deformer ordering without bespoke host code.
110. Native painting/runtime primitives for texture, vertex, mask, projection, fill, clone, smart-material, and UDIM-aware layer stacks with color-space-safe canvas binding.
111. Declarative UV authoring surfaces for seams, chart generation, texel density, UDIM assignment, island packing, distortion inspection, and bake/exchange-safe validation receipts.
112. Canonical brush-engine contracts for stylus pressure/tilt/rotation, alpha/tip libraries, stroke modulation, symmetry, lazy-mouse behavior, and tool-context-safe brush presets.
113. First-class color-management primitives for OCIO configs, LUT families, HDR display profiles, view transforms, material/texture color spaces, and paint-render-compositor parity.
114. Native media-runtime contracts for image sequences, video/audio clips, stage plates, decode caches, transcode graphs, proxy/master promotion, and editorial-safe timing routes.
115. Canonical narrative and dialogue primitives for branching story graphs, objective state, quest progression, cinematic triggers, subtitle/audio variants, and replay-safe save/load binding.
116. First-class haptics contracts for stylus pressure feedback, controller/XR force routes, tactile device reflection, machine feedback safety, and tool-context-bound output profiles.
117. Declarative update and patch-delivery surfaces for channel manifests, differential bundles, rollback plans, compatibility gates, migration-aware installs, and resumable promotion receipts.

## Policy

If a future feature needs one of the above, add the requested language/runtime surface instead of burying the requirement in template-local manual code.
