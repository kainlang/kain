#ifndef LRU_H
#define LRU_H

#include <stdint.h>

typedef struct {
    uintptr_t start;
    uintptr_t limit;
    void* value;
} KainLruRangeEntry;

typedef struct {
    uintptr_t key;
    void* value;
} KainLruEntry;

static inline void kain_lru_range_clear(KainLruRangeEntry* entry) {
    if (!entry) {
        return;
    }
    entry->start = 0u;
    entry->limit = 0u;
    entry->value = 0;
}

static inline void kain_lru_range_update(
    KainLruRangeEntry* entry,
    uintptr_t start,
    uintptr_t limit,
    void* value
) {
    if (!entry) {
        return;
    }
    entry->start = start;
    entry->limit = limit;
    entry->value = value;
}

static inline void* kain_lru_range_lookup(const KainLruRangeEntry* entry, uintptr_t key) {
    if (!entry || !entry->value) {
        return 0;
    }
    return (key >= entry->start && key < entry->limit) ? entry->value : 0;
}

static inline void kain_lru_clear(KainLruEntry* entry) {
    if (!entry) {
        return;
    }
    entry->key = 0u;
    entry->value = 0;
}

static inline void kain_lru_update(KainLruEntry* entry, uintptr_t key, void* value) {
    if (!entry) {
        return;
    }
    entry->key = key;
    entry->value = value;
}

static inline void* kain_lru_lookup(const KainLruEntry* entry, uintptr_t key) {
    if (!entry || entry->key != key) {
        return 0;
    }
    return entry->value;
}

#endif /* LRU_H */
