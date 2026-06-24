// ============================================================================
//  Path B: Kain window + Direct Framebuffer Write
//  ============================================================================
//  Uses the Kain host adapter ("winit") to create the HWND + DIB framebuffer,
//  but BYPASSES the node tree renderer. After ui_render_frame clears the
//  framebuffer to dark background, we write DIRECTLY into host->framebuffer
//  with a colorful gradient + shapes.
//
//  This tests whether:
//   1. The DIB framebuffer is writeable from our test code
//   2. The BitBlt pipeline (with our subclassed WM_PAINT fix) can display
//      arbitrary pixel data
//   3. Direct framebuffer access works end-to-end
//  ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ui_system_internal.h"
#include "ui_host_adapter.h"
#include "../../include/ui_renderer.h"
#include "../../include/ui_layout.h"

// ── KainWin32UiHost (replicated from ui_host_adapter.c) ─────────────────
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
} KainWin32UiHost;

// ── Stubs ──────────────────────────────────────────────────────────────
char* string_new(char* src);
double kain_clampd(double value, double min_value, double max_value);

// ── Window subclassing (fix WM_PAINT: BitBlt from hdc_buffer directly) ──
static WNDPROC g_original_wndproc = NULL;

static LRESULT CALLBACK fixed_wm_paint_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (hdc) {
            KainWin32UiHost* host = (KainWin32UiHost*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
            if (host && host->hdc_buffer) {
                // 🎯 FIXED: BitBlt FROM host->hdc_buffer directly
                BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
                       ps.rcPaint.right - ps.rcPaint.left,
                       ps.rcPaint.bottom - ps.rcPaint.top,
                       host->hdc_buffer, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    return CallWindowProcA(g_original_wndproc, hwnd, msg, wp, lp);
}

// ── Pixel helpers ───────────────────────────────────────────────────────
static uint32_t rgba_pixel(int r, int g, int b, int a) {
    return ((uint32_t)(a & 0xFF) << 24) |
           ((uint32_t)(b & 0xFF) << 16) |
           ((uint32_t)(g & 0xFF) <<  8) |
           ((uint32_t)(r & 0xFF));
}

// ── Pixel helpers for drawing ────────────────────────────────────────
static void fill_rect(uint32_t* fb, int stride, int x, int y, int w, int h, uint32_t color) {
    for (int r = y; r < y + h; r++) {
        for (int c = x; c < x + w; c++) {
            fb[r * stride + c] = color;
        }
    }
}

// ── Paint a real-looking application UI directly into framebuffer ──
static void paint_ui(uint32_t* fb, int w, int h, int stride, HDC gdi_dc) {
    if (!fb || w <= 0 || h <= 0) return;
    int row, col;

    // ── Color palette ───────────────────────────────────────────────
    uint32_t BG         = 0xFF0F172A;  // deep navy
    uint32_t SURFACE    = 0xFF1E293B;  // blue-gray surface
    uint32_t SURFACE2   = 0xFF252540;  // card surface
    uint32_t HEADER     = 0xFF1A1A2E;  // header bar
    uint32_t SIDEBAR    = 0xFF16162A;  // sidebar
    uint32_t BORDER     = 0xFF3A3A5C;  // subtle border
    uint32_t ACCENT     = 0xFF21D4A1;  // green accent
    uint32_t ACCENT2    = 0xFF4A90D9;  // blue accent
    uint32_t ACCENT3    = 0xFFE8914A;  // orange accent
    uint32_t ACCENT4    = 0xFFE84A5F;  // red accent
    uint32_t TEXT_DIM   = 0xFF8888A0;  // dim text

    int header_h = 56;
    int status_h = 28;
    int sidebar_w = 200;
    int content_x = sidebar_w;
    int content_w = w - sidebar_w;
    int content_h = h - header_h - status_h;

    // ── 1. Clear everything to deep navy ────────────────────────────
    for (row = 0; row < h; row++)
        for (col = 0; col < w; col++)
            fb[row * stride + col] = BG;

    // ── 2. Header bar ───────────────────────────────────────────────
    fill_rect(fb, stride, 0, 0, w, header_h, HEADER);
    fill_rect(fb, stride, 0, header_h - 2, w, 2, ACCENT);

    // Green status dot in header
    for (int r = 18; r < 18 + 20; r++)
        for (int c = 16; c < 16 + 20; c++)
            fb[r * stride + c] = ACCENT;

    // ── 3. Sidebar ──────────────────────────────────────────────────
    fill_rect(fb, stride, 0, header_h, sidebar_w, content_h, SIDEBAR);
    fill_rect(fb, stride, sidebar_w - 1, header_h, 1, content_h, BORDER);

    // Sidebar accent line
    fill_rect(fb, stride, 16, header_h + 44, 36, 2, ACCENT);

    // Sidebar items
    const char* menu_items[] = {"Dashboard", "Analytics", "Explorer", "Settings", "Help"};
    uint32_t menu_colors[] = {ACCENT, ACCENT2, ACCENT3, TEXT_DIM, TEXT_DIM};
    int item_y = header_h + 58;
    for (int i = 0; i < 5; i++) {
        // Item background (highlight first)
        uint32_t bg = (i == 0) ? 0xFF2A2A4E : SIDEBAR;
        fill_rect(fb, stride, 8, item_y, sidebar_w - 16, 36, bg);
        // Indicator dot
        fill_rect(fb, stride, 16, item_y + 12, 8, 8, menu_colors[i]);
        // Text
        if (gdi_dc) {
            SetTextColor(gdi_dc, (i == 0) ? RGB(232, 232, 240) : RGB(136, 136, 160));
            SetBkMode(gdi_dc, TRANSPARENT);
            TextOutA(gdi_dc, 32, item_y + 10, menu_items[i], (int)strlen(menu_items[i]));
        }
        item_y += 44;
    }

    // ── 4. Status cards row ─────────────────────────────────────────
    int card_y = header_h + 12;
    int card_w = (content_w - 40) / 4;
    int card_h = 90;
    uint32_t stripe_colors[] = {ACCENT, ACCENT2, ACCENT3, ACCENT4};
    const char* card_titles[] = {"Sessions", "Nodes", "Throughput", "Latency"};
    const char* card_vals[] = {"16", "4,096", "94%", "12ms"};

    for (int i = 0; i < 4; i++) {
        int cx = content_x + 8 + i * (card_w + 8);
        fill_rect(fb, stride, cx, card_y, card_w, card_h, SURFACE2);
        // Accent stripe
        fill_rect(fb, stride, cx, card_y, card_w, 3, stripe_colors[i]);
        // Inner value box
        fill_rect(fb, stride, cx + 12, card_y + 16, card_w - 24, 28, SURFACE);
        // Value text
        if (gdi_dc) {
            SetTextColor(gdi_dc, RGB(232, 232, 240));
            SetBkMode(gdi_dc, TRANSPARENT);
            SelectObject(gdi_dc, GetStockObject(SYSTEM_FONT));
            TextOutA(gdi_dc, cx + 16, card_y + 20, card_vals[i], (int)strlen(card_vals[i]));
            SetTextColor(gdi_dc, RGB(136, 136, 160));
            TextOutA(gdi_dc, cx + 16, card_y + 54, card_titles[i], (int)strlen(card_titles[i]));
        }
    }

    // ── 5. Section label + divider ──────────────────────────────────
    int section_y = card_y + card_h + 16;
    fill_rect(fb, stride, content_x + 8, section_y + 28, content_w - 16, 1, BORDER);
    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(136, 136, 160));
        SelectObject(gdi_dc, GetStockObject(SYSTEM_FONT));
        TextOutA(gdi_dc, content_x + 12, section_y, "SYSTEM ACTIVITY", 15);
    }

    // ── 6. Graph area ───────────────────────────────────────────────
    int graph_y = section_y + 36;
    int graph_h = 160;
    int graph_w = content_w - 24;
    fill_rect(fb, stride, content_x + 12, graph_y, graph_w, graph_h, SURFACE2);
    fill_rect(fb, stride, content_x + 12, graph_y, graph_w, 1, BORDER);

    // Colored bars (simulated chart)
    int bar_count = 8;
    int bar_w_val = (graph_w - 24 - (bar_count - 1) * 4) / bar_count;
    if (bar_w_val < 4) bar_w_val = 4;
    for (int i = 0; i < bar_count; i++) {
        int bh = 20 + (i * 17 + 7) % (graph_h - 40);
        int bx = content_x + 12 + 12 + i * (bar_w_val + 4);
        int by = graph_y + graph_h - 8 - bh;
        uint32_t bar_color = stripe_colors[i % 4];
        fill_rect(fb, stride, bx, by, bar_w_val, bh, bar_color);
    }

    // ── 7. Info bar ─────────────────────────────────────────────────
    int info_y = graph_y + graph_h + 12;
    fill_rect(fb, stride, content_x + 12, info_y, graph_w, 36, SURFACE2);
    fill_rect(fb, stride, content_x + 12, info_y, graph_w, 1, BORDER);
    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(136, 136, 160));
        TextOutA(gdi_dc, content_x + 20, info_y + 10, "1280x720  |  Kain Native UI  |  Direct GDI Backend  |  Z3-Verified", 68);
    }

    // ── 8. Status bar ───────────────────────────────────────────────
    fill_rect(fb, stride, 0, h - status_h, w, status_h, HEADER);
    // Green dot
    fill_rect(fb, stride, 12, h - 22, 12, 12, ACCENT);
    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(136, 136, 160));
        TextOutA(gdi_dc, 30, h - 22, "Running  |  Kain Native UI  |  GDI Backend  |  Path B - Direct FB", 68);
    }
}

// ── Verify framebuffer content ──────────────────────────────────────────
static void verify_pixels(KainWin32UiHost* host, const char* label) {
    if (!host || !host->framebuffer) return;
    uint32_t* fb = (uint32_t*)host->framebuffer;
    int total = host->width * host->height;
    int non_dark = 0;
    for (int i = 0; i < total && i < 50000; i++) {
        if (fb[i] != 0xFF1A1A24) non_dark++;
    }
    printf("  [%s] Non-dark: %d | fb[0]=0x%08X\n", label, non_dark, fb[0]);
}

// ── Main ────────────────────────────────────────────────────────────────
int main(void) {
    int64_t win_w = 1280, win_h = 720;

    printf("=== Path B: Kain Window + Direct Framebuffer Write ===\n\n");

    // ── Init Kain session ─────────────────────────────────────────
    abi_ui_reset();
    int64_t session = abi_ui_session_create("PathB", win_w, win_h);
    if (session <= 0) { fprintf(stderr, "FAIL: session_create\n"); return 1; }
    printf("Session %lld created.\n", (long long)session);

    abi_ui_window_open(session, "Path B: Direct Framebuffer Write", win_w, win_h);
    if (abi_ui_host_attach(session, "winit") != 0) {
        fprintf(stderr, "FAIL: host_attach\n"); return 1;
    }
    printf("Backend: %s\n", abi_ui_host_backend(session));

    // ── Get session + host pointers ─────────────────────────────────
    KainNativeUiSession* ks = abi_ui_find_session(session);
    if (!ks || !ks->host_state) {
        fprintf(stderr, "FAIL: no host_state\n"); return 1;
    }
    KainWin32UiHost* host = (KainWin32UiHost*)ks->host_state;
    printf("Host: hwnd=%p fb=%p stride=%d hdc=%p\n",
           (void*)host->hwnd, (void*)host->framebuffer,
           host->fb_stride, (void*)host->hdc_buffer);

    if (!host->framebuffer) {
        fprintf(stderr, "FAIL: framebuffer is NULL!\n"); return 1;
    }
    printf("Framebuffer is %d x %d = %d bytes\n",
           host->width, host->height, host->fb_stride * host->height);

    // ── Subclass window for fixed WM_PAINT ─────────────────────────
    g_original_wndproc = (WNDPROC)SetWindowLongPtrA(host->hwnd, GWLP_WNDPROC,
                                                     (LONG_PTR)fixed_wm_paint_proc);
    printf("Window subclassed.\n");

    // ── Test 1: Kain renders dark background ───────────────────────
    printf("\n--- Test 1: Kain render (dark clear) ---\n");
    ui_layout_resolve(ks);
    ui_render_frame(ks, (uint32_t*)host->framebuffer,
                    host->width, host->height, host->fb_stride / 4);
    verify_pixels(host, "After Kain clear");

    // ── Test 2: Direct framebuffer write ───────────────────────────
    printf("\n--- Test 2: Direct framebuffer paint ---\n");
    paint_ui((uint32_t*)host->framebuffer,
             host->width, host->height, host->fb_stride / 4,
             host->hdc_buffer);
    verify_pixels(host, "After direct paint");

    // ── Present immediately ────────────────────────────────────────
    InvalidateRect(host->hwnd, NULL, FALSE);
    UpdateWindow(host->hwnd);
    printf("\nWindow should display UI content now.\n");

    // ── Frame loop ─────────────────────────────────────────────────
    printf("\nEntering frame loop. Close the window to exit.\n");
    printf("============================================================\n");

    int64_t frame = 0;
    while (host->running && !ks->host_should_close) {
        abi_ui_host_pump(session);
        if (ks->host_should_close) break;

        abi_ui_begin_frame(session, 16.67);
        abi_ui_end_frame(session);

        // Render: clear via Kain then paint our content directly
        ui_render_frame(ks, (uint32_t*)host->framebuffer,
                        host->width, host->height, host->fb_stride / 4);

        // 🎯 DIRECT paint into Kain's framebuffer
        paint_ui((uint32_t*)host->framebuffer,
                 host->width, host->height, host->fb_stride / 4,
                 host->hdc_buffer);

        InvalidateRect(host->hwnd, NULL, FALSE);

        frame++;
        if (frame % 60 == 0) {
            uint32_t* fb = (uint32_t*)host->framebuffer;
            printf("Frame %lld | fb[0]=0x%08X | fb[100]=0x%08X | center=0x%08X\n",
                   (long long)frame, fb[0], fb[100],
                   fb[host->width * (host->height / 2) + (host->width / 2)]);
        }

        Sleep(16);
    }

    printf("\nShutdown after %lld frames.\n", (long long)frame);
    abi_ui_session_destroy(session);
    printf("Done.\n");
    return 0;
}
