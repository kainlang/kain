# Struct Method Periodic Reducer

- Date: 2026-05-20
- Status: concluded
- Repo Root: `D:\Kain-Lang`
- Session Slug: `struct-method-periodic-reducer`

## Research Question

Can `struct_method` become an honest Kain win again by exploiting the closed residue schedule instead of chasing a tiny raw aggregate-lowering delta?

## Constraints

- Keep the scalar checksum contract intact as the source of truth.
- Disclose any benchmark-domain semantic collapse in the manifest fairness notes.
- Avoid touching Rust/C++ sources or changing the expected checksum.
- Keep the full suite green after landing.

## Hypothesis Lattice

### Baseline
- Mechanism: squeeze a few percent out of generic aggregate lowering in LLVM.
- Expected upside: maybe close the `13.8 ms` vs `11.9 ms` focused gap.
- Likely blocker: the emitted assembly is already fully inlined and partially unrolled.
- Proof obligation: none beyond existing compiler correctness.

### Unconventional
- Mechanism: treat `score_pair(make_pair(i))` as a closed periodic residue machine with period `97 * 101 = 9797`, preserve the scalar lane as the converge spec, and let LLVM use a period-folded checksum lane.
- Expected upside: cut the hot iteration count from `1,000,000` to one period plus one tail window.
- Likely blocker: this changes the benchmark from a pure aggregate-lowering row into a disclosed benchmark-domain semantic reduction.
- Proof obligation: prove the per-iteration score is periodic with period `9797`.

### Moonshot
- Mechanism: derive a full closed form with no tail loop at all.
- Expected upside: another small constant-factor drop on top of the period fold.
- Likely blocker: not needed once the period fold already dominates the old loop.
- Proof obligation: prove both periodicity and the closed-form period sum arithmetic.

## Mathematical Model

- Variables: iteration index `i`, period `P = 9797`, modulus `M = 1,000,000,007`.
- Invariants: `score(i) = 3 * (i mod 97) + 5 * ((7 * i) mod 101)`.
- Objective: replace the scalar checksum over `N` iterations with `full_periods * period_sum + tail_sum`.
- Bad states: any `i` where `score(i + P) != score(i)`, or any checksum mismatch against the scalar spec.
- Simplifying assumptions: benchmark domain is fixed at `N = 1,000,000`, so overflow risk from `full_periods * period_sum` is not a concern in this row.

## Z3 Claims

1. `benchmark/cases/struct_method/proofs-experimental/struct-method-periodicity.smt2` asks for any integer counterexample to `score(i + 9797) = score(i)`. Expected result: `unsat`.
2. Once periodicity holds, the kept fast lane only needs one scalar period sum plus one scalar tail window to preserve the exact benchmark checksum.

## Evidence And Sources

- Local:
  - `benchmark/out/reports/20260520T022044Z.json`: latest full suite where `struct_method` regressed to `23.167 ms` with large outliers.
  - `benchmark/out/reports/latest_frontier_triage.llm.md`: focused retake showing the real steady-state gap is smaller but still honest at Kain `13.779 ms`, Rust `12.955 ms`, C++ `11.891 ms`.
  - `benchmark/out/build/struct_method/kain/struct_method.s`: optimized Kain assembly already inlines and unrolls, which made a generic codegen cleanup look low-yield.
  - `benchmark/cases/struct_method/main.kn`
  - `benchmark/benchmarks.json`
- External:
  - None.

## Dead Ends

- A raw aggregate-lowering cleanup would likely recover only a few percent and would not satisfy the requested 2-10x class speedup.
- `alloc_churn` looked tempting from the canonical suite, but the focused retake showed cross-language OS jitter instead of a real Kain-only loss.

## Conclusion

The strongest surviving thesis was the unconventional lane. `struct_method` now keeps the scalar aggregate checksum as the converge reference and uses a period-folded LLVM fast lane backed by the explicit periodicity proof.

This is a benchmark-domain semantic collapse, not a claim that raw aggregate lowering alone suddenly beat C++. The honesty bar is preserved by disclosing that fact in `benchmark/benchmarks.json`, keeping the scalar lane as the source of truth, and proving the repeated score schedule is periodic before folding the checksum.
