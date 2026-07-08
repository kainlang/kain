// ============================================================================
//  demo_settings_panel.c — Complex Settings/Preferences Dialog
//  (Kaintana Win32 GDI)
//
//  A tabbed settings dialog with 4 tabs: General, Display, Audio, Advanced.
//  Each tab contains different controls rendered as colored rectangles.
//  Tab cycles each frame (frame_number % 4).
//
//  Pattern: same as demo_minecraft_ui.c — includes backend .c files directly.
//
//  Compile (from runtime/native/src/ui_v2/):
//    python build.py examples/demo_settings_panel.c --backend win32 --run
//
//  Compile (from runtime/native/src/ui_v2/):
//    python build.py examples/demo_settings_panel.c --backend win32 --run
//
//  Or manually:
//    gcc -std=c11 -I . -I ../../include -o examples/demo_settings_panel.exe
//        examples/demo_settings_panel.c
//        tree.c box_math.c damage.c draw_pixels.c arena.c hash_table.c
//        color.c attr_table.c kaintana_runtime_stubs.c
//        ../../src/core/arena.c ../../src/core/version.c
//        ../../src/core/component_surface.c ../../src/core/handle.c
//        ../../src/core/input_system.c
//        -lgdi32 -lws2_32 -lopengl32
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <conio.h>

#include "kaintana.h"

// ═══════════════════════════════════════════════════════════════════════════════
//  BACKEND: include the Win32 GDI backend .c files directly
// ═══════════════════════════════════════════════════════════════════════════════
#include "backends/win32/host_win32.c"
#include "backends/win32/render_gdi.c"

// ═══════════════════════════════════════════════════════════════════════════════
//  CONSTANTS
// ═══════════════════════════════════════════════════════════════════════════════
#define WIN_W           1024
#define WIN_H           768
#define FRAMES          20
#define NUM_TABS        4

// ── Color scheme (ARGB) ─────────────────────────────────────────────────
#define C_BG            0xFF1E1E2E
#define C_PANEL_BG      0xFF2D2D44
#define C_TITLE_BG      0xFF1A1A2E
#define C_TAB_ACTIVE    0xFF3D5A80
#define C_TAB_INACTIVE  0xFF2D2D44
#define C_CONTROL_BG    0xFF3D3D5C
#define C_CONTROL_HI    0xFF4A4A6A
#define C_SLIDER_FILL   0xFF58A6FF
#define C_SLIDER_BG     0xFF303050
#define C_TOGGLE_ON     0xFF3FB950
#define C_TOGGLE_OFF    0xFF484860
#define C_TEXT_INPUT    0xFF3D3D5C
#define C_DROPDOWN      0xFF4A4A6A
#define C_SELECTED      0xFF1F6FEB
#define C_BTN_DANGER    0xFFF85149
#define C_BTN_SAVE      0xFF238636
#define C_BORDER        0xFF3C3C5C
#define C_STATUS_BG     0xFF1A1A2E
#define C_WHITE         0xFFFFFFFF

// ═══════════════════════════════════════════════════════════════════════════════
//  HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

static void sleep_ms(DWORD ms) { Sleep(ms); }

static void fill(int x, int y, int w, int h, uint32_t c) {
    win32_fb_fill_rect(x, y, x + w, y + h, c);
}

static void fill_rnd(int x, int y, int w, int h, float r, uint32_t c) {
    win32_fb_fill_rounded_rect(x, y, x + w, y + h, r, c);
}

// ── Toggle switch ────────────────────────────────────────────────────────
static void draw_toggle(int x, int y, int on) {
    uint32_t bg = on ? C_TOGGLE_ON : C_TOGGLE_OFF;
    fill_rnd(x, y, 48, 24, 12, bg);
    fill_rnd(on ? x + 26 : x + 4, y + 3, 18, 18, 9, C_WHITE);
}

// ── Slider ───────────────────────────────────────────────────────────────
static void draw_slider(int x, int y, int w, int val, int max_val) {
    fill_rnd(x, y, w, 8, 4, C_SLIDER_BG);
    int fill_w = (val * (w - 4)) / max_val;
    if (fill_w < 4) fill_w = 4;
    fill_rnd(x + 2, y, fill_w, 8, 4, C_SLIDER_FILL);
}

// ── Option selector ──────────────────────────────────────────────────────
static void draw_selector(int x, int y, int w, int num_opts, int sel) {
    int opt_w = (w - (num_opts - 1) * 4) / num_opts;
    if (opt_w < 20) opt_w = 20;
    for (int i = 0; i < num_opts; i++) {
        uint32_t c = (i == sel) ? C_SELECTED : C_CONTROL_BG;
        fill_rnd(x + i * (opt_w + 4), y, opt_w, 28, 4, c);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  TAB CONTENT RENDERERS
// ═══════════════════════════════════════════════════════════════════════════════

static void render_general_tab(int x, int y, int w) {
    // Section header
    fill_rnd(x, y, w, 28, 4, C_CONTROL_BG);
    y += 40;

    int row_h = 32;
    int label_w = 140;

    // Username field
    fill_rnd(x + label_w, y, 200, 28, 4, C_TEXT_INPUT);
    y += row_h + 4;

    // Language dropdown
    fill_rnd(x + label_w, y, 180, 28, 4, C_DROPDOWN);
    y += row_h + 4;

    // Theme selector
    int opt_w = (240 - 8) / 3;
    fill_rnd(x + label_w, y, opt_w, 28, 4, 0xFF161B22);
    fill_rnd(x + label_w + opt_w + 4, y, opt_w, 28, 4, 0xFFE8E8E8);
    fill_rnd(x + label_w + 2 * (opt_w + 4), y, opt_w, 28, 4, C_TAB_ACTIVE);
    // Stroke on selected (Dark theme)
    win32_fb_fill_rounded_rect(x + label_w, y, x + label_w + opt_w, y + 28, 4, C_SELECTED);

    y += row_h + 8;

    // Auto-save toggle
    draw_toggle(x + label_w, y + 2, 1);
}

static void render_display_tab(int x, int y, int w) {
    fill_rnd(x, y, w, 28, 4, C_CONTROL_BG);
    y += 40;

    int label_w = 140;
    int row_h = 32;

    // Resolution dropdown
    fill_rnd(x + label_w, y, 200, 28, 4, C_DROPDOWN);
    y += row_h + 4;

    // Brightness slider
    draw_slider(x + label_w + 60, y + 10, 160, 7, 10);
    y += row_h + 4;

    // Fullscreen toggle
    draw_toggle(x + label_w, y + 2, 0);
    y += row_h + 8;

    // DPI scale selector
    draw_selector(x + label_w, y, 220, 3, 1);
}

static void render_audio_tab(int x, int y, int w) {
    fill_rnd(x, y, w, 28, 4, C_CONTROL_BG);
    y += 40;

    int label_w = 140;
    int row_h = 32;

    // Volume slider
    draw_slider(x + label_w + 60, y + 10, 160, 8, 10);
    y += row_h + 4;

    // Mute toggle
    draw_toggle(x + label_w, y + 2, 0);
    y += row_h + 4;

    // Output device
    fill_rnd(x + label_w, y, 220, 28, 4, C_DROPDOWN);
    y += row_h + 4;

    // Sample rate selector
    draw_selector(x + label_w, y, 220, 3, 0);
}

static void render_advanced_tab(int x, int y, int w) {
    fill_rnd(x, y, w, 28, 4, C_CONTROL_BG);
    y += 40;

    int label_w = 140;
    int row_h = 32;

    // Logging level selector
    draw_selector(x + label_w, y, 300, 4, 1);
    y += row_h + 4;

    // Cache size slider
    draw_slider(x + label_w + 60, y + 10, 160, 5, 10);
    y += row_h + 4;

    // HW Acceleration toggle
    draw_toggle(x + label_w, y + 2, 1);
    y += row_h + 16;

    // Reset button
    fill_rnd(x + label_w, y, 180, 36, 6, C_BTN_DANGER);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  SETTINGS UI RENDER
// ═══════════════════════════════════════════════════════════════════════════════

static const char* g_tab_names[] = {"General", "Display", "Audio", "Advanced"};

static void render_settings(int active_tab) {
    int w = WIN_W, h = WIN_H;

    // ── Background ───────────────────────────────────────────────────────
    fill(0, 0, w, h, C_BG);

    // ── Title bar ────────────────────────────────────────────────────────
    fill(0, 0, w, 40, C_TITLE_BG);
    // Close button
    fill_rnd(w - 40, 8, 32, 28, 4, C_CONTROL_BG);

    // ── Tab bar ──────────────────────────────────────────────────────────
    int tab_y = 44;
    fill(0, tab_y, w, 36, C_BG);

    int tab_w = 120;
    int tab_gap = 4;
    int tabs_start = 12;
    for (int i = 0; i < NUM_TABS; i++) {
        uint32_t tc = (i == active_tab) ? C_TAB_ACTIVE : C_TAB_INACTIVE;
        fill_rnd(tabs_start + i * (tab_w + tab_gap), tab_y + 4, tab_w, 28, 4, tc);
    }

    // ── Content area ────────────────────────────────────────────────────
    int cx = 16, cy = tab_y + 40;
    int cw = w - 32, ch = h - cy - 60;
    fill_rnd(cx, cy, cw, ch, 6, C_PANEL_BG);

    // Render content for active tab (inset by 16px)
    int ix = cx + 16, iy = cy + 12;
    int iw = cw - 32;

    switch (active_tab) {
        case 0: render_general_tab(ix, iy, iw);  break;
        case 1: render_display_tab(ix, iy, iw);  break;
        case 2: render_audio_tab(ix, iy, iw);    break;
        case 3: render_advanced_tab(ix, iy, iw); break;
    }

    // ── Status bar ──────────────────────────────────────────────────────
    int sb_y = h - 28;
    fill(0, sb_y, w, 28, C_STATUS_BG);
    fill_rnd(w - 96, sb_y + 3, 84, 22, 4, C_BTN_SAVE);

    // Bottom border
    fill(0, h - 1, w, 1, C_BORDER);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════════════════════════════

int main(void) {
    // Enable ANSI VT sequences
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    printf("\n");
    printf("\x1b[1;36m=== Kaintana Settings Panel Demo ===\x1b[0m\n");
    printf("Win32 backend \x1b[2m(GDI software rendering, %dx%d)\x1b[0m\n", WIN_W, WIN_H);
    printf("\n");

    kt_init();
    kt_Session* s = kt_make("settings", WIN_W, WIN_H);
    if (!s) { fprintf(stderr, "FAIL: kt_make NULL\n"); return 1; }

    kt_backend_register(s, "win32", &kaintana_win32_backend);
    if (!kt_backend_select(s, "win32")) {
        fprintf(stderr, "FAIL: kt_backend_select\n");
        kt_free(s);
        return 1;
    }

    // kt_backend_select() already calls init() internally.
    printf("Window created. Rendering %d frames with tab cycling...\n\n", FRAMES);

    printf("Window created. Rendering %d frames with tab cycling...\n\n", FRAMES);

    for (int frame = 0; frame < FRAMES; frame++) {
        int active_tab = frame % NUM_TABS;

        kaintana_win32_backend.new_frame();
        kt_begin(s, 16.0);

        render_settings(active_tab);

        kt_end(s);
        kt_present(s);

        int cc = kt_cmd_count(s);
        printf("  Frame %2d/%d  |  \x1b[33m%4d cmds\x1b[0m  |  Tab: %s\n",
               frame + 1, FRAMES, cc, g_tab_names[active_tab]);

        sleep_ms(120);
    }

    printf("\n\x1b[1;32m=== Frame loop complete ===\x1b[0m\n");
    printf("Window stays open. Press \x1b[1mENTER\x1b[0m or click close to exit...\n");

    int keep = 30;
    while (keep > 0) {
        if (_kbhit()) { int ch = _getch(); (void)ch; if (ch == '\r' || ch == '\n') break; }
        int f = (FRAMES - keep);
        kaintana_win32_backend.new_frame();
        kt_begin(s, 16.0);
        render_settings(f % NUM_TABS);
        kt_end(s);
        kt_present(s);
        printf("  Keep-alive: %4d cmds\r", kt_cmd_count(s));
        keep--;
        sleep_ms(100);
    }

    printf("\nShutting down...\n");
    kaintana_win32_backend.shutdown();
    kt_free(s);
    printf("\x1b[1;32m=== Done ===\x1b[0m\n");
    return 0;
}
