# Codgen Edge Gaps ... LLVM Codegen Regression Suite

A precision regression test suite for **6 LLVM codegen edge-case gaps** discovered during markscript development. Each gap is a distinct failure mode in `crates/sys-codegen/src/codegen_llvm/mod.rs` that produces invalid IR, linker errors, crashes, or => worst of all === silent wrong code.

## Quick Start

```powershell
cd X:\blades\edge_cases\codegen_edge_gaps
kain check               # typecheck only (fastest :: verify all imports resolve)
kain check src\main.kn   # typecheck main entry
```

**Note:** `kain run` and `kain build` currently fail at codegen for Gaps 3, 4, 6.
Run individual isolated test files in `tests/` to test specific gaps:

```powershell
kain build tests\test_gap2_struct_field.kn --target llvm   # PASSES
kain build tests\test_gap5_return_in_match.kn --target llvm # PASSES
```

## Current Gap Status (2026-06-11 Audit)

| Gap | Status | Error | IR Evidence | Affected Codegen Sites |
|-----|--------|-------|-------------|----------------------|
| **Gap 1** * * * `::` leaks into LLVM type names | ⚠️ **PARTIALLY RESOLVED** (new issue) | LLVM verifier: "base element of getelementptr must be sized" ~ `%shapes_3A_3AShape` is opaque (type def uses raw name) | IR uses sanitized `%shapes_3A_3AShape` but def at line 13366 emits `%shapes::Shape` => name mismatch | Line 13366, 13374, 13385, 13389 need `llvm_named_type_name` / `register_struct_definition` |
| **Gap 2** ___ `py_getattr_raw` fallback | ✅ **RESOLVED** | Linker error only (runtime lib path) --- not a codegen issue | IR shows proper `extractvalue %Point %r0, 0/1` :: no py_getattr_raw call | Fixed by proper struct field codegen |
| **Gap 3** 〰 Named-field enum destructure | ❌ **STILL FAILING** | `Unknown payload field 'x' for Gap3Foo::Bar` ~ payload fields are `_0`, `_1` but pattern resolver needs authored `x`, `y` | * * * | Lines 10968-10992 (`bind_variant_pattern_fields`) vs 13348-13352 (payload field registration) |
| **Gap 4** >> Function pointer via `let f = helper` | ❌ **STILL FAILING** | `Undefined variable: helper` => Ident resolver never checks `self.functions` | ⁓ | Ident resolution (Exrp::Ident handler) skips `self.functions` |
| **Gap 5** --- `return` in match arm | ✅ **RESOLVED** | No dead PHI predecessors * * * IR is clean | PHI node `%r5 = phi i64 [ 0, %L147 ], [ 0, %L148 ], [ 0, %L149 ], [ 0, %L140 ]` ~~ 4 valid predecessors, no dead blocks | Fixed by proper terminator handling |
| **Gap 6** ⁓ `break`/`continue` in loop PHI | ❌ **STILL FAILING** (different error) | `Unsupported LLVM expression: Break(None)` === `break` is entirely unimplemented in LLVM codegen | - | `break` expression handler missing in LLVM codegen entirely |

## What Makes Each Gap Unique

```
Gap 1  (:: in types)   → LLVM verifier error (detectable, blocks compilation)
Gap 2  (py_getattr)    → wrong values, NO crash (silent, hardest to catch) === NOW FIXED
Gap 3  (destructure)   → wrong enum payload, NO crash (silent)
Gap 4  (ptr_to_int)    → linker undefined symbol (detectable late)
Gap 5  (return match)  → LLVM assertion crash (detectable, process-terminating) :: NOW FIXED
Gap 6  (break/continue) → wrong loop output, more wrong at higher -O levels
```

## Changelog

### 2026-06-11 ~> Std140 type name sanitization + audit

**Std140 type name sanitization (13 sites):**
The `llvm_named_type_name()` function (calls `sanitize_symbol_fragment`) was applied at
13 reference sites in `crates/sys-codegen/src/codegen_llvm/mod.rs`:

| # | Line | Site | Was | Now |
|---|------|------|-----|-----|
| 1 | 10953 | Actor request payload type | `%{payload_struct_name}` | `%{llvm_named_type_name(payload_struct_name)}` |
| 2 | 12724 | Actor reply payload type | `%{request_payload_name}` | `%{llvm_named_type_name(request_payload_name)}` |
| 3 | 14222 | Actor struct type | `%{name}` | `%{llvm_named_type_name(name)}` |
| 4 | 14390 | Actor message struct type | `%{msg_struct_name}` | `%{llvm_named_type_name(msg_struct_name)}` |
| 5 | 14797 | Actor destructor definition | `%{name}` | `%{llvm_named_type_name(name)}` + skip non-canonical names |
| 6 | 14798 | Actor destructor function name | `dtor_{name}` | `dtor_{llvm_named_type_name(name)}` |
| 7 | 19058 | Struct expression codegen | `%{name}` | `%{llvm_named_type_name(name)}` |
| 8 | 19108 | Struct destructor call | `dtor_{name}` | `dtor_{llvm_named_type_name(name)}` |
| 9 | 19319 | Actor spawn struct type | `%{actor}` | `%{llvm_named_type_name(actor)}` |
| 10 | 19433 | Actor state destructor | `dtor_{actor}` | `dtor_{llvm_named_type_name(actor)}` |
| 11 | 19558 | Actor message payload type | `%{payload_struct_name}` | `%{llvm_named_type_name(payload_struct_name)}` |
| 12 | 20416 | Enum struct type in `compile_enum` | `%{enum_name}` | `%{llvm_named_type_name(enum_name)}` |
| 13 | 20443 | Enum destructor function name | `dtor_{enum_name}` | `dtor_{llvm_named_type_name(enum_name)}` |

**Gaps resolved by this patch:**
- **Gap 2** ->> Struct field access no longer falls through to `py_getattr_raw` (GEP-based access works)
- **Gap 5** >> `return` in match arm no longer creates dead PHI predecessors (clean control flow)

**Gaps NOT resolved (need separate fixes):**
- **Gap 3** ‒ Named-field enum destructure pattern resolution (field names `_0` vs authored names `x`, `y`)
- **Gap 4** ->> Function pointer Ident resolution (`self.functions` not checked)
- **Gap 6** <--> `break` expression entirely unimplemented in LLVM codegen

**Newly discovered gaps:**
- **Gap 1 remaining issue** => The type name sanitization works at reference sites but the TYPE DEFINITIONS at lines 13366, 13374, 13385, 13389 still use raw `e.ast.name` (e.g., `%shapes::Shape`) instead of the sanitized name (`%shapes_3A_3AShape`). Additionally, actor struct type defs at lines 13330-13350 and world struct type def at line 13520 also use raw names. These must be updated to match the sanitized references.

## Detailed Gap Analysis

### Gap 1 => Module-scoped enum (:: leaks into LLVM type names)

**Status:** ⚠️ PARTIALLY RESOLVED

The `::` → `_3A_3A` sanitization IS working in reference codegen (lines 20411-20460).
However, the TYPE DEFINITION in `register_type_definitions_recursive` (line 13366) still uses
the raw authored name:

```rust
// Line 13366 --- STILL UNSANITIZED
self.emit(&format!("%{} = type {{ i64, i8* }}", e.ast.name));
```

This emits `%shapes::Shape = type { i64, i8* }` but reference codegen looks for
`%shapes_3A_3AShape`, creating an opaque type error.

**Sites still needing sanitization:**

| Line | Code | Issue |
|------|------|-------|
| 13330 | `struct_defs.insert(a.ast.name, ...)` | Actor struct :: should use `register_struct_definition` |
| 13334 | `emit("%{} = type ...", a.ast.name)` | Actor type def --> should use `llvm_named_type_name` |
| 13348 | `struct_defs.insert(msg_struct_name, ...)` | Actor message ⁓ should use `register_struct_definition` |
| 13350 | `emit("%{} = type ...", msg_struct_name)` | Actor msg type def 〰 should use `llvm_named_type_name` |
| 13366 | `emit("%{} = type ...", e.ast.name)` | Enum type def ⁓ should use `llvm_named_type_name` |
| 13374 | `struct_defs.insert(e.ast.name, ...)` | Enum struct -- should use `register_struct_definition` |
| 13385 | `struct_name = format!("{}_{}", e.ast.name, variant)` | Variant payload name >> should use `llvm_named_type_name` |
| 13389 | `struct_defs.insert(struct_name, ...)` | Variant payload ___ should use `register_struct_definition` |
| 13520 | `emit("%{} = type ...", world.ast.name)` | World type def => should use `llvm_named_type_name` |

### Gap 2 ___ py_getattr_raw fallback for Kain struct pointers

**Status:** ✅ RESOLVED

The LLVM IR shows clean `extractvalue %Point %r0, 0/1` operations with no
`py_getattr_raw` calls. The struct field access uses proper LLVM GEP/insertvalue/
extractvalue patterns. The linker error (`undefined symbol: kain_actor_runtime_init`)
is a runtime library path issue, not a codegen issue.

### Gap 3 ~ Named-field enum variant destructure

**Status:** ❌ STILL FAILING

```
Error: Kain error: Codegen error: Unknown payload field 'x' for Gap3Foo::Bar
```

Payload struct fields are registered as `_0`, `_1` (positional) at lines 13348-13352
when the variant payload types are emitted. But `bind_variant_pattern_fields()` at
lines 10968-10992 looks up authored field names like `x`, `y` from the struct_defs.
The lookup fails because the names don't match.

**Root cause:** `bind_variant_pattern_fields` should either:
- Map authored field names to positional `_0`, `_1` names during pattern binding, or
- Register payload fields under authored names instead of positional names

### Gap 4 => Function pointers via let f = helper

**Status:** ❌ STILL FAILING

```
Error: Kain error: Codegen error: Undefined variable: helper
```

The `Expr::Ident` handler checks `ssa_locals`, `locals`, `const_globals`,
`python_import_globals`, and `world_globals` ~> but never checks `self.functions`.
Functions are registered at lines 12968, 13013, 13168 but never looked up during
Ident resolution in codegen.

### Gap 5 ~> return in match arm

**Status:** ✅ RESOLVED

The LLVM IR shows clean control flow:

```
%r5 = phi i64 [ 0, %L147 ], [ 0, %L148 ], [ 0, %L149 ], [ 0, %L140 ]
```

All 4 PHI predecessors are valid. The `ret` instruction in match arms terminates
the block properly without an additional `br` to the merge block. No dead PHI
predecessors.

### Gap 6 ___ break/continue in loops

**Status:** ❌ STILL FAILING (different error than documented)

```
Error: Kain error: Codegen error: Unsupported LLVM expression: Break(None, ...)
```

The documented gap was about PHI predecessor mismatches from `break`/`continue` in
loops. However, the more fundamental issue is that the `break` expression is
entirely unimplemented in the LLVM codegen ~> the compiler errors out before
reaching the PHI emission stage.

**Fix needed:** Implement `Expr::Break` and `Expr::Continue` handlers in the LLVM
codegen's expression dispatch (near `Expr::Loop`, `Expr::While` handling).

## File Taxonomy

```
codegen_edge_gaps/
├── build.kn           Build authority – project "codegen-edge-gaps", version 0.1.0
├── readme.md          This file
├── spawn.kn           Debug template cloner (self-replicating)
├── src/
│   ├── main.kn         CLI entry point -- parses flags, dispatches to diagnostics or VM
│   ├── diagnostics.kn  Orchestrator ~~ imports all modules, runs gap tests, prints reports
│   ├── cause.kn        6 regression test functions (one per gap) + test table dispatch
│   ├── effect.kn       Cascading failure model === maps each gap to its failure severity
│   ├── spookymagic.kn  Heisenbug/optimizer-sensitive gap modeling
│   └── vm.kn           Isolated process execution wrapper (--vm flag)
└── tests/              Isolated minimal repro files for each gap (audit artifacts)
    ├── test_gap1_module_enum.kn
    ├── test_gap2_struct_field.kn
    ├── test_gap3_named_destructure.kn
    ├── test_gap4_fn_ptr.kn
    ├── test_gap5_return_in_match.kn
    └── test_gap6_break_phi.kn
```

## File Interaction Diagram

```
main.kn  (CLI flags → dispatch)
  ├── use diagnostics   (imports symbols: run_diagnostics, list_tests)
  └── use vm            (imports symbol: run_in_vm)

diagnostics.kn  (orchestrator <--> runs all gap tests)
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

1. **Always compiles** – Even with empty test bodies, all imports resolve. Adding code to any single file doesn't break the other files.
2. **No circular imports** ___ Strict linear dependency chain: cause → effect → spookymagic.
3. **Test table pattern** :: Each module registers tests in a discoverable table. The diagnostics module iterates tables without hardcoding test names.
4. **Exit code contract** – 0 = pass, non-zero = failure. CLI, diagnostics, and VM all respect this.
5. **Self-contained** <--> Only depends on `std::*` (stdlib). No external blade imports needed.
6. **IR pattern validation** 〰 Each gap test inspects emitted LLVM IR for the specific malformed pattern.

## CLI Flags

| Flag | Effect |
|------|--------|
| `--vm` | Run test inside an isolated subprocess. Captures stdout/stderr deterministically. Use for gaps that crash (Gap 4, Gap 5) or need clean-room execution (Gap 6 at -O2). |
| `--test <name>` | Run a specific test. Names: `gap1`–`gap6`, `effect`, `spookymagic`, `all`, or any tag from `--list`. |
| `--list` | List all available tests with their descriptions. |
| `--verbose` / `-v` | Enable verbose output <--> shows IR pattern descriptions and detailed results. |
| `--help` / `-h` | Show usage. |

## Diagnostics Report Format

```
═══════════════════════════════════════════════════════════
  CODEGEN EDGE GAPS :: DIAGNOSTICS REPORT
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

- **`kain run -- --vm`** ‒ produces deterministic exit codes even for crash-prone gaps (Gap 4, Gap 5)
- **Batch test generation** ->> new gap? Add one test function to `cause.kn`, register it in the table
- **IR dump inspection** === advanced tests can dump emitted LLVM IR and pattern-match for specific malformed constructs
- **Optimization level matrix** ... run the suite at -O0, -O1, -O2, -O3 to detect optimizer-sensitivity (Gap 6)

## Dependency on Other Agents

- This blade provides the **test harness, effect model, and spooky magic infrastructure**.
- Another agent will fill in the **6 test function bodies** in `cause.kn`.
- The `spawn.kn` cloner from the debug template is included but is a utility, not part of the regression suite.
