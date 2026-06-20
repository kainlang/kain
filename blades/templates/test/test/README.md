# Test Structure

This directory contains tests that import from `src/` via the one-way bridge
set up in `build.kn` (`module_roots: ["src"]`).

```
test/
├── fixtures/
│   └── bridge_test.kn      ← Minimal: proves the import bridge works
├── unit/
│   └── test_core.kn        ← Full unit tests for all public functions
└── README.md               ← This file
```

## How the Bridge Works

```
src/  ←──  test/   (one-way: tests import from src, never reverse)

build.kn sets module_root("src") → makes src/ importable project-wide
```

In `test/fixtures/bridge_test.kn`:
```kain
use math_utils    // ← resolves to src/math_utils.kn via build.kn
```

The bridge is **one-way**: source code never imports from test/. Tests are
consumers, not dependencies.

## Running Tests

```bash
# From the project root (blades/templates/test/)

# 1. Quick bridge verification
kain run --project . test/fixtures/bridge_test.kn --target llvm

# 2. Full unit test suite
kain run --project . test/unit/test_core.kn --target llvm

# 3. Build the project (tests are wired into build.kn)
kain build src/main.kn --target llvm

# 4. Run the project
kain run src/main.kn --target llvm
```

## Adding New Tests

1. Create a new file under `test/unit/` or `test/fixtures/`
2. Add `use your_module` to import from `src/`
3. Use `std::test` for structured assertions (`test_bool`, `test_combine`, `test_count_failure`)
4. Wire `main()` to return failure count (0 = pass)
5. Run with `kain run --project . test/unit/your_test.kn --target llvm`

## Test Patterns

```kain
use std::test
use std::runtime
use your_module       // ← imports real source from src/

fn test_something() -> TestOutcome:
    let result = your_module.your_function(args)
    return test_bool("label: expected behavior", result == expected)

fn main() -> Int:
    runtime_init()
    let r = test_something()
    let failures = test_count_failure(r, 0)
    runtime_shutdown()
    return failures
```

## anti-Patterns

- Re-implementing source functions in tests (use `use` instead)
- Running only `kain check` and declaring victory (must `kain run`)
- Source code importing from test/ (bridge is one-way)
- Skipping `test_combine` (failures won't propagate)
- Ignoring exit code (must be 0 for pass)
