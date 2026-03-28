# sazh.md

## Current Assignment
Ownership marshal for the current UI implementation wave: keep the lane split clean, keep ownership explicit, and convert live outputs into collision-safe file slices.

## Changes Made
- Re-read the repo-level architecture and the UI Slate X100 contract.
- Confirmed the split boundary:
  - `kain-core` owns emitted truth and lowering
  - `kain-ui` owns runtime interpretation, invalidation, and patch authority
  - legacy inference stays compatibility-only
- Reframed the swarm so each lane has a narrow, exact file scope.
- Party Bus room `party:mission:kain-ui-overhaul` now has a decision locked for wave v1.
- I opened a file claim on `M:\Code\Kain\party\sazh.md` so the marshal log stays coherent while the rest of the room edits.
- Read the canonical board at `M:\Code\Kain\party\TASKS.md` and aligned the marshal role to it.

## Key Findings
- The repo already has the right semantic surfaces: docking, tabs, signals, computed values, event routes, motion, workspace layout, and realtime bundle export.
- The main risk is not missing capability; it is duplicate ownership and silent drift back into inference.
- The room now needs collision control more than more brainstorming.

## Files Touched
- `M:\Code\Kain\README.md` — reviewed
- `M:\Code\Kain\architecture.md` — reviewed
- `M:\Code\Kain\docs\kainplan\ui_slate_x100\authoring_contract.md` — reviewed
- `M:\Code\Kain\party\TASKS.md` — reviewed
- `M:\Code\Kain\party\sazh.md` — updated

## Next Recommended Move
Current execution order locked in the room:

1. `Cecil`
   - `M:\Code\Kain\crates\kain-core\src\ui.rs`
   - `M:\Code\Kain\crates\kain-core\src\realtime_app_bundle.rs`
   - emitted truth only

2. `Cloud`
   - `M:\Code\Kain\crates\kain-ui\src\lib.rs`
   - runtime fallback call-site audit only

3. `Rikku`
   - `M:\Code\Kain\crates\kain-ui\src\lib.rs`
   - `M:\Code\Kain\crates\kain-core\src\ui.rs`
   - semantic leak cleanup only

4. `Vincent`
   - `M:\Code\Kain\party\vincent.md`
   - compatibility-debt inventory only

5. `Vivi`
   - top 5 contract gaps only
   - owner / file / acceptance signal attached

6. `Barret`
   - minimum non-test validation spine only
   - later surfaces: reload, tabs, docking, focus, selection, overlays, event routing, computed invalidation

7. `Tifa`
   - normalize live outputs into merge artifact now

8. `Tidus`
   - publish implementation wave v1 immediately after Tifa + Sazh land

9. `Balthier`
   - execution-order enforcement and seam prioritization only

10. `Zidane`
   - overlap control only

11. `Sazh`
   - ownership marshal
   - convert Tifa’s artifact into exact collision-safe file slices
   - keep updates current in `M:\Code\Kain\party\sazh.md`

12. `Tifa` + `Tidus`
   - final consolidation / master list

## Role for the parallel work
- I am the ownership marshal.
- My job is to keep file slices collision-safe, track who owns what, and turn merged outputs into exact next cuts.
- I do not widen scope; I keep the wave from trampling itself.

## Current Role Map
- Cecil — truth emission owner
  - kain-core semantics, contracts, bundle emission
  - focuses on the source of truth, not downstream presentation
- Rikku — semantic leak hunter
  - tracks props/strings/host-local assumptions leaking meaning
  - burns down the weird glue seams in kain-ui
- Vincent — compatibility-debt quarantiner
  - inventory only
  - marks bridge surfaces as keep / danger / replace
- Vivi — missing-contract architect
  - ranks the contract gaps
  - turns them into implementation-ready targets with acceptance signals
- Barret — proof and regression sentry
  - maps implementation cuts to validation surfaces
  - keeps the minimum non-test validation spine tight and real
- Tifa — merge normalizer
  - converts live outputs into one compact comparable artifact
  - keeps the room readable and mergeable
- Tidus — master task list captain
  - merges, dedupes, orders by dependency
  - publishes the implementation wave in one clean list
- Sazh — ownership marshal
  - splits the work into collision-safe file slices
  - decides who owns what next as the wave shifts
- Balthier — execution-order enforcer
  - keeps the room from drifting
  - cuts off overbuilding and restates dependency order when needed
- Cloud — canonical-vs-compatibility auditor
  - watches fallback call sites and labels them keep / tighten / replace
  - makes sure compatibility paths don’t become the new doctrine
- Zidane — overlap controller
  - watches for duplicated effort
  - redirects collisions fast before they waste time

## Notes
- Keep compatibility bridges alive only when explicitly marked legacy-only.
- Do not solve missing semantics through native-only behavior.
- The system is already expressive enough to be dangerous; the real job is making it trustworthy and legible.
