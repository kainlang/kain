# Rikku

## Current Assignment
Audit semantic leaks in the Kain UI overhaul. Patch small obvious leaks where safe; otherwise return exact handoff points.

## Changes Made
- Patched `crates/kain-core/src/ui.rs` so HTML-like debug rendering no longer prints concrete event names in attribute strings.
- Event attrs now render as opaque `[event-route]` markers instead of name-bearing placeholder strings.

## Key Findings
- `attrs_to_props_map(...)` already keeps event attrs out of component prop maps; that leak is cut.
- `render_attr_to_string(...)` was still exposing event meaning through debug strings. That is now opaque.
- Remaining semantic-leak candidates are mostly intentional compatibility surfaces, especially:
  - `ui.state_signal.*` props in `record_component_state_signals`
  - `ui.signal.key.*` / `ui.signal.owner.*` session-state bridges
  - `overlay.node.*` compatibility ids in `ui_runtime_systems_from_tree`
  - hot-reload/session-state transfer keys such as `ui.reload.*`
- Those look like the next cut if the goal is to reduce stringly runtime leakage further without breaking the contract layer.

## Files Touched
- `M:\Code\Kain\crates\kain-core\src\ui.rs`
- `M:\Code\Kain\party\rikku.md`

## Next Recommended Move
- Audit `crates/kain-core/src/ui.rs` and `crates/kain-ui/src/lib.rs` for the remaining compatibility bridges above, then decide whether to keep them as explicit contract surfaces or replace them with structured metadata.
