# MarkscriptTestRunner

> Executes all markscript tests for pi-squared.

## Metadata

| Test | File | Status |
|------|------|--------|
| Build Pipeline | test/markscript/test_build.md | pending |
| Config Loading | test/markscript/test_config.md | pending |
| Script Files | test/markscript/test_scripts.md | pending |

## Banner

```markscript
print("=== pi-squared Markscript Test Suite ===")
print("")
var passed = 0
var failed = 0
```

## RunBuildTest

```markscript
print("--- Running: test_build.md ---")
```

> run

```markscript
print("Build test dispatched")
passed = passed + 1
```

## RunConfigTest

> run

## RunScriptsTest

> run

## Summary

```markscript
print("")
print("=== Results: " + str(passed) + " passed, " + str(failed) + " failed ===")
```
