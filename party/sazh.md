# sazh.md

## Current Assignment
Ownership marshal for the implementation wave: lock collision-safe file slices, keep lane boundaries clean, and update the room as ownership shifts.

## Changes Made
- Re-read the repo-level architecture and the UI Slate X100 contract.
- Confirmed the split boundary:
  - `kain-core` owns emitted truth and lowering
  - `kain-ui` owns runtime interpretation, invalidation, and patch authority
  - legacy inference stays compatibility-only
- Reframed the swarm so each lane has a narrow, exact file scope.

## Key Findings
- The repo already has the right semantic surfaces: docking, tabs, signals, computed values, event routes, motion, workspace layout, and realtime bundle export.
- The main risk is not missing capability; it is duplicate ownership and silent drift back into inference.
- The room now needs collision control more than more brainstorming.

## Files Touched
- `M:\Code\Kain\README.md` — reviewed
- `M:\Code\Kain\architecture.md` — reviewed
- `M:\Code\Kain\docs\kainplan\ui_slate_x100\authoring_contract.md` — reviewed
- `M:\Code\Kain\party\sazh.md` — updated

## Next Recommended Move
Use this collision-safe wave split:

1. `Cecil` — compiler-owned UI truth
   - `crates/kain-core/src/ui.rs`
   - `crates/kain-core/src/realtime_app_bundle.rs`
   - Scope: emitted truth only
   - Work: `workspace_layout`, `focus_graph`, `selection_model`, `signal_values`, `computed`, `event_routes`, transaction labels, structure index, and bundle serialization for explicit UI contracts

2. `Cloud` — runtime authority and fallback boundary
   - `crates/kain-ui/src/lib.rs`
   - Scope: runtime execution only
   - Work: `ui_runtime_systems_from_tree(...)`, `UiNativeProjection`, reload/patch application, invalidation routing, and explicit labels on compatibility-only paths

3. `Rikku` — semantic leak cleanup
   - `crates/kain-ui/src/lib.rs`
   - `crates/kain-core/src/ui.rs`
   - Scope: remove accidental host-local meaning
   - Work: props/string leakage, focus/selection/reload/overlay shortcuts, host-local assumptions, and any path that re-infers semantics from widget shape

4. `Vivi` — contract coverage gap list
   - `docs/kainplan/ui_slate_x100/authoring_contract.md`
   - `docs/kainplan/ui_slate_x100/runtime_execution_model.md`
   - `docs/kainplan/ui_slate_x100/current_state_map.md`
   - Scope: enumerate missing or weak contract fields
   - Work: typed events, geometry/containment, anchors, focus traversal, reload/state transfer, widget registry depth, and any verifiability hole still not represented in docs or emitted schema

5. `Balthier` — architecture seam audit
   - `architecture.md`
   - `README.md`
   - Scope: identify the highest-value meaning-inference cuts
   - Work: map where compat inference should stop, where docs are stale, and where the next exact implementation order should be enforced

6. `Barret` — proof matrix
   - `smoketest/UI/*`
   - `smoketest/allinone/*`
   - Scope: runtime proofs only
   - Work: reload, tabs, docking, focus, selection, overlays, event routing, computed invalidation, and a pass/fail matrix tied back to contract fields

7. `Tifa` — mergeability normalization
   - `party/*.md` outputs
   - Scope: synthesis shape only
   - Work: normalize every lane report into `issue / severity / file / owner / dependency / next-action` so Tidus can merge without re-reading prose

8. `Tidus` — global task list merge captain
   - `party/*.md` outputs
   - Scope: canonical task list only
   - Work: dedupe all lane reports, order by dependency, publish the merged execution list

9. `Vincent` — compatibility debt quarantine
   - `crates/kain-ui/src/lib.rs`
   - `crates/kain-core/src/ui.rs`
   - Scope: acceptable bridge vs dangerous doctrine
   - Work: name file-level warnings, replacement targets, and any bridge that must stay explicitly legacy-only

10. `Zidane` — coordination control
    - `party/*.md`
    - Scope: overlap control only
    - Work: watch duplicate effort, redirect collisions, and assign drop/pickup recommendations as the wave changes

11. `Sazh` — ownership marshal
    - `M:\Code\Kain\party\sazh.md`
    - Scope: lane ownership and handoff hygiene
    - Work: keep the split clean, keep the files/modules explicit, and prevent duplicate ownership from creeping back in

12. `Tifa` + `Tidus` merge pass
    - `party/*.md`
    - Scope: final consolidation only
    - Work: compact comparable shapes and emit the dependency-ordered master list

## Notes
- Keep compatibility bridges alive only when explicitly marked legacy-only.
- Do not solve missing semantics through native-only behavior.
- The system is already expressive enough to be dangerous; the real job is making it trustworthy and legible.
