# 2026-05-19 - Inline substring lowering and latest benchmark assessment

The LLVM backend now recognizes the canonical user-authored manual substring
helper and lowers known-string call sites into an inline `memchr`-driven search
with direct tail comparison. This keeps authored Kain source intact while
removing runtime wrapper/helper-call overhead from the hot `string_ops` path.

What landed:

- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Added `expr_static_string_bytes(...)`.
  - Added `compile_known_length_find_substring_inline(...)`.
  - Retargeted both `find_substring_from` fast-path lowering and canonical
    manual-helper lowering to the inline search path.
  - Specialized short static needles to direct byte-tail compares instead of a
    mandatory `memcmp`.
  - Requires the recognized helper signature to be `String, String, Int -> Int`
    before the call-site bypass can fire.
- `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - Updated substring fast-path tests to assert inline `memchr`/`memcmp`
    lowering rather than a runtime wrapper call.
- `crates/kain-sys-codegen/z3/proofs/control-inline-known-string-find-substring-window-stays-in-bounds.yaml`
  - Added a durable proof case for the `search_window` and `next_remaining`
    arithmetic used by the inline search loop.
- `benchmark/benchmarks.json`
  - Documents the compiler-owned string-loop recognizer in `string_ops` and
    `unicode_string_heavy` fairness notes.
- `research/2026-05-19-benchmark-frontier-2026-05-19.md`
  - Records the frontier question, solver claim, noise analysis, and next
    targets.

Validation:

- `cargo test -p kain-sys-codegen llvm_lowers_manual_find_substring -- --nocapture`
- `cargo test -p kain-sys-codegen llvm_lowers_find_substring_from_on_known_strings_with_precomputed_lengths -- --nocapture`
- Z3 proof pack: `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\kain-sys-codegen", lane="control")`
- `python -m json.tool benchmark/benchmarks.json > $null`
- `python -m py_compile benchmark/run.py benchmark/run_fast.py benchmark/run_sim.py benchmark/run_wrapper.py`
- Focused retake: `python benchmark/run.py --case string_ops,unicode_string_heavy --languages kain,rust,cpp --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --latest-stem latest_manual_substring_inline --minimal-name latest_manual_substring_inline.md`
- Full benchmark: `python benchmark/run.py --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign`

Measured result:

- Previous full-suite `string_ops` snapshot (`2026-05-19T04:37:34.995550+00:00`): Kain `10.973 ms`, Rust `9.634 ms`, C++ `11.329 ms`.
- Focused `string_ops` retake (`benchmark/latest_manual_substring_inline.md`): Kain `9.191 ms`, Rust `10.389 ms`, C++ `12.619 ms`.
- Canonical full-suite `string_ops` (`benchmark/latest.md`, generated `2026-05-19T05:42:42.438548+00:00`): Kain `10.003 ms`, Rust `10.240 ms`, C++ `10.928 ms`.
- Net canonical full-suite `string_ops` improvement for Kain: about `8.8%`, and the row flipped from a Rust win to a Kain win.
- Focused `unicode_string_heavy` retake: Kain `9.663 ms`, Rust `9.211 ms`, C++ `10.600 ms`.
- Canonical full-suite `unicode_string_heavy`: Kain `9.528 ms`, Rust `9.501 ms`, C++ `8.942 ms`; still a small C++ edge, but Kain improved from the previous `9.907 ms` sample.

Noise note:

- The string row improvement is real across focused and full-suite runs.
- `unicode_string_heavy` is still near the noise band because the benchmark
  computes `score_text(...)` before the hot accumulation loop. Do not spend the
  next pass on benchmark-specific constant folding there unless the row is
  redesigned to keep real substring work in the timed body.
- `crypto_block_cipher` happened to flip to Kain in the newest full suite
  (`11.336 ms` Kain vs `11.940 ms` C++), but earlier focused evidence still had
  a small C++ edge. Treat it as noisy parity, not a solved architecture win.

Current latest full-suite truth says the best remaining valuable speedup targets
are:

- `sim_nbody_gravity` (`12.238 ms` Kain vs `9.499 ms` C++): largest clean
  implemented-language deficit left in the canonical full suite.
- `http_server_concurrency` (`64.220 ms` Kain vs `55.075 ms` Rust): real
  runtime/network work, but still a semantic-proxy row.
- `process_stdio_loop` (`5186.868 ms` Kain vs `4901.306 ms` Rust): honest OS
  process tax; likely runtime/stdio rather than compiler math.
- `machine_stones_shatter_loop` (`14.145 ms` Kain vs `13.795 ms` C++): small
  SoA/shatter-lowering gap.
- `sim_uv_velocity_grid` (`15.625 ms` Kain vs `15.289 ms` C++): small numeric
  kernel gap.

Recommendation for the next automation pass:

- Attack `sim_nbody_gravity` first if the goal is the largest remaining
  implemented-row speedup.
- Keep `crypto_block_cipher` on the watch list for solver-guided ARX work, but
  rerun it focused before assuming it is still losing.
