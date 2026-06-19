# Error Check Blade -- Diagnostic Code Coverage Pipeline

## Purpose

Proves that every Kain compiler diagnostic code is:
1. **Triggerable** -- a real .kn source file can cause the error
2. **Correct** -- the error message contains the expected substring
3. **Severity-accurate** -- the error severity matches the spec

## Structure

20 subdirectories, one per error domain. Each contains .kn files named
after the error codes they test. Files use `//@` directives for the
`kain test` harness.

## Running

```bash
# Run all error tests with telemetry JSON output
kain run run_all.kn

# Run a single domain
kain check parse/ --json

# Run via kain test (compiletest-style)
kain test . --json
```

## Directive Reference

| Directive | Meaning |
|-----------|---------|
| `//@ check-fail` | Must fail typechecking |
| `//@ build-fail` | Must pass check but fail build (codegen errors) |
| `//@ error: KAIN-XXXX-NNNN` | Expected diagnostic code |
| `//@ expect-error: "substring"` | Expected message substring |
| `//@ severity: error|warning` | Expected severity level |
| `//@ known-gap: reason` | Cannot currently trigger (documented gap) |

## Error Domain Summary

| Domain | Codes | Directory | Type |
|--------|-------|-----------|------|
| PARSE | 0001-0020 (20) | `parse/` | check-fail |
| TYPE | 0001-0026 (26) | `type/` | check-fail |
| CODEGEN | 0001-0011 (11) | `codegen/` | build-fail |
| SHADER | 0001-0012 (12) | `shader/` | build-fail |
| EFFECT | 0001-0012 (12) | `effect/` | check-fail |
| BORROW | 0001-0010 (10) | `borrow/` | check-fail |
| MEMORY | 0001-0008 (8) | `memory/` | check-fail |
| WORLD | 0001-0008 (8) | `world/` | check-fail |
| ACTOR | 0001-0008 (8) | `actor/` | check-fail |
| RUNTIME | 0001-0008 (8) | `runtime/` | build-fail/known-gap |
| COMPTIME | 0001-0010 (10) | `comptime/` | check-fail |
| STATE | 0001-0008 (8) | `state/` | check-fail |
| CONVERGE | 0001-0008 (8) | `converge/` | check-fail |
| ENTANGLE | 0001-0007 (7) | `entangle/` | check-fail |
| PATCH | 0001-0007 (7) | `patch/` | check-fail |
| VALIDATION | 0001 (1) | `validation/` | check-fail |
| IO | 0001-0006 (6) | `io/` | build-fail/known-gap |
| CONFIG | 0001-0006 (6) | `config/` | check-fail/known-gap |
| TEST | 0001 (1) | `test_code/` | check-fail |
| INTERNAL | 0001 (1) | `internal/` | known-gap |

**Total: ~170 error codes across 20 domains**

## Telemetry Output

`run_all.kn` produces `telemetry/report.json`:

```json
{
  "summary": {
    "total_codes": 170,
    "tested": 165,
    "triggered": 160,
    "message_matched": 158,
    "severity_correct": 165,
    "coverage_percent": 94.1
  },
  "by_domain": { ... },
  "failures": [ ... ],
  "known_gaps": [ ... ]
}
```

## CI Integration

This blade runs as part of the release readiness gate:

```bash
kain test blades/edge_cases/error_check/ --json > telemetry/ci_report.json
python scripts/python/check_coverage.py telemetry/ci_report.json --threshold 90
```
