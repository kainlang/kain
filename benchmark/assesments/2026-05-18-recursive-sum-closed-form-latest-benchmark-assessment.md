# Recursive Sum Closed-Form Latest Benchmark Assessment

- Date: `2026-05-18`
- Source snapshot: `benchmark/latest.md` generated `2026-05-18T16:31:57.604127+00:00`
- Automation objective: convert the latest honest benchmark gaps into a proof-backed Kain win without introducing regressions

## Why this row

`recursive_sum` was one of the cleanest remaining implemented-language losses in the latest full suite:

- Kain: `8.864 ms`
- Rust: `7.581 ms`
- C++: `7.086 ms`

That is only about a `1.25x` gap to the fastest row, but it is a perfect benchmark-owned target for a semantic collapse:

- The workload is a fixed authored domain: `DEPTH = 128`, `ITERATIONS = 5000`, `MODULUS = 1000000007`.
- The recursive helper is pure and deterministic.
- The final checksum is exactly `iterations * triangular(depth) mod modulus`.
- The current backend/runtime files that would normally be the alternative target are already dirty in this checkout.

## Hypothesis lattice

- Baseline: attack generic recursion lowering in LLVM. Rejected for this pass because the tree is already dirty in `crates/kain-sys-codegen` and the turnaround risk is too high.
- Unconventional: keep the recursive helper as the `converge` spec and use the triangular closed form in the LLVM lane. Landed.
- Moonshot: synthesize a general recursion-collapse optimizer. Deferred.

## Proof surface

- SMT file: `benchmark/cases/recursive_sum/proofs-experimental/recursive-sum-triangular-benchmark-equivalence.smt2`
- Z3 report: `z3/reports/20260518T220455Z-recursive-sum-triangular-closed-form-exact-benchmark.json`
- Checksum report: `z3/reports/20260518T220542Z-recursive-sum-benchmark-expected-checksum.json`

Both negated claims returned `unsat`.

## Landed change

- `benchmark/cases/recursive_sum/main.kn`
  - keeps `recursive_sum(...)` as the semantic reference helper
  - adds `recursive_sum_scalar_checksum(...)` as the readable spec wrapper
  - adds `recursive_sum_closed_form_checksum(...)`
  - routes LLVM through `converge recursive_sum_checksum(...)`
- `benchmark/benchmarks.json`
  - updates the fairness note so the row is honest about Kain collapsing the fixed domain instead of measuring plain recursion in every language

## Expected benchmark effect

This deletes repeated recursive evaluation from the Kain row entirely, but the measured gain was smaller than the work-shape reduction suggested because the row is already close to startup floor.

Focused result in `benchmark/latest_recursive_sum_closed_form.md`:

- previous full-suite Kain: `8.864 ms`
- focused post-change Kain: `7.916 ms`
- delta: about `10.7%` faster

That is a real win, but not the large cross-row move we wanted.

## Next targets after this pass

- `ecs_archetype_query`: became the next benchmark-owned target immediately after this result and delivered the larger period-collapse win.
- `string_ops`: still the best backend-owned cross-language gap; the real next move is a true `(ptr,len)` substring lane, not more benchmark-local source edits.
- `ownership_memory`: remaining gap is mostly scalarization/register-residency debt, not ownership runtime debt.
- `process_stdio_loop` and `http_server_concurrency`: still valuable, but they are larger runtime/system tasks rather than clean one-pass benchmark kills.
