---
name: tool-z3-bug-hunter
description: 'Use with an owning subsystem skill when auditing Kain compiler, runtime, stdlib, tooling, or authored-system surfaces for real bugs, weird edge cases, invariant breaks, miscompiles, crashes, race windows, ABI/layout mistakes, or other solver-checkable failures. This is the exploratory sibling to `tool-z3-black-magic`: hunt and log defects into `D:\Kain-Lang\BUGS.md`, especially with Z3-backed evidence, but do not fix them here.'
---

# Tool

Never use this skill alone. Pair it with the owning `bootstrap-*`, `runtime-*`, `lang-*`, `test-*`, or `package-*` skill so the bug hunt stays attached to the right subsystem.

## Use This For

- Exploratory bug hunts where the job is to find and log breakage, not patch it.
- Weird edge cases: overflow, underflow, stale state, aliasing, sign-extension, packed-layout overlap, branchless misclassification, race windows, ownership/lifetime holes, ABI mismatches, parser/typechecker corners, and "this should not happen" diagnostics.
- Z3-backed defect discovery where a `sat` counterexample, failed invariant, non-equivalence witness, or bounded-state violation can prove the bug is real.
- Audits of low-level paths that are too unsafe, branchless, concurrent, or cross-layer to trust by intuition alone.

## Rules

- Treat Z3 as the first adversary. If the surface is arithmetic, bounds, bitvectors, layouts, state machines, or equivalence, model it before trusting tests.
- This is a logging pipeline, not a repair lane. Confirm, minimize, and record the bug; do not silently fix it as part of this skill.
- Prefer concrete evidence over vibes: solver witness, crash text, failing command, minimized input, proof report, or deterministic observed misbehavior.
- Pair the hunt with the owning subsystem validation loop, but do not let passing tests overrule a real counterexample.
- **CRITICAL / MANDATORY**: Every logged bug entry MUST be mathematically verified and accompanied by a dedicated Z3 proof file (YAML/SMT2). You MUST save this proof under the nearest subsystem's `z3/proofs/` directory and explicitly document its absolute `file:///` path under the `- Z3 Proof:` field of the bug entry. Bug entries without a verified Z3 proof path are strictly invalid.

## Hunt Loop

1. Search `ARCHITECTURE.md` and `MEMORY.md` for prior failures, proof paths, and subsystem notes before assuming the bug is novel.
2. Inspect the pointed code for closed domains, masks, bounds math, pointer offsets, packed fields, state transitions, concurrency handoffs, lowering mismatches, and other finite surfaces where the machine can search harder than a human.
3. Use the right `mcp__z3_local__` tool for the failure shape.
4. Minimize the witness until a future agent can rerun it without reconstructing your whole session.
5. Append only real, reproducible bugs to `D:\Kain-Lang\BUGS.md`. Each entry MUST include the absolute `file:///` path to the supporting Z3 proof file under the required `- Z3 Proof:` line.
6. If the solver cannot model the issue and you lack strong runtime evidence, log nothing.

## Z3 Lens

- Ask for `sat` when trying to surface the one bad input, flag mix, packet shape, or transition that breaks the claim.
- Ask for `unsat` after negating the intended invariant when trying to prove the break is impossible. If you get `sat`, you found the bug or disproved the assumption.
- Prefer proving the real contract over proving loyalty to the current implementation.
- Prefer `find_counterexample`, `prove_or_witness`, or raw `check_smt2` for general claims.
- Prefer `ptr_offset_ok`, `buffer_growth_ok`, `size_add_ok`, `size_mul_ok`, `content_length_ok`, and `range_check` for bounds and size math.
- Prefer `bitvec_equiv` for branchless fast-lane or lowering-equivalence claims.
- Prefer `state_machine_check` for actor/runtime/protocol transition bugs.
- Prefer `suggest_proof_targets` or `extract_source_proof_cases` when mining suspicious source automatically.

## Write Rule

If no qualifying bug exists, write nothing.

If qualifying bugs exist, append to `D:\Kain-Lang\BUGS.md` without deleting prior entries.

When `BUGS.md` is empty, add:

```markdown
# Kain Bug Log
```

## Entry Format

Use this exact structure so future LLMs can scan and triage quickly:

```markdown
## YYYY-MM-DD - <task or subsystem>
### <Bug Title>
- Categories: <comma-separated list of applicable categories, e.g., correctness, soundness, crash, race, miscompile, UB, bounds, parser, tooling, performance, developer-experience>
- Severity: <Critical | High | Medium | Low>
- Status: <Active | Reproduced | Solver-Proved | Minimized | Patched | Verified>
- Surface: <parser|typechecker|lowering|runtime|stdlib|tooling|build|interop|gpu|proof|actor|ownership|fs|net|ui>
- Trigger: <edge case, input shape, or state boundary that wakes the bug up>
- Symptom: <what failed>
- Why this is a bug: <counterexample, invariant break, miscompile, crash signature, or observed contradiction>
- Minimal repro: <command/file/input/seed>
- Evidence: <error text, report path, solver result, or crash artifact>
- Z3 angle: <what was proved, disproved, or still needs modeling>
- Z3 Proof: [proof-name](file:///absolute/path/to/proof.yaml)  <-- MANDATORY: The absolute file:/// path to the verified Z3 YAML/SMT2 proof file.
- Suggested follow-up: <small concrete next move for a future fixing agent>
```

## Reviewing and Updating Status

If you are a later agent using this log to fix bugs, update the existing entry status to `Patched` or `Verified` and keep the original evidence intact.

## Do Not Record

- Bugs introduced by your own patch in the current turn.
- Pure hunches with no witness, no repro, and no observable contradiction.
- Feature requests or optimization ideas unless they currently cause incorrectness, crash behavior, violated invariants, or other real breakage.
- Duplicate entries that add no new repro, minimization, or evidence.

## Quality Bar

Before writing an entry, ask:

1. Is this a real repo/system bug rather than a mistake in the code I just wrote?
2. Can a future agent reproduce or resume the hunt from the entry alone?
3. Did I either produce solver evidence or explain why the bug is still concrete without it?
4. Did I resist fixing the bug in this pipeline and keep the deliverable focused on logging?

If any answer is no, skip or refine the entry.
