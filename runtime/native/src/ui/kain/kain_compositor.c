// ============================================================================
//  kain_compositor.c — Damage Region Tracking Implementation
//  ============================================================================
//  Dirty-rect accumulator with a fixed ceiling of 64 rects per frame.
//  The damaged_region() accessor computes the bounding union of all
//  accumulated rects. Frame-bounded lifecycle: begin_frame clears the
//  accumulator, end_frame computes the union.
//
//  Part of the Kain UI substrate (KUIF Phase 1).
//  ============================================================================

#include "kain_compositor.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>

#define KAIN_COMPOSITOR_MAX_DAMAGE_RECTS 64

struct KainCompositor {
    int      fb_width;
    int      fb_height;
    float    damage_x[KAIN_COMPOSITOR_MAX_DAMAGE_RECTS];
    float    damage_y[KAIN_COMPOSITOR_MAX_DAMAGE_RECTS];
    float    damage_w[KAIN_COMPOSITOR_MAX_DAMAGE_RECTS];
    float    damage_h[KAIN_COMPOSITOR_MAX_DAMAGE_RECTS];
    int      damage_count;
    bool     has_any_damage;
    kainRect union_rect;
};

// ── Helpers ────────────────────────────────────────────────────────

static inline float minf(float a, float b) { return a < b ? a : b; }
static inline float maxf(float a, float b) { return a > b ? a : b; }

// ── Lifecycle ──────────────────────────────────────────────────────

KainCompositor* kain_compositor_create(int fb_width, int fb_height) {
    KainCompositor* c = (KainCompositor*)calloc(1, sizeof(KainCompositor));
    if (!c) return NULL;
    c->fb_width  = fb_width;
    c->fb_height = fb_height;
    c->damage_count = 0;
    c->has_any_damage = false;
    memset(&c->union_rect, 0, sizeof(kainRect));
    return c;
}

void kain_compositor_destroy(KainCompositor* c) {
    free(c);
}

// ── Frame boundaries ───────────────────────────────────────────────

void kain_compositor_begin_frame(KainCompositor* c) {
    if (!c) return;
    c->damage_count = 0;
    c->has_any_damage = false;
    // Note: union_rect from previous frame is preserved so
    // end_frame callers can still read the last frame's damage.
}

void kain_compositor_end_frame(KainCompositor* c) {
    if (!c || c->damage_count == 0) return;

    // Compute bounding union of all damage rects
    float min_x =  FLT_MAX, min_y =  FLT_MAX;
    float max_x = -FLT_MAX, max_y = -FLT_MAX;

    for (int i = 0; i < c->damage_count; i++) {
        float x2 = c->damage_x[i] + c->damage_w[i];
        float y2 = c->damage_y[i] + c->damage_h[i];
        if (c->damage_x[i] < min_x) min_x = c->damage_x[i];
        if (c->damage_y[i] < min_y) min_y = c->damage_y[i];
        if (x2 > max_x) max_x = x2;
        if (y2 > max_y) max_y = y2;
    }

    // Clamp to framebuffer bounds
    if (min_x < 0.0f) min_x = 0.0f;
    if (min_y < 0.0f) min_y = 0.0f;
    if (max_x > (float)c->fb_width)  max_x = (float)c->fb_width;
    if (max_y > (float)c->fb_height) max_y = (float)c->fb_height;

    c->union_rect.x = min_x;
    c->union_rect.y = min_y;
    c->union_rect.w = (max_x > min_x) ? (max_x - min_x) : 0.0f;
    c->union_rect.h = (max_y > min_y) ? (max_y - min_y) : 0.0f;
}

// ── Damage tracking ────────────────────────────────────────────────

void kain_compositor_damage_rect(KainCompositor* c, float x, float y, float w, float h) {
    if (!c) return;
    if (w <= 0.0f || h <= 0.0f) return;

    c->has_any_damage = true;

    // If we still have room, record the rect
    if (c->damage_count < KAIN_COMPOSITOR_MAX_DAMAGE_RECTS) {
        c->damage_x[c->damage_count] = x;
        c->damage_y[c->damage_count] = y;
        c->damage_w[c->damage_count] = w;
        c->damage_h[c->damage_count] = h;
        c->damage_count++;
    }
    // When full, we still mark has_any_damage but drop the rect.
    // The end_frame union will still cover what was stored, and
    // callers can react to has_any_damage for full-redraw fallback.
}

void kain_compositor_damage_node(KainCompositor* c, int64_t node_id) {
    // Stub — in Phase 1, does nothing (no node tree access).
    // In future phases, looks up node rect and calls damage_rect.
    (void)c;
    (void)node_id;
}

// ── Accessors ──────────────────────────────────────────────────────

kainRect kain_compositor_damaged_region(KainCompositor* c) {
    if (!c) {
        kainRect empty = { 0.0f, 0.0f, 0.0f, 0.0f };
        return empty;
    }
    return c->union_rect;
}

bool kain_compositor_has_damage(KainCompositor* c) {
    if (!c) return false;
    return c->has_any_damage;
}

void kain_compositor_clear_damage(KainCompositor* c) {
    if (!c) return;
    c->damage_count = 0;
    c->has_any_damage = false;
    memset(&c->union_rect, 0, sizeof(kainRect));
}
