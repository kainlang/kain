---
name: lang-feedback
description: Use when an agent has finished authoring, reviewing, debugging, or validating Kain `.kn` files or Kain project/system work and should optionally record fundamental language or toolchain workflow blockers in repo-root `FEEDBACK.md` for future agents. Log only systemic issues in Kain/compiler/runtime/stdlib/tooling surfaces; do not log mistakes in the code the agent just wrote. If no qualifying issue exists, skip writing feedback.
---

# Lang Feedback

Capture durable dogfooding feedback that helps future agents unblock Kain itself.

## Qualifying Issues

Only record issues that are fundamental to Kain or its substrate and that reduced the agent's ability to deliver.

- Parser/AST/typechecker behavior that blocks valid authored intent.
- Lowering/codegen/runtime ABI problems.
- stdlib surface gaps or broken contracts that force bad workarounds.
- Build/check/test/proof/tooling workflow friction caused by repo systems.
- Reproducible diagnostics, crashes, or inconsistent behavior in core Kain flows.

Do not record:

- Bugs introduced in your own patch or authored `.kn` logic.
- Personal preference notes without concrete workflow impact.
- One-off environment noise that is not tied to repo systems.

## Write Rule

If no qualifying issue exists, write nothing.

If qualifying issues exist, append to repo-root `FEEDBACK.md` without deleting prior entries.

When `FEEDBACK.md` is empty, add:

```markdown
# Kain Feedback Log
```

## Entry Format

Use this exact structure so future LLMs can scan quickly:

```markdown
## YYYY-MM-DD - <task or subsystem>
### <Issue Title>
- Categories: <comma-separated list of applicable categories, e.g., regression, enhancement, correctness, performance, developer-experience>
- Status: <Active | Patched | Verified | Bypass-Applied>
- Surface: <parser|typechecker|lowering|runtime|stdlib|tooling|build|interop|gpu|proof>
- Symptom: <what failed>
- Workflow impact: <how this slowed or blocked work>
- Minimal repro: <command/file/input>
- Evidence: <error text, report path, or observed behavior>
- Suggested direction: <small concrete next move>
```

## Reviewing and Updating Status

If you are an agent reviewing feedback for bugs and using this as a list to fix, make sure to update the feedback mentioned with a `patched` or `verified` status (e.g. changing `- Status:` to `Patched` or `Verified`) to indicate it has been resolved.

## Authoring Constraints

- Keep entries short and factual.
- Use flat bullets only; no nested lists.
- Prefer concrete command/file references over abstract commentary.
- Avoid speculation unless labeled as a hypothesis.
- Keep issue titles specific enough for triage, not generic.

## Quality Bar

Before writing an entry, ask:

1. Is this a fundamental repo/system issue rather than my authored code?
2. Can a future agent reproduce this from the note?
3. Will this feedback make a later fix meaningfully faster?

If any answer is no, skip or refine the entry.
