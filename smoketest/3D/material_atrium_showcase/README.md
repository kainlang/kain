# Material Atrium Showcase

`material_atrium_showcase` is the first premium-shell smoke for the new renderer-backend expansion work.

What it proves:

- a source-first native UI shell under `smoketest/3D/` powered by `kain-ui-native`
- a premium 3D viewport scene bound to the new `material_atrium` scene in `kain-3D`
- a UI-first presentation layer that frames the runtime around renderer backends instead of a bare viewer window
- a packaging flow that still goes through `kain build native-ui` instead of inventing a second app materializer
- a four-lane renderer matrix that names the current graphics backend roadmap directly in the shell
- a primitive-backed scene stack that uses the authored Kain 3D primitive library for the atrium massing
- a deterministic repo-local image generator that renders `material_atrium` through Kain's software compatibility lane and labels the current backend truth honestly

What you should see:

- a polished Qt-native shell with a hero atrium viewport
- four renderer cards for `bgfx`, `filament`, `diligent`, and the staged `the-forge` lane
- primitive stack cards and runtime notes around the viewport
- the `material_atrium` scene orbiting slowly in the center viewport

Current lane truth:

- `bgfx` is the first real vendor-backed backend lane in the runtime
- `filament` and `diligent` are named and staged behind the shared backend seam
- `the-forge` is staged as a fourth backend identity for the future bridge-first low-level renderer path
- the smoke shell itself is native-runtime backed through `kain-ui-native` and the Qt host path
- the native Win32 viewport now boots a renderer session and surfaces the requested backend, active backend, service key, vendor runtime, and compatibility executor diagnostics directly in the overlay
- the current visual example is intentionally generated through the Kain software compatibility renderer so the proof stays reproducible on Linux and Windows while the native viewport bridges continue to deepen

Build on Windows:

```powershell
powershell -ExecutionPolicy Bypass -File smoketest/3D/material_atrium_showcase/build_visual_exe.ps1
```

Build on Linux/macOS:

```bash
cargo run -p cli --bin kain -- build native-ui smoketest/3D/material_atrium_showcase/smoke.kn --app-name material-atrium-showcase --window-title "Kain Material Atrium Showcase" -o smoketest/3D/material_atrium_showcase/native-app
```

Launch on Windows:

```bat
launch_visual_exe.bat
```

Launch a specific backend on Windows:

```bat
launch_visual_exe.bat bgfx
```

Launch on Linux/macOS:

```bash
./launch_native_app.sh
```

Launch a specific backend on Linux/macOS:

```bash
./launch_native_app.sh bgfx
```

Generate the deterministic renderer matrix artifact on Windows:

```bat
generate_runtime_matrix.bat
```

Generate the deterministic renderer matrix artifact on Linux/macOS:

```bash
./generate_runtime_matrix.sh
```

Visual example:

- `material_atrium_visual_example.png` is now generated from the real, primitive-backed `material_atrium` scene in `crates/kain-3D` by `cargo run -p kain-3d --bin material_atrium_smoke`.
- The image is a deterministic software compatibility preview of the current runtime backend matrix, not a fake mockup.
- It is still not a direct native viewport capture from `bgfx`, `filament`, `diligent`, or `the-forge`, and the labels inside the image call that out explicitly.
- `generated/material_atrium_runtime_matrix.json` records the backend labels, scene metadata, and frame stats that produced the image.
