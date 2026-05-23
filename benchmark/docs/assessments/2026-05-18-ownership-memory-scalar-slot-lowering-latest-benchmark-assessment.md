# 2026-05-18 - Ownership-memory scalar-slot lowering latest benchmark assessment

The latest full multi-language snapshot before this pass (`benchmark/out/reports/20260519T005630Z.json`, generated `2026-05-19T00:40:47.625400+00:00`) still had one clean compiler-owned memory wound: `ownership_memory` was Kain `14.264 ms`, Rust `11.788 ms`, and C++ `11.245 ms`. The runtime ownership protocol had already been erased out of the hot path, so the remaining gap was no longer about helper calls. It was about the shape of the stack cell LLVM saw.

What landed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Supported 1/2/4/8-byte single-cell ephemeral helper allocations now lower to typed scalar stack slots (`i8` / `i16` / `i32` / `i64`) instead of `[N x i8]`.
  - `EphemeralOwnershipLocalWitness` now carries the storage LLVM type, element type, byte length, and guaranteed slot alignment.
  - `compile_ephemeral_storage_i8_pointer(...)` now preserves the same `i8*` observational lane for both scalar and byte-array storage.
  - `compile_runtime_mem_load(...)` and `compile_runtime_mem_store(...)` now clamp alignment to `min(natural_alignment(access_ty), witness.storage_alignment)` instead of hardcoding `align 1`.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - `llvm_erases_loop_local_ephemeral_single_cell_ownership_to_local_storage` now expects `alloca i64`.
  - `llvm_keeps_ephemeral_zero_init_when_first_use_is_read` now expects typed zero-init with `align 8`.
- `crates/kain-sys-codegen/z3/proofs-experimental/ownership-ephemeral-single-cell-scalar-storage-preserves-byte-lane.smt2`
  - Experimental SMT for the scalar-slot lane.
- `crates/kain-sys-codegen/z3/proofs/memory-ephemeral-single-cell-scalar-storage-preserves-byte-lane-observation.yaml`
  - Durable proof-pack entry for the new lane.

Why this is a real compiler win:

- Before this pass, the generated `ownership_memory.ll` still materialized the erased single cell as `[8 x i8]`, zero-filled it as a byte array, and later bitcast it back to `i64*` under `align 1`.
- That byte-oriented shape pushed LLVM toward awkward byte-lane vector/shuffle code in the final assembly even though the benchmark is just a single scalar cell mutation loop.
- After the change, the IR lowers the cell as `alloca i64` plus typed `store i64 0, i64* ..., align 8`, and the assembly collapses back to scalar integer code.
- This is not a benchmark-only checksum collapse. It is a general backend improvement for erased single-cell helper-owned locals.

Validation:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_erases_loop_local_ephemeral_single_cell_ownership_to_local_storage -- --nocapture`
  - Result: PASS.
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_keeps_ephemeral_zero_init_when_first_use_is_read -- --nocapture`
  - Result: PASS.
- Z3 direct report:
  - `z3/reports/20260519T011925Z-ownership_ephemeral_single_cell_scalar_storage_preserves_byte_lane.json`
  - Result: `unsat`.
- Full `kain-sys-codegen` memory proof lane:
  - `crates/kain-sys-codegen/z3/reports/20260519T011933Z-kain_sys_codegen_memory_lane_post_scalar_ephemeral.json`
  - Result: `9 proved, 0 counterexamples, 0 unknown, 0 errors`.
- `bazel build //:kain --config=release`
  - Result: PASS.

Measured impact:

- Focused rerun:
  - Command: `python benchmark/run.py --case ownership_memory --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --latest-stem latest_ownership_memory_scalar_ephemeral --minimal-name latest_ownership_memory_scalar_ephemeral.md --kain-exe D:\\Kain-Bazel\\output-user-root\\ccujd7ry\\execroot\\_main\\bazel-out\\x64_windows-opt\\bin\\crates\\cli\\kain.exe`
  - Report: `benchmark/out/reports/latest_ownership_memory_scalar_ephemeral.llm.md`
  - Result: Kain `11.554 ms`, Rust `12.177 ms`, C++ `11.090 ms`.
- Canonical full-suite rerun:
  - Command: `python benchmark/run.py --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --kain-exe D:\\Kain-Bazel\\output-user-root\\ccujd7ry\\execroot\\_main\\bazel-out\\x64_windows-opt\\bin\\crates\\cli\\kain.exe`
  - Report: `benchmark/out/reports/latest.llm.md`
  - Result: PASS, refreshed 109 foreign baselines.
  - `ownership_memory`: Kain `10.752 ms`, Rust `12.738 ms`, C++ `11.062 ms`.
- Focused regression sanity:
  - Report: `benchmark/out/reports/latest_scalar_ephemeral_regression_sanity.llm.md`
  - `ownership_memory`: Kain `11.668 ms`, Rust `11.671 ms`, C++ `11.664 ms`.

Current latest full-suite truth:

- The improvement is unquestionably real:
  - Before: Kain `14.264 ms`, Rust `11.788 ms`, C++ `11.245 ms`.
  - After: Kain now lives in the `10.752 ms` to `11.668 ms` band depending on suite noise and build reuse.
- The honest interpretation is not “ownership_memory is permanently solved forever.”
  - It is “the old 26.8% C++ gap is gone, and the row is now near parity with occasional Kain suite wins.”
- The regression sanity rerun also restored the suspicious full-suite drift on nearby rows:
  - `native_map_lookup` returned to a Kain win (`16.178 ms` vs Zig `17.512 ms`).
  - `async_ready_chain` returned to a stronger Kain win (`8.402 ms` vs Rust `9.733 ms`).
  - `memory_stream`, `branch_dispatch`, `string_ops`, and `call_chain` stayed competitive but not yet winning.

Best next honest targets after this pass:

- `http_server_concurrency`: still the largest real loss (`1.58x` behind Rust) and clearly a runtime/network/system mission.
- `sim_uv_velocity_grid`: still the biggest C++ compute loss in the non-proxy rows.
- `string_ops`, `branch_dispatch`, `memory_stream`, `call_chain`, `option_result`, `machine_stones_shatter_loop`, and `ffi_shared_call_stress`: all now live in the “single-digit backend/codegen gap” frontier where a clean compiler win could flip the row.
