#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include "bitfield.h"
#include "union.h"

/*
 * KAIN Native Runtime Low-Level Memory Helpers
 *
 * This header defines the canonical low-level memory helper ABI for the KAIN
 * native runtime. These helpers provide the bridge between compiler-emitted
 * code and native memory operations.
 *
 * Requirements Coverage:
 * - Requirement 3.1: Canonical low-level helper surface
 * - Requirement 3.2: Address-of, bind-local, load/store operations
 * - Requirement 3.6: Pointer and allocation helper behavior
 *
 * Source: runtime/LOW_LEVEL_HELPER_IMPLEMENTATION_CHECKLIST.md
 * Compiler: crates/core/src/low_level_memory.rs
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef union KainAllocHeader {
    struct {
        uint64_t magic_and_slot;
        size_t payload_size;
    } metadata;
    long double alignment;
} KainAllocHeader;

#define KAIN_ALLOC_HEADER_SLOT_TOKEN_MASK UINT64_C(0x000000000000ffff)
#define KAIN_ALLOC_HEADER_MAGIC_TAG UINT64_C(0x4b41494e4d450000)

static inline uint64_t __kain_alloc_header_magic_with_slot(uint16_t slot_token) {
    return KAIN_ALLOC_HEADER_MAGIC_TAG | (uint64_t)slot_token;
}

static inline void __kain_alloc_header_set_magic_and_slot(
    KainAllocHeader* header,
    uint16_t slot_token
) {
    header->metadata.magic_and_slot = __kain_alloc_header_magic_with_slot(slot_token);
}

static inline uint16_t __kain_alloc_header_slot_token(const KainAllocHeader* header) {
    if (header == NULL) {
        return 0u;
    }
    if ((header->metadata.magic_and_slot & ~KAIN_ALLOC_HEADER_SLOT_TOKEN_MASK)
        != KAIN_ALLOC_HEADER_MAGIC_TAG) {
        return 0u;
    }
    return (uint16_t)(header->metadata.magic_and_slot & KAIN_ALLOC_HEADER_SLOT_TOKEN_MASK);
}

static inline int __kain_alloc_header_is_valid(const KainAllocHeader* header) {
    return __kain_alloc_header_slot_token(header) != 0u;
}

static inline void* __kain_alloc_payload_from_header(KainAllocHeader* header) {
    return (void*)(header + 1);
}

static inline KainAllocHeader* __kain_alloc_header_from_payload(const void* ptr) {
    return ((KainAllocHeader*)ptr) - 1;
}

/* ============================================================================
 * Category 1: Pointer and Address Operations
 * ============================================================================ */

/*
 * __kain_bind_local
 *
 * Create a pointer binding to a local variable that has its address taken.
 *
 * Purpose:
 *   When address-taken analysis detects a variable needs a stable pointer,
 *   this helper returns the address of the variable's storage location.
 *
 * Parameters:
 *   ptr - Pointer to the value to bind
 *
 * Returns:
 *   Stable pointer to the value (same as input for stack/heap variables)
 *
 * ABI Considerations:
 *   - Pointer must remain valid for the variable's lifetime
 *   - For stack variables: returns address of stack slot
 *   - For heap variables: returns existing heap pointer
 *   - Handles both mutable and immutable bindings
 *
 * Example Emission:
 *   let x = 42
 *   let ptr = addr_of(x)
 *   // Compiler emits: let __kain_ptr_x = __kain_bind_local(&x)
 */
void* __kain_bind_local(void* ptr);

/*
 * __kain_addr_of
 *
 * Take the address of a value expression (fallback when bind_local not applicable).
 *
 * Purpose:
 *   When taking address of non-addressable expression (rvalue), this helper
 *   may allocate temporary storage and return a pointer to it.
 *
 * Parameters:
 *   ptr - Pointer to the value
 *   size - Size of the value in bytes
 *
 * Returns:
 *   Pointer to storage containing the value
 *
 * ABI Considerations:
 *   - May allocate temporary storage for rvalue
 *   - Storage lifetime must extend to pointer use
 *   - Consider using stack allocation for small values
 *   - Cleanup/deallocation strategy is implementation-defined
 *
 * Example Emission:
 *   let ptr = addr_of(some_function_call())
 *   // Compiler emits: let ptr = __kain_addr_of(&temp, sizeof(temp))
 */
void* __kain_addr_of(void* ptr, size_t size);

/*
 * __kain_ptr_offset
 *
 * Perform pointer arithmetic with explicit stride.
 *
 * Purpose:
 *   Compute pointer offset for array-like access with explicit element size.
 *
 * Parameters:
 *   ptr - Base pointer
 *   offset - Number of elements to offset (may be negative)
 *   stride - Size of each element in bytes
 *
 * Returns:
 *   Pointer offset by (offset * stride) bytes
 *
 * ABI Considerations:
 *   - Computes: ptr + (offset * stride)
 *   - Handles negative offsets
 *   - No bounds checking (unsafe operation)
 *   - Arithmetic overflow returns NULL and sets errno to ERANGE
 *   - Result may be misaligned
 *
 * Example Emission:
 *   let ptr = base_ptr.offset(10)
 *   // Compiler emits: let ptr = __kain_ptr_offset(base_ptr, 10, sizeof(T))
 */
void* __kain_ptr_offset(void* ptr, int64_t offset, int64_t stride);

/*
 * __kain_field_ptr
 *
 * Compute pointer to struct field given base pointer and field offset.
 *
 * Purpose:
 *   Calculate the address of a struct field for address-taken field access.
 *
 * Parameters:
 *   ptr - Pointer to struct base
 *   field - Field name (for diagnostics/debugging only)
 *   offset - Byte offset of field from struct base
 *
 * Returns:
 *   Byte pointer to the field (cast to appropriate type at use site)
 *
 * ABI Considerations:
 *   - Computes: ptr + offset
 *   - Field name is for diagnostics only, not validated
 *   - Offset is pre-computed by layout engine
 *   - Arithmetic overflow returns NULL and sets errno to ERANGE
 *   - Result pointer may require alignment adjustment
 *   - Bitfield fields are NOT handled by this helper (see bitfield helpers)
 *
 * Example Emission:
 *   let field_ptr = addr_of(obj.field)
 *   // Compiler emits: let field_ptr = __kain_field_ptr(__kain_ptr_obj, "field", 16)
 */
void* __kain_field_ptr(void* ptr, const char* field, size_t offset);

/*
 * __kain_index_ptr
 *
 * Compute pointer to array element.
 *
 * Purpose:
 *   Calculate the address of an array element for address-taken array access.
 *
 * Parameters:
 *   ptr - Pointer to array base
 *   index - Element index (may be negative for pointer arithmetic)
 *   stride - Size of each element in bytes
 *
 * Returns:
 *   Pointer to the indexed element
 *
 * ABI Considerations:
 *   - Computes: ptr + (index * stride)
 *   - Identical to __kain_ptr_offset but semantically distinct
 *   - No bounds checking
 *   - Handles negative indices
 *   - Arithmetic overflow returns NULL and sets errno to ERANGE
 *
 * Example Emission:
 *   let elem_ptr = addr_of(arr[5])
 *   // Compiler emits: let elem_ptr = __kain_index_ptr(__kain_ptr_arr, 5, sizeof(T))
 */
void* __kain_index_ptr(void* ptr, int64_t index, int64_t stride);

/* ============================================================================
 * Category 2: Memory Load/Store Operations
 * ============================================================================ */

/*
 * __kain_mem_load
 *
 * Load value from pointer (raw memory read).
 *
 * Purpose:
 *   Read arbitrary bytes from a pointer without type safety.
 *
 * Parameters:
 *   ptr - Pointer to read from
 *   out - Pointer to output buffer
 *   size - Number of bytes to read
 *
 * ABI Considerations:
 *   - Reads 'size' bytes from ptr into out
 *   - No alignment checking (unsafe)
 *   - No null checking (unsafe)
 *   - Preserves bit pattern for unions/bitfields
 *   - Must respect target endianness
 *   - Unaligned loads may trap on some architectures (ARM, older x86)
 *   - Volatile semantics NOT guaranteed (use explicit volatile load if needed)
 *
 * Example Emission:
 *   let val = mem_load(ptr)
 *   // Compiler emits: __kain_mem_load(ptr, &val, sizeof(val))
 */
void __kain_mem_load(const void* ptr, void* out, size_t size);

/*
 * __kain_mem_store
 *
 * Store value to pointer (raw memory write).
 *
 * Purpose:
 *   Write arbitrary bytes to a pointer without type safety.
 *
 * Parameters:
 *   ptr - Pointer to write to
 *   value - Pointer to value to write
 *   size - Number of bytes to write
 *
 * ABI Considerations:
 *   - Writes 'size' bytes from value to ptr
 *   - No alignment checking (unsafe)
 *   - No null checking (unsafe)
 *   - Preserves bit pattern for unions/bitfields
 *   - Must respect target endianness
 *   - Unaligned stores may trap on some architectures
 *   - Volatile semantics NOT guaranteed
 *
 * Example Emission:
 *   mem_store(ptr, 42)
 *   // Compiler emits: __kain_mem_store(ptr, &temp, sizeof(temp))
 */
void __kain_mem_store(void* ptr, const void* value, size_t size);

/* Volatile byte-exact load/store for MMIO and externally-observed memory. */
void __kain_volatile_load(const void* ptr, void* out, size_t size);
void __kain_volatile_store(void* ptr, const void* value, size_t size);

enum {
    KAIN_MEMORY_ORDER_RELAXED = 0,
    KAIN_MEMORY_ORDER_ACQUIRE = 1,
    KAIN_MEMORY_ORDER_RELEASE = 2,
    KAIN_MEMORY_ORDER_ACQ_REL = 3,
    KAIN_MEMORY_ORDER_SEQ_CST = 4
};

/*
 * __kain_atomic_*_seqcst
 *
 * Canonical seq-cst atomic helpers over 64-bit storage cells. The LLVM backend
 * lowers authored atomics directly where possible; these helpers keep the native
 * ABI truthful for runtime-backed and fallback surfaces.
 */
int64_t __kain_atomic_load_seqcst(const void* ptr);
void __kain_atomic_store_seqcst(void* ptr, int64_t value);
int64_t __kain_atomic_add_seqcst(void* ptr, int64_t delta);
int64_t __kain_atomic_sub_seqcst(void* ptr, int64_t delta);
int64_t __kain_atomic_and_seqcst(void* ptr, int64_t mask);
int64_t __kain_atomic_or_seqcst(void* ptr, int64_t bits);
int64_t __kain_atomic_xor_seqcst(void* ptr, int64_t bits);
int64_t __kain_atomic_exchange_seqcst(void* ptr, int64_t value);
int __kain_atomic_compare_exchange_seqcst(void* ptr, int64_t expected, int64_t desired);
int64_t __kain_atomic_load_ordered(const void* ptr, int64_t ordering);
void __kain_atomic_store_ordered(void* ptr, int64_t value, int64_t ordering);
int64_t __kain_atomic_add_ordered(void* ptr, int64_t delta, int64_t ordering);
int64_t __kain_atomic_sub_ordered(void* ptr, int64_t delta, int64_t ordering);
int64_t __kain_atomic_and_ordered(void* ptr, int64_t mask, int64_t ordering);
int64_t __kain_atomic_or_ordered(void* ptr, int64_t bits, int64_t ordering);
int64_t __kain_atomic_xor_ordered(void* ptr, int64_t bits, int64_t ordering);
int64_t __kain_atomic_exchange_ordered(void* ptr, int64_t value, int64_t ordering);
int __kain_atomic_compare_exchange_ordered(
    void* ptr,
    int64_t expected,
    int64_t desired,
    int64_t success_ordering,
    int64_t failure_ordering
);
void __kain_atomic_fence(int64_t ordering);

/* ============================================================================
 * Category 3: Allocation Operations
 * ============================================================================ */

/*
 * __kain_alloc
 *
 * Allocate heap memory with optional zero-initialization.
 *
 * Purpose:
 *   Allocate a block of memory on the heap, optionally zeroed.
 *
 * Parameters:
 *   size - Number of elements to allocate
 *   stride - Size of each element in bytes
 *   zeroed - If non-zero, zero-initialize the memory
 *
 * Returns:
 *   Pointer to allocated memory, or NULL on allocation failure
 *
 * ABI Considerations:
 *   - Allocates (size * stride) bytes
 *   - If zeroed is non-zero, memory is zero-initialized
 *   - Helper-owned allocations carry internal metadata for realloc correctness
 *   - Alignment: natural alignment for stride size
 *   - Returns NULL on allocation failure (no exceptions)
 *   - Allocation strategy: malloc/calloc or custom allocator
 *
 * Example Emission:
 *   let buffer = alloc(1024, u8, zeroed: true)
 *   // Compiler emits: let buffer = __kain_alloc(1024, 1, 1)
 */
void* __kain_alloc(size_t size, size_t stride, int zeroed);

/*
 * __kain_realloc
 *
 * Resize heap allocation with optional zero-fill of new bytes.
 *
 * Purpose:
 *   Resize an existing heap allocation, optionally zeroing new bytes.
 *
 * Parameters:
 *   ptr - Pointer to existing allocation (or NULL for new allocation)
 *   size - New number of elements
 *   stride - Size of each element in bytes
 *   zeroed_new - If non-zero, zero-fill new bytes when growing
 *
 * Returns:
 *   Pointer to resized memory (may be different address), or NULL on failure
 *
 * ABI Considerations:
 *   - Resizes allocation to (size * stride) bytes
 *   - Preserves existing data
 *   - If zeroed_new and size increased, zero-fills new bytes only
 *   - Supported for helper-owned allocations returned by __kain_alloc/__kain_realloc
 *   - May move memory (return different address)
 *   - On failure, original pointer remains valid
 *   - If ptr is NULL, behaves like __kain_alloc
 *   - Invalid or foreign pointers fail in a controlled way and return NULL
 *
 * Example Emission:
 *   let bigger = realloc(buffer, 2048, u8, zeroed_new: true)
 *   // Compiler emits: let bigger = __kain_realloc(buffer, 2048, 1, 1)
 */
void* __kain_realloc(void* ptr, size_t size, size_t stride, int zeroed_new);

/*
 * __kain_free
 *
 * Release a helper-owned allocation produced by __kain_alloc/__kain_realloc.
 * Returns 0 on success and -1 for invalid or foreign pointers.
 */
int __kain_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif /* MEMORY_H */
