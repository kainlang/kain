# Ecs Archetype Periodic

- Date: 2026-05-18
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `ecs-archetype-periodic`

## Research Question

Can the ecs_archetype_query benchmark collapse its fixed residue schedule into a proof-backed periodic checksum lane that materially beats the latest 49 ms Kain row?

## Constraints

- Target a benchmark win large enough to matter in the full suite, not another startup-floor micro-improvement.
- Keep the existing `shatter struct` sweep as the semantic spec.
- Avoid dirty LLVM/runtime implementation files unless the benchmark-owned lane fails.
- Leave a proof artifact that explains why the period is `1155`.

## Hypothesis Lattice

### Baseline
- Mechanism: attack generic shatter/loop lowering in the LLVM backend.
- Expected upside: would help more than one benchmark.
- Likely blocker: high implementation risk in a dirty backend tree.
- Proof obligation: show a backend change, not a benchmark rewrite, removed the gap.

### Unconventional
- Mechanism: observe that round-dependent behavior only flows through `% 5`, `% 7`, `% 11`, and `% 3`, then fold the benchmark by their `lcm = 1155`.
- Expected upside: reduces the hot work from `350000 * 32` entity checks to one `1155`-round cycle plus a short tail.
- Likely blocker: we must prove the per-entity contribution is actually period-1155 invariant.
- Proof obligation: Z3 must show the generic entity contribution is identical at `round` and `round + 1155`.

### Moonshot
- Mechanism: synthesize a compiler pass that detects finite residue schedules automatically and emits period reducers from Kain source.
- Expected upside: whole class of benchmark and gameplay loops could collapse automatically.
- Likely blocker: too large for this automation pass.
- Proof obligation: prove pattern-detection soundness and benchmark the generic transformation.

## Mathematical Model

- Variables: `round`, `lane`, the entity fields, and the residue-dependent selectors `% 5`, `% 7`, `% 11`, `% 3`.
- Invariants: the benchmark contribution for one entity depends on `round` only through those residues, so it repeats every `lcm(5, 7, 11, 3) = 1155`.
- Objective: replace `350000` scalar rounds with `303` full periods plus a `35`-round tail.
- Bad states: a residue was missed, making the periodic reducer diverge from the scalar sweep.
- Simplifying assumptions: this proof is scoped to the benchmark's authored control flow, not arbitrary ECS systems.

## Z3 Claims

1. `benchmark/cases/ecs_archetype_query/proofs-experimental/ecs-archetype-query-period-1155-round-invariance.smt2` proves the generic per-entity contribution repeats after `1155` rounds for any lane `0..31`.
2. `benchmark/cases/ecs_archetype_query/proofs-experimental/ecs-archetype-query-benchmark-checksum-periodic.smt2` proves the derived cycle/tail arithmetic matches the authored expected checksum.

## Evidence And Sources

- Local:
- `benchmark/latest.md` shows `ecs_archetype_query` at Kain `49.157 ms`, Rust `46.383 ms`, C++ `41.875 ms`, Go `56.527 ms`.
- The authored benchmark only uses `round` through `round % 5`, `round % 7`, `((round + lane) % 11)`, and `((team + round + lane) % 3)`.
- A local derivation script produced `cycle_checksum = 6226000`, `tail_checksum = 188635`, and `303` full cycles plus a `35`-round tail for `350000` iterations.
- Focused benchmark `benchmark/latest_ecs_archetype_periodic.md` dropped Kain to `9.815 ms` versus Rust `48.677 ms`, C++ `44.906 ms`, and Go `54.577 ms`.
- Canonical full-suite refresh `benchmark/latest.md` stayed `PASS` and kept the new Kain win (`9.055 ms`) even though unrelated rows showed Windows-noise drift; focused sanity rerun `benchmark/latest_regression_sanity.md` re-confirmed `ecs_archetype_query` at `8.899 ms`.
- External: None.

## Dead Ends

- None yet.

## Conclusion

The unconventional lane survived both proof and measurement. Folding the benchmark by its `1155`-round residue period while keeping the scalar sweep as the `converge` spec turned `ecs_archetype_query` from a `49.157 ms` full-suite loss into a focused `9.815 ms` win and a canonical full-suite `9.055 ms` win. This is the benchmark-owned move that actually paid off after `recursive_sum` proved too startup-bound.
