#include "../../include/buddy.h"

_Static_assert((KAIN_BUDDY_IS_USED_MASK & KAIN_BUDDY_NEXT_FREE_MASK) == 0u, "buddy used/next overlap");
_Static_assert((KAIN_BUDDY_IS_USED_MASK & KAIN_BUDDY_PREV_FREE_MASK) == 0u, "buddy used/prev overlap");
_Static_assert((KAIN_BUDDY_IS_USED_MASK & KAIN_BUDDY_MEMTYPE_MASK) == 0u, "buddy used/memtype overlap");
_Static_assert((KAIN_BUDDY_IS_USED_MASK & KAIN_BUDDY_HEIGHT_MASK) == 0u, "buddy used/height overlap");
_Static_assert((KAIN_BUDDY_NEXT_FREE_MASK & KAIN_BUDDY_PREV_FREE_MASK) == 0u, "buddy next/prev overlap");
_Static_assert((KAIN_BUDDY_NEXT_FREE_MASK & KAIN_BUDDY_MEMTYPE_MASK) == 0u, "buddy next/memtype overlap");
_Static_assert((KAIN_BUDDY_NEXT_FREE_MASK & KAIN_BUDDY_HEIGHT_MASK) == 0u, "buddy next/height overlap");
_Static_assert((KAIN_BUDDY_PREV_FREE_MASK & KAIN_BUDDY_MEMTYPE_MASK) == 0u, "buddy prev/memtype overlap");
_Static_assert((KAIN_BUDDY_PREV_FREE_MASK & KAIN_BUDDY_HEIGHT_MASK) == 0u, "buddy prev/height overlap");
_Static_assert((KAIN_BUDDY_MEMTYPE_MASK & KAIN_BUDDY_HEIGHT_MASK) == 0u, "buddy memtype/height overlap");

static int kain_buddy_is_power_of_two(uint32_t value) {
    return value != 0u && (value & (value - 1u)) == 0u;
}

static uint8_t kain_buddy_log2_exact(uint32_t value) {
    uint8_t height = 0u;
    while (value > 1u) {
        value >>= 1u;
        height += 1u;
    }
    return height;
}

static uint32_t kain_buddy_units_for_height(uint32_t height) {
    return 1u << height;
}

static uint32_t kain_buddy_required_height(uint32_t unit_count) {
    uint32_t rounded_units = 1u;
    uint32_t height = 0u;
    while (rounded_units < unit_count) {
        rounded_units <<= 1u;
        height += 1u;
    }
    return height;
}

static void kain_buddy_node_clear(KainBuddyNode* node) {
    if (node != NULL) {
        node->bits = 0u;
    }
}

static void kain_buddy_node_set_block(
    KainBuddyNode* node,
    uint32_t is_used,
    uint32_t height,
    uint32_t memtype
) {
    node->bits = 0u;
    kain_buddy_node_set_is_used(node, is_used);
    kain_buddy_node_set_next_free(node, KAIN_BUDDY_INDEX_NONE);
    kain_buddy_node_set_prev_free(node, KAIN_BUDDY_INDEX_NONE);
    kain_buddy_node_set_memtype(node, memtype);
    kain_buddy_node_set_height(node, height);
}

static int kain_buddy_memtype_valid(uint32_t memtype) {
    return memtype < KAIN_MEMTYPE_COUNT && kain_memtype_is_legal((uint8_t)memtype);
}

static void kain_buddy_add_to_free_list(KainBuddyHeap* heap, uint32_t index) {
    KainBuddyNode* node = &heap->nodes[index];
    uint32_t height = kain_buddy_node_height(node);
    uint32_t memtype = kain_buddy_node_memtype(node);
    uint32_t head = heap->free_list[height][memtype];
    kain_buddy_node_set_prev_free(node, KAIN_BUDDY_INDEX_NONE);
    kain_buddy_node_set_next_free(node, head);
    if (head != KAIN_BUDDY_INDEX_NONE) {
        kain_buddy_node_set_prev_free(&heap->nodes[head], index);
    }
    heap->free_list[height][memtype] = index;
}

static void kain_buddy_remove_from_free_list(KainBuddyHeap* heap, uint32_t index) {
    KainBuddyNode* node = &heap->nodes[index];
    uint32_t height = kain_buddy_node_height(node);
    uint32_t memtype = kain_buddy_node_memtype(node);
    uint32_t prev = kain_buddy_node_prev_free(node);
    uint32_t next = kain_buddy_node_next_free(node);

    if (prev == KAIN_BUDDY_INDEX_NONE) {
        heap->free_list[height][memtype] = next;
    } else {
        kain_buddy_node_set_next_free(&heap->nodes[prev], next);
    }
    if (next != KAIN_BUDDY_INDEX_NONE) {
        kain_buddy_node_set_prev_free(&heap->nodes[next], prev);
    }

    kain_buddy_node_set_prev_free(node, KAIN_BUDDY_INDEX_NONE);
    kain_buddy_node_set_next_free(node, KAIN_BUDDY_INDEX_NONE);
}

static int kain_buddy_find_source(
    KainBuddyHeap* heap,
    uint32_t target_height,
    uint32_t preferred_memtype,
    uint32_t* out_index,
    uint32_t* out_height,
    uint32_t* out_memtype
) {
    uint32_t normalized_memtype =
        kain_buddy_memtype_valid(preferred_memtype) ? preferred_memtype : heap->default_memtype;

    for (uint32_t height = target_height; height <= heap->max_height; ++height) {
        uint32_t head = heap->free_list[height][normalized_memtype];
        if (head != KAIN_BUDDY_INDEX_NONE) {
            *out_index = head;
            *out_height = height;
            *out_memtype = normalized_memtype;
            return 0;
        }
    }

    for (uint32_t height = target_height; height <= heap->max_height; ++height) {
        for (uint32_t memtype = 0u; memtype < KAIN_MEMTYPE_COUNT; ++memtype) {
            if (!kain_buddy_memtype_valid(memtype) || memtype == normalized_memtype) {
                continue;
            }
            uint32_t head = heap->free_list[height][memtype];
            if (head != KAIN_BUDDY_INDEX_NONE) {
                *out_index = head;
                *out_height = height;
                *out_memtype = memtype;
                return 0;
            }
        }
    }

    return -1;
}

int kain_buddy_init(
    KainBuddyHeap* heap,
    KainBuddyNode* nodes,
    uint32_t total_units,
    KainArenaId arena_id,
    KainMemType default_memtype
) {
    if (heap == NULL || nodes == NULL || !kain_buddy_is_power_of_two(total_units) ||
        total_units > KAIN_BUDDY_FREE_INDEX_MAX || arena_id >= KAIN_ARENA_MAX ||
        !kain_buddy_memtype_valid((uint32_t)default_memtype)) {
        return -1;
    }

    uint8_t max_height = kain_buddy_log2_exact(total_units);
    if (max_height > KAIN_BUDDY_MAX_HEIGHT) {
        return -1;
    }

    heap->nodes = nodes;
    heap->total_units = total_units;
    heap->max_height = max_height;
    heap->arena_id = (uint8_t)arena_id;
    heap->default_memtype = (uint8_t)default_memtype;
    heap->reserved8 = 0u;

    for (uint32_t height = 0u; height <= KAIN_BUDDY_MAX_HEIGHT; ++height) {
        for (uint32_t memtype = 0u; memtype < KAIN_MEMTYPE_COUNT; ++memtype) {
            heap->free_list[height][memtype] = KAIN_BUDDY_INDEX_NONE;
        }
    }
    for (uint32_t index = 0u; index < total_units; ++index) {
        heap->nodes[index].bits = 0u;
    }

    kain_buddy_node_set_block(&heap->nodes[0], 0u, max_height, (uint32_t)default_memtype);
    kain_buddy_add_to_free_list(heap, 0u);
    return 0;
}

uint32_t kain_buddy_alloc(KainBuddyHeap* heap, uint32_t unit_count, KainMemType preferred_memtype) {
    if (heap == NULL || heap->nodes == NULL || unit_count == 0u || unit_count > heap->total_units) {
        return KAIN_BUDDY_INDEX_NONE;
    }

    uint32_t target_height = kain_buddy_required_height(unit_count);
    if (target_height > heap->max_height) {
        return KAIN_BUDDY_INDEX_NONE;
    }

    uint32_t source_index = KAIN_BUDDY_INDEX_NONE;
    uint32_t source_height = 0u;
    uint32_t source_memtype = 0u;
    if (kain_buddy_find_source(
            heap,
            target_height,
            (uint32_t)preferred_memtype,
            &source_index,
            &source_height,
            &source_memtype) != 0) {
        return KAIN_BUDDY_INDEX_NONE;
    }

    kain_buddy_remove_from_free_list(heap, source_index);
    uint32_t current_index = source_index;
    uint32_t current_height = source_height;

    while (current_height > target_height) {
        current_height -= 1u;
        uint32_t buddy_index = current_index + kain_buddy_units_for_height(current_height);
        kain_buddy_node_set_block(&heap->nodes[current_index], 0u, current_height, source_memtype);
        kain_buddy_node_set_block(&heap->nodes[buddy_index], 0u, current_height, source_memtype);
        kain_buddy_add_to_free_list(heap, buddy_index);
    }

    kain_buddy_node_set_block(&heap->nodes[current_index], 1u, target_height, source_memtype);
    return current_index;
}

void kain_buddy_free(KainBuddyHeap* heap, uint32_t node_index) {
    if (heap == NULL || heap->nodes == NULL || node_index >= heap->total_units) {
        return;
    }

    KainBuddyNode* node = &heap->nodes[node_index];
    if (kain_buddy_node_is_used(node) == 0u) {
        return;
    }

    uint32_t current_index = node_index;
    uint32_t current_height = kain_buddy_node_height(node);
    uint32_t memtype = kain_buddy_node_memtype(node);
    kain_buddy_node_set_block(&heap->nodes[current_index], 0u, current_height, memtype);

    while (current_height < heap->max_height) {
        uint32_t buddy_index = current_index ^ kain_buddy_units_for_height(current_height);
        if (buddy_index >= heap->total_units) {
            break;
        }

        KainBuddyNode* buddy = &heap->nodes[buddy_index];
        if (kain_buddy_node_is_used(buddy) != 0u ||
            kain_buddy_node_height(buddy) != current_height ||
            kain_buddy_node_memtype(buddy) != memtype) {
            break;
        }

        kain_buddy_remove_from_free_list(heap, buddy_index);
        kain_buddy_node_clear(buddy);
        kain_buddy_node_clear(&heap->nodes[current_index]);
        current_index = current_index < buddy_index ? current_index : buddy_index;
        current_height += 1u;
        kain_buddy_node_set_block(&heap->nodes[current_index], 0u, current_height, memtype);
    }

    kain_buddy_add_to_free_list(heap, current_index);
}

uint32_t kain_buddy_block_units(const KainBuddyHeap* heap, uint32_t node_index) {
    if (heap == NULL || heap->nodes == NULL || node_index >= heap->total_units) {
        return 0u;
    }
    return kain_buddy_units_for_height(kain_buddy_node_height(&heap->nodes[node_index]));
}
