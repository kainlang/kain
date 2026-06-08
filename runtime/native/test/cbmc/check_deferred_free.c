/*
 * check_deferred_free.c — CBMC verification harness for deferred_free module
 *
 * Follows the pattern of check_arena.c:
 *   - Static backing buffers for pointer provenance
 *   - __CPROVER_havoc_object + __CPROVER_assume for nondet input
 *   - __CPROVER_assert for postconditions
 *   - Tests init, allocate, deferred_free, flush, is_empty, NULL safety,
 *     full queue, empty queue, and multiple-item deferred chains.
 *
 * deferred_free.c defines a deferred deallocation queue that batches
 * free() calls for cache-friendly release.  The queue is a pair of
 * singly-linked lists threaded through a uint32_t 'next' array:
 *
 *   active chain  — slots available for immediate allocation
 *   deferred chain — slots freed but held back until flush()
 *
 * Sentinel == capacity; used_marker == capacity + 1.
 *
 * Run via:
 *   cd X:/runtime/native && python test/scripts/run_pipeline.py cbmc --harness check_deferred_free --unwind 8
 *
 * Direct CBMC invocation:
 *   cbmc --unwind 8 --no-unwinding-assertions --trace test/cbmc/check_deferred_free.c src/core/deferred_free.c -I include -I src/core
 */

#include "deferred_free.h"

/* ──────────────────────────────────────────────────────────────────────
 * Bounded verification constants
 *
 * MAX_CAPACITY must be <= the --unwind bound.  With --unwind 8 and
 * capacity up to 6, all loops unroll fully without unwinding assertions.
 * ────────────────────────────────────────────────────────────────────── */
#define MAX_CAPACITY 6

/* Static backing buffer for the 'next' pointer array.
 * CBMC must know this is a real object for pointer provenance. */
static uint32_t next_storage[MAX_CAPACITY];

/* ══════════════════════════════════════════════════════════════════════
 * Helper: create a valid initialized KainDeferredFreeList
 *
 * The list struct and next_storage are havoc'd (nondet contents), but
 * list->next always points to the static buffer.  This gives CBMC real
 * pointer provenance while keeping input data random.
 * ══════════════════════════════════════════════════════════════════════ */
static KainDeferredFreeList* create_initialized_list(void) {
    static KainDeferredFreeList list;
    uint32_t capacity;
    __CPROVER_havoc_object(&list);
    __CPROVER_havoc_object(next_storage);
    __CPROVER_havoc_object(&capacity);
    __CPROVER_assume(capacity <= MAX_CAPACITY);

    int rc = kain_deferred_free_list_init(&list, next_storage, capacity);
    __CPROVER_assume(rc == 0);

    return &list;
}

/* Shorthand for the list's capacity field after a helper call */
#define LIST_CAP list->capacity

/* ══════════════════════════════════════════════════════════════════════
 * Check 1 — init creates a consistent list
 *
 * Verifies all struct fields, sentinel/used_marker invariants, and
 * the free-chain linked-list structure.
 * ══════════════════════════════════════════════════════════════════════ */
void check_init(void) {
    static KainDeferredFreeList list;
    uint32_t capacity;
    __CPROVER_havoc_object(&list);
    __CPROVER_havoc_object(next_storage);
    __CPROVER_havoc_object(&capacity);
    __CPROVER_assume(capacity <= MAX_CAPACITY);

    int rc = kain_deferred_free_list_init(&list, next_storage, capacity);

    __CPROVER_assert(rc == 0, "init: returns 0 for valid args");
    __CPROVER_assert(list.capacity == capacity, "init: capacity set correctly");
    __CPROVER_assert(list.sentinel == capacity, "init: sentinel == capacity");
    __CPROVER_assert(list.used_marker == capacity + 1u,
                     "init: used_marker == capacity + 1");
    __CPROVER_assert(list.next == next_storage,
                     "init: next points to next_storage");

    /* deferred chain starts empty (both ends at sentinel) */
    __CPROVER_assert(list.deferred_first == capacity,
                     "init: deferred_first == sentinel");
    __CPROVER_assert(list.deferred_last == capacity,
                     "init: deferred_last == sentinel");

    /* active chain rooted at 0 (capacity>0) or sentinel (capacity==0) */
    if (capacity > 0u) {
        __CPROVER_assert(list.active_first == 0u,
                         "init: active_first == 0 when capacity > 0");
    } else {
        __CPROVER_assert(list.active_first == capacity,
                         "init: active_first == sentinel when capacity == 0");
    }

    /* free chain is a singly-linked list terminating at sentinel */
    for (uint32_t i = 0u; i < capacity; ++i) {
        uint32_t expected = (i + 1u < capacity) ? (i + 1u) : capacity;
        __CPROVER_assert(list.next[i] == expected,
                         "init: free-chain slot properly linked");
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * Check 2 — init with NULL list returns -1
 * ══════════════════════════════════════════════════════════════════════ */
void check_init_null_list(void) {
    uint32_t capacity;
    __CPROVER_havoc_object(&capacity);
    __CPROVER_assume(capacity <= MAX_CAPACITY);

    int rc = kain_deferred_free_list_init(NULL, next_storage, capacity);
    __CPROVER_assert(rc == -1, "init(NULL, storage, cap): returns -1");
}

/* ══════════════════════════════════════════════════════════════════════
 * Check 3 — init with NULL storage returns -1
 * ══════════════════════════════════════════════════════════════════════ */
void check_init_null_storage(void) {
    static KainDeferredFreeList list;
    __CPROVER_havoc_object(&list);
    uint32_t capacity;
    __CPROVER_havoc_object(&capacity);
    __CPROVER_assume(capacity <= MAX_CAPACITY);

    int rc = kain_deferred_free_list_init(&list, NULL, capacity);
    __CPROVER_assert(rc == -1, "init(list, NULL, cap): returns -1");
}

/* ══════════════════════════════════════════════════════════════════════
 * Check 4 — allocate returns valid indices
 *
 * When capacity > 0 the first alloc always succeeds and marks the slot
 * as used.  When capacity == 0 it always fails.
 * ══════════════════════════════════════════════════════════════════════ */
void check_allocate(void) {
    KainDeferredFreeList* list = create_initialized_list();

    int idx = kain_deferred_free_list_allocate(list);

    if (LIST_CAP == 0u) {
        __CPROVER_assert(idx == -1,
                         "allocate: returns -1 when capacity == 0");
    } else {
        __CPROVER_assert(idx >= 0,
                         "allocate: returns valid index when capacity > 0");
        __CPROVER_assert((uint32_t)idx < LIST_CAP,
                         "allocate: index within capacity");
        __CPROVER_assert(list->next[idx] == list->used_marker,
                         "allocate: next[index] == used_marker (slot marked in-use)");
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * Check 5 — exhaust all allocations, verify full-list behaviour
 *
 * Allocate as many slots as possible, then confirm the next call
 * returns -1.  Also verifies that an already-full list stays full.
 * ══════════════════════════════════════════════════════════════════════ */
void check_allocate_full(void) {
    KainDeferredFreeList* list = create_initialized_list();

    /* Try to exhaust — loop bounded by MAX_CAPACITY */
    uint32_t allocated = 0u;
    for (uint32_t i = 0u; i < MAX_CAPACITY; ++i) {
        int idx = kain_deferred_free_list_allocate(list);
        if (idx == -1) break;
        __CPROVER_assert((uint32_t)idx < LIST_CAP,
                         "allocate full: each index within capacity");
        allocated++;
    }

    if (LIST_CAP > 0u) {
        __CPROVER_assert(allocated == LIST_CAP,
                         "allocate full: allocated exactly capacity slots");
        /* Next allocation must fail — active chain exhausted */
        int final_alloc = kain_deferred_free_list_allocate(list);
        __CPROVER_assert(final_alloc == -1,
                         "allocate full: returns -1 when completely full");
    } else {
        __CPROVER_assert(allocated == 0u,
                         "allocate full: nothing allocated when capacity == 0");
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * Check 6 — deferred_free moves a slot to the deferred chain
 *
 * After allocate + deferred_free, the slot's next becomes sentinel
 * (end-of-deferred-chain marker) and the deferred chain bookkeeping
 * points to the single slot.
 * ══════════════════════════════════════════════════════════════════════ */
void check_deferred_free(void) {
    KainDeferredFreeList* list = create_initialized_list();
    if (LIST_CAP == 0u) return;

    /* Allocate one slot */
    int idx = kain_deferred_free_list_allocate(list);
    __CPROVER_assume(idx >= 0);
    uint32_t slot = (uint32_t)idx;

    uint32_t old_sentinel = list->sentinel;

    /* Deferred-free the slot */
    kain_deferred_free_list_deferred_free(list, slot);

    /* Slot becomes the sole deferred entry — next[slot] == sentinel */
    __CPROVER_assert(list->next[slot] == old_sentinel,
                     "deferred_free: next[slot] == sentinel (end of deferred chain)");

    /* Deferred chain bookkeeping */
    __CPROVER_assert(list->deferred_first == slot,
                     "deferred_free: deferred_first == slot");
    __CPROVER_assert(list->deferred_last == slot,
                     "deferred_free: deferred_last == slot");
}

/* ══════════════════════════════════════════════════════════════════════
 * Check 7 — deferred_free on non-allocated / OOB slots is a no-op
 *
 * Three cases:
 *   (a) slot still in free chain (never allocated)
 *   (b) index == sentinel == capacity
 *   (c) index >> capacity (way out of bounds)
 * ══════════════════════════════════════════════════════════════════════ */
void check_deferred_free_noop(void) {
    KainDeferredFreeList* list = create_initialized_list();

    /* (a) Free-chain slot — next[index] != used_marker so early-return */
    if (LIST_CAP > 0u) {
        uint32_t pre_next = list->next[0u];
        kain_deferred_free_list_deferred_free(list, 0u);
        __CPROVER_assert(list->next[0u] == pre_next,
                         "deferred_free(free slot): next unchanged (no-op)");
    }

    /* (b) Out-of-bounds index == sentinel == capacity */
    if (LIST_CAP > 0u) {
        uint32_t pre_df = list->deferred_first;
        uint32_t pre_dl = list->deferred_last;
        kain_deferred_free_list_deferred_free(list, list->capacity);
        __CPROVER_assert(list->deferred_first == pre_df,
                         "deferred_free(OOB=sentinel): deferred_first unchanged");
        __CPROVER_assert(list->deferred_last == pre_dl,
                         "deferred_free(OOB=sentinel): deferred_last unchanged");
    }

    /* (c) Way out of bounds */
    uint32_t pre_df = list->deferred_first;
    uint32_t pre_dl = list->deferred_last;
    kain_deferred_free_list_deferred_free(list, 0xFFFFFFFFu);
    __CPROVER_assert(list->deferred_first == pre_df,
                     "deferred_free(way-OOB): deferred_first unchanged");
    __CPROVER_assert(list->deferred_last == pre_dl,
                     "deferred_free(way-OOB): deferred_last unchanged");
}

/* ══════════════════════════════════════════════════════════════════════
 * Check 8 — flush moves deferred slots back to the active chain
 *
 * After allocate + deferred_free + flush, the deferred chain is empty
 * and the previously-deferred slot is now the head of the active chain.
 * ══════════════════════════════════════════════════════════════════════ */
void check_flush(void) {
    KainDeferredFreeList* list = create_initialized_list();
    if (LIST_CAP == 0u) return;

    /* Allocate and defer one slot */
    int idx = kain_deferred_free_list_allocate(list);
    __CPROVER_assume(idx >= 0);
    uint32_t slot = (uint32_t)idx;
    kain_deferred_free_list_deferred_free(list, slot);

    uint32_t old_sentinel = list->sentinel;

    /* Flush */
    kain_deferred_free_list_flush(list);

    /* Deferred chain must be empty (both ends at sentinel) */
    __CPROVER_assert(list->deferred_first == old_sentinel,
                     "flush: deferred_first == sentinel");
    __CPROVER_assert(list->deferred_last == old_sentinel,
                     "flush: deferred_last == sentinel");

    /* The deferred slot becomes the new head of the active chain */
    __CPROVER_assert(list->active_first == slot,
                     "flush: active_first == old deferred_first");
}

/* ══════════════════════════════════════════════════════════════════════
 * Check 9 — flush with an empty deferred chain is a no-op
 *
 * When deferred_first == sentinel the early-return guard fires before
 * any mutations.
 * ══════════════════════════════════════════════════════════════════════ */
void check_flush_empty(void) {
    KainDeferredFreeList* list = create_initialized_list();

    uint32_t pre_active = list->active_first;
    uint32_t pre_df     = list->deferred_first;
    uint32_t pre_dl     = list->deferred_last;

    kain_deferred_free_list_flush(list);

    __CPROVER_assert(list->active_first == pre_active,
                     "flush empty: active_first unchanged");
    __CPROVER_assert(list->deferred_first == pre_df,
                     "flush empty: deferred_first unchanged");
    __CPROVER_assert(list->deferred_last == pre_dl,
                     "flush empty: deferred_last unchanged");
}

/* ══════════════════════════════════════════════════════════════════════
 * Check 10 — allocate → deferred_free → flush → allocate cycle
 *
 * Verifies that a slot can be recycled through the full pipeline and
 * that flush returns it to the front of the active chain (LIFO order).
 * ══════════════════════════════════════════════════════════════════════ */
void check_cycle(void) {
    KainDeferredFreeList* list = create_initialized_list();
    if (LIST_CAP == 0u) return;

    /* Allocate */
    int a = kain_deferred_free_list_allocate(list);
    __CPROVER_assume(a >= 0);

    /* Deferred free */
    kain_deferred_free_list_deferred_free(list, (uint32_t)a);

    /* Flush — connects deferred chain to front of active chain */
    kain_deferred_free_list_flush(list);

    /* Re-allocate — should return the same index (flush puts deferred
     * items at the front). */
    int b = kain_deferred_free_list_allocate(list);
    __CPROVER_assert(b >= 0, "cycle: reallocate succeeds");
    if (b >= 0) {
        __CPROVER_assert((uint32_t)b == (uint32_t)a,
                         "cycle: reallocate returns same slot (flush is LIFO)");
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * Check 11 — is_empty reflects list state
 *
 * After init with capacity 0: is_empty == 1.
 * After init with capacity > 0: is_empty == 0 (free slots exist).
 * After exhausting all allocations: is_empty == 1 (both chains at sentinel).
 * Flush on empty-allocation state keeps is_empty == 1.
 * ══════════════════════════════════════════════════════════════════════ */
void check_is_empty(void) {
    KainDeferredFreeList* list = create_initialized_list();

    /* After init */
    if (LIST_CAP == 0u) {
        __CPROVER_assert(kain_deferred_free_list_is_empty(list) == 1,
                         "is_empty: true when capacity == 0");
    } else {
        __CPROVER_assert(kain_deferred_free_list_is_empty(list) == 0,
                         "is_empty: false when free slots exist");
    }

    /* Exhaust all allocations — bounded by MAX_CAPACITY loop */
    for (uint32_t i = 0u; i < MAX_CAPACITY; ++i) {
        int idx = kain_deferred_free_list_allocate(list);
        if (idx == -1) break;
    }
    __CPROVER_assert(kain_deferred_free_list_is_empty(list) == 1,
                     "is_empty: true after full allocation (no free, no deferred)");

    /* Flush with nothing deferred is a no-op — is_empty stays true */
    kain_deferred_free_list_flush(list);
    __CPROVER_assert(kain_deferred_free_list_is_empty(list) == 1,
                     "is_empty: flush on empty-allocation keeps is_empty == 1");
}

/* ══════════════════════════════════════════════════════════════════════
 * Check 12 — is_empty(NULL) returns 1 (safe)
 * ══════════════════════════════════════════════════════════════════════ */
void check_is_empty_null(void) {
    int rc = kain_deferred_free_list_is_empty(NULL);
    __CPROVER_assert(rc == 1, "is_empty(NULL): returns 1 (safe)");
}

/* ══════════════════════════════════════════════════════════════════════
 * Check 13 — NULL safety for all void-returning functions
 *
 * Calling with list == NULL must not crash or produce UB.
 * ══════════════════════════════════════════════════════════════════════ */
void check_null_safety(void) {
    kain_deferred_free_list_make_all_free(NULL);
    kain_deferred_free_list_deferred_free(NULL, 0u);
    kain_deferred_free_list_flush(NULL);
    /* (no assertions needed — absence of crash is the property) */
}

/* ══════════════════════════════════════════════════════════════════════
 * Check 14 — NULL next pointer safety
 *
 * When the list struct is valid but list->next == NULL, all functions
 * must handle this gracefully via their early-return guards.
 * ══════════════════════════════════════════════════════════════════════ */
void check_null_next_safety(void) {
    KainDeferredFreeList list;
    __CPROVER_havoc_object(&list);
    list.next = NULL;

    /* Void functions must be safe with next == NULL */
    kain_deferred_free_list_make_all_free(&list);
    kain_deferred_free_list_deferred_free(&list, 0u);
    kain_deferred_free_list_flush(&list);

    /* allocate checks next == NULL and returns -1 */
    __CPROVER_assert(kain_deferred_free_list_allocate(&list) == -1,
                     "allocate with next==NULL returns -1");
}

/* ══════════════════════════════════════════════════════════════════════
 * Check 15 — multiple deferred_free items build a correct chain
 *
 * With two deferred slots, the first points to the second, and the
 * second points to sentinel (end marker).  Bookkeeping tracks the
 * head and tail.
 * ══════════════════════════════════════════════════════════════════════ */
void check_multiple_deferred(void) {
    KainDeferredFreeList* list = create_initialized_list();
    if (LIST_CAP < 3u) return;

    /* Allocate two distinct slots */
    int a = kain_deferred_free_list_allocate(list);
    int b = kain_deferred_free_list_allocate(list);
    __CPROVER_assume(a >= 0 && b >= 0);

    /* Defer both, in order */
    kain_deferred_free_list_deferred_free(list, (uint32_t)a);
    kain_deferred_free_list_deferred_free(list, (uint32_t)b);

    /* Chain invariant: a -> b -> sentinel */
    __CPROVER_assert(list->next[(uint32_t)a] == (uint32_t)b,
                     "multi-deferred: first slot -> second slot in chain");
    __CPROVER_assert(list->next[(uint32_t)b] == list->sentinel,
                     "multi-deferred: second slot -> sentinel");

    /* Deferred bookkeeping tracks head and tail */
    __CPROVER_assert(list->deferred_first == (uint32_t)a,
                     "multi-deferred: deferred_first == first slot");
    __CPROVER_assert(list->deferred_last == (uint32_t)b,
                     "multi-deferred: deferred_last == second slot");
}

/* ══════════════════════════════════════════════════════════════════════
 * Main — run all checks, return 0
 * ══════════════════════════════════════════════════════════════════════ */
int main(void) {
    check_init();
    check_init_null_list();
    check_init_null_storage();
    check_allocate();
    check_allocate_full();
    check_deferred_free();
    check_deferred_free_noop();
    check_flush();
    check_flush_empty();
    check_cycle();
    check_is_empty();
    check_is_empty_null();
    check_null_safety();
    check_null_next_safety();
    check_multiple_deferred();
    return 0;
}
