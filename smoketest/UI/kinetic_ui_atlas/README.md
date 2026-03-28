# Kinetic UI Atlas

`kinetic_ui_atlas` is the current showcase-grade native-ui smoke. It stresses the surface as a four-page tabbed desktop shell and acts as the best proof that reloadable authored UI can stay visually distinct without falling back to debug chrome.

What it shows:

- top-tab page switching through semantic `tab_group_id` pages
- theme tokens, variants, typography scales, spacing, and surface modes
- docked editor layouts with inspectors, trees, graphs, timelines, and dense control walls
- shader-backed canvas surfaces in a non-viewport shell
- a real `viewport3d` workspace page using the current native scene lane
- reload-preserving layout identity through the current generated bundle and runtime snapshot path

What to verify:

- product mode opens as a real shell, not a runtime inspector
- tab and dock identity survive materialize/relaunch cycles when the semantic layout is unchanged
- the four pages stay visibly distinct enough to read as editorial, motion/studio, viewport, and operator workspaces
- devtools are not required to understand or use the app during normal launch

Build the executable from `M:\Code\Kain`:

```powershell
powershell -ExecutionPolicy Bypass -File smoketest/UI/kinetic_ui_atlas/build_native_exe.ps1
```

Release build:

```powershell
powershell -ExecutionPolicy Bypass -File smoketest/UI/kinetic_ui_atlas/build_native_exe.ps1 -Release
```

Launch the packaged app:

```powershell
powershell -ExecutionPolicy Bypass -File smoketest/UI/kinetic_ui_atlas/launch_native_exe.ps1
```

Expected output:

- `smoketest/UI/kinetic_ui_atlas/native-app/kinetic-ui-atlas.exe`
- `smoketest/UI/kinetic_ui_atlas/native-app/generated/native_app_bundle.json`
- `smoketest/UI/kinetic_ui_atlas/native-app/state/runtime_snapshot.json`
