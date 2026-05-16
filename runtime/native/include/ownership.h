#ifndef OWNERSHIP_H
#define OWNERSHIP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA = 0,
    KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION = 1,
    KAIN_OWNERSHIP_REGION_RC_OBJECT = 2,
    KAIN_OWNERSHIP_REGION_WORLD_STATE = 3,
    KAIN_OWNERSHIP_REGION_ENTANGLED_AUTHORITY = 4,
    KAIN_OWNERSHIP_REGION_ENTANGLED_MIRROR = 5,
    KAIN_OWNERSHIP_REGION_IMPORTED_POINTER = 6,
};

enum {
    KAIN_OWNERSHIP_STATE_IDLE = 0,
    KAIN_OWNERSHIP_STATE_OBSERVED = 1,
    KAIN_OWNERSHIP_STATE_COLLAPSED = 2,
    KAIN_OWNERSHIP_STATE_DECAYED = 3,
};

enum {
    KAIN_OWNERSHIP_OK = 0,
    KAIN_OWNERSHIP_ERR_INVALID = -1,
    KAIN_OWNERSHIP_ERR_NOT_FOUND = -2,
    KAIN_OWNERSHIP_ERR_CAPACITY = -3,
    KAIN_OWNERSHIP_ERR_OBSERVED = -4,
    KAIN_OWNERSHIP_ERR_COLLAPSED = -5,
    KAIN_OWNERSHIP_ERR_DECAYED = -6,
    KAIN_OWNERSHIP_ERR_OVERFLOW = -7,
    KAIN_OWNERSHIP_ERR_NOT_OBSERVED = -8,
    KAIN_OWNERSHIP_ERR_NOT_COLLAPSED = -9,
};

int __kain_ownership_register(void* ptr, int64_t region_kind, size_t size);
int __kain_ownership_register_imported(void* ptr, size_t size);
int __kain_ownership_register_helper_allocation(void* ptr, size_t size, uint16_t* out_slot_token);
int __kain_ownership_ensure_imported(const void* ptr);
int __kain_ownership_helper_allocation_state(const void* ptr, uint16_t slot_token);
int __kain_ownership_relocate_helper_allocation(
    void* old_ptr,
    void* new_ptr,
    size_t size,
    uint16_t slot_token
);
int __kain_ownership_update(void* old_ptr, void* new_ptr, size_t size);
int __kain_ownership_begin_observe(const void* ptr);
int __kain_ownership_end_observe(const void* ptr);
int __kain_ownership_begin_collapse(void* ptr);
int __kain_ownership_end_collapse(void* ptr);
int __kain_ownership_decay(void* ptr);
int __kain_ownership_begin_observe_helper(const void* ptr);
int __kain_ownership_end_observe_helper(const void* ptr);
int __kain_ownership_begin_collapse_helper(void* ptr);
int __kain_ownership_end_collapse_helper(void* ptr);
int __kain_ownership_decay_helper(void* ptr);
int __kain_ownership_state(const void* ptr);

#ifdef __cplusplus
}
#endif

#endif /* OWNERSHIP_H */
