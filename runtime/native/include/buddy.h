#ifndef BUDDY_H
#define BUDDY_H

#include <stdint.h>
#include "arena.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    KAIN_BUDDY_MAX_HEIGHT = 20,
    KAIN_BUDDY_FREE_INDEX_BITS = 21,
    KAIN_BUDDY_FREE_INDEX_MAX = (1u << KAIN_BUDDY_FREE_INDEX_BITS) - 1u,
    KAIN_BUDDY_INDEX_NONE = UINT32_C(0xffffffff)
};

typedef struct {
    uint64_t bits;
} KainBuddyNode;

typedef struct {
    KainBuddyNode* nodes;
    uint32_t total_units;
    uint8_t max_height;
    uint8_t arena_id;
    uint8_t default_memtype;
    uint8_t reserved8;
    uint32_t free_list[KAIN_BUDDY_MAX_HEIGHT + 1][KAIN_MEMTYPE_COUNT];
} KainBuddyHeap;

enum {
    KAIN_BUDDY_IS_USED_SHIFT = 0,
    KAIN_BUDDY_NEXT_FREE_SHIFT = 1,
    KAIN_BUDDY_PREV_FREE_SHIFT = 22,
    KAIN_BUDDY_MEMTYPE_SHIFT = 43,
    KAIN_BUDDY_HEIGHT_SHIFT = 47
};

enum {
    KAIN_BUDDY_IS_USED_MASK = UINT64_C(0x0000000000000001),
    KAIN_BUDDY_NEXT_FREE_MASK = UINT64_C(0x00000000003ffffe),
    KAIN_BUDDY_PREV_FREE_MASK = UINT64_C(0x000007ffffc00000),
    KAIN_BUDDY_MEMTYPE_MASK = UINT64_C(0x0000780000000000),
    KAIN_BUDDY_HEIGHT_MASK = UINT64_C(0x001f800000000000)
};

static inline uint32_t kain_buddy_node_is_used(const KainBuddyNode* node) {
    return node == NULL ? 0u : (uint32_t)((node->bits & KAIN_BUDDY_IS_USED_MASK) >> KAIN_BUDDY_IS_USED_SHIFT);
}

static inline uint32_t kain_buddy_node_next_free(const KainBuddyNode* node) {
    return node == NULL ? KAIN_BUDDY_INDEX_NONE :
        (uint32_t)((node->bits & KAIN_BUDDY_NEXT_FREE_MASK) >> KAIN_BUDDY_NEXT_FREE_SHIFT);
}

static inline uint32_t kain_buddy_node_prev_free(const KainBuddyNode* node) {
    return node == NULL ? KAIN_BUDDY_INDEX_NONE :
        (uint32_t)((node->bits & KAIN_BUDDY_PREV_FREE_MASK) >> KAIN_BUDDY_PREV_FREE_SHIFT);
}

static inline uint32_t kain_buddy_node_memtype(const KainBuddyNode* node) {
    return node == NULL ? 0u :
        (uint32_t)((node->bits & KAIN_BUDDY_MEMTYPE_MASK) >> KAIN_BUDDY_MEMTYPE_SHIFT);
}

static inline uint32_t kain_buddy_node_height(const KainBuddyNode* node) {
    return node == NULL ? 0u :
        (uint32_t)((node->bits & KAIN_BUDDY_HEIGHT_MASK) >> KAIN_BUDDY_HEIGHT_SHIFT);
}

static inline void kain_buddy_node_set_is_used(KainBuddyNode* node, uint32_t is_used) {
    node->bits = (node->bits & ~KAIN_BUDDY_IS_USED_MASK) |
        (((uint64_t)is_used << KAIN_BUDDY_IS_USED_SHIFT) & KAIN_BUDDY_IS_USED_MASK);
}

static inline void kain_buddy_node_set_next_free(KainBuddyNode* node, uint32_t next_free) {
    node->bits = (node->bits & ~KAIN_BUDDY_NEXT_FREE_MASK) |
        (((uint64_t)next_free << KAIN_BUDDY_NEXT_FREE_SHIFT) & KAIN_BUDDY_NEXT_FREE_MASK);
}

static inline void kain_buddy_node_set_prev_free(KainBuddyNode* node, uint32_t prev_free) {
    node->bits = (node->bits & ~KAIN_BUDDY_PREV_FREE_MASK) |
        (((uint64_t)prev_free << KAIN_BUDDY_PREV_FREE_SHIFT) & KAIN_BUDDY_PREV_FREE_MASK);
}

static inline void kain_buddy_node_set_memtype(KainBuddyNode* node, uint32_t memtype) {
    node->bits = (node->bits & ~KAIN_BUDDY_MEMTYPE_MASK) |
        (((uint64_t)memtype << KAIN_BUDDY_MEMTYPE_SHIFT) & KAIN_BUDDY_MEMTYPE_MASK);
}

static inline void kain_buddy_node_set_height(KainBuddyNode* node, uint32_t height) {
    node->bits = (node->bits & ~KAIN_BUDDY_HEIGHT_MASK) |
        (((uint64_t)height << KAIN_BUDDY_HEIGHT_SHIFT) & KAIN_BUDDY_HEIGHT_MASK);
}

int kain_buddy_init(
    KainBuddyHeap* heap,
    KainBuddyNode* nodes,
    uint32_t total_units,
    KainArenaId arena_id,
    KainMemType default_memtype
);
uint32_t kain_buddy_alloc(KainBuddyHeap* heap, uint32_t unit_count, KainMemType preferred_memtype);
void kain_buddy_free(KainBuddyHeap* heap, uint32_t node_index);
uint32_t kain_buddy_block_units(const KainBuddyHeap* heap, uint32_t node_index);

#ifdef __cplusplus
}
#endif

#endif /* BUDDY_H */
