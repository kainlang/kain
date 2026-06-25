// ============================================================================
//  ui_widget.c — Kain Native UI Widget Library Implementation
//  ============================================================================
//  Immediate-mode widget system built on top of the Kain retained-mode ABI.
//  Each widget draws directly into the DIB framebuffer and renders text
//  via GDI. Nodes are created through the ABI for hit-testing and state
//  tracking.
//
//  References:
//    ui_system.h          — ABI functions (session, node, style, event)
//    ui_system_internal.h — Internal session struct for host access
//    ui_host_adapter.c    — Win32 DIB framebuffer + HDC
//    microui.h/c          — Design inspiration (immediate-mode widget API)
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ui_widget.h"
#include "ui_system.h"          /* from -I../../../include */
#include "ui_system_internal.h"  /* from -I.. */
#include "ui_font.h"             /* new font ABI */
#include "ui_color.h"            /* ui_color_blend */

// ── Win32 Host struct (must match ui_host_adapter.c exactly) ───────────
typedef struct KainWin32UiHost {
    HWND hwnd;
    int width;
    int height;
    int running;
    int initialized;
    uint8_t* framebuffer;
    int fb_stride;
    HDC hdc_buffer;
    HBITMAP hbitmap;
    int64_t session_id;
    int64_t input_session_id;
    float dpi_scale;
} KainWin32UiHost;

// ── Extern stubs from core.c ──────────────────────────────────────────
extern double kain_clampd(double value, double min_value, double max_value);

// ── DPI scaling macro: multiply a logical value by dpi_scale ─────────
#define DS(ctx, v) ((int)((v) * (ctx)->dpi_scale + 0.5))
#define DSD(ctx, v) ((v) * (ctx)->dpi_scale)

// ── Slider / progress bar colors (not in header — use constants) ──────
#define SLIDER_TRACK_COLOR  0xFF3A3A5C
#define SLIDER_FILL_COLOR   0xFF21D4A1
#define PROGRESS_BG_COLOR   0xFF3A3A5C
#define PROGRESS_FILL_COLOR 0xFF21D4A1
#define TEXTBOX_BG_COLOR    0xFF0A0A14

// ============================================================================
//  PIXEL DRAWING HELPERS
// ============================================================================

void ui_widget_fill_rect(uint32_t* fb, int stride, int fb_w, int fb_h,
                         int x, int y, int w, int h, uint32_t color)
{
    if (!fb || w <= 0 || h <= 0) return;
    for (int r = y; r < y + h && r < fb_h; r++) {
        if (r < 0) continue;
        for (int c = x; c < x + w && c < fb_w; c++) {
            if (c < 0) continue;
            fb[r * stride + c] = color;
        }
    }
}

void ui_widget_fill_rounded_rect(uint32_t* fb, int stride, int fb_w, int fb_h,
                                 int x, int y, int w, int h,
                                 uint32_t color, int r)
{
    if (!fb || w <= 0 || h <= 0) return;
    if (r <= 0) {
        ui_widget_fill_rect(fb, stride, fb_w, fb_h, x, y, w, h, color);
        return;
    }
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    int r2 = r * r;

    for (int row = y; row < y + h && row < fb_h; row++) {
        if (row < 0) continue;
        for (int col = x; col < x + w && col < fb_w; col++) {
            if (col < 0) continue;

            int inside = 1;
            // Top-left corner
            if (col < x + r && row < y + r) {
                int dx = (x + r) - col - 1;
                int dy = (y + r) - row - 1;
                inside = (dx >= 0 && dy >= 0 && dx * dx + dy * dy <= r2);
            }
            // Top-right corner
            else if (col >= x + w - r && row < y + r) {
                int dx = col - (x + w - r) + 1;
                int dy = (y + r) - row - 1;
                inside = (dx >= 0 && dy >= 0 && dx * dx + dy * dy <= r2);
            }
            // Bottom-left corner
            else if (col < x + r && row >= y + h - r) {
                int dx = (x + r) - col - 1;
                int dy = row - (y + h - r) + 1;
                inside = (dx >= 0 && dy >= 0 && dx * dx + dy * dy <= r2);
            }
            // Bottom-right corner
            else if (col >= x + w - r && row >= y + h - r) {
                int dx = col - (x + w - r) + 1;
                int dy = row - (y + h - r) + 1;
                inside = (dx >= 0 && dy >= 0 && dx * dx + dy * dy <= r2);
            }

            if (inside) {
                fb[row * stride + col] = color;
            }
        }
    }
}

// ── Draw a thin rectangle border (1px) ────────────────────────────────
static void draw_border_rect(uint32_t* fb, int stride, int fb_w, int fb_h,
                             int x, int y, int w, int h, uint32_t color)
{
    ui_widget_fill_rect(fb, stride, fb_w, fb_h, x, y, w, 1, color);           // top
    ui_widget_fill_rect(fb, stride, fb_w, fb_h, x, y + h - 1, w, 1, color);  // bottom
    ui_widget_fill_rect(fb, stride, fb_w, fb_h, x, y, 1, h, color);          // left
    ui_widget_fill_rect(fb, stride, fb_w, fb_h, x + w - 1, y, 1, h, color);  // right
}

// ── Draw a simple checkmark ────────────────────────────────────────────
static void draw_checkmark(uint32_t* fb, int stride, int fb_w, int fb_h,
                           int x, int y, int size, uint32_t color)
{
    // Simple checkmark: two strokes forming a ✓
    // Left stroke (short upward tick)
    int cw = size / 4;
    int ch = size / 3;
    if (ch < 1) ch = 1;
    for (int i = 0; i < ch; i++) {
        int px = x + cw;
        int py = y + size - 1 - i;
        if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
            fb[py * stride + px] = color;
    }
    // Right stroke (longer outward stroke)
    for (int i = 0; i < cw + ch; i++) {
        int px = x + cw + i;
        int py = y + size - 1 - ch + (i / 2);
        if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
            fb[py * stride + px] = color;
    }
}

// ── Draw an X mark ─────────────────────────────────────────────────────
static void draw_x_mark(uint32_t* fb, int stride, int fb_w, int fb_h,
                        int x, int y, int size, uint32_t color)
{
    for (int i = 0; i < size; i++) {
        int px1 = x + i, py1 = y + i;
        int px2 = x + i, py2 = y + size - 1 - i;
        if (px1 >= 0 && px1 < fb_w && py1 >= 0 && py1 < fb_h)
            fb[py1 * stride + px1] = color;
        if (px2 >= 0 && px2 < fb_w && py2 >= 0 && py2 < fb_h)
            fb[py2 * stride + px2] = color;
    }
}

// ============================================================================
//  STB TRUETYPE TEXT HELPERS
// ============================================================================

// Render a single glyph alpha-mask bitmap into the framebuffer.
// Uses ui_color_blend() for per-pixel alpha blending.
// Z3-proven: ui_color_blend uses div255_fast (shift+add, ~5 cycles vs ~25 for DIV).
static void render_glyph_bitmap(uint32_t* fb, int stride, int fb_w, int fb_h,
                                 int gx, int gy,
                                 const KainUiGlyph* glyph, uint32_t color)
{
    if (!glyph || !glyph->bitmap) return;
    for (int row = 0; row < glyph->height; row++) {
        int py = gy + row;
        if (py < 0 || py >= fb_h) continue;
        for (int col = 0; col < glyph->width; col++) {
            int px = gx + col;
            if (px < 0 || px >= fb_w) continue;
            uint8_t alpha = glyph->bitmap[row * glyph->width + col];
            if (alpha > 0) {
                // Pack source color with glyph alpha, blend over destination
                uint32_t src = (color & 0x00FFFFFF) | ((uint32_t)alpha << 24);
                fb[py * stride + px] = ui_color_blend(src, fb[py * stride + px]);
            }
        }
    }
}

// Draw text at (x,y) using stb_truetype glyph rasterization.
// x,y is the baseline position (y = baseline, typically font ascent pixels
// below the top of the text).
// size parameter is preserved for API compatibility but ignored —
// font size is baked in at load time via abi_ui_font_load_ttf().
// Helper: get the active font resource ID, or 0 if no font is loaded.
static int64_t widget_font_id(KainUiWidgetContext* ctx)
{
    if (!ctx || ctx->default_font < 0 || ctx->default_font >= ctx->font_count)
        return 0;
    return ctx->fonts[ctx->default_font].font_id;
}

void ui_widget_draw_text(KainUiWidgetContext* ctx, int x, int y,
                         const char* text, uint32_t color, int size)
{
    (void)size;
    if (!ctx || !ctx->host || !text || !text[0]) return;

    int64_t font_id = widget_font_id(ctx);
    if (font_id <= 0) return;

    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4;
    int fb_w = ctx->host->width;
    int fb_h = ctx->host->height;
    if (!fb) return;

    int pen_x = x;
    int baseline_y = y;

    for (const char* p = text; *p; ) {
        int codepoint = (unsigned char)*p++;
        if (codepoint == '\n') {
            pen_x = x;
            double h = abi_ui_text_measure_height(ctx->session_id, font_id, "");
            baseline_y += (int)(h + 0.5);
            continue;
        }
        if (codepoint == ' ') {
            // Approximate space width using a narrow character measurement
            pen_x += (int)(abi_ui_text_measure_width(ctx->session_id, font_id, "i") + 0.5);
            continue;
        }

        KainUiGlyph* glyph = abi_ui_font_get_glyph(ctx->session_id, font_id, codepoint);
        if (glyph) {
            if (glyph->bitmap) {
                render_glyph_bitmap(fb, stride, fb_w, fb_h,
                                    pen_x + glyph->x_offset, baseline_y + glyph->y_offset,
                                    glyph, color);
            }
            pen_x += glyph->advance;
            abi_ui_font_release_glyph(glyph);
        } else {
            // Glyph not available — advance by approximate width
            pen_x += (int)(abi_ui_text_measure_width(ctx->session_id, font_id, "i") + 0.5);
        }
    }
}

// Draw text with a specific font resource ID (0 = default font).
void ui_widget_draw_text_ex(KainUiWidgetContext* ctx, int x, int y,
                            const char* text, uint32_t color, int size,
                            int64_t font_id)
{
    if (font_id <= 0) {
        ui_widget_draw_text(ctx, x, y, text, color, size);
        return;
    }
    if (!ctx || !ctx->host || !text || !text[0]) return;

    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4;
    int fb_w = ctx->host->width;
    int fb_h = ctx->host->height;
    if (!fb) return;

    int pen_x = x;
    int baseline_y = y;

    for (const char* p = text; *p; ) {
        int codepoint = (unsigned char)*p++;
        if (codepoint == '\n') {
            pen_x = x;
            double h = abi_ui_text_measure_height(ctx->session_id, font_id, "");
            baseline_y += (int)(h + 0.5);
            continue;
        }
        if (codepoint == ' ') {
            pen_x += (int)(abi_ui_text_measure_width(ctx->session_id, font_id, "i") + 0.5);
            continue;
        }

        KainUiGlyph* glyph = abi_ui_font_get_glyph(ctx->session_id, font_id, codepoint);
        if (glyph) {
            if (glyph->bitmap) {
                render_glyph_bitmap(fb, stride, fb_w, fb_h,
                                    pen_x + glyph->x_offset, baseline_y + glyph->y_offset,
                                    glyph, color);
            }
            pen_x += glyph->advance;
            abi_ui_font_release_glyph(glyph);
        } else {
            pen_x += (int)(abi_ui_text_measure_width(ctx->session_id, font_id, "i") + 0.5);
        }
    }
}

// Draw text centered within a rectangle using stb_truetype glyph rasterization.
void ui_widget_draw_text_centered(KainUiWidgetContext* ctx,
                                  int x, int y, int w, int h,
                                  const char* text, uint32_t color, int size)
{
    (void)size;
    if (!ctx || !ctx->host || !text || !text[0]) return;

    int64_t font_id = widget_font_id(ctx);
    if (font_id <= 0) return;

    int tw = ui_widget_text_width(ctx, text);
    double th = abi_ui_text_measure_height(ctx->session_id, font_id, text);
    int tx = x + (w - tw) / 2;
    // Approximate baseline: half the text height below the rect top, then
    // add roughly ascent (~80% of height) to get baseline from top
    int ty = y + (int)((h - th) / 2 + th * 0.8 + 0.5);
    ui_widget_draw_text(ctx, tx, ty, text, color, 0);
}

// Measure text width in pixels using the font ABI.
int ui_widget_text_width(KainUiWidgetContext* ctx, const char* text)
{
    if (!ctx || !text || !text[0]) return 0;
    if (ctx->default_font < 0 || ctx->default_font >= ctx->font_count)
        return (int)strlen(text) * 7;
    double w = abi_ui_text_measure_width(
        ctx->session_id, ctx->fonts[ctx->default_font].font_id, text);
    return (int)(w + 0.5);
}

// ============================================================================
//  WIDGET CONTEXT LIFECYCLE
// ============================================================================

KainUiWidgetContext* ui_widget_create(int64_t session_id)
{
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) return NULL;

    KainWin32UiHost* host = (KainWin32UiHost*)session->host_state;
    if (!host) return NULL;

    KainUiWidgetContext* ctx = (KainUiWidgetContext*)calloc(1, sizeof(KainUiWidgetContext));
    if (!ctx) return NULL;

    ctx->session_id = session_id;
    ctx->host = host;
    ctx->session = (struct KainNativeUiSession*)session;
    ctx->dpi_scale = session->dpi_scale > 0.0 ? session->dpi_scale : 1.0;

    // Dark theme defaults
    ctx->color_bg          = UI_COLOR_BG;
    ctx->color_surface     = UI_COLOR_SURFACE;
    ctx->color_surface2    = UI_COLOR_SURFACE2;
    ctx->color_header      = UI_COLOR_HEADER;
    ctx->color_accent      = UI_COLOR_ACCENT;
    ctx->color_accent2     = UI_COLOR_ACCENT2;
    ctx->color_text        = UI_COLOR_TEXT;
    ctx->color_text_dim    = UI_COLOR_TEXT_DIM;
    ctx->color_border      = UI_COLOR_BORDER;
    ctx->color_button      = UI_COLOR_BUTTON;
    ctx->color_button_hover = UI_COLOR_BUTTON_HL;
    ctx->color_button_pressed = UI_COLOR_BUTTON_PR;
    ctx->color_title_bg    = UI_COLOR_TITLE_BG;
    ctx->color_input_bg    = UI_COLOR_INPUT_BG;

    // Font table: no fonts loaded by default. User must call
    // ui_widget_load_default_font() or ui_widget_load_font() explicitly.
    ctx->default_font = -1;
    ctx->font_count = 0;

    // No auto-load — user must call ui_widget_load_default_font() or
    // ui_widget_load_font() explicitly. This keeps the library
    // cross-platform and gives the caller control.

    return ctx;
}

void ui_widget_destroy(KainUiWidgetContext* ctx)
{
    if (ctx) {
        // Free all loaded TTF data
        for (int i = 0; i < ctx->font_count; i++) {
            if (ctx->fonts[i].ttf_data) {
                free(ctx->fonts[i].ttf_data);
                ctx->fonts[i].ttf_data = NULL;
            }
        }
        free(ctx);
    }
}

// ============================================================================
//  FONT LOADING
// ============================================================================

int64_t ui_widget_load_font(KainUiWidgetContext* ctx, const char* filepath, double size)
{
    if (!ctx || !filepath) return 0;
    if (ctx->font_count >= UI_WIDGET_MAX_FONTS) return 0;

    // Scale font size by DPI so text appears correctly sized on high-DPI displays
    double scaled_size = DSD(ctx, size);

    FILE* f = fopen(filepath, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    int64_t font_id = 0;
    if (len > 0 && len < 16 * 1024 * 1024) { // sanity: max 16 MB
        uint8_t* data = (uint8_t*)malloc((size_t)len);
        if (data) {
            size_t nread = fread(data, 1, (size_t)len, f);
            if (nread == (size_t)len) {
                font_id = abi_ui_font_load_ttf(
                    ctx->session_id, "", "", scaled_size, data, (int64_t)len);
                if (font_id > 0) {
                    int idx = ctx->font_count++;
                    ctx->fonts[idx].font_id = font_id;
                    ctx->fonts[idx].ttf_data = data;
                    ctx->fonts[idx].ttf_len = (int)len;
                    data = NULL; // ownership transferred
                }
            }
            if (data) free(data);
        }
    }
    fclose(f);
    return font_id;
}

int64_t ui_widget_load_default_font(KainUiWidgetContext* ctx, double size)
{
    if (!ctx) return 0;

#ifdef _WIN32
    const char* font_paths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "C:/Windows/Fonts/consola.ttf",
        NULL
    };
#elif defined(__APPLE__)
    const char* font_paths[] = {
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/SFNS.ttf",
        "/Library/Fonts/Arial.ttf",
        NULL
    };
#else // Linux / POSIX
    const char* font_paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        NULL
    };
#endif

    for (int i = 0; font_paths[i]; i++) {
        int64_t fid = ui_widget_load_font(ctx, font_paths[i], size);
        if (fid > 0) {
            ctx->default_font = ctx->font_count - 1;
            return fid;
        }
    }

    ctx->default_font = -1;
    return 0;
}

// ============================================================================
//  FRAME LIFECYCLE
// ============================================================================

void ui_widget_begin_frame(KainUiWidgetContext* ctx)
{
    if (!ctx || !ctx->host) return;

    // Save previous mouse state
    ctx->mouse_down_prev = ctx->mouse_down;

    // Get current mouse position (Win32)
    POINT pt;
    if (GetCursorPos(&pt) && ScreenToClient(ctx->host->hwnd, &pt)) {
        ctx->mouse_x = (double)pt.x;
        ctx->mouse_y = (double)pt.y;
    }

    // Get mouse button state
    ctx->mouse_down = (GetKeyState(VK_LBUTTON) & 0x8000) ? 1 : 0;

    // Hit-test for hovered node
    ctx->hovered_node = abi_ui_hit_test(ctx->session_id, ctx->mouse_x, ctx->mouse_y);

    // NOTE: pressed_node is managed entirely by individual widgets.
    // Widgets set pressed_node on press (if hovered), and clear it
    // when they handle a matching release. The end_frame cleanup
    // handles stale pressed_node after unhandled releases.
    // DO NOT clear pressed_node here.

    // Reset per-frame counters
    ctx->widget_counter = 0;
    ctx->edit_changed = 0;

    // Reset layout
    ctx->layout_x = 0;
    ctx->layout_y = 0;
    ctx->layout_next_y = 0;
    ctx->layout_type = 0;
    ctx->layout_item = 0;
    ctx->layout_count = 0;
    ctx->layout_size_count = 0;

    // Clear container stack
    ctx->container_depth = 0;
}

void ui_widget_end_frame(KainUiWidgetContext* ctx)
{
    if (!ctx) return;

    // Cleanup: if mouse was released and pressed_node is still set
    // (no widget handled the release), clear it for next frame.
    if (!ctx->mouse_down && ctx->mouse_down_prev) {
        ctx->pressed_node = 0;
    }
}

// ============================================================================
//  LAYOUT SYSTEM
// ============================================================================

void ui_layout_row(KainUiWidgetContext* ctx, int count, const int* widths)
{
    if (!ctx || !widths || count <= 0) return;
    if (count > UI_MAX_LAYOUT_ITEMS) count = UI_MAX_LAYOUT_ITEMS;

    ctx->layout_type = 0; // row
    ctx->layout_count = count;
    ctx->layout_item = 0;
    ctx->layout_size_count = count;

    // Seed position from current container or previous layout
    if (ctx->container_depth > 0) {
        UiContainer* c = &ctx->container_stack[ctx->container_depth - 1];
        ctx->layout_x = c->cursor_x;
        ctx->layout_y = c->cursor_y;
    }

    for (int i = 0; i < count; i++) {
        ctx->layout_sizes[i] = DS(ctx, widths[i]);
    }
}

void ui_layout_column(KainUiWidgetContext* ctx, int count, const int* heights)
{
    if (!ctx || !heights || count <= 0) return;
    if (count > UI_MAX_LAYOUT_ITEMS) count = UI_MAX_LAYOUT_ITEMS;

    ctx->layout_type = 1; // column
    ctx->layout_count = count;
    ctx->layout_item = 0;
    ctx->layout_size_count = count;

    if (ctx->container_depth > 0) {
        UiContainer* c = &ctx->container_stack[ctx->container_depth - 1];
        ctx->layout_x = c->cursor_x;
        ctx->layout_y = c->cursor_y;
    }

    for (int i = 0; i < count; i++) {
        ctx->layout_sizes[i] = DS(ctx, heights[i]);
    }
}

void ui_layout_set_next(KainUiWidgetContext* ctx, int width, int height)
{
    if (!ctx) return;
    ctx->layout_type = 2; // explicit single-item
    ctx->layout_size_count = 2;
    ctx->layout_sizes[0] = DS(ctx, width);
    ctx->layout_sizes[1] = DS(ctx, height);
}

// ── Get next widget rect from layout, with defaults ──────────────────
static void layout_next_slot(KainUiWidgetContext* ctx, int* out_x, int* out_y,
                             int* out_w, int* out_h, int def_w, int def_h)
{
    int x, y, w = def_w, h = def_h;

    if (ctx->container_depth > 0) {
        UiContainer* c = &ctx->container_stack[ctx->container_depth - 1];
        x = (int)c->cursor_x;
        y = (int)c->cursor_y;
    } else {
        x = (int)ctx->layout_x;
        y = (int)ctx->layout_y;
    }

    if (ctx->layout_type == 0 && ctx->layout_count > 0) {
        // Row: use specified width per-item
        if (ctx->layout_item < ctx->layout_count) {
            w = ctx->layout_sizes[ctx->layout_item];
        }
    } else if (ctx->layout_type == 1 && ctx->layout_count > 0) {
        // Column: use specified height per-item
        if (ctx->layout_item < ctx->layout_count) {
            h = ctx->layout_sizes[ctx->layout_item];
        }
    } else if (ctx->layout_type == 2) {
        // Explicit: [0]=width, [1]=height
        w = ctx->layout_sizes[0];
        h = ctx->layout_sizes[1];
        ctx->layout_type = 0; // one-shot
    }

    *out_x = x;
    *out_y = y;
    *out_w = w;
    *out_h = h;
}

// ── Advance layout cursor after placing a widget ─────────────────────
static void layout_advance(KainUiWidgetContext* ctx, int used_w, int used_h)
{
    int spacing = DS(ctx, UI_SPACING);
    int padding = DS(ctx, UI_PADDING);
    if (ctx->container_depth > 0) {
        UiContainer* c = &ctx->container_stack[ctx->container_depth - 1];

        if (ctx->layout_type == 0 && ctx->layout_count > 0) {
            // Row layout: advance horizontally
            c->cursor_x += used_w + spacing;
            ctx->layout_item++;
            if (ctx->layout_item >= ctx->layout_count) {
                c->cursor_y += used_h + spacing;
                c->cursor_x = c->x + padding;
                ctx->layout_item = 0;
                ctx->layout_count = 0;
            }
        } else {
            // Default: advance horizontally with auto-wrap
            c->cursor_x += used_w + spacing;
            // Check if next widget would overflow (rough check)
            if (c->cursor_x + DS(ctx, UI_BUTTON_WIDTH) > c->x + c->w) {
                c->cursor_x = c->x + padding;
                c->cursor_y += used_h + spacing;
            }
        }
        ctx->layout_x = c->cursor_x;
        ctx->layout_y = c->cursor_y;
    } else {
        // Top-level: advance horizontally
        ctx->layout_x += used_w + spacing;
        ctx->layout_next_y = fmax(ctx->layout_next_y,
                                   (double)((int)ctx->layout_y + used_h + spacing));
    }
}

// ── Hit-test helpers ──────────────────────────────────────────────────
static int point_in_rect(double px, double py, double rx, double ry, double rw, double rh)
{
    return (px >= rx && px < rx + rw && py >= ry && py < ry + rh);
}

static int is_hovered(KainUiWidgetContext* ctx, double x, double y, double w, double h)
{
    return point_in_rect(ctx->mouse_x, ctx->mouse_y, x, y, w, h);
}

// ── Stable key helpers ────────────────────────────────────────────────
static void widget_key(KainUiWidgetContext* ctx, const char* prefix,
                       char* out, int out_size)
{
    snprintf(out, out_size, "uiw_%s_%d", prefix, ctx->widget_counter);
}

static int64_t widget_node(KainUiWidgetContext* ctx, const char* key,
                           const char* kind, int x, int y, int w, int h)
{
    int64_t nid = abi_ui_node_find_by_stable_key(ctx->session_id, key);
    if (nid <= 0) {
        nid = abi_ui_node_create(ctx->session_id, kind ? kind : "widget");
        if (nid > 0) {
            abi_ui_node_set_stable_key(ctx->session_id, nid, key);
            if (ctx->container_depth > 0) {
                abi_ui_node_set_parent(ctx->session_id, nid,
                    ctx->container_stack[ctx->container_depth - 1].node_id);
            }
        }
    }
    if (nid > 0) {
        abi_ui_node_set_rect(ctx->session_id, nid, (double)x, (double)y, (double)w, (double)h);
    }
    return nid;
}

// ============================================================================
//  WIDGET: ui_button
// ============================================================================

int ui_button(KainUiWidgetContext* ctx, const char* label)
{
    if (!ctx || !ctx->host) return 0;

    int x, y, w, h;
    layout_next_slot(ctx, &x, &y, &w, &h, DS(ctx, UI_BUTTON_WIDTH), DS(ctx, UI_BUTTON_HEIGHT));

    char key[UI_WIDGET_KEY_SIZE];
    widget_key(ctx, "btn", key, sizeof(key));
    int64_t nid = widget_node(ctx, key, "widget.button", x, y, w, h);
    ctx->widget_counter++;

    int hovered = is_hovered(ctx, (double)x, (double)y, (double)w, (double)h);
    int clicked = 0;
    int pressed = (ctx->pressed_node == nid);

    // If mouse just went down on this button, claim press
    if (ctx->mouse_down && !ctx->mouse_down_prev && hovered) {
        ctx->pressed_node = nid;
        pressed = 1;
    }

    // If mouse just came up and was pressed on this button → click!
    if (!ctx->mouse_down && ctx->mouse_down_prev) {
        if (ctx->pressed_node == nid) {
            if (hovered) clicked = 1;
            ctx->pressed_node = 0;
            pressed = 0;
        }
    }

    // Determine color based on state
    uint32_t color;
    if (pressed && ctx->mouse_down) {
        color = ctx->color_button_pressed;
    } else if (hovered) {
        color = ctx->color_button_hover;
    } else {
        color = ctx->color_button;
    }

    // Draw button
    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4;
    int fb_w = ctx->host->width;
    int fb_h = ctx->host->height;

    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, y, w, h, color, DS(ctx, 4));
    ui_widget_draw_text_centered(ctx, x, y, w, h, label, ctx->color_text, 14);

    // Sync node style for fallback renderer
    abi_ui_node_set_style_string(ctx->session_id, nid, "fill_color",
        "#303050");

    layout_advance(ctx, w, h);
    return clicked;
}

// ============================================================================
//  WIDGET: ui_label
// ============================================================================

int64_t ui_label(KainUiWidgetContext* ctx, const char* text)
{
    if (!ctx || !ctx->host || !text) return 0;

    int tw = text[0] ? ui_widget_text_width(ctx, text) : 0;
    int w = (tw > 0) ? tw + DS(ctx, 4) : DS(ctx, 20);
    int h = DS(ctx, UI_LABEL_HEIGHT);

    int x, y;
    layout_next_slot(ctx, &x, &y, &w, &h, w, h);

    char key[UI_WIDGET_KEY_SIZE];
    widget_key(ctx, "lbl", key, sizeof(key));
    int64_t nid = widget_node(ctx, key, "widget.label", x, y, w, h);
    ctx->widget_counter++;

    ui_widget_draw_text(ctx, x, y + DS(ctx, 2), text, ctx->color_text, 14);

    abi_ui_node_set_text(ctx->session_id, nid, text);

    layout_advance(ctx, w, h);
    return nid;
}

// ============================================================================
//  WIDGET: ui_checkbox
// ============================================================================

int ui_checkbox(KainUiWidgetContext* ctx, const char* label, int* value)
{
    if (!ctx || !ctx->host || !value) return 0;

    int cs = DS(ctx, UI_CHECKBOX_SIZE);
    int tw = (label && label[0]) ? ui_widget_text_width(ctx, label) + DS(ctx, 4) : 0;
    int w = cs + DS(ctx, 6) + tw;
    int h = cs + DS(ctx, 4);

    int x, y;
    layout_next_slot(ctx, &x, &y, &w, &h, w, h);

    char key[UI_WIDGET_KEY_SIZE];
    widget_key(ctx, "chk", key, sizeof(key));
    int64_t nid = widget_node(ctx, key, "widget.checkbox", x, y, w, h);
    ctx->widget_counter++;

    int hovered = is_hovered(ctx, (double)x, (double)y, (double)w, (double)h);
    int toggled = 0;

    if (ctx->mouse_down && !ctx->mouse_down_prev && hovered) {
        ctx->pressed_node = nid;
    }
    if (!ctx->mouse_down && ctx->mouse_down_prev) {
        if (ctx->pressed_node == nid) {
            if (hovered) {
                *value = !(*value);
                toggled = 1;
            }
            ctx->pressed_node = 0;
        }
    }

    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4;
    int fb_w = ctx->host->width;
    int fb_h = ctx->host->height;

    int cb_x = x;
    int cb_y = y + (h - cs) / 2;

    if (*value) {
        // Checked: accent fill
        ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h,
                                     cb_x, cb_y, cs, cs, UI_COLOR_ACCENT, DS(ctx, 3));
        draw_checkmark(fb, stride, fb_w, fb_h, cb_x + DS(ctx, 3), cb_y + DS(ctx, 2), cs - DS(ctx, 5), 0xFFFFFFFF);
    } else {
        // Unchecked: dark fill + border
        ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h,
                                     cb_x, cb_y, cs, cs, UI_COLOR_SURFACE2, DS(ctx, 3));
        draw_border_rect(fb, stride, fb_w, fb_h, cb_x, cb_y, cs, cs, UI_COLOR_BORDER);
    }

    if (label && label[0]) {
        ui_widget_draw_text(ctx, cb_x + cs + DS(ctx, 6), y + (h - DS(ctx, 14)) / 2,
                            label, ctx->color_text, 14);
    }

    layout_advance(ctx, w, h);
    return toggled;
}

// ============================================================================
//  WIDGET: ui_slider
// ============================================================================

int ui_slider(KainUiWidgetContext* ctx, double* value, double lo, double hi)
{
    if (!ctx || !ctx->host || !value) return 0;

    int w = DS(ctx, UI_SLIDER_WIDTH);
    int h = DS(ctx, UI_SLIDER_HEIGHT);

    int x, y;
    layout_next_slot(ctx, &x, &y, &w, &h, w, h);

    char key[UI_WIDGET_KEY_SIZE];
    widget_key(ctx, "sld", key, sizeof(key));
    int64_t nid = widget_node(ctx, key, "widget.slider", x, y, w, h);
    ctx->widget_counter++;

    int hovered = is_hovered(ctx, (double)x, (double)y, (double)w, (double)h);
    int changed = 0;

    // Normalize value
    double range = (hi > lo) ? (hi - lo) : 1.0;
    double norm = (*value - lo) / range;
    if (norm < 0.0) norm = 0.0;
    if (norm > 1.0) norm = 1.0;

    // Track geometry — all sizes scaled by DPI
    int track_x = x;
    int track_y = y + h / 2 - DS(ctx, 3);
    int track_w = w;
    int track_h = DS(ctx, 6);
    int thumb_w = DS(ctx, 10);
    int thumb_h = DS(ctx, 18);
    int thumb_y = y + h / 2 - thumb_h / 2;
    int thumb_x = track_x + (int)(norm * (double)(track_w - thumb_w));

    // Interaction
    int on_thumb = is_hovered(ctx, (double)thumb_x, (double)thumb_y,
                               (double)thumb_w, (double)thumb_h);
    int on_track = is_hovered(ctx, (double)track_x, (double)track_y,
                               (double)track_w, (double)track_h);

    // Press on thumb or track starts drag
    if (ctx->mouse_down && !ctx->mouse_down_prev && (on_thumb || on_track)) {
        ctx->pressed_node = nid;
    }

    // Drag while pressed
    if (ctx->mouse_down && ctx->pressed_node == nid) {
        double new_norm = (ctx->mouse_x - (double)track_x) / (double)(track_w - thumb_w);
        if (new_norm < 0.0) new_norm = 0.0;
        if (new_norm > 1.0) new_norm = 1.0;
        double new_val = lo + new_norm * range;
        if (fabs(new_val - *value) > 0.001) {
            *value = new_val;
            changed = 1;
        }
    }

    // Release: clear pressed state
    if (!ctx->mouse_down && ctx->mouse_down_prev) {
        if (ctx->pressed_node == nid) {
            ctx->pressed_node = 0;
        }
    }

    // Draw
    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4;
    int fb_w = ctx->host->width;
    int fb_h = ctx->host->height;

    // Track background
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h,
                                 track_x, track_y, track_w, track_h,
                                 SLIDER_TRACK_COLOR, DS(ctx, 3));
    // Filled portion
    int fill_w = thumb_x - track_x;
    if (fill_w > 0) {
        ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h,
                                     track_x, track_y, fill_w, track_h,
                                     SLIDER_FILL_COLOR, DS(ctx, 3));
    }

    // Thumb
    uint32_t thumb_color = (ctx->pressed_node == nid) ? UI_COLOR_ACCENT :
                           (hovered || on_thumb) ? UI_COLOR_ACCENT2 : UI_COLOR_SURFACE2;
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h,
                                 thumb_x, thumb_y, thumb_w, thumb_h,
                                 thumb_color, DS(ctx, 4));
    draw_border_rect(fb, stride, fb_w, fb_h,
                     thumb_x, thumb_y, thumb_w, thumb_h, UI_COLOR_BORDER);

    abi_ui_node_set_style_string(ctx->session_id, nid, "fill_color", "#252540");
    abi_ui_node_set_style_f64(ctx->session_id, nid, "slider_value", *value);

    layout_advance(ctx, w, h);
    return changed;
}

// ============================================================================
//  WIDGET: ui_textbox
// ============================================================================

// Per-textbox keyboard state tracking (indices used up to 512)
#define TEXTBOX_MAX_KEYS 512
static int g_textbox_prev_keys[TEXTBOX_MAX_KEYS] = {0};

int ui_textbox(KainUiWidgetContext* ctx, char* buf, int buf_size)
{
    if (!ctx || !ctx->host || !buf || buf_size <= 0) return 0;

    int w = DS(ctx, UI_TEXTBOX_WIDTH);
    int h = DS(ctx, UI_TEXTBOX_HEIGHT);

    int x, y;
    layout_next_slot(ctx, &x, &y, &w, &h, w, h);

    char key[UI_WIDGET_KEY_SIZE];
    widget_key(ctx, "tbx", key, sizeof(key));
    int64_t nid = widget_node(ctx, key, "widget.textbox", x, y, w, h);
    ctx->widget_counter++;

    int hovered = is_hovered(ctx, (double)x, (double)y, (double)w, (double)h);
    int changed = 0;

    // Focus on click
    if (ctx->mouse_down && !ctx->mouse_down_prev) {
        if (hovered) {
            ctx->focused_node = nid;
            ctx->pressed_node = nid;
            abi_ui_focus(ctx->session_id, nid);
        } else {
            // Click elsewhere loses focus from this textbox
            if (ctx->focused_node == nid) {
                ctx->focused_node = 0;
            }
        }
    }

    // Release handling
    if (!ctx->mouse_down && ctx->mouse_down_prev) {
        if (ctx->pressed_node == nid) {
            ctx->pressed_node = 0;
        }
    }

    // Keyboard input when focused
    if (ctx->focused_node == nid) {
        // Use Win32 key state tracking for text input
        // This is a simplified approach; proper IME would use WM_CHAR
        int len = (int)strlen(buf);
        if (len < 0) len = 0;

        // Check printable keys (32-126)
        for (int k = 0x20; k <= 0x5A; k++) {
            int state = GetAsyncKeyState(k) & 0x8001;
            int was_down = g_textbox_prev_keys[k] & 0x8001;
            if (state && !was_down) {
                if (k == VK_BACK) {
                    if (len > 0) {
                        buf[len - 1] = '\0';
                        changed = 1;
                    }
                } else if (k == VK_RETURN || k == VK_TAB) {
                    // Just eat the key
                }
            }
            g_textbox_prev_keys[k] = state;
        }

        // Character input: WM_CHAR-like from vkey mapping
        for (int vk = '0'; vk <= '9'; vk++) {
            int state = GetAsyncKeyState(vk) & 0x8001;
            int was_down = g_textbox_prev_keys[vk + 128] & 0x8001;
            if (state && !was_down && len < buf_size - 1) {
                buf[len] = (char)vk;
                buf[len + 1] = '\0';
                changed = 1;
            }
            g_textbox_prev_keys[vk + 128] = state;
        }
        // Letters
        for (int vk = 'A'; vk <= 'Z'; vk++) {
            int state = GetAsyncKeyState(vk) & 0x8001;
            int was_down = g_textbox_prev_keys[vk + 256] & 0x8001;
            if (state && !was_down && len < buf_size - 1) {
                int shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 1 : 0;
                buf[len] = shift ? (char)vk : (char)(vk + 32);
                buf[len + 1] = '\0';
                changed = 1;
            }
            g_textbox_prev_keys[vk + 256] = state;
        }
        // Space
        int ss = GetAsyncKeyState(VK_SPACE) & 0x8001;
        int sw = g_textbox_prev_keys[384] & 0x8001;
        if (ss && !sw && len < buf_size - 1) {
            buf[len] = ' ';
            buf[len + 1] = '\0';
            changed = 1;
        }
        g_textbox_prev_keys[384] = ss;
    }

    // Draw
    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4;
    int fb_w = ctx->host->width;
    int fb_h = ctx->host->height;

    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, y, w, h,
                                 TEXTBOX_BG_COLOR, DS(ctx, 3));

    uint32_t border = (ctx->focused_node == nid) ? UI_COLOR_ACCENT : UI_COLOR_BORDER;
    draw_border_rect(fb, stride, fb_w, fb_h, x, y, w, h, border);

    if (buf[0]) {
        ui_widget_draw_text(ctx, x + DS(ctx, 4), y + (h - DS(ctx, 14)) / 2,
                            buf, ctx->color_text, 14);
    }

    // Flashing cursor
    if (ctx->focused_node == nid) {
        int cur_x = x + DS(ctx, 4) + ui_widget_text_width(ctx, buf);
        if (cur_x < x + w - DS(ctx, 4)) {
            ui_widget_fill_rect(fb, stride, fb_w, fb_h,
                                cur_x, y + DS(ctx, 4), 1, h - DS(ctx, 8), UI_COLOR_ACCENT);
        }
    }

    layout_advance(ctx, w, h);
    return changed;
}

// ============================================================================
//  WIDGET: ui_panel
// ============================================================================

int64_t ui_panel(KainUiWidgetContext* ctx, const char* title,
                  double x, double y, double w, double h)
{
    if (!ctx || !ctx->host) return 0;

    // Scale from logical to physical pixel space
    double sx = DSD(ctx, x);
    double sy = DSD(ctx, y);
    double sw = DSD(ctx, w);
    double sh = DSD(ctx, h);
    int padding = DS(ctx, UI_PADDING);
    int title_bar_h = DS(ctx, 30);

    char key[UI_WIDGET_KEY_SIZE];
    widget_key(ctx, "pnl", key, sizeof(key));
    int64_t nid = widget_node(ctx, key, "widget.panel",
                               (int)sx, (int)sy, (int)sw, (int)sh);
    ctx->widget_counter++;

    // Push container
    if (ctx->container_depth < UI_MAX_CONTAINERS) {
        UiContainer* c = &ctx->container_stack[ctx->container_depth];
        c->node_id = nid;
        c->x = sx;
        c->y = sy;
        c->w = sw;
        c->h = sh;
        c->cursor_x = sx + padding;
        c->cursor_y = sy + title_bar_h + padding;
        ctx->container_depth++;
    }

    // Draw panel
    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4;
    int fb_w = ctx->host->width;
    int fb_h = ctx->host->height;
    int ix = (int)sx, iy = (int)sy, iw = (int)sw, ih = (int)sh;

    // Background
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, ix, iy, iw, ih,
                                 ctx->color_surface, DS(ctx, 6));
    draw_border_rect(fb, stride, fb_w, fb_h, ix, iy, iw, ih, ctx->color_border);

    // Title bar
    ui_widget_fill_rect(fb, stride, fb_w, fb_h, ix + DS(ctx, 1), iy + DS(ctx, 1), iw - DS(ctx, 2), DS(ctx, 28),
                         ctx->color_title_bg);
    ui_widget_fill_rect(fb, stride, fb_w, fb_h, ix, iy + DS(ctx, 29), iw, DS(ctx, 1), ctx->color_accent);

    if (title && title[0]) {
        ui_widget_draw_text(ctx, ix + DS(ctx, 10), iy + DS(ctx, 6), title, ctx->color_text, 13);
    }

    abi_ui_node_set_style_string(ctx->session_id, nid, "fill_color", "#252540");

    // Sync top-level layout position to panel content
    ctx->layout_x = sx + padding;
    ctx->layout_y = sy + title_bar_h + padding;

    return nid;
}

void ui_panel_end(KainUiWidgetContext* ctx)
{
    if (!ctx || ctx->container_depth <= 0) return;
    ctx->container_depth--;

    // Restore layout position to parent container
    if (ctx->container_depth > 0) {
        UiContainer* c = &ctx->container_stack[ctx->container_depth - 1];
        ctx->layout_x = c->cursor_x;
        ctx->layout_y = c->cursor_y;
    } else {
        ctx->layout_x = 0;
        ctx->layout_y = ctx->layout_next_y;
    }
}

// ============================================================================
//  WIDGET: ui_progress
// ============================================================================

int64_t ui_progress(KainUiWidgetContext* ctx, const char* label,
                     double value, double max)
{
    if (!ctx || !ctx->host) return 0;

    // Estimate label width for total sizing
    int label_w = (label && label[0]) ? (int)(strlen(label) * 7 * ctx->dpi_scale + DS(ctx, 8)) : 0;
    int w = DS(ctx, UI_PROGRESS_WIDTH);
    int h = DS(ctx, UI_PROGRESS_HEIGHT);

    int x, y;
    layout_next_slot(ctx, &x, &y, &w, &h, w, h);

    char key[UI_WIDGET_KEY_SIZE];
    widget_key(ctx, "prg", key, sizeof(key));
    int64_t nid = widget_node(ctx, key, "widget.progress", x, y, w, h);
    ctx->widget_counter++;

    double ratio = (max > 0.0) ? value / max : 0.0;
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;

    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4;
    int fb_w = ctx->host->width;
    int fb_h = ctx->host->height;

    // Background
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, y, w, h,
                                 PROGRESS_BG_COLOR, DS(ctx, 4));

    // Filled portion
    int fill_w = (int)(ratio * (double)w);
    if (fill_w > DS(ctx, 2)) {
        ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, x, y, fill_w, h,
                                     PROGRESS_FILL_COLOR, DS(ctx, 4));
    }

    // Percentage text
    char pct[16];
    snprintf(pct, sizeof(pct), "%d%%", (int)(ratio * 100.0 + 0.5));
    ui_widget_draw_text_centered(ctx, x, y, w, h, pct, UI_COLOR_TEXT, 12);

    // Label
    if (label && label[0]) {
        ui_widget_draw_text(ctx, x + w + DS(ctx, 6), y + (h - DS(ctx, 14)) / 2,
                            label, ctx->color_text_dim, 12);
    }

    abi_ui_node_set_style_f64(ctx->session_id, nid, "progress_value", value);

    layout_advance(ctx, w + label_w, h);
    return nid;
}

// ============================================================================
//  WIDGET: ui_window
// ============================================================================

int ui_window(KainUiWidgetContext* ctx, const char* title,
               double* x, double* y, double w, double h, int* open)
{
    if (!ctx || !ctx->host || !x || !y || !open) return 0;
    if (!*open) return 0;

    // Scale coordinates
    double sx = DSD(ctx, *x);
    double sy = DSD(ctx, *y);
    double sw = DSD(ctx, w);
    double sh = DSD(ctx, h);
    int ix = (int)sx, iy = (int)sy, iw = (int)sw, ih = (int)sh;
    int padding = DS(ctx, UI_PADDING);
    int title_bar_h = DS(ctx, 30);

    char key[UI_WIDGET_KEY_SIZE];
    widget_key(ctx, "win", key, sizeof(key));
    int64_t nid = widget_node(ctx, key, "widget.window", ix, iy, iw, ih);
    ctx->widget_counter++;

    // Push container for window content
    if (ctx->container_depth < UI_MAX_CONTAINERS) {
        UiContainer* c = &ctx->container_stack[ctx->container_depth];
        c->node_id = nid;
        c->x = sx;
        c->y = sy;
        c->w = sw;
        c->h = sh;
        c->cursor_x = sx + padding;
        c->cursor_y = sy + title_bar_h + padding;
        ctx->container_depth++;
    }

    uint32_t* fb = (uint32_t*)ctx->host->framebuffer;
    int stride = ctx->host->fb_stride / 4;
    int fb_w = ctx->host->width;
    int fb_h = ctx->host->height;

    // Shadow
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, ix + DS(ctx, 3), iy + DS(ctx, 3), iw, ih,
                                 0x40000000, DS(ctx, 6));

    // Background
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h, ix, iy, iw, ih,
                                 ctx->color_surface, DS(ctx, 6));
    draw_border_rect(fb, stride, fb_w, fb_h, ix, iy, iw, ih, ctx->color_border);

    // Title bar
    ui_widget_fill_rect(fb, stride, fb_w, fb_h, ix + DS(ctx, 1), iy + DS(ctx, 1), iw - DS(ctx, 2), DS(ctx, 28),
                         ctx->color_title_bg);
    ui_widget_fill_rect(fb, stride, fb_w, fb_h, ix, iy + DS(ctx, 29), iw, DS(ctx, 1), ctx->color_accent);

    if (title && title[0]) {
        ui_widget_draw_text(ctx, ix + DS(ctx, 10), iy + DS(ctx, 6), title, ctx->color_text, 13);
    }

    // Close button (X) in the top-right
    int close_s = DS(ctx, 20);
    int close_x = ix + iw - DS(ctx, 24);
    int close_y = iy + DS(ctx, 4);
    int close_hover = is_hovered(ctx, (double)close_x, (double)close_y,
                                 (double)close_s, (double)close_s);

    if (close_hover) {
        ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h,
                                     close_x, close_y, close_s, close_s,
                                     UI_COLOR_ACCENT4, DS(ctx, 4));
    }
    draw_x_mark(fb, stride, fb_w, fb_h, close_x + DS(ctx, 6), close_y + DS(ctx, 6), DS(ctx, 8), 0xFFFFFFFF);

    // Close button click: use close_nid = nid + 1000000 as pseudo-id
    int64_t close_nid = nid + 1000000;
    if (ctx->mouse_down && !ctx->mouse_down_prev && close_hover) {
        ctx->pressed_node = close_nid;
    }
    if (!ctx->mouse_down && ctx->mouse_down_prev) {
        if (ctx->pressed_node == close_nid) {
            *open = 0;
            ctx->pressed_node = 0;
        }
    }

    // Window dragging — positions stay in scaled physical space
    double title_bar_w = sw - (double)DS(ctx, 24);
    int title_bar_hover = is_hovered(ctx, sx, sy, title_bar_w, (double)title_bar_h);
    static int drag_active = 0;
    static double drag_ox = 0, drag_oy = 0;

    // Drag starts on title bar (only if NOT on close button)
    if (ctx->mouse_down && !ctx->mouse_down_prev && title_bar_hover && !close_hover) {
        drag_active = 1;
        drag_ox = ctx->mouse_x - sx;
        drag_oy = ctx->mouse_y - sy;
        ctx->pressed_node = nid;
    }

    if (ctx->mouse_down && drag_active && ctx->pressed_node == nid) {
        sx = ctx->mouse_x - drag_ox;
        sy = ctx->mouse_y - drag_oy;
        // Clamp to screen bounds
        if (sx < 0) sx = 0;
        if (sy < 0) sy = 0;
        if (sx > (double)(ctx->host->width - DS(ctx, 100))) sx = (double)(ctx->host->width - DS(ctx, 100));
        if (sy > (double)(ctx->host->height - DS(ctx, 40))) sy = (double)(ctx->host->height - DS(ctx, 40));
        // Write back logical coordinates to caller's pointers
        *x = sx / ctx->dpi_scale;
        *y = sy / ctx->dpi_scale;
    }

    // On release, if we were dragging, stop
    if (!ctx->mouse_down && ctx->mouse_down_prev) {
        if (ctx->pressed_node == nid) {
            drag_active = 0;
            ctx->pressed_node = 0;
        }
    }

    // Update node position for dragging (physical coords)
    abi_ui_node_set_rect(ctx->session_id, nid, sx, sy, sw, sh);

    return *open;
}

// ============================================================================
//  ABI WRAPPER FUNCTIONS (expose widget library to Kain's @extern FFI)
// ============================================================================
// Each wrapper receives raw int64/f64 params from Kain, casts to native C
// types, calls the existing widget function, and returns results as int64.

int64_t abi_ui_widget_create(int64_t session_id) {
    KainUiWidgetContext* ctx = ui_widget_create((int)session_id);
    return (int64_t)(uintptr_t)ctx;
}

void abi_ui_widget_destroy(int64_t ctx_ptr) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    ui_widget_destroy(ctx);
}

void abi_ui_widget_begin_frame(int64_t ctx_ptr) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    ui_widget_begin_frame(ctx);
}

void abi_ui_widget_end_frame(int64_t ctx_ptr) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    ui_widget_end_frame(ctx);
}

int64_t abi_ui_widget_load_font(int64_t ctx_ptr, const char* filepath, double size) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    return ui_widget_load_font(ctx, filepath, size);
}

int64_t abi_ui_widget_load_default_font(int64_t ctx_ptr, double size) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    return ui_widget_load_default_font(ctx, size);
}

int64_t abi_ui_widget_button(int64_t ctx_ptr, const char* label) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    return (int64_t)ui_button(ctx, label);
}

int64_t abi_ui_widget_label(int64_t ctx_ptr, const char* text) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    return ui_label(ctx, text);
}

int64_t abi_ui_widget_checkbox(int64_t ctx_ptr, const char* label, int64_t current_value) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    int val = (int)current_value;
    int toggled = ui_checkbox(ctx, label, &val);
    // Packed: bit0=new_value, bit1=toggled
    return (int64_t)((toggled << 1) | val);
}

int64_t abi_ui_widget_slider(int64_t ctx_ptr, double current_value, double lo, double hi) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    double val = current_value;
    int changed = ui_slider(ctx, &val, lo, hi);
    // Pack changed flag into high bit, scaled value into lower 63 bits
    int64_t int_val = (int64_t)(val * 1000.0);
    return (changed ? (int64_t)1 << 63 : 0) | (int_val & 0x7FFFFFFFFFFFFFFF);
}

int64_t abi_ui_widget_textbox_poll(int64_t ctx_ptr, int64_t buf_ptr, int64_t buf_size) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    char* buf = (char*)(uintptr_t)buf_ptr;
    return (int64_t)ui_textbox(ctx, buf, (int)buf_size);
}

int64_t abi_ui_widget_panel_begin(int64_t ctx_ptr, const char* title, double x, double y, double w, double h) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    return ui_panel(ctx, title, x, y, w, h);
}

void abi_ui_widget_panel_end(int64_t ctx_ptr) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    ui_panel_end(ctx);
}

int64_t abi_ui_widget_progress(int64_t ctx_ptr, const char* label, double value, double max_val) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    return ui_progress(ctx, label, value, max_val);
}

int64_t abi_ui_widget_window(int64_t ctx_ptr, const char* title, double x, double y, double w, double h, int64_t open) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    int is_open = (int)open;
    double win_x = x, win_y = y;
    int result = ui_window(ctx, title, &win_x, &win_y, (int)w, (int)h, &is_open);
    // Pack results: returns 1 if still open, 0 if closed
    return result ? (int64_t)(is_open & 1) : 0;
}

int64_t abi_ui_widget_layout_row(int64_t ctx_ptr, int64_t count, const int64_t* widths) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    int cnt = (int)count;
    if (cnt < 0) cnt = 0;
    if (cnt > UI_MAX_LAYOUT_ITEMS) cnt = UI_MAX_LAYOUT_ITEMS;
    int w[UI_MAX_LAYOUT_ITEMS];
    for (int i = 0; i < cnt; i++) {
        w[i] = (int)widths[i];
    }
    ui_layout_row(ctx, cnt, w);
    return 0;
}

int64_t abi_ui_widget_layout_column(int64_t ctx_ptr, int64_t count, const int64_t* heights) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    int cnt = (int)count;
    if (cnt < 0) cnt = 0;
    if (cnt > UI_MAX_LAYOUT_ITEMS) cnt = UI_MAX_LAYOUT_ITEMS;
    int h[UI_MAX_LAYOUT_ITEMS];
    for (int i = 0; i < cnt; i++) {
        h[i] = (int)heights[i];
    }
    ui_layout_column(ctx, cnt, h);
    return 0;
}

int64_t abi_ui_widget_layout_set_next(int64_t ctx_ptr, int64_t w, int64_t h) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    ui_layout_set_next(ctx, (int)w, (int)h);
    return 0;
}

const char* abi_ui_widget_textbox(int64_t ctx_ptr, const char* text, int64_t max_len) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    KainNativeUiSession* session = (KainNativeUiSession*)ctx->session;
    if (!session) {
        return (const char*)((uintptr_t)"" | (uintptr_t)1u);
    }
    // Create stack buffer and call ui_textbox
    char buf[256];
    int buf_sz = (int)(max_len > 0 ? max_len : 255);
    if (buf_sz > 255) buf_sz = 255;
    strncpy(buf, text ? text : "", (size_t)buf_sz);
    buf[buf_sz] = '\0';
    ui_textbox(ctx, buf, buf_sz + 1);
    // Copy result into session frame arena (tagged pointer avoids RC)
    size_t result_len = strlen(buf) + 1u;
    size_t offset = session->frame_arena_offset;
    if (offset + result_len > ABI_UI_FRAME_ARENA_SIZE) {
        // Arena full — return empty, textbox changes lost
        return (const char*)((uintptr_t)"" | (uintptr_t)1u);
    }
    memcpy(session->frame_arena + offset, buf, result_len);
    session->frame_arena_offset = offset + result_len;
    return (const char*)((uintptr_t)(session->frame_arena + offset) | (uintptr_t)1u);
}

int64_t abi_ui_widget_layout_begin(int64_t ctx_ptr, int64_t count, int64_t layout_type) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    int cnt = (int)count;
    if (cnt < 0) cnt = 0;
    if (cnt > UI_MAX_LAYOUT_ITEMS) cnt = UI_MAX_LAYOUT_ITEMS;
    ctx->layout_type = (int)layout_type;  // 0 = row, 1 = column
    ctx->layout_count = cnt;
    ctx->layout_item = 0;
    ctx->layout_size_count = 0;
    return 0;
}

int64_t abi_ui_widget_layout_set_size(int64_t ctx_ptr, int64_t index, int64_t size) {
    KainUiWidgetContext* ctx = (KainUiWidgetContext*)(uintptr_t)ctx_ptr;
    if (index >= 0 && index < UI_MAX_LAYOUT_ITEMS) {
        ctx->layout_sizes[(int)index] = (int)size;
        if ((int)index + 1 > ctx->layout_size_count) {
            ctx->layout_size_count = (int)index + 1;
        }
    }
    return 0;
}
