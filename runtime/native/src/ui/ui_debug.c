// ============================================================================
//  ui_debug.c — Clay-Inspired Debug Overlay for Kain UI Runtime
//  ============================================================================
//  Draws on top of the existing framebuffer AFTER the normal frame render.
//  Zero-cost when ctx->visible is false (single bool check at call site).
//
//  Features:
//    • Info panel (right side) — node count, draw commands, layout stats
//    • Element bounding boxes — color-coded per node kind
//    • Node ID labels — stable_key or kind text above each element
//    • FPS indicator (top-left)
//    • Controls legend (bottom-left)
//
//  All drawing uses KainSoftwareRenderer primitives (kain_render_fill_rect,
//  kain_render_stroke_rect, kain_render_text). No node tree modifications.
// ============================================================================

#include "ui_debug.h"
#include "kain_render_software.h"
#include "kain_font.h"
#include "ui_system_internal.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

// ── Color helpers ─────────────────────────────────────────────────────

static inline kainColor ud_color_u8(int r, int g, int b, int a) {
    return kain_color_rgba(
        (float)r / 255.0f,
        (float)g / 255.0f,
        (float)b / 255.0f,
        (float)a / 255.0f
    );
}

static inline kainColor ud_apply_opacity(kainColor c, float opacity) {
    kainColor r = c;
    r.a *= opacity;
    return r;
}

// ── Public API ────────────────────────────────────────────────────────

void ui_debug_init(UiDebugContext* ctx) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->hovered_node   = -1;
    ctx->selected_node  = -1;
    ctx->opacity        = 0.85f;
    ctx->session_id     = 0;
    ctx->font_id        = 0;
    ctx->font_mono_id   = 0;
}

void ui_debug_toggle(UiDebugContext* ctx) {
    if (ctx) ctx->visible = !ctx->visible;
}

void ui_debug_push_key(UiDebugContext* ctx, int key) {
    if (!ctx) return;
    if (ctx->key_count < 16) {
        ctx->keys[ctx->key_count++] = key;
    }
}

bool ui_debug_process_keys(UiDebugContext* ctx) {
    if (!ctx) return false;
    bool consumed = false;
    for (int i = 0; i < ctx->key_count; i++) {
        int k = ctx->keys[i];
        switch (k) {
        case '`':
            ctx->visible = !ctx->visible;
            consumed = true;
            break;
        case 'B': case 'b':
            ctx->show_bounds = !ctx->show_bounds;
            consumed = true;
            break;
        case 'I': case 'i':
            ctx->show_ids = !ctx->show_ids;
            consumed = true;
            break;
        case 'L': case 'l':
            ctx->show_layout_info = !ctx->show_layout_info;
            consumed = true;
            break;
        case 'R': case 'r':
            ctx->show_render_commands = !ctx->show_render_commands;
            consumed = true;
            break;
        default:
            // Not a debug key — leave in queue so process_keys caller
            // can re-check. (For simplicity, we consume all queued keys
            // and only recognize our own.)
            break;
        }
    }
    ctx->key_count = 0;
    return consumed;
}

// ── Node kind classification for color-coding ─────────────────────────

typedef int UiDebugNodeClass;

#define UD_NODE_TEXT       0
#define UD_NODE_INTERACTIVE 1
#define UD_NODE_CONTAINER  2
#define UD_NODE_LEAF       3

static UiDebugNodeClass ud_classify_node(const KainNativeUiNode* node) {
    // Text nodes: have non-empty text or kind starts with "text"
    if (node->text[0] != '\0') return UD_NODE_TEXT;
    if (strncmp(node->kind, "text", 4) == 0) return UD_NODE_TEXT;

    // Interactive nodes: kind contains "button" or "interactive"
    if (strstr(node->kind, "button") || strstr(node->kind, "interactive"))
        return UD_NODE_INTERACTIVE;

    // Container nodes: have children
    if (node->child_count > 0) return UD_NODE_CONTAINER;

    // Everything else is a leaf
    return UD_NODE_LEAF;
}

static kainColor ud_color_for_class(UiDebugNodeClass cls, float alpha) {
    switch (cls) {
    case UD_NODE_TEXT:        return ud_color_u8(  0, 255, 255, (int)(alpha * 180.0f)); // cyan
    case UD_NODE_INTERACTIVE: return ud_color_u8(255, 136,   0, (int)(alpha * 180.0f)); // orange
    case UD_NODE_CONTAINER:   return ud_color_u8(  0, 255,   0, (int)(alpha * 180.0f)); // green
    case UD_NODE_LEAF:
    default:                  return ud_color_u8(  0, 136, 255, (int)(alpha * 180.0f)); // blue
    }
}

static const char* ud_label_for_class(UiDebugNodeClass cls) {
    switch (cls) {
    case UD_NODE_TEXT:        return "text";
    case UD_NODE_INTERACTIVE: return "interactive";
    case UD_NODE_CONTAINER:   return "container";
    case UD_NODE_LEAF:
    default:                  return "leaf";
    }
}

// ── Draw ──────────────────────────────────────────────────────────────

void ui_debug_draw(
    UiDebugContext* ctx,
    int node_count, int render_cmd_count, int layout_node_count,
    int fb_w, int fb_h,
    KainSoftwareRenderer* renderer)
{
    // Quick-out: nothing to draw
    if (!ctx || !ctx->visible || !renderer) return;

    const float       opacity  = ctx->opacity;
    const int64_t     font_id  = ctx->font_id > 0 ? ctx->font_id : ctx->font_mono_id;
    const int64_t     font_mono = ctx->font_mono_id > 0 ? ctx->font_mono_id : font_id;
    const kainColor   color_white = ud_apply_opacity(KAIN_COLOR_WHITE, opacity);

    // ═════════════════════════════════════════════════════════════════
    //  1. Info Panel (right side) — aggregate stats
    // ═════════════════════════════════════════════════════════════════
    {
        const float panel_w   = 230.0f;
        const float panel_x   = (float)fb_w - panel_w - 8.0f;
        const float panel_y   = 8.0f;
        const float line_h    = 18.0f;
        const float pad       = 8.0f;
        const float text_x    = panel_x + pad;
        const float text_y    = panel_y + pad + 2.0f;

        // Build lines
        char lines[8][64];
        int  line_count = 0;
        line_count += snprintf(lines[line_count], sizeof(lines[0]),
            "Nodes:  %d", node_count);
        line_count += snprintf(lines[line_count], sizeof(lines[0]),
            "Draw:   %d", render_cmd_count);
        line_count += snprintf(lines[line_count], sizeof(lines[0]),
            "Layout: %d", layout_node_count);
        line_count += snprintf(lines[line_count], sizeof(lines[0]),
            "FB:     %dx%d", fb_w, fb_h);
        if (node_count > 0) {
            line_count += snprintf(lines[line_count], sizeof(lines[0]),
                "Hover:  %d", ctx->hovered_node);
            line_count += snprintf(lines[line_count], sizeof(lines[0]),
                "Select: %d", ctx->selected_node);
        }

        const float panel_h = pad * 2.0f + (float)line_count * line_h;

        // Background
        kainRect bg = kain_rect_make(panel_x, panel_y, panel_w, panel_h);
        kain_render_fill_rect(renderer, bg,
            ud_apply_opacity(ud_color_u8(0, 0, 0, 200), opacity));

        // Border
        kain_render_stroke_rect(renderer, bg, 1.0f,
            ud_apply_opacity(ud_color_u8(100, 100, 100, 200), opacity));

        // Title
        if (font_mono > 0) {
            kain_render_text(renderer,
                kain_point_make(text_x, text_y),
                "[ Kain Debug ]", font_mono, 13.0f,
                ud_apply_opacity(ud_color_u8(0, 200, 255, 255), opacity));
        }

        // Data lines
        if (font_mono > 0) {
            for (int i = 0; i < line_count && i < 8; i++) {
                kainColor row_color = (i < 3)
                    ? color_white
                    : ud_apply_opacity(ud_color_u8(180, 180, 180, 255), opacity);
                kain_render_text(renderer,
                    kain_point_make(text_x, text_y + line_h * (float)(i + 1)),
                    lines[i], font_mono, 12.0f, row_color);
            }
        }
    }

    // ═════════════════════════════════════════════════════════════════
    //  2. Element Bounding Boxes + ID Labels
    //     Requires session_id > 0 and a live session.
    // ═════════════════════════════════════════════════════════════════
    if ((ctx->show_bounds || ctx->show_ids) && ctx->session_id > 0) {
        KainNativeUiSession* session = abi_ui_find_session(ctx->session_id);
        if (session) {
            for (int i = 0; i < ABI_UI_MAX_NODES; i++) {
                KainNativeUiNode* node = &session->nodes[i];
                if (!node->in_use) continue;
                if (node->flags & ABI_UI_NODE_HIDDEN) continue;

                const float nx = (float)node->x;
                const float ny = (float)node->y;
                const float nw = (float)node->width;
                const float nh = (float)node->height;
                if (nw <= 0.0f || nh <= 0.0f) continue;

                kainRect nr = kain_rect_make(nx, ny, nw, nh);
                UiDebugNodeClass cls = ud_classify_node(node);
                kainColor col = ud_color_for_class(cls, opacity);

                // ── Bounding box stroke ──
                if (ctx->show_bounds) {
                    kain_render_stroke_rect(renderer, nr, 1.0f, col);
                }

                // ── Node label ──
                if (ctx->show_ids && font_mono > 0) {
                    // Use stable_key, then kind, then class label
                    const char* label = node->stable_key[0]
                        ? node->stable_key
                        : (node->kind[0] ? node->kind : ud_label_for_class(cls));

                    // Position label above the element's top edge
                    float label_y = ny - 14.0f;
                    if (label_y < 0.0f) label_y = ny + 2.0f; // fallback inside

                    kain_render_text(renderer,
                        kain_point_make(nx, label_y),
                        label, font_mono, 10.0f, col);
                }

                // ── Layout info (sizing, padding, direction) ──
                if (ctx->show_layout_info && font_mono > 0 && node->child_count > 0) {
                    char info[64];
                    const char* dir = node->flex_dir_set
                        ? (node->flex_dir == FLEX_DIR_ROW ? "ROW" : "COL")
                        : "?";
                    snprintf(info, sizeof(info), "%s | gap:%.0f", dir, node->flex_gap);
                    kain_render_text(renderer,
                        kain_point_make(nx, ny + nh + 2.0f),
                        info, font_mono, 9.0f,
                        ud_apply_opacity(ud_color_u8(150, 200, 150, 255), opacity));
                }
            }
        }
    }

    // ═════════════════════════════════════════════════════════════════
    //  3. Controls Legend (bottom-left)
    // ═════════════════════════════════════════════════════════════════
    {
        const char* legend[] = {
            "` - Toggle overlay",
            "B - Bounding boxes",
            "I - Node IDs",
            "L - Layout info",
            "R - Render commands",
        };
        const int legend_count = 5;
        const float line_h = 16.0f;
        const float pad   = 6.0f;

        const float box_x = 8.0f;
        const float box_y = (float)fb_h - (float)legend_count * line_h - pad * 2.0f - 8.0f;
        const float box_w = 200.0f;
        const float box_h = (float)legend_count * line_h + pad * 2.0f;

        // Background
        kainRect lbg = kain_rect_make(box_x, box_y, box_w, box_h);
        kain_render_fill_rect(renderer, lbg,
            ud_apply_opacity(ud_color_u8(0, 0, 0, 180), opacity));
        kain_render_stroke_rect(renderer, lbg, 1.0f,
            ud_apply_opacity(ud_color_u8(80, 80, 80, 200), opacity));

        if (font_mono > 0) {
            for (int i = 0; i < legend_count; i++) {
                kainColor c = ud_apply_opacity(
                    ud_color_u8(180, 180, 180, 230), opacity);
                kain_render_text(renderer,
                    kain_point_make(box_x + pad, box_y + pad + (float)i * line_h),
                    legend[i], font_mono, 11.0f, c);
            }
        }
    }

    // ═════════════════════════════════════════════════════════════════
    //  4. Top-left heading (visible even when no fonts loaded)
    // ═════════════════════════════════════════════════════════════════
    {
        const char* title = "Kain Debug Overlay";
        kainRect hdr = kain_rect_make(8.0f, 8.0f, 170.0f, 22.0f);
        kain_render_fill_rect(renderer, hdr,
            ud_apply_opacity(ud_color_u8(0, 0, 0, 180), opacity));
        kain_render_stroke_rect(renderer, hdr, 1.0f,
            ud_apply_opacity(ud_color_u8(80, 80, 80, 200), opacity));

        if (font_id > 0) {
            kain_render_text(renderer,
                kain_point_make(14.0f, 12.0f),
                title, font_id, 12.0f, color_white);
        }
    }
}
