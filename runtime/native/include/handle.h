#ifndef HANDLE_H
#define HANDLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t KainRuntimeHandle;

enum {
    KAIN_HANDLE_KIND_NONE = 0,
    KAIN_HANDLE_KIND_FIXUP_OBJECT = 1,
    KAIN_HANDLE_KIND_PROFILE_ZONE = 2,
};

#define KAIN_RUNTIME_HANDLE_INVALID UINT64_C(0)

typedef struct {
    void* payload;
    uint32_t kind;
    uint32_t magic;
    uint32_t next_free;
    uint32_t occupied;
} KainHandleSlot;

typedef struct {
    KainHandleSlot* slots;
    uint32_t capacity;
    uint32_t first_free;
    uint32_t live_count;
    uint32_t initialized;
} KainHandleTable;

KainRuntimeHandle kain_handle_make(uint32_t kind, uint32_t slot, uint32_t magic);
uint32_t kain_handle_kind(KainRuntimeHandle handle);
uint32_t kain_handle_slot(KainRuntimeHandle handle);
uint32_t kain_handle_magic(KainRuntimeHandle handle);

void kain_handle_table_init(
    KainHandleTable* table,
    KainHandleSlot* slots,
    uint32_t capacity
);

KainRuntimeHandle kain_handle_table_acquire(
    KainHandleTable* table,
    uint32_t kind,
    void* payload
);

void* kain_handle_table_resolve(
    const KainHandleTable* table,
    KainRuntimeHandle handle,
    uint32_t expected_kind
);

int kain_handle_table_rebind(
    KainHandleTable* table,
    KainRuntimeHandle handle,
    uint32_t expected_kind,
    void* payload
);

int kain_handle_table_release(
    KainHandleTable* table,
    KainRuntimeHandle handle,
    uint32_t expected_kind
);

#ifdef __cplusplus
}
#endif

#endif /* HANDLE_H */
