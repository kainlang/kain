## 2026-03-29 - Native UI Shell Reset Toward Docked Workstation

- Started a cleanup pass on the shell presentation contract for the new native UI path.
- `config/ui_theme.json` now turns on the top bar and inspector chrome and tightens spacing/radius/typography so the frame reads less like a web page and more like a mounted DCC workstation.
- `config/ui_shell.json` was nudged further toward workstation language, with the brand summary and operator notes emphasizing docked regions, anchored rails, and no feed-like navigation.
- `scripts/materialize-shell.ps1` was run again so `generated/main.generated.kn` reflects the updated authored shell contract.
- Next seam: if the app still feels wrong in the renderer, the remaining work is in the native UI component tree and layout bindings rather than the authored manifest alone.
## 2026-03-29 - UI Overhaul Pushed Toward DCC Cockpit Language

- Reframed the shell chrome toward classic DCC ergonomics: viewport-centered middle, outliner/tool rail left, attributes/inspectors right, and status/timeline/jobs bottom.
- Renamed the top chrome language in `config/ui_shell.json` toward a `DCC Shell` / `Command Launcher` / `Outliner Rail` / `Attributes` vocabulary so the authored contract stops reading like a generic app dashboard.
- Kept `session/ui_workbench_registry.kn` aligned with the shell renames so the authored workbench descriptors still match the native bundle materialization path.
- Regenerated `generated/main.generated.kn`, `state/runtime_snapshot.json`, and `state/session_document.json` after the manifest edits.
- Attempted `scripts/build-native-ui.ps1`, but the underlying Fabric run failed in `sculpt_brush_projection` and `topology_history_projection` (parser error in sculpt brush projection; missing `mesh_resource_contract_document` field in topology history), so the native bundle did not fully finish from this pass.
- Next seam: fix the Fabric blockers separately, then re-run the native-ui build to verify the new shell actually lands end-to-end.
