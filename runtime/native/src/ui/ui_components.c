// ============================================================================
//  ui_components.c — High-Level UI Component Primitives Implementation
//  ============================================================================
//  Immediate-mode component library built on top of the Kain rendering
//  substrate (kain_render_software.h) and font subsystem (kain_font.h).
//
//  Each component is a single function that draws itself, handles interaction
//  via the mouse state in UiComponentsContext, and returns meaningful results.
//
//  Inspired by Clay's interaction query pattern: during rendering, each
//  component imperatively checks `ctx->mouse_x/y` against its bounds and
//  `ctx->mouse_down/(mouse_down_prev)` for click transitions.
//
//  No retained-mode node tree calls. No Win32 dependencies. Components
//  accept layout positions from the caller — no internal layout engine.
//  ============================================================================

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "ui_components.h"
#include "kain_render_software.h"
#include "kain_font.h"
#include "kain_geometry.h"
#include "ui_theme.h"

// ══════════════════════════════════════════════════════════════════════════
//  INTERNAL HELPERS
// ══════════════════════════════════════════════════════════════════════════

// ── Hit-test: is the mouse inside the given rect? ───────────────────────
static bool point_in_rect(float px, float py, kainRect r)
{
    return (px >= r.x && px <= r.x + r.w &&
            py >= r.y && py <= r.y + r.h);
}

// ── Clamp a float to [min, max] ─────────────────────────────────────────
static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// ── Apply DPI scale to a dimension ──────────────────────────────────────
static inline float dp(UiComponentsContext* ctx, float v)
{
    return v * ctx->dpi_scale;
}

// ── Draw button background with border ──────────────────────────────────
static void draw_button_bg(UiComponentsContext* ctx, kainRect bounds,
                            kainColor fill, kainColor border)
{
    kain_render_fill_rounded_rect(ctx->renderer, bounds, dp(ctx, 6.0f), fill);
    kain_render_stroke_rect(ctx->renderer, bounds, dp(ctx, 1.0f), border);
}

// ── Draw centered text inside a rect ────────────────────────────────────
// Uses kain_font_* for measurement. Falls back to no-op if no font.
static void draw_text_centered(UiComponentsContext* ctx, kainRect bounds,
                                const char* text, float font_size,
                                kainColor color)
{
    if (!text || !text[0] || ctx->default_font_id <= 0) return;

    float tw = kain_font_measure_text(ctx->session_id, ctx->default_font_id, text);
    float lh = kain_font_line_height(ctx->session_id, ctx->default_font_id);
    float sx = dp(ctx, font_size) / lh; // scale factor
    tw *= sx;

    float lx = bounds.x + (bounds.w - tw) / 2.0f;
    float ly = bounds.y + (bounds.h - lh * sx) / 2.0f;

    kain_render_text(ctx->renderer, kain_point_make(lx, ly),
                     text, ctx->default_font_id, dp(ctx, font_size), color);
}

// ── Draw text left-aligned inside a rect ────────────────────────────────
static void draw_text_left(UiComponentsContext* ctx, kainRect bounds,
                            const char* text, float font_size,
                            kainColor color, float pad)
{
    if (!text || !text[0] || ctx->default_font_id <= 0) return;

    float lh = kain_font_line_height(ctx->session_id, ctx->default_font_id);
    float sx = dp(ctx, font_size) / lh;
    float ly = bounds.y + (bounds.h - lh * sx) / 2.0f;

    kain_render_text(ctx->renderer, kain_point_make(bounds.x + pad, ly),
                     text, ctx->default_font_id, dp(ctx, font_size), color);
}

// ══════════════════════════════════════════════════════════════════════════
//  COMPONENT: ui_button
// ══════════════════════════════════════════════════════════════════════════
//  Draws a rounded-rect button with label. Returns clicked=true on the
//  frame the mouse presses down inside bounds. Hover/pressed states are
//  reflected in the fill color.

UiButtonResult ui_button(UiComponentsContext* ctx, float x, float y,
                          float width, float height, const char* label)
{
    kainRect bounds = {x, y, width, height};
    bool hovered = point_in_rect(ctx->mouse_x, ctx->mouse_y, bounds);

    // Detect press-down transition: mouse is down AND was NOT down
    // on the previous frame, AND is within bounds.
    bool pressed_this_frame = hovered && ctx->mouse_down;

    // Determine visual state
    kainColor fill;
    kainColor text_color;
    if (pressed_this_frame) {
        fill = ctx->theme->button_pressed;
        text_color = ctx->theme->text_secondary;
    } else if (hovered) {
        fill = ctx->theme->button_hover;
        text_color = ctx->theme->button_text;
    } else {
        fill = ctx->theme->button_normal;
        text_color = ctx->theme->button_text;
    }

    // Draw
    draw_button_bg(ctx, bounds, fill, ctx->theme->border);
    draw_text_centered(ctx, bounds, label, 14.0f, text_color);

    UiButtonResult result = {
        .clicked = pressed_this_frame,
        .hovered = hovered,
        .bounds = bounds
    };
    return result;
}

// ══════════════════════════════════════════════════════════════════════════
//  COMPONENT: ui_label
// ══════════════════════════════════════════════════════════════════════════
//  Draws text at the given position. Returns the bounding box.

UiLabelResult ui_label(UiComponentsContext* ctx, float x, float y,
                        float width, float height, const char* text,
                        float font_size)
{
    kainRect bounds = {x, y, width, height};
    draw_text_left(ctx, bounds, text, font_size, ctx->theme->text_primary, 0.0f);

    UiLabelResult result = {.bounds = bounds};
    return result;
}

// ══════════════════════════════════════════════════════════════════════════
//  COMPONENT: ui_slider
// ══════════════════════════════════════════════════════════════════════════
//  Horizontal slider with a filled track and a circular thumb.
//  `value` is an in/out pointer in [0..1].

UiSliderResult ui_slider(UiComponentsContext* ctx, float x, float y,
                          float width, float height, float* value)
{
    kainRect bounds = {x, y, width, height};

    // Clamp input value
    if (*value < 0.0f) *value = 0.0f;
    if (*value > 1.0f) *value = 1.0f;

    // Track geometry
    float track_thickness = dp(ctx, 6.0f);
    float track_y = y + (height - track_thickness) / 2.0f;
    kainRect track_rect = {x, track_y, width, track_thickness};

    // Thumb geometry
    float thumb_radius = height * 0.45f;
    float thumb_cx = x + (*value) * (width - 2.0f * thumb_radius) + thumb_radius;
    float thumb_cy = y + height / 2.0f;

    // Hit testing for thumb drag
    float dx = ctx->mouse_x - thumb_cx;
    float dy = ctx->mouse_y - thumb_cy;
    float thumb_dist = sqrtf(dx * dx + dy * dy);
    bool thumb_hovered = (thumb_dist <= thumb_radius + dp(ctx, 4.0f));
    bool track_hovered = point_in_rect(ctx->mouse_x, ctx->mouse_y, bounds);
    bool hovered = thumb_hovered || track_hovered;

    // Drag logic
    bool changed = false;
    if (ctx->mouse_down && hovered) {
        // Compute new value from mouse X
        float min_cx = x + thumb_radius;
        float max_cx = x + width - thumb_radius;
        float raw_cx = ctx->mouse_x;
        if (raw_cx < min_cx) raw_cx = min_cx;
        if (raw_cx > max_cx) raw_cx = max_cx;
        float new_val = (raw_cx - min_cx) / (max_cx - min_cx);
        if (fabsf(new_val - *value) > 0.001f) {
            *value = new_val;
            changed = true;
        }
    }

    // ── Draw ─────────────────────────────────────────────────────────
    // Track background
    kain_render_fill_rounded_rect(ctx->renderer, track_rect,
                                   track_thickness / 2.0f, ctx->theme->slider_track);

    // Filled portion
    float fill_width = thumb_cx - x - thumb_radius;
    if (fill_width > 0.0f) {
        kainRect filled_rect = {x, track_y, fill_width + thumb_radius, track_thickness};
        kain_render_fill_rounded_rect(ctx->renderer, filled_rect,
                                       track_thickness / 2.0f, ctx->theme->slider_fill);
    }

    // Thumb
    kainColor thumb_color = ctx->mouse_down && hovered ?
                            ctx->theme->slider_fill : ctx->theme->slider_thumb;
    kain_render_fill_circle(ctx->renderer,
                             kain_point_make(thumb_cx, thumb_cy),
                             thumb_radius, thumb_color);
    // Thumb border
    kain_render_stroke_circle(ctx->renderer,
                               kain_point_make(thumb_cx, thumb_cy),
                               thumb_radius, dp(ctx, 1.5f), ctx->theme->border);

    UiSliderResult result = {
        .changed = changed,
        .value = *value,
        .bounds = bounds
    };
    return result;
}

// ══════════════════════════════════════════════════════════════════════════
//  COMPONENT: ui_checkbox
// ══════════════════════════════════════════════════════════════════════════
//  Draws a toggle-able square with checkmark and label.

UiCheckboxResult ui_checkbox(UiComponentsContext* ctx, float x, float y,
                              float width, float height, const char* label,
                              float font_size, bool* value)
{
    kainRect bounds = {x, y, width, height};

    // Checkbox square geometry
    float box_size = height * 0.75f;
    float box_x = x;
    float box_y = y + (height - box_size) / 2.0f;
    kainRect box_rect = {box_x, box_y, box_size, box_size};

    bool hovered = point_in_rect(ctx->mouse_x, ctx->mouse_y, bounds);
    bool clicked = hovered && ctx->mouse_down;
    bool toggled = false;

    if (clicked) {
        *value = !(*value);
        toggled = true;
    }

    // ── Draw ─────────────────────────────────────────────────────────
    // Box background
    if (*value) {
        kain_render_fill_rounded_rect(ctx->renderer, box_rect,
                                       dp(ctx, 3.0f), ctx->theme->accent);
    } else {
        kain_render_fill_rounded_rect(ctx->renderer, box_rect,
                                       dp(ctx, 3.0f), ctx->theme->bg_tertiary);
    }
    // Box border
    kain_render_stroke_rect(ctx->renderer, box_rect,
                             dp(ctx, 1.0f), ctx->theme->border);

    // Checkmark when checked (two strokes forming ✓)
    if (*value) {
        float cx = box_x + dp(ctx, 3.0f);
        float cy = box_y + dp(ctx, 2.0f);
        float cs = box_size - dp(ctx, 5.0f);
        // Simple checkmark: two thick pixels
        int steps = (int)(cs * 0.6f);
        if (steps < 1) steps = 1;
        for (int i = 0; i < steps; i++) {
            float sx1 = cx + (float)i;
            float sy1 = cy + (float)i;
            float sx2 = cx + (float)(steps + i / 2);
            float sy2 = cy + (float)(steps - i);
            // These are approximate; a real checkmark would use line drawing
            if (i < steps / 2) {
                kain_render_fill_rect(ctx->renderer,
                    kain_rect_make(sx1, sy1, dp(ctx, 2.0f), dp(ctx, 2.0f)),
                    ctx->theme->text_primary);
            }
            if (i >= steps / 2) {
                int j = i - steps / 2;
                float sx2b = cx + (float)(steps / 2 + j);
                float sy2b = cy + (float)(steps / 2 - j / 2);
                kain_render_fill_rect(ctx->renderer,
                    kain_rect_make(sx2b, sy2b, dp(ctx, 2.0f), dp(ctx, 2.0f)),
                    ctx->theme->text_primary);
            }
            (void)sx2; (void)sy2;
        }
    }

    // Label
    if (label && label[0]) {
        float label_x = box_x + box_size + dp(ctx, 6.0f);
        kainRect label_bounds = {label_x, y, width - (label_x - x), height};
        draw_text_left(ctx, label_bounds, label, font_size, ctx->theme->text_primary, 0.0f);
    }

    UiCheckboxResult result = {
        .toggled = toggled,
        .value = *value,
        .bounds = bounds
    };
    return result;
}

// ══════════════════════════════════════════════════════════════════════════
//  COMPONENT: ui_panel
// ══════════════════════════════════════════════════════════════════════════
//  Draws a titled container with optional close button. Returns content
//  bounds so the caller can position children.

UiPanelResult ui_panel(UiComponentsContext* ctx, float x, float y,
                        float width, float height, const char* title,
                        bool* open)
{
    kainRect bounds = {x, y, width, height};

    // Title bar geometry
    float title_bar_h = dp(ctx, 28.0f);
    kainRect title_rect = {x + dp(ctx, 1.0f), y + dp(ctx, 1.0f),
                           width - dp(ctx, 2.0f), title_bar_h};

    // Content area (below title bar)
    float content_pad = dp(ctx, 8.0f);
    kainRect content_rect = {x + content_pad, y + title_bar_h + content_pad,
                             width - 2.0f * content_pad,
                             height - title_bar_h - 2.0f * content_pad};

    // ── Draw panel background ────────────────────────────────────────
    kain_render_fill_rounded_rect(ctx->renderer, bounds, dp(ctx, 6.0f),
                                   ctx->theme->bg_secondary);
    kain_render_stroke_rect(ctx->renderer, bounds, dp(ctx, 1.0f),
                             ctx->theme->border);

    // ── Draw title bar ───────────────────────────────────────────────
    kain_render_fill_rect(ctx->renderer, title_rect, ctx->theme->bg_tertiary);
    // Accent line under title
    kain_render_fill_rect(ctx->renderer,
        kain_rect_make(x, y + title_bar_h, width, dp(ctx, 1.0f)),
        ctx->theme->accent);

    // Title text
    if (title && title[0]) {
        draw_text_left(ctx, title_rect, title, 13.0f, ctx->theme->text_primary, dp(ctx, 8.0f));
    }

    // ── Close button ─────────────────────────────────────────────────
    bool keep_open = true;
    if (open) {
        // Close button (X) in top-right corner
        float close_size = dp(ctx, 18.0f);
        float close_x = x + width - close_size - dp(ctx, 6.0f);
        float close_y = y + (title_bar_h - close_size) / 2.0f;
        kainRect close_rect = {close_x, close_y, close_size, close_size};

        bool close_hovered = point_in_rect(ctx->mouse_x, ctx->mouse_y, close_rect);
        bool close_clicked = close_hovered && ctx->mouse_down;

        // Draw close button
        kainColor close_fill = close_hovered ? ctx->theme->accent_hover : ctx->theme->error;
        kain_render_fill_rounded_rect(ctx->renderer, close_rect,
                                       dp(ctx, 4.0f), close_fill);

        // Draw X mark (two crossing lines)
        float x_inset = dp(ctx, 5.0f);
        float x_thick = dp(ctx, 1.5f);
        float x1 = close_x + x_inset;
        float y1 = close_y + x_inset;
        float x2 = close_x + close_size - x_inset;
        float y2 = close_y + close_size - x_inset;
        kain_render_fill_rect(ctx->renderer,
            kain_rect_make(x1, y1, x2 - x1, x_thick), ctx->theme->text_primary);
        kain_render_fill_rect(ctx->renderer,
            kain_rect_make(x1, y2 - x_thick, x2 - x1, x_thick), ctx->theme->text_primary);

        if (close_clicked) {
            *open = false;
            keep_open = false;
        }
    }

    UiPanelResult result = {
        .open = keep_open,
        .title_bounds = title_rect,
        .content_bounds = content_rect
    };
    return result;
}

// ══════════════════════════════════════════════════════════════════════════
//  COMPONENT: ui_progress
// ══════════════════════════════════════════════════════════════════════════
//  Draws a horizontal progress bar with percentage text.

UiProgressResult ui_progress(UiComponentsContext* ctx, float x, float y,
                              float width, float height, float value)
{
    kainRect bounds = {x, y, width, height};

    // Clamp value to [0..1]
    float clamped = clampf(value, 0.0f, 1.0f);

    // ── Draw ─────────────────────────────────────────────────────────
    // Background
    kain_render_fill_rounded_rect(ctx->renderer, bounds, dp(ctx, 4.0f),
                                   ctx->theme->slider_track);

    // Filled portion
    float fill_width = clamped * width;
    if (fill_width > dp(ctx, 2.0f)) {
        kain_render_fill_rounded_rect(ctx->renderer,
            kain_rect_make(x, y, fill_width, height),
            dp(ctx, 4.0f), ctx->theme->slider_fill);
    }

    // Percentage text centered on the bar
    char pct_str[16];
    int pct = (int)(clamped * 100.0f + 0.5f);
    int n = snprintf(pct_str, sizeof(pct_str), "%d%%", pct);
    if (n > 0 && ctx->default_font_id > 0) {
        draw_text_centered(ctx, bounds, pct_str, 12.0f, ctx->theme->text_primary);
    }

    UiProgressResult result = {.bounds = bounds};
    return result;
}

// ══════════════════════════════════════════════════════════════════════════
//  LAYOUT HELPERS
// ══════════════════════════════════════════════════════════════════════════
//  Simple row/column cursor advancement. Not a full layout engine —
//  just position calculation helpers.

float ui_layout_row(float start_x, float y, int count,
                     const float* widths, float gap)
{
    float cx = start_x;
    for (int i = 0; i < count; i++) {
        cx += widths[i] + gap;
    }
    return cx - gap; // position after last item
}

float ui_layout_column(float x, float start_y, int count,
                        const float* heights, float gap)
{
    float cy = start_y;
    for (int i = 0; i < count; i++) {
        cy += heights[i] + gap;
    }
    return cy - gap;
}
