#include "../../include/handle.h"

#define KAIN_HANDLE_SLOT_MASK UINT64_C(0x00000000ffffffff)
#define KAIN_HANDLE_MAGIC_MASK UINT64_C(0x00ffffff00000000)
#define KAIN_HANDLE_KIND_MASK UINT64_C(0xff00000000000000)

/* Proof: runtime/native/src/core/z3/proofs/native-handle-nonzero-magic-branchless.yaml */
static uint32_t kain_handle_nonzero_magic(uint32_t magic) {
    uint32_t m = magic & 0x00ffffffu;
    /* Branchless: (m == 0) → 1, else m. Uses SETcc + OR on x86, no branch. */
    return m | (uint32_t)(m == 0u);
}

KainRuntimeHandle kain_handle_make(uint32_t kind, uint32_t slot, uint32_t magic) {
    uint64_t packed_kind = ((uint64_t)(kind & 0xffu)) << 56u;
    uint64_t packed_magic = ((uint64_t)(kain_handle_nonzero_magic(magic) & 0x00ffffffu)) << 32u;
    return packed_kind | packed_magic | (uint64_t)(slot + 1u);
}

uint32_t kain_handle_kind(KainRuntimeHandle handle) {
    return (uint32_t)((handle & KAIN_HANDLE_KIND_MASK) >> 56u);
}

/* Proof: runtime/native/src/core/z3/proofs/native-handle-slot-branchless.yaml */
uint32_t kain_handle_slot(KainRuntimeHandle handle) {
    uint32_t encoded = (uint32_t)(handle & KAIN_HANDLE_SLOT_MASK);
    /* Branchless: encoded=0 wraps to UINT32_MAX (sentinel), else encoded-1 */
    return encoded - 1u;
}

uint32_t kain_handle_magic(KainRuntimeHandle handle) {
    return (uint32_t)((handle & KAIN_HANDLE_MAGIC_MASK) >> 32u);
}

void kain_handle_table_init(
    KainHandleTable* table,
    KainHandleSlot* slots,
    uint32_t capacity
) {
    uint32_t index;
    if (!table) {
        return;
    }
    table->slots = slots;
    table->capacity = capacity;
    table->first_free = capacity == 0u ? UINT32_MAX : 0u;
    table->live_count = 0u;
    table->initialized = 1u;
    if (!slots) {
        return;
    }
    for (index = 0u; index < capacity; ++index) {
        slots[index].payload = 0;
        slots[index].kind = KAIN_HANDLE_KIND_NONE;
        slots[index].magic = 1u;
        slots[index].next_free = index + 1u < capacity ? index + 1u : UINT32_MAX;
        slots[index].occupied = 0u;
    }
}

static int kain_handle_validate_slot(
    const KainHandleTable* table,
    KainRuntimeHandle handle,
    uint32_t expected_kind,
    uint32_t* out_slot
) {
    uint32_t slot;
    uint32_t magic;
    if (!table || !table->initialized || !table->slots || handle == KAIN_RUNTIME_HANDLE_INVALID) {
        return 0;
    }
    slot = kain_handle_slot(handle);
    magic = kain_handle_magic(handle);
    if (slot == UINT32_MAX || slot >= table->capacity || magic == 0u) {
        return 0;
    }
    if (!table->slots[slot].occupied) {
        return 0;
    }
    if (expected_kind != KAIN_HANDLE_KIND_NONE &&
        table->slots[slot].kind != expected_kind) {
        return 0;
    }
    if (table->slots[slot].magic != magic) {
        return 0;
    }
    if (out_slot) {
        *out_slot = slot;
    }
    return 1;
}

KainRuntimeHandle kain_handle_table_acquire(
    KainHandleTable* table,
    uint32_t kind,
    void* payload
) {
    uint32_t slot;
    KainHandleSlot* entry;
    if (!table || !table->initialized || !table->slots || table->first_free == UINT32_MAX) {
        return KAIN_RUNTIME_HANDLE_INVALID;
    }
    slot = table->first_free;
    entry = &table->slots[slot];
    table->first_free = entry->next_free;
    entry->occupied = 1u;
    entry->kind = kind;
    entry->payload = payload;
    entry->magic = kain_handle_nonzero_magic(entry->magic);
    table->live_count += 1u;
    return kain_handle_make(kind, slot, entry->magic);
}

void* kain_handle_table_resolve(
    const KainHandleTable* table,
    KainRuntimeHandle handle,
    uint32_t expected_kind
) {
    uint32_t slot;
    if (!kain_handle_validate_slot(table, handle, expected_kind, &slot)) {
        return 0;
    }
    return table->slots[slot].payload;
}

int kain_handle_table_rebind(
    KainHandleTable* table,
    KainRuntimeHandle handle,
    uint32_t expected_kind,
    void* payload
) {
    uint32_t slot;
    if (!kain_handle_validate_slot(table, handle, expected_kind, &slot)) {
        return -1;
    }
    table->slots[slot].payload = payload;
    return 0;
}

int kain_handle_table_release(
    KainHandleTable* table,
    KainRuntimeHandle handle,
    uint32_t expected_kind
) {
    uint32_t slot;
    KainHandleSlot* entry;
    if (!kain_handle_validate_slot(table, handle, expected_kind, &slot)) {
        return -1;
    }
    entry = &table->slots[slot];
    entry->payload = 0;
    entry->kind = KAIN_HANDLE_KIND_NONE;
    entry->occupied = 0u;
    entry->magic = kain_handle_nonzero_magic(entry->magic + 1u);
    entry->next_free = table->first_free;
    table->first_free = slot;
    if (table->live_count != 0u) {
        table->live_count -= 1u;
    }
    return 0;
}
