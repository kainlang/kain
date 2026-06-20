# Kain Test Pipeline Template

## Purpose

This template shows the **canonical pattern** for testing a Kain project.
It demonstrates:

1. **One-way testing bridge** — test files in `test/` import modules from `src/`
   via `build.kn`'s `module_roots`. Source code never knows about tests.
2. **Bootstrapped tests** — tests use `use math_utils` (real source) instead of
   re-implementing logic. No mocking, no copy-paste.
3. **build.kn wiring** — `source_tests()` and `.requires()` chain tests into the
   build graph.
4. **Runtime execution** — tests run via `kain run --project .`, not just
   `kain check`. Exit code 0 = pass.

## Quick Start

```bash
# From the project root (blades/templates/test/)

# 1. Verify the bridge works
kain run --project . test/fixtures/bridge_test.kn --target llvm
echo "Exit code: $?"   # Must be 0

# 2. Run unit tests
kain run --project . test/unit/test_core.kn --target llvm
echo "Exit code: $?"   # Must be 0

# 3. Build the project (tests are wired into build.kn)
kain build src/main.kn --target llvm

# 4. Run the project
kain run src/main.kn --target llvm
echo "Exit code: $?"   # Must be 0
```

## Architecture

```
src/  ←──  test/   (one-way bridge via build.kn module_roots)

- src/math_utils.kn — real library code, no test awareness
- src/greeter.kn — another real module
- src/main.kn — entry point
- test/unit/test_core.kn — imports from src/, runs real functions
- test/fixtures/bridge_test.kn — minimal bridge verification
- build.kn — sets module_roots=["src"], wires source_tests()
```

## Key Commands

| Command | What It Proves |
|---------|---------------|
| `kain check src/main.kn` | Project typechecks |
| `kain run --project . test/fixtures/bridge_test.kn --target llvm` | Bridge works: tests can import from src/ |
| `kain run --project . test/unit/test_core.kn --target llvm` | Unit tests pass at runtime |
| `kain build src/main.kn --target llvm` | Project compiles to native |
| `kain run src/main.kn --target llvm` | Program runs and exits 0 |

## Adding Tests to Your Project

1. Copy `build.kn` and `test/` to your project
2. Update `module_roots` in build.kn to point to your src/
3. Write test files that `use your_module` to import real code
4. Run with `kain run --project . test/unit/your_test.kn --target llvm`
5. Wire into build.kn with `source_tests()` and `.requires()`

## Design

### Layer 0 — Plain Code

This template stays on Layer 0 of the decision ladder (`fn`, `struct`, `let`,
`if`/`else`, `while`, `return`). Testing infrastructure is pure logic — no
shared state, concurrency, or timing.

For projects that need state, actors, or pipelines, climb the ladder:
- **State authority**: add a `world` for global config
- **Concurrent state**: add an `actor` for background workers
- **Timed recurrence**: add a `pulse` for periodic tasks

### Pure Effects

All library functions are `Pure` — no side effects. Tests validate inputs and
outputs directly. This makes tests deterministic and fast.

### build.kn — The Bridge

The critical line is:
```kain
.module_root("src")
```

This makes `src/` importable from ANY file in the project (including test/).
Without it, test files cannot `use math_utils`.

## Anti-Patterns

- Re-implementing source functions in tests (use `use` instead)
- Running `kain check` and declaring victory (must `kain run`)
- Source code importing from test/ (bridge is one-way)
- Skipping `test_combine` (failures won't propagate)
- Ignoring exit code (must be 0 for pass)

## Requirements

- **Kain toolchain** (`kain check`, `kain build`, `kain run`)
- **Native runtime** — auto-linked during `kain build`/`kain run`
- **Windows, Linux, or WSL** — targets x86_64

## License

This template is part of the Kain language project. Use freely as a starting
point for your own test suites.
