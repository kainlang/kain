# Task: jit.kn — OrcJIT + W^X JIT Execution

## Agent: kain-writer
## Wave: 1
## File to write: X:/blades/kain/src/jit.kn
## Target lines: ~300
## Dependencies: None
## Parallel: Yes

**CRITICAL:** Read markscript/src/jit.kn FULLY (670 lines) before writing. This file follows its proven W^X pattern.

---

## What to Build

The JIT execution path — compile an LLVM module in memory with OrcJIT, extract a function pointer, and call it via an assembly trampoline with proper W^X memory protections.

Two paths:
1. **OrcJIT path** — LLVM OrcJIT compiles the module and gives us a function pointer
2. **W^X trampoline** — the assembly call sequence that invokes the JITed function pointer safely

## Public API Contract

```kain
include <llvm-c/Orc.h> as llvm_orc

/// JIT-compile an LLVM module and return the JIT handle
pub fn jit_create() -> ptr<Byte> with Unsafe

/// Add an LLVM module to the JIT and compile it
pub fn jit_add_module(jit: ptr<Byte>, mod: ptr<Byte>) -> Int with Unsafe

/// Look up a symbol in the JITed module by name
pub fn jit_lookup(jit: ptr<Byte>, name: String) -> ptr<Byte> with Unsafe

/// JIT compile a module, extract the "main" function, and call it with args.
/// Returns the exit code from main().
pub fn jit_run(mod: ptr<Byte>, arg: Int) -> Int with Unsafe

/// JIT compile a module and return a callable function pointer for any symbol
pub fn jit_get_fn(jit: ptr<Byte>, symbol: String) -> ptr<Byte> with Unsafe

/// Dispose the JIT instance (call at shutdown)
pub fn jit_dispose(jit: ptr<Byte>) with Unsafe
```

## Internal W^X Memory Lifecycle

From metal.kn primitives (proven in benchmark):

```kain
fn call_jit_code(code_ptr: ptr<Byte>, arg: Int) -> Int with Unsafe:
    let scratch: ptr<Int> = alloc_zeroed(2, "Int")
    mem_store(scratch, ptr_to_int(code_ptr), "Int")
    mem_store(ptr_offset(scratch, 1, "Int"), arg, "Int")
    let sc_int = ptr_to_int(scratch)

    asm("mov rax, [rdi]\nmov rdi, [rdi+8]\ncall rax\nmov [rdi+8], rax",
        sc_int,
        constraints = "{rdi}",
        clobbers = "rax,rcx,rdx,rdi",
        memory = true, intel = true)

    let result = mem_load(ptr_offset(scratch, 1, "Int"), "Int")
    decay scratch
    return result
```

The W^X pattern (for direct binary JIT, used by markscript):
1. `vm_map(size)` — allocate RW pages
2. `mem_store` — write JIT code bytes into RW pages
3. `vm_protect_execute_read(ptr, size)` — seal pages RW → RX
4. `cache_flush(ptr, size)` — flush CPU instruction cache
5. `full_fence()` — memory barrier
6. Call via asm trampoline

For OrcJIT, LLVM handles steps 1-5 internally. We only need the asm trampoline (step 6).

## LLVM-C OrcJIT API Used

| Kain call | LLVM-C function |
|-----------|----------------|
| jit_create | LLVMOrcCreateLLJIT() |
| jit_add_module | LLVMOrcLLJITAddLLVMIRModule(jit, mod) |
| jit_lookup | LLVMOrcLLJITLookup(jit, name) |
| jit_dispose | LLVMOrcDisposeLLJIT(jit) |

## Research to Read

- X:/blades/kain/research/06-jit-markscript-metal-architecture.md — FULL document, this is your primary reference
- X:/blades/kain/research/03-llvm-codegen-jit.md — Section on OrcJIT and W^X
- X:/blades/kain/research/SELFHOST-KN.MD — Appendix F (LLVM IR Emission Patterns)

## Reference Files to Study (READ THESE BEFORE WRITING)

- X:/blades/markscript/src/jit.kn — READ THE FULL 670 LINES. This is the proven pattern.
- X:/blades/kain/reference/metal.kn — lines 1-200 (asm pause, cache flush, vm_page_torture cases)
- X:/blades/kain/reference/SYSTEMS_PROGRAMMING.MD — sections on asm, vm_protect, cache_flush

## Neighboring Files

| File | What it needs from jit.kn |
|------|--------------------------|
| driver.kn | `jit_run(mod, arg)` for Jit target |
| context.kn | module created by context, passed to jit |
| runtime.kn | `vm_map()`, `vm_protect_execute_read()`, `cache_flush()`, `full_fence()` |

## Test Expectations

- `kain check src/jit.kn` passes
- LLVM-C OrcJIT headers findable on build machine
- `jit_create()` returns non-null handle
- `jit_dispose(jit)` is safe to call
- The asm trampoline correctly passes arguments and captures return value

## Code Patterns

- Every function uses `with Unsafe` — JIT is inherently unsafe
- LLVM handles as `ptr<Byte>`
- String arguments to LLVM-C functions: pass as C strings via alloc + mem_store
- Dispose in reverse order: jit → module → context
- Return Int 0 for success, non-zero for failure
