# Tidus

## Current Assignment
Merge captain for the Kain UI swarm. Build the global task list from the roster outputs, dedupe overlap, and order work by dependency.

## Changes Made
- Reviewed the current party outputs from Cloud, Balthier, Cecil, Barret, Sazh, and Zidane.
- Collapsed the repeated themes into one dependency-ordered task spine.
- Kept the merge focused on Kain UI overhaul work only.

## Key Findings
- The work converges on one core sequence: compiler-owned truth first, then runtime ownership, then compatibility cleanup, then proof.
- `kain-core` is the source of semantic truth; `kain-ui` owns runtime behavior and state authority.
- Compatibility paths are acceptable only when they are clearly labeled legacy-only bridges.
- The biggest recurring risk is inference leaking into places that should be explicitly emitted contracts.
- One real bug already landed: transaction labels must stay attached to event-route truth, not vanish into side effects.

## Files Touched
- `M:\Code\Kain\party\Tidus.md`
- Reviewed:
  - `M:\Code\Kain\party\cloud.md`
  - `M:\Code\Kain\party\balthier.md`
  - `M:\Code\Kain\party\cecil.md`
  - `M:\Code\Kain\party\Barret.md`
  - `M:\Code\Kain\party\sazh.md`
  - `M:\Code\Kain\party\zidane.md`

## Global Task List
1. **Lock compiler-owned UI truth in `kain-core`.**
   - Emit typed event/command routes as first-class contract data.
   - Keep transaction labels on route truth.
   - Emit workspace/layout contract blobs and any missing authored truth directly from core.
   - Resolve computed lowering / workspace emission gaps before anything else.

2. **Make runtime ownership explicit in `kain-ui`.**
   - Keep workspace/tab/dock identity and persistence in runtime-owned state, not inferred tree shape.
   - Tighten focus, selection, overlay, anchor, containment, and traversal surfaces so they are inspectable instead of guessed.
   - Preserve runtime invalidation / reload / patch authority as runtime behavior, not backend posture.

3. **Label every compatibility bridge as legacy-only.**
   - `UiNativeProjection`, `ui_runtime_systems_from_tree(...)`, and similar fallback paths must stay visibly subordinate.
   - Native adapter behavior should realize contracts, not define product semantics.
   - Remove or quarantine any branch where the bridge looks canonical but behaves like a source of truth.

4. **Eliminate inference leaks across the UI stack.**
   - Replace stringly / host-discovered / shape-discovered semantics with explicit emitted descriptors.
   - Remove backend chrome, devtools gating, and product posture from adapter-owned decision points.
   - Keep surface identity, capability truth, and unsupported-state behavior in authored contracts.

5. **Keep the migration safe with a minimal validation spine.**
   - Validate packaged shader-canvas/runtime snapshot behavior first.
   - Then check product-mode shell contamination, semantic tabs/docking, hot reload compatibility, runtime focus parity, and graphics reset behavior.
   - Treat the regression matrix as proof, not as a place to redesign the system.

## Merge Note
The swarm is aligned on the same dependency chain, so the duplicate output mostly compresses cleanly: truth emission first, runtime authority second, compatibility bridges third, proof last. The only concrete code-level fix already surfaced is the transaction-label preservation in event routing; everything else should stay ordered behind the compiler/runtime contract cut.

## Next Recommended Move
Publish this dependency-ordered task list back to the swarm, then hand implementation lanes to the appropriate owners without reopening architecture already agreed by the group.