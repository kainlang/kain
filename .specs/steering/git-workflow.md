# Git Workflow

## Branching

- Default to working on `master` in this repository unless the user explicitly
  asks for a branch.
- Keep changes scoped to one coherent objective where possible, even when work
  lands directly on `master`.
- For risky work, isolate the risk with spec packages, labs, feature flags, or
  disposable generated outputs instead of relying on long-lived branches.

## Commits

- Prefer clear, reviewable commits with accurate intent.
- Keep commit messages precise enough that another agent can understand the change history.
- Spec-driven work should mention the spec slug or initiative name in the
  commit message when it materially helps traceability.

## Review

- Validate before requesting review.
- Call out risks, assumptions, migrations, and rollback steps when they exist.
- Link the relevant `.specs/<slug>/` package in review context when the change is spec-driven.
- When reference parity is the goal, include the exact reference surface used as
  the acceptance oracle.

## Merge and Release

- Push after each coherent validated change set.
- Use feature flags, staged rollout, labs proofs, or rollback steps when the
  change warrants it.
- Preserve the spec package until rollout confidence is high and follow-up work
  is closed.
