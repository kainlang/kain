---
name: formal-verification
description: Drive debugging, assessment, and correctness work through formal verification, solver-backed counterexample hunting, and durable proof-pack workflows instead of guesswork. Use when assessing a folder, debugging low-level/runtime/native code, validating memory math, proving parser/state-machine/ABI invariants, translating architectural logic or abstract constraints into mathematical claims, or deciding whether a bug is real across C, C++, Rust, Python, and other systems-heavy code.
---

# Formal Verification

Treat proof and counterexample search as the default debugging loop. Start with the strongest claim you can state clearly, shrink it to the smallest sound obligation, then either prove it or obtain a witness that explains the break.

## Default Workflow

1. Scope the claim.
- Name the property in plain English first: `cursor never exceeds input length`, `allocation growth cannot overflow`, `only validated frames can drive reads`.
- Decide whether the real question is arithmetic, bounds, aliasing, state transition, protocol ordering, layout equivalence, or an architectural invariant.

2. Build the mathematical model.
- Translate the code or architecture into variables, constraints, and a claim.
- Prefer structural invariants over example inputs.
- Restate abstract requirements in one of these shapes whenever possible:
  - monotonicity
  - conservation or accounting
  - bounds
  - exclusivity or single-writer
  - reachability or forbidden transition
  - equivalence between two implementations
  - implication: if guard `G` holds, bad state `B` is impossible

3. Use the `z3_local` workflow path first.
- Prefer repo-local permanence through a `z3/` proof pack.
- Reach first for `analyze_source_file`, `suggest_proof_targets`, `extract_source_proof_cases(save=true)`, `save_proof_case_to_pack`, `prove_or_witness`, `run_proof_pack`, and `run_workspace_proofs`.
- Use `list_templates(context_path=<source>, debug=true)` when you need to see which templates or plugins fired, which bindings were extracted, and what proof arguments were rendered.
- Use `counterexample_to_test` only after you already have a useful witness and need a regression artifact.

4. Prefer durable proofs over chat-only wins.
- Save useful claims into `z3/proofs` or `z3/generated`.
- Keep reports under `z3/reports`.
- Record the minimum assumptions that make the proof sound.

5. Turn the result into engineering action.
- If proved, say exactly what was proved and under which assumptions.
- If not proved, surface the smallest counterexample and the code seam it attacks.
- Recommend the patch that kills the witness, then rerun the proof.

## Translate Architecture Into Math

Do not stop at line-level arithmetic. Convert higher-level design rules into claims the solver can check.

Common translations:
- `Only one actor writes this state` -> mutual exclusion or single-writer invariant
- `This parser cannot skip validation` -> forbidden transition in a state machine
- `Two paths produce the same layout or result` -> equivalence proof
- `A handle is valid until close` -> lifetime or reachability constraint
- `A scheduler cannot strand work in this bounded model` -> progress or no-stuck-state claim
- `A frame length is trustworthy only after header validation` -> implication from validated-header predicate to size or bounds claim

If a full-system proof is too large, prove the critical seam:
- a helper function
- a state transition
- a layout formula
- a cast boundary
- a pointer offset
- a loop growth rule

## Escalate Into Plugins Early

When extraction is noisy, repetitive, or language-specific, stop doing manual glue and create or extend a proof plugin.

Escalate quickly when:
- the same bug class appears more than twice
- the language needs AST or compile-aware extraction
- the proof depends on real symbol names, spans, or concrete types
- folder-local patterns would benefit future agents

Preferred escalation path:
- use built-in templates if they already fit
- add pack-local `z3/templates/*.yaml` for folder-specific `match / extract / prove` logic
- add a pack-local plugin under `z3/plugins/<plugin-id>` when templates are not enough
- promote broadly useful plugins into `C:\Dev\polytools\z3-mcp\plugins`
- use `$z3-create-plugins` when the plugin needs a runtime sidecar or reusable matcher bundle

Choose the smallest runtime that solves the problem:
- `template_bundle` for pure data-driven matchers
- `python_script` for lightweight AST or analysis logic
- `rust_binary` for high-signal structural analysis, Rust-heavy repos, or compile-aware source work

## Proof Habits That Pay Off

- Prefer width-aware signed and unsigned modeling for low-level code.
- State assumptions explicitly instead of smuggling them in.
- Keep declarations typed and names meaningful.
- Prove the narrowest dangerous arithmetic first.
- Save counterexamples that would be expensive to rediscover.
- Treat `unsat` as the gold standard and `sat` as a debugging gift.
- Do not reach for unit tests first when the question is fundamentally mathematical.

## Deliverables

Return:
- the property you modeled
- the assumptions
- whether it was proved or broken
- the smallest witness or strongest proof summary
- the exact file or function seam
- the saved proof, report, and plugin artifacts
- the next code or plugin action

If you need to change the `z3-mcp` server itself, pair this skill with `z3-mcp-validation`.
