# Kinetic UI Atlas

`kinetic_ui_atlas` is a fresh native-ui smoke that stresses the current Kain UI surface as a four-page tabbed desktop shell.

What it shows:

- top-tab page switching through semantic `tab_group_id` pages
- theme tokens, variants, typography scales, spacing, and surface modes
- docked editor layouts with inspectors, trees, graphs, timelines, and dense control walls
- shader-backed canvas surfaces in a non-viewport shell
- a real `viewport3d` workspace page using the current native scene lane

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
