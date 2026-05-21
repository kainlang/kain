---
name: tool-release-readiness
description: Use when adding, changing, running, or debugging the release-readiness gate under `release/` and `scripts/python/release_readiness_gate.py`, including evidence policy, profile selection, and blocker wiring across benchmark, attrition, conformance, and import-rule lanes.
---

# Tool Release Readiness

Use this skill for the repo-level gate that decides whether the current Kain surface is honestly ready, not for fixing one benchmark or one runtime lane in isolation.

## Owns

- `release/readiness_policy.json` as the source of truth for profiles, evidence ids, hooks, and coverage classes.
- `scripts/python/release_readiness_gate.py` and its tests as the runner.
- Wiring between benchmark, attrition, conformance, and import-rule evidence when the gate itself changes.

## Does Not Own

- The implementation of benchmark rows, attrition cases, or runtime conformance lanes themselves. Fix those in their owning skills first.
- Build plumbing or launcher provenance. Use `tool-build-system`.
- Ad hoc hardcoded release logic inside Python when the change is really policy data.

## Working Rules

- Keep policy data-shaped: add ids and profile membership in JSON before adding special cases in Python.
- Treat the matrix as a blocker classifier, not a single megatest.
- When a new blocker class appears, wire the smallest durable proof lane for it and then attach that lane to the policy.
- Keep quick focused on common pre-release truth and full on deeper substrate proof.

## Validation

```powershell
python scripts/python/release_readiness_gate.py --list-profiles
python -m py_compile scripts/python/release_readiness_gate.py scripts/python/test_release_readiness_gate.py
python scripts/python/test_release_readiness_gate.py
python scripts/python/release_readiness_gate.py --profile quick --run
python scripts/python/release_readiness_gate.py --profile full --run
```
