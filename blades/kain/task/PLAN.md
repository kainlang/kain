# Self-Host Compiler — Parallel Agent Deployment Plan

**Date:** 2026-06-12  
**Based on:** 12,792 lines of research (9 documents)  
**Goal:** Build `blades/kain/src/` — a pure-Kain self-hosting compiler  
**Strategy:** 4-6 parallel kain-writer agents per wave, phased by dependency

---

## The 5-Wave Deployment

```
WAVE 0: CLI Shell (1 agent, 2 files — already exists)
  → main.kn, cli.kn, build.kn ✅

WAVE 1: Foundation + JIT (6 agents, 6 files — ALL PARALLEL)
  → driver.kn, platform.kn, context.kn, jit.kn, target.kn, runtime.kn

WAVE 2: Lexer + Parser + AST (7 agents, 7 files — 5 PARALLEL + 2 sequential)
  → lexer.kn, lexer_unicode.kn, literals.kn, ast.kn, pratt_parser.kn (PARALLEL)
  → parser.kn (sequential — depends on ast + pratt)
  → modules.kn (parallel — no hard dep)

WAVE 3: Typechecker (7 agents, 7 files — 5 PARALLEL + 2 sequential)
  → effects.kn, import_c.kn, import_python.kn, import_rust.kn, diagnostics.kn (PARALLEL)
  → types.kn (sequential — depends on effects + ast)
  → monomorphize.kn (parallel to types, needs types.kn structs)

WAVE 4: Codegen (2 agents, 2 files — SEQUENTIAL but single wave)
  → optimizer.kn (parallel — just LLVM pass manager)
  → codegen.kn (sequential — depends on all of types + ast)

WAVE 5: Fusion + Ouroboros (2 agents, 2 files — PARALLEL)
  → orchestrator.kn (markscript fusion integration)
  → selfhost.kn (self-host pipeline orchestrator)

WAVE 6: Bridge + Verification (2 agents, 2 files — PARALLEL)
  → bridge.kn (Rust DLL bridge — Phase 1-3 fallback)
  → build.kn rewrite (Kain project authority for self-host)
```

---

## Total: 27 agents, 27 files, ~14,500 lines

### Wave Dependency Graph

```
WAVE 0  ──────────────────────────────────────────────────────────────
  main.kn ✅  cli.kn ✅  build.kn ✅

WAVE 1  ──────────────────────────────────────────────────────────────
  driver.kn  platform.kn  context.kn  jit.kn  target.kn  runtime.kn

WAVE 2  ──────────────────────────────────────────────────────────────
  lexer.kn  lexer_unicode.kn  literals.kn  ast.kn  pratt_parser.kn
  ↓                         ──────────────────────────┘
  parser.kn ← depends on ast.kn + pratt_parser.kn + lexer.kn
  modules.kn

WAVE 3  ──────────────────────────────────────────────────────────────
  effects.kn  import_c.kn  import_python.kn  import_rust.kn  diagnostics.kn
  ↓
  types.kn ← depends on effects.kn + ast.kn
  monomorphize.kn ← depends on ast.kn + types.kn

WAVE 4  ──────────────────────────────────────────────────────────────
  optimizer.kn
  ↓
  codegen.kn ← depends on types.kn + ast.kn + optimizer.kn

WAVE 5  ──────────────────────────────────────────────────────────────
  orchestrator.kn  selfhost.kn

WAVE 6  ──────────────────────────────────────────────────────────────
  bridge.kn  build.kn
```

---

## Agent Task Files

Each agent gets a task file in this directory specifying:
- The file to write
- The research documents to read
- The reference files to study
- The neighboring files to coordinate with
- The test expectations

### Task File Map

| Wave | Task File | File to Write | Lines | Dependencies |
|------|-----------|---------------|-------|-------------|
| 1 | `task/wave01-a-driver.md` | `src/driver.kn` | ~300 | None |
| 1 | `task/wave01-b-platform.md` | `src/platform.kn` | ~100 | None |
| 1 | `task/wave01-c-context.md` | `src/context.kn` | ~100 | None |
| 1 | `task/wave01-d-jit.md` | `src/jit.kn` | ~300 | None |
| 1 | `task/wave01-e-target.md` | `src/target.kn` | ~100 | None |
| 1 | `task/wave01-f-runtime.md` | `src/runtime.kn` | ~200 | None |
| 2 | `task/wave02-a-lexer.md` | `src/lexer.kn` | ~600 | None |
| 2 | `task/wave02-b-lexer-unicode.md` | `src/lexer_unicode.kn` | ~200 | None |
| 2 | `task/wave02-c-literals.md` | `src/literals.kn` | ~300 | None |
| 2 | `task/wave02-d-ast.md` | `src/ast.kn` | ~500 | None |
| 2 | `task/wave02-e-pratt-parser.md` | `src/pratt_parser.kn` | ~500 | None |
| 2 | `task/wave02-f-parser.md` | `src/parser.kn` | ~3,000 | lexer, ast, pratt |
| 2 | `task/wave02-g-modules.md` | `src/modules.kn` | ~300 | None |
| 3 | `task/wave03-a-effects.md` | `src/effects.kn` | ~200 | None |
| 3 | `task/wave03-b-import-c.md` | `src/import_c.kn` | ~300 | None |
| 3 | `task/wave03-c-import-python.md` | `src/import_python.kn` | ~200 | None |
| 3 | `task/wave03-d-import-rust.md` | `src/import_rust.kn` | ~200 | None |
| 3 | `task/wave03-e-diagnostics.md` | `src/diagnostics.kn` | ~300 | None |
| 3 | `task/wave03-f-types.md` | `src/types.kn` | ~2,000 | effects, ast |
| 3 | `task/wave03-g-monomorphize.md` | `src/monomorphize.kn` | ~500 | ast, types |
| 4 | `task/wave04-a-optimizer.md` | `src/optimizer.kn` | ~200 | None |
| 4 | `task/wave04-b-codegen.md` | `src/codegen.kn` | ~3,000 | types, ast, optimizer |
| 5 | `task/wave05-a-orchestrator.md` | `src/orchestrator.kn` | ~500 | None |
| 5 | `task/wave05-b-selfhost.md` | `src/selfhost.kn` | ~500 | None |
| 6 | `task/wave06-a-bridge.md` | `src/bridge.kn` | ~200 | None |
| 6 | `task/wave06-b-build.md` | `build.kn` | ~50 | None |

---

## Agent Capability Requirements

Each kain-writer agent must:
1. Read the assigned research documents BEFORE writing
2. Read relevant reference files from `X:\blades\kain\reference\`
3. Coordinate with sibling agents (where files have type/symbol dependencies)
4. Write idiomatic Kain following the RULEBOOK.md decision ladder
5. Use `use std::*` imports from the existing Kain stdlib where applicable
6. Produce compilable Kain that passes `kain check`
7. Write doc comments (`///`) on every public item

---

## Key Decision: What The Compiler Uses vs Doesn't

From research docs 01 + 02, the self-host compiler is **Layer 0 code**:

**USES:** `fn`, `struct`, `enum`, `trait`, `impl`, `let`, `mut`, `const`, `if`/`elif`/`else`, `match`, `for`, `while`, `loop`, `break`, `continue`, `return`, `defer`, `ptr<T>`, `collapse`/`observe`/`decay`, `use`, `mod`, `pub`, `include`, `asm`, `Unsafe` effect, `Pure` effect, `IO` effect

**DOES NOT USE:** `world`, `actor`, `converge`, `orchestrate`, `patch`, `law`, `pulse`, `resonate`, `shatter`, `teleport`, `axiom`, `entangle`, `component`, `shader`, `comptime`, `macro`, `test`, `spawn`, `send`, `share`, `fanout`

**EXCEPTION:** `src/orchestrator.kn` and `src/selfhost.kn` may use `world` for compiler state if it simplifies the design. Decision per agent.

---

## Line Count Budgets (Tokei-Verified)

| Wave | Files | Target Total Lines |
|------|-------|-------------------|
| 0 | 3 ✅ | ~1,000 (exists) |
| 1 | 6 | ~1,100 |
| 2 | 7 | ~5,400 |
| 3 | 7 | ~3,700 |
| 4 | 2 | ~3,200 |
| 5 | 2 | ~1,000 |
| 6 | 2 | ~250 |
| **ALL** | **29** | **~15,650** |
