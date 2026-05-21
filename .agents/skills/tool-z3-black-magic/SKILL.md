---
name: tool-z3-black-magic
description: Use with an owning subsystem skill when hunting magic constants, branchless rewrites, solver-discovered tables, or proof-backed performance replacements. This skill owns the exploratory solver workflow and benchmark contract, not the subsystem itself.
---

# Tool Z3 Black Magic

Never use this skill alone. Pair it with the owning `bootstrap-*`, `runtime-*`, `lang-*`, or `package-*` skill so the strange math stays attached to the right subsystem.

## Use This For

- Magic-number hunts, branchless selectors, de Bruijn tables, compact classifiers, hash/probe redesigns, and proof-backed unsafe fast paths.
- Solver-guided discovery where the first goal is "find a candidate" and the second goal is "prove the replacement contract".
- Benchmark rows or hot loops where the normal rewrite is too timid.

## Rules

- Treat Z3 as both a search engine and a proof engine: discovery may start with `sat`, but landed replacements need an `unsat` proof for equivalence or the stronger contract you actually rely on.
- Save exploratory artifacts under the nearest subsystem's `z3/proofs-experimental` or pack-local proof area, not in random temp notes.
- Do not replace the owning skill's validation loop; add this workflow on top of it.
- Do not land a weird optimization with proof only or benchmark only. Kain wants both.

## Validation Loop

1. Benchmark the current hot path or capture the current failing pressure row.
2. Use `mcp__z3_local__` discovery tools such as `find_counterexample`, `optimize`, `bitvec_equiv`, or `prove_or_witness` to search for a candidate.
3. Save the exploratory proof, then rerun the nearest durable proof pack or `check_smt2`.
4. Re-run the owning subsystem benchmark, attrition lane, or conformance lane and report the measured delta.
