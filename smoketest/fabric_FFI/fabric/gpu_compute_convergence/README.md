# Fabric GPU Compute Convergence Smoke

This smoke is the minimal end-to-end proof for:

- `python`
- `kain`
- `gpu_compute`
- `node`

The runtime flow is:

1. Python emits scalar settings.
2. Kain materializes `src` and `dst` shared buffers.
3. `gpu_compute` runs `FabricGpuCopy` through the Vulkan executor and writes the result into `dst`.
4. Node reads the resulting shared buffer through canonical Fabric contract projection and returns a summary string.

It also now has a minimal viewport-first native proof lane:

- `build_visual_exe.ps1` reruns the Fabric manifest, reads the newest session report, generates `generated/main.generated.kn`, and packages a minimal native viewport shell under `visual-native-app/`.
- The generated shell now treats the showcase as what it actually is: a Fabric-driven viewport proof with a narrow session HUD instead of a fake editor dashboard.
- `build_visual_exe.ps1 -Release` does the same thing through an isolated release cargo target dir so the smoke can produce a durable demo artifact without fighting the workspace `target/`.
- `launch_visual_exe.bat` launches `visual-native-app/fabric-studio-3d-editor.exe`.
- `capture_visual_demo.ps1 -Release` maximizes the native viewport window, captures the window bounds instead of the whole desktop, and writes `generated/fabric_gpu_visual_showcase.png`.

Run:

```powershell
cargo run -p cli --bin kain -- fabric validate --manifest smoketest/fabric/gpu_compute_convergence/KAIN.fabric.toml
cargo run -p cli --bin kain -- fabric run --manifest smoketest/fabric/gpu_compute_convergence/KAIN.fabric.toml
powershell -ExecutionPolicy Bypass -File smoketest/fabric/gpu_compute_convergence/build_visual_exe.ps1 -Release
start smoketest/fabric/gpu_compute_convergence/visual-native-app/fabric-studio-3d-editor.exe
powershell -ExecutionPolicy Bypass -File smoketest/fabric/gpu_compute_convergence/capture_visual_demo.ps1 -Release
```
