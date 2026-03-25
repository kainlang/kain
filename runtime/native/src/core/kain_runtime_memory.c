/*
 * KAIN Native Runtime Low-Level Memory Helpers
 *
 * Implementation of canonical low-level memory helper ABI for the KAIN
 * native runtime. These helpers provide the bridge between compiler-emitted
 * code and native memory operations.
 *
 * Requirements Coverage:
 * - Requirement 3.2: Address-of, bind-local, load/store operations
 * - Requirement 3.3: Pointer and allocation helper behavior
 * - Requirement 3.6: Memory layout and ABI policy
 *
 * Source: runtime/LOW_LEVEL_HELPER_IMPLEMENTATION_CHECKLIST.md
 * Headers: runtime/native/include/kain_runtime_memory.h
 */

#include "../../include/kain_runtime_memory.h"
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * Category 1: Pointer and Address Operations
 * ============================================================================ */

/*
 * __kain_bind_local
 *
 * Create a pointer binding to a local variable that has its address taken.
 * For stack/heap variables, this simply returns the input pointer.
 */
void* __kain_bind_local(void* ptr) {
    /* For stack and heap variables, the pointer is already stable.
     * This helper exists for ABI consistency and potential future
     * provenance tracking. */
    return ptr;
}

/*
 * __kain_addr_of
 *
 * Take the address of a value expression.
 * For rvalues, the caller has already placed the value in temporary storage,
 * so we just return the pointer to that storage.
 */
void* __kain_addr_of(void* ptr, size_t size) {
    /* The compiler has already allocated temporary storage for the rvalue
     * and passed us a pointer to it. We simply return that pointer.
     * The size parameter is provided for potential future validation. */
    (void)size; /* Unused in current implementation */
    return ptr;
}

/*
 * __kain_ptr_offset
 *
 * Perform pointer arithmetic with explicit stride.
 * Computes: ptr + (offset * stride)
 */
void* __kain_ptr_offset(void* ptr, int64_t offset, int64_t stride) {
    /* Cast to char* for byte-level arithmetic */
    char* base = (char*)ptr;
    int64_t byte_offset = offset * stride;
    return (void*)(base + byte_offset);
}

/*
 * __kain_field_ptr
 *
 * Compute pointer to struct field given base pointer and field offset.
 * The field name is for diagnostics only.
 */
void* __kain_field_ptr(void* ptr, const char* field, size_t offset) {
    /* Field name is for diagnostics/debugging only */
    (void)field;
    
    /* Cast to char* for byte-level arithmetic */
    char* base = (char*)ptr;
    return (void*)(base + offset);
}

/*
 * __kain_index_ptr
 *
 * Compute pointer to array element.
 * Semantically distinct from ptr_offset but identical implementation.
 */
void* __kain_index_ptr(void* ptr, int64_t index, int64_t stride) {
    /* Identical to __kain_ptr_offset but semantically represents array indexing */
    char* base = (char*)ptr;
    int64_t byte_offset = index * stride;
    return (void*)(base + byte_offset);
}

/* ============================================================================
 * Category 2: Memory Load/Store Operations
 * ============================================================================ */

/*
 * __kain_mem_load
 *
 * Load value from pointer (raw memory read).
 * Reads 'size' bytes from ptr into out.
 */
void __kain_mem_load(const void* ptr, void* out, size_t size) {
    /* Raw memory copy - no alignment or null checking (unsafe operation) */
    memcpy(out, ptr, size);
}

/*
 * __kain_mem_store
 *
 * Store value to pointer (raw memory write).
 * Writes 'size' bytes from value to ptr.
 */
void __kain_mem_store(void* ptr, const void* value, size_t size) {
    /* Raw memory copy - no alignment or null checking (unsafe operation) */
    memcpy(ptr, value, size);
}

/* ============================================================================
 * Category 3: Allocation Operations
 * ============================================================================ */

/*
 * __kain_alloc
 *
 * Allocate heap memory with optional zero-initialization.
 * Allocates (size * stride) bytes.
 */
void* __kain_alloc(size_t size, size_t stride, int zeroed) {
    size_t total_size = size * stride;
    void* ptr;
    
    if (zeroed) {
        /* Use calloc for zero-initialized memory */
        ptr = calloc(size, stride);
    } else {
        /* Use malloc for uninitialized memory */
        ptr = malloc(total_size);
    }
    
    /* Return NULL on allocation failure (no exceptions) */
    return ptr;
}

/*
 * __kain_realloc
 *
 * Resize heap allocation with optional zero-fill of new bytes.
 * If ptr is NULL, behaves like __kain_alloc.
 */
void* __kain_realloc(void* ptr, size_t size, size_t stride, int zeroed_new) {
    size_t new_size = size * stride;
    void* new_ptr;
    
    if (ptr == NULL) {
        /* If ptr is NULL, behave like __kain_alloc */
        return __kain_alloc(size, stride, zeroed_new);
    }
    
    if (zeroed_new) {
        /* Need to track old size to zero new bytes, but we don't have it.
         * For now, we use realloc and accept that new bytes are uninitialized.
         * A production implementation would need to track allocation sizes. */
        new_ptr = realloc(ptr, new_size);
        
        /* Note: This is a limitation - we cannot zero new bytes without
         * knowing the old size. A full implementation would require an
         * allocation tracking system. */
    } else {
        /* Simple realloc without zeroing */
        new_ptr = realloc(ptr, new_size);
    }
    
    /* Return NULL on failure, original pointer remains valid */
    return new_ptr;
}
