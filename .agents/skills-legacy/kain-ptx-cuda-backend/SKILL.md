---
name: kain-ptx-cuda-backend
description: Use when adding, changing, debugging, validating, or reviewing Kain's raw PTX/CUDA backend, `CompileTarget::Cuda`, derived PTX shader artifacts, or the NVIDIA Driver API runtime path.
---

# Kain PTX/CUDA Backend

Use this skill for work touching Kain's CUDA target surface. This lane is raw
PTX, not `.cu` transpilation: the compiler emits PTX text and the runtime loads it
with the NVIDIA Driver API when an installed driver is available.

## Ownership Map

- `crates/gpu/src/codegen_ptx.rs` owns PTX emission from typed compute shader ASTs.
- `crates/gpu/src/lib.rs` exports `generate_ptx`.
- `crates/kain-core/src/lib.rs` owns `CompileTarget::Cuda` and aliases `cuda`, `ptx`, and `nvptx`.
- `crates/kain-core/src/shader_artifact.rs` owns `ShaderArtifactFormat::Ptx`.
- `crates/kain-driver/src/lib.rs` wires target compilation and optional derived PTX shader-bundle sidecars.
- `crates/cli/src/gpu_artifacts.rs`, `crates/kain-build/src/workspace.rs`, and `crates/kain-omni/src/lib.rs` materialize `.ptx` outputs.
- `crates/kain-gpu-runtime/src/nvidia_ptx.rs` owns the Windows NVIDIA Driver API loader/launcher.
- `crates/gpu/z3` owns durable SPIR-V/PTX backend proofs.

## Invariants

- SPIR-V remains the canonical shader bundle payload. PTX is a derived compute-only peer output.
- Do not add an `nvcc` or CUDA Toolkit dependency to the normal path.
- The runtime path should load PTX from memory via the driver, not write `.cu`/`.ptx` temp files and shell out.
- Keep compute residency metadata compiler-owned; CUDA must consume existing shader/residency bundles rather than inventing a separate plan schema.
- The PTX emitter currently uses conservative `.version 7.8` and `sm_50` for broad driver JIT compatibility. Raise this only when emitted instructions or targets require newer PTX.
- Non-compute shaders are not PTX eligible in v1. Bundle derivation should skip with a reflection note instead of failing the canonical SPIR-V bundle.
- Runtime binding slots must be unique. Codegen and runtime both sort by binding
  slot before declaring/passing PTX kernel params, so duplicate slots are an ABI
  ambiguity and should be rejected early.

## Validation

Run the solver lane first when changing address math, builtin lowering, parameter layout, or runtime dispatch calculations:

```powershell
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\gpu --lane ptx
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\gpu --lane full
```

Then run focused Rust checks:

```powershell
cargo fmt -p kain-core -p gpu -p kain-driver -p cli -p kain-gpu-runtime -p kain-build -p kain-omni
cargo test -p gpu --test ptx_codegen --target-dir target\codex-ptx-tests -- --nocapture
cargo test -p kain-driver compile_shader_artifact_bundle --target-dir target\codex-ptx-tests -- --nocapture
cargo test -p kain-gpu-runtime ptx_dispatch_group_count_rounds_up --target-dir target\codex-ptx-tests -- --nocapture
cargo test -p kain-gpu-runtime nvidia_ptx_executor_can_launch_tiny_kernel_when_driver_is_available --target-dir target\codex-ptx-tests -- --nocapture
cargo check -p gpu -p kain-driver -p cli -p kain-gpu-runtime --target-dir target\codex-ptx-check
```

The tiny-kernel smoke skips cleanly when `NvidiaPtxExecutor::try_new()` cannot
initialize a CUDA Driver API context. If it runs, it proves in-memory PTX loading,
kernel launch, synchronization, and output-buffer copyback.

## Common Traps

- Do not confuse latest PTX ISA docs with the minimum practical emission target.
  A newer `.version` can silently exclude users on older but perfectly usable
  display drivers.
- Keep `StorageBuffer<Vec3>` style strides at 16 bytes. PTX and SPIR-V should
  agree on the shader memory contract.
- If a kernel behaves like parameters are swapped, inspect binding-slot ordering
  first. `emit_uniform_params` and `NvidiaPtxExecutor` must keep the same
  ascending-slot ABI.
- Scalar constructor calls like `Float(i)` must typecheck before codegen. If they
  regress, inspect `crates/kain-core/src/types.rs` before weakening PTX lowering.
- Do not commit generated `crates/gpu/z3/reports/` files unless explicitly needed.
