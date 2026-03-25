# KAIN Native Runtime Helper ABI Summary

**Task:** 4.2 Add canonical helper declarations to native headers  
**Date:** 2025-01-XX  
**Status:** ✅ COMPLETE

## Overview

This document summarizes the canonical low-level helper ABI declarations added to the KAIN native runtime headers. These declarations provide the bridge between compiler-emitted code and native memory operations.

## Requirements Coverage

- ✅ **Requirement 3.1:** Canonical low-level helper surface defined
- ✅ **Requirement 3.2:** Address-of, bind-local, load/store, field/index pointer operations declared
- ✅ **Requirement 3.6:** Union, bitfield, and memory operation helpers declared

## Header Files Created

### 1. `kain_runtime_memory.h`

**Purpose:** Core pointer, memory, and allocation operations

**Helpers Declared (11 total):**

#### Pointer and Address Operations (5 helpers)
1. `void* __kain_bind_local(void* ptr)` - Create pointer binding to local variable
2. `void* __kain_addr_of(void* ptr, size_t size)` - Take address of rvalue expression
3. `void* __kain_ptr_offset(void* ptr, int64_t offset, int64_t stride)` - Pointer arithmetic
4. `void* __kain_field_ptr(void* ptr, const char* field, size_t offset)` - Struct field pointer
5. `void* __kain_index_ptr(void* ptr, int64_t index, int64_t stride)` - Array element pointer

#### Memory Load/Store Operations (2 helpers)
6. `void __kain_mem_load(const void* ptr, void* out, size_t size)` - Raw memory read
7. `void __kain_mem_store(void* ptr, const void* value, size_t size)` - Raw memory write

#### Allocation Operations (2 helpers)
8. `void* __kain_alloc(size_t size, size_t stride, int zeroed)` - Heap allocation
9. `void* __kain_realloc(void* ptr, size_t size, size_t stride, int zeroed_new)` - Heap reallocation

**Documentation:**
- Comprehensive function comments with purpose, parameters, returns, ABI considerations
- Example compiler emission patterns
- Safety and alignment notes
- Cross-references to checklist document

---

### 2. `kain_runtime_bitfield.h`

**Purpose:** C-compatible bitfield access operations

**Helpers Declared (2 total):**

1. `int64_t __kain_bitfield_get(...)` - Extract bitfield value with sign extension
2. `void __kain_bitfield_set(...)` - Write bitfield value preserving other fields

**Key Features:**
- Bitfield unit size: 8 bytes (uint64_t)
- Bit ordering: LSB-first (x86_64, ARM64, WASM)
- Integer promotion: fields < 32 bits promote to i32/u32
- Sign extension: applied for signed fields during get operations

**Documentation:**
- Algorithm descriptions for get/set operations
- Bitfield packing rules
- ABI considerations for atomicity and preservation
- Example emission patterns

---

### 3. `kain_runtime_union.h`

**Purpose:** C-compatible union access operations

**Helpers Declared (3 total):**

1. `void __kain_union_get(...)` - Read union field with type-safe access
2. `void __kain_union_set(...)` - Write union field with deterministic zeroing
3. `void __kain_union_wrap(...)` - Initialize union during aggregate initialization

**Key Features:**
- Type punning: allowed (C-compatible)
- Padding bytes: undefined (do not rely on them)
- Active field tracking: NOT automatic (application responsibility)
- Initialization: entire union is zeroed before field write

**Documentation:**
- Algorithm descriptions for get/set/wrap operations
- Union semantics and safety notes
- ABI considerations for type punning
- Example emission patterns

---

## Total Helper Count

**13 canonical helpers declared across 3 header files:**

| Category | Helper Count | Header File |
|----------|--------------|-------------|
| Pointer/Address Operations | 5 | `kain_runtime_memory.h` |
| Memory Load/Store | 2 | `kain_runtime_memory.h` |
| Allocation Operations | 2 | `kain_runtime_memory.h` |
| Bitfield Operations | 2 | `kain_runtime_bitfield.h` |
| Union Operations | 3 | `kain_runtime_union.h` |
| **TOTAL** | **13** | **3 headers** |

---

## ABI Compatibility

All helpers are declared with C linkage (`extern "C"`) for compatibility with:
- LLVM backend (`kain-sys-codegen/codegen_llvm`)
- C++ backend (`kain-sys-codegen/codegen_cpp`)
- Native C runtime (`runtime/native/src/core`)

**Target Support:**
- ✅ x86_64 Linux (64-bit pointers, little-endian, LSB-first bitfields)
- ✅ x86_64 Windows (64-bit pointers, little-endian, LSB-first bitfields)
- ✅ ARM64 (64-bit pointers, little-endian, LSB-first bitfields)
- ⚠️ WASM32 (32-bit pointers, limited pointer support)
- ⚠️ WASM64 (64-bit pointers, limited pointer support)

---

## Documentation Standards

Each helper declaration includes:

1. **Purpose:** High-level description of what the helper does
2. **Algorithm:** Step-by-step description of the operation (for complex helpers)
3. **Parameters:** Detailed parameter descriptions with types and semantics
4. **Returns:** Return value description and failure modes
5. **ABI Considerations:** Target-specific behavior, alignment, safety notes
6. **Example Emission:** Compiler emission pattern showing usage context

---

## Next Steps

**Task 4.3:** Implement pointer and memory helpers in `runtime/native/src/core/kain_runtime_memory.c`

**Task 4.4:** Implement bitfield helpers in `runtime/native/src/core/kain_runtime_bitfield.c`

**Task 4.5:** Implement union helpers in `runtime/native/src/core/kain_runtime_union.c`

**Task 4.6:** Add conformance tests in `runtime/conformance/07_low_level_memory/`

---

## References

- **Checklist:** `runtime/LOW_LEVEL_HELPER_IMPLEMENTATION_CHECKLIST.md`
- **Compiler Source:** `crates/kain-core/src/low_level_memory.rs`
- **Design Document:** `crates/kain-core/LOW_LEVEL_MEMORY_LAYER_DESIGN.md`
- **Requirements:** `.kiro/specs/kain-native-runtime-completion/requirements.md` (Req 3.1, 3.2, 3.6)

---

## Validation

**Header Validation:**
- ✅ All 13 helpers from checklist are declared
- ✅ C linkage (`extern "C"`) for all helpers
- ✅ Include guards present in all headers
- ✅ Comprehensive documentation for each helper
- ✅ ABI considerations documented
- ✅ Example emission patterns provided
- ✅ Cross-references to source documents

**Completeness Check:**

| Helper | Checklist | Header Declared | Implementation | Tests |
|--------|-----------|-----------------|----------------|-------|
| `__kain_bind_local` | ✅ | ✅ | ⏳ Task 4.3 | ⏳ Task 4.6 |
| `__kain_addr_of` | ✅ | ✅ | ⏳ Task 4.3 | ⏳ Task 4.6 |
| `__kain_ptr_offset` | ✅ | ✅ | ⏳ Task 4.3 | ⏳ Task 4.6 |
| `__kain_field_ptr` | ✅ | ✅ | ⏳ Task 4.3 | ⏳ Task 4.6 |
| `__kain_index_ptr` | ✅ | ✅ | ⏳ Task 4.3 | ⏳ Task 4.6 |
| `__kain_mem_load` | ✅ | ✅ | ⏳ Task 4.3 | ⏳ Task 4.6 |
| `__kain_mem_store` | ✅ | ✅ | ⏳ Task 4.3 | ⏳ Task 4.6 |
| `__kain_bitfield_get` | ✅ | ✅ | ⏳ Task 4.4 | ⏳ Task 4.6 |
| `__kain_bitfield_set` | ✅ | ✅ | ⏳ Task 4.4 | ⏳ Task 4.6 |
| `__kain_union_get` | ✅ | ✅ | ⏳ Task 4.5 | ⏳ Task 4.6 |
| `__kain_union_set` | ✅ | ✅ | ⏳ Task 4.5 | ⏳ Task 4.6 |
| `__kain_union_wrap` | ✅ | ✅ | ⏳ Task 4.5 | ⏳ Task 4.6 |
| `__kain_alloc` | ✅ | ✅ | ⏳ Task 4.3 | ⏳ Task 4.6 |
| `__kain_realloc` | ✅ | ✅ | ⏳ Task 4.3 | ⏳ Task 4.6 |

**Legend:**
- ✅ Complete
- ⏳ Pending (scheduled in subsequent tasks)
- ❌ Not started

---

**Task 4.2 Status:** ✅ COMPLETE

All canonical helper declarations have been added to native headers under `runtime/native/include/` with comprehensive documentation, ABI specifications, and example emission patterns.
