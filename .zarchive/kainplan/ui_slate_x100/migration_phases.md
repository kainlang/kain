# Atlas Migration Phases

- Purpose: Give Forge, Vector, Delta, and later Slate/UE work a strict order that preserves compiler-owned semantics and avoids backend-local shortcuts.

## Phase 1: Contract Freeze And Leak Containment

- Owner lane: `Atlas`
- Exit condition: Current ownership and leak points are documented and accepted as the cut line.

Actions:

- Freeze the current seam: `kain-core` authoring and lowering, `kain-ui` runtime graph, `kain-ui-native` adapter, `kain-driver` packaging, `crates/ue5` metadata.
- Treat `UiNativeProjection`, `ui_runtime_systems_from_tree`, and runtime snapshot chrome as compatibility-only.
- Block new host-only semantics from landing in `kain-ui-native`.

Why first:

- Forge and Vector need a stable split before adding more runtime depth or authoring expressiveness.

## Phase 2: Compiler Contract Deepening

- Owner lanes: `Vector` with `Forge` alignment
- Exit condition: New semantic contracts are emitted as typed bundle truth, not inferred by the host.

Required bundle additions:

- event routes
- command definitions and dispatch targets
- focus scope and selection scope declarations
- transaction-capable interaction contracts
- schema-driven widget definitions for inspectors, property grids, menus, tables, and trees
- paint primitives and motion tracks
- capability requirement ids and fallback expectations

Required removals from the long-term contract:

- stringified event props as the main event boundary
- host-discovered widget meaning
- runtime-only assumptions about docking, chrome, or command surfaces

## Phase 3: Runtime Graph Grounding

- Owner lane: `Forge`
- Exit condition: `kain-ui` can execute the deeper contract without relying on tree-shape heuristics for primary behavior.

Required work:

- replace inferred event/command/focus/selection/runtime entries with emitted contract consumption
- keep `UiPatch` backend-agnostic
- make transactions, scheduler phases, and invalidation first-class runtime state
- preserve workspace layout persistence and hot reload transfer through semantic ids

Compatibility rule:

- `ui_runtime_systems_from_tree(...)` may remain for old assets, but new vertical slices must not depend on it.

## Phase 4: Native Host Reset

- Owner lane: `Delta`
- Exit condition: Product apps launch as authored shells by default, with devtools opt-in only.

Required work:

- remove default topbar and inspector injection from normal app mode
- stop using runtime snapshot sidecars as product-shell identity
- keep backend fallbacks explicit through capability tables
- realize authored chrome instead of host chrome

Allowed host responsibilities:

- rendering
- input plumbing
- viewport execution
- optional devtools mode
- capability reporting

Forbidden host responsibilities:

- inventing product bars, labels, or runtime workspace metadata
- changing widget semantics based on hidden host mode

## Phase 5: Adapter Contract Extraction

- Owner lanes: `Atlas` handoff to future Slate/UE implementation work
- Exit condition: A real `kain-ui-slate` path can consume the same semantic contracts as native.

Required work:

- define adapter-facing bundle sections that are backend-neutral
- preserve `crates/ue5` as metadata and lowering support
- map semantic widget families to backend-local realizations through backend capability tables

Success bar:

- Slate/UE work should need backend mapping and widget realization, not a second semantic model.

## Phase 6: Legacy Compatibility Retirement

- Owner lanes: `Sweep` and `Scribe` after implementation lands
- Exit condition: Old debug-host habits stop teaching the wrong architecture.

Retire or demote:

- demo-first host shell defaults
- runtime snapshot-driven chrome
- host-only labels in product mode
- weak smoketests that pass without semantic depth

Keep only if clearly marked compatibility:

- `UiNativeProjection`
- legacy runtime-system synthesis
- old authored trees without the new contracts

## Merge Order

1. Atlas docs
2. Vector compiler contract updates
3. Forge runtime graph updates
4. Delta native host reset and showcase refresh
5. Aegis acceptance gate alignment
6. Slate/UE adapter implementation work
7. Sweep cleanup and Scribe durable docs

## Blockers

- If Vector and Forge disagree on whether a concept is compile-time truth or runtime-derived state, work stops until the boundary is resolved.
- Delta cannot solve missing semantics by shipping prettier host-only widgets.
- Slate/UE work must not begin from `UiNativeProjection` as if it were the target ABI.

## Choke Point Checklist (Phase Ownership)

This is the explicit mapping of the known choke points to phases so work does not drift.

- `kain-core/src/ui.rs` lowering gaps: Phase 2 (Vector) with Phase 3 (Forge) consumption.
- Event placeholder strings: Phase 2.
- Heuristic `UiRuntimeSystems` synthesis (`ui_runtime_systems_from_tree`): Phase 3 (runtime consumes emitted truth; synthesis becomes legacy-only).
- `RealtimeAppBundle` narrowness and surface truth dependency: Phase 2 emits stable surface truth; Phase 3 makes runtime authoritative; Phase 4 ensures native host does not invent missing surface meaning.
- `UiNativeProjection` flattening: Phase 1 freezes it as compatibility-only; Phase 5 extracts a true adapter contract so Slate/UE does not depend on the projection.

## Versioning And Compatibility Policy (Do Not Wing This)

- Any change that removes or repurposes `UiNativeProjection` must be treated as a schema/ABI migration because non-Rust consumers depend on its serialized tags.
- `ui_runtime_systems_from_tree` must remain callable for legacy bundles until Sweep retires the old smokes, but new authored UI must emit runtime systems explicitly (no new dependence on inference).
- `RealtimeAppBundle` should remain schema-stable and focused on realtime surfaces. If new UI semantics need to cross the boundary, they belong in the UI runtime bundle or a dedicated UI contract bundle, not by expanding `RealtimeAppBundle` into a "kitchen sink."
