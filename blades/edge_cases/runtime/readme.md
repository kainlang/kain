# Debug Template — Rapid Kain Edge-Case Testing

A self-contained, instantly copy-pasteable template for rapidly creating and testing Kain edge cases. Designed for maximum speed: copy the folder, write code in `cause.kn`, run `kain run`, get precision diagnostics.

## Quick Start

```powershell
# Clone the template (fastest way — no manual copying)
cd X:\blades\templates\debug
kain run spawn.kn --target llvm -- --name my-bug

# Or manually copy
cp -r X:\blades\templates\debug .\my-debug-session

# Run full diagnostics (typecheck + execute all tests)
cd .\my-debug-session
kain run

# Run only cause.kn tests
kain run -- --test cause

# Run inside an isolated process (deterministic capture)
kain run -- --vm

# List all available tests
kain run -- --list --verbose
```

## File Taxonomy

```
debug/
├── build.kn           Build authority + project config
├── readme.md          This file
├── spawn.kn           Self-replicating cloner script (--name, --output, --source)
└── src/
    ├── main.kn         CLI entry point — parses flags, dispatches to diagnostics or VM
    ├── diagnostics.kn  Orchestrator — imports all modules, runs tests, prints reports
    ├── cause.kn        PRIMARY — most agents write code here
    ├── effect.kn       Downstream effect modeling
    ├── spookymagic.kn  Black-box / spooky-magic behaviors
    └── vm.kn           Isolated process execution wrapper (--vm flag)
```

## File Interaction Diagram

```
main.kn  (CLI flags → dispatch)
  ├── use diagnostics   (imports symbols: run_diagnostics, list_tests)
  └── use vm            (imports symbol: run_in_vm)

diagnostics.kn  (orchestrator)
  ├── use cause         (imports: get_cause_tests, run_cause_test_by_tag, etc.)
  ├── use effect        (imports: effect_sanity_check, compute_effect, etc.)
  └── use spookymagic   (imports: spookymagic_sanity_check, get_spooky_factor, etc.)

cause.kn  (where you write tests)
  ├── use effect        (imports: effect_sanity_check, compute_effect, etc.)
  └── use spookymagic   (imports: spookymagic_sanity_check, run_spooky_test, etc.)

effect.kn  (downstream effects)
  └── use spookymagic   (imports: get_spooky_factor, etc.)

spookymagic.kn  (black-box behaviors)
  └── no imports (standalone)

vm.kn  (process isolation)
  └── uses std::process for subprocess spawning
```

**Key Kain import rule:** `use module` imports all public symbols directly into scope
(like Python's `from module import *`). You call `function_name()`, NOT
`module.function_name()`. The qualified-dot syntax is a module path reference that
the typechecker accepts but codegen treats as an undefined variable.

## spawn.kn — Self-Replicating Template Cloner

Instead of manually copying the folder every time, use `spawn.kn` to instantly
clone the template to any location with a custom name:

```powershell
# Clone to ./my-bug/ (fastest)
cd X:\blades\templates\debug
kain run spawn.kn --target llvm -- --name my-bug

# Clone to a custom output directory
kain run spawn.kn --target llvm -- --name ownership-crash --output D:\work\

# Clone from a copy of the template (self-replicating)
cd .\existing-clone
kain run spawn.kn --target llvm -- --name another-clone

# Show help
kain run spawn.kn --target llvm -- --help
```

**Flags:**

| Flag | Default | Effect |
|------|---------|--------|
| `--name <name>` | `debug-template` | Folder name for the clone |
| `--output <path>` | current dir | Parent directory for the clone |
| `--source <path>` | current dir | Template source directory |
| `--help` / `-h` | — | Show usage |

`spawn.kn` copies all 10 template files (including itself) and the clone
immediately passes `kain check` 6/6. No manual renaming, no extra tool calls.

## The Three Test Files

### `cause.kn` — Root Cause Definition

**This is where most agents will write code.** Define the root cause of a bug, edge case, or semantic experiment.

- **Import pattern:** `use effect` + `use spookymagic`
- **Export pattern:** Register test functions in `get_cause_tests()` table with tags
- **Test function signature:** `pub fn test_<name>() -> Int` (return 0 = pass)
- **IMPORTANT:** `use module` imports symbols directly — call `function()` not `module.function()`

```kain
// Example: add a new test in cause.kn
pub fn test_my_bug() -> Int:
    // Call imported functions directly (no module prefix!)
    let result = compute_effect(42)
    if result == 84:
        return 0  // pass
    return 1      // fail

// 1. Add dispatch case in run_cause_test_by_tag():
//    if tag == "my_bug": return test_my_bug()

// 2. Register it in the test table:
pub fn get_cause_tests() -> Array<CauseTest>:
    var tests: Array<CauseTest> = []
    push(tests, CauseTest {
        name: "my_bug",
        tag: "my_bug",
        description: "Reproduces the ownership-after-teleport crash"
    })
    return tests
```

### `effect.kn` — Downstream Effects

Model cascading consequences. Define what happens *after* the root cause triggers.

- **Import pattern:** `use spookymagic` (optional)
- **Export pattern:** Helper functions and data types
- Called by `cause.kn` to model full error cascade

### `spookymagic.kn` — Spooky Magic / Black-Box Behaviors

For Heisenbugs, race windows, cache coherence issues, timing-dependent failures, or any behavior that doesn't fit a clean cause→effect model.

- **Import pattern:** None (standalone — prevents circular deps)
- **Export pattern:** Factor functions, test functions, spooky error structs
- Can be imported by both `cause.kn` and `effect.kn`

## CLI Flags

| Flag | Effect |
|------|--------|
| `--vm` | Run test inside an isolated subprocess. Captures stdout/stderr deterministically. Use for black-box errors that need clean-room execution. |
| `--test <name>` | Run a specific test. Names: `cause`, `effect`, `spookymagic`, `all`, or any test name from `--list`. |
| `--list` | List all available tests with their descriptions. |
| `--verbose` / `-v` | Enable verbose output — shows test descriptions and detailed results. |
| `--help` / `-h` | Show usage and file taxonomy. |

### The `--vm` Flag

The VM flag spawns the template binary as a child process with piped stdio, runs the specified test, captures all output, and reports the exit code. This provides:

1. **Deterministic capture** — stdout/stderr are fully captured, not interleaved with the parent process
2. **Process isolation** — crashes in the child don't take down the diagnostic harness
3. **Clean-room execution** — each run starts fresh, preventing state leakage between tests
4. **Timeout protection** — child processes are killed after 30 seconds

**Advanced:** For bytecode-level isolation, copy the Markscript VM (`X:\blades\markscript\src/vm.kn`, `types.kn`, `error.kn`) into this template and compile your test logic to Markscript bytecode for fully deterministic execution through `execute_bytecode()`.

## Fastest Workflow: Adding a New Error Test

```powershell
# 1. Open cause.kn
# 2. Add your test function
# 3. Register it in get_cause_tests()
# 4. Run:
kain check              # typecheck only (fastest)
kain run                # full compile + execute
kain run -- --test cause_sanity --verbose  # run one test
```

**Pattern for maximum speed:** Write the test body first, `kain check` to verify syntax, then `kain run` to execute. If something weird happens, add `--vm` for isolation.

## Diagnostics Report Format

```
═══════════════════════════════════════════════════════════
  DIAGNOSTICS REPORT
═══════════════════════════════════════════════════════════
  Total:   7
  Passed:  7
  Failed:  0
  Warnings:0
───────────────────────────────────────────────────────────
  [PASS] cause::cause_sanity
  [PASS] cause::cause_effect_chain
  [PASS] cause::cause_spooky_integration
  [PASS] effect::effect_sanity
  [PASS] effect::effect_compute
  [PASS] spookymagic::spookymagic_sanity
  [PASS] spookymagic::spookymagic_factor
═══════════════════════════════════════════════════════════
  VERDICT: ALL TESTS PASSED
```

## Architecture Principles

1. **Always compiles** — Even with empty test bodies, all imports resolve. Adding code to any single file doesn't break the other files.
2. **No circular imports** — Strict linear dependency chain: cause → effect → spookymagic.
3. **Test table pattern** — Each module registers tests in a discoverable table. The diagnostics module iterates tables without hardcoding test names.
4. **Exit code contract** — 0 = pass, non-zero = failure. CLI, diagnostics, and VM all respect this.
5. **Self-contained** — Only depends on `std::*` (stdlib). No external blade imports needed.

## Learning Kain End-to-End: The Smoketest Album

This debug template is a **surgical instrument** — designed for rapid single-bug reproduction.

To understand Kain as a **whole** — every semantic layer, every effect, every compile
target, and every interop lane exercised in one unified proof surface — read the
**Smoketest Album** at `X:\smoketest\README.md`.

The smoketest is the single most comprehensive Kain feature surface in the repo:
- **8-layer decision ladder** (world → entangle → patch → law → converge → orchestrate → axiom → actor)
- **All 65+ stdlib modules** exercised
- **GPU compute & graphics** (SPIR-V, PTX, HLSL)
- **C interop** (SQLite 9.1 MB amalgamation binding, Win32, companion C discovery)
- **Python interop** (subprocess orchestration, `python_exec`)
- **WASM target** cross-compilation
- **UI components & OpenGL visualizer bridge**
- **Actor system** with typed message passing
- **Telemetry evidence DAG** with composition checksums
- **Build authority** with 9 module roots, 3 compile targets, GPU artifact generation

It is the definitive teaching ground for how Kain works end-to-end.

```
X:\smoketest\README.md   ← The Kain Album: everything in one place
```

## Automation Ready

This template is designed for later automation via a repo init script. The structure supports:

- **Batch test generation:** Script can write `cause.kn` test functions automatically
- **CI integration:** `kain run -- --vm` produces deterministic exit codes
- **Fuzzing harness:** Replace `cause.kn` test bodies with fuzzer-generated code
- **Regression suite:** Copy template per-bug, add test, commit as evidence

## Advanced: Markscript VM Integration

For bytecode-level determinism (beyond process isolation), integrate the Markscript bytecode VM:

1. Copy these files from `X:\blades\markscript\src/` into `src/`:
   - `vm.kn` → `markscript_vm.kn`
   - `types.kn` → `markscript_types.kn`
   - `error.kn` → `markscript_error.kn`
2. Compile your test logic to Markscript bytecode (flat `Array<Int>`)
3. Execute through `execute_bytecode(vm, bytecode)` for:
   - Exact instruction-level determinism
   - Typed operand stack tracing
   - Handler dispatch inspection
   - Zero native-side non-determinism

See `X:\blades\markscript\README.md` for the full Markscript specification.
