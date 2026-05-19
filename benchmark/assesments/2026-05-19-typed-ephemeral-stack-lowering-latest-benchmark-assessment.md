# 2026-05-19 - Typed ephemeral stack lowering latest benchmark assessment

The benchmark automation pass after the full-suite snapshot at `benchmark/latest.md` generated `2026-05-19T05:42:42.438548+00:00` attacked the biggest clean LLVM-side sim loss that still looked like compiler shape rather than missing language/runtime capability: `sim_nbody_gravity` at Kain `12.238 ms` versus Rust `11.433 ms` and C++ `9.499 ms`.

## What changed

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Added `HelperAllocStorageLayout` so the compiler can reason about bounded helper buffer element count, stride, byte span, and zeroed state in one place.
  - Expanded the ephemeral helper theorem from single-cell scalars to bounded 1/2/4/8-byte multi-cell arrays, lowering them to typed stack storage such as `alloca [4 x i64]` instead of byte arrays.
  - Relaxed the statement-order matcher so decay-only helper traces can still be erased when all remaining statements are safe local uses of the pointer before the final `decay`.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - Added decay-only and multi-cell regressions that prove the new lane emits typed stack arrays and skips helper alloc / decay runtime calls.
- `crates/kain-sys-codegen/z3/proofs/memory-ephemeral-typed-array-stack-layout-keeps-element-offsets-aligned.yaml`
  - Durable `unsat` proof that typed stack arrays preserve slot identity, bounds, and alignment for supported helper strides.
- `research/2026-05-19-benchmark-frontier-typed-stack-sim-retake.md`
  - Records the accepted compiler win plus the rejected alloc-metadata branch.

## Validation

- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_erases_bounded_ephemeral_ptr_offset_buffer_to_local_storage -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_erases_decay_only_bounded_helper_buffer_to_typed_local_storage -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_erases_decay_only_float_buffer_to_aligned_typed_local_storage -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_uses_typed_gep_and_natural_alignment_for_helper_owned_ptr_offset_accesses -- --nocapture`
- `mcp__z3_local__.run_proof_pack(path=\"D:/Kain-Lang/crates/kain-sys-codegen\", lane=\"memory\", report_name=\"kain-sys-codegen-memory-lane-post-alloc-attrs-revert\")`
  - Result: `10 proved, 0 counterexamples, 0 unknown, 0 errors`
- `bazel build //:kain --config=release`
  - Result: PASS

## Focused sim evidence

- Focused probe `benchmark/out/reports/latest_sim_ephemeral_typed_arrays.llm.md`
  - `sim_nbody_gravity`: Kain `10.153 ms`, Rust `11.033 ms`, C++ `10.186 ms`
  - `sim_uv_velocity_grid`: Kain `16.200 ms`, Rust `15.890 ms`, C++ `14.564 ms`
  - `sim_cfd_pressure_projection`: Kain `8.441 ms`, Rust `10.365 ms`, C++ `8.710 ms`

This was the cleanest evidence that the typed-stack widening itself was good.

## Rejected side branch

I also tried adding LLVM `noalias` / `allocsize` metadata to helper alloc declarations. That branch improved gravity in a focused retake, but it pushed `sim_cfd_pressure_projection` the wrong way and was rolled back. The final landed patch is the typed-stack theorem only, not the alloc-metadata experiment.

## Latest full benchmark truth

Post-pass full suite:

- `benchmark/latest.md` generated `2026-05-19T06:38:03.056721+00:00`
- Command: `python benchmark/run.py --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`

Selected outcomes relative to the pre-pass `2026-05-19T05:42:42.438548+00:00` snapshot:

- `sim_nbody_gravity`: `12.238 ms` -> `9.731 ms` for Kain, a `20.48%` median reduction. Kain is now only `2.46%` slower than C++ on the full suite row.
- `memory_stream`: `10.283 ms` -> `9.646 ms`
- `machine_stones_shatter_loop`: `14.145 ms` -> `13.166 ms`
- `option_result`: `10.387 ms` -> `9.415 ms`
- `unicode_string_heavy`: `9.528 ms` -> `8.696 ms`, flipping the row back to a Kain win in this full snapshot.

Rows that still need work in the same latest full snapshot:

- `sim_cfd_pressure_projection`: Kain `9.971 ms`, C++ `8.727 ms`
- `sim_uv_velocity_grid`: Kain `16.584 ms`, C++ `15.340 ms`
- `machine_stones_shatter_loop`: Kain `13.166 ms`, C++ `12.054 ms`
- `crypto_block_cipher`: Kain `11.389 ms`, C++ `10.580 ms`
- `evolutionary_loop`: Kain `25.582 ms`, Rust `23.919 ms`

## Noise caveat

The same latest full suite reported `ffi_shared_call_stress` at Kain `97.057 ms`, which did not match either the pre-pass full suite or the shape of the compiler change. A dedicated nine-run retake right after the full suite produced:

- `benchmark/out/reports/latest_ffi_regression_probe.llm.md`
  - Kain `54.504 ms`
  - Rust `56.026 ms`
  - C++ `53.480 ms`

Treat the `97.057 ms` full-suite sample as a suite-order or warmup artifact, not as the new steady-state truth of the landed compiler patch.

## Best next targets

- Generalize literal-count reasoning so derived sizes like `nx * ny * nz` can join the typed-stack decay-local lane. That is the likely CFD unlock.
- Attack `sim_uv_velocity_grid` and `machine_stones_shatter_loop` next; both still smell like compiler-visible numeric/raw-memory shape, not missing runtime semantics.
- Keep `ffi_shared_call_stress` honest by retaking it in isolation whenever it appears as the last-suite casualty.
