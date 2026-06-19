# Clean – Workspace Cleanup

Removes build artifacts, cached outputs, and compiled binaries.

## Banner

```markscript
print("=== CLEAN PIPELINE ===")
print("Project: pi-squared")
print("Targets: cache | out | exe")
```

## StageCleanCache

```markscript
print("--- Stage 1: Clean cache ---")
print("Target: .kain/out/")
```

> file exists

```markscript
print("Cache directory found -- dispatching cleanup")
```

> run

```markscript
print("Cache cleaned")
```

## StageCleanOut

```markscript
print("--- Stage 2: Clean build output ---")
print("Target: Z:/_b/ build artifacts")
```

> run

```markscript
print("Build output cleaned")
```

## StageCleanExe

```markscript
print("--- Stage 3: Clean compiled binaries ---")
print("Target: .exe files under scripts/")
```

> file exists

```markscript
print("Binaries found -- dispatching cleanup")
```

> run

```markscript
print("Binaries cleaned")
```

## Summary

```markscript
print("=== CLEAN PIPELINE COMPLETE ===")
print("3 stages dispatched through MarkScript IVT")
```
