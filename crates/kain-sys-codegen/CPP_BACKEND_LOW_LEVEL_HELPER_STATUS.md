# C++ Backend Low-Level Helper Status

**Task:** 4.5 Improve C++ backend clarity or parity path  
**Date:** 2025-01-XX  
**Spec:** `.kiro/specs/kain-native-runtime-completion`

## Overview

The C++ backend (`crates/kain-sys-codegen/src/codegen_cpp/mod.rs`) provides **partial support** for the canonical low-level helper surface defined in `runtime/native/include/kain_runtime_memory.h`.

This document tracks which helpers are supported, which are unsupported, and the path forward for achieving full ABI parity.

## Current Status

### ✅ SUPPORTED Helpers

The following helpers are **fully implemented** as inline C++ template functions:

#### Union Operations
- `__kain_union_wrap` - Initialize union with active field
- `__kain_union_get` - Read union field with type-safe access
- `__kain_union_set` - Write union field with type-safe access

**Implementation:** Template functions using `std::memcpy` for type punning (C++ compatible)

#### Bitfield Operations
- `__kain_bitfield_get` - Extract bitfield value from struct
- `__kain_bitfield_set` - Write bitfield value to struct
- `__kain_load_bitfield_unit` - Load 8-byte bitfield unit
- `__kain_store_bitfield_unit` - Store 8-byte bitfield unit
- `__kain_bitfield_mask` - Generate bitfield mask
- `__kain_sign_extend` - Sign-extend bitfield value

**Implementation:** Template functions matching the canonical ABI behavior

#### Allocation Operations (Partial)
- `__kain_alloc` - Heap allocation with optional zero-initialization
- `__kain_realloc` - Heap reallocation (zeroed_new not fully implemented)

**Implementation:** Inline wrappers around `std::malloc`, `std::calloc`, `std::realloc`

**Limitation:** `__kain_realloc` does not fully implement the `zeroed_new` parameter (would require tracking old allocation size)

---

### ❌ UNSUPPORTED Helpers

The following helpers are **not implemented** and will cause **linker errors** if the compiler emits calls to them:

#### Pointer Operations
- `__kain_bind_local` - Create pointer binding to local variable
- `__kain_addr_of` - Take address of value expression
- `__kain_ptr_offset` - Pointer arithmetic with explicit stride
- `__kain_field_ptr` - Compute pointer to struct field
- `__kain_index_ptr` - Compute pointer to array element

**Reason:** The C++ backend currently generates inline pointer arithmetic and does not emit calls to these helpers. Forward declarations exist to make unsupported usage explicit.

#### Memory Load/Store Operations
- `__kain_mem_load` - Load value from pointer (raw memory read)
- `__kain_mem_store` - Store value to pointer (raw memory write)

**Reason:** The C++ backend generates direct memory access code and does not emit calls to these helpers.

---

## Why Partial Support?

The C++ backend was designed as a **transpiler** that generates idiomatic C++ code rather than a low-level ABI-compliant backend. It:

1. **Generates inline code** for most operations instead of calling runtime helpers
2. **Uses C++ language features** (templates, references, operator overloading) instead of explicit pointer manipulation
3. **Targets modern C++17** with standard library support rather than bare-metal C ABI

This approach works well for:
- Rapid prototyping and testing
- Generating human-readable C++ code
- Leveraging C++ type safety and standard library

But it means the C++ backend **does not emit calls** to the canonical pointer/memory helpers that the LLVM backend and native C runtime use.

---

## Path Forward

### Option A: Keep Current Approach (Recommended for Now)

**Status:** ✅ Implemented in Task 4.5

- Document which helpers are supported vs unsupported
- Add forward declarations for unsupported helpers (causes linker errors if used)
- Add clear comments in generated code explaining the limitation
- Focus full ABI parity efforts on LLVM backend + native C runtime

**Pros:**
- No breaking changes to existing C++ backend
- Clear failure mode (linker error) if unsupported features are used
- Allows C++ backend to remain a transpiler rather than an ABI-compliant backend

**Cons:**
- C++ backend cannot be used for programs that require low-level pointer operations
- Divergence from canonical ABI remains

---

### Option B: Emit Helper Calls (Future Work)

**Status:** ⚠️ Not implemented (future task)

Modify the C++ backend to emit calls to the canonical helpers instead of inline code:

1. **Pointer operations:** Emit `__kain_ptr_offset(ptr, offset, stride)` instead of `ptr + offset`
2. **Memory operations:** Emit `__kain_mem_load(ptr, &out, size)` instead of `*ptr`
3. **Provide runtime library:** Ship a C++ runtime library with helper implementations

**Pros:**
- Full ABI parity with LLVM backend and native C runtime
- Consistent behavior across all backends
- Enables advanced low-level memory features in C++ output

**Cons:**
- Requires significant refactoring of C++ codegen
- Generated code becomes less idiomatic C++
- Requires shipping a runtime library with C++ output
- May break existing C++ backend users

**Estimated Effort:** 3-5 days

---

### Option C: Hybrid Approach (Future Work)

**Status:** ⚠️ Not implemented (future task)

Emit helper calls only when necessary, inline code otherwise:

1. **Simple operations:** Generate inline C++ code (current behavior)
2. **Complex operations:** Emit calls to canonical helpers
3. **Provide optional runtime library:** Ship helpers for programs that need them

**Pros:**
- Best of both worlds: idiomatic C++ for simple cases, ABI parity for complex cases
- Gradual migration path
- Minimal breaking changes

**Cons:**
- More complex codegen logic
- Harder to reason about when helpers are used vs inline code
- Still requires runtime library for some programs

**Estimated Effort:** 5-7 days

---

## Testing Strategy

### Current Tests (Task 4.5)

✅ **Compile-time validation:**
- C++ backend compiles without errors
- Generated code includes helper declarations
- Comments document supported vs unsupported helpers

✅ **Documentation:**
- This status document
- Inline comments in generated C++ code
- Forward declarations for unsupported helpers

### Future Tests (Option B or C)

If full ABI parity is implemented:

1. **Conformance tests:** Port tests from `runtime/conformance/07_low_level_memory/` to C++
2. **Parity tests:** Compare C++ backend output against LLVM backend for same input
3. **Runtime tests:** Validate helper behavior matches canonical ABI
4. **Integration tests:** Test C++ output with native C runtime

---

## Recommendations

### For Task 4.5 (Current)

✅ **DONE:**
1. Document supported vs unsupported helpers (this file)
2. Add forward declarations for unsupported helpers
3. Add clear comments in generated C++ code
4. Make unsupported usage fail explicitly (linker error)

### For Future Work

⚠️ **DEFER to later tasks:**
1. Full ABI parity (Option B or C) should be a separate task
2. Focus current efforts on LLVM backend + native C runtime parity
3. Revisit C++ backend parity after LLVM/native parity is complete
4. Consider whether C++ backend should remain a transpiler or become ABI-compliant

---

## Related Files

- **Canonical ABI:** `runtime/native/include/kain_runtime_memory.h`
- **Helper Checklist:** `runtime/LOW_LEVEL_HELPER_IMPLEMENTATION_CHECKLIST.md`
- **C++ Backend:** `crates/kain-sys-codegen/src/codegen_cpp/mod.rs`
- **LLVM Backend:** `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
- **Low-Level Memory:** `crates/kain-core/src/low_level_memory.rs`

---

## Requirements Coverage

This document and the Task 4.5 implementation satisfy:

- **Requirement 1.4:** Backend/runtime helper parity (documented divergence)
- **Requirement 3.5:** Unsupported low-level behavior fails explicitly (linker error)
- **Requirement 14.5:** Implementation shortcuts documented as temporary limitations

---

**Document Status:** ✅ COMPLETE  
**Next Steps:** Focus on LLVM backend + native C runtime parity (Tasks 4.4, 4.6)
