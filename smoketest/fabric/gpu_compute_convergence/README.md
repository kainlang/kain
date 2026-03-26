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

Run:

```powershell
cargo run -p cli --bin kain -- fabric validate --manifest smoketest/fabric/gpu_compute_convergence/KAIN.fabric.toml
cargo run -p cli --bin kain -- fabric run --manifest smoketest/fabric/gpu_compute_convergence/KAIN.fabric.toml
```
