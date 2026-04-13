# Alpha Hot Reload Contract

- Owner lane: `Alpha`
- Scope: reload-safe runtime semantics for `kain-core` emission and `kain-ui` execution
- Status: handoff note for compiler/runtime integration

## Contract We Are Making True

The UI pipeline must preserve authored meaning across recompiles without backend-local invention.

The reload contract is:

- signals keep stable identities across reloads when their authored ids do not change
- derived values lower to runtime-executable expressions, not just dependency metadata
- workspace layout survives reload through compiler-owned layout identity, roots, and active-tab mapping
- focus, selection, overlay order, and motion state transfer through explicit runtime state
- patch application after reload is bounded and explainable, not a hidden full-tree reset
- spatial truth remains inspectable after reload through stable node identity and structure indices

## Compiler / Runtime Seam

`kain-core` owns authored truth:

- event routes
- command routes
- transaction labels
- signal declarations
- computed declarations
- workspace schema and layout intent
- reload identity aliases and contract payloads

`kain-ui` owns execution truth:

- retained node identity
- signal storage
- computed invalidation and recompute
- hot-reload transfer
- workspace/focus/selection/overlay state
- animation playback policy
- spatial snapshot and traversal queries

The seam is explicit: compiler output must name the identity and dependency facts the runtime needs. The runtime may reconcile and transfer state, but it must not guess at product meaning.

## Current Landed Slice

- authored `<computed>` declarations lower to runtime `UiComputed { writes_signal, expr, invalidates_nodes, scheduler_phase }`
- compiler-emitted event routes now carry stable route ids plus explicit command and transaction metadata
- `UiRuntime::reload(...)` applies hot-reload transfer as a first-class runtime operation and emits reload system patches instead of hiding the swap
- the realtime UI contract bundle now exposes computed, route, reload, focus, selection, overlay, motion, and workspace contract payloads for downstream consumers

## Validation Scenarios

Use these as narrow spec checks, not broad test runs:

1. Reload a tree where one component keeps the same identity key and one changes it. The stable node must preserve its signal-backed state and the changed node must not inherit it accidentally.
2. Update an authored `<computed>` so its runtime expression changes, then confirm the recompute path produces a new derived value and only the dependent nodes are invalidated.
3. Reload a workspace shell with tabs and docking. The same active tab should survive when its persistent identity is unchanged, and the fallback should be deterministic when it is not.
4. Reload a shell with focus, selection, and overlay state. The transfer report should show which scopes and overlays were preserved and which were intentionally dropped.
5. Query the spatial snapshot before and after reload. The same ownership and anchor relationships should remain inspectable from structure, not screenshots.

## Handoff Risk

The highest-risk edge is any place where runtime behavior still depends on inferred tree shape instead of emitted contract data. If a reload path needs a heuristic to decide ownership, it is still too weak for the target architecture.
