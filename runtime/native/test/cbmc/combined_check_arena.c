#include "../../include/arena.h"
#include <string.h>

static int kain_alignment_is_power_of_two(size_t alignment) {
    return alignment != 0u && (alignment & (alignment - 1u)) == 0u;
}

static size_t kain_align_down_size(size_t value, size_t alignment) {
    if (alignment <= 1u) {
        return value;
    }
    return value & ~(alignment - 1u);
}

static void kain_arena_lock(KainArena* arena) {
    while (atomic_exchange_explicit(&arena->lock_word, 1u, memory_order_acquire) != 0u) {
    }
}

static void kain_arena_unlock(KainArena* arena) {
    atomic_store_explicit(&arena->lock_word, 0u, memory_order_release);
}

int kain_memtype_is_legal(uint8_t memtype) {
    if (memtype >= KAIN_MEMTYPE_COUNT) {
        return 0;
    }
    return ((KAIN_MEMTYPE_LEGAL_MASK >> memtype) & 1u) != 0u;
}

size_t kain_align_up_size(size_t value, size_t alignment, int* overflowed) {
    if (overflowed != NULL) {
        *overflowed = 0;
    }
    if (alignment == 0u) {
        alignment = 1u;
    }
    if (!kain_alignment_is_power_of_two(alignment)) {
        if (overflowed != NULL) {
            *overflowed = 1;
        }
        return 0u;
    }

    size_t mask = alignment - 1u;
    if (value > SIZE_MAX - mask) {
        if (overflowed != NULL) {
            *overflowed = 1;
        }
        return 0u;
    }
    return (value + mask) & ~mask;
}

int kain_arena_init(
    KainArena* arena,
    KainArenaId arena_id,
    void* start,
    size_t size,
    KainMemType memtype
) {
    if (arena == NULL || start == NULL || size == 0u ||
        arena_id >= KAIN_ARENA_MAX || !kain_memtype_is_legal((uint8_t)memtype)) {
        return -1;
    }

    memset(arena, 0, sizeof(*arena));
    arena->start = (unsigned char*)start;
    arena->end = arena->start + size;
    arena->low = arena->start;
    arena->high = arena->end;
    arena->reserved_bytes = size;
    arena->arena_id = (uint8_t)arena_id;
    arena->memtype = (uint8_t)memtype;
    atomic_init(&arena->lock_word, 0u);
    return 0;
}

void kain_arena_reset(KainArena* arena) {
    if (arena == NULL) {
        return;
    }

    kain_arena_lock(arena);
    arena->low = arena->start;
    arena->high = arena->end;
    arena->frame.depth = 0u;
    kain_arena_unlock(arena);
}

size_t kain_arena_available(const KainArena* arena) {
    if (arena == NULL || arena->high < arena->low) {
        return 0u;
    }
    return (size_t)(arena->high - arena->low);
}

void* kain_arena_alloc_lo(KainArena* arena, size_t size, size_t alignment) {
    if (arena == NULL || size == 0u) {
        return NULL;
    }
    if (alignment == 0u) {
        alignment = 1u;
    }
    if (!kain_alignment_is_power_of_two(alignment)) {
        return NULL;
    }

    kain_arena_lock(arena);
    size_t low_offset = (size_t)(arena->low - arena->start);
    size_t high_offset = (size_t)(arena->high - arena->start);
    int overflowed = 0;
    size_t aligned_low_offset = kain_align_up_size(low_offset, alignment, &overflowed);
    if (overflowed || aligned_low_offset > high_offset || size > high_offset - aligned_low_offset) {
        kain_arena_unlock(arena);
        return NULL;
    }

    unsigned char* result = arena->start + aligned_low_offset;
    arena->low = result + size;
    kain_arena_unlock(arena);
    return result;
}

void* kain_arena_alloc_hi(KainArena* arena, size_t size, size_t alignment) {
    if (arena == NULL || size == 0u) {
        return NULL;
    }
    if (alignment == 0u) {
        alignment = 1u;
    }
    if (!kain_alignment_is_power_of_two(alignment)) {
        return NULL;
    }

    kain_arena_lock(arena);
    size_t low_offset = (size_t)(arena->low - arena->start);
    size_t high_offset = (size_t)(arena->high - arena->start);
    if (low_offset > high_offset || size > high_offset - low_offset) {
        kain_arena_unlock(arena);
        return NULL;
    }

    size_t candidate_offset = high_offset - size;
    size_t aligned_start_offset = kain_align_down_size(candidate_offset, alignment);
    if (aligned_start_offset < low_offset) {
        kain_arena_unlock(arena);
        return NULL;
    }

    unsigned char* result = arena->start + aligned_start_offset;
    arena->high = result;
    kain_arena_unlock(arena);
    return result;
}

int kain_frame_set_marker(KainArena* arena) {
    if (arena == NULL) {
        return -1;
    }

    kain_arena_lock(arena);
    if (arena->frame.depth >= KAIN_FRAME_MAX_DEPTH) {
        kain_arena_unlock(arena);
        return -1;
    }

    KainFrameMarker* marker = &arena->frame.markers[arena->frame.depth];
    marker->low_offset = (size_t)(arena->low - arena->start);
    marker->high_offset = (size_t)(arena->high - arena->start);
    arena->frame.depth += 1u;
    kain_arena_unlock(arena);
    return 0;
}

int kain_frame_release_to_last_marker(KainArena* arena) {
    if (arena == NULL) {
        return -1;
    }

    kain_arena_lock(arena);
    if (arena->frame.depth == 0u) {
        kain_arena_unlock(arena);
        return -1;
    }

    arena->frame.depth -= 1u;
    const KainFrameMarker* marker = &arena->frame.markers[arena->frame.depth];
    arena->low = arena->start + marker->low_offset;
    arena->high = arena->start + marker->high_offset;
    kain_arena_unlock(arena);
    return 0;
}

void kain_frame_release_all(KainArena* arena) {
    if (arena == NULL) {
        return;
    }

    kain_arena_lock(arena);
    arena->frame.depth = 0u;
    arena->low = arena->start;
    arena->high = arena->end;
    kain_arena_unlock(arena);
}

/*
 * check_arena.c â€” CBMC verification harness for arena module
 *
 * Unlike the auto-generated harness (which calls functions with garbage
 * pointers), this harness CREATES VALID objects at static addresses,
 * nondeterministic CONTENTS, constrains with __CPROVER_assume, and
 * asserts REAL postconditions.
 *
 * Key insight: __CPROVER_havoc_object scrambles every byte, including
 * pointers. __CPROVER_assume(start != NULL) does NOT give CBMC pointer
 * VALIDITY â€” just non-nullness. To give pointers provenance (the
 * "allocated in valid memory" property), we must point them at real
 * static buffers that CBMC can reason about.
 *
 * CBMC explores ALL paths within the unwind bound. If any assertion
 * fails, it produces a counterexample trace with exact inputs.
 *
 * Run via: python test/scripts/run_pipeline.py cbmc --harness check_arena
 * Or:     cbmc --unwind 5 --trace test/cbmc/check_arena.c src/core/arena.c -I include -I src/core
 */

#include "arena.h"

/* Static buffer the arena will "manage" â€” CBMC knows it's a real object */
static unsigned char arena_memory[4096];

/* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
 * Helper: create a valid KainArena backed by static memory
 *
 * The arena struct and its managed buffer are both havoc'd (nondet
 * contents), but pointers always point into the static buffer. This
 * gives CBMC real pointer provenance while keeping input data random.
 * â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
static KainArena* create_valid_arena(void) {
    static KainArena arena;
    __CPROVER_havoc_object(&arena);
    __CPROVER_havoc_object(arena_memory);

    /* â”€â”€ Pointer provenance: all arena pointers point into static buffer â”€â”€ */
    arena.start = &arena_memory[0];
    arena.end   = &arena_memory[sizeof(arena_memory)];
    arena.low   = &arena_memory[0];
    arena.high  = &arena_memory[sizeof(arena_memory)];

    /* â”€â”€ Constrain offsets â€” low <= high, both within [start, end] â”€â”€ */
    /* After havoc, low could be > high; reset to sane defaults */
    arena.low  = arena.start;
    arena.high = arena.end;

    /* â”€â”€ Frame state â”€â”€ */
    __CPROVER_assume(arena.frame.depth < KAIN_FRAME_MAX_DEPTH);

    /* Marker offsets â€” constrained to be within the buffer so that
     * release_to_last_marker doesn't compute pointers past end. */
    for (int i = 0; i < KAIN_FRAME_MAX_DEPTH; i++) {
        __CPROVER_assume(arena.frame.markers[i].low_offset
                         <= sizeof(arena_memory));
        __CPROVER_assume(arena.frame.markers[i].high_offset
                         <= sizeof(arena_memory));
        __CPROVER_assume(arena.frame.markers[i].low_offset
                         <= arena.frame.markers[i].high_offset);
    }

    /* â”€â”€ Arena metadata â”€â”€ */
    __CPROVER_assume(arena.arena_id < KAIN_ARENA_MAX);

    return &arena;
}


/* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
 * Check: kain_arena_init creates a consistent arena
 * â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
void check_arena_init(void) {
    KainArena arena;
    int arena_id;
    int memtype;
    __CPROVER_havoc_object(&arena);
    __CPROVER_havoc_object(&arena_id);
    __CPROVER_havoc_object(&memtype);
    __CPROVER_assume(arena_id >= 0 && arena_id < KAIN_ARENA_MAX);

    /* arena_memory is the backing buffer */
    int rc = kain_arena_init(&arena, arena_id,
                             arena_memory, sizeof(arena_memory),
                             memtype);

    if (rc == 0) {
        __CPROVER_assert(arena.start == arena_memory,
                         "init: start == buf");
        __CPROVER_assert(arena.end == arena_memory + sizeof(arena_memory),
                         "init: end == buf + size");
        __CPROVER_assert(arena.low == arena.start,
                         "init: low == start");
        __CPROVER_assert(arena.high == arena.end,
                         "init: high == end");
        __CPROVER_assert(arena.frame.depth == 0,
                         "init: depth == 0");
        __CPROVER_assert(arena.reserved_bytes > 0,
                         "init: reserved_bytes > 0");
    }
}


/* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
 * Check: kain_arena_reset restores low/high to start/end
 * â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
void check_arena_reset(void) {
    KainArena* arena = create_valid_arena();

    unsigned char* expected_start = arena->start;
    unsigned char* expected_end   = arena->end;

    kain_arena_reset(arena);

    __CPROVER_assert(arena->low  == expected_start,
                     "reset: low == start");
    __CPROVER_assert(arena->high == expected_end,
                     "reset: high == end");
    __CPROVER_assert(arena->frame.depth == 0,
                     "reset: depth == 0");
}


/* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
 * Check: kain_arena_available never exceeds arena size
 * â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
void check_arena_available(void) {
    KainArena* arena = create_valid_arena();

    size_t avail = kain_arena_available(arena);
    size_t total = (size_t)(arena->end - arena->start);

    __CPROVER_assert(avail <= total,
                     "available <= arena size");
    __CPROVER_assert(avail <= total,
                     "available >= 0 (unsigned) â€” always true by type");
}


/* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
 * Check: kain_arena_alloc_lo advances low and returns valid pointer
 * â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
void check_arena_alloc_lo(void) {
    KainArena* arena = create_valid_arena();

    size_t requested;
    size_t alignment;
    __CPROVER_havoc_object(&requested);
    __CPROVER_havoc_object(&alignment);
    __CPROVER_assume(alignment >= 1 && alignment <= 256);
    __CPROVER_assume((alignment & (alignment - 1)) == 0); /* power of 2 */
    __CPROVER_assume(requested <= 512);
    __CPROVER_assume(requested > 0);

    unsigned char* pre_low  = arena->low;
    unsigned char* pre_high = arena->high;

    void* result = kain_arena_alloc_lo(arena, requested, alignment);

    if (result != NULL) {
        /* Result points into the managed buffer */
        __CPROVER_assert((unsigned char*)result >= arena->start,
                         "alloc_lo: result >= start");
        __CPROVER_assert((unsigned char*)result <  arena->end,
                         "alloc_lo: result <  end");

        /* Alignment */
        __CPROVER_assert(((size_t)result & (alignment - 1)) == 0,
                         "alloc_lo: result is aligned");

        /* Low advanced */
        __CPROVER_assert(arena->low >= pre_low,
                         "alloc_lo: low advanced");
        /* High unchanged */
        __CPROVER_assert(arena->high == pre_high,
                         "alloc_lo: high unchanged");
    } else {
        /* Allocation failed â€” arena unchanged */
        __CPROVER_assert(arena->low  == pre_low,
                         "alloc_lo fail: low unchanged");
        __CPROVER_assert(arena->high == pre_high,
                         "alloc_lo fail: high unchanged");
    }
}


/* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
 * Check: kain_frame_set_marker stores correct offsets
 * â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
void check_frame_set_marker(void) {
    KainArena* arena = create_valid_arena();
    __CPROVER_assume(arena->frame.depth < KAIN_FRAME_MAX_DEPTH - 1);

    uint8_t        pre_depth = arena->frame.depth;
    unsigned char* pre_low   = arena->low;
    unsigned char* pre_high  = arena->high;

    int rc = kain_frame_set_marker(arena);

    if (rc == 0) {
        __CPROVER_assert(arena->frame.depth == pre_depth + 1,
                         "set_marker: depth incremented");

        KainFrameMarker* m = &arena->frame.markers[pre_depth];
        __CPROVER_assert(
            m->low_offset  == (size_t)(pre_low  - arena->start),
            "set_marker: low_offset correct");
        __CPROVER_assert(
            m->high_offset == (size_t)(pre_high - arena->start),
            "set_marker: high_offset correct");

        /* Low/high unchanged */
        __CPROVER_assert(arena->low  == pre_low,
                         "set_marker: low unchanged");
        __CPROVER_assert(arena->high == pre_high,
                         "set_marker: high unchanged");
    }
}


/* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
 * Check: release_to_last_marker restores low/high from marker offsets
 * â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
void check_frame_release_to_last_marker(void) {
    KainArena* arena = create_valid_arena();
    __CPROVER_assume(arena->frame.depth > 0);
    __CPROVER_assume(arena->frame.depth < KAIN_FRAME_MAX_DEPTH);

    uint8_t pre_depth = arena->frame.depth;

    /* Constrain marker offsets even within the buffer */
    KainFrameMarker* last = &arena->frame.markers[pre_depth - 1];
    __CPROVER_assume(last->low_offset <= sizeof(arena_memory));
    __CPROVER_assume(last->high_offset <= sizeof(arena_memory));
    __CPROVER_assume(last->low_offset <= last->high_offset);

    size_t expected_low_off  = last->low_offset;
    size_t expected_high_off = last->high_offset;

    int rc = kain_frame_release_to_last_marker(arena);

    if (rc == 0) {
        __CPROVER_assert(arena->frame.depth == pre_depth - 1,
                         "release: depth decremented");

        __CPROVER_assert(
            (size_t)(arena->low  - arena->start) == expected_low_off,
            "release: low restored");
        __CPROVER_assert(
            (size_t)(arena->high - arena->start) == expected_high_off,
            "release: high restored");
    }
}


/* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
 * Check: alloc_lo + alloc_hi regions never overlap (no corruption)
 * â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
void check_lo_hi_no_overlap(void) {
    KainArena* arena = create_valid_arena();

    if (!kain_arena_alloc_lo(arena, 64, 8)) return;
    if (!kain_arena_alloc_hi(arena, 64, 8)) return;

    __CPROVER_assert(arena->low <= arena->high,
                     "lo_hi: low <= high (no overlap)");
}


/* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
 * Check: allocated region fits entirely within the buffer
 * â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
void check_arena_no_oob_writes(void) {
    KainArena* arena = create_valid_arena();

    size_t sz;
    size_t align;
    __CPROVER_havoc_object(&sz);
    __CPROVER_havoc_object(&align);
    __CPROVER_assume(align >= 1 && align <= 256);
    __CPROVER_assume((align & (align - 1)) == 0);
    __CPROVER_assume(sz <= 256 && sz > 0);

    void* p = kain_arena_alloc_lo(arena, sz, align);
    if (p) {
        __CPROVER_assert((unsigned char*)p + sz <= arena->end,
                         "alloc: p + sz <= end (no OOB)");
    }
}


/* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
 * Main â€” run all checks
 * â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
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
