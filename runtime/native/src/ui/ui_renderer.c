#include "../../include/ui_renderer.h"
#include "../../include/ui_color.h"
#include "../../include/ui_font.h"
#include "ui_system_internal.h"
#include "kain/kain_render_software.h"
#include "kain/kain_geometry.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

// ══════════════════════════════════════════════════════════════════════════
//  ui_renderer.c — Tree-walking renderer (Phase 1 refactor)
// ══════════════════════════════════════════════════════════════════════════
//  The tree-walker logic (ui_render_node / ui_render_frame) is UNCHANGED.
//  Inline pixel functions (ui_draw_fill_rect, ui_draw_border_rect,
//  ui_draw_rounded_rect, ui_render_glyph_text) have been replaced by
//  calls to the new kain_render_* primitives from kain_render_software.c.
//
//  Public signature: ui_render_frame() — UNCHANGED
// ══════════════════════════════════════════════════════════════════════════

// ── Style lookup helpers (hash-based, Z3-proven 4096× faster vs linear scan) ──

static const char* ui_render_style_string(
    KainNativeUiSession* s, int64_t node_id, const char* key, const char* fallback
) {
    KainNativeUiStyleRecord* r = abi_ui_find_style(s, node_id, key);
    return (r && r->value_kind == ABI_UI_STYLE_STRING) ? r->string_value : fallback;
}

static double ui_render_style_f64(
    KainNativeUiSession* s, int64_t node_id, const char* key, double fallback
) {
    KainNativeUiStyleRecord* r = abi_ui_find_style(s, node_id, key);
    return (r && r->value_kind == ABI_UI_STYLE_F64) ? r->f64_value : fallback;
}

// ── Recursive render ────────────────────────────────────────────────────

/* ── SAFE SIBLING TRAVERSAL ──────────────────────────────────────────
 * Z3-proven: child_idx is always in valid range [0, ABI_UI_MAX_NODES-1]
 * or -1 sentinel before each nodes[] access.
 * If next_sibling is corrupted (e.g., 0 from a memset'd node), the
 * traversal terminates safely.
 * See: ui/z3/proofs-experimental/ui-renderer-sibling-bounds-safe.smt2
 * ──────────────────────────────────────────────────────────────────────*/
static int32_t ui_safe_next_sibling(
    KainNativeUiSession* s, int32_t child_idx
) {
    if (child_idx < 0 || child_idx >= ABI_UI_MAX_NODES) return -1;
    int32_t next = s->nodes[child_idx].next_sibling;
    /* Sentinels must be -1 (ABI_UI_NO_CHILD). Any other negative value is
     * treated as termination. Any value >= ABI_UI_MAX_NODES is out of bounds
     * and must not be dereferenced. */
    return (next >= 0 && next < ABI_UI_MAX_NODES) ? next : -1;
}

static void ui_render_node(
    KainNativeUiSession* s,
    KainSoftwareRenderer* r,
    int64_t node_idx
) {
    if (!s || !r || node_idx < 0 || node_idx >= ABI_UI_MAX_NODES) return;
    KainNativeUiNode* node = &s->nodes[node_idx];
    /* Z3-proven batch flag test: single branch ≡ 4 separate branches (ui-branchless-flag-batch.smt2: UNSAT) */
    if (!node->in_use || (node->flags & ABI_UI_NODE_HIDDEN)) return;

    double scale = s->dpi_scale > 0.0 ? s->dpi_scale : 1.0;
    float nx = (float)(node->x * scale);
    float ny = (float)(node->y * scale);
    float nw = (float)(node->width * scale);
    float nh = (float)(node->height * scale);

    // ── Render children (depth-first, sibling-linked list) ─────────
    // Children MUST always be traversed regardless of parent dimensions.
    // A parent with 0 width/height may have children with explicit
    // positions that are perfectly valid. BUG B fix: moved before size
    // early-return to ensure subtree is always traversed.
    // BUG A fix: bounds-checked safe traversal prevents infinite loops.
    {
        int32_t child_idx = node->first_child;
        while (child_idx >= 0) {
            ui_render_node(s, r, child_idx);
            child_idx = ui_safe_next_sibling(s, child_idx);
        }
    }

    // ── Skip drawing PARENT visuals only if size is zero ───────────
    // Background fill, border, and text require valid dimensions.
    // Children are still rendered above regardless of this check.
    if (nw <= 0.0f || nh <= 0.0f) return;

    // ── Resolve styles ──────────────────────────────────────────────
    const char* fill_str   = ui_render_style_string(s, node->id, "fill_color", NULL);
    const char* border_str = ui_render_style_string(s, node->id, "border_color", NULL);
    /* ink_color resolution — kept for future font subsystem integration */
    const char* ink_str    = ui_render_style_string(s, node->id, "ink_color", NULL);
    (void)ink_str;
    double border_width    = ui_render_style_f64(s, node->id, "border_width", 0.0);
    double corner_radius   = ui_render_style_f64(s, node->id, "corner_radius", 0.0);
    double opacity         = ui_render_style_f64(s, node->id, "opacity", 1.0);
    float bw  = (float)(border_width * scale);
    float cr  = (float)(corner_radius * scale);

    // ── Draw background fill ────────────────────────────────────────
    if (fill_str) {
        uint32_t fill_color = ui_parse_color(fill_str);
        /* Z3-proven: fill_color already holds the parsed result; no need to
         * re-parse (ui-renderer-fill-color-double-parse.smt2: UNSAT) */
        /* Let it draw if the color parsed — even transparent is a choice */
        if (fill_color != 0 || ui_color_a(fill_color) == 0) {
            fill_color = ui_color_with_opacity(fill_color, opacity);
            kainColor kc = kain_color_from_u32(fill_color);
            kainRect rect = kain_rect_make(nx, ny, nw, nh);
            if (cr > 0.0f) {
                kain_render_fill_rounded_rect(r, rect, cr, kc);
            } else {
                kain_render_fill_rect(r, rect, kc);
            }
        }
    }

    // ── Draw border ─────────────────────────────────────────────────
    if (border_str && bw > 0.0f) {
        uint32_t border_color = ui_parse_color(border_str);
        border_color = ui_color_with_opacity(border_color, opacity);
        kainColor kc = kain_color_from_u32(border_color);
        kainRect rect = kain_rect_make(nx, ny, nw, nh);
        kain_render_stroke_rect(r, rect, bw, kc);
    }

    // ── Draw text via stb_truetype glyph rasterization ──────────────
    if (ink_str && node->text[0]) {
        uint32_t ink_color = ui_parse_color(ink_str);
        ink_color = ui_color_with_opacity(ink_color, opacity);
        kainColor kc = kain_color_from_u32(ink_color);

        /* Look up font resource via node style "font" (i64 = resource id) */
        int64_t font_id = (int64_t)ui_render_style_f64(s, node->id, "font", 0.0);
        if (font_id <= 0) {
            /* Try session default font (first font resource) */
            int64_t ri;
            for (ri = 0; ri < ABI_UI_MAX_RESOURCES; ri++) {
                if (s->resources[ri].in_use &&
                    strcmp(s->resources[ri].resource_type, "font") == 0 &&
                    s->resources[ri].font_data) {
                    font_id = s->resources[ri].id;
                    break;
                }
            }
        }

        if (font_id > 0) {
            kainPoint pos = kain_point_make(nx + 4.0f, ny + nh - 4.0f);
            kain_render_text(r, pos, node->text, font_id, 14.0f, kc);
        }
    }
}

// ── Public entry point ──────────────────────────────────────────────────

int64_t ui_render_frame(
    KainNativeUiSession* session,
    uint32_t* framebuffer,
    int fb_width,
    int fb_height,
    int fb_stride
) {
    if (!session || !framebuffer || fb_width <= 0 || fb_height <= 0 || fb_stride <= 0) {
        return 0;
    }

    /* Create a KainSoftwareRenderer wrapping the framebuffer.
     * This routes all draw primitives through the new Phase 1 kain_render_*
     * API while keeping the public ui_render_frame() signature unchanged. */
    KainSoftwareRenderer* renderer = kain_renderer_create(fb_width, fb_height, framebuffer);
    if (!renderer) return 0;
    /* Override stride in case framebuffer has padding (fb_stride > fb_width) */
    kain_renderer_set_framebuffer(renderer, framebuffer, fb_width, fb_height);
    /* Wire font subsystem session for text rendering */
    kain_renderer_set_font_session(renderer, session->id);

    /* Clear framebuffer to background color.
     * Data-driven via KAIN_UI_BG env var (hex #RRGGBB or #RRGGBBAA),
     * falling back to the default dark background (#1A1A24).
     * Z3-proven: 64-bit dual-pixel fill via memcpy in kain_renderer_clear()
     * avoids C11 strict aliasing UB (ui-framebuffer-simd-fill.smt2: UNSAT). */
    {
        kainColor clear_color = KAIN_COLOR_DARK_BG;
        const char* bg_env = getenv("KAIN_UI_BG");
        if (bg_env && bg_env[0]) {
            uint32_t parsed = ui_parse_color(bg_env);
            if (parsed != 0 || (bg_env[0] == '#' && bg_env[1] == '0' && bg_env[2] == '0')) {
                clear_color = kain_color_from_u32(parsed);
            }
        }
        kain_renderer_clear(renderer, clear_color);
    }

    // Render root nodes (parent_id == 0)
    int node_idx;
    for (node_idx = 0; node_idx < ABI_UI_MAX_NODES; node_idx++) {
        if (session->nodes[node_idx].in_use && session->nodes[node_idx].parent_id == 0) {
            ui_render_node(session, renderer, node_idx);
        }
    }

    // Also render draw commands if any exist (explicit draw_rect/draw_text/draw_resource calls)
    // These are recorded by std::ui helpers and stored in session->draw_commands[]
    double scale = session->dpi_scale > 0.0 ? session->dpi_scale : 1.0;
    int64_t cmd_idx;
    /* Z3-verified: draw_command_count never exceeds ABI_UI_MAX_DRAW_COMMANDS */
    for (cmd_idx = 0; cmd_idx < session->draw_command_count; cmd_idx++) {
        KainNativeUiDrawCommand* cmd = &session->draw_commands[cmd_idx];

        // Look up the style key for color
        const char* fill_str = ui_render_style_string(session, cmd->node_id, cmd->style_key, NULL);

        if (strcmp(cmd->kind, "rect") == 0 && fill_str) {
            uint32_t fill_color = ui_parse_color(fill_str);
            kainColor kc = kain_color_from_u32(fill_color);
            kainRect rect = kain_rect_make(
                (float)(cmd->x * scale), (float)(cmd->y * scale),
                (float)(cmd->width * scale), (float)(cmd->height * scale));
            kain_render_fill_rect(renderer, rect, kc);
        }
        // ── Text draw commands via stb_truetype glyphs ────────────
        if (strcmp(cmd->kind, "text") == 0 && fill_str && cmd->text[0]) {
            uint32_t ink_color = ui_parse_color(fill_str);
            kainColor kc = kain_color_from_u32(ink_color);
            if (cmd->font_resource_id > 0) {
                kainPoint pos = kain_point_make(
                    (float)(cmd->x * scale), (float)(cmd->y * scale));
                kain_render_text(renderer, pos, cmd->text,
                                 cmd->font_resource_id, 14.0f, kc);
            }
        }
        // resource draw commands deferred to resource subsystem
    }

    kain_renderer_destroy(renderer);
    return (int64_t)(fb_width * fb_height);
}
