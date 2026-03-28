# Kain Fabric DCC Suite Memory

This file preserves the durable design intent for `apps/kain-fabric-dcc-suite`.

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
