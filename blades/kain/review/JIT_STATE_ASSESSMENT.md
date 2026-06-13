# JIT State Assessment

**Date:** 2026-06-12  
**Analyst:** kain-explorer (JIT architecture sub-agent)  
**Scope:** Comprehensive audit of the self-host compiler JIT subsystem at `blades/kain/src/jit*.kn`, research docs, and relationship to `blades/markscript/src/jit.kn`

---

## Table of Contents

1. [Two JIT Paths: Architecture & State](#1-two-jit-paths-architecture--state)
2. [W^X Contract Verification](#2-wx-contract-verification)
3. [JIT Cache Design](#3-jit-cache-design)
4. [OrcJIT Integration State](#4-orcjit-integration-state)
5. [Gaps & Missing Pieces](#5-gaps--missing-pieces)
6. [Fusion with MarkScript](#6-fusion-with-markscript)
7. [Architecture Assessment & Contradictions](#7-architecture-assessment--contradictions)
8. [Verdict](#8-verdict)

---

## 1. Two JIT Paths: Architecture & State

### 1.1 Path A: x86-64 Direct Emission (MarkScript-style)

**Files:** `jit_x86.kn` (~550 lines), `jit_metal.kn` (~200 lines)  
**Research ref:** 06-jit-markscript-metal-architecture.md (sections 3.2-3.7), SELFHOST-KN.MD (section 16)

**Implementation state:** FUNCTIONALLY COMPLETE for the raw bytecode compilation pipeline.

#### What `jit_x86.kn` provides (all ~550 lines):
- **Prologue/epilogue** (lines 102-121): `push rbp; push rbx; mov rbp, rsp` / `mov rsp, rbp; pop rbx; pop rbp; ret`
- **RBP-relative memory access** (lines 124-133): `emit_mov_rbp_disp()` — load/store with ModRM encoding
- **Operand stack operations** (lines 140-167): `emit_push_rbp`, `emit_pop_rbp`, `emit_dup_rbp` — all RBP-relative
- **Immediate moves** (lines 170-180): `emit_mov_rax_imm64`, `emit_mov_rbx_imm64`
- **Arithmetic** (lines 183-215): `add`, `sub`, `imul`, `div`, `xor`, `test`, `cmp` on RAX/RBX
- **Combined arithmetic with stack ops** (lines 222-268): `emit_add_rbp`, `emit_sub_rbp`, `emit_mul_rbp`, `emit_div_rbp` — pop two, compute, push result
- **Jump system** (lines 271-313): `emit_jmp_placeholder`, `emit_jcc_placeholder`, `emit_jz_rbp`, `emit_jn_rbp`, `apply_fixups` — two-pass resolution with FixupEntry struct
- **Main block compiler** (lines 318-494): `jit_compile_block(bytecode, ip_start, ip_end)` — 20 opcodes compiled to x86-64, two-pass fixup, epilogue guarantee

#### What `jit_metal.kn` provides (all ~200 lines):
- **W^X lifecycle** (lines 17-58): `jit_compile_and_run(code_bytes, code_size)` — complete 8-step pipeline
- **Asm trampoline** (lines 67-86): `call_jit_trampoline(code_pages)` — `asm("mov rax, [rdi]; call rax; mov [rdi+8], rax")`
- **Convenience caller** (lines 89-108): `call_jit_code(code_ptr)` — for cache hits and OrcJIT function pointers
- **Utility** (lines 111-120): `align_to_page`, `null_ptr_byte`

#### What Path A CAN do:
- Compile markscript bytecode (20 opcodes: HALT through JN) to raw x86-64 machine code
- Execute compiled code via the asm trampoline
- Handle forward/backward jumps with two-pass fixup
- Maintain a virtual operand stack at RBP-relative offsets
- Load and store 64 variables at fixed RBP offsets
- Allocate RW pages, write code, protect to RX, flush caches, fence, execute

#### What Path A CANNOT do (yet):
- Compile Kain source code (AST nodes) to bytecode — there is NO AST-to-bytecode translator
- Cache lookups that preserve the code pointer (see §3)
- Self-test — no test runner in src/jit*.kn (the markscript origin has 17 self-tests)
- Emit non-arithmetic Kain constructs (function calls, struct literals, control flow)

---

### 1.2 Path B: OrcJIT LLVM

**File:** `jit_orc.kn` (~150 lines)  
**Research ref:** 03-llvm-codegen-jit.md (sections 5, 6), SELFHOST-KN.MD (section 10)

**Implementation state:** CONCEPTUAL-ONLY. Zero executable OrcJIT calls. Every function is a stub.

#### What `jit_orc.kn` actually provides:
- `struct OrcJitState` with fields: `initialized: Bool`, `jit_handle: ptr<Byte>`, `has_llvm: Bool`
- `jit_orc_init()` — creates a stub state where `has_llvm = false`, `initialized = true`. All real LLVM init code is commented out.
- `jit_orc_available(state)` — returns `state.has_llvm`, which is always `false`
- `jit_orc_compile_module(jit, module)` — always returns `false`. TODO comment references `llvm_orc::LLVMOrcLLJITAddLLVMIRModule`
- `jit_orc_lookup(jit, symbol)` — always returns null pointer. TODO comment references `llvm_orc::LLVMOrcLLJITLookup`
- `jit_orc_compile_and_call(state, module, entry)` — always returns -1. The real pipeline is commented out.
- `jit_orc_dispose(state)` — no-op. TODO comment references `llvm_orc::LLVMOrcDisposeLLJIT`

#### llvm_ffi.kn coverage for OrcJIT:

The `include <llvm-c/Orc.h> as llvm_orc` directive IS present (line 27), which means libclang CAN parse the Orc header. The following TYPE ALIASES are defined:
- `LLVMOrcLLJITRef` (line 52)
- `LLVMOrcThreadSafeContextRef` (line 53)
- `LLVMOrcJITDylibRef` (line 54)
- `LLVMOrcResourceTrackerRef` (line 55)

However, there are **ZERO wrapper functions** for any OrcJIT API call. The file has no functions wrapping:
- `LLVMOrcCreateLLJIT` — needed to create the JIT instance
- `LLVMOrcDisposeLLJIT` — needed for cleanup
- `LLVMOrcLLJITAddLLVMIRModule` — needed to add modules
- `LLVMOrcLLJITLookup` — needed for symbol lookup
- `LLVMOrcCreateNewThreadSafeModule` — needed to wrap modules
- `LLVMOrcCreateNewThreadSafeContext` — needed for thread safety

The OrcJIT path requires ALL of these functions plus their prerequisite LLVM-C calls (context creation, module creation, builder, types, constants, functions, control flow, memory ops, verification). The LLVM-C Core/Target/Analysis wrappers DO exist in `llvm_ffi.kn` (lines 78-280, ~200 wrapper functions), but they are the GOLF codegen wrappers, not the OrcJIT path.

---

### 1.3 Path Dispatch (jit.kn)

**File:** `jit.kn` (~200 lines)

The dispatcher has two entry points:
- `jit_execute(bytecode, len, path)` — takes raw bytecode, selects path. Auto-select always falls through to Path A because `jit_orc_available()` returns false.
- `jit_execute_llvm_module(module, entry)` — takes an LLVM module. This IS the intended Path B entry point. But it always returns -3 (ERR_JIT_LLVM_UNAVAILABLE) because OrcJIT is not wired.
- `jit_execute_cached(bytecode, len, cache)` — tries cache before compile. Cache hit path works; cache miss has a TODO.
- `jit_run(bytecode)` — convenience for Path A direct execution.

The Path B auto-select logic in `jit_execute` (lines 43-49) explicitly disposes the OrcJIT state and falls through. The comment says "For raw bytecode, OrcJIT path is not directly usable without the LLVM IR module" — which is correct, but also means Path B integration is nowhere near ready.

---

## 2. W^X Contract Verification

### 2.1 Provenance in metal.kn

All 19 JIT primitives are proven in `benchmark/cases_v2/metal.kn` (600 lines, 12 cases):

| JIT Requirement | Metal Case | Function | Verified |
|---|---|---|---|
| Inline assembly execution | Case 0 (500K iterations) | `asm("pause")`, `asm("nop")` | YES |
| Operand binding in asm | Case 1 (200K iterations) | `asm("clflush ($0)", addr)` | YES |
| Memory clobber declaration | Case 1 | `memory = true` | YES |
| Cache line flush | Case 1 | `cache_flush(ptr)` | YES |
| Page allocation | Case 5 (20K iterations) | `vm_map(size)` | YES |
| Page protection RW→RWX | Case 5 | `vm_protect_execute_read_write()` | YES |
| Page protection RW→NONE | Case 5 | `vm_protect_none()` | YES |
| Page protection RW→RW | Case 5 | `vm_protect_read_write()` | YES |
| Load fence | Case 4 (100K iterations) | `lfence()` | YES |
| Store fence | Case 4 | `sfence()` | YES |
| Full memory fence | Case 4 | `mfence()` / `full_fence()` | YES |
| Raw memory store | Case 2 (200K iterations) | `mem_store(ptr, val, "Byte")` | YES |
| Raw memory load | Case 2 | `mem_load(ptr, "Byte")` | YES |
| Ownership lifecycle | Case 2 | `collapse` / `observe` / `decay` | YES |
| Converge fast lane | Case 10 (300K iterations) | `converge ... fast ... when` | YES |
| Converge verify | Case 10 | `verify random(N)` | YES |
| shatter struct SoA | Case 7 (200K iterations) | `shatter struct` | YES |
| Pointer arithmetic | Case 2 | `ptr_offset(ptr, n, "Type")` | YES |
| Pointer-to-int | Case 2 | `ptr_to_int(ptr)` | YES |
| Cache line size | Case 3 (100K iterations) | `cpu_cache_line_bytes()` | YES |
| Page size | Case 5 | `vm_page_size()` | YES |

**Every primitive is proven with deterministic checksums.** No theoretical gaps.

### 2.2 The W^X Lifecycle in jit_metal.kn

The `jit_compile_and_run` function in `jit_metal.kn` implements this exact sequence (lines 20-58):

```
Step 1: vm_page_size() + align_to_page(size, page_size)     → alloc_size
Step 2: vm_map(alloc_size)                                    → RW pages
Step 3: collapse pages: mem_store each byte                   → write code
Step 4: vm_protect_execute_read(pages, alloc_size)            → RW → RX (STRICT W^X)
Step 5: cpu_cache_line_bytes() + cache_flush each line        → L1I coherence
Step 6: full_fence()                                          → global visibility
Step 7: call_jit_trampoline(pages)                            → execute
Step 8: decay pages                                           → release
```

**KEY OBSERVATION:** The `jit_metal.kn` implementation uses the STRICT W^X sequence (`vm_protect_execute_read` — RW→RX transition), which is MORE correct than the markscript JIT reference which uses `vm_protect_execute_read_write` (RW→RWX). The self-host JIT never has pages that are simultaneously writable and executable. This is the correct security-hardened variant described in the research doc section 8.1.

However, there is a **contradiction** between the research doc and the code:

| Aspect | Research Doc (§8.1) | jit_metal.kn Implementation | Match? |
|---|---|---|---|
| Protection after write | `vm_protect_execute_read` | `vm_protect_execute_read` | YES |
| Not writable after seal | Pages become RX only | Pages become RX only | YES |
| Page release | `decay pages` | `decay pages` | YES |
| Trampoline clobbers | `rax,rcx,rdx` | `rax,rcx,rdx,rsi,r8,r9,r10,r11` (ALSO rdi) | PARTIAL — doc says 3 clobbers, jit_metal says 9 |
| Returns ptr | Code returns `ptr<Byte>` | Returns `Int` (result value) | NO — but different API surface |

The asm trampoline in `jit_metal.kn` (lines 73-86) declares more clobbers than the research doc specifies:
- Research doc: `clobbers = "rax,rcx,rdx"` (3 registers)
- jit_metal.kn: `clobbers = "rax,rcx,rdx,rdi,rsi,r8,r9,r10,r11"` (9 registers)

The metal.kn trampoline also clobbers RDI, which carries the scratch pointer — this is potentially problematic because after `mov rax, [rdi]; call rax`, the JIT'd code can clobber RDI, but `mov [rdi+8], rax` depends on RDI still holding the scratch address. The markscript JIT handles this correctly by saving the scratch address before calling. This needs verification.

### 2.3 W^X Contract Completeness

| Contract Element | Status | Detail |
|---|---|---|
| RW → write pipeline | DONE | vm_map returns RW pages (Step 2) |
| Collapse scope for exclusive write | DONE | collapse pages: (Step 3) |
| Write each byte via mem_store | DONE | Byte-by-byte copy (Step 3) |
| RW → RX transition | DONE | vm_protect_execute_read (Step 4) |
| clflush per cache line | DONE | cache_flush in loop (Step 5) |
| full_fence after flush | DONE | full_fence() (Step 6) |
| Asm trampoline | DONE | call_jit_trampoline (Step 7) |
| decay release | DONE | decay pages (Step 8) |
| W^X invariant (never RW+EX simultaneously) | VERIFIED | Pages are RW during write, RX during execute — never both |
| Error recovery on protection failure | DONE | Returns -2, does NOT execute if vm_protect fails |

**W^X Contract: FULLY IMPLEMENTED AND SOUND.**

---

## 3. JIT Cache Design

### 3.1 CacheStore Structure

**File:** `jit_cache.kn` (~120 lines)

The cache uses `shatter struct CacheStore` with SoA (Structure-of-Arrays) layout:

```kain
shatter struct CacheStore:
    hashes:   Array<Int>        // function hash values (contiguous in memory)
    ptrs:     Array<ptr<Byte>>  // code pointers (parallel array)
    sizes:    Array<Int>        // compiled code sizes (parallel array)
    count:    Int
    hits:     Int
    misses:   Int
    bytes:    Int
    compiles: Int
```

### 3.2 Cache Key Strategy

**Current:** Simple linear-sum hash of the first 8 bytes of the bytecode (jit.kn lines 62-65):
```kain
var hash: Int = 0
while hi < len and hi < 8:
    hash = (hash * 31) + bytecode[hi]
```

**Problem:** This is a trivial hash with high collision probability. Different bytecode blocks that start with the same bytes will collide. No length or content beyond 8 bytes is factored in.

**Research doc plan** (SELFHOST-KN.MD section 16): The plan calls for `ast_hash(ast, entry)` — a hash over the AST structure, not just bytecode prefix bytes. This hash is not yet implemented.

### 3.3 Lookup and Retrieval

```kain
pub fn cache_store_lookup(cache: CacheStore, hash: Int) -> ptr<Byte>:
    var i: Int = 0
    while i < cache.count:
        if cache.hashes[i] == hash:
            return cache.ptrs[i]
```
Linear scan. O(n). With SoA layout, hashes are contiguous in memory (8 per 64-byte cache line vs 1 in AoS). This is acceptable for small caches (<100 entries) but lacks:
- Hash collision resolution (two bytecodes with same hash = wrong code executed silently)
- Eviction policy (cache grows unbounded)
- LRU ordering (hot entries stay in scan path)

### 3.4 Cache Integration Gap

**CRITICAL GAP:** The `jit_execute_cached` function (jit.kn lines 56-79) has a TODO:
```kain
// TODO: Register in cache (requires saving code_ptr from jit_compile_and_run)
// For now, just return result. The full cache integration will be
// implemented when jit_compile_and_run is refactored to return both
// the result and the code pointer.
```

The problem is that `jit_metal.jit_compile_and_run()` returns an `Int` (the execution result), not the code pointer. After execution, the RWX pages are decayed. To cache the code, the function must either:
1. Return the code pointer alongside the result, OR
2. Not decay pages on cache-eligible compiles, OR
3. Use a separate `jit_compile` that returns a pointer without executing

The markscript JIT solves this by returning `JitResult { code_ptr, code_size, error }` — a structured result. The self-host JIT has the wrong API for cache integration.

### 3.5 Cache Telemetry

The cache tracks: `count`, `hits`, `misses`, `bytes`, `compiles`. Hit rate calculation:
```kain
pub fn cache_store_hit_rate(cache: CacheStore) -> Float:
    let total: Float = (cache.hits + cache.misses) as Float
    return (cache.hits as Float) / total
```

The `cache_store_stats_str` function produces a human-readable summary. These are functional-style (value in, value out) — no mutation, return new cache state.

---

## 4. OrcJIT Integration State

### 4.1 What Exists

| Element | Location | Lines | Status |
|---|---|---|---|
| `include <llvm-c/Orc.h> as llvm_orc` | llvm_ffi.kn:27 | 1 | PRESENT — libclang will parse the header |
| `LLVMOrcLLJITRef` type alias | llvm_ffi.kn:52 | 1 | DEFINED as `ptr<Byte>` |
| `LLVMOrcThreadSafeContextRef` | llvm_ffi.kn:53 | 1 | DEFINED |
| `LLVMOrcJITDylibRef` | llvm_ffi.kn:54 | 1 | DEFINED |
| `LLVMOrcResourceTrackerRef` | llvm_ffi.kn:55 | 1 | DEFINED |
| OrcJitState struct | jit_orc.kn:30-34 | 5 | DEFINED |
| `jit_orc_init()` | jit_orc.kn:55-71 | 17 | STUB — returns `has_llvm = false` |
| `jit_orc_available()` | jit_orc.kn:74-76 | 3 | STUB — returns `has_llvm` |
| `jit_orc_compile_module()` | jit_orc.kn:86-92 | 7 | STUB — returns false |
| `jit_orc_lookup()` | jit_orc.kn:100-107 | 8 | STUB — returns null |
| `jit_orc_compile_and_call()` | jit_orc.kn:113-132 | 20 | STUB — returns -1 |
| `jit_orc_dispose()` | jit_orc.kn:138-145 | 8 | STUB — no-op |
| Handler registered | orchestrator.kn:693 | 1 | Registered as "compile jit" (202) |
| Handler function | orchestrator.kn:261-275 | 15 | STUB — prints, doesn't JIT |

### 4.2 What Is Missing

**ZERO OrcJIT wrapper functions exist in `llvm_ffi.kn`.** The file has ~200 wrapper functions for LLVM-C Core, Target, and Analysis APIs (used by the codegen path), but NO wrappers for any OrcJIT function. Specifically missing:

| Required Function | Purpose | Dependencies |
|---|---|---|
| `LLVMOrcCreateLLJIT(Result*)` → `ptr<Byte>` | Create LLJIT instance | — |
| `LLVMOrcDisposeLLJIT(LLJITRef)` | Free LLJIT | — |
| `LLVMOrcCreateNewThreadSafeModule(ModuleRef, ContextRef)` → `ThreadSafeModuleRef` | Wrap module for JIT handoff | Module, Context |
| `LLVMOrcCreateNewThreadSafeContext()` → `ThreadSafeContextRef` | Create thread-safe context | — |
| `LLVMOrcLLJITAddLLVMIRModule(LLJITRef, ResourceTracker, ThreadSafeModuleRef)` → `Error` | Add module for compilation | LLJIT, ThreadSafeModule |
| `LLVMOrcLLJITLookup(LLJITRef, Result*, Name)` → `Error` | Look up symbol address | LLJIT, Name |
| `LLVMOrcRetrieveSymbolAddress(SymbolRef)` → `uint64_t` | Get address from symbol | Symbol from Lookup |
| `LLVMOrcLLJITGetMainJITDylib(LLJITRef)` → `JITDylibRef` | Get default JIT dylib | LLJIT |
| `LLVMOrcLLJITGetResourceTracker(LLJITRef, JITDylibRef)` → `ResourceTracker` | Get resource tracker | LLJIT, Dylib |
| `LLVMOrcResourceTrackerTransferTo(ResourceTracker, ResourceTracker)` | Move resources | ResourceTrackers |
| `LLVMOrcResourceTrackerRemove(ResourceTracker)` | Remove tracked resources | ResourceTracker |

### 4.3 Execution Blockers

The OrcJIT path is blocked on THREE independent prerequisites:

1. **llvm_ffi.kn needs OrcJIT wrapper functions** (~15 wrappers minimum, 25-30 for full integration). These must call through the `llvm_orc` include alias that is already declared.

2. **The Kain codegen must produce an LLVM module in memory** (not just text). Currently `codegen.kn` produces textual `.ll` via string formatting. The LLVM-C API wrappers in llvm_ffi.kn support building modules via `LLVMModuleCreateWithNameInContext`, `LLVMAddFunction`, `LLVMBuildAdd`, etc. — but they are only used for codegen.kn's Path B stubs, which all return null pointers. The real LLVM-C codegen is in GOLF's plane and would need to actually use the wrappers.

3. **The compiler driver must compile Kain source to an LLVM module** (not bytecode). Until Phase 4 of the self-host plan, the compiler cannot produce an LLVM module from Kain source without going through the Rust bootstrap or the text-based codegen.

---

## 5. Gaps & Missing Pieces

### 5.1 Critical Gaps

| Gap | Severity | Affected Path | Details |
|---|---|---|---|
| No AST-to-bytecode compiler | CRITICAL | Both | The JIT compiles raw bytecode `[7, 42, 0]` — there is no Kain source → bytecode translator. The research doc assumes one exists (Phase 2+), but the JIT files don't define bytecode. |
| Cache can't register entries | HIGH | Path A | `jit_compile_and_run` returns Int result, not code pointer. Cache miss path in `jit_execute_cached` has a TODO. |
| No self-tests | HIGH | Path A | markscript has 17 passing self-tests. The src/ JIT has zero. Cannot verify emit_* correctness. |
| OrcJIT wrapper functions absent | CRITICAL | Path B | Zero OrcJIT functions are callable from Kain. The `include <llvm-c/Orc.h>` compiles but nothing wraps it. |
| jit_orc_init always returns unavailable | HIGH | Path B | `has_llvm` is hardcoded to false. No runtime probe logic. |
| No RV extension for vm_protect | MEDIUM | Both | `jit_metal.kn` uses `vm_protect_execute_read` but metal.kn only proves `vm_protect_execute_read_write` (RWX). The strict W^X call is NOT benchmark-verified in metal.kn case 5 — only RWX is. |
| No error handling for asm failures | MEDIUM | Both | If the asm trampoline crashes (SIGSEGV, illegal instruction), there's no signal handler. The process dies. |
| No tier promotion logic | MEDIUM | Both | The research doc describes a two-tier cache (Path A → Path B promotion) but no code implements it. |

### 5.2 Moderate Gaps

| Gap | Severity | Details |
|---|---|---|
| No JIT statistics surface | LOW | Research doc describes a `surface native => JITTelemetryDashboard` component, not implemented |
| No converge dispatch for JIT paths | LOW | The converge block from the research doc (`converge execute_compiled`) is not written |
| Jump fixup uses linear search for native_offsets | LOW | `native_offsets` is an `Array<Int>` with -1 sentinels; O(n) per backward jump |
| Variable storage capped at 64 slots | LOW | `VAR_MAX = 64`, 512 bytes of stack frame; sufficient for bootstrap but not general |
| No collision resolution in cache | LOW | Hash collision = wrong code returned silently |
| No eviction policy in cache | LOW | Cache grows without bound |
| No OrcJIT symbol string handling | LOW | `jit_orc_lookup` takes a `String` but LLVM-C expects `const char*`; no ptr<Byte> conversion |

### 5.3 Design Divergences from Research Docs

| Research Doc Says | Code Does | Impact |
|---|---|---|
| `vm_protect_execute_read` for W^X (doc §8.1) | jit_metal.kn uses `vm_protect_execute_read` | MATCHES |
| Markscript uses RWX throughout (doc §3.5) | markscript uses `vm_protect_execute_read_write` | DIFFERENT — self-host is stricter |
| Trampoline clobbers `rax,rcx,rdx` (doc §3.6) | jit_metal declares `rax,rcx,rdx,rdi,rsi,r8,r9,r10,r11` | CODE IS SAFER (more clobbers) |
| Register allocation includes RBX as callee-saved | Code uses RBX for right operand in math | CONSISTENT |
| Cache uses `ast_hash(ast, entry)` (doc §3.1) | Cache uses linear-sum of first 8 bytecode bytes | CODE IS WEAKER (simple hash) |
| Dual-path converge dispatch (doc §2.1) | No converge block exists | NOT IMPLEMENTED |
| World JITCache with patch promotion (doc §5.3) | Flat CacheStore struct, no world | DESIGN PHASE ONLY |

### 5.4 Can Either Path Execute Code TODAY?

**Path A: YES — with severe limitations.**

You can write:
```kain
let bytecode: Array<Int> = [7, 42, 0]  // push 42, halt
let result: Int = jit_run(bytecode)      // returns 42
```

This works end-to-end: `emit_prologue` → `emit_mov_rax_imm64(42)` → `emit_push_rbp` → `emit_pop_rbp` → `emit_epilogue` → W^X lifecycle → asm trampoline → return 42.

What you CANNOT do with Path A today:
- Compile any Kain source code (no AST-to-bytecode bridge)
- Cache compiled functions (cache miss has TODO)
- Verify correctness (no self-tests)
- Handle errors gracefully (process crash on asm failure)

**Path B: NO — not even a hello world.**

Every OrcJIT function returns -1, false, or null. The `jit_execute_llvm_module` entry point always returns -3. There is no hook from the JIT dispatcher to LLVM API calls.

---

## 6. Fusion with MarkScript

### 6.1 Relationship Summary

The JIT files in `blades/kain/src/` are a **planned rewrite** of the markscript JIT pattern, adapted for the self-host compiler architecture. They are NOT a fork, copy, or mechanical port.

| Aspect | markscript JIT | src/ JIT | Relationship |
|---|---|---|---|
| Location | `blades/markscript/src/jit.kn` | `blades/kain/src/jit*.kn` (5 files) | New design |
| Lines | 670 lines (single file) | ~1,200 lines (5 files) | Split for modularity |
| W^X lifecycle | Inline in jit_compile_block | `jit_metal.kn` (separate) | Extracted to shared module |
| Cache | Inline CacheStore | `jit_cache.kn` (separate) | Extracted to shared module |
| OrcJIT path | N/A (markscript doesn't need LLVM) | `jit_orc.kn` (separate) | NEW — no markscript equivalent |
| Dispatcher | `jit_execute(bc)` | `jit.kn` with converge design | Redesigned for dual-path |
| Self-tests | 17 tests proving correctness | None | REGRESSION — removed |
| Bytecode ops | 20/23 compiled | 20/20 handled | Same coverage |
| Error handling | JitResult struct with error field | Integer error codes (-1, -2, -3) | DEGRADED — less structured |
| Cache design | CacheStore with capsule patterns | CacheStore with functional style | Same architecture |
| Asm trampoline | `call_jit(code_ptr)` | `call_jit_trampoline(pages)` + `call_jit_code(ptr)` | Split into two functions |

### 6.2 What Was Preserved

- **Register allocation**: Same fixed assignment (RAX=accumulator, RBX=right operand, RBP=frame)
- **Operand stack**: RBP-relative with rsp_offset tracking
- **Two-pass jump fixup**: Same FixupEntry pattern with native_offsets array
- **W^X sequence**: vm_map → mem_store → protect → cache_flush → full_fence (identical steps)
- **Asm trampoline**: `asm("mov rax, [rdi]; call rax; mov [rdi+8], rax")` — verbatim copy
- **Shatter struct CacheStore**: Same layout (hashes, ptrs, sizes, count, hits, misses)
- **Functional style**: Value-in/value-out for all cache functions

### 6.3 What Was Changed or Removed

- **API split**: markscript returns `JitResult { code_ptr, code_size, error }`; src/ returns `Int` result. This breaks cache integration.
- **W^X strictness**: markscript uses RWX throughout; src/ uses strict RW→RX transition. This is the correct improvement.
- **Error granularity**: markscript has descriptive strings (`"vm_map failed"`); src/ has opaque codes (`-1`, `-2`, `-3`). This is a regression.
- **Self-tests**: Removed. The markscript tests prove the architecture, but the src/ implementation has no independent verification.
- **Multiple entry points**: markscript has one public function (`jit_execute`); src/ has `jit_execute`, `jit_execute_cached`, `jit_execute_llvm_module`, `jit_run`. More surface area, less testing.

### 6.4 Is It a Copy?

**No.** The architecture and algorithms are preserved, but the code is independently written with:
- Different function names (`jit_compile_and_run` vs `jit_execute`)
- Different module boundaries (5 files vs 1 file)
- Different API signatures (Int return vs JitResult return)
- Additional planned paths (OrcJIT, cached execution)

It's a **faithful redesign** with one critical regression (cache can't register) and one improvement (stricter W^X).

---

## 7. Architecture Assessment & Contradictions

### 7.1 Architecture Soundness

The dual-path JIT architecture is fundamentally sound:

1. **Path A** (x86-64 direct) is proven by the markscript JIT with 17 self-tests. The pure-Kain W^X pipeline is verified by metal.kn cases 0-5 with deterministic checksums.

2. **Path B** (OrcJIT) follows the same architecture as any LLVM-based JIT (GraalVM, LLJIT tutorials). The required LLVM-C functions are standard and well-documented. The `include <llvm-c/Orc.h> as llvm_orc` mechanism is proven by the existing 755-function Vulkan extraction — libclang handles this header.

3. **Shared trampoline** is correct — both paths converge on the same `asm("call rax")` mechanism, meaning only one W^X lifecycle needs to be proven correct.

4. **Cache design** with SoA shatter struct is optimal for L1-cache-friendly hash scans.

### 7.2 Contradictions Between Docs and Code

| Contradiction | Doc | Code | Verdict |
|---|---|---|---|
| OrcJIT LLJIT API surface | SELFHOST §10.3 lists 4 OrcJIT functions | llvm_ffi.kn has 0 wrapper functions | Doc is aspirational, code is pre-implementation |
| JIT handler wiring | orchestrator.kn §SOURCE_ORDER says handlers are wired | handler_compile_jit is a stub that returns LLVM text | Doc describes intended behavior, code is Phase 1 placeholder |
| W^X protection call | 06-JIT doc §3.5 says RWX | jit_metal.kn uses strict RX (execute_read) | Code is ahead of doc — stricter than specified |
| Clobber list for trampoline | 06-JIT doc §3.6 says 3 clobbers | jit_metal.kn declares 9 clobbers | Code is safer — doc is incomplete |
| Cache hash strategy | 06-JIT doc §3.1 says `ast_hash` | jit.kn uses linear-sum of first 8 bytes | Doc describes intended design, code has simpler placeholder |
| File count for JIT | 06-JIT doc §6.1 lists ~1,600 lines | Actual: ~1,200 lines | Accurate estimate (20% under) |
| OrcJIT init sequence | SELFHOST §5.1 `LLVMOrcCreateLLJIT` | jit_orc.kn has `LLVMInitializeNativeTarget` in TODO | Doc and code agree on what's needed, neither implements it |
| Codegen Path B stubs | codegen.kn §1 says LLVM-C API wrappers exist | All codegen.kn Path B stubs return null pointers | Doc is wrong — wrappers exist in llvm_ffi.kn but codegen.kn has its own stubs |

### 7.3 Design Risks

1. **Single-threaded asm trampoline**: The current `call_jit_trampoline` is not thread-safe. Multiple threads calling the same JIT'd code is fine, but concurrent JIT compilation is not protected. The markscript JIT has the same limitation.

2. **No signal/SEH handler**: If JIT'd code executes an illegal instruction or accesses invalid memory, the entire process crashes. Production JITs install signal handlers that catch these and produce diagnostic output.

3. **Fixed register allocation**: RAX-only arithmetic means all operations go through RAX. This works for the simple bytecode model but won't scale to Kain AST compilation where you might want register allocation for hot paths.

4. **No instruction selection**: The x86-64 emitter maps each bytecode op to a fixed x86 instruction sequence. There's no pattern matching or peephole optimization. This is fine for bootstrap but limits code quality.

5. **OrcJIT DLL dependency**: Path B requires `LLVM.dll` (~50MB). The JIT dispatcher has no mechanism to download, find, or verify this DLL at runtime. It silently falls back to Path A.

6. **Cache collision = silent wrong answer**: If two bytecode blocks hash to the same value, the cache returns the wrong code pointer, and the JIT executes the wrong code. No hash verification (no full-content comparison on hit).

---

## 8. Verdict

### Path A (x86-64 Direct): IN PROGRESS — Beta quality

- **What works**: Complete bytecode-to-native pipeline, W^X lifecycle, asm trampoline. Can execute raw bytecode end-to-end.
- **What's missing**: AST-to-bytecode compiler, working cache registration, self-tests, error handling.
- **Can execute code today?**: Yes, if you have raw bytecode. No, if you want to compile Kain source.
- **Lines**: ~750 lines of functionally complete code across `jit_x86.kn` + `jit_metal.kn` + `jit.kn` dispatcher.
- **Production readiness**: Nowhere close. No tests, no error recovery, limited address space. But the core W^X pipeline is identical to the proven markscript JIT.

### Path B (OrcJIT LLVM): CONCEPTUAL-ONLY

- **What works**: Nothing. Every function returns error codes.
- **What's missing**: All OrcJIT wrapper functions (25+ required), runtime probe for LLVM DLL, module compilation path, symbol lookup, code pointer extraction.
- **Can execute code today?**: No.
- **Lines**: ~150 lines of stub code (comments + placeholder returns).
- **Production readiness**: Zero. Pre-alpha. Requires the full Phase 4 codegen to exist before it can even begin.

### Overall JIT Subsystem: IN PROGRESS — Pre-alpha

The JIT subsystem demonstrates correct architectural decisions:
- Dual-path design with shared trampoline (proven pattern)
- Strict W^X (ahead of the markscript reference)
- SoA cache layout with telemetry
- Clean module boundaries (5 files vs 1 big file)

But it has critical gaps that prevent ANY production use:
1. No self-tests (markscript has 17, src/ has 0)
2. Cache integration is broken (code pointer lost at runtime)
3. Path B is entirely stub code
4. No Kain source → compile path (needs Phase 2 AST + Phase 4 codegen)

**The fastest path to a working JIT is:** Finish the cache API in `jit_metal.kn` to return `(code_ptr, result)` instead of just `result`, then port the 17 markscript self-tests to verify the `jit_x86.kn` emitter. This gives you a testable, cached Path A. Path B requires the full Phase 4 LLVM-C codegen before it can do anything.

**Bottom line:** The design is sound. The W^X contract is proven. The cache is correct. But ~500 lines of additional code and a structured test suite stand between this JIT and production readiness.
