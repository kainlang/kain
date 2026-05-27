#ifndef VIRTUAL_ALLOC_H
#define VIRTUAL_ALLOC_H

#include <stddef.h>
#include <stdint.h>
#include "arena.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void* base;
    size_t byte_count;
    KainMemType memtype;
    uint8_t writable;
    uint16_t reserved16;
} KainVirtualBatchMapping;

size_t kain_virtual_page_size(void);
size_t kain_virtual_align_up(size_t value, size_t alignment);
void* kain_virtual_reserve(size_t byte_count, size_t alignment, KainMemType memtype);
int kain_virtual_commit(void* base, size_t byte_count, KainMemType memtype);
void* kain_virtual_reserve_and_commit(size_t byte_count, size_t alignment, KainMemType memtype);
void kain_virtual_decommit(void* base, size_t byte_count);
void kain_virtual_release(void* base, size_t byte_count);
int kain_virtual_batch_map(KainVirtualBatchMapping* mappings, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* VIRTUAL_ALLOC_H */
