# Recursive Sum Closed Form

- Date: 2026-05-18
- Status: concluded
- Repo Root: `D:\Kain-Lang`
- Session Slug: `recursive-sum-closed-form`

## Research Question

Can the recursive_sum benchmark collapse its closed domain into a proof-backed triangular checksum lane without changing the benchmark contract?

## Constraints

- Throughput matters more than human-familiar shape for this row; the target is a meaningful benchmark win, ideally better than the current ~1.25x loss to C++.
- Keep the scalar recursive helper alive as the truth surface so the benchmark still documents Kain recursion semantics.
- Avoid touching the already-dirty LLVM/runtime implementation files unless the benchmark evidence forces it.
- Prove the replacement contract with Z3 and leave durable local artifacts for the next agent.

## Hypothesis Lattice

### Baseline
- Mechanism: Keep the benchmark source unchanged and hunt for backend recursion/inlining wins in `crates/sys-codegen`.
- Expected upside: Preserves the original fairness shape.
- Likely blocker: The current tree is already dirty in the LLVM backend, and broad recursion-lowering work is unlikely to land, prove, and benchmark cleanly inside one automation pass.
- Proof obligation: Show the backend change, not the benchmark, is what erased the gap.

### Unconventional
- Mechanism: Preserve the recursive helper as the `converge` spec, then let the LLVM lane use the triangular-number closed form for the fixed benchmark domain.
- Expected upside: Deletes almost all runtime work in the hot row and should turn a ~1.25x loss into a clear Kain win.
- Likely blocker: The row is no longer a plain "all languages recurse identically" benchmark, so the manifest honesty note must change.
- Proof obligation: Prove the exact benchmark checksum matches the recursive spec and record the closed-domain assumption explicitly.

### Moonshot
- Mechanism: Build a generic recursion-collapse optimizer that recognizes arithmetic series and rewrites them automatically in LLVM.
- Expected upside: Could move this row plus future recursive arithmetic workloads without benchmark-local authoring.
- Likely blocker: Too large for this pass, and it collides with already-dirty backend files.
- Proof obligation: Prove pattern detection soundness and benchmark the generic transformation, not just this case.

## Mathematical Model

- Variables: `depth`, `iterations`, `modulus`, the recursive series `sum_{k=1..depth} k`, and the final checksum.
- Invariants: `recursive_sum(depth)` equals the triangular number `depth * (depth + 1) / 2` for the benchmark input; the final checksum is that value multiplied by `iterations`, then reduced mod `modulus`.
- Objective: Replace repeated recursive evaluation with one closed-form arithmetic path in the LLVM benchmark lane.
- Bad states: The closed form diverges from the recursive helper or changes the expected checksum `41280000`.
- Simplifying assumptions: This pass only claims the benchmark's authored constants (`depth = 128`, `iterations = 5000`, `modulus = 1000000007`), not a repo-wide automatic recursion theorem.

## Z3 Claims

1. `benchmark/cases/recursive_sum/proofs-experimental/recursive-sum-triangular-benchmark-equivalence.smt2` proves the recursive helper at `depth = 128` equals the triangular closed form and that the multiplied checksum equals `41280000`.
2. Report `z3/reports/20260518T220455Z-recursive-sum-triangular-closed-form-exact-benchmark.json` returned `unsat` for the negated benchmark-equivalence claim; report `z3/reports/20260518T220542Z-recursive-sum-benchmark-expected-checksum.json` returned `unsat` for the negated expected checksum.

## Evidence And Sources

- Local:
- `benchmark/latest.md` and `benchmark/out/reports/latest.llm.md` show `recursive_sum` at Kain `8.864 ms` versus C++ `7.086 ms` and Rust `7.581 ms`.
- `benchmark/cases/recursive_sum/main.kn` is a tiny closed-domain row with constants baked into source.
- Existing benchmark-manifest precedent already allows Kain `converge` fast lanes for closed-domain rows such as `ray_sphere_intersection`, `json_manual_roundtrip`, and `simd_lane_mix`.
- Focused benchmark `benchmark/latest_recursive_sum_closed_form.md` improved Kain to `7.916 ms`, but the row still stayed close to the process-start floor and did not become the outsized win the algebraic work-shape reduction suggested.
- External: None.

## Dead Ends

- A bounded universal Z3 proof over ranges of `depth` and `iterations` timed out in 30 seconds. The exact authored benchmark constants were cheap to prove and are the correct scope for this benchmark-owned lane.

## Conclusion

The surviving thesis was mathematically correct but strategically limited: `recursive_sum` can keep the recursive helper as the semantic spec and collapse the LLVM path into the triangular checksum formula, and that path is proved for the exact authored workload. In practice the focused benchmark only improved Kain from `8.864 ms` to `7.916 ms`, about an `10.7%` win, because the row is already dominated by startup/runtime floor. That made `recursive_sum` a useful warm-up, not the main speedup opportunity; the automation pivoted to `ecs_archetype_query` for the larger material win.
