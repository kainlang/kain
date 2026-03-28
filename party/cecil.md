# Cecil

## Current Assignment
Audit kain-core truth emission in `crates/kain-core/src/ui.rs` and `crates/kain-core/src/realtime_app_bundle.rs`, then patch gaps in computed lowering, event-route contracts, transaction labels, workspace schema, or contract bundle emission. Keep tests light.

## Changes Made
- Tightened event-route truth emission so contract JSON now carries `transaction_label` explicitly, while keeping the older `transaction` key as a compatibility alias.
- Mirrored that label under `ui.event.route.*.transaction_label` in session state, so downstream contract readers have a stable canonical key.
- Confirmed workspace schema emission is already wired through `ui.contract.workspace_schema.json` and included in the realtime contract bundle path.

## Key Findings
- The computed lowering path is already contract-first: authored specs are resolved after the tree and session-state contract keys exist, then serialized into `ui.contract.computed_registry.json`.
- The most visible truth gap was naming drift in event-route contracts: runtime state used `transaction`, but the emitted JSON contract only exposed the shorter alias instead of the canonical transaction label field.
- `realtime_app_bundle.rs` already gathers UI contracts from session state and includes workspace layout / schema / route payloads when present; the bundle path is not the weak point.
- Legacy inference is still intentionally present via `ui_runtime_systems_from_tree`, but only as compatibility backfill when the authored contract marker is absent.

## Files Touched
- `M:\Code\Kain\crates\kain-core\src\ui.rs`
- `M:\Code\Kain\party\Cecil.md`

## Next Recommended Move
- Add a small regression test for event-route contract emission, especially canonical `transaction_label` preservation and compatibility alias behavior.
- If more truth-layer work is desired, inspect computed contract output for any similar naming drift or missing canonical fields before touching runtime inference.
