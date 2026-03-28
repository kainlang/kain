# Barret.md

## Current Assignment
Own proof and regression shape for the Kain UI overhaul.

## Changes Made
- Tightened the post-landing validation shape down to the minimum non-test matrix that still covers the risky UI surfaces.
- Matched each risk area to an exact repo file or harness instead of vague “run the UI tests” advice.
- Kept the plan biased toward fast conformance and smoke coverage, not heavy end-to-end grind.

## Key Findings
- The UI overhaul’s real truth surface is split across `crates/kain-ui/src/lib.rs`, `crates/kain-ui/src/runtime_execution.rs`, and the native projection path in `crates/kain-ui/tests/ui_runtime_native_projection_parity.rs`.
- Hot reload needs to prove both state transfer and layout preservation, so `runtime/conformance/hot_reload` and the hot-reload tests in `crates/kain-ui/src/lib.rs` are the core checks.
- Focus, selection, overlays, and event routing already have direct coverage in `runtime/conformance/ui_runtime/test_ui_runtime_focus.c` plus the UI runtime Rust tests in `crates/kain-ui/src/lib.rs`.
- The minimal matrix should cover reload, tabs/docking, focus, selection, overlays, event routing, and computed invalidation once each — no duplicate hero runs.

## Files Touched
- M:\Code\Kain\party\Barret.md

## Next Recommended Move
- After the code wave lands, run this minimal validation set in order:
  1. `runtime/conformance/hot_reload` — `test_hot_reload_compatibility.c`, `test_hot_reload_lifecycle.c`, plus `run_tests.sh`
  2. `runtime/conformance/ui_runtime` — `test_ui_runtime_focus.c`, `test_ui_runtime_parity.c`, `test_ui_runtime_bundle.c`, plus `run_tests.sh`
  3. `runtime/conformance/graphics_runtime` — `test_graphics_runtime_smoke.c`, `test_graphics_runtime_binding_rules.c`, plus `run_tests.sh` for reload-reset sanity
  4. `crates/kain-ui/tests/ui_runtime_native_projection_parity.rs` — canonical/native projection parity guard
  5. `crates/kain-ui/src/lib.rs` unit tests around `workspace_layout_snapshot_preserves_active_tab_state`, `workspace_layout_solver_and_snapshot_round_trip`, and `hot_reload_transfer_preserves_runtime_state`
  6. `crates/kain-ui/src/runtime_execution.rs` — event routing + computed invalidation path exercised through the Rust unit suite in `crates/kain-ui/src/lib.rs`

## Validation Matrix
- Reload: `runtime/conformance/hot_reload/{test_hot_reload_compatibility.c,test_hot_reload_lifecycle.c}` and `crates/kain-ui/src/lib.rs::hot_reload_transfer_preserves_runtime_state`
- Tabs/docking: `crates/kain-ui/src/lib.rs::workspace_layout_snapshot_preserves_active_tab_state` and `::workspace_layout_solver_and_snapshot_round_trip`
- Focus: `runtime/conformance/ui_runtime/test_ui_runtime_focus.c`
- Selection: `crates/kain-ui/src/lib.rs::hot_reload_transfer_preserves_runtime_state` and the selection-transfer assertions inside `ui_transfer_hot_reload_state`
- Overlays: `crates/kain-ui/src/lib.rs::hot_reload_transfer_preserves_runtime_state` plus overlay-transfer code in `ui_transfer_hot_reload_state`
- Event routing: `runtime/conformance/ui_runtime/test_ui_runtime_focus.c` and `crates/kain-ui/src/lib.rs::ui_runtime` coverage
- Computed invalidation: `crates/kain-ui/src/lib.rs` tests around invalidated node reporting, driven by `crates/kain-ui/src/runtime_execution.rs`
- Canonical/native parity: `crates/kain-ui/tests/ui_runtime_native_projection_parity.rs` and `runtime/conformance/ui_runtime/test_ui_runtime_parity.c`

## Follow-up Tasks
- Merge every agent’s output into one de-duplicated global task list.
- Keep the matrix minimal, not ceremonial.
- Don’t turn host-only behavior into doctrine; prove it in the real runtime lanes.
