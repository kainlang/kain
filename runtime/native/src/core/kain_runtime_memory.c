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
#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef union KainAllocHeader {
    struct {
        uint64_t magic;
        size_t payload_size;
    } metadata;
    max_align_t alignment;
} KainAllocHeader;

static const uint64_t KAIN_ALLOC_MAGIC = 0x4b41494e4d454d31ULL;

static int kain_mul_overflow_size(size_t left, size_t right, size_t* out) {
    if (left != 0 && right > (SIZE_MAX / left)) {
        return 1;
    }
    *out = left * right;
    return 0;
}

static int kain_add_overflow_size(size_t left, size_t right, size_t* out) {
    if (right > (SIZE_MAX - left)) {
        return 1;
    }
    *out = left + right;
    return 0;
}

static void* kain_payload_from_header(KainAllocHeader* header) {
    return (void*)(header + 1);
}

static KainAllocHeader* kain_header_from_payload(void* ptr) {
    return ((KainAllocHeader*)ptr) - 1;
}

static int kain_validate_helper_header(KainAllocHeader* header) {
    return header != NULL && header->metadata.magic == KAIN_ALLOC_MAGIC;
}

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
    size_t payload_size = 0;
    size_t allocation_size = 0;
    KainAllocHeader* header = NULL;

    if (kain_mul_overflow_size(size, stride, &payload_size)
        || kain_add_overflow_size(sizeof(KainAllocHeader), payload_size, &allocation_size)) {
        errno = ENOMEM;
        return NULL;
    }

    if (zeroed) {
        header = (KainAllocHeader*)calloc(1, allocation_size);
    } else {
        header = (KainAllocHeader*)malloc(allocation_size);
    }
    if (header == NULL) {
        return NULL;
    }

    header->metadata.magic = KAIN_ALLOC_MAGIC;
    header->metadata.payload_size = payload_size;
    return kain_payload_from_header(header);
}

/*
 * __kain_realloc
 *
 * Resize heap allocation with optional zero-fill of new bytes.
 * If ptr is NULL, behaves like __kain_alloc.
 */
void* __kain_realloc(void* ptr, size_t size, size_t stride, int zeroed_new) {
    if (ptr == NULL) {
        return __kain_alloc(size, stride, zeroed_new);
    }

    size_t new_payload_size = 0;
    size_t allocation_size = 0;
    KainAllocHeader* old_header = kain_header_from_payload(ptr);
    KainAllocHeader* new_header = NULL;
    size_t old_payload_size = 0;

    if (!kain_validate_helper_header(old_header)) {
        errno = EINVAL;
        return NULL;
    }

    if (kain_mul_overflow_size(size, stride, &new_payload_size)
        || kain_add_overflow_size(sizeof(KainAllocHeader), new_payload_size, &allocation_size)) {
        errno = ENOMEM;
        return NULL;
    }

    old_payload_size = old_header->metadata.payload_size;
    new_header = (KainAllocHeader*)realloc(old_header, allocation_size);
    if (new_header == NULL) {
        return NULL;
    }

    new_header->metadata.magic = KAIN_ALLOC_MAGIC;
    new_header->metadata.payload_size = new_payload_size;

    if (zeroed_new && new_payload_size > old_payload_size) {
        memset(
            ((char*)kain_payload_from_header(new_header)) + old_payload_size,
            0,
            new_payload_size - old_payload_size
        );
    }

    return kain_payload_from_header(new_header);
}
