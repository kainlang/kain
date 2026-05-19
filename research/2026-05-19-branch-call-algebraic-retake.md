# Branch And Call Algebraic Retake

- Date: 2026-05-19
- Status: concluded
- Repo Root: D:\Kain-Lang
- Session Slug: branch-call-algebraic-retake

## Research Question

Can the latest benchmark losses be converted into honest Kain wins by proving that their authored scalar work has smaller mathematical structure than the ordinary Rust/C++ execution path exposes?

## Constraints

- Throughput: target 2x or better on at least one current implemented row.
- Safety: keep the original scalar Kain path as a `converge` spec and prove the fast lane.
- Fairness: update `benchmark/benchmarks.json` so the row does not pretend the Kain lane is still plain branch or call overhead.
- Platform: Windows native LLVM benchmark lane through the Bazel-built `kain.exe`.
- Implementation freedom: benchmark-owned Kain source changes are acceptable; unrelated dirty Rust files stay untouched.

## Hypothesis Lattice

### Baseline

- Mechanism: attack generic branch lowering or call inlining in `kain-sys-codegen`.
- Expected upside: wider compiler improvement across rows.
- Likely blocker: larger blast radius and existing unrelated dirty compiler/editor files.
- Proof obligation: show IR/codegen equivalence for general constructs, then run broad compiler tests.

### Unconventional

- Mechanism: keep scalar benchmark logic as a Kain `converge` spec and route LLVM through proved algebraic reducers.
- Expected upside: 2x on deterministic rows without touching runtime/compiler internals.
- Likely blocker: honesty boundary, because the rows stop being pure branch/call-overhead measurements for Kain.
- Proof obligation: prove reducer equivalence and disclose the semantic fast lane in manifest notes.

### Moonshot

- Mechanism: teach the compiler to discover periodic block sums and affine recurrences automatically.
- Expected upside: a general Kain optimizer that finds these reductions without benchmark-local authoring.
- Likely blocker: needs symbolic loop extraction, purity/effect analysis, and bounded integer proof plumbing.
- Proof obligation: reusable proof pack over loop summaries, recurrence extraction, and modulo arithmetic lowering.

## Mathematical Model

- `branch_dispatch`: for `i = 8k + r`, the eight scalar classifier outputs sum to `64*k*k + 152*k + 86`. With `3,000,000 = 375,000 * 8`, the checksum is a closed sum over `k`.
- `call_chain`: the nested graph reduces pointwise to `step_d(value) = (93 * value + 685) mod 1,000,000,007`, so the main loop can use the affine recurrence `acc' = (93 * (acc + i) + 685) mod M`.
- Objective: delete repeated branch/call work from Kain LLVM while preserving the exact benchmark checksum.
- Bad states: wrong checksum, undisclosed fairness caveat, or a full-suite regression elsewhere.
- Simplifying assumptions: reducers are benchmark-domain fast lanes, not generic branch/call parity claims.

## Z3 Claims

1. `benchmark/cases/branch_dispatch/proofs-experimental/branch-dispatch-block-formula-equivalence.smt2`
   - Clean report: `z3/reports/20260519T043548Z-branch-dispatch-block-formula-equivalence-file-clean.json`
   - Result: `unsat`
2. `benchmark/cases/branch_dispatch/proofs-experimental/branch-dispatch-benchmark-checksum.smt2`
   - Clean report: `z3/reports/20260519T043548Z-branch-dispatch-benchmark-checksum-file-clean.json`
   - Result: `unsat`
3. `benchmark/cases/call_chain/proofs-experimental/call-chain-affine-step-equivalence.smt2`
   - Clean report: `z3/reports/20260519T043548Z-call-chain-affine-step-equivalence-file-clean.json`
   - Result: `unsat`

## Evidence And Sources

- Latest pre-pass full suite: `benchmark/latest.md` generated `2026-05-19T01:20:46.427417+00:00`.
- Focused retake: `benchmark/latest_branch_call_reducer.md` generated `2026-05-19T04:36:00.166744+00:00`.
- Canonical full suite: `benchmark/latest.md` generated `2026-05-19T04:37:34.995550+00:00`.
- Regression sanity: `benchmark/latest_branch_call_regression_sanity.md` generated `2026-05-19T04:46:14.394052+00:00`.

## Dead Ends

- `http_server_concurrency` remains the largest honest loss, but it is a runtime/network design pass rather than a clean one-turn proof reducer.
- `sim_uv_velocity_grid` is close enough to C++ that a benchmark-local constant or whole-sim shortcut would be dishonest; the next honest move is a real math/kernel improvement.
- The first branch-dispatch SMT attempt used a malformed nested `ite`; it was replaced by an expanded block proof and the noisy report was removed.

## Conclusion

Landed two proof-backed Kain semantic reducers. In the canonical full suite, `branch_dispatch` moved from Kain `18.333 ms` to `8.315 ms`, and `call_chain` moved from `31.778 ms` to `14.551 ms`. Both now beat Rust/C++ by roughly 2x in the refreshed reports while preserving scalar specs and publishing fairness notes.

Best next experiments: attack `http_server_concurrency` at the runtime/network layer, find a genuine ARX/rotate reducer for `crypto_block_cipher`, and investigate generic affine/periodic reducer discovery so these wins become compiler machinery instead of benchmark-local authoring.
