# Packed Two-Byte Substring Lane Benchmark Assessment

- Date: 2026-05-20
- Pre-pass focused baseline: `benchmark/latest_string_frontier_current.md`
- Focused post-pass retake: `benchmark/latest_string_frontier_packed_two_byte.md`
- Post-pass canonical report: `benchmark/out/reports/latest.llm.md`

## Why This Frontier

After the process/runtime win, the clean stable compiler frontier was back to `string_ops`:

- focused pre-pass `string_ops`: Kain `10.535 ms`
- Rust `9.588 ms`
- C++ `9.674 ms`

The row already had a compiler-owned manual substring recognizer, but the hot path was still paying a `memchr`-driven search shape even when the needle was only two static bytes wide.

## Landed Shape

- `crates/sys-codegen/src/codegen_llvm/mod.rs`
  - routes static two-byte needles into `compile_known_length_find_substring_inline_static_two_byte_needle(...)`
  - keeps the authored helper shape and general substring path intact
  - replaces the `memchr` call with a stride-1 packed 16-bit compare for the tiny-static-needle lane
- `crates/sys-codegen/tests/llvm_codegen_test.rs`
  - adds `llvm_lowers_static_two_byte_find_substring_from_to_packed_stride_one_search`
  - keeps the existing known-string and manual-helper general-path regressions green
- Proof artifacts:
  - durable bounds proof:
    - `crates/sys-codegen/z3/proofs/control-inline-known-string-static-two-byte-find-substring-stride-stays-in-bounds.yaml`
  - exploratory first-match proof:
    - `crates/sys-codegen/z3/proofs-experimental/inline-known-string-static-two-byte-first-match-selection.smt2`
- `benchmark/benchmarks.json`
  - updates the row honesty text so reports describe compiler-owned inline substring search with the packed two-byte lane

## Measured Outcome

Focused retake:

- `string_ops`: Kain `7.969 ms`, Rust `9.463 ms`, C++ `9.882 ms`
- `unicode_string_heavy`: Kain `9.052 ms`, Rust `9.163 ms`, C++ `8.466 ms`

Canonical 9-run full suite:

- `string_ops`: Kain `8.288 ms`, Rust `10.481 ms`, C++ `11.003 ms`
- `unicode_string_heavy`: Kain `9.777 ms`, Rust `9.737 ms`, C++ `10.753 ms`
- suite summary: `kain_regressions = 0`, `alert_regressions = 0`

That means the stable focused `string_ops` median improved by about `24.4%` (`10.535 -> 7.969`), and the canonical full suite now shows a decisive Kain win on the row.

## Honesty

This is not a benchmark-only checksum collapse.

- The benchmark source did not change.
- The haystack is not constant-folded into an immediate answer.
- The kept specialization only fires when the needle bytes are statically visible and exactly two bytes wide.
- The general known-string search path and its existing proof remain in place for broader shapes.

The rejected alien move for this pass was literal-call constant folding. It would have been faster, but it would stop measuring the declared substring-search substrate and would not be honest.

## Regression Sanity

The canonical suite showed a scary `machine_stones_shatter_loop` spike (`74.797 ms`) that does not touch this code path.

Focused retake disproved it as a real regression:

- `benchmark/latest_machine_stones_regression_probe.md`
- Kain `12.400 ms`, Rust `12.711 ms`, C++ `12.169 ms`

So the substring lane landed cleanly, and the machine-stones spike is a suite-order/noise artifact rather than fallout from this patch. The adjacent `unicode_string_heavy` row remains honest but noisy: the clean commit-shaped suite still shows only a tiny Rust edge.

## Next Frontier

After this pass, the most valuable remaining honest rows are:

- `http_server_concurrency`: Kain `55.194 ms`, Rust `47.584 ms`
- `sim_uv_velocity_grid`: Kain `17.150 ms`, Rust `15.234 ms`, C++ `14.134 ms`
- `ownership_memory`: Kain `12.037 ms`, Rust `11.525 ms`, C++ `11.352 ms`
- `sim_nbody_gravity`: Kain `9.899 ms`, Rust `9.519 ms`, C++ `8.998 ms`
