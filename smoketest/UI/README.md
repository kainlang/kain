# UI Smoke Tests

This folder is the proof surface for Kain's semantic UI compiler lane.

Each smoke lives in its own folder so we can keep authoring patterns, runner scripts, and expectations isolated per case. Product-mode smokes should read like authored software. Probe and devtools surfaces should stay opt-in and never become the default shell posture.

## Proof Surfaces

- `theme_authoring_shell`: compiler-owned theme blocks, widget variant maps, and text roles
- `qt_plasma_runtime_lounge`: Qt Quick host proof with deterministic screenshot capture and a Plasma-style control-deck shell
- `dock_layout_workbench`: dock composition, width and height constraints, split ratios, and resizable rails
- `surface_modes_gallery`: widget-specific surface-mode mapping across panel, inspector, tree, graph, timeline, and viewport widgets
- `spv_ui_surface_probe`: shader-canvas and SPIR-V proof lane for opt-in inspection and packaging verification
- `gpu_compute_surface_probe`: compute metadata and residency packaging proof for the native host lane
- `kinetic_ui_atlas`: multi-page editorial/operator/workbench shell and the current hot-reload/state-preservation proof
- `website_clone_signalcraft`: top navigation, hero landing layout, scrollable rails, and mount motion in a product-style shell

## Operator Flow

Use `build_all_native_exes.bat` when you want the baseline suite materialized together.
Use `launch_all_native_exes.bat` when you want the current packaged proof surfaces opened together.

Each smoke folder contains:

- `smoke.kn`
- `run_test.bat`
- `run_interpret.bat`
- `run_all.bat`
- `build_native_exe.bat` or `build_native_exe.ps1`
- `launch_native_exe.bat` or `launch_native_exe.ps1`

`test` proves compiler and semantic lowering paths.

`interpret` is the quickest user-facing execution path for manual inspection.

`build_native_exe.*` materializes a native app project and builds a desktop `.exe`.

`launch_native_exe.*` opens the built `.exe` and builds it first if needed.

For the operator-facing checklist, see [operator_guide.md](/M:/Code/Kain/smoketest/UI/operator_guide.md).
