/*
 * check_handle.c — CBMC verification harness for handle module
 *
 * Tests handle encoding, table initialization, acquire/release/resolve
 * lifecycle, NULL-safety, stale-handle rejection, and full-table
 * pressure — all bounded to small slot arrays for CBMC tractability.
 *
 * Key invariants verified:
 *   - kain_handle_make round-trips through extractors (kind, slot, magic)
 *   - kain_handle_table_init produces consistent free-list
 *   - Acquired handle resolves back to correct payload
 *   - Released handle becomes stale (resolve returns NULL)
 *   - Wrong-kind resolve/rebind/release are rejected
 *   - All table functions are NULL-safe (no crash paths)
 *   - Full table returns KAIN_RUNTIME_HANDLE_INVALID
 *   - Re-acquire reuses slot with bumped generation (magic)
 *
 * Run via: python test/scripts/run_pipeline.py cbmc --harness check_handle --unwind 8
 * Or:     cbmc --unwind 8 --trace test/cbmc/check_handle.c src/core/handle.c
 *             -I include -I src/core
 */

#include "handle.h"
#include <stddef.h>

/* ── Bounded slot array — CBMC knows these are real objects ── */
#define MAX_CAPACITY 8
static KainHandleSlot slot_array[MAX_CAPACITY];

/* Static payload objects (pointers with valid provenance) */
static int payload_a;
static int payload_b;
static int payload_c;


/* ═══════════════════════════════════════════════════════════════════════
 * Helpers
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * create_valid_table
 *
 * Havoc the table struct and the slot array, then init with nondet
 * capacity bounded to [1, MAX_CAPACITY].  Returns a pointer to a
 * static table that CBMC can reason about.
 * ────────────────────────────────────────────────────────────────────── */
static KainHandleTable* create_valid_table(void) {
    static KainHandleTable table;
    uint32_t cap;
    __CPROVER_havoc_object(&table);
    __CPROVER_havoc_object(slot_array);
    __CPROVER_havoc_object(&cap);
    __CPROVER_assume(cap >= 1 && cap <= MAX_CAPACITY);

    kain_handle_table_init(&table, slot_array, cap);

    return &table;
}

/* ──────────────────────────────────────────────────────────────────────
 * create_valid_table_min
 *
 * Like create_valid_table but the nondet capacity is further constrained
 * to be >= min_cap.  Useful for tests that need several slots.
 * ────────────────────────────────────────────────────────────────────── */
static KainHandleTable* create_valid_table_min(uint32_t min_cap) {
    static KainHandleTable table;
    uint32_t cap;
    __CPROVER_havoc_object(&table);
    __CPROVER_havoc_object(slot_array);
    __CPROVER_havoc_object(&cap);
    __CPROVER_assume(cap >= min_cap && cap <= MAX_CAPACITY);

    kain_handle_table_init(&table, slot_array, cap);

    return &table;
}


/* ═══════════════════════════════════════════════════════════════════════
 * Handle encoding / extraction checks
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_handle_make_roundtrip
 *
 * For any kind [0..255], slot [0..MAX_CAPACITY], magic [any uint32]:
 *   make(kind, slot, magic) → extract kind/slot/magic → original values
 *
 * Key edge cases:
 *   - magic=0 is bumped to 1 internally
 *   - slot=0 round-trips correctly (stored as 1, subtract 1 on extract)
 *   - kind is masked to 8 bits
 * ────────────────────────────────────────────────────────────────────── */
void check_handle_make_roundtrip(void) {
    uint32_t kind;
    uint32_t slot;
    uint32_t magic;
    __CPROVER_havoc_object(&kind);
    __CPROVER_havoc_object(&slot);
    __CPROVER_havoc_object(&magic);
    __CPROVER_assume(slot <= MAX_CAPACITY);

    KainRuntimeHandle h = kain_handle_make(kind, slot, magic);

    /* Kind round-trip: only low 8 bits preserved */
    uint32_t extracted_kind = kain_handle_kind(h);
    __CPROVER_assert(extracted_kind == (kind & 0xffu),
                     "make_roundtrip: kind matches");

    /* Slot round-trip: handle stores slot+1, extractor subtracts 1 */
    uint32_t extracted_slot = kain_handle_slot(h);
    __CPROVER_assert(extracted_slot == slot,
                     "make_roundtrip: slot matches");

    /* Magic round-trip: 0 becomes 1, otherwise masked to 24 bits */
    uint32_t expected_magic = (magic & 0x00ffffffu) == 0u
                              ? 1u
                              : (magic & 0x00ffffffu);
    uint32_t extracted_magic = kain_handle_magic(h);
    __CPROVER_assert(extracted_magic == expected_magic,
                     "make_roundtrip: magic matches");
}


/* ──────────────────────────────────────────────────────────────────────
 * check_handle_slot_invalid
 *
 * kain_handle_slot(KAIN_RUNTIME_HANDLE_INVALID) must return UINT32_MAX
 * because the encoded slot portion is 0.
 * ────────────────────────────────────────────────────────────────────── */
void check_handle_slot_invalid(void) {
    uint32_t extracted_slot = kain_handle_slot(KAIN_RUNTIME_HANDLE_INVALID);
    __CPROVER_assert(extracted_slot == UINT32_MAX,
                     "slot_invalid: UINT32_MAX for INVALID handle");
}


/* ──────────────────────────────────────────────────────────────────────
 * check_handle_make_zero
 *
 * make(0, 0, 0) → kind=0, slot=0, magic=1 (zero bumped to 1).
 * ────────────────────────────────────────────────────────────────────── */
void check_handle_make_zero(void) {
    KainRuntimeHandle h = kain_handle_make(0, 0, 0);
    __CPROVER_assert(kain_handle_kind(h) == 0,
                     "make_zero: kind == 0");
    __CPROVER_assert(kain_handle_slot(h) == 0,
                     "make_zero: slot == 0");
    __CPROVER_assert(kain_handle_magic(h) == 1,
                     "make_zero: magic == 1 (zero bumped)");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Table initialisation checks
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_table_init
 *
 * After init with capacity C (1 <= C <= MAX_CAPACITY):
 *   - slots pointer set, capacity = C
 *   - first_free = 0, live_count = 0, initialized = 1
 *   - each slot: payload = 0, kind = NONE, magic = 1, occupied = 0
 *   - free list: next_free = i+1 for i < C-1, last = UINT32_MAX
 * ────────────────────────────────────────────────────────────────────── */
void check_table_init(void) {
    KainHandleTable table;
    uint32_t cap;
    __CPROVER_havoc_object(&table);
    __CPROVER_havoc_object(slot_array);
    __CPROVER_havoc_object(&cap);
    __CPROVER_assume(cap >= 1 && cap <= MAX_CAPACITY);

    kain_handle_table_init(&table, slot_array, cap);

    __CPROVER_assert(table.slots == slot_array,
                     "init: slots pointer set");
    __CPROVER_assert(table.capacity == cap,
                     "init: capacity set");
    __CPROVER_assert(table.first_free == 0,
                     "init: first_free == 0");
    __CPROVER_assert(table.live_count == 0,
                     "init: live_count == 0");
    __CPROVER_assert(table.initialized == 1,
                     "init: initialized == 1");

    /* Every slot in the free-list is initialised */
    for (uint32_t i = 0; i < cap; ++i) {
        __CPROVER_assert(slot_array[i].payload == 0,
                         "init: slot payload == NULL");
        __CPROVER_assert(slot_array[i].kind == KAIN_HANDLE_KIND_NONE,
                         "init: slot kind == NONE");
        __CPROVER_assert(slot_array[i].magic == 1,
                         "init: slot magic == 1");
        __CPROVER_assert(slot_array[i].occupied == 0,
                         "init: slot not occupied");
        if (i + 1u < cap) {
            __CPROVER_assert(slot_array[i].next_free == i + 1u,
                             "init: next_free points forward");
        } else {
            __CPROVER_assert(slot_array[i].next_free == UINT32_MAX,
                             "init: last next_free == UINT32_MAX");
        }
    }
}


/* ──────────────────────────────────────────────────────────────────────
 * check_table_init_null
 *
 * Passing NULL table is a no-op (no crash).
 * ────────────────────────────────────────────────────────────────────── */
void check_table_init_null(void) {
    kain_handle_table_init(NULL, slot_array, 4);
    __CPROVER_assert(1, "init_null: no crash");
}


/* ──────────────────────────────────────────────────────────────────────
 * check_table_init_zero_capacity
 *
 * Zero capacity: first_free = UINT32_MAX, capacity = 0, no slots
 * initialised (but slots pointer IS set).
 * ────────────────────────────────────────────────────────────────────── */
void check_table_init_zero_capacity(void) {
    KainHandleTable table;
    __CPROVER_havoc_object(&table);

    kain_handle_table_init(&table, slot_array, 0);

    __CPROVER_assert(table.capacity == 0,
                     "init_zero: capacity == 0");
    __CPROVER_assert(table.first_free == UINT32_MAX,
                     "init_zero: first_free == UINT32_MAX");
    __CPROVER_assert(table.live_count == 0,
                     "init_zero: live_count == 0");
    __CPROVER_assert(table.initialized == 1,
                     "init_zero: initialized == 1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Acquire / resolve lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_acquire_resolve
 *
 * 1. Acquire a handle with a known kind and payload.
 * 2. Resolve with the same kind → payload matches.
 * 3. Resolve with KAIN_HANDLE_KIND_NONE (wildcard) → payload matches.
 * 4. Resolve with the wrong kind → NULL.
 * ────────────────────────────────────────────────────────────────────── */
void check_acquire_resolve(void) {
    KainHandleTable* table = create_valid_table();

    uint32_t kind;
    __CPROVER_havoc_object(&kind);
    __CPROVER_assume(kind == KAIN_HANDLE_KIND_FIXUP_OBJECT ||
                     kind == KAIN_HANDLE_KIND_PROFILE_ZONE);

    /* Acquire */
    KainRuntimeHandle h = kain_handle_table_acquire(table, kind, &payload_a);
    __CPROVER_assert(h != KAIN_RUNTIME_HANDLE_INVALID,
                     "acquire: succeeded");
    __CPROVER_assert(table->live_count > 0,
                     "acquire: live_count > 0");

    /* Resolve with same kind */
    void* resolved = kain_handle_table_resolve(table, h, kind);
    __CPROVER_assert(resolved == &payload_a,
                     "resolve: payload matches");

    /* Resolve with KAIN_HANDLE_KIND_NONE (wildcard — any kind) */
    resolved = kain_handle_table_resolve(table, h, KAIN_HANDLE_KIND_NONE);
    __CPROVER_assert(resolved == &payload_a,
                     "resolve: wildcard kind matches");

    /* Resolve with wrong kind */
    uint32_t wrong_kind = (kind == KAIN_HANDLE_KIND_FIXUP_OBJECT)
                              ? KAIN_HANDLE_KIND_PROFILE_ZONE
                              : KAIN_HANDLE_KIND_FIXUP_OBJECT;
    resolved = kain_handle_table_resolve(table, h, wrong_kind);
    __CPROVER_assert(resolved == NULL,
                     "resolve: wrong kind returns NULL");
}


/* ──────────────────────────────────────────────────────────────────────
 * check_acquire_full_table
 *
 * After acquiring all C slots, the next acquire returns INVALID.
 * ────────────────────────────────────────────────────────────────────── */
void check_acquire_full_table(void) {
    KainHandleTable* table = create_valid_table();

    for (uint32_t i = 0; i < table->capacity; ++i) {
        KainRuntimeHandle h = kain_handle_table_acquire(
            table, KAIN_HANDLE_KIND_FIXUP_OBJECT, &payload_a);
        __CPROVER_assert(h != KAIN_RUNTIME_HANDLE_INVALID,
                         "acquire_full: slot acquired");
    }

    /* Table should be full now */
    __CPROVER_assert(table->first_free == UINT32_MAX,
                     "acquire_full: first_free == UINT32_MAX");
    __CPROVER_assert(table->live_count == table->capacity,
                     "acquire_full: live_count == capacity");

    /* Next acquire fails */
    KainRuntimeHandle h = kain_handle_table_acquire(
        table, KAIN_HANDLE_KIND_FIXUP_OBJECT, &payload_a);
    __CPROVER_assert(h == KAIN_RUNTIME_HANDLE_INVALID,
                     "acquire_full: returns INVALID");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Release / re-acquire lifecycle
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_release_reacquire
 *
 * 1. Acquire a handle h1 at slot S with kind K.
 * 2. Release h1 → slot S is free, magic bumped.
 * 3. Resolving the old handle h1 → NULL (stale magic).
 * 4. Re-acquire → gets slot S with new handle h2 (new magic).
 * 5. Resolve h2 → correct (new) payload.
 * ────────────────────────────────────────────────────────────────────── */
void check_release_reacquire(void) {
    KainHandleTable* table = create_valid_table();

    uint32_t kind;
    __CPROVER_havoc_object(&kind);
    __CPROVER_assume(kind == KAIN_HANDLE_KIND_FIXUP_OBJECT ||
                     kind == KAIN_HANDLE_KIND_PROFILE_ZONE);

    /* Acquire */
    KainRuntimeHandle h1 = kain_handle_table_acquire(table, kind, &payload_a);
    __CPROVER_assert(h1 != KAIN_RUNTIME_HANDLE_INVALID,
                     "release_reacquire: first acquire succeeded");

    /* Resolve works before release */
    __CPROVER_assert(kain_handle_table_resolve(table, h1, kind) == &payload_a,
                     "release_reacquire: resolve before release");

    /* Release */
    int rc = kain_handle_table_release(table, h1, kind);
    __CPROVER_assert(rc == 0,
                     "release_reacquire: release succeeded");

    /* Old handle stale — resolve returns NULL */
    __CPROVER_assert(kain_handle_table_resolve(table, h1, kind) == NULL,
                     "release_reacquire: stale handle -> NULL");

    /* Re-acquire — same slot, new magic */
    KainRuntimeHandle h2 = kain_handle_table_acquire(table, kind, &payload_b);
    __CPROVER_assert(h2 != KAIN_RUNTIME_HANDLE_INVALID,
                     "release_reacquire: reacquire succeeded");

    /* New handle resolves correctly */
    __CPROVER_assert(kain_handle_table_resolve(table, h2, kind) == &payload_b,
                     "release_reacquire: reacquire payload matches");

    /* Stale handle still fails */
    __CPROVER_assert(kain_handle_table_resolve(table, h1, kind) == NULL,
                     "release_reacquire: stale handle still fails");

    /* Live count reflects one live handle */
    __CPROVER_assert(table->live_count == 1,
                     "release_reacquire: live_count == 1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Rebind
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_rebind
 *
 * 1. Acquire a handle → payload_a.
 * 2. Rebind → payload_b.  Resolve returns payload_b.
 * 3. Rebind with wrong kind → -1.  Payload stays payload_b.
 * ────────────────────────────────────────────────────────────────────── */
void check_rebind(void) {
    KainHandleTable* table = create_valid_table();

    uint32_t kind;
    __CPROVER_havoc_object(&kind);
    __CPROVER_assume(kind == KAIN_HANDLE_KIND_FIXUP_OBJECT ||
                     kind == KAIN_HANDLE_KIND_PROFILE_ZONE);

    KainRuntimeHandle h = kain_handle_table_acquire(table, kind, &payload_a);
    __CPROVER_assert(h != KAIN_RUNTIME_HANDLE_INVALID,
                     "rebind: acquire succeeded");

    /* Rebind to new payload */
    int rc = kain_handle_table_rebind(table, h, kind, &payload_b);
    __CPROVER_assert(rc == 0,
                     "rebind: rebind succeeded");
    __CPROVER_assert(kain_handle_table_resolve(table, h, kind) == &payload_b,
                     "rebind: payload changed");

    /* Rebind with wrong kind fails */
    uint32_t wrong_kind = (kind == KAIN_HANDLE_KIND_FIXUP_OBJECT)
                              ? KAIN_HANDLE_KIND_PROFILE_ZONE
                              : KAIN_HANDLE_KIND_FIXUP_OBJECT;
    rc = kain_handle_table_rebind(table, h, wrong_kind, &payload_c);
    __CPROVER_assert(rc == -1,
                     "rebind: wrong kind returns -1");

    /* Payload unchanged */
    __CPROVER_assert(kain_handle_table_resolve(table, h, kind) == &payload_b,
                     "rebind: payload unchanged after failed rebind");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Wrong-kind release
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_release_wrong_kind
 *
 * Acquire with FIXUP_OBJECT → release with PROFILE_ZONE fails.
 * Handle remains live.
 * ────────────────────────────────────────────────────────────────────── */
void check_release_wrong_kind(void) {
    KainHandleTable* table = create_valid_table();

    KainRuntimeHandle h = kain_handle_table_acquire(
        table, KAIN_HANDLE_KIND_FIXUP_OBJECT, &payload_a);
    __CPROVER_assert(h != KAIN_RUNTIME_HANDLE_INVALID,
                     "release_wrong_kind: acquire succeeded");

    int rc = kain_handle_table_release(
        table, h, KAIN_HANDLE_KIND_PROFILE_ZONE);
    __CPROVER_assert(rc == -1,
                     "release_wrong_kind: wrong kind returns -1");

    /* Handle still live */
    __CPROVER_assert(kain_handle_table_resolve(
                         table, h, KAIN_HANDLE_KIND_FIXUP_OBJECT) == &payload_a,
                     "release_wrong_kind: payload still valid");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Invalid handle (KAIN_RUNTIME_HANDLE_INVALID)
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_invalid_handle
 *
 * Resolve/release/rebind of INVALID handle gracefully returns NULL/-1.
 * ────────────────────────────────────────────────────────────────────── */
void check_invalid_handle(void) {
    KainHandleTable* table = create_valid_table();

    __CPROVER_assert(
        kain_handle_table_resolve(table, KAIN_RUNTIME_HANDLE_INVALID,
                                  KAIN_HANDLE_KIND_NONE) == NULL,
        "invalid_handle: resolve returns NULL");

    __CPROVER_assert(
        kain_handle_table_release(table, KAIN_RUNTIME_HANDLE_INVALID,
                                  KAIN_HANDLE_KIND_NONE) == -1,
        "invalid_handle: release returns -1");

    __CPROVER_assert(
        kain_handle_table_rebind(table, KAIN_RUNTIME_HANDLE_INVALID,
                                 KAIN_HANDLE_KIND_NONE, &payload_a) == -1,
        "invalid_handle: rebind returns -1");
}


/* ═══════════════════════════════════════════════════════════════════════
 * NULL safety
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_null_table
 *
 * All table functions accept NULL table gracefully.  Additionally,
 * init with NULL slots produces a valid-but-inert table.
 * ────────────────────────────────────────────────────────────────────── */
void check_null_table(void) {
    /* Init NULL table */
    kain_handle_table_init(NULL, slot_array, 4);
    __CPROVER_assert(1, "null_table: init(NULL) no crash");

    /* Acquire NULL table */
    __CPROVER_assert(
        kain_handle_table_acquire(NULL, KAIN_HANDLE_KIND_FIXUP_OBJECT,
                                  &payload_a) == KAIN_RUNTIME_HANDLE_INVALID,
        "null_table: acquire(NULL) returns INVALID");

    /* Resolve NULL table */
    __CPROVER_assert(
        kain_handle_table_resolve(NULL, 123, KAIN_HANDLE_KIND_NONE) == NULL,
        "null_table: resolve(NULL) returns NULL");

    /* Release NULL table */
    __CPROVER_assert(
        kain_handle_table_release(NULL, 123, KAIN_HANDLE_KIND_NONE) == -1,
        "null_table: release(NULL) returns -1");

    /* Rebind NULL table */
    __CPROVER_assert(
        kain_handle_table_rebind(NULL, 123, KAIN_HANDLE_KIND_NONE,
                                 &payload_a) == -1,
        "null_table: rebind(NULL) returns -1");

    /* Init with NULL slots (table is valid, but slots pointer is NULL) */
    {
        KainHandleTable table;
        __CPROVER_havoc_object(&table);
        kain_handle_table_init(&table, NULL, 4);
        __CPROVER_assert(table.slots == NULL,
                         "null_table: NULL slots pointer");
        __CPROVER_assert(table.initialized == 1,
                         "null_table: initialized still 1");
        __CPROVER_assert(table.capacity == 4,
                         "null_table: capacity set");

        /* Acquire on a table with NULL slots fails */
        __CPROVER_assert(
            kain_handle_table_acquire(&table, KAIN_HANDLE_KIND_FIXUP_OBJECT,
                                      &payload_a) == KAIN_RUNTIME_HANDLE_INVALID,
            "null_table: acquire on NULL-slots table returns INVALID");
    }
}


/* ═══════════════════════════════════════════════════════════════════════
 * Multiple handles — uniqueness & live_count
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_multiple_acquire_unique
 *
 * Three acquires produce three valid handles with distinct slots.
 * Each resolves to the correct payload.
 * ────────────────────────────────────────────────────────────────────── */
void check_multiple_acquire_unique(void) {
    /* Need at least 3 slots for 3 handles */
    KainHandleTable* table = create_valid_table_min(3);

    KainRuntimeHandle h1 = kain_handle_table_acquire(
        table, KAIN_HANDLE_KIND_FIXUP_OBJECT, &payload_a);
    KainRuntimeHandle h2 = kain_handle_table_acquire(
        table, KAIN_HANDLE_KIND_PROFILE_ZONE, &payload_b);
    KainRuntimeHandle h3 = kain_handle_table_acquire(
        table, KAIN_HANDLE_KIND_FIXUP_OBJECT, &payload_c);

    /* All valid */
    __CPROVER_assert(h1 != KAIN_RUNTIME_HANDLE_INVALID,
                     "multiple: h1 valid");
    __CPROVER_assert(h2 != KAIN_RUNTIME_HANDLE_INVALID,
                     "multiple: h2 valid");
    __CPROVER_assert(h3 != KAIN_RUNTIME_HANDLE_INVALID,
                     "multiple: h3 valid");

    /* Distinct encoded slot values */
    uint32_t s1 = kain_handle_slot(h1);
    uint32_t s2 = kain_handle_slot(h2);
    uint32_t s3 = kain_handle_slot(h3);
    __CPROVER_assert(s1 != s2 || s1 != s3 || s2 != s3,
                     "multiple: slots are distinct");

    /* Each resolves to its own payload */
    __CPROVER_assert(
        kain_handle_table_resolve(table, h1, KAIN_HANDLE_KIND_FIXUP_OBJECT)
            == &payload_a,
        "multiple: h1 -> payload_a");
    __CPROVER_assert(
        kain_handle_table_resolve(table, h2, KAIN_HANDLE_KIND_PROFILE_ZONE)
            == &payload_b,
        "multiple: h2 -> payload_b");
    __CPROVER_assert(
        kain_handle_table_resolve(table, h3, KAIN_HANDLE_KIND_FIXUP_OBJECT)
            == &payload_c,
        "multiple: h3 -> payload_c");

    __CPROVER_assert(table->live_count == 3,
                     "multiple: live_count == 3");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Double-free protection
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_release_double_free
 *
 * First release succeeds (live_count → 0).
 * Second release on the same (now-stale) handle returns -1.
 * ────────────────────────────────────────────────────────────────────── */
void check_release_double_free(void) {
    KainHandleTable* table = create_valid_table();

    KainRuntimeHandle h = kain_handle_table_acquire(
        table, KAIN_HANDLE_KIND_FIXUP_OBJECT, &payload_a);
    __CPROVER_assert(h != KAIN_RUNTIME_HANDLE_INVALID,
                     "double_free: acquire succeeded");
    __CPROVER_assert(table->live_count == 1,
                     "double_free: live_count == 1");

    /* First release */
    int rc = kain_handle_table_release(table, h, KAIN_HANDLE_KIND_FIXUP_OBJECT);
    __CPROVER_assert(rc == 0,
                     "double_free: first release succeeded");
    __CPROVER_assert(table->live_count == 0,
                     "double_free: live_count == 0 after release");

    /* Second release (stale magic) */
    rc = kain_handle_table_release(table, h, KAIN_HANDLE_KIND_FIXUP_OBJECT);
    __CPROVER_assert(rc == -1,
                     "double_free: second release returns -1");
    __CPROVER_assert(table->live_count == 0,
                     "double_free: live_count unchanged after failed release");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Zero-capacity table
 * ═══════════════════════════════════════════════════════════════════════ */

/* ──────────────────────────────────────────────────────────────────────
 * check_zero_capacity_table
 *
 * A zero-capacity table has first_free == UINT32_MAX, so acquire
 * returns INVALID.  Resolve of INVALID handle returns NULL.
 * ────────────────────────────────────────────────────────────────────── */
void check_zero_capacity_table(void) {
    KainHandleTable table;
    __CPROVER_havoc_object(&table);
    kain_handle_table_init(&table, slot_array, 0);

    __CPROVER_assert(
        kain_handle_table_acquire(&table, KAIN_HANDLE_KIND_FIXUP_OBJECT,
                                  &payload_a) == KAIN_RUNTIME_HANDLE_INVALID,
        "zero_cap: acquire returns INVALID");

    __CPROVER_assert(
        kain_handle_table_resolve(&table, KAIN_RUNTIME_HANDLE_INVALID,
                                  KAIN_HANDLE_KIND_NONE) == NULL,
        "zero_cap: resolve returns NULL");
}


/* ═══════════════════════════════════════════════════════════════════════
 * Main — run all checks
 * ═══════════════════════════════════════════════════════════════════════ */
int main(void) {
    check_handle_make_roundtrip();
    check_handle_slot_invalid();
    check_handle_make_zero();
    check_table_init();
    check_table_init_null();
    check_table_init_zero_capacity();
    check_acquire_resolve();
    check_acquire_full_table();
    check_release_reacquire();
    check_rebind();
    check_release_wrong_kind();
    check_invalid_handle();
    check_null_table();
    check_multiple_acquire_unique();
    check_release_double_free();
    check_zero_capacity_table();
    return 0;
}
