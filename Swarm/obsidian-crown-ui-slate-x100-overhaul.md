# Swarm: Obsidian Crown

- Swarm Slug: obsidian-crown-ui-slate-x100-overhaul
- Mission: Overhaul Kain UI from a debug-heavy retained-shell prototype into an authored-first, renderer-agnostic, tooling-grade UI system that exceeds Slate in semantic depth, widget breadth, styling freedom, motion, and native/editor integration.
- Workspace Root: M:\Code\Kain
- Swarm Status: active
- Swarm Owner: Sovereign
- Created At: 2026-03-26 21:24 EDT
- Updated At: 2026-03-27 18:58 EDT
- Completion Rule: When every lane is done or cancelled, Sovereign moves this file into ./Swarm/completed/.

## Objectives

- [ ] Replace the current debug-first native host posture with an authored-first runtime posture where product UI owns the screen and debug tooling is explicitly opt-in.
- [ ] Expand compiler-owned UI semantics to cover reactive state, derived values, event routes, command dispatch, focus/selection scopes, transactions, motion, schema-driven widgets, and richer paint/surface primitives.
- [ ] Land a robust widget and chrome system for desktop/editor tools: top bars, tab wells, toolbars, menus, command palettes, property grids, trees, tables, timelines, graphs, overlays, status bars, and viewport-adjacent controls.
- [ ] Make theme, typography, color, paint, motion, and composition expressive enough that separate apps can feel deliberately different instead of collapsing into the same host aesthetic.
- [ ] Define the backend contract so `kain-ui-native`, future web lowering, and a real Slate/UE editor adapter all consume one compiler-owned semantic model.
- [ ] Refresh the smoke matrix so the repo proves bold authored shells, dense operator tools, data-heavy editors, animated surfaces, and Slate-class workflows without relying on host debug chrome.

## Shared Constraints

- Preserve compiler-owned truth. `kain-core` and `kain-ui` define UI meaning; hosts consume emitted semantics rather than inventing parallel models.
- Do not let `kain-ui-native` remain the implicit source of app chrome, widget identity, or debug behavior. Default app mode must be product mode, not inspector mode.
- Keep the system native-first and tooling-first as stated in `crates/kain-ui/NORTH_STAR_SPEC.md`.
- Prefer data-driven registries for widget capabilities, event routes, paint primitives, backend support, and style mappings over scattered string switches.
- LLM legibility is a first-class design requirement. Authoring, runtime data, widget registries, and paint/motion contracts must be explicit enough that a strong model can understand and extend the system without backend archaeology.
- Spatial verifiability is also a first-class design requirement. Runtime truth must expose ownership, geometry, containment, anchor zones, overlay order, and focus travel strongly enough that layout correctness is machine-checkable.
- Separate debug tooling from product UI explicitly. Semantic tree inspectors, patch logs, and runtime diagnostics must not contaminate normal packaged apps by default.
- Avoid smoke-local hacks. New demos should prove reusable platform capability, not one-off custom drawing paths that bypass the semantic model.
- Preserve backward migration strategy where feasible, but do not protect weak abstractions if they block the target architecture.

## Launch Order

1. Sovereign
2. Atlas
3. Forge + Vector
4. Delta + Aegis
5. Scribe
6. Sweep

## Lane: Sovereign

- Role: Lead planner, dependency arbitration, cross-lane merge strategy, and archive owner.
- Status: in_progress
- Claimed By: Sovereign
- Claimed At: 2026-03-26 21:24 EDT
- Depends On: none
- Deliverables:
  - Finalized swarm plan and dependency map
  - Cross-lane decision log inside this file
  - `docs/kainplan/ui_slate_x100/00_SOVEREIGN_KICKOFF.md`
  - Merge order and cutover strategy for the UI overhaul
  - Final completion review and archive move into `./Swarm/completed/`
- Task List:
  - [x] Convert the current pain points into a hard acceptance bar for “Slate x100” rather than allowing vague success criteria.
  - [x] Keep Atlas, Forge, Vector, and Delta ownership non-overlapping at kickoff and document the seam in the Sovereign kickoff material.
  - [x] Decide the phase boundaries between semantic-model work, authoring-syntax work, native-host reset, and smoke refresh.
  - [ ] Revisit this file after each major lane lands and tighten priorities, blockers, and merge order.
  - [ ] Own the final go/no-go decision for removing legacy host chrome defaults and retiring weak demos.
- Notes:
  - Sovereign is the only lane allowed to redefine scope or accept architectural tradeoffs that affect multiple lanes.
  - Kickoff source of truth: `docs/kainplan/ui_slate_x100/00_SOVEREIGN_KICKOFF.md`

## Lane: Atlas

- Role: Architecture mapping, subsystem boundaries, migration design, and backend split strategy.
- Status: done
- Claimed By: Atlas
- Claimed At: 2026-03-27 05:42 EDT
- Depends On: Sovereign
- Deliverables:
  - `docs/kainplan/ui_slate_x100/current_state_map.md`
  - `docs/kainplan/ui_slate_x100/target_architecture.md`
  - `docs/kainplan/ui_slate_x100/migration_phases.md`
  - `docs/kainplan/ui_slate_x100/backend_boundary_matrix.md`
- Task List:
  - [x] Inventory the current ownership split across `crates/kain-core/src/ui.rs`, `crates/kain-ui`, `crates/kain-ui-native`, `crates/kain-driver`, and UE/editor-facing crates.
  - [x] Identify every place where host code is currently leaking product chrome, debug posture, or renderer-local semantics into normal app behavior.
  - [x] Define the target subsystem boundaries for semantic IR, runtime graph, authoring contracts, widget registry, paint system, backend capability tables, and devtools.
  - [x] Propose a staged migration that keeps the compiler-owned bundle model intact while widening semantics enough for Slate-class tooling surfaces.
  - [x] Specify the eventual adapter boundary for a real `kain-ui-slate` or UE editor integration path so backend work does not get trapped inside egui-specific assumptions.
- Notes:
  - Atlas should treat `crates/kain-ui/NORTH_STAR_SPEC.md` as intent, then translate it into implementation-ready repo boundaries.

## Lane: Forge

- Role: Core semantic runtime lane for retained graph behavior, reactivity, scheduler depth, and mutation semantics.
- Status: in_progress
- Claimed By: Forge
- Claimed At: 2026-03-27 18:39 EDT
- Depends On: Atlas
- Deliverables:
  - `kain-ui` runtime expansion for signal/dependency tracking, invalidation, transactions, and command-ready node state
  - Patch model upgrades for interaction, motion, and richer widget updates
  - `docs/kainplan/ui_slate_x100/runtime_execution_model.md`
- Task List:
  - [ ] Design and implement the next runtime layer after the current retained tree: exact dependency invalidation, derived values, computed nodes, and transaction-aware updates.
  - [ ] Introduce first-class runtime structures for event routes, command buffers, focus graph, selection model, animation tracks, and scheduler decisions instead of leaving them as partial or disconnected fields.
  - [ ] Replace broad host-side assumptions with explicit patch/event semantics that backends can consume uniformly.
  - [ ] Define how imperative editor interactions such as docking, drag/drop, graph editing, and viewport tools mutate the retained graph without collapsing into ad hoc host callbacks.
  - [ ] Prove that the patch stream can express complex UI state changes without needing host-local widget state as the hidden source of truth.
  - [ ] Keep performance and inspectability first-class: every new runtime subsystem should remain explainable and backend-agnostic.
- Notes:
  - Forge owns the semantic execution model, not parser syntax and not host chrome.
  - 2026-03-27 18:35 EDT: Atlas is done. Forge is unblocked and should treat the Atlas target architecture plus the LLM-legibility rule as hard constraints.

## Lane: Vector

- Role: Authoring contracts, parser/lowering, schema-driven UI, and compiler-emitted bundle truth.
- Status: done
- Claimed By: Vector
- Claimed At: 2026-03-27 18:39 EDT
- Depends On: Atlas
- Deliverables:
  - `docs/kainplan/ui_slate_x100/authoring_contract.md`
  - `docs/kainplan/ui_slate_x100/widget_registry_schema.md`
  - `docs/kainplan/ui_slate_x100/paint_motion_schema.md`
  - compiler/runtime bundle updates in `crates/kain-core`
- Task List:
  - [x] Extend `.kn` UI authoring so state, derived values, actions, event routes, command dispatch, focus scopes, selection scopes, and transactions are cleanly expressible instead of host-invented.
  - [x] Define a compiler-owned widget registry and schema-driven UI contract for inspectors, property grids, forms, menus, tables, trees, and command surfaces.
  - [x] Add compiler-owned paint and motion semantics for backgrounds, gradients, images, layered surfaces, masks, blur, transitions, and authored animation intent.
  - [x] Ensure all new semantics flow through `UiBuildOutput`, runtime bundles, and realtime app bundles without backend-specific gaps.
  - [x] Tighten authoring ergonomics so rich UI does not require verbose host-aware attribute soup for common desktop/editor patterns.
  - [x] Preserve the distinction between semantic truth and backend lowering, especially for future Slate and web adapters.
- Notes:
  - Vector owns author-facing expressiveness and emitted truth. It should delete parser pain, not hide it behind host sugar.
  - 2026-03-27 18:35 EDT: Atlas is done. Vector is unblocked and should optimize for explicit, LLM-legible authoring and bundle contracts instead of terse prop soup.
  - 2026-03-27 18:58 EDT: Landed `authoring_contract.md`, `widget_registry_schema.md`, and `paint_motion_schema.md` plus compiler-side emission upgrades (`kain-core/src/ui.rs`, `kain-core/src/realtime_app_bundle.rs`) with an explicit spatial-verifiability surface (`RealtimeAppBundle.ui_contracts.structure_index` + optional `workspace_layout`) and named contract JSON hooks (`ui_*_registry` / `ui_motion_policy` / `ui_workspace_schema`).

## Lane: Delta

- Role: Native host reset, widget/chrome realization, backend capability delivery, and proof-of-look integration.
- Status: blocked
- Claimed By: unclaimed
- Claimed At: unclaimed
- Depends On: Atlas, Forge, Vector
- Deliverables:
  - `kain-ui-native` default app-mode reset with debug chrome opt-in
  - authored chrome realization for tabs, top bars, toolbars, sidebars, status bars, menus, command surfaces, and property-grid-class widgets
  - `smoketest/UI` showcase refresh proving genuinely different visual languages
  - `docs/kainplan/ui_slate_x100/native_host_reset.md`
- Task List:
  - [ ] Remove hardcoded host labels, default inspector posture, and other debug-first shell elements from normal packaged apps.
  - [ ] Make authored chrome first-class so tabs, top bars, workspace frames, panels, badges, menus, and status strips can be owned by semantic UI rather than injected by host scaffolding.
  - [ ] Expand the native realization of widgets and paint surfaces so typography, backgrounds, overlay stacks, motion, and dense tooling layouts do not all flatten into the same egui look.
  - [ ] Introduce capability-driven fallbacks rather than silent visual degradation when a backend cannot yet realize a semantic feature.
  - [ ] Build new showcase apps that prove bold editorial shells, data-dense operator tools, serious property-grid workflows, and viewport-adjacent editor chrome without looking like the current debug host.
  - [ ] Keep the native host aligned with future backend contracts instead of turning it into a second UI framework with its own semantics.
- Notes:
  - Delta owns the “stop looking generic and glitchy” mandate. Product UI must visually overpower host scaffolding, not the other way around.
  - Delta may inventory contamination immediately, but major realization work stays blocked until Atlas, Forge, and Vector are done.

## Lane: Aegis

- Role: Validation strategy, invariants, regression criteria, performance checks, and acceptance gate design.
- Status: done
- Claimed By: Aegis
- Claimed At: 2026-03-27 01:22 EDT
- Depends On: Sovereign
- Deliverables:
  - `docs/kainplan/ui_slate_x100/acceptance_matrix.md`
  - `docs/kainplan/ui_slate_x100/backend_capability_matrix.md`
  - `docs/kainplan/ui_slate_x100/regression_plan.md`
  - smoke and validation criteria for native, bundle, and authoring lanes
- Task List:
  - [x] Define objective acceptance criteria for “Slate x100” in repo terms: widget depth, interaction depth, styling freedom, backend parity, debug separation, and authored-shell distinctiveness.
  - [x] Map invariants that must hold across compiler output, runtime graph, patch streams, backend realization, and packaged app behavior.
  - [x] Design validation for expressive UI features that are easy to fake badly: tabs, docking, menus, property grids, tables, command surfaces, animation, paint layers, and viewport overlays.
  - [x] Establish performance and responsiveness guardrails so richer semantics do not regress interactive tool shells into sluggish host behavior.
  - [x] Define how to validate backend fallback behavior explicitly rather than letting unsupported semantics disappear silently.
- Notes:
  - Aegis is the lane that prevents “looks cooler in one smoke” from masquerading as a platform overhaul.
  - Aegis can begin once the Sovereign kickoff exists; Atlas output will refine, not unlock, the acceptance draft.
  - 2026-03-27 01:22 EDT: Claim started. First pass covers the acceptance gate, backend capability matrix, and regression plan with platform-level rejection criteria.
  - 2026-03-27 01:26 EDT: Published `acceptance_matrix.md`, `backend_capability_matrix.md`, and `regression_plan.md`. The gate now explicitly rejects screenshot-only wins, host-owned semantics, silent fallback, and default debug contamination.
  - 2026-03-27 05:42 EDT: Atlas lane resumed after prior worker hit a usage limit; continuing the same lane claim.
  - 2026-03-27 05:46 EDT: Updated Atlas deliverables with explicit choke-point mapping (`kain-core` lowering gaps, placeholder event lowering, heuristic runtime systems, realtime bundle surface truth dependency, and `UiNativeProjection` ABI risk) plus a concrete backend boundary matrix and phased migration order.

## Lane: Scribe

- Role: Durable docs, architecture narrative, memory capture, migration guidance, and author/operator handoff.
- Status: blocked
- Claimed By: unclaimed
- Claimed At: unclaimed
- Depends On: Atlas, Forge, Vector, Delta
- Deliverables:
  - refreshed `crates/kain-ui/NORTH_STAR_SPEC.md`
  - updated `ARCHITECTURE.md` and `MEMORY.md`
  - `docs/kainplan/ui_slate_x100/author_guide.md`
  - `docs/kainplan/ui_slate_x100/operator_guide.md`
  - `docs/kainplan/ui_slate_x100/migration_notes.md`
- Task List:
  - [ ] Rewrite the north-star and repo architecture docs so they reflect the real target system and not only the current prototype posture.
  - [ ] Capture the design decisions that future agents must preserve: compiler-owned semantics, debug separation, backend capability tables, and authored-first chrome ownership.
  - [ ] Write author guidance for building expressive UIs in `.kn` without relying on renderer-local hacks.
  - [ ] Write operator guidance for packaging, inspecting, and debugging apps without defaulting product builds into devtools mode.
  - [ ] Keep migration notes concrete enough that workers can retrofit existing smoketests and apps deliberately instead of cargo-culting old patterns.
- Notes:
  - Scribe should leave behind a system another frontier agent can extend correctly in one read.
  - Blocked until the architecture and contract shape is stable enough to document without churn.

## Lane: Sweep

- Role: Cleanup, codemods, dead-path retirement, smoke retrofit, consistency pass, and edge-case closure.
- Status: blocked
- Claimed By: unclaimed
- Claimed At: unclaimed
- Depends On: Forge, Vector, Delta, Aegis
- Deliverables:
  - retired or rewritten weak legacy smoketests
  - codemods for renamed attributes, widget contracts, and host defaults
  - consistency cleanup across `smoketest/UI`, docs, and runtime defaults
  - `docs/kainplan/ui_slate_x100/cleanup_report.md`
- Task List:
  - [ ] Remove or quarantine demos that only prove the old debug-host look and would confuse future work about the target quality bar.
  - [ ] Apply codemods and compatibility shims for renamed UI attributes, widget contracts, paint tokens, and host chrome controls.
  - [ ] Retrofit existing smoketests and app scaffolds onto the new authored-first UI posture so the repo stops teaching outdated habits.
  - [ ] Close consistency gaps across bundle generation, native packaging helpers, smoke scripts, and docs after the core work lands.
  - [ ] Produce a finish-pass audit of leftover hardcoded chrome, duplicated visual logic, and dead experimental branches that should not survive the overhaul.
- Notes:
  - Sweep is the “make the repo stop lying about the UI system” lane.
  - Blocked until the new semantics, host posture, and acceptance gates are real enough to retrofit against.

## Cross-Lane Coordination Notes

- Atlas defines target boundaries first. Forge, Vector, and Delta should not diverge on ownership.
- Forge and Vector must agree on the exact seam between runtime semantics and authoring syntax before Delta realizes new widgets or chrome.
- Delta should not land a prettier host-only shell as a substitute for Vector-authored semantics and Forge runtime depth.
- Aegis defines the acceptance gate early so visual polish does not outrun semantic capability.
- Scribe updates only after the architecture and contracts are real enough to be stable.
- Sweep lands late and should delete misleading artifacts aggressively once the new path is proven.

## Sovereign Decision Log

- 2026-03-27 00:09 EDT: The active acceptance bar now requires authored product mode by default, explicit debug/devtools separation, deeper compiler-owned interaction semantics, richer paint/motion contracts, and three clearly distinct showcase-grade native apps.
- 2026-03-27 00:09 EDT: Phase order is now fixed as Architecture -> Semantic Runtime + Authoring -> Native Host Reset + Visual Proof -> Acceptance Gate + Retrofit -> Durable Docs.
- 2026-03-27 00:09 EDT: Delta may inventory and remove host contamination immediately, but substantial widget/chrome realization must follow Forge/Vector contract alignment rather than inventing backend-local semantics.
- 2026-03-27 18:35 EDT: LLM-legible-by-construction is now a platform rule. If the system is so opaque that strong models keep collapsing into the same toy-block shell, the design is failing even before human ergonomics are considered.
- 2026-03-27 18:35 EDT: Atlas is complete. Forge and Vector are unblocked to define runtime and authoring contracts that are explicit enough for both humans and LLMs to extend without backend archaeology.
- 2026-03-27 19:05 EDT: The old K_OS TypeScript shell confirms the missing ingredient is explicit structure, not just visual treatment. Workspace graphs, panel state, command registries, motion policy, and behavior verification now serve as reference patterns for Kain's spatially verifiable runtime and authoring contracts.
