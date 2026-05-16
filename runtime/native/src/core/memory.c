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
 * Headers: runtime/native/include/memory.h
 */

#include "../../include/memory.h"
#include "../../include/ownership.h"
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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

static int kain_mul_overflow_i64(int64_t left, int64_t right, int64_t* out) {
    if (left == 0 || right == 0) {
        *out = 0;
        return 0;
    }

    if ((left > 0 && right > 0 && left > (INT64_MAX / right))
        || (left > 0 && right < 0 && right < (INT64_MIN / left))
        || (left < 0 && right > 0 && left < (INT64_MIN / right))
        || (left < 0 && right < 0 && left < (INT64_MAX / right))) {
        return 1;
    }

    *out = left * right;
    return 0;
}

static int kain_add_overflow_uintptr(uintptr_t left, uintptr_t right, uintptr_t* out) {
    if (right > (UINTPTR_MAX - left)) {
        return 1;
    }
    *out = left + right;
    return 0;
}

static int kain_sub_underflow_uintptr(uintptr_t left, uintptr_t right, uintptr_t* out) {
    if (right > left) {
        return 1;
    }
    *out = left - right;
    return 0;
}

static int kain_pointer_with_signed_byte_offset(void* ptr, int64_t byte_offset, void** out) {
    uintptr_t base_address = (uintptr_t)ptr;
    uintptr_t result_address = 0;

    if (byte_offset >= 0) {
        if (kain_add_overflow_uintptr(base_address, (uintptr_t)byte_offset, &result_address)) {
            return 1;
        }
    } else {
        uint64_t magnitude = (uint64_t)(-(byte_offset + 1)) + 1;
        if (magnitude > (uint64_t)UINTPTR_MAX
            || kain_sub_underflow_uintptr(base_address, (uintptr_t)magnitude, &result_address)) {
            return 1;
        }
    }

    *out = (void*)result_address;
    return 0;
}

static int kain_pointer_with_size_offset(void* ptr, size_t byte_offset, void** out) {
    uintptr_t result_address = 0;
    if (kain_add_overflow_uintptr((uintptr_t)ptr, (uintptr_t)byte_offset, &result_address)) {
        return 1;
    }
    *out = (void*)result_address;
    return 0;
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
    int64_t byte_offset = 0;
    void* result = NULL;

    if (kain_mul_overflow_i64(offset, stride, &byte_offset)
        || kain_pointer_with_signed_byte_offset(ptr, byte_offset, &result)) {
        errno = ERANGE;
        return NULL;
    }

    return result;
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

    void* result = NULL;
    if (kain_pointer_with_size_offset(ptr, offset, &result)) {
        errno = ERANGE;
        return NULL;
    }

    return result;
}

/*
 * __kain_index_ptr
 *
 * Compute pointer to array element.
 * Semantically distinct from ptr_offset but identical implementation.
 */
void* __kain_index_ptr(void* ptr, int64_t index, int64_t stride) {
    int64_t byte_offset = 0;
    void* result = NULL;

    if (kain_mul_overflow_i64(index, stride, &byte_offset)
        || kain_pointer_with_signed_byte_offset(ptr, byte_offset, &result)) {
        errno = ERANGE;
        return NULL;
    }

    return result;
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
    uint16_t slot_token = 0u;

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

    header->metadata.payload_size = payload_size;
    void* payload = __kain_alloc_payload_from_header(header);
    if (__kain_ownership_register_helper_allocation(
            payload,
            payload_size,
            &slot_token
        ) != KAIN_OWNERSHIP_OK) {
        header->metadata.magic_and_slot = 0;
        free(header);
        return NULL;
    }
    __kain_alloc_header_set_magic_and_slot(header, slot_token);
    return payload;
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
    KainAllocHeader* old_header = __kain_alloc_header_from_payload(ptr);
    KainAllocHeader* new_header = NULL;
    size_t old_payload_size = 0;
    uint16_t slot_token = 0u;

    if (!__kain_alloc_header_is_valid(old_header)) {
        errno = EINVAL;
        return NULL;
    }

    old_payload_size = old_header->metadata.payload_size;
    slot_token = __kain_alloc_header_slot_token(old_header);
    if (slot_token == 0u) {
        errno = EINVAL;
        return NULL;
    }
    int ownership_state = __kain_ownership_helper_allocation_state(ptr, slot_token);
    if (ownership_state != KAIN_OWNERSHIP_STATE_IDLE) {
        errno = ownership_state == KAIN_OWNERSHIP_ERR_NOT_FOUND ? EINVAL : EBUSY;
        return NULL;
    }

    if (kain_mul_overflow_size(size, stride, &new_payload_size)
        || kain_add_overflow_size(sizeof(KainAllocHeader), new_payload_size, &allocation_size)) {
        errno = ENOMEM;
        return NULL;
    }

    new_header = (KainAllocHeader*)realloc(old_header, allocation_size);
    if (new_header == NULL) {
        return NULL;
    }

    new_header->metadata.payload_size = new_payload_size;
    __kain_alloc_header_set_magic_and_slot(new_header, slot_token);

    if (zeroed_new && new_payload_size > old_payload_size) {
        memset(
            ((char*)__kain_alloc_payload_from_header(new_header)) + old_payload_size,
            0,
            new_payload_size - old_payload_size
        );
    }

    void* payload = __kain_alloc_payload_from_header(new_header);
    if (__kain_ownership_relocate_helper_allocation(ptr, payload, new_payload_size, slot_token)
        != KAIN_OWNERSHIP_OK) {
        return payload;
    }

    return payload;
}

int __kain_free(void* ptr) {
    if (ptr == NULL) {
        return 0;
    }

    KainAllocHeader* header = __kain_alloc_header_from_payload(ptr);
    if (!__kain_alloc_header_is_valid(header)) {
        errno = EINVAL;
        return -1;
    }

    header->metadata.magic_and_slot = 0;
    header->metadata.payload_size = 0;
    free(header);
    return 0;
}
