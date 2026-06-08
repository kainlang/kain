/*
 * check_arena.c — CBMC verification harness for arena module
 *
 * Unlike the auto-generated harness (which calls functions with garbage
 * pointers), this harness CREATES VALID objects with __CPROVER_havoc_object,
 * constrains them with __CPROVER_assume, and asserts REAL postconditions.
 *
 * CBMC explores ALL paths within the unwind bound. If any assertion
 * fails, it produces a counterexample trace with the exact inputs.
 *
 * Run via: python test/scripts/run_pipeline.py cbmc --harness check_arena
 * Or:     cbmc --unwind 5 --trace test/cbmc/check_arena.c src/core/arena.c -I include -I src/core
 */

#include "arena.h"

/* ──────────────────────────────────────────────────────────────────────
 * Helper: create a valid nondet KainArena with constrained pointers
 * ────────────────────────────────────────────────────────────────────── */
static KainArena* create_valid_arena(void) {
    /* Allocate a nondet KainArena object — CBMC picks symbolic values */
    static KainArena arena;
    __CPROVER_havoc_object(&arena);

    /* Constrain: start < end and both are valid pointers */
    __CPROVER_assume(arena.start != NULL);
    __CPROVER_assume(arena.end != NULL);
    __CPROVER_assume(arena.start < arena.end);

    /* Constrain: low/high within [start, end] */
    __CPROVER_assume(arena.low >= arena.start);
    __CPROVER_assume(arena.low <= arena.end);
    __CPROVER_assume(arena.high >= arena.start);
    __CPROVER_assume(arena.high <= arena.end);

    /* Constrain: frame metadata in valid range */
    __CPROVER_assume(arena.frame.depth < KAIN_FRAME_MAX_DEPTH);

    /* Constrain: arena_id is valid */
    __CPROVER_assume(arena.arena_id < KAIN_ARENA_MAX);

    return &arena;
}

/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_arena_init creates a consistent arena
 * ────────────────────────────────────────────────────────────────────── */
void check_arena_init(void) {
    KainArena arena;
    unsigned char buf[4096];
    int arena_id;
    int memtype;

    /* Create nondet but valid inputs */
    __CPROVER_havoc_object(&arena);
    __CPROVER_havoc_object(buf);
    __CPROVER_havoc_object(&arena_id);
    __CPROVER_havoc_object(&memtype);

    /* Constrain inputs to valid ranges */
    __CPROVER_assume(arena_id >= 0 && arena_id < KAIN_ARENA_MAX);
    __CPROVER_assume(buf != NULL);

    int rc = kain_arena_init(&arena, arena_id, buf, sizeof(buf), memtype);

    /* Postconditions */
    if (rc == 0) {
        __CPROVER_assert(arena.start == buf,      "arena.start == buf");
        __CPROVER_assert(arena.end == buf + sizeof(buf), "arena.end == buf + size");
        __CPROVER_assert(arena.low == arena.start, "arena.low == arena.start after init");
        __CPROVER_assert(arena.high == arena.end,  "arena.high == arena.end after init");
        __CPROVER_assert(arena.frame.depth == 0,   "frame depth == 0 after init");
        __CPROVER_assert(arena.reserved_bytes == 0,"no reserved bytes after init");
    }
}

/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_arena_reset restores arena to initialized state
 * ────────────────────────────────────────────────────────────────────── */
void check_arena_reset(void) {
    KainArena* arena = create_valid_arena();
    if (!arena) return;

    /* Record pre-state */
    unsigned char* expected_start = arena->start;
    unsigned char* expected_end = arena->end;

    kain_arena_reset(arena);

    /* Postconditions */
    __CPROVER_assert(arena->low == expected_start,  "reset: low == start");
    __CPROVER_assert(arena->high == expected_end,   "reset: high == end");
    __CPROVER_assert(arena->frame.depth == 0,       "reset: depth == 0");
}

/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_arena_available never returns garbage
 * ────────────────────────────────────────────────────────────────────── */
void check_arena_available(void) {
    KainArena* arena = create_valid_arena();
    if (!arena) return;

    size_t avail = kain_arena_available(arena);

    /* Must not exceed total arena size */
    size_t total = (size_t)(arena->end - arena->start);
    __CPROVER_assert(avail <= total, "available <= arena size");

    /* Must be a multiple of alignment (no weird partial values) */
    /* Available space = needed when low and high are valid */
    if (arena->high >= arena->low) {
        size_t computed = (size_t)(arena->high - arena->low);
        __CPROVER_assert(avail == computed, "available == high - low when valid");
    } else {
        __CPROVER_assert(avail == 0, "available == 0 when high < low");
    }
}

/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_arena_alloc_lo advances low and returns valid pointer
 * ────────────────────────────────────────────────────────────────────── */
void check_arena_alloc_lo(void) {
    KainArena* arena = create_valid_arena();
    if (!arena) return;

    size_t requested;
    size_t alignment;
    __CPROVER_havoc_object(&requested);
    __CPROVER_havoc_object(&alignment);

    /* Constrain alignment to power of 2 */
    __CPROVER_assume(alignment >= 1);
    __CPROVER_assume(alignment <= 256);
    __CPROVER_assume((alignment & (alignment - 1)) == 0);  /* power of 2 */

    /* Constrain size to something reasonable */
    __CPROVER_assume(requested <= 1024);

    unsigned char* pre_low = arena->low;
    unsigned char* pre_high = arena->high;

    void* result = kain_arena_alloc_lo(arena, requested, alignment);

    if (result != NULL) {
        /* Allocation succeeded — result must be in [start, end) */
        __CPROVER_assert((unsigned char*)result >= arena->start, "alloc_lo: result >= start");
        __CPROVER_assert((unsigned char*)result < arena->end,    "alloc_lo: result < end");

        /* Alignment must be satisfied */
        __CPROVER_assert(((size_t)result & (alignment - 1)) == 0, "alloc_lo: result is aligned");

        /* Low must have advanced past the allocation */
        __CPROVER_assert(arena->low >= pre_low, "alloc_lo: low advanced");

        /* High must NOT have moved */
        __CPROVER_assert(arena->high == pre_high, "alloc_lo: high unchanged");
    } else {
        /* Allocation failed — arena unchanged */
        __CPROVER_assert(arena->low == pre_low,   "alloc_lo fail: low unchanged");
        __CPROVER_assert(arena->high == pre_high, "alloc_lo fail: high unchanged");
    }
}

/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_frame_set_marker creates a valid marker
 * ────────────────────────────────────────────────────────────────────── */
void check_frame_set_marker(void) {
    KainArena* arena = create_valid_arena();
    if (!arena) return;

    /* Constrain: not at max depth already */
    __CPROVER_assume(arena->frame.depth < KAIN_FRAME_MAX_DEPTH - 1);

    uint8_t pre_depth = arena->frame.depth;
    unsigned char* pre_low = arena->low;
    unsigned char* pre_high = arena->high;

    int rc = kain_frame_set_marker(arena);

    if (rc == 0) {
        /* Depth incremented by 1 */
        __CPROVER_assert(arena->frame.depth == pre_depth + 1,
                         "set_marker: depth incremented");

        /* Marker stored at pre_depth position */
        KainFrameMarker* m = &arena->frame.markers[pre_depth];
        __CPROVER_assert(m->low_offset  == (size_t)(pre_low  - arena->start),
                         "set_marker: low_offset correct");
        __CPROVER_assert(m->high_offset == (size_t)(pre_high - arena->start),
                         "set_marker: high_offset correct");

        /* Low/high unchanged */
        __CPROVER_assert(arena->low == pre_low,   "set_marker: low unchanged");
        __CPROVER_assert(arena->high == pre_high, "set_marker: high unchanged");
    }
}

/* ──────────────────────────────────────────────────────────────────────
 * Check: kain_frame_release_to_last_marker restores state
 * ────────────────────────────────────────────────────────────────────── */
void check_frame_release_to_last_marker(void) {
    KainArena* arena = create_valid_arena();
    if (!arena) return;

    /* Must have at least one marker */
    __CPROVER_assume(arena->frame.depth > 0);
    __CPROVER_assume(arena->frame.depth < KAIN_FRAME_MAX_DEPTH);

    uint8_t pre_depth = arena->frame.depth;
    KainFrameMarker* last = &arena->frame.markers[pre_depth - 1];
    size_t expected_low_off = last->low_offset;
    size_t expected_high_off = last->high_offset;

    int rc = kain_frame_release_to_last_marker(arena);

    if (rc == 0) {
        /* Depth decremented */
        __CPROVER_assert(arena->frame.depth == pre_depth - 1,
                         "release: depth decremented");

        /* State restored to marker's recorded offsets */
        __CPROVER_assert((size_t)(arena->low  - arena->start) == expected_low_off,
                         "release: low restored");
        __CPROVER_assert((size_t)(arena->high - arena->start) == expected_high_off,
                         "release: high restored");
    }
}

/* ──────────────────────────────────────────────────────────────────────
 * Check: alloc_lo then alloc_hi regions don't overlap (no corruption)
 * ────────────────────────────────────────────────────────────────────── */
void check_lo_hi_no_overlap(void) {
    KainArena* arena = create_valid_arena();
    if (!arena) return;

    if (!kain_arena_alloc_lo(arena, 64, 8)) return;
    if (!kain_arena_alloc_hi(arena, 64, 8)) return;

    /* After lo allocation and hi allocation, low must be <= high */
    __CPROVER_assert(arena->low <= arena->high,
                     "lo_hi: low <= high (no overlap)");
}

/* ──────────────────────────────────────────────────────────────────────
 * Check:  arena never writes past end
 * ────────────────────────────────────────────────────────────────────── */
void check_arena_no_oob_writes(void) {
    KainArena* arena = create_valid_arena();
    if (!arena) return;

    size_t sz;
    size_t align;
    __CPROVER_havoc_object(&sz);
    __CPROVER_havoc_object(&align);
    __CPROVER_assume(align >= 1);
    __CPROVER_assume(align <= 256);
    __CPROVER_assume((align & (align - 1)) == 0);

    void* p = kain_arena_alloc_lo(arena, sz, align);
    if (p) {
        /* Allocated region must fit entirely within [start, end) */
        __CPROVER_assert((unsigned char*)p + sz <= arena->end,
                         "alloc: no out-of-bounds write");
    }
}

/* ──────────────────────────────────────────────────────────────────────
 * Main — run all checks
 * ────────────────────────────────────────────────────────────────────── */
int main(void) {
    check_arena_init();
    check_arena_reset();
    check_arena_available();
    check_arena_alloc_lo();
    check_frame_set_marker();
    check_frame_release_to_last_marker();
    check_lo_hi_no_overlap();
    check_arena_no_oob_writes();
    return 0;
}
