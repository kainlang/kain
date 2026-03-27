# UI Slate X100 Sovereign Kickoff

- Owner: Sovereign
- Swarm: `Obsidian Crown`
- Workspace: `M:\Code\Kain`
- Created At: 2026-03-27 00:09 EDT
- Purpose: Convert the UI-overhaul mission from aspiration into an execution bar that all worker lanes can build against without ambiguity.

## Mission Restatement

Kain UI is not allowed to remain a retained tree plus host chrome that makes every packaged app look like the same debug shell.

The target is a platform where:

- authored UI owns the visible shell
- compiler-owned semantics own state, interaction, paint, and widget meaning
- native/editor tooling surfaces are first-class instead of awkward demos
- different apps can feel radically different without escaping the semantic model
- debug tooling is powerful but fully separated from normal product presentation
- the system is explicit enough that strong LLMs can inspect, author, and extend UI without getting lost in backend-local quirks
- the system exposes enough spatial truth that strong LLMs can tell when layout, ownership, anchoring, or focus flow is wrong without relying only on screenshots

## Current Failure Modes To Eliminate

- Default packaged apps still boot in a debug-heavy posture, with runtime inspector and host shell framing contaminating the product view.
- The native host injects hardcoded chrome and labels that flatten authored UI identity.
- The authoring layer has layout/theme/token power, but not enough state, command, paint, motion, and widget semantics to produce truly deep tooling UI.
- Widget realization is too shallow. Panels exist, but the system is not yet proving property-grid, menu, table, toolbar, command, and complex interaction depth.
- The repo currently teaches the wrong lesson through old smokes: that Kain UI is mostly a styled shell around debug furniture instead of a serious authored platform.

## Hard Acceptance Bar

The overhaul is only successful if all of the following are true.

### 1. Product Mode Owns The Screen

- Packaged apps launch with debug tooling hidden by default.
- No hardcoded host badges, root labels, or injected framing survive in normal app mode unless explicitly authored or explicitly enabled as chrome.
- The runtime inspector, semantic tree, patch stream, and similar tooling move into opt-in devtools surfaces.

### 2. Compiler-Owned UI Semantics Become Deep Enough For Real Tools

- `.kn` authoring supports first-class reactive state, derived values, event routes, command dispatch, focus scopes, selection scopes, and transactions.
- Paint and motion become semantic contracts, not host-only tricks: gradients, images, layered surfaces, masks, blur, transitions, and authored animations must flow through emitted bundles.
- Schema-driven UI becomes first-class for inspectors, property grids, tables, menus, and tool metadata views.

### 2B. LLM-Legible By Construction

- Authoring and runtime truth must be explicit enough that a strong model can inspect the codebase and understand how a UI works without tracing hidden backend heuristics.
- Core UI behavior must not depend on opaque prop strings, smoke-specific drawing branches, or backend-owned fallback magic.
- Widget, paint, motion, command, and schema systems should be driven by named contracts and registries rather than ad hoc literal combinations that make authored UIs collapse into the same shape.

### 2C. Spatial Verifiability By Construction

- Runtime truth must expose workspace graphs, panel ownership, active tab membership, computed rects, containment, overlay order, and focus-order edges explicitly.
- Menus, popovers, tooltips, and other anchored surfaces must have inspectable anchor-zone data instead of backend-local placement guesses.
- Resize and docking constraints must be explicit enough that a regression tool or LLM can tell whether a control moved into the wrong region.
- Reference comparison: [k_os_shell_lessons.md](/M:/Code/Kain/docs/kainplan/ui_slate_x100/k_os_shell_lessons.md)

### 3. Widget Depth Reaches Serious Desktop/Editor Territory

- The platform can express and realize top bars, tab wells, sidebars, toolbars, menus, status bars, command palettes, splitters, docking, trees, tables, property grids, timelines, graphs, overlays, and viewport-adjacent controls.
- Complex interaction patterns such as drag/drop, docking moves, command invocation, keyboard focus movement, selection changes, and viewport/tool coupling are backed by semantic runtime structures rather than hidden backend state.
- Capability gaps are explicit and data-driven; unsupported semantics degrade through declared fallback paths instead of silently disappearing.

### 4. Visual Language Stops Collapsing Into One Host Aesthetic

- The repo must contain at least three showcase-grade native apps with clearly different visual identities:
- a bold editorial/information-heavy shell
- a dense operator/production shell
- a property-grid or editor-workbench shell with serious tooling chrome
- Those apps must look authored, not host-framed. If screenshots still read as the same debug shell with different text, the overhaul fails.

### 5. Backend Contracts Stay Clean

- `kain-ui-native`, future web lowering, and a future Slate/UE adapter all consume one compiler-owned semantic model.
- Backend-specific conveniences may exist, but they cannot become the source of UI meaning.
- The native host must stop acting like a parallel UI framework with its own hidden product semantics.

## Explicit Failure Conditions

Treat the overhaul as failed if any of these remain true after the supposed landing:

- new smokes still require hiding host contamination manually to look acceptable
- widget meaning lives mainly in host code instead of compiler/runtime contracts
- paint/motion richness exists only through smoke-specific renderer hacks
- complex interaction depends on backend-local widget state with no semantic event/transaction model
- “Slate x100” is claimed based on prettier screenshots without deeper state/interaction/widget contracts
- layout correctness still depends mainly on screenshots because runtime geometry and ownership truth are too weak to inspect directly

## Phase Boundaries

### Phase 1: Architecture Cut Lines

- Atlas defines current-state ownership, target boundaries, and migration order.
- Deliverable quality bar: a worker should know exactly what lives in `kain-core`, `kain-ui`, `kain-ui-native`, backend adapters, and devtools.

### Phase 2: Semantic Runtime And Authoring Depth

- Forge and Vector widen the runtime model and authoring contracts together.
- No Delta widget/chrome realization work should outrun this phase’s contract decisions.

### Phase 3: Native Host Reset And Visual Proof

- Delta removes debug-first defaults, realizes authored chrome, and proves non-generic shells.
- This phase is not “make egui prettier.” It is “make authored semantics visibly dominate the host.”

### Phase 4: Acceptance Gate And Retrofit

- Aegis locks the acceptance matrix.
- Sweep retires misleading legacy demos and retrofits surviving smokes/apps onto the new posture.

### Phase 5: Durable Narrative And Handoff

- Scribe updates north-star docs, architecture, memory, author guidance, and migration notes only after the platform shape is stable enough to preserve.

## Blocker Policy

- Atlas blockers are architecture blockers. Sovereign resolves them before Forge, Vector, or Delta diverge.
- Forge and Vector disagreements on semantic ownership block Delta by default.
- Delta visual wins do not override semantic deficits. A prettier host with shallow contracts does not count as progress against the mission.
- Aegis can reject “done” claims from any lane that meet demo goals but fail platform goals.

## First Worker Kickoff Orders

### Atlas

- Start with `crates/kain-core/src/ui.rs`, `crates/kain-ui`, `crates/kain-ui-native`, and `crates/kain-ui/NORTH_STAR_SPEC.md`.
- Produce target boundaries and current-state leak map before anyone widens backend realization.

### Forge

- Prepare runtime structures and patch/event model expansion, but do not finalize ownership until Atlas publishes boundaries.

### Vector

- Prepare authoring and bundle contract proposals in parallel with Forge, especially around state, commands, paint, motion, and schema-driven widgets.

### Delta

- Begin with a host contamination inventory and a list of debug-first defaults to remove, but hold larger widget/chrome realization choices until Forge and Vector stabilize semantics.

### Aegis

- Start drafting the acceptance matrix immediately so every lane has a visible finish line.

## Sovereign Decisions Logged At Kickoff

- Kain UI is being judged as a platform, not as an isolated smoke renderer.
- Host debug chrome is now considered contamination unless explicitly enabled.
- Visual distinctiveness is a required output, but semantic depth is the primary bar.
- A future Slate/UE adapter is an architectural requirement, not a post-hoc aspiration.
