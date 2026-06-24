// ============================================================================
//  full_demo.c — Full UI Demo: Dashboard with Cards, Charts, Input, Status
//  ============================================================================
//  Demonstrates:
//    - Rich dashboard layout with sidebar, header, cards
//    - Multiple colored status cards with animated values
//    - Simulated bar chart with live updates
//    - Input system integration (keyboard/mouse event logging)
//    - Window subclass for raw Win32 message monitoring
//    - Status bar showing FPS, event counts, backend info
//    - Indicator dots that pulse/change color
//  ============================================================================
//  Build:
//    clang -std=c11 -g -O0 full_demo.c ../TEST/stubs.c ^
//      ../ui_system.c ../ui_host_adapter.c ../ui_renderer.c ../ui_layout.c ../ui_color.c ^
//      ../../core/input_system.c ^
//      -I../../../include -I.. -I../../core ^
//      -luser32 -lgdi32 -lopengl32 -o full_demo.exe
//  ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ui_system.h"
#include "ui_system_internal.h"
#include "ui_host_adapter.h"
#include "../../include/ui_renderer.h"
#include "../../include/ui_layout.h"
#include "../../include/ui_color.h"
#include "../../include/input_system.h"

// ── Stubs ──────────────────────────────────────────────────────────────
char* string_new(char* src);
double kain_clampd(double value, double min_value, double max_value);

// ── KainWin32UiHost ────────────────────────────────────────────────────
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

// ── Color palette ──────────────────────────────────────────────────────
#define C_BG        0xFF0F172A
#define C_SURFACE   0xFF1E293B
#define C_SURFACE2  0xFF252540
#define C_HEADER    0xFF1A1A2E
#define C_SIDEBAR   0xFF16162A
#define C_BORDER    0xFF3A3A5C
#define C_ACCENT    0xFF21D4A1
#define C_ACCENT2   0xFF4A90D9
#define C_ACCENT3   0xFFE8914A
#define C_ACCENT4   0xFFE84A5F
#define C_TEXT      0xFFE8E8F0
#define C_TEXT_DIM  0xFF8888A0
#define C_HIGHLIGHT 0xFF2A2A4E

// ── Application state ──────────────────────────────────────────────────
static double g_sim_values[8] = {16.0, 4096.0, 94.0, 12.0, 256.0, 99.9, 42.0, 3.14};
static double g_bar_heights[8] = {0};
static int64_t g_total_events = 0;
static int64_t g_frame_count = 0;
static double g_fps = 60.0;
static double g_fps_timer = 0.0;
static int64_t g_last_event_count = 0;
static int64_t g_input_session = 0;
static char g_last_event_text[128] = {0};

// ── Pixel helpers ──────────────────────────────────────────────────────
static void fill_rect(uint32_t* fb, int stride, int x, int y, int w, int h, uint32_t color) {
    for (int r = y; r < y + h && r < 2000; r++)
        for (int c = x; c < x + w && c < 2000; c++)
            if (r >= 0 && c >= 0) fb[r * stride + c] = color;
}

static void fill_rounded_rect(uint32_t* fb, int stride, int fb_w, int fb_h,
                              int x, int y, int w, int h, uint32_t color, int radius) {
    if (radius <= 0) { fill_rect(fb, stride, x, y, w, h, color); return; }
    int r2 = radius * radius;
    for (int row = y; row < y + h && row < fb_h; row++) {
        for (int col = x; col < x + w && col < fb_w; col++) {
            if (row < 0 || col < 0) continue;
            int inside = 1;
            if (col < x + radius && row < y + radius) {
                int dx = (x + radius) - col; int dy = (y + radius) - row;
                inside = (dx*dx + dy*dy) <= r2;
            } else if (col >= x + w - radius && row < y + radius) {
                int dx = col - (x + w - radius); int dy = (y + radius) - row;
                inside = (dx*dx + dy*dy) <= r2;
            } else if (col < x + radius && row >= y + h - radius) {
                int dx = (x + radius) - col; int dy = row - (y + h - radius);
                inside = (dx*dx + dy*dy) <= r2;
            } else if (col >= x + w - radius && row >= y + h - radius) {
                int dx = col - (x + w - radius); int dy = row - (y + h - radius);
                inside = (dx*dx + dy*dy) <= r2;
            }
            if (inside) fb[row * stride + col] = color;
        }
    }
}

static void draw_line_h(uint32_t* fb, int stride, int x, int y, int len, uint32_t color) {
    for (int c = x; c < x + len && c >= 0 && c < 4000; c++)
        if (y >= 0 && y < 2000) fb[y * stride + c] = color;
}

static void draw_line_v(uint32_t* fb, int stride, int x, int y, int len, uint32_t color) {
    for (int r = y; r < y + len && r >= 0 && r < 2000; r++)
        if (x >= 0 && x < 4000) fb[r * stride + x] = color;
}

// ── Dashboard paint ────────────────────────────────────────────────────
static void paint_dashboard(uint32_t* fb, int w, int h, int stride, HDC gdi_dc) {
    int row, col;

    // ── 1. Clear to deep navy ──────────────────────────────────────
    for (row = 0; row < h; row++)
        for (col = 0; col < w; col++)
            fb[row * stride + col] = C_BG;

    int header_h = 52;
    int status_h = 26;
    int sidebar_w = 180;

    // ── 2. Header bar ──────────────────────────────────────────────
    fill_rect(fb, stride, 0, 0, w, header_h, C_HEADER);
    fill_rect(fb, stride, 0, header_h - 2, w, 2, C_ACCENT);

    // Pulsing status indicator
    double pulse = 0.5 + 0.5 * sin(g_frame_count * 0.05);
    uint32_t dot_color = (uint32_t)(
        (0xFF << 24) |
        ((int)(0x21 * pulse) << 16) |
        ((int)(0xD4 * pulse) << 8) |
        (int)(0xA1 * pulse)
    );
    fill_rounded_rect(fb, stride, w, h, 16, 16, 20, 20, dot_color, 10);

    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0xE8, 0xE8, 0xF0));
        SetBkMode(gdi_dc, TRANSPARENT);
        SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
        TextOutA(gdi_dc, 44, 10, "Kain Native UI — Full Demo Dashboard", 37);
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        char fps_str[32];
        snprintf(fps_str, sizeof(fps_str), "FPS: %.1f", g_fps);
        TextOutA(gdi_dc, w - 120, 10, fps_str, (int)strlen(fps_str));
        snprintf(fps_str, sizeof(fps_str), "Frame: %lld", (long long)g_frame_count);
        TextOutA(gdi_dc, w - 120, 28, fps_str, (int)strlen(fps_str));
    }

    // ── 3. Sidebar ─────────────────────────────────────────────────
    int content_y = header_h;
    int content_h = h - header_h - status_h;
    fill_rect(fb, stride, 0, content_y, sidebar_w, content_h, C_SIDEBAR);
    fill_rect(fb, stride, sidebar_w - 1, content_y, 1, content_h, C_BORDER);

    // Sidebar accent line
    fill_rect(fb, stride, 14, content_y + 28, 40, 2, C_ACCENT);

    // Sidebar items
    const char* items[] = {"Dashboard", "Analytics", "Explorer", "Settings", "Help"};
    uint32_t item_colors[] = {C_ACCENT, C_ACCENT2, C_ACCENT3, C_TEXT_DIM, C_TEXT_DIM};
    int item_y = content_y + 44;
    if (gdi_dc) {
        for (int i = 0; i < 5; i++) {
            fill_rect(fb, stride, 8, item_y, sidebar_w - 16, 32, i == 0 ? C_HIGHLIGHT : 0);
            fill_rounded_rect(fb, stride, w, h, 14, item_y + 10, 8, 8, item_colors[i], 4);
            SetTextColor(gdi_dc, i == 0 ? RGB(0xE8,0xE8,0xF0) : RGB(0x88,0x88,0xA0));
            TextOutA(gdi_dc, 32, item_y + 8, items[i], (int)strlen(items[i]));
            item_y += 40;
        }
    }

    // ── 4. Status cards row ────────────────────────────────────────
    int content_x = sidebar_w;
    int card_y = content_y + 10;
    int card_w = (w - sidebar_w - 48) / 4;
    int card_h = 85;

    const char* card_titles[] = {"Sessions", "Nodes", "Throughput", "Latency"};
    uint32_t stripe_colors[] = {C_ACCENT, C_ACCENT2, C_ACCENT3, C_ACCENT4};

    // Animate values slightly
    g_sim_values[0] = 15.5 + 1.5 * sin(g_frame_count * 0.02);
    g_sim_values[1] = 4000 + 200 * sin(g_frame_count * 0.015);
    g_sim_values[2] = 92.0 + 4.0 * sin(g_frame_count * 0.025);
    g_sim_values[3] = 10.0 + 4.0 * sin(g_frame_count * 0.03);

    char live_vals[4][16];
    snprintf(live_vals[0], 16, "%.0f", g_sim_values[0]);
    snprintf(live_vals[1], 16, "%.0f", g_sim_values[1]);
    snprintf(live_vals[2], 16, "%.1f%%", g_sim_values[2]);
    snprintf(live_vals[3], 16, "%.0fms", g_sim_values[3]);
    const char* live_vals_p[] = {live_vals[0], live_vals[1], live_vals[2], live_vals[3]};

    if (gdi_dc) {
        for (int i = 0; i < 4; i++) {
            int cx = content_x + 10 + i * (card_w + 8);
            fill_rounded_rect(fb, stride, w, h, cx, card_y, card_w, card_h, C_SURFACE2, 6);
            fill_rect(fb, stride, cx, card_y, card_w, 3, stripe_colors[i]);

            // Value text
            SetTextColor(gdi_dc, RGB(0xE8, 0xE8, 0xF0));
            HFONT val_font = CreateFontA(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                          CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                          DEFAULT_PITCH, "Consolas");
            SelectObject(gdi_dc, val_font);
            TextOutA(gdi_dc, cx + 12, card_y + 12, live_vals_p[i], (int)strlen(live_vals_p[i]));
            DeleteObject(val_font);

            // Title
            SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
            SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
            TextOutA(gdi_dc, cx + 12, card_y + 50, card_titles[i], (int)strlen(card_titles[i]));
        }
    }

    // ── 5. Chart section ───────────────────────────────────────────
    int section_y = card_y + card_h + 14;
    int chart_x = content_x + 10;
    int chart_w = w - sidebar_w - 20;
    int chart_h = 160;

    // Section label
    fill_rect(fb, stride, chart_x, section_y + 24, chart_w, 1, C_BORDER);
    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        TextOutA(gdi_dc, chart_x + 4, section_y + 2, "SYSTEM ACTIVITY — LIVE", 22);
    }

    // Chart area
    int graph_y = section_y + 30;
    int graph_h = chart_h - 34;
    fill_rounded_rect(fb, stride, w, h, chart_x, graph_y, chart_w, graph_h, C_SURFACE2, 6);
    fill_rect(fb, stride, chart_x, graph_y, chart_w, 1, C_BORDER);

    // Grid lines
    for (int i = 1; i <= 3; i++) {
        int gy = graph_y + graph_h * i / 4;
        for (int c = chart_x; c < chart_x + chart_w; c++)
            fb[gy * stride + c] = 0xFF2A2A44;
    }

    // Animated bars
    int bar_count = 8;
    if (g_frame_count % 5 == 0) {
        for (int i = 0; i < bar_count; i++) {
            g_bar_heights[i] = 25.0 + 100.0 * (0.2 + 0.8 * (0.5 + 0.5 * sin(g_frame_count * 0.03 + i * 0.8)));
        }
    }

    int bar_area_w = chart_w - 20;
    int bar_w = (bar_area_w - (bar_count - 1) * 5) / bar_count;
    uint32_t bar_colors[] = {C_ACCENT, C_ACCENT2, C_ACCENT3, C_ACCENT4,
                             C_ACCENT, C_ACCENT2, C_ACCENT3, C_ACCENT4};

    for (int i = 0; i < bar_count; i++) {
        int bh = (int)g_bar_heights[i];
        if (bh > graph_h - 10) bh = graph_h - 10;
        int bx = chart_x + 10 + i * (bar_w + 5);
        int by = graph_y + graph_h - 8 - bh;
        fill_rounded_rect(fb, stride, w, h, bx, by, bar_w, bh, bar_colors[i], 3);
    }

    // ── 6. Info panel ──────────────────────────────────────────────
    int info_y = graph_y + graph_h + 8;
    int info_h = 32;
    fill_rounded_rect(fb, stride, w, h, chart_x, info_y, chart_w, info_h, C_SURFACE2, 6);

    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        char info_str[128];
        snprintf(info_str, sizeof(info_str), "%dx%d  |  Backend: %s  |  Events: %lld  |  %s",
                 w, h, "GDI (winit backend)", (long long)g_total_events, g_last_event_text);
        TextOutA(gdi_dc, chart_x + 10, info_y + 8, info_str, (int)strlen(info_str));
    }

    // ── 7. Interactive button demo ─────────────────────────────────
    int btn_y = info_y + info_h + 10;
    if (gdi_dc) {
        // Draw some "action buttons" that indicate interactivity
        const char* btn_labels[] = {"Deploy", "Cancel", "Refresh"};
        uint32_t btn_colors[] = {C_ACCENT, C_ACCENT4, C_ACCENT2};
        for (int i = 0; i < 3; i++) {
            int bx = chart_x + i * 110;
            fill_rounded_rect(fb, stride, w, h, bx, btn_y, 100, 34, btn_colors[i], 6);
            SetTextColor(gdi_dc, RGB(0xFF, 0xFF, 0xFF));
            SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
            RECT r = { bx, btn_y, bx + 100, btn_y + 34 };
            DrawTextA(gdi_dc, btn_labels[i], -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }

    // ── 8. Input event log area ────────────────────────────────────
    int log_y = btn_y + 44;
    int log_h = h - status_h - log_y - 4;
    if (log_h > 100) log_h = 100;

    fill_rounded_rect(fb, stride, w, h, chart_x, log_y, chart_w, log_h, C_SURFACE2, 6);
    fill_rect(fb, stride, chart_x + 1, log_y + 1, chart_w - 2, log_h - 2, C_SURFACE);

    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        TextOutA(gdi_dc, chart_x + 8, log_y + 4, "Input Events:", 13);

        // Show recent event
        if (g_last_event_text[0]) {
            SetTextColor(gdi_dc, RGB(0x21, 0xD4, 0xA1));
            TextOutA(gdi_dc, chart_x + 8, log_y + 22, g_last_event_text,
                    (int)strlen(g_last_event_text));
        }

        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        char hint[64];
        snprintf(hint, sizeof(hint), "Total: %lld events this session",
                 (long long)g_total_events);
        TextOutA(gdi_dc, chart_x + 8, log_y + log_h - 20, hint, (int)strlen(hint));
    }

    // ── 9. Status bar ──────────────────────────────────────────────
    int sb_y = h - status_h;
    fill_rect(fb, stride, 0, sb_y, w, status_h, C_HEADER);
    fill_rect(fb, stride, 12, sb_y + 7, 12, 12, dot_color);

    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        char status[128];
        snprintf(status, sizeof(status), "Running  |  %.1f FPS  |  %lld frames  |  Kain Native UI  |  GDI Backend  |  TEST_V2",
                 g_fps, (long long)g_frame_count);
        TextOutA(gdi_dc, 30, sb_y + 6, status, (int)strlen(status));
    }
}

// ── Window subclass ────────────────────────────────────────────────────
static WNDPROC g_orig_wndproc = NULL;

static LRESULT CALLBACK demo_window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (hdc) {
                KainWin32UiHost* host = (KainWin32UiHost*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
                if (host && host->hdc_buffer) {
                    BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
                           ps.rcPaint.right - ps.rcPaint.left,
                           ps.rcPaint.bottom - ps.rcPaint.top,
                           host->hdc_buffer, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
                }
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN: {
            if (wp == VK_ESCAPE) { PostQuitMessage(0); return 0; }

            // Log key events
            g_total_events++;
            snprintf(g_last_event_text, sizeof(g_last_event_text),
                     "KEY_DOWN: VK=0x%lX (%c)", (unsigned long)wp,
                     wp >= 32 && wp < 127 ? (char)wp : '?');
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int x = (int)(short)LOWORD(lp);
            int y = (int)(short)HIWORD(lp);
            g_total_events++;
            snprintf(g_last_event_text, sizeof(g_last_event_text),
                     "MOUSE_CLICK: (%d,%d)", x, y);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE: {
            // Only log every 120 frames worth of moves to avoid spam
            if (g_frame_count % 120 == 0) {
                int x = (int)(short)LOWORD(lp);
                int y = (int)(short)HIWORD(lp);
                snprintf(g_last_event_text, sizeof(g_last_event_text),
                         "MOUSE_MOVE: (%d,%d)", x, y);
            }
            return CallWindowProcA(g_orig_wndproc, hwnd, msg, wp, lp);
        }
    }
    return CallWindowProcA(g_orig_wndproc, hwnd, msg, wp, lp);
}

// ── Main ───────────────────────────────────────────────────────────────
int main(void) {
    int win_w = 1280, win_h = 720;

    printf("=== Full Demo Dashboard — Kain Native UI ===\n");
    printf("Build: " __DATE__ " " __TIME__ "\n\n");

    // Init input system
    abi_input_reset();
    g_input_session = abi_input_session_create("full_demo");
    abi_input_bind_action(g_input_session, "keyboard", "key_down", "Escape", "action.quit");
    printf("[INPUT] Session %lld\n", (long long)g_input_session);

    // Init UI
    abi_ui_reset();
    int64_t session = abi_ui_session_create("FullDemo", win_w, win_h);
    if (session <= 0) { fprintf(stderr, "FAIL: session_create\n"); return 1; }

    abi_ui_window_open(session, "Full Demo — Kain Native UI", win_w, win_h);
    if (abi_ui_host_attach(session, "winit") != 0) {
        fprintf(stderr, "FAIL: host_attach\n"); return 1;
    }
    printf("[UI] Session %lld  Backend: %s\n", (long long)session, abi_ui_host_backend(session));

    KainNativeUiSession* ks = abi_ui_find_session(session);
    if (!ks || !ks->host_state) { fprintf(stderr, "FAIL: no host state\n"); return 1; }
    KainWin32UiHost* host = (KainWin32UiHost*)ks->host_state;

    // Subclass window
    g_orig_wndproc = (WNDPROC)SetWindowLongPtrA(host->hwnd, GWLP_WNDPROC,
                                                  (LONG_PTR)demo_window_proc);
    printf("[UI] Window: %dx%d  hwnd=%p  fb=%p\n",
           host->width, host->height, (void*)host->hwnd, (void*)host->framebuffer);

    // Build minimal Kain node tree
    int64_t root = abi_ui_node_create(session, "root");
    abi_ui_node_set_rect(session, root, 0, 0, win_w, win_h);
    int64_t bg = abi_ui_node_create(session, "bg");
    abi_ui_node_set_parent(session, bg, root);
    abi_ui_node_set_rect(session, bg, 0, 0, win_w, win_h);
    abi_ui_node_set_style_string(session, bg, "fill_color", "#0F172A");

    printf("\nFrame loop running. Close window or press Esc to exit.\n");
    printf("Mouse clicks and key presses are logged in the event area.\n");
    printf("========================================================\n");

    int64_t frame = 0;
    MSG msg;

    while (1) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { host->running = 0; break; }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!host->running) break;

        abi_ui_begin_frame(session, 16.67);
        abi_ui_end_frame(session);

        // Update FPS counter
        g_frame_count++;
        g_fps_timer += 16.67;
        if (g_fps_timer >= 1000.0) {
            g_fps = (double)g_frame_count * 1000.0 / g_fps_timer;
            g_fps_timer = 0.0;
            g_frame_count = 0;
        }

        // Poll input system
        abi_input_begin_frame(g_input_session, 16.67);
        int64_t ec = abi_input_event_count(g_input_session);
        if (ec > g_last_event_count) {
            g_total_events += (ec - g_last_event_count);
            g_last_event_count = ec;
        }

        // Render
        if (host->framebuffer) {
            paint_dashboard((uint32_t*)host->framebuffer,
                           host->width, host->height, host->fb_stride / 4,
                           host->hdc_buffer);
            InvalidateRect(host->hwnd, NULL, FALSE);
        }

        if (frame % 60 == 0) {
            printf("Frame %lld | FPS: %.1f | Events: %lld | fb[0]=0x%08X\n",
                   (long long)frame, g_fps, (long long)g_total_events,
                   host->framebuffer ? *(uint32_t*)host->framebuffer : 0);
        }

        frame++;
        Sleep(16);
    }

    printf("\nShutdown after %lld frames.\n", (long long)frame);
    printf("Total input events: %lld\n", (long long)g_total_events);

    abi_input_session_destroy(g_input_session);
    abi_ui_session_destroy(session);
    printf("Done.\n");
    return 0;
}
