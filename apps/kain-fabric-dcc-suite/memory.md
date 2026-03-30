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

## 2026-03-29 - Native UI build stayed green after viewport promotion cuts

- Rebuilt `apps/kain-fabric-dcc-suite` native UI after the viewport-promotion / slot-kill cut landed.
- The only code drift needed for the build was in `crates/kain-ui-native/src/lib.rs`: two `let`-chain conditions were rewritten as nested `if let` checks so the crate stays compatible with the repo's current Rust edition.
- Validation passed: `apps/kain-fabric-dcc-suite/native-app/kain-fabric-dcc-suite.exe` was rebuilt and synced successfully.
- Build still emits a lot of pre-existing warnings in `ue5-materials`, `ue5-graphs`, `ue5-gas`, and `cli`, but no new errors.
