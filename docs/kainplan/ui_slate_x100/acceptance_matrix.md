# UI Slate X100 Acceptance Matrix

- Owner: Aegis
- Swarm: `Obsidian Crown`
- Purpose: Define the acceptance gate for the UI overhaul as a platform decision, not a screenshot decision.
- Inputs: [00_SOVEREIGN_KICKOFF.md](/M:/Code/Kain/docs/kainplan/ui_slate_x100/00_SOVEREIGN_KICKOFF.md), [NORTH_STAR_SPEC.md](/M:/Code/Kain/crates/kain-ui/NORTH_STAR_SPEC.md), [ARCHITECTURE.md](/M:/Code/Kain/ARCHITECTURE.md)

## Gate Rules

- The overhaul is not accepted because one smoke looks better. It is accepted only when compiler-owned semantics, runtime behavior, backend realization, and packaged-app posture all meet this document together.
- `Native` is the shipping proof backend for this phase. `Web` and `Slate` are still part of the gate at the contract level: semantics cannot be native-only inventions.
- `Debug` is not a product backend. It may inspect or visualize semantic truth, but it may not be the hidden place where required product behavior actually lives.
- Any lane may claim visual progress. Aegis may still reject the result if the proof depends on host-local state, smoke-local hacks, or silent backend degradation.

## Acceptance Levels

- `P0`: Launch blocker. Any miss fails the overhaul.
- `P1`: Required platform depth. These must land before the swarm calls the UI reset complete.
- `P2`: Strongening work. Important, but not allowed to masquerade as completion if `P0` or `P1` is missing.

## P0 Acceptance Gate

| Area | Required Outcome | Evidence Required | Reject If |
| --- | --- | --- | --- |
| Product mode ownership | Packaged apps open in authored product mode by default. No runtime inspector, root label, host badge, or debug shell chrome appears unless explicitly enabled. | Native packaged app captures plus launch notes for each showcase app. | A clean screenshot still depends on manually hiding host UI, debug flags, or patched smoke-local defaults. |
| Compiler-owned meaning | State, derived values, command routes, focus, selection, transactions, paint, motion, and widget semantics are represented in compiler/runtime truth rather than invented inside `kain-ui-native`. | Bundle examples, semantic/runtime docs, and patch traces showing authored meaning survives compilation. | The backend must infer required widget behavior from local code paths or handwritten smoke callbacks. |
| Patch-stream authority | Complex UI changes are observable as semantic graph and patch-stream updates with stable node identity. | Patch traces for tab changes, docking operations, property edits, menu invocation, and command execution. | The backend mutates hidden widget state with no corresponding semantic or patch record. |
| Explicit backend capability model | Unsupported backend features are declared and routed through fallback policy. No silent disappearance. | Capability matrix entry plus proof of fallback or unsupported-state surfacing. | Native, web, or future Slate behavior quietly omits authored meaning. |
| Showcase distinctiveness | The repo proves at least three clearly different native shells: editorial, operator, and workbench/property-grid. | Screenshot set, package output names, and authored asset/theme references. | All examples still read as the same debug host with different content. |

## P1 Required Platform Depth

| Area | Required Outcome | Evidence Required | Reject If |
| --- | --- | --- | --- |
| Widget/chrome depth | The platform can semantically express and natively realize top bars, tab wells, toolbars, sidebars, menus, status bars, command surfaces, property grids, trees, tables, overlays, and viewport-adjacent controls. | Showcase matrix plus per-widget bundle and runtime proofs. | A widget class exists only as handcrafted host drawing or smoke-specific code. |
| Interaction depth | Drag/drop, docking, keyboard focus travel, selection changes, command invocation, panel open/close, and viewport-to-shell coordination are driven by runtime semantics. | Interaction traces, patch deltas, and packaged-app recordings. | These flows only work because the native host keeps private state that other backends cannot see. |
| Paint and motion depth | Gradients, layered surfaces, images, masks, blur, transitions, and authored animations survive the bundle path and degrade explicitly when unsupported. | Bundle payload examples, native proof captures, and fallback proofs. | Rich styling exists only through renderer-local hacks or post-bundle mutation. |
| Schema-driven tooling surfaces | Property grids, inspectors, tables, forms, menus, and metadata views can be generated from compiler-owned schema/widget contracts. | Schema inputs, emitted bundle excerpts, and realized examples. | Tooling surfaces are still one-off handwritten widget trees with no reusable contract. |
| Backend portability discipline | Native-specific conveniences do not redefine UI meaning. Web and Slate can consume the same semantic model without reverse engineering the native host. | Capability matrix entries tied to bundle/runtime structures. | Native realization introduces semantics that have no bundle-level representation. |

## P2 Strongening Work

| Area | Required Outcome | Evidence Required | Reject If |
| --- | --- | --- | --- |
| Accessibility and text depth | Accessibility tree mapping, text roles, keyboard navigation, and rich text semantics are explicit and backend-aware. | Backend capability entries, text-role proofs, and navigation traces. | Accessibility is deferred by burying semantics in visual-only nodes. |
| Visual-system range | Typography, palettes, surface recipes, spacing systems, and motion tokens are data-driven enough that multiple product lines can coexist. | Theme/token registries plus cross-app comparison. | Distinctiveness still depends on editing backend code or ad hoc widget styling branches. |
| Devtools isolation | Semantic tree viewers, patch viewers, and runtime diagnostics exist as dedicated devtools surfaces, not as default product chrome. | Devtools activation proof and packaged-app product proof. | Debug and product modes are still coupled operationally. |

## Platform Invariants

These invariants are cross-cutting. Breaking any of them is grounds for rejection even if the screenshots look good.

### Authoring To Bundle

- Every required UI behavior must have a compiler-owned representation in emitted UI/runtime bundles.
- Backend-only attributes are not allowed to become the sole source of product behavior.
- Schema-driven surfaces must emit enough metadata that a backend can realize them without hardcoded knowledge of a specific smoke.

### Bundle To Runtime Graph

- The retained runtime graph must preserve stable identity for nodes that survive an interaction.
- Focus, selection, commands, transactions, and animations must be runtime-visible structures, not incidental backend bookkeeping.
- `workspace_layout.active_tabs` style state is the pattern to preserve: authored meaning persists through shared runtime truth instead of renderer-local storage.

### Runtime Graph To Patch Stream

- Local interactions must produce bounded, explainable patches. Full-root repatching for local edits is a failure unless the authored change truly invalidates the root.
- Patch streams must show enough information to explain widget-state transitions, chrome changes, and motion scheduling.
- Idle UI must settle. Quiescent apps are not allowed to emit endless patch churn.

### Patch Stream To Backend Realization

- A backend may optimize realization, but it may not invent missing meaning.
- Capability checks must happen before fallback. Fallback must be explicit in data, logs, or visual markers.
- Any backend-specific approximation must preserve command, focus, and selection semantics even when visuals degrade.

### Backend Realization To Packaged App

- Product builds must default to product mode.
- Debug tooling must be opt-in, discoverable, and separate from product chrome.
- Packaged app output must match the semantic bundle rather than a debug-host template with the authored UI mounted inside it.

## Required Proof Set

The overhaul is not accepted until the repo can present all of the following:

1. A compiler/bundle proof for each major semantic family: state, commands, focus, selection, transactions, paint, motion, schema widgets, and viewport-adjacent controls.
2. A runtime/patch proof for each hard-to-fake interaction: tab change, dock move, command execution, property edit, menu open, and overlay interaction.
3. A packaged native proof for three distinct showcase shells: editorial, operator, and workbench/property-grid.
4. A backend-capability proof showing that unsupported semantics are surfaced explicitly instead of vanishing.
5. A regression run that compares the accepted bundle/runtime traces against future changes.

## Explicit Failure Conditions

- “Slate x100” is claimed on the strength of screenshots while state, command, focus, selection, paint, or motion semantics still live mainly in host code.
- Docking, graph editing, command routing, or property-grid behavior depends on backend-local widget state with no semantic transaction model.
- Native captures look good only because the runtime inspector or host shell was hidden manually after launch.
- Distinctive visuals come from one-off smoke renderers instead of reusable semantic paint and chrome contracts.
- Web or future Slate realization would need native-host reverse engineering because the emitted truth is too thin.
- The backend capability table says a feature is unsupported, but the product surface gives no explicit fallback or unsupported-state signal.

## Exit Decision

The UI overhaul may be called complete only when `P0` and `P1` are fully satisfied, the invariants hold, and the regression plan in [regression_plan.md](/M:/Code/Kain/docs/kainplan/ui_slate_x100/regression_plan.md) has an accepted baseline.
