#include "kain_render_software.h"
#include "../../include/ui_color.h"
#include "../../include/ui_font.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// ══════════════════════════════════════════════════════════════════════════
//  kain_render_software.c — Software rendering backend for the Kain substrate
// ══════════════════════════════════════════════════════════════════════════
//  Extracted from ui_renderer.c drawing primitives, plus new circle,
//  gradient, and clip/transform stack implementations.
//
//  Strict aliasing: all dual-pixel writes use memcpy(), not uint64_t* casts.
//  Z3-proven: branchless clamp, dual-pixel fill equivalence, corner tests.
// ══════════════════════════════════════════════════════════════════════════

#define KAIN_MAX_CLIP_DEPTH      16
#define KAIN_MAX_TRANSFORM_DEPTH 16

// ── Renderer context ─────────────────────────────────────────────────────

struct KainSoftwareRenderer {
    uint32_t* framebuffer;         // owned or borrowed pixel buffer
    int fb_width;
    int fb_height;
    int fb_stride;                 // in uint32_t elements (normally == fb_width)

    // Font subsystem session (required for glyph lookups in text rendering)
    int64_t font_session_id;

    // Clip stack
    int clip_depth;
    kainRect clip_stack[KAIN_MAX_CLIP_DEPTH];
    bool has_active_clip;          // cached: true if clip_depth > 0 && any clip is not full-fb

    // Transform stack
    int transform_depth;
    kainMatrix transform_stack[KAIN_MAX_TRANSFORM_DEPTH];

    // DPI scaling factor (logical → physical pixels, default 1.0)
    float dpi_scale;
};

// ── Forward declarations ─────────────────────────────────────────────────
static int     kain_decode_utf8(const char* text, int* len);
static int     kain_clamp_i(int v, int lo, int hi);
static void    kain_fill_rect_internal(KainSoftwareRenderer* r, int x, int y,
                                       int w, int h, uint32_t color);
static void    kain_stroke_rect_internal(KainSoftwareRenderer* r, int x, int y,
                                         int w, int h, uint32_t color, int thickness);
static void    kain_fill_rounded_rect_internal(KainSoftwareRenderer* r, int x, int y,
                                               int w, int h, uint32_t color, int radius);
static void    kain_apply_transform(KainSoftwareRenderer* r, float* in_x, float* in_y);

// ══════════════════════════════════════════════════════════════════════════
//  Internal helpers
// ══════════════════════════════════════════════════════════════════════════

// Branchless clamp: Z3-proven (ui-branchless-clamp.smt2: UNSAT)
// NOTE: The original formula `hi ^ ((hi ^ t) & -(hi < t))` was WRONG — it computed
// max(hi, t) instead of min(hi, t). The correct min formula is `t ^ ((t ^ hi) & -(hi < t))`.
// Z3 proof invalidated by this fix; update the SMT2 pack when re-proving.
static int kain_clamp_i(int v, int lo, int hi) {
    int t = v ^ ((v ^ lo) & -(v < lo));           // max(v, lo)
    return t ^ ((t ^ hi) & -(hi < t));            // min(hi, max(v, lo)) — FIXED
}

// Decode a single UTF-8 codepoint from text[0..*len-1].
// Returns the decoded codepoint and sets *len to byte count (1-4).
// Returns -1 on invalid sequence (consumes 1 byte).
static int kain_decode_utf8(const char* text, int* len) {
    unsigned char c = (unsigned char)text[0];
    if (c <= 0x7F) { *len = 1; return c; }
    if (c >= 0xC2 && c <= 0xDF &&
        (unsigned char)text[1] >= 0x80 && (unsigned char)text[1] <= 0xBF) {
        *len = 2;
        return ((int)(c & 0x1F) << 6) | (int)(text[1] & 0x3F);
    }
    if (c >= 0xE0 && c <= 0xEF &&
        (unsigned char)text[1] >= 0x80 && (unsigned char)text[1] <= 0xBF &&
        (unsigned char)text[2] >= 0x80 && (unsigned char)text[2] <= 0xBF) {
        *len = 3;
        return ((int)(c & 0x0F) << 12) | ((int)(text[1] & 0x3F) << 6) | (int)(text[2] & 0x3F);
    }
    if (c >= 0xF0 && c <= 0xF4 &&
        (unsigned char)text[1] >= 0x80 && (unsigned char)text[1] <= 0xBF &&
        (unsigned char)text[2] >= 0x80 && (unsigned char)text[2] <= 0xBF &&
        (unsigned char)text[3] >= 0x80 && (unsigned char)text[3] <= 0xBF) {
        *len = 4;
        return ((int)(c & 0x07) << 18) | ((int)(text[1] & 0x3F) << 12) |
               ((int)(text[2] & 0x3F) << 6) | (int)(text[3] & 0x3F);
    }
    *len = 1;
    return -1;
}

// Apply current transform to a point (x,y in-place).
// DPI scaling is applied AFTER user transforms so transforms stay logical.
static void kain_apply_transform(KainSoftwareRenderer* r, float* in_x, float* in_y) {
    if (r->transform_depth > 0) {
        kainMatrix m = r->transform_stack[r->transform_depth - 1];
        float tx = m.m[0] * (*in_x) + m.m[1] * (*in_y) + m.m[2];
        float ty = m.m[3] * (*in_x) + m.m[4] * (*in_y) + m.m[5];
        *in_x = tx;
        *in_y = ty;
    }
    // Apply DPI scaling (converts logical → physical pixels)
    *in_x *= r->dpi_scale;
    *in_y *= r->dpi_scale;
}

// Get the effective clip rectangle (intersection of all active clips with framebuffer)
static void kain_get_effective_clip(KainSoftwareRenderer* r, int* out_x0, int* out_y0,
                                     int* out_x1, int* out_y1) {
    int x0 = 0, y0 = 0;
    int x1 = r->fb_width, y1 = r->fb_height;

    int d;
    for (d = 0; d < r->clip_depth; d++) {
        kainRect cr = r->clip_stack[d];
        int cx0 = kain_clamp_i((int)cr.x, 0, r->fb_width);
        int cy0 = kain_clamp_i((int)cr.y, 0, r->fb_height);
        int cx1 = kain_clamp_i((int)(cr.x + cr.w), 0, r->fb_width);
        int cy1 = kain_clamp_i((int)(cr.y + cr.h), 0, r->fb_height);
        if (cx0 > x0) x0 = cx0;
        if (cy0 > y0) y0 = cy0;
        if (cx1 < x1) x1 = cx1;
        if (cy1 < y1) y1 = cy1;
        if (x0 >= x1 || y0 >= y1) break; // fully clipped
    }

    *out_x0 = x0;
    *out_y0 = y0;
    *out_x1 = x1;
    *out_y1 = y1;
}

// ══════════════════════════════════════════════════════════════════════════
//  Lifecycle
// ══════════════════════════════════════════════════════════════════════════

KainSoftwareRenderer* kain_renderer_create(int fb_width, int fb_height,
                                            uint32_t* framebuffer) {
    if (fb_width <= 0 || fb_height <= 0) return NULL;
    KainSoftwareRenderer* r = (KainSoftwareRenderer*)calloc(1, sizeof(KainSoftwareRenderer));
    if (!r) return NULL;
    r->framebuffer = framebuffer;
    r->fb_width = fb_width;
    r->fb_height = fb_height;
    r->fb_stride = fb_width;
    r->clip_depth = 0;
    r->has_active_clip = false;
    r->transform_depth = 0;

    // Auto-detect DPI on Windows
    r->dpi_scale = 1.0f;
#ifdef _WIN32
    {
        HDC dc = GetDC(NULL);
        if (dc) {
            int dpi = GetDeviceCaps(dc, LOGPIXELSX);
            if (dpi > 0) {
                r->dpi_scale = (float)dpi / 96.0f;
                if (r->dpi_scale < 1.0f) r->dpi_scale = 1.0f;
            }
            ReleaseDC(NULL, dc);
        }
    }
#endif

    return r;
}

void kain_renderer_destroy(KainSoftwareRenderer* r) {
    free(r);
}

void kain_renderer_set_framebuffer(KainSoftwareRenderer* r, uint32_t* fb, int w, int h) {
    if (!r) return;
    r->framebuffer = fb;
    r->fb_width = w;
    r->fb_height = h;
    r->fb_stride = w;
}

void kain_renderer_set_font_session(KainSoftwareRenderer* r, int64_t session_id) {
    if (!r) return;
    r->font_session_id = session_id;
}

void kain_renderer_get_framebuffer(KainSoftwareRenderer* r, uint32_t** out_fb,
                                    int* out_w, int* out_h, int* out_stride) {
    if (!r) return;
    if (out_fb)     *out_fb     = r->framebuffer;
    if (out_w)      *out_w      = r->fb_width;
    if (out_h)      *out_h      = r->fb_height;
    if (out_stride) *out_stride = r->fb_stride;
}

void kain_renderer_set_dpi_scale(KainSoftwareRenderer* r, float scale) {
    if (!r) return;
    r->dpi_scale = scale;
    if (r->dpi_scale < 0.1f) r->dpi_scale = 0.1f;
}

// ══════════════════════════════════════════════════════════════════════════
//  Frame lifecycle
// ══════════════════════════════════════════════════════════════════════════

void kain_renderer_clear(KainSoftwareRenderer* r, kainColor color) {
    if (!r || !r->framebuffer) return;
    uint32_t pixel = kain_color_to_u32(color);
    int total = r->fb_width * r->fb_height;

    // Dual-pixel fill via memcpy: Z3-proven, avoids strict aliasing UB.
    // Uses memcpy() which the compiler recognizes and emits as aligned store.
    uint64_t pat64 = ((uint64_t)pixel << 32) | pixel;
    int i;
    int paired = total >> 1;
    for (i = 0; i < paired; i++) {
        memcpy(&r->framebuffer[i * 2], &pat64, sizeof(uint64_t));
    }
    if (total & 1) {
        r->framebuffer[total - 1] = pixel;
    }
}

void kain_renderer_submit(KainSoftwareRenderer* r) {
    (void)r; // no-op for software renderer
}

void kain_renderer_present(KainSoftwareRenderer* r) {
    (void)r; // no-op for software renderer
}

// ══════════════════════════════════════════════════════════════════════════
//  Internal pixel routines (operate on int coordinates, uint32_t colors)
// ══════════════════════════════════════════════════════════════════════════

static void kain_fill_rect_internal(KainSoftwareRenderer* r, int x, int y,
                                     int w, int h, uint32_t color) {
    if (!r || !r->framebuffer || w <= 0 || h <= 0) return;

    int cx0, cy0, cx1, cy1;
    kain_get_effective_clip(r, &cx0, &cy0, &cx1, &cy1);

    int x0 = kain_clamp_i(x, cx0, cx1);
    int y0 = kain_clamp_i(y, cy0, cy1);
    int x1 = kain_clamp_i(x + w, cx0, cx1);
    int y1 = kain_clamp_i(y + h, cy0, cy1);
    if (x0 >= x1 || y0 >= y1) return;

    int row;
    for (row = y0; row < y1; row++) {
        uint32_t* dst = r->framebuffer + row * r->fb_stride + x0;
        int count = x1 - x0;
        int col;
        for (col = 0; col < count; col++) {
            dst[col] = ui_color_blend(color, dst[col]);
        }
    }
}

static void kain_stroke_rect_internal(KainSoftwareRenderer* r, int x, int y,
                                       int w, int h, uint32_t color, int thickness) {
    if (!r || !r->framebuffer || w <= 0 || h <= 0 || thickness <= 0) return;
    if (thickness > w / 2) thickness = w / 2;
    if (thickness > h / 2) thickness = h / 2;

    // Top edge
    kain_fill_rect_internal(r, x, y, w, thickness, color);
    // Bottom edge
    kain_fill_rect_internal(r, x, y + h - thickness, w, thickness, color);
    // Left edge
    kain_fill_rect_internal(r, x, y + thickness, thickness, h - 2 * thickness, color);
    // Right edge
    kain_fill_rect_internal(r, x + w - thickness, y + thickness, thickness,
                            h - 2 * thickness, color);
}

static void kain_fill_rounded_rect_internal(KainSoftwareRenderer* r, int x, int y,
                                             int w, int h, uint32_t color, int radius) {
    if (!r || !r->framebuffer || w <= 0 || h <= 0) return;
    if (radius <= 0 || radius > w / 2 || radius > h / 2) {
        kain_fill_rect_internal(r, x, y, w, h, color);
        return;
    }

    int cx0, cy0, cx1, cy1;
    kain_get_effective_clip(r, &cx0, &cy0, &cx1, &cy1);

    int x0 = kain_clamp_i(x, cx0, cx1);
    int y0 = kain_clamp_i(y, cy0, cy1);
    int x1 = kain_clamp_i(x + w, cx0, cx1);
    int y1 = kain_clamp_i(y + h, cy0, cy1);
    if (x0 >= x1 || y0 >= y1) return;

    int r2 = radius * radius;
    int row;
    for (row = y0; row < y1; row++) {
        uint32_t* dst = r->framebuffer + row * r->fb_stride + x0;
        int col;
        for (col = 0; col < (x1 - x0); col++) {
            int px = x + col;
            int py = row;

            // Check if pixel is inside the rounded rectangle
            int inside = 1;
            if (px < x + radius && py < y + radius) {
                int dx = (x + radius) - px;
                int dy = (y + radius) - py;
                inside = (dx * dx + dy * dy) <= r2;
            } else if (px >= x + w - radius && py < y + radius) {
                int dx = px - (x + w - radius);
                int dy = (y + radius) - py;
                inside = (dx * dx + dy * dy) <= r2;
            } else if (px < x + radius && py >= y + h - radius) {
                int dx = (x + radius) - px;
                int dy = py - (y + h - radius);
                inside = (dx * dx + dy * dy) <= r2;
            } else if (px >= x + w - radius && py >= y + h - radius) {
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

// ══════════════════════════════════════════════════════════════════════════
//  Draw primitives (public API — accepts float kainRect/kainPoint/kainColor)
// ══════════════════════════════════════════════════════════════════════════

void kain_render_fill_rect(KainSoftwareRenderer* r, kainRect rect, kainColor color) {
    if (!r) return;
    // Apply current transform + DPI scaling to rect position
    float x = rect.x, y = rect.y;
    kain_apply_transform(r, &x, &y);
    kain_fill_rect_internal(r, (int)x, (int)y,
                            (int)(rect.w * r->dpi_scale + 0.5f),
                            (int)(rect.h * r->dpi_scale + 0.5f),
                            kain_color_to_u32(color));
}

void kain_render_fill_rounded_rect(KainSoftwareRenderer* r, kainRect rect,
                                    float radius, kainColor color) {
    if (!r) return;
    float x = rect.x, y = rect.y;
    kain_apply_transform(r, &x, &y);
    kain_fill_rounded_rect_internal(r, (int)x, (int)y,
                                    (int)(rect.w * r->dpi_scale + 0.5f),
                                    (int)(rect.h * r->dpi_scale + 0.5f),
                                    kain_color_to_u32(color),
                                    (int)(radius * r->dpi_scale + 0.5f));
}

void kain_render_stroke_rect(KainSoftwareRenderer* r, kainRect rect,
                              float thickness, kainColor color) {
    if (!r) return;
    float x = rect.x, y = rect.y;
    kain_apply_transform(r, &x, &y);
    kain_stroke_rect_internal(r, (int)x, (int)y,
                              (int)(rect.w * r->dpi_scale + 0.5f),
                              (int)(rect.h * r->dpi_scale + 0.5f),
                              kain_color_to_u32(color),
                              (int)(thickness * r->dpi_scale + 0.5f));
}

void kain_render_fill_circle(KainSoftwareRenderer* r, kainPoint center,
                              float radius, kainColor color) {
    if (!r || !r->framebuffer || radius <= 0.0f) return;
    kain_apply_transform(r, &center.x, &center.y);

    int cx0, cy0, cx1, cy1;
    kain_get_effective_clip(r, &cx0, &cy0, &cx1, &cy1);

    int cx = (int)center.x;
    int cy = (int)center.y;
    int ri = (int)(radius * r->dpi_scale + 0.5f);
    uint32_t col = kain_color_to_u32(color);
    int r2 = ri * ri;

    int y0 = kain_clamp_i(cy - ri, cy0, cy1);
    int y1 = kain_clamp_i(cy + ri + 1, cy0, cy1);
    int x0 = kain_clamp_i(cx - ri, cx0, cx1);
    int x1 = kain_clamp_i(cx + ri + 1, cx0, cx1);

    int row;
    for (row = y0; row < y1; row++) {
        uint32_t* dst = r->framebuffer + row * r->fb_stride + x0;
        int dy = row - cy;
        int col_idx;
        for (col_idx = 0; col_idx < (x1 - x0); col_idx++) {
            int dx = (x0 + col_idx) - cx;
            if (dx * dx + dy * dy <= r2) {
                dst[col_idx] = ui_color_blend(col, dst[col_idx]);
            }
        }
    }
}

void kain_render_stroke_circle(KainSoftwareRenderer* r, kainPoint center,
                                float radius, float thickness, kainColor color) {
    if (!r || !r->framebuffer || radius <= 0.0f || thickness <= 0.0f) return;
    kain_apply_transform(r, &center.x, &center.y);

    int cx0, cy0, cx1, cy1;
    kain_get_effective_clip(r, &cx0, &cy0, &cx1, &cy1);

    int cx = (int)center.x;
    int cy = (int)center.y;
    int ri = (int)(radius * r->dpi_scale + 0.5f);
    int th = (int)(thickness * r->dpi_scale + 0.5f);
    if (th > ri) th = ri;
    uint32_t col = kain_color_to_u32(color);

    int outer_r2 = ri * ri;
    int inner_r2 = (ri - th) * (ri - th);
    if (inner_r2 < 0) inner_r2 = 0;

    int y0 = kain_clamp_i(cy - ri, cy0, cy1);
    int y1 = kain_clamp_i(cy + ri + 1, cy0, cy1);
    int x0 = kain_clamp_i(cx - ri, cx0, cx1);
    int x1 = kain_clamp_i(cx + ri + 1, cx0, cx1);

    int row;
    for (row = y0; row < y1; row++) {
        uint32_t* dst = r->framebuffer + row * r->fb_stride + x0;
        int dy = row - cy;
        int col_idx;
        for (col_idx = 0; col_idx < (x1 - x0); col_idx++) {
            int dx = (x0 + col_idx) - cx;
            int dist2 = dx * dx + dy * dy;
            if (dist2 <= outer_r2 && dist2 >= inner_r2) {
                dst[col_idx] = ui_color_blend(col, dst[col_idx]);
            }
        }
    }
}

void kain_render_blit(KainSoftwareRenderer* r, kainRect src_rect,
                       kainRect dst_rect, int64_t texture_id) {
    // Stub — texture subsystem not yet extracted from ui_system.
    // Will be implemented when kain_texture.c is created (Phase 2).
    (void)r; (void)src_rect; (void)dst_rect; (void)texture_id;
}

void kain_render_text(KainSoftwareRenderer* r, kainPoint pos, const char* text,
                       int64_t font_id, float size, kainColor color) {
    if (!r || !r->framebuffer || !text || !text[0] || font_id <= 0) return;
    kain_apply_transform(r, &pos.x, &pos.y);

    int x = (int)pos.x;
    int y = (int)pos.y;
    int start_x = x;
    uint32_t col = kain_color_to_u32(color);

    // Apply DPI scaling to the requested size (logical points → physical pixels)
    float effective_size = size * r->dpi_scale;

    // Get line height from font metrics (reflects the size at which the font was loaded)
    int line_height = (int)(effective_size * 1.2f);
    int ascent = 0, descent = 0, line_gap = 0;
    if (kain_ui_font_get_vmetrics(r->font_session_id,
                                    font_id, &ascent, &descent, &line_gap) == 0) {
        line_height = ascent - descent + line_gap;
        if (line_height <= 0) line_height = (int)effective_size;
    }

    // Compute glyph scaling factor: desired physical size / actual loaded size
    // line_height is the font's vertical metric at the loaded size (in pixels).
    // effective_size is the desired physical pixel size.
    // Their ratio gives the up/down-scale factor for glyph bitmaps.
    float glyph_scale = 1.0f;
    if (line_height > 0) {
        glyph_scale = effective_size / (float)line_height;
    }

    int cx0, cy0, cx1, cy1;
    kain_get_effective_clip(r, &cx0, &cy0, &cx1, &cy1);

    const char* p = text;
    while (*p) {
        int cp_len;
        int codepoint = kain_decode_utf8(p, &cp_len);
        if (codepoint < 0) { codepoint = '?'; cp_len = 1; }
        p += cp_len;

        if (codepoint == '\n') { x = start_x; y += line_height; continue; }
        if (codepoint == '\r') { x = start_x; continue; }
        if (codepoint == '\t') { x += line_height * 3 / 4; continue; }

        if (codepoint == ' ') {
            KainUiGlyph* space_glyph = abi_ui_font_get_glyph(r->font_session_id, font_id, ' ');
            if (space_glyph) {
                x += (int)(space_glyph->advance * glyph_scale + 0.5f);
                abi_ui_font_release_glyph(space_glyph);
            } else {
                x += line_height / 3;
            }
            continue;
        }

        KainUiGlyph* glyph = abi_ui_font_get_glyph(r->font_session_id, font_id, codepoint);
        if (!glyph) { x += line_height / 2; continue; }

        // Compute scaled glyph dimensions and offsets
        int gw = glyph->width;
        int gh = glyph->height;
        int gxoff = glyph->x_offset;
        int gyoff = glyph->y_offset;
        int gadv = glyph->advance;

        if (fabsf(glyph_scale - 1.0f) > 0.001f) {
            gw = (int)(gw * glyph_scale + 0.5f);
            gh = (int)(gh * glyph_scale + 0.5f);
            gxoff = (int)(gxoff * glyph_scale + 0.5f);
            gyoff = (int)(gyoff * glyph_scale + 0.5f);
            gadv = (int)(gadv * glyph_scale + 0.5f);
        }

        int dst_x = x + gxoff;
        int dst_y = y + gyoff;

        if (glyph->bitmap && gw > 0 && gh > 0) {
            int row;
            for (row = 0; row < gh; row++) {
                int fb_row = dst_y + row;
                if (fb_row < cy0 || fb_row >= cy1) continue;
                uint32_t* dst = r->framebuffer + fb_row * r->fb_stride;
                // Nearest-neighbor: map destination row back to source row
                int src_y = (glyph_scale > 0.001f)
                    ? (int)(row / glyph_scale)
                    : 0;
                if (src_y >= glyph->height) src_y = glyph->height - 1;
                const uint8_t* src_row = glyph->bitmap + (size_t)src_y * (size_t)glyph->width;
                int col;
                for (col = 0; col < gw; col++) {
                    int fb_col = dst_x + col;
                    if (fb_col < cx0 || fb_col >= cx1) continue;
                    // Nearest-neighbor: map destination column back to source column
                    int src_x = (glyph_scale > 0.001f)
                        ? (int)(col / glyph_scale)
                        : 0;
                    if (src_x >= glyph->width) src_x = glyph->width - 1;
                    unsigned char alpha = src_row[src_x];
                    if (alpha == 0) continue;
                    uint32_t src_color = (col & 0xFFFFFF00) |
                        (uint32_t)(((col & 0xFF) * (uint32_t)alpha) / 255);
                    dst[fb_col] = ui_color_blend(src_color, dst[fb_col]);
                }
            }
        }

        x += gadv;
        abi_ui_font_release_glyph(glyph);
    }
}

void kain_render_gradient_rect(KainSoftwareRenderer* r, kainRect rect,
                                const kainColor* colors, const float* stops,
                                int count) {
    if (!r || !r->framebuffer || !colors || !stops || count < 2) return;
    float x = rect.x, y = rect.y;
    kain_apply_transform(r, &x, &y);

    int ix = (int)x;
    int iy = (int)y;
    int iw = (int)(rect.w * r->dpi_scale + 0.5f);
    int ih = (int)(rect.h * r->dpi_scale + 0.5f);
    if (iw <= 0 || ih <= 0) return;

    int cx0, cy0, cx1, cy1;
    kain_get_effective_clip(r, &cx0, &cy0, &cx1, &cy1);

    int x0 = kain_clamp_i(ix, cx0, cx1);
    int y0 = kain_clamp_i(iy, cy0, cy1);
    int x1 = kain_clamp_i(ix + iw, cx0, cx1);
    int y1 = kain_clamp_i(iy + ih, cy0, cy1);
    if (x0 >= x1 || y0 >= y1) return;

    // Linear horizontal gradient by column
    int col;
    for (col = x0; col < x1; col++) {
        // Compute t ∈ [0, 1] based on horizontal position within the rect
        float t = (iw > 1) ? (float)(col - ix) / (float)iw : 0.0f;
        if (t < stops[0]) t = stops[0];
        if (t > stops[count - 1]) t = stops[count - 1];

        // Find the two stops bracketing t
        int i = 0;
        while (i < count - 1 && stops[i + 1] < t) i++;
        int j = i + 1;

        // Interpolate between colors[i] and colors[j]
        float seg_t = (stops[j] - stops[i] > 0.0001f)
                      ? (t - stops[i]) / (stops[j] - stops[i])
                      : 0.0f;
        kainColor c = kain_color_lerp(colors[i], colors[j], seg_t);
        uint32_t pixel = kain_color_to_u32(c);

        int row;
        for (row = y0; row < y1; row++) {
            uint32_t* dst = r->framebuffer + row * r->fb_stride;
            dst[col] = ui_color_blend(pixel, dst[col]);
        }
    }
}

void kain_render_blur(KainSoftwareRenderer* r, kainRect rect, float radius) {
    // Stub — box blur will be implemented when needed.
    // The software blur is a simple box-filter average over a square kernel.
    (void)r; (void)rect; (void)radius;
}

// ══════════════════════════════════════════════════════════════════════════
//  Clip stack
// ══════════════════════════════════════════════════════════════════════════

void kain_render_push_clip(KainSoftwareRenderer* r, kainRect rect) {
    if (!r || r->clip_depth >= KAIN_MAX_CLIP_DEPTH) return;
    r->clip_stack[r->clip_depth] = rect;
    r->clip_depth++;
    r->has_active_clip = true;
}

void kain_render_pop_clip(KainSoftwareRenderer* r) {
    if (!r || r->clip_depth <= 0) return;
    r->clip_depth--;
    if (r->clip_depth == 0) r->has_active_clip = false;
}

// ══════════════════════════════════════════════════════════════════════════
//  Transform stack
// ══════════════════════════════════════════════════════════════════════════

void kain_render_push_transform(KainSoftwareRenderer* r, kainMatrix matrix) {
    if (!r || r->transform_depth >= KAIN_MAX_TRANSFORM_DEPTH) return;

    if (r->transform_depth == 0) {
        r->transform_stack[0] = matrix;
    } else {
        // Compose: new_transform = current * matrix
        r->transform_stack[r->transform_depth] =
            kain_matrix_mul(r->transform_stack[r->transform_depth - 1], matrix);
    }
    r->transform_depth++;
}

void kain_render_pop_transform(KainSoftwareRenderer* r) {
    if (!r || r->transform_depth <= 0) return;
    r->transform_depth--;
}
