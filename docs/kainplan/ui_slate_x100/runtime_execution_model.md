# UI Slate X100 Runtime Execution Model

- Owner lane: `Forge`
- Scope: `crates/kain-ui` runtime contract (retained graph execution, invalidation, transactions, and spatial verifiability).
- Non-goal: inventing backend-local UI meaning. Backends realize; runtime owns semantics + state + patches.

## Contract Summary

`kain-ui` owns the runtime truth for:

- retained semantic graph (`UiTree` and stable `UiNodeId`)
- runtime-owned state (`UiRuntimeSystems`: signals, resources, workspace/tabs, focus/selection, motion, overlays, scheduler)
- mutation authority (`UiTreeMutator` emits patches for every semantic graph mutation)
- transaction log (`UiTransaction` is the explainable record of “what changed and why”)
- spatial verifiability (computed rects + containment + overlay order + anchors + focus traversal are queryable from explicit structures)

Backends (native/web/slate) must treat these as their source of truth. They may optimize realization, but they must not become the hidden owner of state or geometry.

## Primary Data Types (Runtime-Owned)

### Retained Graph

- `UiTree`: nodes and parent-child links (structural ownership).
- `UiNode`: semantic widget kind, props, layout spec, style spec, watches (signal deps), focus/selection scopes.
- `UiPatch`: backend-neutral patch stream for semantic graph mutations (`SetProp`, `SetLayout`, etc).

### Runtime Systems

All are explicit structures inside `UiRuntimeSystems`:

- Reactivity: `signal_values`, `computed`, `scheduler`
- Transactions: `transactions` (with `id`, `touched_nodes`, `changed_signals`, `patch_count`, and `dispatched_commands`)
- Workspace geometry + tabs: `workspace_layout` (`roots`, `active_tabs`, persistence key)
- Focus + selection: `focus_graph` (`focused`, optional `traversal_edges`), `selection_model` (`primary`, `selected`)
- Commands: `command_registry` (source-owned descriptors + snapshot), `command_buffer` (pending/executed/rejections)
- Motion: `motion_policy` (mode, performance tier, capacitor, interaction flags)
- Overlays: `overlay_stack` (explicit overlay z-order + optional anchors)

## Execution Model

The runtime entrypoint is `UiRuntime` ([runtime_execution.rs](/M:/Code/Kain/crates/kain-ui/src/runtime_execution.rs)).

### Step Input/Output

- Input: `UiRuntimeStepInput` (events, signal updates, `delta_ms`, optional transaction label).
- Output: `UiRuntimeStepOutput`:
  - `tree_patches`: semantic graph mutations as `UiPatch`
  - `system_patches`: backend-neutral “runtime system” deltas (signals updated, focus/selection changes, active tab changes, animation advanced, external command dispatched)
  - `invalidation`: exact invalidated node set + scheduled entries (`UiInvalidationResult`)
  - `scheduler`: a coalesced scheduler report (bounded, explainable)

### Phase Ordering (Conceptual)

1. Route `UiRuntimeEvent` into commands via compiler-emitted `UiEventRoute` (no host heuristics).
2. Execute pending commands:
   - builtin runtime mutations apply through `UiTreeMutator` and update runtime systems
   - registered external commands are *dispatched*, not “implemented” in the host in secret
3. Apply signal updates with exact invalidation:
   - `signal -> watching nodes` (from `UiNode.watches`)
   - `signal -> computed dependents` (from `UiRuntimeSystems.computed`)
   - invalidated nodes are de-duplicated and scheduler entries are generated/coalesced
4. Advance animation playback only when `motion_policy.should_animate()` allows it.

Every mutation creates a `UiTransaction` entry so the patch stream can be audited without backend archaeology.

## Exact Invalidation (No Full-Root Repatching By Default)

`UiRuntime` builds an index:

- `UiSignalId -> [UiNodeId]` watchers (direct deps)
- `UiSignalId -> [UiComputed]` dependents (derived deps)

Signal updates only invalidate nodes that depend on the changed signals, and they schedule only the required phases (`UiSchedulerEntry`). This is the runtime-side “bounded, explainable invalidation” bar from the acceptance matrix.

## Spatial Verifiability (First-Class)

The runtime must expose enough spatial truth that tools and strong models can answer layout-correctness questions directly, for example:

- Which panel owns this node?
- Which tab well is active?
- Is this node outside its parent’s clipped region?
- What is the overlay order and what is each overlay anchored to?
- What is the focus traversal order for a scope?

### Geometry Snapshot API

`kain-ui` provides a backend-neutral spatial snapshot:

- `ui_solve_workspace_layout(tree, systems, viewport_size)` computes `UiResolvedLayout` (rects per node).
- `ui_compute_spatial_snapshot(tree, systems, viewport_size)` returns `UiSpatialSnapshot`:
  - `nodes`: rects + parent + `owner_panel`
  - `active_tabs`: tab-group selection
  - `overlays`: explicit z-order plus optional anchor target + rect
  - `containment_violations`: overflow-claimed parents whose children escaped bounds
  - `focus_traversal`: explicit or derived traversal order per scope

This is intentionally backend-neutral: it’s computed from semantic layout specs and runtime-owned state, not from native widget instance geometry.

### Anchors And Overlay Order

- `UiOverlayStack.entries` defines overlay z-order (`order`) and optional anchoring (`UiAnchorSpec { target, placement, offset_px, constraint }`).
- Spatial snapshots surface the anchor relationship and target geometry so correctness can be verified without host-specific placement rules.

## Resize Contract (Geometry Behavior Is Predictable)

The resize math must be verifiable and non-accumulating:

- size is computed as `initial_size + delta_from_start` (never `prev + delta`)
- clamps (`min_px`, `max_px`) are explicit

See `ui_resize_size_from_drag_start(...)` in [runtime_execution.rs](/M:/Code/Kain/crates/kain-ui/src/runtime_execution.rs) and its unit tests.

## Motion Policy (Named, Not Ad Hoc)

Motion is controlled by `UiMotionPolicy`:

- `mode`: `full | balanced | reduced`
- `performance_tier`: `warp | cruise | safe`
- interaction flags (`is_resizing`, `is_pointer_active`)
- derived `capacitor` state

Runtime playback (animation stepping) is gated by `motion_policy.should_animate()` so motion reduction is structural, not backend-local hacks.

## Commands (A Real Surface)

`UiCommandRegistry` mirrors the proven “source-owned registration + snapshot” model:

- commands register per source id, rebuild an explicit snapshot, and remain inspectable
- runtime executes only declared builtin mutations
- external commands are dispatched explicitly (`UiTransaction.dispatched_commands` and `UiRuntimeSystemPatch::ExternalCommandDispatched`)

This prevents “command palettes” from becoming backend inventions with hidden state.

## Implementation References

- Runtime executor and spatial snapshot: [runtime_execution.rs](/M:/Code/Kain/crates/kain-ui/src/runtime_execution.rs)
- Core semantic types and layout solver: [lib.rs](/M:/Code/Kain/crates/kain-ui/src/lib.rs)

