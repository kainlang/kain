// ============================================================================
//  widget_hello.c — Hello World Widget Demo
//  ============================================================================
//  Demonstrates:
//    - Minimal widget-style UI with panel, label, and button
//    - Button click changes label text (visual feedback)
//    - FPS counter displayed in title bar
//    - Clean exit on Escape key or window close
//    - Direct framebuffer rendering with GDI text overlay
//    - Window subclass for input handling
//  ============================================================================
//  Build:
//    clang -std=c11 -g -O0 widget_hello.c stubs.c ^
//      ..\ui_system.c ..\ui_host_adapter.c ..\ui_renderer.c ..\ui_layout.c ..\ui_color.c ^
//      ..\..\core\input_system.c ..\..\core\component_surface.c ^
//      -I ..\..\..\include -I .. -I ..\..\core ^
//      -luser32 -lgdi32 -lopengl32 ^
//      -o widget_hello.exe
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
#define C_BG        0xFF1A1A24
#define C_SURFACE   0xFF252540
#define C_PANEL     0xFF2A2A44
#define C_PANEL_BDR 0xFF3A3A5C
#define C_ACCENT    0xFF21D4A1
#define C_ACCENT2   0xFF4A90D9
#define C_TEXT      0xFFE8E8F0
#define C_TEXT_DIM  0xFF8888A0
#define C_HEADER    0xFF1E1E32
#define C_BUTTON    0xFF303050
#define C_BUTTON_HL 0xFF404068
#define C_DISPLAY   0xFF0A0A14

// ── App state ──────────────────────────────────────────────────────────
#define MAX_LABEL 128

static char g_label_text[MAX_LABEL] = "Hello, Widget!";
static int g_click_count = 0;
static int g_highlight_btn = -1;
static double g_fps = 60.0;
static int64_t g_frame_count = 0;
static double g_fps_timer = 0.0;
static int g_show_alt_text = 0;

// ── Button regions ─────────────────────────────────────────────────────
typedef struct {
    double x, y, w, h;
    const char* label;
    int id;
} WidgetRegion;

#define MAX_WIDGETS 16
static WidgetRegion g_widgets[MAX_WIDGETS];
static int g_widget_count = 0;

static void layout_widgets(int win_w, int win_h) {
    g_widget_count = 0;
    int pad = 20;

    // ── Panel background ──────────────────────────────────────────────
    WidgetRegion* panel = &g_widgets[g_widget_count++];
    panel->x = pad;
    panel->y = 70;
    panel->w = win_w - 2 * pad;
    panel->h = win_h - 100;
    panel->label = NULL;
    panel->id = 0; // panel

    // ── Label display area ────────────────────────────────────────────
    WidgetRegion* label_area = &g_widgets[g_widget_count++];
    label_area->x = panel->x + 20;
    label_area->y = panel->y + 30;
    label_area->w = panel->w - 40;
    label_area->h = 90;
    label_area->label = NULL;
    label_area->id = 1; // display

    // ── Hello button ──────────────────────────────────────────────────
    WidgetRegion* btn_hello = &g_widgets[g_widget_count++];
    btn_hello->x = panel->x + 40;
    btn_hello->y = label_area->y + label_area->h + 30;
    btn_hello->w = (panel->w - 100) / 2;
    btn_hello->h = 50;
    btn_hello->label = "Click Me!";
    btn_hello->id = 10; // hello button

    // ── Exit button ───────────────────────────────────────────────────
    WidgetRegion* btn_exit = &g_widgets[g_widget_count++];
    btn_exit->x = btn_hello->x + btn_hello->w + 20;
    btn_exit->y = btn_hello->y;
    btn_exit->w = btn_hello->w;
    btn_exit->h = 50;
    btn_exit->label = "Exit";
    btn_exit->id = 11; // exit button
}

static int hit_test_widget(double mx, double my) {
    for (int i = 0; i < g_widget_count; i++) {
        WidgetRegion* w = &g_widgets[i];
        if (w->id >= 10 && mx >= w->x && mx < w->x + w->w &&
            my >= w->y && my < w->y + w->h) {
            return i;
        }
    }
    return -1;
}

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
                int dx = (x + radius) - col, dy = (y + radius) - row;
                inside = (dx*dx + dy*dy) <= r2;
            } else if (col >= x + w - radius && row < y + radius) {
                int dx = col - (x + w - radius), dy = (y + radius) - row;
                inside = (dx*dx + dy*dy) <= r2;
            } else if (col < x + radius && row >= y + h - radius) {
                int dx = (x + radius) - col, dy = row - (y + h - radius);
                inside = (dx*dx + dy*dy) <= r2;
            } else if (col >= x + w - radius && row >= y + h - radius) {
                int dx = col - (x + w - radius), dy = row - (y + h - radius);
                inside = (dx*dx + dy*dy) <= r2;
            }
            if (inside) fb[row * stride + col] = color;
        }
    }
}

// ── Paint the widget UI ────────────────────────────────────────────────
static void paint_hello(uint32_t* fb, int w, int h, int stride, HDC gdi_dc) {
    // Clear background
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++)
            fb[r * stride + c] = C_BG;

    // ── Header bar ─────────────────────────────────────────────────
    int header_h = 48;
    fill_rect(fb, stride, 0, 0, w, header_h, C_HEADER);
    fill_rect(fb, stride, 0, header_h - 2, w, 2, C_ACCENT);

    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0xE8, 0xE8, 0xF0));
        SetBkMode(gdi_dc, TRANSPARENT);
        SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
        TextOutA(gdi_dc, 14, 6, "Widget Hello", 12);
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));

        char header_info[64];
        snprintf(header_info, sizeof(header_info), "Kain Native UI  |  %.1f FPS", g_fps);
        TextOutA(gdi_dc, 14, 26, header_info, (int)strlen(header_info));

        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        char frame_str[32];
        snprintf(frame_str, sizeof(frame_str), "Frame %lld", (long long)g_frame_count);
        TextOutA(gdi_dc, w - 120, 14, frame_str, (int)strlen(frame_str));
    }

    // ── Panel background ───────────────────────────────────────────
    WidgetRegion* panel = &g_widgets[0];
    fill_rounded_rect(fb, stride, w, h, (int)panel->x, (int)panel->y,
                      (int)panel->w, (int)panel->h, C_PANEL, 8);
    fill_rounded_rect(fb, stride, w, h, (int)panel->x, (int)panel->y,
                      (int)panel->w, (int)panel->h, C_PANEL_BDR, 8);
    fill_rounded_rect(fb, stride, w, h, (int)panel->x + 1, (int)panel->y + 1,
                      (int)panel->w - 2, (int)panel->h - 2, C_PANEL, 7);

    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
        TextOutA(gdi_dc, (int)panel->x + 14, (int)panel->y + 8, "Widget Panel", 12);
    }

    // ── Label display area ─────────────────────────────────────────
    WidgetRegion* disp = &g_widgets[1];
    fill_rounded_rect(fb, stride, w, h, (int)disp->x, (int)disp->y,
                      (int)disp->w, (int)disp->h, C_DISPLAY, 6);
    fill_rounded_rect(fb, stride, w, h, (int)disp->x, (int)disp->y,
                      (int)disp->w, (int)disp->h, 0xFF3A3A5C, 6);
    fill_rounded_rect(fb, stride, w, h, (int)disp->x + 1, (int)disp->y + 1,
                      (int)disp->w - 2, (int)disp->h - 2, C_DISPLAY, 5);

    if (gdi_dc) {
        // Display label text
        SetTextColor(gdi_dc, RGB(0xE8, 0xE8, 0xF0));
        SetBkMode(gdi_dc, TRANSPARENT);
        HFONT label_font = CreateFontA(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                        DEFAULT_PITCH, "Segoe UI");
        SelectObject(gdi_dc, label_font);

        RECT text_r = {
            (int)disp->x + 12, (int)disp->y + 8,
            (int)disp->x + (int)disp->w - 12, (int)disp->y + (int)disp->h - 8
        };
        DrawTextA(gdi_dc, g_label_text, -1, &text_r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DeleteObject(label_font);

        // Click count sub-label
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
        char count_str[64];
        snprintf(count_str, sizeof(count_str), "Clicks: %d", g_click_count);
        RECT sub_r = {
            (int)disp->x + 12, (int)disp->y + (int)disp->h - 24,
            (int)disp->x + (int)disp->w - 12, (int)disp->y + (int)disp->h - 4
        };
        DrawTextA(gdi_dc, count_str, -1, &sub_r, DT_RIGHT | DT_SINGLELINE);
    }

    // ── Buttons ────────────────────────────────────────────────────
    HFONT btn_font = CreateFontA(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                  DEFAULT_PITCH, "Segoe UI");

    for (int i = 0; i < g_widget_count; i++) {
        WidgetRegion* wr = &g_widgets[i];
        if (wr->id < 10) continue; // skip non-buttons

        int bx = (int)wr->x, by = (int)wr->y, bw = (int)wr->w, bh = (int)wr->h;

        uint32_t btn_color;
        uint32_t text_color;

        if (i == g_highlight_btn) {
            btn_color = C_BUTTON_HL;
            text_color = 0xFFFFFFFF;
        } else if (wr->id == 11) { // Exit
            btn_color = C_ACCENT2;
            text_color = 0xFFFFFFFF;
        } else {
            btn_color = C_ACCENT;
            text_color = 0xFFFFFFFF;
        }

        fill_rounded_rect(fb, stride, w, h, bx, by, bw, bh, btn_color, 8);
        // Subtle inset
        fill_rounded_rect(fb, stride, w, h, bx + 1, by + 1, bw - 2, bh - 2,
                          ui_color_blend(0x40000000, btn_color), 7);

        if (gdi_dc) {
            SetTextColor(gdi_dc, RGB((text_color >> 16) & 0xFF,
                                     (text_color >> 8) & 0xFF,
                                     text_color & 0xFF));
            SetBkMode(gdi_dc, TRANSPARENT);
            SelectObject(gdi_dc, btn_font);
            RECT btn_r = { bx, by, bx + bw, by + bh };
            DrawTextA(gdi_dc, wr->label, -1, &btn_r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
    DeleteObject(btn_font);

    // ── Status bar ─────────────────────────────────────────────────
    int status_y = h - 24;
    fill_rect(fb, stride, 0, status_y, w, 24, C_HEADER);
    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
        char status[128];
        snprintf(status, sizeof(status),
                 "Click \"Click Me!\" to change the label  |  Esc to exit  |  %.1f FPS",
                 g_fps);
        TextOutA(gdi_dc, 10, status_y + 4, status, (int)strlen(status));
    }
}

// ── Window subclass ────────────────────────────────────────────────────
static WNDPROC g_orig_wndproc = NULL;

static LRESULT CALLBACK hello_window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
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
        case WM_LBUTTONDOWN: {
            int mx = (int)(short)LOWORD(lp);
            int my = (int)(short)HIWORD(lp);
            int hit = hit_test_widget((double)mx, (double)my);
            if (hit >= 0) {
                g_highlight_btn = hit;
                WidgetRegion* w = &g_widgets[hit];
                if (w->id == 10) {
                    // Hello button — toggle text
                    g_click_count++;
                    g_show_alt_text = !g_show_alt_text;
                    if (g_show_alt_text) {
                        snprintf(g_label_text, sizeof(g_label_text),
                                 "Clicked %d time%s!", g_click_count,
                                 g_click_count == 1 ? "" : "s");
                    } else {
                        snprintf(g_label_text, sizeof(g_label_text),
                                 "Hello, Widget!");
                    }
                } else if (w->id == 11) {
                    PostQuitMessage(0);
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            g_highlight_btn = -1;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_KEYDOWN: {
            if (wp == VK_ESCAPE) { PostQuitMessage(0); return 0; }
            if (wp == VK_SPACE || wp == VK_RETURN) {
                // Simulate click on hello button
                g_click_count++;
                g_show_alt_text = !g_show_alt_text;
                if (g_show_alt_text) {
                    snprintf(g_label_text, sizeof(g_label_text),
                             "Clicked %d time%s!", g_click_count,
                             g_click_count == 1 ? "" : "s");
                } else {
                    snprintf(g_label_text, sizeof(g_label_text), "Hello, Widget!");
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            return 0;
        }
    }
    return CallWindowProcA(g_orig_wndproc, hwnd, msg, wp, lp);
}

// ── Main ───────────────────────────────────────────────────────────────
int main(void) {
    int win_w = 400, win_h = 300;

    printf("=== Widget Hello — Kain Native UI ===\n");
    printf("Build: " __DATE__ " " __TIME__ "\n\n");

    // Init
    snprintf(g_label_text, sizeof(g_label_text), "Hello, Widget!");

    abi_ui_reset();
    int64_t session = abi_ui_session_create("WidgetHello", win_w, win_h);
    if (session <= 0) { fprintf(stderr, "FAIL: session_create\n"); return 1; }

    abi_ui_window_open(session, "Widget Hello — Kain Native UI", win_w, win_h);
    if (abi_ui_host_attach(session, "winit") != 0) {
        fprintf(stderr, "FAIL: host_attach\n"); return 1;
    }
    printf("Session: %lld  Backend: %s\n", (long long)session, abi_ui_host_backend(session));

    KainNativeUiSession* ks = abi_ui_find_session(session);
    if (!ks || !ks->host_state) { fprintf(stderr, "FAIL: no host state\n"); return 1; }
    KainWin32UiHost* host = (KainWin32UiHost*)ks->host_state;

    // Subclass window
    g_orig_wndproc = (WNDPROC)SetWindowLongPtrA(host->hwnd, GWLP_WNDPROC,
                                                  (LONG_PTR)hello_window_proc);
    printf("Window: hwnd=%p  fb=%p  %dx%d\n",
           (void*)host->hwnd, (void*)host->framebuffer, host->width, host->height);

    // Build node tree
    int64_t root = abi_ui_node_create(session, "window");
    abi_ui_node_set_rect(session, root, 0, 0, win_w, win_h);

    int64_t bg = abi_ui_node_create(session, "bg");
    abi_ui_node_set_parent(session, bg, root);
    abi_ui_node_set_rect(session, bg, 0, 0, win_w, win_h);
    abi_ui_node_set_style_string(session, bg, "fill_color", "#1A1A24");

    // Layout
    layout_widgets(win_w, win_h);

    printf("\nFrame loop running. Click \"Click Me!\" or press Space/Enter.\n");
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

        // Update FPS
        g_frame_count++;
        g_fps_timer += 16.67;
        if (g_fps_timer >= 1000.0) {
            g_fps = (double)frame * 1000.0 / g_fps_timer;
            g_fps_timer = 0.0;
            frame = 0;
        }

        // Render directly to framebuffer
        if (host->framebuffer) {
            paint_hello((uint32_t*)host->framebuffer,
                       host->width, host->height, host->fb_stride / 4,
                       host->hdc_buffer);
            InvalidateRect(host->hwnd, NULL, FALSE);
        }

        if (g_frame_count % 60 == 0) {
            printf("Frame %lld | text='%s' | clicks=%d | fps=%.1f\n",
                   (long long)g_frame_count, g_label_text, g_click_count, g_fps);
        }

        Sleep(16);
    }

    printf("\nShutdown after %lld frames. Total clicks: %d\n",
           (long long)g_frame_count, g_click_count);
    printf("Final label: %s\n", g_label_text);
    abi_ui_session_destroy(session);
    printf("Done.\n");
    return 0;
}
