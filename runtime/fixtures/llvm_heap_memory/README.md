# LLVM Heap Memory Fixture

This fixture compiles to LLVM, links against the native runtime, executes the produced binary, and validates:

- `alloc_zeroed` lowers to the canonical `__kain_alloc(size, stride, zeroed)` helper
- `realloc_mem(..., true)` lowers to the canonical `__kain_realloc(ptr, size, stride, zeroed_new)` helper
- `mem_store` preserves existing bytes across reallocation
- helper-owned realloc growth zero-fills newly exposed bytes

Expected result:

- The executable exits with code `0`
- The emitted LLVM IR contains `__kain_alloc`, `__kain_realloc`, `__kain_mem_store`, and `__kain_mem_load`
