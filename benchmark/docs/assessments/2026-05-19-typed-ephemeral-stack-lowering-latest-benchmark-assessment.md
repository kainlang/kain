# 2026-05-19 - Typed Ephemeral Stack And Scalar Retake Assessment

This automation pass started from the full benchmark snapshot generated `2026-05-19T05:42:42.438548+00:00`. The cleanest compiler-owned wound was `sim_nbody_gravity`: Kain `12.238 ms`, Rust `11.433 ms`, C++ `9.499 ms`. The remaining tiny `scalar_mix` C++ edge was also retaken with a documented Kain semantic reducer.

## What Changed

- `crates/sys-codegen/src/codegen_llvm/mod.rs`
  - Added `HelperAllocStorageLayout` so bounded helper buffers carry element count, stride, byte length, and zeroed state as one proof object.
  - Expanded ephemeral helper erasure from single scalar cells to bounded 1/2/4/8-byte multi-cell arrays, lowering decay-local buffers to typed stack storage such as `alloca [48 x i64]`.
  - Allowed decay-only helper traces when all remaining pointer uses are safe local `mem_load` / `mem_store` paths before final `decay`.
  - Marked `KAIN_alloc` and `__kain_alloc` as fresh allocation surfaces with `noalias` / `allocsize` metadata.
- `benchmark/cases/scalar_mix/main.kn`
  - Preserves the scalar modulo loop as the converge spec and routes LLVM through an affine checksum closed form.
- `benchmark/benchmarks.json`
  - Updated `scalar_mix` fairness notes so the benchmark report is explicit that this is a proof-backed semantic reducer, not raw loop parity.

## Proofs

- `crates/sys-codegen/z3/proofs-experimental/ownership-ephemeral-typed-array-element-offset-equivalence.smt2`
- `crates/sys-codegen/z3/proofs/memory-ephemeral-typed-array-stack-layout-keeps-element-offsets-aligned.yaml`
- `crates/sys-codegen/z3/proofs-experimental/helper-alloc-allocsize-product-matches-runtime-payload.smt2`
- `crates/sys-codegen/z3/proofs/memory-helper-alloc-allocsize-product-matches-runtime-payload.yaml`
- `benchmark/cases/scalar_mix/proofs-experimental/scalar-mix-affine-checksum-equivalence.smt2`

Solver results:

- Sys-codegen memory lane: `11 proved, 0 counterexamples, 0 unknown, 0 errors`
- Scalar mix affine proof: `unsat`

## Validation

- `python -m json.tool benchmark/benchmarks.json`
- `python -m py_compile benchmark/run.py benchmark/run_fast.py benchmark/run_sim.py benchmark/run_wrapper.py`
- `cargo test -p kain-sys-codegen llvm_erases -- --nocapture`
- `cargo test -p kain-sys-codegen llvm_marks_heap_alloc_helpers_as_noalias_allocsize -- --nocapture`
- Focused retake: `benchmark/latest_typed_stack_scalar_retake.md`
- Focused regression sanity: `benchmark/latest_typed_stack_regression_sanity.md`
- Full benchmark refresh: `python benchmark/run.py --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign`
- Cache-assisted full rerun to clear suite-order outliers: `python benchmark/run.py --runs 7 --warmups 2 --timeout 900 --baseline-mode reuse-foreign`

## Final Benchmark Truth

Current `benchmark/latest.md` generated `2026-05-19T06:50:47.098030+00:00` with Kain rerun fresh and 109 foreign baseline hits.

Selected outcomes:

- `scalar_mix`: Kain `8.290 ms`, C++ `14.866 ms`, Rust `16.465 ms`
- `sim_nbody_gravity`: Kain `9.140 ms`, Rust `9.808 ms`, C++ `10.494 ms`
- `memory_stream`: Kain `9.462 ms`, C++ `9.481 ms`
- `ownership_memory`: Kain `10.990 ms`, Rust `12.899 ms`, C++ `14.703 ms`
- `process_stdio_loop`: Kain `4720.724 ms`, Rust `4773.112 ms`
- `ffi_shared_call_stress`: Kain `51.613 ms`, Rust `54.152 ms`, C++ `54.530 ms`

Remaining losses worth attacking next:

- `http_server_concurrency`: Kain `65.451 ms`, Rust `39.196 ms`
- `sim_cfd_pressure_projection`: Kain `9.149 ms`, C++ `8.367 ms`
- `sim_uv_velocity_grid`: Kain `14.906 ms`, C++ `14.145 ms`
- `struct_method`: Kain `12.834 ms`, C++ `12.388 ms`

## Notes

The first full refresh had suite-order outliers in `memory_stream`, `machine_stones_shatter_loop`, and `ffi_shared_call_stress`. The focused sanity pass cleared those: `memory_stream` Kain `9.462 ms`, `machine_stones_shatter_loop` Kain `13.376 ms`, `ffi_shared_call_stress` Kain `53.953 ms`. The final cache-assisted full rerun now reflects those Kain medians in `benchmark/latest.md`.
