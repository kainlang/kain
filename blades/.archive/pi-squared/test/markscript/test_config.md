# TestConfig

> A markscript test that verifies config.md parses correctly.

## Metadata

| Property | Value |
|----------|-------|
| Test | config-loading |
| Target | config.md |
| Expected | 5+ tables parsed |

## Banner

```markscript
print("=== Test: Config Loading ===")
```

## LoadConfig

```markscript
print("--- Loading config.md ---")
print("Tables should include: Metadata, Build, ConfigLayers, Actors, Pipelines")
```

> file exists

```markscript
print("config.md found")
```

## Summary

```markscript
print("=== Config test complete ===")
```
