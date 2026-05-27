/*
 * KAIN Native Runtime Ownership Helpers
 *
 * Runtime guard surface for the compiler-owned collapse/observe/decay memory
 * model. The compiler owns the syntax and lowering; this file owns checked
 * native state transitions for helper-owned heap memory and explicitly
 * registered imported pointers.
 */

#include "../../include/ownership.h"
#include "../../include/fixup.h"
#include "../../include/memory.h"
#include "../../include/profile.h"
#include "../../include/runtime_tiers.h"
#include <errno.h>
#include <stdint.h>
#include <stdatomic.h>
#include <string.h>

#define KAIN_OWNERSHIP_MAX_REGIONS 4096u
#define KAIN_OWNERSHIP_WORD_BITS 64u
#define KAIN_OWNERSHIP_WORD_COUNT (KAIN_OWNERSHIP_MAX_REGIONS / KAIN_OWNERSHIP_WORD_BITS)
#define KAIN_OWNERSHIP_INDEX_CAPACITY 8192u
#define KAIN_OWNERSHIP_INDEX_MASK (KAIN_OWNERSHIP_INDEX_CAPACITY - 1u)
#define KAIN_OWNERSHIP_INDEX_TOMBSTONE UINT32_MAX
#if (KAIN_OWNERSHIP_MAX_REGIONS % KAIN_OWNERSHIP_WORD_BITS) != 0
#error "KAIN_OWNERSHIP_MAX_REGIONS must be divisible by 64 for occupancy-word indexing."
#endif
#if KAIN_OWNERSHIP_MAX_REGIONS > UINT16_MAX
#error "KAIN_OWNERSHIP_MAX_REGIONS must fit in the helper allocation slot token."
#endif
#if (KAIN_OWNERSHIP_INDEX_CAPACITY & KAIN_OWNERSHIP_INDEX_MASK) != 0
#error "KAIN_OWNERSHIP_INDEX_CAPACITY must be a power of two for masked probing."
#endif

typedef struct KainOwnershipRegion {
    void* ptr;
    size_t size;
    int64_t kind;
    int state;
    uint32_t observers;
    KainRuntimeHandle relocation_handle;
    uint8_t decay_queued;
    int occupied;
} KainOwnershipRegion;

static KainOwnershipRegion KAIN_OWNERSHIP_REGIONS[KAIN_OWNERSHIP_MAX_REGIONS];
static uint64_t KAIN_OWNERSHIP_OCCUPANCY_WORDS[KAIN_OWNERSHIP_WORD_COUNT];
static uint32_t KAIN_OWNERSHIP_POINTER_INDEX[KAIN_OWNERSHIP_INDEX_CAPACITY];
static uint16_t KAIN_OWNERSHIP_DEFERRED_DECAY_RING[KAIN_OWNERSHIP_MAX_REGIONS];
static uint32_t KAIN_OWNERSHIP_DEFERRED_DECAY_HEAD = 0u;
static uint32_t KAIN_OWNERSHIP_DEFERRED_DECAY_TAIL = 0u;
static uint32_t KAIN_OWNERSHIP_DEFERRED_DECAY_COUNT = 0u;
static atomic_flag KAIN_OWNERSHIP_REGISTRY_LOCK = ATOMIC_FLAG_INIT;

static void kain_ownership_lock(void) {
    while (atomic_flag_test_and_set_explicit(
        &KAIN_OWNERSHIP_REGISTRY_LOCK,
        memory_order_acquire
    )) {
    }
}

static void kain_ownership_unlock(void) {
    atomic_flag_clear_explicit(&KAIN_OWNERSHIP_REGISTRY_LOCK, memory_order_release);
}

static int kain_ownership_errno_from_status(int status) {
    switch (status) {
        case KAIN_OWNERSHIP_OK:
            return 0;
        case KAIN_OWNERSHIP_ERR_CAPACITY:
        case KAIN_OWNERSHIP_ERR_OVERFLOW:
            return ENOMEM;
        case KAIN_OWNERSHIP_ERR_OBSERVED:
        case KAIN_OWNERSHIP_ERR_COLLAPSED:
            return EBUSY;
        case KAIN_OWNERSHIP_ERR_NOT_FOUND:
        case KAIN_OWNERSHIP_ERR_DECAYED:
        case KAIN_OWNERSHIP_ERR_NOT_OBSERVED:
        case KAIN_OWNERSHIP_ERR_NOT_COLLAPSED:
        case KAIN_OWNERSHIP_ERR_INVALID:
        default:
            return EINVAL;
    }
}

static int kain_ownership_fail(int status) {
    errno = kain_ownership_errno_from_status(status);
    return status;
}

static uint64_t kain_ownership_mix_pointer(const void* ptr) {
    uint64_t x = (uint64_t)(uintptr_t)ptr;
    x ^= x >> 30u;
    x *= UINT64_C(0xbf58476d1ce4e5b9);
    x ^= x >> 27u;
    x *= UINT64_C(0x94d049bb133111eb);
    x ^= x >> 31u;
    return x;
}

static uint32_t kain_ownership_pointer_index_slot(const void* ptr) {
    return (uint32_t)(kain_ownership_mix_pointer(ptr) & KAIN_OWNERSHIP_INDEX_MASK);
}

static uint64_t kain_ownership_isolate_low_bit_u64(uint64_t value) {
    return value & (0u - value);
}

static unsigned int kain_ownership_low_bit_index_u64(uint64_t one_hot) {
    static const unsigned char debruijn_index[64] = {
        0, 1, 48, 2, 57, 49, 28, 3,
        61, 58, 50, 42, 38, 29, 17, 4,
        62, 55, 59, 36, 53, 51, 43, 22,
        45, 39, 33, 30, 24, 18, 12, 5,
        63, 47, 56, 27, 60, 41, 37, 16,
        54, 35, 52, 21, 44, 32, 23, 11,
        46, 26, 40, 15, 34, 20, 31, 10,
        25, 14, 19, 9, 13, 8, 7, 6
    };
    return debruijn_index[(one_hot * UINT64_C(0x03f79d71b4cb0a89)) >> 58u];
}

static int kain_ownership_index_insert_unlocked(const void* ptr, int slot) {
    uint32_t index;
    uint32_t encoded_slot;
    uint32_t first_tombstone = KAIN_OWNERSHIP_INDEX_CAPACITY;

    if (ptr == NULL || slot < 0 || (uint32_t)slot >= KAIN_OWNERSHIP_MAX_REGIONS) {
        return KAIN_OWNERSHIP_ERR_INVALID;
    }

    index = kain_ownership_pointer_index_slot(ptr);
    encoded_slot = (uint32_t)slot + 1u;
    for (uint32_t probe = 0u; probe < KAIN_OWNERSHIP_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (index + probe) & KAIN_OWNERSHIP_INDEX_MASK;
        uint32_t candidate = KAIN_OWNERSHIP_POINTER_INDEX[candidate_index];
        if (candidate == KAIN_OWNERSHIP_INDEX_TOMBSTONE) {
            if (first_tombstone == KAIN_OWNERSHIP_INDEX_CAPACITY) {
                first_tombstone = candidate_index;
            }
            continue;
        }
        if (candidate == encoded_slot) {
            return KAIN_OWNERSHIP_OK;
        }
        if (candidate == 0u) {
            KAIN_OWNERSHIP_POINTER_INDEX[
                first_tombstone == KAIN_OWNERSHIP_INDEX_CAPACITY
                    ? candidate_index
                    : first_tombstone
            ] = encoded_slot;
            return KAIN_OWNERSHIP_OK;
        }
    }

    if (first_tombstone != KAIN_OWNERSHIP_INDEX_CAPACITY) {
        KAIN_OWNERSHIP_POINTER_INDEX[first_tombstone] = encoded_slot;
        return KAIN_OWNERSHIP_OK;
    }

    return KAIN_OWNERSHIP_ERR_CAPACITY;
}

static int kain_ownership_index_remove_unlocked(const void* ptr, int slot) {
    uint32_t index;
    uint32_t encoded_slot;

    if (ptr == NULL || slot < 0 || (uint32_t)slot >= KAIN_OWNERSHIP_MAX_REGIONS) {
        return KAIN_OWNERSHIP_ERR_INVALID;
    }

    index = kain_ownership_pointer_index_slot(ptr);
    encoded_slot = (uint32_t)slot + 1u;
    for (uint32_t probe = 0u; probe < KAIN_OWNERSHIP_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (index + probe) & KAIN_OWNERSHIP_INDEX_MASK;
        uint32_t candidate = KAIN_OWNERSHIP_POINTER_INDEX[candidate_index];
        if (candidate == 0u) {
            return KAIN_OWNERSHIP_ERR_NOT_FOUND;
        }
        if (candidate == KAIN_OWNERSHIP_INDEX_TOMBSTONE) {
            continue;
        }
        if (candidate == encoded_slot) {
            KAIN_OWNERSHIP_POINTER_INDEX[candidate_index] = KAIN_OWNERSHIP_INDEX_TOMBSTONE;
            return KAIN_OWNERSHIP_OK;
        }
    }

    return KAIN_OWNERSHIP_ERR_NOT_FOUND;
}

static void kain_ownership_rebuild_pointer_index_unlocked(void) {
    memset(KAIN_OWNERSHIP_POINTER_INDEX, 0, sizeof(KAIN_OWNERSHIP_POINTER_INDEX));
    for (uint32_t slot = 0u; slot < KAIN_OWNERSHIP_MAX_REGIONS; ++slot) {
        if (KAIN_OWNERSHIP_REGIONS[slot].occupied) {
            (void)kain_ownership_index_insert_unlocked(
                KAIN_OWNERSHIP_REGIONS[slot].ptr,
                (int)slot
            );
        }
    }
}

static int kain_ownership_enqueue_deferred_decay_unlocked(int slot) {
    KainOwnershipRegion* region;
    if (slot < 0 || (uint32_t)slot >= KAIN_OWNERSHIP_MAX_REGIONS) {
        return KAIN_OWNERSHIP_ERR_INVALID;
    }
    region = &KAIN_OWNERSHIP_REGIONS[slot];
    if (region->decay_queued) {
        return KAIN_OWNERSHIP_OK;
    }
    if (KAIN_OWNERSHIP_DEFERRED_DECAY_COUNT >= KAIN_OWNERSHIP_MAX_REGIONS) {
        return KAIN_OWNERSHIP_ERR_CAPACITY;
    }
    KAIN_OWNERSHIP_DEFERRED_DECAY_RING[KAIN_OWNERSHIP_DEFERRED_DECAY_TAIL] = (uint16_t)slot;
    KAIN_OWNERSHIP_DEFERRED_DECAY_TAIL =
        (KAIN_OWNERSHIP_DEFERRED_DECAY_TAIL + 1u) % KAIN_OWNERSHIP_MAX_REGIONS;
    KAIN_OWNERSHIP_DEFERRED_DECAY_COUNT += 1u;
    region->decay_queued = 1u;
    return KAIN_OWNERSHIP_OK;
}

static int kain_ownership_find_slot(const void* ptr) {
    uint32_t index;

    if (ptr == NULL) {
        return -1;
    }

    index = kain_ownership_pointer_index_slot(ptr);
    for (uint32_t probe = 0u; probe < KAIN_OWNERSHIP_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (index + probe) & KAIN_OWNERSHIP_INDEX_MASK;
        uint32_t encoded_slot = KAIN_OWNERSHIP_POINTER_INDEX[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return -1;
        }
        if (encoded_slot == KAIN_OWNERSHIP_INDEX_TOMBSTONE) {
            continue;
        }
        slot = encoded_slot - 1u;
        if (slot < KAIN_OWNERSHIP_MAX_REGIONS &&
            KAIN_OWNERSHIP_REGIONS[slot].occupied &&
            KAIN_OWNERSHIP_REGIONS[slot].ptr == ptr) {
            return (int)slot;
        }
    }

    return -1;
}

static void kain_ownership_clear_slot_unlocked(int slot) {
    if (slot < 0 || (uint32_t)slot >= KAIN_OWNERSHIP_MAX_REGIONS) {
        return;
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    if (!region->occupied) {
        return;
    }

    (void)kain_ownership_index_remove_unlocked(region->ptr, slot);
    region->ptr = NULL;
    region->size = 0;
    region->kind = 0;
    region->observers = 0;
    region->relocation_handle = KAIN_RUNTIME_HANDLE_INVALID;
    region->decay_queued = 0u;
    /* Keep OCCUPIED=0 ahead of DECAYED so future lock-free readers cannot see
     * an alive-looking slot that already published terminal state.
     * Proof: runtime/native/src/core/z3/proofs/native-ownership-clear-slot-clears-occupied-before-decayed.yaml
     */
    region->occupied = 0;
    region->state = KAIN_OWNERSHIP_STATE_DECAYED;
    KAIN_OWNERSHIP_OCCUPANCY_WORDS[(uint32_t)slot / KAIN_OWNERSHIP_WORD_BITS] &=
        ~(UINT64_C(1) << ((uint32_t)slot % KAIN_OWNERSHIP_WORD_BITS));
}

static int kain_ownership_find_free_slot(void) {
    for (uint32_t word_index = 0u; word_index < KAIN_OWNERSHIP_WORD_COUNT; ++word_index) {
        uint64_t free_mask = ~KAIN_OWNERSHIP_OCCUPANCY_WORDS[word_index];
        if (free_mask != 0u) {
            uint64_t low_bit = kain_ownership_isolate_low_bit_u64(free_mask);
            unsigned int bit_index = kain_ownership_low_bit_index_u64(low_bit);
            uint32_t slot = word_index * KAIN_OWNERSHIP_WORD_BITS + bit_index;
            if (slot < KAIN_OWNERSHIP_MAX_REGIONS) {
                return (int)slot;
            }
        }
    }
    return -1;
}

static int kain_ownership_region_is_heap(const KainOwnershipRegion* region) {
    return region->kind == KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION;
}

static int kain_ownership_status_for_busy_region(const KainOwnershipRegion* region) {
    if (region->state == KAIN_OWNERSHIP_STATE_DECAYED) {
        return KAIN_OWNERSHIP_ERR_DECAYED;
    }
    if (region->state == KAIN_OWNERSHIP_STATE_SHARED) {
        return KAIN_OWNERSHIP_ERR_COLLAPSED;
    }
    if (region->state == KAIN_OWNERSHIP_STATE_COLLAPSED) {
        return KAIN_OWNERSHIP_ERR_COLLAPSED;
    }
    if (region->state == KAIN_OWNERSHIP_STATE_OBSERVED || region->observers != 0) {
        return KAIN_OWNERSHIP_ERR_OBSERVED;
    }
    return KAIN_OWNERSHIP_ERR_INVALID;
}

static int kain_ownership_helper_slot_from_token_unlocked(const void* ptr, uint16_t slot_token) {
    if (ptr == NULL || slot_token == 0u) {
        return -1;
    }

    uint32_t slot = (uint32_t)slot_token - 1u;
    /* Proof: runtime/native/src/core/z3/proofs/native-ownership-helper-slot-token-stays-within-registry-capacity.yaml */
    if (slot >= KAIN_OWNERSHIP_MAX_REGIONS) {
        return -1;
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    /* Proof: runtime/native/src/core/z3/proofs/native-ownership-helper-fast-path-requires-heap-slot-match.yaml */
    if (!region->occupied || region->ptr != ptr || !kain_ownership_region_is_heap(region)) {
        return -1;
    }

    return (int)slot;
}

static int kain_ownership_find_helper_slot_unlocked(const void* ptr) {
    if (ptr == NULL) {
        return -1;
    }

    KainAllocHeader* header = __kain_alloc_header_from_payload(ptr);
    return kain_ownership_helper_slot_from_token_unlocked(
        ptr,
        __kain_alloc_header_slot_token(header)
    );
}

static int kain_ownership_upsert_unlocked(
    void* ptr,
    int64_t region_kind,
    size_t size,
    int state,
    uint32_t observers,
    int* out_slot
) {
    int is_new_slot = 0;
    if (ptr == NULL) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_INVALID);
    }

    int slot = kain_ownership_find_slot(ptr);
    if (slot < 0) {
        slot = kain_ownership_find_free_slot();
        if (slot < 0) {
            return kain_ownership_fail(KAIN_OWNERSHIP_ERR_CAPACITY);
        }
        is_new_slot = 1;
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    region->ptr = ptr;
    region->size = size;
    region->kind = region_kind;
    region->state = state;
    region->observers = observers;
    if (is_new_slot) {
        region->relocation_handle = KAIN_RUNTIME_HANDLE_INVALID;
    }
    if (region_kind != KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION) {
        region->relocation_handle = KAIN_RUNTIME_HANDLE_INVALID;
    }
    region->decay_queued = 0u;
    region->occupied = 1;
    if (is_new_slot) {
        uint32_t word_index = (uint32_t)slot / KAIN_OWNERSHIP_WORD_BITS;
        uint64_t bit = UINT64_C(1) << ((uint32_t)slot % KAIN_OWNERSHIP_WORD_BITS);
        int index_status;
        KAIN_OWNERSHIP_OCCUPANCY_WORDS[word_index] |= bit;
        index_status = kain_ownership_index_insert_unlocked(ptr, slot);
        if (index_status != KAIN_OWNERSHIP_OK) {
            region->occupied = 0;
            KAIN_OWNERSHIP_OCCUPANCY_WORDS[word_index] &= ~bit;
            return kain_ownership_fail(index_status);
        }
    }
    if (out_slot != NULL) {
        *out_slot = slot;
    }
    return KAIN_OWNERSHIP_OK;
}

int __kain_ownership_register(void* ptr, int64_t region_kind, size_t size) {
    kain_ownership_lock();
    int status = kain_ownership_upsert_unlocked(
        ptr,
        region_kind,
        size,
        KAIN_OWNERSHIP_STATE_IDLE,
        0u,
        NULL
    );
    kain_ownership_unlock();
    return status;
}

int __kain_ownership_register_imported(void* ptr, size_t size) {
    kain_ownership_lock();
    int status = kain_ownership_upsert_unlocked(
        ptr,
        KAIN_OWNERSHIP_REGION_IMPORTED_POINTER,
        size,
        KAIN_OWNERSHIP_STATE_IDLE,
        0u,
        NULL
    );
    kain_ownership_unlock();
    return status;
}

int __kain_ownership_register_helper_allocation(void* ptr, size_t size, uint16_t* out_slot_token) {
    int slot = -1;
    kain_ownership_lock();
    int status = kain_ownership_upsert_unlocked(
        ptr,
        KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION,
        size,
        KAIN_OWNERSHIP_STATE_IDLE,
        0u,
        &slot
    );
    if (status == KAIN_OWNERSHIP_OK && slot >= 0) {
        KainRuntimeHandle handle = kain_fixup_track_allocation(ptr, size);
        if (KAIN_RUNTIME_FIXUP_ENABLED() && handle == KAIN_RUNTIME_HANDLE_INVALID) {
            kain_ownership_clear_slot_unlocked(slot);
            status = kain_ownership_fail(KAIN_OWNERSHIP_ERR_CAPACITY);
        } else {
            KAIN_OWNERSHIP_REGIONS[slot].relocation_handle = handle;
        }
    }
    kain_ownership_unlock();
    if (status == KAIN_OWNERSHIP_OK && out_slot_token != NULL) {
        *out_slot_token = (uint16_t)((uint32_t)slot + 1u);
    }
    return status;
}

int __kain_ownership_ensure_imported(const void* ptr) {
    if (ptr == NULL) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_INVALID);
    }

    kain_ownership_lock();
    /* Proof: runtime/native/src/core/z3/proofs/native-ownership-imported-ensure-fake-header-does-not-bypass-registration.yaml */
    int slot = kain_ownership_find_slot(ptr);
    int status = slot >= 0
        ? KAIN_OWNERSHIP_OK
        : kain_ownership_upsert_unlocked(
            (void*)ptr,
            KAIN_OWNERSHIP_REGION_IMPORTED_POINTER,
            0,
            KAIN_OWNERSHIP_STATE_IDLE,
            0u,
            NULL
        );
    kain_ownership_unlock();
    return status;
}

int __kain_ownership_helper_allocation_state(const void* ptr, uint16_t slot_token) {
    kain_ownership_lock();
    int slot = kain_ownership_helper_slot_from_token_unlocked(ptr, slot_token);
    int status = slot < 0 ? KAIN_OWNERSHIP_ERR_NOT_FOUND : KAIN_OWNERSHIP_REGIONS[slot].state;
    kain_ownership_unlock();
    return status;
}

int __kain_ownership_relocate_helper_allocation(
    void* old_ptr,
    void* new_ptr,
    size_t size,
    uint16_t slot_token
) {
    if (new_ptr == NULL) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_INVALID);
    }

    kain_ownership_lock();
    int slot = kain_ownership_helper_slot_from_token_unlocked(old_ptr, slot_token);
    if (slot < 0) {
        kain_ownership_unlock();
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    /* Proof: runtime/native/src/core/z3/proofs/native-ownership-helper-realloc-slot-fast-path-rejects-non-idle-region.yaml */
    if (region->state != KAIN_OWNERSHIP_STATE_IDLE || region->observers != 0u) {
        int status = kain_ownership_fail(kain_ownership_status_for_busy_region(region));
        kain_ownership_unlock();
        return status;
    }

    if (region->relocation_handle != KAIN_RUNTIME_HANDLE_INVALID &&
        kain_fixup_relocate_allocation(region->relocation_handle, old_ptr, new_ptr, size) != 0) {
        kain_ownership_unlock();
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_INVALID);
    }
    region->ptr = new_ptr;
    region->size = size;
    region->kind = KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION;
    if (new_ptr != old_ptr) {
        kain_ownership_rebuild_pointer_index_unlocked();
    }
    kain_ownership_unlock();
    return KAIN_OWNERSHIP_OK;
}

static int kain_ownership_update_unlocked(void* old_ptr, void* new_ptr, size_t size) {
    int slot = -1;
    KainRuntimeHandle handle = KAIN_RUNTIME_HANDLE_INVALID;
    if (new_ptr == NULL) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_INVALID);
    }
    if (old_ptr == NULL) {
        int status = kain_ownership_upsert_unlocked(
            new_ptr,
            KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION,
            size,
            KAIN_OWNERSHIP_STATE_IDLE,
            0u,
            &slot
        );
        if (status != KAIN_OWNERSHIP_OK) {
            return status;
        }
        handle = kain_fixup_track_allocation(new_ptr, size);
        if (KAIN_RUNTIME_FIXUP_ENABLED() && handle == KAIN_RUNTIME_HANDLE_INVALID) {
            kain_ownership_clear_slot_unlocked(slot);
            return kain_ownership_fail(KAIN_OWNERSHIP_ERR_CAPACITY);
        }
        KAIN_OWNERSHIP_REGIONS[slot].relocation_handle = handle;
        return KAIN_OWNERSHIP_OK;
    }

    slot = kain_ownership_find_slot(old_ptr);
    if (slot < 0) {
        int status = kain_ownership_upsert_unlocked(
            new_ptr,
            KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION,
            size,
            KAIN_OWNERSHIP_STATE_IDLE,
            0u,
            &slot
        );
        if (status != KAIN_OWNERSHIP_OK) {
            return status;
        }
        handle = kain_fixup_track_allocation(new_ptr, size);
        if (KAIN_RUNTIME_FIXUP_ENABLED() && handle == KAIN_RUNTIME_HANDLE_INVALID) {
            kain_ownership_clear_slot_unlocked(slot);
            return kain_ownership_fail(KAIN_OWNERSHIP_ERR_CAPACITY);
        }
        KAIN_OWNERSHIP_REGIONS[slot].relocation_handle = handle;
        return KAIN_OWNERSHIP_OK;
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    if (region->state != KAIN_OWNERSHIP_STATE_IDLE || region->observers != 0) {
        return kain_ownership_fail(kain_ownership_status_for_busy_region(region));
    }

    if (region->relocation_handle != KAIN_RUNTIME_HANDLE_INVALID) {
        if (kain_fixup_relocate_allocation(region->relocation_handle, old_ptr, new_ptr, size) != 0) {
            return kain_ownership_fail(KAIN_OWNERSHIP_ERR_INVALID);
        }
    } else {
        handle = kain_fixup_track_allocation(new_ptr, size);
        if (KAIN_RUNTIME_FIXUP_ENABLED() && handle == KAIN_RUNTIME_HANDLE_INVALID) {
            return kain_ownership_fail(KAIN_OWNERSHIP_ERR_CAPACITY);
        }
        region->relocation_handle = handle;
    }
    region->ptr = new_ptr;
    region->size = size;
    region->kind = KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION;
    kain_ownership_rebuild_pointer_index_unlocked();
    return KAIN_OWNERSHIP_OK;
}

int __kain_ownership_update(void* old_ptr, void* new_ptr, size_t size) {
    kain_ownership_lock();
    int status = kain_ownership_update_unlocked(old_ptr, new_ptr, size);
    kain_ownership_unlock();
    return status;
}

static int kain_ownership_begin_observe_slot_unlocked(int slot) {
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    if (region->state == KAIN_OWNERSHIP_STATE_DECAYED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_DECAYED);
    }
    if (region->state == KAIN_OWNERSHIP_STATE_SHARED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_COLLAPSED);
    }
    if (region->state == KAIN_OWNERSHIP_STATE_COLLAPSED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_COLLAPSED);
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-ownership-observer-count-does-not-overflow-when-guard-passes.yaml */
    if (region->observers == UINT32_MAX) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_OVERFLOW);
    }

    region->observers += 1;
    region->state = KAIN_OWNERSHIP_STATE_OBSERVED;
    return KAIN_OWNERSHIP_OK;
}

static int kain_ownership_begin_observe_registered_unlocked(const void* ptr) {
    int slot = kain_ownership_find_slot(ptr);
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }
    return kain_ownership_begin_observe_slot_unlocked(slot);
}

static int kain_ownership_begin_observe_helper_unlocked(const void* ptr) {
    return kain_ownership_begin_observe_slot_unlocked(kain_ownership_find_helper_slot_unlocked(ptr));
}

int __kain_ownership_begin_observe(const void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_begin_observe_registered_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

int __kain_ownership_begin_observe_helper(const void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_begin_observe_helper_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

static int kain_ownership_end_observe_slot_unlocked(int slot) {
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    if (region->state != KAIN_OWNERSHIP_STATE_OBSERVED || region->observers == 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_OBSERVED);
    }

    region->observers -= 1;
    if (region->observers == 0) {
        region->state = KAIN_OWNERSHIP_STATE_IDLE;
    }
    return KAIN_OWNERSHIP_OK;
}

static int kain_ownership_end_observe_registered_unlocked(const void* ptr) {
    int slot = kain_ownership_find_slot(ptr);
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }
    return kain_ownership_end_observe_slot_unlocked(slot);
}

static int kain_ownership_end_observe_helper_unlocked(const void* ptr) {
    return kain_ownership_end_observe_slot_unlocked(kain_ownership_find_helper_slot_unlocked(ptr));
}

int __kain_ownership_end_observe(const void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_end_observe_registered_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

int __kain_ownership_end_observe_helper(const void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_end_observe_helper_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

static int kain_ownership_begin_collapse_slot_unlocked(int slot) {
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    if (region->state == KAIN_OWNERSHIP_STATE_DECAYED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_DECAYED);
    }
    if (region->state == KAIN_OWNERSHIP_STATE_SHARED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_COLLAPSED);
    }
    if (region->state == KAIN_OWNERSHIP_STATE_COLLAPSED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_COLLAPSED);
    }
    if (region->state == KAIN_OWNERSHIP_STATE_OBSERVED || region->observers != 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_OBSERVED);
    }

    region->state = KAIN_OWNERSHIP_STATE_COLLAPSED;
    return KAIN_OWNERSHIP_OK;
}

static int kain_ownership_begin_collapse_registered_unlocked(void* ptr) {
    int slot = kain_ownership_find_slot(ptr);
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }
    return kain_ownership_begin_collapse_slot_unlocked(slot);
}

static int kain_ownership_begin_collapse_helper_unlocked(void* ptr) {
    return kain_ownership_begin_collapse_slot_unlocked(kain_ownership_find_helper_slot_unlocked(ptr));
}

int __kain_ownership_begin_collapse(void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_begin_collapse_registered_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

int __kain_ownership_begin_collapse_helper(void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_begin_collapse_helper_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

static int kain_ownership_end_collapse_slot_unlocked(int slot) {
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    if (region->state != KAIN_OWNERSHIP_STATE_COLLAPSED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_COLLAPSED);
    }

    region->state = KAIN_OWNERSHIP_STATE_IDLE;
    return KAIN_OWNERSHIP_OK;
}

static int kain_ownership_end_collapse_registered_unlocked(void* ptr) {
    int slot = kain_ownership_find_slot(ptr);
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }
    return kain_ownership_end_collapse_slot_unlocked(slot);
}

static int kain_ownership_end_collapse_helper_unlocked(void* ptr) {
    return kain_ownership_end_collapse_slot_unlocked(kain_ownership_find_helper_slot_unlocked(ptr));
}

int __kain_ownership_end_collapse(void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_end_collapse_registered_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

int __kain_ownership_end_collapse_helper(void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_end_collapse_helper_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

static int kain_ownership_begin_share_slot_unlocked(int slot) {
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    /* Proof: runtime/native/src/core/z3/proofs/native-ownership-share-requires-idle-region.yaml */
    if (region->state == KAIN_OWNERSHIP_STATE_DECAYED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_DECAYED);
    }
    if (region->state == KAIN_OWNERSHIP_STATE_SHARED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_COLLAPSED);
    }
    if (region->state == KAIN_OWNERSHIP_STATE_COLLAPSED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_COLLAPSED);
    }
    if (region->state == KAIN_OWNERSHIP_STATE_OBSERVED || region->observers != 0u) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_OBSERVED);
    }

    region->state = KAIN_OWNERSHIP_STATE_SHARED;
    return KAIN_OWNERSHIP_OK;
}

static int kain_ownership_begin_share_registered_unlocked(void* ptr) {
    int slot = kain_ownership_find_slot(ptr);
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }
    return kain_ownership_begin_share_slot_unlocked(slot);
}

static int kain_ownership_begin_share_helper_unlocked(void* ptr) {
    return kain_ownership_begin_share_slot_unlocked(kain_ownership_find_helper_slot_unlocked(ptr));
}

int __kain_ownership_begin_share(void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_begin_share_registered_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

int __kain_ownership_begin_share_helper(void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_begin_share_helper_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

static int kain_ownership_end_share_slot_unlocked(int slot) {
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    if (region->state != KAIN_OWNERSHIP_STATE_SHARED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_COLLAPSED);
    }

    region->state = KAIN_OWNERSHIP_STATE_IDLE;
    return KAIN_OWNERSHIP_OK;
}

static int kain_ownership_end_share_registered_unlocked(void* ptr) {
    int slot = kain_ownership_find_slot(ptr);
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }
    return kain_ownership_end_share_slot_unlocked(slot);
}

static int kain_ownership_end_share_helper_unlocked(void* ptr) {
    return kain_ownership_end_share_slot_unlocked(kain_ownership_find_helper_slot_unlocked(ptr));
}

int __kain_ownership_end_share(void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_end_share_registered_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

int __kain_ownership_end_share_helper(void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_end_share_helper_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

static int kain_ownership_decay_slot_unlocked(void* ptr, int slot, int reclaim_helper_slot) {
    (void)reclaim_helper_slot;
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    /* Proof: runtime/native/src/core/z3/proofs/native-ownership-decay-free-only-for-idle-heap-region.yaml */
    if (region->state == KAIN_OWNERSHIP_STATE_DECAYED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_DECAYED);
    }
    if (region->state == KAIN_OWNERSHIP_STATE_SHARED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_COLLAPSED);
    }
    if (region->state == KAIN_OWNERSHIP_STATE_COLLAPSED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_COLLAPSED);
    }
    if (region->state == KAIN_OWNERSHIP_STATE_OBSERVED || region->observers != 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_OBSERVED);
    }

    if (kain_ownership_region_is_heap(region)) {
        region->state = KAIN_OWNERSHIP_STATE_DECAYED;
        region->observers = 0u;
        return kain_ownership_enqueue_deferred_decay_unlocked(slot);
    }

    region->state = KAIN_OWNERSHIP_STATE_DECAYED;
    region->observers = 0;
    return KAIN_OWNERSHIP_OK;
}

static int kain_ownership_decay_registered_unlocked(void* ptr) {
    int slot = kain_ownership_find_slot(ptr);
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }
    return kain_ownership_decay_slot_unlocked(ptr, slot, 0);
}

static int kain_ownership_decay_helper_unlocked(void* ptr) {
    return kain_ownership_decay_slot_unlocked(
        ptr,
        kain_ownership_find_helper_slot_unlocked(ptr),
        1
    );
}

int __kain_ownership_decay(void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_decay_registered_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

int __kain_ownership_decay_helper(void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_decay_helper_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

static int kain_ownership_state_unlocked(const void* ptr) {
    int slot = kain_ownership_find_slot(ptr);
    if (slot < 0) {
        return KAIN_OWNERSHIP_ERR_NOT_FOUND;
    }
    return KAIN_OWNERSHIP_REGIONS[slot].state;
}

int __kain_ownership_state(const void* ptr) {
    kain_ownership_lock();
    int state = kain_ownership_state_unlocked(ptr);
    kain_ownership_unlock();
    return state;
}

void __kain_ownership_flush_deferred_decay(void) {
    KainProfileScope scope;
    kain_profile_scope_begin(
        &scope,
        "ownership.deferred_decay_flush",
        __FILE__,
        (uint32_t)__LINE__
    );
    for (;;) {
        int slot = -1;
        void* ptr = NULL;
        KainRuntimeHandle handle = KAIN_RUNTIME_HANDLE_INVALID;
        KainOwnershipRegion* region;
        kain_ownership_lock();
        if (KAIN_OWNERSHIP_DEFERRED_DECAY_COUNT == 0u) {
            kain_ownership_unlock();
            break;
        }
        slot = (int)KAIN_OWNERSHIP_DEFERRED_DECAY_RING[KAIN_OWNERSHIP_DEFERRED_DECAY_HEAD];
        KAIN_OWNERSHIP_DEFERRED_DECAY_HEAD =
            (KAIN_OWNERSHIP_DEFERRED_DECAY_HEAD + 1u) % KAIN_OWNERSHIP_MAX_REGIONS;
        KAIN_OWNERSHIP_DEFERRED_DECAY_COUNT -= 1u;
        region = &KAIN_OWNERSHIP_REGIONS[slot];
        if (!region->occupied || !region->decay_queued || !kain_ownership_region_is_heap(region)) {
            region->decay_queued = 0u;
            kain_ownership_unlock();
            continue;
        }
        region->decay_queued = 0u;
        ptr = region->ptr;
        handle = region->relocation_handle;
        kain_ownership_unlock();

        if (handle != KAIN_RUNTIME_HANDLE_INVALID) {
            (void)kain_fixup_unregister_allocation(handle);
        }
        (void)__kain_free(ptr);

        kain_ownership_lock();
        region = &KAIN_OWNERSHIP_REGIONS[slot];
        if (region->occupied && region->ptr == ptr && region->state == KAIN_OWNERSHIP_STATE_DECAYED) {
            kain_ownership_clear_slot_unlocked(slot);
        }
        kain_ownership_unlock();
    }
    kain_profile_scope_end(&scope);
}

uint64_t __kain_ownership_deferred_decay_count(void) {
    uint64_t count;
    kain_ownership_lock();
    count = (uint64_t)KAIN_OWNERSHIP_DEFERRED_DECAY_COUNT;
    kain_ownership_unlock();
    return count;
}
