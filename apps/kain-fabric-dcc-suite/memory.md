## 2026-03-29 - Runtime lane roster now projects from the authored registry

- Extended the Kain Fabric DCC shell so `config/runtime_lanes.json` now feeds a new `runtime_lane_registry_summary` metric in `state/runtime_snapshot.json` and `config/ui_shell.json`.
- The top shell now has a `Lane Roster` status item alongside the existing lane map / health signals, which makes the authored ownership matrix visible as registry-backed data instead of only as compact runtime codes.
- Updated `config/surfaces.json` notes so `runtime_lane_map` explicitly calls out the roster projection seam.
- Reran `scripts/materialize-session-state.ps1` and `scripts/materialize-shell.ps1` so `state/runtime_snapshot.json`, `state/session_document.json`, and `generated/main.generated.kn` stayed aligned.
- Clean extension seam: if the lane registry grows more owners or sub-lanes, the same registry summary slot can keep projecting the authored roster without giving the native shell semantic ownership.

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

## 2026-03-29 - Render room now carries richer preview and review state

- Extended the render session contract to carry `accumulation_profile`, `denoise_profile`, and `review_capture_profile` alongside the existing camera, render profile, lighting profile, and AOV set.
- Tightened the render command registry so the lounge commands speak in viewport-quality preview, pathtrace accumulation, denoise, delegate routing, lighting review, and review capture terms instead of generic preview language.
- Refreshed the render workbench copy so the render room reads like a real control surface for viewport preview, AOV review, and frame capture.
- The clean extension seam is still the same: keep render semantics in the session/config projections and let the native shell consume those projections rather than inventing its own render vocabulary.

## 2026-03-29 - Bridge contract constants centralized for mesh/topology seams

- Moved the canonical mesh contract, active edit target, imported payload, authored primitive, topology output, and topology-history ids/URIs plus report metadata into `native-app/src/bridge_contract.rs`.
- Updated `native-app/src/runtime_bridge.rs` to write the shared constants back into session/report state so the live bridge stops duplicating those ids as local literals.
- This reduces schema drift in the Fabric/runtime lane and keeps the native bridge behaving like a thin adapter instead of a parallel contract source.

## 2026-03-29 - Material lane got a painter/sampler polish pass

- Extended the material session contract with explicit smart-mask and scan-ingest profiles alongside the existing brush, UV, texel-density, and deformation fields.
- Tightened the lookdev workbench copy and tool shelf so the material lane reads more like layered paint + smart materials + sampler-style ingestion instead of just generic PBR authoring.
- The paint-runtime and export projections now emit richer receipts for smart masks, scan ingestion, channel-pack profiles, and runtime delivery targets.
- Clean seam: keep the richer painter contracts in session/config/projection files; the host should stay a projector, not the source of truth.

## 2026-03-29 - Asset intake now has a visible top-rail status

- Added `asset_ingest_status` and related summary/count fields to the projected runtime snapshot so ingest has a direct shell-facing seam instead of only living inside the ingest block.
- Surfaced a new `Asset Intake` metric in `config/ui_shell.json` so the top rail now shows source-id-first ingest status alongside lane ownership, bridge health, and render chain telemetry.
- Re-materialized `state/runtime_snapshot.json`, `state/session_document.json`, and `generated/main.generated.kn` after the shell update so the live projection stayed aligned.
- Clean extension seam: if ingest grows richer package, transcode, or lineage telemetry later, the same status slot can keep projecting it without giving the native shell semantic ownership.
