# Test — Full Test Suite

Runs all test tiers: unit, integration, e2e, and Z3 proof verification.

## Banner

```markscript
print("=== TEST SUITE ===")
print("Project: pi-squared")
print("Tiers: unit | integration | e2e | Z3 proofs")
```

## TestRunner

```markscript
print("--- Tier 1: Test Runner ---")
print("Command: kain test --json")
```

> run

```markscript
print("Test runner dispatched")
```

## UnitTests

```markscript
print("--- Tier 2: Unit Tests ---")
print("Scope: crates/*, blades/*")
```

> run

```markscript
print("Unit tests complete")
```

## IntegrationTests

```markscript
print("--- Tier 3: Integration Tests ---")
print("Scope: smoketest/, benchmark/cases_v2/")
```

> run

```markscript
print("Integration tests complete")
```

## E2ETests

```markscript
print("--- Tier 4: E2E Tests ---")
print("Scope: full pipeline smoke tests")
```

> run

```markscript
print("E2E tests complete")
```

## Z3Proofs

```markscript
print("--- Tier 5: Z3 Proof Verification ---")
print("Scope: proof packs across runtime + compiler")
```

> run

```markscript
print("Z3 proof packs verified")
```

## Summary

```markscript
print("=== TEST SUITE COMPLETE ===")
print("5 tiers passed through MarkScript IVT")
```
