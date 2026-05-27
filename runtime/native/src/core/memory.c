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
#include "../../include/diagnostics.h"
#include "../../include/ownership.h"
#include "../../include/virtual_alloc.h"
#include <errno.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KAIN_ALLOC_CACHE_HASH_BUCKETS 64u
#define KAIN_ALLOC_CACHE_SMALL_QUANTUM 16u
#define KAIN_ALLOC_CACHE_SMALL_MAX_PAYLOAD 8192u
#define KAIN_ALLOC_CACHE_SMALL_BIN_COUNT \
    (KAIN_ALLOC_CACHE_SMALL_MAX_PAYLOAD / KAIN_ALLOC_CACHE_SMALL_QUANTUM)
#define KAIN_ALLOC_CACHE_LARGE_MIN_PAYLOAD 2048u
#define KAIN_ALLOC_CACHE_MAX_PAYLOAD 262144u
#define KAIN_ALLOC_CACHE_MAX_BYTES (8u * 1024u * 1024u)
#define KAIN_ALLOC_CACHE_MAX_NODES 256u
#define KAIN_ALLOC_VIRTUAL_THRESHOLD_MAIN (1024u * 1024u)
#define KAIN_ALLOC_VIRTUAL_THRESHOLD_GPU (256u * 1024u)
#define KAIN_ALLOC_DEFERRED_DECAY_FLUSH_SIZE_THRESHOLD (KAIN_ALLOC_CACHE_MAX_PAYLOAD + 1u)
#define KAIN_DEFERRED_DECAY_FLUSH_WATERMARK 1024u

typedef struct {
    KainAllocHeader* small_bins[KAIN_MEMTYPE_COUNT][KAIN_ALLOC_CACHE_SMALL_BIN_COUNT];
    KainAllocHeader* large_buckets[KAIN_ALLOC_CACHE_HASH_BUCKETS];
    size_t bytes;
    size_t nodes;
    atomic_flag lock;
} KainAllocArenaCache;

typedef struct {
    uint8_t arena_id;
    uint8_t default_memtype;
    uint16_t reserved16;
    size_t virtual_threshold;
    KainAllocArenaCache cache;
} KainAllocatorArenaState;

#define KAIN_ALLOC_ARENA_CACHE_INIT { .lock = ATOMIC_FLAG_INIT }
#define KAIN_ALLOC_ARENA_STATE_INIT(arena_id_, memtype_, threshold_) \
    { \
        .arena_id = (uint8_t)(arena_id_), \
        .default_memtype = (uint8_t)(memtype_), \
        .reserved16 = 0u, \
        .virtual_threshold = (threshold_), \
        .cache = KAIN_ALLOC_ARENA_CACHE_INIT \
    }

static KainAllocatorArenaState KAIN_ALLOCATOR_ARENAS[KAIN_ARENA_MAX] = {
    KAIN_ALLOC_ARENA_STATE_INIT(
        KAIN_ARENA_MAIN,
        KAIN_MEMTYPE_DEFAULT,
        KAIN_ALLOC_VIRTUAL_THRESHOLD_MAIN),
    KAIN_ALLOC_ARENA_STATE_INIT(
        KAIN_ARENA_SHARED,
        KAIN_MEMTYPE_DEFAULT,
        KAIN_ALLOC_VIRTUAL_THRESHOLD_MAIN),
    KAIN_ALLOC_ARENA_STATE_INIT(
        KAIN_ARENA_GPU,
        KAIN_MEMTYPE_DEFAULT_GPU_RW,
        KAIN_ALLOC_VIRTUAL_THRESHOLD_GPU),
    KAIN_ALLOC_ARENA_STATE_INIT(
        KAIN_ARENA_SCRATCH,
        KAIN_MEMTYPE_DEFAULT,
        KAIN_ALLOC_VIRTUAL_THRESHOLD_MAIN),
};
static atomic_flag KAIN_ATOMIC_STORE_INVALID_ORDER_WARNED = ATOMIC_FLAG_INIT;
static atomic_flag KAIN_ATOMIC_COMPARE_EXCHANGE_FAILURE_SHAPE_WARNED = ATOMIC_FLAG_INIT;
static atomic_flag KAIN_ATOMIC_COMPARE_EXCHANGE_FAILURE_CLAMP_WARNED = ATOMIC_FLAG_INIT;

_Static_assert(sizeof(KainAllocHeader) == 16u, "KainAllocHeader proof constants require 16-byte header accounting.");
_Static_assert((KAIN_ALLOC_CACHE_HASH_BUCKETS & (KAIN_ALLOC_CACHE_HASH_BUCKETS - 1u)) == 0u, "alloc cache hash buckets require a power-of-two mask.");
_Static_assert((KAIN_ALLOC_CACHE_SMALL_QUANTUM & (KAIN_ALLOC_CACHE_SMALL_QUANTUM - 1u)) == 0u, "alloc cache small quantum requires a power-of-two alignment.");
_Static_assert((KAIN_ALLOC_CACHE_SMALL_MAX_PAYLOAD % KAIN_ALLOC_CACHE_SMALL_QUANTUM) == 0u, "alloc cache small payload ceiling must divide by the small quantum.");
_Static_assert((KAIN_ALLOC_HEADER_SLOT_TOKEN_MASK & KAIN_ALLOC_HEADER_ARENA_ID_MASK) == 0u, "alloc header slot/arena overlap");
_Static_assert((KAIN_ALLOC_HEADER_SLOT_TOKEN_MASK & KAIN_ALLOC_HEADER_MEMTYPE_MASK) == 0u, "alloc header slot/memtype overlap");
_Static_assert((KAIN_ALLOC_HEADER_SLOT_TOKEN_MASK & KAIN_ALLOC_HEADER_FLAGS_MASK) == 0u, "alloc header slot/flags overlap");
_Static_assert((KAIN_ALLOC_HEADER_ARENA_ID_MASK & KAIN_ALLOC_HEADER_MEMTYPE_MASK) == 0u, "alloc header arena/memtype overlap");
_Static_assert((KAIN_ALLOC_HEADER_ARENA_ID_MASK & KAIN_ALLOC_HEADER_FLAGS_MASK) == 0u, "alloc header arena/flags overlap");
_Static_assert((KAIN_ALLOC_HEADER_MEMTYPE_MASK & KAIN_ALLOC_HEADER_FLAGS_MASK) == 0u, "alloc header memtype/flags overlap");

static int kain_add_overflow_size(size_t left, size_t right, size_t* out);

static KainAllocatorArenaState* kain_allocator_state_for_arena(uint8_t arena_id) {
    if (arena_id >= KAIN_ARENA_MAX) {
        return &KAIN_ALLOCATOR_ARENAS[KAIN_ARENA_MAIN];
    }
    return &KAIN_ALLOCATOR_ARENAS[arena_id];
}

static int kain_alloc_should_flush_deferred_decay(size_t payload_size) {
    if (payload_size >= KAIN_ALLOC_DEFERRED_DECAY_FLUSH_SIZE_THRESHOLD) {
        return 1;
    }
    return __kain_ownership_deferred_decay_count() >= KAIN_DEFERRED_DECAY_FLUSH_WATERMARK;
}

static void kain_alloc_cache_lock(KainAllocArenaCache* cache) {
    while (atomic_flag_test_and_set_explicit(&cache->lock, memory_order_acquire)) {
    }
}

static void kain_alloc_cache_unlock(KainAllocArenaCache* cache) {
    atomic_flag_clear_explicit(&cache->lock, memory_order_release);
}

static int kain_alloc_cache_small_eligible(size_t payload_size, uint8_t flags) {
    return (flags & KAIN_ALLOC_HEADER_FLAG_VIRTUAL) == 0u &&
        payload_size >= sizeof(KainAllocHeader*) &&
        payload_size <= KAIN_ALLOC_CACHE_SMALL_MAX_PAYLOAD &&
        (payload_size & (KAIN_ALLOC_CACHE_SMALL_QUANTUM - 1u)) == 0u;
}

static int kain_alloc_cache_large_eligible(size_t payload_size, uint8_t flags) {
    return (flags & KAIN_ALLOC_HEADER_FLAG_VIRTUAL) == 0u &&
        payload_size >= KAIN_ALLOC_CACHE_LARGE_MIN_PAYLOAD &&
        payload_size <= KAIN_ALLOC_CACHE_MAX_PAYLOAD &&
        payload_size >= sizeof(KainAllocHeader*);
}

static size_t kain_alloc_cache_small_bin(size_t payload_size) {
    /* Proof: runtime/native/src/core/z3/proofs-experimental/memory-small-cache-bin-bounds.smt2 */
    return (payload_size >> 4u) - 1u;
}

static size_t kain_alloc_cache_large_bucket(size_t payload_size) {
    uint64_t mixed = (uint64_t)payload_size * UINT64_C(11400714819323198485);
    mixed ^= mixed >> 33u;
    return (size_t)(mixed & (KAIN_ALLOC_CACHE_HASH_BUCKETS - 1u));
}

static KainAllocHeader** kain_alloc_cache_next_cell(KainAllocHeader* header) {
    return (KainAllocHeader**)__kain_alloc_payload_from_header(header);
}

static KainAllocHeader* kain_alloc_cache_take(uint8_t arena_id, size_t payload_size, uint8_t memtype) {
    if (memtype >= KAIN_MEMTYPE_COUNT) {
        return NULL;
    }

    KainAllocatorArenaState* arena_state = kain_allocator_state_for_arena(arena_id);
    KainAllocArenaCache* cache = &arena_state->cache;
    KainAllocHeader* result = NULL;
    kain_alloc_cache_lock(cache);
    if (kain_alloc_cache_small_eligible(payload_size, 0u)) {
        size_t small_bin = kain_alloc_cache_small_bin(payload_size);
        KainAllocHeader** link = &cache->small_bins[memtype][small_bin];
        result = *link;
        if (result != NULL) {
            *link = *kain_alloc_cache_next_cell(result);
            cache->nodes -= 1u;
            cache->bytes -= sizeof(KainAllocHeader) + payload_size;
        }
        kain_alloc_cache_unlock(cache);
        return result;
    }

    if (kain_alloc_cache_large_eligible(payload_size, 0u)) {
        size_t bucket = kain_alloc_cache_large_bucket(payload_size);
        KainAllocHeader** link = &cache->large_buckets[bucket];
        while (*link != NULL) {
            KainAllocHeader* candidate = *link;
            KainAllocHeader** next = kain_alloc_cache_next_cell(candidate);
            if (candidate->metadata.payload_size == payload_size &&
                __kain_alloc_header_memtype(candidate) == memtype) {
                *link = *next;
                cache->nodes -= 1u;
                cache->bytes -= sizeof(KainAllocHeader) + payload_size;
                result = candidate;
                break;
            }
            link = next;
        }
    }
    kain_alloc_cache_unlock(cache);
    return result;
}

static int kain_alloc_cache_release(
    KainAllocHeader* header,
    size_t payload_size,
    uint8_t arena_id,
    uint8_t memtype
) {
    size_t allocation_size = 0;
    if (header == NULL || memtype >= KAIN_MEMTYPE_COUNT ||
        (!kain_alloc_cache_small_eligible(payload_size, __kain_alloc_header_flags(header)) &&
            !kain_alloc_cache_large_eligible(payload_size, __kain_alloc_header_flags(header))) ||
        kain_add_overflow_size(sizeof(KainAllocHeader), payload_size, &allocation_size)) {
        return 0;
    }

    KainAllocatorArenaState* arena_state = kain_allocator_state_for_arena(arena_id);
    KainAllocArenaCache* cache = &arena_state->cache;
    kain_alloc_cache_lock(cache);
    /* Proof: runtime/native/src/core/z3/proofs/native-memory-helper-allocation-cache-bounds.yaml */
    if (cache->nodes >= KAIN_ALLOC_CACHE_MAX_NODES ||
        cache->bytes > KAIN_ALLOC_CACHE_MAX_BYTES - allocation_size) {
        kain_alloc_cache_unlock(cache);
        return 0;
    }

    __kain_alloc_header_set_fields(
        header,
        0u,
        arena_state->arena_id,
        memtype,
        KAIN_ALLOC_HEADER_FLAG_CACHED);
    header->metadata.payload_size = payload_size;
    if (kain_alloc_cache_small_eligible(payload_size, 0u)) {
        size_t small_bin = kain_alloc_cache_small_bin(payload_size);
        *kain_alloc_cache_next_cell(header) = cache->small_bins[memtype][small_bin];
        cache->small_bins[memtype][small_bin] = header;
    } else {
        size_t bucket = kain_alloc_cache_large_bucket(payload_size);
        *kain_alloc_cache_next_cell(header) = cache->large_buckets[bucket];
        cache->large_buckets[bucket] = header;
    }
    cache->nodes += 1u;
    cache->bytes += allocation_size;
    kain_alloc_cache_unlock(cache);
    return 1;
}

static int kain_virtual_allocation_size(size_t payload_size, size_t* out_virtual_bytes) {
    size_t allocation_size = 0u;
    if (kain_add_overflow_size(sizeof(KainAllocHeader), payload_size, &allocation_size)) {
        return 1;
    }

    size_t virtual_bytes = kain_virtual_align_up(allocation_size, kain_virtual_page_size());
    if (virtual_bytes == 0u) {
        return 1;
    }

    *out_virtual_bytes = virtual_bytes;
    return 0;
}

static KainAllocHeader* kain_alloc_raw(
    size_t payload_size,
    int zeroed,
    uint8_t arena_id,
    uint8_t memtype,
    uint8_t* out_header_flags
) {
    KainAllocatorArenaState* arena_state = kain_allocator_state_for_arena(arena_id);
    uint8_t header_flags = zeroed ? KAIN_ALLOC_HEADER_FLAG_ZEROED : 0u;
    KainAllocHeader* header = kain_alloc_cache_take(arena_state->arena_id, payload_size, memtype);
    if (header != NULL) {
        if (zeroed && payload_size != 0u) {
            memset(__kain_alloc_payload_from_header(header), 0, payload_size);
        }
        header->metadata.payload_size = payload_size;
        __kain_alloc_header_set_fields(
            header,
            0u,
            arena_state->arena_id,
            memtype,
            header_flags);
        if (out_header_flags != NULL) {
            *out_header_flags = header_flags;
        }
        return header;
    }

    if (arena_state->virtual_threshold != 0u && payload_size >= arena_state->virtual_threshold) {
        size_t virtual_bytes = 0u;
        if (!kain_virtual_allocation_size(payload_size, &virtual_bytes)) {
            header = (KainAllocHeader*)kain_virtual_reserve_and_commit(
                virtual_bytes,
                kain_virtual_page_size(),
                (KainMemType)memtype);
            if (header != NULL) {
                header_flags |= KAIN_ALLOC_HEADER_FLAG_VIRTUAL;
                header->metadata.payload_size = payload_size;
                __kain_alloc_header_set_fields(
                    header,
                    0u,
                    arena_state->arena_id,
                    memtype,
                    header_flags);
                if (out_header_flags != NULL) {
                    *out_header_flags = header_flags;
                }
                return header;
            }
        }
    }

    size_t allocation_size = 0u;
    if (kain_add_overflow_size(sizeof(KainAllocHeader), payload_size, &allocation_size)) {
        return NULL;
    }

    header = zeroed
        ? (KainAllocHeader*)calloc(1, allocation_size)
        : (KainAllocHeader*)malloc(allocation_size);
    if (header == NULL) {
        return NULL;
    }

    header->metadata.payload_size = payload_size;
    __kain_alloc_header_set_fields(
        header,
        0u,
        arena_state->arena_id,
        memtype,
        header_flags);
    if (out_header_flags != NULL) {
        *out_header_flags = header_flags;
    }
    return header;
}

static void kain_release_raw(KainAllocHeader* header) {
    if (header == NULL) {
        return;
    }

    size_t payload_size = header->metadata.payload_size;
    uint8_t arena_id = __kain_alloc_header_arena_id(header);
    uint8_t memtype = __kain_alloc_header_memtype(header);
    uint8_t flags = __kain_alloc_header_flags(header);
    if ((flags & KAIN_ALLOC_HEADER_FLAG_VIRTUAL) == 0u &&
        kain_alloc_cache_release(header, payload_size, arena_id, memtype)) {
        return;
    }

    if ((flags & KAIN_ALLOC_HEADER_FLAG_VIRTUAL) != 0u) {
        size_t virtual_bytes = 0u;
        if (!kain_virtual_allocation_size(payload_size, &virtual_bytes)) {
            header->metadata.payload_size = 0u;
            kain_virtual_release(header, virtual_bytes);
            return;
        }
    }

    header->metadata.payload_size = 0u;
    free(header);
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

static const char* kain_memory_order_name_from_code(int64_t ordering) {
    switch (ordering) {
    case KAIN_MEMORY_ORDER_RELAXED:
        return "relaxed";
    case KAIN_MEMORY_ORDER_ACQUIRE:
        return "acquire";
    case KAIN_MEMORY_ORDER_RELEASE:
        return "release";
    case KAIN_MEMORY_ORDER_ACQ_REL:
        return "acq_rel";
    case KAIN_MEMORY_ORDER_SEQ_CST:
    default:
        return "seq_cst";
    }
}

static void kain_memory_emit_ordering_warning_once(
    atomic_flag* gate,
    const char* message,
    const char* detail
) {
    KainDiagnostic diag;
    if (atomic_flag_test_and_set_explicit(gate, memory_order_relaxed)) {
        return;
    }
    kain_diagnostic_create(
        &diag,
        KAIN_DIAG_SUBSYSTEM_MEMORY,
        KAIN_DIAG_SEVERITY_WARNING,
        KAIN_DIAG_CODE_GENERIC_ERROR,
        message,
        detail,
        "runtime/native/src/core/memory.c"
    );
    kain_diagnostic_print(&diag);
}

static int kain_memory_c11_success_strength(int64_t ordering) {
    switch (ordering) {
    case KAIN_MEMORY_ORDER_RELAXED:
        return 0;
    case KAIN_MEMORY_ORDER_ACQUIRE:
        return 2;
    case KAIN_MEMORY_ORDER_RELEASE:
        return 3;
    case KAIN_MEMORY_ORDER_ACQ_REL:
        return 4;
    case KAIN_MEMORY_ORDER_SEQ_CST:
    default:
        return 5;
    }
}

static int kain_memory_c11_failure_strength(int64_t ordering) {
    switch (ordering) {
    case KAIN_MEMORY_ORDER_RELAXED:
        return 0;
    case KAIN_MEMORY_ORDER_SEQ_CST:
        return 5;
    case KAIN_MEMORY_ORDER_ACQUIRE:
    case KAIN_MEMORY_ORDER_RELEASE:
    case KAIN_MEMORY_ORDER_ACQ_REL:
    default:
        return 2;
    }
}

static int64_t kain_memory_normalize_failure_order_code(int64_t ordering, int* warned_invalid_shape) {
    switch (ordering) {
    case KAIN_MEMORY_ORDER_RELAXED:
    case KAIN_MEMORY_ORDER_ACQUIRE:
    case KAIN_MEMORY_ORDER_SEQ_CST:
        return ordering;
    case KAIN_MEMORY_ORDER_RELEASE:
    case KAIN_MEMORY_ORDER_ACQ_REL:
        if (warned_invalid_shape != NULL) {
            *warned_invalid_shape = 1;
        }
        return KAIN_MEMORY_ORDER_ACQUIRE;
    default:
        return KAIN_MEMORY_ORDER_SEQ_CST;
    }
}

static int64_t kain_memory_clamp_failure_order_code(
    int64_t success_ordering,
    int64_t failure_ordering,
    int* warned_invalid_shape,
    int* warned_clamp
) {
    int64_t normalized_failure =
        kain_memory_normalize_failure_order_code(failure_ordering, warned_invalid_shape);
    if (kain_memory_c11_failure_strength(normalized_failure) <=
        kain_memory_c11_success_strength(success_ordering)) {
        return normalized_failure;
    }
    if (warned_clamp != NULL) {
        *warned_clamp = 1;
    }
    return kain_memory_c11_success_strength(success_ordering) >=
            kain_memory_c11_failure_strength(KAIN_MEMORY_ORDER_ACQUIRE)
        ? KAIN_MEMORY_ORDER_ACQUIRE
        : KAIN_MEMORY_ORDER_RELAXED;
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
    if (ordering == KAIN_MEMORY_ORDER_ACQUIRE || ordering == KAIN_MEMORY_ORDER_ACQ_REL) {
        char detail[KAIN_DIAG_DETAIL_MAX];
        snprintf(
            detail,
            sizeof(detail),
            "atomic_store requested %s; plain stores only accept relaxed, release, or seq_cst. The runtime ABI helper will canonicalize this call to release semantics.",
            kain_memory_order_name_from_code(ordering)
        );
        kain_memory_emit_ordering_warning_once(
            &KAIN_ATOMIC_STORE_INVALID_ORDER_WARNED,
            "atomic_store received an invalid ordering for a plain store",
            detail
        );
    }
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
    int warned_invalid_shape = 0;
    int warned_clamp = 0;
    /* Proof: runtime/native/src/core/z3/proofs/native-memory-cas-failure-order-clamp-prevents-ub.yaml */
    int64_t normalized_failure_ordering = kain_memory_clamp_failure_order_code(
        success_ordering,
        failure_ordering,
        &warned_invalid_shape,
        &warned_clamp
    );
    if (warned_invalid_shape) {
        char detail[KAIN_DIAG_DETAIL_MAX];
        snprintf(
            detail,
            sizeof(detail),
            "atomic_compare_exchange requested failure ordering %s. Failure orderings cannot be release or acq_rel, so the runtime ABI helper canonicalized it to acquire before executing the C11 primitive.",
            kain_memory_order_name_from_code(failure_ordering)
        );
        kain_memory_emit_ordering_warning_once(
            &KAIN_ATOMIC_COMPARE_EXCHANGE_FAILURE_SHAPE_WARNED,
            "atomic_compare_exchange failure ordering shape was invalid",
            detail
        );
    }
    if (warned_clamp) {
        char detail[KAIN_DIAG_DETAIL_MAX];
        snprintf(
            detail,
            sizeof(detail),
            "atomic_compare_exchange requested success=%s and failure=%s. The runtime ABI helper clamped the failure ordering to %s so the C11 primitive never sees failure stronger than success.",
            kain_memory_order_name_from_code(success_ordering),
            kain_memory_order_name_from_code(failure_ordering),
            kain_memory_order_name_from_code(normalized_failure_ordering)
        );
        kain_memory_emit_ordering_warning_once(
            &KAIN_ATOMIC_COMPARE_EXCHANGE_FAILURE_CLAMP_WARNED,
            "atomic_compare_exchange failure ordering was stronger than success",
            detail
        );
    }
    return atomic_compare_exchange_strong_explicit(
               cell,
               &expected_value,
               (int_least64_t)desired,
               kain_memory_order_from_code(success_ordering),
               kain_memory_failure_order_from_code(normalized_failure_ordering)
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
    uint8_t arena_id = KAIN_ARENA_MAIN;
    uint8_t memtype = KAIN_MEMTYPE_DEFAULT;
    uint8_t header_flags = 0u;

    /*
     * Proof: runtime/native/src/core/z3/proofs/native-memory-alloc-payload-size-does-not-wrap-before-header-accounting.yaml
     * Proof: runtime/native/src/core/z3/proofs/native-memory-alloc-header-plus-payload-does-not-wrap-before-allocation.yaml
     */
    if (kain_mul_overflow_size(size, stride, &payload_size)
        || kain_add_overflow_size(sizeof(KainAllocHeader), payload_size, &allocation_size)) {
        errno = ENOMEM;
        return NULL;
    }

    if (kain_alloc_should_flush_deferred_decay(payload_size)) {
        __kain_ownership_flush_deferred_decay();
    }
    header = kain_alloc_raw(payload_size, zeroed, arena_id, memtype, &header_flags);
    if (header == NULL) {
        return NULL;
    }

    void* payload = __kain_alloc_payload_from_header(header);
    if (__kain_ownership_register_helper_allocation(
            payload,
            payload_size,
            &slot_token
        ) != KAIN_OWNERSHIP_OK) {
        kain_release_raw(header);
        return NULL;
    }
    __kain_alloc_header_set_fields(header, slot_token, arena_id, memtype, header_flags);
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
    uint8_t arena_id = KAIN_ARENA_MAIN;
    uint8_t memtype = KAIN_MEMTYPE_DEFAULT;
    uint8_t header_flags = 0u;

    if (!__kain_alloc_header_is_valid(old_header)) {
        errno = EINVAL;
        return NULL;
    }

    old_payload_size = old_header->metadata.payload_size;
    slot_token = __kain_alloc_header_slot_token(old_header);
    arena_id = __kain_alloc_header_arena_id(old_header);
    memtype = __kain_alloc_header_memtype(old_header);
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

    if (kain_alloc_should_flush_deferred_decay(new_payload_size)) {
        __kain_ownership_flush_deferred_decay();
    }
    new_header = kain_alloc_raw(new_payload_size, 0, arena_id, memtype, &header_flags);
    if (new_header == NULL) {
        return NULL;
    }

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

    if (zeroed_new) {
        header_flags |= KAIN_ALLOC_HEADER_FLAG_ZEROED;
    }
    __kain_alloc_header_set_fields(new_header, slot_token, arena_id, memtype, header_flags);
    relocate_status =
        __kain_ownership_relocate_helper_allocation(ptr, payload, new_payload_size, slot_token);
    if (relocate_status != KAIN_OWNERSHIP_OK) {
        kain_release_raw(new_header);
        errno =
            relocate_status == KAIN_OWNERSHIP_ERR_NOT_FOUND
                || relocate_status == KAIN_OWNERSHIP_ERR_INVALID
            ? EINVAL
            : EBUSY;
        return NULL;
    }

    kain_release_raw(old_header);
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

    kain_release_raw(header);
    return 0;
}
