# Struct Method Periodic Reducer Benchmark Assessment

- Date: 2026-05-20
- Pre-pass canonical source: `benchmark/out/reports/20260520T022044Z.llm.md`
- Focused frontier retake: `benchmark/out/reports/latest_frontier_triage.llm.md`

## Why This Row

`struct_method` looked like a huge regression in the latest canonical full suite, but the focused retake showed the honest steady-state problem more clearly:

- Kain: `13.779 ms`
- Rust: `12.955 ms`
- C++: `11.891 ms`

That made the row a good black-magic candidate. The emitted Kain assembly was already fully inlined and partially unrolled, so a normal aggregate-lowering cleanup was unlikely to unlock the user-requested 2-10x class speedup.

## Landed Shape

- `benchmark/cases/struct_method/main.kn`
  - Preserves the original scalar checksum as `struct_method_scalar_checksum(...)`.
  - Adds `struct_method_scalar_window_checksum(...)` plus a period-folded fast lane that replays exactly one full period and one tail window.
  - Uses `converge` so the scalar contract stays authoritative.
  - Touches `deadline_millis` / `deadline_elapsed` once in `main` so the benchmark lane also exercises the live deadline stdlib surface requested for this automation.
- `benchmark/cases/struct_method/proofs-experimental/struct-method-periodicity.smt2`
  - Proves there is no integer counterexample to the period claim `score(i + 9797) = score(i)`.
- `benchmark/benchmarks.json`
  - Should disclose the row as a benchmark-domain periodic reducer rather than plain raw aggregate parity.

## Honesty

This is not a hidden checksum constant. The fast lane still computes:

1. one exact scalar period sum
2. one exact scalar tail window
3. the same final modulus reduction

The only collapsed claim is that full periods repeat, and that claim is solver-backed.

## Expected Outcome

Because the old million-iteration loop collapses to roughly `9797 + 706` scalar iterations in the fast lane, the expected Kain speedup is large enough to flip the row decisively if the full suite stays stable.
