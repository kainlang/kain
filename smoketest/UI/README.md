# UI Smoke Tests

These smokes are the proof surface for Kain's semantic UI compiler lane.

Each smoke lives in its own folder so we can keep authoring patterns, runner scripts, and expectations isolated per case.

## Current Smokes

- `theme_authoring_shell`: bundle-authored theme blocks, widget variant maps, and text roles
- `dock_layout_workbench`: dock composition, width/height constraints, split ratios, and resizable rails
- `surface_modes_gallery`: widget-specific surface-mode mapping across panel, inspector, tree, graph, timeline, and viewport widgets
- `gpu_compute_surface_probe`: explicit `primary_compute` metadata packaged into a native UI smoke with shader bundle and residency sidecars
- `website_clone_signalcraft`: top navigation, hero landing layout, scrollable rails, and native mount motion

## Run Model

Each smoke folder contains:

- `smoke.kn`
- `run_test.bat`
- `run_interpret.bat`
- `run_all.bat`
- `build_native_exe.bat`
- `launch_native_exe.bat`

`test` proves the compiler and semantic lowering path.

`interpret` is the closer user-facing execution path for quick manual verification.

`build_native_exe.bat` materializes a native app project and builds a desktop `.exe`.

`launch_native_exe.bat` opens the built `.exe` and will build it first if needed.

From this folder you can also use:

- `build_all_native_exes.bat`
- `launch_all_native_exes.bat`
