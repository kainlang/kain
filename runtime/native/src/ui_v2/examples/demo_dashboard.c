// ============================================================================
//  demo_dashboard.c — Real-Time Data Dashboard Demo (Kaintana Win32 GDI)
//
//  A Grafana-inspired monitoring dashboard with 4 stat panels, a line chart
//  area, data table, sidebar navigation, and top bar. Data values change
//  each frame to simulate live monitoring.
//
//  Pattern: same as demo_minecraft_ui.c — includes backend .c files directly.
//
//  Compile (from runtime/native/src/ui_v2/):
//    python build.py examples/demo_dashboard.c --backend win32 --run
//
//  Compile (from runtime/native/src/ui_v2/):
//    python build.py examples/demo_dashboard.c --backend win32 --run
//
//  Or manually:
//    gcc -std=c11 -I . -I ../../include -o examples/demo_dashboard.exe
//        examples/demo_dashboard.c
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
#define WIN_W           1280
#define WIN_H           800
#define CHART_BARS      30
#define TABLE_ROWS      20

// ── Dark Grafana theme colors (ARGB) ─────────────────────────────────────
#define C_BG            0xFF0D1117
#define C_PANEL_BG      0xFF161B22
#define C_SIDEBAR_BG    0xFF161B22
#define C_TOPBAR_BG     0xFF161B22
#define C_NAV_HOVER     0xFF21262D
#define C_NAV_ACTIVE    0xFF1F6FEB
#define C_BORDER        0xFF30363D
#define C_CPU           0xFF58A6FF
#define C_MEM           0xFF3FB950
#define C_DISK          0xFFD29922
#define C_NET           0xFFF85149
#define C_BAR_BG        0xFF30363D
#define C_GREEN         0xFF3FB950
#define C_YELLOW        0xFFD29922
#define C_RED           0xFFF85149
#define C_BTN_REFRESH   0xFF238636
#define C_DARK_BG       0xFF0D1117
#define C_DARK_BG2      0xFF1A2332

// ═══════════════════════════════════════════════════════════════════════════════
//  HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

static void sleep_ms(DWORD ms) { Sleep(ms); }

typedef struct {
    int cpu_usage;
    int mem_usage;
    int disk_io;
    int net_traffic;
    int chart_vals[CHART_BARS];
} DashboardData;

static void update_data(DashboardData* d, int frame) {
    d->cpu_usage    = ((frame * 13 + 25) % 70) + 10;
    d->mem_usage    = ((frame * 3  + 40) % 50) + 30;
    d->disk_io      = ((frame * 7  + 5)  % 55) + 5;
    if ((frame % 5) == 0) d->disk_io = 75 + (frame % 20);
    d->net_traffic  = ((frame * 11 + 3)  % 45) + 3;
    for (int i = 0; i < CHART_BARS; i++) {
        int base = (frame * 7 + i * 13 + 5) % 200;
        d->chart_vals[i] = (base < 20) ? 20 : base;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  DIRECT FRAMEBUFFER RENDERERS
// ═══════════════════════════════════════════════════════════════════════════════

static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    win32_fb_fill_rect(x, y, x + w, y + h, color);
}

static void fill_round(int x, int y, int w, int h, float r, uint32_t color) {
    win32_fb_fill_rounded_rect(x, y, x + w, y + h, r, color);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  DASHBOARD RENDER
// ═══════════════════════════════════════════════════════════════════════════════

static void render_dashboard(const DashboardData* d) {
    int w = WIN_W, h = WIN_H;
    int sidebar_w = 200;
    int top_h = 44;
    int pad = 12;
    int stat_h = 110;

    // ── Background ───────────────────────────────────────────────────────
    fill_rect(0, 0, w, h, C_BG);

    // ── Top bar ──────────────────────────────────────────────────────────
    fill_rect(0, 0, w, top_h, C_TOPBAR_BG);

    // Logo / title
    fill_round(12, 8, 160, top_h - 16, 4, C_NAV_HOVER);
    // Time range selector
    fill_round(w - 220, 8, 100, top_h - 16, 4, C_NAV_HOVER);
    // Refresh button
    fill_round(w - 108, 8, 96, top_h - 16, 4, C_BTN_REFRESH);

    // ── Sidebar ──────────────────────────────────────────────────────────
    fill_rect(0, top_h, sidebar_w, h - top_h, C_SIDEBAR_BG);
    int sy = top_h + 8;

    // Navigation header
    fill_round(6, sy, sidebar_w - 12, 30, 4, C_NAV_HOVER);
    sy += 38;

    // Nav items
    uint32_t nav_colors[]   = {C_NAV_ACTIVE, C_NAV_HOVER, C_NAV_HOVER,
                               C_NAV_HOVER,  C_NAV_HOVER};
    for (int i = 0; i < 5; i++) {
        fill_round(6, sy, sidebar_w - 12, 28, 4, nav_colors[i]);
        sy += 34;
    }

    // ── Main content area ────────────────────────────────────────────────
    int mx = sidebar_w + pad;
    int my = top_h + pad;
    int mw = w - mx - pad;
    int content_y = my;

    // ── Stat panels row ──────────────────────────────────────────────────
    int panel_w = (mw - 3 * pad) / 4;  // 4 panels with gaps
    int px = mx;

    struct { const char* label; int value; uint32_t bar_color; } panels[] = {
        {"CPU Usage",   d->cpu_usage,   C_CPU},
        {"Memory",      d->mem_usage,   C_MEM},
        {"Disk I/O",    d->disk_io,     C_DISK},
        {"Network",     d->net_traffic, C_NET},
    };

    for (int i = 0; i < 4; i++) {
        int x = px + i * (panel_w + pad);
        // Panel background
        fill_round(x, content_y, panel_w, stat_h, 6, C_PANEL_BG);
        // Label area
        fill_rect(x + 12, content_y + 10, panel_w - 24, 20, C_PANEL_BG);
        // Value box
        uint32_t val_color = panels[i].value > 70 ? C_RED :
                             (panels[i].value > 50 ? C_YELLOW : C_GREEN);
        fill_round(x + 12, content_y + 36, (panel_w - 24) * 3 / 5, 32, 4, val_color);
        // Bar background
        fill_round(x + 12, content_y + stat_h - 20, panel_w - 24, 8, 4, C_BAR_BG);
        // Bar fill
        int bar_fill_w = (panel_w - 24) * panels[i].value / 100;
        if (bar_fill_w < 4) bar_fill_w = 4;
        fill_round(x + 12, content_y + stat_h - 20, bar_fill_w, 8, 4, panels[i].bar_color);
    }

    content_y += stat_h + pad;

    // ── Chart section ────────────────────────────────────────────────────
    int chart_h = 220;
    fill_round(mx, content_y, mw, chart_h, 6, C_PANEL_BG);

    // Chart header
    fill_rect(mx, content_y, mw, 26, C_NAV_HOVER);

    // Chart area
    int chart_x = mx + 10;
    int chart_y = content_y + 34;
    int chart_w = mw - 20;
    int chart_h_inner = chart_h - 48;

    fill_rect(chart_x, chart_y, chart_w, chart_h_inner, C_DARK_BG);

    // 30 vertical bars
    int bar_w = (chart_w - (CHART_BARS - 1) * 3) / CHART_BARS;
    if (bar_w < 4) bar_w = 4;
    for (int i = 0; i < CHART_BARS; i++) {
        int v = d->chart_vals[i];
        uint32_t bar_clr = (v > 150) ? C_RED : ((v > 80) ? C_YELLOW : C_CPU);
        int bar_h = v * (chart_h_inner - 8) / 220 + 4;
        if (bar_h > chart_h_inner - 8) bar_h = chart_h_inner - 8;
        int bx = chart_x + 4 + i * (bar_w + 3);
        int by = chart_y + chart_h_inner - 4 - bar_h;
        fill_round(bx, by, bar_w, bar_h, 2, bar_clr);
    }

    content_y += chart_h + pad;

    // ── Table section ───────────────────────────────────────────────────
    int table_h = h - content_y - pad;
    if (table_h < 100) table_h = 100;

    fill_round(mx, content_y, mw, table_h, 6, C_PANEL_BG);

    // Table header
    fill_rect(mx + 2, content_y + 4, mw - 4, 28, C_NAV_HOVER);

    // Column headers
    int col_x[] = { mx + 12, mx + (mw * 3 / 5), mx + (mw * 4 / 5), mx + (mw * 19 / 20) };
    // "Process", "CPU", "Memory", "Disk" - just colored header sections

    // Table data rows
    int row_y = content_y + 36;
    for (int i = 0; i < TABLE_ROWS && row_y < content_y + table_h - 4; i++) {
        uint32_t row_bg = (i % 2 == 0) ? C_PANEL_BG : C_DARK_BG2;
        fill_rect(mx + 2, row_y, mw - 4, 24, row_bg);

        // CPU cell colored by value
        int cpu_val = ((d->cpu_usage * 3 + i * 7 + 5) % 40) + 1;
        uint32_t cpu_clr = (cpu_val > 30) ? C_RED : ((cpu_val > 15) ? C_YELLOW : C_GREEN);
        fill_round(col_x[1] + 4, row_y + 3, 48, 18, 3, cpu_clr);

        // Memory cell
        int mem_val = ((d->mem_usage * 2 + i * 11 + 3) % 60) + 10;
        uint32_t mem_clr = (mem_val > 50) ? C_RED : ((mem_val > 25) ? C_YELLOW : C_GREEN);
        fill_round(col_x[2] + 4, row_y + 3, 48, 18, 3, mem_clr);

        // Disk cell
        int dsk_val = ((d->disk_io * 5 + i * 3 + 7) % 30);
        uint32_t dsk_clr = (dsk_val > 20) ? C_YELLOW : C_GREEN;
        fill_round(col_x[3] + 4, row_y + 3, 40, 18, 3, dsk_clr);

        row_y += 26;
    }
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
    printf("\x1b[1;36m=== Kaintana Dashboard Demo ===\x1b[0m\n");
    printf("Win32 backend \x1b[2m(GDI software rendering, %dx%d)\x1b[0m\n", WIN_W, WIN_H);
    printf("\n");

    kt_init();
    kt_Session* s = kt_make("dashboard", WIN_W, WIN_H);
    if (!s) { fprintf(stderr, "FAIL: kt_make NULL\n"); return 1; }

    // Register Win32 backend (using the backend pointer from host_win32.c)
    kt_backend_register(s, "win32", &kaintana_win32_backend);
    if (!kt_backend_select(s, "win32")) {
        fprintf(stderr, "FAIL: kt_backend_select\n");
        kt_free(s);
        return 1;
    }

    // kt_backend_select() already calls init() internally.
    printf("Window created. Running until close request...\n\n");

    DashboardData data;
    memset(&data, 0, sizeof(data));

    int frame = 0;
    while (!kt_should_close(s)) {
        update_data(&data, frame);

        kaintana_win32_backend.new_frame();
        kt_begin(s, 16.0);
        kt_end(s);        // Empty tree — process before direct rendering
        kt_present(s);    // Present before direct rendering to framebuffer

        // Render directly to framebuffer (after kt_present so win32_render
        // doesn't clear our pixels with the cmd_count==0 early return)
        render_dashboard(&data);

        // Force a second present to show the direct rendering
        g_needs_present = true;
        win32_present_to_screen();

        printf("  Frame %5d  |  CPU:%2d%%  MEM:%2d%%  DSK:%2d%%  NET:%2d%%  (%d cmds)\r",
               frame + 1,
               data.cpu_usage, data.mem_usage, data.disk_io, data.net_traffic,
               kt_cmd_count(s));

        frame++;
        sleep_ms(80);
    }

    printf("\n\x1b[1;32m=== Frame loop complete ===\x1b[0m\n");
    printf("\nShutting down...\n");
    kaintana_win32_backend.shutdown();
    kt_free(s);
    printf("\x1b[1;32m=== Done ===\x1b[0m\n");
    return 0;
}
