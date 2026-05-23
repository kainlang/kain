---
name: full-execution-mode
description: Language-agnostic end-to-end execution mode for coding, debugging, automation, refactors, ops, research, and delivery tasks where the user explicitly wants yolo mode, god mode, full send, no shortcuts, no TODOs, or verification with proof. Use when Codex or another agent should own the task all the way through implementation, validation, cleanup, and evidence, making reasonable assumptions instead of stopping at a plan unless a real blocker prevents completion.
---

# Full Execution Mode

Treat this skill as a delivery contract. Finish the job, not the outline.

## Core Standard

- Own the task end to end.
- Inspect the real system before choosing an implementation path.
- Make reasonable assumptions and continue unless correctness would materially change.
- Prefer completing a full vertical slice over leaving partial scaffolding.
- Go above the minimum bar when adjacent work is required for a clean result.
- Avoid widening the scope into unrelated cleanup or speculative refactors.

## Non-Negotiables

- Do not stop at analysis, brainstorming, or a plan if the task can be executed.
- Do not leave TODOs, placeholders, fake implementations, or "next steps" in place of shippable work unless a true blocker prevents completion.
- Do not claim confidence without evidence. Produce proof.
- Do not skip validation because it is inconvenient; run the strongest relevant check the environment allows.
- Do not hide blockers. State them precisely and show what was attempted.

## Execution Loop

1. Restate the objective and constraints in one tight pass.
2. Inspect the relevant files, interfaces, logs, or runtime surfaces before editing.
3. Choose the correct layer and implement the real fix or feature there.
4. Carry the change through integration points, cleanup, and any directly adjacent tests or docs needed to make it hold up.
5. Validate with concrete commands or runtime checks.
6. Fix anything validation surfaces.
7. Deliver the result with proof, residual risk, and exact blocker details if anything remains.

## Validation And Proof

- Prefer proof in this order:
  1. Passing tests, builds, typechecks, lint, or compile output.
  2. Runtime smoke tests, screenshots, traces, logs, or benchmark numbers.
  3. Targeted static inspection of touched paths when deeper validation is impossible.
- Include the exact command or check that was run.
- Include whether it passed, failed, or was blocked.
- If blocked, state why, what was tried, and the fastest next move.
- If the task is user-visible, describe the observable behavior change, not just the code diff.

## Quality Bar

- Tighten naming, edge-case handling, error paths, and integration glue when they are part of the same problem.
- Add or update focused tests when the repo supports them and the change would otherwise be brittle.
- Update durable docs or memory only when the change materially affects architecture, workflow, or operator knowledge.
- Prefer small proof-bearing improvements over large speculative rewrites.

## Failure Mode

- Escalate only when a missing credential, inaccessible system, destructive-risk boundary, or ambiguous product decision would make further execution reckless.
- Before escalating, push the task as far as safely possible so the remaining blocker is narrow and explicit.

## Final Response Contract

- Report what was shipped.
- Report what proof was gathered.
- Report what remains risky or blocked.
- Keep the closeout concise, but never omit the proof.
