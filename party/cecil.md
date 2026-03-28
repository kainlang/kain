# Cecil

## Current Assignment
Complete the current kain-core truth-emission task in `crates/kain-core/src/ui.rs` and `crates/kain-core/src/realtime_app_bundle.rs`. Keep tests light.

## Changes Made
- Tightened event-route truth emission so contract JSON now carries `transaction_label` explicitly, while keeping the older `transaction` key as a compatibility alias.
- Mirrored that label under `ui.event.route.*.transaction_label` in session state, so downstream contract readers have a stable canonical key.
- Confirmed workspace schema emission is already wired through `ui.contract.workspace_schema.json` and included in the realtime contract bundle path.
- Added a root wave tracker at `M:\Code\Kain\party\TASKS.md` so the parallel push has one canonical ordering surface.

## Key Findings
- The computed lowering path is already contract-first: authored specs are resolved after the tree and session-state contract keys exist, then serialized into `ui.contract.computed_registry.json`.
- The most visible truth gap was naming drift in event-route contracts: runtime state used `transaction`, but the emitted JSON contract only exposed the shorter alias instead of the canonical transaction label field.
- `realtime_app_bundle.rs` already gathers UI contracts from session state and includes workspace layout / schema / route payloads when present; the bundle path is not the weak point.
- Legacy inference is still intentionally present via `ui_runtime_systems_from_tree`, but only as compatibility backfill when the authored contract marker is absent.

## Files Touched
- `M:\Code\Kain\crates\kain-core\src\ui.rs`
- `M:\Code\Kain\party\Cecil.md`

## Next Recommended Move
- Handoff remaining truth-layer gaps to the appropriate lane if any new divergence appears.
- Otherwise, treat this task as complete and let the room proceed from the canonical board.
