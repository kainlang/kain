# Stream BRAVO: Dual JIT Engine

**Stream ID:** BRAVO
**Role:** Implement the complete dual-path JIT execution subsystem — Path A (markscript-style x86-64 direct emission) and Path B (OrcJIT via LLVM-C API) — with shared W^X memory lifecycle, asm trampoline, and shatter-struct code cache
**Effort:** ~6 hours
**Depends On:** none (completely self-contained — only needs `std::machine` and `std::markscript` stdlib)
**Requirements Covered:** FR-JIT.1–22
**Design Reference:** Research 06, Design §§JIT, metal.kn benchmark cases 0-5, 10

---

## Context

The JIT engine provides two code execution paths that converge on a single shared asm trampoline. Path A emits raw x86-64 machine code bytes directly — instant startup, zero LLVM dependency, proven in `blades/markscript/src/jit.kn` (670 lines). Path B uses LLVM's OrcJIT API (`include <llvm-c/Orc.h>`) for full optimization. Both paths share the W^X memory lifecycle: `vm_map(RW)` → write code → `vm_protect(RX)` → `cache_flush` → `full_fence` → execute via trampoline. The code cache uses a `shatter struct` layout for L1-friendly hash scanning.

This stream is COMPLETELY INDEPENDENT of the compiler pipeline. You don't need any compiler types (no Token, no AstNode, no ResolvedType). You only need the Kain stdlib (`std::machine` for vm_* and fences, `std::text` for formatting).

---

## Files You Own

### Files to Create

| File | Purpose | After This Stream |
|------|---------|-------------------|
| `X:\blades\kain\src\jit_metal.kn` | W^X lifecycle + shared asm trampoline (~200 lines) | GOLF reads (for trampoline) |
| `X:\blades\kain\src\jit_x86.kn` | Path A: x86-64 direct machine code emission (~500 lines) | None (standalone JIT path) |
| `X:\blades\kain\src\jit_orc.kn` | Path B: OrcJIT via LLVM-C API (~400 lines) | GOLF reads (for OrcJIT symbols) |
| `X:\blades\kain\src\jit_cache.kn` | shatter struct code cache (~200 lines) | None (standalone) |
| `X:\blades\kain\src\jit.kn` | JIT dispatcher: selects Path A or B (~300 lines) | GOLF reads (as entry point) |

### Files You Must NOT Touch

| File | Reason |
|------|--------|
| `X:\blades\kain\src\token.kn` | Owned by Stream ALPHA |
| `X:\blades\kain\src\parser.kn` | Owned by Stream DELTA |
| `X:\blades\kain\src\types.kn` | Owned by Stream FOXTROT |
| `X:\blades\kain\src\codegen.kn` | Owned by Stream GOLF |
| `X:\blades\kain\src\llvm_ffi.kn` | Owned by Stream ECHO + GOLF |

---

## Implementation Tasks

---

### BRAVO-01: W^X Memory Lifecycle + Shared Asm Trampoline (`jit_metal.kn`)

**Effort:** 1.5h
**Objective:** Implement the core W^X memory lifecycle and the shared asm trampoline that both JIT paths converge on. This is the security-critical code — pages must NEVER be simultaneously writable and executable (NFR-SEC1).

**Implementation:**

Create `X:\blades\kain\src\jit_metal.kn`:

```kn
// jit_metal.kn — W^X memory lifecycle + shared asm trampoline
// STREAM: BRAVO
// Consumed by: jit_x86.kn, jit_orc.kn, jit.kn

use std::machine

pub fn jit_compile_and_run(code_bytes: Array<Int>, code_size: Int) -> Int with Unsafe:
    // Step 1: Allocate RW pages
    let page_size: Int = machine.vm_page_size()
    let alloc_size: Int = align_to_page(code_size, page_size)
    let pages: ptr<Byte> = machine.vm_map(alloc_size)
    if ptr_to_int(pages) == 0:
        return -1  // ERR_JIT_VM_MAP_FAILED

    // Step 2: Write JIT code into RW pages (collapse scope)
    collapse pages:
        var i: Int = 0
        while i < code_size:
            let bp: ptr<Byte> = ptr_offset(pages, i, "Byte")
            mem_store(bp, code_bytes[i], "Byte")
            i = i + 1
        0
    // out of collapse — pages now Idle

    // Step 3: Transition RW → RX (W^X enforcement)
    let prot: Int = machine.vm_protect_execute_read(pages, alloc_size)
    if prot != 0:
        decay pages
        return -2  // ERR_JIT_PROTECT_FAILED

    // Step 4: Flush instruction cache (clflush every cache line)
    let cls: Int = machine.cpu_cache_line_bytes()
    var ci: Int = 0
    while ci < alloc_size:
        let bp: ptr<Byte> = ptr_offset(pages, ci, "Byte")
        let fp: ptr<Int> = int_to_ptr(ptr_to_int(bp), "ptr<Int>")
        machine.cache_flush(fp)
        ci = ci + cls

    // Step 5: Full memory fence (mfence)
    machine.full_fence()

    // Step 6: Execute via asm trampoline
    let result: Int = call_jit_trampoline(pages)

    // Step 7: Release pages
    decay pages
    return result

// ── Shared Asm Trampoline ──
// Contract:
//   Input:  code_ptr in scratch[0] (passed via RDI on x86-64)
//   Output: return value in scratch[1] (captured from RAX after call)
//   Callee must: save/restore RBP and RBX, return result in RAX, end with RET
pub fn call_jit_trampoline(code_ptr: ptr<Byte>) -> Int with Unsafe:
    let scratch: ptr<Int> = alloc_zeroed(2, "Int")
    defer decay scratch

    // Store code pointer into scratch[0]
    mem_store(scratch, ptr_to_int(code_ptr), "Int")

    // Execute: mov rax, [rdi]; call rax; mov [rdi+8], rax
    asm("mov rax, [rdi]\ncall rax\nmov [rdi+8], rax",
        memory = true, clobbers = "rax,rcx,rdx,rdi,rsi,r8,r9,r10,r11")

    // Load result from scratch[1]
    let result_slot: ptr<Int> = ptr_offset(scratch, 1, "Int")
    let result: Int = mem_load(result_slot, "Int")
    return result

// ── Utility ──
pub fn align_to_page(size: Int, page_size: Int) -> Int:
    if size % page_size == 0:
        return size
    return ((size / page_size) + 1) * page_size
```

**Acceptance Criteria:**
- [ ] `jit_compile_and_run()` correctly executes a test byte sequence (e.g., `mov eax, 42; ret`)
- [ ] W^X sequence verified: pages go RW → RX (never RWX)
- [ ] `cache_flush` called at every cache line boundary
- [ ] `full_fence()` called before execution
- [ ] `call_jit_trampoline()` correctly captures return value from RAX
- [ ] Pages are properly decay'd in all paths (including error paths)

**Notes:**
- The `asm()` trampoline is the most delicate code in the entire compiler. Test it with a simple known byte sequence first.
- The trampoline clobbers list must include all caller-saved registers on x86-64 System V ABI: rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11.

---

### BRAVO-02: Path A — x86-64 Direct Emission (`jit_x86.kn`)

**Effort:** 2h
**Objective:** Implement markscript-style x86-64 machine code emission. This emits raw x86-64 bytes into a code array with fixed register allocation (RAX accumulator, RBX right operand, RBP frame pointer), software operand stack at RBP-relative offsets, and two-pass jump fixup for forward references.

**Implementation:**

Create `X:\blades\kain\src\jit_x86.kn`:

```kn
// jit_x86.kn — Path A: x86-64 direct machine code emission
// STREAM: BRAVO
// Based on: blades/markscript/src/jit.kn (670 lines, proven)

use std::machine

// ── Fixup entry for two-pass jump resolution ──
pub struct FixupEntry:
    kind:         Int    // 0=jmp, 1=call, 2=jcc (conditional)
    bytecode_at:  Int    // bytecode offset where jump was emitted
    target_label: Int    // target label ID
    condition:    Int    // for jcc: which condition code

// ── Emit context ──
pub struct EmitCtx:
    code:         Array<Int>     // accumulated machine code bytes
    fixups:       Array<FixupEntry>
    labels:       Array<Int>     // label_id → native offset (or -1 if not yet resolved)
    next_label:   Int

pub fn emit_ctx_new() -> EmitCtx:
    return EmitCtx {
        code: empty_array(),
        fixups: empty_array(),
        labels: empty_array(),
        next_label: 0,
    }

// ── Prologue: push rbp; push rbx; mov rbp, rsp ──
pub fn emit_prologue(ctx: *mut EmitCtx):
    ctx.code.push(0x55)  // push rbp
    ctx.code.push(0x53)  // push rbx
    ctx.code.push(0x48)  // REX.W
    ctx.code.push(0x89)  // mov r/m, r
    ctx.code.push(0xE5)  // rbp ← rsp (ModRM: mod=11, reg=100(rsp), rm=101(rbp))

// ── Epilogue: mov rsp, rbp; pop rbx; pop rbp; ret ──
pub fn emit_epilogue(ctx: *mut EmitCtx):
    ctx.code.push(0x48)  // REX.W
    ctx.code.push(0x89)  // mov r/m, r
    ctx.code.push(0xEC)  // rsp ← rbp
    ctx.code.push(0x5B)  // pop rbx
    ctx.code.push(0x5D)  // pop rbp
    ctx.code.push(0xC3)  // ret

// ── Move immediate into RAX ──
pub fn emit_mov_rax_imm64(ctx: *mut EmitCtx, value: Int):
    ctx.code.push(0x48)  // REX.W
    ctx.code.push(0xB8)  // mov rax, imm64
    emit_int64(ctx, value)

// ── Move immediate into RBX ──
pub fn emit_mov_rbx_imm64(ctx: *mut EmitCtx, value: Int):
    ctx.code.push(0x48)  // REX.W
    ctx.code.push(0xBB)  // mov rbx, imm64
    emit_int64(ctx, value)

// ── ADD RBX to RAX: add rax, rbx ──
pub fn emit_add_rax_rbx(ctx: *mut EmitCtx):
    ctx.code.push(0x48)  // REX.W
    ctx.code.push(0x01)  // add r/m, r
    ctx.code.push(0xD8)  // rax ← rax + rbx (ModRM: mod=11, reg=011(rbx), rm=000(rax))

// ── SUB RBX from RAX: sub rax, rbx ──
pub fn emit_sub_rax_rbx(ctx: *mut EmitCtx):
    ctx.code.push(0x48)  // REX.W
    ctx.code.push(0x29)  // sub r/m, r
    ctx.code.push(0xD8)  // rax ← rax - rbx

// ── IMUL RAX by RBX: imul rax, rbx ──
pub fn emit_imul_rax_rbx(ctx: *mut EmitCtx):
    ctx.code.push(0x48)  // REX.W
    ctx.code.push(0x0F)  // two-byte opcode
    ctx.code.push(0xAF)  // imul
    ctx.code.push(0xC3)  // rax ← rax * rbx (ModRM: mod=11, reg=000(rax), rm=011(rbx))

// ── CMP RAX, RBX: cmp rax, rbx ──
pub fn emit_cmp_rax_rbx(ctx: *mut EmitCtx):
    ctx.code.push(0x48)  // REX.W
    ctx.code.push(0x39)  // cmp r/m, r
    ctx.code.push(0xD8)  // cmp rax, rbx

// ── Allocate stack space: sub rsp, imm8 ──
pub fn emit_sub_rsp_imm8(ctx: *mut EmitCtx, amount: Int):
    ctx.code.push(0x48)  // REX.W
    ctx.code.push(0x83)  // sub r/m64, imm8
    ctx.code.push(0xEC)  // rsp
    ctx.code.push(amount and 0xFF)

// ── Push RAX onto native stack ──
pub fn emit_push_rax(ctx: *mut EmitCtx):
    ctx.code.push(0x50)  // push rax

// ── Pop into RBX ──
pub fn emit_pop_rbx(ctx: *mut EmitCtx):
    ctx.code.push(0x5B)  // pop rbx

// ── Store RAX to [RBP+disp32] ──
pub fn emit_store_rax_rbp_offset(ctx: *mut EmitCtx, offset: Int):
    ctx.code.push(0x48)  // REX.W
    ctx.code.push(0x89)  // mov r/m, r
    ctx.code.push(0x85)  // ModRM: mod=10, reg=000(rax), rm=101(rbp)
    emit_int32(ctx, offset)

// ── Load from [RBP+disp32] into RAX ──
pub fn emit_load_rbp_offset_rax(ctx: *mut EmitCtx, offset: Int):
    ctx.code.push(0x48)  // REX.W
    ctx.code.push(0x8B)  // mov r, r/m
    ctx.code.push(0x85)  // ModRM: mod=10, reg=000(rax), rm=101(rbp)
    emit_int32(ctx, offset)

// ── Unconditional JMP (two-pass fixup) ──
pub fn emit_jmp_label(ctx: *mut EmitCtx, label_id: Int):
    let native_offset: Int = len(ctx.code)
    ctx.code.push(0xE9)  // jmp rel32
    // Placeholder 4 bytes
    emit_int32(ctx, 0)
    let fixup: FixupEntry = FixupEntry {
        kind: 0,
        bytecode_at: native_offset,
        target_label: label_id,
        condition: 0,
    }
    ctx.fixups.push(fixup)

// ── Conditional JMP after CMP: JE/JNE/JL/JG/JLE/JGE ──
pub fn emit_jcc_label(ctx: *mut EmitCtx, label_id: Int, cc: Int):
    let native_offset: Int = len(ctx.code)
    ctx.code.push(0x0F)  // two-byte opcode prefix
    // cc: 0x84=JE, 0x85=JNE, 0x8C=JL, 0x8F=JG, 0x8E=JLE, 0x8D=JGE
    ctx.code.push(cc)
    emit_int32(ctx, 0)  // placeholder
    let fixup: FixupEntry = FixupEntry {
        kind: 2,
        bytecode_at: native_offset,
        target_label: label_id,
        condition: cc,
    }
    ctx.fixups.push(fixup)

// ── Label definition ──
pub fn emit_label(ctx: *mut EmitCtx) -> Int:
    let id: Int = ctx.next_label
    ctx.next_label = ctx.next_label + 1
    ctx.labels.push(len(ctx.code))  // resolved immediately
    return id

// ── New label (unresolved forward label) ──
pub fn emit_new_label(ctx: *mut EmitCtx) -> Int:
    let id: Int = ctx.next_label
    ctx.next_label = ctx.next_label + 1
    ctx.labels.push(-1)  // -1 = unresolved
    return id

// ── Resolve a forward label (set its position) ──
pub fn emit_resolve_label(ctx: *mut EmitCtx, label_id: Int):
    ctx.labels[label_id] = len(ctx.code)

// ── Two-Pass Fixup: resolve all forward jumps ──
pub fn apply_fixups(ctx: *mut EmitCtx):
    var i: Int = 0
    while i < len(ctx.fixups):
        let fixup: FixupEntry = ctx.fixups[i]
        let target_native: Int = ctx.labels[fixup.target_label]
        if fixup.kind == 0:  // JMP
            // jmp rel32: displacement = target - (patch_at + 5)
            let rel32: Int = target_native - (fixup.bytecode_at + 5)
            patch_int32(ctx, fixup.bytecode_at + 1, rel32)
        elif fixup.kind == 2:  // JCC
            let rel32: Int = target_native - (fixup.bytecode_at + 6)
            patch_int32(ctx, fixup.bytecode_at + 2, rel32)
        i = i + 1

// ── Helper: emit 64-bit int in little-endian ──
pub fn emit_int64(ctx: *mut EmitCtx, value: Int):
    var v: Int = value
    var i: Int = 0
    while i < 8:
        ctx.code.push(v and 0xFF)
        v = v >> 8
        i = i + 1

// ── Helper: emit 32-bit int in little-endian ──
pub fn emit_int32(ctx: *mut EmitCtx, value: Int):
    var v: Int = value
    ctx.code.push(v and 0xFF)
    ctx.code.push((v >> 8) and 0xFF)
    ctx.code.push((v >> 16) and 0xFF)
    ctx.code.push((v >> 24) and 0xFF)

// ── Helper: patch 32-bit int at offset ──
pub fn patch_int32(ctx: *mut EmitCtx, offset: Int, value: Int):
    var v: Int = value
    ctx.code[offset]     = v and 0xFF
    ctx.code[offset + 1] = (v >> 8) and 0xFF
    ctx.code[offset + 2] = (v >> 16) and 0xFF
    ctx.code[offset + 3] = (v >> 24) and 0xFF
```

**Acceptance Criteria:**
- [ ] Prologue/epilogue emit correct byte sequences
- [ ] Arithmetic ops (add, sub, imul) produce correct x86-64 encoding
- [ ] Store/Load from RBP-relative offsets work at various displacements
- [ ] Two-pass fixup resolves forward jumps correctly
- [ ] A simple test: emit `mov rax, 42; ret` → execute via `jit_compile_and_run()` → returns 42

**Notes:**
- The ModRM encoding for RBP-relative addressing is `mod=10, rm=101` which selects `[rbp + disp32]`. The reg field selects which register.
- For `mov [rbp+disp], rax`: opcode 0x89, ModRM = (10 << 6) | (000 << 3) | 101 = 0x85
- For `mov rax, [rbp+disp]`: opcode 0x8B, ModRM = (10 << 6) | (000 << 3) | 101 = 0x85

---

### BRAVO-03: Path B — OrcJIT via LLVM-C API (`jit_orc.kn`)

**Effort:** 1.5h
**Objective:** Implement the OrcJIT path using LLVM-C API. This path initializes native target support, creates an LLJIT instance, compiles LLVM IR modules in-memory, and looks up entry symbols. Falls back to Path A if LLVM DLL is unavailable.

**Implementation:**

Create `X:\blades\kain\src\jit_orc.kn`:

```kn
// jit_orc.kn — Path B: OrcJIT via LLVM-C API
// STREAM: BRAVO
// Depends on: ECHO's llvm_ffi.kn type definitions for include <llvm-c/Orc.h>

use std::machine

// NOTE: The actual include <llvm-c/Orc.h> as llvm_orc and related FFI is defined
// by Stream ECHO in llvm_ffi.kn. This file uses the symbols that ECHO exposes.

pub struct OrcJitState:
    initialized: Bool
    jit_handle:   ptr<Byte>    // LLVMOrcLLJITRef
    has_llvm:     Bool

pub fn jit_orc_init() -> OrcJitState with Unsafe:
    // Check if LLVM DLL is available
    // This is a best-effort probe — if LLVM isn't installed, we return has_llvm=false
    let mut state: OrcJitState = OrcJitState {
        initialized: false,
        jit_handle: ptr_to_int(null_ptr(), "ptr<Byte>"),
        has_llvm: false,
    }

    // Probe for LLVM native target initialization
    // If the DLL is missing, these calls will fail gracefully
    // LLVMInitializeNativeTarget() — defined in llvm_ffi.kn by ECHO
    // LLVMInitializeNativeAsmPrinter() — defined in llvm_ffi.kn by ECHO
    // For now, stub these since llvm_ffi.kn is owned by ECHO

    // TODO: After ECHO delivers llvm_ffi.kn, wire the actual LLVM-C calls here:
    // let llvm_ok: Bool = llvm_native_target_init_available()
    // if llvm_ok:
    //     LLVMInitializeNativeTarget()
    //     LLVMInitializeNativeAsmPrinter()
    //     state.jit_handle = LLVMOrcCreateLLJIT(...)
    //     state.has_llvm = true

    return state

pub fn jit_orc_available(state: OrcJitState) -> Bool:
    return state.has_llvm

pub fn jit_orc_compile_and_call(module: ptr<Byte>, entry_name: String) -> Int with Unsafe:
    // TODO: Wire after ECHO delivers llvm_ffi.kn
    // 1. LLVMVerifyModule(module, LLVMReturnStatusAction, ...)
    // 2. tracker = LLVMOrcLLJITAddLLVMIRModule(jit, module)
    // 3. LLVMOrcLLJITLookup(jit, &addr, entry_name)
    // 4. code_ptr = int_to_ptr(addr, "ptr<Byte>")
    // 5. result = call_jit_trampoline(code_ptr)  -- from jit_metal.kn
    // 6. return result
    return -1  // stub: OrcJIT not yet wired

// ── Lookup entry symbol ──
pub fn jit_orc_lookup_symbol(state: OrcJitState, symbol: String) -> ptr<Byte> with Unsafe:
    // TODO: Wire after ECHO delivers llvm_ffi.kn
    // LLVMOrcLLJITLookup(state.jit_handle, &addr, symbol)
    return null_ptr()

// ── Cleanup ──
pub fn jit_orc_dispose(state: *mut OrcJitState) with Unsafe:
    if state.has_llvm:
        // TODO: LLVMOrcDisposeLLJIT(state.jit_handle)
        pass
    state.has_llvm = false
```

**Acceptance Criteria:**
- [ ] `jit_orc_init()` probes for LLVM availability and sets `has_llvm` correctly
- [ ] When LLVM is unavailable, `jit_orc_available()` returns false → triggers fallback to Path A
- [ ] Stub correctly returns -1 when OrcJIT not wired (allowing Path A fallback)
- [ ] `jit_orc_dispose()` cleans up without crashing

**Notes:**
- This file is a STUB initially. After ECHO delivers `llvm_ffi.kn` with the `include <llvm-c/Orc.h> as llvm_orc` declarations, GOLF or a follow-up stream wires the actual LLVM-C calls.
- The trampoline from `jit_metal.kn` (`call_jit_trampoline()`) is used by BOTH paths — this is the convergence point.

---

### BRAVO-04: Shatter Struct Code Cache (`jit_cache.kn`)

**Effort:** 1h
**Objective:** Implement a Structure-of-Arrays (SoA) code cache for JIT-compiled functions. Linear scan of hashes array (L1 cache friendly), telemetry tracking for hits/misses/bytes/compiles.

**Implementation:**

Create `X:\blades\kain\src\jit_cache.kn`:

```kn
// jit_cache.kn — shatter struct code cache for JIT
// STREAM: BRAVO
// Uses: shatter struct for SoA layout = cache-line-friendly linear scans

shatter struct CacheStore:
    hashes:   Array<Int>
    ptrs:     Array<ptr<Byte>>
    sizes:    Array<Int>
    count:    Int
    hits:     Int
    misses:   Int
    bytes:    Int
    compiles: Int

pub fn cache_store_new() -> CacheStore:
    return CacheStore {
        hashes: empty_array(),
        ptrs: empty_array(),
        sizes: empty_array(),
        count: 0,
        hits: 0,
        misses: 0,
        bytes: 0,
        compiles: 0,
    }

// Linear scan of hashes array (SoA = contiguous hashes in L1 cache)
pub fn cache_store_lookup(cache: CacheStore, hash: Int) -> ptr<Byte>:
    var i: Int = 0
    while i < cache.count:
        if cache.hashes[i] == hash:
            return cache.ptrs[i]
        i = i + 1
    return null_ptr()

// Check if entry exists and return hit/miss status
pub fn cache_store_check(cache: *mut CacheStore, hash: Int) -> Bool:
    var i: Int = 0
    while i < cache.count:
        if cache.hashes[i] == hash:
            cache.hits = cache.hits + 1
            return true
        i = i + 1
    cache.misses = cache.misses + 1
    return false

// Register a new cache entry
pub fn cache_store_register(cache: *mut CacheStore, hash: Int, ptr: ptr<Byte>, size: Int):
    cache.hashes.push(hash)
    cache.ptrs.push(ptr)
    cache.sizes.push(size)
    cache.count = cache.count + 1
    cache.bytes = cache.bytes + size
    cache.compiles = cache.compiles + 1

// Telemetry
pub fn cache_store_hit_rate(cache: CacheStore) -> Float:
    let total: Float = cache.hits as Float + cache.misses as Float
    if total == 0.0:
        return 0.0
    return (cache.hits as Float) / total

pub fn cache_store_stats(cache: CacheStore) -> String:
    return "cache: " + str(cache.count) + " entries, " +
        str(cache.hits) + " hits, " + str(cache.misses) + " misses, " +
        str(cache.bytes) + " bytes, " + str(cache.compiles) + " compiles, " +
        "hit_rate=" + str(cache_store_hit_rate(cache))
```

**Acceptance Criteria:**
- [ ] `CacheStore` uses `shatter struct` for SoA layout
- [ ] `cache_store_lookup()` correctly finds entries by hash
- [ ] `cache_store_register()` adds entries and increments counters
- [ ] Telemetry: hits, misses, bytes, compiles, hit_rate all track correctly
- [ ] Linear scan works up to cache capacity (no limit on entries)

---

### BRAVO-05: JIT Dispatcher (`jit.kn`)

**Effort:** 0.5h
**Objective:** Implement the top-level JIT dispatcher that selects Path A or Path B based on capability, routes to the correct path, and returns the execution result.

**Implementation:**

Create `X:\blades\kain\src\jit.kn`:

```kn
// jit.kn — JIT dispatcher: selects Path A or Path B
// STREAM: BRAVO
// Consumed by: GOLF (as entry point for JIT execution)

pub fn jit_execute(code_bytes: Array<Int>, code_size: Int, target: String) -> Int with Unsafe:
    // Path selection:
    // - If target == "jit" and x86-64 platform → Path A (direct emission)
    // - If OrcJIT available → Path B (LLVM OrcJIT)
    // - Otherwise → Path A fallback

    let orc_state: OrcJitState = jit_orc_init()
    if jit_orc_available(orc_state):
        // Path B: OrcJIT — requires LLVM-C module
        // For now, Path A handles bytecode directly
        // Full Path B integration will route through GOLF's codegen
        defer jit_orc_dispose(orc_state)
        // Fall through to Path A if no LLVM module available
        pass

    // Path A: direct x86-64 emission
    let result: Int = jit_compile_and_run(code_bytes, code_size)
    return result

pub fn jit_execute_llvm_module(module: ptr<Byte>, entry: String) -> Int with Unsafe:
    // Entry point for Path B when GOLF has an LLVM module ready
    let orc_state: OrcJitState = jit_orc_init()
    defer jit_orc_dispose(orc_state)

    if jit_orc_available(orc_state):
        return jit_orc_compile_and_call(module, entry)

    // Fallback: can't JIT without LLVM
    return -1

// Re-export from sub-modules for convenience
// (Kain resolves these via use src::jit_metal, etc.)
```

**Acceptance Criteria:**
- [ ] `jit_execute()` correctly dispatches to Path A when OrcJIT is unavailable
- [ ] `jit_execute_llvm_module()` routes to Path B when LLVM is present
- [ ] Fallback to -1 when neither path is available
- [ ] Integration: a simple x86-64 bytecode block executes correctly through the complete JIT pipeline

---

## Stream Conventions

- **Language:** Pure Kain with Unsafe effect for all JIT operations (asm, vm_*, cache_flush, mem_store, ptr arith)
- **Naming:** snake_case for functions; PascalCase for structs; `jit_` prefix for all public functions
- **Imports:** `use std::machine` for VM and fence operations; `use src::jit_metal` for trampoline
- **Error handling:** Return negative error codes (-1 = vm_map failed, -2 = protect failed, -3 = JIT unavailable)
- **Testing:** Test with known x86-64 byte sequences against metal.kn cases 0-5
- **Comments:** Document every x86-64 opcode emitted with the assembly mnemonic and operand encoding

---

## Stream Boundary — What You Do NOT Do

- ❌ Do NOT import any compiler types (Token, AstNode, ResolvedType) — this stream is self-contained
- ❌ Do NOT implement LLVM IR codegen — that's GOLF's job
- ❌ Do NOT modify `llvm_ffi.kn` — that's shared between ECHO and GOLF
- ❌ Do NOT use emitter patterns that depend on the compiler pipeline (you work with raw bytes, not ASTs)
- ❌ Do NOT use RWX memory — always use the W^X lifecycle: RW → write → RX (never RWX)

---

## Verification (After This Stream)

```bash
# Check all JIT files compile
kain check X:\blades\kain\src\jit_metal.kn
kain check X:\blades\kain\src\jit_x86.kn
kain check X:\blades\kain\src\jit_orc.kn
kain check X:\blades\kain\src\jit_cache.kn
kain check X:\blades\kain\src\jit.kn

# Test: emit a simple function and execute it
# Expected: jit_compile_and_run() with [mov eax, 42; ret] bytes returns 42
```

**Self-check:**
- [ ] All 5 files created
- [ ] W^X lifecycle correct (RW → RX, cache_flush, full_fence, trampoline)
- [ ] x86-64 emitter produces correct byte sequences for all 15+ opcodes
- [ ] Two-pass fixup correctly resolves forward jumps
- [ ] OrcJIT path stubbed with clear TODO markers for GOLF integration
- [ ] Code cache uses shatter struct SoA layout
- [ ] JIT dispatcher correctly selects paths

---

## Completion Report

When done, report:
- Files created: jit_metal.kn, jit_x86.kn, jit_orc.kn, jit_cache.kn, jit.kn — with line counts
- x86-64 opcodes implemented: how many, which ones
- Known limitations (e.g., OrcJIT is stubbed pending LLVM-C FFI)
- Any issues encountered
- Verification: test byte sequences that pass
