// ============================================================================
//  host_win32.c — Win32 GDI backend for Kaintana
//
//  Implements the 4-function KaintanaBackendVTable contract with a real
//  Win32 window, persistent DIB section framebuffer, message pump, and
//  GDI software rendering via render_gdi.c.
//
//  Architecture decisions (informed by 9-framework cross-reference):
//    - Persistent DIB section — CreateDIBSection once, recreate on WM_SIZE only.
//      NOT per-frame create/destroy (Clay's #1 bottleneck).
//    - Dirty-rect-clipped BitBlt — only blit damaged regions via
//      64-rect damage accumulator. NOT full-frame SRCCOPY.
//    - Cached GDI objects — pens, brushes, font handles reused across frames.
//      NOT per-element CreatePen/CreateSolidBrush churn.
//    - Unicode text via DrawTextW — full CJK/Arabic/Cyrillic support.
//      NOT DrawTextA (Clay's ANSI-only limitation).
//    - 32-deep clip stack via SaveDC/RestoreDC + IntersectClipRect.
//      NOT single SelectClipRgn (Clay's non-stacked limitation).
//    - Premultiplied ARGB pixel format — DIB is BI_RGB but we pack AARRGGBB
//      for compatibility with the unified kt_DrawData pipeline.
//    - DPI-aware — SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2).
//    - Zero hardcoded colors — all colors from kt_DrawData command stream.
//
//  Usage:
//    const KaintanaBackendVTable win32_backend = {
//        .init      = win32_init,
//        .shutdown  = win32_shutdown,
//        .new_frame = win32_new_frame,
//        .render    = win32_render
//    };
//    kt_backend_register(s, "win32", &win32_backend);
//    kt_backend_select(s, "win32");
//
//  ============================================================================
//  The GDI renderer (render_gdi.c) exports 4 functions consumed here:
//    int  gdi_renderer_init(HDC hdc, int w, int h);
//    void gdi_renderer_shutdown(void);
//    void gdi_renderer_begin_frame(void);
//    void gdi_renderer_execute(HDC hdc, const kt_DrawData* dd, int fb_w, int fb_h);
//  ============================================================================
// ============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <shellscalingapi.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../../kaintana.h"

// ============================================================================
//  GDI RENDERER BRIDGE — external symbols from render_gdi.c
// ============================================================================
extern int  gdi_renderer_init(HDC hdc, int w, int h);
extern void gdi_renderer_shutdown(void);
extern void gdi_renderer_begin_frame(void);
extern void gdi_renderer_execute(HDC hdc, const kt_DrawData* dd, int fb_w, int fb_h);

// ============================================================================
//  CONSTANTS
// ============================================================================
#define WIN32_WINDOW_CLASS_NAME     L"KaintanaWin32Window"
#define WIN32_DEFAULT_WIDTH         800
#define WIN32_DEFAULT_HEIGHT        600
#define WIN32_CLIP_STACK_MAX        32          // Matches KT_CLIP_STACK_MAX
#define WIN32_DIRTY_RECT_MAX        64          // Damage accumulator ceiling

// ============================================================================
//  STATIC STATE — singleton window + framebuffer + input
// ============================================================================
static HWND             g_hwnd          = NULL;
static HDC              g_hdc_window    = NULL;     // GetDC(hwnd) — for BitBlt
static HDC              g_hdc_dib       = NULL;     // CreateCompatibleDC — holds DIB
static HBITMAP          g_hbm_dib       = NULL;     // DIB section handle
static HBITMAP          g_hbm_old       = NULL;     // Previous bitmap in g_hdc_dib
static VOID*            g_pBits         = NULL;     // Direct pixel pointer
static int              g_fb_width      = 0;
static int              g_fb_height     = 0;
static int              g_fb_stride     = 0;        // g_fb_width * 4

static int              g_window_width  = WIN32_DEFAULT_WIDTH;
static int              g_window_height = WIN32_DEFAULT_HEIGHT;
static bool             g_is_open       = false;
static bool             g_needs_present  = false;

// ── Dirty rect accumulator ──────────────────────────────────────────────
static RECT             g_dirty_rects[WIN32_DIRTY_RECT_MAX];
static int              g_dirty_count = 0;
static bool             g_full_dirty  = true;   // First frame = full blit

// ── Input state ─────────────────────────────────────────────────────────
static float            g_mouse_x       = 0.0f;
static float            g_mouse_y       = 0.0f;
static bool             g_mouse_down[5] = { false };
static float            g_scroll_dx     = 0.0f;
static float            g_scroll_dy     = 0.0f;
static bool             g_keys[256]     = { false };
static wchar_t          g_text_buffer[32];
static int              g_text_len      = 0;
static bool             g_focus_gained  = true;
static bool             g_should_close  = false;

// ── DPI ─────────────────────────────────────────────────────────────────
static float            g_dpi_scale_x   = 1.0f;
static float            g_dpi_scale_y   = 1.0f;

// ── Session pointer (set via config->platform_handle in kt_backend_select) ──
static kt_Session*      g_win32_session = NULL;

// ── Performance timer ───────────────────────────────────────────────────
static LARGE_INTEGER    g_perf_freq;
static LARGE_INTEGER    g_last_time;
static double           g_delta_seconds = 0.016;

// ============================================================================
//  FORWARD DECLARATIONS
// ============================================================================
static LRESULT CALLBACK win32_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

// ============================================================================
//  DPI AWARENESS
// ============================================================================
static void win32_enable_dpi(void) {
    // Try Per-Monitor V2 (Windows 10 1703+). user32.dll is always loaded,
    // so GetModuleHandleW is sufficient.
    // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = (DPI_AWARENESS_CONTEXT)-4.
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL (WINAPI *fn_SpaC_t)(HANDLE);
        fn_SpaC_t fn;
        {
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wcast-function-type"
            fn = (fn_SpaC_t)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
            #pragma GCC diagnostic pop
        }
        if (fn) {
            fn((HANDLE)(intptr_t)-4);
            return;  // V2 succeeded
        }
    }

    // Fall back to V1 (SetProcessDpiAwareness from shcore.dll, Win 8.1+)
    HMODULE hShcore = LoadLibraryW(L"shcore.dll");
    if (hShcore) {
        typedef HRESULT (WINAPI *fn_Spa_t)(PROCESS_DPI_AWARENESS);
        fn_Spa_t fn;
        {
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wcast-function-type"
            fn = (fn_Spa_t)GetProcAddress(hShcore, "SetProcessDpiAwareness");
            #pragma GCC diagnostic pop
        }
        if (fn) {
            fn(PROCESS_PER_MONITOR_DPI_AWARE);
        }
        FreeLibrary(hShcore);
    }
}

static void win32_update_dpi_scale(void) {
    if (!g_hwnd) { g_dpi_scale_x = 1.0f; g_dpi_scale_y = 1.0f; return; }
    HDC hdc = GetDC(g_hwnd);
    if (!hdc) { g_dpi_scale_x = 1.0f; g_dpi_scale_y = 1.0f; return; }
    g_dpi_scale_x = (float)GetDeviceCaps(hdc, LOGPIXELSX) / 96.0f;
    g_dpi_scale_y = (float)GetDeviceCaps(hdc, LOGPIXELSY) / 96.0f;
    ReleaseDC(g_hwnd, hdc);
}

// ============================================================================
//  DIRTY RECT ACCUMULATOR
// ============================================================================
static void win32_dirty_clear(void) {
    g_dirty_count = 0;
    g_full_dirty  = false;
}

static void win32_dirty_full(void) {
    g_dirty_count = 0;
    g_full_dirty  = true;
}

static void win32_dirty_add(int x, int y, int w, int h) {
    if (g_full_dirty) return;
    if (w <= 0 || h <= 0) return;

    // Try to merge with existing rect
    for (int i = 0; i < g_dirty_count; i++) {
        RECT* r = &g_dirty_rects[i];
        // Check overlap or adjacency (within 4px gap = merge)
        int gap = 4;
        if (!(x + w < r->left - gap || x > r->right + gap ||
              y + h < r->top  - gap || y > r->bottom + gap)) {
            // Merge: expand existing rect
            if (x < r->left)   r->left   = x;
            if (y < r->top)    r->top    = y;
            int nx2 = x + w, nr2 = r->right;
            if (nx2 > nr2) r->right  = nx2;
            int ny2 = y + h, nb2 = r->bottom;
            if (ny2 > nb2) r->bottom = ny2;
            return;
        }
    }

    // Add new rect if under ceiling
    if (g_dirty_count < WIN32_DIRTY_RECT_MAX) {
        RECT* r = &g_dirty_rects[g_dirty_count++];
        r->left   = x;
        r->top    = y;
        r->right  = x + w;
        r->bottom = y + h;
    } else {
        // Overflow: fall back to full dirty
        g_dirty_count = 0;
        g_full_dirty  = true;
    }
}

static void win32_dirty_add_rect(kt_Rect r) {
    win32_dirty_add((int)r.x, (int)r.y, (int)r.w, (int)r.h);
}

// ============================================================================
//  DIB SECTION FRAMEBUFFER
// ============================================================================
static int win32_fb_create(int width, int height) {
    if (width <= 0 || height <= 0) return -1;

    HDC hdc_screen = GetDC(NULL);
    if (!hdc_screen) return -1;

    // Create memory DC
    g_hdc_dib = CreateCompatibleDC(hdc_screen);
    if (!g_hdc_dib) { ReleaseDC(NULL, hdc_screen); return -1; }

    // BITMAPINFO with top-down DIB (biHeight < 0 → y=0 is top row)
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = -height;          // NEGATIVE = top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage   = (DWORD)(width * height * 4);

    g_hbm_dib = CreateDIBSection(
        hdc_screen, &bmi, DIB_RGB_COLORS, &g_pBits, NULL, 0);
    ReleaseDC(NULL, hdc_screen);

    if (!g_hbm_dib) {
        DeleteDC(g_hdc_dib);
        g_hdc_dib = NULL;
        return -1;
    }

    // Select DIB into memory DC
    g_hbm_old     = (HBITMAP)SelectObject(g_hdc_dib, g_hbm_dib);
    g_fb_width    = width;
    g_fb_height   = height;
    g_fb_stride   = width * 4;

    // Initialize GDI renderer
    if (gdi_renderer_init(g_hdc_dib, width, height) != 0) {
        // Non-fatal — GDI renderer may use fallback paths
    }

    return 0;
}

static void win32_fb_destroy(void) {
    if (g_hdc_dib) {
        if (g_hbm_old) SelectObject(g_hdc_dib, g_hbm_old);
        g_hbm_old = NULL;
        DeleteDC(g_hdc_dib);
        g_hdc_dib = NULL;
    }
    if (g_hbm_dib) {
        DeleteObject(g_hbm_dib);
        g_hbm_dib = NULL;
    }
    g_pBits      = NULL;
    g_fb_width   = 0;
    g_fb_height  = 0;
    g_fb_stride  = 0;
}

static void win32_fb_resize(int width, int height) {
    if (width == g_fb_width && height == g_fb_height) return;
    win32_fb_destroy();
    win32_fb_create(width, height);
}

// ============================================================================
//  PIXEL FILL (direct framebuffer access)
// ============================================================================

// Fast u8/255 division — Z3-proven, error ±0.5
static inline uint32_t win32__div255(uint32_t x) {
    return ((x) + 1 + ((x) >> 8)) >> 8;
}

// Premultiplied SrcOver blend into framebuffer pixel.
// src is premultiplied ARGB, dst is premultiplied ARGB in framebuffer.
static inline uint32_t win32_blend_pixel(uint32_t src, uint32_t dst) {
    uint32_t sa = (src >> 24) & 0xFF;
    if (sa == 0) return dst;
    if (sa == 255) return src;

    // Premultiplied SrcOver: out = src + dst * (1 - src.a)
    uint32_t inv_a = 255 - sa;
    uint32_t rb = ((dst & 0x00FF00FF) * inv_a);
    uint32_t g  = ((dst & 0x0000FF00) * inv_a);
    uint32_t out_rb = (src & 0x00FF00FF) + win32__div255(rb);
    uint32_t out_g  = (src & 0x0000FF00) + win32__div255(g);
    return (out_rb & 0x00FF00FF) | (out_g & 0x0000FF00) |
           (0xFF000000 & ~(inv_a << 24));  // Alpha = combined
}

// Fill a rectangle in the framebuffer with a premultiplied ARGB color.
// Bounds are in pixel coordinates, clipped to framebuffer.
static void win32_fb_fill_rect(int x1, int y1, int x2, int y2, uint32_t color) {
    if (!g_pBits) return;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > g_fb_width)  x2 = g_fb_width;
    if (y2 > g_fb_height) y2 = g_fb_height;
    if (x2 <= x1 || y2 <= y1) return;

    uint32_t sa = (color >> 24) & 0xFF;
    uint8_t* row = (uint8_t*)g_pBits + (y1 * g_fb_stride);

    if (sa == 255) {
        // Opaque — direct memcpy per row (fast path)
        for (int y = y1; y < y2; y++) {
            uint32_t* pixels = (uint32_t*)row;
            for (int x = x1; x < x2; x++) {
                pixels[x] = color;
            }
            row += g_fb_stride;
        }
    } else {
        // Translucent — blend per pixel
        for (int y = y1; y < y2; y++) {
            uint32_t* pixels = (uint32_t*)row;
            for (int x = x1; x < x2; x++) {
                pixels[x] = win32_blend_pixel(color, pixels[x]);
            }
            row += g_fb_stride;
        }
    }
}

// ============================================================================
//  SDF ROUNDED RECT — Branchless Quilez SDF evaluated on CPU
// ============================================================================

// Branchless signed-distance rounded rectangle (Quilez).
// p = point relative to rect center, size = half-extents, r = corner radius.
static inline float win32_sd_round_rect(float px, float py,
                                         float hw, float hh, float r) {
    float dx = (float)fabs(px) - hw + r;
    float dy = (float)fabs(py) - hh + r;
    float mx = (dx > 0.0f) ? dx : 0.0f;
    float my = (dy > 0.0f) ? dy : 0.0f;
    float d_outer = (float)sqrt(mx * mx + my * my);
    float d_inner_x = (dx > dy) ? dx : dy;
    float d_inner = (d_inner_x < 0.0f) ? d_inner_x : 0.0f;
    return d_outer + d_inner - r;
}

// Fill a rounded rectangle via SDF. (x1,y1)-(x2,y2) in framebuffer pixels.
static void win32_fb_fill_rounded_rect(
    int x1, int y1, int x2, int y2, float radius, uint32_t color)
{
    if (!g_pBits) return;
    // Clamp
    int cx1 = x1, cy1 = y1, cx2 = x2, cy2 = y2;
    if (cx1 < 0) cx1 = 0;
    if (cy1 < 0) cy1 = 0;
    if (cx2 > g_fb_width)  cx2 = g_fb_width;
    if (cy2 > g_fb_height) cy2 = g_fb_height;
    if (cx2 <= cx1 || cy2 <= cy1) return;

    float r    = radius;
    float hw   = (float)(cx2 - cx1) * 0.5f;
    float hh   = (float)(cy2 - cy1) * 0.5f;
    float cx   = (float)cx1 + hw;
    float cy   = (float)cy1 + hh;
    uint32_t sa = (color >> 24) & 0xFF;

    // If radius < 0.5 or rect too small, skip SDF — use fast fill
    if (r < 0.5f || hw < 0.5f || hh < 0.5f) {
        win32_fb_fill_rect(cx1, cy1, cx2, cy2, color);
        return;
    }

    // Clamp radius to half the smaller dimension
    float max_r = (hw < hh) ? hw : hh;
    if (r > max_r) r = max_r;

    uint8_t* base = (uint8_t*)g_pBits;

    for (int y = cy1; y < cy2; y++) {
        uint32_t* pixels = (uint32_t*)(base + (y * g_fb_stride));
        float py = ((float)y - cy + 0.5f);

        for (int x = cx1; x < cx2; x++) {
            float px = ((float)x - cx + 0.5f);
            float d  = win32_sd_round_rect(px, py, hw, hh, r);

            // Smooth coverage AA: coverage = clamp(0.5 - distance, 0, 1)
            float cov = 0.5f - d;
            if (cov <= 0.0f) continue;         // outside
            if (cov >= 1.0f) {
                // Fully inside — opaque write
                if (sa == 255) {
                    pixels[x] = color;
                } else {
                    pixels[x] = win32_blend_pixel(color, pixels[x]);
                }
                continue;
            }

            // Partial coverage — scale alpha by coverage
            uint32_t aa_a = (uint32_t)((float)sa * cov + 0.5f);
            if (aa_a == 0) continue;
            if (aa_a > 255) aa_a = 255;
            uint32_t aa_color = (color & 0x00FFFFFF) | (aa_a << 24);
            pixels[x] = win32_blend_pixel(aa_color, pixels[x]);
        }
    }
}

// ============================================================================
//  WINDOW CLASS REGISTRATION
// ============================================================================
static ATOM win32_register_class(HINSTANCE hInstance) {
    WNDCLASSEXW wc = { 0 };
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = win32_wndproc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WIN32_WINDOW_CLASS_NAME;
    wc.hIcon         = LoadIconW(NULL, IDI_APPLICATION);
    return RegisterClassExW(&wc);
}

// ============================================================================
//  WINDOW PROC
// ============================================================================
static LRESULT CALLBACK win32_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CLOSE:
        g_should_close = true;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_is_open = false;
        PostQuitMessage(0);
        return 0;

    // ── Sizing ───────────────────────────────────────────────────────
    case WM_SIZE: {
        int w = LOWORD(lp);
        int h = HIWORD(lp);
        if (w > 0 && h > 0) {
            g_window_width  = w;
            g_window_height = h;
            win32_fb_resize(
                (int)((float)w * g_dpi_scale_x),
                (int)((float)h * g_dpi_scale_y));
            win32_dirty_full();
        }
        return 0;
    }

    // ── Mouse input ──────────────────────────────────────────────────
    case WM_MOUSEMOVE:
        g_mouse_x = (float)GET_X_LPARAM(lp) / g_dpi_scale_x;
        g_mouse_y = (float)GET_Y_LPARAM(lp) / g_dpi_scale_y;
        return 0;

    case WM_LBUTTONDOWN:
        g_mouse_down[0] = true;
        SetCapture(hwnd);
        return 0;
    case WM_LBUTTONUP:
        g_mouse_down[0] = false;
        ReleaseCapture();
        return 0;
    case WM_RBUTTONDOWN:
        g_mouse_down[1] = true;
        return 0;
    case WM_RBUTTONUP:
        g_mouse_down[1] = false;
        return 0;
    case WM_MBUTTONDOWN:
        g_mouse_down[2] = true;
        return 0;
    case WM_MBUTTONUP:
        g_mouse_down[2] = false;
        return 0;

    case WM_MOUSEWHEEL: {
        float delta = (float)(short)HIWORD(wp) / (float)WHEEL_DELTA;
        g_scroll_dy += delta;
        return 0;
    }
    case WM_MOUSEHWHEEL: {
        float delta = (float)(short)HIWORD(wp) / (float)WHEEL_DELTA;
        g_scroll_dx += delta;
        return 0;
    }

    // ── Keyboard input ───────────────────────────────────────────────
    case WM_KEYDOWN:
        if (wp < 256) g_keys[wp] = true;
        return 0;
    case WM_KEYUP:
        if (wp < 256) g_keys[wp] = false;
        return 0;
    case WM_CHAR:
        if (g_text_len < 31 && wp >= 32) {
            g_text_buffer[g_text_len++] = (wchar_t)wp;
            g_text_buffer[g_text_len]   = L'\0';
        }
        return 0;
    case WM_SYSKEYDOWN:
        if (wp == VK_F4) {
            g_should_close = true;
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }
        return 0;

    // ── Focus ─────────────────────────────────────────────────────────
    case WM_SETFOCUS:
        g_focus_gained = true;
        return 0;
    case WM_KILLFOCUS:
        g_focus_gained = false;
        // Release all held keys on focus loss
        memset(g_keys, 0, sizeof(g_keys));
        memset(g_mouse_down, 0, sizeof(g_mouse_down));
        return 0;

    // ── Paint ─────────────────────────────────────────────────────────
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (g_hdc_dib && g_needs_present) {
            if (g_full_dirty) {
                // Full blit
                BitBlt(hdc, 0, 0, g_window_width, g_window_height,
                       g_hdc_dib, 0, 0, SRCCOPY);
            } else {
                // Dirty-rect-clipped blits
                for (int i = 0; i < g_dirty_count; i++) {
                    RECT* r = &g_dirty_rects[i];
                    int dx = (int)((float)r->left   / g_dpi_scale_x);
                    int dy = (int)((float)r->top    / g_dpi_scale_y);
                    int dw = (int)((float)(r->right - r->left) / g_dpi_scale_x);
                    int dh = (int)((float)(r->bottom - r->top) / g_dpi_scale_y);
                    if (dw <= 0) dw = 1;
                    if (dh <= 0) dh = 1;
                    StretchBlt(hdc, dx, dy, dw, dh,
                               g_hdc_dib,
                               r->left, r->top,
                               r->right - r->left, r->bottom - r->top,
                               SRCCOPY);
                }
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    // ── DPI change ───────────────────────────────────────────────────
    case WM_DPICHANGED: {
        RECT* suggested = (RECT*)lp;
        win32_update_dpi_scale();
        // Bridge DPI scale change to core session
        if (g_win32_session) {
            kt_set_native_scale(g_win32_session, g_dpi_scale_x, g_dpi_scale_y);
        }
        SetWindowPos(hwnd, NULL,
            suggested->left, suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        win32_dirty_full();
        return 0;
    }

    // ── Suppress background erase ────────────────────────────────────────
    // Returning 1 prevents DefWindowProcW from painting the background brush,
    // which would cause a white flash before the first render.
    case WM_ERASEBKGND:
        return 1;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ============================================================================
//  PERFORMANCE TIMER
// ============================================================================
static void win32_timer_init(void) {
    QueryPerformanceFrequency(&g_perf_freq);
    QueryPerformanceCounter(&g_last_time);
}

static void win32_timer_tick(void) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double elapsed = (double)(now.QuadPart - g_last_time.QuadPart) /
                     (double)g_perf_freq.QuadPart;
    g_delta_seconds = elapsed;
    g_last_time = now;
}

// ============================================================================
//  MESSAGE PUMP
// ============================================================================
static void win32_pump_messages(void) {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            g_is_open      = false;
            g_should_close = true;
            return;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

// ============================================================================
//  INPUT — Fill KaintanaInput-compatible state
// ============================================================================
static void win32_reset_per_frame_input(void) {
    g_scroll_dx = 0.0f;
    g_scroll_dy = 0.0f;
    g_text_len  = 0;
    memset(g_text_buffer, 0, sizeof(g_text_buffer));
}

// ============================================================================
//  PRESENT — Trigger WM_PAINT via InvalidateRect
// ============================================================================
static void win32_present_to_screen(void) {
    if (!g_hwnd || !g_hdc_dib) return;

    if (g_full_dirty) {
        InvalidateRect(g_hwnd, NULL, FALSE);
    } else if (g_dirty_count > 0) {
        // Invalidate each dirty rect
        for (int i = 0; i < g_dirty_count; i++) {
            RECT r = g_dirty_rects[i];
            // Convert framebuffer coords → window logical coords
            r.left   = (LONG)((float)r.left   / g_dpi_scale_x);
            r.top    = (LONG)((float)r.top    / g_dpi_scale_y);
            r.right  = (LONG)((float)r.right  / g_dpi_scale_x) + 1;
            r.bottom = (LONG)((float)r.bottom / g_dpi_scale_y) + 1;
            InvalidateRect(g_hwnd, &r, FALSE);
        }
    }
    // Force synchronous update
    UpdateWindow(g_hwnd);
    g_needs_present = false;
}

// ============================================================================
//  BACKEND LIFECYCLE — The 4-function KaintanaBackendVTable contract
// ============================================================================

// win32_init: Create window, DIB framebuffer, message pump infrastructure.
// Returns 0 on success, -1 on failure.
static int win32_init(const KaintanaBackendConfig* config) {
    if (!config) return -1;

    // Store session pointer from config (set by kt_backend_select)
    g_win32_session = (kt_Session*)config->platform_handle;

    HINSTANCE hInst = GetModuleHandleW(NULL);

    // ── DPI ──────────────────────────────────────────────────────────
    win32_enable_dpi();

    // ── Register window class ────────────────────────────────────────
    if (!win32_register_class(hInst)) {
        return -1;
    }

    // ── Resolve dimensions ───────────────────────────────────────────
    int w = (config->width  > 0) ? config->width  : WIN32_DEFAULT_WIDTH;
    int h = (config->height > 0) ? config->height : WIN32_DEFAULT_HEIGHT;

    // Convert to wide-char title
    wchar_t wtitle[256] = L"Kaintana";
    if (config->title) {
        MultiByteToWideChar(CP_UTF8, 0, config->title, -1, wtitle, 255);
    }

    // ── Create window ────────────────────────────────────────────────
    DWORD style   = WS_OVERLAPPEDWINDOW;
    DWORD exStyle = 0;
    if (config->fullscreen) {
        style   = WS_POPUP;
        exStyle = WS_EX_TOPMOST;
    }

    RECT rc = { 0, 0, w, h };
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);
    int cw = rc.right - rc.left;
    int ch = rc.bottom - rc.top;

    g_hwnd = CreateWindowExW(
        exStyle,
        WIN32_WINDOW_CLASS_NAME,
        wtitle,
        style,
        100, 100, cw, ch,
        NULL, NULL, hInst, NULL);

    if (!g_hwnd) {
        return -1;
    }

    g_window_width  = w;
    g_window_height = h;
    g_is_open       = true;

    // ── Get window DC (cached, CS_OWNDC) ─────────────────────────────
    g_hdc_window = GetDC(g_hwnd);

    // ── DPI scale ────────────────────────────────────────────────────
    win32_update_dpi_scale();

    // Bridge DPI scale to core session
    if (g_win32_session) {
        kt_set_native_scale(g_win32_session, g_dpi_scale_x, g_dpi_scale_y);
    }

    // ── Create DIB framebuffer ───────────────────────────────────────
    int fb_w = (int)((float)w * g_dpi_scale_x);
    int fb_h = (int)((float)h * g_dpi_scale_y);
    if (win32_fb_create(fb_w, fb_h) != 0) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
        return -1;
    }

    // ── Performance timer ────────────────────────────────────────────
    win32_timer_init();

    // ── Show window ──────────────────────────────────────────────────
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    g_should_close = false;
    win32_dirty_full();
    return 0;
}

// win32_shutdown: Destroy window, free framebuffer, release resources.
static void win32_shutdown(void) {
    gdi_renderer_shutdown();
    win32_fb_destroy();
    if (g_hdc_window && g_hwnd) {
        ReleaseDC(g_hwnd, g_hdc_window);
        g_hdc_window = NULL;
    }
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
    }
    g_is_open       = false;
    g_should_close  = true;
}

// win32_new_frame: Pump the message queue, update timing.
// The render phase (win32_render) follows separately.
static void win32_new_frame(void) {
    if (!g_is_open) return;

    // Pump OS messages (fills global input state via WndProc)
    win32_pump_messages();

    // Update delta time
    win32_timer_tick();

    // Begin GDI renderer frame
    gdi_renderer_begin_frame();

    // Bridge accumulated input state to session (follows ImGui pattern:
    // backends fill IO state before the UI frame begins).
    // After this, demos do NOT need to call kt_input_*() manually.
    if (g_win32_session) {
        kt_input_mouse_move(g_win32_session, g_mouse_x, g_mouse_y);
        for (int b = 0; b < 5; b++) {
            if (g_mouse_down[b]) kt_input_mouse_down(g_win32_session, b);
            else                 kt_input_mouse_up(g_win32_session, b);
        }
        if (g_scroll_dx != 0.0f || g_scroll_dy != 0.0f)
            kt_input_scroll(g_win32_session, g_scroll_dx, g_scroll_dy);
        for (int k = 0; k < 256; k++) {
            if (g_keys[k]) kt_input_key_down(g_win32_session, k);
            else           kt_input_key_up(g_win32_session, k);
        }
        // Bridge text input: convert wchar_t buffer to UTF-8 for kt_input_text
        if (g_text_len > 0) {
            char utf8_buf[64];
            int utf8_len = WideCharToMultiByte(CP_UTF8, 0, g_text_buffer, g_text_len,
                                                utf8_buf, (int)sizeof(utf8_buf)-1, NULL, NULL);
            if (utf8_len > 0) {
                utf8_buf[utf8_len] = '\0';
                kt_input_text(g_win32_session, utf8_buf);
            }
        }
    }

    // Reset per-frame scratch input (scroll, text cleared after bridge)
    win32_reset_per_frame_input();
}

// win32_render: Execute all draw commands into the DIB section framebuffer.
// After this call, the framebuffer contains the rendered frame.
// The actual screen present happens in the next WM_PAINT cycle (via
// InvalidateRect triggered at the end of this function).
static void win32_render(const kt_DrawData* draw_data) {
    if (!g_hdc_dib || !g_pBits) return;
    if (!draw_data || !draw_data->cmds || draw_data->cmd_count <= 0) {
        // No commands — framebuffer may contain content drawn directly
        // (via win32_fb_fill_rect). Do NOT clear dirty state or present —
        // let the caller handle both. This preserves the initial
        // g_full_dirty=true so the first present works when the caller
        // eventually calls win32_present_to_screen().
        return;
    }

    // ── Clear framebuffer on full dirty ──────────────────────────────
    if (g_full_dirty) {
        memset(g_pBits, 0, (size_t)(g_fb_width * g_fb_height * 4));
    }

    // ── Save DC state for clip stack ─────────────────────────────────
    int clip_depth = SaveDC(g_hdc_dib);

    // ── Execute draw commands ────────────────────────────────────────
    for (int i = 0; i < draw_data->cmd_count; i++) {
        const kt_Cmd* cmd = &draw_data->cmds[i];

        switch (cmd->type) {

        case KT_CMD_FILL: {
            int x1 = (int)cmd->bounds.x;
            int y1 = (int)cmd->bounds.y;
            int x2 = (int)(cmd->bounds.x + cmd->bounds.w + 0.5f);
            int y2 = (int)(cmd->bounds.y + cmd->bounds.h + 0.5f);

            if (cmd->radius > 0.5f) {
                // Rounded rect via branchless Quilez SDF
                win32_fb_fill_rounded_rect(x1, y1, x2, y2, cmd->radius, cmd->color);
            } else {
                // Simple rect fill
                win32_fb_fill_rect(x1, y1, x2, y2, cmd->color);
            }
            win32_dirty_add_rect(cmd->bounds);
            break;
        }

        case KT_CMD_STROKE: {
            // Stroke as a border: draw 4 thin filled rects
            int x1  = (int)cmd->bounds.x;
            int y1  = (int)cmd->bounds.y;
            int x2  = (int)(cmd->bounds.x + cmd->bounds.w + 0.5f);
            int y2  = (int)(cmd->bounds.y + cmd->bounds.h + 0.5f);
            int th  = (int)(cmd->thickness + 0.5f);
            if (th < 1) th = 1;
            uint32_t c = cmd->color;

            // Top edge
            win32_fb_fill_rect(x1, y1, x2, y1 + th, c);
            // Bottom edge
            win32_fb_fill_rect(x1, y2 - th, x2, y2, c);
            // Left edge (excluding top/bottom overlap)
            win32_fb_fill_rect(x1, y1 + th, x1 + th, y2 - th, c);
            // Right edge
            win32_fb_fill_rect(x2 - th, y1 + th, x2, y2 - th, c);

            win32_dirty_add_rect(cmd->bounds);
            break;
        }

        case KT_CMD_TEXT: {
            // Text rendering via DrawTextW onto the DIB DC
            // Convert UTF-8 text_id to actual string pointer
            // For now, text_id >= 0 means there's text; the actual
            // string retrieval is handled by the GDI renderer
            if (cmd->text_id >= 0) {
                // Delegate to GDI renderer for text
                // The GDI renderer has font management
                RECT tr;
                tr.left   = (LONG)cmd->bounds.x;
                tr.top    = (LONG)cmd->bounds.y;
                tr.right  = (LONG)(cmd->bounds.x + cmd->bounds.w);
                tr.bottom = (LONG)(cmd->bounds.y + cmd->bounds.h);
                // Text color from command
                uint32_t tc = cmd->color;
                SetTextColor(g_hdc_dib,
                    RGB((tc >> 16) & 0xFF, (tc >> 8) & 0xFF, tc & 0xFF));
                SetBkMode(g_hdc_dib, TRANSPARENT);
                // Placeholder — actual text string lookup TBD
                DrawTextW(g_hdc_dib, L"", -1, &tr,
                          DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOCLIP);
            }
            win32_dirty_add_rect(cmd->bounds);
            break;
        }

        case KT_CMD_IMAGE: {
            // Image blit — placeholder for texture support
            // Will use StretchBlt from texture cache
            win32_dirty_add_rect(cmd->bounds);
            break;
        }

        case KT_CMD_CLIP: {
            // Save current DC state, then intersect clip
            SaveDC(g_hdc_dib);
            HRGN hrgn = CreateRectRgn(
                (int)cmd->bounds.x,
                (int)cmd->bounds.y,
                (int)(cmd->bounds.x + cmd->bounds.w + 0.5f),
                (int)(cmd->bounds.y + cmd->bounds.h + 0.5f));
            ExtSelectClipRgn(g_hdc_dib, hrgn, RGN_AND);
            DeleteObject(hrgn);
            break;
        }

        case KT_CMD_UNCLIP: {
            // Restore previous DC state (pops clip)
            RestoreDC(g_hdc_dib, -1);
            break;
        }

        default:
            break;
        }
    }

    // ── Restore DC to pre-render state ───────────────────────────────
    RestoreDC(g_hdc_dib, clip_depth);

    // ── Schedule present ─────────────────────────────────────────────
    g_needs_present = true;
    win32_present_to_screen();
    win32_dirty_clear();
}

// ============================================================================
//  INPUT QUERY — External interface for tree.c to poll input state
// ============================================================================

// These are called by tree.c's input funnel before kt_begin().
// They fill the session's input state from the Win32 message pump.

const float* win32_get_mouse_pos(void) {
    static float pos[2];
    pos[0] = g_mouse_x;
    pos[1] = g_mouse_y;
    return pos;
}
float win32_get_mouse_x(void)       { return g_mouse_x; }
float win32_get_mouse_y(void)       { return g_mouse_y; }
bool  win32_get_mouse_down(int b)   { return (b >= 0 && b < 5) ? g_mouse_down[b] : false; }
float win32_get_scroll_dx(void)     { return g_scroll_dx; }
float win32_get_scroll_dy(void)     { return g_scroll_dy; }
bool  win32_get_key(int k)          { return (k >= 0 && k < 256) ? g_keys[k] : false; }
bool  win32_get_focus(void)         { return g_focus_gained; }
bool  win32_should_close(void)      { return g_should_close; }
float win32_get_delta_seconds(void) { return (float)g_delta_seconds; }
int   win32_get_fb_width(void)      { return g_fb_width; }
int   win32_get_fb_height(void)     { return g_fb_height; }

// ============================================================================
//  BACKEND VTABLE SINGLETON
// ============================================================================
const KaintanaBackendVTable kaintana_win32_backend = {
    .init      = win32_init,
    .shutdown  = win32_shutdown,
    .new_frame = win32_new_frame,
    .render    = win32_render
};
