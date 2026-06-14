# TestScripts

> Verifies all pi-squared markscript scripts are present.

## Metadata

| Script | Path | Expected |
|--------|------|----------|
| build | scripts/build.md | exists |
| dev | scripts/dev.md | exists |
| test | scripts/test.md | exists |
| clean | scripts/clean.md | exists |
| help | scripts/help.md | exists |

## Banner

```markscript
print("=== Test: Script Files ===")
```

## CheckBuild

```markscript
print("--- Checking scripts/build.md ---")
```

> file exists

```markscript
print("build.md verified")
```

## CheckDev

> file exists

## CheckTest

> file exists

## CheckClean

> file exists

## CheckHelp

> file exists

## Summary

```markscript
print("=== All script files verified ===")
```
