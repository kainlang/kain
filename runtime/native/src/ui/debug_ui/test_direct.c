// ============================================================================
//  Kain UI — Direct Win32 Test
//  ============================================================================
//  Goal: Get VISIBLE content on screen using the Kain UI system.
//
//  Strategy:
//    1. Create bare Win32 window with DIB framebuffer (micro-ui style)
//    2. BUILD UI NODE TREE via public ABI (ui_system.h)
//    3. RENDER NODES manually from framebuffer via public ABI getters
//    4. Blit framebuffer to window via BitBlt
//    5. Fallback: if no nodes with fill_color found, draw colored test pattern
//
//  This uses ZERO internal structs from ui_system_internal.h — only public ABI.
//  The node positions ARE the explicit rects set by abi_ui_node_set_rect.
//  The layout engine (ui_layout_resolve) may OVERRIDE these positions, but
//  we read positions AFTER layout via public ABI getters.
//
//  Compile:
//    clang -std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -g -O0 ^
//      test_direct.c stubs.c ^
//      ..\ui_system.c ..\ui_host_adapter.c ..\ui_renderer.c ..\ui_layout.c ..\ui_color.c ^
//      ..\..\core\input_system.c ^
//      -I ..\..\..\include -I .. -I ..\..\core ^
//      -luser32 -lgdi32 -o test_direct.exe
// ============================================================================

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── Kain UI public ABI only ────────────────────────────────────────────
#include "ui_system.h"
#include "ui_color.h"       // ui_parse_color, ui_color_blend

// ── Stubs ──────────────────────────────────────────────────────────────
char* string_new(char* src);
double kain_clampd(double value, double min_value, double max_value);

// ── Forward declare for color parsing ──────────────────────────────────
// (declared in ui_color.h already)

// ══════════════════════════════════════════════════════════════════════
//  Win32 Window + DIB Framebuffer (entirely self-contained)
// ══════════════════════════════════════════════════════════════════════

typedef struct {
    HWND hwnd;
    int width;
    int height;
    uint32_t* pixels;       // DIB pixel buffer (0xAARRGGBB)
    int stride;             // stride in uint32_t elements
    HDC mem_dc;             // memory DC with bitmap selected
    HBITMAP hbitmap;        // DIB section handle
    int running;
} Framebuffer;

// Fill a rectangle of solid color (direct write, no blending)
static void fb_fill_rect(Framebuffer* fb, int x, int y, int w, int h,
                          uint32_t color) {
    if (!fb || !fb->pixels) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > fb->width ? fb->width : (x + w);
    int y1 = (y + h) > fb->height ? fb->height : (y + h);
    if (x0 >= x1 || y0 >= y1) return;
    for (int row = y0; row < y1; row++) {
        uint32_t* row_ptr = fb->pixels + row * fb->stride;
        for (int col = x0; col < x1; col++) {
            row_ptr[col] = color;
        }
    }
}

// Clear entire framebuffer to a single color
static void fb_clear(Framebuffer* fb, uint32_t color) {
    if (!fb || !fb->pixels) return;
    int total = fb->width * fb->height;
    for (int i = 0; i < total; i++) {
        fb->pixels[i] = color;
    }
}

// Blit framebuffer to the window
static void fb_blit(Framebuffer* fb) {
    if (!fb || !fb->hwnd) return;
    HDC hdc = GetDC(fb->hwnd);
    if (hdc) {
        BitBlt(hdc, 0, 0, fb->width, fb->height,
               fb->mem_dc, 0, 0, SRCCOPY);
        ReleaseDC(fb->hwnd, hdc);
    }
}

// Window procedure
static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    Framebuffer* fb = (Framebuffer*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTA* cs = (CREATESTRUCTA*)l;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcA(hwnd, msg, w, l);
    }
    case WM_CLOSE:
        if (fb) fb->running = 0;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (fb) fb->running = 0;
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        return 1;  // We paint everything
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (hdc && fb && fb->pixels) {
            HDC mem = CreateCompatibleDC(hdc);
            if (mem) {
                HBITMAP old = (HBITMAP)SelectObject(mem, fb->hbitmap);
                BitBlt(hdc,
                       ps.rcPaint.left, ps.rcPaint.top,
                       ps.rcPaint.right - ps.rcPaint.left,
                       ps.rcPaint.bottom - ps.rcPaint.top,
                       mem,
                       ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
                SelectObject(mem, old);
                DeleteDC(mem);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SIZE:
        if (fb) {
            int new_w = LOWORD(l);
            int new_h = HIWORD(l);
            if (new_w > 0 && new_h > 0) {
                fb->width = new_w;
                fb->height = new_h;
            }
        }
        return 0;
    }
    return DefWindowProcA(hwnd, msg, w, l);
}

// Create window + DIB framebuffer, fill with initial color
static int fb_create(Framebuffer* fb, int width, int height,
                      const char* title, uint32_t init_color) {
    memset(fb, 0, sizeof(*fb));
    fb->width = width;
    fb->height = height;
    fb->running = 1;

    // Register window class
    WNDCLASSA wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "KainDirectTest";
    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        fprintf(stderr, "FAIL: RegisterClassA\n");
        return 0;
    }

    // Compute window size to get desired CLIENT area
    RECT cr = {0, 0, width, height};
    AdjustWindowRect(&cr, WS_OVERLAPPEDWINDOW, FALSE);
    int win_w = cr.right - cr.left;
    int win_h = cr.bottom - cr.top;

    // Create window with explicit positioning
    fb->hwnd = CreateWindowExA(
        0, "KainDirectTest", title,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, win_w, win_h,
        NULL, NULL, GetModuleHandleA(NULL), fb);
    if (!fb->hwnd) {
        fprintf(stderr, "FAIL: CreateWindowExA (err=%lu)\n", GetLastError());
        return 0;
    }

    // Create DIB section
    HDC screen_dc = GetDC(NULL);
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;   // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    fb->hbitmap = CreateDIBSection(screen_dc, &bmi, DIB_RGB_COLORS,
                                    (void**)&fb->pixels, NULL, 0);
    if (!fb->hbitmap || !fb->pixels) {
        fprintf(stderr, "FAIL: CreateDIBSection (err=%lu)\n", GetLastError());
        ReleaseDC(NULL, screen_dc);
        return 0;
    }

    fb->mem_dc = CreateCompatibleDC(screen_dc);
    if (!fb->mem_dc) {
        fprintf(stderr, "FAIL: CreateCompatibleDC\n");
        ReleaseDC(NULL, screen_dc);
        return 0;
    }
    SelectObject(fb->mem_dc, fb->hbitmap);
    fb->stride = width;  // 32-bit pixels, stride in u32 = width
    ReleaseDC(NULL, screen_dc);

    // Fill with initial visible color
    fb_clear(fb, init_color);

    printf("[OK] Window %dx%d, fb=%p\n", width, height, (void*)fb->pixels);
    return 1;
}

static void fb_destroy(Framebuffer* fb) {
    if (!fb) return;
    if (fb->hbitmap) DeleteObject(fb->hbitmap);
    if (fb->mem_dc) DeleteDC(fb->mem_dc);
    if (fb->hwnd && IsWindow(fb->hwnd)) DestroyWindow(fb->hwnd);
    memset(fb, 0, sizeof(*fb));
}

// ══════════════════════════════════════════════════════════════════════
//  UI Rendering from Public ABI
// ══════════════════════════════════════════════════════════════════════

// Render all nodes into framebuffer using public ABI getters.
// Returns number of nodes with fill_color rendered.
static int render_nodes_from_abi(int64_t session, Framebuffer* fb) {
    int count = 0;
    int max_check = 500;  // safety limit
    for (int64_t nid = 1; nid <= max_check; nid++) {
        if (!abi_ui_node_exists(session, nid)) continue;

        double x = abi_ui_node_x(session, nid);
        double y = abi_ui_node_y(session, nid);
        double w = abi_ui_node_width(session, nid);
        double h = abi_ui_node_height(session, nid);

        if (w <= 0 || h <= 0) continue;

        const char* fill = abi_ui_node_style_string(session, nid,
                                                     "fill_color", NULL);
        if (!fill) continue;

        uint32_t color = ui_parse_color(fill);
        // Skip fully transparent
        if ((color & 0xFF000000) == 0 && strcmp(fill, "transparent") != 0) {
            // If fill is a valid hex color but alpha is 0... check if name
            // Just parse it as-is
        }

        fb_fill_rect(fb, (int)x, (int)y, (int)w, (int)h, color);
        count++;

        // Border
        const char* border = abi_ui_node_style_string(session, nid,
                                                        "border_color", NULL);
        double bw = abi_ui_node_style_f64(session, nid,
                                           "border_width", 0.0);
        if (border && bw > 0) {
            uint32_t bcol = ui_parse_color(border);
            int bt = (int)bw;
            int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
            fb_fill_rect(fb, ix, iy, iw, bt, bcol);
            fb_fill_rect(fb, ix, iy+ih-bt, iw, bt, bcol);
            fb_fill_rect(fb, ix, iy+bt, bt, ih-2*bt, bcol);
            fb_fill_rect(fb, ix+iw-bt, iy+bt, bt, ih-2*bt, bcol);
        }

        // Debug: first 3 nodes
        if (count <= 3) {
            printf("  Node %lld: (%.0f,%.0f %.0fx%.0f) fill=%s\n",
                   (long long)nid, x, y, w, h, fill);
        }
    }
    return count;
}

// ══════════════════════════════════════════════════════════════════════
//  Test Pattern Fallback
// ══════════════════════════════════════════════════════════════════════

static void draw_test_pattern(Framebuffer* fb) {
    // Color bars
    uint32_t colors[] = {0xFFFF3333, 0xFF33FF33, 0xFF3333FF,
                         0xFFFFFF33, 0xFFFF33FF, 0xFF33FFFF};
    int n = sizeof(colors) / sizeof(colors[0]);
    int bw = fb->width / n;
    for (int i = 0; i < n; i++) {
        fb_fill_rect(fb, i * bw, 50, bw, fb->height - 100, colors[i]);
    }
    // White border
    fb_fill_rect(fb, 0, 0, fb->width, 4, 0xFFFFFFFF);
    fb_fill_rect(fb, 0, fb->height - 4, fb->width, 4, 0xFFFFFFFF);
    fb_fill_rect(fb, 0, 0, 4, fb->height, 0xFFFFFFFF);
    fb_fill_rect(fb, fb->width - 4, 0, 4, fb->height, 0xFFFFFFFF);
    // Text rectangle (crude "KAIN UI" border box)
    int cx = fb->width / 2 - 120;
    int cy = fb->height / 2 - 30;
    fb_fill_rect(fb, cx, cy, 240, 60, 0xFFFFFFFF);
    fb_fill_rect(fb, cx + 4, cy + 4, 232, 52, 0xFF000000);
    printf("[OK] Test pattern drawn (%dx%d)\n", fb->width, fb->height);
}

// ══════════════════════════════════════════════════════════════════════
//  MAIN
// ══════════════════════════════════════════════════════════════════════

int main(void) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  Kain UI — Direct Win32 Test                    ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    int win_w = 1024;
    int win_h = 768;

    // ── Step 1: Create window + framebuffer ────────────────────────
    // Use deep indigo so we KNOW it's our framebuffer being shown
    printf("[1] Creating window + DIB framebuffer...\n");
    Framebuffer fb;
    if (!fb_create(&fb, win_w, win_h,
                   "Kain UI — Direct Test", 0xFF2D1B69)) {
        fprintf(stderr, "FATAL: Window creation failed\n");
        return 1;
    }

    // Force an immediate paint to confirm the window + DIB work
    // (This should show the deep indigo background)
    InvalidateRect(fb.hwnd, NULL, FALSE);
    UpdateWindow(fb.hwnd);
    printf("[OK] Window visible with initial color\n");

    // ── Step 2: Initialize UI system ───────────────────────────────
    printf("[2] Initializing UI system...\n");
    if (abi_ui_reset() != ABI_UI_OK) {
        fprintf(stderr, "FAIL: abi_ui_reset\n");
        fb_destroy(&fb);
        return 1;
    }

    int64_t session = abi_ui_session_create("DirectTest", win_w, win_h);
    if (session <= 0) {
        fprintf(stderr, "FAIL: abi_ui_session_create\n");
        fb_destroy(&fb);
        return 1;
    }
    printf("[OK] Session %lld created\n", (long long)session);

    abi_ui_window_open(session, "Kain UI — Direct Test", win_w, win_h);

    // ── Step 3: Build node tree ────────────────────────────────────
    printf("[3] Building node tree...\n");

    // Helper macros
    #define ADD_NODE(s, parent, k, X, Y, W, H, fill, ...) do { \
        int64_t n = abi_ui_node_create(s, k); \
        if (parent > 0) abi_ui_node_set_parent(s, n, parent); \
        abi_ui_node_set_rect(s, n, X, Y, W, H); \
        if (fill) abi_ui_node_set_style_string(s, n, "fill_color", fill); \
        __VA_ARGS__; \
    } while(0)

    // Root — full window, dark background
    int64_t root = abi_ui_node_create(session, "root");
    abi_ui_node_set_rect(session, root, 0, 0, win_w, win_h);
    abi_ui_node_set_style_string(session, root, "fill_color", "#1A1A24");

    // ── Header bar ─────────────────────────────────────────────────
    int64_t hdr = abi_ui_node_create(session, "header");
    abi_ui_node_set_parent(session, hdr, root);
    abi_ui_node_set_rect(session, hdr, 0, 0, win_w, 56);
    abi_ui_node_set_style_string(session, hdr, "fill_color", "#1E1E32");

    // Accent line under header
    int64_t acct = abi_ui_node_create(session, "accent");
    abi_ui_node_set_parent(session, acct, root);
    abi_ui_node_set_rect(session, acct, 0, 56, win_w, 2);
    abi_ui_node_set_style_string(session, acct, "fill_color", "#21D4A1");

    // ── Sidebar ────────────────────────────────────────────────────
    int64_t sb = abi_ui_node_create(session, "sidebar");
    abi_ui_node_set_parent(session, sb, root);
    abi_ui_node_set_rect(session, sb, 0, 58, 220, win_h - 58 - 36);
    abi_ui_node_set_style_string(session, sb, "fill_color", "#202038");

    // Sidebar accent
    int64_t sba = abi_ui_node_create(session, "sb_accent");
    abi_ui_node_set_parent(session, sba, sb);
    abi_ui_node_set_rect(session, sba, 16, 8, 36, 2);
    abi_ui_node_set_style_string(session, sba, "fill_color", "#21D4A1");

    // Sidebar items
    const char* items[] = {"Dashboard", "Analytics", "Settings", "Help"};
    for (int i = 0; i < 4; i++) {
        int64_t mi = abi_ui_node_create(session, "menu_item");
        abi_ui_node_set_parent(session, mi, sb);
        abi_ui_node_set_rect(session, mi, 8, 20 + i*44, 204, 36);
        abi_ui_node_set_style_string(session, mi, "fill_color",
            i == 0 ? "#21D4A122" : "#202038");
        // Dot indicator
        int64_t dot = abi_ui_node_create(session, "dot");
        abi_ui_node_set_parent(session, dot, mi);
        abi_ui_node_set_rect(session, dot, 12, 12, 12, 12);
        abi_ui_node_set_style_f64(session, dot, "corner_radius", 6.0);
        abi_ui_node_set_style_string(session, dot, "fill_color",
            i == 0 ? "#21D4A1" : i == 1 ? "#4A90D9" : i == 2 ? "#8888A0" : "#8888A0");
    }

    // ── Main content area ──────────────────────────────────────────
    int64_t main_x = 228;
    int64_t main_w = win_w - main_x - 8;
    int64_t main_h = win_h - 58 - 36 - 8;

    int64_t main_panel = abi_ui_node_create(session, "main");
    abi_ui_node_set_parent(session, main_panel, root);
    abi_ui_node_set_rect(session, main_panel, main_x, 58, main_w, main_h);
    abi_ui_node_set_style_string(session, main_panel, "fill_color", "#1A1A24");

    // ── Cards row ──────────────────────────────────────────────────
    int cw = (main_w - 32) / 4;
    struct { const char* val; const char* color; } card_data[] = {
        {"16", "#21D4A1"}, {"4096", "#4A90D9"}, {"8192", "#E8914A"}, {"1024", "#E84A5F"}
    };
    for (int i = 0; i < 4; i++) {
        int cx = 8 + i * (cw + 8);
        int64_t card = abi_ui_node_create(session, "card");
        abi_ui_node_set_parent(session, card, main_panel);
        abi_ui_node_set_rect(session, card, cx, 8, cw, 100);
        abi_ui_node_set_style_string(session, card, "fill_color", "#252540");

        // Top accent stripe
        int64_t strip = abi_ui_node_create(session, "strip");
        abi_ui_node_set_parent(session, strip, card);
        abi_ui_node_set_rect(session, strip, 0, 0, cw, 3);
        abi_ui_node_set_style_string(session, strip, "fill_color", card_data[i].color);

        // Value box
        int64_t val = abi_ui_node_create(session, "value");
        abi_ui_node_set_parent(session, val, card);
        abi_ui_node_set_rect(session, val, 12, 14, cw - 24, 32);
        abi_ui_node_set_style_string(session, val, "fill_color", "#2E2E48");
    }

    // ── Graph area ─────────────────────────────────────────────────
    int64_t graph = abi_ui_node_create(session, "graph");
    abi_ui_node_set_parent(session, graph, main_panel);
    abi_ui_node_set_rect(session, graph, 8, 120, main_w - 16, 180);
    abi_ui_node_set_style_string(session, graph, "fill_color", "#252540");

    // Bar chart
    const char* bar_colors[] = {"#21D4A1", "#4A90D9", "#E8914A", "#E84A5F",
                                 "#21D4A1", "#4A90D9", "#21D4A1", "#E8914A"};
    int bar_count = 8;
    int bar_w_val = (main_w - 40 - 7 * 4) / bar_count;
    if (bar_w_val < 6) bar_w_val = 6;
    for (int i = 0; i < bar_count; i++) {
        int bh = 24 + (i * 17 + 7) % 136;
        int bx = 16 + i * (bar_w_val + 4);
        int by = 180 - 12 - bh;
        int64_t bar = abi_ui_node_create(session, "bar");
        abi_ui_node_set_parent(session, bar, graph);
        abi_ui_node_set_rect(session, bar, bx, by, bar_w_val, bh);
        abi_ui_node_set_style_string(session, bar, "fill_color", bar_colors[i]);
    }

    // ── Status bar ─────────────────────────────────────────────────
    int64_t status = abi_ui_node_create(session, "status");
    abi_ui_node_set_parent(session, status, root);
    abi_ui_node_set_rect(session, status, 0, win_h - 36, win_w, 36);
    abi_ui_node_set_style_string(session, status, "fill_color", "#1E1E32");

    // Status dot
    int64_t sdot = abi_ui_node_create(session, "sdot");
    abi_ui_node_set_parent(session, sdot, status);
    abi_ui_node_set_rect(session, sdot, 12, 10, 16, 16);
    abi_ui_node_set_style_string(session, sdot, "fill_color", "#21D4A1");

    printf("[OK] %lld nodes created\n", (long long)abi_ui_node_count(session));

    // ── Step 4: First render ───────────────────────────────────────
    printf("[4] First render (through public ABI)...\n");

    abi_ui_begin_frame(session, 16.67);
    abi_ui_end_frame(session);

    // Clear to dark and render nodes from public ABI
    fb_clear(&fb, 0xFF1A1A24);
    int rendered = render_nodes_from_abi(session, &fb);
    printf("[OK] %d nodes rendered from public ABI\n", rendered);

    if (rendered == 0) {
        printf("[WARN] No fill_color nodes found via ABI — drawing fallback\n");
        draw_test_pattern(&fb);
    }

    // Blit to screen
    fb_blit(&fb);
    printf("[OK] Framebuffer blitted to screen\n");

    // ── Step 5: Main loop ──────────────────────────────────────────
    printf("\n[5] Entering main loop. Close window to exit.\n");
    printf("============================================================\n");

    int frame = 0;
    while (fb.running) {
        // Pump messages
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                fb.running = 0;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!fb.running) break;

        // Begin/end frame (resets arena + draw commands)
        abi_ui_begin_frame(session, 16.67);
        abi_ui_end_frame(session);

        // Render (only need to re-render if something changed)
        // For static UI, we render each frame for simplicity
        fb_clear(&fb, 0xFF1A1A24);
        render_nodes_from_abi(session, &fb);

        // Blit to screen
        fb_blit(&fb);

        Sleep(16);
        frame++;

        if (frame % 120 == 0) {
            printf("Frame %d\n", frame);
        }
    }

    // ── Cleanup ─────────────────────────────────────────────────────
    printf("\nShutdown...\n");
    abi_ui_session_destroy(session);
    fb_destroy(&fb);
    printf("Done. %d frames.\n", frame);
    return 0;
}
