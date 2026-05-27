---
name: runtime-gpu
description: Use when adding, changing, debugging, validating, or reviewing runtime-side GPU and graphics execution, especially `crates/gpu-runtime`, `runtime/native/include/graphics_*.h`, `renderer_*.h`, `runtime/conformance/graphics_runtime`, or native shader-bundle consumption. Not for SPIR-V/PTX codegen or blade-local Vulkan packages.
---

# Runtime Gpu

Read `ARCHITECTURE.md` and `MEMORY.md`, then separate runtime execution from compiler backend work before editing.

## Owns

- Runtime-side GPU executors in `crates/gpu-runtime/**`, including Vulkan consumption of shader bundles and the NVIDIA PTX executor.
- Native graphics/runtime ABI surfaces such as `runtime/native/include/graphics_bundle.h`, `graphics_system.h`, `renderer_backend.h`, and `renderer_session.h`.
- Runtime graphics conformance under `runtime/conformance/graphics_runtime/**`.

## Does Not Own

- SPIR-V or PTX emission in `crates/gpu/src/codegen_*`. Use `bootstrap-gpu`.
- Blade-local bridges, shaders, or package UX in `blades/vulkain` or `blades/kaintana-vulkan`. Use `package-vulkain` or `package-kaintana`.
- General runtime startup/service-table work unless the graphics runtime ABI itself changed. Co-trigger `runtime-core` if needed.

## Working Rules

- Keep shader-bundle production compiler-owned and keep bundle consumption runtime-owned.
- Preserve the contract that SPIR-V is the canonical portable payload and PTX is a derived compute path.
- When a runtime ABI or binding layout changes, validate the executor, the native graphics harness, and the nearest solver proof together.
- CUDA PTX launch policy is consumed from compute-residency metadata. `dynamic_shared_memory_bytes` maps directly to `cuLaunchKernel`, `cuda_stream_policy = "non_blocking"` creates a CUDA driver stream, and `cuda_graph_policy = "capture_once"` is represented but intentionally rejected until graph capture/instantiate/launch is implemented end-to-end.

## Validation

```powershell
cargo check -p kain-gpu-runtime --target-dir target\codex-runtime-gpu
cargo test -p kain-gpu-runtime ptx_dispatch_group_count_rounds_up --target-dir target\codex-runtime-gpu -- --nocapture
cargo test -p kain-gpu-runtime ptx_dispatch_plan_reads_cuda_launch_policy -- --nocapture
cargo test -p kain-gpu-runtime nvidia_ptx_executor_can_launch_tiny_kernel_when_driver_is_available --target-dir target\codex-runtime-gpu -- --nocapture
bash runtime/conformance/graphics_runtime/run_tests.sh --verbose
mcp__z3_local__.run_proof_pack(path="runtime/native/src/core", lane="graphics")
```
