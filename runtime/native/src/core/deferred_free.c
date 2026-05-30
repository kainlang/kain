#include "../../include/deferred_free.h"

#include <stddef.h>

int kain_deferred_free_list_init(
    KainDeferredFreeList* list,
    uint32_t* next_storage,
    uint32_t capacity
) {
    if (list == NULL || next_storage == NULL) {
        return -1;
    }

    list->capacity = capacity;
    list->sentinel = capacity;
    list->used_marker = capacity + 1u;
    list->next = next_storage;
    kain_deferred_free_list_make_all_free(list);
    return 0;
}

void kain_deferred_free_list_make_all_free(KainDeferredFreeList* list) {
    if (list == NULL || list->next == NULL) {
        return;
    }

    list->active_first = list->capacity == 0u ? list->sentinel : 0u;
    list->deferred_first = list->sentinel;
    list->deferred_last = list->sentinel;
    for (uint32_t index = 0u; index < list->capacity; ++index) {
        list->next[index] = (index + 1u) < list->capacity ? (index + 1u) : list->sentinel;
    }
}

int kain_deferred_free_list_allocate(KainDeferredFreeList* list) {
    if (list == NULL || list->next == NULL || list->active_first == list->sentinel) {
        return -1;
    }

    uint32_t index = list->active_first;
    list->active_first = list->next[index];
    list->next[index] = list->used_marker;
    return (int)index;
}

void kain_deferred_free_list_deferred_free(KainDeferredFreeList* list, uint32_t index) {
    if (list == NULL || list->next == NULL || index >= list->capacity ||
        list->next[index] != list->used_marker) {
        return;
    }

    list->next[index] = list->sentinel;
    if (list->deferred_first == list->sentinel) {
        list->deferred_first = index;
        list->deferred_last = index;
        return;
    }

    list->next[list->deferred_last] = index;
    list->deferred_last = index;
}

void kain_deferred_free_list_flush(KainDeferredFreeList* list) {
    if (list == NULL || list->next == NULL || list->deferred_first == list->sentinel) {
        return;
    }

    list->next[list->deferred_last] = list->active_first;
    list->active_first = list->deferred_first;
    list->deferred_first = list->sentinel;
    list->deferred_last = list->sentinel;
}

int kain_deferred_free_list_is_empty(const KainDeferredFreeList* list) {
    if (list == NULL) {
        return 1;
    }
    return list->active_first == list->sentinel && list->deferred_first == list->sentinel;
}
