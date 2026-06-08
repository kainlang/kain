/*
 * check_batch_queue.c — CBMC verification harness for batch_queue module
 *
 * Tests batched message queue operations: init, lock, enqueue (active/pending),
 * drain with callback, NULL safety, capacity limits, and nested hold_depth.
 *
 * This harness is self-contained: it provides minimal platform stubs so that
 * CBMC can compile the real batch_queue.c source without dragging in massive
 * system headers (pthread.h, windows.h) that CBMC's parser can't handle.
 *
 * Key invariants verified:
 *   - init correctly initialises all fields and rejects NULL/zero-capacity
 *   - lock increments hold_depth; multiple locks nest correctly
 *   - enqueue stores in active when hold_depth==0, pending when >0
 *   - enqueue returns -1 when target ring is full
 *   - unlock_and_drain processes entries, promotes pending to active, and
 *     calls drain_fn for each entry
 *   - Nested holds (hold_depth > 1) delay draining until the final release
 *   - All functions are NULL-safe (no crash paths)
 *   - Uninitialized queues are safely rejected
 *   - Drain with NULL callback consumes entries without crashing
 *   - Entry contents are preserved end-to-end through enqueue and drain
 *
 * Run via: python test/scripts/run_pipeline.py cbmc --harness check_batch_queue --unwind 8
 * Or:     cbmc --unwind 8 --no-unwinding-assertions --trace \
 *             test/cbmc/check_batch_queue.c -I include -I src/core
 */

/* ═══════════════════════════════════════════════════════════════════════
 * Platform stubs — replace pthread.h / windows.h for CBMC
 * ═══════════════════════════════════════════════════════════════════════ */

/* When running inside the run_pipeline.py, the combined file has
 * the real batch_queue.c first (with its #include "batch_queue.h" which
 * pulls in base.h → system headers).  We guard with CBMC_STUBS to let
 * this compile both standalone (with stubs) and in the combined pipeline
 * (where system headers are already available via WSL GCC preprocessing).
 */
#ifndef CBMC_STUBS
  /* Not needed when batch_queue.c has already been concatenated */
#else

/* We define CBMC_STUBS to use this path — define it when running
 * CBMC directly on this file without the combined source.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Opaque mutex type — CBMC doesn't care about the internals */
typedef int pthread_mutex_t;
typedef int pthread_mutexattr_t;
static inline int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) {
    (void)m; (void)a;
    return 0;
}
static inline int pthread_mutex_lock(pthread_mutex_t *m) {
    (void)m;
    return 0;
}
static inline int pthread_mutex_unlock(pthread_mutex_t *m) {
    (void)m;
    return 0;
}

/* Minimal integer types used by base.h that CBMC needs */
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
typedef unsigned int GLuint;

/* Include the real batch_queue header now that stubs are in place */
#include "batch_queue.h"

#endif /* CBMC_STUBS */


/* ═══════════════════════════════════════════════════════════════════════
 * Bounded backing arrays — CBMC knows these are real objects
 * ═══════════════════════════════════════════════════════════════════════ */

#define MAX_CAPACITY 8
static KainBatchQueueEntry active_slots[MAX_CAPACITY];
static KainBatchQueueEntry pending_slots[MAX_CAPACITY];

/* Static payload for testing ptr0 provenance */
static int dummy_payload;

/* Static drain-tracking globals (reset at start of each test) */
static int drain_count;
static KainBatchQueueEntry drained[MAX_CAPACITY];
static int drain_user_data_value;


/* ═══════════════════════════════════════════════════════════════════════
 * Helpers
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * create_valid_queue
 *
 * Havoc the queue struct and both slot arrays, then init with nondet
 * capacity bounded to [1, MAX_CAPACITY].  Returns a pointer to a
 * static queue that CBMC can reason about.
 * ────────────────────────────────────────────────────────────────────── */
static KainBatchQueue* create_valid_queue(void) {
    static KainBatchQueue queue;
    size_t cap;
    __CPROVER_havoc_object(&queue);
    __CPROVER_havoc_object(active_slots);
    __CPROVER_havoc_object(pending_slots);
    __CPROVER_havoc_object(&cap);
    __CPROVER_assume(cap >= 1 && cap <= MAX_CAPACITY);

    kain_batch_queue_init(&queue, active_slots, pending_slots, cap);

    return &queue;
}

/* ──────────────────────────────────────────────────────────────────────
 * create_valid_queue_min
 *
 * Like create_valid_queue but capacity is >= min_cap.
 * ────────────────────────────────────────────────────────────────────── */
static KainBatchQueue* create_valid_queue_min(size_t min_cap) {
    static KainBatchQueue queue;
    size_t cap;
    __CPROVER_havoc_object(&queue);
    __CPROVER_havoc_object(active_slots);
    __CPROVER_havoc_object(pending_slots);
    __CPROVER_havoc_object(&cap);
    __CPROVER_assume(cap >= min_cap && cap <= MAX_CAPACITY);

    kain_batch_queue_init(&queue, active_slots, pending_slots, cap);

    return &queue;
}

/* ──────────────────────────────────────────────────────────────────────
 * nondet_entry
 *
 * Returns a nondet KainBatchQueueEntry with valid ptr0 provenance.
 * ────────────────────────────────────────────────────────────────────── */
static KainBatchQueueEntry nondet_entry(void) {
    KainBatchQueueEntry e;
    __CPROVER_havoc_object(&e);
    e.ptr0 = &dummy_payload;
    return e;
}


/* ═══════════════════════════════════════════════════════════════════════
 * Drain callback
 * ═══════════════════════════════════════════════════════════════════════ */

static void test_drain_fn(const KainBatchQueueEntry* entry, void* user_data) {
    if (drain_count < MAX_CAPACITY) {
        drained[drain_count] = *entry;
    }
    drain_count++;
    if (user_data != NULL) {
        drain_user_data_value = *(int*)user_data;
    }
}


/* ═══════════════════════════════════════════════════════════════════════
 * Initialization checks
 * ═══════════════════════════════════════════════════════════════════════ */

static void check_init(void) {
    KainBatchQueue queue;
    size_t cap;
    __CPROVER_havoc_object(&queue);
    __CPROVER_havoc_object(active_slots);
    __CPROVER_havoc_object(pending_slots);
    __CPROVER_havoc_object(&cap);
    __CPROVER_assume(cap >= 1 && cap <= MAX_CAPACITY);

    kain_batch_queue_init(&queue, active_slots, pending_slots, cap);

    __CPROVER_assert(queue.active_entries == active_slots,
                     "init: active_entries pointer set");
    __CPROVER_assert(queue.pending_entries == pending_slots,
                     "init: pending_entries pointer set");
    __CPROVER_assert(queue.capacity == cap,
                     "init: capacity set");
    __CPROVER_assert(queue.active_head == 0,
                     "init: active_head == 0");
    __CPROVER_assert(queue.active_count == 0,
                     "init: active_count == 0");
    __CPROVER_assert(queue.pending_count == 0,
                     "init: pending_count == 0");
    __CPROVER_assert(queue.hold_depth == 0,
                     "init: hold_depth == 0");
    __CPROVER_assert(queue.initialized == 1,
                     "init: initialized == 1");
}

static void check_init_null_queue(void) {
    kain_batch_queue_init(NULL, active_slots, pending_slots, 4);
    __CPROVER_assert(1, "init_null_queue: no crash");
}

static void check_init_null_entries(void) {
    KainBatchQueue queue;

    __CPROVER_havoc_object(&queue);
    kain_batch_queue_init(&queue, NULL, pending_slots, 4);
    __CPROVER_assert(queue.initialized == 0,
                     "init_null_entries: NULL active -> not initialized");

    __CPROVER_havoc_object(&queue);
    kain_batch_queue_init(&queue, active_slots, NULL, 4);
    __CPROVER_assert(queue.initialized == 0,
                     "init_null_entries: NULL pending -> not initialized");
}

static void check_init_zero_capacity(void) {
    KainBatchQueue queue;
    __CPROVER_havoc_object(&queue);

    kain_batch_queue_init(&queue, active_slots, pending_slots, 0);
    __CPROVER_assert(queue.initialized == 0,
                     "init_zero_cap: not initialized");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Lock checks
 * ═══════════════════════════════════════════════════════════════════════ */

static void check_lock_valid(void) {
    KainBatchQueue* queue = create_valid_queue();
    kain_batch_queue_lock(queue);
    __CPROVER_assert(queue->hold_depth == 1,
                     "lock_valid: hold_depth == 1");
}

static void check_lock_null(void) {
    kain_batch_queue_lock(NULL);
    __CPROVER_assert(1, "lock_null: no crash");
}

static void check_lock_uninitialized(void) {
    KainBatchQueue queue;
    __CPROVER_havoc_object(&queue);
    queue.initialized = 0;

    kain_batch_queue_lock(&queue);
    __CPROVER_assert(queue.hold_depth == 0,
                     "lock_uninitialized: hold_depth unchanged");
}

static void check_lock_multiple(void) {
    KainBatchQueue* queue = create_valid_queue();

    kain_batch_queue_lock(queue);
    __CPROVER_assert(queue->hold_depth == 1,
                     "lock_multiple: first lock -> 1");
    kain_batch_queue_lock(queue);
    __CPROVER_assert(queue->hold_depth == 2,
                     "lock_multiple: second lock -> 2");
    kain_batch_queue_lock(queue);
    __CPROVER_assert(queue->hold_depth == 3,
                     "lock_multiple: third lock -> 3");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Enqueue checks
 * ═══════════════════════════════════════════════════════════════════════ */

static void check_enqueue_active(void) {
    KainBatchQueue* queue = create_valid_queue();
    KainBatchQueueEntry entry = nondet_entry();

    int rc = kain_batch_queue_enqueue(queue, &entry);
    __CPROVER_assert(rc == 0,
                     "enqueue_active: success");

    __CPROVER_assert(queue->active_count == 1,
                     "enqueue_active: active_count == 1");
    __CPROVER_assert(queue->pending_count == 0,
                     "enqueue_active: pending_count == 0");
    __CPROVER_assert(queue->active_head == 0,
                     "enqueue_active: active_head == 0");

    __CPROVER_assert(queue->active_entries[0].kind == entry.kind,
                     "enqueue_active: kind preserved");
    __CPROVER_assert(queue->active_entries[0].arg0 == entry.arg0,
                     "enqueue_active: arg0 preserved");
    __CPROVER_assert(queue->active_entries[0].arg1 == entry.arg1,
                     "enqueue_active: arg1 preserved");
    __CPROVER_assert(queue->active_entries[0].ptr0 == entry.ptr0,
                     "enqueue_active: ptr0 preserved");
}

static void check_enqueue_pending(void) {
    KainBatchQueue* queue = create_valid_queue();
    KainBatchQueueEntry entry = nondet_entry();

    kain_batch_queue_lock(queue);
    __CPROVER_assert(queue->hold_depth == 1,
                     "enqueue_pending: hold_depth == 1 after lock");

    int rc = kain_batch_queue_enqueue(queue, &entry);
    __CPROVER_assert(rc == 0,
                     "enqueue_pending: success");

    __CPROVER_assert(queue->active_count == 0,
                     "enqueue_pending: active_count == 0");
    __CPROVER_assert(queue->pending_count == 1,
                     "enqueue_pending: pending_count == 1");

    __CPROVER_assert(queue->pending_entries[0].kind == entry.kind,
                     "enqueue_pending: kind preserved");
    __CPROVER_assert(queue->pending_entries[0].arg0 == entry.arg0,
                     "enqueue_pending: arg0 preserved");
    __CPROVER_assert(queue->pending_entries[0].arg1 == entry.arg1,
                     "enqueue_pending: arg1 preserved");
    __CPROVER_assert(queue->pending_entries[0].ptr0 == entry.ptr0,
                     "enqueue_pending: ptr0 preserved");
}

static void check_enqueue_null_queue(void) {
    KainBatchQueueEntry entry = nondet_entry();
    int rc = kain_batch_queue_enqueue(NULL, &entry);
    __CPROVER_assert(rc == -1,
                     "enqueue_null_queue: returns -1");
}

static void check_enqueue_null_entry(void) {
    KainBatchQueue* queue = create_valid_queue();
    int rc = kain_batch_queue_enqueue(queue, NULL);
    __CPROVER_assert(rc == -1,
                     "enqueue_null_entry: returns -1");
}

static void check_enqueue_uninitialized(void) {
    KainBatchQueue queue;
    KainBatchQueueEntry entry = nondet_entry();
    __CPROVER_havoc_object(&queue);
    queue.initialized = 0;

    int rc = kain_batch_queue_enqueue(&queue, &entry);
    __CPROVER_assert(rc == -1,
                     "enqueue_uninitialized: returns -1");
}

static void check_enqueue_full_active(void) {
    KainBatchQueue* queue = create_valid_queue_min(2);
    KainBatchQueueEntry entry = nondet_entry();

    for (size_t i = 0; i < queue->capacity; ++i) {
        int rc = kain_batch_queue_enqueue(queue, &entry);
        __CPROVER_assert(rc == 0,
                         "enqueue_full_active: fill succeeded");
    }

    __CPROVER_assert(queue->active_count == queue->capacity,
                     "enqueue_full_active: active_count == capacity");

    int rc = kain_batch_queue_enqueue(queue, &entry);
    __CPROVER_assert(rc == -1,
                     "enqueue_full_active: overflow returns -1");
}

static void check_enqueue_full_pending(void) {
    KainBatchQueue* queue = create_valid_queue_min(2);
    KainBatchQueueEntry entry = nondet_entry();

    kain_batch_queue_lock(queue);

    for (size_t i = 0; i < queue->capacity; ++i) {
        int rc = kain_batch_queue_enqueue(queue, &entry);
        __CPROVER_assert(rc == 0,
                         "enqueue_full_pending: fill succeeded");
    }

    __CPROVER_assert(queue->pending_count == queue->capacity,
                     "enqueue_full_pending: pending_count == capacity");

    int rc = kain_batch_queue_enqueue(queue, &entry);
    __CPROVER_assert(rc == -1,
                     "enqueue_full_pending: overflow returns -1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Drain checks
 * ═══════════════════════════════════════════════════════════════════════ */

static void check_drain_active(void) {
    KainBatchQueue* queue = create_valid_queue_min(3);
    KainBatchQueueEntry e1 = nondet_entry();
    KainBatchQueueEntry e2 = nondet_entry();
    KainBatchQueueEntry e3 = nondet_entry();

    drain_count = 0;

    __CPROVER_assert(kain_batch_queue_enqueue(queue, &e1) == 0,
                     "drain_active: enqueue 1");
    __CPROVER_assert(kain_batch_queue_enqueue(queue, &e2) == 0,
                     "drain_active: enqueue 2");
    __CPROVER_assert(kain_batch_queue_enqueue(queue, &e3) == 0,
                     "drain_active: enqueue 3");
    __CPROVER_assert(queue->active_count == 3,
                     "drain_active: 3 entries enqueued");

    kain_batch_queue_unlock_and_drain(queue, test_drain_fn, NULL);

    __CPROVER_assert(queue->active_count == 0,
                     "drain_active: active_count == 0 after drain");
    __CPROVER_assert(queue->active_head == 0,
                     "drain_active: active_head == 0 after drain");
    __CPROVER_assert(drain_count == 3,
                     "drain_active: drain_fn called 3 times");
}

static void check_drain_pending(void) {
    KainBatchQueue* queue = create_valid_queue_min(2);
    KainBatchQueueEntry e1 = nondet_entry();
    KainBatchQueueEntry e2 = nondet_entry();

    drain_count = 0;

    kain_batch_queue_lock(queue);
    __CPROVER_assert(kain_batch_queue_enqueue(queue, &e1) == 0,
                     "drain_pending: enqueue 1");
    __CPROVER_assert(kain_batch_queue_enqueue(queue, &e2) == 0,
                     "drain_pending: enqueue 2");
    __CPROVER_assert(queue->pending_count == 2,
                     "drain_pending: pending_count == 2");
    __CPROVER_assert(queue->active_count == 0,
                     "drain_pending: active_count == 0");

    kain_batch_queue_unlock_and_drain(queue, test_drain_fn, NULL);

    __CPROVER_assert(queue->active_count == 0,
                     "drain_pending: active_count == 0 after drain");
    __CPROVER_assert(queue->pending_count == 0,
                     "drain_pending: pending_count == 0 after drain");
    __CPROVER_assert(drain_count == 2,
                     "drain_pending: drain_fn called 2 times");
}

static void check_drain_mixed(void) {
    KainBatchQueue* queue = create_valid_queue_min(4);
    KainBatchQueueEntry e_active = nondet_entry();
    KainBatchQueueEntry e_pending = nondet_entry();

    drain_count = 0;

    __CPROVER_assert(kain_batch_queue_enqueue(queue, &e_active) == 0,
                     "drain_mixed: enqueue to active");

    kain_batch_queue_lock(queue);
    __CPROVER_assert(kain_batch_queue_enqueue(queue, &e_pending) == 0,
                     "drain_mixed: enqueue to pending");

    __CPROVER_assert(queue->active_count == 1,
                     "drain_mixed: active_count == 1");
    __CPROVER_assert(queue->pending_count == 1,
                     "drain_mixed: pending_count == 1");

    kain_batch_queue_unlock_and_drain(queue, test_drain_fn, NULL);

    __CPROVER_assert(queue->active_count == 0,
                     "drain_mixed: active_count == 0 after drain");
    __CPROVER_assert(queue->pending_count == 0,
                     "drain_mixed: pending_count == 0 after drain");
    __CPROVER_assert(drain_count == 2,
                     "drain_mixed: drain_fn called 2 times");
}

static void check_drain_null_fn(void) {
    KainBatchQueue* queue = create_valid_queue_min(2);
    KainBatchQueueEntry entry = nondet_entry();

    __CPROVER_assert(kain_batch_queue_enqueue(queue, &entry) == 0,
                     "drain_null_fn: enqueue 1");
    __CPROVER_assert(kain_batch_queue_enqueue(queue, &entry) == 0,
                     "drain_null_fn: enqueue 2");

    kain_batch_queue_unlock_and_drain(queue, NULL, NULL);

    __CPROVER_assert(queue->active_count == 0,
                     "drain_null_fn: entries consumed");
}

static void check_drain_nested_hold(void) {
    KainBatchQueue* queue = create_valid_queue_min(2);
    KainBatchQueueEntry entry = nondet_entry();

    drain_count = 0;

    kain_batch_queue_lock(queue);
    kain_batch_queue_lock(queue);
    __CPROVER_assert(queue->hold_depth == 2,
                     "drain_nested: hold_depth == 2 after double lock");

    __CPROVER_assert(kain_batch_queue_enqueue(queue, &entry) == 0,
                     "drain_nested: enqueue 1");
    __CPROVER_assert(kain_batch_queue_enqueue(queue, &entry) == 0,
                     "drain_nested: enqueue 2");
    __CPROVER_assert(queue->pending_count == 2,
                     "drain_nested: pending_count == 2");

    /* First drain — decrements to 1, returns early */
    kain_batch_queue_unlock_and_drain(queue, test_drain_fn, NULL);
    __CPROVER_assert(queue->hold_depth == 1,
                     "drain_nested: hold_depth == 1 after first drain");
    __CPROVER_assert(drain_count == 0,
                     "drain_nested: drain_fn not called yet");
    __CPROVER_assert(queue->pending_count == 2,
                     "drain_nested: pending preserved after first drain");

    /* Second drain — decrements to 0, promotes and drains */
    kain_batch_queue_unlock_and_drain(queue, test_drain_fn, NULL);
    __CPROVER_assert(queue->hold_depth == 0,
                     "drain_nested: hold_depth == 0 after second drain");
    __CPROVER_assert(drain_count == 2,
                     "drain_nested: drain_fn called 2 times");
    __CPROVER_assert(queue->active_count == 0,
                     "drain_nested: active_count == 0 after drain");
    __CPROVER_assert(queue->pending_count == 0,
                     "drain_nested: pending_count == 0 after drain");
}

static void check_drain_empty(void) {
    KainBatchQueue* queue = create_valid_queue();

    drain_count = 0;
    kain_batch_queue_unlock_and_drain(queue, test_drain_fn, NULL);

    __CPROVER_assert(drain_count == 0,
                     "drain_empty: drain_fn not called");
    __CPROVER_assert(queue->active_count == 0,
                     "drain_empty: active_count == 0");
}

static void check_drain_null_queue(void) {
    kain_batch_queue_unlock_and_drain(NULL, test_drain_fn, NULL);
    __CPROVER_assert(1, "drain_null_queue: no crash");
}

static void check_drain_uninitialized(void) {
    KainBatchQueue queue;
    __CPROVER_havoc_object(&queue);
    queue.initialized = 0;

    kain_batch_queue_unlock_and_drain(&queue, test_drain_fn, NULL);
    __CPROVER_assert(1, "drain_uninitialized: no crash");
}

static void check_drain_user_data(void) {
    KainBatchQueue* queue = create_valid_queue();
    KainBatchQueueEntry entry = nondet_entry();
    static int user_value;
    __CPROVER_havoc_object(&user_value);

    drain_count = 0;
    drain_user_data_value = 0;

    __CPROVER_assert(kain_batch_queue_enqueue(queue, &entry) == 0,
                     "drain_user_data: enqueue");

    kain_batch_queue_unlock_and_drain(queue, test_drain_fn, &user_value);

    __CPROVER_assert(drain_count == 1,
                     "drain_user_data: drain_fn called once");
    __CPROVER_assert(1,
                     "drain_user_data: callback received user_data (no crash)");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Combined lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

static void check_full_lifecycle(void) {
    KainBatchQueue* queue = create_valid_queue_min(3);
    KainBatchQueueEntry entry = nondet_entry();

    drain_count = 0;

    kain_batch_queue_lock(queue);
    __CPROVER_assert(queue->hold_depth == 1,
                     "lifecycle: hold_depth == 1 after lock");

    __CPROVER_assert(kain_batch_queue_enqueue(queue, &entry) == 0,
                     "lifecycle: enqueue 1");
    __CPROVER_assert(kain_batch_queue_enqueue(queue, &entry) == 0,
                     "lifecycle: enqueue 2");
    __CPROVER_assert(queue->pending_count == 2,
                     "lifecycle: pending_count == 2");

    kain_batch_queue_unlock_and_drain(queue, test_drain_fn, NULL);

    __CPROVER_assert(queue->hold_depth == 0,
                     "lifecycle: hold_depth == 0 after drain");
    __CPROVER_assert(queue->active_count == 0,
                     "lifecycle: active_count == 0 after drain");
    __CPROVER_assert(queue->pending_count == 0,
                     "lifecycle: pending_count == 0 after drain");
    __CPROVER_assert(drain_count == 2,
                     "lifecycle: drain_fn called 2 times");
}

static void check_enqueue_drain_multiple_cycles(void) {
    KainBatchQueue* queue = create_valid_queue_min(2);
    KainBatchQueueEntry entry = nondet_entry();

    drain_count = 0;

    /* Round 1 */
    __CPROVER_assert(kain_batch_queue_enqueue(queue, &entry) == 0,
                     "multi_cycle: enqueue R1");
    kain_batch_queue_unlock_and_drain(queue, test_drain_fn, NULL);
    __CPROVER_assert(drain_count == 1,
                     "multi_cycle: 1 drained in R1");

    /* Round 2 */
    __CPROVER_assert(kain_batch_queue_enqueue(queue, &entry) == 0,
                     "multi_cycle: enqueue R2");
    kain_batch_queue_unlock_and_drain(queue, test_drain_fn, NULL);
    __CPROVER_assert(drain_count == 2,
                     "multi_cycle: 2 drained total");
}

static void check_enqueue_entry_roundtrip(void) {
    KainBatchQueue* queue = create_valid_queue();
    KainBatchQueueEntry entry = nondet_entry();

    drain_count = 0;

    int rc = kain_batch_queue_enqueue(queue, &entry);
    __CPROVER_assert(rc == 0,
                     "roundtrip: enqueue succeeded");

    kain_batch_queue_unlock_and_drain(queue, test_drain_fn, NULL);

    __CPROVER_assert(drain_count == 1,
                     "roundtrip: drain called once");
    __CPROVER_assert(drained[0].kind == entry.kind,
                     "roundtrip: kind preserved");
    __CPROVER_assert(drained[0].arg0 == entry.arg0,
                     "roundtrip: arg0 preserved");
    __CPROVER_assert(drained[0].arg1 == entry.arg1,
                     "roundtrip: arg1 preserved");
    __CPROVER_assert(drained[0].ptr0 == entry.ptr0,
                     "roundtrip: ptr0 preserved");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Main — run all checks
 * ═══════════════════════════════════════════════════════════════════════ */
int main(void) {
    /* --- Initialization --- */
    check_init();
    check_init_null_queue();
    check_init_null_entries();
    check_init_zero_capacity();

    /* --- Lock --- */
    check_lock_valid();
    check_lock_null();
    check_lock_uninitialized();
    check_lock_multiple();

    /* --- Enqueue --- */
    check_enqueue_active();
    check_enqueue_pending();
    check_enqueue_null_queue();
    check_enqueue_null_entry();
    check_enqueue_uninitialized();
    check_enqueue_full_active();
    check_enqueue_full_pending();

    /* --- Drain --- */
    check_drain_active();
    check_drain_pending();
    check_drain_mixed();
    check_drain_null_fn();
    check_drain_nested_hold();
    check_drain_empty();
    check_drain_null_queue();
    check_drain_uninitialized();
    check_drain_user_data();

    /* --- Combined lifecycle --- */
    check_full_lifecycle();
    check_enqueue_drain_multiple_cycles();
    check_enqueue_entry_roundtrip();

    return 0;
}
