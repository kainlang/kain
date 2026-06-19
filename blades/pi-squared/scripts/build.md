# Build 〰 Full Build Pipeline

Orchestrates the complete Kain compilation pipeline through MarkScript.

## Banner

```markscript
print("=== BUILD PIPELINE ===")
print("Project: pi-squared")
print("Orchestrator: MarkScript")
```

## StageCheck

```markscript
print("--- Stage 1: Typecheck ---")
print("Command: kain check")
```

> run

```markscript
print("Typecheck dispatched")
```

## StageBuild

```markscript
print("--- Stage 2: Build ---")
print("Command: kain build --target llvm")
```

> run

```markscript
print("Build dispatched")
```

## StageVerify

```markscript
print("--- Stage 3: Verify artifacts ---")
```

> file exists

```markscript
print("Verify dispatched")
```

## Summary

```markscript
print("=== BUILD PIPELINE COMPLETE ===")
print("3 stages dispatched through MarkScript IVT")
```
