# Kain Fabric DCC Suite Memory

## 2026-03-28 - Report Inventory Is Now A First-Class Shell Telemetry Signal

- Updated `README.md` and `ARCHITECTURE.md` so the app docs now explicitly mention the generated shell's `report_count` telemetry in the top band.
- This keeps the operator-facing contract aligned with `config/ui_shell.json` and the materialized shell: report inventory is visible at a glance, not buried in a browser panel.
- No runtime semantics changed; this is a documentation sync that helps future passes keep the registry, shell, and bridge vocabulary aligned.

Important design decision:

- Report inventory belongs in the shell telemetry strip because it is a live operator signal, not just a deep-dive artifact.

Current risk:

- The shell projection still depends on materialization, so any future telemetry drift should be rechecked against `scripts/materialize-shell.ps1`.

Next recommended step:

- Keep the report browser and report-count telemetry mirrored in both docs and materializer output when new report families land.


This file preserves the durable design intent for `apps/kain-fabric-dcc-suite`.

## 2026-03-28 - Multi-Runtime Lanes Need An Explicit Registry Surface

- The scaffold already has concrete pipeline coverage for `python`, `kain`, `gpu_compute`, `c_abi`, and `rust_crate`, and the app manifest already advertises `tensor.runtime`, `render.runtime`, `compositor.runtime`, `fabric.runtime`, and `automation.runtime` capabilities.
- The next clean seam is a small registry surface that names the runtime-lane ownership matrix explicitly so shell/session tooling can see which lane is owned by Kain, Fabric, GPU, native C, Rust, Python, or external Node bridges without inferring it from docs alone.
- That registry should stay descriptive first; the native host and bridge can consume it later without moving semantic ownership out of Kain.

## 2026-03-28 - Mesh And Topology Reports Are Now Visible In The Live Bridge

- Added a small `reports` block to `session/session_schema.kn` so the live session can carry both the mesh contract report and the topology history report as first-class state.
- Updated `native-app/src/runtime_bridge.rs` so mesh command paths stamp `mesh_contract_report` and topology rebuilds stamp `topology_history_report` alongside the session state, keeping the registry-owned report vocabulary visible to the shell instead of only existing in projection text.
- Updated `scripts/materialize-session-state.ps1` so the bootstrap snapshot mirrors the same report block, keeping generated session state aligned with the live bridge.
- The change is intentionally narrow: Kain still owns the report contracts, while durable mesh normalization and topology solving remain native runtime seams.

Important design decision:

- Report seams should be discoverable in session state, not only in generated receipts. That keeps the shell and the runtime bridge aligned with registry-owned vocabulary.

Current risk:

- The bridge is still a heuristic JSON mutator. Exposing the report ids and paths helps operator visibility, but the longer-term fix is still a typed reducer/driver seam shared by the planner and live bridge.

Next recommended step:

- Thread the expanded report block into the report browser / shell projection path so mesh and topology receipts can be inspected without digging through raw state files.


## 2026-03-28 - Topology History Report Joined The Registry Surface

- Added `topology_history_report` to `config/report_kinds.json` and `session/report_registry.kn` so the topology-lineage projection is now registry-owned instead of only existing as a generated file path.
- Aligned `session/session_schema.kn` so the default topology history state now points at `state/topology_history_report.json`, matching `src/topology_history_projection.kn` and the native bridge constant.
- Updated the architecture notes to call out topology lineage as a first-class report seam alongside the other app-owned registries.

Important design decision:

- Report vocabulary should stay synchronized across config, session registry, schema defaults, and projection writers. When the projection already exists, the registry should name it too.

Current risk:

- The topology history seam is still report-grade, not a true topology database. It records lineage cleanly, but undoable retopology and replacement persistence still need a native solver/runtime seam.

Next recommended step:

- Keep threading the topology-history report through live bridge and shell surfaces so the lineage record becomes visible to operators instead of only to the projection pipeline.

## 2026-03-28 - Rig Sync Contract Became Data-Driven

- Added `config/rig_resource_contract.json` plus new rig resource entries in `config/resource_kinds.json` and `session/resource_registry.kn` so the rig lane now has app-owned control, deformation, and solver-bridge documents.
- Updated `src/rig_sync_bridge.kn` so the emitted `rig_sync_report.json` now carries canonical rig resource URIs alongside the topology dependency and limitation note.
- The rig seam now stays honest about the current runtime split: Kain owns the contract and readiness report, while the native IK/bone runtime remains an external Rust/C++ extension seam.

Important design decision:

- Rig synchronization should be modeled like the rest of the scaffold: registry-owned ids and URIs first, projection report second, native solver adapter later.

Current risk:

- The rig lane still does not have a real solver backend or round-trippable pose persistence. The new contract is only the seam, not the runtime.

Next recommended step:

- Bind `rig_sync_report.json` to a concrete rig reducer or bridge path so control targets and solved transforms can round-trip through the new resource ids instead of remaining report-only.

## 2026-03-28 - Mesh Session Intents Got A Data-Driven Contract Graph

- Added `fabric/intents/mesh_session.fabric.toml` plus `src/mesh_session_projection.kn` so the scaffold now has a Kain-authored mesh session graph instead of only ad-hoc intent references in the planner.
- The new graph carries the mesh command family (`mesh.open_document`, `mesh.set_edit_target`, `mesh.set_authoring_policy`, `mesh.create_primitive`, `mesh.import_asset`, `mesh.edit_topology`, and `mesh.rebuild_topology`) through a durable `mesh_contract_report` projection at `state/mesh_contract_report.json`.
- Extended `config/fabric_intents.json` and `config/report_kinds.json` so the mesh intent/report vocabulary is registry-owned rather than implied only by the session reducer.
- The mesh projection makes the current limitation explicit: Kain owns session routing and contract truth, but durable mesh normalization and topology solving still need a native runtime seam.

Important design decision:

- Mesh session control should be data-driven like the rest of the scaffold. The planner can route mesh commands, but the graph and report registry should own the canonical mesh vocabulary.

Current risk:

- The mesh session report is still orchestration-grade. It documents intent and ownership, but it does not yet normalize real mesh payloads or persist solved topology state.

Next recommended step:

- Bind the mesh session report to the live bridge and resource registry so active edit targets, import receipts, primitive definitions, and topology outputs round-trip through persisted resource ids instead of only through projection text.

## 2026-03-28 - Rig Sync Now Emits A Durable Readiness Report

- Replaced the string-only `src/rig_sync_bridge.kn` stub with a real Kain projection writer that emits `state/rig_sync_report.json`.
- The rig sync seam now records `project`, `workspace_mode`, the upstream `topology_report`, a `rig_profile` for control/deformation readiness, and an explicit limitation that Kain still lacks a native high-performance IK solver and bone evaluation engine.
- The bridge keeps the extension seam honest: the report says where the native Rust/C++ animation runtime should plug in, without pretending the solver already exists.

Important design decision:

- Rig sync should behave like the other durable lane projections: Kain owns the report contract, while the actual solver/runtime stays behind a narrow FFI seam.

Current risk:

- The rig lane is still report-grade, not a true bone runtime. It needs a real control/deformation backend and a contract for round-tripping solved transforms before it can claim production rig ownership.

Next recommended step:

- Bind the rig sync report to a concrete rig resource registry and a native solver adapter so control targets, pose state, and solved transforms can persist through the same app-owned resource ids.

## 2026-03-28 - Mesh Contract IDs Aligned Across Config, Registry, And Bridge

- Aligned `config/mesh_resource_contract.json` with the canonical mesh document ids already used by `session/resource_registry.kn` and `native-app/src/runtime_bridge.rs`.
- The contract ids now match the app-wide seams directly: `imported_mesh_payload_document`, `authored_primitive_definition_document`, `active_editable_mesh_document`, and `topology_output_mesh_document`.
- This removes one layer of naming drift between the authored contract metadata and the live bridge/runtime registry, which makes the mesh seam easier to audit and less likely to fork over time.

Important design decision:

- Canonical mesh ids should be identical across config, session registry, and runtime bridge seams. If those names diverge, the app starts lying to itself about ownership.

Current risk:

- The bridge still mutates JSON session state heuristically. The mesh contract is now aligned, but the system would still benefit from a typed reducer or driver contract if we want the live session to share a single source of truth.

Next recommended step:

- Bind the session planner and live bridge more directly to the mesh resource registry so imported payloads, authored primitives, active edit targets, and topology outputs round-trip through persisted resource ids instead of only through contract-aware bridge state.

## 2026-03-28 - Live Bridge Canonical Mesh Ids Centralized In Code

- Tightened `native-app/src/runtime_bridge.rs` so mesh command handling no longer invents bridge-scoped synthetic mesh ids.
- Centralized the canonical mesh contract ids in Rust constants for `mesh_resource_contract_document`, `active_editable_mesh_document`, `imported_mesh_payload_document`, `authored_primitive_definition_document`, and `topology_output_mesh_document`.
- `mesh.open_document` now resolves the app-owned mesh contract/document seam directly: the active document becomes `mesh_resource_contract_document` and the active edit target becomes `active_editable_mesh_document`.
- `mesh.create_primitive` now binds to `authored_primitive_definition_document` plus the shared active edit target seam instead of minting ad-hoc primitive document ids.
- `mesh.import_asset` now binds to `imported_mesh_payload_document` and records the imported payload against the mesh contract instead of creating bridge-specific import document ids.
- `mesh.rebuild_topology` now writes through `topology_output_mesh_document` as the active document for the rebuilt topology result.

Important design decision:

- The live bridge should project canonical mesh contract ids, not session-local placeholder ids. Centralizing the ids in one place makes the bridge easier to audit and lowers the chance of future drift.

Current risk:

- The bridge still mutates JSON session state heuristically. It is better aligned now, but it still needs a typed reducer or driver contract if we want the mesh ids, resource receipts, and host UI to share one stronger source of truth.

Next recommended step:

- Bind the session planner and live bridge more directly to the mesh resource registry so imported payloads, authored primitives, active edit targets, and topology outputs round-trip through persisted resource ids instead of only through contract-aware bridge state.

## 2026-03-28 - Live Bridge Mesh URIs Aligned With Canonical Resource IDs

- Extended `native-app/src/runtime_bridge.rs` to project canonical mesh resource URIs alongside the ids, so the live bridge now writes `mesh://contract/current`, `mesh://editing/active`, `mesh://imports/current/payloads`, `mesh://primitives/authored/definitions`, and `mesh://topology/output/current` instead of bridge-local placeholder URIs.
- `mesh.open_document`, `mesh.set_edit_target`, `mesh.create_primitive`, `mesh.import_asset`, and `mesh.rebuild_topology` now keep both the resource id and the resource URI aligned in the live session document.

Important design decision:

- The bridge should not only agree on mesh ids; it should also keep the URI vocabulary canonical so the native shell, resource registry, and future runtime consumers can resolve the same app-owned contract without translation layers.

Current risk:

- The bridge is still a heuristic JSON mutator. The URI alignment reduces vocabulary drift, but the stronger fix is still a typed reducer/driver seam shared by the planner and live bridge.

Next recommended step:

- Bind the mesh planner and live bridge to a registry-backed mesh contract adapter so resource ids, URIs, and payload receipts round-trip through one shared schema.

## 2026-03-28 - Mesh Contract URIs Are Now Bootstrapped From Registry Data

- Updated `scripts/python_suite_bootstrap.py` to load `config/mesh_resource_contract.json` and emit `mesh_contract_resource_uris` as structured bootstrap data instead of keeping the URI vocabulary only in `src/main.kn`.
- Updated `src/main.kn` so `build_mesh_resource_contract()` now reads the canonical mesh URI registry from `fabric_inputs.python_suite_bootstrap.mesh_contract_resource_uris`.
- This removes one more hardcoded copy of the mesh URI vocabulary from the seed path and pushes the scaffold slightly closer to the data-driven registry pattern Kain wants.

Important design decision:

- Canonical mesh URIs should flow from the registry file through bootstrap data into the Kain seed, not be retyped in multiple authored seams.

Current risk:

- The bootstrap still materializes the registry by filename lookup and the seed still assembles a compact string contract; a stronger typed mesh contract adapter would be cleaner if/when Kain grows richer config loading.

Next recommended step:

- Thread the same bootstrap-emitted mesh contract payload into the mesh session and live bridge projections so the registry-backed URIs, ids, and receipts all round-trip through one shared contract object.

## 2026-03-27 (Later) - First Mesh Import And Primitive Projection Writers Landed

- Added `src/mesh_import_projection.kn` and `src/primitive_mesh_authoring.kn` so the mesh contract now has real Kain-authored projection writers instead of only registry placeholders.
- Wired `KAIN.fabric.toml` so the mesh import lane runs before primitive authoring, and primitive authoring runs before the material projection chain. That preserves the intended Kain-first ownership order: imported payloads, then authored primitives, then lookdev.
- The new writers emit app-rooted JSON receipts under `apps/kain-fabric-dcc-suite/state/` for imported payloads and authored primitives, making the contract visible to downstream shell and runtime consumers.

Important design decision:

- Mesh resources should stay app-owned and data-driven. The writer modules are only durable receipts and contract projections; they are not the mesh engine itself.

Current risk:

- The mesh import and primitive receipts are still orchestration-grade. They describe contract state but do not yet normalize real payloads or persist actual mesh serialization.

Next recommended step:

- Bind these projection writers to a first-class mesh session/materialization path so imported assets, authored primitives, and topology outputs can round-trip through persistent ids instead of synthetic contract text.

## 2026-03-28 - Topology History Got A First-Class Mesh Resource Contract

- Extended `config/mesh_resource_contract.json`, `config/resource_kinds.json`, and `session/resource_registry.kn` with `topology_history_mesh_document` at `mesh://topology/history/current`.
- The new topology history resource gives authored topology decisions and replacement lineage a durable app-owned home instead of leaving that information implicit in planner or bridge state.
- This keeps the mesh lane aligned with the broader Kain pattern: important DCC semantics belong in registries and session-owned contracts, not in transient runtime heuristics.

Important design decision:

- Topology history should be modeled as a first-class resource alongside the active edit target and topology output. That makes the contract easier to reason about when sculpt, import, and rebuild passes start competing for the same mesh lineage.

Current risk:

- The contract now exists in the registries, but there is still no dedicated writer yet. The history resource is visible to the scaffold, but the live bridge and planner still need an explicit projection seam to populate it.

Next recommended step:

- Add a small topology-history projection writer and thread it through the mesh rebuild flow so the history resource receives durable lineage records whenever topology is regenerated.

## 2026-03-28 - Topology History Projection Writer Landed

- Added `src/topology_history_projection.kn` so the scaffold now writes a durable `state/topology_history_report.json` lineage receipt instead of only carrying the history resource in registries.
- Wired `KAIN.fabric.toml` to run the new topology-history projection after `rig_graph_analysis` and to keep the tensor training lane downstream of that lineage receipt.
- The new report records the active edit target, topology output, upstream topology report, and an explicit limitation that Kain still needs a native solver seam for undoable retopology and replacement-graph persistence.

Important design decision:

- Topology history should stay as an app-owned Kain projection for now, because the scaffold can already retain lineage and expose it to downstream consumers without pretending it owns the solver.

Current risk:

- The history report is durable, but it is still a report-grade seam. A real native topology engine will still need to own the edit graph, persistence, and undo semantics behind a typed runtime contract.

Next recommended step:

- Bind the topology-history projection to the live bridge and mesh resource registry so topology rebuilds can round-trip lineage through persisted resource ids instead of only emitting receipt text.

## 2026-03-28 - Report Browser Now Names Topology Lineage Explicitly

- Tightened `config/surfaces.json` so the `report_browser` surface now names "topology lineage" in the summary, latest-report label, and filter list instead of only saying generic topology.
- Aligned `config/ui_shell.json` operator notes so the report browser explicitly promises mesh, topology lineage, rig, render, tensor, automation, and publish receipts.
- This is a small but useful shell polish pass: the app already had the topology-history seam, but now the browser copy makes that lineage visible and discoverable without changing runtime semantics.

Important design decision:

- If a report seam exists, the shell should say what it is in plain operator language instead of hiding behind generic labels.

Current risk:

- The browser copy is clearer now, but the real next step is still a richer report browser reader that can sort and surface the lineage receipts directly from state.

Next recommended step:

- Thread the history block into the live shell projections and any future topology inspector so the UI can read a canonical lineage record instead of inferring it from dirty state.

## 2026-03-28 - Topology History Report Now Uses The Mesh Contract Seams

- Tightened `src/topology_history_projection.kn` so the report now references the mesh resource contract document plus the canonical active edit target and topology output resources instead of the generic scene bootstrap document.
- Aligned the report payload with the app-owned mesh contract vocabulary (`topology_history_mesh_document`, `mesh://editing/active`, `mesh://topology/output/current`) so topology lineage reads like the rest of the scaffold instead of a scene-generic placeholder.
- The current limitation is still honest: Kain can retain topology lineage as a durable report, but true undoable retopology and replacement-graph persistence still need a native solver/runtime seam.

Important design decision:

- Topology history should cite the same registry-owned mesh ids as the rest of the lane. That keeps the report projection auditable and reduces semantic drift between the history seam and the live mesh contract.

Current risk:

- The report is still orchestration-grade rather than a full topology database. It documents lineage, but it does not yet round-trip solved mesh state through a native reducer or driver contract.

Next recommended step:

- Bind the topology-history projection to the live bridge and mesh resource registry so topology rebuilds can round-trip lineage through persisted resource ids instead of only emitting receipt text.

## 2026-03-28 - Registry Bridge Extension Seam Clarified

- Refined the architecture notes to make the next seam explicit: a typed reducer/driver bridge shared by `session/resource_registry.kn`, `session/report_registry.kn`, and `native-app/src/runtime_bridge.rs`.
- This keeps the current mesh/topology lineage work honest: the scaffold can already project canonical ids, URIs, and lineage receipts, but the bridge still mutates JSON heuristically instead of flowing through one registry-backed contract.
- The clean follow-up is to move canonical mesh/session truth into a shared reducer or driver layer rather than adding more one-off bridge logic.

## 2026-03-28 - Mesh Contract Report Joined The Registry Surface

- Added `mesh_contract_report` to `session/report_registry.kn` so the mesh session projection is now registry-owned instead of only existing as a generated file path and fabric-intent output.
- This keeps the mesh contract aligned across `config/fabric_intents.json`, `config/report_kinds.json`, `src/mesh_session_projection.kn`, and the session report registry.
- The mesh session seam is still orchestration-grade: it records routing and limitation boundaries, but durable mesh normalization and topology solving still need a native runtime seam.

Next recommended step:

- Bind the mesh session projection to the live bridge and report consumers so the registry-owned report vocabulary surfaces in the shell instead of remaining projection-only.

## 2026-03-28 - Report Browser Materializer Now Gives Mesh And Topology Lineage A Dedicated Variant

- Updated `scripts/materialize-shell.ps1` so the generated report browser uses its own `report_browser` variant instead of the generic overlay card treatment.
- The generated shell now also emits a small callout that keeps mesh and topology lineage visibly first in the browser, matching the authored surface intent from `config/surfaces.json` and `config/ui_shell.json`.
- This keeps the shell projection aligned with the app docs without inventing new runtime semantics.

Important design decision:

- Mesh and topology lineage should stay obvious in the shell frame because they are identity-rich reports, not incidental debug output.

Current risk:

- The live bridge is still heuristic, so shell visibility helps operators today but does not replace the typed reducer/driver seam that should eventually own session truth.

Next recommended step:

- Keep threading report-browser semantics through the shell materializer and runtime snapshot so future surface additions do not drift away from the authored contract.

## 2026-03-28 - Report Browser Now Surfaces Mesh And Topology Lineage

- Updated `config/surfaces.json` and `config/ui_shell.json` so the report browser now explicitly surfaces mesh and topology lineage alongside render, tensor, automation, and publish lanes.
- Refined `README.md` and `ARCHITECTURE.md` so the app docs call out the report browser as part of the authored shell contract instead of leaving it implicit.
- The change is intentionally narrow: it improves operator visibility without inventing new runtime semantics.

Important design decision:

- Mesh and topology lineage should stay obvious in the shell frame because they are identity-rich reports, not incidental debug output.

Current risk:

- The live bridge is still heuristic, so shell visibility helps operators today but does not replace the typed reducer/driver seam that should eventually own session truth.

Next recommended step:

- Thread the report browser surface through the generated shell materializer so the visibility change is guaranteed to stay in sync with future shell rebuilds.

## 2026-03-28 - Report Browser Materializer Got A Dedicated Variant

- Updated `scripts/materialize-shell.ps1` so the generated shell now renders `report_browser` with its own `report_browser` variant instead of the generic overlay card treatment.
- Added a small report-browser callout in the generated surface card to keep mesh and topology lineage visually foregrounded during shell rebuilds.
- This keeps the authored shell contract and the materializer in sync without changing runtime semantics.

Important design decision:

- The materializer should preserve the same lineage priority as the config docs: mesh and topology stay first-class in the report browser rather than looking like an incidental inspector.

Current risk:

- The bridge is still heuristic and the shell is still projection-driven. Styling clarity helps, but the typed reducer/driver seam remains the real long-term fix.

Next recommended step:

- Keep threading report-browser semantics through the materializer and generated shell so future surface additions do not drift away from the authored contract.

## 2026-03-28 - Docs Companion Added For The Scaffold

- Added `docs/ARCHITECTURE.md` as a lightweight companion architecture note so the `docs/` area reflects the same high-signal scaffold shape as the app root.
- The doc mirrors the app's durable boundaries: registries, session truth, Fabric intent graphs, Kain projection writers, and the narrow file-backed native bridge.
- It also records the same honest extension seams for mesh normalization, topology history, rig solving, tensor, sim, compositor, and sculpt runtime work.
- This was a safe, high-leverage cleanup step: it improves future agent navigation without changing app semantics.

## 2026-03-28 - Report Browser Now Surfaces Mesh And Topology Lineage

- Updated `config/surfaces.json` so the `report_browser` inspector now explicitly calls out mesh and topology reports alongside render, tensor, automation, and publish lanes.
- Updated `config/ui_shell.json` operator notes so the shell guidance tells future materializers to keep mesh and topology lineage obvious in the report browser instead of burying it in generic report language.

Important design decision:

- The report browser should expose the scaffold's most identity-rich lineage first. Mesh and topology are now part of the surface vocabulary, not just hidden lane receipts.

Current risk:

- This is still a shell-level semantic improvement. The deeper typed reducer/driver seam is still the real fix for keeping live bridge and registry truth aligned.

Next recommended step:

- Thread the report browser surface into the generated shell materializer so the runtime chrome reflects the updated mesh/topology focus automatically.

## 2026-03-28 - Shell Telemetry Now Surfaces Report Count

- Updated `config/ui_shell.json` so the authored shell status items now include a `report_count` metric alongside the other top-bar telemetry.
- Updated `scripts/materialize-shell.ps1` so the generated top bar now renders five status pills instead of four, letting report inventory show up without hiding any existing registry telemetry.
- This is a safe shell-only visibility pass: it does not change runtime semantics, but it makes the report registry feel more like a first-class workspace signal.

Important design decision:

- Report inventory belongs in the same operator telemetry band as commands, pipelines, jobs, and seams; it should be visible at a glance rather than buried in a registry panel.

Current risk:

- The runtime snapshot already carries enough data for this metric, but the shell still depends on projection materialization. If the materializer drifts, the new telemetry will lag until the next rebuild.

Next recommended step:

- Consider threading the same `report_count` signal into any future host-side status surfaces so the live bridge and generated shell stay aligned.

## 2026-03-28 - Report Browser Filters Expanded For Rig And Tensor Lanes

- Expanded `config/surfaces.json` so the `report_browser` filters now include rig and tensor alongside mesh, topology, render, and publish.
- This is a small but high-leverage registry fix: the browser now reflects the suite's multi-runtime intent graph instead of only the early hot-path lanes.
- No runtime semantics changed; this stayed safely in shell projection data.

Important design decision:

- Report browsing should keep pace with the scaffold's lane vocabulary so operators do not have to guess which runtime seams exist.

Current risk:

- The richer filters still depend on report writers and live bridge projections landing the corresponding receipts.

Next recommended step:

- Mirror the lane coverage into any future generated shell summaries if they start lagging behind the registry.

## 2026-03-28 - Runtime Lane Ownership Registry Is Now Explicit

- Added `config/runtime_lanes.json` and threaded it through both `KAIN.toml` and `config/app_manifest.json` so the scaffold now names the Kain / Fabric / Python / GPU / native C / Rust / Node bridge ownership matrix directly instead of leaving it implicit in prose.
- This is the cleanest high-leverage seam right now: shell and session tooling can read the runtime ownership map from registry data, while Kain still owns the semantic model.
- No runtime semantics changed; this is a registry-and-docs pass that makes the multi-runtime scaffold easier to inspect and less likely to drift.

Important design decision:

- Runtime-lane ownership should be data, not guesswork. The registry is descriptive first and can be consumed by the shell or bridge later without moving ownership out of Kain.

Current risk:

- The new registry is static until a materializer or live surface consumes it, so future work should mirror it into the shell projection when the lane matrix becomes operator-facing.

Next recommended step:

- Thread `config/runtime_lanes.json` into the shell materializer or session registries once the app wants the lane matrix visible in runtime chrome.

## 2026-03-28 - Typed Reducer/Driver Seam Remains The Clean Follow-Up

- The current bridge is still a heuristic JSON mutator, even though the mesh and topology report vocabulary is now registry-owned and visible in the shell.
- The clean extension seam is a typed reducer/driver bridge shared by `session/resource_registry.kn`, `session/report_registry.kn`, and `native-app/src/runtime_bridge.rs`.
- That seam would let canonical ids, URIs, and lineage receipts round-trip through one contract instead of being patched together across several files.

Important design decision:

- Keep session truth in the app registries and reducers, then make the live bridge a narrow driver over that shared contract.

Current risk:

- Until that seam lands, the bridge can still drift from registry truth if future lane vocabulary grows faster than the heuristic mutator.

Next recommended step:

- Bind the planner and live bridge to one shared registry-backed reducer/driver contract before adding more one-off bridge logic.
