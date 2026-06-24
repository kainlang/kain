// ============================================================================
//  Path A: Full Kain Pipeline (with WM_PAINT fix via subclassing)
//  ============================================================================
//  Uses the COMPLETE Kain UI pipeline:
//    - session_create → window_open → host_attach("winit")
//    - Build a full node tree with styles, colors, rectangles
//    - Frame loop: pump → begin_frame → end_frame → host_present
//
//  The ONLY fix: subclass the Win32 window to repair the WM_PAINT handler.
//  Original bug: WM_PAINT creates a temp DC and tries to SelectObject the DIB
//  section into it, but the DIB is already selected into host->hdc_buffer.
//  SelectObject fails, the temp DC retains its default 1×1 white monochrome
//  bitmap, and BitBlt stretches it to fill the window → BLANK WHITE WINDOW.
//
//  Our fix: BitBlt DIRECTLY from host->hdc_buffer in the subclassed WM_PAINT.
//  ============================================================================
//
//  Compile:
//    clang -std=c11 -g -O0 path_a_full_pipeline.c stubs.c ^
//      ..\ui_system.c ..\ui_host_adapter.c ..\ui_renderer.c ..\ui_layout.c ..\ui_color.c ^
//      ..\..\core\input_system.c ^
//      -I ..\..\..\include -I .. -I ..\..\core ^
//      -luser32 -lgdi32 -lopengl32 -o path_a_full_pipeline.exe

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ui_system_internal.h"     // KainNativeUiSession, host_state
#include "ui_host_adapter.h"
#include "../../include/ui_renderer.h"
#include "../../include/ui_layout.h"
#include "../../include/ui_color.h"

// ── Replicate KainWin32UiHost for access from test code ────────────────
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

// ── Window subclassing ──────────────────────────────────────────────────
// The ORIGINAL wndproc has a bug: it creates a temporary DC and tries to
// select host->hbitmap into it, but the bitmap is already in host->hdc_buffer.
// Our subclass intercepts WM_PAINT and BitBlts FROM host->hdc_buffer directly.
static WNDPROC g_original_wndproc = NULL;

static LRESULT CALLBACK fixed_wm_paint_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (hdc) {
            // Get host from GWLP_USERDATA (set by WM_NCCREATE in original proc)
            KainWin32UiHost* host = (KainWin32UiHost*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
            if (host && host->hdc_buffer) {
                // 🎯 FIX: BitBlt FROM host->hdc_buffer (which has the DIB selected)
                // instead of creating a temp DC and trying to SelectObject into it.
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

// ── Color palette ──────────────────────────────────────────────────────
#define C_BG       "#1A1A24"
#define C_SURFACE  "#252540"
#define C_SURFACE2 "#2E2E48"
#define C_HEADER   "#1E1E32"
#define C_SIDEBAR  "#202038"
#define C_BORDER   "#3A3A5C"
#define C_ACCENT   "#21D4A1"
#define C_ACCENT2  "#4A90D9"
#define C_ACCENT3  "#E8914A"
#define C_ACCENT4  "#E84A5F"
#define C_TEXT     "#E8E8F0"
#define C_TEXT_DIM "#8888A0"
#define C_HIGHLIGHT "#21D4A122"

// ── Node helpers ───────────────────────────────────────────────────────
static int64_t make_node(int64_t session, int64_t parent, const char* kind,
                         double x, double y, double w, double h) {
    int64_t n = abi_ui_node_create(session, kind ? kind : "node");
    if (parent > 0) abi_ui_node_set_parent(session, n, parent);
    abi_ui_node_set_rect(session, n, x, y, w, h);
    return n;
}

static void set_fill(int64_t session, int64_t node, const char* color) {
    if (color) abi_ui_node_set_style_string(session, node, "fill_color", color);
}

static void set_border(int64_t session, int64_t node, const char* color, double width) {
    if (color) abi_ui_node_set_style_string(session, node, "border_color", color);
    if (width > 0) abi_ui_node_set_style_f64(session, node, "border_width", width);
}

static void set_radius(int64_t session, int64_t node, double r) {
    if (r > 0) abi_ui_node_set_style_f64(session, node, "corner_radius", r);
}

static void set_opacity(int64_t session, int64_t node, double o) {
    abi_ui_node_set_style_f64(session, node, "opacity", o);
}

static void set_text(int64_t session, int64_t node, const char* text) {
    if (text) abi_ui_node_set_text(session, node, text);
}

static int64_t make_card(int64_t session, int64_t parent,
                         double x, double y, double w, double h,
                         const char* fill, const char* border,
                         const char* text) {
    int64_t card = make_node(session, parent, "card", x, y, w, h);
    set_fill(session, card, fill ? fill : C_SURFACE);
    set_border(session, card, border ? border : C_BORDER, 1.0);
    set_radius(session, card, 10.0);
    set_text(session, card, text ? text : "");
    return card;
}

// ── Build a rich UI node tree ──────────────────────────────────────────
static void build_ui_tree(int64_t session, int64_t win_w, int64_t win_h) {
    int64_t root = make_node(session, 0, "root", 0, 0, win_w, win_h);

    // ── Header ─────────────────────────────────────────────────────
    int64_t header = make_card(session, root, 0, 0, win_w, 60, C_HEADER, NULL, NULL);
    set_radius(session, header, 0.0);
    set_text(session, header, "Kain UI Pipeline - Path A");

    // Status dot
    int64_t dot = make_card(session, header, 16, 18, 24, 24, C_ACCENT, NULL, NULL);
    set_radius(session, dot, 12.0);

    // Accent line under header
    int64_t accent = make_card(session, root, 0, 58, win_w, 2, C_ACCENT, NULL, NULL);
    set_radius(session, accent, 0.0);

    // ── Content area ───────────────────────────────────────────────
    int64_t content_y = 62;
    int64_t content_h = win_h - 62 - 32;
    int64_t content = make_card(session, root, 0, content_y, win_w, content_h, C_BG, NULL, NULL);
    set_radius(session, content, 0.0);

    // ── Sidebar ────────────────────────────────────────────────────
    int64_t sidebar = make_card(session, content, 0, 0, 220, content_h, C_SIDEBAR, NULL, NULL);
    set_radius(session, sidebar, 0.0);
    set_text(session, sidebar, "NAVIGATION");
    
    int64_t sbar = make_card(session, sidebar, 16, 44, 36, 2, C_ACCENT, NULL, NULL);
    set_radius(session, sbar, 0.0);

    // Menu items
    const char* items[] = {"Dashboard", "Analytics", "Explorer", "Settings"};
    const char* colors[] = {C_ACCENT, C_ACCENT2, C_ACCENT3, C_TEXT_DIM};
    for (int i = 0; i < 4; i++) {
        int64_t mi = make_card(session, sidebar, 8, 58 + i * 44, 204, 38,
                               i == 0 ? C_HIGHLIGHT : C_SIDEBAR,
                               i == 0 ? C_ACCENT : C_SIDEBAR, NULL);
        set_radius(session, mi, 6.0);
        int64_t d = make_card(session, mi, 12, 11, 8, 8, colors[i], NULL, NULL);
        set_radius(session, d, 4.0);
        set_text(session, mi, items[i]);
    }

    // ── Main panel ──────────────────────────────────────────────────
    int64_t main_panel = make_card(session, content, 228, 0, win_w - 236, content_h, C_BG, NULL, NULL);
    set_radius(session, main_panel, 0.0);

    // ── Status cards row ────────────────────────────────────────────
    int card_w = (win_w - 260) / 4;
    struct { const char* title; const char* value; const char* color; } cards[] = {
        {"Sessions", "16", C_ACCENT},
        {"Nodes", "4096", C_ACCENT2},
        {"Styles", "8192", C_ACCENT3},
        {"Events", "1024", C_ACCENT4},
    };
    for (int i = 0; i < 4; i++) {
        int64_t c = make_card(session, main_panel, 8 + i * (card_w + 8), 8,
                              card_w, 100, C_SURFACE, C_BORDER, NULL);
        int64_t s = make_card(session, c, 0, 0, card_w, 3, cards[i].color, NULL, NULL);
        set_radius(session, s, 0.0);
        int64_t v = make_card(session, c, 12, 14, card_w - 24, 32, C_SURFACE, NULL, NULL);
        set_radius(session, v, 4.0);
        set_text(session, v, cards[i].value);
        set_text(session, c, cards[i].title);
    }

    // ── Activity graph area ─────────────────────────────────────────
    int64_t graph = make_card(session, main_panel, 8, 120, win_w - 244, 180,
                              C_SURFACE, C_BORDER, NULL);
    set_radius(session, graph, 8.0);

    // Chart bars
    const char* bar_colors[] = {C_ACCENT, C_ACCENT2, C_ACCENT3, C_ACCENT4,
                                 C_ACCENT, C_ACCENT2, C_ACCENT, C_ACCENT3};
    int bar_count = 8;
    int bw = (win_w - 268 - (bar_count - 1) * 4) / bar_count;
    if (bw < 4) bw = 4;
    for (int i = 0; i < bar_count; i++) {
        int bh = 20 + (i * 17 + 7) % 140;
        int by = 180 - 8 - bh;
        int64_t bar = make_card(session, graph, 12 + i * (bw + 4), by, bw, bh,
                                bar_colors[i], NULL, NULL);
        set_radius(session, bar, 3.0);
        set_opacity(session, bar, 0.7 + (i % 3) * 0.15);
    }

    // ── Status bar ──────────────────────────────────────────────────
    int64_t status = make_card(session, root, 0, win_h - 32, win_w, 32, C_HEADER, NULL, NULL);
    set_radius(session, status, 0.0);
    set_text(session, status, "Path A | Full Kain Pipeline | Win32 DIB Framebuffer");
}

// ── Verify pixel output ────────────────────────────────────────────────
static void verify_pixels(KainWin32UiHost* host) {
    if (!host || !host->framebuffer) return;
    
    uint32_t* fb = (uint32_t*)host->framebuffer;
    int total = host->width * host->height;
    int non_dark = 0;
    int bright_pixels = 0;
    
    for (int i = 0; i < total && i < 100000; i++) {
        if (fb[i] != 0xFF1A1A24) {
            non_dark++;
            uint8_t r = (fb[i] >> 0) & 0xFF;
            uint8_t g = (fb[i] >> 8) & 0xFF;
            uint8_t b = (fb[i] >> 16) & 0xFF;
            uint8_t a = (fb[i] >> 24) & 0xFF;
            if (r > 30 || g > 30 || b > 30 && a > 200) {
                bright_pixels++;
            }
        }
    }
    
    printf("  Pixel verification (sample of first 100K):\n");
    printf("    Non-background pixels: %d\n", non_dark);
    printf("    Bright/colored pixels: %d\n", bright_pixels);
    printf("    Framebuffer[0]      = 0x%08X\n", fb[0]);
    printf("    Framebuffer[100]    = 0x%08X\n", fb[100]);
    printf("    Framebuffer[50000]  = 0x%08X\n", fb[50000]);
    
    if (non_dark == 0) {
        printf("  ⚠️  WARNING: All sampled pixels are dark background!\n");
        printf("     Kain's node tree may not be rendering (no fill_color on root?)\n");
    }
    if (bright_pixels > 0) {
        printf("  ✅ Colored pixels detected — something is rendering!\n");
    }
}

// ── Main ────────────────────────────────────────────────────────────────
int main(void) {
    int64_t win_w = 1280, win_h = 720;

    printf("=== Path A: Full Kain Pipeline (with WM_PAINT fix) ===\n\n");

    // ── Init ──────────────────────────────────────────────────────
    abi_ui_reset();
    int64_t session = abi_ui_session_create("PathA", win_w, win_h);
    if (session <= 0) { fprintf(stderr, "FAIL: session_create\n"); return 1; }
    printf("Session %lld created.\n", (long long)session);

    if (abi_ui_window_open(session, "Path A: Full Kain Pipeline", win_w, win_h) != 0) {
        fprintf(stderr, "FAIL: window_open\n"); return 1;
    }

    printf("Attaching win32 host...\n");
    if (abi_ui_host_attach(session, "winit") != 0) {
        fprintf(stderr, "FAIL: host_attach\n"); return 1;
    }
    printf("Backend: %s\n", abi_ui_host_backend(session));

    // ── Get session pointer for internal access ────────────────────
    KainNativeUiSession* ks = abi_ui_find_session(session);
    if (!ks || !ks->host_state) {
        fprintf(stderr, "FAIL: cannot access session host_state\n");
        return 1;
    }
    KainWin32UiHost* host = (KainWin32UiHost*)ks->host_state;
    printf("Host: hwnd=%p fb=%p fb_stride=%d hdc_buffer=%p\n",
           (void*)host->hwnd, (void*)host->framebuffer,
           host->fb_stride, (void*)host->hdc_buffer);

    // ── Subclass window to fix WM_PAINT ────────────────────────────
    g_original_wndproc = (WNDPROC)SetWindowLongPtrA(host->hwnd, GWLP_WNDPROC,
                                                     (LONG_PTR)fixed_wm_paint_proc);
    printf("Window subclassed. Original=%p\n", (void*)g_original_wndproc);

    // ── Build UI tree ─────────────────────────────────────────────
    printf("Building UI tree...\n");
    build_ui_tree(session, win_w, win_h);
    printf("Nodes created: %lld\n", (long long)abi_ui_node_count(session));

    // ── Diagnose node tree ─────────────────────────────────────────
    printf("\nNode tree diagnostics:\n");
    for (int i = 0; i < 5; i++) {
        if (ks->nodes[i].in_use) {
            printf("  Node[%d]: id=%lld kind=%s parent=%lld rect=(%.0f,%.0f,%.0f,%.0f) children=%lld\n",
                   i, (long long)ks->nodes[i].id, ks->nodes[i].kind,
                   (long long)ks->nodes[i].parent_id,
                   ks->nodes[i].x, ks->nodes[i].y,
                   ks->nodes[i].width, ks->nodes[i].height,
                   (long long)ks->nodes[i].child_count);
        }
    }

    // ── Render first frame (manual) ─────────────────────────────────
    printf("\nRendering first frame...\n");
    ui_layout_resolve(ks);
    int64_t pixels = ui_render_frame(ks, (uint32_t*)host->framebuffer,
                                     host->width, host->height,
                                     host->fb_stride / 4);
    printf("Rendered %lld pixels\n", (long long)pixels);

    // Check pixels
    verify_pixels(host);

    // Present
    InvalidateRect(host->hwnd, NULL, FALSE);
    UpdateWindow(host->hwnd);

    printf("\nEntering frame loop. Close the window to exit.\n");
    printf("============================================================\n");

    // ── Frame loop ─────────────────────────────────────────────────
    int64_t frame = 0;
    while (host->running && !ks->host_should_close) {
        abi_ui_host_pump(session);
        if (ks->host_should_close) break;

        abi_ui_begin_frame(session, 16.67);
        abi_ui_end_frame(session);

        // Full Kain pipeline: layout + render + present
        abi_ui_host_present(session);
        // Our subclassed WM_PAINT handles the BitBlt correctly

        frame++;
        if (frame % 60 == 0) {
            printf("Frame %lld | nodes=%lld | fb[0]=0x%08X | fb[100]=0x%08X\n",
                   (long long)frame,
                   (long long)abi_ui_node_count(session),
                   host->framebuffer ? ((uint32_t*)host->framebuffer)[0] : 0,
                   host->framebuffer ? ((uint32_t*)host->framebuffer)[100] : 0);
        }

        // Window resize check
        RECT rc;
        GetClientRect(host->hwnd, &rc);
        if (rc.right != host->width || rc.bottom != host->height) {
            printf("  Window resized: %dx%d -> %dx%d\n",
                   host->width, host->height, (int)rc.right, (int)rc.bottom);
        }

        Sleep(16);
    }

    printf("\nShutdown after %lld frames.\n", (long long)frame);
    abi_ui_session_destroy(session);
    printf("Done.\n");
    return 0;
}
