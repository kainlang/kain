// ============================================================================
//  fb_input_test.c — Framebuffer + Input Test
//  ============================================================================
//  Based on path_b_direct_fb's proven approach, adds live input polling.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_system.h"
#include "ui_system_internal.h"
#include "ui_host_adapter.h"
#include "../../include/ui_renderer.h"
#include "../../include/ui_layout.h"
#include "../../include/input_system.h"

// ── Stubs ──────────────────────────────────────────────────────────────
char* string_new(char* src);
double kain_clampd(double value, double min_value, double max_value);

// ── KainWin32UiHost ───────────────────────────────────────────────────
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

static int64_t g_input_session = 0;
static int g_running = 1;

// ── Window subclass for fixed WM_PAINT ────────────────────────────────
static WNDPROC g_orig_wndproc = NULL;
static LRESULT CALLBACK fixed_paint_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
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
    // Log input events
    switch (msg) {
        case WM_KEYDOWN:
            printf("  KEY: vk=%lu code=%c\n", (unsigned long)wp, (char)(wp >= 32 ? wp : '?'));
            break;
        case WM_LBUTTONDOWN:
            printf("  CLICK: (%d,%d)\n", (int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
            break;
        case WM_CHAR:
            printf("  CHAR: '%c' (%lu)\n", (char)wp, (unsigned long)wp);
            break;
    }
    return CallWindowProcA(g_orig_wndproc, hwnd, msg, wp, lp);
}

// ── Pixel fill ────────────────────────────────────────────────────────
static void fill_rect(uint32_t* fb, int stride, int x, int y, int w, int h, uint32_t c) {
    for (int r = y; r < y + h; r++)
        for (int cl = x; cl < x + w; cl++)
            fb[r * stride + cl] = c;
}

static void paint_ui(uint32_t* fb, int w, int h, int stride, HDC gdi_dc) {
    uint32_t BG = 0xFF0F172A, SURF = 0xFF1E293B, SURF2 = 0xFF252540;
    uint32_t HDR = 0xFF1A1A2E, ACC = 0xFF21D4A1, ACC2 = 0xFF4A90D9;
    uint32_t ACC3 = 0xFFE8914A, ACC4 = 0xFFE84A5F;
    int header_h = 56, status_h = 28, sidebar_w = 200;

    // Clear
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++)
            fb[r * stride + c] = BG;

    // Header
    fill_rect(fb, stride, 0, 0, w, header_h, HDR);
    fill_rect(fb, stride, 0, header_h-2, w, 2, ACC);
    fill_rect(fb, stride, 16, 18, 20, 20, ACC);

    // Sidbar
    fill_rect(fb, stride, 0, header_h, sidebar_w, h-header_h-status_h, 0xFF16162A);
    fill_rect(fb, stride, 16, header_h+44, 36, 2, ACC);
    uint32_t mcols[] = {ACC, ACC2, ACC3, 0xFF8888A0, 0xFF8888A0};
    for (int i = 0; i < 5; i++) {
        int iy = header_h + 58 + i * 44;
        fill_rect(fb, stride, 8, iy, sidebar_w-16, 36, i==0?0xFF2A2A4E:0xFF16162A);
        fill_rect(fb, stride, 16, iy+12, 8, 8, mcols[i]);
    }

    // Cards
    int cx = sidebar_w + 8, cw = w - sidebar_w - 16;
    int card_w = (cw - 40) / 4, card_h = 90, cards_y = header_h + 8;
    uint32_t sc[] = {ACC, ACC2, ACC3, ACC4};
    for (int i = 0; i < 4; i++) {
        int ccx = cx + 8 + i * (card_w + 8);
        fill_rect(fb, stride, ccx, cards_y, card_w, card_h, SURF2);
        fill_rect(fb, stride, ccx, cards_y, card_w, 3, sc[i]);
        fill_rect(fb, stride, ccx+12, cards_y+16, card_w-24, 28, SURF);
    }

    // Graph
    int gy = cards_y + card_h + 12 + 8, gw = cw - 16, gh = 160;
    fill_rect(fb, stride, cx+8, gy, gw, gh, SURF2);
    for (int i = 0; i < 8; i++) {
        int bw = (gw - 24 - 28) / 8, bh = 20 + (i * 17 + 7) % 120;
        fill_rect(fb, stride, cx+8+12+i*(bw+4), gy+gh-8-bh, bw, bh, sc[i%4]);
    }

    // Info bar
    fill_rect(fb, stride, cx+8, gy+gh+8, gw, 36, SURF2);

    // Status bar
    fill_rect(fb, stride, 0, h-status_h, w, status_h, HDR);

    // Text via GDI
    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(232, 232, 240));
        SetBkMode(gdi_dc, TRANSPARENT);
        SelectObject(gdi_dc, GetStockObject(SYSTEM_FONT));
        TextOutA(gdi_dc, 16, h-status_h+6, "Kain UI + Input Pipeline  |  Move mouse  |  Press keys  |  Watch terminal", 84);
        char buf[128];
        int64_t ec = abi_input_event_count(g_input_session);
        snprintf(buf, sizeof(buf), "Input events: %lld", (long long)ec);
        TextOutA(gdi_dc, cx+8, gy+gh+12, buf, (int)strlen(buf));
    }
}

// ── Main ───────────────────────────────────────────────────────────────
int main(void) {
    printf("=== Kain UI + Input Pipeline Test ===\n\n");

    // Init input system
    abi_input_reset();
    g_input_session = abi_input_session_create("input_test");
    abi_input_bind_action(g_input_session, "keyboard", "key_down", "Escape", "action.quit");
    printf("[INPUT] Session %lld\n", (long long)g_input_session);

    // Init UI + window
    abi_ui_reset();
    int64_t session = abi_ui_session_create("input_test", 1280, 720);
    if (session <= 0) return 1;
    abi_ui_window_open(session, "Kain UI + Input Test", 1280, 720);
    if (abi_ui_host_attach(session, "winit") != 0) return 1;
    printf("[UI] Session %lld\n", (long long)session);

    // Get session and subclass window
    KainNativeUiSession* ks = abi_ui_find_session(session);
    if (!ks || !ks->host_state) { fprintf(stderr, "No host state\n"); return 1; }
    KainWin32UiHost* host = (KainWin32UiHost*)ks->host_state;
    if (host->hwnd) {
        g_orig_wndproc = (WNDPROC)SetWindowLongPtrA(host->hwnd, GWLP_WNDPROC,
                                                      (LONG_PTR)fixed_paint_proc);
    }
    printf("[UI] Window active, hwnd=%p fb=%p\n", (void*)host->hwnd, (void*)host->framebuffer);

    // Build node tree (test that the engine works)
    int64_t root = abi_ui_node_create(session, "root");
    abi_ui_node_set_rect(session, root, 0, 0, 1280, 720);
    int64_t panel = abi_ui_node_create(session, "panel");
    abi_ui_node_set_parent(session, panel, root);
    abi_ui_node_set_rect(session, panel, 40, 40, 600, 400);
    abi_ui_node_set_style_string(session, panel, "fill_color", "#1E293B");
    printf("[UI] Tree built: %lld nodes\n", (long long)abi_ui_node_count(session));

    // Frame loop
    printf("\nFrame loop running. Press Escape on the window or close it to exit.\n");
    int64_t frame = 0, last_input_count = 0;
    // Frame loop — keep running until window close or Escape
    for (frame = 0; g_running; frame++) {
        abi_ui_host_pump(session);
        if (abi_ui_host_should_close(session)) break;

        abi_ui_begin_frame(session, 16.67);
        abi_ui_end_frame(session);

        // Render
        if (host && host->framebuffer) {
            ui_layout_resolve(ks);
            paint_ui((uint32_t*)host->framebuffer, host->width, host->height,
                     host->fb_stride / 4, host->hdc_buffer);
            InvalidateRect(host->hwnd, NULL, FALSE);
        }

        // Poll input system
        abi_input_begin_frame(g_input_session, 16.67);
        int64_t ec = abi_input_event_count(g_input_session);
        if (ec != last_input_count && frame % 5 == 0) {
            printf("  Frame %lld: %lld input events\n", (long long)frame, (long long)ec);
            for (int64_t i = last_input_count; i < ec && i < last_input_count + 3; i++) {
                printf("    event[%lld]: kind=%s code=%s\n",
                       (long long)i,
                       abi_input_event_kind(g_input_session, i),
                       abi_input_event_code(g_input_session, i));
            }
            last_input_count = ec;
        }

        if (abi_input_action_pressed(g_input_session, "action.quit")) {
            printf("  Escape pressed — quitting.\n");
            break;
        }

        Sleep(16);
    }

    abi_input_session_destroy(g_input_session);
    abi_ui_session_destroy(session);
    printf("\nDone — %lld frames\n", (long long)frame);
    return 0;
}
