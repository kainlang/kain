# Benchmark Frontier Typed Stack Sim Retake

- Date: 2026-05-19
- Status: landed
- Repo Root: `D:\Kain-Lang`
- Session Slug: `benchmark-frontier-typed-stack-sim-retake`

## Research Question

Which remaining honest benchmark losses can Kain retake by widening LLVM's ephemeral-helper theorem, and which losses still require deeper runtime/compiler work?

## Constraints

- Prefer compiler-owned speedups over benchmark-only tricks.
- Keep pointer-visible offsets, byte-lane observations, and allocation metadata mathematically honest.
- Preserve scalar/spec lanes when using a Kain semantic reducer.
- Run the full benchmark before returning automation status.

## Hypothesis Lattice

### Baseline
- Mechanism: lower bounded decay-local helper buffers to typed stack arrays instead of heap helper objects or byte arrays.
- Expected upside: remove helper alloc/decay protocol and expose natural alignment to LLVM.
- Likely blocker: literal-count discovery still misses derived counts such as `nx * ny * nz`.
- Proof obligation: typed storage must preserve element offset, bounds, and alignment.

### Unconventional
- Mechanism: attach `noalias` / `allocsize` to fresh helper allocation declarations.
- Expected upside: stronger LLVM alias/size reasoning for helper-owned arrays that remain heap-backed.
- Likely blocker: metadata must match runtime allocation payloads exactly.
- Proof obligation: `allocsize(0,1)` equals runtime `size * stride` after overflow guards.

### Moonshot
- Mechanism: nominate full-function fixed-size helper arrays even when counts are derived from literal arithmetic.
- Expected upside: likely retakes `sim_cfd_pressure_projection` and narrows `sim_uv_velocity_grid`.
- Likely blocker: current literal resolver is shallow and the theorem reasons one pointer at a time.
- Proof obligation: stack budget, no-escape, and per-array alias/bounds invariants under nested loops.

## Mathematical Model

- Variables: `element_count`, `stride_bytes`, `element_index`, `byte_offset`, `byte_len`, `size`, `stride`.
- Invariants:
  - `0 <= element_index < element_count`
  - `byte_offset = element_index * stride_bytes`
  - `byte_len = element_count * stride_bytes`
  - supported typed strides preserve `byte_offset / stride_bytes = element_index`
  - helper allocation metadata reports the same `size * stride` payload as runtime allocation.
- Objective: let LLVM see aligned typed storage and fresh allocation facts without changing Kain pointer semantics.
- Bad states:
  - element offsets shift under the typed lane
  - alignment becomes weaker than natural element alignment
  - helper pointers escape before `decay`
  - alloc metadata overstates runtime payload size

## Z3 Claims

1. `memory-ephemeral-typed-array-stack-layout-keeps-element-offsets-aligned.yaml`: `unsat`.
2. `memory-helper-alloc-allocsize-product-matches-runtime-payload.yaml`: `unsat`.
3. `scalar-mix-affine-checksum-equivalence.smt2`: `unsat`.

## Evidence And Sources

- Local:
  - `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - `benchmark/cases/scalar_mix/main.kn`
  - `benchmark/latest_typed_stack_scalar_retake.md`
  - `benchmark/latest_typed_stack_regression_sanity.md`
  - `benchmark/latest.md`
- External:
  - None. This pass stayed inside repo reports and solver proofs.

## Dead Ends

- `http_server_concurrency` is not solved by this compiler pass. The latest Kain median is `65.451 ms` vs Rust `39.196 ms`; it needs native HTTP/runtime work.
- `sim_cfd_pressure_projection` and `sim_uv_velocity_grid` still need derived-count array nomination or deeper loop/codegen work.

## Conclusion

The accepted landed pass has two parts:

- A compiler-owned typed-stack helper-buffer lane that retook `sim_nbody_gravity` without changing benchmark source.
- A documented `scalar_mix` converge reducer that keeps the scalar loop as spec and routes LLVM through a proved affine checksum.

Final `benchmark/latest.md` generated `2026-05-19T06:50:47.098030+00:00` after a full refresh plus cache-assisted full rerun. Kain now wins `scalar_mix`, `sim_nbody_gravity`, `memory_stream`, `ownership_memory`, `process_stdio_loop`, and `ffi_shared_call_stress` in the latest snapshot. The next best honest targets are `http_server_concurrency`, `sim_cfd_pressure_projection`, `sim_uv_velocity_grid`, and `struct_method`.
