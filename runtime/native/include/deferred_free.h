#ifndef DEFERRED_FREE_H
#define DEFERRED_FREE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t capacity;
    uint32_t active_first;
    uint32_t deferred_first;
    uint32_t deferred_last;
    uint32_t sentinel;
    uint32_t used_marker;
    uint32_t* next;
} KainDeferredFreeList;

int kain_deferred_free_list_init(
    KainDeferredFreeList* list,
    uint32_t* next_storage,
    uint32_t capacity
);
void kain_deferred_free_list_make_all_free(KainDeferredFreeList* list);
int kain_deferred_free_list_allocate(KainDeferredFreeList* list);
void kain_deferred_free_list_deferred_free(KainDeferredFreeList* list, uint32_t index);
void kain_deferred_free_list_flush(KainDeferredFreeList* list);
int kain_deferred_free_list_is_empty(const KainDeferredFreeList* list);

#ifdef __cplusplus
}
#endif

#endif /* DEFERRED_FREE_H */
