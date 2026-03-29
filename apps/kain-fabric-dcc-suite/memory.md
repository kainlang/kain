## 2026-03-29 - Default Inspector Chrome Removed From Product Theme

- Flipped `config/ui_theme.json` so the inspector chrome is no longer authored as visible by default in the product shell.
- This closes the gap where the native runtime could still reintroduce the right-rail debug surface even after the codepath was made conditional.
- The main remaining risk is that generated shell artifacts may still need to be rematerialized so the runtime snapshot and authored theme stay aligned.

## 2026-03-29 - UI Devtools Hidden By Default In Product Shell

- Reduced the native UI’s tendency to present itself like a debug console by making the right-rail runtime devtools contingent on an explicit runtime setting in product-shell mode.
- This should cut down the blocky, inspector-heavy feel that was crowding out the actual DCC frame and should also reduce some of the obvious UI noise/perf drag from rendering the full debug tree and patch stream by default.
- Next seam: if the native viewport still isn’t dominant after this, the renderer needs a deeper pass so the center viewport becomes the primary surface and the devtools become an opt-in maintenance view only.

## 2026-03-29 - UI Shell Pushed Toward A Real Docked DCC Workstation

- Tightened the authored UI shell language so the native UI can read the app as a mounted workstation frame instead of a generic page.
- Strengthened `config/ui_shell.json` operator notes to explicitly reject card/feed behavior, discourage scroll as the default, and emphasize mechanically docked viewport/rail/inspector/status regions.
- Adjusted `session/ui_workbench_registry.kn` hero summaries so the core Scene, Model, and Lookdev workbenches now describe a docked viewport frame rather than a page-like layout.
- Next seam: if the native renderer still feels webby after this, the actual UI component tree needs to bind harder to the docked-region contract instead of only consuming the authored metadata.

## 2026-03-29 - Native Presentation Enforced Toward Docked Workstation Feel

- Pushed the app-owned presentation contract toward a fixed workstation frame instead of a scrollable page model.
- `config/app_manifest.json` now declares a fixed docked workspace presentation with locked regions and document-flow disabled.
- `config/ui_shell.json`, `config/surfaces.json`, and `session/ui_workbench_registry.kn` were tightened so the authored shell/workbench frame keeps the viewport centered and the rails anchored left/right/bottom/top.
- `native-app/src/bridge_contract.rs`, `native-app/src/main.rs`, and `native-app/src/runtime_bridge.rs` now expose presentation env vars and mirror a `dcc_suite_state.presentation` block into the live runtime snapshot.
- Next seam: if the host UI still scrolls like a page, wire the native UI renderer to these new presentation hints rather than letting layout drift back into document flow.

## 2026-03-29 - Asset Pipeline Manifest Reached The Model And Render Benches

- Surfaced `asset_pipeline_manifest` on the model, lookdev, and render workbenches in both `config/ui_shell.json` and `session/ui_workbench_registry.kn`, so the source-id-first intake policy is visible wherever operators jump between topology, materials, and render review.
- Regenerated `generated/main.generated.kn`, `state/runtime_snapshot.json`, and `state/session_document.json` after the registry edit so the live bridge artifacts stayed aligned with the authored shell/session data.
- The app still treats the ingest lane as an authored routing contract rather than a native interchange runtime; the clean extension seam is still a real importer/residency bridge with typed lineage receipts.
