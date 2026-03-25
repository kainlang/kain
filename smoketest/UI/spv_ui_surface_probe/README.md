# SPV UI Surface Probe

This smoke is the honest first probe for "SPV-based UI" inside the current Kain repo.

It deliberately combines:

- a semantic UI shell with a `viewport3d` placeholder surface
- an authored `shader compute` item intended to behave like a procedural UI canvas
- explicit `comptime` compute metadata so the shader emits SPIR-V plus runtime sidecars
- a direct `gpu-artifacts` helper so the `.spv` can be inspected without building the full native app lane first

What this smoke proves today:

- Kain can author a UI-facing smoke that also emits SPIR-V artifacts
- the compute plan can describe a procedural "surface" contract instead of a pure tensor math demo
- native UI packaging can stage shader/runtime sidecars beside a desktop executable

What this smoke does **not** prove yet:

- a real fullscreen-quad host that samples the SPIR-V output as the primary widget renderer
- live pointer routing from the native host into the shader
- text rendering via font atlas sampling
- retained widget state fed back from GPU execution into the UI host

Run:

```powershell
run_all.bat
build_native_exe.bat
launch_native_exe.bat
emit_gpu_artifacts.bat
cargo run -q -p cli --bin kain -- smoketest/UI/spv_ui_surface_probe/smoke.kn -t interpret
cargo run -q -p cli --bin kain -- smoketest/UI/spv_ui_surface_probe/smoke.kn -t test
```

Artifact inspection:

- `emit_gpu_artifacts.bat` writes a `.spv`, Rust host wrapper, reflection JSON, and shader bundle JSON into `generated/spv_ui_surface_probe.*`
- `build_native_exe.bat` materializes `native-app/` with the runtime-side packaging lane

## Output Hygiene

- `native-app/` is disposable and should not stay checked in.
- `generated/spv_ui_surface_probe.*` is disposable probe output unless you deliberately archive it under `docs/validation/` or `docs/recent/`.
