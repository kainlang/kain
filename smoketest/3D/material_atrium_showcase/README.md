# Material Atrium Showcase

`material_atrium_showcase` is the source-first 3D smoke for Kain's native runtime. The smoke source is embedded directly into the native launcher, the top bar is owned by Kain language code, and the shell treats `material_atrium` as a first-class runtime scene instead of a bundle example.

What it proves:

- a source-first native UI smoke under `smoketest/3D/` powered by `kain-ui-native`
- Kain-owned runtime state for backend selection, runtime-owner messaging, and scene mode switching
- a premium `viewport3d` scene bound to the authored `material_atrium` runtime profile
- a top bar that can switch the smoke mood between `bgfx`, `filament`, `diligent`, and `the-forge`
- a native-app launcher that includes `smoke.kn` directly instead of depending on a generated runtime bundle as the source of truth
- the current Qt shell as a compatibility host while the native viewport bridge deepens
- a Windows native viewport profile and geometry branch for `material_atrium` in the runtime itself
- a deterministic preview image sidecar that remains a fallback proof artifact, not the runtime truth

How to build:

Windows:

```powershell
powershell -ExecutionPolicy Bypass -File smoketest/3D/material_atrium_showcase/build_visual_exe.ps1
```

Linux/macOS:

```bash
cargo run -p cli --bin kain -- build native-ui smoketest/3D/material_atrium_showcase/smoke.kn --app-name material-atrium-showcase --window-title "Kain Material Atrium Showcase" -o smoketest/3D/material_atrium_showcase/native-app
```

How to launch:

Windows:

```bat
launch_visual_exe.bat
```

Linux/macOS:

```bash
./launch_native_app.sh
```

Pick a backend mood at launch:

```bash
./launch_native_app.sh bgfx
./launch_native_app.sh filament
./launch_native_app.sh diligent
./launch_native_app.sh the-forge
```

The Windows launcher accepts the same backend identifiers as its first argument.

Current lane truth:

- `bgfx` is the baseline lane that the smoke defaults to
- `filament`, `diligent`, and `the-forge` are staged backend identities in the top bar and runtime metadata
- the Qt shell is still the compatibility host, not the final live renderer surface
- the native Win32 viewport now recognizes `material_atrium` as a real scene profile and has a dedicated geometry branch for it
- Linux still lacks a fully native viewport host, so the current smoke remains a compatibility-hosted proof there

Visual example:

- `material_atrium_visual_example.png` is a deterministic preview artifact generated from the authored `material_atrium` scene
- it proves the scene composition and shell presentation path without claiming to be a live vendor-direct viewport capture
- `generated/material_atrium_runtime_matrix.json` records the backend labels, scene metadata, and frame stats used to produce it
