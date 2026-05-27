#include "../../include/fixup.h"
#include "../../include/base.h"
#include "../../include/lru.h"
#include "../../include/profile.h"
#include "../../include/runtime_tiers.h"

#include <stdatomic.h>
#include <string.h>

#define KAIN_FIXUP_MAX_OBJECTS 4096u
#define KAIN_FIXUP_MAX_REFS 16384u
#define KAIN_FIXUP_REF_NONE UINT32_MAX

typedef struct {
    KainRuntimeHandle handle;
    void* base;
    size_t size;
    uint32_t first_ref;
    uint32_t live;
} KainFixupObject;

typedef struct {
    void** location;
    KainRuntimeHandle handle;
    uintptr_t offset;
    uint32_t next;
    uint32_t next_free;
    uint8_t occupied;
} KainFixupKnownRef;

static KainHandleSlot KAIN_FIXUP_HANDLE_SLOTS[KAIN_FIXUP_MAX_OBJECTS];
static KainHandleTable KAIN_FIXUP_HANDLE_TABLE;
static KainFixupObject KAIN_FIXUP_OBJECTS[KAIN_FIXUP_MAX_OBJECTS];
static KainFixupKnownRef KAIN_FIXUP_REFS[KAIN_FIXUP_MAX_REFS];
static KainLruRangeEntry KAIN_FIXUP_LAST_RANGE;
static atomic_flag KAIN_FIXUP_LOCK = ATOMIC_FLAG_INIT;
static atomic_int KAIN_FIXUP_INITIALIZED;
static uint32_t KAIN_FIXUP_FIRST_FREE_REF = KAIN_FIXUP_REF_NONE;
static uint64_t KAIN_FIXUP_REF_COUNT = 0u;
static atomic_uint_fast64_t KAIN_FIXUP_RELOCATION_COUNT;
static atomic_uint_fast64_t KAIN_FIXUP_LAST_HANDLE;

static void kain_fixup_lock(void) {
    while (atomic_flag_test_and_set_explicit(&KAIN_FIXUP_LOCK, memory_order_acquire)) {
    }
}

static void kain_fixup_unlock(void) {
    atomic_flag_clear_explicit(&KAIN_FIXUP_LOCK, memory_order_release);
}

static int kain_fixup_try_range_limit(uintptr_t base, size_t size, uintptr_t* out_limit) {
    if (!out_limit) {
        return 0;
    }
    if (size > (size_t)(UINTPTR_MAX - base)) {
        return 0;
    }
    *out_limit = base + (uintptr_t)size;
    return 1;
}

static void kain_fixup_refresh_range_cache_unlocked(KainFixupObject* object) {
    uintptr_t start;
    uintptr_t limit;
    if (!object || !object->base) {
        kain_lru_range_clear(&KAIN_FIXUP_LAST_RANGE);
        return;
    }
    start = (uintptr_t)object->base;
    if (!kain_fixup_try_range_limit(start, object->size, &limit)) {
        kain_lru_range_clear(&KAIN_FIXUP_LAST_RANGE);
        return;
    }
    kain_lru_range_update(&KAIN_FIXUP_LAST_RANGE, start, limit, object);
}

static void kain_fixup_init_unlocked(void) {
    uint32_t index;
    if (atomic_load_explicit(&KAIN_FIXUP_INITIALIZED, memory_order_acquire) != 0) {
        return;
    }
    kain_handle_table_init(
        &KAIN_FIXUP_HANDLE_TABLE,
        KAIN_FIXUP_HANDLE_SLOTS,
        KAIN_FIXUP_MAX_OBJECTS
    );
    for (index = 0u; index < KAIN_FIXUP_MAX_OBJECTS; ++index) {
        KAIN_FIXUP_OBJECTS[index].handle = KAIN_RUNTIME_HANDLE_INVALID;
        KAIN_FIXUP_OBJECTS[index].base = 0;
        KAIN_FIXUP_OBJECTS[index].size = 0u;
        KAIN_FIXUP_OBJECTS[index].first_ref = KAIN_FIXUP_REF_NONE;
        KAIN_FIXUP_OBJECTS[index].live = 0u;
    }
    for (index = 0u; index < KAIN_FIXUP_MAX_REFS; ++index) {
        KAIN_FIXUP_REFS[index].location = 0;
        KAIN_FIXUP_REFS[index].handle = KAIN_RUNTIME_HANDLE_INVALID;
        KAIN_FIXUP_REFS[index].offset = 0u;
        KAIN_FIXUP_REFS[index].next = KAIN_FIXUP_REF_NONE;
        KAIN_FIXUP_REFS[index].next_free = index + 1u < KAIN_FIXUP_MAX_REFS ? index + 1u : KAIN_FIXUP_REF_NONE;
        KAIN_FIXUP_REFS[index].occupied = 0u;
    }
    KAIN_FIXUP_FIRST_FREE_REF = 0u;
    KAIN_FIXUP_REF_COUNT = 0u;
    kain_lru_range_clear(&KAIN_FIXUP_LAST_RANGE);
    atomic_store_explicit(&KAIN_FIXUP_INITIALIZED, 1, memory_order_release);
}

void kain_fixup_init(void) {
    if (!KAIN_RUNTIME_FIXUP_ENABLED()) {
        return;
    }
    kain_fixup_lock();
    kain_fixup_init_unlocked();
    kain_fixup_unlock();
}

static KainFixupObject* kain_fixup_object_for_handle_unlocked(KainRuntimeHandle handle) {
    uint32_t slot;
    if (handle == KAIN_RUNTIME_HANDLE_INVALID) {
        return 0;
    }
    slot = kain_handle_slot(handle);
    if (slot >= KAIN_FIXUP_MAX_OBJECTS) {
        return 0;
    }
    if (!kain_handle_table_resolve(&KAIN_FIXUP_HANDLE_TABLE, handle, KAIN_HANDLE_KIND_FIXUP_OBJECT)) {
        return 0;
    }
    if (!KAIN_FIXUP_OBJECTS[slot].live || KAIN_FIXUP_OBJECTS[slot].handle != handle) {
        return 0;
    }
    return &KAIN_FIXUP_OBJECTS[slot];
}

static KainFixupObject* kain_fixup_find_object_by_pointer_unlocked(const void* ptr) {
    uintptr_t address;
    KainFixupObject* cached;
    uint32_t slot;
    if (!ptr) {
        return 0;
    }
    address = (uintptr_t)ptr;
    cached = (KainFixupObject*)kain_lru_range_lookup(&KAIN_FIXUP_LAST_RANGE, address);
    if (cached && cached->live) {
        return cached;
    }
    for (slot = 0u; slot < KAIN_FIXUP_MAX_OBJECTS; ++slot) {
        uintptr_t start;
        uintptr_t limit;
        KainFixupObject* object = &KAIN_FIXUP_OBJECTS[slot];
        if (!object->live || !object->base) {
            continue;
        }
        start = (uintptr_t)object->base;
        if (!kain_fixup_try_range_limit(start, object->size, &limit)) {
            continue;
        }
        if (address >= start && address < limit) {
            kain_lru_range_update(&KAIN_FIXUP_LAST_RANGE, start, limit, object);
            return object;
        }
    }
    return 0;
}

static int kain_fixup_remove_ref_by_location_unlocked(void** location) {
    uint32_t slot;
    if (!location) {
        return -1;
    }
    for (slot = 0u; slot < KAIN_FIXUP_MAX_OBJECTS; ++slot) {
        KainFixupObject* object = &KAIN_FIXUP_OBJECTS[slot];
        uint32_t ref_index = object->first_ref;
        uint32_t prev = KAIN_FIXUP_REF_NONE;
        if (!object->live) {
            continue;
        }
        while (ref_index != KAIN_FIXUP_REF_NONE) {
            KainFixupKnownRef* ref = &KAIN_FIXUP_REFS[ref_index];
            if (ref->occupied && ref->location == location) {
                if (prev == KAIN_FIXUP_REF_NONE) {
                    object->first_ref = ref->next;
                } else {
                    KAIN_FIXUP_REFS[prev].next = ref->next;
                }
                ref->occupied = 0u;
                ref->location = 0;
                ref->handle = KAIN_RUNTIME_HANDLE_INVALID;
                ref->offset = 0u;
                ref->next = KAIN_FIXUP_REF_NONE;
                ref->next_free = KAIN_FIXUP_FIRST_FREE_REF;
                KAIN_FIXUP_FIRST_FREE_REF = ref_index;
                if (KAIN_FIXUP_REF_COUNT != 0u) {
                    KAIN_FIXUP_REF_COUNT -= 1u;
                }
                return 0;
            }
            prev = ref_index;
            ref_index = ref->next;
        }
    }
    return -1;
}

KainRuntimeHandle kain_fixup_track_allocation(void* base, size_t size) {
    KainRuntimeHandle handle = KAIN_RUNTIME_HANDLE_INVALID;
    KainFixupObject* existing;
    KainFixupObject* object;
    uint32_t slot;
    KainProfileScope scope;
    if (!KAIN_RUNTIME_FIXUP_ENABLED() || !base || size == 0u) {
        return KAIN_RUNTIME_HANDLE_INVALID;
    }
    kain_profile_scope_begin(&scope, "fixup.track", __FILE__, (uint32_t)__LINE__);
    kain_fixup_lock();
    kain_fixup_init_unlocked();
    existing = kain_fixup_find_object_by_pointer_unlocked(base);
    if (existing && existing->base == base) {
        existing->size = size;
        handle = existing->handle;
        kain_fixup_refresh_range_cache_unlocked(existing);
        kain_fixup_unlock();
        kain_profile_scope_end(&scope);
        return handle;
    }
    handle = kain_handle_table_acquire(
        &KAIN_FIXUP_HANDLE_TABLE,
        KAIN_HANDLE_KIND_FIXUP_OBJECT,
        base
    );
    if (handle == KAIN_RUNTIME_HANDLE_INVALID) {
        kain_fixup_unlock();
        kain_profile_scope_end(&scope);
        return KAIN_RUNTIME_HANDLE_INVALID;
    }
    slot = kain_handle_slot(handle);
    object = &KAIN_FIXUP_OBJECTS[slot];
    object->handle = handle;
    object->base = base;
    object->size = size;
    object->first_ref = KAIN_FIXUP_REF_NONE;
    object->live = 1u;
    (void)kain_handle_table_rebind(
        &KAIN_FIXUP_HANDLE_TABLE,
        handle,
        KAIN_HANDLE_KIND_FIXUP_OBJECT,
        object
    );
    kain_fixup_refresh_range_cache_unlocked(object);
    kain_fixup_unlock();
    kain_profile_scope_end(&scope);
    return handle;
}

KainRuntimeHandle kain_fixup_handle_for_pointer(const void* ptr) {
    KainRuntimeHandle handle = KAIN_RUNTIME_HANDLE_INVALID;
    if (!KAIN_RUNTIME_FIXUP_ENABLED() || !ptr) {
        return KAIN_RUNTIME_HANDLE_INVALID;
    }
    kain_fixup_lock();
    kain_fixup_init_unlocked();
    {
        KainFixupObject* object = kain_fixup_find_object_by_pointer_unlocked(ptr);
        if (object) {
            handle = object->handle;
        }
    }
    kain_fixup_unlock();
    return handle;
}

void* kain_fixup_resolve_handle(KainRuntimeHandle handle) {
    void* result = 0;
    if (!KAIN_RUNTIME_FIXUP_ENABLED()) {
        return 0;
    }
    kain_fixup_lock();
    kain_fixup_init_unlocked();
    {
        KainFixupObject* object = kain_fixup_object_for_handle_unlocked(handle);
        if (object) {
            result = object->base;
        }
    }
    kain_fixup_unlock();
    return result;
}

size_t kain_fixup_handle_size(KainRuntimeHandle handle) {
    size_t result = 0u;
    if (!KAIN_RUNTIME_FIXUP_ENABLED()) {
        return 0u;
    }
    kain_fixup_lock();
    kain_fixup_init_unlocked();
    {
        KainFixupObject* object = kain_fixup_object_for_handle_unlocked(handle);
        if (object) {
            result = object->size;
        }
    }
    kain_fixup_unlock();
    return result;
}

int kain_fixup_view(KainRuntimeHandle handle, KainFixupTrackedView* out_view) {
    int status = -1;
    if (!out_view) {
        return -1;
    }
    out_view->handle = KAIN_RUNTIME_HANDLE_INVALID;
    out_view->base = 0;
    out_view->size = 0u;
    if (!KAIN_RUNTIME_FIXUP_ENABLED()) {
        return -1;
    }
    kain_fixup_lock();
    kain_fixup_init_unlocked();
    {
        KainFixupObject* object = kain_fixup_object_for_handle_unlocked(handle);
        if (object) {
            out_view->handle = object->handle;
            out_view->base = object->base;
            out_view->size = object->size;
            status = 0;
        }
    }
    kain_fixup_unlock();
    return status;
}

int kain_fixup_register_known_ref(void** location) {
    KainFixupObject* object;
    uint32_t ref_index;
    uintptr_t target_addr;
    uintptr_t base_addr;
    KainProfileScope scope;
    if (!KAIN_RUNTIME_FIXUP_ENABLED() || !location || !*location) {
        return -1;
    }
    kain_profile_scope_begin(&scope, "fixup.register_ref", __FILE__, (uint32_t)__LINE__);
    kain_fixup_lock();
    kain_fixup_init_unlocked();
    object = kain_fixup_find_object_by_pointer_unlocked(*location);
    if (!object || KAIN_FIXUP_FIRST_FREE_REF == KAIN_FIXUP_REF_NONE) {
        kain_fixup_unlock();
        kain_profile_scope_end(&scope);
        return -1;
    }
    (void)kain_fixup_remove_ref_by_location_unlocked(location);
    ref_index = KAIN_FIXUP_FIRST_FREE_REF;
    KAIN_FIXUP_FIRST_FREE_REF = KAIN_FIXUP_REFS[ref_index].next_free;
    target_addr = (uintptr_t)(*location);
    base_addr = (uintptr_t)object->base;
    KAIN_FIXUP_REFS[ref_index].occupied = 1u;
    KAIN_FIXUP_REFS[ref_index].location = location;
    KAIN_FIXUP_REFS[ref_index].handle = object->handle;
    KAIN_FIXUP_REFS[ref_index].offset = target_addr - base_addr;
    KAIN_FIXUP_REFS[ref_index].next = object->first_ref;
    KAIN_FIXUP_REFS[ref_index].next_free = KAIN_FIXUP_REF_NONE;
    object->first_ref = ref_index;
    KAIN_FIXUP_REF_COUNT += 1u;
    kain_fixup_unlock();
    kain_profile_scope_end(&scope);
    return 0;
}

int kain_fixup_unregister_known_ref(void** location) {
    int status;
    if (!KAIN_RUNTIME_FIXUP_ENABLED() || !location) {
        return -1;
    }
    kain_fixup_lock();
    kain_fixup_init_unlocked();
    status = kain_fixup_remove_ref_by_location_unlocked(location);
    kain_fixup_unlock();
    return status;
}

int kain_fixup_update_known_ref(void** location, void* value) {
    int status = 0;
    if (!KAIN_RUNTIME_FIXUP_ENABLED() || !location) {
        return -1;
    }
    kain_fixup_lock();
    kain_fixup_init_unlocked();
    (void)kain_fixup_remove_ref_by_location_unlocked(location);
    *location = value;
    if (!value) {
        kain_fixup_unlock();
        return 0;
    }
    {
        KainFixupObject* object = kain_fixup_find_object_by_pointer_unlocked(value);
        uint32_t ref_index;
        uintptr_t target_addr;
        uintptr_t base_addr;
        if (!object || KAIN_FIXUP_FIRST_FREE_REF == KAIN_FIXUP_REF_NONE) {
            kain_fixup_unlock();
            return -1;
        }
        ref_index = KAIN_FIXUP_FIRST_FREE_REF;
        KAIN_FIXUP_FIRST_FREE_REF = KAIN_FIXUP_REFS[ref_index].next_free;
        target_addr = (uintptr_t)value;
        base_addr = (uintptr_t)object->base;
        KAIN_FIXUP_REFS[ref_index].occupied = 1u;
        KAIN_FIXUP_REFS[ref_index].location = location;
        KAIN_FIXUP_REFS[ref_index].handle = object->handle;
        KAIN_FIXUP_REFS[ref_index].offset = target_addr - base_addr;
        KAIN_FIXUP_REFS[ref_index].next = object->first_ref;
        KAIN_FIXUP_REFS[ref_index].next_free = KAIN_FIXUP_REF_NONE;
        object->first_ref = ref_index;
        KAIN_FIXUP_REF_COUNT += 1u;
    }
    kain_fixup_unlock();
    return status;
}

int kain_fixup_relocate_allocation(
    KainRuntimeHandle handle,
    void* old_base,
    void* new_base,
    size_t size
) {
    KainFixupObject* object;
    uint32_t ref_index;
    uintptr_t range_limit;
    KainProfileScope scope;
    if (!KAIN_RUNTIME_FIXUP_ENABLED() || handle == KAIN_RUNTIME_HANDLE_INVALID || !new_base || size == 0u) {
        return -1;
    }
    kain_profile_scope_begin(&scope, "fixup.relocate", __FILE__, (uint32_t)__LINE__);
    kain_fixup_lock();
    kain_fixup_init_unlocked();
    object = kain_fixup_object_for_handle_unlocked(handle);
    if (!object || (old_base && object->base != old_base)) {
        kain_fixup_unlock();
        kain_profile_scope_end(&scope);
        return -1;
    }
    if (!kain_fixup_try_range_limit((uintptr_t)new_base, size, &range_limit)) {
        kain_fixup_unlock();
        kain_profile_scope_end(&scope);
        return -1;
    }
    (void)range_limit;
    object->base = new_base;
    object->size = size;
    ref_index = object->first_ref;
    while (ref_index != KAIN_FIXUP_REF_NONE) {
        KainFixupKnownRef* ref = &KAIN_FIXUP_REFS[ref_index];
        if (ref->occupied && ref->handle == handle && ref->location) {
            *ref->location = (void*)((uintptr_t)new_base + ref->offset);
        }
        ref_index = ref->next;
    }
    kain_fixup_refresh_range_cache_unlocked(object);
    atomic_fetch_add_explicit(&KAIN_FIXUP_RELOCATION_COUNT, 1u, memory_order_relaxed);
    atomic_store_explicit(&KAIN_FIXUP_LAST_HANDLE, handle, memory_order_release);
    kain_fixup_unlock();
    kain_profile_scope_end(&scope);
    return 0;
}

int kain_fixup_unregister_allocation(KainRuntimeHandle handle) {
    KainFixupObject* object;
    uint32_t ref_index;
    KainProfileScope scope;
    if (!KAIN_RUNTIME_FIXUP_ENABLED() || handle == KAIN_RUNTIME_HANDLE_INVALID) {
        return -1;
    }
    kain_profile_scope_begin(&scope, "fixup.unregister", __FILE__, (uint32_t)__LINE__);
    kain_fixup_lock();
    kain_fixup_init_unlocked();
    object = kain_fixup_object_for_handle_unlocked(handle);
    if (!object) {
        kain_fixup_unlock();
        kain_profile_scope_end(&scope);
        return -1;
    }
    ref_index = object->first_ref;
    while (ref_index != KAIN_FIXUP_REF_NONE) {
        KainFixupKnownRef* ref = &KAIN_FIXUP_REFS[ref_index];
        uint32_t next = ref->next;
        if (ref->occupied) {
            if (ref->location) {
                *ref->location = 0;
            }
            ref->occupied = 0u;
            ref->location = 0;
            ref->handle = KAIN_RUNTIME_HANDLE_INVALID;
            ref->offset = 0u;
            ref->next = KAIN_FIXUP_REF_NONE;
            ref->next_free = KAIN_FIXUP_FIRST_FREE_REF;
            KAIN_FIXUP_FIRST_FREE_REF = ref_index;
            if (KAIN_FIXUP_REF_COUNT != 0u) {
                KAIN_FIXUP_REF_COUNT -= 1u;
            }
        }
        ref_index = next;
    }
    object->first_ref = KAIN_FIXUP_REF_NONE;
    object->live = 0u;
    object->base = 0;
    object->size = 0u;
    object->handle = KAIN_RUNTIME_HANDLE_INVALID;
    (void)kain_handle_table_release(
        &KAIN_FIXUP_HANDLE_TABLE,
        handle,
        KAIN_HANDLE_KIND_FIXUP_OBJECT
    );
    kain_lru_range_clear(&KAIN_FIXUP_LAST_RANGE);
    kain_fixup_unlock();
    kain_profile_scope_end(&scope);
    return 0;
}

uint64_t kain_fixup_known_ref_count(void) {
    uint64_t count;
    kain_fixup_lock();
    count = KAIN_FIXUP_REF_COUNT;
    kain_fixup_unlock();
    return count;
}

uint64_t kain_fixup_relocation_count(void) {
    return atomic_load_explicit(&KAIN_FIXUP_RELOCATION_COUNT, memory_order_acquire);
}

KainRuntimeHandle kain_fixup_last_handle(void) {
    return atomic_load_explicit(&KAIN_FIXUP_LAST_HANDLE, memory_order_acquire);
}
