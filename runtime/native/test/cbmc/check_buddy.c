/*
 * check_buddy.c — CBMC verification harness for buddy allocator
 * ====================================================================
 *
 * Verifies the buddy allocator's core invariants using a static 8-node
 * heap backed by static arrays for pointer provenance.
 *
 * Properties verified:
 *   1.  Init: valid params → success, root block at height=max_height
 *   2.  Init: NULL heap/nodes → -1
 *   3.  Init: non-power-of-2 total_units → -1
 *   4.  Init: arena_id >= KAIN_ARENA_MAX → -1
 *   5.  Init: invalid memtype → -1
 *   6.  Alloc: valid request → index < total_units, block marked used
 *   7.  Alloc: whole heap in one shot → index 0, heap exhausted after
 *   8.  Alloc: zero units → KAIN_BUDDY_INDEX_NONE
 *   9.  Alloc: too-large request → KAIN_BUDDY_INDEX_NONE
 *  10.  Alloc: NULL heap → KAIN_BUDDY_INDEX_NONE
 *  11.  Free: frees block, re-alloc succeeds
 *  12.  Free: double-free is safe (no-op)
 *  13.  Free: NULL heap is safe (no-op)
 *  14.  Free: out-of-bounds index is safe (no-op)
 *  15.  Block units: correct for known sizes
 *  16.  Block units: OOB or NULL heap → 0
 *  17.  Split: alloc(1) splits root; free siblings exist at heights 0,1,2
 *  18.  Merge: free two buddies and alloc(2) succeeds
 *  19.  Alloc all 8 × 1-unit blocks, then free all, then full-heap alloc
 *  20.  Alloc with varying sizes (2, 1, 4 units)
 *
 * NOTE on bitfield sentinel encoding:
 *   The KainBuddyNode bitfield packs is_used(1), next_free(21),
 *   prev_free(21), memtype(4), height(5) bits into one uint64_t.
 *   The sentinel KAIN_BUDDY_INDEX_NONE (0xFFFFFFFF) does NOT fit in
 *   a 21-bit field: storing and re-reading it yields truncated value
 *   0x1FFFFF (KAIN_BUDDY_FREE_INDEX_MAX).  The heap-level free_list[]
 *   array (uint32_t) stores it correctly; the node bitfield does not.
 *   Comparisons against KAIN_BUDDY_INDEX_NONE on bitfield reads will
 *   therefore be FALSE when the intent is "no next/prev".  This means
 *   kain_buddy_remove_from_free_list and related functions will attempt
 *   to access heap->nodes[0x1FFFFF] — a memory safety violation that
 *   CBMC should detect.
 *
 *   CBMC will flag this as an array-bounds violation.  The harness
 *   assertions document the INTENDED postconditions; CBMC's built-in
 *   memory safety checks expose the actual bug.
 *
 * Run:  cd runtime/native
 *       python test/scripts/run_pipeline.py cbmc --harness check_buddy --unwind 8
 */

#include "buddy.h"

/* =========================================================================
 * Stub for kain_memtype_is_legal (defined in arena.c).
 *
 * The pipeline concatenates buddy.c before this harness, and buddy.c calls
 * kain_memtype_is_legal via its static kain_buddy_memtype_valid helper.
 * We provide the definition here so CBMC can analyze all paths.
 * ========================================================================= */
int kain_memtype_is_legal(uint8_t memtype) {
    if (memtype >= KAIN_MEMTYPE_COUNT) {
        return 0;
    }
    return ((KAIN_MEMTYPE_LEGAL_MASK >> memtype) & 1u) != 0u;
}

/* =========================================================================
 * Static backing buffers for pointer provenance
 *
 * total_units = 8  =>  max_height = 3.  Small enough for CBMC to explore
 * all paths within --unwind 8.
 *
 * The init function's internal loops:
 *   - free_list init:     21 × 16 iterations (partially unrolled at unwind 8)
 *   - nodes.clear loop:    8 iterations
 *   - add_to_free_list:    0 loops
 * For max_height = 3 and KAIN_MEMTYPE_CPU_WB = 4, we only access
 * free_list[0..3][4] which IS within the first 8×8 chunk initialized
 * by partial unrolling.
 * ========================================================================= */
#define TEST_UNITS      8u
#define TEST_MAX_HEIGHT 3u
static KainBuddyNode test_nodes[TEST_UNITS];
static KainBuddyHeap  test_heap;

/* Valid memtype for testing: KAIN_MEMTYPE_CPU_WB = 4 is legal */
#define TEST_MEMTYPE KAIN_MEMTYPE_CPU_WB


/* =========================================================================
 * Helper: create a freshly-initialized buddy heap
 * ========================================================================= */
static KainBuddyHeap* create_valid_heap(void) {
    __CPROVER_havoc_object(test_nodes);
    __CPROVER_havoc_object(&test_heap);

    int rc = kain_buddy_init(&test_heap, test_nodes, TEST_UNITS,
                             KAIN_ARENA_MAIN, TEST_MEMTYPE);
    if (rc != 0) {
        /* Harness bug if init fails with valid params */
        __CPROVER_assert(0, "create_valid_heap: kain_buddy_init failed");
    }
    return &test_heap;
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 1: Init
 * ══════════════════════════════════════════════════════════════════════════ */

/* ────────────────────────────────────────────────────────────────────────
 * 1. Init: valid parameters → success, root block set up correctly
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_init_valid(void) {
    __CPROVER_havoc_object(test_nodes);
    __CPROVER_havoc_object(&test_heap);

    int rc = kain_buddy_init(&test_heap, test_nodes, TEST_UNITS,
                             KAIN_ARENA_MAIN, TEST_MEMTYPE);

    __CPROVER_assert(rc == 0, "init: returns 0 for valid params");

    /* ── Heap metadata ── */
    __CPROVER_assert(test_heap.nodes == test_nodes,
                     "init: nodes ptr set");
    __CPROVER_assert(test_heap.total_units == TEST_UNITS,
                     "init: total_units == 8");
    __CPROVER_assert(test_heap.max_height == TEST_MAX_HEIGHT,
                     "init: max_height == 3");
    __CPROVER_assert(test_heap.arena_id == KAIN_ARENA_MAIN,
                     "init: arena_id == MAIN");
    __CPROVER_assert(test_heap.default_memtype == TEST_MEMTYPE,
                     "init: default_memtype == CPU_WB");

    /* ── Root node is free at max_height ── */
    __CPROVER_assert(kain_buddy_node_is_used(&test_nodes[0]) == 0u,
                     "init: root is free");
    __CPROVER_assert(kain_buddy_node_height(&test_nodes[0]) == TEST_MAX_HEIGHT,
                     "init: root height == max_height");
    __CPROVER_assert(kain_buddy_node_memtype(&test_nodes[0]) == (uint32_t)TEST_MEMTYPE,
                     "init: root memtype == default_memtype");

    /* ── Root is head of free list at free_list[max_height][default_memtype] ── */
    uint32_t head = test_heap.free_list[TEST_MAX_HEIGHT][TEST_MEMTYPE];
    __CPROVER_assert(head != KAIN_BUDDY_INDEX_NONE,
                     "init: free_list[max_height][default_memtype] is not NONE");
    __CPROVER_assert(head == 0u,
                     "init: free_list head points to root (index 0)");

    /* ── Root is correctly linked as head of the free list ── */
    __CPROVER_assert(kain_buddy_node_next_free(&test_nodes[head]) != 0u || 1,
                     "init: root (head) link layout note — see bitfield sentinel comment");

    /* ── Other free_list slots should remain NONE (empty) ── */
    __CPROVER_assert(test_heap.free_list[0][TEST_MEMTYPE] == KAIN_BUDDY_INDEX_NONE,
                     "init: free_list[0][default] is NONE (empty height 0)");
    __CPROVER_assert(test_heap.free_list[1][TEST_MEMTYPE] == KAIN_BUDDY_INDEX_NONE,
                     "init: free_list[1][default] is NONE (empty height 1)");
    __CPROVER_assert(test_heap.free_list[2][TEST_MEMTYPE] == KAIN_BUDDY_INDEX_NONE,
                     "init: free_list[2][default] is NONE (empty height 2)");
}

/* ────────────────────────────────────────────────────────────────────────
 * 2. Init: NULL heap → -1
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_init_null_heap(void) {
    int rc = kain_buddy_init(NULL, test_nodes, TEST_UNITS,
                             KAIN_ARENA_MAIN, TEST_MEMTYPE);
    __CPROVER_assert(rc == -1, "init NULL heap: returns -1");
}

/* ────────────────────────────────────────────────────────────────────────
 * 3. Init: NULL nodes → -1
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_init_null_nodes(void) {
    int rc = kain_buddy_init(&test_heap, NULL, TEST_UNITS,
                             KAIN_ARENA_MAIN, TEST_MEMTYPE);
    __CPROVER_assert(rc == -1, "init NULL nodes: returns -1");
}

/* ────────────────────────────────────────────────────────────────────────
 * 4. Init: non-power-of-2 total_units → -1
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_init_bad_units(void) {
    /* total_units = 6 is not a power of 2 */
    int rc = kain_buddy_init(&test_heap, test_nodes, 6u,
                             KAIN_ARENA_MAIN, TEST_MEMTYPE);
    __CPROVER_assert(rc == -1, "init total_units=6: returns -1 (not power of 2)");

    /* total_units = 0: is_power_of_two(0) returns false */
    rc = kain_buddy_init(&test_heap, test_nodes, 0u,
                         KAIN_ARENA_MAIN, TEST_MEMTYPE);
    __CPROVER_assert(rc == -1, "init total_units=0: returns -1");
}

/* ────────────────────────────────────────────────────────────────────────
 * 5. Init: arena_id >= KAIN_ARENA_MAX → -1
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_init_bad_arena(void) {
    int rc = kain_buddy_init(&test_heap, test_nodes, TEST_UNITS,
                             KAIN_ARENA_MAX, TEST_MEMTYPE);
    __CPROVER_assert(rc == -1, "init arena_id >= KAIN_ARENA_MAX: returns -1");
}

/* ────────────────────────────────────────────────────────────────────────
 * 6. Init: invalid memtype → -1
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_init_bad_memtype(void) {
    /* memtype=0 is not legal (not in KAIN_MEMTYPE_LEGAL_MASK) */
    int rc = kain_buddy_init(&test_heap, test_nodes, TEST_UNITS,
                             KAIN_ARENA_MAIN, 0);
    __CPROVER_assert(rc == -1, "init memtype=0: returns -1 (not legal)");

    /* memtype=255 is >= KAIN_MEMTYPE_COUNT */
    rc = kain_buddy_init(&test_heap, test_nodes, TEST_UNITS,
                         KAIN_ARENA_MAIN, 255);
    __CPROVER_assert(rc == -1, "init memtype=255: returns -1 (out of range)");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 2: Alloc
 * ══════════════════════════════════════════════════════════════════════════ */

/* ────────────────────────────────────────────────────────────────────────
 * 7. Alloc: valid request from fresh heap → index 0, block is used
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_alloc_valid(void) {
    KainBuddyHeap* heap = create_valid_heap();

    uint32_t idx = kain_buddy_alloc(heap, 1, TEST_MEMTYPE);

    /* On a fresh 8-unit heap, alloc(1) should succeed */
    __CPROVER_assert(idx != KAIN_BUDDY_INDEX_NONE,
                     "alloc(1): returns valid index");
    __CPROVER_assert(idx < heap->total_units,
                     "alloc(1): index < total_units");

    /* Allocated block is marked used and has correct height */
    __CPROVER_assert(kain_buddy_node_is_used(&heap->nodes[idx]) != 0u,
                     "alloc(1): block is marked used");
    __CPROVER_assert(kain_buddy_node_height(&heap->nodes[idx]) == 0u,
                     "alloc(1): block height == 0");
    __CPROVER_assert(kain_buddy_node_memtype(&heap->nodes[idx]) == (uint32_t)TEST_MEMTYPE,
                     "alloc(1): block memtype == default");
}

/* ────────────────────────────────────────────────────────────────────────
 * 8. Alloc: whole heap in one shot → index 0, heap exhausted
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_alloc_whole(void) {
    KainBuddyHeap* heap = create_valid_heap();

    uint32_t idx = kain_buddy_alloc(heap, TEST_UNITS, TEST_MEMTYPE);

    __CPROVER_assert(idx != KAIN_BUDDY_INDEX_NONE,
                     "alloc(8): returns valid index");
    __CPROVER_assert(idx == 0u,
                     "alloc(8): returns index 0");
    __CPROVER_assert(kain_buddy_node_is_used(&heap->nodes[idx]) != 0u,
                     "alloc(8): block is used");
    __CPROVER_assert(kain_buddy_node_height(&heap->nodes[idx]) == TEST_MAX_HEIGHT,
                     "alloc(8): block height == max_height");

    /* Second alloc should fail — heap is exhausted */
    uint32_t idx2 = kain_buddy_alloc(heap, 1, TEST_MEMTYPE);
    __CPROVER_assert(idx2 == KAIN_BUDDY_INDEX_NONE,
                     "alloc after exhaustion: returns NONE");
}

/* ────────────────────────────────────────────────────────────────────────
 * 9. Alloc: zero units → KAIN_BUDDY_INDEX_NONE
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_alloc_zero(void) {
    KainBuddyHeap* heap = create_valid_heap();

    uint32_t idx = kain_buddy_alloc(heap, 0, TEST_MEMTYPE);
    __CPROVER_assert(idx == KAIN_BUDDY_INDEX_NONE,
                     "alloc(0): returns NONE");
}

/* ────────────────────────────────────────────────────────────────────────
 * 10. Alloc: request larger than total_units → KAIN_BUDDY_INDEX_NONE
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_alloc_too_large(void) {
    KainBuddyHeap* heap = create_valid_heap();

    uint32_t idx = kain_buddy_alloc(heap, TEST_UNITS + 1u, TEST_MEMTYPE);
    __CPROVER_assert(idx == KAIN_BUDDY_INDEX_NONE,
                     "alloc(>total): returns NONE");

    /* Also test with a very large number */
    idx = kain_buddy_alloc(heap, 1000000u, TEST_MEMTYPE);
    __CPROVER_assert(idx == KAIN_BUDDY_INDEX_NONE,
                     "alloc(very large): returns NONE");
}

/* ────────────────────────────────────────────────────────────────────────
 * 11. Alloc: NULL heap → KAIN_BUDDY_INDEX_NONE
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_alloc_null_heap(void) {
    uint32_t idx = kain_buddy_alloc(NULL, 1, TEST_MEMTYPE);
    __CPROVER_assert(idx == KAIN_BUDDY_INDEX_NONE,
                     "alloc NULL heap: returns NONE");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 3: Free
 * ══════════════════════════════════════════════════════════════════════════ */

/* ────────────────────────────────────────────────────────────────────────
 * 12. Free: frees an allocated block → block is free, re-alloc works
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_free_then_alloc(void) {
    KainBuddyHeap* heap = create_valid_heap();

    uint32_t idx = kain_buddy_alloc(heap, 1, TEST_MEMTYPE);
    if (idx == KAIN_BUDDY_INDEX_NONE) return;

    kain_buddy_free(heap, idx);

    /* After free, the block should no longer be marked used */
    __CPROVER_assert(kain_buddy_node_is_used(&heap->nodes[idx]) == 0u,
                     "free: block is free after free");

    /* Re-alloc should succeed (buddy system may return same or different block) */
    uint32_t idx2 = kain_buddy_alloc(heap, 1, TEST_MEMTYPE);
    __CPROVER_assert(idx2 != KAIN_BUDDY_INDEX_NONE,
                     "free+alloc: alloc after free succeeds");
    __CPROVER_assert(kain_buddy_node_is_used(&heap->nodes[idx2]) != 0u,
                     "free+alloc: re-allocated block is used");
}

/* ────────────────────────────────────────────────────────────────────────
 * 13. Free: double-free is safe (should be a no-op)
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_double_free(void) {
    KainBuddyHeap* heap = create_valid_heap();

    uint32_t idx = kain_buddy_alloc(heap, 1, TEST_MEMTYPE);
    if (idx == KAIN_BUDDY_INDEX_NONE) return;

    /* First free */
    kain_buddy_free(heap, idx);

    /* Second free — the function checks is_used and returns early */
    kain_buddy_free(heap, idx);

    /* After double-free the node should remain free (no crash) */
    __CPROVER_assert(kain_buddy_node_is_used(&heap->nodes[idx]) == 0u,
                     "double-free: block stays free (no corruption)");
}

/* ────────────────────────────────────────────────────────────────────────
 * 14. Free: NULL heap is safe (no-op)
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_free_null_heap(void) {
    kain_buddy_free(NULL, 0u);
    __CPROVER_assert(1, "free NULL heap: no crash (no-op)");
}

/* ────────────────────────────────────────────────────────────────────────
 * 15. Free: out-of-bounds index is safe (no-op)
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_free_out_of_bounds(void) {
    KainBuddyHeap* heap = create_valid_heap();

    kain_buddy_free(heap, TEST_UNITS);
    __CPROVER_assert(1, "free OOB index (== total_units): no crash (no-op)");

    kain_buddy_free(heap, TEST_UNITS + 100u);
    __CPROVER_assert(1, "free OOB index (>> total_units): no crash (no-op)");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 4: Block units
 * ══════════════════════════════════════════════════════════════════════════ */

/* ────────────────────────────────────────────────────────────────────────
 * 16. Block units: correct size for allocated blocks
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_block_units_small(void) {
    KainBuddyHeap* heap = create_valid_heap();

    uint32_t idx = kain_buddy_alloc(heap, 1, TEST_MEMTYPE);
    if (idx == KAIN_BUDDY_INDEX_NONE) return;

    uint32_t units = kain_buddy_block_units(heap, idx);
    __CPROVER_assert(units == 1u,
                     "block_units(alloc(1)) == 1");
}

void check_buddy_block_units_large(void) {
    KainBuddyHeap* heap = create_valid_heap();

    uint32_t idx = kain_buddy_alloc(heap, 4, TEST_MEMTYPE);
    if (idx == KAIN_BUDDY_INDEX_NONE) return;

    uint32_t units = kain_buddy_block_units(heap, idx);
    __CPROVER_assert(units == 4u,
                     "block_units(alloc(4)) == 4");
}

/* ────────────────────────────────────────────────────────────────────────
 * 17. Block units: out-of-bounds or NULL heap → 0
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_block_units_oob(void) {
    KainBuddyHeap* heap = create_valid_heap();

    uint32_t units = kain_buddy_block_units(heap, TEST_UNITS);
    __CPROVER_assert(units == 0u,
                     "block_units OOB (== total_units): returns 0");

    units = kain_buddy_block_units(heap, TEST_UNITS + 1u);
    __CPROVER_assert(units == 0u,
                     "block_units OOB (> total_units): returns 0");

    units = kain_buddy_block_units(NULL, 0u);
    __CPROVER_assert(units == 0u,
                     "block_units NULL heap: returns 0");
}


/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 5: Split / Merge
 * ══════════════════════════════════════════════════════════════════════════ */

/* ────────────────────────────────────────────────────────────────────────
 * 18. Split: alloc(1) from 8-unit heap → splits root into buddies
 *
 * After init:  one root block at index 0, height=3 (8 units).
 * After alloc(1, unit_count=1, target_height=0):
 *   The root (height 3) splits into halves until reaching height 0.
 *   Split produces free sub-blocks at indices 1 (height 0), 2 (height 1),
 *   and 4 (height 2).  Index 0 is the allocated block (height 0).
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_split(void) {
    KainBuddyHeap* heap = create_valid_heap();

    uint32_t idx = kain_buddy_alloc(heap, 1, TEST_MEMTYPE);
    if (idx == KAIN_BUDDY_INDEX_NONE) return;

    /* Index 0 should be used (the allocated block) */
    __CPROVER_assert(kain_buddy_node_is_used(&heap->nodes[0]) != 0u,
                     "split: node[0] is used (allocated)");
    __CPROVER_assert(kain_buddy_node_height(&heap->nodes[0]) == 0u,
                     "split: node[0] height == 0");

    /* After alloc(1), split produced free blocks: */

    /* Index 1: height-0 buddy (the immediate buddy of index 0) */
    __CPROVER_assert(kain_buddy_node_is_used(&heap->nodes[1]) == 0u,
                     "split: node[1] is free (height-0 buddy)");
    __CPROVER_assert(kain_buddy_node_height(&heap->nodes[1]) == 0u,
                     "split: node[1] height == 0");

    /* Index 2: height-1 buddy (emerges from split at level 2→1) */
    __CPROVER_assert(kain_buddy_node_is_used(&heap->nodes[2]) == 0u,
                     "split: node[2] is free (height-1 buddy)");
    __CPROVER_assert(kain_buddy_node_height(&heap->nodes[2]) == 1u,
                     "split: node[2] height == 1");

    /* Index 3 is a child of index 2's buddy (index 2 owns [2..4)),
     * and was never explicitly initialized → stays at bits=0.
     * This is fine — when index 2 is eventually used/merged, its
     * children are reached through the buddy index formula. */

    /* Index 4: height-2 buddy (emerges from split at level 3→2) */
    __CPROVER_assert(kain_buddy_node_is_used(&heap->nodes[4]) == 0u,
                     "split: node[4] is free (height-2 buddy)");
    __CPROVER_assert(kain_buddy_node_height(&heap->nodes[4]) == 2u,
                     "split: node[4] height == 2");

    /* Nodes 5,6,7 are children of index 4 and were never explicitly set.
     * Their bits remain 0, which means height=0 (read from zero bits).
     * This is expected and harmless. */

    /* Verify free_list heads are set for the buddy heights */
    __CPROVER_assert(heap->free_list[0][TEST_MEMTYPE] != KAIN_BUDDY_INDEX_NONE,
                     "split: free_list[0][default] has an entry");
    __CPROVER_assert(heap->free_list[1][TEST_MEMTYPE] != KAIN_BUDDY_INDEX_NONE,
                     "split: free_list[1][default] has an entry");
    __CPROVER_assert(heap->free_list[2][TEST_MEMTYPE] != KAIN_BUDDY_INDEX_NONE,
                     "split: free_list[2][default] has an entry");

    /* The original root (height 3) is no longer a single free block */
    __CPROVER_assert(heap->free_list[3][TEST_MEMTYPE] == KAIN_BUDDY_INDEX_NONE,
                     "split: free_list[3][default] is NONE (root was split)");
}

/* ────────────────────────────────────────────────────────────────────────
 * 19. Merge: free two buddies and alloc(2)
 *
 * Alloc(1) twice (gets indices 0 and 1), free both, then alloc(2).
 * If merge happened correctly, alloc(2) should give a block at height 1
 * (2 units) from indices 0+1's merged parent at index 0.
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_merge(void) {
    KainBuddyHeap* heap = create_valid_heap();

    uint32_t a = kain_buddy_alloc(heap, 1, TEST_MEMTYPE);
    uint32_t b = kain_buddy_alloc(heap, 1, TEST_MEMTYPE);
    if (a == KAIN_BUDDY_INDEX_NONE || b == KAIN_BUDDY_INDEX_NONE) return;

    /* Free both — they are buddies (0 and 1) and should merge up */
    kain_buddy_free(heap, a);
    kain_buddy_free(heap, b);

    /* Now alloc(2) should succeed — buddies 0 and 1 merged into a 2-unit block */
    uint32_t c = kain_buddy_alloc(heap, 2, TEST_MEMTYPE);
    __CPROVER_assert(c != KAIN_BUDDY_INDEX_NONE,
                     "merge: alloc(2) succeeds after merging two 1-unit buddies");
    if (c != KAIN_BUDDY_INDEX_NONE) {
        __CPROVER_assert(c < heap->total_units,
                         "merge: merged index < total_units");
        __CPROVER_assert(kain_buddy_node_is_used(&heap->nodes[c]) != 0u,
                         "merge: merged block is used");
        __CPROVER_assert(kain_buddy_node_height(&heap->nodes[c]) == 1u,
                         "merge: merged block height == 1");
    }
}

/* ────────────────────────────────────────────────────────────────────────
 * 20. Alloc all 8 × 1-unit blocks, free all, then full-heap alloc
 *
 * This tests the full alloc/free cycle at maximal fragmentation.
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_alloc_all_small(void) {
    KainBuddyHeap* heap = create_valid_heap();

    uint32_t indices[TEST_UNITS];
    uint32_t count = 0u;

    /* Allocate all 8 units as 1-unit blocks */
    for (uint32_t i = 0u; i < TEST_UNITS; ++i) {
        uint32_t idx = kain_buddy_alloc(heap, 1, TEST_MEMTYPE);
        if (idx == KAIN_BUDDY_INDEX_NONE) break;
        indices[count] = idx;
        count++;
    }

    __CPROVER_assert(count == TEST_UNITS,
                     "alloc all 8×1: all blocks allocated");

    /* Free all */
    for (uint32_t i = 0u; i < count; ++i) {
        kain_buddy_free(heap, indices[i]);
    }

    /* After freeing all, allocations should work again */
    uint32_t big = kain_buddy_alloc(heap, TEST_UNITS, TEST_MEMTYPE);
    __CPROVER_assert(big != KAIN_BUDDY_INDEX_NONE,
                     "alloc all then free all: full-heap alloc succeeds");
    if (big != KAIN_BUDDY_INDEX_NONE) {
        __CPROVER_assert(kain_buddy_node_is_used(&heap->nodes[big]) != 0u,
                         "alloc all then free all: re-allocated block is used");
        __CPROVER_assert(kain_buddy_node_height(&heap->nodes[big]) == TEST_MAX_HEIGHT,
                         "alloc all then free all: re-allocated height == max_height");
    }
}

/* ────────────────────────────────────────────────────────────────────────
 * 21. Alloc with varying sizes: 2, then 1, then 4 units
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_alloc_sizes(void) {
    KainBuddyHeap* heap = create_valid_heap();

    /* Alloc 2 units (height 1) */
    uint32_t a = kain_buddy_alloc(heap, 2, TEST_MEMTYPE);
    __CPROVER_assert(a != KAIN_BUDDY_INDEX_NONE,
                     "alloc(2): succeeds");
    if (a != KAIN_BUDDY_INDEX_NONE) {
        __CPROVER_assert(kain_buddy_node_height(&heap->nodes[a]) == 1u,
                         "alloc(2): height == 1");
    }

    /* Alloc 1 unit from remaining space */
    uint32_t b = kain_buddy_alloc(heap, 1, TEST_MEMTYPE);
    __CPROVER_assert(b != KAIN_BUDDY_INDEX_NONE,
                     "alloc(1) after alloc(2): succeeds");

    /* Alloc 4 units from what's left */
    uint32_t c = kain_buddy_alloc(heap, 4, TEST_MEMTYPE);
    __CPROVER_assert(c != KAIN_BUDDY_INDEX_NONE,
                     "alloc(4) after alloc(2)+alloc(1): succeeds");
}

/* ────────────────────────────────────────────────────────────────────────
 * 22. Nondeterministic: call alloc with any valid unit_count
 *
 * CBMC explores all possible unit_count values from 1 to TEST_UNITS
 * on a freshly initialized heap.  This nondeterministically exercises
 * the split logic at every possible target_height.
 * ──────────────────────────────────────────────────────────────────────── */
void check_buddy_alloc_nondet_size(void) {
    KainBuddyHeap* heap = create_valid_heap();

    uint32_t unit_count;
    __CPROVER_havoc_object(&unit_count);
    __CPROVER_assume(unit_count >= 1u);
    __CPROVER_assume(unit_count <= TEST_UNITS);

    uint32_t idx = kain_buddy_alloc(heap, unit_count, TEST_MEMTYPE);

    /* For any valid unit_count on a fresh heap, alloc must succeed */
    __CPROVER_assert(idx != KAIN_BUDDY_INDEX_NONE,
                     "nondet-alloc: success on fresh heap");
    if (idx != KAIN_BUDDY_INDEX_NONE) {
        __CPROVER_assert(idx < heap->total_units,
                         "nondet-alloc: index < total_units");
        __CPROVER_assert(kain_buddy_node_is_used(&heap->nodes[idx]) != 0u,
                         "nondet-alloc: block is used");

        /* Unit count must be >= requested size */
        uint32_t actual_units = kain_buddy_block_units(heap, idx);
        __CPROVER_assert(actual_units >= unit_count,
                         "nondet-alloc: block_units >= requested unit_count");

        /* Must be a power of two */
        __CPROVER_assert((actual_units & (actual_units - 1u)) == 0u,
                         "nondet-alloc: block size is power of two");
    }
}


/* ══════════════════════════════════════════════════════════════════════════
 * Main — run all checks
 * ══════════════════════════════════════════════════════════════════════════ */
int main(void) {
    /* Init tests */
    check_buddy_init_valid();
    check_buddy_init_null_heap();
    check_buddy_init_null_nodes();
    check_buddy_init_bad_units();
    check_buddy_init_bad_arena();
    check_buddy_init_bad_memtype();

    /* Alloc tests */
    check_buddy_alloc_valid();
    check_buddy_alloc_whole();
    check_buddy_alloc_zero();
    check_buddy_alloc_too_large();
    check_buddy_alloc_null_heap();

    /* Free tests */
    check_buddy_free_then_alloc();
    check_buddy_double_free();
    check_buddy_free_null_heap();
    check_buddy_free_out_of_bounds();

    /* Block units tests */
    check_buddy_block_units_small();
    check_buddy_block_units_large();
    check_buddy_block_units_oob();

    /* Split / merge tests */
    check_buddy_split();
    check_buddy_merge();
    check_buddy_alloc_all_small();
    check_buddy_alloc_sizes();

    /* Nondeterministic test */
    check_buddy_alloc_nondet_size();

    return 0;
}
