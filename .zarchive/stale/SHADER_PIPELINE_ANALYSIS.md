# KAIN Shader Pipeline Analysis — SPIR-V vs USF vs HLSL

> **Date:** 2026-03-01  
> **Purpose:** Compare the three shader backends and propose consolidation strategy

---

## Current State: Three Shader Backends

KAIN has **three separate shader codegen pipelines**:

| Backend | Crate | Output | Lines | Status | Last Updated |
|---------|-------|--------|-------|--------|--------------|
| **SPIR-V** | `gpu/codegen_spirv.rs` | `.spv` binary | ~1000 | ⚠️ Stale | Pre-2025 |
| **HLSL** | `gpu/codegen_hlsl.rs` | `.hlsl` text | ~879 | ⚠️ Stale | Pre-2025 |
| **USF** | `ue5-shaders/codegen_usf.rs` | `.usf` + C++ | ~3000+ | ✅ Active | Feb 2026 |

---

## Architecture Comparison

### SPIR-V Backend (`gpu/codegen_spirv.rs`)

**Approach:** Binary IR generation via `rspirv::dr::Builder`

```
TypedProgram → TypedShader
  ↓
emit_shader(builder, shader)
  ↓
- Configure ExecutionModel (Vertex/Fragment/Compute)
- Emit OpVariable for uniforms (StorageClass::Uniform)
- Emit OpVariable for inputs/outputs (StorageClass::Input/Output)
- emit_block → emit_expr
  ↓
builder.assemble() → Vec<u8> SPIR-V binary
```

**Key Features:**
- Uses `rspirv` crate for structured IR building
- Supports GLSL extended instructions (math intrinsics)
- Outputs cross-platform SPIR-V binary
- Can be transpiled to WGSL/GLSL/Metal via `naga`

**Type Mapping:**
```rust
Float → OpTypeFloat 32
Vec2/3/4 → OpTypeVector float N
Mat4 → OpTypeMatrix vec4 4
Int → OpTypeInt 32 1
Bool → OpTypeBool
```

**Uniform Handling:**
- Samplers: `StorageClass::UniformConstant`
- Data uniforms: Wrapped in struct with `Block` decoration
- Matrices: `ColMajor` + `MatrixStride(16)` decorations
- Storage buffers: `StorageClass::StorageBuffer`

**Compute Built-ins:**
```rust
global_invocation_id → OpBuiltIn GlobalInvocationId
local_invocation_id → OpBuiltIn LocalInvocationId
workgroup_id → OpBuiltIn WorkgroupId
local_invocation_index → OpBuiltIn LocalInvocationIndex
```

**Known Gaps:**
- ❌ No texture sampling (OpImage* instructions not implemented)
- ❌ Hardcoded LocalSize(8,8,1) for compute
- ❌ No shader permutations
- ❌ No UE5-specific features
- ❌ No C++ reflection header generation

---

### HLSL Backend (`gpu/codegen_hlsl.rs`)

**Approach:** Direct HLSL text generation

```
TypedProgram → TypedShader
  ↓
emit_shader(shader)
  ↓
- Generate cbuffer for scalar uniforms
- Generate Texture2D/SamplerState declarations
- Generate input/output structs (VSInput/PSOutput)
- Generate entry point (VSMain/PSMain/CSMain)
- emit_block → emit_stmt → emit_expr
  ↓
String (HLSL source code)
```

**Key Features:**
- Direct HLSL text emission (no IR middleman)
- 400+ lines of intrinsic mapping in `emit_function_call`
- Supports vertex/fragment/compute stages
- Swizzle support (`.rgb`, `.xyz`, etc.)

**Type Mapping:**
```rust
Float → float
Vec2/3/4 → float2/3/4
Mat4 → float4x4
Int → int
Bool → bool
Sampler2D → Texture2D + SamplerState
```

**Uniform Classification:**
```rust
Texture/Sampler → Texture2D name : register(t0)
                  SamplerState name_sampler : register(s0)
Scalars → cbuffer ShaderParams : register(b0) { float name; }
Buffers → StructuredBuffer<T> name : register(u0)
```

**Intrinsic Library (400+ lines):**
- Trig: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`
- Math: `abs`, `floor`, `ceil`, `round`, `frac`, `sqrt`, `pow`, `exp`, `log`
- Vector: `length`, `distance`, `normalize`, `dot`, `cross`, `reflect`, `refract`
- Interpolation: `lerp`, `clamp`, `smoothstep`, `saturate`
- Texture: `sample`, `sample_lod`, `sample_grad`, `sample_bias`, `load`
- Derivatives: `ddx`, `ddy`, `fwidth`
- Constructors: `vec2/3/4`, `mat4`

**Known Gaps:**
- ❌ No UE5-specific features (no `Platform.ush` includes)
- ❌ No C++ reflection header generation
- ❌ No shader permutations
- ❌ No RDG integration
- ❌ No `StructuredBuffer<T>` support (only Texture2D)

---

### USF Backend (`ue5-shaders/codegen_usf.rs`)

**Approach:** UE5-specific shader + C++ reflection generation

```
TypedProgram → TypedShader
  ↓
generate_usf(program) → .usf file
generate_cpp_header(program) → .h file (FGlobalShader + FParameters)
generate_cpp_implementation(program) → .cpp file (IMPLEMENT_GLOBAL_SHADER)
  ↓
Full UE5 shader pipeline:
- .usf with #include "/Engine/Public/Platform.ush"
- C++ FGlobalShader subclass
- SHADER_PARAMETER_STRUCT with RDG bindings
- Exec() helper for RDG dispatch
- Permutation domain support
```

**Key Features:**
- ✅ Full UE5 integration (Platform.ush, RDG, FGlobalShader)
- ✅ C++ reflection header generation
- ✅ Shader permutations (`CFG_*`/`ENABLE_*` → `SHADER_PERMUTATION_BOOL`)
- ✅ RDG resource management (FRDGTextureUAV, FRDGTexture)
- ✅ POD struct mirroring for component data
- ✅ Shared shader libraries (`.ush` generation)
- ✅ TypeMapper (unified KAIN→HLSL type mapping)
- ✅ Validation (ShaderValidator)
- ✅ Compute/Fragment/Vertex/Surface stages
- ✅ Array literals (`[a, b, c]` → `static const float arr[] = {a, b, c}`)
- ✅ Cast expressions (`expr as Float` → `(float)expr`)

**Type Mapping (via TypeMapper):**
```rust
Float → float
Vec2/3/4 → float2/3/4
Mat4 → float4x4
Int → int
UInt → uint
Bool → bool
Image2D → RWTexture2D<float4>
Buffer<T> → StructuredBuffer<T>
RWBuffer<T> → RWStructuredBuffer<T>
```

**Uniform Classification:**
```rust
Permutations (CFG_*/ENABLE_*) → SHADER_PERMUTATION_BOOL (compile-time)
Textures → SHADER_PARAMETER_RDG_TEXTURE + SHADER_PARAMETER_SAMPLER
UAVs → SHADER_PARAMETER_RDG_TEXTURE_UAV
Scalars → SHADER_PARAMETER or SHADER_PARAMETER_STRUCT
```

**C++ Reflection Header:**
```cpp
class FMyShaderCS : public FGlobalShader {
    DECLARE_GLOBAL_SHADER(FMyShaderCS);
    SHADER_USE_PARAMETER_STRUCT(FMyShaderCS, FGlobalShader);
    
    // Permutation Domain
    class CFG_FEATURE : SHADER_PERMUTATION_BOOL("CFG_FEATURE");
    using FPermutationDomain = TShaderPermutationDomain<CFG_FEATURE>;
    
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER(float, MyScalar)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MyTexture)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
    END_SHADER_PARAMETER_STRUCT()
    
    static void Exec(FRDGBuilder& GraphBuilder, const FParameters& Parameters, FIntVector GroupCount);
};
```

**Known Gaps:**
- ⚠️ No cross-platform SPIR-V output (UE5-only)
- ⚠️ No standalone HLSL output (always generates UE5 wrapper)

---

## Feature Matrix

| Feature | SPIR-V | HLSL | USF |
|---------|--------|------|-----|
| **Cross-platform** | ✅ (via naga) | ✅ | ❌ (UE5-only) |
| **Texture sampling** | ❌ | ✅ | ✅ |
| **Compute shaders** | ✅ | ✅ | ✅ |
| **Vertex shaders** | ✅ | ✅ | ✅ |
| **Fragment shaders** | ✅ | ✅ | ✅ |
| **Math intrinsics** | ✅ (GLSL ext) | ✅ (400+ lines) | ✅ (via HLSL) |
| **Shader permutations** | ❌ | ❌ | ✅ |
| **C++ reflection** | ❌ | ❌ | ✅ |
| **RDG integration** | ❌ | ❌ | ✅ |
| **POD struct mirroring** | ❌ | ❌ | ✅ |
| **Shared libraries** | ❌ | ❌ | ✅ (.ush) |
| **Type validation** | ❌ | ❌ | ✅ (ShaderValidator) |
| **Array literals** | ❌ | ❌ | ✅ |
| **Cast expressions** | ❌ | ❌ | ✅ |
| **Last updated** | Pre-2025 | Pre-2025 | Feb 2026 |

---

## Consolidation Strategy

### Option 1: Merge HLSL into USF (Recommended)

**Rationale:**
- USF is the most feature-complete backend (3000+ lines, actively maintained)
- HLSL backend (879 lines) is essentially a subset of USF
- USF already does direct HLSL text generation + UE5 wrappers
- Merging would eliminate code duplication

**Implementation:**
1. Add `--standalone-hlsl` flag to USF backend
2. When flag is set, skip UE5-specific features:
   - No `#include "/Engine/Public/Platform.ush"`
   - No C++ reflection header generation
   - No RDG integration
   - Output pure HLSL with cbuffer/Texture2D/SamplerState
3. Reuse USF's intrinsic library (already has 400+ lines of math functions)
4. Deprecate `gpu/codegen_hlsl.rs`

**Benefits:**
- ✅ Single source of truth for HLSL generation
- ✅ Reuse USF's TypeMapper, validation, array literals, cast expressions
- ✅ Maintain UE5 features while adding standalone HLSL support
- ✅ Reduce maintenance burden (one codebase instead of two)

**Migration Path:**
```rust
// In ue5-shaders/src/codegen_usf.rs
pub fn generate_standalone_hlsl(program: &TypedProgram) -> KainResult<String> {
    // Reuse existing USF codegen but skip UE5-specific features
    generate_usf_internal(program, StandaloneMode::HLSL)
}

enum StandaloneMode {
    UE5,      // Full UE5 integration (current behavior)
    HLSL,     // Pure HLSL output (no UE5 wrappers)
}
```

---

### Option 2: Keep SPIR-V Separate, Merge HLSL into USF

**Rationale:**
- SPIR-V serves a different purpose (cross-platform binary IR)
- SPIR-V can be transpiled to WGSL/GLSL/Metal via `naga`
- HLSL and USF are both text-based HLSL generators (redundant)

**Implementation:**
1. Keep SPIR-V backend in `gpu/` crate (update it to match USF feature parity)
2. Merge HLSL into USF as described in Option 1
3. Update SPIR-V to support:
   - Texture sampling (OpImage* instructions)
   - Shader permutations (via specialization constants)
   - Dynamic compute dispatch sizes

**Benefits:**
- ✅ SPIR-V for cross-platform (Vulkan, WebGPU, Metal via naga)
- ✅ USF for UE5 + standalone HLSL
- ✅ No redundant HLSL backends

**Drawbacks:**
- ⚠️ SPIR-V backend needs significant updates (texture sampling, permutations)
- ⚠️ Maintenance burden of two separate pipelines

---

### Option 3: Full Consolidation into `ue5-shaders`

**Rationale:**
- Move SPIR-V backend into `ue5-shaders` crate
- Rename crate to `kain-shaders` (generic, not UE5-specific)
- Support multiple output modes: SPIR-V, HLSL, USF

**Implementation:**
```rust
// In kain-shaders/src/lib.rs
pub enum ShaderTarget {
    SPIRV,      // Cross-platform binary IR
    HLSL,       // Standalone HLSL text
    USF,        // UE5 shader + C++ reflection
}

pub fn generate(program: &TypedProgram, target: ShaderTarget) -> KainResult<ShaderOutput> {
    match target {
        ShaderTarget::SPIRV => generate_spirv(program),
        ShaderTarget::HLSL => generate_hlsl(program),
        ShaderTarget::USF => generate_usf(program),
    }
}
```

**Benefits:**
- ✅ Single crate for all shader backends
- ✅ Shared infrastructure (TypeMapper, validation, intrinsics)
- ✅ Easier to maintain consistency across targets

**Drawbacks:**
- ⚠️ Large refactor (move SPIR-V from `gpu/` to `ue5-shaders/`)
- ⚠️ Crate rename (`ue5-shaders` → `kain-shaders`)
- ⚠️ Potential breaking changes for existing code

---

## Recommendation: Option 1 (Merge HLSL into USF)

**Why:**
1. **Minimal disruption** — USF is already the production backend
2. **Immediate value** — Adds standalone HLSL support without breaking UE5 workflows
3. **Code reuse** — Leverages USF's 3000+ lines of battle-tested code
4. **Maintainability** — Eliminates redundant HLSL backend (879 lines)
5. **Future-proof** — Keeps SPIR-V separate for cross-platform needs

**Implementation Steps:**

1. **Add `StandaloneMode` enum to USF backend**
   ```rust
   enum StandaloneMode {
       UE5,      // Full UE5 integration (current)
       HLSL,     // Pure HLSL output
   }
   ```

2. **Refactor `generate_usf` to accept mode parameter**
   ```rust
   fn generate_usf_internal(program: &TypedProgram, mode: StandaloneMode) -> KainResult<String> {
       match mode {
           StandaloneMode::UE5 => {
               // Include Platform.ush, RDG, etc.
           }
           StandaloneMode::HLSL => {
               // Skip UE5-specific features
               // Output pure HLSL with cbuffer/Texture2D
           }
       }
   }
   ```

3. **Add public API for standalone HLSL**
   ```rust
   pub fn generate_standalone_hlsl(program: &TypedProgram) -> KainResult<String> {
       generate_usf_internal(program, StandaloneMode::HLSL)
   }
   ```

4. **Update CLI to support `--target hlsl`**
   ```rust
   // In cli/src/main.rs
   match target {
       "hlsl" => {
           let hlsl = ue5_shaders::generate_standalone_hlsl(&program)?;
           fs::write(output_path, hlsl)?;
       }
       "usf" | "ue5" => {
           let usf = ue5_shaders::generate_usf(&program)?;
           // ... existing UE5 logic
       }
   }
   ```

5. **Deprecate `gpu/codegen_hlsl.rs`**
   - Add deprecation notice
   - Update documentation to point to `ue5-shaders::generate_standalone_hlsl`
   - Remove in next major version

6. **Update SPIR-V backend (optional, future work)**
   - Add texture sampling support (OpImage* instructions)
   - Add shader permutation support (specialization constants)
   - Add dynamic compute dispatch sizes

---

## SPIR-V Backend Modernization (Future Work)

If we decide to keep SPIR-V separate, here are the gaps to address:

### 1. Texture Sampling

**Current:** ❌ No OpImage* instructions  
**Needed:** ✅ OpImageSampleImplicitLod, OpImageSampleExplicitLod, OpImageLoad

```rust
// In emit_function_call
"sample" => {
    let (sampler, _) = emit_expr(ctx, &args[0].value)?;
    let (coords, _) = emit_expr(ctx, &args[1].value)?;
    
    // Need to emit:
    // %sampled_image = OpSampledImage %sampled_image_type %texture %sampler
    // %result = OpImageSampleImplicitLod %vec4 %sampled_image %coords
    
    let sampled_image_type = ctx.b.type_sampled_image(texture_type);
    let sampled_image = ctx.b.sampled_image(sampled_image_type, None, texture, sampler).unwrap();
    let result = ctx.b.image_sample_implicit_lod(vec4_type, None, sampled_image, coords, None, vec![]).unwrap();
    Ok((result, Type::Named { name: "Vec4".into(), generics: vec![], span: expr.span() }))
}
```

### 2. Shader Permutations

**Current:** ❌ No permutation support  
**Needed:** ✅ Specialization constants

```rust
// Detect CFG_* / ENABLE_* uniforms
if is_permutation_param(&uniform.name) {
    // Emit OpSpecConstantTrue/False instead of OpVariable
    let spec_id = uniform.binding;
    let bool_type = ctx.b.type_bool();
    let spec_const = ctx.b.spec_constant_true(bool_type);
    ctx.b.decorate(spec_const, Decoration::SpecId, vec![Operand::LiteralBit32(spec_id)]);
    ctx.vars.insert(uniform.name.clone(), VarBinding { id: spec_const, ty: uniform.ty.clone(), is_ptr: false });
}
```

### 3. Dynamic Compute Dispatch

**Current:** ❌ Hardcoded LocalSize(8,8,1)  
**Needed:** ✅ Parse from shader attributes

```rust
// In emit_shader
let (x, y, z) = if let Some(thread_count) = shader.ast.thread_count {
    (thread_count.x, thread_count.y, thread_count.z)
} else {
    (8, 8, 1) // Default
};

ctx.b.execution_mode(
    main_fn,
    ExecutionMode::LocalSize,
    vec![x, y, z],
);
```

---

## Conclusion

**Immediate Action:** Merge HLSL into USF (Option 1)
- Low risk, high value
- Eliminates code duplication
- Adds standalone HLSL support
- Maintains UE5 production workflows

**Future Work:** Modernize SPIR-V backend
- Add texture sampling
- Add shader permutations
- Add dynamic compute dispatch
- Keep as cross-platform option (Vulkan, WebGPU, Metal via naga)

**Long-term Vision:** Three shader targets
1. **SPIR-V** — Cross-platform binary IR (Vulkan, WebGPU, Metal)
2. **HLSL** — Standalone DirectX shaders (via USF backend)
3. **USF** — Full UE5 integration (shaders + C++ reflection + RDG)

---

## Next Steps

1. ✅ Create this analysis document
2. ⏳ Implement `StandaloneMode` in USF backend
3. ⏳ Add `generate_standalone_hlsl()` public API
4. ⏳ Update CLI to support `--target hlsl`
5. ⏳ Deprecate `gpu/codegen_hlsl.rs`
6. ⏳ Write tests for standalone HLSL output
7. ⏳ Update documentation (TECH.md, CRATE_REFERENCE.md)
8. ⏳ (Future) Modernize SPIR-V backend

