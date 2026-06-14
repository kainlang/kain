# Dev — Interactive Development Loop

Hot-reload dev pipeline. Run with: mks run scripts/dev.md
For live reload: mks watch scripts/dev.md

## Banner

```markscript
print("=== DEV LOOP ===")
print("Project: my-kain-app")
print("Mode: interactive")
```

## StageCheck

```markscript
print("--- Stage 1: Typecheck ---")
print("kain check src/")
```

> run

```markscript
print("Check OK")
```

## StageBuild

```markscript
print("--- Stage 2: Rebuild ---")
print("kain build src/ --target llvm")
```

> run

```markscript
print("Build OK")
```

## StageRun

```markscript
print("--- Stage 3: Launch ---")
print("kain run src/main.kn --target llvm")
```

> run

```markscript
print("App closed")
```

## Hints

```markscript
print("")
print("=== DEV QUICK REFERENCE ===")
print("")
print("Commands:")
print("  mks run scripts/dev.md       -- Full dev loop")
print("  mks run scripts/build.md     -- Build pipeline")
print("  mks run scripts/test.md      -- Run tests")
print("  mks run scripts/clean.md     -- Clean artifacts")
print("  mks run scripts/help.md      -- All commands")
print("  mks watch scripts/dev.md     -- Live reload")
print("")
print("Direct Kain CLI:")
print("  kain check src/              -- Typecheck only")
print("  kain build src/ --target llvm -- Compile")
print("  kain run src/main.kn         -- Run directly")
print("  kain test tests/ --json      -- Run tests")
```

## Pipeline

| Stage | Handler | Command |
|-------|---------|---------|
| 1 | run | kain check src/ |
| 2 | run | kain build src/ --target llvm |
| 3 | run | kain run src/main.kn --target llvm |
