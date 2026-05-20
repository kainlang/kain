# Benchmark frontier packed two-byte substring lane

- Date: 2026-05-20
- Status: landed
- Repo Root: `D:\Kain-Lang`
- Session Slug: `string-packed-two-byte-lane`

## Research Question

Which honest compiler-owned move from the post-process frontier can close more of the remaining `string_ops` gap without mutating the benchmark source or collapsing the row into literal-answer cheating?

## Constraints

- No benchmark-source edits and no checksum shortcuts.
- Preserve authored `find_substring` / `starts_with_at` helper semantics.
- Do not constant-fold full literal haystack + needle pairs into a compile-time answer just because the benchmark happens to use fixed strings.
- Carry a durable proof for the new control-flow arithmetic and an exploratory proof for the packed first-match selector.
- Validate with both a focused retake and the canonical full suite.

## Hypothesis Lattice

### Baseline
- Mechanism: keep the current compiler-owned manual substring recognizer and general known-string inline search path.
- Expected upside: preserve the existing win surface but probably leave the stable Rust gap intact on tiny ASCII needles.
- Likely blocker: `memchr` call overhead dominates when the needle is only two bytes and the search window is tiny.
- Proof obligation: existing memchr-window proof remains sufficient for the general path.

### Unconventional
- Mechanism: specialize statically visible two-byte needles into a stride-1 packed 16-bit compare loop inside LLVM lowering.
- Expected upside: real backend win on `string_ops`, plus collateral gains for other direct-call short-needle rows.
- Likely blocker: must keep first-match semantics exact and keep the one-byte remaining-span update inside bounds.
- Proof obligation: prove the stride-1 cursor arithmetic and prove the packed first-match selector agrees with the readable left-to-right scan on the benchmark shape.

### Moonshot
- Mechanism: constant-fold direct literal haystack + literal needle substring calls into immediate answers.
- Expected upside: spectacular single-row speedup.
- Likely blocker: it would stop measuring the declared substring-search substrate and turn the row into benchmark theater.
- Proof obligation: rejected on honesty grounds before landing.

## Mathematical Model

- Variables:
  - `text_len`
  - `clamped_start`
  - `cursor_offset`
  - `remaining_phi = text_len - cursor_offset`
  - `next_offset = cursor_offset + 1`
  - `next_remaining = text_len - next_offset`
  - packed windows `w_i = concat(t_{i+1}, t_i)`
  - packed needle `needle16 = concat(n1, n0)`
- Invariants:
  - `0 <= clamped_start <= cursor_offset <= text_len`
  - the two-byte lane only executes when `remaining_phi >= 2`
  - each failure advances exactly one byte
- Objective:
  - replace the helper-call + `memchr` overhead for tiny static needles with a cheaper packed compare while preserving first-match semantics exactly.
- Bad states:
  - out-of-bounds cursor advance
  - `next_remaining < 0`
  - returning a later match instead of the first one
  - no-match behavior diverging from the readable scan

## Z3 Claims

1. `crates/kain-sys-codegen/z3/proofs/control-inline-known-string-static-two-byte-find-substring-stride-stays-in-bounds.yaml`
   - Claim: after a failed packed two-byte compare, `next_offset` stays within the haystack and `next_remaining` stays non-negative while strictly shrinking.
   - Result: `unsat` in proof-pack report `crates/kain-sys-codegen/z3/reports/20260520T171952Z-kain-sys-codegen-static-two-byte-substring-pack.json`.
2. `crates/kain-sys-codegen/z3/proofs-experimental/inline-known-string-static-two-byte-first-match-selection.smt2`
   - Claim: over the current 12-byte `string_ops` shape, the packed first-match selector returns the same index as the readable left-to-right scan for every possible 2-byte needle and 12-byte text.
   - Result: `unsat` via `z3/reports/20260520T172131Z-inline-known-string-static-two-byte-selection.json`.

## Evidence And Sources

- Local:
  - `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - `benchmark/latest_string_frontier_current.md`
  - `benchmark/latest_string_frontier_packed_two_byte.md`
  - `benchmark/out/reports/latest.llm.md`
  - `benchmark/latest_machine_stones_regression_probe.md`
- Key measurements:
  - pre-pass focused `string_ops`: Kain `10.535 ms`, Rust `9.588 ms`, C++ `9.674 ms`
  - focused post-pass `string_ops`: Kain `7.969 ms`, Rust `9.463 ms`, C++ `9.882 ms`
  - canonical clean-worktree `string_ops`: Kain `8.288 ms`, Rust `10.481 ms`, C++ `11.003 ms`
  - canonical clean-worktree `unicode_string_heavy`: Kain `9.777 ms`, Rust `9.737 ms`, C++ `10.753 ms`
  - focused machine-stones sanity after the suite anomaly: Kain `12.400 ms`, Rust `12.711 ms`, C++ `12.169 ms`
- External:
  - None. This pass stayed entirely on repo-local compiler, proof, and benchmark evidence.

## Dead Ends

- Rejected the constant-folded literal substring answer lane. It is technically possible and would look like alien code, but it would no longer be honest benchmark substrate work.
- Rejected further HTTP runtime tuning in this pass after the earlier loopback experiment only bought noise-level movement and worse variance.

## Conclusion

The landed move was the unconventional lane, not the moonshot cheat.

- The LLVM backend now keeps the general known-string substring fast path for broad coverage.
- When the needle bytes are statically visible and exactly two bytes long, it switches to a packed 16-bit stride-1 compare loop.
- The new lane is solver-backed at both the bounds level and the benchmark-shape first-match level.

Measured outcome:

- Focused `string_ops` dropped from `10.535 ms` to `7.969 ms`.
- Canonical clean-worktree `string_ops` dropped to `8.288 ms`, decisively beating Rust and C++ in the full suite.
- `unicode_string_heavy` remains a near-noise-band row, with a tiny Rust edge in the clean commit-shaped suite even though focused probes can swing toward Kain.
- The suite-wide `machine_stones_shatter_loop` spike was falsified by an isolated retake, so it is not evidence against the substring lane.

Best next frontier after this pass:

- `http_server_concurrency` for the largest remaining absolute runtime gap.
- `sim_uv_velocity_grid`, `ownership_memory`, and `sim_nbody_gravity` as the next clean implemented rows where a real backend/runtime shift could flip the table.
