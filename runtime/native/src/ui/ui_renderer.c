#include "../../include/ui_renderer.h"
#include "../../include/ui_color.h"
#include "ui_system_internal.h"

#include <string.h>
#include <math.h>

// ── Drawing primitives (all operate on uint32_t* framebuffer) ──────────

// Clamp value to range
static int ui_clamp_i(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Fill a rectangle with a solid color
static void ui_draw_fill_rect(
    uint32_t* fb, int fb_width, int fb_height, int fb_stride,
    int x, int y, int w, int h,
    uint32_t color
) {
    if (!fb || w <= 0 || h <= 0) return;
    int x0 = ui_clamp_i(x, 0, fb_width);
    int y0 = ui_clamp_i(y, 0, fb_height);
    int x1 = ui_clamp_i(x + w, 0, fb_width);
    int y1 = ui_clamp_i(y + h, 0, fb_height);
    if (x0 >= x1 || y0 >= y1) return;

    int row;
    for (row = y0; row < y1; row++) {
        uint32_t* dst = fb + row * fb_stride + x0;
        int count = x1 - x0;
        int col;
        for (col = 0; col < count; col++) {
            dst[col] = ui_color_blend(color, dst[col]);
        }
    }
}

// Draw a rectangle border (outline)
static void ui_draw_border_rect(
    uint32_t* fb, int fb_width, int fb_height, int fb_stride,
    int x, int y, int w, int h,
    uint32_t color, int thickness
) {
    if (!fb || w <= 0 || h <= 0 || thickness <= 0) return;
    if (thickness > w / 2) thickness = w / 2;
    if (thickness > h / 2) thickness = h / 2;

    // Top edge
    ui_draw_fill_rect(fb, fb_width, fb_height, fb_stride, x, y, w, thickness, color);
    // Bottom edge
    ui_draw_fill_rect(fb, fb_width, fb_height, fb_stride, x, y + h - thickness, w, thickness, color);
    // Left edge
    ui_draw_fill_rect(fb, fb_width, fb_height, fb_stride, x, y + thickness, thickness, h - 2 * thickness, color);
    // Right edge
    ui_draw_fill_rect(fb, fb_width, fb_height, fb_stride, x + w - thickness, y + thickness, thickness, h - 2 * thickness, color);
}

// Draw a rounded rectangle (filled)
static void ui_draw_rounded_rect(
    uint32_t* fb, int fb_width, int fb_height, int fb_stride,
    int x, int y, int w, int h,
    uint32_t color, int radius
) {
    if (!fb || w <= 0 || h <= 0) return;
    if (radius <= 0 || radius > w / 2 || radius > h / 2) {
        ui_draw_fill_rect(fb, fb_width, fb_height, fb_stride, x, y, w, h, color);
        return;
    }

    int x0 = ui_clamp_i(x, 0, fb_width);
    int y0 = ui_clamp_i(y, 0, fb_height);
    int x1 = ui_clamp_i(x + w, 0, fb_width);
    int y1 = ui_clamp_i(y + h, 0, fb_height);
    if (x0 >= x1 || y0 >= y1) return;

    int r2 = radius * radius;
    int row;
    for (row = y0; row < y1; row++) {
        uint32_t* dst = fb + row * fb_stride + x0;
        int col;
        for (col = 0; col < (x1 - x0); col++) {
            int px = x + col;
            int py = row;

            // Check if pixel is inside the rounded rectangle
            int inside = 1;
            if (px < x + radius && py < y + radius) {
                // Top-left corner
                int dx = (x + radius) - px;
                int dy = (y + radius) - py;
                inside = (dx * dx + dy * dy) <= r2;
            } else if (px >= x + w - radius && py < y + radius) {
                // Top-right corner
                int dx = px - (x + w - radius);
                int dy = (y + radius) - py;
                inside = (dx * dx + dy * dy) <= r2;
            } else if (px < x + radius && py >= y + h - radius) {
                // Bottom-left corner
                int dx = (x + radius) - px;
                int dy = py - (y + h - radius);
                inside = (dx * dx + dy * dy) <= r2;
            } else if (px >= x + w - radius && py >= y + h - radius) {
                // Bottom-right corner
                int dx = px - (x + w - radius);
                int dy = py - (y + h - radius);
                inside = (dx * dx + dy * dy) <= r2;
            }

            if (inside) {
                dst[col] = ui_color_blend(color, dst[col]);
            }
        }
    }
}

// ── Style lookup helpers ────────────────────────────────────────────────

static const char* ui_render_style_string(
    KainNativeUiSession* s, int64_t node_id, const char* key, const char* fallback
) {
    int i;
    for (i = 0; i < ABI_UI_MAX_STYLES; i++) {
        if (s->styles[i].in_use && s->styles[i].node_id == node_id) {
            if (strcmp(s->styles[i].key, key) == 0) {
                if (s->styles[i].value_kind == ABI_UI_STYLE_STRING) {
                    return s->styles[i].string_value;
                }
                break;
            }
        }
    }
    return fallback;
}

static double ui_render_style_f64(
    KainNativeUiSession* s, int64_t node_id, const char* key, double fallback
) {
    int i;
    for (i = 0; i < ABI_UI_MAX_STYLES; i++) {
        if (s->styles[i].in_use && s->styles[i].node_id == node_id) {
            if (strcmp(s->styles[i].key, key) == 0) {
                if (s->styles[i].value_kind == ABI_UI_STYLE_F64) {
                    return s->styles[i].f64_value;
                }
                break;
            }
        }
    }
    return fallback;
}

// ── Recursive render ────────────────────────────────────────────────────

static void ui_render_node(
    KainNativeUiSession* s,
    uint32_t* fb, int fb_w, int fb_h, int fb_stride,
    int64_t node_idx
) {
    if (!s || !fb || node_idx < 0 || node_idx >= ABI_UI_MAX_NODES) return;
    KainNativeUiNode* node = &s->nodes[node_idx];
    if (!node->in_use) return;

    // Skip hidden nodes
    if (node->flags & ABI_UI_NODE_HIDDEN) return;

    int nx = (int)node->x;
    int ny = (int)node->y;
    int nw = (int)node->width;
    int nh = (int)node->height;

    // Skip zero-size nodes
    if (nw <= 0 || nh <= 0) return;

    // ── Resolve styles ──────────────────────────────────────────────
    const char* fill_str   = ui_render_style_string(s, node->id, "fill_color", NULL);
    const char* border_str = ui_render_style_string(s, node->id, "border_color", NULL);
    const char* ink_str    = ui_render_style_string(s, node->id, "ink_color", NULL);
    double border_width    = ui_render_style_f64(s, node->id, "border_width", 0.0);
    double corner_radius   = ui_render_style_f64(s, node->id, "corner_radius", 0.0);
    double opacity         = ui_render_style_f64(s, node->id, "opacity", 1.0);

    // ── Draw background fill ────────────────────────────────────────
    if (fill_str) {
        uint32_t fill_color = ui_parse_color(fill_str);
        if (fill_color != 0 || ui_color_a(ui_parse_color(fill_str)) == 0) {
            // Don't draw fully transparent fills (but allow explicit alpha=0)
            // Actually, let it draw if the color parsed — even transparent is a choice
            fill_color = ui_color_with_opacity(fill_color, opacity);
            if (corner_radius > 0.0) {
                ui_draw_rounded_rect(fb, fb_w, fb_h, fb_stride,
                                     nx, ny, nw, nh, fill_color, (int)corner_radius);
            } else {
                ui_draw_fill_rect(fb, fb_w, fb_h, fb_stride,
                                  nx, ny, nw, nh, fill_color);
            }
        }
    }

    // ── Draw border ─────────────────────────────────────────────────
    if (border_str && border_width > 0.0) {
        uint32_t border_color = ui_parse_color(border_str);
        border_color = ui_color_with_opacity(border_color, opacity);
        ui_draw_border_rect(fb, fb_w, fb_h, fb_stride,
                            nx, ny, nw, nh, border_color, (int)border_width);
    }

    // ── Draw text (placeholder — actual font rendering via font subsystem) ──
    // TODO: integrate ui_font glyph rasterization for text rendering
    // For now, if the node has text and an ink color, draw a small indicator
    if (ink_str && node->text[0]) {
        (void)ink_str;  // text rendering deferred to font subsystem
        // uint32_t ink_color = ui_parse_color(ink_str);
        // ui_draw_text(fb, fb_w, fb_h, fb_stride, nx + 4, ny + 4, node->text, ink_color);
    }

    // ── Render children (depth-first, paint order) ──────────────────
    int i;
    for (i = 0; i < ABI_UI_MAX_NODES; i++) {
        if (s->nodes[i].in_use && s->nodes[i].parent_id == node->id) {
            ui_render_node(s, fb, fb_w, fb_h, fb_stride, i);
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

    // Clear framebuffer to a default dark background
    int i;
    for (i = 0; i < fb_width * fb_height; i++) {
        framebuffer[i] = 0xFF1A1A24;
    }

    // Render root nodes (parent_id == 0)
    int node_idx;
    for (node_idx = 0; node_idx < ABI_UI_MAX_NODES; node_idx++) {
        if (session->nodes[node_idx].in_use && session->nodes[node_idx].parent_id == 0) {
            ui_render_node(session, framebuffer, fb_width, fb_height, fb_stride, node_idx);
        }
    }

    // Also render draw commands if any exist (explicit draw_rect/draw_text/draw_resource calls)
    // These are recorded by std::ui helpers and stored in session->draw_commands[]
    int64_t cmd_idx;
    for (cmd_idx = 0; cmd_idx < session->draw_command_count && cmd_idx < ABI_UI_MAX_DRAW_COMMANDS; cmd_idx++) {
        KainNativeUiDrawCommand* cmd = &session->draw_commands[cmd_idx];

        // Look up the style key for color
        const char* fill_str = ui_render_style_string(session, cmd->node_id, cmd->style_key, NULL);

        if (strcmp(cmd->kind, "rect") == 0 && fill_str) {
            uint32_t fill_color = ui_parse_color(fill_str);
            ui_draw_fill_rect(framebuffer, fb_width, fb_height, fb_stride,
                              (int)cmd->x, (int)cmd->y,
                              (int)cmd->width, (int)cmd->height,
                              fill_color);
        }
        // text and resource draw commands deferred to font/resource subsystems
    }

    return (int64_t)(fb_width * fb_height);
}
