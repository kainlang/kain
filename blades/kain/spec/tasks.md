# Master Task Plan: Kain Self-Host Compiler (kainc)

**Phase:** 3 of 3 — Tasks (FINAL)
**Created:** 2026-06-12
**Status:** Ready for Parallel Implementation
**Based on:** /spec/requirements.md (Phase 1), /spec/design.md (Phase 2)

---

## Stream Overview

| Stream | File | Role | Effort | Files Touched | Wave | Parallel? |
|--------|------|------|--------|---------------|------|-----------|
| ALPHA | `tasks_alpha.md` | Foundation types + Lexer (~750 lines) | ~4h | `token.kn`, `error.kn`, `span.kn`, `lexer.kn` | Wave 1 | ✅ Launch Wave 1 |
| BRAVO | `tasks_bravo.md` | Dual JIT Engine (~1600 lines) | ~6h | `jit.kn`, `jit_metal.kn`, `jit_x86.kn`, `jit_orc.kn`, `jit_cache.kn` | Wave 1 | ✅ Launch Wave 1 |
| CHARLIE | `tasks_charlie.md` | MarkScript Orchestration (~500 lines) | ~3h | `orchestrator.kn` | Wave 1 | ✅ Launch Wave 1 |
| DELTA | `tasks_delta.md` | Parser + AST (~3500 lines) | ~12h | `ast.kn` (impl), `parser.kn` | Wave 2 | ⏳ After ALPHA foundation types |
| ECHO | `tasks_echo.md` | Runtime Contract + FFI (~500 lines) | ~3h | `runtime.kn`, `builtins.kn`, `llvm_ffi.kn` (type defs) | Wave 1 | ✅ Launch Wave 1 |
| FOXTROT | `tasks_foxtrot.md` | Typechecker + Monomorphizer (~2100 lines) | ~8h | `types.kn`, `effects.kn`, `monomorphize.kn` | Wave 3 | ⏳ After DELTA ast.kn |
| GOLF | `tasks_golf.md` | LLVM Codegen + CLI Driver (~3600 lines) | ~14h | `codegen.kn`, `llvm_ffi.kn` (impl), `compiler.kn`, `cli.kn`, `main.kn` | Wave 4 | ⏳ After FOXTROT |

**Total estimated effort:** ~50 hours (but parallelism brings wall-clock down to ~18h)

---

## Dependency Graph

```
WAVE 1 — Launch immediately, all 4 in parallel:
  ┌──────────────────────────────────────────────────────┐
  │                                                      │
  │  ALPHA: token.kn → error.kn → span.kn → lexer.kn    │
  │  BRAVO: jit_metal.kn → jit_x86.kn → jit_orc.kn      │──► Wave 2
  │  CHARLIE: orchestrator.kn                            │
  │  ECHO: runtime.kn → builtins.kn → llvm_ffi.kn (defs)│
  │                                                      │
  └──────────────────────────────────────────────────────┘
                    │
                    │ (ALPHA must finish TOKEN.KN and ERROR.KN first —
                    │  DELTA needs TokenKind, Token, Diagnostic types)
                    ▼
WAVE 2 — After ALPHA token.kn + error.kn complete:
  ┌──────────────────────────────────────────────┐
  │                                              │
  │  DELTA: ast.kn → parser.kn                  │
  │         (needs TokenKind, Token from ALPHA)  │
  │                                              │
  └──────────────────────────────────────────────┘
                    │
                    │ (FOXTROT needs AST_* constants and AstNode struct from DELTA)
                    ▼
WAVE 3 — After DELTA ast.kn complete:
  ┌──────────────────────────────────────────────┐
  │                                              │
  │  FOXTROT: types.kn → effects.kn → mono.kn   │
  │           (needs AstNode, AST_* from DELTA)  │
  │                                              │
  └──────────────────────────────────────────────┘
                    │
                    │ (GOLF needs ResolvedType, TypedProgram from FOXTROT)
                    ▼
WAVE 4 — After FOXTROT types complete:
  ┌──────────────────────────────────────────────┐
  │                                              │
  │  GOLF: codegen.kn → llvm_ffi.kn (impl)      │
  │         → compiler.kn → cli.kn → main.kn    │
  │         (needs TypedProgram from FOXTROT,    │
  │          llvm_ffi type defs from ECHO)       │
  │                                              │
  └──────────────────────────────────────────────┘
```

---

## Spawn Strategy

### Wave 1 — Launch NOW (parallel, 4 streams simultaneously)

Spawn these subagents at the same time. They have zero inter-dependencies and can run completely in parallel:

```
Agent 1 → tasks_alpha.md   (ALPHA: Foundation types + Lexer)
Agent 2 → tasks_bravo.md   (BRAVO: Dual JIT Engine)
Agent 3 → tasks_charlie.md (CHARLIE: MarkScript Orchestration)
Agent 4 → tasks_echo.md    (ECHO: Runtime Contract + FFI)
```

**Note on ALPHA:** ALPHA must complete token.kn and error.kn FIRST (these are the foundation types that DELTA needs). The ALPHA stream file orders tasks so token.kn + error.kn come before lexer.kn. As soon as ALPHA finishes ALPHA-01 through ALPHA-03 (token.kn, error.kn, span.kn), DELTA can safely start.

### Wave 2 — Launch after ALPHA token.kn + error.kn complete

```
Agent 5 → tasks_delta.md   (DELTA: Parser + AST)
```

This depends ONLY on ALPHA's token.kn types (TokenKind enum, Token struct, Diagnostic struct). The DELTA agent reads those type definitions from the completed ALPHA files. DELTA can start while ALPHA is still finishing lexer.kn — DELTA only needs the type definitions, not the lexer implementation.

### Wave 3 — Launch after DELTA ast.kn complete

```
Agent 6 → tasks_foxtrot.md (FOXTROT: Typechecker + Monomorphizer)
```

FOXTROT needs the AST_* constants and AstNode struct from DELTA. FOXTROT can start while DELTA is still finishing parser.kn — it only needs the AST type definitions.

### Wave 4 — Launch after FOXTROT types.kn complete

```
Agent 7 → tasks_golf.md    (GOLF: LLVM Codegen + CLI Driver)
```

GOLF needs ResolvedType and TypedProgram from FOXTROT's types.kn. GOLF can start while FOXTROT is still finishing effects.kn and monomorphize.kn — it only needs the core type definitions.

---

## Shared Files (Cross-Stream Conflicts)

| File | Streams | Conflict Resolution |
|------|---------|---------------------|
| `src/token.kn` | ALPHA (owner), DELTA (consumer) | ALPHA writes the COMPLETE file. DELTA reads it — DOES NOT MODIFY. |
| `src/error.kn` | ALPHA (owner), ALL (consumer) | ALPHA writes the COMPLETE file. Other streams read it. |
| `src/span.kn` | ALPHA (owner), DELTA (consumer) | ALPHA writes the COMPLETE file. DELTA reads it. |
| `src/ast.kn` | ALPHA (constants), DELTA (AstNode struct + constructors), FOXTROT (consumer), GOLF (consumer) | ALPHA writes ONLY the `AST_*` and `BINOP_*` and `UNOP_*` integer constants (lines 1-100). DELTA appends the `AstNode` struct, `ast_new_node()`, `ast_push_child()`, `ast_get_child()` implementation. The file is clearly partitioned with `// === STREAM: ALPHA ===` and `// === STREAM: DELTA ===` comments. |
| `src/llvm_ffi.kn` | ECHO (type defs, llvm-c include), GOLF (LLVM-C API wrapper functions) | ECHO writes the `include <llvm-c/Core.h> as llvm` header and type alias section (ptr<Byte> wrappers, enum constants). GOLF writes the LLVM builder wrapper functions (`llvm_build_add()`, etc.). File partitioned with clear stream headers. |
| `src/build.kn` | CHARLIE (owner) | CHARLIE creates this. No other stream touches it. |
| `src/buildex.md` | CHARLIE (owner) | CHARLIE creates this. No other stream touches it. |
| `src/KAIN.toml` | GOLF (owner) | GOLF creates this as part of workspace setup. No other stream touches it. |

**Rule:** If a file appears in this table, each stream's task file explicitly shows which section/region it owns. No stream should modify code outside its declared region.

---

## Merge & Verification Plan

After all waves complete, the parent agent should:

### 1. Verify no cross-stream conflicts
Check shared files for merge issues:
- `src/ast.kn`: Verify ALPHA's constants section and DELTA's struct/impl section don't overlap
- `src/llvm_ffi.kn`: Verify ECHO's type defs and GOLF's wrapper functions don't overlap

### 2. Run typecheck
```bash
cd X:\blades\kain
kain check src/
```
Expected: zero type errors on the full ~13,000-line compiler source.

### 3. Integration tests
- Lexer standalone: tokenize a known Kain file, verify token count matches expected
- Parser standalone: parse a known Kain file, verify AST node count matches expected
- Typechecker standalone: typecheck a known Kain file, verify zero errors
- Full pipeline: `kain build src/ --target llvm` produces valid LLVM IR

### 4. Ouroboros verification
```bash
kainc selfhost --verify-ouroboros
```
Expected: "OUROBOROS VERIFIED" with exit code 0.

### 5. Run the full test suite
```bash
kain test spec/
```

### 6. Verify against requirements
Check the traceability matrix below. Every FR, NFR, EC, and ERR must have a checkmark.

---

## Requirements Traceability Matrix

### FR-LEX (Lexer)
| Requirement | Stream | Status |
|-------------|--------|--------|
| FR-LEX.1-16 | ALPHA | ⬜ |
| FR-LEX.17-22 | ALPHA | ⬜ |

### FR-PARSE (Parser/AST)
| Requirement | Stream | Status |
|-------------|--------|--------|
| FR-PARSE.1-3 | DELTA | ⬜ |
| FR-PARSE.4-25 | DELTA | ⬜ |
| FR-PARSE.26-36 | DELTA | ⬜ |
| FR-PARSE.37-43 | DELTA | ⬜ |
| FR-PARSE.44-50 | DELTA | ⬜ |
| FR-PARSE.51-57 | DELTA | ⬜ |
| FR-PARSE.58-62 | DELTA | ⬜ |
| FR-PARSE.63-66 | DELTA | ⬜ |
| FR-PARSE.67-70 | DELTA | ⬜ |
| FR-PARSE.71-74 | DELTA | ⬜ |

### FR-TYPE (Typechecker)
| Requirement | Stream | Status |
|-------------|--------|--------|
| FR-TYPE.1-5 | FOXTROT | ⬜ |
| FR-TYPE.6-12 | FOXTROT | ⬜ |
| FR-TYPE.13-23 | FOXTROT | ⬜ |
| FR-TYPE.24-30 | FOXTROT | ⬜ |
| FR-TYPE.31-35 | FOXTROT | ⬜ |
| FR-TYPE.36-43 | FOXTROT | ⬜ |

### FR-CODEGEN (LLVM Codegen)
| Requirement | Stream | Status |
|-------------|--------|--------|
| FR-CODEGEN.1-3 | GOLF | ⬜ |
| FR-CODEGEN.4-14 | GOLF | ⬜ |
| FR-CODEGEN.15-18 | GOLF | ⬜ |
| FR-CODEGEN.19-22 | GOLF | ⬜ |
| FR-CODEGEN.23-33 | GOLF | ⬜ |
| FR-CODEGEN.34-35 | GOLF | ⬜ |
| FR-CODEGEN.36-39 | GOLF | ⬜ |
| FR-CODEGEN.40-43 | GOLF | ⬜ |
| FR-CODEGEN.44-46 | GOLF | ⬜ |

### FR-JIT (Dual JIT)
| Requirement | Stream | Status |
|-------------|--------|--------|
| FR-JIT.1-5 | BRAVO | ⬜ |
| FR-JIT.6-10 | BRAVO | ⬜ |
| FR-JIT.11-13 | BRAVO | ⬜ |
| FR-JIT.14-17 | BRAVO | ⬜ |
| FR-JIT.18-20 | BRAVO | ⬜ |
| FR-JIT.21-22 | BRAVO | ⬜ |

### FR-CLI (CLI Driver)
| Requirement | Stream | Status |
|-------------|--------|--------|
| FR-CLI.1-13 | GOLF | ⬜ |
| FR-CLI.14-17 | GOLF | ⬜ |
| FR-CLI.18-20 | GOLF | ⬜ |
| FR-CLI.21-23 | GOLF | ⬜ |

### FR-RUNTIME (Runtime/FFI)
| Requirement | Stream | Status |
|-------------|--------|--------|
| FR-RUNTIME.1-4 | ECHO | ⬜ |
| FR-RUNTIME.5-9 | ECHO | ⬜ |
| FR-RUNTIME.10-11 | ECHO | ⬜ |
| FR-RUNTIME.12-13 | ECHO + GOLF | ⬜ |
| FR-RUNTIME.14 | ECHO | ⬜ |
| FR-RUNTIME.15-17 | ECHO | ⬜ |

### FR-ORCH (MarkScript)
| Requirement | Stream | Status |
|-------------|--------|--------|
| FR-ORCH.1-2 | CHARLIE | ⬜ |
| FR-ORCH.3-11 | CHARLIE | ⬜ |
| FR-ORCH.12-14 | CHARLIE | ⬜ |
| FR-ORCH.15-17 | CHARLIE | ⬜ |
| FR-ORCH.18-19 | CHARLIE | ⬜ |

### NFR (Non-Functional)
| Requirement | Stream | Status |
|-------------|--------|--------|
| NFR-P1-P6 (performance) | ALL (verified at integration) | ⬜ |
| NFR-C1-C3 (correctness/ouroboros) | ALL (verified at integration) | ⬜ |
| NFR-S1-S3 (code size) | ALL (verified at integration) | ⬜ |
| NFR-M1-M2 (memory) | ALL (verified at integration) | ⬜ |
| NFR-O1-O2 (observability) | GOLF | ⬜ |
| NFR-SEC1-SEC2 (security) | BRAVO, ECHO | ⬜ |

---

## Risk Register

| Risk | Mitigation Stream | Contingency |
|------|------------------|-------------|
| **R1:** TokenKind enum (102 variants) gets desynchronized between ALPHA and DELTA | ALPHA (source of truth) | ALPHA write token.kn first. DELTA reads it. If mismatch, grep + align. |
| **R2:** AstNode data[] encoding misunderstood by DELTA/FOXTROT/GOLF | DELTA (defines encoding) | DELTA's ast.kn includes test cases for every AstNode kind in spec/ast_spec.md. FOXTROT and GOLF verify against those tests. |
| **R3:** LLVM-C API wrapper functions in GOLF don't match ECHO's type definitions | ECHO + GOLF | ECHO defines the function signatures in llvm_ffi.kn comments. GOLF implements them. Integration gate: `kain check src/llvm_ffi.kn` must pass. |
| **R4:** JIT W^X lifecycle fails on Windows due to page protection | BRAVO | Proven in metal.kn cases 0-5. If fails, check vm_protect constants against runtime/native. |
| **R5:** MarkScript VM API changes between versions | CHARLIE | Pin to markscript 2.0 API surface defined in research doc 07. Use only the 20 public API functions. |
| **R6:** Typechecker stub strategy loses information needed by codegen | FOXTROT | FOXTROT stores ALL parsed data in TypedProgram. Stubs only affect semantic validation, NOT data preservation. |
| **R7:** Ouroboros fails because combined source order affects LLVM IR output | GOLF | GOLF uses deterministic combined source order from KAIN.toml. Order is stable across compilations. |

---

## Notes for Parent Agent

1. **Spawn order matters:** Launch Wave 1 (ALPHA, BRAVO, CHARLIE, ECHO) immediately. These 4 streams have ZERO shared dependencies and can run fully in parallel.

2. **ALPHA gates DELTA:** When ALPHA finishes ALPHA-01 (token.kn) and ALPHA-02 (error.kn), immediately launch DELTA. DELTA doesn't need ALPHA's lexer.kn — it only needs the type definitions.

3. **DELTA gates FOXTROT:** When DELTA finishes DELTA-01 (ast.kn type defs), launch FOXTROT. FOXTROT doesn't need the full parser — just the AST node types.

4. **FOXTROT gates GOLF:** When FOXTROT finishes FOXTROT-01 (types.kn ResolvedType), launch GOLF. GOLF needs the type system to compile.

5. **Integration testing:** After all streams complete, run `kain check src/` from `X:\blades\kain\`. This is the first integration gate.

6. **Watch for shared file conflicts:** The primary risk is ast.kn and llvm_ffi.kn since two streams write to each. Check these files first.

7. **Subagent launch examples:**
```
// Wave 1 (parallel):
Agent(tasks_alpha.md,   "Implement foundation types + lexer for kainc")
Agent(tasks_bravo.md,   "Implement dual JIT engine for kainc")
Agent(tasks_charlie.md, "Implement MarkScript orchestration for kainc")
Agent(tasks_echo.md,    "Implement runtime contract + FFI layer for kainc")

// Wave 2 (after ALPHA token.kn done):
Agent(tasks_delta.md,   "Implement parser + AST for kainc")

// Wave 3 (after DELTA ast.kn done):
Agent(tasks_foxtrot.md, "Implement typechecker + monomorphizer for kainc")

// Wave 4 (after FOXTROT types.kn done):
Agent(tasks_golf.md,    "Implement LLVM codegen + CLI driver for kainc")
```

8. **Completion verification:** When all streams report completion, run the merge & verification plan above.

9. **File location:** All source files go under `X:\blades\kain\src\`. Test specifications go under `X:\blades\kain\spec\`.

---

## Post-Completion

After all streams finish and pass integration:

1. Run `kain check src/` — typecheck the full compiler
2. Run `kain build src/ --target llvm` — produce LLVM IR
3. Run `kain test spec/` — run all test specifications
4. Run `kainc selfhost --verify-ouroboros` — final acceptance
5. Verify all items in the traceability matrix are checked
6. Tag the commit: `git tag kainc-v1.0-selfhost`
