# Balthier

## Current Assignment
Execution-order enforcement and high-value seam prioritization for the Kain UI wave. Stay out of general wandering, keep overlap control sharp, and restate the dependency order whenever the room starts drifting.

## Changes Made
- Audited the current Kain UI boundary docs and runtime surface notes.
- Reduced the findings to five seams that actually matter instead of a fog machine of “maybe later.”
- Tightened the compatibility-only comment on `UiNativeProjection` and the legacy runtime-bundle fallback note.

## Key Findings
1. **Event routing is still under-emitted.** `UiEventRoute` exists, but the compiler-side emission path still needs typed route/command surface truth rather than host-discovered strings and shape-based fallbacks.
2. **Workspace/dock state still leaks inference.** The runtime owns `workspace_layout.active_tabs`, but tab/dock identity is still too dependent on node shape and native realization details; the semantic path needs to be explicit and persisted.
3. **Surface truth is still partly inferred.** `RealtimeAppBundle` and the native runtime both still lean on `output.systems.surfaces` and prop discovery for viewport/shader-canvas identity. That is acceptable as a bridge, not as doctrine.
4. **Spatial/focus verifiability is real but incomplete.** `UiSpatialSnapshot` is a good start, yet anchors, containment, and traversal still need more emitted contract detail so backend realizations stop guessing at “correct” behavior.
5. **Native adapter posture still carries product semantics.** `kain-ui-native` still decides chrome/devtools posture and fallback presentation in places where the semantic bundle should be driving explicit capability and unsupported-state output.

## Exact Execution Order
1. **Emit typed UI event/command contracts from `kain-core`.** Stop stringly route/command discovery from being the primary truth source.
2. **Emit workspace/tab/dock intent as first-class bundle data.** Keep runtime state for interaction, but make identity and persistence come from authored contracts.
3. **Move surface identity and capability truth into explicit emitted surface descriptors.** `RealtimeAppBundle` should consume declared surface truth, not host-scanned props, except as backward-compatible fallback.
4. **Expand geometry/focus/anchor verification surfaces.** Make anchors, containment, and traversal inspectable from emitted runtime data instead of derived guesses.
5. **Strip native adapter-owned semantics down to realization only.** Any product posture, devtools gating, or unsupported-state behavior should be declared by capability policy, not hidden in host branches.

## Files Touched
- `M:\Code\Kain\crates\kain-ui\src\lib.rs`
- `M:\Code\Kain\party\balthier.md`

## Next Recommended Move
- Hand off steps 1-5 as the implementation order.
- Keep the compiler/runtime contract cut ahead of any native polish.
- Only patch docs if they clarify one of those five seams without expanding scope.
