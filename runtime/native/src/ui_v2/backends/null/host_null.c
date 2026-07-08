// ============================================================================
//  host_null.c — Headless/null testing backend for Kaintana
//
//  Implements KaintanaBackendVTable with an in-memory uint32_t framebuffer.
//  No windowing, no OS headers, no GPU. Pure C11 math.
//
//  Purpose:
//    - Unit testing without a display
//    - Regression screenshot comparison (pixel-perfect framebuffer dumps)
//    - CI validation pipelines
//    - Reference implementation for new backends (~80 lines of logic)
//
//  Usage:
//    static const KaintanaBackendVTable null_backend = {
//        .init     = null_init,
//        .shutdown = null_shutdown,
//        .new_frame = null_new_frame,
//        .render   = null_render
//    };
//    kt_backend_register(s, "null", &null_backend);
//    kt_backend_select(s, "null");
//
//  Design:
//    - init()     allocates width x height x 4 bytes via calloc, zero-initialized
//    - shutdown() frees the framebuffer
//    - new_frame() zeros the framebuffer and resets clip stack
//    - render()   iterates draw_data->cmds[0..cmd_count]:
//                   KT_CMD_FILL   → fill bounding rect with premultiplied ARGB color
//                   KT_CMD_CLIP   → push clip rect (intersected with current clip)
//                   KT_CMD_UNCLIP → pop clip rect
//                   Other command types are silently skipped (no-op).
//    - Pixel fill is clipped against the current clip rectangle.
//
//  Verify compilation:
//    gcc -std=c11 -Wall -Wextra -pedantic -Werror
//        -I X:/runtime/native/include
//        -I X:/runtime/native/src/ui_v2
//        -fsyntax-only X:/runtime/native/src/ui_v2/backends/null/host_null.c
// ============================================================================

#include "kaintana.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
//  STATIC STATE — framebuffer + clip stack (singleton, 1 session)
// ============================================================================

uint32_t* kaintana_null_fb         = NULL;  // width * height * sizeof(uint32_t)
int       kaintana_null_width      = 0;

// ── Session pointer (set via config->platform_handle in kt_backend_select) ──
static kt_Session*      g_null_session  = NULL;
int       kaintana_null_height     = 0;

// ── Clip rect stack ─────────────────────────────────────────────────────
#define NULL_CLIP_MAX_DEPTH 16

static kt_Rect  g_clip_stack[NULL_CLIP_MAX_DEPTH];
static int      g_clip_depth       = -1;   // -1 = no clip, full framebuffer

// ============================================================================
//  CLIP RECT HELPERS
// ============================================================================

// Return the effective clip rect. If the stack is empty, the full framebuffer
// rectangle is returned. Every KT_CMD_CLIP push intersects with this.
static inline kt_Rect null_clip_current(void) {
    kt_Rect full;
    full.x = 0.0f;
    full.y = 0.0f;
    full.w = (float)kaintana_null_width;
    full.h = (float)kaintana_null_height;

    if (g_clip_depth < 0)
        return full;

    return g_clip_stack[g_clip_depth];
}

// Push a new clip rect, intersecting `r` with the current clip.
static void null_clip_push(kt_Rect r) {
    if (g_clip_depth >= NULL_CLIP_MAX_DEPTH - 1)
        return;

    kt_Rect cur = null_clip_current();

    // Compute intersection: max of left/top, min of right/bottom
    float x1 = (r.x > cur.x) ? r.x : cur.x;
    float y1 = (r.y > cur.y) ? r.y : cur.y;
    float r_r = r.x + r.w;
    float cur_r = cur.x + cur.w;
    float r_b = r.y + r.h;
    float cur_b = cur.y + cur.h;
    float x2 = (r_r < cur_r) ? r_r : cur_r;
    float y2 = (r_b < cur_b) ? r_b : cur_b;

    // Clamp degenerate rects to zero area (no negative w/h)
    if (x2 < x1) x2 = x1;
    if (y2 < y1) y2 = y1;

    g_clip_depth++;
    g_clip_stack[g_clip_depth].x = x1;
    g_clip_stack[g_clip_depth].y = y1;
    g_clip_stack[g_clip_depth].w = x2 - x1;
    g_clip_stack[g_clip_depth].h = y2 - y1;
}

// Pop the current clip rect. If nothing to pop, silently no-op.
static void null_clip_pop(void) {
    if (g_clip_depth >= 0)
        g_clip_depth--;
}

// ============================================================================
//  PIXEL FILL — Fill a bounding rectangle with a solid color.
//
//  The fill rect is first intersected with the current clip rect, then
//  clamped to the framebuffer dimensions.  Every pixel inside the
//  intersected region is set to `color` (premultiplied ARGB).
// ============================================================================

static void null_fill_rect(kt_Rect bounds, uint32_t color) {
    if (!kaintana_null_fb)
        return;

    kt_Rect clip = null_clip_current();

    // Intersect bounds with clip rect
    float x1 = (bounds.x > clip.x) ? bounds.x : clip.x;
    float y1 = (bounds.y > clip.y) ? bounds.y : clip.y;
    float r_r = bounds.x + bounds.w;
    float c_r = clip.x + clip.w;
    float r_b = bounds.y + bounds.h;
    float c_b = clip.y + clip.h;
    float x2 = (r_r < c_r) ? r_r : c_r;
    float y2 = (r_b < c_b) ? r_b : c_b;

    // Degenerate → nothing to draw
    if (x2 <= x1 || y2 <= y1)
        return;

    // Clamp to framebuffer bounds
    int ix1 = (int)x1;
    if (ix1 < 0) ix1 = 0;
    int iy1 = (int)y1;
    if (iy1 < 0) iy1 = 0;
    int ix2 = (int)(x2 + 0.5f);
    if (ix2 > kaintana_null_width)  ix2 = kaintana_null_width;
    int iy2 = (int)(y2 + 0.5f);
    if (iy2 > kaintana_null_height) iy2 = kaintana_null_height;

    for (int y = iy1; y < iy2; y++) {
        uint32_t* row = kaintana_null_fb + (y * kaintana_null_width);
        for (int x = ix1; x < ix2; x++) {
            row[x] = color;
        }
    }
}

// ============================================================================
//  BACKEND LIFECYCLE — The 4-function KaintanaBackendVTable contract
// ============================================================================

// null_init: Allocate framebuffer. Returns 0 on success, -1 on allocation failure.
static int null_init(const KaintanaBackendConfig* config) {
    if (!config)
        return -1;

    // Store session pointer from config (set by kt_backend_select)
    g_null_session = (kt_Session*)config->platform_handle;

    kaintana_null_width  = config->width;
    kaintana_null_height = config->height;

    if (kaintana_null_width <= 0 || kaintana_null_height <= 0) {
        kaintana_null_width = 0;
        kaintana_null_height = 0;
        return -1;
    }

    kaintana_null_fb = (uint32_t*)calloc((size_t)(kaintana_null_width * kaintana_null_height), sizeof(uint32_t));
    if (!kaintana_null_fb) {
        kaintana_null_width  = 0;
        kaintana_null_height = 0;
        return -1;
    }

    g_clip_depth = -1;

    // Report DPI scale to core (always 1.0 for headless)
    if (g_null_session) {
        kt_set_native_scale(g_null_session, 1.0f, 1.0f);
    }
    return 0;
}

// null_shutdown: Free the framebuffer and reset all state.
static void null_shutdown(void) {
    free(kaintana_null_fb);
    kaintana_null_fb         = NULL;
    kaintana_null_width      = 0;
    kaintana_null_height     = 0;
    g_clip_depth = -1;
}

// null_new_frame: Clear the framebuffer and reset the clip stack.
static void null_new_frame(void) {
    if (kaintana_null_fb) {
        memset(kaintana_null_fb, 0, (size_t)(kaintana_null_width * kaintana_null_height) * sizeof(uint32_t));
    }
    g_clip_depth = -1;
}

// null_render: Process all draw commands into the framebuffer.
//   - KT_CMD_FILL:   Fill the bounds rect with the command's color.
//   - KT_CMD_CLIP:   Push a clip rect (intersected with current clip).
//   - KT_CMD_UNCLIP: Pop the clip rect stack.
//   - Other commands (STROKE, TEXT, IMAGE): silently skipped.
//
//   clip rects affect all subsequent FILL commands until popped.
static void null_render(const kt_DrawData* draw_data) {
    if (!kaintana_null_fb || !draw_data || !draw_data->cmds || draw_data->cmd_count <= 0)
        return;

    for (int i = 0; i < draw_data->cmd_count; i++) {
        const kt_Cmd* cmd = &draw_data->cmds[i];

        switch (cmd->type) {
            case KT_CMD_FILL:
                null_fill_rect(cmd->bounds, cmd->color);
                break;

            case KT_CMD_CLIP:
                null_clip_push(cmd->bounds);
                break;

            case KT_CMD_UNCLIP:
                null_clip_pop();
                break;

            // KT_CMD_STROKE, KT_CMD_TEXT, KT_CMD_IMAGE:
            // Not required for headless testing. Silently skipped.
            default:
                break;
        }
    }
}

// ============================================================================
//  BACKEND VTABLE SINGLETON
//
//  Register with the Kaintana session at startup:
//    extern const KaintanaBackendVTable kaintana_null_backend;
//    kt_backend_register(s, "null", &kaintana_null_backend);
//    kt_backend_select(s, "null");
// ============================================================================

const KaintanaBackendVTable kaintana_null_backend = {
    .init     = null_init,
    .shutdown = null_shutdown,
    .new_frame = null_new_frame,
    .render   = null_render
};
// NOTE: Test helper accessors (kaintana_test_get_fb_ptr etc.) are now in
// tests/kaintana_test_helpers.c which is the companion C translation unit
// for the include'ed kaintana_test_helpers.h header.  They reference the
// non-static globals kaintana_null_fb/kaintana_null_width/kaintana_null_height
// via extern declarations.
