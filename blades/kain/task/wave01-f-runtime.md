# Task: runtime.kn — Native Runtime ABI Declarations

## Agent: kain-writer
## Wave: 1
## File to write: X:/blades/kain/src/runtime.kn
## Target lines: ~200
## Dependencies: None
## Parallel: Yes

---

## What to Build

Declares the C ABI functions in the Kain native runtime (kain_runtime.lib — 47 C files). These are split into two categories:

1. **Compile-time calls** — Functions the COMPILER calls while running (alloc for AST nodes, print for diagnostics, read_file for source loading)
2. **Emitted declarations** — Functions the compiler EMITS in generated LLVM IR so user programs can call them (runtime_init, alloc, print, etc.)

## Public API Contract

### Compiler-Only Functions (called by kainc at compile time)

```kain
include <runtime/native/include/core/core.h> as kain_rt

// Startup / shutdown
pub fn runtime_init() -> Int with Unsafe
pub fn runtime_shutdown() -> Int with Unsafe

// Memory (for compiler's own allocations)
pub fn runtime_alloc(size: Int, alignment: Int) -> ptr<Byte> with Unsafe
pub fn runtime_alloc_zeroed(size: Int, alignment: Int) -> ptr<Byte> with Unsafe
pub fn runtime_free(ptr: ptr<Byte>) with Unsafe
pub fn runtime_realloc(ptr: ptr<Byte>, new_size: Int) -> ptr<Byte> with Unsafe

// IO (for compiler diagnostics)
pub fn runtime_print(msg: String) with IO
pub fn runtime_println(msg: String) with IO
pub fn runtime_eprint(msg: String) with IO  // stderr

// File IO
pub fn runtime_read_file(path: String) -> String with IO
pub fn runtime_write_file(path: String, content: String) -> Int with IO

// VM / Memory management (for JIT)
pub fn runtime_get_page_size() -> Int
pub fn runtime_vm_map(size: Int) -> ptr<Byte> with Unsafe
pub fn runtime_vm_protect(ptr: ptr<Byte>, size: Int, prot: Int) -> Int with Unsafe
pub fn runtime_vm_unmap(ptr: ptr<Byte>, size: Int) -> Int with Unsafe

// Cache / Fence (for JIT)
pub fn runtime_cache_flush(ptr: ptr<Byte>, size: Int) with Unsafe
pub fn runtime_full_fence() with Unsafe

// Exit
pub fn runtime_exit(code: Int) with Unsafe  // noreturn
pub fn runtime_panic(msg: String) with Unsafe  // noreturn
```

### Emitted Runtime Calls (functions generated code calls)

These are the LLVM `declare` statements that the codegen emits:

```
declare i32 @kain_runtime_init()
declare i32 @kain_runtime_shutdown()
declare i8* @kain_alloc(i64 %size, i64 %alignment)
declare i8* @kain_alloc_zeroed(i64 %size, i64 %alignment)
declare void @kain_free(i8* %ptr)
declare void @kain_print_string(i8* %str)
declare void @kain_print_int(i64 %val)
declare void @kain_println_string(i8* %str)
declare void @kain_exit(i32 %code)
declare i8* @kain_read_entire_file(i8* %path, i64* %out_len)
declare i32 @kain_get_page_size()
declare i8* @kain_vm_map(i64 %size)
declare i32 @kain_vm_protect_execute_read(i8* %ptr, i64 %size)
declare void @kain_cache_flush(i8* %ptr, i64 %size)
declare void @kain_full_fence()
```

## Internal Structure

Provide two views:

1. **Host-side wrappers** — Kain functions that call the runtime via FFI (for compiler's own use). These use `include` or `@extern fn` patterns.

2. **Codegen references** — A `RuntimeExterns` struct that lists every runtime function the codegen needs to emit `declare` statements for. The codegen iterates this and emits one `declare` per entry.

```kain
pub struct RuntimeExtern:
    name: String
    return_type: String     // LLVM type string like "i32", "i8*", "void"
    params: Array<String>   // LLVM param type strings

/// All runtime functions that generated code might call
pub fn runtime_externs() -> Array<RuntimeExtern>
```

## Research to Read

- X:/blades/kain/research/05-runtime-contract-ffi.md — FULL document, primary reference
- X:/blades/kain/research/SELFHOST-KN.MD — Section 10 (Native Runtime Contract)
- X:/blades/kain/reference/C.MD — C FFI guide, include ... as ... pattern
- X:/blades/kain/reference/C_GUIDE.MD — practical C interop

## Reference Files to Study

- X:/runtime/native/include/core/core.h — the ACTUAL runtime header
- X:/crates/core/src/runtime_contract.rs — lines 1-300 for emit_externs pattern

## Neighboring Files

| File | What it needs from runtime.kn |
|------|------------------------------|
| driver.kn | `runtime_init()` at startup, `runtime_shutdown()` at exit |
| jit.kn | `runtime_vm_map()`, `runtime_vm_protect()`, `runtime_cache_flush()`, `runtime_full_fence()` |
| codegen.kn | `runtime_externs()` to emit LLVM declare statements |

## Test Expectations

- `kain check src/runtime.kn` passes
- Runtime header must be findable
- `runtime_init()` returns 0 on success
- `runtime_get_page_size()` returns 4096 or 65536
