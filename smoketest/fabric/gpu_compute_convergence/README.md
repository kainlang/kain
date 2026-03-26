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

It also now has a native visual proof lane:

- `build_visual_exe.ps1` reruns the Fabric manifest, reads the newest session report, generates `generated/main.generated.kn`, and packages a native executable under `visual-native-app/`.
- `launch_visual_exe.bat` launches that packaged executable.
- `capture_visual_demo.ps1` launches the executable, captures a screenshot into `generated/fabric_gpu_visual_showcase.png`, and closes the app again.

Run:

```powershell
cargo run -p cli --bin kain -- fabric validate --manifest smoketest/fabric/gpu_compute_convergence/KAIN.fabric.toml
cargo run -p cli --bin kain -- fabric run --manifest smoketest/fabric/gpu_compute_convergence/KAIN.fabric.toml
powershell -ExecutionPolicy Bypass -File smoketest/fabric/gpu_compute_convergence/build_visual_exe.ps1
start smoketest/fabric/gpu_compute_convergence/visual-native-app/fabric-gpu-visual-showcase.exe
```
