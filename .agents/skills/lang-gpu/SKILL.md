---
name: lang-gpu
description: >-
  Use when authoring, explaining, reviewing, or repairing Kain-side GPU and rendering code: shader vertex/fragment/compute items, `uniform ... @N` bindings, StorageBuffer compute kernels, `std::gpu` resource policies, `std::graphics` command-recording sessions, `std::graphics::shared` resource views, GPU artifact generation, SPIR-V/HLSL/PTX sidecars, Kain host orchestration around graphics/compute work, and semantic blends using world, entangle, pulse, collapse, converge, axiom, shatter, or orchestrate. Use this when writing IN Kain; use bootstrap/runtime/package skills when changing backend emitters, executors, native graphics ABI, or Vulkain bridge internals.
---

# Lang GPU

This is the authored rendering and GPU pipeline field manual. Use it when Kain source is expressing shaders, graphics resources, compute kernels, render loops, GPU artifact flows, or Kain semantics around GPU work.

## Prime Directive

- Keep GPU intent in Kain when the authored surface is the feature. Kain should own shader programs, resource policy, frame cadence, semantic state, and validation shape.
- Do not flatten Kain into GLSL cosplay. Use `shader`, `world`, `entangle`, `pulse`, `collapse`, `observe`, `decay`, `converge`, `axiom`, `shatter`, and `orchestrate` when they describe the real rendering system.
- Separate the two pipelines: CPU/native host code goes through LLVM and runtime ABI calls; GPU shader code goes through SPIR-V artifact generation.
- Do not claim the generic native graphics ABI is a full Vulkan/D3D renderer today. It records graphics sessions, buffers, shaders, meshes, pipelines, frames, and draw commands; direct Vulkan/D3D executors are declared but not attached in that C layer yet.
- Use package bridges such as Vulkain when you need an actual presentable Vulkan window today; keep package-local Vulkan details in `package-vulkain`.
- If a shader compiles under `kain gpu-artifacts` but host execution fails, debug the host bridge/runtime lane. If SPIR-V generation fails, debug shader authoring first, then escalate to `bootstrap-gpu`.

## Fast Operator Loop

```powershell
rg -n "\bshader (vertex|fragment|compute)\b|StorageBuffer|uniform .* @|std::gpu|std::graphics|graphics_shared" library_of_kain blades benchmark smoketest stdlib
python query_stdlib.py --module gpu --limit 80
python query_stdlib.py --module graphics --contains shader --limit 40
python query_stdlib.py --module graphics::shared --limit 40
kain check <kernels.kn> --target spirv
kain build <kernels.kn> --target spirv -o .kain/gpu/<pipeline>/<kernel>.spv
kain gpu-artifacts <kernels.kn> --output .kain/gpu/<pipeline>/<kernel>.spv
spirv-val --target-env vulkan1.3 .kain/gpu/<pipeline>/<kernel>.spv
kain check <host.kn-or-blade> --target llvm
python benchmark/run_gpu.py --case vec3_storage_copy --languages kain,cpp --no-run --runs 1 --warmups 0 --timeout 300
```

Best first examples:

- `benchmark/gpu/cases/vec3_storage_copy/kain.kn`: minimal `StorageBuffer<Vec3>` compute kernel with count/width guard.
- `benchmark/gpu/cases/semantic_ping_pong/kain.kn`: golden nested-branch, loop-heavy SPIR-V compute row with Vulkan parity evidence.
- `library_of_kain/gpu_semantic_ping_pong.kn`: authoring-library copy of the semantic ping-pong lane.
- `blades/kain-labs/src/kernels.kn`: multi-kernel compute set with particle/fluid/composite kernels.
- `benchmark/cases/gpu_graphics_submit/main.kn`: low-level `std::graphics` command recorder loop.
- `library_of_kain/gpu_graphics_submit.kn`: compact graphics submit copy.
- `library_of_kain/kloner_scene.kn`: authored Kain scene packet over Vulkain plus `std::math`.
- `blades/vulkain/src/vulkain.kn`: package-local Vulkan presentation facade; use with `package-vulkain`.

## Pipeline Map

Kain rendering has three pipelines that meet at runtime:

```text
Kain .kn source
-> kain-core parser / AST / typecheck / shader metadata / compute metadata
-> split

CPU/native host lane:
  CompileTarget::Llvm
  -> crates/sys-codegen
  -> LLVM IR
  -> native runtime ABI calls
  -> std::graphics, std::gpu policy use, stdlib calls, worlds, actors, pulse, ownership, @extern bridges

SPIR-V shader lane:
  CompileTarget::Spirv
  -> crates/gpu
  -> canonical SPIR-V bytes through rspirv
  -> runtime/package executor consumes artifacts

CUDA/PTX native lane (FIRST-CLASS, not derived):
  CompileTarget::Cuda
  -> crates/gpu
  -> direct PTX emission via codegen_ptx
  -> NVIDIA Driver API JIT-load + launch via kain-gpu-runtime
  -> supports fused kernels, warp intrinsics, bitpack dot-product, shared memory
  -> NO spirv roundtrip, NO hlsl derivation — pure CUDA blood
```

Target selection via `kain gpu-artifacts --target <name>`:

| `--target` | Primary artifact | Sidecars | Residency |
| --- | --- | --- | --- |
| `all` (default) | SPIR-V + derived PTX/HLSL | .gpu.rs, .reflect.json, .shader_bundle.json | yes |
| `spirv` / `vulkan` | SPIR-V only | .gpu.rs, .reflect.json, .shader_bundle.json | yes (opt-out) |
| `cuda` / `ptx` | PTX only (CUDA-native path) | .gpu.rs, .reflect.json, .shader_bundle.json | yes |
| `hlsl` / `d3d` | SPIR-V + derived HLSL | .gpu.rs, .reflect.json, .shader_bundle.json | yes (opt-out) |

Additional flags:
- `--no-residency`: skip compute residency sidecar generation (.json + .bin staging files)
- `--no-derived`: skip derived cross-target artifacts (HLSL from SPIR-V, PTX from SPIR-V)

Core source anchors:

- Shader parser: `crates/core/src/parser.rs` `parse_shader`, including `shader vertex`, `shader fragment`, contextual `shader compute`, and `uniform name: Type @binding`.
- Shader AST: `crates/core/src/ast.rs` `Shader`, `ShaderStage`, `Uniform`, `ComputeMetadata`.
- Shader typecheck: `crates/core/src/types.rs` `check_shader`; `StorageBuffer<T>` resolves as slice-like `ResolvedType::Slice`.
- LLVM host lane: `crates/sys-codegen/src/codegen_llvm/mod.rs` `generate` and `compile_module`.
- SPIR-V backend: `crates/gpu/src/codegen_spirv.rs`; it uses `rspirv` directly, not LLVM SPIR-V.
- PTX backend: `crates/gpu/src/codegen_ptx.rs`; full CUDA-native codegen with warp intrinsics, shared memory, bitpack ops, fused kernel lowering.
- Artifact bundle driver: `crates/driver/src/lib.rs` `compile_shader_artifact_bundle`, `compile_cuda_artifact_bundle`.
- CLI artifact writer: `crates/cli/src/gpu_artifacts.rs`.
- CUDA runtime executor: `crates/gpu-runtime/src/nvidia_ptx.rs`; dynamic CUDA Driver API, JIT PTX load, zero toolkit dependency.
- Vulkan compute executor: `crates/gpu-runtime/src/executor.rs`.
- NVIDIA PTX executor: `crates/gpu-runtime/src/nvidia_ptx.rs`.
- Native graphics ABI: `runtime/native/include/graphics_system.h`, `runtime/native/src/core/graphics_system.c`.

## Layer Truth

| Layer | Owns | Does Not Own |
| --- | --- | --- |
| `shader` syntax | Kain-authored GPU programs and uniforms | Host frame loop, native windowing, driver handles |
| `std::gpu` | resource policy, stages, queues, access, residency, descriptors, layouts, shared buffers/images | actual rendering executor |
| `std::graphics` | native graphics command-recording ABI facade: sessions, backend selection, buffers, SPIR-V registration, meshes, pipelines, begin/end/present, draw inspection | full Vulkan/D3D executor in the C layer |
| `std::graphics::shared` | adapters from `std::gpu` resources to graphics vertex/index/uniform/storage/image/attachment views | device submission, fence/semaphore/barrier execution |
| `std::math` | engine math and GPU layout helpers: `Std140`, `Std430`, `CBuffer`, `GpuLayoutInfo`, padded vec3/mat helpers | backend memory allocation |
| `std::cuda` | CUDA-side driver API bindings: JIT PTX load, buffer staging, kernel dispatch, warp intrinsics (`cuda_lane_id`, `cuda_warp_reduce_sum_u32`, etc.), residency manifest inspection | CUDA Toolkit installation |
| `kain gpu-artifacts` | SPIR-V, CUDA/PTX, HLSL, Rust host wrapper, reflection JSON, shader bundle JSON, compute residency sidecars | presenting frames by itself |
| `kain-gpu-runtime` | concrete Vulkan compute dispatch and NVIDIA PTX dispatch from sidecars (dynamic CUDA Driver API, zero toolkit) | authored render-loop UX |
| `package-vulkain` | package-owned Vulkan window/presenter bridge and examples | generic compiler/runtime GPU truth |

## Shader Authoring Rules

Shader syntax:

```kn
shader fragment FieldFragment(uv: Vec2) -> Vec4:
    uniform accent: Vec3 @0
    let ring: Float = fbm2(uv, 4)
    return vec4(accent.x * ring, accent.y, accent.z, 1.0)

shader compute ParticleStep(id: UVec3) -> Vec4:
    uniform positions: StorageBuffer<Vec4> @0
    uniform velocity: StorageBuffer<Vec4> @1
    uniform next_positions: StorageBuffer<Vec4> @2
    uniform count: UInt @3
    uniform width: UInt @4
    uniform LOCAL_SIZE_X: UInt @100
    uniform LOCAL_SIZE_Y: UInt @101
    uniform LOCAL_SIZE_Z: UInt @102

    let i = id.x + id.y * width
    if i >= count:
        return vec4(0.0, 0.0, 0.0, 0.0)

    let p = positions[i]
    let v = velocity[i]
    let out = vec4(p.x + v.x, p.y + v.y, p.z + v.z, 1.0)
    next_positions[i] = out
    return out
```

Rules:

- Stages are `shader vertex`, `shader fragment`, and contextual `shader compute`.
- Uniform syntax is `uniform name: Type @binding`; the parser stores that binding in `Uniform.binding`.
- SPIR-V uses descriptor set `0` and binding `@N` for non-input uniforms.
- `StorageBuffer<T>` typechecks as a slice-like type but lowers to a block-wrapped runtime array in `StorageClass::StorageBuffer`.
- Storage buffer stride follows Vulkan-ish layout: scalar 4, vec2 8, vec3/vec4 16, mat4 64.
- Compute params are aliases to builtins. `id: UVec3` maps to `GlobalInvocationId`.
- Compute also exposes `global_invocation_id`, `local_invocation_id`, `workgroup_id`, `local_invocation_index`, plus HLSL-flavored `dispatch_thread_id`, `group_thread_id`, `group_id`, `group_index`.
- `LOCAL_SIZE_X`, `LOCAL_SIZE_Y`, and `LOCAL_SIZE_Z` are special compute uniforms; current SPIR-V lowering treats them as local-size constants with defaults `8, 8, 1`.
- Prefixes such as `CFG_`, `ENABLE_`, `USE_`, `WITH_`, `HAS_`, `ALLOW_`, and `SUPPORT_` are recognized as permutation/spec-constant-style uniforms in the SPIR-V backend.
- Prefer explicit `count` and `width` uniforms and guard every storage-buffer index with `if idx >= count: return`.
- Prefer explicit component-wise vector math when `spirv-val` or current frontend behavior dislikes higher-level vector sugar. The golden `semantic_ping_pong` row documents this current boundary.

## Artifact Flow

Use `kain gpu-artifacts` when the deliverable is the shader pipeline artifact set:

```powershell
# Default: SPIR-V + all derived sidecars + residency
kain gpu-artifacts path/to/kernels.kn --output .kain/gpu/my_pipeline/kernel.spv

# CUDA-only: pure PTX, no SPIRV, no HLSL — one clean artifact set for NVIDIA
kain gpu-artifacts path/to/kernels.kn --output kain --target cuda

# SPIR-V only: skip derived HLSL/PTX cross-compilation
kain gpu-artifacts path/to/kernels.kn --output kernel.spv --target spirv --no-derived

# Minimal: just the shader bundle and reflection, no residency staging files
kain gpu-artifacts path/to/kernels.kn --output kernel --target cuda --no-residency

# HLSL-only for D3D consumption
kain gpu-artifacts path/to/kernels.kn --output kernel --target hlsl
```

That writes (depending on target):

- `.spv`: canonical SPIR-V bytes (spirv/hlsl/all targets).
- `.derived.ptx`: PTX bytes — CUDA-native codegen when `--target cuda`; derived from SPIR-V when `--target all`.
- `.derived.hlsl`: derived HLSL (hlsl/all targets).
- `.gpu.rs`: generated Rust host wrapper sidecar.
- `.reflect.json`: GPU reflection metadata.
- `.shader_bundle.json`: portable shader bundle metadata.
- `*_compute_residency.json` + `*.bin`: compute residency staging sidecars (skipped with `--no-residency`).

Use `kain build <file> --target spirv -o <file.spv>` for a raw SPIR-V binary when the sidecars are not needed.
Use `kain build <file> --target cuda -o <file.ptx>` for a raw PTX binary.

Artifact rules:

- SPIR-V is canonical for Vulkan.
- PTX is canonical for CUDA — it is NOT a "derived sidecar" when targeting CUDA. It IS a derived sidecar when the primary target is SPIR-V.
- HLSL is always a derived sidecar.
- PTX supports compute-stage shader programs only; graphics shaders (vertex/fragment) stay in SPIR-V/HLSL.
- Validate with `spirv-val --target-env vulkan1.3` when available.
- For real runtime dispatch, consume the shader bundle through `kain-gpu-runtime`, a benchmark dispatcher, Fabric GPU step, or a package bridge until stdlib exposes richer public fence/semaphore/barrier APIs.
- CUDA dispatch uses `std::cuda` Kain-side bindings + `crates/gpu-runtime/src/nvidia_ptx.rs` runtime executor — no CUDA Toolkit required, just the NVIDIA driver.

## Host Graphics Loop

`std::graphics` is a low-level command recorder and inspection surface. It is perfect for proof blades and benchmark submit pressure.

```kn
use std::graphics

fn choose_backend() -> String:
    if graphics_backend_supported("vulkan") == 1 and graphics_backend_available("vulkan") == 0:
        return "vulkan"
    if graphics_backend_supported("d3d12") == 1 and graphics_backend_available("d3d12") == 0:
        return "d3d12"
    return ""

fn graphics_submit_probe() -> Int:
    let _reset = graphics_reset()
    let backend = choose_backend()
    if backend == "":
        return 0

    let session = graphics_session_create("gpu.submit.probe", 320, 240)
    if session <= 0:
        return 1
    let _backend = graphics_backend_select(session, backend)

    let vb = graphics_buffer_create_from_hex(session, "vertex", "probe.vertices", "00000000010000000200000003000000", 12)
    let ib = graphics_buffer_create_from_hex(session, "index", "probe.indices", "000000000100000002000000000000000200000003000000", 4)
    let mesh = graphics_mesh_create(session, "probe.mesh", vb, ib, 4, 6)
    let vs = graphics_shader_spirv_from_hex(session, "probe.vertex", "vertex", "main", "03022307")
    let fs = graphics_shader_spirv_from_hex(session, "probe.fragment", "fragment", "main", "03022307")
    let pipeline = graphics_pipeline_create(session, "probe.pipeline", vs, fs, backend)

    let _begin = graphics_begin_frame(session, 16.0)
    let _draw = graphics_draw_mesh(session, pipeline, mesh, 3)
    let end_count = graphics_end_frame(session)
    let present = graphics_present(session)
    let draw_count = graphics_draw_command_count(session)
    let _destroy = graphics_session_destroy(session)
    if present < 0:
        return 2
    return end_count + draw_count
```

Host graphics rules:

- Always create/destroy sessions.
- Select a backend only after checking support/availability.
- Current native `auto` and `software` are available command-recording paths. `vulkan` and `d3d12` are declared targets but not attached direct C executors in `runtime/native/src/core/graphics_system.c`.
- Use draw command counters and query helpers as proof that the host loop recorded the intended shape.
- Use `package-vulkain` for presentable Vulkan windows, screenshots, and vendor-loader work.

## Resource Policy Flow

Use `std::gpu` to describe resources and `std::graphics::shared` to make them graphics-ready:

```kn
use std::gpu
use std::graphics::shared

fn storage_buffer_contract() -> Int:
    let policy = gpu_resource_policy(
        gpu_shared_memory_policy(
            GPU_ACCESS_READ_WRITE,
            GPU_QUEUE_COMPUTE | GPU_QUEUE_TRANSFER | GPU_QUEUE_HOST,
            GPU_LAYOUT_STD430,
            GPU_DESCRIPTOR_STORAGE_BUFFER
        ),
        GPU_BUFFER_USAGE_STORAGE | GPU_BUFFER_USAGE_TRANSFER_SRC | GPU_BUFFER_USAGE_TRANSFER_DST,
        "particles.next"
    )
    let buffer = gpu_shared_buffer_zeroed("f32", [4], "f32", "application/octet-stream", policy)
    let descriptor = gpu_buffer_descriptor(buffer)
    let view = graphics_shared_storage_binding(buffer, 0, GPU_STAGE_COMPUTE, GPU_ACCESS_READ_WRITE)
    if view.ready == false:
        return 1
    if json_get_string(descriptor, "descriptor_kind") != GPU_DESCRIPTOR_STORAGE_BUFFER:
        return 2
    return buffer.byte_length
```

Policy rules:

- Use `GPU_STAGE_*` to describe where a binding is visible.
- Use `GPU_QUEUE_*` to describe graphics/compute/transfer/present/host intent.
- Use `GPU_ACCESS_*` to describe read/write/atomic/persistent-map intent.
- Use `GPU_RESIDENCY_*` to describe host-visible, coherent, shared, device-local, imported, readback, upload, persistent, zero-copy, sparse shapes.
- Use `GPU_DESCRIPTOR_*` to distinguish storage buffer, uniform buffer, sampled image, storage image, raw buffer/image.
- Use `GPU_LAYOUT_STD140` for uniform-style layout and `GPU_LAYOUT_STD430` for storage-buffer style layout.
- Use `std::math` layout wrappers such as `std140_vec3`, `std140_mat4`, `std430_vec3a`, and `cbuffer_mat4` when CPU-authored data must match shader expectations.

## Compute Runtime Synchronization

The compute runtime is more concrete than the public native graphics command recorder:

- `crates/gpu-runtime/src/executor.rs` consumes shader bundles and compute residency sidecars. It creates a Vulkan compute shader module, sorts descriptor bindings by slot, creates host-visible/coherent buffers, builds descriptor set layout and compute pipeline, binds descriptor sets, dispatches workgroups, inserts a `COMPUTE_SHADER -> HOST` memory barrier, submits the queue, waits idle, and maps output buffers back.
- `crates/gpu-runtime/src/nvidia_ptx.rs` loads the CUDA Driver API dynamically, JIT-loads PTX in memory, uploads storage buffers, launches the kernel, calls `cuCtxSynchronize`, downloads output buffers. **No CUDA Toolkit required** — just the NVIDIA driver. Supports fused kernels, warp-level intrinsics, shared memory, bitpack dot-product ops, and multi-kernel residency manifests.
- Kain-side CUDA authoring lives in `std::cuda`: `cuda_driver_available()`, `cuda_runtime_library_available()`, `cuda_has_compute_key()`, `cuda_dispatch()`, `cuda_write_binding_payload_bytes()`, warp intrinsics (`cuda_lane_id()`, `cuda_warp_reduce_sum_u32()`, `cuda_warp_reduce_max_u32()`), `cuda_manifest_debug_from_path()`, and compute residency inspection.
- The semantic search engine (`mcp/semantic_search/`) is the canonical CUDA dogfood: fused score+topk single-kernel pipeline, bitpack dot-product prefilter, warp-reduce block merging, and hybrid CPU/GPU reranking.
- For CUDA-native kernels, use `--target cuda` with `kain gpu-artifacts` to skip the SPIR-V roundtrip entirely.

Authoring consequence:

- Use `kain gpu-artifacts --target cuda` for pure CUDA compute kernels.
- Use `kain gpu-artifacts --target spirv` for Vulkan compute or graphics shaders.
- Use `kain gpu-artifacts` (default `all`) when you need cross-platform artifact bundles.
- Use benchmark GPU dispatchers or `kain-gpu-runtime` for actual compute synchronization.
- Use a package bridge when you need window presentation, swapchain, explicit semaphores/fences, or vendor-specific runtime APIs before they are public stdlib.

## Kain Semantics Mesh

Kain GPU is strongest when the host semantics make the GPU lane obvious:

| Kain Semantic | GPU/Rendering Use |
| --- | --- |
| `world` | authoritative CPU/UI/render state such as camera, mode, frame, simulation tick |
| `entangle` | mirror world state into presentation or telemetry worlds before upload |
| `pulse` | frame cadence, simulation tick, or compute dispatch cadence |
| `collapse` | exclusive CPU buffer mutation before upload or staging |
| `observe` | readback/inspection region after compute or frame record |
| `decay` | teardown of CPU-owned staging memory or temporary resource views |
| `converge` | scalar reference lane plus GPU/native fast lane with verification |
| `axiom` | capability gates such as Vulkan, compute, CUDA/PTX, AVX2, zero-copy |
| `shatter struct` | cache/GPU-friendly data layout intent before buffer materialization |
| `orchestrate` | silicon-native stage graph for CPU/GPU, dispatch, converge, law, patch, world, C/Python, and legacy adapter crossings; use `residency`, `transfer`, `guarded by`, `fallback`, `requires`, and `policy` clauses when GPU/CPU ownership should be visible |
| `law`/`patch` | invariant-checked render state commits and journaled scene mutation |

Semantic skeleton:

```kn
use std::runtime
use std::intent
use std::gpu
use std::graphics

axiom render_machine_truth:
    when target("llvm")
    when capability("graphics.vulkan")
    when capability("gpu.compute")
    guarantee "render host can stage semantic world state into GPU resource policy and shader artifacts"
    fallback scalar_frame_score

world RenderAuthority:
    state frame: Int = 0
    state particles: Int = 65536

world RenderMirror:
    state frame_copy: Int = 0
    state particles_copy: Int = 65536

entangle RenderAuthority.frame <-> RenderMirror.frame_copy with single_writer
entangle RenderAuthority.particles <-> RenderMirror.particles_copy with single_writer

law frame_valid(value: Int) -> Bool:
    return value >= 0

patch commit_frame(authority: RenderAuthority, next_frame: Int) -> Int:
    authority.frame = next_frame
    return authority.frame

fn scalar_frame_score(frame: Int) -> Int:
    return ((frame * 31) + 7) % 1000000007

converge frame_score(frame: Int) -> Int:
    spec reference:
        return scalar_frame_score(frame)
    fast gpu_lane when capability("gpu.compute"):
        return scalar_frame_score(frame)
    verify random(4)

pulse render_clock every 16ms jitter 2ms:
    let next = RenderAuthority.frame + 1
    let committed = commit_frame(RenderAuthority, next)
    let legal = law_status(frame_valid(committed))
    let _shape = frame_score(committed) + legal + pulse_tick + pulse_dt_ms + pulse_missed
```

Rules for semantic GPU authoring:

- Put shader code in `.kn` as shader items, not as opaque strings when Kain can own it.
- Put host frame/session/resource orchestration in normal Kain functions or blades.
- Use worlds and patches for state that should be durable, mirrored, journaled, or UI-visible.
- Use pulse for cadence instead of burying frame timing in a plain `while` loop when the timing is semantic.
- Use collapse/observe/decay around CPU buffer lifetimes when raw memory staging is part of the proof.
- Use converge when you can express a scalar/reference version and a GPU/native fast version.
- Use axiom to state capability assumptions and fallback behavior.

## Rendering Workflows

For a pure compute kernel:

1. Author `shader compute`.
2. Use `StorageBuffer<T>` uniforms and explicit binding slots.
3. Guard all indices with `count` and shape uniforms.
4. Run `kain check <kernels.kn> --target spirv`.
5. Run `kain gpu-artifacts <kernels.kn> --output .kain/gpu/<case>/<kernel>.spv`.
6. Validate with `spirv-val` when available.
7. Dispatch through `benchmark/gpu`, `kain-gpu-runtime`, Fabric GPU step, or a package bridge.

For a graphics command-recorder proof:

1. Use `std::graphics`.
2. Create session, select backend, create buffers, register SPIR-V, create mesh/pipeline.
3. Begin frame, draw, end frame, present.
4. Inspect draw command count/kind/mesh/pipeline/instances.
5. Destroy the session.
6. Use `benchmark/cases/gpu_graphics_submit/main.kn` as the small proof shape.

For a presentable Vulkan window:

1. Co-trigger `package-vulkain`.
2. Keep Kain authoring focused on scene parameters, shader paths, math, UI/semantic state, and package API calls.
3. Keep Vulkan loader, swapchain, push constants, platform locks, and bridge bounds in the package.
4. Run Vulkain package scripts and local Z3 proof when bridge bounds/indexing change.

For a semantics-heavy render system:

1. Author kernels and host state in Kain.
2. Use `std::math` for camera/layout/noise/color.
3. Use `std::gpu`/`std::graphics::shared` for resource policy.
4. Use `world`/`entangle`/`patch`/`law` for scene state.
5. Use `pulse` for frame cadence.
6. Use `converge` and benchmark GPU rows for truth.
7. Use Z3 for buffer bounds, dispatch index math, stride/layout, and draw count clamps.

## Validation Ladder

Quick authored shader check:

```powershell
kain check benchmark/gpu/cases/vec3_storage_copy/kain.kn --target spirv
kain gpu-artifacts benchmark/gpu/cases/vec3_storage_copy/kain.kn --output target/gpu/vec3_storage_copy.spv
spirv-val --target-env vulkan1.3 target/gpu/vec3_storage_copy.spv
```

GPU benchmark no-run build/validation:

```powershell
python benchmark/run_gpu.py --case vec3_storage_copy --languages kain,cpp --no-run --runs 1 --warmups 0 --timeout 300
python benchmark/run_gpu.py --case semantic_ping_pong --languages kain,cpp --no-run --runs 1 --warmups 0 --timeout 300
```

GPU benchmark runtime proof:

```powershell
python benchmark/run_gpu.py --case semantic_ping_pong --languages kain,cpp --runs 1 --warmups 0 --timeout 300
```

Host graphics proof:

```powershell
kain check benchmark/cases/gpu_graphics_submit/main.kn --target llvm
kain run benchmark/cases/gpu_graphics_submit/main.kn --target llvm
```

Runtime/package validation handoffs:

```powershell
cargo check -p kain-gpu-runtime --target-dir target/codex-runtime-gpu
cargo test -p kain-gpu-runtime ptx_dispatch_group_count_rounds_up --target-dir target/codex-runtime-gpu -- --nocapture
powershell -ExecutionPolicy Bypass -File .\blades\vulkain\build-vulkain.ps1
powershell -ExecutionPolicy Bypass -File .\blades\vulkain\examples\mesh-scene\run.ps1
```

Z3/proof expectations:

- Prove `idx < count` implies storage-buffer access stays within the allocated span.
- Prove `x + y * width + z * width * height` cannot overflow the intended dispatch span.
- Prove `StorageBuffer<Vec3>` host payload accounts for 16-byte GPU stride when the runtime expects padding.
- Prove draw-vertex clamps, shader byte bounds, swapchain image counts, and attachment dimensions in package bridges.
- Keep proof reports in `z3/reports` or subsystem proof packs when the result matters.

## Handoff Matrix

- Use `bootstrap-gpu` when changing shader parsing/type metadata only if the compiler frontend/GPU lowering truth must change: SPIR-V, PTX, HLSL, layout lowering, builtins, `StorageBuffer<T>`, local size, spec constants.
- Use `runtime-gpu` when changing `crates/gpu-runtime`, native graphics ABI, runtime-side shader bundle consumption, Vulkan compute execution, NVIDIA PTX execution, or generic graphics runtime conformance.
- Use `package-vulkain` when the task touches `blades/vulkain`, package-local Vulkan bridges, shader paths, platform locks, example windows, or screenshots.
- Use `lang-c-abi` when the GPU path crosses C ABI, platform packages, vendor DLLs, driver loaders, or OS window contracts.
- Use `lang-ui` or `package-kaintana` when GPU rendering is attached to authored UI, Kaintana surfaces, overlays, or desktop UX.
- Use `lang-semantics` when GPU code fuses with `world`, `entangle`, `pulse`, `converge`, `axiom`, `shatter`, `teleport`, `law`, or `patch`.
- Use `lang-systems` when raw memory, packed buffers, ownership regions, cache layout, branchless route math, or low-level staging dominates the task.
- Use `test-bench` when the claim is performance, shader density, GPU duration, or parity against C++/GLSL.

## Anti-Patterns

- Do not call `std::gpu` a renderer. It is policy and resource description.
- Do not call `std::graphics` a direct Vulkan renderer in the generic native C layer. Today it is command recording plus inspection unless a package/runtime executor is explicitly involved.
- Do not hide shader source in C strings when Kain can author `shader compute`, `shader vertex`, or `shader fragment`.
- Do not author unguarded `StorageBuffer` indexing. Carry `count`, `width`, and shape constants.
- Do not reuse binding slots accidentally. Binding numbers are the descriptor contract.
- Do not skip `spirv-val` when changing shader shape or backend-sensitive constructs.
- Do not benchmark GPU through the general CPU benchmark lane when `benchmark/gpu` is the right artifact/runtime truth surface.
- Do not let Vulkain package scripts become generic repo build-system truth; route broad build plumbing to `tool-build-system`.
