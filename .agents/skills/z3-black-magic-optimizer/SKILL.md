---
name: z3-black-magic-optimizer
description: "Use when Codex is asked to find magic numbers, alien math, black-magic performance tricks, branchless replacements, perfect hashes, bit masks, de Bruijn decoders, selector tables, token classifiers, unsafe performance hacks, inverse-square-root style tricks, solver-guided speedups, or entirely new proof-backed computational pathways in C, C++, Rust, Kain, TypeScript, Go, Python, shaders, or other code. Use when the task is exploratory optimization, computational invention, or solver-guided redesign rather than ordinary formal validation: search for strange constants, compact formulas, alternate encodings, radically smaller mechanisms, or replacement algorithms with Z3 MCP, then prove equivalence, prove the stronger replacement contract, and benchmark before landing code."
---

# Z3 Black Magic Optimizer

Use this skill to hunt for high-leverage constants, branchless formulas, and entirely different computational routes. Make Z3 search the weird mathematical space first, then promote only candidates that have a proof, a clear benchmark win, or a stronger proved contract than the code they replace.

Think like a Carmack-style performance hunter crossed with a solver-guided systems researcher: direct, unsentimental, willing to use unsafe code, dirty hacks, inverse square roots, compressed state machines, bizarre encodings, and other ugly little miracles when they are measurably right for the job. Keep the ugliness contained, named, and proved.

Do not treat the current code shape as sacred. If the solver points toward a better mechanism, delete the old pathway and replace it. A 200-line branch forest may deserve a 12-line alien formula if the new path is proved and faster.

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

## Promotion Rules

- State the closed domain. Magic that is only correct for the current token universe must say so in the code and proof.
- Keep exploratory SMT in the nearest `proofs-experimental/` folder until the strategy settles; this is mandatory because the experiments become future examples. Move only durable, settled claims into the relevant curated proof pack after landing.
- Name constants by role, not vibes: `SERVICE_TOKEN_MAGIC_MULTIPLIER`, `ACTOR_SLOT_DEBRUIJN_64`, `UI_FLAG_SELECTOR_MASK`.
- Add a short comment with the proof path and the invariant. Do not paste large proof text into source.
- If a candidate is faster but fragile, keep it behind a data-driven switch or table with a plain fallback.
- Unsafe code is allowed when the gain is real and the proof surface is clear. Dirty hacks are allowed when they are the shortest path to a measurable win. Inverse square roots are allowed when they are the right approximation and the error bounds are understood.
- Allow non-equivalent rewrites when the old path is inferior. In that case, prove the replacement contract directly and document what changed semantically, operationally, or in domain assumptions.
- Prefer brutal simplification over respectful preservation. If 200 lines collapse into alien code plus a proof and benchmark win, land the collapse.

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
