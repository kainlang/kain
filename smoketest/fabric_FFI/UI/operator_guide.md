# UI Operator Guide

This is the working guide for the UI proof suite under `smoketest/UI`.

The folder is split on purpose:

- product-mode showcase surfaces prove authored software
- probe surfaces prove backend capability and inspectability
- foundation surfaces prove layout, theming, and chrome contracts

## Canonical Lanes

| Lane | What It Proves | Default Posture |
| --- | --- | --- |
| `theme_authoring_shell` | Theme tokens, variants, widget defaults, and text roles stay compiler-owned | foundation |
| `dock_layout_workbench` | Semantic layout lowering and resizable dock structure remain explicit | foundation |
| `surface_modes_gallery` | Surface-mode mappings vary by widget family without host-local hacks | foundation |
| `geometry_fixture` | Shader-canvas, packed font, and SPIR-V proof paths stay opt-in and inspectable | devtools / probe |
| `geometry_fixture` | Compute residency and native packaging are materialized as real proof artifacts | probe |
| `kinetic_ui_atlas` | Distinct product shells and hot-reload state preservation stay believable in practice | showcase / reload |
| `website_clone_signalcraft` | The native lane can ship a product-like editorial page instead of a debug shell | showcase |

## Hot Reload Loop

Use `kinetic_ui_atlas` as the current hot-reload proof surface.

The repeatable loop is:

1. Materialize the smoke with `build_native_exe.ps1`.
2. Launch the packaged app from `native-app/`.
3. Make a semantic edit in `showcase.kn`.
4. Re-run the materializer and relaunch.
5. Verify the layout still reads as the same authored workspace and that the preserved focus, selection, and tab state behave like a real product reload rather than a reset.

The current native app bundle already carries reload-preservation metadata in its generated output, so the proof is in the visible reload behavior plus the emitted runtime files, not in hidden host state.

## What To Inspect

- Product mode should open without runtime inspector chrome unless a lane explicitly asks for a probe surface.
- Reloadable shells should preserve stable identity for authored layout regions, not just render the same pixels after a clean restart.
- Distinct showcase lanes should look deliberately different. Editorial, operator, and workbench shells should not collapse into one generic host aesthetic.
- Verification should come from authored structure, runtime snapshots, and bundle output, not screenshot-only intuition.

## Commands

From `M:\Code\Kain`:

```bat
cmd /c smoketest\UI\build_all_native_exes.bat
cmd /c smoketest\UI\launch_all_native_exes.bat
```

Per lane:

```powershell
powershell -ExecutionPolicy Bypass -File smoketest/UI/kinetic_ui_atlas/build_native_exe.ps1
powershell -ExecutionPolicy Bypass -File smoketest/UI/kinetic_ui_atlas/launch_native_exe.ps1
```

```bat
cmd /c smoketest\UI\spv_ui_surface_probe\build_native_exe.bat
cmd /c smoketest\UI\spv_ui_surface_probe\launch_native_exe.bat
cmd /c smoketest\UI\website_clone_signalcraft\build_native_exe.bat
cmd /c smoketest\UI\website_clone_signalcraft\launch_native_exe.bat
```

## Operator Rule

If a surface is meant to be a product shell, keep devtools out of the default path.
If a surface is meant to be a probe, make that explicit in the doc, the launcher, and the visible UI.
