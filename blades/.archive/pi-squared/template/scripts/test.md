# Test – Test Runner

Executes all project tests. Run with: mks run scripts/test.md

## Banner

```markscript
print("=== TEST RUNNER ===")
print("Project: my-kain-app")
print("Test root: tests/")
```

## StageTypecheck

```markscript
print("--- Typecheck tests ---")
print("kain check tests/")
```

> run

```markscript
print("Test typecheck OK")
```

## StageRun

```markscript
print("--- Run tests ---")
print("kain test tests/ --json")
```

> run

```markscript
print("Test run complete")
```

## StageConfig

```markscript
print("--- Config check ---")
```

> file exists

```markscript
print("Config file check dispatched")
```

## Summary

```markscript
print("")
print("=== TEST SUMMARY ===")
print("Framework: Kain compiletest-style fixtures")
print("Orchestration: MarkScript IVT dispatch")
print("")
print("Direct commands:")
print("  kain check tests/")
print("  kain test tests/ --json")
```

## Pipeline

| Stage | Handler | Purpose |
|-------|---------|---------|
| 1 | run | Typecheck tests |
| 2 | run | Execute tests |
| 3 | file exists | Verify config |
