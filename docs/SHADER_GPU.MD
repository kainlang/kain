# Shader & GPU Pipeline 〰 Full Reference

**Status:** Research synthesis, 2026-06-07
**Based on:** `crates/core/src/ast.rs`, `crates/core/src/parser.rs`, `crates/core/src/types.rs`, `crates/gpu/`, `crates/gpu-runtime/`, `crates/shader-text/`, `crates/sys-codegen/src/codegen_llvm/mod.rs`, `crates/sys-codegen/src/codegen_rust/gpu_artifacts.rs`, `crates/sys-codegen/src/codegen_rust/gpu_host.rs`, `crates/cli/src/gpu_artifacts.rs`, `crates/driver/src/lib.rs`, `stdlib/gpu.kn`, `stdlib/graphics.kn`, `stdlib/cuda.kn`, `stdlib/build.kn`, `benchmark/cases_v2/gpu_cpu_pipeline.kn`, `benchmark/cases/gpu_graphics_submit/main.kn`, `library_of_kain/`, `blades/cuda/`, RULEBOOK.MD, lang-gpu SKILL, lang-semantics SKILL

______________________________________________________________________

## Table of Contents

1. [Introduction - The Dual Nature of GPU Work](#introduction)
1. [Shader Authoring in Kain](#shader-authoring)
1. [Shader Syntax Reference](#shader-syntax)
1. [Uniform Bindings and Resource Kinds](#uniform-bindings)
1. [Compute Metadata -- The `comptime` Plan](#compute-metadata)
1. [Host-Side Dispatch :: The `dispatch` Keyword](#host-dispatch)
1. [The Full GPU Pipeline](#full-pipeline)
1. [Backend: SPIR-V (Vulkan)](#backend-spirv)
1. [Backend: PTX / CUDA (NVIDIA)](#backend-ptx)
1. [Backend: HLSL (DirectX)](#backend-hlsl)
1. [Backend: WGSL (WebGPU)](#backend-wgsl)
1. [GPU Runtime Executors](#gpu-runtime)
1. [Stdlib: `std::gpu` ~ Resource Policy](#stdlib-gpu)
1. [Stdlib: `std::graphics` ->> Rendering Commands](#stdlib-graphics)
1. [Stdlib: `std::cuda` === CUDA Bridge](#stdlib-cuda)
1. [Using GPU in `build.kn`](#build-kn)
1. [The CLI `gpu-artifacts` Command](#cli)
1. [Semantic Fusion 〰 Worlds, Entangle, Patches, Laws, and Shaders](#semantic-fusion)
1. [Real Examples: LLM Training Kernels](#llm-training)
1. [Real Examples: Fluid Simulation Kernels](#fluid-sim)
1. [Real Examples: Graphics Pipeline](#graphics-pipeline)
1. [Real Examples: Sculpt Brush Kernels](#sculpt-brushes)
1. [Anti-Patterns](#anti-patterns)
1. [Validation Ladder](#validation-ladder)
1. [Source Anchors](#source-anchors)
1. [Quick Reference](#quick-reference)

______________________________________________________________________

## Introduction

Kain has a **dual GPU architecture**: shader authoring and host-side dispatch are both first-class language semantics, but they occupy two distinct lanes.

```
.kn source → parser/typechecker → split:
  → CPU host lane: LLVM IR → native ABI calls → dispatch / std::gpu / std::graphics
  → SPIR-V lane:   crates/gpu → rspirv → canonical .spv binary
  → PTX lane:      crates/gpu → direct PTX emission → NVIDIA Driver API
  → HLSL lane:     crates/gpu → crates/shader-text → .hlsl text
  → WGSL lane:     crates/gpu → crates/shader-text → .wgsl text
```

**Key insight:** A single `.kn` file can contain `shader compute` items AND host-side `dispatch "key" [x, y, z]` statements AND world/entangle/patch/law/converge/orchestrate semantics. The compiler separates GPU kernels from host code during compilation, emits sidecar artifacts for the GPU kernels, and the host code calls them through the runtime.

The canonical example is `benchmark/cases_v2/gpu_cpu_pipeline.kn` -- a single file with:

- 2 `world` declarations with `entangle`
- 1 `law`, 1 `patch`, 1 `converge`, 1 `orchestrate`
- Ownership regions (`collapse`/`observe`/`decay`)
- A `shader compute` kernel with `comptime` metadata
- `dispatch` statements for GPU launch
- Runtime contract telemetry for all semantic layers

______________________________________________________________________

## Shader Authoring

### Shader Stages

Kain supports three shader stage declarations:

| Stage | Declaration | Description |
|-------|-------------|-------------|
| Vertex | `shader vertex Name(inputs) -> Vec4:` | Transforms vertices, outputs position |
| Fragment | `shader fragment Name(inputs) -> Vec4:` | Colors pixels, outputs RGBA |
| Compute | `shader compute Name(id: UVec3) -> Void:` | General-purpose GPU compute kernel |

(In the AST, there is also `ShaderStage::Surface`, but the current parser path is vertex/fragment/compute.)

### Shader Inputs

Shader inputs are function parameters on the shader declaration:

```kn
shader vertex SmokeVertex(position: Vec3, uv: Vec2) -> Vec4:
```

For vertex shaders, inputs are vertex attributes with semantic locations. For compute shaders, the canonical input is:

```kn
shader compute MyKernel(id: UVec3) -> Void:
```

The `id: UVec3` is the dispatch thread ID (equivalent to `SV_DispatchThreadID` in HLSL or `global_invocation_id` in WGSL). This is the only input meaningful for compute kernels ~~ everything else comes through `uniform` bindings.

### Shader Uniforms

Uniforms are declared inside the shader body with `@N` binding annotations:

```kn
shader compute MyKernel(id: UVec3) -> Void:
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1
    uniform params: Vec4 @2
    // shader body follows
```

Uniform binding syntax: `uniform name: Type @binding_number`

### Type Reference for Shaders

| Kain Type | SPIR-V | HLSL | WGSL | PTX |
|-----------|--------|------|------|-----|
| `Void` | void | void | () | void |
| `Float` | f32 | float | f32 | f32 |
| `Int` | i32 | int | i32 | s32 |
| `UInt` | u32 | uint | u32 | u32 |
| `Bool` | bool | bool | bool | pred |
| `Vec2` | vec2<f32> | float2 | vec2<f32> | v2.f32 |
| `Vec3` | vec3<f32> | float3 | vec3<f32> | v3.f32 |
| `Vec4` | vec4<f32> | float4 | vec4<f32> | v4.f32 |
| `IVec2` | vec2<i32> | int2 | vec2<i32> | v2.s32 |
| `UVec2` | vec2<u32> | uint2 | vec2<u32> | v2.u32 |
| `UVec3` | vec3<u32> | uint3 | vec3<u32> | v3.u32 |
| `Mat4` | mat4x4<f32> | float4x4 | mat4x4<f32> | - |
| `StorageBuffer<T>` | StorageBuffer | RWStructuredBuffer<T> | array<T> | .param / .global |
| `Sampler2D` | Sampler2D | Texture2D | texture_2d<f32> | tex |
| `Vec3A` | vec3<f32> | float3 | vec3<f32> | v3.f32 |

______________________________________________________________________

## Shader Syntax

### Compute Shader (Canonical)

```kn
shader compute ParticleStep(id: UVec3) -> Void:
    uniform particles: StorageBuffer<Vec4> @0
    uniform field: StorageBuffer<Vec4> @1

    let p = particles[id.x]
    let v = field[id.x]
    particles[id.x] = vec4(p.x + v.x, p.y + v.y, p.z + v.z, 1.0)
```

### Compute Shader with `workgroup`

```kn
shader compute MyKernel(id: UVec3) -> Void workgroup(8, 8, 1):
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1

    let i = id.x + id.y * UInt(256)
    if i > UInt(254) * UInt(256) + UInt(254):
        return
    dst[i] = src[i] + 1.0
```

The `workgroup(N, M, K)` after the return type sets the workgroup size. For:

- HLSL: emitted as `[numthreads(N, M, K)]`
- WGSL: emitted as `@workgroup_size(N, M, K)`
- SPIR-V: emitted as `LocalSize` execution mode
- PTX: emitted as `.maxnreg` metadata

### Fragment Shader

```kn
shader fragment FieldFragment(uv: Vec2) -> Vec4:
    uniform accent: Vec3 @0
    let ring: Float = fbm2(uv, 4)
    return vec4(accent.x * ring, accent.y, accent.z, 1.0)
```

### Vertex Shader

```kn
shader vertex SmokeVertex(position: Vec3, uv: Vec2) -> Vec4:
    uniform model: Mat4 @0
    uniform view: Mat4 @1
    let world_pos = model * vec4(position.x, position.y, position.z, 1.0)
    return view * world_pos
```

### Shader Body Capabilities

Shader bodies support:

- `let` bindings with type inference
- Arithmetic (`+`, `-`, `*`, `/`, `%`)
- Comparisons (`==`, `!=`, `<`, `<=`, `>`, `>=`)
- Logical (`&&`, `||`, `!`)
- Bitwise (`&`, `|`, `^`, `<<`, `>>`, `~`)
- Function calls (only to identifiable names ⁓ no complex callees)
- Method calls for swizzle (`.xyzw`, `.rgba`) and standard GPU methods
- Field/index access (`object.field`, `array[index]`)
- Assignment (`=`, `+=`, `-=`, etc.)
- `if`/`else`/`else if`
- `while` loops
- `return` statements (with value for fragment/vertex, bare for compute)
- `break` / `continue`

Shader bodies do NOT support:

- `for` loops ⁓ use `while` with explicit bounds
- `loop` / `fanout` / `defer`
- `dispatch` statements (host-side only)
- String expressions
- Destructuring patterns
- Complex function callees

### The Return Type Distinction

| Stage | Return | Meaning |
|-------|--------|---------|
| `compute` | `-> Void` | Returns nothing (`return;`) |
| `fragment` | `-> Vec4` | Returns pixel color |
| `vertex` | `-> Vec4` | Returns clip-space position |

______________________________________________________________________

## Uniform Bindings and Resource Kinds

### Binding Syntax

```kn
uniform name: Type @binding_number
```

The binding number maps to descriptor set slots:

- SPIR-V: `@group(0) @binding(N)` or DescriptorSet=0, Binding=N
- HLSL: `register(bN)`, `register(tN)`, `register(uN)`
- WGSL: `@group(0) @binding(N)`
- PTX: `.param` / kernel parameter position

### Resource Classification

The `shader-text` crate classifies each uniform by name and type:

| Classification | Examples | Backend Mapping |
|---------------|----------|-----------------|
| `Uniform` | `Vec4`, `Float`, `Int`, `Mat4` | cbuffer (HLSL), `<uniform>` (WGSL), push_constant (SPIR-V) |
| `StorageBuffer` | `StorageBuffer<T>` | RWStructuredBuffer (HLSL), `var<storage, read_write>` (WGSL) |
| `Texture` | `Sampler2D`, `Texture2D` | Texture2D + SamplerState (HLSL) |
| `Sampler` | `SamplerState` | SamplerState (HLSL), `sampler` (WGSL) |
| `LocalSize` | `LOCAL_SIZE_X/Y/Z` | Inlined compile-time constants |

Special uniforms `LOCAL_SIZE_X`, `LOCAL_SIZE_Y`, `LOCAL_SIZE_Z` are reserved for accessing workgroup dimensions inside shader bodies. They are recognized by name matching (not type) and are NOT emitted as actual resource bindings ~> they get inlined as compile-time constants matching the `workgroup(...)` value.

______________________________________________________________________

## Compute Metadata

Compute shaders can (and should) carry compute metadata in a `comptime:` block:

```kn
shader compute MyKernel(id: UVec3) -> Void:
    uniform src: StorageBuffer<UInt> @0
    uniform dst: StorageBuffer<UInt> @1

    comptime:
        let compute = (
            [32, 1, 1],           // dispatch_size: [x, y, z]
            [                       // tensor_plans
                ("src", "u32", ["dispatch.x"], "input", "kain.shared.buffer"),
                ("dst", "u32", ["dispatch.x"], "output", "kain.shared.buffer"),
            ],
            [],                     // neural_node_plans (optional)
        )
```

### Legacy vs Extended Form

**Legacy (dispatch + tensors + nodes):**

```kn
let compute = (
    [32, 1, 1],                                       // dispatch
    [("src", "u32", ["grid"], "input", "kain.shared.buffer")],  // tensors
    []                                                  // nodes
)
```

**Extended (workgroup + dispatch + tensors + streams + nodes):**

```kn
let compute = (
    [8, 1, 1],                                        // workgroup_size
    [32, 1, 1],                                        // dispatch_size
    [("src", "u32", ["grid"], "input", "kain.shared.buffer")],  // tensors
    [("src", "ingress", "per-dispatch", "kain.shared.buffer")], // streams
    []                                                  // nodes
)
```

### Tensor Plan Tuple Elements

| Position | Name | Example | Description |
|----------|------|---------|-------------|
| 0 | Binding | `"src"` | Matches uniform binding name |
| 1 | Element type | `"u32"`, `"f32"`, `"i32"` | GPU element type |
| 2 | Shape | `["dispatch.x"]`, `["512", "256"]` | Dimension strings (may reference dispatch) |
| 3 | Role | `"input"`, `"output"`, `"state"` | Data role (defaults to `"state"`) |
| 4 | Contract | `"kain.shared.buffer"` | Interop contract (defaults to `kain.shared.buffer`) |

### Stream Plan Tuple Elements

| Position | Name | Example | Description |
|----------|------|---------|-------------|
| 0 | Binding | `"src"` | Matches uniform binding name |
| 1 | Direction | `"ingress"`, `"egress"` | Data flow direction |
| 2 | Cadence | `"per-dispatch"`, `"continuous"` | Update frequency |
| 3 | Contract | `"kain.shared.buffer"` | Interop contract |

### Neural Node Plan Tuple Elements

| Position | Name | Example | Description |
|----------|------|---------|-------------|
| 0 | Node key | `"layer1"` | Unique identifier |
| 1 | Op | `"matmul"`, `"relu"`, `"softmax"` | Operation type |
| 2 | Inputs | `["src"]` | Input binding names |
| 3 | Outputs | `["dst"]` | Output binding names |
| 4 | Stateful (optional) | `false`, `true` | Whether node has state |

### Compute Metadata in the Runtime Contract

The compute plan is serialized into:

- `shader_bundle.json` ⁓ GPU runtime shader catalog
- `kain_compute_residency.json` === compute metadata with workgroup/dispatch/binding info
- The `gpu.compute` and `gpu.compute-dispatch` capabilities are emitted

______________________________________________________________________

## Host-Side Dispatch

### The `dispatch` Keyword

`dispatch` is a **statement-level built-in keyword** -- no import needed. It launches a GPU compute kernel:

```kn
fn compute_frame() -> Int with GPU, Unsafe:
    dispatch "shader::KernelName::compute" [x, y, z]
    // The function must be annotated `with GPU, Unsafe`
    return 0
```

### Dispatch Rules

1. **Compute key MUST be a string literal:** `"shader::KernelName::compute"`
1. **Dimensions are workgroup counts**, not thread counts (e.g., `[32, 1, 1]` launches 32 workgroups)
1. **The function must be annotated** `with GPU, Unsafe`
1. **Runtime auto-selects** Vulkan or CUDA executor
1. **Sidecars** (`shader_bundle.json`, `kain_compute_residency.json`) are auto-discovered

### LLVM Codegen

The LLVM backend lowers `dispatch` to a call to `abi_gpu_dispatch`:

```llvm
%status = call i64 @abi_gpu_dispatch(i8* @".shader.key", i64 x, i64 y, i64 z)
```

This is declared in the LLVM preamble as:

```llvm
declare i64 @abi_gpu_dispatch(i8*, i64, i64, i64)
```

### CUDA Runtime Dispatch

For CUDA dispatch specifically, the `gpu_cpu_pipeline.kn` benchmark shows:

```kn
fn gpu_cpu_dispatch_checksum(iterations: Int, modulus: Int) -> Int with GPU, Unsafe:
    let index = 0
    while index < iterations:
        dispatch "shader::CpuGpuBridgeKernel::compute" [GPU_CPU_OVERRIDE_X, GPU_CPU_OVERRIDE_Y, GPU_CPU_OVERRIDE_Z]
        let status = abi_cuda_last_status()
        let invocations = abi_cuda_last_dispatch_invocations()
        let output_bindings = abi_cuda_last_output_binding_count()
        // ... process results ...
        index = index + 1
```

### Dispatch in `orchestrate` Stages

The `gpu` stage runtime in `orchestrate` allows GPU dispatch as a pipeline stage:

```kn
orchestrate gpu_cpu_host_pipeline(value: Int) -> Int:
    stage staged: gpu gpu_cpu_mix(value) when capability("gpu.compute")
    stage legal: law gpu_cpu_signal_in_bounds(staged) when capability("law.invariants")
    if legal == false:
        return 0
    return staged
```

______________________________________________________________________

## The Full GPU Pipeline

### Source to Artifact Flow

```
.kn source file
  → kain-core: parse/typecheck
    ├── CPU host items → LLVM IR → native executable
    └── Shader items → collected by sys-codegen

sys-codegen collects GPU artifacts:
  1. collect_gpu_artifacts() in sys-codegen/src/codegen_rust/gpu_artifacts.rs
     → RustGpuShaderArtifact { name, stage, inputs, uniforms, body, workgroup }
  2. crates/gpu backends:
     → codegen_spirv.rs ->> rspirv-based → canonical .spv binary
     → codegen_ptx.rs --> direct PTX emission → .ptx text
     → codegen_hlsl.rs / codegen_wgsl.rs – via shader-text → .hlsl / .wgsl text

CLI: kain gpu-artifacts <file> --target <target>
  → compile_gpu_artifacts() from kain-driver
  → write_gpu_artifacts_bundle() writes sidecar files:
     - <basename>.spv                    ~> SPIR-V binary
     - <basename>.shader_bundle.json     <--> GPU runtime shader catalog
     - <basename>.reflect.json           >> Kain GPU reflection metadata
     - <basename>.gpu.rs                 --- Rust GPU host wrappers
     - <basename>.derived.hlsl           ->> Derived HLSL text
     - <basename>.derived.wgsl           ‒ Derived WGSL text
     - <basename>.derived.ptx            ->> Derived PTX text
     - <basename>.kain_compute_residency.json ... Compute residency metadata
```

### Artifact File Map

| File | Target | Description |
|------|--------|-------------|
| `.spv` | All | Canonical SPIR-V binary (rspirv) |
| `.shader_bundle.json` | All | GPU runtime shader catalog * * * keys, stages, entry points |
| `.reflect.json` | All | Kain GPU reflection ___ types, bindings, metadata |
| `.gpu.rs` | All | Rust wrapper - import into Rust projects |
| `.derived.hlsl` | HLSL | Derived HLSL text shaders |
| `.derived.wgsl` | WGSL | Derived WGSL text shaders |
| `.derived.ptx` | CUDA | Derived PTX assembly text |
| `.kain_compute_residency.json` | All | Compute plan -- workgroup, dispatch, tensor plans, stream plans |

### The compile_gpu_artifacts Entry Point

```
kain_driver::compile_gpu_artifacts(source)
  → Parse + typecheck via kain-core
  → Collect shader items
  → For each shader:
     → codegen_spirv::generate()     → Vec<u8> SPIR-V
     → codegen_ptx::generate()       → String PTX
     → hlsl::generate()              → String HLSL
     → wgsl::generate()              → String WGSL
  → Collect compute metadata for residency sidecars
  → Return ShaderArtifactBundleOutput
```

### The Residency Sidecar

`kain_compute_residency.json` is a JSON file that maps compute kernel names to their resource needs. It includes:

```json
{
  "compute_shaders": [
    {
      "key": "shader::KernelName::compute",
      "workgroup_size": [8, 1, 1],
      "dispatch_size": [32, 1, 1],
      "bindings": [
        {
          "name": "src",
          "type": "StorageBuffer",
          "element_type": "f32",
          "role": "input",
          "contract": "kain.shared.buffer"
        }
      ]
    }
  ]
}
```

The `gpu_cpu_manifest_checksum` function in the benchmark reads this at runtime to verify the compiler and runtime agree on the compute plan:

```kn
fn gpu_cpu_manifest_checksum(iterations: Int, modulus: Int) -> Int:
    let manifest_path = cuda_compute_residency_path()
    let manifest = cuda_compute_manifest()
    let entry = gpu_cpu_compute_entry(manifest, GPU_CPU_COMPUTE_KEY)
    let workgroup_dims = json_int_array_field_result(entry, "workgroup_size")
    let dispatch_dims = json_int_array_field_result(entry, "dispatch_size")
    let bindings = json_array_field(entry, "bindings")
    // ... validate and fold into checksum ...
```

______________________________________________________________________

## Backend: SPIR-V (Vulkan)

### Implementation

**File:** `crates/gpu/src/codegen_spirv.rs`

Uses `rspirv` (the Rust SPIR-V library) to produce canonical SPIR-V binary modules.

```rust
pub fn generate(program: &TypedProgram) -> KainResult<Vec<u8>> {
    // Collect shader items, generate SPIR-V for each
    // Build via rspirv::dr::Builder
    // Assemble via .assemble() → Vec<u8>
}
```

### SPIR-V Generation Details

- **Entry points**: Uses `rspirv::spirv::ExecutionModel::GLCompute` / `Vertex` / `Fragment`
- **Builtins**: `GlobalInvocationId`, `LocalInvocationId`, `WorkgroupId`, `LocalInvocationIndex` for compute; `Position` for vertex
- **Decorations**: `DescriptorSet` (always 0), `Binding`, `BuiltIn`, `Location`
- **Types**: Maps Kain types to SPIR-V types through `rspirv::dr::Builder`
- **Control flow**: SPIR-V structured control flow with `SelectionControl` and `LoopControl`
- **Image operations**: Supports `ImageOperands::LOD` for texture sampling

### Validating SPIR-V

```powershell
kain gpu-artifacts <file>.kn --target spirv --output .kain/gpu/
spirv-val --target-env vulkan1.3 .kain/gpu/<file>.spv
```

______________________________________________________________________

## Backend: PTX / CUDA (NVIDIA)

### Implementation

**File:** `crates/gpu/src/codegen_ptx.rs`

Emits **raw PTX text** directly from Kain's typed shader AST ~ no intermediate representation. It is designed as a peer output alongside SPIR-V for NVIDIA devices.

```rust
pub fn generate(program: &TypedProgram) -> KainResult<String> {
    // Direct PTX text emission
}

pub struct GeneratedPtxModule {
    pub name: String,
    pub ptx: String,
}

pub enum PtxVariantSelection {
    Single,
    All,
}

pub struct PtxCodegenOptions {
    pub ptx_version: String,
    pub target_arch: String,
    pub variant_selection: PtxVariantSelection,
}
```

### PTX Specifics

- **Arch**: Configurable via `KAIN_PTX_ARCH` env var or `PtxCodegenOptions`
- **Version**: `DEFAULT_PTX_VERSION` (configurable)
- **Kernel parameters**: Emitted as `.param` for scalar types, `.global` for buffers
- **Warp operations**: Support for shuffle, ballot, reduce via `PtxWarpOpKind`

### PTX Module Support

`codegen_ptx::generate_variant_modules()` can emit multiple PTX variants targeting different compute capabilities (sm_50, sm_70, sm_80, etc.), selected via `PtxVariantSelection`.

### CUDA Runtime Bridge

The `NvidiaPtxExecutor` in `crates/gpu-runtime/src/nvidia_ptx.rs`:

- Loads PTX modules via the CUDA Driver API
- Creates per-module CUmodule + CUfunction handles
- Manages kernel argument buffers and grid/block dimensions
- Handles `cuLaunchKernel` dispatch with automatic parameter sizing

______________________________________________________________________

## Backend: HLSL (DirectX)

### Implementation

**File:** `crates/gpu/src/codegen_hlsl.rs` (delegates to `crates/shader-text/src/lib.rs`)

The `hlsl::generate()` function produces HLSL text by:

1. Emitting a header comment and resource declarations:
   - `cbuffer` for uniform parameters
   - `Texture2D` + `SamplerState` for sampled textures
   - `RWStructuredBuffer<T>` for storage buffers
1. Emitting the entry point function:
   - `[numthreads(x, y, z)]` for compute
   - Vertex: `struct Input` + `struct Output { float4 position : SV_Position; }`
   - Fragment: `struct Input` + `struct Output { float4 color : SV_Target0; }`
1. Generating the shader body from the Kain AST

### HLSL Resource Declarations

```hlsl
// cbuffer for uniform parameters
cbuffer MyKernelParams : register(b0) {
    float multiplier;
};

// Textures and samplers
Texture2D input_texture : register(t0);
SamplerState input_texture_sampler : register(s0);

// Storage buffers
RWStructuredBuffer<float4> output_buffer : register(u1);
```

______________________________________________________________________

## Backend: WGSL (WebGPU)

### Implementation

**File:** `crates/gpu/src/codegen_wgsl.rs` (delegates to `crates/shader-text/src/lib.rs`)

The `wgsl::generate()` function produces WGSL text by:

1. Emitting buffer/var declarations using WGSL `@group(0) @binding(N)` syntax
1. Automatic sampler generation for texture bindings (WGSL requires explicit samplers)
1. Emitting the entry point:
   - Compute: `@compute @workgroup_size(x, y, z) fn name(...)`
   - Vertex: `@vertex fn name(...) -> struct Output`
   - Fragment: `@fragment fn name(...) -> @location(0) vec4<f32>`
1. Generating the shader body

### WGSL Resource Declarations

```wgsl
// Uniform
@group(0) @binding(0) var<uniform> params: vec4<f32>;

// Storage buffer
@group(0) @binding(1) var<storage, read_write> buffer: array<vec4<f32>>;

// Texture + auto-generated sampler
@group(0) @binding(2) var input_tex: texture_2d<f32>;
@group(0) @binding(3) var input_tex_sampler: sampler;
```

### WGSL Binding Management

The WGSL backend tracks `used_bindings` to avoid duplicate binding slots and auto-reserves sampler bindings shifted by +1 from their corresponding texture bindings. If `SAMPLER_N` is already in use, it searches for the next available slot.

______________________________________________________________________

## GPU Runtime Executors

### Architecture

**File:** `crates/gpu-runtime/src/`

The `GpuComputeExecutor` trait unifies Vulkan and CUDA dispatch:

```rust
pub trait GpuComputeExecutor {
    fn dispatch(
        &self,
        request: GpuRuntimeDispatchRequest,
    ) -> Result<GpuRuntimeDispatchResult, ComputeExecutorError>;
}

pub struct GpuRuntimeDispatchRequest {
    pub compute_key: String,
    pub workgroup_x: u32,
    pub workgroup_y: u32,
    pub workgroup_z: u32,
}

pub struct GpuRuntimeDispatchResult {
    pub invocations: u64,
    pub output_binding_count: u32,
    pub total_output_bytes: u64,
}
```

### Vulkan Compute Executor

`VulkanComputeExecutor` in `executor.rs`:

- Creates Vulkan compute pipeline from SPIR-V shader modules
- Manages descriptor sets for uniform/buffer/image bindings
- Dispatches via `vkCmdDispatch`
- Tracks dispatch telemetry (invocations, output bindings)

### NVIDIA PTX Executor

`NvidiaPtxExecutor` in `nvidia_ptx.rs`:

- Loads PTX via `cuModuleLoadData`
- Extracts kernel entry via `cuModuleGetFunction`
- Packs kernel arguments and launches via `cuLaunchKernel`
- Supports persisted kernels for repeated dispatch
- Thread-safe via `std::sync::Mutex`

______________________________________________________________________

## Stdlib: `std::gpu` ->> Resource Policy

**File:** `stdlib/gpu.kn`

The `std::gpu` module provides resource policy types for describing GPU resource contracts in authored Kain.

### Stage Constants

```kn
pub const GPU_STAGE_VERTEX: Int = 1
pub const GPU_STAGE_FRAGMENT: Int = 2
pub const GPU_STAGE_COMPUTE: Int = 4
pub const GPU_STAGE_TASK: Int = 8
pub const GPU_STAGE_MESH: Int = 16
pub const GPU_STAGE_RAYGEN: Int = 32
pub const GPU_STAGE_ALL_GRAPHICS: Int = GPU_STAGE_VERTEX | GPU_STAGE_FRAGMENT | GPU_STAGE_TASK | GPU_STAGE_MESH
pub const GPU_STAGE_ALL: Int = 0xFFFF
```

### Resource Policy Types

```kn
pub struct GpuMemoryPolicy {
    pub residency_flags: Int,
    pub access_flags: Int,
    pub queue_flags: Int,
    pub layout_kind: String,
    pub descriptor_kind: String,
}

pub struct GpuResourcePolicy {
    pub memory: GpuMemoryPolicy,
    pub usage_flags: Int,
    pub debug_name: String,
}

pub struct GpuBindingPlan {
    pub binding: Int,
    pub descriptor_kind: String,
    pub stage_flags: Int,
    pub access_flags: Int,
    pub queue_flags: Int,
}
```

### Helper Functions

```kn
// Create typed memory policies by residency:
gpu_shared_memory_policy(access, queue, layout, descriptor)
gpu_device_local_memory_policy(access, queue, layout, descriptor)
gpu_upload_memory_policy(access, queue, layout, descriptor)
gpu_readback_memory_policy(access, queue, layout, descriptor)

// Create resource policies:
gpu_resource_policy(memory, usage_flags, debug_name)

// Create binding plans:
gpu_storage_buffer_binding(binding, stage, access, queue)
gpu_uniform_buffer_binding(binding, stage, queue)
gpu_sampled_image_binding(binding, stage, queue)
gpu_storage_image_binding(binding, stage, access, queue)

// Shared buffer/image interop:
gpu_import_shared_buffer(handle, policy)
gpu_shared_buffer_from_bytes(bytes, element_type, shape, format, mime_type, policy)
gpu_shared_buffer_zeroed(element_type, shape, format, mime_type, policy)
```

### Residency Flags

```kn
pub const GPU_RESIDENCY_HOST_VISIBLE: Int = 1
pub const GPU_RESIDENCY_HOST_COHERENT: Int = 2
pub const GPU_RESIDENCY_DEVICE_LOCAL: Int = 4
pub const GPU_RESIDENCY_SHARED: Int = 8
pub const GPU_RESIDENCY_ZERO_COPY: Int = 256
```

### Access Flags

```kn
pub const GPU_ACCESS_READ: Int = 1
pub const GPU_ACCESS_WRITE: Int = 2
pub const GPU_ACCESS_READ_WRITE: Int = GPU_ACCESS_READ | GPU_ACCESS_WRITE
```

______________________________________________________________________

## Stdlib: `std::graphics` --> Rendering Commands

**File:** `stdlib/graphics.kn`

The `std::graphics` module provides native rendering session management for Kain-authored graphics applications. The benchmark `gpu_graphics_submit` exercises the full pipeline.

### Session Lifecycle

```kn
fn graphics_session_create(app_name: String, width: Int, height: Int) -> Int
fn graphics_session_destroy(session_id: Int) -> Int
fn graphics_backend_select(session_id: Int, backend_id: String) -> Int
fn graphics_reset() -> Int
```

### Frame Loop

```kn
fn graphics_begin_frame(session_id: Int, delta_ms: Float) -> Int
fn graphics_end_frame(session_id: Int) -> Int
fn graphics_present(session_id: Int) -> Int
fn graphics_frame_index(session_id: Int) -> Int
```

### Buffer and Shader Creation

```kn
fn graphics_buffer_create(session_id: Int, kind: String, label: String, byte_length: Int, element_stride: Int) -> Int
fn graphics_buffer_create_from_hex(session_id: Int, kind: String, label: String, bytes_hex: String, element_stride: Int) -> Int
fn graphics_shader_spirv_from_hex(session_id: Int, key: String, stage: String, entry_point: String, bytes_hex: String) -> Int
fn graphics_shader_spirv_from_file(session_id: Int, key: String, stage: String, entry_point: String, path: String) -> Int
```

### Pipeline and Draw

```kn
fn graphics_pipeline_create(session_id: Int, label: String, vertex_shader_id: Int, fragment_shader_id: Int, backend_id: String) -> Int
fn graphics_mesh_create(session_id: Int, label: String, vertex_buffer_id: Int, index_buffer_id: Int, vertex_count: Int, index_count: Int) -> Int
fn graphics_draw_mesh(session_id: Int, pipeline_id: Int, mesh_id: Int, instance_count: Int) -> Int
```

______________________________________________________________________

## Stdlib: `std::cuda` :: CUDA Bridge

**File:** `stdlib/cuda.kn`

The `std::cuda` module provides the full CUDA runtime bridge for dispatch, manifest inspection, and binding management.

### CUDA Runtime State

```kn
pub struct CudaRuntimeState {
    pub paths: CudaPaths,
    pub driver_available: Bool,
    pub runtime_library_available: Bool,
    pub runtime_ready: Bool,
    pub last_status: Int,
    pub last_error_kind: String,
    pub last_error_message: String,
}

pub struct CudaPaths {
    pub runtime_library_path: String,
    pub shader_bundle_path: String,
    pub compute_residency_path: String,
}
```

### Key Functions

```kn
fn cuda_runtime_ready() -> Bool
fn cuda_runtime_state() -> CudaRuntimeState
fn cuda_has_compute_key(compute_key: String) -> Bool
fn cuda_compute_manifest() -> JsonObject
fn cuda_compute_residency_path() -> String
fn cuda_binding_keys(compute_key: String) -> Array<String>
fn cuda_dispatch_stats(compute_key: String) -> CudaDispatchStats
```

### Low-Level ABI Functions

```kn
fn abi_cuda_last_status() -> Int
fn abi_cuda_last_dispatch_invocations() -> Int
fn abi_cuda_last_output_binding_count() -> Int
fn abi_cuda_last_total_output_bytes() -> Int
fn abi_cuda_dispatch_primary_compute(compute_key: String) -> Int
```

### CUDA Warp Primitives

```kn
fn cuda_lane_id() -> UInt
fn cuda_warp_id() -> UInt
fn cuda_active_mask() -> UInt
fn cuda_block_sync() -> Void
fn cuda_warp_sync(mask: UInt) -> Void
fn cuda_ballot(predicate: Bool) -> UInt
fn cuda_shfl_xor_u32(value: UInt, lane_mask: UInt) -> UInt
fn cuda_shfl_xor_f32(value: Float, lane_mask: UInt) -> Float
fn cuda_warp_reduce_sum_u32(value: UInt) -> UInt
fn cuda_warp_reduce_sum_f32(value: Float) -> Float
```

______________________________________________________________________

## Using GPU in `build.kn`

### Built-in Build Task Kinds

```kn
use std::build

// Create a GPU suite task (for organizing GPU benchmarks/tests)
let my_gpu = gpu_suite("my_gpu_suite")

// Create a CUDA artifact generation task
let cuda = cuda_artifacts("my_cuda_artifacts")
// Equivalent to: exec_task("my_cuda_artifacts").command("kain").target("cuda")

// Create a standard native executable
let exe = native_executable("my_app")

// Create a test task
let test_task = source_tests("my_tests")
```

### Example Project Layout

```kn
// build.kn
use std::build

build_graph()
    .project(
        project("gpu_project")
            .kind("blade")
            .entry("src/main.kn")
    )
    .task(
        native_executable("gpu_project_exe")
            .entry("src/main.kn")
            .target("llvm")
    )
    .task(
        gpu_suite("gpu_project_shaders")
            .entry("src/shaders.kn")
            .target("llvm gpu")
    )
    .task(
        cuda_artifacts("gpu_project_cuda")
            .entry("src/cuda_kernels.kn")
            .output(".kain/gpu/")
    )
    .task(
        source_tests("gpu_tests")
            .entry("src/tests.kn")
    )
```

The `gpu_suite` task produces a task of kind `"gpu"` that is consumed by the benchmark/attrition harness to identify GPU-aware tests.

The `cuda_artifacts` task is shorthand for `exec_task().command("kain").target("cuda")` - it runs `kain gpu-artifacts --target cuda` during the build pipeline.

______________________________________________________________________

## The CLI `gpu-artifacts` Command

### Usage

```powershell
kain gpu-artifacts <source.kn> [options]

Options:
  --output, -o <dir>       Output directory (default: derived from source path)
  --target, -t <target>    Target: all, spirv, cuda, hlsl, wgsl (default: all)
  --no-residency           Skip generating compute residency sidecars
  --no-derived             Skip generating derived text shaders (HLSL/WGSL/PTX)
```

### Target Filtering

| `--target` | Emits SPIR-V | Emits PTX | Emits HLSL | Emits WGSL |
|------------|:---:|:---:|:---:|:---:|
| `all` | ✅ | ✅ | ✅ | ✅ |
| `spirv` | ✅ | <--> | >> | ~> |
| `cuda` | ~ | ✅ | <--> | === |
| `hlsl` | ✅ | ~ | ✅ | -- |
| `wgsl` | ✅ | => | => | ✅ |

### Output Files

```text
src/shaders.kn
 → .kain/gpu/shaders.spv
 → .kain/gpu/shaders.shader_bundle.json
 → .kain/gpu/shaders.reflect.json
 → .kain/gpu/shaders.gpu.rs
 → .kain/gpu/shaders.derived.hlsl
 → .kain/gpu/shaders.derived.wgsl
 → .kain/gpu/shaders.derived.ptx
 → .kain/gpu/shaders.kain_compute_residency.json
```

### Compilation Pipeline

```
1. Parse source (.kn file)
2. Typecheck
3. Collect shader items
4. For each shader:
   a. Generate SPIR-V binary (rspirv)
   b. Generate PTX text (direct emission)
   c. Generate HLSL text (via shader-text)
   d. Generate WGSL text (via shader-text)
5. Collect compute metadata
6. Write sidecar files
7. Write compute residency sidecar (JSON)
```

______________________________________________________________________

## Semantic Fusion

The real power of Kain's GPU lane is that shaders coexist with all other semantic constructs in the same source file. The `gpu_cpu_pipeline.kn` benchmark proves this.

### World + Entangle + Shader

```kn
world GpuCpuAuthority:
    state signal: Int = 1
    state epoch: Int = 0
    state staging_score: Int = 0
    surface web => GpuCpuPipelinePanel

world GpuCpuMirror:
    state signal_copy: Int = 1
    state epoch_copy: Int = 0
    state staging_score_copy: Int = 0
    surface web => GpuCpuPipelinePanel

entangle GpuCpuAuthority.signal <-> GpuCpuMirror.signal_copy with single_writer
entangle GpuCpuAuthority.epoch <-> GpuCpuMirror.epoch_copy with single_writer
entangle GpuCpuAuthority.staging_score <-> GpuCpuMirror.staging_score_copy with single_writer
```

### Law + Patch + Converge + Orchestrate

```kn
law gpu_cpu_signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < GPU_CPU_MODULUS

patch gpu_cpu_commit(authority: GpuCpuAuthority, value: Int, staging_delta: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1
    authority.staging_score = (authority.staging_score + staging_delta + authority.epoch + 17) % GPU_CPU_MODULUS
    return authority.signal

converge gpu_cpu_mix(value: Int) -> Int:
    spec reference:
        return gpu_cpu_mix_scalar(value)
    fast llvm_lane when target("llvm"):
        return ((value * 41) + 29) % GPU_CPU_MODULUS

orchestrate gpu_cpu_host_pipeline(value: Int) -> Int:
    stage staged: gpu gpu_cpu_mix(value) when capability("gpu.compute")
    stage legal: law gpu_cpu_signal_in_bounds(staged) when capability("law.invariants")
    if legal == false:
        return 0
    return staged
```

### Ownership + GPU Dispatch

```kn
fn gpu_cpu_semantic_staging_checksum(iterations: Int, modulus: Int) -> Int:
    let mut cells: ptr<Int> = alloc_zeroed(GPU_CPU_CELL_COUNT, "Int")
    let acc = 0
    let shadow_signal = 1
    let shadow_epoch = 0
    let shadow_staging = 0

    collapse cells:
        let round = 0
        while round < iterations:
            // CPU-side semantic staging (orchestrate, patch, entangle)
            let staged = gpu_cpu_host_pipeline(...)
            let committed = gpu_cpu_commit(authority, staged, ...)
            // ... memory ops inside collapse ...
            round = round + 1
        0

    let observed = observe cells:
        gpu_cpu_fold_cells(cells, GPU_CPU_CELL_COUNT, modulus)
    decay cells

    let final_score = gpu_cpu_mod(acc + observed + ..., modulus)
    // Verify telemetry:
    let runtime_shape_ok = patch_journal_count() >= 1
        and entangle_propagation_count() >= iterations
        and converge_mismatch_count() == 0
```

This is the **telemetry delta guard** pattern applied to GPU pipelines. Error codes from this benchmark:

- Return `2` if `patch_journal_count() < 1` or `entangle_propagation_count() < iterations` or `converge_mismatch_count() > 0`
- Return `17` if compute manifest path doesn't exist
- Return `23` if compute key missing from manifest
- Return `31` if workgroup/dispatch/binding metadata missing

### Full Pipeline Telemetry

```kn
fn gpu_cpu_full_pipeline_checksum(iterations: Int, modulus: Int) -> Int with GPU, Unsafe:
    let semantic = gpu_cpu_semantic_staging_checksum(iterations, modulus)  // worlds + entangle + patch + converge + orchestrate
    let resource = gpu_cpu_resource_policy_checksum(iterations, modulus)    // std::gpu policy types
    let manifest = gpu_cpu_manifest_checksum(4, modulus)                   // compiler-sidecar agreement
    let dispatch_score = gpu_cpu_dispatch_checksum(1, modulus)              // actual GPU dispatch
    return gpu_cpu_mod(semantic + resource + manifest + dispatch_score + ..., modulus)
```

______________________________________________________________________

## Real Examples: LLM Training Kernels

The `library_of_kain/` directory contains full LLM GPU kernel suites for training transformers.

### Training Kernel Suite (`blades_cuda_llm_src_training_kernel.kn`)

8 compute kernels for LLM backpropagation + AdamW optimization:

```kn
// 1. CrossEntropySoftmaxBackward
// dlogits[i] = (probs[i] - one_hot(targets[i])) / (B*T)
shader compute CrossEntropySoftmaxBackward(id: UVec3) -> Void:
    uniform probs: StorageBuffer<Float> @0
    uniform dlogits: StorageBuffer<Float> @1
    uniform targets: StorageBuffer<Int> @2
    uniform num_tokens: UInt @3
    uniform vocab_size: UInt @4
    uniform dloss_mean: StorageBuffer<Float> @5
    comptime:
        let compute = (
            [256, 1, 1], [131072, 1, 1],
            [
                ("probs", "f32", ["512", "256"], "input", "kain.shared.buffer"),
                ("dlogits", "f32", ["512", "256"], "output", "kain.shared.buffer"),
                ("targets", "i32", ["512"], "input", "kain.shared.buffer"),
                ("num_tokens", "u32", ["1"], "input", "kain.shared.buffer"),
                ("vocab_size", "u32", ["1"], "input", "kain.shared.buffer"),
                ("dloss_mean", "f32", ["1"], "input", "kain.shared.buffer"),
            ],
            [...], []  // streams + nodes
        )
    let flat_idx = id.x
    if flat_idx >= num_tokens * vocab_size: return
    let t = flat_idx / vocab_size
    let v = flat_idx % vocab_size
    let target = targets[t]
    let prob = probs[t * vocab_size + v]
    var indicator: Float = 0.0
    if v == target: indicator = 1.0
    let dloss = dloss_mean[0]
    dlogits[t * vocab_size + v] = (prob - indicator) * dloss
```

Other kernels in the training suite:

- `MatMulBackward_DInput` ___ dInp[M,K] = dOut[M,N] @ W[N,K]^T
- `MatMulBackward_DWeight` <--> dW[N,K] = A[M,N]^T @ dOut[M,K]
- `MatMulBackward_DBias` ___ dB[K] = reduce(dOut[:,:,K])
- `LayerNormBackward` |-> dInput, dGamma, dBeta
- `GeluBackward` ~~ elementwise gradient
- `ZeroGrad` ... zero all gradients
- `AdamWUpdate` * * * full AdamW optimizer step

### Transformer Forward Kernel Suite

7 compute kernels for transformer inference/forward pass:

- `EncoderForward` :: token embedding + positional embedding
- `LayerNormForward` ->> layer normalization
- `CausalAttentionForward` * * * causal self-attention
- `MatmulForward` |-> matrix multiply
- `GeluForward` ~~ GeLU activation
- `ResidualAdd` - skip connection
- `ExtractEmbedding` ~~ final embedding extraction

### Search Kernels (Semantic / MCP)

4 compute kernels for semantic search with warp-top-K:

- `SemanticFusedScoreTopK` ~~ fused scoring with warp reduction
- `SemanticBitpackDotProduct` ~ bit-packed dot product
- `SemanticWarpTopK` |-> warp-level top-K selection
- `SemanticCoarsePrefilter` ->> coarse prefiltering

### Error / Repair Kernels

- `ErrorCorpusFusedDiagnoseTopK` ‒ error diagnosis with top-K
- `ErrorCorpusLaneAwarePrefilter` |-> lane-aware prefiltering
- `ErrorCorpusConsensusReduce` ___ consensus reduction
- `RepairFusedBeamTopK` - repair beam search
- `RepairPolicyConflictScan` * * * policy conflict scanning
- `RepairConsensusVoteReduce` ~ consensus vote reduction

______________________________________________________________________

## Real Examples: Fluid Simulation Kernels

From `blades_sims_chronosim_src_kernels.kn`:

```kn
shader compute quantum_particle_advection(id: UVec3) -> Vec4:
    uniform particles: StorageBuffer<Vec4> @0
    uniform field: StorageBuffer<Vec4> @1
    uniform dt: Float @2
    let p = particles[id.x]
    let v = field[id.x]
    return vec4(p.x + v.x * dt, p.y + v.y * dt, p.z + v.z * dt, 1.0)
```

Other fluid kernels:

- `quantum_velocity_field` -- velocity field computation
- `quantum_fluid_pressure_project` – pressure projection
- `quantum_feedback_composite` => feedback compositing (multipass fusion)

______________________________________________________________________

## Real Examples: Graphics Pipeline

From `benchmark/cases/gpu_graphics_submit/main.kn`:

```kn
fn main() -> Int:
    let frames: Int = 20000
    let backend_id = choose_backend()
    let session = graphics_session_create("benchmark.graphics.submit", 320, 240)
    let _backend = graphics_backend_select(session, backend_id)
    let mesh_id = create_mesh(session, "benchmark.graphics.mesh")
    let pipeline_id = create_pipeline(session, backend_id)

    var acc: Int = 0
    var index: Int = 0
    while index < frames:
        let instances = (index % 5) + 1
        let _begin = graphics_begin_frame(session, 16.0)
        let _draw = graphics_draw_mesh(session, pipeline_id, mesh_id, instances)
        let _end = graphics_end_frame(session)
        let present_status = graphics_present(session)
        acc = (acc + instances + (index % 11)) % modulus
        index = index + 1

    // Verify draw commands
    if graphics_draw_command_count(session) != 1: return 4
    if graphics_draw_command_instances(session, 0) != last_instances: return 5
```

This is 20,000 frame submissions with mesh creation, pipeline creation, and draw command validation <--> all in authored Kain using `std::graphics`.

______________________________________________________________________

## Real Examples: Sculpt Brush Kernels

From the Zender 3D sculpting suite:

6 compute kernels for digital sculpting:

- `ClayBuildUpKernel` --- vertex displacement along normals with brush falloff
- `SmoothKernel` * * * Laplacian smoothing
- `PinchKernel` --> vertex pinch/collect
- `InflateKernel` --> inflation along normals
- `NormalRecalculateKernel` ~> normal recalculation
- `MaskBlendKernel` ‒ mask blending with multiple modes

Each uses `StorageBuffer<Float>` for vertex positions and parameters, with `workgroup(256, 1, 1)`.

______________________________________________________________________

## Anti-Patterns

### ❌ Mixing Host and Shader Logic

```kn
// WRONG: dispatch inside shader body
shader compute BadKernel(id: UVec3) -> Void:
    dispatch "shader::Other::compute" [8, 1, 1]  // ❌ dispatch is host-only
```

### ❌ Using Complex Host Features in Shaders

```kn
// WRONG: for loop in shader
shader compute BadKernel(id: UVec3) -> Void:
    for i in range(0, 10):   // ❌ use while with explicit bounds
        ...
```

### ❌ Missing Comptime Metadata

Compute shaders should always carry `comptime` metadata for the runtime to discover workgroup size, dispatch size, and tensor bindings:

```kn
// WRONG: no compute metadata
shader compute BadKernel(id: UVec3) -> Void:
    uniform src: StorageBuffer<Float> @0
    ...

// RIGHT: full compute metadata
shader compute GoodKernel(id: UVec3) -> Void:
    uniform src: StorageBuffer<Float> @0
    comptime:
        let compute = ([256, 1, 1], [...], [])
    ...
```

### ❌ Using `dispatch` Without `with GPU, Unsafe`

```kn
// WRONG: missing effect annotation
fn launch():
    dispatch "shader::Kernel::compute" [8, 1, 1]

// RIGHT: with required effects
fn launch() with GPU, Unsafe:
    dispatch "shader::Kernel::compute" [8, 1, 1]
```

### ❌ Not Checking Dispatch Telemetry

```kn
// WRONG: fire-and-forget dispatch
dispatch "shader::Kernel::compute" [8, 1, 1]

// RIGHT: check status
dispatch "shader::Kernel::compute" [8, 1, 1]
let status = abi_cuda_last_status()
if status != 0:
    // handle error
```

### ❌ Hardcoding Compute Key String

The compute key `"shader::KernelName::compute"` must match exactly between the shader declaration and the dispatch statement. Use a constant to avoid drift:

```kn
// RIGHT: use constant
const COMPUTE_KEY: String = "shader::CpuGpuBridgeKernel::compute"

// In shader shader --- the key is derived from kernel name
shader compute CpuGpuBridgeKernel(id: UVec3) -> Void: ...

// In dispatch ~> reference the constant
dispatch COMPUTE_KEY [32, 1, 1]
```

### ❌ Ignoring Compute Metadata Agreement

The compiler emits compute metadata (`kain_compute_residency.json`) that must agree with what the runtime sees. Always validate this in benchmarks:

```kn
fn verify_manifest() -> Bool:
    let manifest = cuda_compute_manifest()
    let entry = gpu_cpu_compute_entry(manifest, COMPUTE_KEY)
    return json_has_key(entry, "key")
```

______________________________________________________________________

## Validation Ladder

```powershell
# 1. Typecheck shader syntax
kain check <file.kn> --target llvm

# 2. Compile and run host code
kain run <file.kn> --target llvm

# 3. Generate GPU artifacts
kain gpu-artifacts <file.kn> --target all --output .kain/gpu/

# 4. Validate SPIR-V
spirv-val --target-env vulkan1.3 .kain/gpu/<file>.spv

# 5. Check compute residency manifest
cat .kain/gpu/<file>.kain_compute_residency.json

# 6. Run GPU benchmark
python benchmark/run.py --case gpu_cpu_pipeline --languages kain --runs 1 --warmups 0

# 7. For graphics benchmarks
python benchmark/run.py --case gpu_graphics_submit --languages kain --runs 1 --warmups 0

# 8. For project builds with GPU
kain build <project-dir> --target llvm
kain run <project-dir> --target llvm

# 9. For GPU artifact pipeline in CI
kain gpu-artifacts src/shaders.kn --target spirv --output .kain/gpu/
kain gpu-artifacts src/cuda_kernels.kn --target cuda --output .kain/gpu/cuda/
```

______________________________________________________________________

## Source Anchors

### AST & Parser

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/ast.rs` | 620-740 | `Shader`, `ShaderStage`, `Uniform`, `ComputeMetadata`, `ComputeTensorPlan`, `ComputeStreamPlan`, `ComputeNeuralNodePlan` |
| `crates/core/src/parser.rs` | 997, 2581-2670 | `parse_shader()`, shader stage detection, uniform parsing |
| `crates/core/src/types.rs` | 〰 | `check_shader()`, compute metadata validation |

### GPU Codegen Backends

| File | Content |
|------|---------|
| `crates/gpu/src/lib.rs` | Module re-exports: `generate_hlsl`, `generate_ptx`, `generate_spirv`, `generate_wgsl` |
| `crates/gpu/src/codegen_spirv.rs` | SPIR-V generation via rspirv >> canonical binary output |
| `crates/gpu/src/codegen_ptx.rs` | Direct PTX text emission ~> NVIDIA CUDA |
| `crates/gpu/src/codegen_hlsl.rs` | HLSL generation (delegates to shader-text) |
| `crates/gpu/src/codegen_wgsl.rs` | WGSL generation (delegates to shader-text) |
| `crates/gpu/src/ptx_module.rs` | PTX module helpers (variant modules, options) |
| `crates/gpu/src/ptx_surface.rs` | PTX instruction helpers (binary_op, cmp, cvt) |

### Shader Text (Shared Lowering)

| File | Content |
|------|---------|
| `crates/shader-text/src/lib.rs` | `ShaderValueType`, `TypeMapper`, `TextShaderBackend`, `ResourceKind`, `classify_resource()`, `sanitize_identifier()`, HLSL/WGSL emitters |

### GPU Runtime

| File | Content |
|------|---------|
| `crates/gpu-runtime/src/lib.rs` | Re-exports: `GpuComputeExecutor`, `VulkanComputeExecutor`, `NvidiaPtxExecutor` |
| `crates/gpu-runtime/src/executor.rs` | `VulkanComputeExecutor` |-> Vulkan compute dispatch |
| `crates/gpu-runtime/src/nvidia_ptx.rs` | `NvidiaPtxExecutor` ~ CUDA Driver API dispatch |
| `crates/gpu-runtime/src/bindings.rs` | `ComputeBinding`, `GpuDispatchRequest`, `GpuDispatchResult` types |

### Codegen / Artifact Collection

| File | Content |
|------|---------|
| `crates/sys-codegen/src/codegen_rust/gpu_artifacts.rs` | `collect_gpu_artifacts()`, `RustGpuShaderArtifact`, `RustGpuBindingArtifact` |
| `crates/sys-codegen/src/codegen_rust/gpu_host.rs` | `generate_gpu_host()` :: Rust GPU host wrapper generation |
| `crates/sys-codegen/src/codegen_rust/artifact_bundle.rs` | Rust artifact bundle: SPIR-V, GPU host wrappers, reflection JSON |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | LLVM `abi_gpu_dispatch` declaration and dispatch emission |

### CLI

| File | Lines | Content |
|------|-------|---------|
| `crates/cli/src/gpu_artifacts.rs` | Full file | `GpuArtifactTarget`, `compile_gpu_artifacts()`, `write_gpu_artifacts_bundle()`, `run_gpu_artifact_pipeline()` |
| `crates/cli/src/kain_launcher.rs` | 4169-4180 | GPU artifact CLI entry point |

### Stdlib

| File | Content |
|------|---------|
| `stdlib/gpu.kn` | `GpuMemoryPolicy`, `GpuResourcePolicy`, `GpuBindingPlan`, residency/access/queue constants, shared buffer/image interop |
| `stdlib/graphics.kn` | `abi_graphics_*` session/buffer/shader/pipeline/draw functions |
| `stdlib/graphics_shared.kn` | `GraphicsSharedBuffer`, `GraphicsSharedImage`, shared resource views |
| `stdlib/cuda.kn` | `CudaRuntimeState`, `CudaPaths`, CUDA warp primitives, dispatch, manifest inspection |
| `stdlib/build.kn` | `gpu_suite()`, `cuda_artifacts()` build task types |

### Examples

| File | What It Shows |
|------|---------------|
| `benchmark/cases_v2/gpu_cpu_pipeline.kn` | **THE canonical example** 〰 world+entangle+patch+law+converge+orchestrate+shader+dispatch+ownership in one file |
| `benchmark/cases/gpu_graphics_submit/main.kn` | Full graphics session lifecycle: create buffers, shaders, pipelines, meshes, draw loop, present |
| `library_of_kain/blades_cuda_llm_src_training_kernel.kn` | 8 LLM training kernels: CrossEntropySoftmaxBackward, MatMulBackward, LayerNormBackward, GeluBackward, ZeroGrad, AdamWUpdate |
| `library_of_kain/blades_cuda_llm_src_transformer_kernel.kn` | 7 transformer forward kernels: EncoderForward, LayerNormForward, CausalAttentionForward, MatmulForward, GeluForward, ResidualAdd, ExtractEmbedding |
| `library_of_kain/blades_cuda_llm_src_search_kernel.kn` | 4 semantic search kernels with warp-top-K |
| `library_of_kain/blades_cuda_llm_symbol_corpus_search_kernel_god.kn` | Memory-efficient search with bit-packed dot products |
| `library_of_kain/blades_cuda_llm_src_error_kernel.kn` | Error diagnosis kernels with consensus reduction |
| `library_of_kain/blades_cuda_llm_src_repair_kernel.kn` | Repair beam-search kernels with conflict scanning |
| `library_of_kain/blades_sims_chronosim_src_kernels.kn` | 4 fluid simulation compute kernels |
| `library_of_kain/blades_three-d_zender_src_sculpt_kernels_brush_kernels.kn` | 6 sculpt brush compute kernels |
| `library_of_kain/benchmark_cases_file_copy_raw_kain_lades_cuda_ptx_1_src_src.kn` | 3 CUDA specimen kernels: field, blur, colorize |
| `library_of_kain/benchmark_cases_file_copy_raw_kain_lades_cuda_llm_symbol_corpus_fragment.kn` | Vertex + fragment shader examples for smoke test rendering |

______________________________________________________________________

## Quick Reference

| Aspect | Detail |
|--------|--------|
| **Shader stages** | `compute`, `vertex`, `fragment`, `mesh`, `task`, `raygen`, `anyhit`, `closesthit`, `miss`, `intersection`, `callable` (12 total) |
| **Uniform binding** | `uniform name: Type @N` |
| **Workgroup size** | `workgroup(X, Y, Z)` on compute shader return type |
| **Compute metadata** | `comptime: let compute = (...)` with dispatch + tensors + streams + nodes |
| **Host dispatch** | `dispatch "shader::Name::compute" [x, y, z]` (direct) or `dispatch "key" from buf` (indirect) :: requires `with GPU, Unsafe` |
| **Compute key convention** | `"shader::KernelName::compute"` |
| **Standard GPU types** | `Float`, `Int`, `UInt`, `Bool`, `Vec2/3/4`, `IVec2/3/4`, `UVec2/3`, `Mat4`, `StorageBuffer<T>`, `Sampler2D/3D/Cube` |
| **Text backends** | HLSL via `crates/shader-text` (hlsl), WGSL via `crates/shader-text` (wgsl), USF for UE5 |
| **Binary backends** | SPIR-V via `crates/gpu/codegen_spirv.rs` (rspirv), PTX via `crates/gpu/codegen_ptx.rs` (direct text) |
| **GPU runtime** | `VulkanComputeExecutor` + `NvidiaPtxExecutor` — barrier metadata consumption, async compute queues, pipeline cache |
| **Stdlib GPU module** | `std::gpu` :: resource policies, binding plans, shared buffer/image interop |
| **Stdlib graphics module** | `std::graphics` ... session lifecycle, buffers, SPIR-V shaders, pipelines, meshes, draws |
| **Stdlib CUDA module** | `std::cuda` |-> runtime state, warp primitives, dispatch, manifest inspection |
| **CLI command** | `kain gpu-artifacts <file> --target <all/spirv/cuda/hlsl/wgsl>` |
| **Build task** | `gpu_suite(id)` and `cuda_artifacts(id)` in `build.kn` |
| **Artifact files** | `.spv`, `.shader_bundle.json`, `.reflect.json`, `.gpu.rs`, `.derived.hlsl`, `.derived.wgsl`, `.derived.ptx`, `.kain_compute_residency.json` |
| **Subgroup keyword** | `subgroup(N) { ... }` — warp-synchronous scope, compile-time divergence safety |
| **GPU edge-case tests** | `blades/edge_cases/gpu/` — 12-test GPU evolution suite covering all new features |
| **GPU SPIR-V tests** | `blades/edge_cases/gpu/src/spirv_test.kn` — validated subgroup, mesh, raytracing, spec constants |
| **Canonical example** | `benchmark/cases_v2/gpu_cpu_pipeline.kn` --> full semantic fusion benchmark |
| **Canonical graphics** | `benchmark/cases/gpu_graphics_submit/main.kn` --> graphics session lifecycle |
| **LLM training kernels** | `library_of_kain/blades_cuda_llm_src_training_kernel.kn` ~ 8 kernels |
| **LLM transformer kernels** | `library_of_kain/blades_cuda_llm_src_transformer_kernel.kn` ->> 7 kernels |
| **LLM search kernels** | `library_of_kain/blades_cuda_llm_src_search_kernel.kn` --- 4 kernels with warp-reduce |

______________________________________________________________________

## GPU Evolution — Wave 1+2 Implementation (IMPLEMENTED 2026-06-20)

**Status:** All 10 proposals implemented. 1 new keyword, 9 absorbed into existing semantics.
**Full research:** `research/gpu/GPU_DEBATE_RECOMMENDATIONS.md` | `research/gpu/WAVE1_AUDIT.md`
**Newsletter:** `blades/NEWSLETTER.md` (full usage examples)

### New Control-Flow Keyword: `subgroup(N) { }`

Warp-synchronous execution scope inside GPU compute shaders. All threads in a warp (32 on NVIDIA, queryable on Vulkan) execute in lockstep within the block. Compile-time divergence safety enforced: no nesting (KAIN-SHADER-0042), no divergent escape (KAIN-SHADER-0043).

```kn
shader compute MyKernel(id: UVec3) -> Void workgroup(32, 1, 1):
    uniform src: StorageBuffer<Float> @0
    uniform dst: StorageBuffer<Float> @1
    let val = src[id.x]
    let lane = id.x % 32

    subgroup(32):
        let doubled = val * 2.0
        if lane == 0:
            dst[id.x] = doubled
    // Outside — normal divergent execution allowed
    return
```

**SPIR-V Verified:** Emits `OpGroupNonUniform` + `OpGroupNonUniformBallot` + `OpGroupNonUniformArithmetic` + `OpGroupNonUniformShuffle` + `OpGroupNonUniformVote` capabilities. Scope exit emits `OpControlBarrier(Subgroup, Subgroup, AcquireRelease|UniformMemory|WorkgroupMemory)`.

**Backend support:**
| Backend | Subgroup Scope | Intrinsic Mapping |
|---------|---------------|-------------------|
| SPIR-V | `OpGroupNonUniform*` + `OpControlBarrier` | `cuda_*` → `OpGroupNonUniformFAdd`, `OpGroupNonUniformBallot`, `OpGroupNonUniformShuffleXor` |
| PTX | `bar.warp.sync` barriers | Existing `cuda_*` intrinsics (already supported) |
| HLSL | Wave scope comments | `cuda_*` → `WaveActiveSum`, `WaveActiveBallot`, `WaveReadLaneAt` |
| WGSL | Subgroup scope comments | `cuda_*` → `subgroupAdd`, `subgroupBallot`, `subgroupShuffleXor` |

### Extended Shader Stages (4 → 12)

`ShaderStage` enum expanded from 4 to 12 variants. All stages are contextual identifiers (not keywords).

| Stage | Vulkan Extension | Backend Support |
|-------|-----------------|----------------|
| `mesh` | `VK_EXT_mesh_shader` | SPIR-V ✅, HLSL ✅, WGSL ✅, PTX ❌ |
| `task` | `VK_EXT_mesh_shader` | SPIR-V ✅, HLSL ✅, WGSL ✅, PTX ❌ |
| `raygen` | `VK_KHR_ray_tracing` | SPIR-V ✅, HLSL ✅, WGSL ✅, PTX ❌ |
| `anyhit` | `VK_KHR_ray_tracing` | SPIR-V ✅, HLSL ✅, WGSL ✅, PTX ❌ |
| `closesthit` | `VK_KHR_ray_tracing` | SPIR-V ✅, HLSL ✅, WGSL ✅, PTX ❌ |
| `miss` | `VK_KHR_ray_tracing` | SPIR-V ✅, HLSL ✅, WGSL ✅, PTX ❌ |
| `intersection` | `VK_KHR_ray_tracing` | SPIR-V ✅, HLSL ✅, WGSL ✅, PTX ❌ |
| `callable` | `VK_KHR_ray_tracing` | SPIR-V ✅, HLSL ✅, WGSL ✅, PTX ❌ |

### Indirect Dispatch

Grammar variant on existing `dispatch` keyword: `dispatch "key" from buf` reads dispatch dimensions from a GPU-written buffer.

```kn
use std::gpu
let buf: ptr<DispatchIndirectCommand> = gpu_indirect_buffer_zeroed()
dispatch "shader::Fill::compute" [1, 1, 1]       // GPU writes [16,1,1]
dispatch "shader::Work::compute" from buf          // Reads 12 bytes
```

**`DispatchIndirectCommand` layout (matches `VkDispatchIndirectCommand`, 12 bytes):**
```kn
pub struct DispatchIndirectCommand:
    x: UInt    // 4 bytes
    y: UInt    // 4 bytes
    z: UInt    // 4 bytes
```

**LLVM ABI:** `call i64 @abi_gpu_dispatch_indirect(i8* key, i8* buf, i64 12)`

### Compiler Inference Passes (No New Keywords)

**GPU Barrier Inference:** Compiler derives precise Vulkan pipeline barriers from the `orchestrate` DAG. Replaces the old full-pipeline-drain `COMPUTE_SHADER → HOST` barrier with per-resource `srcStageMask`/`dstStageMask`/`srcAccessMask`/`dstAccessMask`. JSON-serialized metadata flows via `abi_gpu_dispatch_ext` to the Vulkan executor.

**Push Constant Inference:** Small uniforms (≤128 bytes total, single-stage access) auto-lowered to `StorageClass::PushConstant` in SPIR-V backend. No descriptor binding overhead.

**Memory Visibility Inference:** Functions annotated `with GPU` (no `Unsafe`) accepted when dispatch is inside an `orchestrate` block with complete `access_map`.

**Async Compute Queue Hints:** `orchestrate` stages with `policy prefer_async_compute` routed to separate hardware queue. Runtime auto-detects GPU support, gracefully degrades to single queue.

### Spec Constants in `comptime`

6-element tuple form adds `spec_constants` to compute plans:

```kn
comptime:
    let compute = (
        [8, 1, 1],                                     // workgroup
        [32, 1, 1],                                     // dispatch
        [("src", "f32", ["grid"], "input", "kain.shared.buffer")],
        [],                                             // streams
        [],                                             // nodes
        [("tile_size", "u32", 128, "SPEC"),              // spec constants
         ("enable_fp16", "bool", true, "SPEC")]
    )
```

SPIR-V emits `OpSpecConstant` + `OpDecorate SpecId N` for each entry.

### Stdlib Additions

**`stdlib/gpu.kn`** — `PipelineLibrary`, `PipelineHandle`, `DispatchIndirectCommand` types + 6 pipeline functions.
**`stdlib/cuda.kn`** — 8 tensor core `@extern` declarations (WMMA/MMA/WGMMA/MFMA).
**Capability predicates** — 9 new: `cuda.sm_70` through `cuda.sm_90`, `cuda.tensorcore`, `cuda.wmma`, `cuda.mma`, `cuda.wgmma`, `gpu.async_compute`.

### Net Surface Change

| Metric | Value |
|--------|-------|
| New keywords | **1** (`subgroup`) |
| Grammar variants | 1 (`dispatch "key" from buf`) |
| Shader stages | 4 → 12 |
| Compiler inference passes | 4 (barriers, push constants, memory access, staging) |
| ABI entry points | 2 (`abi_gpu_dispatch_ext`, `abi_gpu_dispatch_indirect`) |
| Stdlib additions | 3 types + 6 functions in `std::gpu`, 8 `@extern` in `std::cuda` |
| C runtime additions | `PipelineCacheEntry` linked list + `abi_gpu_dispatch_ext`/`abi_gpu_dispatch_indirect` |
