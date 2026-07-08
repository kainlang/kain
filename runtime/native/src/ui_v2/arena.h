// ============================================================================
//  arena.h — Grow-only frame arena wrappers for the Kaintana UI substrate.
//
//  Public API for per-frame allocation backed by Kain's core arena
//  (kain_arena_alloc_lo). The arena buffer is EMBEDDED in the session struct
//  (64KB via KAINTANA_ARENA_SIZE). Markers enable O(1) per-frame cleanup.
//
//  Design tenets:
//    - No individual free — per-frame reset via markers is the strategy
//    - Frame markers have max depth 8 (kt_begin/kt_end nesting is shallow)
//    - Geometric growth (1.5x) governed by Kain's core arena, not here
//    - All formulas Z3-proven (arena_proofs.yaml): alignment, frame save/restore
//
//  Internal helpers (kaintana__arena_push / kaintana__arena_reset) are
//  declared in kaintana.h §19 and implemented in arena.c.
// ============================================================================

#ifndef KAINTANA_ARENA_H
#define KAINTANA_ARENA_H

#include "kaintana.h"
// Include the core arena header for KainArena, kain_arena_init, kain_arena_alloc_lo,
// kain_frame_set_marker, kain_frame_release_to_last_marker, kain_arena_reset.
// internal.h includes "arena.h" expecting the core header; including it here
// ensures types are visible regardless of include path order.
#include "../../include/arena.h"
// ---------------------------------------------------------------------------
//  PUBLIC API — Called by external code (backends, tests, Kain stdlib bridge).
//  Hot-path callers within ui_v2/ use the static inline helpers in
//  internal.h (kaintana__arena_alloc / kaintana__arena_mark / etc.) directly.
// ---------------------------------------------------------------------------

// Initialize the frame arena with the session's embedded buffer.
// Called once at session creation (from kt_make in tree.c).
void kaintana_arena_init(kt_Session* s);

// Allocate size bytes with given alignment from the frame arena.
// Returns NULL if the arena is exhausted (should not happen at 64KB default).
void* kaintana_arena_alloc(kt_Session* s, size_t size, size_t align);

// Set a frame marker. Call at the start of kt_begin().
// Saves the current low/high offsets for rollback at end_frame.
void kaintana_arena_mark(kt_Session* s);

// Release all allocations back to the last marker. Call at the end of
// kt_end(). O(1) — restores low/high to saved marker offsets.
void kaintana_arena_release(kt_Session* s);

// Full arena reset. Call at session destroy (kt_free).
// Resets low/high to the buffer start/end and clears the frame stack.
void kaintana_arena_reset(kt_Session* s);

#endif // KAINTANA_ARENA_H
