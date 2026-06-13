# GPU — Shader + Dispatch (SPEC)

**Target file:** `src/GPU.kn`
**Date:** 2026-06-12
**Budget spec:** Integration guide for the self-host compiler's GPU typechecker + codegen

---

## 1. Architecture Overview

Kain's GPU subsystem has a dual architecture: shader authoring (compile-time) and host dispatch (runtime) in the same `.kn` source.

```
src/GPU.kn
  ├── Shader types: TypedShader, ShaderStage, TypedShaderUniform
  ├── check_shader()         — typecheck shader item (compute/vertex/fragment)
  ├── check_dispatch()       — typecheck dispatch statement
  ├── validate_shader_type() — ensure GPU-compatible type (Vec3, UVec3, etc.)
  ├── validate_workgroup()   — validate workgroup(W, H, D) positive integers
  ├── validate_uniforms()    — validate uniform binding numbers (unique, @N)
  ├── codegen_spirv_builder() — emit SPIR-V binary via FFI or native emit
  ├── codegen_ptx_emit()     — emit PTX text
  ├── codegen_hlsl_emit()    — emit HLSL text
  ├── codegen_wgsl_emit()    — emit WGSL text
  └── emit_compute_residency() — emit compute_residency.json sidecar
```

### 1.1 The Dual Pipeline

```
.kn source → parser/typechecker → split:
  ├── CPU host lane: LLVM IR → native ABI calls → dispatch / std::gpu / std::graphics
  └── Shader lane:   gpu-artifacts pipeline → .spv / .ptx / .hlsl / .wgsl
```

### 1.2 Three Shader Stage Kinds

| Stage | Declaration | Return | Input Convention |
|-------|-------------|--------|-----------------|
| Compute | `shader compute Name(id: UVec3) -> Void` | `Void` | `id: UVec3` = dispatch thread ID |
| Vertex | `shader vertex Name(position: Vec3, uv: Vec2) -> Vec4` | `Vec4` | Vertex attributes as params |
| Fragment | `shader fragment Name(uv: Vec2) -> Vec4` | `Vec4` | Interpolated vertex outputs |

---

## 2. AST Representation

### 2.1 Shader Item (AST_ITEM_SHADER = 22)

```kain
// Flat ast_data layout:
// ast_data[0]  = name_idx (string pool)
// ast_data[1]  = stage_kind (0=compute, 1=vertex, 2=fragment)
// ast_data[2]  = input_count (N)
// ast_data[3..3+2N] = input pairs: [name_idx, type_idx]
// ast_data[3+2N] = uniform_count (M)
// ast_data[3+2N+1..+4M] = uniform quads:
//   [name_idx, type_idx, binding_idx, resource_kind_idx]
// ast_data[3+2N+1+4M]     = workgroup_x (or -1 if none)
// ast_data[3+2N+2+4M]     = workgroup_y
// ast_data[3+2N+3+4M]     = workgroup_z
// ast_data[3+2N+4+4M]     = return_type_idx
// ast_data[3+2N+5+4M]     = comptime_block_idx (or -1 if none)
// ast_data[3+2N+6+4M]     = body_expr_idx
```

### 2.2 Dispatch Statement (AST_STMT_DISPATCH = 59)

```kain
// Dispatch stmt ast_data layout:
// ast_data[0] = compute_key_idx (string pool — "shader::KernelName::compute")
// ast_data[1] = dim_x_expr_idx
// ast_data[2] = dim_y_expr_idx
// ast_data[3] = dim_z_expr_idx
```

### 2.3 GPU-Compatible Types

| Kain type | Allowed in shader? | Uniform binding? | StorageBuffer<T> element? |
|-----------|-------------------|------------------|--------------------------|
| `Void` | return only | no | no |
| `Bool` | yes | no | no |
| `Int` | yes | yes | no |
| `UInt` | yes | yes | no |
| `Float` | yes | yes | no |
| `Vec2` | yes | yes | no |
| `Vec3` | yes | yes | no |
| `Vec4` | yes | yes | no |
| `IVec2` | yes | yes | no |
| `UVec2` | yes | yes | no |
| `UVec3` | yes (id param) | yes | no |
| `Mat4` | yes | yes | no |
| `StorageBuffer<T>` | no (uniform only) | yes | yes |
| `Sampler2D` | no (uniform only) | yes | no |

---

## 3. Typechecking

### 3.1 `check_shader()` — Validate Shader Item

```
Input:  AstNode with kind == AST_ITEM_SHADER
Output: TypedItem { kind: AST_ITEM_SHADER, effects: EFF_GPU, ... }

Algorithm:
  1. Extract name from ast_data[0]. stage from ast_data[1].
  2. Validate stage rules:
     a. Compute (0): return type must be Void. First param must be UVec3 named "id".
     b. Vertex (1): return type must be Vec4. No workgroup allowed.
     c. Fragment (2): return type must be Vec4. No workgroup allowed.
  3. Validate input types from ast_data[3..3+2N]:
     a. Each type must be in the GPU-compatible set (Vec2/3/4, UVec2/3, IVec2, Float, Int, UInt).
     b. ERR_SHADER_INVALID_INPUT_TYPE if not.
  4. Validate uniforms from ast_data[3+2N+1..+4M]:
     a. Each binding number must be unique (no duplicates).
     b. Type must be StorageBuffer<T>, Vec2/3/4, Float, Int, UInt, Mat4, or Sampler2D.
     c. StorageBuffer<T> element type T must be GPU-compatible.
     d. ERR_SHADER_DUPLICATE_BINDING for duplicate @N.
     e. ERR_SHADER_INVALID_UNIFORM_TYPE for unsupported types.
  5. Validate workgroup (compute only):
     a. workgroup_x/y/z must be positive integer constants (>= 1).
     b. ERR_SHADER_ZERO_WORKGROUP if any is 0 or negative.
  6. Validate body restrictions:
     a. Walk body AST recursively.
     b. Reject: AST_EXPR_FOR, AST_EXPR_LOOP, AST_EXPR_DEFER, AST_STMT_DISPATCH.
     c. Reject: AST_EXPR_STRING (string literals), AST_EXPR_FSTRING.
     d. Allow: AST_EXPR_INT/FLOAT/BOOL, ASSIGN, IF, WHILE, RETURN, BREAK, CONTINUE,
              field access, index, binary/unary ops, method calls (swizzle).
  7. Validate comptime block (if present):
     a. Must contain compute metadata tuple [dispatch_size, tensor_plans, ...].
     b. Tensor plan binding names must reference declared uniform names.
  8. Return TypedItem with EFF_GPU effect set.
```

### 3.2 `check_dispatch()` — Validate Dispatch Statement

```
Input:  AstNode with kind == AST_STMT_DISPATCH
Output: ResolvedType::Unit

Algorithm:
  1. Extract compute_key from ast_data[0] string pool.
  2. Verify compute_key matches pattern "shader::KernelName::compute".
     ERR_DISPATCH_INVALID_KEY if malformed.
  3. Verify enclosing function has EFF_GPU in its effects. ERR_MISSING_GPU_EFFECT.
  4. Verify enclosing function has EFF_UNSAFE in its effects. ERR_MISSING_UNSAFE_EFFECT.
  5. Verify dimension expressions are integer-typed:
     check_expr(dim_x), check_expr(dim_y), check_expr(dim_z) must return Int-compatible.
     ERR_DISPATCH_DIM_NOT_INT if not.
  6. (Optional) Look up shader name from compute key in env.
     Verify the shader exists and is a compute shader.
     ERR_SHADER_NOT_FOUND or ERR_SHADER_NOT_COMPUTE.
  7. Return Unit.
```

### 3.3 `validate_shader_type()` — GPU Type Compatibility

```kain
pub fn validate_shader_type(ty: ResolvedType) -> Bool:
    // Returns true if the type is valid for shader parameters or uniforms.
    match ty.kind:
        RT_FLOAT, RT_INT -> true (Int is accepted as i32)
        RT_STRUCT -> true if struct name is one of:
            Vec2, Vec3, Vec4, IVec2, UVec2, UVec3, Mat4
        RT_GENERIC -> true if name is StorageBuffer and inner type is GPU-compatible
        RT_SAMPLER -> true (Sampler2D)
        _ -> false
```

---

## 4. Codegen

### 4.1 Artifact Pipeline

The self-host codegen for shaders produces GPU artifacts rather than LLVM IR.
The high-level flow:

```kain
pub struct GpuArtifactBundle:
    spirv_bytes: Array<Int>     // SPIR-V binary as byte array
    ptx_text: String            // PTX assembly text
    hlsl_text: String           // HLSL shader text
    wgsl_text: String           // WGSL shader text
    compute_residency: String   // JSON metadata

pub fn generate_gpu_artifacts(program: MonomorphizedProgram) -> GpuArtifactBundle:
    // Collect shader items from program
    // For each shader, call all 4 backend generators
    // Collect compute metadata → compute_residency JSON
    // Return bundle
```

### 4.2 SPIR-V Emission (Recommended: Rust FFI)

Since implementing a full SPIR-V builder in Kain would be ~2000+ lines, the recommended approach is:

**Option A (recommended):** `use rust::rspirv` FFI to call the existing Rust SPIR-V generator.
```kain
use rust::rspirv::generate_shader_spirv as native_generate_spirv
```
This reuses 3760 lines of proven Rust code. The `use rust::` FFI is Kain's Rust-crate bridge.

**Option B (if FFI unavailable):** Minimal native SPIR-V builder emitting key opcodes:
- `OpCapability Shader`, `OpMemoryModel GLSL450`
- `OpEntryPoint GLCompute` / `Vertex` / `Fragment`
- `OpDecorate` for DescriptorSet, Binding, BuiltIn, Location
- `OpType*` for all GPU types (Vec2/3/4, StorageBuffer, etc.)
- Structured CFG with `OpSelectionMerge` + `OpLoopMerge`
- `OpBranchConditional` + `OpSwitch` for if/while

### 4.3 PTX Emission

PTX is simpler (text-based, direct emission):

```kain
pub fn generate_ptx(shader: TypedShader) -> String:
    // Header
    ptx = ".version 7.8\n"
    ptx = ptx + ".target sm_80\n"
    ptx = ptx + ".address_size 64\n\n"

    // Entry point
    ptx = ptx + ".entry " + shader_name + "(\n"
    for param in params:
        if param.type is StorageBuffer:
            ptx = ptx + "  .param .u64 " + param.name + ",\n"
        else:
            ptx = ptx + "  .param ." + ptx_type(param.type) + " " + param.name + ",\n"
    ptx = ptx + ")\n{\n"

    // Body (translate Kain body to PTX instruction sequence)
    ptx = ptx + translate_body_to_ptx(shader.body) + "\n"
    ptx = ptx + "  ret;\n"
    ptx = ptx + "}\n"
    return ptx
```

### 4.4 HLSL + WGSL Emission via shader-text

HLSL and WGSL share a common path through `crates/shader-text`. Recommended: `use rust::shader_text` FFI.

If native emission is needed:

```kain
// HLSL header:
//   cbuffer params : register(bN) { ... }
//   RWStructuredBuffer<T> buf : register(uN)
//   [numthreads(x, y, z)]
//   void main(InputType input) { ... }

// WGSL header:
//   @group(0) @binding(N) var<uniform> params: vec4<f32>;
//   @group(0) @binding(N) var<storage, read_write> buf: array<vec4<f32>>;
//   @compute @workgroup_size(x, y, z)
//   fn main(@builtin(global_invocation_id) id: vec3<u32>) { ... }
```

### 4.5 Dispatch Codegen (LLVM lane)

The host-side dispatch emits an `abi_gpu_dispatch` call:

```llvm
; dispatch "shader::MyKernel::compute" [32, 1, 1]
%key = getelementptr [33 x i8], [33 x i8]* @.str.shader_key, i64 0, i64 0
%status = call i64 @abi_gpu_dispatch(i8* %key, i64 32, i64 1, i64 1)
```

With `declare` in preamble:
```llvm
declare i64 @abi_gpu_dispatch(i8*, i64, i64, i64)
```

### 4.6 Compute Residency Sidecar

Emit `kain_compute_residency.json` from the compute metadata collected during typechecking:

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

---

## 5. Edge Cases

| # | Scenario | Handling |
|---|----------|----------|
| 1 | Shader name doesn't match dispatch key | Warning if shader lookup active; no error if shader is in different module |
| 2 | No uniform bindings declared | Valid: shader with only `id: UVec3` has zero uniforms |
| 3 | Duplicate uniform binding `@0` | `ERR_SHADER_DUPLICATE_BINDING` |
| 4 | Compute shader with no workgroup | Use default workgroup (8, 8, 1) |
| 5 | workgroup(0, 0, 0) | `ERR_SHADER_ZERO_WORKGROUP` |
| 6 | vertex shader with workgroup | `ERR_SHADER_VERTEX_WORKGROUP` (vertex has no workgroup) |
| 7 | dispatch in `Pure` function | `ERR_MISSING_GPU_EFFECT` |
| 8 | dispatch in `IO` without GPU | `ERR_MISSING_GPU_EFFECT` + `ERR_MISSING_UNSAFE_EFFECT` |
| 9 | String literal in shader body | `ERR_SHADER_DISALLOWED_EXPR` |
| 10 | `for` loop in shader body | `ERR_SHADER_DISALLOWED_EXPR` (use while) |
| 11 | StorageBuffer<String> | `ERR_SHADER_INVALID_STORAGE_ELEMENT` (strings not GPU-compatible) |
| 12 | Sampler2D in compute shader without explicit binding | Allowed (auto slot assignment) |
| 13 | comptime tensor plan references nonexistent uniform | `ERR_SHADER_COMPTIME_BINDING_MISMATCH` |
| 14 | Float return on compute shader | `ERR_SHADER_COMPUTE_MUST_RETURN_VOID` |
| 15 | Missing `id: UVec3` on compute | `ERR_SHADER_COMPUTE_NO_ID_PARAM` |

---

## 6. Codegen Stubs Already Present

| File | Lines | Status |
|------|-------|--------|
| `src/parser.kn` line 3147 | `parse_shader()` | Implemented (stage, name, params, uniforms, workgroup, body) |
| `src/parser.kn` line 882 | `parse_dispatch_stmt()` | Implemented (key string + dims) |
| `src/types.kn` | `AST_ITEM_SHADER = 22`, `AST_STMT_DISPATCH = 59` | Constants defined |
| `src/codegen.kn` | `abi_gpu_dispatch` declare | Missing — needs preamble entry |

**New codegen functions needed:**

```kain
pub fn compile_dispatch_stmt(gen: LlvmGenerator, node: AstNode, program: MonomorphizedProgram) -> GenVoidResult
// Emits: call i64 @abi_gpu_dispatch(i8*, i64, i64, i64)
```
