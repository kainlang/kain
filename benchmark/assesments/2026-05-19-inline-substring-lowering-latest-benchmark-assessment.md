# 2026-05-19 - Inline substring lowering and latest benchmark assessment

The LLVM backend now recognizes the canonical user-authored manual substring
helper and lowers known-string call sites into an inline `memchr`-driven search
with direct tail comparison. This keeps the authored Kain source intact while
removing the runtime wrapper call from the hot path.

What landed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Added `expr_static_string_bytes(...)`.
  - Added `compile_known_length_find_substring_inline(...)`.
  - Retargeted both `find_substring_from` fast-path lowering and canonical
    manual-helper lowering to the inline search path.
  - Specialized short static needles to direct byte-tail compares instead of a
    mandatory `memcmp`.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - Updated the substring fast-path tests to assert inline `memchr`/`memcmp`
    lowering rather than a runtime wrapper call.
- `crates/kain-sys-codegen/z3/proofs/control-inline-known-string-find-substring-window-stays-in-bounds.yaml`
  - Added a durable proof case for the `search_window` and `next_remaining`
    arithmetic used by the inline search loop.
- `research/2026-05-19-benchmark-frontier-2026-05-19.md`
  - Recorded the frontier question, solver claim, noise analysis, and next
    targets.

Validation:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test find_substring -- --nocapture`
- `bazel build //:kain --config=release`
- Z3 `check_smt2` report `D:\Kain-Lang\z3\reports\20260519T053356Z-inline-known-string-find-substring-window.json` returned `unsat`
- `python benchmark/run.py --case string_ops,unicode_string_heavy --languages kain,rust,cpp --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --latest-stem latest_string_validation --minimal-name latest_string_validation.md --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`
- `python benchmark/run.py --timeout 900 --baseline-mode refresh-foreign --latest-stem latest --minimal-name latest.md --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe`

Measured result:

- Focused `string_ops` validation: Kain `9.668 ms`, Rust `11.125 ms`, C++ `11.488 ms`.
- Focused `unicode_string_heavy` validation: Kain `10.814 ms`, Rust `10.037 ms`, C++ `10.055 ms`.
- Latest full-suite `string_ops`: Kain `8.578 ms`, Rust `10.188 ms`, C++ `11.169 ms`.
- Previous full-suite `string_ops` snapshot (`2026-05-19T04:37:34.995550+00:00`): Kain `10.973 ms`.
- Net full-suite `string_ops` improvement for Kain: about `21.8%`, and the row flipped from a Rust win to a Kain win.
- Latest full-suite `machine_stones_shatter_loop`: Kain `12.990 ms`, Rust `13.313 ms`, C++ `13.233 ms`.
- Previous full-suite `machine_stones_shatter_loop` snapshot (`2026-05-19T04:37:34.995550+00:00`): Kain `14.114 ms`.
- Net full-suite `machine_stones_shatter_loop` improvement for Kain: about `8.0%`, and the row also flipped into a Kain win.

Noise note:

- The full-suite tail showed synchronized inflation across multiple late cases:
  `unicode_string_heavy`, `ffi_shared_call_stress`, `gpu_graphics_submit`, and
  `allocator_large_object_churn` all got slower for every language, not just
  Kain.
- The cooled focused rerun restored `unicode_string_heavy` to a near-parity
  shape (`10.814 ms` Kain vs `10.037 ms` Rust vs `10.055 ms` C++), so the full
  suite's `169.885 ms` Kain number is not the correct semantic read.
- Treat the canonical `string_ops` and `machine_stones_shatter_loop` wins as
  real, and treat the late `unicode_string_heavy` full-suite spike as machine
  noise.

Current latest full-suite truth says the best remaining valuable implemented
speedup targets are:

- `crypto_block_cipher` (`1.231x` behind C++): the cleanest honest next target.
  This wants alien bit-lane work, rotate/mix lowering scrutiny, and maybe a
  solver-backed table or branchless substitution attack.
- `sim_nbody_gravity` (`1.172x` behind C++): promising math/codegen territory,
  but likely broader than the string lane.
- `sim_cfd_pressure_projection` (`1.130x` behind C++): still a real numeric-core
  deficit with no proxy caveat.
- `ownership_memory` (`1.122x` behind C++): smells like scalarization or
  register-residency debt.
- `struct_method` (`1.089x` behind C++): small but honest call/lowering debt.
- `native_map_lookup` is no longer a C++ problem; the real frontier is the
  small `1.051x` gap to Zig.

Rows that should stay out of the next "easy compiler win" bucket:

- `http_server_concurrency` is still a `semantic-proxy` runtime/network lane.
- `rayon_parallel_reduce` remains explicitly proxy-shaped.
- `unicode_string_heavy` does not justify a new benchmark row yet; the focused
  rerun says it is already near parity and most substring work sits outside the
  timed inner loop.

Recommendation for the next automation pass:

- Move from text lowering to `crypto_block_cipher` next; that row has the best
  mix of honesty, remaining deficit, and plausible solver-backed upside.
- If a new benchmark is added later, use `std::time::deadline_millis` /
  `deadline_elapsed` inside the Kain row only as a symmetric guardrail for
  long-running work, not as a measurement shortcut.
