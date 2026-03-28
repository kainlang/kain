# Vivi

## Current Assignment
Turn the missing-contract list into implementation-ready cuts for UI Slate X100.

## Changes Made
- Reviewed the UI docs and current UI lowering/runtime shape.
- Ranked missing contracts by leverage for editor-grade UI and LLM legibility.
- Converted the gap list into an owner/file/acceptance format for the next wave.

## Key Findings
1. Typed event routes are the highest-value missing contract.
2. Geometry/containment/anchor facts need to be explicit and queryable.
3. Focus traversal and reload/state transfer should be structural, not inferred.
4. Widget registry depth is a major legibility lever for models and adapters.
5. Compatibility paths must remain visibly compatibility-only.

## Files Touched
- `M:\Code\Kain\party\vivi.md`

## Implementation-Ready Hit List
1. Typed event routes
   - Owner: Cecil
   - File: `crates/kain-core/src/ui.rs`
   - Acceptance: events lower to typed route/state data with route id, target, phase, handler id, and optional command/transaction linkage; no new event meaning via string props.

2. Geometry / containment / region facts
   - Owner: Cloud + Rikku
   - File: `crates/kain-ui/src/lib.rs`
   - Acceptance: runtime exposes explicit spatial snapshot facts for containment, ownership, anchors, and overlay order; verification no longer depends on renderer heuristics.

3. Reload / state transfer
   - Owner: Cloud
   - File: `crates/kain-ui/src/runtime_execution.rs`
   - Acceptance: reload preserves or explicitly drops focus, selection, overlays, active tabs, and transaction continuity through runtime-owned transfer data.

4. Anchor intent contracts
   - Owner: Cecil
   - File: `crates/kain-core/src/ui.rs`
   - Acceptance: authored anchor zone/target data survives lowering into bundle-visible truth, not backend-local placement guesses.

5. Widget registry depth
   - Owner: Vivi
   - File: `docs/kainplan/ui_slate_x100/widget_registry_schema.md`
   - Acceptance: registry describes enough semantic categories and capabilities for LLMs/adapters to reason about widget ownership, command surfaces, and fallback behavior.

## Next Recommended Move
- Let Tidus merge this into the global roster list.
- Let Tifa normalize all current outputs into one compact comparison artifact.
- Keep compatibility-debt work separate from the runtime-authority lane.
