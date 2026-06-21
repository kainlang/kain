# Kain Language Newsletter - Issue #1

**Date:** 2026-06-20
**Subject:** GPU System Evolution — Subgroup Scope, Mesh/Raytracing Stages, Precise Barriers, and More
**Philosophy:** Compiler-owned semantics over keyword bloat. 10 features proposed → 9 absorbed into existing constructs → 1 new control-flow keyword.

---

## Executive Summary

Kain's GPU surface just got a major upgrade. Ten features were researched, debated, and implemented — but only one new keyword was added. The rest were absorbed into the existing semantic stack: `orchestrate` owns barrier inference and async compute routing, `comptime` owns specialization constants, `converge` owns tensor core fast lanes, and the compiler inference engine now derives push constants and memory visibility from data it already has.

**Net language surface change: 0.9% (1 keyword / 111 total).**

---

## What Changed

### 1. New Control-Flow Keyword: `subgroup(N) { }`

**What it is:** A block-level scope for warp-synchronous execution inside GPU compute shaders. All threads in a warp (typically 32 on NVIDIA, queryable on Vulkan) execute in lockstep within the block. The compiler validates divergence safety at compile time — no nesting, no divergent escape.

**Why we added it:** This is the only GPU construct with no CPU analog and no home in Kain's existing semantic ladder. `orchestrate` handles multi-stage pipelines, `converge` handles function-level dispatch — neither can express "lockstep execution of N threads within a larger thread block." The Graphics Programmer in our debate made the decisive argument: if Kain owns GPU semantics the way it owns state semantics through `world`/`patch`/`law`, it needs ownership of warp execution scope too.

**Classification:** Control flow keyword (parallel to `if`/`for`/`while`) — NOT a semantic keyword. No decision ladder entry, no runtime contract, no typechecker rules beyond divergence validation.

**Compile-time safety enforced:**
- `KAIN-SHADER-0042`: Nested `subgroup` is illegal
- `KAIN-SHADER-0043`: `return`/`break`/`continue` that exits the scope without reconvergence is illegal
- Only valid inside `shader compute` bodies — host code rejected

**Example:**
```kn
use std::cuda

shader compute ReduceMax(id: UVec3) -> Void workgroup(32, 1, 1):
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1

    let lane = cuda_lane_id()
    let val = src[id.x]

    // Warp-synchronous scope — all 32 lanes execute in lockstep
    subgroup(32):
        let max_val = cuda_warp_reduce_max_f32(val)
        // Only lane 0 writes the result — all lanes reconverge after the block
        if lane == 0:
            dst[id.x] = max_val

    // Outside subgroup — normal divergent execution allowed
    if lane < 16:
        dst[id.x] = val

    return
```

**Backend support:**
| Backend | Subgroup Scope | Intrinsic Mapping |
|---------|---------------|-------------------|
| SPIR-V | `OpGroupNonUniform*` + `OpControlBarrier` | `cuda_*` → `OpGroupNonUniformFAdd`, `OpGroupNonUniformBallot`, `OpGroupNonUniformShuffleXor` |
| PTX | `bar.warp.sync` barriers | Existing `cuda_*` intrinsics (already supported) |
| HLSL | Wave scope comments | `cuda_*` → `WaveActiveSum`, `WaveActiveBallot`, `WaveReadLaneAt` |
| WGSL | Subgroup scope comments | `cuda_*` → `subgroupAdd`, `subgroupBallot`, `subgroupShuffleXor` |

---

### 2. Mesh, Task, and Raytracing Shader Stages (4 → 12)

**What it is:** The `ShaderStage` enum expanded from 4 to 12 variants, adding Mesh, Task, RayGen, AnyHit, ClosestHit, Miss, Intersection, and Callable. These are contextual identifiers (not keywords) — parsed the same way `compute` has always been parsed.

**Why we added it:** Modern GPU pipelines (Vulkan 1.3 with `VK_EXT_mesh_shader`, `VK_KHR_ray_tracing`) need these stages. The parser already handled `shader <stage> Name` generically — extending the enum was a pure data change with zero parser restructuring.

**New stages available:**

| Stage | Vulkan Extension | Use Case |
|-------|-----------------|----------|
| `mesh` | `VK_EXT_mesh_shader` | Amplification-free geometry generation |
| `task` | `VK_EXT_mesh_shader` | Meshlet culling & LOD selection |
| `raygen` | `VK_KHR_ray_tracing` | Ray tracing entry point |
| `anyhit` | `VK_KHR_ray_tracing` | Alpha-test / transparency |
| `closesthit` | `VK_KHR_ray_tracing` | Material evaluation |
| `miss` | `VK_KHR_ray_tracing` | Ray miss shader |
| `intersection` | `VK_KHR_ray_tracing` | Custom geometry intersection |
| `callable` | `VK_KHR_ray_tracing` | Ray tracing utility dispatch |

**Example:**
```kn
shader mesh GenerateGeometry(out positions: Vec3, out indices: UInt) -> Void:
    // Generate geometry directly on GPU — no vertex shader needed
    let idx = global_invocation_id().x
    positions[idx] = vec3(float(idx) * 0.1, 0.0, 0.0)
    indices[idx] = UInt(idx)
    return

shader raygen RayGen() -> Vec4:
    // Trace rays from camera
    return vec4(1.0, 0.0, 0.0, 1.0)

shader closesthit ClosestHit() -> Vec4:
    // Evaluate material at hit point
    return vec4(0.0, 1.0, 0.0, 1.0)
```

**Backend support:**
| Stage | SPIR-V | HLSL | WGSL | PTX |
|-------|--------|------|------|-----|
| Mesh | `MeshEXT` | `[outputtopology("triangle")]` | `@mesh` | ❌ Error (use SPIR-V) |
| Task | `TaskEXT` | `[amplificationmethod]` | `@task` | ❌ Error (use SPIR-V) |
| RayGen | `RayGenerationKHR` | `[shader("raygeneration")]` | `@raygen` | ❌ Error (use SPIR-V) |
| AnyHit | `AnyHitKHR` | `[shader("anyhit")]` | `@anyhit` | ❌ Error (use SPIR-V) |
| ClosestHit | `ClosestHitKHR` | `[shader("closesthit")]` | `@closesthit` | ❌ Error (use SPIR-V) |
| Miss | `MissKHR` | `[shader("miss")]` | `@miss` | ❌ Error (use SPIR-V) |
| Intersection | `IntersectionKHR` | `[shader("intersection")]` | `@intersection` | ❌ Error (use SPIR-V) |
| Callable | `CallableKHR` | `[shader("callable")]` | `@callable` | ❌ Error (use SPIR-V) |

---

### 3. Indirect Dispatch

**What it is:** A grammar variant on the existing `dispatch` keyword. Dispatch dimensions come from a GPU-written buffer instead of compile-time expressions. Same keyword, same semantics, new form.

**Why we added it:** GPU-driven workloads (particle systems, culling, LOD) need the GPU to determine how much work to do next. Without indirect dispatch, the CPU must read back GPU data, decide dimensions, and re-dispatch — a synchronization bottleneck. Now the GPU writes its own dispatch dimensions.

**Example:**
```kn
use std::gpu

// Direct dispatch (existing):
dispatch "shader::MyKernel::compute" [32, 1, 1]

// Indirect dispatch (new):
let buf: ptr<DispatchIndirectCommand> = gpu_indirect_buffer_zeroed()
dispatch "shader::FillDispatch::compute" [1, 1, 1]      // GPU writes [16, 1, 1] into buf
dispatch "shader::ActualWork::compute" from buf          // Reads 12 bytes: dispatch [16, 1, 1]
```

**`DispatchIndirectCommand` layout** (matches `VkDispatchIndirectCommand`, 12 bytes):
```kn
pub struct DispatchIndirectCommand:
    x: UInt    // 4 bytes
    y: UInt    // 4 bytes
    z: UInt    // 4 bytes
```

**LLVM ABI:** `call i64 @abi_gpu_dispatch_indirect(i8* key, i8* buf, i64 12)`

---

### 4. GPU Barrier Inference (Compiler-Owned)

**What it is:** The compiler now derives precise Vulkan pipeline barriers from the `orchestrate` DAG. Previously, every dispatch was followed by a full-pipeline-drain barrier (`COMPUTE_SHADER → HOST`). Now, the compiler analyzes which resources are read/written by each stage, computes exact `srcStageMask`/`dstStageMask`/`srcAccessMask`/`dstAccessMask`, and emits only the necessary barriers.

**Why we added it:** The orchestrate DAG already carries per-stage residency, transfer, shader stage, and resource binding information. The compiler *already knows* what the barriers should be. Writing them manually is error-prone and violates Kain's core philosophy: the compiler should own what it can derive.

**No new syntax.** The compiler inference pass runs automatically on every `orchestrate` block containing GPU stages.

**What changed under the hood:**
- `orchestrate` DAG → `access_map` (HashMap of resource → access kind per stage)
- `infer_barrier_metadata()` → precise `BarrierSpec` per adjacent stage pair
- `barrier_metadata_json()` → serialized JSON passed to runtime via `abi_gpu_dispatch_ext`
- Vulkan executor consumes JSON → `vkCmdPipelineBarrier` with exact flags
- Fallback: when no JSON is available (single-stage, no orchestrate), the old full-pipeline-drain barrier is used

**Impact:** Reduces GPU pipeline stalls. A stage that only reads buffer A and writes buffer B no longer waits for all prior GPU work to complete — it only waits for the specific stages that wrote B.

---

### 5. Push Constant Inference (Compiler-Owned, SPIR-V Only)

**What it is:** Small shader uniforms (≤128 bytes total, accessed by a single stage) are automatically lowered to Vulkan push constants (`StorageClass::PushConstant`) instead of descriptor-set uniforms. This eliminates descriptor binding overhead for small parameter blocks.

**Why we added it:** The Vulkan 128-byte `maxPushConstantsSize` is wasted if the compiler doesn't use it. The data is already there — uniform name, type, size, and access stage. The compiler just needed to check the size and emit the right SPIR-V storage class.

**No new syntax.** The existing `uniform name: Type @N` syntax is unchanged. Push constant lowering is an invisible optimization.

**Example — two uniforms that auto-lower to push constants:**
```kn
shader compute MyKernel(id: UVec3) -> Void:
    uniform params: Vec4 @0      // 16 bytes → PushConstant
    uniform color: Vec3 @1       // 12 bytes → PushConstant
    // Total: 28 bytes ≤ 128 → no descriptor set, just push constants
```

**SPIR-V output:** Single `OpTypeStruct` with `StorageClass::PushConstant` and `Decoration::Block`. No `DescriptorSet` or `Binding` decorations.

---

### 6. Specialization Constants in `comptime`

**What it is:** Shader constants that can be overridden at dispatch time without recompiling the SPIR-V module. Declared inside existing `comptime` blocks using the 6-element tuple form.

**Why we added it:** Vulkan specialization constants allow a single compiled shader module to be specialized for different tile sizes, data types, or feature flags at pipeline creation time. The `comptime` metadata block was already carrying tensor plans and stream plans — adding a `spec_constants` entry was a natural extension.

**Example:**
```kn
shader compute MyKernel(id: UVec3) -> Void:
    uniform src: StorageBuffer<Float> @0
    comptime:
        let compute = (
            [256, 1, 1],                              // workgroup
            [32, 1, 1],                                // dispatch
            [("src", "f32", ["grid"], "input", "kain.shared.buffer")],
            [],                                        // streams
            [],                                        // neural nodes
            [("tile_size", "u32", 128, "SPEC"),        // spec constants
             ("enable_fp16", "bool", true, "SPEC")]
        )
```

**SPIR-V output:** `OpSpecConstant` + `OpDecorate SpecId N` for each spec constant. Host override syntax (`dispatch ... with { tile_size: 64 }`) is planned for a future release.

---

### 7. Tensor Core `@extern` Declarations + Axiom Presets

**What it is:** Tensor core operations (NVIDIA WMMA/MMA/WGMMA, AMD MFMA) are available as `@extern` declarations with `axiom` capability gating. No new keywords — the existing `@extern` + `axiom` machinery handles everything.

**Why we added it:** Tensor cores are the fastest math units on modern GPUs (up to 1000+ TFLOPS on H100). Kain already had `cuda_warp_reduce_*` and `cuda_shfl_*` intrinsics — tensor cores are the same pattern, just different PTX instructions.

**Available intrinsics (stdlib/cuda.kn):**
| Function | Instruction Class | Min SM |
|----------|------------------|--------|
| `cuda_wmma_matmul_f16_f32` | WMMA matmul | sm_70 (Volta) |
| `cuda_wmma_activate_f32` | WMMA activation | sm_70 |
| `cuda_wmma_store_f32` | WMMA store to memory | sm_70 |
| `cuda_mma_matmul_f16_f32` | MMA matmul | sm_80 (Ampere) |
| `cuda_mma_matmul_f32_f32` | MMA matmul | sm_80 |
| `cuda_wgmma_matmul_f16_f32` | WGMMA matmul | sm_90 (Hopper) |
| `cuda_mfma_f32_f32` | AMD MFMA matmul | CDNA |
| `cuda_mfma_f16_f32` | AMD MFMA matmul | CDNA |

**Capability gating (9 new predicates):**
| Predicate | Bit | Meaning |
|-----------|-----|---------|
| `cuda.sm_70` | 12 | NVIDIA Volta SM 7.0+ |
| `cuda.sm_75` | 13 | NVIDIA Turing SM 7.5+ |
| `cuda.sm_80` | 14 | NVIDIA Ampere SM 8.0+ |
| `cuda.sm_90` | 15 | NVIDIA Hopper SM 9.0+ |
| `cuda.tensorcore` | 16 | Any tensor core available |
| `cuda.wmma` | 17 | WMMA instructions |
| `cuda.mma` | 18 | MMA instructions |
| `cuda.wgmma` | 19 | WGMMA instructions |
| `gpu.async_compute` | 20 | Async compute queue available |

**Example — gated tensor core matmul with scalar fallback:**
```kn
use std::cuda

fn scalar_matmul(a: ptr<Float>, b: ptr<Float>, c: ptr<Float>, M: UInt, N: UInt, K: UInt) -> Void:
    // Scalar fallback implementation
    return

axiom sm_90_tensorcore:
    when capability("cuda.sm_90")
    when capability("cuda.tensorcore")
    when capability("cuda.wgmma")
    guarantee "sm_90 tensor core matmul available via WGMMA"
    fallback scalar_matmul

fn matmul_dispatch() with GPU, Unsafe:
    // axiom selects cuda_wgmma_matmul_f16_f32 on Hopper, scalar_matmul on older GPUs
    return
```

---

### 8. Async Compute Queue Hints

**What it is:** Orchestrate stages with `policy prefer_async_compute` are routed to a separate hardware compute queue, enabling concurrent graphics and compute work. The runtime auto-detects whether the GPU supports async compute and gracefully degrades to a single queue when unavailable.

**Why we added it:** Modern GPUs (NVIDIA since Maxwell, AMD GCN, Intel Arc) have dedicated async compute hardware. Without it, compute dispatches block the graphics queue. The orchestrate DAG already expresses parallelism — the compiler just needed to hint at queue affinity.

**Example:**
```kn
orchestrate frame_pipeline(gfx_cmds: Int, compute_cmds: Int) -> Int:
    stage gfx: gpu submit_graphics(gfx_cmds)
        residency host policy static
    stage cmp: gpu submit_compute(compute_cmds)
        residency host policy prefer_async_compute
    // These two stages run in parallel on different hardware queues
    stage composite: cpu merge_results(gfx, cmp) after [gfx, cmp]
```

**No new keyword.** The `policy prefer_async_compute` clause uses the existing orchestrate policy system.

---

### 9. Pipeline Library Types

**What it is:** `std::gpu::PipelineLibrary` and `std::gpu::PipelineHandle` provide runtime pipeline caching and reuse. Pipelines compiled once can be dispatched many times without recompilation.

**Why we added it:** GPU pipeline compilation is expensive (10-100ms). Recompiling on every dispatch wastes GPU time. The pipeline library is a stdlib type, not a language keyword — the runtime owns caching, compilation, and introspection.

**Usage:**
```kn
use std::gpu

fn setup_pipelines() -> PipelineHandle:
    let lib = gpu_pipeline_library_create("my_library")
    let handle = gpu_pipeline_library_register(lib, "shader::MyKernel::compute", [256, 1, 1])
    return handle

fn dispatch_with_cache(h: PipelineHandle) with GPU, Unsafe:
    dispatch h [32, 1, 1]     // Uses cached VkPipeline — no recompilation
```

**New types in `stdlib/gpu.kn`:**
| Type | Fields | Purpose |
|------|--------|---------|
| `PipelineHandle` | `id: Int, library_name: String, compute_key: String, dispatch_size: [Int; 3]` | Opaque reference to cached pipeline |
| `PipelineLibrary` | `name: String, pipeline_count: Int` | Named cache of compiled pipelines |
| `DispatchIndirectCommand` | `x: UInt, y: UInt, z: UInt` | 12-byte GPU-written dispatch size (matches `VkDispatchIndirectCommand`) |

**New functions:**
- `gpu_pipeline_library_create(name)` → `PipelineLibrary`
- `gpu_pipeline_library_register(lib, key, dims)` → `PipelineHandle`
- `gpu_pipeline_library_find(lib, key)` → `PipelineHandle` (id = -1 if not found)
- `gpu_pipeline_library_destroy(lib)` → `Int`
- `gpu_indirect_buffer_zeroed()` → `ptr<DispatchIndirectCommand>`
- `gpu_indirect_buffer_from_bytes(hex)` → `ptr<DispatchIndirectCommand>`

---

### 10. Memory Visibility Inference (Effect Checker)

**What it is:** Functions annotated `with GPU` (no `Unsafe`) are now accepted when the `dispatch` statement is inside an `orchestrate` block. The compiler infers memory access safety from the orchestrate DAG's `access_map`.

**Why we added it:** The `with GPU, Unsafe` annotation was a blunt instrument — any GPU dispatch required the `Unsafe` effect. But when dispatch is wrapped in an orchestrate block, the compiler has full knowledge of resource bindings, read/write access patterns, and residency. It can prove memory safety without the programmer declaring `Unsafe`.

**This is a Phase 1 partial implementation.** Full shrink of `with GPU, Unsafe` toward `with GPU` for all dispatch patterns is future work.

**Example:**
```kn
// OLD — required Unsafe:
fn old_dispatch() with GPU, Unsafe:
    dispatch "key" [1, 1, 1]

// NEW — orchestrate provides safety proof:
fn new_dispatch() with GPU:                    // No Unsafe needed
    orchestrate my_pipeline:
        stage main: gpu dispatch "key" [1, 1, 1]
            residency host policy static
```

---

## The Absorption Model (Design Philosophy)

The central insight from our GPU debate: Kain GPU improvements should prefer absorption over expansion. Before adding ANY GPU-specific syntax, ask:

1. Does an existing semantic construct already cover this? (`orchestrate`, `converge`, `world`, `patch`, `law`, `axiom`, `comptime`, `dispatch`)
2. Can a compiler inference pass derive the information from what the programmer already declares?
3. Is the proposed syntax a *control flow* construct (no CPU analog → maybe new keyword) or a *semantic* construct (should fold into existing ladder)?

| GPU Concern | Absorbed Into | Mechanism |
|---|---|---|
| Async compute | `orchestrate` | Parallel DAG stages + `policy prefer_async_compute` |
| GPU barriers | `orchestrate` | `access_map` inference from DAG |
| Memory visibility | `orchestrate` + `dispatch` | `access_map` → LLVM ABI |
| Push constants | `uniform` | SPIR-V backend inference |
| Spec constants | `comptime` | Metadata extension |
| Pipeline caching | `std::gpu` | Library types |
| Indirect dispatch | `dispatch` | Grammar variant (`from buf`) |
| Tensor cores | `@extern` + `axiom` | FFI + capability gating |
| Mesh/raytracing | `shader` | Enum extension |
| Subgroup/warp | **New:** `subgroup(N) { }` | Control flow keyword — no CPU analog |

**Net result:** 10 proposals, 1 new keyword. Language surface growth: 0.9%.

---

## Implementation Status (Wave 1 + Wave 2 Complete)

| Layer | What Was Built | Files Changed |
|-------|---------------|---------------|
| **Syntax** | `subgroup(N) { }` keyword, `DispatchSize` enum, `ShaderStage` 4→12, spec constants in `comptime` | `ast.rs`, `parser.rs`, `lexer.rs`, `types.rs`, `error/code.rs` |
| **Inference** | GPU barrier inference, push constant classifier, async compute policy, Z3 proofs | `orchestrate/graph.rs`, `orchestrate/planner.rs`, `sys-codegen/mod.rs` |
| **Stdlib** | `PipelineLibrary`, `PipelineHandle`, `DispatchIndirectCommand`, 8 tensor core `@extern`, 9 capability predicates, C runtime cache | `stdlib/gpu.kn`, `stdlib/cuda.kn`, `cuda_runtime.c` |
| **GPU Codegen** | SPIR-V/PTX/HLSL/WGSL: subgroup intrinsics, 12-stage mapping, push/spec constants | `codegen_spirv.rs`, `codegen_ptx.rs`, `shader-text/lib.rs`, `gpu_artifacts.rs` |
| **Runtime** | Barrier JSON consumption, async compute queues, pipeline cache, indirect dispatch | `executor.rs`, `bindings.rs`, `cuda_runtime.c`, `nvidia_ptx.rs` |
| **Downstream** | 25 error site fixes, rspirv 0.12 API migration | 9 files across `cli/`, `fmt/`, `sys-codegen/`, `build/` |

**Total:** ~55 files changed across `crates/`, `runtime/`, `stdlib/`. All crates compile clean. End-to-end barrier JSON flows from compiler inference → LLVM IR → C ABI → Vulkan executor → `vkCmdPipelineBarrier`.

---

## What You Can Do Now

### Use `subgroup(32):` for warp-synchronous reductions
```kn
shader compute MaxPool(id: UVec3) -> Void workgroup(32, 1, 1):
    uniform input: StorageBuffer<Float> @0
    uniform output: StorageBuffer<Float> @1
    let val = input[id.x]
    subgroup(32):
        let max_val = cuda_warp_reduce_max_f32(val)
        if cuda_lane_id() == 0:
            output[id.x / 32] = max_val
    return
```

### Use mesh shaders for GPU-driven geometry
```kn
shader mesh TerrainGen(out positions: Vec3, out indices: UInt) -> Void:
    // Generate terrain directly on GPU
    let idx = global_invocation_id().x
    positions[idx] = vec3(float(idx) * 0.1, sin(float(idx) * 0.01) * 10.0, 0.0)
    return
```

### Use indirect dispatch for GPU-driven workloads
```kn
fn gpu_driven_pipeline() with GPU, Unsafe:
    let cull_buf: ptr<DispatchIndirectCommand> = gpu_indirect_buffer_zeroed()
    dispatch "shader::CullObjects::compute" [1, 1, 1]           // GPU culling
    dispatch "shader::RenderVisible::compute" from cull_buf     // Indirect render
```

### Use orchestrate for parallel graphics + compute
```kn
orchestrate frame_pipeline:
    stage gfx: gpu render_scene() residency host policy static
    stage cull: gpu cull_next_frame() residency host policy prefer_async_compute
    // Render current frame while culling next frame — on separate queues
```

### Gate tensor cores with axiom
```kn
axiom has_tensor_cores:
    when capability("cuda.tensorcore")
    guarantee "hardware tensor core acceleration available"
    fallback cpu_matmul
```

---

## What Did NOT Change (The Circle in the Sand)

These proposals were debated and deliberately rejected:

| Rejected Keyword | Why | Instead Use |
|-----------------|-----|-------------|
| `gpu_barrier` | Barriers are inferrable from orchestrate DAG | Compiler inference |
| `push_constants` | Should be invisible compiler optimization | SPIR-V backend inference |
| `specialize` | `comptime` already owns this space | 6-element comptime tuple |
| `dispatch_indirect` | Grammar variant on `dispatch` is cleaner | `dispatch "key" from buf` |
| `async_dispatch` | Parallel orchestrate stages already express this | `policy prefer_async_compute` |
| `tensor_matmul` | `@extern` + `axiom` is the right approach | `@extern` declarations |
| `pipeline_library` | Library type in `std::gpu`, not a language keyword | `std::gpu::PipelineLibrary` |
| `subgroup` as semantic keyword | It's control flow — parallel to `if`/`while`, not to `world`/`orchestrate` | `subgroup(N) { }` as control flow |

---

## Shader Gallery — Real Kain Shaders in Production

Two SPIR-V validated shaders showcasing the full GPU pipeline:

### 🌊 Ray-Traced Ocean (`blades/shaderlib/ocean.kn`)

Translated from a Shadertoy GLSL original by afl_ext. Full wave-based ocean simulation with raymarching, atmospheric scattering, ACES tonemapping, and Fresnel reflection — all in pure Kain.

**Features exercised:**
- Fragment shader with 4 uniforms (`time`, `resolution`, `mouse`, `StorageBuffer`)
- 200+ line fully inlined raymarcher with 64-step water column march
- Wave physics via summed octaves with `pow(E, sin(x)-1.0)` exponential waves
- Camera orbit via rotation matrix multiplication (no mat3 — hand-rolled)
- Sky/water split with atmospheric scattering (Mie + Rayleigh approximation)
- Fresnel-based reflection with subsurface scattering
- ACES filmic tonemapping with gamma correction

**Artifacts generated:** SPIR-V (validated), HLSL derived shader
**Key insight:** Kain fragment shaders support all math primitives (`sin`, `cos`, `sqrt`, `pow`, `abs`, `min`, `max`, `FMA`) via GLSL.std.450 extended instruction set. No SPIR-V capabilities beyond `Shader` are needed.

```kn
// Simplified wave function — full version at blades/shaderlib/ocean.kn
shader fragment OceanFragment(uv: Vec2) -> Vec4:
    uniform time: Float @0
    uniform resolution: Vec2 @1
    uniform mouse: Vec2 @2
    uniform _pad: StorageBuffer<Float> @3

    // Ray origin + direction, raymarch through water column
    // For each step: compute wave height via exp(sin(x)-1.0)
    // If height > ray.y: hit water surface, compute Fresnel/reflection
    // Else: step forward and continue
    // After march: compute atmosphere for sky, or water color for hit
    return vec4(r, g, b, 1.0)
```

**SPIR-V output:** 2,086 lines, 11 while-loops, proper structured control flow (`OpSelectionMerge`/`OpLoopMerge`). Validated against vulkan1.3.

---

### 🌀 Schwarzschild Black Hole (`blades/shaderlib/blackhole.kn`)

A gravitational raymarcher that renders a rotating black hole with full general-relativistic light bending. The camera orbits around the black hole at `CAM_DIST = 16.0` Schwarzschild radii, and at each raymarch step, the ray is deflected toward the black hole by an amount proportional to `Rs / r²`.

**Features exercised:**
- Camera orbit with mouse control (phi/theta angles)
- Full 3D camera basis construction (forward/right/up) from spherical coordinates
- 512-step maximum raymarch through curved spacetime
- Event horizon capture (black hole interior goes to pure black)
- Accretion disk detection with temperature gradient (hottest at inner edge)
- Gravitational redshift shifting disk colors toward red near the event horizon
- Doppler beaming (approximate: approaching side appears brighter)
- Photon ring detection at 1.5 × Schwarzschild radius
- Einstein ring gravitational lensing glow
- Starfield background with Milky Way band modulation
- Reinhard tone mapping + gamma correction

```kn
// Core raymarch loop — full version at blades/shaderlib/blackhole.kn
var px: Float = cam_x
var py: Float = cam_y
var pz: Float = cam_z
var dx: Float = rx
var dy: Float = ry
var dz: Float = rz

while step_i < 512:
    // Check event horizon
    // Check accretion disk intersection
    // Gravitational lensing: bend ray toward BH
    // Accumulate starfield + disk glow
    px = px + dx * step  // march forward
    ...
```

**Artifacts generated:** SPIR-V, HLSL, PTX variants, Rust wrapper, reflection JSON, shader bundle

**What makes this a monster:** 512 iterations of inline raymarching, photon sphere sampling (128-step sub-march for closest approach estimation), 3D camera orbit, and full gravitational lensing physics — all in a single Kain shader body. No nested functions, no closures — all math is inlined as scalar operations on `var Float` variables, because Kain shader bodies only support `let`/`var` on scalar types, `vec2/3/4()` constructors, `while` loops, and `if/else`. Every trig function (sin, cos, sqrt, pow) goes through GLSL.std.450 extended instructions.

### Key Takeaways for Shader Authors

1. **No nested functions or closures inside shader bodies.** All math must be inlined. Use `while` loops for iteration, scalar `var Float` for mutable state.
2. **Use `vec2()`, `vec3()`, `vec4()` constructors, not struct syntax.** `Vec3 {x:1, y:2, z:3}` is host-only. In shaders, use `vec3(1.0, 2.0, 3.0)`.
3. **Module-level `const` values are NOT accessible from shader bodies.** Inline all constants as literal values inside the shader.
4. **Available math:** `sin`, `cos`, `sqrt`, `pow`, `abs`, `max`, `min`, `floor`, `ceil` (through GLSL.std.450). NOT `exp`, `atan2`, `acos`, `asin`.
5. **`StorageBuffer` padding trick:** If a fragment shader's uniforms are under 128 bytes total, the SPIR-V backend attempts push-constant inference which has a known AccessChain bug. Add a dummy `uniform _pad: StorageBuffer<Float> @N` to force descriptor-based binding.
6. **`var X: Vec3 = ...` is not supported** — Kain shader bodies only support `var` on scalar `Float`, `Int`, `UInt` types. Use separate `var r: Float`, `var g: Float`, `var b: Float` for mutable color state.
7. **`vec3 + vec3` works in `let` expressions** but NOT on the left side of an assignment to a `var`.

---

## Research & Documentation

- Full debate transcript: `research/gpu/gpu_debate.md`
- Debate recommendations: `research/gpu/GPU_DEBATE_RECOMMENDATIONS.md`
- Implementation plan: `research/gpu/IMPLEMENTATION.md`
- Wave 1 audit: `research/gpu/WAVE1_AUDIT.md`
- Codebase maps: `research/gpu/MAP_DELTA.md`, `research/gpu/MAP_ECHO.md`, `research/gpu/MAP_P1_FIXES.md`
- Stream task files: `research/gpu/tasks.md`, `research/gpu/tasks_syntax.md`, `research/gpu/tasks_inference.md`, `research/gpu/tasks_stdlib.md`, `research/gpu/tasks_codegen.md`, `research/gpu/tasks_runtime.md`

---
