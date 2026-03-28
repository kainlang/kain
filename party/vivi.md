# Vivi

## Current Assignment
Attach owner / file / acceptance signal to the top 5 missing UI contracts.

## Changes Made
- Kept the missing-contract list narrowed to the highest-leverage UI semantics.
- Aligned the ranking to the current room directives: truth emission, runtime authority, and compatibility-only boundaries.

## Key Findings
1. Typed event routes remain the top missing contract.
2. Runtime-visible geometry, containment, anchors, and focus traversal still need stronger structure.
3. Reload/state transfer must preserve or intentionally drop focus, selection, overlays, and active tabs.
4. Widget registry depth still matters for LLM legibility and adapter mapping.
5. Compatibility-only paths must stay labeled so they do not become doctrine.

## Files Touched
- `M:\Code\Kain\party\vivi.md`

## Top 5 Gaps With Owner / File / Acceptance
1. Typed event routes
   - Owner: Cecil
   - File: `M:\Code\Kain\crates\kain-core\src\ui.rs`
   - Acceptance: event lowering emits typed route data with handler identity, target, phase, and optional command/transaction linkage; no string placeholder semantics.

2. Anchor intent / surface placement facts
   - Owner: Cecil
   - File: `M:\Code\Kain\crates\kain-core\src\ui.rs`
   - Acceptance: authored anchor zone/target survive into bundle-visible truth.

3. Spatial snapshot / containment / overlay truth
   - Owner: Cloud + Rikku
   - File: `M:\Code\Kain\crates\kain-ui\src\lib.rs`
   - Acceptance: runtime exposes explicit geometry, containment, overlay order, owner_panel, and anchor facts from structure alone.

4. Reload / state transfer authority
   - Owner: Cloud
   - File: `M:\Code\Kain\crates\kain-ui\src\runtime_execution.rs`
   - Acceptance: reload explicitly preserves or drops focus, selection, overlays, active tabs, and transaction continuity via runtime-owned transfer data.

5. Widget registry depth
   - Owner: Vivi
   - File: `M:\Code\Kain\docs\kainplan\ui_slate_x100\widget_registry_schema.md`
   - Acceptance: registry has enough semantic categories and capability references for LLMs/adapters to infer widget ownership, command surfaces, and fallback behavior.

## Next Recommended Move
- Let Cecil and Cloud land cuts first.
- Then let Tifa/Tidus normalize and merge the live output.
- Stay ready to refine the gap list if a more concrete contract emerges from code.
