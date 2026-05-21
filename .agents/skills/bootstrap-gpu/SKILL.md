---
name: bootstrap-gpu
description: >-
  Use when changing compiler, frontend, or selfhost GPU truth in `crates/gpu`,
  `crates/kain-core`, `crates/kain-driver`, or GPU artifact staging: shader
  and compute typing, SPIR-V or PTX emission, `CompileTarget::Cuda`,
  residency or bundle metadata, or compile-time validation. Do not use for
  `kain-gpu-runtime`, `runtime/native` graphics substrate, or authored shader
  demos.
---

# Bootstrap Gpu

Use this skill when the compiler-owned GPU pipeline changes: frontend eligibility, emitted shader artifacts, or bundle semantics.

## Trigger Surface

- `crates/gpu/src/codegen_spirv.rs`, `crates/gpu/src/codegen_ptx.rs`, and `crates/gpu/z3/**` for backend math, layout, constructor lowering, control flow, PTX parameter order, and storage-buffer bounds.
- `crates/kain-core/**` for shader and compute typing, `CompileTarget::Cuda`, and compile-time artifact model changes.
- `crates/kain-driver/**`, `crates/cli/src/gpu_artifacts.rs`, `crates/kain-build/src/workspace.rs`, and adjacent staging code when emitted SPIR-V or derived PTX bundle shape changes.

## Boundaries

- Co-trigger `runtime-gpu` when `crates/kain-gpu-runtime`, CUDA driver loading, Vulkan execution, or native graphics and compute substrate changes are required.
- Co-trigger `lang-gpu` when the deliverable is authored Kain shaders, compute kernels, or specimen blades.
- Co-trigger `package-vulkain` when the proof surface is `blades/vulkain*` rather than the compiler-owned backend itself.
- Co-trigger `tool-build-system` when Bazel sync, generated BUILD files, or packaging scripts must move with GPU compiler changes.

## Workflow

1. Treat `crates/gpu` as backend truth. Fix typed eligibility before loosening backend math to accept bad inputs.
2. Keep SPIR-V canonical. PTX is a derived compute-only peer output, not a separate semantic model.
3. Put layout, indexing, and parameter-order invariants in `crates/gpu/z3` before trusting green tests or benchmark rows.
4. Keep bundle metadata compiler-owned. If executors must adapt, co-trigger `runtime-gpu` instead of moving runtime policy into this skill.

## Validation Loop

```powershell
cargo test -p gpu --lib storage_buffer_stride_matches_vulkan_base_alignment_for_common_types --target-dir target\codex-bootstrap-gpu-lib -- --nocapture
cargo test -p gpu --test spirv_layout --target-dir target\codex-bootstrap-gpu-spirv -- --nocapture
cargo test -p gpu --test ptx_codegen --target-dir target\codex-bootstrap-gpu-ptx -- --nocapture
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\gpu --lane layout
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\gpu --lane ptx
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\gpu --lane full
```

If bundle or artifact staging changed, also run:

```powershell
cargo test -p kain-driver compile_shader_artifact_bundle --target-dir target\codex-bootstrap-gpu-driver -- --nocapture
cargo check -p gpu -p kain-driver -p cli -p kain-build --target-dir target\codex-bootstrap-gpu-driver
```

## Guardrails

- Do not add an `nvcc` or CUDA Toolkit requirement to the normal path.
- Do not let runtime executors redefine shader bundle semantics or binding-slot order.
- Keep Vulkan and PTX memory contracts aligned; `Vec3` storage-buffer stride remains 16 bytes, not 12.
