# GPU — Shader + Dispatch (TASKS)

**Target file:** `src/GPU.kn`
**Date:** 2026-06-12
**Wave:** After FOXTROT (needs ResolvedType, check_expr, effects.kn)
**Parallel with:** L7 Systems tasks (no cross-dependency)

---

## G1: GPU Type System (src/GPU.kn, ~100 lines)

**Files:** `src/GPU.kn`

Implement GPU-compatible type validation and the shader type constants.

**Acceptance criteria:**
- [ ] `GPU_SHADER_STAGE_COMPUTE: Int = 0`, `GPU_SHADER_VERTEX: Int = 1`, `GPU_SHADER_FRAGMENT: Int = 2`
- [ ] `GPU_COMPATIBLE_TYPES: Array<Int>` containing `RT_FLOAT`, `RT_INT`, and struct names `Vec2`, `Vec3`, `Vec4`, `IVec2`, `UVec2`, `UVec3`, `Mat4`
- [ ] `GPU_STORAGE_BUFFER_ELEMENT_TYPES: Array<Int>` — subset of GPU-compatible types valid as `StorageBuffer<T>` elements
- [ ] `is_gpu_compatible_type(ty: ResolvedType) -> Bool`:
  - Returns true for Float, Int, Vec2/3/4, UVec2/3, IVec2, Mat4
  - Returns true for `StorageBuffer<T>` if T is GPU-compatible
  - Returns false for String, Char, Array, Tuple, Function types
- [ ] `is_gpu_uniform_type(ty: ResolvedType) -> Bool`:
  - Like `is_gpu_compatible_type` plus StorageBuffer, Sampler2D
- [ ] `error_shader_invalid_input_type(kind: Int, name: String) -> String` — error message builder
- [ ] `error_shader_duplicate_binding(binding: Int) -> String`
- [ ] `error_shader_zero_workgroup() -> String`
- [ ] `error_missing_gpu_effect() -> String`
- [ ] File checks standalone: `kain check src/GPU.kn` passes

---

## G2: Shader Check (src/GPU.kn + src/types.kn, ~200 lines)

**Files:** `src/GPU.kn` (implementation), `src/types.kn` (dispatch in check_item)

Implement `check_shader()` with full validation.

**Acceptance criteria:**
- [ ] `check_shader(env, ast_node, program) -> Result<TypedItem, String>`:
  - Extracts and validates shader stage from ast_data[1]
  - Stage-specific rules: compute returns Void with `id: UVec3`, vertex/fragment return Vec4
  - Validates each input parameter type via `is_gpu_compatible_type`
  - Validates each uniform: checks binding uniqueness (no duplicates), type validity, binding integer range (0..65535)
  - Allocates default workgroup (8,8,1) if missing on compute; rejects workgroup on vertex/frag
  - Validates workgroup dimensions are positive integer constants (>= 1)
  - Walks body AST, rejects forbidden expressions:
    - For, Loop, Defer → `ERR_SHADER_DISALLOWED_EXPR`
    - String, FString → `ERR_SHADER_DISALLOWED_EXPR`
    - Dispatch → `ERR_SHADER_DISALLOWED_EXPR`
  - Validates comptime block (if present): dispatch_size, tensor_plans match uniforms
  - Returns TypedItem with `effects = EFF_GPU`
- [ ] Wire `check_item()` in `src/types.kn` to dispatch `AST_ITEM_SHADER` to `check_shader()`
- [ ] All error constants defined and used

---

## G3: Dispatch Check (src/GPU.kn + src/types.kn, ~80 lines)

**Files:** `src/GPU.kn` (implementation), `src/types.kn` (dispatch in check_stmt)

Implement `check_dispatch()` with GPU/Unsafe effect validation.

**Acceptance criteria:**
- [ ] `check_dispatch(env, ast_node, program) -> Result<ResolvedType, String>`:
  - Extracts compute key from string pool; verifies pattern `"shader::KernelName::compute"` via regex or manual `split("::")` check
  - Verifies `env.current_effects & EFF_GPU != 0` → `ERR_MISSING_GPU_EFFECT`
  - Verifies `env.current_effects & EFF_UNSAFE != 0` → `ERR_MISSING_UNSAFE_EFFECT`
  - Infers dimension expressions (dim_x/y/z from ast_data[1..3]) are Int-compatible
  - (Optional) Looks up shader name in env to verify compute shader exists
  - Returns Unit
- [ ] Wire `check_stmt()` in `src/types.kn` to dispatch `AST_STMT_DISPATCH` to `check_dispatch()`
- [ ] Error codes: `ERR_DISPATCH_INVALID_KEY`, `ERR_MISSING_GPU_EFFECT`, `ERR_MISSING_UNSAFE_EFFECT`, `ERR_DISPATCH_DIM_NOT_INT`

---

## G4: Dispatch Codegen (src/codegen.kn, ~80 lines)

**Files:** `src/codegen.kn` (extend compile_stmt)

Implement `compile_dispatch_stmt()` — emit `abi_gpu_dispatch` LLVM call.

**Acceptance criteria:**
- [ ] `compile_dispatch_stmt(gen, node, program) -> GenVoidResult`:
  - Loads compute key string constant (global `@.str.<key>`)
  - Compiles dim_x/y/z expressions to LLVM i64 values
  - Emits: `call i64 @abi_gpu_dispatch(i8* %key, i64 %x, i64 %y, i64 %z)`
  - Discards return value (dispatch is a statement, not expression)
- [ ] `declare i64 @abi_gpu_dispatch(i8*, i64, i64, i64)` emitted in LLVM preamble section
- [ ] Wire `compile_stmt()` to dispatch `AST_STMT_DISPATCH` to `compile_dispatch_stmt()`

---

## G5: GPU Artifact Sidecar Emission (src/GPU.kn + src/codegen.kn, ~120 lines)

**Files:** `src/GPU.kn`, `src/codegen.kn`

Implement the compute residency JSON manifest generation and the artifact bundle struct.

**Acceptance criteria:**
- [ ] `GpuArtifactBundle` struct with: `spirv_bytes: Array<Int>`, `ptx_text: String`, `hlsl_text: String`, `wgsl_text: String`, `compute_residency: String`
- [ ] `collect_shader_items(program: MonomorphizedProgram) -> Array<TypedItem>`:
  - Iterates `program.items`, filters for items with `kind == AST_ITEM_SHADER`
- [ ] `generate_compute_residency(shaders: Array<TypedItem>) -> String`:
  - Builds JSON object per compute shader with: key, workgroup_size, dispatch_size, bindings
  - Each binding: name, type (StorageBuffer/etc.), element_type, role, contract
  - Returns JSON string conforming to spec §4.6 format
- [ ] `emit_gpu_artifacts(program: MonomorphizedProgram, output_dir: String) -> String`:
  - Collects shader items, generates all 4 artifact strings
  - Writes files: `<name>.spv`, `<name>.derived.ptx`, `<name>.derived.hlsl`, `<name>.derived.wgsl`, `<name>.kain_compute_residency.json`
  - Returns output directory path
- [ ] SPIR-V generation via `use rust::rspirv` FFI (preferred):
  - Call `native_generate_spirv(program)` → `Vec<u8>` → convert to `Array<Int>`
  - If FFI unavailable: skeleton with placeholder comment
- [ ] PTX generation via native text emission:
  - `.version 7.8` / `.target sm_80` header
  - `.entry` declaration with params
  - Kain body → PTX instruction translation (basic: load, store, add, mul, ret)
- [ ] HLSL + WGSL generation via `use rust::shader_text` FFI (preferred) or skeleton

---

## G6: Workgroup + Uniform Validation (src/GPU.kn, ~60 lines)

**Files:** `src/GPU.kn`

Implement dedicated workgroup and uniform validation functions.

**Acceptance criteria:**
- [ ] `validate_workgroup(x: Int, y: Int, z: Int, stage_kind: Int) -> Result<(Int, Int, Int), String>`:
  - Vertex or fragment stage: error `ERR_SHADER_VERTEX_WORKGROUP`
  - Any zero or negative value: error `ERR_SHADER_ZERO_WORKGROUP`
  - Returns validated (x, y, z) tuple
- [ ] `validate_uniform_bindings(uniforms: Array<UniformEntry>) -> Result<(), String>`:
  - Builds set of binding numbers; rejects duplicates with `ERR_SHADER_DUPLICATE_BINDING`
  - Verify binding number in range 0..65535
  - Returns Ok or first error
- [ ] `UniformEntry` struct: `name: String, type_name: String, binding: Int, resource_kind: Int`
- [ ] `uniform_binding_to_hlsl_register(binding: Int, kind: Int) -> String`: maps @N to register(bN)/tN/uN
- [ ] `uniform_binding_to_wgsl_group(binding: Int) -> String`: `@group(0) @binding(N)`

---

## G7: Edge Case Tests (smoketest/, ~80 lines)

**Files:** `smoketest/src/semantics/GPU_shader.kn`, `smoketest/src/semantics/GPU_dispatch.kn`

Acceptance tests for the shader and dispatch typechecker.

**Acceptance criteria:**
- [ ] `GPU_shader.kn` passes `kain check`:
  - `test_compute_shader`: `shader compute TestKernel(id: UVec3) -> Void workgroup(8,8,1)` with one StorageBuffer uniform
  - `test_vertex_shader`: `shader vertex TestVert(pos: Vec3, uv: Vec2) -> Vec4` with a Mat4 uniform
  - `test_fragment_shader`: `shader fragment TestFrag(uv: Vec2) -> Vec4` with a Vec3 uniform
  - `test_shader_while_loop`: compute shader with while loop in body
  - `test_shader_no_body_disallowed`: compute shader with for loop → diagnostics error
- [ ] `GPU_dispatch.kn` passes `kain check`:
  - `test_dispatch_valid`: `fn run() with GPU, Unsafe: dispatch "shader::MyKernel::compute" [32, 1, 1]`
  - `test_dispatch_missing_gpu` (compile error): `fn run() with Unsafe: dispatch "shader::K::compute" [1,1,1]` → `ERR_MISSING_GPU_EFFECT`
  - `test_dispatch_missing_unsafe` (compile error): `fn run() with GPU: dispatch "shader::K::compute" [1,1,1]` → `ERR_MISSING_UNSAFE_EFFECT`
  - `test_dispatch_invalid_key` (compile error): `fn run() with GPU, Unsafe: dispatch "not-a-valid-key" [1,1,1]` → `ERR_DISPATCH_INVALID_KEY`
- [ ] Each error test uses `//@ check:` compiletest directive with the expected error code
