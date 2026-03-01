# ue5-shaders — UE5 Shader Codegen Reference

> **Last Updated:** 2026-03-01
> **Status:** Production — the largest codebase in the UE5 suite. Handles compute, fragment, vertex, and surface shaders for USF output.

---

## Purpose

Generates UE5 Shader Format (`.usf`) code from KAIN `shader` items. Also generates the C++ `FGlobalShader` / `FMeshMaterialShader` subclasses, dispatch helpers, and POD mirror structs used to pass parameters from CPU to GPU.

---

## Source Files

| File | Size | Purpose |
|---|---|---|
| `codegen_usf.rs` | 189KB | Main USF codegen — all shader types |
| `validation.rs` | 136KB | Shader validation — type safety, resource binding, permutations |
| `pod_mirror.rs` | 30KB | POD C++ struct generation for shader parameters |
| `shader_knowledge.rs` | 19KB | Data-driven shader type registry |
| `type_mapping.rs` | 10KB | KAIN types → HLSL/USF types |

---

## Public API

```rust
pub fn generate(program: &TypedProgram) -> KainResult<ShaderOutput>

pub struct ShaderOutput {
    pub usf_source: String,           // .usf shader text
    pub cpp_header: String,           // FGlobalShader subclass .h
    pub cpp_source: String,           // Implementation .cpp
    pub shared_ush: Option<String>,   // Shared {Plugin}Common.ush
}
```

---

## Supported Shader Types

| KAIN | USF output | C++ class |
|---|---|---|
| `shader compute Name(thread_id: Vec3)` | `[numthreads(X,Y,Z)] void Name_CS(...)` | `FNameCS : public FGlobalShader` |
| `shader fragment Name(uv: Vec2) -> Vec4` | `void Name_PS(FPSInput In, out FPSOutput Out)` | `FNamePS : public FGlobalShader` |
| `shader vertex Name(pos: Vec3) -> Vec4` | `void Name_VS(FVSInput In, out FVSOutput Out)` | `FNameVS : public FGlobalShader` |
| `shader surface Name` | Surface expression graph | `FMeshMaterialShader` subclass |

---

## USF Generation (`codegen_usf.rs`)

### Header Structure

All generated `.usf` files start with:
```hlsl
#include "Platform.ush"
#include "{Plugin}Common.ush"   // if multi-shader plugin
```

### Uniform Classification

Shader uniforms are split into two output types:
- **Texture uniforms** (`Sampler2D`, `Texture2D`, `RWTexture2D`) → `Texture2D Name : register(t0)`; texture excluded from dispatch signature
- **Scalar uniforms** (`Float`, `Vec*`, `Int`, `Bool`) → `cbuffer ShaderConstants : register(b0) { ... }`

### Compute Shader Pipeline

For compute shaders:
1. `uniform buffer_name: RWBuffer<T> @slot` → `RWStructuredBuffer<T> buffer_name : register(u0)`
2. `uniform grid_size: Int @0` → `int grid_size` in cbuffer
3. Dispatch helper function generated: `void Dispatch_Name(FRHICommandListImmediate& RHICommandList, int X, int Y, int Z)`
4. UAV resource transitions handled at dispatch time

### Shader Permutations

Uniforms with `CFG_` or `ENABLE_` prefix generate permutation compile-time branches:

```kain
uniform CFG_ENABLE_FOG: Bool @3
```

→

```hlsl
#define ENABLE_FOG_PERMUTATION_BOOL
SHADER_PERMUTATION_BOOL("ENABLE_FOG");
```

Zero runtime cost — compile-time branches.

### Shared Shader Library

When a plugin has >1 shader, a `{Plugin}Common.ush` is auto-generated containing shared math helpers:
- `IsInBounds(float3 pos, float3 bounds)`
- `PixelToUV(float2 pixel, float2 texSize)`
- `HashNoise(float3 pos)`
- `Grayscale(float3 color)`

---

## Validation (`validation.rs`, 136KB)

Pre-codegen shader validation:

| Check | Rule |
|---|---|
| Thread group size | Max 1024 total threads (X × Y × Z ≤ 1024) |
| Binding slot uniqueness | Duplicate `@slot` assignments flagged |
| UAV type consistency | `RWTexture2D` and `RWBuffer` cannot share slot with SRV |
| POD struct validation | Shader parameter structs must be plain data (no virtuals, no strings) |
| Conditional shader directory mapping | Duplicate file path assertions |
| Type compatibility | Shader input/output types must match HLSL semantic |

---

## POD Mirror Structs (`pod_mirror.rs`, 30KB)

For compute shaders that read from CPU-side data, POD mirror structs are generated:

```cpp
// Generated POD mirror for KAIN struct ParticleData
struct FParticleData_GPUMirror {
    FVector3f Position;   // float3 packed
    float     Velocity;   // float scalar
    uint32    Flags;      // uint bitfield
};
static_assert(sizeof(FParticleData_GPUMirror) % 16 == 0, "Must be 16-byte aligned");
```

The mirror generator validates:
- No non-POD members
- 16-byte alignment for GPU buffer upload
- Field order matches KAIN struct declaration order

---

## Type Mapping (`type_mapping.rs`)

| KAIN | HLSL/USF |
|---|---|
| `Float` | `float` |
| `Vec2/3/4` | `float2/3/4` |
| `Mat4` | `float4x4` |
| `Int` | `int` |
| `Bool` | `bool` |
| `Sampler2D` | `Texture2D` + `SamplerState` |
| `RWTexture2D` | `RWTexture2D<float4>` |
| `Buffer<T>` | `StructuredBuffer<T>` |
| `RWBuffer<T>` | `RWStructuredBuffer<T>` |

---

## Shader Knowledge (`shader_knowledge.rs`)

Data-driven shader type registry — 19KB of structured metadata covering:
- Supported input/output semantics per shader stage
- Built-in HLSL intrinsic function signatures
- Valid parameter attribute combinations
- Engine-specific USF includes by usage category
