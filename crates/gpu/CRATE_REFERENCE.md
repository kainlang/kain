# gpu — GPU Backend Crates Reference

> **Last Updated:** 2026-03-01
> **Status:** Production. SPIR-V via rspirv. HLSL via direct text generation. Both target `TypedShader` items.

---

## Purpose

GPU compilation targets for KAIN shaders. Both backends operate exclusively on `TypedShader` items extracted from a `TypedProgram`.

| Target | Backend | Output |
|---|---|---|
| `spirv` | SPIR-V via `rspirv` | `.spv` binary bytes |
| `hlsl` | Direct text gen | `.hlsl` / `.usf` text |

Note: The UE5-specific shader target (`usf`) is handled by **`ue5-shaders`**, not this crate. This crate provides the generic cross-platform GPU backends.

---

## Source Files

| File | Size | Purpose |
|---|---|---|
| `codegen_spirv.rs` | 30KB | SPIR-V binary generation via `rspirv::dr::Builder` |
| `codegen_hlsl.rs` | 37KB | Direct HLSL text emission with full intrinsic mapping |

---

## Public API

```rust
pub use codegen_spirv::generate as generate_spirv; // -> KainResult<Vec<u8>>
pub use codegen_hlsl::generate  as generate_hlsl;  // -> KainResult<String>
```

---

## SPIR-V Backend (`codegen_spirv.rs`, 30KB)

Uses **`rspirv`** (the Rust SPIR-V library) for structured IR building rather than raw binary encoding.

### Architecture

```
TypedProgram
  → for each TypedShader item:
    → emit_shader(builder, shader)
      → configure ExecutionModel, Capability, MemoryModel
      → emit uniform input/output variables
      → emit_block → emit_expr
      → builder.assemble() → Vec<u8>
```

### `ShaderContext`

Per-shader context carries:
- `builder: &mut Builder`
- `locals: HashMap<String, u32>` — SPIR-V `word` IDs for local variables
- `glsl_ext: Option<u32>` — GLSL extended instruction set (lazy-loaded via `get_glsl_ext()`)

### Shader Stage → SPIR-V Execution Model

| KAIN stage | SPIR-V `ExecutionModel` |
|---|---|
| `Vertex` | `ExecutionModel::Vertex` |
| `Fragment` | `ExecutionModel::Fragment` + `OriginUpperLeft` |
| `Compute` | `ExecutionModel::GLCompute` + `LocalSize(16,16,1)` |
| `Surface` | `ExecutionModel::Fragment` |

### Type Mapping (`map_ast_type`)

| KAIN type | SPIR-V type |
|---|---|
| `Float` | `OpTypeFloat 32` |
| `Vec2/Vec3/Vec4` | `OpTypeVector float N` |
| `Mat4` | `OpTypeMatrix vec4 4` |
| `Int` | `OpTypeInt 32 1` |
| `Bool` | `OpTypeBool` |
| `Unit` | `OpTypeVoid` |

### Uniform Handling

Each `shader uniform` → `OpVariable` in `StorageClass::Uniform` with `Decoration::Binding` and `Decoration::DescriptorSet(0)`.

### Expression Emission (`emit_expr`)

Returns `(word_id, Type)` pair for each expression. Supports:
- Arithmetic: `OpFAdd`, `OpFMul`, `OpFSub`, `OpFDiv`, `OpIMul`, etc.
- Comparisons
- Float/Int literals
- Variable loads (`OpLoad`)
- Struct field access
- Function calls → GLSL extended instructions (math intrinsics)
- `Vec2/Vec3/Vec4` constructors via `OpCompositeConstruct`

### Cross-Platform via `naga`

SPIR-V output from this backend can be fed through `naga` (not in this crate) to produce WGSL, GLSL, or Metal Shading Language.

---

## HLSL Backend (`codegen_hlsl.rs`, 37KB)

Direct HLSL text generation — no SPIR-V middleman. This gives more control over the DirectX-specific output and is reused/adapted by `ue5-shaders` for `.usf` generation.

### Architecture

```
TypedProgram
  → for each TypedShader:
    → emit_shader(shader)
      → generate struct types (cbuffer, in/out structs)
      → generate global uniforms (Texture2D, SamplerState, cbuffer registers)
      → generate entry point function (VSMain/PSMain/CSMain)
      → emit_block → emit_stmt → emit_expr
```

### `HLSLContext`

Carries `indent_level: usize` and `output: Vec<String>` lines. Methods: `indent()`, `push_indent()`, `pop_indent()`.

### Shader Stage → HLSL Entry Point

| KAIN stage | HLSL |
|---|---|
| `Vertex` | `VSInput/VSOutput` structs + `VSMain(input: VSInput): VSOutput` |
| `Fragment` | `PSInput/PSOutput` structs + `PSMain(input: PSInput): PSOutput` |
| `Compute` | `[numthreads(16,16,1)] void CSMain(uint3 tid: SV_DispatchThreadID)` |

### Uniform Classification

| Uniform type | HLSL output |
|---|---|
| `Sampler2D` / `Texture` | `Texture2D name: register(t0)` / `SamplerState name_sampler: register(s0)` |
| `Float`, `Vec*`, `Int` | `cbuffer Constants : register(b0) { float name; }` |

### `emit_function_call` (400+ lines)

The heart of the HLSL intrinsic library. Maps KAIN function names to HLSL intrinsics:

| KAIN call | HLSL |
|---|---|
| `sample(tex, uv)` | `tex.Sample(tex_sampler, uv)` |
| `dot(a, b)` | `dot(a, b)` |
| `lerp(a, b, t)` | `lerp(a, b, t)` |
| `normalize(v)` | `normalize(v)` |
| `saturate(x)` | `saturate(x)` |
| `sqrt(x)`, `abs(x)`, `log(x)`, `exp(x)` | Direct HLSL |
| `pow(b, e)` | `pow(b, e)` |
| `clamp(x, lo, hi)` | `clamp(x, lo, hi)` |
| `sin()`, `cos()`, `tan()` | HLSL trig |
| `min/max` | HLSL min/max |
| `cross(a, b)` | `cross(a, b)` |
| `length(v)` | `length(v)` |
| `distance(a, b)` | `distance(a, b)` |
| `floor/ceil/round/frac` | HLSL equivalents |
| Vector constructors `vec2(x,y)` | `float2(x, y)` |
| Matrix `mat4(...)` | `float4x4(...)` |
| Swizzle `.rgb`, `.xyz`, etc. | HLSL swizzle syntax |

### Type Mapping (`map_type_to_hlsl`)

```
Float     → float
Vec2/3/4  → float2/3/4
Mat4      → float4x4
Int       → int
Bool      → bool
Sampler2D → Texture2D (+ SamplerState)
Unit      → void
```

### `infer_swizzle_type`

Infers the return type of a HLSL swizzle access based on swizzle string length:
- 1 component (`.r`, `.x`) → `float`
- 2 components (`.rg`, `.xy`) → `float2`
- 3 components (`.rgb`, `.xyz`) → `float3`
- 4 components (`.rgba`, `.xyzw`) → `float4`

---

## Known Gaps

| Gap | Notes |
|---|---|
| SPIR-V no `OpImage*` | Texture sampling via SPIR-V not yet implemented |
| SPIR-V `compute` dispatch size | Hardcoded `LocalSize(16,16,1)` — not driven by KAIN thread_id annotation |
| HLSL no `#include` directives | Common shared constants must be inlined |
| HLSL no `StructuredBuffer<T>` | Only `Texture2D` + scalars supported as uniforms |

---

## Dependencies

| Crate | Role |
|---|---|
| `kain-core` | `TypedProgram`, `TypedShader`, `AST`, effects |
| `rspirv` | SPIR-V module IR builder + binary assembler |
| `serde` / `serde_json` | Metadata support |
