# KN.MD — Kain Self-Host Compiler (kainc) State Document

**Version:** 2.0 | **Date:** 2026-06-12 | **Status:** Authoritative — Single Source of Truth
**Source root:** `X:\blades\kain\src\` — 23 `.kn` files, 15,097 lines | **Master spec:** `research/SELFHOST-KN.MD` (v2.0)
**Overall completion:** ~45% (stubs inflate line count; real functionality ~35%)

---

## 1. KAINC STATE AT A GLANCE

| Subsystem | File | Lines | Real % | Rust Equivalent | Rust Lines | Verdict |
|-----------|------|-------|--------|-----------------|------------|---------|
| **Token** | `token.kn` | 187 | 100% | `ast.rs` partial | ~400 | DONE — 127 constants |
| **Span** | `span.kn` | 56 | 100% | `span.rs` | ~150 | DONE — byte→line:col |
| **AST Constants** | `ast.kn` | 357 | 100% | `ast.rs` | 3,819 | DONE — 38 item, 56 expr, 9 pat |
| **Error/Diag** | `error.kn` | 99 | 60% | `error.rs` + `diagnostics.rs` | 770 | PARTIAL — bag works, no render |
| **Lexer** | `lexer.kn` | 778 | 95% | `lexer.rs` | 583 | DONE — DFA, indent, 127 kinds |
| **Parser** | `parser.kn` | 3,345 | 90% | `parser.rs` | 11,081 | DONE — All 108 keywords, Pratt |
| **Typechecker** | `types.kn` | 2,230 | 55% | `types.rs` + `monomorphize.rs` | 18,201 | PARTIAL — infer_expr mostly real, struct/enum partial, L1-L7 stubs |
| **Effects** | `effects.kn` | 129 | 100% | `effects.rs` | 140 | DONE — 8-effect lattice |
| **Monomorphize** | `monomorphize.kn` | 460 | 20% | `monomorphize.rs` | 2,077 | FAIL — infra exists, loop doesn't instantiate |
| **Codegen** | `codegen.kn` | 2,516 | 40% | `sys-codegen` crates | ~21,000 | PARTIAL — Path A real for ~17/56 expr; Path B 100% stubs |
| **LLVM FFI** | `llvm_ffi.kn` | 670 | 50% | `sys-codegen` llvm-c | ~200 | PARTIAL — types real, wrappers stubbed |
| **Runtime Table** | `runtime.kn` | 550 | 90% | `runtime.rs` | 10,418 | DONE — 132 functions, shadowed by codegen local stub |
| **JIT Metal** | `jit_metal.kn` | 130 | 100% | (none in bootstrap) | 0 | DONE — W^X lifecycle |
| **JIT x86** | `jit_x86.kn` | 515 | 100% | (none) | 0 | DONE — 20-opcode bytecode compiler |
| **JIT Orc** | `jit_orc.kn` | 146 | 0% | (none) | 0 | STUB — all returns -1/false/null |
| **JIT Cache** | `jit_cache.kn` | 113 | 80% | (none) | 0 | PARTIAL — shatter struct violation |
| **JIT Dispatch** | `jit.kn` | 110 | 70% | (none) | 0 | PARTIAL — cache reg broken |
| **Builtins** | `builtins.kn` | 314 | 90% | types.rs scatter | ~500 | DONE — 33 types, 36 functions |
| **Build Config** | `build.kn` | 118 | 50% | `blades/src/` | ~2,400 | PARTIAL — unresolvable `std::markscript` |
| **Orchestrator** | `orchestrator.kn` | 897 | 60% | (markscript only) | 0 | PARTIAL — 9 handlers: 6 real, 3 stub |
| **Compiler Driver** | `compiler.kn` | 541 | 60% | `driver/src/` | 5,370 | PARTIAL — pipeline wired, local stubs |
| **CLI** | `cli.kn` | 402 | 70% | `cli/src/` | 7,882 | PARTIAL — 12 subcommands, dispatch stubs |
| **Entry** | `main.kn` | 233 | 80% | `kain.rs` | ~200 | PARTIAL — version banner, dispatch |

---

## 2. RUST FRONTEND MAP — Crate Elimination

### Crate Line Counts & Kainc Equivalents

| Rust Crate | Lines | Purpose | Kainc File(s) | Kainc Lines | % | What's Missing |
|------------|-------|---------|---------------|-------------|---|----------------|
| `crates/core` | 64,942 | Lexer, parser, AST, types, effects, runtime contract, monomorphize, error, span, module resolution | `lexer.kn`, `parser.kn`, `ast.kn`, `token.kn`, `types.kn`, `effects.kn`, `monomorphize.kn`, `error.kn`, `span.kn`, `builtins.kn`, `runtime.kn` | 9,373 | 60% | L1-L7 typecheck, module resolution, generic instantiation |
| `crates/sys-codegen` | ~21,000 | LLVM IR emission (text + LLVM-C) | `codegen.kn`, `llvm_ffi.kn` | 3,186 | 30% | LLVM-C Path B real, struct field types, string ABI, 30+ expr kinds |
| `crates/cli` | 7,882 | CLI arg parsing, subcommands | `cli.kn`, `main.kn` | 635 | 20% | Subcommand wiring, JSON output, `kain selfhost` |
| `crates/driver` | 5,370 | DriverSession, compilation pipeline | `compiler.kn` | 541 | 20% | Real pipeline (local stubs shadow real implementations) |
| `crates/error` | ~5,000 | Diagnostic engine, render, JSON, TOML registry | `error.kn` (planned: `diagnostics.kn`, `diagnostics_render.kn`) | 99 | 2% | Renderer, JSON output, registry, codes |
| `crates/amalgamate` | 2,770 | Capsule pack/inspect/unpack | (planned: `amalgamate.kn`) | 0 | 0% | Not started — port plan complete |
| `crates/c-ffi` | 6,500 | libclang header extraction | `llvm_ffi.kn` (include directives), (planned: `import_c.kn`) | 670 | 10% | libclang extraction delegated to Rust; self-host uses inline type aliases |
| `crates/monomorphize` | 2,077 | Generic instantiation | `monomorphize.kn` | 460 | 20% | Instantiation loop doesn't call `instantiate_generic()` |
| `crates/actor` | ~4,000 | Actor typechecking, lowering | (none — GOLD stream, deferred) | 0 | 0% | Not started |
| `crates/ownership` | ~3,000 | Collapse/observe/decay typechecking | (none — GOLD stream, deferred) | 0 | 0% | Not started |
| `crates/gpu` | ~3,000 | Shader codegen (SPIR-V, PTX, HLSL, WGSL) | (none — GOLD stream, deferred) | 0 | 0% | Not started |
| `crates/blades` | 2,462 | Workspace discovery | `compiler.kn` (`discover_workspace`) | ~30 | 20% | Basic directory ascent works; blade resolution missing |

**43 of 67 crates eliminated** from self-host compiler scope (64% by count, remainder are stdlib/runtime/GPU/UE5/LSP that stay in Rust or are deferred).

---

## 3. DECISION LADDER IMPLEMENTATION STATUS

### LAYER 0: PLAIN CODE — fn, struct, enum, trait, impl, let, match, if, for, while

| Subsystem | Typecheck | Codegen | Notes |
|-----------|-----------|---------|-------|
| `fn` | 100% REAL | 70% | Func sigs checked, bodies walked. Codegen real for calls, missing return-type resolution |
| `struct` | 70% | 40% | Field types resolved, duplicates caught. Codegen: fields all `i64` |
| `enum` | 50% | 60% | Variants registered, no lookup map. Codegen: tag+payload store works |
| `trait` + `impl` | 30% | N/A | Trait methods registered. Impl doesn't verify trait contract |
| `let` / `var` / `mut` | 90% | 80% | Type-inferred. Codegen: alloca+store/load pattern works |
| `if` / `elif` / `else` | 100% | 90% | Branch+phi merge works. Both arms compiled |
| `match` | 60% | 50% | First-arm-only type inference. Codegen: tag dispatch + phi, no pattern binding |
| `for` | 60% | 20% | Loop context tracked. Codegen: structure correct, bound hardcoded `10` |
| `while` | 90% | 80% | Header+body+exit blocks, loop stack |
| `loop` / `break` / `continue` | 70% | 40% | Loop depth tracking real. Codegen: branches work, break value ignored, block-level routing broken |
| `return` / `defer` | 80% | 70% | Return type checked. Codegen: `ret` works, defer not emitted |
| Binary ops | 90% | 80% | All 21 ops typed. Codegen: all integer, missing type variance |
| Unary ops | 80% | 70% | 6 ops typed. Codegen: real |
| Call | 60% | 20% | Args checked. Codegen: always `call i64` regardless of actual return type |
| Method call | 30% | 20% | No receiver-type resolution. Codegen: synthetic names, no resolution |
| Array lit | 20% | 80% | No element type inference. Codegen: alloca+gep+store real, all `i64` |
| Lambda | 10% | 0% | Returns `rt_i64()` placeholder. Codegen: returns `i64 0` |
| Index | 40% | 70% | Type inferred. Codegen: GEP+load real, element always `i64` |
| Field access | 80% | 50% | Field name resolved. Codegen: name→index walk works (O(n) AST scan), loads `i64` |
| String | 60% | 0% | Type inferred. Codegen: always returns `@.str.empty`, no fat pointer ABI |
| Cast | 80% | 50% | Typed. Codegen: real for int↔float, missing others |
| And/Or | 90% | 100% | Typed Bool. Codegen: short-circuit + phi real |
| Spawn/Send | 0% stub | 0% | Both stubs. Spawn returns `i64 0`. Send doesn't exist |
| Collapse/Observe/Decay | 0% stub | 0% | All return placeholder types. No codegen functions |

### LAYER 1: STATE AUTHORITY — world, entangle, surface, single_writer

| Construct | Typecheck | Codegen | Verdict |
|-----------|-----------|---------|---------|
| `world` | STUB `check_world_stub` — returns TypedItem with `rt_struct_as(name_idx)`, no field/surface validation | NOT EXIST — no `emit_world_globals` | FAIL |
| `entangle` | STUB `check_entangle_stub` — no `check_entangle_item` exists | NOT EXIST — no `emit_entangle_sync` | FAIL |
| `surface` | STUB (parsed by parser, not checked) | NOT EXIST | FAIL |

### LAYER 2: STATE INTEGRITY — patch, law

| Construct | Typecheck | Codegen | Verdict |
|-----------|-----------|---------|---------|
| `patch` | STUB `check_patch_law_stub` — ret `rt_i64()`, no body checking, no journal tracking | NOT EXIST | FAIL |
| `law` | STUB `check_patch_law_stub` — ret `rt_bool()`, no predicate validation | NOT EXIST | FAIL |

### LAYER 3: DISPATCH — converge, spec, fast, verify

| Construct | Typecheck | Codegen | Verdict |
|-----------|-----------|---------|---------|
| `converge` | STUB `check_converge_stub` — returns minimal TypedItem, no lane validation | NOT EXIST — no `compile_converge_textual` | FAIL |

### LAYER 4: STAGE GRAPH — orchestrate, stage, deps, residency, transfer

| Construct | Typecheck | Codegen | Verdict |
|-----------|-----------|---------|---------|
| `orchestrate` | STUB `check_orchestrate_stub` — no stage dep/residency/transfer checking | NOT EXIST | FAIL |

### LAYER 5: TEMPORAL — pulse, resonate, dampen

| Construct | Typecheck | Codegen | Verdict |
|-----------|-----------|---------|---------|
| `pulse` | STUB via `check_pulse_resonate_stub` | NOT EXIST | FAIL |
| `resonate` | STUB via `check_pulse_resonate_stub` | NOT EXIST | FAIL |

### LAYER 6: MACHINE STONES — axiom, shatter, teleport

| Construct | Typecheck | Codegen | Verdict |
|-----------|-----------|---------|---------|
| `axiom` | STUB `check_axiom_stub` — no `check_axiom_item` | NOT EXIST | FAIL |
| `shatter` | NOT EXIST — no `check_shatter_item` | NOT EXIST | FAIL |
| `teleport` | STUB (no dedicated checker, parsed by parser) | NOT EXIST — no `compile_teleport_textual` | FAIL |

### LAYER 7: SYSTEMS — actor, collapse, observe, decay, spawn, send, share, fanout

| Construct | Typecheck | Codegen | Verdict |
|-----------|-----------|---------|---------|
| `actor` | NOT EXIST — no `check_actor_item` | NOT EXIST — no `emit_actor_dispatch`, `compile_send_textual` | FAIL |
| `collapse`/`observe`/`decay` | STUB — infer returns placeholder types | NOT EXIST — no ownership codegen functions | FAIL |
| `share`/`fanout` | STUB | NOT EXIST | FAIL |

### COMPONENTS & UI — component, render, JSX

| Construct | Typecheck | Codegen | Verdict |
|-----------|-----------|---------|---------|
| `component` | NOT EXIST — no `check_component_item` | NOT EXIST — no `compile_component_textual` | FAIL |

### GPU & SHADERS — shader, vertex, fragment, compute, dispatch

| Construct | Typecheck | Codegen | Verdict |
|-----------|-----------|---------|---------|
| `shader` (all variants) | STUB `check_shader_stub` — no uniform/workgroup validation | NOT EXIST — no GPU codegen | FAIL |

---

## 4. STREAM STATUS

### RED — Typechecker Completion
| Task | Status | Notes |
|------|--------|-------|
| RED-1: Thread TypeEnv through check_item | **PASS (90%)** | TypedItemAndEnv exists, all check_* return it, pass4 threads env. `push_type_slot` is a no-op (cosmetic). |
| RED-2: check_struct_item real | **PARTIAL (80%)** | Field type resolution + duplicate detection real. Generic params NOT registered. Field info orphaned. |
| RED-3: check_enum_item real | **PARTIAL (83%)** | Variant payload types resolved. No variant-name-to-payload lookup map. |
| RED-4: check_trait_impl_item real | **PARTIAL (87%)** | Trait methods registered. Impl does NOT verify trait methods present or check signatures. |
| RED-5: infer_expr_type complete | **PARTIAL (82%)** | 57/57 expr kinds handled. 4 return `rt_unknown()`. Many return simplified types. Match only uses first arm. Method call no receiver-type dispatch. |
| RED-6: types_compatible complete | **PARTIAL (85%)** | 17/20 variant combinations handled. Generic escape valve returns true without substitution. No struct-to-trait. |
| RED-7: check_block_body for match/loop/break/continue | **PASS (with caveats)** | Loop context + break/continue validation real. Break value type not checked. |
| RED-8: Generic monomorphization | **FAIL (37%)** | `monomorphize()` loop detects generics but NEVER calls `instantiate_generic()`. `unify()` is real but unreachable. |

### GREEN — Ouroboros Pipeline
| Task | Status | Notes |
|------|--------|-------|
| GREEN-1: llvm_ffi.kn conditional includes | **PARTIAL (83%)** | `include` directives commented out (manual toggle). `HAS_LLVM_HEADERS=0` defined but no `if` branching. `llvm_stub_types.kn` (201 lines) exists but orphaned. |
| GREEN-2: handler_selfhost_phase1 (source combine) | **PASS (87%)** | REAL: reads 23 files, combines with separators, writes `kainc_bootstrap.kn`, runs compile check. Hardcoded source order (not parsed from KAIN.toml). |
| GREEN-3: handler_selfhost_phase2 (self-compile) | **PASS (100%)** | REAL: stage1 compilation → link → stage2 execution via `os_system` → byte-by-byte comparison. Stub detection returns code 2 when IR is stub output. |
| GREEN-4: handler_build_link | **PASS (100%)** | REAL: scans `.ll` files, constructs clang command, `os_system` invocation, error reporting. |
| GREEN-5: handler_build_package | **PASS (100%)** | REAL: orchestrates check → codegen → link. No manifest generation (deferred). |
| GREEN-6: Workspace discovery | **PASS (100%)** | REAL: directory ascent for KAIN.toml/build.kn/.git. `compile_workspace`/`check_workspace` real with line-by-line TOML parser. |
| GREEN-7: KAIN.toml complete | **PASS (100%)** | REAL: 23 files in `[source_order]`, full `[selfhost]` section, runtime manifest path, package metadata. |

### BLUE — Codegen Completion
| Task | Status | Evidence |
|------|--------|----------|
| BLUE-1: Real struct type defs | **PARTIAL** | Field count correct, all fields hardcoded `i64`, no zero-field support |
| BLUE-2: Match codegen | **PARTIAL** | Tag dispatch + phi real, no pattern variable binding |
| BLUE-3: For-range loop codegen | **PARTIAL** | While-loop structure works, bound hardcoded to `10` |
| BLUE-4: Loop/break/continue codegen | **PARTIAL** | Branches work, break value ignored, **block-level routing broken** (FOR/LOOP/BREAK/CONTINUE not routed from `compile_block_textual` to `compile_stmt_textual`) |
| BLUE-5: Array literal codegen | **PASS** | Alloca + GEP + store chain complete |
| BLUE-6: Enum variant construction | **PASS** | Tag + payload store works |
| BLUE-7: Lambda/closure codegen | **FAIL** | Returns `i64 0` with "lambda stub" comment |
| BLUE-8: Method call codegen | **PARTIAL** | Compiles args + emits call, no method resolution |
| BLUE-9: And/Or short-circuit | **PASS** | Correct short-circuit with phi merge |
| BLUE-10: Index expression | **PASS** | GEP + load, element always `i64` |
| BLUE-11: Field access mapping | **PARTIAL** | Field name→index real, O(n) AST walk, always loads `i64` |
| BLUE-12: Runtime declares | **PARTIAL** | `runtime.kn` has 132 declares. `codegen.kn` LOCAL `runtime_table_init()` shadow returns EMPTY table → 17 fallback declares. |
| BLUE-13: String constants | **FAIL** | Globals emitted, expression ALWAYS returns `@.str.empty` pointer |
| BLUE-14: Call with real signatures | **FAIL** | Always `call i64`, no TypedItem lookup |
| BLUE-15: Spawn/Send codegen | **FAIL** | Spawn returns `i64 0` stub, send doesn't exist |

### GOLD — L1-L7 Stub→Real
| Category | Status | Note |
|----------|--------|------|
| GOLD-TC: All 8 typecheck stubs | **FAIL (0/8)** | All still stubs. `check_actor_item`, `check_component_item`, `check_entangle_item`, `check_axiom_item`, `check_shatter_item` — none exist |
| GOLD-CG: All 7 codegen targets | **FAIL (0/7)** | No world/actor/ownership/converge/orchestrate/pulse/GPU codegen functions exist |
| GOLD-INT: Pipeline wiring | **FAIL** | Only `AST_EXPR_SPAWN` has a stub dispatch. Zero L1-L7 sections in `codegen_textual` |

---

## 5. KNOWN BLOCKERS (Ranked)

| # | Blocker | File | Stream | Effort | Details |
|---|---------|------|--------|--------|---------|
| **1** | Monomorphize doesn't instantiate generics | `monomorphize.kn:205-230` | RED | 3-4 weeks | `monomorphize()` detects generics but never calls `instantiate_generic()`. `Array<T>` etc. pass through as templates. Blocks ouroboros for any generic usage. |
| **2** | Codegen block-level statement routing broken | `codegen.kn:1468-1507` | BLUE | 1 day | `compile_block_textual` only routes LET/RETURN/EXPR/WHILE to `compile_stmt_textual`. FOR/LOOP/BREAK/CONTINUE fall to unknown expr → `add i64 0, 0`. Breaks most control flow in function bodies. |
| **3** | Call return type always `i64` | `codegen.kn:1203` | BLUE | 1 week | `let ret_ty: String = "i64"` hardcoded. No TypedItem lookup. All functions return i64 regardless of actual type. |
| **4** | String constants always empty | `codegen.kn:1120-1126` | BLUE | 3 days | Globals are emitted but expression compiler always returns `@.str.empty`. No fat pointer ABI. |
| **5** | All types hardcoded to `i64` | `codegen.kn` (systemic) | BLUE | 2-3 weeks | Struct fields, array elements, GEP types, loads, returns all use `i64`. String/Float/Bool/struct types produce incorrect LLVM IR. |
| **6** | Runtime table shadowed by local stub | `codegen.kn:44-46` | BLUE | 1 day | Local `runtime_table_init()` returns empty table. Real 132-entry table in `runtime.kn` only works at combine time. 17-declare fallback used in isolation. |
| **7** | `std::markscript` import unresolvable | `build.kn`, `orchestrator.kn` | GREEN | 1 week | Module doesn't exist in stdlib. Blocks `kain check` of both files. |
| **8** | Lambda returns `i64 0` (not even a pointer) | `codegen.kn:2400-2408` | BLUE | 2 weeks | Crash risk — returns literal zero instead of function pointer. Needs closure struct + trampoline. |
| **9** | No codegen MonomorphizedProgram access | `codegen.kn` (compiler fns) | BLUE | 1 week | Expr compilers only get `AstNode` + `ast_nodes`, never `MonomorphizedProgram`. Can't look up TypedItem field maps, function sigs. |
| **10** | `jit_cache.kn` uses `shatter struct` (L6 violation) | `jit_cache.kn` | Phase 1-4 | 1 day | Violates "Layer 0 only" constraint. Must change to plain `struct`. |

---

## 6. JIT SUBSYSTEM

### Path A — x86-64 Direct (proven, working)
- **File:** `jit_x86.kn` (515 lines) + `jit_metal.kn` (130 lines)
- **Status:** FUNCTIONAL for raw bytecode. Can compile + execute 20 opcodes via W^X pipeline.
- **Proven by:** `benchmark/cases_v2/metal.kn` (17 self-tests, deterministic checksums)
- **W^X contract:** STRICT (RW→RX, better than markscript's RWX)
- **Limitations:** No AST-to-bytecode compiler. Cache can't register entries (`jit_compile_and_run` returns Int, not code pointer). No self-tests in src/. Fixed register allocation (RAX, RBX, RBP). 3/23 markscript opcodes skipped.

### Path B — OrcJIT (conceptual only)
- **File:** `jit_orc.kn` (146 lines)
- **Status:** 100% STUB. Every function returns null/false/-1. `has_llvm` hardcoded `false`.
- **Blockers:** 25+ OrcJIT wrapper functions missing in `llvm_ffi.kn`. Codegen must produce in-memory LLVM module (not just text). LLVM.DLL runtime dependency (~50MB).

### Cache
- **File:** `jit_cache.kn` (113 lines)
- **Design:** `shatter struct CacheStore` (VIOLATION — must be `struct`). SoA layout, linear scan, simple hash (first 8 bytes). No collision resolution, no eviction, no LRU.
- **Critical bug:** `jit_execute_cached` has TODO — can't register entries because `jit_compile_and_run` returns Int, not code pointer.

### Dispatcher
- **File:** `jit.kn` (110 lines)
- **Auto-select:** Tries Path B → falls through to Path A (Path B alway returns unavailable). Correct architecture.

---

## 7. OUROBOROS PIPELINE

### Phase 1: Source Concatenation
- **Handler:** `handler_selfhost_phase1` in `orchestrator.kn:508-549`
- **Status:** REAL. Reads 23 files in `SOURCE_ORDER`, combines with `// ── BEGIN/END: <file> ──` separators, writes to `.selfhost/bootstrap/combined/kainc_bootstrap.kn` (681KB).
- **Issue:** Hardcoded source order (not parsed from KAIN.toml). Functionally identical.

### Phase 2: Self-Compile + Verify
- **Handler:** `handler_selfhost_phase2` in `orchestrator.kn:558-650`
- **Status:** REAL. Stage1: compile combined source → `.ll` → clang link → `.exe`. Stage2: run kainc.exe on combined source → `stage2.ll`. Compare byte-by-byte.
- **Stub detection:** Returns code 2 if IR is stub output (< 10 meaningful lines).
- **Missing:** Metadata stripping (would cause false diffs with real codegen).

### Current Ouroboros Blockers
1. Monomorphize doesn't instantiate → generic code produces wrong IR
2. Codegen block-level routing broken → most control flow silent no-ops
3. Call always `i64` → type mismatches in LLVM IR
4. String constants always empty → string-using code broken
5. All types `i64` → Float/String/Bool produce incorrect IR

---

## 8. PORT QUEUE

### Amalgamate Port
- **Research:** `research/PORT_AMALGAMATE.md` — complete, ~1,920 estimated Kain lines
- **Status:** Not started. Port plan: single file `amalgamate.kn` (Layer 0 only), 7 phases, ~15 days estimated. TOML→JSON format migration, dual-format backward compat, no zstd compression.
- **Integration:** CLI subcommand + orchestrator handler + auto-materialize in compiler pipeline.

### Error/Diagnostics Port
- **Research:** `research/PORT_ERROR.md` — complete, ~1,000 estimated Kain lines
- **Status:** Not started. 4 files planned: `diagnostics.kn`, `diagnostics_render.kn`, `diagnostics_builders.kn`, `diagnostics_registry.kn`. Phase 1-2: core types + renderer (480 lines). Phase 3: JSON output + registry (130 lines). Deferred: semantic coprocessor, TOML codegen.
- **Current:** `error.kn` (99 lines) is a minimal bag — no rendering, no JSON, no source display.

---

## 9. FILE MANIFEST

| # | File | Lines | Stream | Real % | Purpose |
|---|------|-------|--------|--------|---------|
| 1 | `token.kn` | 187 | N/A (foundation) | 100% | 127 TokenKind constants, Token struct, `token_to_string` |
| 2 | `error.kn` | 99 | N/A (foundation) | 60% | KcDiagnostic, KcDiagnosticBag, 33 error codes |
| 3 | `span.kn` | 56 | N/A (foundation) | 100% | Span struct, line:col computation |
| 4 | `ast.kn` | 357 | N/A (foundation) | 100% | 38 item, 12 stmt, 56 expr, 9 pattern, 14 type AST, 21 binop, 6 unary tag constants. Flat AstNode struct. StringTable. |
| 5 | `build.kn` | 118 | GREEN | 50% | Config key constants, metadata helpers. `std::markscript` import unresolvable. |
| 6 | `lexer.kn` | 778 | N/A (core) | 95% | Hand-written DFA lexer, indent processor, 127 token kinds, full literal parsing |
| 7 | `builtins.kn` | 314 | N/A (core) | 90% | 27 primitive types, 36 builtin functions, LLVM type mapping, `BUILTIN_UNSAFE_NAMES` |
| 8 | `runtime.kn` | 550 | BLUE | 90% | 132 runtime function entries, 11 categories, `emit_runtime_declares`, C type mapping |
| 9 | `llvm_ffi.kn` | 670 | GREEN/BLUE | 50% | 17 LLVM-C type aliases, 70+ wrapper functions (all stubs). `include` directives commented out. |
| 10 | `jit_metal.kn` | 130 | N/A (JIT) | 100% | W^X lifecycle: vm_map → write → protect RX → flush → fence → asm trampoline. STRICT W^X. |
| 11 | `jit_x86.kn` | 515 | N/A (JIT) | 100% | Direct x86-64 emission: 20 opcodes, 2-pass jump fixup, RBP-relative operand stack |
| 12 | `jit_orc.kn` | 146 | N/A (JIT) | 0% | OrcJIT binding — ALL stubs. Returns false/null/-1. `has_llvm` hardcoded false. |
| 13 | `jit_cache.kn` | 113 | N/A (JIT) | 80% | CacheStore (shatter struct VIOLATION), linear scan, simple 8-byte hash |
| 14 | `jit.kn` | 110 | N/A (JIT) | 70% | Path dispatcher. Auto-select tries Path B, falls through to Path A. Cache registration broken. |
| 15 | `parser.kn` | 3,345 | N/A (core) | 90% | Recursive descent + Pratt. All 108 keywords. L1-L7 item parsers. JSX. Pattern parsing. ~20 item parsers. |
| 16 | `types.kn` | 2,230 | RED | 55% | 4-pass pipeline. 20 ResolvedType variants. Infer for 57 expr kinds (4 unknown, many simplified). check_function_item real. Struct/enum partial. 9 L1-L7 stub checkers. |
| 17 | `effects.kn` | 129 | N/A (core) | 100% | 8-effect lattice (Pure→IO/GPU/Async/Reactive/Alloc/Panic→Unsafe). `can_call()` 4-rule. |
| 18 | `monomorphize.kn` | 460 | RED | 20% | `unify()` real but unreachable. `instantiate_generic()` never called. Loop passes through generics. |
| 19 | `codegen.kn` | 2,516 | BLUE | 40% | Path A textual .ll: 17/56 expr compilers real. Path B stubs (100%). Struct defs all i64. Local RuntimeTable shadow. Block-level routing bug. |
| 20 | `orchestrator.kn` | 897 | GREEN | 60% | 9 IVT handlers: 6 real (phase1, phase2, build_link, build_package, compile_check, compile_codegen), 3 stubs. BuildConfig. `std::markscript` unresolvable. |
| 21 | `compiler.kn` | 541 | GREEN | 60% | DriverSession 7-phase pipeline. `discover_workspace` real. `compile_workspace`/`check_workspace` real. LOCAL stubs shadow real implementations. |
| 22 | `cli.kn` | 402 | GREEN | 70% | 12 subcommands. `parse_args` real. Dispatch stubs (delegates to orchestrator). |
| 23 | `main.kn` | 233 | N/A (entry) | 80% | Entry point. `version_string`, `print_banner`. Arg routing. |
| — | `llvm_stub_types.kn` | 201 | GREEN | 100% (orphaned) | Type alias definitions for LLVM-C. NOT imported by `llvm_ffi.kn`. Exists but unused. |

**Total:** 23 `.kn` files, 15,097 lines (+ 1 orphaned stub at 201 lines)

---

## 10. QUICK REFERENCE

### Compile & Check
```powershell
kain check src/                        # Typecheck all 23 files
kain build . --target llvm             # Compile whole workspace to LLVM IR
kain run . --target llvm               # Compile + execute
```

### Ouroboros Pipeline
| Phase | Handler ID | Description |
|-------|-----------|-------------|
| Combine | 204 (`handler_selfhost_phase1`) | Concatenate 23 files → `kainc_bootstrap.kn` |
| Compile+Verify | 205 (`handler_selfhost_phase2`) | Compile → link → self-compile → diff IR |

### Key Files — Never Touch Without Coordination
| File | Owner | Reason |
|------|-------|--------|
| `types.kn` | RED stream | Typechecker core — all streams depend on TypedItem shape |
| `codegen.kn` | BLUE stream | Path A real, Path B stubs — BLUE owns expression compilers |
| `parser.kn` | STABLE | Parser is done — 3,345 lines, all 108 keywords. Do not modify. |
| `lexer.kn` | STABLE | Lexer is done — 778 lines, DFA + indent. Do not modify. |
| `orchestrator.kn` | GREEN stream | Handler wiring, source order, markscript integration |
| `monomorphize.kn` | RED stream | Generic instantiation — critical for ouroboros |

### Stream Ownership Map
```
RED (Typechecker):         types.kn, monomorphize.kn
GREEN (Ouroboros):         orchestrator.kn, compiler.kn, KAIN.toml, llvm_ffi.kn, cli.kn, main.kn
BLUE (Codegen):            codegen.kn, runtime.kn
GOLD (L1-L7, DEFERRED):    types.kn (L1-L7 sections), codegen.kn (L1-L7 sections)
FOUNDATION (STABLE):       token.kn, error.kn, span.kn, ast.kn, lexer.kn, parser.kn, effects.kn, builtins.kn
JIT (STABLE):              jit_metal.kn, jit_x86.kn, jit_orc.kn, jit_cache.kn, jit.kn
UNOWNED (port planned):    amalgamate.kn (PORT_AMALGAMATE.md), diagnostics*.kn (PORT_ERROR.md)
```

### Construct Violations (Phase 1-4)
1. **`jit_cache.kn`**: Uses `shatter struct` (Layer 6) — must be plain `struct`
2. **`build.kn` + `orchestrator.kn`**: Import `std::markscript` — module doesn't exist in stdlib

### Dependency Graph
```
Wave 1 (parallel):
  RED ─────────────────────┐
  GREEN ───────────────────┤
                           │
Wave 2 (after RED):        │
  BLUE ────────────────────┘ ← consumes TypedProgram from RED
                           │
Wave 3 (deferred):         │
  GOLD ────────────────────┘ ← needs real L0 typecheck + codegen foundation
```

### Source Order (Ouroboros Concatenation)
1. token.kn → error.kn → span.kn → ast.kn → build.kn
2. lexer.kn → builtins.kn → runtime.kn
3. llvm_ffi.kn → jit_metal.kn → jit_x86.kn → jit_orc.kn → jit_cache.kn → jit.kn
4. parser.kn
5. types.kn → effects.kn → monomorphize.kn
6. codegen.kn
7. orchestrator.kn → compiler.kn
8. cli.kn → main.kn

First definition of any symbol wins at combine time. Later duplicates are shadowed. This is critical for `runtime_table_init()` (real in runtime.kn, stub in codegen.kn) and all duplicated types in compiler.kn/orchestrator.kn.

### Master Research Docs
| Doc | Purpose |
|-----|---------|
| `research/SELFHOST-KN.MD` | Master specification (v2.0, 1,727 lines) |
| `research/00-AGENT_SYNTHESIS.md` | Cross-document synthesis |
| `research/GAP_ANALYSIS_SRC_VS_CRATES.md` | src vs crates gap analysis |
| `research/COMPILER_IMPL_AUDIT.md` | Per-file implementation audit |
| `research/JIT_STATE_ASSESSMENT.md` | JIT subsystem state |
| `research/VERIFY_RED_GREEN.md` | RED+GREEN task verification |
| `research/VERIFY_BLUE_GOLD.md` | BLUE+GOLD task verification |
| `research/PORT_AMALGAMATE.md` | Amalgamate port plan |
| `research/PORT_ERROR.md` | Error/diagnostics port plan |

### Task Specs
| Spec | Stream | Status |
|------|--------|--------|
| `spec/tasks_NEXT.md` | Master plan | Sprint 2 active |
| `spec/tasks_RED.md` | Typechecker | In progress (partially verified) |
| `spec/tasks_GREEN.md` | Ouroboros | Verified (all tasks PASS) |
| `spec/tasks_BLUE.md` | Codegen | In progress (4 PASS, 5 PARTIAL, 6 FAIL) |
| `spec/tasks_GOLD.md` | L1-L7 | Deferred (0/22 started) |

---

*End of KN.MD — v2.0, 2026-06-12*
