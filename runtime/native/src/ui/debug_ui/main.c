// ============================================================================
//  Kain UI System — Standalone Win32 Demo
//  ============================================================================
//  Direct C application using the current UI pipeline:
//    ui_system.c    — session management, node lifecycle, hash tables
//    ui_host_adapter.c — real Win32 window (HWND + DIB framebuffer)
//    ui_renderer.c  — pixel-perfect node tree rendering
//    ui_layout.c    — flexbox-style layout engine
//    ui_color.c     — color parsing (#hex, rgba, named)
//    input_system.c — universal input event bridge
//  ============================================================================
//  Compile:
//    clang main.c stubs.c ^
//      ..\ui_system.c ..\ui_host_adapter.c ..\ui_renderer.c ..\ui_layout.c ..\ui_color.c ^
//      ..\..\core\input_system.c ^
//      -I ..\..\..\include -I .. -I ..\..\core ^
//      -luser32 -lgdi32 -lopengl32 -o KainUIDemo.exe
//  ============================================================================

#include "ui_system.h"
#include "ui_host_adapter.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ── Stubs ─────────────────────────────────────────────────────────────────
char* string_new(char* src);
double kain_clampd(double value, double min_value, double max_value);

// ── Color palette (dark theme) ────────────────────────────────────────────
#define C_BG        "#1A1A24"   // Window background
#define C_SURFACE   "#252540"   // Card/surface background
#define C_SURFACE2  "#2E2E48"   // Elevated surface
#define C_HEADER    "#1E1E32"   // Header bar
#define C_SIDEBAR   "#202038"   // Sidebar
#define C_BORDER    "#3A3A5C"   // Subtle borders
#define C_ACCENT    "#21D4A1"   // Primary accent (green)
#define C_ACCENT2   "#4A90D9"   // Secondary accent (blue)
#define C_ACCENT3   "#E8914A"   // Warning accent (orange)
#define C_ACCENT4   "#E84A5F"   // Danger accent (red)
#define C_TEXT      "#E8E8F0"   // Primary text
#define C_TEXT_DIM  "#8888A0"   // Dim text
#define C_HIGHLIGHT "#21D4A122" // Highlight overlay (rgba)
#define C_DIVIDER   "#3A3A5C"   // Dividers

// ── Layout helpers ────────────────────────────────────────────────────────
static int64_t create_node(int64_t session, int64_t parent, const char* kind,
                           double x, double y, double w, double h)
{
    int64_t n = abi_ui_node_create(session, kind && kind[0] ? kind : "demo.node");
    if (parent > 0) abi_ui_node_set_parent(session, n, parent);
    abi_ui_node_set_rect(session, n, x, y, w, h);
    return n;
}

static void set_fill(int64_t session, int64_t node, const char* color) {
    if (color) abi_ui_node_set_style_string(session, node, "fill_color", color);
}

static void set_border(int64_t session, int64_t node, const char* color, double width) {
    if (color) abi_ui_node_set_style_string(session, node, "border_color", color);
    if (width > 0.0) abi_ui_node_set_style_f64(session, node, "border_width", width);
}

static void set_radius(int64_t session, int64_t node, double r) {
    if (r > 0.0) abi_ui_node_set_style_f64(session, node, "corner_radius", r);
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
                         const char* text)
{
    int64_t card = create_node(session, parent, "demo.card", x, y, w, h);
    set_fill(session, card, fill ? fill : C_SURFACE);
    set_border(session, card, border ? border : C_BORDER, 1.0);
    set_radius(session, card, 10.0);
    set_text(session, card, text ? text : "");
    return card;
}

// ── Main ──────────────────────────────────────────────────────────────────
int main(void) {
    int64_t session;
    int64_t win_w = 1280;
    int64_t win_h = 720;

    printf("Kain UI System — Standalone Demo\n");
    printf("Creating session...\n");

    // ── Initialize UI system ───────────────────────────────────────
    if (abi_ui_reset() != ABI_UI_OK) {
        fprintf(stderr, "FAIL: abi_ui_reset\n");
        return 1;
    }

    session = abi_ui_session_create("KainUIDemo", win_w, win_h);
    if (session <= 0) {
        fprintf(stderr, "FAIL: abi_ui_session_create\n");
        return 1;
    }
    printf("Session %lld created (%dx%d)\n", (long long)session, (int)win_w, (int)win_h);

    if (abi_ui_window_open(session, "Kain UI System — Standalone Demo", win_w, win_h) != ABI_UI_OK) {
        fprintf(stderr, "FAIL: abi_ui_window_open\n");
        return 1;
    }

    // ── Attach Win32 host adapter (creates REAL HWND window) ───────
    printf("Attaching win32 host adapter...\n");
    int64_t attach_status = abi_ui_host_attach(session, "winit");
    if (attach_status != ABI_UI_OK) {
        fprintf(stderr, "FAIL: abi_ui_host_attach (status=%lld)\n", (long long)attach_status);
        return 1;
    }
    printf("Window created. Host backend: %s\n", abi_ui_host_backend(session));

    // ── Build UI node tree ─────────────────────────────────────────
    printf("Building UI tree...\n");

    // Root node — fills entire window
    int64_t root = create_node(session, 0, "demo.root", 0, 0, win_w, win_h);
    // No fill_color on root — the renderer clears to dark by default

    // ════════════════════════════════════════════════════════════════
    //  HEADER BAR (60px tall, spans full width)
    // ════════════════════════════════════════════════════════════════
    int64_t header = make_card(session, root, 0, 0, win_w, 60, C_HEADER, NULL, NULL);
    set_radius(session, header, 0.0);

    // Status dot — real-time visual indicator (green circle)
    int64_t status_dot = make_card(session, header, 16, 18, 24, 24, C_ACCENT, NULL, NULL);
    set_radius(session, status_dot, 12.0);

    // Title text node (we set text, but rendering is deferred)
    int64_t title_text = create_node(session, header, "demo.title", 52, 14, 400, 32);
    set_text(session, title_text, "Kain Native UI System — Standalone Demo");

    // Accent line under header — thin bright bar
    int64_t accent_line = make_card(session, root, 0, 58, win_w, 2, C_ACCENT, NULL, NULL);
    set_radius(session, accent_line, 0.0);

    // ════════════════════════════════════════════════════════════════
    //  CONTENT AREA (fills between header + status bar)
    // ════════════════════════════════════════════════════════════════
    int64_t content_y = 62;
    int64_t content_h = win_h - 62 - 32;  // minus status bar height
    int64_t content = make_card(session, root, 0, content_y, win_w, content_h,
                                C_BG, NULL, NULL);
    set_radius(session, content, 0.0);

    // ── Sidebar (220px wide, full height) ──────────────────────────
    int64_t sidebar = make_card(session, content, 0, 0, 220, content_h, C_SIDEBAR, NULL, NULL);
    set_radius(session, sidebar, 0.0);

    // Sidebar title area
    int64_t sidebar_title = make_card(session, sidebar, 0, 0, 220, 48, C_SIDEBAR, C_DIVIDER, NULL);
    set_radius(session, sidebar_title, 0.0);
    set_text(session, sidebar_title, "N A V I G A T I O N");

    // Sidebar accent bar (thin green line under title)
    int64_t sidebar_accent = make_card(session, sidebar, 16, 44, 36, 2, C_ACCENT, NULL, NULL);
    set_radius(session, sidebar_accent, 0.0);

    // Sidebar menu items
    const char* menu_items[] = {
        "Dashboard", "Analytics", "Explorer", "Settings", "Help"
    };
    const char* menu_colors[] = {
        C_ACCENT, C_ACCENT2, C_ACCENT3, C_TEXT_DIM, C_TEXT_DIM
    };
    for (int i = 0; i < 5; i++) {
        int64_t mi_y = 58 + i * 44;
        int64_t item = make_card(session, sidebar, 8, mi_y, 204, 38,
                                 i == 0 ? C_HIGHLIGHT : C_SIDEBAR,
                                 i == 0 ? C_ACCENT : C_SIDEBAR, NULL);
        set_radius(session, item, 6.0);

        // Menu item indicator dot
        int64_t dot = make_card(session, item, 12, 11, 8, 8,
                                menu_colors[i], NULL, NULL);
        set_radius(session, dot, 4.0);

        set_text(session, item, menu_items[i]);
    }

    // ── Main panel (fills remaining space right of sidebar) ────────
    int64_t main_x = 228;
    int64_t main_w = win_w - main_x - 8;
    int64_t main_panel = make_card(session, content, main_x, 0, main_w, content_h,
                                   C_BG, NULL, NULL);
    set_radius(session, main_panel, 0.0);

    // ── Status cards row (4 cards, 280px each with gap) ────────────
    int64_t cards_y = 8;
    int64_t card_w = (main_w - 40) / 4;  // 4 cards with 3 gaps of 8px + margins
    int64_t card_h = 100;

    struct {
        const char* title;
        const char* value;
        const char* color;
        const char* desc;
    } card_data[] = {
        {"Sessions", "16",  C_ACCENT,  "Active UI sessions"},
        {"Nodes",    "4096", C_ACCENT2, "Max nodes per session"},
        {"Styles",   "8192", C_ACCENT3, "Style record capacity"},
        {"Events",   "1024", C_ACCENT4, "Event ring buffer"},
    };

    for (int i = 0; i < 4; i++) {
        int64_t cx = 8 + i * (card_w + 8);
        int64_t card = make_card(session, main_panel, cx, cards_y, card_w, card_h,
                                 C_SURFACE, C_BORDER, NULL);
        set_radius(session, card, 10.0);

        // Accent stripe at top of card
        int64_t stripe = make_card(session, card, 0, 0, card_w, 3,
                                   card_data[i].color, NULL, NULL);
        set_radius(session, stripe, 0.0);

        // Value indicator
        int64_t value_node = make_card(session, card, 12, 14, card_w - 24, 32,
                                       C_SURFACE, NULL, NULL);
        set_radius(session, value_node, 4.0);
        set_text(session, value_node, card_data[i].value);

        // Title text
        set_text(session, card, card_data[i].title);
    }

    // ── Section title — "System Activity" ──────────────────────────
    int64_t section_y = cards_y + card_h + 12;
    int64_t section_label = make_card(session, main_panel, 8, section_y, 240, 24,
                                      C_BG, NULL, NULL);
    set_text(session, section_label, "SYSTEM ACTIVITY");

    // Thin divider line
    int64_t divider_y = section_y + 30;
    int64_t divider = make_card(session, main_panel, 8, divider_y, main_w - 16, 1,
                                C_DIVIDER, NULL, NULL);
    set_radius(session, divider, 0.0);

    // ── Activity visualization — colored bars (placeholder graph) ──
    int64_t graph_y = divider_y + 12;
    int64_t graph_h = 180;
    int64_t graph_w = main_w - 16;
    int64_t graph = make_card(session, main_panel, 8, graph_y, graph_w, graph_h,
                              C_SURFACE, C_BORDER, NULL);
    set_radius(session, graph, 8.0);

    // Colored vertical bars inside the graph area (simulating a chart)
    const char* bar_colors[] = { C_ACCENT, C_ACCENT2, C_ACCENT3, C_ACCENT4,
                                 C_ACCENT, C_ACCENT2, C_ACCENT, C_ACCENT3 };
    int bar_count = 8;
    int bar_w = (graph_w - 24 - (bar_count - 1) * 4) / bar_count;
    if (bar_w < 4) bar_w = 4;
    int bar_gap = 4;
    int bars_start_x = 12;
    int bar_base_h = 20;
    int bar_max_h = graph_h - 24;

    for (int i = 0; i < bar_count; i++) {
        int bh = bar_base_h + (i * 17 + 7) % (bar_max_h - bar_base_h);
        int bx = bars_start_x + i * (bar_w + bar_gap);
        int by = graph_h - 8 - bh;

        int64_t bar = make_card(session, graph, bx, by, bar_w, bh,
                                bar_colors[i], NULL, NULL);
        set_radius(session, bar, 3.0);
        set_opacity(session, bar, 0.7 + (i % 3) * 0.15);
    }

    // ── Bottom section — info row ──────────────────────────────────
    int64_t info_y = graph_y + graph_h + 12;
    int64_t info_row = make_card(session, main_panel, 8, info_y, main_w - 16, 40,
                                 C_SURFACE2, C_BORDER, NULL);
    set_radius(session, info_row, 6.0);

    // Info items
    set_text(session, info_row,
             "1280x720  |  Z3-Verified  |  UI System C Pipeline  |  Standalone");

    // ════════════════════════════════════════════════════════════════
    //  STATUS BAR (32px tall, bottom of window)
    // ════════════════════════════════════════════════════════════════
    int64_t status_y = win_h - 32;
    int64_t status_bar = make_card(session, root, 0, status_y, win_w, 32,
                                   C_HEADER, NULL, NULL);
    set_radius(session, status_bar, 0.0);

    // Status indicator
    int64_t status_dot2 = make_card(session, status_bar, 12, 10, 12, 12,
                                    C_ACCENT, NULL, NULL);
    set_radius(session, status_dot2, 6.0);
    set_opacity(session, status_dot2, 0.8);

    // Status text
    set_text(session, status_bar,
             "Running  |  Frame 0  |  Kain Native UI System  |  Win32 DIB Framebuffer");

    printf("UI tree built. %lld nodes.\n", (long long)abi_ui_node_count(session));

    // ── Frame loop ─────────────────────────────────────────────────
    printf("Entering frame loop. Close the window to exit.\n");
    printf("============================================================\n");

    int64_t frame_index = 0;
    int running = 1;

    while (running) {
        // 1. Pump host messages (Win32 message queue)
        abi_ui_host_pump(session);

        // 2. Check if window was closed
        if (abi_ui_host_should_close(session)) {
            printf("Window close requested.\n");
            running = 0;
            break;
        }

        // 3. Begin frame (resets draw command count + frame arena)
        abi_ui_begin_frame(session, 16.67);

        // 4. (Optional) add draw commands here for per-frame effects
        //    For static UI, the node tree renderer handles everything.

        // 5. End frame
        int64_t draw_count = abi_ui_end_frame(session);

        // 6. Host present — renders node tree to DIB framebuffer
        //    Internally calls: ui_layout_resolve → ui_render_frame → InvalidateRect
        abi_ui_host_present(session);

        // 7. Rate-limit to ~60fps
        //    In a real app this would use proper vsync/timing
        Sleep(16);

        frame_index++;
        if (frame_index % 60 == 0) {
            printf("Frame %lld | nodes=%lld\n",
                   (long long)frame_index,
                   (long long)abi_ui_node_count(session));
        }
    }

    // ── Cleanup ─────────────────────────────────────────────────────
    printf("Shutting down...\n");
    abi_ui_session_destroy(session);

    printf("Done. %lld frames rendered.\n", (long long)frame_index);
    return 0;
}
