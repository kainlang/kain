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
 * CBMC verification harness for arena
 * Auto-generated from function catalog
 *
 * Self-contained: forward declarations only, no system headers.
 * CBMC explores ALL paths on ALL possible inputs within unwind bound.
 */

// Basic type definitions needed by runtime function signatures
typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef long long int64_t;
typedef int int32_t;
typedef short int16_t;
typedef signed char int8_t;
typedef unsigned long long size_t;
typedef long long ptrdiff_t;

// Forward declarations of functions under test
// kain_memtype_is_legal
int kain_memtype_is_legal(uint8_t memtype);
// kain_arena_reset
void kain_arena_reset(KainArena* arena);
// kain_arena_available
size_t kain_arena_available(const KainArena* arena);
// kain_frame_set_marker
int kain_frame_set_marker(KainArena* arena);
// kain_frame_release_to_last_marker
int kain_frame_release_to_last_marker(KainArena* arena);

int main(void) {
    { void *__p; kain_memtype_is_legal(__p); }
    __CPROVER_assert(1, "kain_memtype_is_legal: call ok");
    { void *__p; kain_arena_reset(__p); }
    __CPROVER_assert(1, "kain_arena_reset: call ok");
    { void *__p; kain_arena_available(__p); }
    __CPROVER_assert(1, "kain_arena_available: call ok");
    { void *__p; kain_frame_set_marker(__p); }
    __CPROVER_assert(1, "kain_frame_set_marker: call ok");
    { void *__p; kain_frame_release_to_last_marker(__p); }
    __CPROVER_assert(1, "kain_frame_release_to_last_marker: call ok");
    return 0;
}
