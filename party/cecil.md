# Cecil

## Role
Architecture, contracts, and truth-layer review.

## Working Notes
- Current focus: the UI system is intended to be compiler-owned, highly expressive, and legible to LLMs.
- Priority: identify where meaning is still inferred, flattened, or host-owned.
- Keep the distinction clear between authored semantics, runtime graph behavior, and backend realization.

## Files / Sources Reviewed
- docs/kainplan/ui_slate_x100/target_architecture.md
- docs/kainplan/ui_slate_x100/current_state_map.md
- docs/kainplan/ui_slate_x100/widget_registry_schema.md
- docs/kainplan/ui_slate_x100/authoring_contract.md

## Key Takeaways
- The strongest direction is explicit contracts: widget registry, command registry, workspace schema, motion/paint registries, and structure-indexed verification.
- Legacy inference paths should remain compatibility-only.
- Spatial verifiability matters as much as visual polish.
- The system should be evaluated by whether a strong model can answer ownership and placement questions from structure alone.

## Risks to Watch
- `ui_runtime_systems_from_tree` becoming the de facto truth again.
- `UiNativeProjection` drifting upward into a canonical ABI.
- Any backend injecting semantics, chrome, or posture instead of realizing contracts.

## Suggested Next Move
- Hard audit of compiler emission vs runtime inference vs adapter realization.
- Define the minimum contract set required for Slate-class authoring and LLM-legible UI.
- Build regression tests for wrong-region detection, tab ownership, anchor binding, and command routing.
