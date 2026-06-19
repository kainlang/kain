# Build -- Full Build Pipeline

Orchestrates the complete Kain compilation pipeline through MarkScript.
Each heading is a domain (stage). Run with: mks run scripts/build.md

## Banner

```markscript
print("=== BUILD PIPELINE ===")
print("Project: my-kain-app")
print("Orchestrator: MarkScript")
```

## StageCheck

```markscript
print("--- Stage 1: Typecheck ---")
print("Command: kain check src/")
```

> run

```markscript
print("Typecheck dispatched (handler 4: FN_PROCESS_OUTPUT)")
```

## StageBuild

```markscript
print("--- Stage 2: Build ---")
print("Command: kain build src/ --target llvm")
```

> run

```markscript
print("Build dispatched (handler 4: FN_PROCESS_OUTPUT)")
```

## StageVerify

```markscript
print("--- Stage 3: Verify ---")
print("Checking for output artifacts...")
```

> file exists

```markscript
print("Verify dispatched (handler 3: FN_FS_EXISTS)")
```

## StageRun

```markscript
print("--- Stage 4: Run ---")
print("Command: kain run src/main.kn --target llvm")
```

> run

```markscript
print("Run dispatched (handler 4: FN_PROCESS_OUTPUT)")
```

## Summary

```markscript
print("=== BUILD PIPELINE COMPLETE ===")
print("4 stages dispatched through MarkScript IVT")
print("")
print("Note: Command strings are hashed by the current binary.")
print("Run kain commands directly:")
print("  kain check src/")
print("  kain build src/ --target llvm")
print("  kain run src/main.kn --target llvm")
```

## Pipeline

| Stage | Handler | IVT Phrase | Kain Equivalent |
|-------|---------|------------|-----------------|
| 1 | FN_PROCESS_OUTPUT | run | kain check src/ |
| 2 | FN_PROCESS_OUTPUT | run | kain build src/ --target llvm |
| 3 | FN_FS_EXISTS | file exists | dir .kain\out |
| 4 | FN_PROCESS_OUTPUT | run | kain run src/main.kn --target llvm |
