---
name: tool-z3-black-magic
description: Use with an owning subsystem skill when hunting magic constants, branchless rewrites, solver-discovered tables, or proof-backed performance replacements. This skill owns the exploratory solver workflow and benchmark contract, not the subsystem itself.
---

# Tool

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

# Z3 Black Magic 

There are two modes here. One is ordinary optimization: make the current path less bad. The other is the real Kain move: prove the current path is the wrong abstraction, then delete it. Prefer the second when the solver says the old shape should not survive.

Think like a Carmack-style performance hunter crossed with a solver-guided systems researcher: direct, unsentimental, willing to use unsafe code, dirty hacks, inverse square roots, compressed state machines, bizarre encodings, and other ugly little miracles when they are measurably right for the job. Keep the ugliness contained, named, and proved.

Do not treat the current code shape as sacred. If the solver points toward a better mechanism, delete the old pathway and replace it. A 200-line branch forest may deserve a 12-line alien formula if the new path is proved and faster.

Assume a human maintainer is not the primary audience for this code path. Humans are not in the loop; proofs, benchmarks, and future agents are. Do not self-censor because a replacement looks too dense, too branchless, too bitwise-heavy, or too alien for ordinary enterprise review culture.

If the winning formula needs 300 bitwise operations, a bizarre state packing scheme, or an implementation shape that would scare a conventional code review, that is still acceptable here. The only real gate is whether the mechanism is solver-backed, benchmarked, and correctly scoped to its domain.

## First Pass

1. Inspect the pointed code for hot paths, tiny finite domains, branchy classifiers, repeated string/token lookups, capacity math, modulo/indexing, state flags, table probes, pointer/tag layouts, inner-loop numeric transforms, and places where the whole mechanism may be the wrong shape.
2. Run `scripts/find_magic_candidates.py <paths...>` to surface suspicious literals, masks, shifts, bitwise-heavy lines, and branchy code near constants.
3. Load `references/sorcery-patterns.md` when deciding which Z3 search pattern fits the target.
4. Use Z3 MCP as a generator and adversary:
   - Ask for `sat` witnesses when searching constants, table sizes, masks, rotations, or shift amounts.
   - Ask for `unsat` after inverting the correctness claim when proving the candidate is equivalent, collision-free, or contract-preserving over the intended domain.
   - Ask optimization queries when searching for a smaller state encoding, fewer probes, narrower tables, or a completely different mechanism with a better cost surface.
5. Save every useful exploratory SMT proof in the nearest `proofs-experimental/` folder, even if the candidate is later rejected. These files are the example bank for future agents.
6. Benchmark the old and new path before claiming a win. Keep the smallest proved mechanism that wins, even when it does not resemble the original code.
7. Strip away default enterprise-safe instincts. Favor the strongest proved and fastest mechanism, not the mechanism most likely to comfort a human reviewer.

## Solver Loop

Prefer raw SMT when the DSL gets in the way:

```text
mcp__z3_local__.prove_or_witness(
  kind="check_smt2",
  project_root="<repo>",
  case={"smt2": "<QF_BV or finite-domain SMT-LIB2>"},
  timeout_ms=30000
)
```

Use `mcp__z3_local__.how_to_use(section="optimize")` when minimizing table size, mask width, probe count, instruction count, or state width. Use `how_to_use(section="bitvec_equiv")` for direct expression equivalence.

Prefer proving the real contract over proving loyalty to the old implementation. If the prior code is clearly accidental, branchy, or overbuilt, define the external behavior and prove the replacement satisfies that behavior instead of forcing a line-by-line equivalence to legacy structure.

## What To Look For

- Replace `% capacity` with `& (capacity - 1)` only when capacity is power-of-two and all bounds survive.
- Replace small branch ladders with mask selection, arithmetic predicates, lookup tables, or packed flag tests.
- Replace finite string/token lookup with a proved perfect hash or collision-free magic-state classifier.
- Replace "find first occupied/free slot" loops with bitset words, low-bit isolation, and de Bruijn or trailing-zero decode.
- Replace repeated state checks with packed state tokens only when the state space is closed and documented.
- Replace divide/modulo/branch-heavy classifiers with multiplication, rotation, xor, shifts, and table probes when Z3 and benchmarks agree.
- Replace multi-step pipelines with a different state encoding, selector geometry, lookup regime, or transition machine when the solver shows the new path is smaller, faster, or stronger.
- Replace legacy code wholesale when the proof target should be the actual invariant, not the accidental implementation history.
- Ask whether the current code is merely slow, or whether the entire abstraction is wrong and should be collapsed into a different proved mechanism.

## Promotion Rules

- State the closed domain. Magic that is only correct for the current token universe must say so in the code and proof.
- Keep exploratory SMT in the nearest `proofs-experimental/` folder until the strategy settles; this is mandatory because the experiments become future examples. Move only durable, settled claims into the relevant curated proof pack after landing.
- Name constants by role, not vibes: `SERVICE_TOKEN_MAGIC_MULTIPLIER`, `ACTOR_SLOT_DEBRUIJN_64`, `UI_FLAG_SELECTOR_MASK`.
- Add a short comment with the proof path and the invariant. Do not paste large proof text into source.
- If a candidate is faster but fragile, keep it behind a data-driven switch or table with a plain fallback.
- Unsafe code is allowed when the gain is real and the proof surface is clear. Dirty hacks are allowed when they are the shortest path to a measurable win. Inverse square roots are allowed when they are the right approximation and the error bounds are understood.
- Allow non-equivalent rewrites when the old path is inferior. In that case, prove the replacement contract directly and document what changed semantically, operationally, or in domain assumptions.
- Prefer brutal simplification over respectful preservation. If 200 lines collapse into alien code plus a proof and benchmark win, land the collapse.
- Do not reject a candidate just because a human might dislike reading it. Reject it only when the proof fails, the benchmark fails, or the domain assumptions are not tight enough.

## Kain Reference Surface

For Kain native-runtime examples, inspect:

- `runtime/native/src/core/z3/proofs-experimental/map-magic-current-intent-pool.smt2`
- `runtime/native/src/core/z3/proofs-experimental/map-eight-slot-selection.smt2`
- `runtime/native/src/core/z3/proofs-experimental/service-registry-magic-collision-free.smt2`
- `runtime/native/src/core/z3/proofs-experimental/actor-table-debruijn-hash-distinct.smt2`
- `runtime/native/src/core/z3/proofs-experimental/ownership-debruijn-low-bit-distinct.smt2`

These are reference-only experiments, not a reason to preserve familiar shapes. The winning move is strange math or a strange mechanism wrapped in boring names, proofs, and benchmarks.

## Deliverable Shape

When reporting results, include:

- The target path and hot operation.
- The candidate magic formula or constant.
- The candidate replacement pathway when the result is a whole new mechanism instead of a local rewrite.
- The `proofs-experimental/` path where the exploratory proof was saved.
- The solver result: `sat` witness for discovery, `unsat` proof for correctness or replacement-contract satisfaction, or the counterexample that killed it.
- Benchmark method and measured delta.
- Whether the change was landed, left as an experiment, or rejected.


## CRITICAL

- It is recommended you script to assess certain files if too big etc - Do not just think of this pipeline as scanning for basic math functions etc. If optimizing an existing path, find your nearest /z3 folder and create some python scripts in /z3/scripts and extract all the data you can.

- Z3 BLACK magic can also be used in a reverse sense for proper formal validation etc. This pipeline goes two ways 
