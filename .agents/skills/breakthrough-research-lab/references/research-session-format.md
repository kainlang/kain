# Research Session Format

Use this reference when opening or maintaining the repo-local `research/` artifact for a long-running session.

## Note Skeleton

Use this structure unless the repo already has a better local convention:

```markdown
# <session title>

- Date: YYYY-MM-DD
- Status: active | paused | concluded
- Repo Root: <absolute path>
- Session Slug: <short-hyphen-name>

## Research Question

<one-sentence frontier question>

## Constraints

- latency / throughput / memory / energy / platform / safety / implementation freedom

## Hypothesis Lattice

### Baseline
- Mechanism:
- Expected upside:
- Likely blocker:
- Proof obligation:

### Unconventional
- Mechanism:
- Expected upside:
- Likely blocker:
- Proof obligation:

### Moonshot
- Mechanism:
- Expected upside:
- Likely blocker:
- Proof obligation:

## Mathematical Model

- Variables:
- Invariants:
- Objective:
- Bad states:
- Simplifying assumptions:

## Z3 Claims

1. ...
2. ...

## Evidence And Sources

- Local:
- External:

## Dead Ends

- Record broken ideas so future agents do not rediscover them blindly.

## Conclusion

Pending.
```

## Response Rhythm

- After each major turn, update the note with the newest hypothesis, equation, proof result, or blocker.
- Keep `proved`, `plausible`, and `speculative` clearly separated.
- Preserve failures. Dead ends are research assets.
- If external research matters, capture the source link and the date it was checked.

## Closure Standard

When the user says the session is done, the research note should end with:

- the strongest surviving thesis
- the strongest failed thesis
- the minimum assumptions behind the remaining claims
- the best next experiment, proof, or benchmark
- unresolved risks that could invalidate the direction later
