# UI Slate X100 Backend Capability Matrix

- Owner: Aegis
- Purpose: Define the backend contract that all UI semantics must pass through so `Native`, future `Web`, future `Slate`, and `Debug` surfaces consume one compiler-owned model.
- Current baseline: [crates/kain-ui/src/lib.rs](/M:/Code/Kain/crates/kain-ui/src/lib.rs) already exposes a starter `UiBackendCapabilities` registry. That registry is useful, but it is too shallow for Slate X100 and must expand toward the matrix below.

## Capability Status Vocabulary

- `Shipping`: Required and proven in the current shipping backend.
- `Contract`: Must exist in bundle/runtime truth now so a backend can consume it later without native-only reverse engineering.
- `Fallback`: Backend may degrade the visual or interaction shape, but only through an explicit capability path.
- `Forbidden`: Backend must not provide this mode for product output.

## Backend Roles

| Backend | Role In This Overhaul | Non-Negotiable Rule |
| --- | --- | --- |
| `Native` | Shipping proof backend | Must realize all `P0` and `P1` platform features in packaged product mode. |
| `Web` | Future adapter target | Must share the same semantic contracts even where realization lags. No native-only semantics. |
| `Slate` | Future editor/backend target | Must be able to consume the same command, docking, focus, selection, and schema contracts. |
| `Debug` | Inspection and diagnostics only | Must never be the place where product meaning actually lives. |

## Capability Matrix

| Capability Family | Compiler/Runtime Truth Required | Native | Web | Slate | Debug | Fallback Rule | Reject If |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Product shell ownership | Authored shell nodes, chrome roles, and launch posture flags | Shipping | Contract | Contract | Forbidden for default product mode | Backend may add window plumbing, not product chrome meaning | Packaged apps still inherit host shell furniture by default |
| Tabs and workspace persistence | Tab-group semantics plus persisted active-state model such as `workspace_layout.active_tabs` | Shipping | Contract | Contract | Contract | If a backend cannot draw native tabs, it still must preserve active-state semantics and expose a declared alternate presentation | Tab identity or persistence lives only in backend-local state |
| Docking and split layout | Dock regions, splits, drag targets, transactions, and persistence metadata | Shipping | Contract | Shipping | Contract | Backend may offer a simplified presentation only if the semantic dock graph and transactions remain intact | Dock behavior is implemented as host-local layout code with no bundle/runtime representation |
| Menus, command bars, and command palettes | Command registry, routes, enablement state, keyboard bindings, and invocation transactions | Shipping | Contract | Shipping | Contract | Unsupported visual affordances may fall back to a listed command surface, never to hidden host shortcuts | Commands exist only as backend callbacks or raw key handlers |
| Focus and selection | Focus graph, selection scopes, traversal policy, and transaction semantics | Shipping | Contract | Shipping | Contract | Visual cues may degrade; semantics may not | Focus and selection are implicit backend widget behavior |
| Property grids and schema-driven inspectors | Schema metadata, editor widgets, validation rules, grouping, and read/write channels | Shipping | Contract | Shipping | Contract | Backends may swap the concrete control family, but not the schema contract | Inspectors are still bespoke handwritten layouts with no shared schema |
| Trees, tables, and dense data views | Node identity, virtualization hints, sort/filter metadata, column definitions, and selection semantics | Shipping | Contract | Shipping | Contract | When virtualization or rich cells are unavailable, backend must declare downgraded behavior | Large-data surfaces require backend-specific one-offs |
| Graphs, timelines, canvases, and overlays | Surface semantics, event routes, drag transactions, selection, camera/viewport coupling, and patchable scene state | Shipping | Contract | Contract | Contract | Fallback may simplify rendering, but not erase authored structure or interaction meaning | Complex editor surfaces bypass the semantic runtime and mutate host scene state directly |
| Viewport embedding | Viewport slots, overlay layers, tool handles, command routes, and input-capture policy | Shipping | Contract | Shipping | Contract | If a backend lacks live 3D embedding, it must expose an explicit placeholder or unsupported-state surface | Viewports stay a backend special case with no semantic slot/overlay contract |
| Paint layers and surface recipes | Background layers, gradients, images, masks, blur, border recipes, elevation, and compositing intent | Shipping | Contract | Contract | Contract | Visual downgrades must be capability-driven and inspectable | Rich paint is injected after bundle load or only exists in smoke renderers |
| Motion and animation | Transition descriptors, animation tracks, timing model, interrupt policy, and reduced-motion handling | Shipping | Contract | Contract | Contract | Backend may reduce effects based on capability policy, but it must preserve authored state transitions | Animation exists only as backend timers with no semantic schedule |
| Text and accessibility | Text roles, rich text spans, accessibility tree mapping, labels, descriptions, and keyboard nav semantics | Shipping | Shipping | Contract | Contract | Backend may declare partial accessibility support, but the semantic tree must still exist | Accessibility is treated as a native-only concern or omitted from emitted truth |
| Devtools surfaces | Inspector routes, patch-stream viewer hooks, capability diagnostics, and product/devtools separation metadata | Shipping | Contract | Contract | Shipping | Devtools may be richer on debug backends, but the hooks must attach to shared runtime truth | Debug surfaces are required to make product mode usable |

## Required Capability Table Expansion

The current `UiBackendCapabilities` booleans cover only:

- windowing
- DOM embedding
- GPU viewports
- docking
- rich text
- pointer capture
- accessibility tree

Slate X100 needs the capability system widened so validation can ask explicit questions about:

- product-mode versus devtools-mode posture
- authored chrome ownership
- schema-driven widget families
- command surfaces and keyboard routing
- focus and selection models
- motion support and reduced-motion policy
- paint/compositing depth
- viewport-overlay coupling
- fallback and unsupported-state presentation

## Fallback Policy

- Fallback is acceptable only when the capability table says it is acceptable.
- Fallback must preserve semantic identity, focus behavior, selection state, commands, and persistence.
- Fallback must be user-visible or trace-visible. Silent omission is a failure.
- `Debug` may expose raw inspectors or logs, but product backends cannot require `Debug` to achieve correct behavior.

## Validation Expectations Per Backend

| Backend | Minimum Validation Requirement |
| --- | --- |
| `Native` | Prove the full product shell, widget/chrome depth, interaction depth, paint, motion, packaging posture, and performance behavior. |
| `Web` | Prove that bundle/runtime contracts are sufficient to lower authored meaning without native-specific assumptions. Explicit unsupported-state and fallback records are mandatory where realization is incomplete. |
| `Slate` | Prove that command, docking, focus, selection, property-grid, and viewport contracts are representable without re-authoring the semantics. |
| `Debug` | Prove that inspectors and traces consume shared runtime truth and remain opt-in. |

## Backend-Rejection Triggers

- A backend introduces product semantics that do not exist in compiler/runtime truth.
- A feature is marked supported only because the backend silently substitutes a weaker behavior.
- Native is treated as the semantic source of truth and `Web` or `Slate` are told to copy its implementation details later.
- `Debug` remains the default route to inspect or even access important product interactions.

## Exit Condition

The capability model is acceptable when the matrix above is encoded as data, tied to emitted truth, and referenced by the regression plan in [regression_plan.md](/M:/Code/Kain/docs/kainplan/ui_slate_x100/regression_plan.md).
