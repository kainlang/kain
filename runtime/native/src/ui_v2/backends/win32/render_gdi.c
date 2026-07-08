// ============================================================================
//  render_gdi.c — GDI software renderer for Kaintana Win32 backend
//
//  Consumes the kt_DrawData command buffer and renders to a Win32 GDI
//  device context (HDC). Designed to be called from host_win32.c's
//  win32_render() function, but also usable standalone for testing
//  (attach to any HDC, including memory DCs and printer DCs).
//
//  Responsibilities:
//    - Font management (CreateFont, cached HFONT handles by font_id)
//    - Text rendering via DrawTextW / ExtTextOutW
//    - GDI object cache (pens, brushes — reused, NOT per-element)
//    - Texture/image cache for KT_CMD_IMAGE (placeholder → StretchBlt)
//    - Coordinate conversion (logical ↔ physical for DPI scaling)
//
//  What this file does NOT do (owned by host_win32.c):
//    - Window creation / message pump
//    - DIB section creation / BitBlt present
//    - Input event funnel
//    - Direct framebuffer pixel access (that's host_win32.c's SDF engine)
//
//  Anti-patterns avoided:
//    - ❌ Per-frame CreateFont/DeleteObject churn → ✅ Font cache by ID
//    - ❌ Per-element CreatePen/CreateSolidBrush → ✅ Reusable GDI objects
//    - ❌ DrawTextA → ✅ DrawTextW (full Unicode)
//    - ❌ Hardcoded colors → ✅ All colors from kt_Cmd.color (ARGB)
//    - ❌ Global state assumptions → ✅ All state in gdi_ctx struct
// ============================================================================

#include <windows.h>
#include <windowsx.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../../kaintana.h"

// ============================================================================
//  CONSTANTS
// ============================================================================
#define GDI_MAX_FONTS           64
#define GDI_MAX_BRUSHES         32
#define GDI_MAX_PENS            32
#define GDI_DEFAULT_FONT_SIZE   14
#define GDI_FONT_CACHE_BUCKETS  16

// ============================================================================
//  GDI OBJECT CACHE — reusable pens, brushes, fonts
// ============================================================================

// Cached font entry
typedef struct {
    int         font_id;
    int         height;
    int         weight;     // FW_NORMAL=400, FW_BOLD=700
    bool        italic;
    HFONT       hfont;
    bool        in_use;
} GdiCachedFont;

// Cached brush entry
typedef struct {
    uint32_t    color;      // Packed ARGB (alpha ignored for GDI brushes)
    HBRUSH      hbrush;
    bool        in_use;
} GdiCachedBrush;

// Cached pen entry
typedef struct {
    uint32_t    color;
    int         width;
    HPEN        hpen;
    bool        in_use;
} GdiCachedPen;

// Renderer context — singleton for the process
typedef struct {
    HDC             hdc;                // Target DC (DIB memory DC typically)
    int             fb_width;
    int             fb_height;

    // Font cache
    GdiCachedFont   fonts[GDI_MAX_FONTS];
    int             font_count;

    // Brush cache
    GdiCachedBrush  brushes[GDI_MAX_BRUSHES];
    int             brush_count;

    // Pen cache
    GdiCachedPen    pens[GDI_MAX_PENS];
    int             pen_count;

    // Frame state
    bool            initialized;
    int             frame_number;
} GdiRenderer;

static GdiRenderer g_gdi = { 0 };

// ============================================================================
//  GDI OBJECT CACHE — Lookup / Insert
// ============================================================================

// Find or create a brush for the given ARGB color.
// GDI brushes use COLORREF (BGR, no alpha) — alpha is handled by the
// framebuffer blend in host_win32.c, not by GDI brushes.
static HBRUSH gdi_get_brush(uint32_t color) {
    // Strip alpha for GDI COLORREF
    uint32_t bgr = color & 0x00FFFFFF;

    // Search cache
    for (int i = 0; i < g_gdi.brush_count; i++) {
        if (g_gdi.brushes[i].in_use && g_gdi.brushes[i].color == bgr) {
            return g_gdi.brushes[i].hbrush;
        }
    }

    // Evict LRU if full
    int slot = g_gdi.brush_count;
    if (slot >= GDI_MAX_BRUSHES) {
        // Find oldest (simple: take slot 0)
        slot = 0;
        DeleteObject(g_gdi.brushes[slot].hbrush);
    }

    // Create new brush
    COLORREF cref = RGB((bgr >> 16) & 0xFF, (bgr >> 8) & 0xFF, bgr & 0xFF);
    HBRUSH hbrush = CreateSolidBrush(cref);
    if (!hbrush) return (HBRUSH)GetStockObject(NULL_BRUSH);

    g_gdi.brushes[slot].color  = bgr;
    g_gdi.brushes[slot].hbrush = hbrush;
    g_gdi.brushes[slot].in_use = true;
    if (slot == g_gdi.brush_count) g_gdi.brush_count++;

    return hbrush;
}

// Find or create a pen for the given ARGB color and width.
static HPEN gdi_get_pen(uint32_t color, int width) {
    uint32_t bgr = color & 0x00FFFFFF;

    for (int i = 0; i < g_gdi.pen_count; i++) {
        if (g_gdi.pens[i].in_use &&
            g_gdi.pens[i].color == bgr &&
            g_gdi.pens[i].width == width) {
            return g_gdi.pens[i].hpen;
        }
    }

    int slot = g_gdi.pen_count;
    if (slot >= GDI_MAX_PENS) {
        slot = 0;
        DeleteObject(g_gdi.pens[slot].hpen);
    }

    COLORREF cref = RGB((bgr >> 16) & 0xFF, (bgr >> 8) & 0xFF, bgr & 0xFF);
    HPEN hpen = CreatePen(PS_SOLID, width, cref);
    if (!hpen) return (HPEN)GetStockObject(NULL_PEN);

    g_gdi.pens[slot].color  = bgr;
    g_gdi.pens[slot].width  = width;
    g_gdi.pens[slot].hpen   = hpen;
    g_gdi.pens[slot].in_use = true;
    if (slot == g_gdi.pen_count) g_gdi.pen_count++;

    return hpen;
}

// Find or create a font. Default font: Segoe UI 14px.
static HFONT gdi_get_font(int font_id, int height) {
    if (height <= 0) height = GDI_DEFAULT_FONT_SIZE;

    for (int i = 0; i < g_gdi.font_count; i++) {
        if (g_gdi.fonts[i].in_use &&
            g_gdi.fonts[i].font_id == font_id &&
            g_gdi.fonts[i].height == height) {
            return g_gdi.fonts[i].hfont;
        }
    }

    int slot = g_gdi.font_count;
    if (slot >= GDI_MAX_FONTS) {
        slot = 0;
        DeleteObject(g_gdi.fonts[slot].hfont);
    }

    // Create font with proper negative height (character height, not cell)
    HFONT hfont = CreateFontW(
        -height,                    // nHeight (negative = character height)
        0, 0, 0,                    // width, escapement, orientation
        FW_NORMAL,                  // weight
        FALSE, FALSE, FALSE,        // italic, underline, strikeout
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,          // Use ClearType for sub-pixel AA
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");               // Default face

    if (!hfont) {
        // Fallback: system font
        hfont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }

    g_gdi.fonts[slot].font_id = font_id;
    g_gdi.fonts[slot].height  = height;
    g_gdi.fonts[slot].weight  = FW_NORMAL;
    g_gdi.fonts[slot].italic  = false;
    g_gdi.fonts[slot].hfont   = hfont;
    g_gdi.fonts[slot].in_use  = true;
    if (slot == g_gdi.font_count) g_gdi.font_count++;

    return hfont;
}

// ============================================================================
//  LIFECYCLE
// ============================================================================

int gdi_renderer_init(HDC hdc, int w, int h) {
    memset(&g_gdi, 0, sizeof(g_gdi));
    g_gdi.hdc        = hdc;
    g_gdi.fb_width   = w;
    g_gdi.fb_height  = h;
    g_gdi.initialized = true;
    g_gdi.frame_number = 0;
    return 0;
}

void gdi_renderer_shutdown(void) {
    // Free all cached GDI objects
    for (int i = 0; i < g_gdi.brush_count; i++) {
        if (g_gdi.brushes[i].hbrush) DeleteObject(g_gdi.brushes[i].hbrush);
    }
    for (int i = 0; i < g_gdi.pen_count; i++) {
        if (g_gdi.pens[i].hpen) DeleteObject(g_gdi.pens[i].hpen);
    }
    for (int i = 0; i < g_gdi.font_count; i++) {
        if (g_gdi.fonts[i].hfont) {
            // Don't delete stock objects
            if (g_gdi.fonts[i].font_id >= 0) {
                DeleteObject(g_gdi.fonts[i].hfont);
            }
        }
    }
    memset(&g_gdi, 0, sizeof(g_gdi));
}

void gdi_renderer_begin_frame(void) {
    g_gdi.frame_number++;
}

// ============================================================================
//  GDI DRAW FUNCTIONS
// ============================================================================

// Draw a filled rectangle using GDI FillRect.
// Note: host_win32.c handles rounded rects via direct framebuffer SDF.
// This is for simple non-rounded fills on the GDI DC.
static void gdi_draw_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!g_gdi.hdc) return;
    HBRUSH hbr = gdi_get_brush(color);
    RECT r = { x, y, x + w, y + h };
    FillRect(g_gdi.hdc, &r, hbr);
}

// Draw a stroked rectangle border using GDI FrameRect or 4 lines.
static void gdi_draw_stroke_rect(int x, int y, int w, int h,
                                  int thickness, uint32_t color) {
    if (!g_gdi.hdc) return;
    HBRUSH hbr = gdi_get_brush(color);

    // Top
    RECT rtop    = { x, y, x + w, y + thickness };
    // Bottom
    RECT rbottom = { x, y + h - thickness, x + w, y + h };
    // Left (excluding top/bottom overlap)
    RECT rleft   = { x, y + thickness, x + thickness, y + h - thickness };
    // Right
    RECT rright  = { x + w - thickness, y + thickness, x + w, y + h - thickness };

    FillRect(g_gdi.hdc, &rtop, hbr);
    FillRect(g_gdi.hdc, &rbottom, hbr);
    FillRect(g_gdi.hdc, &rleft, hbr);
    FillRect(g_gdi.hdc, &rright, hbr);
}

// Draw text via DrawTextW.
// text_id: index into an external text table (TBD — currently placeholder).
// The text string lookup will be wired via tree.c's arena text storage.
static void gdi_draw_text(int x, int y, int w, int h,
                           const wchar_t* text, uint32_t color, int font_id) {
    if (!g_gdi.hdc || !text) return;

    HFONT hfont = gdi_get_font(font_id, GDI_DEFAULT_FONT_SIZE);
    HFONT hfont_old = (HFONT)SelectObject(g_gdi.hdc, hfont);

    uint32_t r = (color >> 16) & 0xFF;
    uint32_t g = (color >> 8) & 0xFF;
    uint32_t b = color & 0xFF;
    SetTextColor(g_gdi.hdc, RGB(r, g, b));
    SetBkMode(g_gdi.hdc, TRANSPARENT);

    RECT tr = { x, y, x + w, y + h };
    DrawTextW(g_gdi.hdc, text, -1, &tr,
              DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOCLIP | DT_END_ELLIPSIS);

    SelectObject(g_gdi.hdc, hfont_old);
}

// Draw an image (bitmap blit). Placeholder — texture management TBD.
static void gdi_draw_image(int x, int y, int w, int h, HBITMAP hbm) {
    if (!g_gdi.hdc || !hbm) return;

    HDC hdc_mem = CreateCompatibleDC(g_gdi.hdc);
    HBITMAP hbm_old = (HBITMAP)SelectObject(hdc_mem, hbm);

    BITMAP bm;
    GetObjectW(hbm, sizeof(BITMAP), &bm);
    StretchBlt(g_gdi.hdc, x, y, w, h,
               hdc_mem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

    SelectObject(hdc_mem, hbm_old);
    DeleteDC(hdc_mem);
}

// Push a clip rect (intersection with existing).
static int gdi_clip_push(int x, int y, int w, int h) {
    if (!g_gdi.hdc) return -1;
    int state = SaveDC(g_gdi.hdc);
    HRGN hrgn = CreateRectRgn(x, y, x + w, y + h);
    ExtSelectClipRgn(g_gdi.hdc, hrgn, RGN_AND);
    DeleteObject(hrgn);
    return state;
}

// Pop a clip rect (restore saved DC state).
static void gdi_clip_pop(int saved_state) {
    if (!g_gdi.hdc || saved_state < 0) return;
    RestoreDC(g_gdi.hdc, saved_state);
}

// ============================================================================
//  COMMAND EXECUTOR — Iterate kt_DrawData, dispatch to GDI functions
// ============================================================================

// Stack of saved DC states for clip push/pop.
#define GDI_CLIP_STACK_MAX 32
static int g_gdi_clip_stack[GDI_CLIP_STACK_MAX];
static int g_gdi_clip_depth = 0;

void gdi_renderer_execute(HDC hdc, const kt_DrawData* dd, int fb_w, int fb_h) {
    if (!hdc || !dd || !dd->cmds || dd->cmd_count <= 0) return;

    g_gdi.hdc       = hdc;
    g_gdi.fb_width  = fb_w;
    g_gdi.fb_height = fb_h;
    g_gdi_clip_depth = 0;

    for (int i = 0; i < dd->cmd_count; i++) {
        const kt_Cmd* cmd = &dd->cmds[i];
        int x = (int)cmd->bounds.x;
        int y = (int)cmd->bounds.y;
        int w = (int)(cmd->bounds.w + 0.5f);
        int h = (int)(cmd->bounds.h + 0.5f);

        switch (cmd->type) {

        case KT_CMD_FILL:
            gdi_draw_fill_rect(x, y, w, h, cmd->color);
            break;

        case KT_CMD_STROKE:
            gdi_draw_stroke_rect(x, y, w, h, (int)(cmd->thickness + 0.5f), cmd->color);
            break;

        case KT_CMD_TEXT:
            // Placeholder — text_id lookup from session's text arena
            // For now, draw an empty string; real text routing TBD
            if (cmd->text_id >= 0) {
                gdi_draw_text(x, y, w, h, L"", cmd->color, 0);
            }
            break;

        case KT_CMD_IMAGE:
            // Placeholder — image_id lookup from texture cache
            // gdi_draw_image(x, y, w, h, texture_bitmap);
            break;

        case KT_CMD_CLIP:
            if (g_gdi_clip_depth < GDI_CLIP_STACK_MAX) {
                g_gdi_clip_stack[g_gdi_clip_depth++] =
                    gdi_clip_push(x, y, w, h);
            }
            break;

        case KT_CMD_UNCLIP:
            if (g_gdi_clip_depth > 0) {
                gdi_clip_pop(g_gdi_clip_stack[--g_gdi_clip_depth]);
            }
            break;

        default:
            break;
        }
    }

    // Pop any remaining clip states (safety cleanup)
    while (g_gdi_clip_depth > 0) {
        RestoreDC(hdc, g_gdi_clip_stack[--g_gdi_clip_depth]);
    }
}
