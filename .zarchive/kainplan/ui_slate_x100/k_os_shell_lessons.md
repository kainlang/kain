# K_OS Shell Lessons For UI Slate X100

- Owner: Sovereign
- Purpose: Capture the concrete strengths of the older TypeScript shell in `M:\K_OS\src-frontend\ui\shell` that Kain must meet or exceed.
- Why this matters: the old shell is not just visually different. It is more explicit about layout, motion, commands, and behavior, which makes it easier for humans and LLMs to inspect, extend, and verify.

## Core Lesson

The old shell is more legible because it names the workspace model directly instead of hiding it behind generic container props and backend-local behavior.

That legibility comes from a few specific patterns:

1. Explicit workspace graph
2. Explicit panel state
3. Explicit command registry
4. Explicit motion policy
5. Explicit resize and performance behavior
6. Explicit behavior tests

Kain needs those same qualities in compiler-owned and runtime-owned form.

## What The Old Shell Gets Right

### 1. Workspace Structure Is Named

`AppShell.tsx` defines `WorkspaceNode`, `WorkspacePanelConfig`, `WorkspacePanelState`, and `WorkspaceState` directly.

That matters because:

- panel positions are explicit
- split direction is explicit
- fixed vs resizable edges are explicit
- active tabs are explicit
- persisted size and collapsed state are explicit

An LLM can reason about the shell quickly because the spatial model is not implicit.

## 2. Geometry Behavior Is Predictable

`AppShell.tsx` and `useResizablePanel.ts` make resize behavior bounded and inspectable.

That matters because:

- drag deltas are relative to drag start
- min and max clamps are explicit
- persistence keys are explicit
- resize completion has a named lifecycle
- `requestAnimationFrame` usage is deliberate instead of incidental

Kain should expose this same kind of geometry lifecycle in runtime structures, not bury it in a backend callback path.

## 3. Motion Has A Named Policy

`motionSystem.ts` separates:

- motion mode
- performance tier
- interaction state
- the derived visual state that results

This is the right shape for Kain too. Motion should be authored and emitted as named policy plus runtime playback state, not as ad hoc animation flags on random widgets.

## 4. Commands Are A Real Surface

`quickMenuRegistry.ts` treats commands as a first-class registry with:

- stable ids
- labels
- source registration
- snapshots
- subscription semantics

Kain should move command surfaces into compiler-owned and runtime-owned truth the same way. A command palette should not need host-local invention.

## 5. Verification Is Structural

`shell-ui.test.tsx` proves behavior through structure:

- tab rendering and active state
- collapsed panel behavior
- resize keyboard and pointer semantics
- preset switching
- resize math correctness

This is the important point: the old shell is verifiable because its behavior is expressed through named contracts that tests can target directly.

## What Kain Must Add To Exceed It

The target is not to rebuild the TypeScript shell in Rust. The target is to keep Kain's stronger semantic/runtime ambitions while becoming at least this explicit.

Kain should add:

- a compiler-owned workspace schema with explicit split graphs, tab wells, docking intent, and persisted layout identity
- a runtime-owned geometry graph with stable node ids, computed rects, sizing constraints, and layout relations
- command registry contracts that survive authoring, bundling, runtime, and backend realization
- motion policy contracts with named modes, performance tiers, and fallback rules
- verification surfaces that can assert layout, focus, selection, command, and interaction correctness without screenshot-only comparisons

## Spatial Verifiability Standard

To be LLM-friendly, Kain must expose enough spatial truth that a strong model can answer questions like:

- Which panel owns this button?
- Which tab well is active?
- Is this button inside the wrong region?
- Did a resize violate constraints?
- Did focus move to the expected target?
- Did a command surface open in the right anchor zone?

That requires explicit data structures for:

- computed rects
- parent/child spatial containment
- docking and split relationships
- z-order and overlay stacks
- focus order and navigation edges
- anchor targets for menus, popovers, and tooltips

## Performance Standard

The old shell already encodes a better performance posture than Kain's current UI path in a few places. Kain should preserve and expand that posture:

- bounded invalidation
- frame-aware resize updates
- explicit idle/quiescent behavior
- named motion reduction policies
- explicit interaction budgets for heavy shells

The runtime should make performance observable instead of anecdotal.

## Design Rule Going Forward

If a UI concept cannot be described in explicit semantic, runtime, and verification structures, then Kain is not ready to claim it as a first-class UI capability.

That rule applies to:

- layout
- tabs
- docking
- commands
- motion
- property grids
- overlays
- trees
- tables
- viewports
- editor chrome
