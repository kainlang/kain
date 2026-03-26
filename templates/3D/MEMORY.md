# 3D Template Memory

## 2026-03-25 - Workspace Preset Materializer Consumer Path

This run closed the main durability gap left by the prior preset-export work: the template had a named workspace-preset materialization lane, but the reusable graph-materialization pack still did not declare the concrete downstream consumer path.

What changed:

- upgraded `KAIN.toml` to `2.3.2`
- expanded `graph_materialization_runtime.kn` with explicit workspace-preset materialization input, output, and consumer descriptors so the reusable pack now declares the preset manifest/build-graph/channel inputs plus the launch, receipt, reflection, distribution, and bundle-receipt roots it feeds
- wired the graph-materialization contract directly to the existing reusable consumer ids: `workspace_preset_export_descriptor`, `runtime_bundle_launch_profile`, `workspace_preset_materialization_receipts`, `workspace_preset_delivery_batch`, `workspace_preset_delivery_registry`, and `runtime_bundle_registry`
- refreshed the universal workbench shell so the graph-materialization, runtime-bundle, and delivery surfaces describe the preset-export consumer path explicitly instead of only gesturing at generic receipts
- updated `README.md` and `ARCHITECTURE.md` so future runs keep preset-materializer bridges explicit and reusable rather than hiding them in loosely named metadata

Important decisions:

- keep the work template-local because the missing piece was contract depth inside the reusable runtime packs, not an upstream `M:\\Code\\Kain` blocker
- deepen the existing workspace-preset export lane instead of adding another runtime app, graph, or host-side materializer
- treat graph materialization as the authoritative place to declare preset export inputs, output roots, and downstream bundle/delivery consumers so future generators can extend one shared contract

Current data snapshot:

- engine systems: `118`
- GPU kernels: `114`
- tensor pipelines: `109`
- runtime apps: `31`
- workspace presets: `109`
- UI surfaces: `116`
- registered sources: `232`
- build graphs: `87`
- distribution channels: `87`

Current gaps / next recommended step:

- the contract path is now explicit, but there is still no concrete generated payload schema or manifest emitter that writes launch packets/receipt contents into those roots end to end
- the next useful pass is to deepen one reusable emitter, likely runtime bundles or delivery materialization, so it produces a concrete preset launch artifact/receipt shape from `workspace_presets.json`
- no heavy validation was run this turn by design

## 2026-03-25 - Workspace Preset Export Lane And Receipt Registry

This run closed the next gap called out by prior memory: workspace presets were manifest-backed and reflection-visible, but there was still no dedicated export/materialization lane that downstream tools could consume directly.

What changed:

- upgraded `KAIN.toml` to `2.3.1` and registered `generated/workspace-presets` as a dedicated runtime output root
- expanded `workspace_preset_runtime.kn` with explicit export and receipt descriptors so preset materialization is owned by the reusable preset pack instead of implied by bundle/runtime logic
- expanded `reflection_runtime.kn` with a dedicated workspace-preset receipt catalog at `generated/runtime-reflection/workspace-preset-receipts`
- expanded `delivery_runtime.kn` with a workspace-preset delivery graph and batch descriptor for receipt-aware preset exports
- expanded `runtime_bundle_runtime.kn` and `graph_materialization_runtime.kn` so launch exports and preset provenance are modeled explicitly inside the reusable bundle/materialization packs
- added `workspace_preset_materialization_graph` to `build_graphs.json` and `workspace_preset_delivery_registry` to `distribution_channels.json`
- aligned the runtime-bundle, runtime-reflection, delivery-materializer, and graph-materialization build graphs so they all consume `workspace_presets.json` where their contracts already depended on it
- updated README and architecture guidance so future runs treat preset exports as a first-class downstream contract rather than generic reflection spillover

Important decisions:

- keep preset export logic template-local and manifest-driven; this does not require upstream `M:\\Code\\Kain` changes
- avoid creating another runtime app for preset export because the behavior difference is materialization/packaging, not a new executable surface
- treat preset receipts as a reusable downstream contract for launch bundles, role-aware workspace materializers, and future generators

Current data snapshot:

- engine systems: `118`
- GPU kernels: `114`
- tensor pipelines: `109`
- runtime apps: `31`
- workspace presets: `109`
- UI surfaces: `116`
- registered sources: `232`
- build graphs: `87`
- distribution channels: `87`

Current gaps / next recommended step:

- the template now declares where preset exports and receipts belong, but there is still no concrete generator/materializer implementation that emits the receipt payloads end to end
- the next useful pass is to deepen one downstream consumer, likely runtime bundles or delivery materialization, so it materializes actual preset launch receipts from `workspace_presets.json`
- no heavy validation was run this turn by design

## 2026-03-25 - Manifest-Backed Workspace Preset Routing

This run deepened the runtime side of the runtime-app versus workspace-preset split. The template already had `workspace_presets.json`, but the stdlib and workbench still carried stale UI-local preset assumptions and the docs still reported preset counts like app counts.

What changed:

- added `workspace_preset_runtime.kn` as the reusable manifest-backed preset-registry contract for launch policy, layout routing, and curated workbench shortcuts
- updated `editor_runtime.kn`, `runtime_bundle_runtime.kn`, and `workspace_layout_runtime.kn` so preset selection points directly at `manifests/workspace_presets.json`
- replaced the exhaustive workbench-local preset table in `ui_workbench.kn` with a small shortcut/favorites layer that explicitly defers the full catalog to the manifest
- added `workspace_presets.json` as an input to the preset-sensitive build graphs so downstream materializers can consume lane-selection data directly
- corrected README, architecture, and generated-output guidance so runtime app counts stay canonical and workspace presets are tracked separately

Important decisions:

- keep workspace preset selection manifest-driven and runtime-pack-owned instead of burying another growing preset registry inside the workbench UI shell
- allow the workbench to expose a curated shortcut layer, but treat `workspace_presets.json` as the exhaustive downstream contract
- keep this entirely template-local; no upstream `M:\\Code\\Kain` change was required

Current data snapshot:

- engine systems: `118`
- GPU kernels: `114`
- tensor pipelines: `109`
- runtime apps: `31`
- workspace presets: `109`
- UI surfaces: `116`
- registered sources: `232`
- build graphs: `86`
- distribution channels: `86`

Current gaps / next recommended step:

- add a lightweight generated registry/export path once a downstream materializer exists that can emit workspace-preset receipts or launch bundles from `workspace_presets.json`
- no heavy validation was run this turn by design

## 2026-03-25 - Capture Review And Bundle Introspection Spine Deepened

This run deepened an existing reusable slice of the template instead of widening the lane catalog again. The focus was the capture-to-review-to-delivery spine shared by virtual production, lookdev, runtime reflection, and runtime bundle materialization.

What changed:

- upgraded `KAIN.toml` to `2.3.0`
- expanded [`virtual_production_runtime.kn`](/mnt/m/Templates/3D/src-kain/stdlib/three_d_runtime/virtual_production_runtime.kn) with tracked-lens calibration and playback-sync descriptors plus a dedicated stage calibration export root
- expanded [`lookdev_runtime.kn`](/mnt/m/Templates/3D/src-kain/stdlib/three_d_runtime/lookdev_runtime.kn) with wedge-set scoring, perceptual-diff-aware approval routing, and a generated wedge output root
- expanded [`reflection_runtime.kn`](/mnt/m/Templates/3D/src-kain/stdlib/three_d_runtime/reflection_runtime.kn) so runtime reflection now models workspace-preset catalogs, build-graph catalogs, distribution-receipt catalogs, and bundle/compatibility query surfaces instead of only schema and GPU metadata
- expanded [`runtime_bundle_runtime.kn`](/mnt/m/Templates/3D/src-kain/stdlib/three_d_runtime/runtime_bundle_runtime.kn) with explicit workspace-preset routing, bundle receipt exports, and runtime/delivery compatibility gates
- updated the universal workbench shell, README, and architecture guidance so stage calibration, runtime bundle receipts, and lookdev approval surfaces are visible as first-class template concepts instead of hidden inside pack internals

Important decisions:

- keep this work template-local because the missing piece was reusable pack depth and downstream structure, not an upstream Kain compiler/runtime blocker
- prefer enriching a smaller set of reusable packs that future generators/materializers can consume over adding more runtime apps or duplicate presets
- treat workspace presets, build graphs, distribution receipts, and bundle compatibility as reflection-visible template assets so downstream tools can stay manifest-driven

Current gaps / next recommended step:

- the new descriptors are ready for future generators/materializers, but no generator currently consumes the deeper stage-calibration, wedge, or bundle-receipt contracts end to end
- the next useful pass is to make one manifest-driven materializer/export lane consume these descriptors directly, likely through runtime reflection or delivery graph generation
- heavy validation was not run this turn by design

## 2026-03-25 - Canonical Runtime Targets And Workspace Preset Split

This run corrected a structural drift in the template: runtime apps had expanded into a lane catalog even though most entries pointed at the same `universal_3d_workbench` source and did not represent distinct packaging.

What changed:

- upgraded `KAIN.toml` to `2.2.0` and registered a new `workspace_presets.json` manifest
- added a dedicated workspace-preset catalog so lane selection is manifest-driven instead of encoded as duplicate runtime apps
- reduced `runtime_apps.json` down to canonical runtime targets keyed by real `runtime_kind` and host differences
- updated runtime bundle and project deployment contracts so launch/profile policy points at the workspace preset manifest
- updated README and architecture guidance to keep future expansion in presets/runtime packs rather than multiplying app registrations

Important decisions:

- treat `runtime_apps.json` as a packaging/runtime target manifest, not a substitute for workspace presets or lane docs
- keep the broad 3D lane surface available through manifest-driven presets so downstream users still get the full template breadth without app catalog duplication
- avoid any upstream `M:\\Code\\Kain` changes because this cleanup was template-local and not blocked on compiler/runtime work

Current data snapshot:

- canonical runtime apps: `31`
- workspace presets: `109`

Current gaps / next recommended step:

- wire future generators/materializers to consume `workspace_presets.json` directly wherever launch-profile selection needs to be explicit
- heavy validation was not run this turn by design

## 2026-03-25 - Color Media Narrative Haptics And Updates Manifest Sync

This run corrected a real template drift bug: the color, media, narrative, haptics, and updates packs already existed in `src-kain`, but they were not fully registered in the manifest-driven layer. The workbench text and docs had started treating those lanes as present even though the downstream registries did not.

What changed:

- upgraded `KAIN.toml` to `2.1.1` to mark the manifest-sync repair
- registered 5 missing engine systems, 5 GPU kernels, 5 tensor pipelines, 5 runtime apps, 5 UI surfaces, 5 build graphs, 5 distribution channels, and 10 source entries for the existing color/media/narrative/haptics/updates packs
- extended [`ui_workbench.kn`](/mnt/m/Templates/3D/src-kain/stdlib/three_d_runtime/ui_workbench.kn) with explicit surfaces and workspace presets for the five lanes so the structured workbench defaults match the textual shell
- updated [`README.md`](/mnt/m/Templates/3D/README.md) so the published counts and latest-expansion notes match the actual manifest surface

Important decisions:

- treat this as template-local drift, not an upstream Kain limitation, because the missing pieces were manifest registration and workbench metadata rather than language/runtime capability
- prefer repairing the existing reusable packs over adding more shallow lanes; this run deepened manifest integrity instead of widening template scope
- keep the template downstream-friendly by expressing color/media/narrative/haptics/updates through stdlib packs, manifests, kernels, and workbench surfaces rather than host-local code

Current data snapshot:

- engine systems: `118`
- GPU kernels: `114`
- tensor pipelines: `109`
- runtime apps: `109`
- UI surfaces: `116`
- registered sources: `232`
- build graphs: `86`
- distribution channels: `86`

Current gaps / next recommended step:

- audit older high-count README sections for stale examples and duplicated snapshots; the runtime surface is now correct, but some long-form docs still carry historical baggage
- no heavy validation was run this turn by design

## 2026-03-25 - Color Media Narrative Haptics And Updates Expansion

This run deepened the universal 3D template around systems that full DCC suites, game engines, streamed desktop tools, and ZBrush-class native applications still need beyond the already broad platform shell.

What changed:

- upgraded `KAIN.toml` to `2.1.0` and added generated output roots for color, media, narrative, haptics, and updates artifacts
- added 5 new stdlib runtime packs: color, media, narrative, haptics, and updates
- added 5 new SPIR-V kernel seeds for color-pipeline resolution, media plate streaming, narrative-state graph resolution, haptic-feedback routing, and update-channel manifest resolution
- expanded runtime registration with new engine systems, tensor pipelines, runtime apps, UI surfaces, build graphs, distribution channels, and source registrations for the five new lanes
- upgraded the universal workbench, README, generated-output guide, architecture notes, and limitations tracking so these runtime/application lanes are first-class instead of implied

Important decisions:

- keep color, media, narrative, haptics, and updates expressed as reusable Kain stdlib/manifests/kernels instead of drifting into host-specific tool code or service glue
- treat color-management parity, media-routing, branching narrative state, tactile feedback, and patch/update rollout as universal substrate for full 3D products rather than optional extras
- continue recording missing upstream/runtime surfaces in `limitations.md` instead of hiding them in template-local workaround code

Current data snapshot:

- engine systems: `113`
- GPU kernels: `109`
- tensor pipelines: `104`
- runtime apps: `104`
- UI surfaces: `111`
- registered sources: `222`
- build graphs: `81`
- distribution channels: `81`

Current gaps / next recommended step:

- deepen next around first-class source syntax and runtime reflection for color documents, media timelines, narrative graphs, haptic profiles, and update-channel manifests once upstream Kain exposes them
- heavy validation was not run this turn by design

## 2026-03-25 - Rigging Deformation Painting UV And Brush Expansion

This run deepened the universal 3D template around missing authoring substrate that real DCC suites, game editors, and ZBrush-class applications still need beyond the already-broad platform shell.

What changed:

- upgraded `KAIN.toml` to `2.0.0` and added generated output roots for rigging, deformation, painting, UV, and brush artifacts
- added 5 new stdlib runtime packs: rigging, deformation, painting, UV, and brush
- added 5 new SPIR-V kernel seeds for control-rig solving, deformer-stack evaluation, paint-layer blending, UV chart packing, and brush-dab accumulation
- expanded runtime registration with new engine systems, tensor pipelines, runtime apps, UI surfaces, build graphs, distribution channels, and source registrations for the five new lanes
- upgraded the universal workbench, generated-output guide, README, architecture notes, and limitations tracking so these DCC-authoring lanes are first-class instead of implied

Important decisions:

- keep rigging, deformation, painting, UV, and brush capability expressed as reusable Kain stdlib/manifests/kernels instead of drifting into host-local editor subsystems
- treat brush engines, paint canvases, UV layout, and deformer stacks as universal substrate for DCC suites, game engines, and sculpt-heavy native tools
- continue recording missing upstream/runtime surfaces in `limitations.md` instead of hiding them in template-local workaround code

Current gaps / next recommended step:

- deepen next around first-class source syntax for scene, rig, brush, paint, and UV documents plus richer runtime reflection/query surfaces for these newly added lanes
- heavy validation was not run this turn by design

## 2026-03-25 - Identity Cloud Marketplace DataOps And Fleet Expansion

This run deepened the universal 3D template around platform-scale operational systems needed for serious engine, DCC, and ZBrush-class products rather than only authoring/runtime lanes.

What changed:

- upgraded `KAIN.toml` to `1.9.0` and added generated output roots for identity, cloud, marketplace, dataops, and fleet artifacts
- added 5 new stdlib runtime packs: identity, cloud, marketplace, dataops, and fleet
- added 5 new SPIR-V kernel seeds for entitlement resolution, cloud session bursting, marketplace catalog routing, dataset lineage shaping, and fleet capacity dispatch
- expanded runtime registration with new engine systems, tensor pipelines, runtime apps, UI surfaces, build graphs, distribution channels, and source registrations for the five new lanes
- upgraded the universal workbench, README, generated-output guide, architecture notes, and limitations tracking so these platform-ops lanes are first-class instead of implied

Important decisions:

- keep identity, cloud, marketplace, dataops, and fleet expressed as reusable Kain stdlib/manifests/kernels instead of bespoke host dashboards or service glue
- treat operator authority, remote sessions, package licensing, dataset lineage, and worker-fleet dispatch as universal 3D platform substrate rather than optional enterprise sidecars
- continue recording missing upstream/runtime surfaces in `limitations.md` instead of hiding them in template-local host code

Current data snapshot:

- engine systems: `108`
- GPU kernels: `104`
- tensor pipelines: `99`
- runtime apps: `99`
- UI surfaces: `106`
- registered sources: `212`
- build graphs: `76`
- distribution channels: `76`

Current gaps / next recommended step:

- deepen next around source-level declarative scene/material/widget syntax, richer runtime reflection/query surfaces, and upstream-native authoring/runtime primitives for these new operational lanes
- heavy validation was not run this turn by design


## 2026-03-25 - Mesh Baking Scripting AI And Modding Expansion

This run deepened the universal 3D template around classic engine-grade systems that were still underrepresented as first-class Kain-owned lanes.

What changed:

- upgraded `KAIN.toml` to `1.8.0` and added generated output roots for mesh, baking, scripting, AI, and modding artifacts
- added 5 new stdlib runtime packs: mesh, baking, scripting, AI, and modding
- added 5 new SPIR-V kernel seeds for mesh topology clustering, bake atlas scheduling, script host scheduling, AI agent planning, and mod-package mounting
- expanded runtime registration with new engine systems, tensor pipelines, runtime apps, UI surfaces, build graphs, distribution channels, and source registrations for the five new lanes
- upgraded the universal workbench, README, generated-output guide, architecture notes, and limitations tracking so these lanes are first-class instead of implied

Important decisions:

- keep mesh processing, baking, scripting, AI, and modding expressed as reusable stdlib/manifests/kernels rather than app-local engine code
- treat script execution, agent planning, and mod-package mounting as Kain-owned runtime surfaces with capability gates and approval-aware delivery, not loose host glue
- continue recording missing upstream/runtime surfaces in `limitations.md` rather than papering over gaps with template-local workaround code

Current gaps / next recommended step:

- deepen next around richer authored source syntax for scene, material, and widget systems plus stronger runtime reflection and compatibility surfaces
- heavy validation was not run this turn by design


## 2026-03-25 - Material Source And Compatibility Expansion

This run deepened the universal 3D template around source-authoring, editor desktop semantics, mutation receipts, render routing, and compatibility governance instead of only widening domain breadth.

What changed:

- upgraded `KAIN.toml` to `1.8.0` and added generated output roots for material source, editor widgets, scene mutation, render delegation, resource reflection, and runtime compatibility
- added 6 new stdlib runtime packs for material-source authoring, editor widget suites, scene mutation, render delegation, resource reflection, and runtime compatibility workflows
- added 6 new SPIR-V kernel seeds for material-source documents, editor-widget suites, scene-mutation deltas, render-delegate packets, resource-reflection catalogs, and runtime-compatibility resolution
- expanded runtime registration to cover 98 engine systems, 94 GPU kernels, 89 tensor pipelines, 89 runtime apps, 96 UI surfaces, 192 registered sources, 66 build graphs, and 66 distribution channels
- upgraded the universal workbench shell, README, generated-output docs, architecture, and `limitations.md` so these substrate systems are first-class instead of implied

Important decisions:

- keep material-source authoring, editor widget suites, scene mutation, render delegation, resource reflection, and runtime compatibility in Kain stdlib/manifests/kernels instead of drifting into bespoke host engine code
- treat source-level material docs, pane-family widget suites, mutation receipts, render handoff packets, resource introspection catalogs, and launch-readiness matrices as universal substrate for engines, DCC suites, and ZBrush-class tools
- continue keeping Rust out of downstream authoring requirements while recording missing language/runtime surfaces in `limitations.md`

Current data snapshot:

- engine systems: `98`
- GPU kernels: `94`
- tensor pipelines: `89`
- runtime apps: `89`
- UI surfaces: `96`
- registered sources: `192`
- build graphs: `66`
- distribution channels: `66`

Current gaps / next recommended step:

- deepen next into stronger source-level scene and material semantics, richer editor command/widget behavior primitives, and first-class compatibility/reflection query syntax once upstream Kain exposes them
- no heavy validation was run this turn by design

# 3D Template Memory

## 2026-03-24 - Initial Universal Template Foundation

This folder started as a greenfield Kain-first 3D runtime template.

What was established:

- a new manifest-driven workspace under `Templates/3D`
- a native Kain UI workbench entrypoint for a full editor shell
- reusable stdlib contracts for project profiles, engine systems, tensor pipelines, scene/runtime data, UI surface defaults, and FFI bridge policy
- authored compute kernel seeds for renderer visibility, viewport compositing, sculpting, and cloth simulation
- explicit limitations tracking for language/runtime gaps instead of hiding those needs in manual engine code

Design decisions:

- keep the template free of Rust host project requirements for downstream users
- treat Kain UI + native-ui materialization as the default shell lane
- treat SPIR-V/tensor kernels as first-class registered assets, not incidental side files
- keep registration data in JSON manifests so future tools can add or remove lanes without rewriting source structure

Risks and next step:

- this is a strong authoring foundation, but the template still needs future generated artifact passes and real project-specific kernel expansion as features solidify
- when the language/runtime adds missing surfaces from `limitations.md`, wire them into manifests before adding manual workaround code

## 2026-03-24 - Universal Runtime Expansion

This run turned the template from a thin foundation into a broader universal 3D runtime platform.

What changed:

- expanded the template workspace metadata in `KAIN.toml` to declare template, manifest, generated-output, and runtime roots
- grew the stdlib from 6 core files to a broader pack covering render, assets, materials, animation, simulation, world streaming, editor transactions, and richer project/deployment policy
- widened the system registry to 14 engine/editor/runtime lanes spanning foundation, render, scene, compute, assets, materials, animation, simulation, world, editor, UI, and interop
- widened the GPU catalog to 12 kernel seeds covering render, sculpt, materials, animation, simulation, and world streaming
- widened tensor orchestration to 7 manifest-driven pipelines instead of only the original renderer/sculpt/cloth set
- widened runtime app registration to 6 output lanes including a native workbench, asset pipeline studio, headless orchestration, game runtime shell, render lab, and simulation console
- widened UI registration to 12 surfaces including asset, graph, curve, simulation, world, and automation views
- expanded `limitations.md` with more explicit upstream language/runtime asks for scene queries, packaging, collaboration, streaming callbacks, and graph reflection

Current data snapshot:

- engine systems: `14`
- GPU kernels: `12`
- tensor pipelines: `7`
- runtime apps: `6`
- UI surfaces: `12`
- registered sources: `26`

Important decisions:

- keep the template manifest-driven instead of creating app-specific one-off source layouts
- keep all new 3D system areas expressed as reusable stdlib/runtime contracts rather than burying logic in a single app file
- use kernel seeds only where SPIR-V/tensor authoring is the correct Kain-owned lane; otherwise push capability into stdlib contracts
- continue treating FFI as optional and contract-driven, never the primary ownership layer

Next recommended step:

- add generation/orchestration docs or scripts that can walk `gpu_kernels.json` and `runtime_apps.json` to materialize artifacts in batch
- add more domain packs next where the template still feels intentionally generic: procedural modeling, grooming, volumetrics, photogrammetry, audio, collaboration, and plugin packaging
- when validation is desired later, run a focused `kain` pass against the expanded `.kn` surface instead of broad repo testing

## 2026-03-24 - Universal Domain Pack Expansion

This run pushed the template from a broad 3D runtime into a more serious universal platform skeleton for engines, DCC suites, scan tools, and ZBrush-class desktop apps.

What changed:

- upgraded `KAIN.toml` to `0.3.0` and added plugin package output roots
- expanded project/build policy to include photogrammetry, audio, collaboration, and plugin packaging toggles
- expanded the stdlib with 7 new runtime packs: procedural, grooming, volumetric, photogrammetry, audio, collaboration, and plugin packaging
- expanded the main engine runtime registry to cover 21 registered system lanes
- added 6 new authored SPIR-V kernel seeds for procedural scatter, strand interpolation, volumetric integration, scan fusion, audio-reactive analysis, and collaboration sync
- expanded tensor orchestration to 13 registered pipelines
- expanded runtime app registration to 12 app lanes, including sculpt, procedural world, volumetric FX, scan ingest, collaboration review, and plugin package builders
- expanded UI registration to 19 surfaces spanning procedural, groom, volume, scan, audio, collaboration, and packaging views
- expanded the workbench shell copy/layout to expose the new domains directly instead of leaving them implicit
- expanded `limitations.md` with upstream asks for photogrammetry, volumetric authoring, richer plugin ABI support, and broader tensor resource classes

Current data snapshot:

- engine systems: `21`
- GPU kernels: `18`
- tensor pipelines: `13`
- runtime apps: `12`
- UI surfaces: `19`
- registered sources: `39`

Important decisions:

- keep extending the template through reusable stdlib packs and manifests rather than inventing app-local systems
- treat audio, collaboration, scan reconstruction, volumetrics, grooming, and packaging as first-class 3D platform concerns instead of follow-up extras
- keep the single workbench shell as the universal authoring surface while exposing specialized lanes through manifests and presets
- continue using FFI only as a contract-driven escape hatch instead of the ownership layer

Current gaps / next recommended step:

- add manifest-driven generation/orchestration surfaces so runtime apps, packages, and SPIR-V artifacts can be materialized from one command graph
- add higher-end packs next where the platform still has conceptual room: constraints/solvers, terrain biomes, destruction authoring, review annotation schemas, and package publishing channels
- when validation is desired later, run a focused `kain` pass over the template rather than broad test suites; this run intentionally did not run heavy validation

## 2026-03-24 - Orchestration And Publishing Expansion

This run pushed the template from a broad universal 3D platform into a more materializable application platform.

What changed:

- upgraded `KAIN.toml` to `0.4.0` and registered build-graph and distribution manifests alongside richer generated output roots
- added 6 new stdlib runtime packs: orchestration, constraints, biome authoring, destruction authoring, review annotations, and publishing
- added 5 new SPIR-V kernel seeds for artifact graph materialization, multi-domain constraint solving, biome/ecosystem distribution, clustered fracture, and review overlay rasterization
- added `build_graphs.json` and `distribution_channels.json` so artifact generation and delivery stay manifest-driven instead of app-local
- expanded runtime registration to 27 engine systems, 23 GPU kernels, 18 tensor pipelines, 18 runtime apps, 25 UI surfaces, and 50 registered sources
- expanded the universal workbench shell, workspace presets, editor policy, and docs to expose orchestration, solver, biome, destruction, review, and publishing lanes directly
- expanded `limitations.md` with upstream asks for native constraint graphs, destruction authoring, biome resource types, review delivery, and distribution registries

Important decisions:

- treat artifact generation, review delivery, and release publishing as first-class Kain platform lanes instead of external tooling glue
- keep new 3D capability expressed as reusable stdlib/runtime contracts, manifest data, and kernel seeds rather than hand-written host code
- keep Rust out of the downstream authoring requirement while still leaving FFI available as a contract-driven escape hatch
- keep future automation growth additive by using manifest-driven registries and stable lane ids instead of one-off app forks

Current data snapshot:

- engine systems: `27`
- GPU kernels: `23`
- tensor pipelines: `18`
- runtime apps: `18`
- UI surfaces: `25`
- registered sources: `50`
- build graphs: `3`
- distribution channels: `3`

Current gaps / next recommended step:

- add more platform-depth packs next where the template still has room: renderer graph authoring, node-based behavior systems, robotics/toolpath lanes, CAD/NURBS surfaces, and live asset synchronization
- when validation is desired later, run a focused `kain` pass over the expanded `.kn` and manifest surface; this run intentionally avoided heavy validation per direction

## 2026-03-24 - Platform Depth Expansion

This run pushed the template deeper into universal application-platform territory instead of only widening the outer shell.

What changed:

- upgraded `KAIN.toml` to `0.5.0` and added generated output roots for sync mirrors, CAD exchange, and robotics/toolpath exports
- added 5 new stdlib runtime packs: renderer graph, behavior graphs, CAD/NURBS, robotics/toolpaths, and live sync
- added 5 new SPIR-V kernel seeds for renderer graph scheduling, behavior state evaluation, NURBS tessellation, toolpath kinematics, and live asset delta merge
- expanded runtime registration to cover 32 engine systems, 28 GPU kernels, 23 tensor pipelines, 23 runtime apps, 30 UI surfaces, and 60 registered sources
- added 2 new build graphs and 2 new distribution channels so live sync and machine/cad delivery stay manifest-driven
- upgraded the universal workbench shell, project profile toggles, generated-output docs, and limitations tracking to expose the new lanes directly

Important decisions:

- keep exact modeling, robotics, behavior, renderer graph, and live-sync capability in stdlib contracts plus manifest data instead of drifting into hand-written host logic
- treat CAD exchange, machine toolpaths, and live mirrors as first-class generated outputs under Kain ownership
- continue recording missing native/runtime surfaces in `limitations.md` rather than papering over them with manual 3D engine code

Current data snapshot:

- engine systems: `32`
- GPU kernels: `28`
- tensor pipelines: `23`
- runtime apps: `23`
- UI surfaces: `30`
- registered sources: `60`
- build graphs: `5`
- distribution channels: `5`

Current gaps / next recommended step:

- expand next into domain-adjacent platform packs where the template can still go deeper: compositor/video editing, USD scene exchange policy, large review databases, metrology/inspection, and fabrication scheduling
- when validation is requested later, run a focused `kain` pass over the new `.kn` and manifest surfaces; this run intentionally did not run heavy validation

## 2026-03-24 - Exchange Inspection And Finishing Expansion

This run extended the universal 3D template into production-adjacent delivery, review, and manufacturing lanes instead of only authoring/runtime lanes.

What changed:

- upgraded `KAIN.toml` to `0.6.0` and added generated roots for compositor, scene exchange, inspection, fabrication, and review database outputs
- added 5 new stdlib runtime packs: compositor, scene exchange, metrology, fabrication scheduling, and review database
- added 5 new SPIR-V kernel seeds for frame composition, scene exchange stage resolve, deviation analysis, fabrication scheduling, and review evidence indexing
- expanded runtime registration to cover 37 engine systems, 33 GPU kernels, 28 tensor pipelines, 28 runtime apps, 35 UI surfaces, and 70 registered sources
- added 2 new build graphs and 2 new distribution channels so exchange delivery and review database materialization stay manifest-driven
- upgraded the universal workbench, project feature toggles, generated-output docs, and limitations tracking to expose finishing, exchange, inspection, fabrication, and persistent review records directly

Important decisions:

- keep editorial/compositor, exchange, inspection, fabrication, and review database capability in Kain stdlib contracts plus manifests instead of drifting into bespoke host tooling
- treat scene exchange and persistent review records as first-class generated outputs under Kain ownership
- continue recording missing upstream/runtime surfaces in `limitations.md` rather than hiding gaps behind manual code

Current data snapshot:

- engine systems: `37`
- GPU kernels: `33`
- tensor pipelines: `28`
- runtime apps: `28`
- UI surfaces: `35`
- registered sources: `70`
- build graphs: `7`
- distribution channels: `7`

Current gaps / next recommended step:

- deepen the platform next around scene semantics, native 3D query/selection contracts, richer packaging/runtime bundling, and domain-specific generated output materializers that consume the new manifests directly
- when validation is requested later, run a focused `kain` pass over the expanded `.kn` and manifest surfaces; this run intentionally avoided heavy validation

## 2026-03-24 - Scene Semantics And Materialization Expansion

This run deepened the universal 3D template around scene semantics, query resolution, runtime bundling, materialization receipts, and asset lineage instead of widening into unrelated domains.

What changed:

- upgraded `KAIN.toml` to `0.7.0` and added generated output roots for runtime bundles, materialization receipts, and asset registry exports
- added 5 new stdlib runtime packs: scene semantics, query/selection, runtime bundles, materialization, and asset registry
- added 5 new SPIR-V kernel seeds for semantic indexing, selection queries, bundle assembly, receipt indexing, and asset lineage resolution
- expanded runtime registration to cover 42 engine systems, 38 GPU kernels, 33 tensor pipelines, 33 runtime apps, 40 UI surfaces, and 80 registered sources
- added 3 new build graphs and 3 new distribution channels so bundles, materialization receipts, and asset registries stay manifest-driven
- upgraded the workbench shell, workspace presets, project feature toggles, generated-output docs, and limitations tracking to expose the new lanes directly

Important decisions:

- keep scene semantics, query/picking, bundling, materialization, and provenance inside Kain stdlib contracts plus manifests rather than drifting into manual host code
- treat bundles, receipts, and asset registries as first-class generated outputs under Kain ownership
- continue recording missing upstream/runtime surfaces in `limitations.md` rather than hiding them behind bespoke pipeline glue

Current data snapshot:

- engine systems: `42`
- GPU kernels: `38`
- tensor pipelines: `33`
- runtime apps: `33`
- UI surfaces: `40`
- registered sources: `80`
- build graphs: `10`
- distribution channels: `10`

Current gaps / next recommended step:

- deepen next into domain-specific authoring packs such as lighting/lookdev review semantics, virtual production/stage control, advanced simulation authoring, and richer scene/bundle reflection surfaces
- when validation is requested later, run a focused `kain` pass over the expanded `.kn` and manifest surfaces; this run intentionally avoided heavy validation

## 2026-03-24 - Character Stage Geospatial XR And Networking Expansion

This run pushed the template into engine/runtime-critical domains that were still missing as first-class systems.

What changed:

- upgraded `KAIN.toml` to `0.8.0` and added generated roots for character, virtual production, geospatial, XR, and networking outputs
- added 5 new stdlib runtime packs: character, virtual production, geospatial, XR, and networking
- added 5 new SPIR-V kernel seeds for facial solving, LED volume sync, geospatial tile resolution, XR stereo frame resolve, and runtime network replication
- expanded runtime registration to cover 47 engine systems, 43 GPU kernels, 38 tensor pipelines, 38 runtime apps, 45 UI surfaces, and 90 registered sources
- added 5 new build graphs and 5 new distribution channels so the new domains stay manifest-driven instead of app-local
- repaired `tensor_runtime.kn` so the stdlib catalog now covers the full manifest surface instead of stopping short of newer lanes
- upgraded the workbench shell, generated-output docs, and limitations tracking to expose the new domains directly

Important decisions:

- keep character, stage, geospatial, XR, and networking capability expressed as Kain stdlib contracts plus manifests instead of manual host/runtime code
- treat virtual production, digital twins, immersive shells, and replicated runtime/editor sessions as first-class universal-template lanes
- keep downstream authoring free of Rust requirements while continuing to record missing language/runtime surfaces in `limitations.md`

Current data snapshot:

- engine systems: `47`
- GPU kernels: `43`
- tensor pipelines: `38`
- runtime apps: `38`
- UI surfaces: `45`
- registered sources: `90`
- build graphs: `15`
- distribution channels: `15`

Current gaps / next recommended step:

- deepen next around native device/runtime reflection, advanced simulation authoring, lighting/lookdev review semantics, and richer generated materializers that consume the expanded delivery graphs directly
- when validation is requested later, run a focused `kain` pass over the expanded `.kn` and manifest surfaces; this run intentionally avoided heavy validation

## 2026-03-24 - Device Reflection Lookdev Solver And Delivery Expansion

This run deepened the template around runtime-operational surfaces that a full engine or DCC suite needs once the broad domain map already exists.

What changed:

- upgraded `KAIN.toml` to `0.9.0` and added generated roots for device control, runtime reflection, lookdev, simulation authoring, and delivery materialization
- added 5 new stdlib runtime packs: device runtime, reflection runtime, lookdev runtime, simulation authoring runtime, and delivery runtime
- added 5 new SPIR-V kernel seeds for device synchronization, reflection indexing, lookdev review, solver-graph resolution, and delivery batch materialization
- expanded runtime registration to cover 52 engine systems, 48 GPU kernels, 43 tensor pipelines, 43 runtime apps, 50 UI surfaces, and 100 registered sources
- added 5 new build graphs and 5 new distribution channels so device, reflection, lookdev, solver-authoring, and delivery outputs stay manifest-driven
- upgraded the universal workbench, generated-output docs, README, and limitations tracking to expose the new lanes directly

Important decisions:

- keep runtime-operational depth in Kain stdlib contracts, manifests, and tensor/SPIR-V seeds rather than inventing host-local systems
- treat device control, reflection metadata, lookdev approval, advanced solver authoring, and delivery materializers as first-class 3D platform domains for engines and DCC suites
- continue keeping Rust out of downstream authoring requirements while recording any missing runtime/language surfaces in `limitations.md`

Current data snapshot:

- engine systems: `52`
- GPU kernels: `48`
- tensor pipelines: `43`
- runtime apps: `43`
- UI surfaces: `50`
- registered sources: `100`
- build graphs: `20`
- distribution channels: `20`

Current gaps / next recommended step:

- deepen next into authorable scene/runtime bundle semantics, richer native widget contracts, and broader import/export surfaces once upstream runtime support exists
- when validation is requested later, run a focused `kain` pass over the expanded `.kn` and manifest surfaces; this run intentionally avoided heavy validation

## 2026-03-24 - Scene Bundles Widgets Interchange Automation And Knowledge Expansion

This run deepened the universal 3D template around authorable scene packages, native widget tooling, broad interchange, production automation recipes, and searchable knowledge/ops context.

What changed:

- upgraded `KAIN.toml` to `1.0.0` and added generated roots for scene bundles, native widgets, interchange, automation, and knowledge outputs
- added 5 new stdlib runtime packs: scene bundle runtime, widget runtime, interchange runtime, automation runtime, and knowledge runtime
- added 5 new SPIR-V kernel seeds for scene bundle composition, widget layout resolve, interchange transcode, automation recipe scheduling, and knowledge graph indexing
- expanded runtime registration to cover 57 engine systems, 53 GPU kernels, 48 tensor pipelines, 48 runtime apps, 55 UI surfaces, and 110 registered sources
- added 5 new build graphs and 5 new distribution channels so bundles, widget schemas, interchange artifacts, automation receipts, and knowledge catalogs stay manifest-driven
- upgraded the universal workbench, generated-output docs, README, limitations tracking, and project profile so the new lanes are first-class

Important decisions:

- keep scene bundles, native desktop widget systems, interchange, automation recipes, and project knowledge in Kain stdlib contracts plus manifests instead of manual host glue
- treat widget schemas, automation receipts, exchange bundles, and knowledge indexes as generated artifacts under Kain ownership
- continue keeping Rust out of downstream authoring requirements while recording upstream/runtime gaps explicitly in `limitations.md`

Current data snapshot:

- engine systems: `57`
- GPU kernels: `53`
- tensor pipelines: `48`
- runtime apps: `48`
- UI surfaces: `55`
- registered sources: `110`
- build graphs: `25`
- distribution channels: `25`

Current gaps / next recommended step:

- deepen next into stronger source-level scene bundle syntax, richer native UI primitives, format-schema reflection, and automation/knowledge runtime primitives as upstream Kain capabilities
- when validation is requested later, run a focused `kain` pass over the expanded `.kn` and manifest surfaces; this run intentionally avoided heavy validation

## 2026-03-24 - Sequencing Navigation Vehicle Lighting And Broadcast Expansion

This run extended the universal 3D template into cinematic sequencing, runtime navigation, vehicle dynamics, lighting pipeline control, and live broadcast operations.

What changed:

- upgraded `KAIN.toml` to `1.1.0` and added generated roots for sequencing, navigation, vehicle, lighting, and broadcast outputs
- added 5 new stdlib runtime packs: sequencing runtime, navigation runtime, vehicle runtime, lighting runtime, and broadcast runtime
- added 5 new SPIR-V kernel seeds for sequence timeline resolve, navmesh corridor solve, vehicle dynamics integration, lighting probe resolve, and broadcast program mixing
- expanded runtime registration to cover 62 engine systems, 58 GPU kernels, 53 tensor pipelines, 53 runtime apps, 60 UI surfaces, and 120 registered sources
- added 5 new build graphs and 5 new distribution channels so these runtime/editor lanes stay manifest-driven
- upgraded the workbench shell, generated-output docs, README, limitations tracking, project profile, engine system bundles, and tensor catalog so the new lanes are first-class

Important decisions:

- keep sequencing, navigation, vehicle, lighting, and broadcast capability in Kain stdlib/manifests/kernels instead of drifting into bespoke host logic
- treat editorial timing, crowd/pathing, vehicle handling, lighting continuity, and live program control as core universal-platform concerns for engines, DCC suites, and stage tools
- continue keeping Rust out of downstream authoring requirements while recording runtime/language gaps explicitly in `limitations.md`

Current data snapshot:

- engine systems: `62`
- GPU kernels: `58`
- tensor pipelines: `53`
- runtime apps: `53`
- UI surfaces: `60`
- registered sources: `120`
- build graphs: `30`
- distribution channels: `30`

Current gaps / next recommended step:

- deepen next into authored gameplay/runtime input semantics, save/load/state migration, and richer scene-entity mutation surfaces once upstream Kain exposes them
- when validation is requested later, run a focused `kain` pass over the expanded `.kn` and manifest surfaces; this run intentionally avoided heavy validation

Run time: `2026-03-24T15:49:08-04:00`

## 2026-03-24 - Operational Runtime Systems Expansion

This run deepened the universal 3D template around engine-grade operational systems that serious DCC suites and runtime engines need, instead of only adding more domain shells.

What changed:

- upgraded `KAIN.toml` to `1.2.0` and added generated roots for evaluation, viewport, interaction, persistence, jobs, telemetry, and security outputs
- added 7 new stdlib runtime packs: evaluation, viewport, interaction, persistence, jobs, telemetry, and security
- added 7 new SPIR-V kernel seeds for evaluation scheduling, viewport frame packets, gizmo/input resolution, persistence snapshots, job dispatch, telemetry profiling, and capability-policy resolution
- expanded runtime registration to cover 69 engine systems, 65 GPU kernels, 60 tensor pipelines, 60 runtime apps, 67 UI surfaces, and 134 registered sources
- added 7 new build graphs and 7 new distribution channels so the new operational lanes stay manifest-driven instead of turning into host glue
- upgraded the workbench shell, README, generated-output docs, project profile, tensor catalog, and limitations tracking so the operational lanes are first-class and not implied

Important decisions:

- keep evaluation, viewport, interaction, persistence, jobs, telemetry, and security in Kain stdlib/manifests/kernels instead of hand-written engine code
- treat farm orchestration, observability, and capability policy as universal 3D platform requirements for engines, DCC suites, and ZBrush-class tools
- continue keeping Rust out of downstream authoring requirements while recording missing runtime/language surfaces in `limitations.md`

Current data snapshot:

- engine systems: `69`
- GPU kernels: `65`
- tensor pipelines: `60`
- runtime apps: `60`
- UI surfaces: `67`
- registered sources: `134`
- build graphs: `37`
- distribution channels: `37`

Current gaps / next recommended step:

- deepen next into first-class scene-authoring syntax, richer native UI widgets, and runtime materializers that can directly consume the larger operational graph surface
- no heavy validation was run this turn by design

## 2026-03-24 - Authoring Substrate Expansion

This run pushed the universal 3D template deeper into the actual authoring substrate that a game engine, DCC suite, or ZBrush-class tool needs beneath the higher-level domain packs.

What changed:

- upgraded `KAIN.toml` to `1.3.0` and added generated output roots for entity, prefab, command, state-migration, and workspace-layout artifacts
- added 5 new stdlib runtime packs: entity, prefab, command, state migration, and workspace layout
- added 5 new SPIR-V kernel seeds for entity deltas, prefab composition, command replay, schema migration, and workspace dock-flow resolution
- expanded runtime registration to cover 74 engine systems, 70 GPU kernels, 65 tensor pipelines, 65 runtime apps, 72 UI surfaces, 144 registered sources, 42 build graphs, and 42 distribution channels
- upgraded the workbench shell, README, generated-output docs, project profile, limitations tracking, and manifest catalogs so these authoring-substrate systems are first-class instead of implied

Important decisions:

- keep entity mutation, prefab composition, command transactions, state upgrades, and workspace layout in Kain stdlib/manifests/kernels instead of drifting into host-local engine glue
- treat reusable archetypes, prefab variants, command receipts, migration plans, and workspace layouts as generated artifacts under Kain ownership
- continue keeping Rust out of downstream authoring requirements while recording missing runtime/language surfaces in `limitations.md`

Current data snapshot:

- engine systems: `74`
- GPU kernels: `70`
- tensor pipelines: `65`
- runtime apps: `65`
- UI surfaces: `72`
- registered sources: `144`
- build graphs: `42`
- distribution channels: `42`

Current gaps / next recommended step:

- deepen next into authored scene-source syntax, richer native widget primitives, and graph materializers that consume the wider operational and authoring substrate directly
- no heavy validation was run this turn by design

## 2026-03-24 - Gameplay Presentation Replay Streaming And Point-Cloud Expansion

This run extended the universal 3D template into engine-facing gameplay/presentation systems plus operational replay, localization, remote-streaming, and lidar/point-cloud lanes.

What changed:

- upgraded `KAIN.toml` to `1.4.0` and added generated output roots for gameplay, presentation, replay, localization, streaming, and point-cloud artifacts
- added 6 new stdlib runtime packs: gameplay, presentation, replay, localization, streaming, and point-cloud
- added 6 new SPIR-V kernel seeds for gameplay-state resolve, presentation-shell routing, replay capture indexing, localization bundle resolution, remote-stream session orchestration, and point-cloud stream/splat resolution
- expanded runtime registration to cover 80 engine systems, 76 GPU kernels, 71 tensor pipelines, 71 runtime apps, 78 UI surfaces, 156 registered sources, 48 build graphs, and 48 distribution channels
- upgraded the workbench shell, README, generated-output docs, project profile, tensor catalog, and limitations tracking so these lanes are first-class instead of side-band concerns

Important decisions:

- keep gameplay framework, presentation shell, replay, localization, remote streaming, and point-cloud capability in Kain stdlib/manifests/kernels instead of bespoke host glue
- treat in-game/native presentation parity, deterministic repro capture, cloud/thin-client control, and lidar/point-cloud authoring as universal 3D platform concerns instead of genre-specific extras
- continue keeping Rust out of downstream authoring requirements while recording missing language/runtime surfaces explicitly in `limitations.md`

Current data snapshot:

- engine systems: `80`
- GPU kernels: `76`
- tensor pipelines: `71`
- runtime apps: `71`
- UI surfaces: `78`
- registered sources: `156`
- build graphs: `48`
- distribution channels: `48`

Current gaps / next recommended step:

- deepen next into authored scene-source syntax, richer native widget primitives, direct graph materializers, and broader engine-facing runtime gameplay source semantics once upstream Kain surfaces exist
- no heavy validation was run this turn by design

## 2026-03-25 - Source Widget Materialization Input Camera And Cache Expansion

This run deepened the universal 3D template around shared engine-grade substrate that every serious engine, DCC suite, and ZBrush-class application ends up needing beneath feature domains.

What changed:

- upgraded `KAIN.toml` to `1.5.0`, added an [`ARCHITECTURE.md`](/mnt/m/Templates/3D/ARCHITECTURE.md), and registered generated output roots for scene-source, widget-primitives, graph-materialization, input, camera, and cache artifacts
- added 6 new stdlib runtime packs: scene source, widget primitives, graph materialization, input, camera, and cache
- added 6 new SPIR-V kernel seeds for scene-source documents, widget primitive composition, direct graph materialization, input action mapping, camera-rig packets, and cache-lineage streams
- expanded manifest registration to cover 86 engine systems, 82 GPU kernels, 77 tensor pipelines, 77 runtime apps, 84 UI surfaces, 168 registered sources, 54 build graphs, and 54 distribution channels
- upgraded the universal workbench shell, README, generated-output docs, project profile, tensor catalog, and `limitations.md` so these substrate lanes are first-class instead of implied

Important decisions:

- keep scene-source semantics, widget primitives, graph materialization, input maps, camera rigs, and cache lineage in Kain stdlib/manifests/kernels instead of pushing them into host-local 3D engine code
- treat these lanes as reusable platform substrate for all downstream 3D apps, not app-specific features
- continue keeping Rust out of downstream authoring requirements while recording missing source/runtime primitives in `limitations.md`

Current data snapshot:

- engine systems: `86`
- GPU kernels: `82`
- tensor pipelines: `77`
- runtime apps: `77`
- UI surfaces: `84`
- registered sources: `168`
- build graphs: `54`
- distribution channels: `54`

Current gaps / next recommended step:

- deepen next into richer authored scene-object syntax, broader native UI widget families, and more explicit runtime/import/export schema primitives once upstream Kain surfaces exist
- no heavy validation was run this turn by design

## 2026-03-25T03:51:35-04:00

Expanded `/mnt/m/Templates/3D` into deeper engine-substrate lanes for scene objects, native controls, runtime schemas, physics, shader permutations, and resource residency.

Delivered this run:
- upgraded `KAIN.toml` to `1.6.0` and added generated roots for scene objects, native controls, schemas, physics, shaders, and resources
- added 6 new stdlib runtime packs for scene-object authoring, native control families, runtime/import-export schemas, physics, shader systems, and resource governance
- added 6 new SPIR-V kernel seeds for scene-object graphs, native control state, schema contract indexing, rigid-body islands, shader permutation scheduling, and resource residency resolution
- expanded manifest registration to 92 engine systems, 88 GPU kernels, 83 tensor pipelines, 83 runtime apps, 90 UI surfaces, 180 registered sources, 60 build graphs, and 60 distribution channels
- upgraded the universal workbench shell, README, generated-output docs, architecture, and `limitations.md` so these engine-grade substrate lanes are first-class instead of implied

Important decisions:
- keep scene objects, native controls, schemas, physics, shader systems, and resource residency in Kain stdlib/manifests/kernels instead of drifting into bespoke host engine code
- treat these as universal substrate required by serious engines, DCC suites, game runtimes, and ZBrush-class tools rather than optional vertical features
- continue keeping Rust out of downstream authoring requirements while recording missing language/runtime surfaces in `limitations.md`

Current data snapshot:

- engine systems: `92`
- GPU kernels: `88`
- tensor pipelines: `83`
- runtime apps: `83`
- UI surfaces: `90`
- registered sources: `180`
- build graphs: `60`
- distribution channels: `60`

Current gaps / next recommended step:

- deepen next into stronger source-level material/shader authoring syntax, broader native UI/editor widgets, richer runtime scene mutation semantics, and explicit resource/runtime reflection primitives once upstream Kain exposes them
- no heavy validation was run this turn by design
