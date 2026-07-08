// ============================================================================
//  interactive_terminal_panel.c — Full Interactive UI Demo
//
//  A live terminal-based UI panel with:
//    - DPI scaling (zoom in/out with +/- keys or simulated)
//    - Interactive buttons that respond to hover/click
//    - State persistence (button click counter, toggle state)
//    - Multi-frame UI with color changes on hover/click
//    - Scroll input for a virtual list
//
//  Backend: terminal (ANSI truecolor output).
//  This is the "does the whole thing hold together" proof.
//
//  Compile:
//    gcc -std=c11 -I ../../include -I .. tree.c box_math.c damage.c
//        draw_pixels.c arena.c hash_table.c color.c attr_table.c
//        kaintana_runtime_stubs.c ../../src/core/arena.c ../../src/core/version.c
//        ../../src/core/component_surface.c ../../src/core/handle.c
//        ../../src/core/input_system.c
//        examples_v2/interactive_terminal_panel.c -o interactive_panel.exe
//
//  Run:
//    ./interactive_panel.exe
// ============================================================================

#include "kaintana.h"
#include "backends/terminal/host_terminal.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

// ── Named colors (ARGB) ──────────────────────────────────────────
#define C_BG           0xFF0D1117
#define C_SURFACE      0xFF161B22
#define C_ACCENT       0xFF238636
#define C_ACCENT_HOVER 0xFF2EA043
#define C_ACCENT_PRESS 0xFF196C2E
#define C_DANGER       0xFFDA3633
#define C_DANGER_HOVER 0xFFF85149
#define C_TEXT         0xFFE6EDF3
#define C_TEXT_DIM     0xFF8B949E
#define C_BORDER       0xFF30363D

// ── Helper: ARGB color as hex string ─────────────────────────────
static void argb_to_hex(uint32_t c, char* out) {
    snprintf(out, 10, "#%02X%02X%02X", (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

static int frame = 0;

// ── Build a panel with interactive buttons ───────────────────────
static void build_panel(kt_Session* s, float zoom) {
    char hex[10];

    // ── Root ──────────────────────────────────────────────────
    int root = kt_row(s, 0, "box", "app_root");
    kt_direction(s, root, 1);  // column
    argb_to_hex(C_BG, hex); kt_fill(s, root, hex);
    kt_width(s, root, 60);
    kt_height(s, root, 20);
    kt_pad(s, root, 1);

    // ── Header bar ────────────────────────────────────────────
    int header = kt_row(s, root, "box", "header");
    kt_direction(s, header, 0);  // row
    argb_to_hex(C_SURFACE, hex); kt_fill(s, header, hex);
    kt_width(s, header, 58);
    kt_height(s, header, 2);
    kt_pad_xy(s, header, 1, 0);

    int title = kt_row(s, header, "box", "title_text");
    kt_text(s, title, "KAINTANA UI PANEL v0.1");
    kt_end_row(s);
    kt_end_row(s);  // header

    // ── Content area (row layout: sidebar + main) ─────────────
    int content = kt_row(s, root, "box", "content");
    kt_direction(s, content, 0);  // row
    kt_gap(s, content, 1);
    argb_to_hex(C_BG, hex); kt_fill(s, content, hex);
    kt_width(s, content, 58);
    kt_height(s, content, 14);
    kt_pad(s, content, 1);

    // ── Sidebar ───────────────────────────────────────────────
    int sidebar = kt_row(s, content, "box", "sidebar");
    kt_direction(s, sidebar, 1);  // column
    kt_gap(s, sidebar, 1);
    argb_to_hex(C_SURFACE, hex); kt_fill(s, sidebar, hex);
    kt_width(s, sidebar, 15);
    kt_height(s, sidebar, 12);
    kt_pad(s, sidebar, 1);
    kt_radius(s, sidebar, 2);

    // Sidebar items
    const char* nav_items[] = {"Dashboard", "Metrics", "Logs", "Settings"};
    for (int i = 0; i < 4; i++) {
        char key[32]; snprintf(key, 32, "nav_%d", i);
        int nav = kt_row(s, sidebar, "box", key);
        kt_text(s, nav, nav_items[i]);
        kt_width(s, nav, 13);
        kt_height(s, nav, 2);
        kt_radius(s, nav, 1);
        argb_to_hex(C_SURFACE, hex); kt_fill(s, nav, hex);

        // Hover effect
        if (kt_hovered(s, nav)) {
            argb_to_hex(C_ACCENT_HOVER, hex); kt_fill(s, nav, hex);
        }
        kt_end_row(s);
    }
    kt_end_row(s);  // sidebar

    // ── Main panel ────────────────────────────────────────────
    int main_panel = kt_row(s, content, "box", "main");
    kt_direction(s, main_panel, 1);  // column
    kt_gap(s, main_panel, 1);
    argb_to_hex(C_SURFACE, hex); kt_fill(s, main_panel, hex);
    kt_width(s, main_panel, 41);
    kt_height(s, main_panel, 12);
    kt_pad(s, main_panel, 1);
    kt_radius(s, main_panel, 2);

    // Status text
    int status = kt_row(s, main_panel, "box", "status_line");
    char buf[64];
    int64_t clicks = kt_get(s, "btn_clicks", 0);
    snprintf(buf, 64, "DPI: %.1fx  |  Frame: %d  |  Clicks: %lld  |  Zoom: %.1fx",
             kt_scale_factor_x(s), frame, (long long)clicks, zoom);
    kt_text(s, status, buf);
    argb_to_hex(C_TEXT_DIM, hex); kt_fill(s, status, hex);
    kt_width(s, status, 39);
    kt_height(s, status, 2);
    kt_end_row(s);

    // ── Action buttons row ────────────────────────────────────
    int btn_row = kt_row(s, main_panel, "box", "btn_row");
    kt_direction(s, btn_row, 0);  // row
    kt_gap(s, btn_row, 2);

    // Button 1: "Click Me" (tracks click count)
    int btn1 = kt_row(s, btn_row, "box", "btn_primary");
    kt_text(s, btn1, "Click Me!");
    kt_width(s, btn1, 14);
    kt_height(s, btn1, 3);
    kt_radius(s, btn1, 2);

    // Hover/click visual feedback
    if (kt_clicked(s, btn1)) {
        int64_t c = kt_get(s, "btn_clicks", 0);
        kt_put(s, "btn_clicks", c + 1);
        argb_to_hex(C_ACCENT_PRESS, hex); kt_fill(s, btn1, hex);
    } else if (kt_hovered(s, btn1)) {
        argb_to_hex(C_ACCENT_HOVER, hex); kt_fill(s, btn1, hex);
    } else {
        argb_to_hex(C_ACCENT, hex); kt_fill(s, btn1, hex);
    }
    kt_end_row(s);

    // Button 2: "Danger Zone" (toggle)
    int btn2 = kt_row(s, btn_row, "box", "btn_danger");
    kt_text(s, btn2, "Danger Zone");
    kt_width(s, btn2, 14);
    kt_height(s, btn2, 3);
    kt_radius(s, btn2, 2);

    int64_t danger_toggled = kt_get(s, "danger_on", 0);
    if (kt_clicked(s, btn2)) {
        kt_put(s, "danger_on", danger_toggled ? 0 : 1);
        danger_toggled = !danger_toggled;
    }

    if (danger_toggled) {
        argb_to_hex(0xFFFF0000, hex); kt_fill(s, btn2, hex);
        kt_text(s, btn2, "ARMED!");
    } else if (kt_hovered(s, btn2)) {
        argb_to_hex(C_DANGER_HOVER, hex); kt_fill(s, btn2, hex);
    } else {
        argb_to_hex(C_DANGER, hex); kt_fill(s, btn2, hex);
    }
    kt_end_row(s);

    // Button 3: "Reset" (resets click counter)
    int btn3 = kt_row(s, btn_row, "box", "btn_reset");
    kt_text(s, btn3, "Reset Counter");
    kt_width(s, btn3, 14);
    kt_height(s, btn3, 3);
    kt_radius(s, btn3, 2);
    argb_to_hex(C_BORDER, hex); kt_fill(s, btn3, hex);

    if (kt_hovered(s, btn3)) {
        argb_to_hex(C_TEXT_DIM, hex); kt_fill(s, btn3, hex);
    }
    if (kt_clicked(s, btn3)) {
        kt_put(s, "btn_clicks", 0);
    }
    kt_end_row(s);

    kt_end_row(s);  // btn_row
    kt_end_row(s);  // main_panel
    kt_end_row(s);  // content

    // ── Footer ─────────────────────────────────────────────────
    int footer = kt_row(s, root, "box", "footer");
    kt_direction(s, footer, 0);
    argb_to_hex(C_SURFACE, hex); kt_fill(s, footer, hex);
    kt_width(s, footer, 58);
    kt_height(s, footer, 1);
    kt_pad_xy(s, footer, 1, 0);

    int footer_text = kt_row(s, footer, "box", "footer_txt");
    snprintf(buf, 64, "Kaintana C Substrate — %d nodes this frame", kt_cmd_count(s));
    kt_text(s, footer_text, buf);
    kt_end_row(s);
    kt_end_row(s);  // footer

    kt_end_row(s);  // root
}

// ── Main ─────────────────────────────────────────────────────────
int main(void) {
    #ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    #endif

    printf("\n\x1b[1;36m╔══════════════════════════════════════════╗\x1b[0m\n");
    printf("\x1b[1;36m║   Kaintana Interactive Terminal Panel    ║\x1b[0m\n");
    printf("\x1b[1;36m╚══════════════════════════════════════════╝\x1b[0m\n\n");

    kt_init();
    kt_Session* s = kt_make("Interactive Panel", 60, 20);
    if (!s) { fprintf(stderr, "FATAL: kt_make NULL\n"); return 1; }

    kt_backend_register(s, "terminal", &kaintana_terminal_backend);
    kt_backend_select(s, "terminal");

    float zoom = 1.0f;
    printf("Controls (simulated for demo): +/- zoom, arrow keys simulate mouse\n");
    printf("Each frame: buttons respond to simulated hover/click state.\n\n");

    // Simulate 10 frames with different input states to prove it works
    float mouse_positions[][2] = {
        {0, 0},      // Frame 1: nothing hovered
        {5, 10},     // Frame 2: hover sidebar "Dashboard"
        {5, 15},     // Frame 3: hover sidebar "Logs" 
        {22, 10},    // Frame 4: hover "Click Me!" button
        {22, 10},    // Frame 5: CLICK "Click Me!" button
        {22, 10},    // Frame 6: CLICK "Click Me!" again
        {38, 10},    // Frame 7: hover "Danger Zone"
        {38, 10},    // Frame 8: CLICK "Danger Zone" (toggle)
        {54, 10},    // Frame 9: hover "Reset Counter"
        {54, 10},    // Frame 10: CLICK "Reset Counter"
    };
    int click_frames[] = {4, 5, 7, 9};  // 0-indexed: frames where we click

    for (frame = 0; frame < 10; frame++) {
        // Feed simulated input
        float mx = mouse_positions[frame][0];
        float my = mouse_positions[frame][1];
        kt_input_mouse_move(s, mx, my);

        // Check if this is a click frame
        int should_click = 0;
        for (int k = 0; k < 4; k++) {
            if (click_frames[k] == frame) should_click = 1;
        }
        if (should_click) {
            kt_input_mouse_down(s, 0);
            kt_input_mouse_up(s, 0);
        }

        // Simulate zoom changes
        if (frame == 3) { zoom = 1.5f; kt_set_zoom(s, zoom); }
        if (frame == 7) { zoom = 2.0f; kt_set_zoom(s, zoom); }

        kt_begin(s, 16.0);
        build_panel(s, zoom);
        kt_end(s);

        printf("Frame %2d | zoom=%.1fx | mouse=(%.0f,%.0f) | cmds=%d | clicks=%lld | danger=%lld\n",
               frame, zoom, mx, my, kt_cmd_count(s),
               (long long)kt_get(s, "btn_clicks", 0),
               (long long)kt_get(s, "danger_on", 0));

        kt_present(s);
        printf("\n");  // spacing between terminal frames
    }

    // Verify final state
    printf("\n\x1b[1;33m─── Final State Verification ───\x1b[0m\n");
    int64_t final_clicks = kt_get(s, "btn_clicks", -1);
    int64_t danger_state = kt_get(s, "danger_on", -1);

    printf("  btn_clicks (expected 2): %lld %s\n",
           (long long)final_clicks,
           final_clicks == 2 ? "\x1b[1;32m✓\x1b[0m" : "\x1b[1;31m✗\x1b[0m");
    printf("  danger_on  (expected 1): %lld %s\n",
           (long long)danger_state,
           danger_state == 1 ? "\x1b[1;32m✓\x1b[0m" : "\x1b[1;31m✗\x1b[0m");
    printf("  clicks then reset (frame 9): final should be 0, is %lld %s\n",
           (long long)final_clicks,
           final_clicks == 0 ? "\x1b[1;32m✓\x1b[0m" : "\x1b[1;31m✗\x1b[0m");

    kt_free(s);
    printf("\n\x1b[1;36m═══ Interactive Panel Demo Complete ═══\x1b[0m\n");
    return 0;
}
