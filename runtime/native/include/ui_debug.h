#ifndef KAIN_UI_DEBUG_H
#define KAIN_UI_DEBUG_H

#include <stdint.h>
#include <stdbool.h>
#include "kain_geometry.h"

/* Opaque forward declaration — defined in kain_render_software.h */
typedef struct KainSoftwareRenderer KainSoftwareRenderer;

#ifdef __cplusplus
extern "C" {
#endif

// ══════════════════════════════════════════════════════════════════════════
//  ui_debug.h — Clay-Inspired Debug Overlay for the Kain UI Runtime
// ══════════════════════════════════════════════════════════════════════════
//  An OPTIONAL debug overlay that draws on top of the existing framebuffer
//  after the normal frame render. Provides:
//    • FPS counter (top-left)
//    • Element bounding boxes with color-coded node types
//    • Node stable-key / ID labels
//    • Aggregate overlay stats panel (right side)
//    • Controls legend (bottom-left)
//
//  All drawing is performed via KainSoftwareRenderer primitives.
//  The overlay is zero-cost when ctx->visible is false (single bool check).
//
//  Usage (in your app's render loop):
//    // Draw normal UI...
//    kain_renderer_clear(r, bg_color);
//    // ...
//
//    // Debug overlay on top
//    if (g_app.debug.visible) {
//        ui_debug_draw(&g_app.debug, node_count, cmd_count,
//                      layout_count, fb_w, fb_h, r);
//    }
// ══════════════════════════════════════════════════════════════════════════

// ── Debug context (app-owned, persists across frames) ────────────

typedef struct UiDebugContext {
    // Toggle state (app sets these)
    bool      visible;            // is the overlay shown this frame?
    bool      show_bounds;        // show element bounding boxes
    bool      show_ids;           // show stable keys / node IDs
    bool      show_layout_info;   // show sizing/alignment info
    bool      show_render_commands; // show render command count/layers

    // Appearance
    float     opacity;            // overlay opacity [0..1] (default 0.85)

    // Tree navigation
    int       hovered_node;       // currently hovered node (-1 if none)
    int       selected_node;      // currently selected node (-1 if none)

    // Resources (app must set these after loading fonts)
    int64_t   session_id;         // UI session for node tree access (0 = disabled)
    int64_t   font_id;            // primary overlay font (0 = disabled)
    int64_t   font_mono_id;       // monospace font for data (0 = same as font_id)

    // Key input ring buffer (app pushes here before calling process_keys)
    int       keys[16];
    int       key_count;
} UiDebugContext;

// ── API ──────────────────────────────────────────────────────────

// Initialize debug context to default state (all off, opacity 0.85)
void ui_debug_init(UiDebugContext* ctx);

// Toggle the overlay on/off (flips visible flag)
void ui_debug_toggle(UiDebugContext* ctx);

// Queue a key press for deferred processing (ring buffer, max 16)
void ui_debug_push_key(UiDebugContext* ctx, int key);

// Process all queued keys and update state. Returns true if any key
// was consumed (useful for "key handled" propagation).
bool ui_debug_process_keys(UiDebugContext* ctx);

// Draw the debug overlay on top of the existing framebuffer.
//   ctx             - debug context (must be non-NULL)
//   node_count      - total nodes in tree this frame
//   render_cmd_count - render commands emitted this frame
//   layout_node_count - nodes re-laid-out this frame
//   fb_w, fb_h      - framebuffer dimensions (pixels)
//   renderer        - KainSoftwareRenderer to draw on (must be non-NULL)
//
// If ctx->session_id > 0 AND (show_bounds or show_ids) is true,
// the function accesses the UI node tree via abi_ui_find_session().
// Otherwise only aggregate info is displayed.
void ui_debug_draw(
    UiDebugContext* ctx,
    int node_count, int render_cmd_count, int layout_node_count,
    int fb_w, int fb_h,
    KainSoftwareRenderer* renderer
);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_UI_DEBUG_H */
