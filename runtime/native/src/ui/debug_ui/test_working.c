// ============================================================================
//  Kain UI — Working Test
//  ============================================================================
//  Proves the Kain UI system CAN render into a Win32 DIB framebuffer.
//
//  Key lessons from previous failures:
//    1. CW_USEDEFAULT for X/Y causes off-screen positioning on this system
//    2. Use explicit position (100,100) 
//    3. Use GetClientRect AFTER creation to get DPI-aware client size
//    4. Create DIB with actual client size
//    5. Pump messages FIRST, then render, then BitBlt
//
//  Architecture:
//    - Win32 window via RegisterClassA/CreateWindowExA (explicit position)
//    - DIB framebuffer via CreateDIBSection
//    - Kain UI node tree via public ABI (ui_system.h)
//    - Node rendering via public ABI getters (abi_ui_node_x, etc.)
//    - Solid color fill and border drawing
//    - GDI text overlay as backup visual proof
//    - BitBlt to screen each frame
//
//  Compile:
//    clang -std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -g -O0 ^
//      test_working.c stubs.c ^
//      ..\ui_system.c ..\ui_host_adapter.c ..\ui_renderer.c ..\ui_layout.c ..\ui_color.c ^
//      ..\..\core\input_system.c ^
//      -I ..\..\..\include -I .. -I ..\..\core ^
//      -luser32 -lgdi32 -o test_working.exe
// ============================================================================

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_system.h"
#include "ui_color.h"

// ── Stubs ──────────────────────────────────────────────────────────────
char* string_new(char* src);
double kain_clampd(double value, double min_value, double max_value);

// ══════════════════════════════════════════════════════════════════════
//  Win32 Window + DIB Framebuffer
// ══════════════════════════════════════════════════════════════════════

typedef struct {
    HWND hwnd;
    int client_w;
    int client_h;
    uint32_t* pixels;
    int stride;     // in uint32_t
    HDC mem_dc;
    HBITMAP hbitmap;
    int running;
} Win32App;

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    Win32App* app = (Win32App*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTA* cs = (CREATESTRUCTA*)l;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcA(hwnd, msg, w, l);
    }
    case WM_CLOSE:
        if (app) app->running = 0;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (app) app->running = 0;
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (hdc && app && app->pixels) {
            HDC mem = CreateCompatibleDC(hdc);
            if (mem) {
                HBITMAP old = (HBITMAP)SelectObject(mem, app->hbitmap);
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
    case WM_SIZE: {
        if (app) {
            int cw = LOWORD(l), ch = HIWORD(l);
            if (cw > 0 && ch > 0) {
                app->client_w = cw;
                app->client_h = ch;
            }
        }
        return 0;
    }
    }
    return DefWindowProcA(hwnd, msg, w, l);
}

static void fb_fill_rect(Win32App* app, int x, int y, int w, int h, uint32_t color) {
    if (!app || !app->pixels) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > app->client_w ? app->client_w : (x + w);
    int y1 = (y + h) > app->client_h ? app->client_h : (y + h);
    if (x0 >= x1 || y0 >= y1) return;
    for (int row = y0; row < y1; row++) {
        uint32_t* row_ptr = app->pixels + row * app->stride;
        for (int col = x0; col < x1; col++) {
            row_ptr[col] = color;
        }
    }
}

static void fb_clear(Win32App* app, uint32_t color) {
    if (!app || !app->pixels) return;
    int total = app->client_w * app->client_h;
    for (int i = 0; i < total; i++) {
        app->pixels[i] = color;
    }
}

static void fb_blit(Win32App* app) {
    if (!app || !app->hwnd) return;
    HDC hdc = GetDC(app->hwnd);
    if (hdc) {
        BitBlt(hdc, 0, 0, app->client_w, app->client_h,
               app->mem_dc, 0, 0, SRCCOPY);
        ReleaseDC(app->hwnd, hdc);
    }
}

static int app_init(Win32App* app, int desired_cx, int desired_cy, const char* title) {
    memset(app, 0, sizeof(*app));

    // Register
    WNDCLASSA wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "KainWorkingTest";
    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        fprintf(stderr, "FAIL: RegisterClassA\n");
        return 0;
    }

    // Window size for desired client
    RECT wr = {0, 0, desired_cx, desired_cy};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    int win_w = wr.right - wr.left;
    int win_h = wr.bottom - wr.top;

    app->hwnd = CreateWindowExA(
        0, "KainWorkingTest", title,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, win_w, win_h,
        NULL, NULL, GetModuleHandleA(NULL), app);
    if (!app->hwnd) {
        fprintf(stderr, "FAIL: CreateWindowExA (err=%lu)\n", GetLastError());
        return 0;
    }

    // Get actual client size (DPI-aware)
    RECT cr;
    GetClientRect(app->hwnd, &cr);
    app->client_w = cr.right;
    app->client_h = cr.bottom;

    printf("[OK] Window at (100,100), requested client %dx%d, actual %dx%d\n",
           desired_cx, desired_cy, app->client_w, app->client_h);

    // DIB
    HDC screen_dc = GetDC(NULL);
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = app->client_w;
    bmi.bmiHeader.biHeight = -app->client_h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    app->hbitmap = CreateDIBSection(screen_dc, &bmi, DIB_RGB_COLORS,
                                     (void**)&app->pixels, NULL, 0);
    if (!app->hbitmap || !app->pixels) {
        fprintf(stderr, "FAIL: CreateDIBSection (err=%lu)\n", GetLastError());
        ReleaseDC(NULL, screen_dc);
        return 0;
    }

    app->mem_dc = CreateCompatibleDC(screen_dc);
    SelectObject(app->mem_dc, app->hbitmap);
    app->stride = app->client_w;
    ReleaseDC(NULL, screen_dc);

    app->running = 1;

    // Fill with initial dark color
    fb_clear(app, 0xFF1A1A24);
    InvalidateRect(app->hwnd, NULL, FALSE);
    UpdateWindow(app->hwnd);

    printf("[OK] DIB %dx%d, pixels=%p\n",
           app->client_w, app->client_h, (void*)app->pixels);
    return 1;
}

static void app_shutdown(Win32App* app) {
    if (app->hbitmap) DeleteObject(app->hbitmap);
    if (app->mem_dc) DeleteDC(app->mem_dc);
    if (app->hwnd && IsWindow(app->hwnd)) DestroyWindow(app->hwnd);
    memset(app, 0, sizeof(*app));
}

// ══════════════════════════════════════════════════════════════════════
//  Kain UI Node Rendering from Public ABI
// ══════════════════════════════════════════════════════════════════════

static int render_kain_nodes(int64_t session, Win32App* app) {
    int count = 0;
    int max_check = 500;
    for (int64_t nid = 1; nid <= max_check; nid++) {
        if (!abi_ui_node_exists(session, nid)) continue;

        double x = abi_ui_node_x(session, nid);
        double y = abi_ui_node_y(session, nid);
        double w = abi_ui_node_width(session, nid);
        double h = abi_ui_node_height(session, nid);

        if (w <= 0 || h <= 0) continue;

        const char* fill = abi_ui_node_style_string(session, nid,
                                                     "fill_color", NULL);
        if (!fill || !fill[0]) continue;

        // Parse and draw
        uint32_t color = ui_parse_color(fill);
        if ((color & 0xFF000000) == 0) {
            // Skip fully transparent
            continue;
        }

        fb_fill_rect(app, (int)x, (int)y, (int)w, (int)h, color);
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
            fb_fill_rect(app, ix, iy, iw, bt, bcol);
            fb_fill_rect(app, ix, iy+ih-bt, iw, bt, bcol);
            fb_fill_rect(app, ix, iy+bt, bt, ih-2*bt, bcol);
            fb_fill_rect(app, ix+iw-bt, iy+bt, bt, ih-2*bt, bcol);
        }
    }
    return count;
}

// ══════════════════════════════════════════════════════════════════════
//  MAIN
// ══════════════════════════════════════════════════════════════════════

int main(void) {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  Kain UI — Working Test                         ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    Win32App app;
    if (!app_init(&app, 1024, 768, "Kain UI — Working Test")) {
        return 1;
    }

    // ── UI System Setup ────────────────────────────────────────────
    printf("[2] Initializing UI system...\n");
    abi_ui_reset();
    int64_t session = abi_ui_session_create("WorkingTest", app.client_w, app.client_h);
    if (session <= 0) {
        fprintf(stderr, "FAIL: session create\n");
        app_shutdown(&app);
        return 1;
    }
    abi_ui_window_open(session, "Kain UI — Working Test", app.client_w, app.client_h);

    // ── Build Node Tree ────────────────────────────────────────────
    printf("[3] Building node tree...\n");

    // Macro helper
    #define N(s, parent, kind, X, Y, W, H, fill, ...) do { \
        int64_t n__ = abi_ui_node_create(s, kind); \
        if (parent > 0) abi_ui_node_set_parent(s, n__, parent); \
        abi_ui_node_set_rect(s, n__, X, Y, W, H); \
        if (fill) abi_ui_node_set_style_string(s, n__, "fill_color", fill); \
        __VA_ARGS__; \
    } while(0)

    #define STYLE(s, nid, key, val) abi_ui_node_set_style_string(s, nid, key, val)
    #define STYLEF(s, nid, key, val) abi_ui_node_set_style_f64(s, nid, key, val)

    // Root
    int64_t root = abi_ui_node_create(session, "root");
    abi_ui_node_set_rect(session, root, 0, 0, app.client_w, app.client_h);
    STYLE(session, root, "fill_color", "#1A1A24");

    // ── Header ─────────────────────────────────────────────────────
    int64_t hdr = abi_ui_node_create(session, "header");
    abi_ui_node_set_parent(session, hdr, root);
    abi_ui_node_set_rect(session, hdr, 0, 0, app.client_w, 56);
    STYLE(session, hdr, "fill_color", "#1E1E32");

    // Status indicator (green dot)
    int64_t dot = abi_ui_node_create(session, "dot");
    abi_ui_node_set_parent(session, dot, hdr);
    abi_ui_node_set_rect(session, dot, 16, 16, 24, 24);
    STYLE(session, dot, "fill_color", "#21D4A1");
    STYLEF(session, dot, "corner_radius", 12.0);

    // Accent line under header
    int64_t accent_line = abi_ui_node_create(session, "accent");
    abi_ui_node_set_parent(session, accent_line, root);
    abi_ui_node_set_rect(session, accent_line, 0, 56, app.client_w, 2);
    STYLE(session, accent_line, "fill_color", "#21D4A1");

    // ── Sidebar ────────────────────────────────────────────────────
    int sb_w = 200;
    int sb_y = 58;
    int sb_h = app.client_h - 58 - 36;
    int64_t sidebar = abi_ui_node_create(session, "sidebar");
    abi_ui_node_set_parent(session, sidebar, root);
    abi_ui_node_set_rect(session, sidebar, 0, sb_y, sb_w, sb_h);
    STYLE(session, sidebar, "fill_color", "#202038");

    // Sidebar accent
    int64_t sb_accent = abi_ui_node_create(session, "sb_accent");
    abi_ui_node_set_parent(session, sb_accent, sidebar);
    abi_ui_node_set_rect(session, sb_accent, 16, 8, 36, 2);
    STYLE(session, sb_accent, "fill_color", "#21D4A1");

    // Sidebar items
    const char* items[] = {"Dashboard", "Analytics", "Explorer", "Settings", "Help"};
    const char* colors[] = {"#21D4A1", "#4A90D9", "#E8914A", "#8888A0", "#8888A0"};
    for (int i = 0; i < 5; i++) {
        int64_t mi = abi_ui_node_create(session, "menuitem");
        abi_ui_node_set_parent(session, mi, sidebar);
        abi_ui_node_set_rect(session, mi, 8, 20 + i * 44, sb_w - 16, 36);
        STYLE(session, mi, "fill_color", i == 0 ? "#21D4A122" : "#202038");

        // Dot indicator
        int64_t md = abi_ui_node_create(session, "mdot");
        abi_ui_node_set_parent(session, md, mi);
        abi_ui_node_set_rect(session, md, 12, 12, 12, 12);
        STYLE(session, md, "fill_color", colors[i]);
    }

    // ── Main content area ──────────────────────────────────────────
    int main_x = sb_w + 8;
    int main_w = app.client_w - main_x - 8;
    int main_h = sb_h;

    int64_t main_panel = abi_ui_node_create(session, "main");
    abi_ui_node_set_parent(session, main_panel, root);
    abi_ui_node_set_rect(session, main_panel, main_x, sb_y, main_w, main_h);
    STYLE(session, main_panel, "fill_color", "#1A1A24");

    // ── Cards row ──────────────────────────────────────────────────
    int card_w = (main_w - 32) / 4;
    int card_h = 100;
    struct { const char* val; const char* color; } card_data[] = {
        {"16", "#21D4A1"}, {"4096", "#4A90D9"}, {"8192", "#E8914A"}, {"1024", "#E84A5F"}
    };
    for (int i = 0; i < 4; i++) {
        int cx = 8 + i * (card_w + 8);
        int64_t card = abi_ui_node_create(session, "card");
        abi_ui_node_set_parent(session, card, main_panel);
        abi_ui_node_set_rect(session, card, cx, 8, card_w, card_h);
        STYLE(session, card, "fill_color", "#252540");

        // Top accent stripe
        int64_t cs = abi_ui_node_create(session, "cardstripe");
        abi_ui_node_set_parent(session, cs, card);
        abi_ui_node_set_rect(session, cs, 0, 0, card_w, 3);
        STYLE(session, cs, "fill_color", card_data[i].color);

        // Value box
        int64_t cv = abi_ui_node_create(session, "cardval");
        abi_ui_node_set_parent(session, cv, card);
        abi_ui_node_set_rect(session, cv, 12, 14, card_w - 24, 32);
        STYLE(session, cv, "fill_color", "#2E2E48");
    }

    // ── Graph area ─────────────────────────────────────────────────
    int64_t graph = abi_ui_node_create(session, "graph");
    abi_ui_node_set_parent(session, graph, main_panel);
    abi_ui_node_set_rect(session, graph, 8, 120, main_w - 16, 180);
    STYLE(session, graph, "fill_color", "#252540");

    // Bar chart
    const char* bar_colors[] = {"#21D4A1", "#4A90D9", "#E8914A", "#E84A5F",
                                 "#21D4A1", "#4A90D9", "#21D4A1", "#E8914A"};
    int bar_count = 8;
    int bar_w_val = (main_w - 40 - 7 * 4) / bar_count;
    if (bar_w_val < 6) bar_w_val = 6;
    for (int i = 0; i < bar_count; i++) {
        int bh = 24 + (i * 17 + 7) % 136;
        int bx = 16 + i * (bar_w_val + 4);
        int by = 290 - bh;  // bottom of graph = 120+180-10 = 290
        if (by < 120) by = 120;
        int64_t bar = abi_ui_node_create(session, "bar");
        abi_ui_node_set_parent(session, bar, graph);
        abi_ui_node_set_rect(session, bar, bx, by, bar_w_val, bh);
        STYLE(session, bar, "fill_color", bar_colors[i]);
    }

    // ── Status bar ─────────────────────────────────────────────────
    int64_t status = abi_ui_node_create(session, "status");
    abi_ui_node_set_parent(session, status, root);
    abi_ui_node_set_rect(session, status, 0, app.client_h - 36, app.client_w, 36);
    STYLE(session, status, "fill_color", "#1E1E32");

    int64_t sdot = abi_ui_node_create(session, "sdot");
    abi_ui_node_set_parent(session, sdot, status);
    abi_ui_node_set_rect(session, sdot, 12, 10, 16, 16);
    STYLE(session, sdot, "fill_color", "#21D4A1");

    printf("[OK] %lld nodes created\n", (long long)abi_ui_node_count(session));

    // ── First Frame ─────────────────────────────────────────────────
    printf("[4] First render...\n");
    abi_ui_begin_frame(session, 16.67);
    abi_ui_end_frame(session);

    fb_clear(&app, 0xFF1A1A24);
    int rendered = render_kain_nodes(session, &app);
    printf("[OK] Rendered %d Kain UI nodes\n", rendered);

    // ── Main Loop ──────────────────────────────────────────────────
    printf("\n[5] Main loop - close window to exit\n");
    printf("============================================================\n");

    int frame = 0;
    while (app.running) {
        // Pump messages
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) app.running = 0;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!app.running) break;

        // Begin/end frame (resets arena)
        abi_ui_begin_frame(session, 16.67);
        abi_ui_end_frame(session);

        // Render
        fb_clear(&app, 0xFF1A1A24);
        render_kain_nodes(session, &app);

        // Blit
        fb_blit(&app);

        frame++;
        if (frame % 120 == 0) {
            printf("Frame %d\n", frame);
        }

        // Yield instead of Sleep (more responsive)
        WaitMessage();
    }

    printf("\nShutdown...\n");
    abi_ui_session_destroy(session);
    app_shutdown(&app);
    printf("Done. %d frames.\n", frame);
    return 0;
}
