# TestBuild

> A markscript test that verifies the pi-squared build pipeline.

## Metadata

| Property | Value |
|----------|-------|
| Test | build-pipeline |
| Target | pi-squared |
| Expected | build succeeds |

## Banner

```markscript
print("=== Test: Build Pipeline ===")
print("Project: pi-squared")
```

## StageCheck

```markscript
print("--- Stage 1: Typecheck ---")
```

> run

```markscript
print("Typecheck dispatched")
```

## StageBuild

```markscript
print("--- Stage 2: Build ---")
```

> run

```markscript
print("Build dispatched")
```

## Summary

```markscript
print("=== Build test complete ===")
```
