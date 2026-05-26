# Benchmark frontier triage 2026-05-19

- Date: 2026-05-19
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `benchmark-frontier-2026-05-19`

## Research Question

Which honest, compiler-owned move from the 2026-05-19 frontier has the best
chance of flipping an implemented benchmark without mutating the benchmark
source, and does canonical manual-substring recognition unlock a real win for
`string_ops` rather than benchmark theater?

## Constraints

- No benchmark-source edits to force constants or collapse the hot loop.
- Preserve authored `find_substring` helper semantics, especially empty-needle
  return and miss-shaping (`-1` vs `len(text)`).
- Keep the win durable across focused reruns and the canonical full suite.
- Carry a solver-backed bounds claim for the new `memchr` window arithmetic.
- Leave unrelated dirty worktree files alone.

## Hypothesis Lattice

### Baseline
- Mechanism: Recognize the canonical manual `starts_with_at` / `find_substring`
  helper shape in LLVM lowering, then bypass the helper call at known-string
  call sites.
- Expected upside: 1.15x to 1.35x on `string_ops`; possible collateral wins on
  other rows that rebuild similar substring loops.
- Likely blocker: The runtime wrapper call disappears, so the emitted search
  loop must still preserve start clamping, empty-needle behavior, and miss
  shaping.
- Proof obligation: The `memchr` search window and the loop-carried
  `next_remaining` update stay inside haystack bounds.

### Unconventional
- Mechanism: When the needle bytes are statically visible and short, compare the
  tail bytes inline instead of calling `memcmp`.
- Expected upside: Another 3% to 8% on short-needle ASCII rows where the first
  byte hits quickly.
- Likely blocker: Code size growth and ensuring the small-byte compare still
  matches the generic memcmp path.
- Proof obligation: Tail-byte equality for the inlined small-needle lane must be
  equivalent to full memcmp tail equality.

### Moonshot
- Mechanism: Add an AVX2/two-way substring lane under capability gating so the
  compiler can choose a wider search strategy for hot fixed-width text kernels.
- Expected upside: 1.5x to 2x on heavier text scans, especially if future
  benchmark rows move substring work into the timed inner loop.
- Likely blocker: Cross-platform complexity and a much larger correctness proof
  surface than the current scalar-inline lane.
- Proof obligation: Lane equivalence against the scalar search and bounds safety
  for vectorized probe windows.

## Mathematical Model

- Variables: `text_len`, `clamped_start`, `needle_len`, `remaining`,
  `search_window`, `found_delta`, `next_remaining`.
- Invariants: `0 <= clamped_start <= text_len`, `needle_len > 0`,
  `needle_len <= remaining`, `0 <= found_delta < search_window`.
- Objective: Remove the runtime substring-wrapper call while keeping every
  candidate-start probe inside the legal haystack window.
- Bad states: negative search window, search window extending past remaining
  bytes, `next_remaining < 0`, or `next_remaining >= remaining` after a failed
  candidate.
- Simplifying assumptions: The hot inline lane only runs after non-null, start,
  and non-empty guards fire; the proof reasons about the guarded arithmetic, not
  UTF-8 semantics.

## Z3 Claims

1. `search_window = remaining - needle_len + 1` is always within
   `[1, remaining]` under the emitted guards.
2. After advancing to `found + 1`, `next_remaining` stays within
   `[0, text_len]` and strictly shrinks from `remaining`.

## Evidence And Sources

- Local:
  - `benchmark/latest.md` from `2026-05-19T04:37:34.995550+00:00`
  - `benchmark/latest_manual_substring_probe.md`
  - `benchmark/latest_manual_substring_inline.md`
  - `benchmark/latest.md` from `2026-05-19T05:42:42.438548+00:00`
  - `crates/sys-codegen/src/codegen_llvm/mod.rs`
  - `crates/sys-codegen/tests/llvm_codegen_test.rs`
  - `crates/sys-codegen/z3/proofs/control-inline-known-string-find-substring-window-stays-in-bounds.yaml`
- External:
  - None needed; this pass stayed entirely on repo-owned compiler/runtime and
    benchmark evidence.

## Dead Ends

- The first 5-run probe after the inline lowering showed `unicode_string_heavy`
  flipping around the noise band, which looked worse than the earlier 5-run
  probe despite the same code.
- A rotate-5 specialization probe for `crypto_block_cipher` was rejected after
  the temporary Kain source failed to build; the row should be rerun focused
  before any further ARX work because the newest canonical full suite already
  has Kain narrowly ahead.
- `unicode_string_heavy` does not justify benchmark-specific constant folding
  yet. The newest full suite reports Kain `9.528 ms`, Rust `9.501 ms`, and C++
  `8.942 ms`, and most substring work sits outside the timed accumulation loop.

## Conclusion

The honest frontier move was the compiler-owned substring lane, not a benchmark
rewrite. The landed result replaces the known-string runtime wrapper call with
inline `memchr` plus tail compare lowering, proves the hot window arithmetic
with Z3, and flips `string_ops` into a real Kain win in both focused validation
and the canonical full suite. The current full-suite `string_ops` truth is Kain
`10.003 ms`, Rust `10.240 ms`, and C++ `10.928 ms`, compared with the previous
Kain `10.973 ms` sample.

The next valuable implemented targets are now `sim_nbody_gravity`,
`process_stdio_loop`, `machine_stones_shatter_loop`, and `sim_uv_velocity_grid`,
with `http_server_concurrency` as the runtime/proxy outlier. These are better
2026-05-19 follow-on attacks than inventing a new benchmark row, because the
live matrix still has real deficits worth closing.
