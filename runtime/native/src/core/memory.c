/*
 * KAIN Native Runtime Low-Level Memory Helpers
 *
 * Implementation of canonical low-level memory helper ABI for the KAIN
 * native runtime. These helpers provide the bridge between compiler-emitted
 * code and native memory operations.
 *
 *
 * Source: runtime/LOW_LEVEL_HELPER_IMPLEMENTATION_CHECKLIST.md
 * Headers: runtime/native/include/memory.h
 */

#include "../../include/memory.h"
#include "../../include/ownership.h"
#include <errno.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#define KAIN_ALLOC_CACHE_BUCKETS 64u
#define KAIN_ALLOC_CACHE_MIN_PAYLOAD 4096u
#define KAIN_ALLOC_CACHE_MAX_PAYLOAD 262144u
#define KAIN_ALLOC_CACHE_MAX_BYTES (8u * 1024u * 1024u)
#define KAIN_ALLOC_CACHE_MAX_NODES 256u

static KainAllocHeader* KAIN_ALLOC_CACHE[KAIN_ALLOC_CACHE_BUCKETS];
static size_t KAIN_ALLOC_CACHE_BYTES = 0;
static size_t KAIN_ALLOC_CACHE_NODES = 0;
static atomic_flag KAIN_ALLOC_CACHE_LOCK = ATOMIC_FLAG_INIT;

_Static_assert(sizeof(KainAllocHeader) == 16u, "KainAllocHeader proof constants require 16-byte header accounting.");

static int kain_add_overflow_size(size_t left, size_t right, size_t* out);

static void kain_alloc_cache_lock(void) {
    while (atomic_flag_test_and_set_explicit(&KAIN_ALLOC_CACHE_LOCK, memory_order_acquire)) {
    }
}

static void kain_alloc_cache_unlock(void) {
    atomic_flag_clear_explicit(&KAIN_ALLOC_CACHE_LOCK, memory_order_release);
}

static int kain_alloc_cache_eligible(size_t payload_size) {
    return payload_size >= KAIN_ALLOC_CACHE_MIN_PAYLOAD &&
        payload_size <= KAIN_ALLOC_CACHE_MAX_PAYLOAD &&
        payload_size >= sizeof(KainAllocHeader*);
}

static size_t kain_alloc_cache_bucket(size_t payload_size) {
    uint64_t mixed = (uint64_t)payload_size * UINT64_C(11400714819323198485);
    mixed ^= mixed >> 33u;
    return (size_t)(mixed & (KAIN_ALLOC_CACHE_BUCKETS - 1u));
}

static KainAllocHeader** kain_alloc_cache_next_cell(KainAllocHeader* header) {
    return (KainAllocHeader**)__kain_alloc_payload_from_header(header);
}

static KainAllocHeader* kain_alloc_cache_take(size_t payload_size) {
    if (!kain_alloc_cache_eligible(payload_size)) {
        return NULL;
    }

    KainAllocHeader* result = NULL;
    size_t bucket = kain_alloc_cache_bucket(payload_size);
    kain_alloc_cache_lock();
    KainAllocHeader** link = &KAIN_ALLOC_CACHE[bucket];
    while (*link != NULL) {
        KainAllocHeader* candidate = *link;
        KainAllocHeader** next = kain_alloc_cache_next_cell(candidate);
        if (candidate->metadata.payload_size == payload_size) {
            *link = *next;
            KAIN_ALLOC_CACHE_NODES -= 1u;
            KAIN_ALLOC_CACHE_BYTES -= sizeof(KainAllocHeader) + payload_size;
            result = candidate;
            break;
        }
        link = next;
    }
    kain_alloc_cache_unlock();
    return result;
}

static int kain_alloc_cache_release(KainAllocHeader* header, size_t payload_size) {
    size_t allocation_size = 0;
    if (header == NULL || !kain_alloc_cache_eligible(payload_size) ||
        kain_add_overflow_size(sizeof(KainAllocHeader), payload_size, &allocation_size)) {
        return 0;
    }

    size_t bucket = kain_alloc_cache_bucket(payload_size);
    kain_alloc_cache_lock();
    /* Proof: runtime/native/src/core/z3/proofs/native-memory-helper-allocation-cache-bounds.yaml */
    if (KAIN_ALLOC_CACHE_NODES >= KAIN_ALLOC_CACHE_MAX_NODES ||
        KAIN_ALLOC_CACHE_BYTES > KAIN_ALLOC_CACHE_MAX_BYTES - allocation_size) {
        kain_alloc_cache_unlock();
        return 0;
    }

    header->metadata.magic_and_slot = 0;
    header->metadata.payload_size = payload_size;
    *kain_alloc_cache_next_cell(header) = KAIN_ALLOC_CACHE[bucket];
    KAIN_ALLOC_CACHE[bucket] = header;
    KAIN_ALLOC_CACHE_NODES += 1u;
    KAIN_ALLOC_CACHE_BYTES += allocation_size;
    kain_alloc_cache_unlock();
    return 1;
}

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
    /* Proof: runtime/native/src/core/z3/proofs/native-memory-pointer-size-offset-does-not-wrap-before-pointer-rebuild.yaml */
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

void __kain_volatile_load(const void* ptr, void* out, size_t size) {
    const volatile unsigned char* src = (const volatile unsigned char*)ptr;
    unsigned char* dst = (unsigned char*)out;
    for (size_t index = 0; index < size; ++index) {
        dst[index] = src[index];
    }
}

void __kain_volatile_store(void* ptr, const void* value, size_t size) {
    volatile unsigned char* dst = (volatile unsigned char*)ptr;
    const unsigned char* src = (const unsigned char*)value;
    for (size_t index = 0; index < size; ++index) {
        dst[index] = src[index];
    }
}

static memory_order kain_memory_order_from_code(int64_t ordering) {
    switch (ordering) {
    case KAIN_MEMORY_ORDER_RELAXED:
        return memory_order_relaxed;
    case KAIN_MEMORY_ORDER_ACQUIRE:
        return memory_order_acquire;
    case KAIN_MEMORY_ORDER_RELEASE:
        return memory_order_release;
    case KAIN_MEMORY_ORDER_ACQ_REL:
        return memory_order_acq_rel;
    case KAIN_MEMORY_ORDER_SEQ_CST:
    default:
        return memory_order_seq_cst;
    }
}

static memory_order kain_memory_load_order_from_code(int64_t ordering) {
    switch (ordering) {
    case KAIN_MEMORY_ORDER_RELAXED:
        return memory_order_relaxed;
    case KAIN_MEMORY_ORDER_RELEASE:
    case KAIN_MEMORY_ORDER_ACQUIRE:
    case KAIN_MEMORY_ORDER_ACQ_REL:
        return memory_order_acquire;
    case KAIN_MEMORY_ORDER_SEQ_CST:
    default:
        return memory_order_seq_cst;
    }
}

static memory_order kain_memory_store_order_from_code(int64_t ordering) {
    switch (ordering) {
    case KAIN_MEMORY_ORDER_RELAXED:
        return memory_order_relaxed;
    case KAIN_MEMORY_ORDER_ACQUIRE:
    case KAIN_MEMORY_ORDER_RELEASE:
    case KAIN_MEMORY_ORDER_ACQ_REL:
        return memory_order_release;
    case KAIN_MEMORY_ORDER_SEQ_CST:
    default:
        return memory_order_seq_cst;
    }
}

static memory_order kain_memory_failure_order_from_code(int64_t ordering) {
    switch (ordering) {
    case KAIN_MEMORY_ORDER_RELAXED:
        return memory_order_relaxed;
    case KAIN_MEMORY_ORDER_RELEASE:
    case KAIN_MEMORY_ORDER_ACQUIRE:
    case KAIN_MEMORY_ORDER_ACQ_REL:
        return memory_order_acquire;
    case KAIN_MEMORY_ORDER_SEQ_CST:
    default:
        return memory_order_seq_cst;
    }
}

int64_t __kain_atomic_load_ordered(const void* ptr, int64_t ordering) {
    const atomic_int_least64_t* cell = (const atomic_int_least64_t*)ptr;
    return (int64_t)atomic_load_explicit(cell, kain_memory_load_order_from_code(ordering));
}

void __kain_atomic_store_ordered(void* ptr, int64_t value, int64_t ordering) {
    atomic_int_least64_t* cell = (atomic_int_least64_t*)ptr;
    atomic_store_explicit(cell, (int_least64_t)value, kain_memory_store_order_from_code(ordering));
}

int64_t __kain_atomic_add_ordered(void* ptr, int64_t delta, int64_t ordering) {
    atomic_int_least64_t* cell = (atomic_int_least64_t*)ptr;
    return (int64_t)atomic_fetch_add_explicit(
        cell,
        (int_least64_t)delta,
        kain_memory_order_from_code(ordering)
    );
}

int64_t __kain_atomic_sub_ordered(void* ptr, int64_t delta, int64_t ordering) {
    atomic_int_least64_t* cell = (atomic_int_least64_t*)ptr;
    return (int64_t)atomic_fetch_sub_explicit(
        cell,
        (int_least64_t)delta,
        kain_memory_order_from_code(ordering)
    );
}

int64_t __kain_atomic_and_ordered(void* ptr, int64_t mask, int64_t ordering) {
    atomic_int_least64_t* cell = (atomic_int_least64_t*)ptr;
    return (int64_t)atomic_fetch_and_explicit(
        cell,
        (int_least64_t)mask,
        kain_memory_order_from_code(ordering)
    );
}

int64_t __kain_atomic_or_ordered(void* ptr, int64_t bits, int64_t ordering) {
    atomic_int_least64_t* cell = (atomic_int_least64_t*)ptr;
    return (int64_t)atomic_fetch_or_explicit(
        cell,
        (int_least64_t)bits,
        kain_memory_order_from_code(ordering)
    );
}

int64_t __kain_atomic_xor_ordered(void* ptr, int64_t bits, int64_t ordering) {
    atomic_int_least64_t* cell = (atomic_int_least64_t*)ptr;
    return (int64_t)atomic_fetch_xor_explicit(
        cell,
        (int_least64_t)bits,
        kain_memory_order_from_code(ordering)
    );
}

int64_t __kain_atomic_exchange_ordered(void* ptr, int64_t value, int64_t ordering) {
    atomic_int_least64_t* cell = (atomic_int_least64_t*)ptr;
    return (int64_t)atomic_exchange_explicit(
        cell,
        (int_least64_t)value,
        kain_memory_order_from_code(ordering)
    );
}

int __kain_atomic_compare_exchange_ordered(
    void* ptr,
    int64_t expected,
    int64_t desired,
    int64_t success_ordering,
    int64_t failure_ordering
) {
    atomic_int_least64_t* cell = (atomic_int_least64_t*)ptr;
    int_least64_t expected_value = (int_least64_t)expected;
    return atomic_compare_exchange_strong_explicit(
               cell,
               &expected_value,
               (int_least64_t)desired,
               kain_memory_order_from_code(success_ordering),
               kain_memory_failure_order_from_code(failure_ordering)
           )
        ? 1
        : 0;
}

void __kain_atomic_fence(int64_t ordering) {
    atomic_thread_fence(kain_memory_order_from_code(ordering));
}

int64_t __kain_atomic_load_seqcst(const void* ptr) {
    return __kain_atomic_load_ordered(ptr, KAIN_MEMORY_ORDER_SEQ_CST);
}

void __kain_atomic_store_seqcst(void* ptr, int64_t value) {
    __kain_atomic_store_ordered(ptr, value, KAIN_MEMORY_ORDER_SEQ_CST);
}

int64_t __kain_atomic_add_seqcst(void* ptr, int64_t delta) {
    return __kain_atomic_add_ordered(ptr, delta, KAIN_MEMORY_ORDER_SEQ_CST);
}

int64_t __kain_atomic_sub_seqcst(void* ptr, int64_t delta) {
    return __kain_atomic_sub_ordered(ptr, delta, KAIN_MEMORY_ORDER_SEQ_CST);
}

int64_t __kain_atomic_and_seqcst(void* ptr, int64_t mask) {
    return __kain_atomic_and_ordered(ptr, mask, KAIN_MEMORY_ORDER_SEQ_CST);
}

int64_t __kain_atomic_or_seqcst(void* ptr, int64_t bits) {
    return __kain_atomic_or_ordered(ptr, bits, KAIN_MEMORY_ORDER_SEQ_CST);
}

int64_t __kain_atomic_xor_seqcst(void* ptr, int64_t bits) {
    return __kain_atomic_xor_ordered(ptr, bits, KAIN_MEMORY_ORDER_SEQ_CST);
}

int64_t __kain_atomic_exchange_seqcst(void* ptr, int64_t value) {
    return __kain_atomic_exchange_ordered(ptr, value, KAIN_MEMORY_ORDER_SEQ_CST);
}

int __kain_atomic_compare_exchange_seqcst(void* ptr, int64_t expected, int64_t desired) {
    return __kain_atomic_compare_exchange_ordered(
        ptr,
        expected,
        desired,
        KAIN_MEMORY_ORDER_SEQ_CST,
        KAIN_MEMORY_ORDER_SEQ_CST
    );
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

    /*
     * Proof: runtime/native/src/core/z3/proofs/native-memory-alloc-payload-size-does-not-wrap-before-header-accounting.yaml
     * Proof: runtime/native/src/core/z3/proofs/native-memory-alloc-header-plus-payload-does-not-wrap-before-allocation.yaml
     */
    if (kain_mul_overflow_size(size, stride, &payload_size)
        || kain_add_overflow_size(sizeof(KainAllocHeader), payload_size, &allocation_size)) {
        errno = ENOMEM;
        return NULL;
    }

    header = kain_alloc_cache_take(payload_size);
    if (header != NULL) {
        if (zeroed) {
            memset(__kain_alloc_payload_from_header(header), 0, payload_size);
        }
    } else if (zeroed) {
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
        header->metadata.payload_size = payload_size;
        if (!kain_alloc_cache_release(header, payload_size)) {
            header->metadata.payload_size = 0;
            free(header);
        }
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
    size_t bytes_to_copy = 0;
    KainAllocHeader* old_header = __kain_alloc_header_from_payload(ptr);
    KainAllocHeader* new_header = NULL;
    size_t old_payload_size = 0;
    uint16_t slot_token = 0u;
    void* payload = NULL;
    int relocate_status = KAIN_OWNERSHIP_OK;

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

    /*
     * Proof: runtime/native/src/core/z3/proofs/native-memory-realloc-payload-size-does-not-wrap-before-header-accounting.yaml
     * Proof: runtime/native/src/core/z3/proofs/native-memory-realloc-header-plus-payload-does-not-wrap-before-allocation.yaml
     */
    if (kain_mul_overflow_size(size, stride, &new_payload_size)
        || kain_add_overflow_size(sizeof(KainAllocHeader), new_payload_size, &allocation_size)) {
        errno = ENOMEM;
        return NULL;
    }

    if (new_payload_size == old_payload_size) {
        return ptr;
    }

    new_header = kain_alloc_cache_take(new_payload_size);
    if (new_header == NULL) {
        new_header = (KainAllocHeader*)malloc(allocation_size);
    }
    if (new_header == NULL) {
        return NULL;
    }

    new_header->metadata.payload_size = new_payload_size;
    payload = __kain_alloc_payload_from_header(new_header);
    bytes_to_copy = old_payload_size < new_payload_size ? old_payload_size : new_payload_size;
    if (bytes_to_copy != 0u) {
        memcpy(payload, ptr, bytes_to_copy);
    }

    if (zeroed_new && new_payload_size > old_payload_size) {
        memset(
            ((char*)payload) + old_payload_size,
            0,
            new_payload_size - old_payload_size
        );
    }

    __kain_alloc_header_set_magic_and_slot(new_header, slot_token);
    relocate_status =
        __kain_ownership_relocate_helper_allocation(ptr, payload, new_payload_size, slot_token);
    if (relocate_status != KAIN_OWNERSHIP_OK) {
        new_header->metadata.magic_and_slot = 0;
        if (!kain_alloc_cache_release(new_header, new_payload_size)) {
            new_header->metadata.payload_size = 0;
            free(new_header);
        }
        errno =
            relocate_status == KAIN_OWNERSHIP_ERR_NOT_FOUND
                || relocate_status == KAIN_OWNERSHIP_ERR_INVALID
            ? EINVAL
            : EBUSY;
        return NULL;
    }

    old_header->metadata.magic_and_slot = 0;
    if (!kain_alloc_cache_release(old_header, old_payload_size)) {
        old_header->metadata.payload_size = 0;
        free(old_header);
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

    size_t payload_size = header->metadata.payload_size;
    header->metadata.magic_and_slot = 0;
    if (kain_alloc_cache_release(header, payload_size)) {
        return 0;
    }
    header->metadata.payload_size = 0;
    free(header);
    return 0;
}
