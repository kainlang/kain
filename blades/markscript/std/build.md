# Build

Markscript build orchestration — compile, check, run, and test Kain projects.
Dispatches through the IVT to Kain's process and filesystem bridges.

---

## check

Check (typecheck only) a Kain project or file.

> run "kain check path/to/project"

```markscript
# Run kain check on a project
push("kain check path/to/project")
call("run")
# Output is captured on the stack
```

---

## build

Build a Kain project to a native executable.

> run "kain build path/to/project --target llvm"

```markscript
# Compile the project through LLVM
push("kain build path/to/project --target llvm")
call("run")
```

---

## test

Run the test suite for a Kain project.

> run "kain test path/to/tests/"

```markscript
# Execute the project's test suite
push("kain test path/to/tests/")
call("run")
```

---

## bench

Run benchmarks and capture timing output.

> run "kain bench path/to/benchmarks/"

```markscript
# Run benchmarks
push("kain bench path/to/benchmarks/")
call("run")
```

---

## clean

Clean build artifacts from the output directory.

> run "rm -rf path/to/.kain/out"

```markscript
# Remove all build cache and artifacts
push("rm -rf path/to/.kain/out")
call("run")
```

---

## rebuild

Full clean + build cycle.

> run "kain build path/to/project --target llvm"

```markscript
# Clean first, then rebuild
push("rm -rf path/to/.kain/out")
call("run")
push("kain build path/to/project --target llvm")
call("run")
```

---

## watch

Watch a project for changes and rebuild automatically.

> print "Watching for changes..."
> spawn "kain run dev path/to/project"

```markscript
# Start watch mode (uses spawn for long-running process)
push("kain run dev path/to/project")
call("spawn")
```

---

## fmt

Format Kain source files.

> run "kain fmt path/to/src/ --check"

```markscript
# Check formatting (--check = verify only, no write)
push("kain fmt path/to/src/ --check")
call("run")
```

---

## version

Print the Kain toolchain version.

> run "kain --version"

```markscript
# Show compiler version
push("kain --version")
call("run")
```
