/*
 * KAIN Native Runtime Ownership Helpers
 *
 * Runtime guard surface for the compiler-owned collapse/observe/decay memory
 * model. The compiler owns the syntax and lowering; this file owns checked
 * native state transitions for helper-owned heap memory and explicitly
 * registered imported pointers.
 */

#include "../../include/kain_runtime_ownership.h"
#include "../../include/kain_runtime_memory.h"
#include <errno.h>
#include <stdint.h>
#include <stdatomic.h>

#define KAIN_OWNERSHIP_MAX_REGIONS 4096u

typedef struct KainOwnershipRegion {
    void* ptr;
    size_t size;
    int64_t kind;
    int state;
    uint32_t observers;
    int occupied;
} KainOwnershipRegion;

static KainOwnershipRegion KAIN_OWNERSHIP_REGIONS[KAIN_OWNERSHIP_MAX_REGIONS];
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

static int kain_ownership_find_slot(const void* ptr) {
    if (ptr == NULL) {
        return -1;
    }
    for (uint32_t i = 0; i < KAIN_OWNERSHIP_MAX_REGIONS; ++i) {
        if (KAIN_OWNERSHIP_REGIONS[i].occupied && KAIN_OWNERSHIP_REGIONS[i].ptr == ptr) {
            return (int)i;
        }
    }
    return -1;
}

static int kain_ownership_find_free_slot(void) {
    for (uint32_t i = 0; i < KAIN_OWNERSHIP_MAX_REGIONS; ++i) {
        if (!KAIN_OWNERSHIP_REGIONS[i].occupied) {
            return (int)i;
        }
    }
    return -1;
}

static int kain_ownership_region_is_heap(const KainOwnershipRegion* region) {
    return region->kind == KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION;
}

static int kain_ownership_register_unlocked(void* ptr, int64_t region_kind, size_t size) {
    if (ptr == NULL) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_INVALID);
    }

    int slot = kain_ownership_find_slot(ptr);
    if (slot < 0) {
        slot = kain_ownership_find_free_slot();
        if (slot < 0) {
            return kain_ownership_fail(KAIN_OWNERSHIP_ERR_CAPACITY);
        }
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    region->ptr = ptr;
    region->size = size;
    region->kind = region_kind;
    region->state = KAIN_OWNERSHIP_STATE_IDLE;
    region->observers = 0;
    region->occupied = 1;
    return KAIN_OWNERSHIP_OK;
}

int __kain_ownership_register(void* ptr, int64_t region_kind, size_t size) {
    kain_ownership_lock();
    int status = kain_ownership_register_unlocked(ptr, region_kind, size);
    kain_ownership_unlock();
    return status;
}

int __kain_ownership_register_imported(void* ptr, size_t size) {
    kain_ownership_lock();
    int status = kain_ownership_register_unlocked(
        ptr,
        KAIN_OWNERSHIP_REGION_IMPORTED_POINTER,
        size
    );
    kain_ownership_unlock();
    return status;
}

static int kain_ownership_update_unlocked(void* old_ptr, void* new_ptr, size_t size) {
    if (new_ptr == NULL) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_INVALID);
    }
    if (old_ptr == NULL) {
        return kain_ownership_register_unlocked(
            new_ptr,
            KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION,
            size
        );
    }

    int slot = kain_ownership_find_slot(old_ptr);
    if (slot < 0) {
        return kain_ownership_register_unlocked(
            new_ptr,
            KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION,
            size
        );
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    if (region->state != KAIN_OWNERSHIP_STATE_IDLE || region->observers != 0) {
        return kain_ownership_fail(region->state == KAIN_OWNERSHIP_STATE_DECAYED
            ? KAIN_OWNERSHIP_ERR_DECAYED
            : KAIN_OWNERSHIP_ERR_OBSERVED);
    }

    region->ptr = new_ptr;
    region->size = size;
    region->kind = KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION;
    return KAIN_OWNERSHIP_OK;
}

int __kain_ownership_update(void* old_ptr, void* new_ptr, size_t size) {
    kain_ownership_lock();
    int status = kain_ownership_update_unlocked(old_ptr, new_ptr, size);
    kain_ownership_unlock();
    return status;
}

static int kain_ownership_begin_observe_unlocked(const void* ptr) {
    int slot = kain_ownership_find_slot(ptr);
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    if (region->state == KAIN_OWNERSHIP_STATE_DECAYED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_DECAYED);
    }
    if (region->state == KAIN_OWNERSHIP_STATE_COLLAPSED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_COLLAPSED);
    }
    if (region->observers == UINT32_MAX) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_OVERFLOW);
    }

    region->observers += 1;
    region->state = KAIN_OWNERSHIP_STATE_OBSERVED;
    return KAIN_OWNERSHIP_OK;
}

int __kain_ownership_begin_observe(const void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_begin_observe_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

static int kain_ownership_end_observe_unlocked(const void* ptr) {
    int slot = kain_ownership_find_slot(ptr);
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

int __kain_ownership_end_observe(const void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_end_observe_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

static int kain_ownership_begin_collapse_unlocked(void* ptr) {
    int slot = kain_ownership_find_slot(ptr);
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    if (region->state == KAIN_OWNERSHIP_STATE_DECAYED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_DECAYED);
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

int __kain_ownership_begin_collapse(void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_begin_collapse_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

static int kain_ownership_end_collapse_unlocked(void* ptr) {
    int slot = kain_ownership_find_slot(ptr);
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

int __kain_ownership_end_collapse(void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_end_collapse_unlocked(ptr);
    kain_ownership_unlock();
    return status;
}

static int kain_ownership_decay_unlocked(void* ptr) {
    int slot = kain_ownership_find_slot(ptr);
    if (slot < 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_NOT_FOUND);
    }

    KainOwnershipRegion* region = &KAIN_OWNERSHIP_REGIONS[slot];
    if (region->state == KAIN_OWNERSHIP_STATE_DECAYED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_DECAYED);
    }
    if (region->state == KAIN_OWNERSHIP_STATE_COLLAPSED) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_COLLAPSED);
    }
    if (region->state == KAIN_OWNERSHIP_STATE_OBSERVED || region->observers != 0) {
        return kain_ownership_fail(KAIN_OWNERSHIP_ERR_OBSERVED);
    }

    if (kain_ownership_region_is_heap(region)) {
        int free_status = __kain_free(ptr);
        if (free_status != 0) {
            return kain_ownership_fail(KAIN_OWNERSHIP_ERR_INVALID);
        }
    }

    region->state = KAIN_OWNERSHIP_STATE_DECAYED;
    region->observers = 0;
    return KAIN_OWNERSHIP_OK;
}

int __kain_ownership_decay(void* ptr) {
    kain_ownership_lock();
    int status = kain_ownership_decay_unlocked(ptr);
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
