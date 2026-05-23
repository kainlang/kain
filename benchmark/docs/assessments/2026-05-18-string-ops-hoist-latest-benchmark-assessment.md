# 2026-05-18 - String-ops loop hoist and latest benchmark assessment

The LLVM backend now primes loop-carried string parameter lengths at function entry, which closes a meaningful chunk of the remaining `string_ops` gap without touching the benchmark source. The transform is guarded by an AST loop scan and a reassignment-aware proof model, so this is a real backend win, not a benchmark-specific trick.

What landed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Added a structural loop scan for identifiers inside blocks/expressions/statements.
  - Added `prime_string_param_length_cache(...)` so loop-carried string params get a single entry-time `@len(i8*)` call.
  - Wired the priming into named callables and methods only when the parameter is actually loop-mentioned.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - Added `llvm_hoists_loop_carried_string_param_lengths_out_of_loop_bodies`.
- `crates/kain-sys-codegen/z3/proofs-experimental/string-param-loop-length-cache-valid-under-reassign-guard.smt2`
  - Encodes the reassignment guard and proves the emitted length agrees with semantic `len(current_ptr)`.

Validation:

- `bazel build //:kain --config=release`
- `cargo test -p kain-sys-codegen llvm_hoists_loop_carried_string_param_lengths_out_of_loop_bodies -- --nocapture`
- `python benchmark/run.py --case string_ops --languages kain,rust,cpp,javascript,python --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --latest-stem latest_string_ops_len_hoist --minimal-name latest_string_ops_len_hoist.md --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`
- `python benchmark/run.py --timeout 900 --baseline-mode refresh-foreign --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`

Measured result:

- Focused `string_ops` refresh: Kain `10.553 ms`, Rust `9.357 ms`, C++ `9.389 ms`.
- Latest full-suite `string_ops`: Kain `11.865 ms`, Rust `8.819 ms`, C++ `9.542 ms`.
- Previous full-suite `string_ops` snapshot (`20260518T094400Z`): Kain `13.958 ms`.
- Net full-suite improvement for Kain: about `15.0%`.

Current latest full-suite truth (`benchmark/latest.md`) says the best remaining valuable speedup targets are:

- `string_ops` (`1.345x` behind the fastest language): still the cleanest compiler-owned text hot path. Next move should be a first-class `(ptr,len)` substring lane so `starts_with_at` / `find_substring` stop rediscovering lengths through plain-pointer plumbing.
- `ownership_memory` (`1.274x`): this now looks more like scalarization / box-elision / register-residency debt than native-runtime helper debt.
- `recursive_sum` (`1.251x`): likely inlining / call-shape / tail-lowering territory.
- `ecs_archetype_query` (`1.174x`): real data-layout/codegen opportunity, but broader than the string lane.
- `option_result` (`1.126x`): still wants stronger unboxed lowering or escape analysis.

Rows that are large but should not be confused with the next honest language-core target:

- `rayon_parallel_reduce` (`1.795x`) is still explicitly a parallel proxy against Rayon.
- `http_server_concurrency` (`1.603x`) is a native HTTP/runtime lane, not a simple scalar backend tax.
- `dynamic_vtable_thrashing` (`1.464x`) is still marked `dispatch-proxy`.

Noise note:

- The first full-suite refresh produced a suspicious `allocator_large_object_churn` shape where all native languages showed bimodal samples.
- A focused rerun immediately restored the expected Kain win (`9.598 ms` vs Rust `42.030 ms` and C++ `41.295 ms`), so the correct response was a second full-suite refresh rather than treating the first snapshot as a semantic regression.
- The final canonical `benchmark/latest.md` is the second refresh, where `allocator_large_object_churn` is back to a Kain win at `11.598 ms`.

Recommendation for the next automation pass:

- Stay on `string_ops` one more round, but push harder: make length an owned lowering artifact instead of a rediscovered helper fact.
- After that, switch to `ownership_memory` or `option_result` only if the backend attack is broad enough to remove boxing/scalarization debt across multiple rows rather than only one benchmark.
