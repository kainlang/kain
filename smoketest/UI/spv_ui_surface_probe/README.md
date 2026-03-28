# SPV UI Surface Probe

This smoke is the opt-in verification surface for Kain's shader-canvas UI lane.

It deliberately combines:

- a semantic UI shell authored in Kain
- a real `<canvas>` surface that resolves through `RealtimeAppBundle.shader_canvases`
- a fragment shader emitted as SPIR-V and consumed through the native shader bundle lane
- a packaged relative `font_asset` that the native host turns into a packed MSDF atlas texture
- a fragment preview that visibly samples the runtime-provided atlas instead of faking the path with CPU text drawing

What this smoke proves now:

- Kain can author a shader-canvas surface directly in UI schema instead of falling back to a viewport placeholder
- native app materialization resolves the smoke's relative `font_asset` from the authored source root and packages it beside the executable
- the native host provisions the packaged font into the shader-canvas atlas cache and exposes it as a shader-readable texture
- the shader-canvas executable path remains SPIR-V-canonical even while the current native host consumes WGSL/WGPU derivatives

What this smoke still does **not** prove:

- retained widget interaction fully round-tripping from GPU-authored state back into the semantic UI runtime
- final editor-grade text layout, selection, caret, and shaping behavior
- a Vulkan-first raw-SPIR-V host that bypasses the current WGSL/WGPU bridge

Run:

```powershell
run_all.bat
build_native_exe.bat
launch_native_exe.bat
emit_gpu_artifacts.bat
```

Notes:

- `build_native_exe.bat` is the supported executable path for this smoke because it stages `assets/ui_smoke_default.ttf` from the local Windows font directory if the smoke asset is missing.
- the schema visual for this smoke lives in `docs/shader_canvas_ui_schema.svg`
- this lane is for verification, not product-mode default shell posture

Artifact inspection:

- `emit_gpu_artifacts.bat` writes a `.spv`, Rust host wrapper, reflection JSON, and shader bundle JSON into `generated/spv_ui_surface_probe.*`
- `build_native_exe.bat` materializes `native-app/` with the packaged realtime font asset, native UI bundle, realtime bundle, and shader bundle sidecars

## Output Hygiene

- `native-app/` is disposable and should not stay checked in.
- `generated/spv_ui_surface_probe.*` is disposable probe output unless you deliberately archive it under `docs/validation/` or `docs/recent/`.
