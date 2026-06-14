# Dev — Development Workflow

Interactive development loop with watch, compile, test, and iteration.

## Banner

```markscript
print("=== DEV LOOP ===")
print("Project: pi-squared")
print("Mode: interactive development")
```

## StageCheck

```markscript
print("--- Stage 1: Typecheck ---")
print("Command: kain check")
```

> run

```markscript
print("Typecheck passed")
```

## StageBuild

```markscript
print("--- Stage 2: Build ---")
print("Command: kain build --target llvm --debug")
```

> run

```markscript
print("Debug build dispatched")
```

## StageTest

```markscript
print("--- Stage 3: Run tests ---")
print("Command: kain test")
```

> run

```markscript
print("Tests dispatched")
```

## WatchLoop

```markscript
print("--- Stage 4: Watch loop ---")
print("Command: kain dev")
print("Watching for changes...")
```

> run

```markscript
print("Watch loop started -- auto-rebuild on save")
```

## Summary

```markscript
print("=== DEV LOOP ACTIVE ===")
print("4 stages active through MarkScript IVT")
```
