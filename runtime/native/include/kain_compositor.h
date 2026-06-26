// ============================================================================
//  kain_compositor.h — Damage Region Tracker
//  ============================================================================
//  Tracks dirty (damaged) rectangles within a framebuffer so the renderer
//  can skip redrawing unchanged regions. Accumulates up to 64 rects per
//  frame; the damaged_region() accessor returns the bounding union.
//
//  Part of the Kain UI substrate (KUIF Phase 1). Widget-free, tree-free.
//  ============================================================================

#ifndef KAIN_COMPOSITOR_H
#define KAIN_COMPOSITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "kain_geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Opaque compositor type ─────────────────────────────────────────
typedef struct KainCompositor KainCompositor;

// ── Lifecycle ──────────────────────────────────────────────────────
KainCompositor* kain_compositor_create(int fb_width, int fb_height);
void            kain_compositor_destroy(KainCompositor* c);

// ── Frame boundaries ───────────────────────────────────────────────
// begin_frame: resets per-frame damage accumulator (call at frame start)
// end_frame:   computes union_rect from accumulated damage rects
void kain_compositor_begin_frame(KainCompositor* c);
void kain_compositor_end_frame(KainCompositor* c);

// ── Damage tracking ────────────────────────────────────────────────
// damage_rect:  mark a rectangle as damaged (needs redraw)
// damage_node:  stub — in Phase 1 does nothing (no node tree access)
//               In future phases, looks up node rect and calls damage_rect.
void     kain_compositor_damage_rect(KainCompositor* c, float x, float y, float w, float h);
void     kain_compositor_damage_node(KainCompositor* c, int64_t node_id);

// ── Accessors ──────────────────────────────────────────────────────
// damaged_region: returns the bounding union of all damage rects this frame
// has_damage:     true if any damage rects were recorded this frame
// clear_damage:   resets all damage state (useful for full redraw scenarios)
kainRect kain_compositor_damaged_region(KainCompositor* c);
bool     kain_compositor_has_damage(KainCompositor* c);
void     kain_compositor_clear_damage(KainCompositor* c);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_COMPOSITOR_H */
