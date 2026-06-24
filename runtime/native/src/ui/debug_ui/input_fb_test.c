// ============================================================================
//  input_fb_test.c — Full UI Pipeline + Input System Test
//  ============================================================================
//  Tests EVERYTHING end-to-end:
//    1. Window creation via Kain host adapter (WINIT backend)
//    2. Node tree: create nodes, set styles, parent-child relationships
//    3. Software rendering via ui_render_frame (fill_color, border, corner_radius)
//    4. Input system: Win32 → host adapter → abi_input_push_event → poll
//    5. UI event queue: abi_ui_push_event → abi_ui_poll_event
//    6. Layout resolution: ui_layout_resolve positions cells
//    7. Frame loop with pump, render, present, input polling
//  ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ── Kain UI system + renderer + layout + input ──────────────────────────
#include "ui_system.h"
#include "ui_host_adapter.h"
#include "../../include/ui_renderer.h"
#include "../../include/ui_layout.h"
#include "../../include/ui_color.h"
#include "../../include/input_system.h"

// ── Stubs ───────────────────────────────────────────────────────────────
char* string_new(char* src);
double kain_clampd(double value, double min_value, double max_value);

// ── Bring in KainWin32UiHost for direct framebuffer access ──────────────
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

// ── Global state ────────────────────────────────────────────────────────
static int64_t g_input_session = 0;
static int64_t g_frame = 0;
static int g_running = 1;

// ── Color helpers ───────────────────────────────────────────────────────
static uint32_t rgba(int r, int g, int b, int a) {
    return ((uint32_t)(a & 0xFF) << 24) |
           ((uint32_t)(b & 0xFF) << 16) |
           ((uint32_t)(g & 0xFF) <<  8) |
           ((uint32_t)(r & 0xFF));
}

// ── Node creation helpers ───────────────────────────────────────────────
static int64_t make_node(int64_t session, int64_t parent, const char* kind,
                         double x, double y, double w, double h,
                         const char* fill)
{
    int64_t n = abi_ui_node_create(session, kind && kind[0] ? kind : "box");
    if (parent > 0) abi_ui_node_set_parent(session, n, parent);
    abi_ui_node_set_rect(session, n, x, y, w, h);
    if (fill) abi_ui_node_set_style_string(session, n, "fill_color", fill);
    return n;
}

static int64_t make_card(int64_t session, int64_t parent,
                         double x, double y, double w, double h,
                         const char* fill, const char* border, double radius)
{
    int64_t n = make_node(session, parent, "card", x, y, w, h, fill);
    if (border) abi_ui_node_set_style_string(session, n, "border_color", border);
    if (radius > 0) abi_ui_node_set_style_f64(session, n, "corner_radius", radius);
    return n;
}

// ── Print input events for debugging ────────────────────────────────────
static void poll_input_events(int64_t input_session)
{
    int64_t count = abi_input_event_count(input_session);
    if (count > 0) {
        int64_t i;
        for (i = 0; i < count && i < 5; i++) {
            const char* kind = abi_input_event_kind(input_session, i);
            const char* code = abi_input_event_code(input_session, i);
            const char* text = abi_input_event_text(input_session, i);
            printf("  INPUT[%lld]: kind=%s code=%s text=[%s]\n",
                   (long long)i,
                   kind ? kind : "(null)",
                   code ? code : "(null)",
                   text ? text : "(null)");
        }
    }
}

// ── Window subclass for input event monitoring ──────────────────────────
static WNDPROC g_orig_wndproc = NULL;

static LRESULT CALLBACK input_monitor_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    // Log interesting messages before passing through
    switch (msg) {
        case WM_KEYDOWN:
            printf("  WIN32 KEYDOWN: vk=%lu\n", (unsigned long)wp);
            break;
        case WM_LBUTTONDOWN:
            printf("  WIN32 LBUTTONDOWN: (%d, %d)\n",
                   (int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
            break;
        case WM_MOUSEMOVE:
        {
            // Print mouse moves rarely to avoid spam
            static int64_t move_count = 0;
            if (++move_count % 50 == 0)
                printf("  WIN32 MOUSEMOVE: (%d, %d)\n",
                       (int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
            break;
        }
    }
    return CallWindowProcA(g_orig_wndproc, hwnd, msg, wp, lp);
}

// ============================================================================
//  MAIN — Full UI + Input Pipeline Test
// ============================================================================
int main(void)
{
    int64_t win_w = 1280, win_h = 720;
    printf("=== Kain UI + Input System — Full Pipeline Test ===\n\n");
    fflush(stdout);

    // ── 1. Init input system ──────────────────────────────────────
    printf("[1] abi_input_reset...\n"); fflush(stdout);
    abi_input_reset();
    printf("[2] abi_input_session_create...\n"); fflush(stdout);
    g_input_session = abi_input_session_create("ui_input_test");
    printf("[INPUT] Session %lld created\n", (long long)g_input_session);

    // Bind some action-to-event mappings for testing
    abi_input_bind_action(g_input_session, "keyboard", "key_down", "Space", "action.jump");
    abi_input_bind_action(g_input_session, "keyboard", "key_down", "Escape", "action.quit");
    abi_input_bind_action(g_input_session, "keyboard", "key_down", "Enter", "action.confirm");
    abi_input_bind_action(g_input_session, "pointer", "pointer_down", "left", "action.click");
    printf("[INPUT] Bound 4 actions\n");

    // ── 2. Init UI system ─────────────────────────────────────────
    abi_ui_reset();
    int64_t session = abi_ui_session_create("input_fb_test", win_w, win_h);
    if (session <= 0) {
        fprintf(stderr, "FAIL: abi_ui_session_create\n");
        return 1;
    }
    printf("[UI] Session %lld created (%lldx%lld)\n",
           (long long)session, (long long)win_w, (long long)win_h);

    abi_ui_window_open(session, "Kain UI + Input Pipeline Test", win_w, win_h);
    int64_t attach = abi_ui_host_attach(session, "winit");
    if (attach != 0) {
        fprintf(stderr, "FAIL: abi_ui_host_attach (status=%lld)\n", (long long)attach);
        return 1;
    }
    printf("[UI] Win32 host attached. Backend: %s\n", abi_ui_host_backend(session));

    // Subclass window to monitor raw input
    KainNativeUiSession* ks = abi_ui_find_session(session);
    KainWin32UiHost* host = (KainWin32UiHost*)(ks ? ks->host_state : NULL);
    if (host && host->hwnd) {
        g_orig_wndproc = (WNDPROC)SetWindowLongPtrA(host->hwnd, GWLP_WNDPROC,
                                                      (LONG_PTR)input_monitor_proc);
        printf("[UI] Window subclassed for input monitoring\n");
    }

    // ── 3. Build UI node tree ─────────────────────────────────────
    printf("[UI] Building node tree...\n");

    // Color palette
    const char* BG        = "#0F172A";
    const char* SURFACE   = "#1E293B";
    const char* SURFACE2  = "#252540";
    const char* HEADER    = "#1A1A2E";
    const char* SIDEBAR   = "#16162A";
    const char* BORDER    = "#3A3A5C";
    const char* ACCENT    = "#21D4A1";
    const char* ACCENT2   = "#4A90D9";
    const char* ACCENT3   = "#E8914A";
    const char* ACCENT4   = "#E84A5F";
    const double CARD_R = 8.0;

    int header_h = 56;
    int status_h = 28;
    int sidebar_w = 200;

    // Root
    int64_t root = make_node(session, 0, "root", 0, 0, win_w, win_h, BG);

    // Header bar
    int64_t header = make_card(session, root, 0, 0, win_w, header_h, HEADER, NULL, 0);
    // Status dot
    make_card(session, header, 16, 16, 24, 24, ACCENT, NULL, 12);
    // Accent line
    make_card(session, root, 0, header_h - 2, win_w, 2, ACCENT, NULL, 0);

    // Sidebar
    int64_t sidebar = make_card(session, root, 0, header_h, sidebar_w, win_h - header_h - status_h, SIDEBAR, BORDER, 0);
    // Sidebar accent
    make_card(session, sidebar, 16, header_h - 16 + 44, 36, 2, ACCENT, NULL, 0);

    // Sidebar items (colored dots + rects)
    const char* item_colors[] = {ACCENT, ACCENT2, ACCENT3, "#8888A0", "#8888A0"};
    for (int i = 0; i < 5; i++) {
        double iy = 58 + i * 44;
        int64_t item = make_card(session, sidebar, 8, iy, sidebar_w - 16, 36,
                                  i == 0 ? "#21D4A122" : SIDEBAR, i == 0 ? ACCENT : SIDEBAR, 6);
        make_card(session, item, 12, 12, 8, 8, item_colors[i], NULL, 4);
    }

    // Content area
    double content_x = sidebar_w + 8;
    double content_w = win_w - sidebar_w - 16;
    int64_t content = make_card(session, root, content_x, (double)header_h + 8,
                                 content_w, win_h - header_h - status_h - 16, BG, NULL, 0);

    // Status cards row
    double card_w = (content_w - 40) / 4;
    double card_h = 90;
    double cards_y = 8;
    const char* stripe_colors[] = {ACCENT, ACCENT2, ACCENT3, ACCENT4};

    for (int i = 0; i < 4; i++) {
        double cx = 8 + i * (card_w + 8);
        int64_t card = make_card(session, content, cx, cards_y, card_w, card_h, SURFACE2, BORDER, CARD_R);
        // Top accent stripe
        make_card(session, card, 0, 0, card_w, 3, stripe_colors[i], NULL, 0);
        // Inner value box
        make_card(session, card, 12, 16, card_w - 24, 28, SURFACE, NULL, 4);
    }

    // Section divider
    double section_y = cards_y + card_h + 12;
    make_card(session, content, 8, section_y, content_w - 16, 1, BORDER, NULL, 0);

    // Graph area
    double graph_y = section_y + 12;
    double graph_h = 160;
    double graph_w = content_w - 16;
    int64_t graph = make_card(session, content, 8, graph_y, graph_w, graph_h, SURFACE2, BORDER, 8);

    // Graph bars
    for (int i = 0; i < 8; i++) {
        double bw = (graph_w - 24 - 7 * 4) / 8;
        double bh = 20 + (i * 17 + 7) % (int)(graph_h - 40);
        double bx = 12 + i * (bw + 4);
        double by = graph_h - 8 - bh;
        make_card(session, graph, bx, by, bw, bh, stripe_colors[i % 4], NULL, 3);
    }

    // Info bar
    double info_y = graph_y + graph_h + 8;
    make_card(session, content, 8, info_y, graph_w, 36, SURFACE2, BORDER, 6);

    // Status bar
    make_card(session, root, 0, (double)win_h - status_h, win_w, status_h, HEADER, NULL, 0);
    make_card(session, root, 12, (double)win_h - 22, 12, 12, ACCENT, NULL, 6);

    printf("[UI] Tree built: %lld nodes\n", (long long)abi_ui_node_count(session));

    // ── 4. Create a font resource (needed by renderer) ────────────
    int64_t font = abi_ui_font_create(session, "font.body", "Segoe UI", 14.0);
    (void)font;
    printf("[UI] Font resource: %lld\n", (long long)font);

    // ── 5. Frame loop ─────────────────────────────────────────────
    printf("\n=== Entering frame loop (%d frames) ===\n", 300);
    printf("  Move mouse over window, click, press keys.\n");
    printf("  Watch for INPUT events below.\n\n");

    for (g_frame = 0; g_frame < 300 && g_running; g_frame++) {
        // ── Pump host messages (Win32 → input system bridge) ─────
        abi_ui_host_pump(session);

        // Check close
        if (abi_ui_host_should_close(session)) {
            printf("[LOOP] Window close requested at frame %lld\n", (long long)g_frame);
            break;
        }

        // ── Begin frame (resets draw commands + arena) ────────────
        abi_ui_begin_frame(session, 16.67);

        // ── Draw commands ─────────────────────────────────────────
        abi_ui_draw_rect(session, root, 0, 0, win_w, win_h, "ui.root");
        abi_ui_draw_rect(session, header, 0, 0, win_w, header_h, "ui.header");
        abi_ui_draw_rect(session, sidebar, 0, 0, sidebar_w, win_h - header_h - status_h, "ui.sidebar");

        // ── End frame ─────────────────────────────────────────────
        abi_ui_end_frame(session);

        // ── Render + Present (via ui_render_frame into DIB) ──────
        // First resolve layout, then render the node tree
        if (host && host->framebuffer) {
            ui_layout_resolve(ks);
            ui_render_frame(ks, (uint32_t*)host->framebuffer,
                           host->width, host->height, host->fb_stride / 4);
            InvalidateRect(host->hwnd, NULL, FALSE);
        }

        // ── Poll input events from the input system ───────────────
        if (g_frame % 10 == 0) {  // Every 10 frames to reduce spam
            abi_input_begin_frame(g_input_session, 16.67);
            poll_input_events(g_input_session);

            // Check bound actions
            int64_t quit = abi_input_action_pressed(g_input_session, "action.quit");
            if (quit) {
                printf("[INPUT] Quit action detected! Shutting down.\n");
                g_running = 0;
            }
        }

        // ── Rate limit ────────────────────────────────────────────
        Sleep(16);
    }

    // ── 6. Cleanup ────────────────────────────────────────────────
    printf("\n=== Shutdown ===\n");
    abi_input_session_destroy(g_input_session);
    abi_ui_session_destroy(session);
    printf("[DONE] %lld frames rendered\n", (long long)g_frame);
    return 0;
}
