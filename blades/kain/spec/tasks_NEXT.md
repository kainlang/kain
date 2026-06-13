# Master Task Plan: kainc Self-Host Compiler — Sprint 2 (Typechecker + Codegen + Ouroboros)

**Phase:** 3 of 3 — Tasks (FINAL for Sprint 2)
**Created:** 2026-06-12
**Status:** Ready for Parallel Implementation
**Based on:** 7 review files, bootstrap_assessment.md, gap_analysis.md, FINAL_GAPS.md
**Goal:** Get from current state (~25% semantic surface) to **OUROBOROS VERIFIED**

---

## Current State Summary

| Subsystem | File | Real % | Status |
|-----------|------|--------|--------|
| Lexer | `lexer.kn` (778 lines) | 95% | ✅ FULL — DFA tokenizer, indent processor |
| Parser | `parser.kn` (3345 lines) | 100% | ✅ FULL — All 108 keywords, Pratt engine |
| AST | `ast.kn` (357 lines) | 100% | ✅ FULL — 38 item, 64 expr, 12 stmt kinds |
| Typechecker | `types.kn` (1873 lines) | 75% L0 | ⚠️ check_function_item REAL. check_struct_item PARTIAL. check_enum_item STUB (just returns TypedItem). check_trait_impl_item STUB. infer_expr_type handles ~35/64 expr kinds. |
| Monomorphizer | `monomorphize.kn` (420 lines) | 50% | ⚠️ Passes through non-generic items. Generic instantiation loop minimal. |
| Codegen | `codegen.kn` (1563 lines) | 70% L0 | ⚠️ 17/30+ expr kinds lowered. Struct defs `type opaque`. Const globals zeroinitializer. No runtime declares emitted. |
| Orchestrator | `orchestrator.kn` (897 lines) | 50% | ⚠️ First 4 handlers wired. Handlers 205-208 STUB. |
| Compiler | `compiler.kn` (387 lines) | 80% | ✅ Pipeline wired. `compile_file()`, `check_file()`, `emit_diagnostics_to_stderr()` real. |
| CLI | `cli.kn` (461 lines) | 80% | ✅ 12 subcommand dispatch. Exit codes, help text. |
| Entry | `main.kn` (59 lines) | 100% | ✅ Arg parsing, subcommand routing. |

---

## Stream Overview

| Stream | File | Role | Effort | Files Touched | Parallel? |
|--------|------|------|--------|---------------|-----------|
| **RED** | `tasks_RED.md` | Typechecker Completion — make all item checking real, complete expression inference | 2-3 weeks | `types.kn`, `monomorphize.kn` | ✅ Wave 1 |
| **GREEN** | `tasks_GREEN.md` | Ouroboros Pipeline — wire selfhost handlers, fix llvm_ffi.kn, multi-file compilation, KAIN.toml | 1-2 weeks | `orchestrator.kn`, `compiler.kn`, `llvm_ffi.kn`, `KAIN.toml` | ✅ Wave 1 |
| **BLUE** | `tasks_BLUE.md` | Codegen Completion — complete expression lowering, runtime declares, string ABI, struct codegen | 2-3 weeks | `codegen.kn`, `runtime.kn` | ⏳ Wave 2 (after RED) |
| **GOLD** | `tasks_GOLD.md` | L1-L7 Stub→Real — world/actor/converge/orchestrate/pulse/shatter typecheck + codegen | 4-6 weeks | `types.kn`, `codegen.kn` (L1-L7 sections) | ⏳ Wave 3 (after RED+BLUE) |

---

## Dependency Graph

```
WAVE 1 (launch simultaneously):
  RED ─────────────────────┐ (Typechecker: types.kn)
  GREEN ───────────────────┤ (Ouroboros: orchestrator.kn, compiler.kn, KAIN.toml, llvm_ffi.kn)
                           │
WAVE 2:                    │
  BLUE ────────────────────┘ ← depends on RED (consumes TypedProgram)
                           │
WAVE 3 (deferred):         │
  GOLD ────────────────────┘ ← depends on RED + BLUE (needs L0 typecheck + codegen foundation)
```

---

## Spawn Strategy

### Wave 1 — Launch NOW (parallel, no inter-dependencies)

Spawn these subagents simultaneously:

```
Agent 1 → tasks_RED.md    (RED: Typechecker Completion — types.kn + monomorphize.kn)
Agent 2 → tasks_GREEN.md  (GREEN: Ouroboros Pipeline — orchestrator.kn, compiler.kn, llvm_ffi.kn, KAIN.toml)
```

RED and GREEN touch completely disjoint files. They can run at the same time.

### Wave 2 — Launch AFTER RED completes

RED must finish first because BLUE consumes `TypedProgram` items whose structure depends on real typechecking (struct field maps, enum variant indices, function signatures). Wait for RED, then launch:

```
Agent 3 → tasks_BLUE.md   (BLUE: Codegen Completion — codegen.kn, runtime.kn)
```

### Wave 3 — Launch AFTER RED+BLUE (deferred)

GOLD is fully deferred — it depends on RED (typechecker) and BLUE (codegen) being fully real for L0 before L1-L7 can be implemented meaningfully.

---

## Shared Files (Cross-Stream Conflicts)

| File | Streams | Conflict Resolution |
|------|---------|---------------------|
| `types.kn` | RED, GOLD | RED owns L0 sections (lines 1-1610). GOLD owns L1-L7 stubs (lines 1401-1456 → replace with real). RED's work does NOT touch L1-L7 stubs. |
| `codegen.kn` | BLUE, GOLD | BLUE owns L0 expression lowering + runtime declares (lines 1-1563). GOLD adds L1-L7 sections after BLUE completes. |
| `orchestrator.kn` | GREEN | GREEN is the ONLY stream touching orchestrator.kn in Wave 1. No conflict. |
| `compiler.kn` | GREEN | GREEN is the ONLY stream touching compiler.kn in Wave 1. No conflict. |

**Rule:** If a file appears in this table, each stream's task file explicitly shows which section/region it owns. No stream should modify code outside its declared region.

---

## Merge & Verification Plan

After all waves complete, the parent agent should:

1. **Verify no cross-stream conflicts** — check shared files (types.kn, codegen.kn) for merge issues
2. **Run kain check on all 23 files individually** — `kain check src/`
3. **Run the ouroboros Phase 1** — `kain selfhost bootstrap --manifest src/KAIN.toml` to verify combine
4. **Build kainc.exe** — `kain build . --target llvm`
5. **Run kainc.exe --version** — verify binary runs
6. **Run kainc.exe check src/token.kn** — verify self-check works
7. **Verify against requirements** — check the traceability matrix below

---

## Requirements Traceability Matrix

| Requirement | Stream(s) | Description |
|-------------|-----------|-------------|
| FR-typecheck | RED | Real typechecking for all L0 item kinds |
| FR-infer | RED | Real expression type inference for all 64 expr kinds |
| FR-compat | RED | Complete `types_compatible()` for all 20 ResolvedType variants |
| FR-struct-check | RED | Struct field resolution, duplicate detection, env registration |
| FR-enum-check | RED | Enum variant payload validation |
| FR-trait-check | RED | Trait method registration, impl method signature matching |
| FR-mono | RED | Generic monomorphization loop real |
| FR-codegen-expr | BLUE | Expression lowering for all expression kinds |
| FR-codegen-cf | BLUE | Control flow: match, for, loop, break, continue |
| FR-codegen-fn | BLUE | Real function body compilation |
| FR-codegen-struct | BLUE | Struct field access with field-name-to-index mapping |
| FR-codegen-runtime | BLUE | Runtime function declares (allocator, string, IO) |
| FR-codegen-string | BLUE | String ABI marshaling (fat pointer {i8*, i64}) |
| FR-combine | GREEN | Ouroboros Phase 1: source concatenation |
| FR-selfhost | GREEN | Ouroboros Phase 2: compile combined source |
| FR-verify | GREEN | Ouroboros verification: byte-identical IR |
| FR-llvm-ffi | GREEN | Fix llvm_ffi.kn for machines without LLVM-C headers |
| FR-multifile | GREEN | Multi-file compilation (resolve `use` imports) |
| FR-workspace | GREEN | Workspace discovery (directory ascent for KAIN.toml) |
| FR-l1-l7-tc | GOLD | L1-L7 typechecking (world/actor/converge/orchestrate/...) |
| FR-l1-l7-cg | GOLD | L1-L7 codegen (world globals, actor dispatch, GPU, ...) |

---

## Risk Register

| Risk | Mitigation Stream | Contingency |
|------|------------------|-------------|
| Typechecker env threading incomplete — mutations don't persist across items | RED | Add explicit `TypeEnv` return from check_item, thread through pass4 loop |
| Codegen field access uses hardcoded index 0 — real types have multiple fields | BLUE | After RED finishes, TypedItem will carry field map; use it for GEP indexing |
| llvm_ffi.kn fails on machines without LLVM headers | GREEN | Make includes conditional; provide stub type definitions when headers absent |
| Ouroboros Phase 2 blocked on codegen depth | BLUE | GREEN prepares infrastructure; actual ouroboros will light up after BLUE |
| GOLD L1-L7 scope too large | GOLD | Prioritize world + actor first, defer GPU/component/orchestrate to later |

---

## Notes for Parent Agent

### Spawning subagents

For Wave 1, launch two agents simultaneously:

```
Agent 1: Read tasks_RED.md → work on X:/blades/kain/src/types.kn and monomorphize.kn
Agent 2: Read tasks_GREEN.md → work on X:/blades/kain/src/orchestrator.kn, compiler.kn, llvm_ffi.kn, KAIN.toml
```

Both agents can run independently — they touch completely disjoint files.

### After RED completes

Launch Agent 3:
```
Agent 3: Read tasks_BLUE.md → work on X:/blades/kain/src/codegen.kn
```

### Verification after all waves

```bash
# 1. Check all files individually
cd X:\blades\kain
kain check src/

# 2. Run ouroboros Phase 1 (combine)
kain selfhost bootstrap --manifest src/KAIN.toml

# 3. Build the compiler
kain build . --target llvm

# 4. Verify the binary
.\.kain\out\kainc.exe --version
.\.kain\out\kainc.exe check src/token.kn
```

### Key files to watch
- `X:/blades/kain/src/types.kn` — RED's primary target; 1873 lines, many stubs to replace
- `X:/blades/kain/src/codegen.kn` — BLUE's primary target; 1563 lines, 17 expr kinds lowered
- `X:/blades/kain/src/orchestrator.kn` — GREEN's primary target; handlers 205-208 are stubs
- `X:/blades/kain/src/KAIN.toml` — GREEN needs to ensure this has full [selfhost] config
