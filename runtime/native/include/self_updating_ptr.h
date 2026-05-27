#ifndef SELF_UPDATING_PTR_H
#define SELF_UPDATING_PTR_H

#include <stdint.h>

#include "fixup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    KainRuntimeHandle handle;
    uintptr_t offset;
    void* current;
} KainSelfUpdatingPtr;

static inline void kain_self_updating_ptr_init(KainSelfUpdatingPtr* ptr) {
    if (!ptr) {
        return;
    }
    ptr->handle = KAIN_RUNTIME_HANDLE_INVALID;
    ptr->offset = 0u;
    ptr->current = 0;
}

static inline int kain_self_updating_ptr_bind(
    KainSelfUpdatingPtr* ptr,
    KainRuntimeHandle handle,
    void* target
) {
    KainFixupTrackedView view;
    KainRuntimeHandle previous_handle;
    uintptr_t previous_offset;
    void* previous_current;
    uintptr_t base_addr;
    uintptr_t target_addr;
    if (!ptr) {
        return -1;
    }
    if (kain_fixup_view(handle, &view) != 0 || !view.base || !target) {
        return -1;
    }
    base_addr = (uintptr_t)view.base;
    target_addr = (uintptr_t)target;
    if (target_addr < base_addr || target_addr - base_addr >= view.size) {
        return -1;
    }
    previous_handle = ptr->handle;
    previous_offset = ptr->offset;
    previous_current = ptr->current;
    ptr->handle = handle;
    ptr->offset = target_addr - base_addr;
    ptr->current = target;
    if (kain_fixup_register_known_ref((void**)&ptr->current) != 0) {
        ptr->handle = previous_handle;
        ptr->offset = previous_offset;
        ptr->current = previous_current;
        return -1;
    }
    return 0;
}

static inline void* kain_self_updating_ptr_get(const KainSelfUpdatingPtr* ptr) {
    return ptr ? ptr->current : 0;
}

static inline int kain_self_updating_ptr_rebind(KainSelfUpdatingPtr* ptr) {
    KainFixupTrackedView view;
    void* next;
    void* previous_current;
    if (!ptr || kain_fixup_view(ptr->handle, &view) != 0 || !view.base || ptr->offset >= view.size) {
        return -1;
    }
    next = (void*)((uintptr_t)view.base + ptr->offset);
    previous_current = ptr->current;
    ptr->current = next;
    if (kain_fixup_register_known_ref((void**)&ptr->current) != 0) {
        ptr->current = previous_current;
        return -1;
    }
    return 0;
}

static inline void kain_self_updating_ptr_clear(KainSelfUpdatingPtr* ptr) {
    if (!ptr) {
        return;
    }
    (void)kain_fixup_unregister_known_ref((void**)&ptr->current);
    kain_self_updating_ptr_init(ptr);
}

#ifdef __cplusplus
}
#endif

#endif /* SELF_UPDATING_PTR_H */
