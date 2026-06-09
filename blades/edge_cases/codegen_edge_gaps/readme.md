# Codgen Edge Gaps — LLVM Codegen Regression Suite

A precision regression test suite for **6 LLVM codegen edge-case gaps** discovered during markscript development. Each gap is a distinct failure mode in `crates/sys-codegen/src/codegen_llvm/mod.rs` that produces invalid IR, linker errors, crashes, or — worst of all — silent wrong code.

## Quick Start

```powershell
cd X:\blades\edge_cases\codegen_edge_gaps
kain check               # typecheck only (fastest — verify all imports resolve)
kain run                 # full compile + run all 6 gap regression tests
kain run -- --test gap1  # run a specific gap test
kain run -- --vm         # run inside isolated subprocess (for crash-catching)
```

## The 6 Codegen Gaps

| Gap | Root Cause | Severity | Validates |
|-----|-----------|----------|-----------|
| **Gap 1** — `::` leaks into LLVM type names | Module-scoped enums produce invalid IR because `::` separator is used verbatim in LLVM struct type names. LLVM verifier rejects the type. | **LLVM verifier rejection** — compile-time, detectable | `test_gap1_type_names()` — emits a module-scoped enum and inspects the IR type string |
| **Gap 2** — `py_getattr_raw` fallback incorrectly fires | The `py_getattr_raw` fallback path triggers for Kain-to-Kain struct field access, returning a Python-object-type lookup instead of a struct GEP. Silent wrong data — no crash, just incorrect field values. | **Silent wrong data** ⚠️ most dangerous | `test_gap2_struct_field_read()` — reads struct fields and verifies values match Kain-level semantics, not Python interop fallback |
| **Gap 3** — Named-field enum variant destructure fails | When pattern-matching a named-field enum variant, the LLVM codegen emits payload fields named `_0`, `_1` but the pattern matching resolver looks for the authored field names (e.g. `x`, `y`). The accessor never finds the named field. | **Silent wrong payload extraction** ⚠️ | `test_gap3_named_enum_destructure()` — destructures a named-field enum variant and asserts correct field access |
| **Gap 4** — Function pointers via `ptr_to_int` missing | The `ptr_to_int` lowering path calls the Ident resolver to look up function symbols, but the resolver never checks `self.functions`. The function pointer emission produces an undefined external symbol that fails at link time. | **Linker error** — detectable | `test_gap4_func_ptr()` — takes a function pointer, converts to int, and verifies the IR contains the correct function symbol |
| **Gap 5** — `return` in match arm produces invalid IR | When a `return` appears inside a match arm, the codegen emits both a `ret` instruction AND a `br` to the merge block, creating a dead PHI predecessor. LLVM's PHI validation crashes. | **LLVM crash / assertion failure** — detectable | `test_gap5_return_in_match()` — a match expression with `return` in one arm; verifies no dead PHI predecessors in emitted IR |
| **Gap 6** — PHI node predecessor mismatches from `break`/`continue` in loops | When `break` or `continue` inside a loop creates alternative exit paths, the PHI nodes at the loop merge point have mismatched predecessor counts. This produces miscompiled output (wrong values from loop-carried dependencies). | **Runtime misbehaviour** | `test_gap6_break_continue_phi()` — loop with conditional `break` that produces a value; verifies correct PHI predecessor count |

## What Makes Each Gap Unique

```
Gap 1  (:: in types)   → LLVM verifier error (detectable, blocks compilation)
Gap 2  (py_getattr)    → wrong values, NO crash (silent, hardest to catch)
Gap 3  (destructure)   → wrong enum payload, NO crash (silent)
Gap 4  (ptr_to_int)    → linker undefined symbol (detectable late)
Gap 5  (return match)  → LLVM assertion crash (detectable, process-terminating)
Gap 6  (break/continue) → wrong loop output, more wrong at higher -O levels
```

## File Taxonomy

```
codegen_edge_gaps/
├── build.kn           Build authority — project "codegen-edge-gaps", version 0.1.0
├── readme.md          This file
└── src/
    ├── main.kn         CLI entry point — parses flags, dispatches to diagnostics or VM
    ├── diagnostics.kn  Orchestrator — imports all modules, runs gap tests, prints reports
    ├── cause.kn        6 regression test functions (one per gap) + test table dispatch
    ├── effect.kn       Cascading failure model — maps each gap to its failure severity
    ├── spookymagic.kn  Heisenbug/optimizer-sensitive gap modeling
    └── vm.kn           Isolated process execution wrapper (--vm flag)
```

## File Interaction Diagram

```
main.kn  (CLI flags → dispatch)
  ├── use diagnostics   (imports symbols: run_diagnostics, list_tests)
  └── use vm            (imports symbol: run_in_vm)

diagnostics.kn  (orchestrator — runs all gap tests)
  ├── use cause         (imports: get_cause_tests, run_cause_test_by_tag, etc.)
  ├── use effect        (imports: effect_sanity_check, compute_effect, etc.)
  └── use spookymagic   (imports: spookymagic_sanity_check, get_spooky_factor, etc.)

cause.kn  (6 gap regression test functions)
  ├── use effect        (imports: compute_effect, get_effect_table, etc.)
  └── use spookymagic   (imports: run_spooky_test, get_spooky_table, etc.)

effect.kn  (cascading failure severity model)
  └── use spookymagic   (imports: get_spooky_factor, etc.)

spookymagic.kn  (Heisenbug / optimizer-sensitivity model)
  └── no imports (standalone)
```

**Key Kain import rule:** `use module` imports all public symbols directly into scope
(like Python's `from module import *`). You call `function_name()`, NOT
`module.function_name()`. The qualified-dot syntax is a module path reference that
the typechecker accepts but codegen treats as an undefined variable.

## Architecture Principles

1. **Always compiles** — Even with empty test bodies, all imports resolve. Adding code to any single file doesn't break the other files.
2. **No circular imports** — Strict linear dependency chain: cause → effect → spookymagic.
3. **Test table pattern** — Each module registers tests in a discoverable table. The diagnostics module iterates tables without hardcoding test names.
4. **Exit code contract** — 0 = pass, non-zero = failure. CLI, diagnostics, and VM all respect this.
5. **Self-contained** — Only depends on `std::*` (stdlib). No external blade imports needed.
6. **IR pattern validation** — Each gap test inspects emitted LLVM IR for the specific malformed pattern.

## CLI Flags

| Flag | Effect |
|------|--------|
| `--vm` | Run test inside an isolated subprocess. Captures stdout/stderr deterministically. Use for gaps that crash (Gap 4, Gap 5) or need clean-room execution (Gap 6 at -O2). |
| `--test <name>` | Run a specific test. Names: `gap1`–`gap6`, `effect`, `spookymagic`, `all`, or any tag from `--list`. |
| `--list` | List all available tests with their descriptions. |
| `--verbose` / `-v` | Enable verbose output — shows IR pattern descriptions and detailed results. |
| `--help` / `-h` | Show usage. |

## Diagnostics Report Format

```
═══════════════════════════════════════════════════════════
  CODEGEN EDGE GAPS — DIAGNOSTICS REPORT
═══════════════════════════════════════════════════════════
  Total:   12
  Passed:  12
  Failed:  0
  Warnings:0
───────────────────────────────────────────────────────────
  [PASS] cause::gap1_type_names
  [PASS] cause::gap2_struct_field_read
  [PASS] cause::gap3_named_enum_destructure
  [PASS] cause::gap4_func_ptr
  [PASS] cause::gap5_return_in_match
  [PASS] cause::gap6_break_continue_phi
  [PASS] effect::effect_sanity
  [PASS] effect::effect_compute
  [PASS] spookymagic::spookymagic_sanity
  [PASS] spookymagic::spookymagic_factor
  [PASS] spookymagic::gap6_break_continue_phi
  [PASS] spookymagic::gap5_return_match_arm
═══════════════════════════════════════════════════════════
  VERDICT: ALL TESTS PASSED
```

## Reference Plan

See the full investigation plan at `X:\research\patch_edge_cases_plan.md` for the
root-cause analysis, proposed fixes for each gap, and the order in which patches
should be applied to `crates/sys-codegen/src/codegen_llvm/mod.rs`.

## Automation Ready

This regression suite is designed for CI integration:

- **`kain run -- --vm`** — produces deterministic exit codes even for crash-prone gaps (Gap 4, Gap 5)
- **Batch test generation** — new gap? Add one test function to `cause.kn`, register it in the table
- **IR dump inspection** — advanced tests can dump emitted LLVM IR and pattern-match for specific malformed constructs
- **Optimization level matrix** — run the suite at -O0, -O1, -O2, -O3 to detect optimizer-sensitivity (Gap 6)

## Dependency on Other Agents

- This blade provides the **test harness, effect model, and spooky magic infrastructure**.
- Another agent will fill in the **6 test function bodies** in `cause.kn`.
- The `spawn.kn` cloner from the debug template has been removed — this is a fixed-target regression suite, not a template.
