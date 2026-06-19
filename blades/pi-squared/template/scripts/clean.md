# Clean --- Artifact Cleanup

Removes all build artifacts and caches.
Run with: mks run scripts/clean.md

## Banner

```markscript
print("=== CLEAN ===")
print("Project: my-kain-app")
```

## StageClean

```markscript
print("--- Clean artifacts ---")
print("Removing: .kain/out, .kain/cache")
```

> run

```markscript
print("Clean dispatched")
```

## StageVerify

```markscript
print("--- Verify sources intact ---")
```

> file exists

```markscript
print("Source verification dispatched")
```

## Summary

```markscript
print("")
print("=== CLEAN COMPLETE ===")
print("")
print("Removed: .kain/out, .kain/cache")
print("Preserved: src/, scripts/, tests/, config.md, Mksfile.md")
print("")
print("Next build: mks run scripts/build.md")
```

## Pipeline

| Stage | Handler | Purpose |
|-------|---------|---------|
| 1 | run | Remove build artifacts |
| 2 | file exists | Verify source integrity |
