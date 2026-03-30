## 2026-03-29 - Render chain now shows up as first-class shell telemetry

- Added a `render_preview_chain` snapshot metric and surfaced it in `config/ui_shell.json` as a top-rail `Render Chain` status item with the authored `pathtrace -> accumulation -> denoise` spine.
- Threaded the metric through `scripts/materialize-session-state.ps1` and `scripts/materialize-shell.ps1` so `state/runtime_snapshot.json` and `generated/main.generated.kn` stay aligned with the render-first product stance.
- This gives the scaffold a more visible progressive-preview lane without pretending the host owns the render semantics.
- Clean extension seam: if the preview spine later grows a real accumulation or denoise runtime, the same metric slot can keep projecting the chain state.

## 2026-03-29 - Runtime lane signal now carries explanatory detail

- Extended the shell/runtime snapshot lane-health seam so `runtime_lane_health_detail` now rides alongside the concise `runtime_lane_health` value.
- Threaded that detail through `scripts/materialize-session-state.ps1`, `scripts/materialize-shell.ps1`, and `config/ui_shell.json`, which adds a second `Lane Signal` status item to the authored shell.
- This keeps the app feeling more like a live control room: operators see both the coarse health label and the bridge/fabric explanation without moving semantic ownership into the host.
- Reran `scripts/materialize-session-state.ps1` and `scripts/materialize-shell.ps1` so `state/runtime_snapshot.json`, `state/session_document.json`, and `generated/main.generated.kn` stayed aligned.
- Clean extension seam: richer bridge/runtime telemetry can keep flowing into the same status rail later without hardcoding lane truth in the native shell.

## 2026-03-29 - Native shell now reads canonical presentation hints from the runtime snapshot

- Extended `crates/kain-ui-native` so the native host recognizes `dcc_suite_state.presentation` as the source for fixed-workspace / centered-layout shell behavior instead of relying only on host-local app/theme heuristics.
- The DCC runtime snapshot already carries the presentation block from `native-app/src/runtime_bridge.rs`; the native UI now consumes it directly for topbar/inspector suppression and product-shell detection.
- This is a small but important drift cut: the shell keeps its chrome decisions closer to the projected runtime contract rather than inventing presentation semantics locally.

## 2026-03-29 - Bridge contract constants centralized for mesh/topology seams

- Moved the canonical mesh contract, active edit target, imported payload, authored primitive, topology output, and topology-history ids/URIs plus report metadata into `native-app/src/bridge_contract.rs`.
- Updated `native-app/src/runtime_bridge.rs` to write the shared constants back into session/report state so the live bridge stops duplicating those ids as local literals.
- This reduces schema drift in the Fabric/runtime lane and keeps the native bridge behaving like a thin adapter instead of a parallel contract source.
