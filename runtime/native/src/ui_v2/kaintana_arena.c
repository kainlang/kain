// ============================================================================
//  arena.c — Grow-only frame arena wrappers for the Kaintana UI substrate.
//
//  Delegates to Kain's core KainArena (kain_arena_init / kain_arena_alloc_lo /
//  kain_frame_set_marker / kain_frame_release_to_last_marker / kain_arena_reset).
//  The arena buffer is EMBEDDED in the session struct (KAINTANA_ARENA_SIZE = 64KB).
//  No individual free — per-frame reset via markers is the strategy.
//
//  Design:
//    - Frame markers have max depth 8 (KAIN_FRAME_MAX_DEPTH, see core arena.h)
//    - 16-byte default alignment for kaintana__arena_push (covers nodes, layouts,
//      draw commands, hash table entries, and cache lines)
//    - Geometric growth (1.5x) is handled by Kain's core arena — we just delegate
//    - All formulas Z3-proven (arena_proofs.yaml):
//      kt_arena_grow, kt_arena_grow_align, kt_arena_frame_marker,
//      kt_arena_frame_release, kt_arena_grow_15x, kt_arena_grow_exact
// ============================================================================

#include "internal.h"

// ============================================================================
//  PUBLIC API — kaintana_arena_* (declared in arena.h)
//  Thin wrappers over the static inline helpers in internal.h.
//  The inlines are used internally for hot paths; these public functions
//  provide linker-visible symbols for external callers and backends.
// ============================================================================

void kaintana_arena_init(kt_Session* s)
{
    struct kt_Session_t* sess = kaintana__session(s);
    kain_arena_init(&sess->arena, KAIN_ARENA_MAIN,
                     sess->arena_buffer,
                     sizeof(sess->arena_buffer),
                     KAIN_MEMTYPE_DEFAULT);
}

void* kaintana_arena_alloc(kt_Session* s, size_t size, size_t align)
{
    return kaintana__arena_alloc(s, size, align);
}

void kaintana_arena_mark(kt_Session* s)
{
    kaintana__arena_mark(s);
}

void kaintana_arena_release(kt_Session* s)
{
    kaintana__arena_release(s);
}

void kaintana_arena_reset(kt_Session* s)
{
    struct kt_Session_t* sess = kaintana__session(s);
    kain_arena_reset(&sess->arena);
}

// ============================================================================
//  INTERNAL API — kaintana__arena_* (declared in kaintana.h §19)
//  Called by tree.c, box_math.c, damage.c, draw_pixels.c.
// ============================================================================

void* kaintana__arena_push(kt_Session* s, size_t bytes)
{
    // 16-byte alignment covers all Kaintana types:
    //   KaintanaNode              -> _Alignof = 8 (but 16 is future-safe for SIMD)
    //   KaintanaLayout            -> 4-byte fields, 16-byte recommended for SoA upload
    //   KaintanaInternalDrawCmd   -> 32-byte struct, 16-byte alignment
    //   KaintanaLayoutCache       -> 12 bytes, 4-byte alignment
    //   KaintanaPhaseHeap indices -> int32_t, 4-byte alignment
    return kaintana_arena_alloc(s, bytes, 16);
}

void kaintana__arena_reset(kt_Session* s)
{
    kaintana_arena_reset(s);
}
