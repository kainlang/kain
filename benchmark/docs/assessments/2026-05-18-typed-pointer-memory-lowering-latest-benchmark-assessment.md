# 2026-05-18 - Typed helper-pointer lowering latest benchmark assessment

The latest full multi-language snapshot before this pass (`benchmark/latest.md` generated `2026-05-18T23:37:06.421184+00:00`) had one especially honest compiler-owned wound left: `memory_stream` was Kain `37.481 ms` versus Rust `10.447 ms` and C++ `8.811 ms`. The row is just sequential write/read over a helper-owned integer buffer, so a gap that large was almost certainly lowering shape, not language semantics.

What landed:

- `crates/sys-codegen/src/codegen_llvm/mod.rs`
  - `ownership_pointer_provenance_for_expr(...)` now propagates helper-owned provenance through `PtrOffset` and canonical `__kain_ptr_offset` / `__kain_index_ptr` surfaces.
  - Added `compile_non_ephemeral_typed_memory_pointer(...)`, which lowers helper-owned typed accesses to `getelementptr <ty>` plus the strongest honest alignment instead of round-tripping through byte-addressed `i64` math for every `mem_load` / `mem_store`.
  - `Expr::PtrOffset` now uses the same power-of-two shift strength reduction as the raw helper path when the offset is proven non-negative.
- `crates/sys-codegen/tests/llvm_codegen_test.rs`
  - Added `llvm_uses_typed_gep_and_natural_alignment_for_helper_owned_ptr_offset_accesses`.
- `crates/sys-codegen/z3/proofs-experimental/power-of-two-ptr-offset-shift-equivalence.smt2`
  - Proves `offset * 8 == offset << 3` on the bounded non-negative 64-bit domain used by the strength reduction.
- `research/2026-05-18-typed-pointer-memory-lowering.md`
  - Captures the research lattice, rejected benchmark-cheat route, and measured result.

Why this is a real compiler win:

- The generated Kain LLVM for `memory_stream` now contains a typed `getelementptr i64, i64*` walk with `align 8` loads/stores instead of the old byte-addressed `ptr_offset` shape that hid alignment and pointer intent from LLVM.
- This is a general lowering improvement for helper-owned typed raw-memory accesses, not a benchmark-only checksum reducer or a case-local native helper.

Validation:

- `cargo test -p kain-sys-codegen llvm_uses_typed_gep_and_natural_alignment_for_helper_owned_ptr_offset_accesses -- --nocapture`
  - Result: PASS.
- Z3 MCP report:
  - `z3/reports/20260519T001145Z-power-of-two-ptr-offset-shift-equivalence.json`
  - Result: `unsat`.
- `bazel build //:kain --config=release`
  - Result: PASS.
- Focused benchmark:
  - Command: `python benchmark/run.py --case memory_stream,ownership_memory,zero_copy_binary_wire,simd_lane_mix --languages kain,rust,cpp,zig,go --runs 5 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --latest-stem latest_typed_pointer_memory_probe --minimal-name latest_typed_pointer_memory_probe.md --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`
  - Result: PASS.
  - `memory_stream`: Kain `9.749 ms`, Rust `10.169 ms`, C++ `9.222 ms`.
- Canonical full-suite rerun:
  - Command: `python benchmark/run.py --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`
  - Result: PASS, refreshed 109 foreign baselines.
  - Snapshot: `benchmark/latest.md` generated `2026-05-19T00:14:32.341687+00:00`.

Measured impact:

- `memory_stream` flipped from a severe loss to a Kain full-suite win:
  - Before: Kain `37.481 ms`, Rust `10.447 ms`, C++ `8.811 ms`.
  - After: Kain `8.446 ms`, Rust `9.652 ms`, C++ `9.835 ms`.
- The focused probe showed the same shape without relying on full-suite luck:
  - Kain `9.749 ms`, Rust `10.169 ms`, C++ `9.222 ms`.

Current latest full-suite truth:

- The `memory_stream` wound is closed for now; the next honest language/runtime targets are `ownership_memory`, `string_ops`, `dynamic_vtable_thrashing`, `sim_uv_velocity_grid`, `option_result`, `ffi_shared_call_stress`, and the real runtime/system rows `http_server_concurrency` and `process_stdio_loop`.
- `native_map_lookup` is now only slightly behind Zig (`17.161 ms` vs `16.574 ms`) and is no longer the highest-urgency broad gap.
- `ownership_memory` remains a likely scalarization/register-residency problem, not a raw pointer-lowering problem.
