// ============================================================================
//  retrowave.c — "RETRO WAVE 2084" Synthwave UI Demo
//  ============================================================================
//  A visually stunning synthwave/cyberpunk UI demo for Kain's Native UI
//  runtime. Pushes every pixel-pushing capability of the renderer.
//
//  Features:
//    - Animated perspective grid (road-runner style scrolling toward viewer)
//    - Glowing neon sun with horizontal scanlines (retro-wave sunset)
//    - 5 transparent glowing panels with live content:
//      1. SYS.LINK — bouncing equalizer bars
//      2. DATA.CORE — rotating 3D wireframe cube
//      3. SIGNAL   — scrolling sine wave with glow trail
//      4. TERMINAL — Matrix-style green text rain
//      5. CLOCK    — large glowing digital clock
//    - 6+ loaded fonts, each panel uses a different font
//    - Two bouncing "cassette tape" icons (click-drag to catch)
//    - Glow effects on every element (multi-pass alpha-blended)
//    - Interactive controls: slider, button, toggle, textbox
//    - Keyboard shortcuts: G=grid, C=color, Space=animation, Esc=exit
//    - Glitch effect (horizontal band shift every ~5 sec)
//    - 3 color schemes: retrowave, matrix green, ocean blue
//    - FPS counter and performance info
//
//  Build:
//    cd X:\runtime\native\src\ui\test_ui_v2
//    build.bat
//  ============================================================================

#define WIN32_LEAN_AND_MEAN
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "../widgets/ui_widget.h"
#include "ui_system.h"
#include "ui_system_internal.h"
#include "ui_host_adapter.h"
#include "../../include/ui_renderer.h"
#include "../../include/ui_layout.h"
#include "../../include/ui_color.h"

// ── Stubs from core.c ─────────────────────────────────────────────────
char* string_new(char* src);
double kain_clampd(double value, double min_value, double max_value);

// ── KainWin32UiHost (must match ui_host_adapter.c exactly) ────────────
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

// ── Forward declarations ──────────────────────────────────────────────
static LRESULT CALLBACK retrowave_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l);

// ============================================================================
//  CONSTANTS
// ============================================================================

#define SCREEN_W         1280
#define SCREEN_H         720
#define GRID_VANISH_X    640
#define GRID_VANISH_Y    380
#define MAX_BOUNCERS     2
#define EQ_BARS          8
#define WAVE_TRAIL_LEN   70
#define RAIN_DROPS       35
#define RAIN_CHARS       24

// ── Retro wave palette (0xAARRGGBB) ───────────────────────────────────
#define RW_BG       0xFF0A0A1A  // deep dark navy
#define RW_GRID1    0xFFFF00AA  // hot pink
#define RW_GRID2    0xFF00FFFF  // cyan
#define RW_SUN1     0xFFFF00AA  // hot pink
#define RW_SUN2     0xFFAA00FF  // purple
#define RW_SUN3     0xFFFF5500  // orange
#define RW_GLOW_PK  0xFFFF00AA  // pink glow
#define RW_GLOW_CY  0xFF00FFFF  // cyan glow
#define RW_GLOW_PU  0xFFAA00FF  // purple glow
#define RW_TEXT     0xFFE8E8F0
#define RW_TEXT_DIM 0xFF8888C0

// Matrix green scheme
#define MG_BG       0xFF0A0A0A
#define MG_GRID1    0xFF00FF41  // matrix green
#define MG_GRID2    0xFF00AA2E
#define MG_SUN1     0xFF00FF41
#define MG_SUN2     0xFF00CC33
#define MG_SUN3     0xFF008800
#define MG_TEXT     0xFF00FF41
#define MG_TEXT_DIM 0xFF008800

// Ocean blue scheme
#define OB_BG       0xFF0A0A20
#define OB_GRID1    0xFF00AAFF  // bright blue
#define OB_GRID2    0xFF0055AA
#define OB_SUN1     0xFF00AAFF
#define OB_SUN2     0xFF0055DD
#define OB_SUN3     0xFF003388
#define OB_TEXT     0xFFAADDFF
#define OB_TEXT_DIM 0xFF446688

// ============================================================================
//  COLOR SCHEME TABLE
// ============================================================================

typedef struct {
    uint32_t bg;
    uint32_t grid1, grid2, grid3;
    uint32_t sun1, sun2, sun3, sun4;
    uint32_t glow_pink, glow_cyan, glow_purple;
    uint32_t panel_border;
    uint32_t text, text_dim;
    uint32_t accent;
} ColorScheme;

static ColorScheme g_schemes[3];

static void init_color_schemes(void) {
    // Retro wave
    g_schemes[0].bg          = 0xFF0A0A1A;
    g_schemes[0].grid1       = 0xFFFF00AA;
    g_schemes[0].grid2       = 0xFF00FFFF;
    g_schemes[0].grid3       = 0xFF5500FF;
    g_schemes[0].sun1        = 0xFFFF00AA;
    g_schemes[0].sun2        = 0xFFCC00DD;
    g_schemes[0].sun3        = 0xFFFF5500;
    g_schemes[0].sun4        = 0xFFFFCC00;
    g_schemes[0].glow_pink   = 0xFFFF00AA;
    g_schemes[0].glow_cyan   = 0xFF00FFFF;
    g_schemes[0].glow_purple = 0xFFAA00FF;
    g_schemes[0].panel_border = 0xFFFF00AA;
    g_schemes[0].text        = 0xFFF0E0FF;
    g_schemes[0].text_dim    = 0xFF8888C0;
    g_schemes[0].accent      = 0xFFFF00AA;

    // Matrix green
    g_schemes[1].bg          = 0xFF0A0A0A;
    g_schemes[1].grid1       = 0xFF00FF41;
    g_schemes[1].grid2       = 0xFF00AA2E;
    g_schemes[1].grid3       = 0xFF005500;
    g_schemes[1].sun1        = 0xFF00FF41;
    g_schemes[1].sun2        = 0xFF00CC33;
    g_schemes[1].sun3        = 0xFF008800;
    g_schemes[1].sun4        = 0xFF004400;
    g_schemes[1].glow_pink   = 0xFF00FF41;
    g_schemes[1].glow_cyan   = 0xFF00CC33;
    g_schemes[1].glow_purple = 0xFF008800;
    g_schemes[1].panel_border = 0xFF00FF41;
    g_schemes[1].text        = 0xFF00FF41;
    g_schemes[1].text_dim    = 0xFF006600;
    g_schemes[1].accent      = 0xFF00FF41;

    // Ocean blue
    g_schemes[2].bg          = 0xFF0A0A20;
    g_schemes[2].grid1       = 0xFF00AAFF;
    g_schemes[2].grid2       = 0xFF0055AA;
    g_schemes[2].grid3       = 0xFF003366;
    g_schemes[2].sun1        = 0xFF00AAFF;
    g_schemes[2].sun2        = 0xFF0055DD;
    g_schemes[2].sun3        = 0xFF003388;
    g_schemes[2].sun4        = 0xFF001144;
    g_schemes[2].glow_pink   = 0xFF00AAFF;
    g_schemes[2].glow_cyan   = 0xFF0066CC;
    g_schemes[2].glow_purple = 0xFF003388;
    g_schemes[2].panel_border = 0xFF00AAFF;
    g_schemes[2].text        = 0xFFAADDFF;
    g_schemes[2].text_dim    = 0xFF446688;
    g_schemes[2].accent      = 0xFF00AAFF;
}

// ============================================================================
//  APPLICATION STATE
// ============================================================================

typedef struct {
    double time;
    int frame;
    int running;
    int64_t session_id;

    // Grid
    double grid_offset;
    double grid_speed;
    int grid_enabled;
    int anim_enabled;

    // Color
    int color_scheme;

    // Font IDs from widget context
    int64_t font_clock;    // Arial (large clock digits)
    int64_t font_mono;     // Consolas (terminal rain)
    int64_t font_heading;  // Impact (panel titles)
    int64_t font_body;     // Segoe UI (body text)
    int64_t font_fancy;    // Gabriola (decorative)
    int64_t font_label;    // Verdana (labels)

    // Interactive state
    char status_text[128];
    int grid_toggle_val;
    int prev_grid_toggle_val;
    int prev_color_btn;

    // Bouncers
    double bx[MAX_BOUNCERS], by[MAX_BOUNCERS];
    double bvx[MAX_BOUNCERS], bvy[MAX_BOUNCERS];
    int dragging_bouncer;
    double drag_off_x, drag_off_y;

    // Equalizer
    double eq_phases[EQ_BARS];
    double eq_heights[EQ_BARS];

    // Cube
    double cube_rot_x, cube_rot_y, cube_rot_z;

    // Sine wave trail
    double wave_trail_x[WAVE_TRAIL_LEN];
    double wave_trail_y[WAVE_TRAIL_LEN];
    int wave_trail_head;

    // Matrix rain
    double rain_y[RAIN_DROPS];
    double rain_speed[RAIN_DROPS];
    int rain_x[RAIN_DROPS];
    int rain_len[RAIN_DROPS];
    char rain_text[RAIN_DROPS][RAIN_CHARS + 1];

    // Glitch
    int glitch_timer;
    int glitch_active;
    int glitch_y;
    int glitch_h;

    // Screen shake
    double shake_x, shake_y;
    double shake_intensity;

    // Controls
    double control_slider_val;

    // Performance
    double fps;
    double fps_timer;
    int fps_count;
    char fps_str[32];
} AppState;

static AppState g_app;
static KainWin32UiHost* g_host;       // global for wndproc access
static WNDPROC g_orig_wndproc = NULL;

// ============================================================================
//  PIXEL HELPERS
// ============================================================================

static uint32_t* get_fb(KainWin32UiHost* host, int* out_stride) {
    if (!host || !host->framebuffer) return NULL;
    *out_stride = host->fb_stride / 4;
    return (uint32_t*)host->framebuffer;
}

static void write_px(KainWin32UiHost* host, int x, int y, uint32_t color) {
    if (!host || !host->framebuffer) return;
    int w = host->width, h = host->height;
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    int stride = host->fb_stride / 4;
    ((uint32_t*)host->framebuffer)[y * stride + x] = color;
}

// Safe pixel blend with bounds checking
static void blend_px_safe(KainWin32UiHost* host, int x, int y, uint32_t color) {
    if (!host || !host->framebuffer) return;
    int w = host->width, h = host->height;
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    int stride = host->fb_stride / 4;
    blend_px_safe(host, x, y, color);
}

// Blend a color over a pixel with alpha
static void blend_px(uint32_t* dst, uint32_t src) {
    uint8_t sa = (src >> 24) & 0xFF;
    if (sa == 0) return;
    if (sa == 255) { *dst = src; return; }
    uint8_t sr = (src >> 16) & 0xFF;
    uint8_t sg = (src >> 8) & 0xFF;
    uint8_t sb = src & 0xFF;
    uint8_t da = 255 - sa;
    uint8_t dr = ((uint16_t)sr * sa + ((*dst >> 16) & 0xFF) * da) / 255;
    uint8_t dg = ((uint16_t)sg * sa + ((*dst >> 8) & 0xFF) * da) / 255;
    uint8_t db = ((uint16_t)sb * sa + (*dst & 0xFF) * da) / 255;
    *dst = 0xFF000000 | ((uint32_t)dr << 16) | ((uint32_t)dg << 8) | db;
}

// ── Fill rect with bounds checking ────────────────────────────────────
static void fill_rect(KainWin32UiHost* host, int x, int y, int w, int h, uint32_t color) {
    if (!host || !host->framebuffer || w <= 0 || h <= 0) return;
    int fb_w = host->width, fb_h = host->height;
    int stride = host->fb_stride / 4;
    uint32_t* fb = (uint32_t*)host->framebuffer;
    for (int r = y; r < y + h && r < fb_h; r++) {
        if (r < 0) continue;
        for (int c = x; c < x + w && c < fb_w; c++) {
            if (c < 0) continue;
            blend_px(&fb[r * stride + c], color);
        }
    }
}

// ── Fill rect with alpha ──────────────────────────────────────────────
static void fill_rect_alpha(KainWin32UiHost* host, int x, int y, int w, int h,
                             uint32_t color, uint8_t alpha) {
    uint32_t c = (color & 0x00FFFFFF) | ((uint32_t)alpha << 24);
    fill_rect(host, x, y, w, h, c);
}

// ── Draw rect border (1px) ────────────────────────────────────────────
static void draw_rect_border(KainWin32UiHost* host, int x, int y, int w, int h,
                              uint32_t color) {
    fill_rect(host, x, y, w, 1, color);
    fill_rect(host, x, y + h - 1, w, 1, color);
    fill_rect(host, x, y, 1, h, color);
    fill_rect(host, x + w - 1, y, 1, h, color);
}

// ── Draw a line using Bresenham ───────────────────────────────────────
static void draw_line(KainWin32UiHost* host, int x1, int y1, int x2, int y2,
                       uint32_t color) {
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        write_px(host, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

// ── Draw circle outline ───────────────────────────────────────────────
static void draw_circle(KainWin32UiHost* host, int cx, int cy, int r,
                         uint32_t color) {
    int x = 0, y = r, d = 3 - 2 * r;
    while (y >= x) {
        write_px(host, cx + x, cy + y, color);
        write_px(host, cx - x, cy + y, color);
        write_px(host, cx + x, cy - y, color);
        write_px(host, cx - x, cy - y, color);
        write_px(host, cx + y, cy + x, color);
        write_px(host, cx - y, cy + x, color);
        write_px(host, cx + y, cy - x, color);
        write_px(host, cx - y, cy - x, color);
        x++;
        if (d > 0) { y--; d = d + 4 * (x - y) + 10; }
        else d = d + 4 * x + 6;
    }
}

// ── Draw filled circle ────────────────────────────────────────────────
static void draw_filled_circle(KainWin32UiHost* host, int cx, int cy, int r,
                                uint32_t color) {
    for (int y = -r; y <= r; y++) {
        int row = cy + y;
        if (row < 0 || row >= host->height) continue;
        int hw = (int)(sqrt((double)(r * r - y * y)) + 0.5);
        for (int x = -hw; x <= hw; x++) {
            int col = cx + x;
            if (col < 0 || col >= host->width) continue;
            int stride = host->fb_stride / 4;
            blend_px_safe(host, col, row, color);
        }
    }
}

// ── Draw glow around a rect ───────────────────────────────────────────
static void draw_glow(KainWin32UiHost* host, int x, int y, int w, int h,
                       uint32_t color, int spread) {
    // Outer glow passes (decreasing alpha)
    uint8_t alphas[] = {30, 60, 90, 120};
    int spreads[] = {spread * 3, spread * 2, spread, 1};
    for (int i = 0; i < 4; i++) {
        int s = spreads[i];
        uint32_t c = (color & 0x00FFFFFF) | ((uint32_t)alphas[i] << 24);
        for (int dx = -s; dx <= s; dx += s) {
            for (int dy = -s; dy <= s; dy += s) {
                if (dx == 0 && dy == 0) continue;
                fill_rect(host, x + dx, y + dy, w, h, c);
            }
        }
    }
}

// ============================================================================
//  GRID
// ============================================================================

static void draw_perspective_grid(KainWin32UiHost* host, int w, int h,
                                   double offset, const ColorScheme* cs) {
    int vx = GRID_VANISH_X, vy = GRID_VANISH_Y;
    int num_h_lines = 40;
    int num_v_lines = 30;

    // Horizontal lines (get closer toward vanishing point, scroll down)
    for (int i = 1; i < num_h_lines; i++) {
        double t = (double)i / (double)num_h_lines;
        // Perspective: lines bunch up near vanishing point
        double z = t * t;  // quadratic spacing
        double yy = vy + (h - vy) * z;
        // Scroll
        yy += fmod(offset * (1.0 - z * 0.7), (h - vy) * 0.08);
        int iy = (int)yy;
        if (iy >= vy && iy < h) {
            // Calculate alpha based on distance from viewer
            double dist = (double)(iy - vy) / (double)(h - vy);
            uint8_t alpha = (uint8_t)(80 + 175 * dist);
            uint32_t color = (i % 2 == 0) ? cs->grid1 : cs->grid2;
            color = (color & 0x00FFFFFF) | ((uint32_t)kain_clampd(alpha, 0, 255) << 24);
            // Draw horizontal line with perspective fade near edges
            int half_w = (int)((double)w * 0.5 * (1.0 + 2.0 * z));
            int lx1 = vx - half_w;
            int lx2 = vx + half_w;
            for (int px = lx1; px <= lx2; px++) {
                if (px >= 0 && px < w) {
                    // Edge fade
                    double edge_dist = (px < vx) ?
                        (double)(px - lx1) / (double)(vx - lx1) :
                        (double)(lx2 - px) / (double)(lx2 - vx);
                    uint8_t ea = (uint8_t)(alpha * kain_clampd(edge_dist * 2.0, 0.0, 1.0));
                    uint32_t pc = (i % 2 == 0) ? cs->grid1 : cs->grid2;
                    pc = (pc & 0x00FFFFFF) | ((uint32_t)ea << 24);
                    int stride = host->fb_stride / 4;
                    blend_px_safe(host, px, iy, pc);
                }
            }
        }
    }

    // Vertical lines (radiating from vanishing point)
    for (int i = 0; i < num_v_lines; i++) {
        double angle = -1.2 + (double)i / (double)(num_v_lines - 1) * 2.4;
        double dx = sin(angle) * 800.0;
        double dy = cos(angle) * 800.0;
        int ex1 = vx + (int)dx;
        int ey1 = vy + (int)dy;
        // Also draw mirror lines
        uint8_t alpha = (uint8_t)(80 + (1.0 - fabs(angle) / 1.2) * 120);
        uint32_t color = (i % 3 == 0) ? cs->grid1 :
                         (i % 3 == 1) ? cs->grid2 : cs->grid3;
        color = (color & 0x00FFFFFF) | ((uint32_t)kain_clampd(alpha, 0, 255) << 24);
        draw_line(host, vx, vy, ex1, ey1, color);
        // Mirror
        int ex2 = vx - (int)dx;
        draw_line(host, vx, vy, ex2, ey1, color);
    }

    // Glow on vanishing point
    uint32_t glow_c = cs->grid1;
    draw_filled_circle(host, vx, vy, 4, glow_c);
    for (int r = 6; r <= 20; r += 2) {
        uint8_t a = (uint8_t)(30 - 20 * (r - 6) / 14);
        uint32_t gc = (glow_c & 0x00FFFFFF) | ((uint32_t)a << 24);
        draw_circle(host, vx, vy, r, gc);
    }
}

// ============================================================================
//  NEON SUN
// ============================================================================

static void draw_neon_sun(KainWin32UiHost* host, int w, int h,
                           const ColorScheme* cs) {
    int cx = GRID_VANISH_X;
    int cy = h - 60;
    int r = 160;
    // Outer glow
    for (int i = 0; i < 5; i++) {
        uint32_t glow_c = cs->glow_pink;
        uint8_t ga = (uint8_t)(15 - i * 3);
        if (ga > 0) {
            draw_circle(host, cx, cy, r + i * 6 + 4,
                       (glow_c & 0x00FFFFFF) | ((uint32_t)ga << 24));
        }
    }

    // Sun body: horizontal scanlines with gradient colors
    for (int y = cy - r; y <= cy + r; y++) {
        if (y < 0 || y >= h) continue;
        int dy = y - cy;
        int half = (int)(sqrt((double)(r * r - dy * dy)) + 0.5);
        if (half <= 0) continue;

        // Color gradient based on height: pink top → purple mid → orange bottom
        double t = (double)(y - (cy - r)) / (double)(r * 2);
        uint32_t color;
        if (t < 0.25) {
            color = cs->sun1;
        } else if (t < 0.50) {
            color = cs->sun2;
        } else if (t < 0.75) {
            color = cs->sun3;
        } else {
            color = cs->sun4;
        }

        // Add scanlines (every other pixel row is darker or skip for retro CRT feel)
        int scanline = (y % 3 == 0) ? 1 : 0;
        uint8_t alpha = scanline ? 220 : 160;

        // Edge fade
        for (int x = cx - half; x <= cx + half; x++) {
            if (x < 0 || x >= w) continue;
            double edge = (double)(x - (cx - half)) / (double)(half * 2);
            edge = 1.0 - fabs(edge - 0.5) * 2.0; // 0 at edges, 1 at center
            uint8_t ea = (uint8_t)(alpha * (0.3 + 0.7 * edge));
            uint32_t c = (color & 0x00FFFFFF) | ((uint32_t)ea << 24);
            int stride = host->fb_stride / 4;
            blend_px_safe(host, x, y, c);
        }
    }

    // Bright horizon line below sun
    for (int x = 0; x < w; x++) {
        int sy = cy + r + 2;
        if (sy >= 0 && sy < h) {
            uint32_t hl = cs->sun3;
            int stride = host->fb_stride / 4;
            blend_px_safe(host, x, sy, hl);
        }
    }

    // Sun glow reflection below
    for (int y = cy + r + 3; y < cy + r + 60 && y < h; y++) {
        double t = (double)(y - (cy + r)) / 60.0;
        uint8_t a = (uint8_t)(40 * (1.0 - t));
        if (a > 0) {
            int half = (int)(r * 0.6 * (1.0 - t * 0.5));
            uint32_t c = (cs->sun3 & 0x00FFFFFF) | ((uint32_t)a << 24);
            for (int x = cx - half; x <= cx + half; x++) {
                if (x >= 0 && x < w) {
                    int stride = host->fb_stride / 4;
                    blend_px_safe(host, x, y, c);
                }
            }
        }
    }
}

// ============================================================================
//  EQUALIZER PANEL (SYS.LINK)
// ============================================================================

static void update_equalizer(double dt) {
    for (int i = 0; i < EQ_BARS; i++) {
        g_app.eq_phases[i] += dt * (1.5 + (double)i * 0.3);
        // Simulate music with multiple sine waves
        double val = sin(g_app.eq_phases[i]) * 0.5 +
                     sin(g_app.eq_phases[i] * 1.7 + 1.0) * 0.3 +
                     sin(g_app.eq_phases[i] * 0.3 + 2.0) * 0.2;
        g_app.eq_heights[i] = 0.3 + (val + 1.0) * 0.35;
    }
}

static void draw_equalizer(KainWin32UiHost* host, int px, int py, int pw, int ph) {
    int bar_w = pw / (EQ_BARS * 2 + 1);
    if (bar_w < 3) bar_w = 3;
    int gap = bar_w;
    int bar_area_h = ph - 20;
    uint32_t bar_color = g_schemes[g_app.color_scheme].glow_cyan;

    for (int i = 0; i < EQ_BARS; i++) {
        int bx = px + gap + i * (bar_w + gap);
        int bh = (int)(g_app.eq_heights[i] * bar_area_h);
        if (bh < 2) bh = 2;
        int by = py + 20 + bar_area_h - bh;

        // Glow for each bar
        uint32_t glow_c = bar_color;
        for (int g = 0; g < 3; g++) {
            uint8_t ga = (uint8_t)(20 - g * 6);
            fill_rect(host, bx - g, by - g, bar_w + g * 2, bh + g * 2,
                     (glow_c & 0x00FFFFFF) | ((uint32_t)ga << 24));
        }

        // Bar body
        fill_rect(host, bx, by, bar_w, bh, bar_color);

        // Bright tip
        fill_rect(host, bx, by, bar_w, (bh > 4) ? 3 : bh,
                  (bar_color & 0x00FFFFFF) | 0xFF000000);
    }
}

// ============================================================================
//  WIREFRAME CUBE (DATA.CORE)
// ============================================================================

typedef struct { double x, y, z; } Vec3;

static void project_3d(Vec3 p, int* sx, int* sy, int cx, int cy, int size) {
    // Simple perspective: x' = x / (z + 3) * size, y' = y / (z + 3) * size
    double iz = 1.0 / (p.z + 3.0);
    *sx = cx + (int)(p.x * iz * size);
    *sy = cy + (int)(p.y * iz * size);
}

static void update_cube(double dt) {
    g_app.cube_rot_x += dt * 0.6;
    g_app.cube_rot_y += dt * 0.8;
    g_app.cube_rot_z += dt * 0.4;
    if (g_app.cube_rot_x > 6.2832) g_app.cube_rot_x -= 6.2832;
    if (g_app.cube_rot_y > 6.2832) g_app.cube_rot_y -= 6.2832;
    if (g_app.cube_rot_z > 6.2832) g_app.cube_rot_z -= 6.2832;
}

static void draw_wireframe_cube(KainWin32UiHost* host, int cx, int cy, int size) {
    // Cube vertices in local space (-1 to 1)
    Vec3 verts[8];
    for (int i = 0; i < 8; i++) {
        double x = (i & 1) ? 1.0 : -1.0;
        double y = (i & 2) ? 1.0 : -1.0;
        double z = (i & 4) ? 1.0 : -1.0;
        verts[i].x = x; verts[i].y = y; verts[i].z = z;
    }

    // Rotate
    double rx = g_app.cube_rot_x, ry = g_app.cube_rot_y, rz = g_app.cube_rot_z;
    double cxr = cos(rx), sxr = sin(rx), cyr = cos(ry), syr = sin(ry);
    double czr = cos(rz), szr = sin(rz);

    for (int i = 0; i < 8; i++) {
        double x = verts[i].x, y = verts[i].y, z = verts[i].z;
        // Rotate Z
        double x1 = x * czr - y * szr;
        double y1 = x * szr + y * czr;
        double z1 = z;
        // Rotate Y
        double x2 = x1 * cyr + z1 * syr;
        double z2 = -x1 * syr + z1 * cyr;
        double y2 = y1;
        // Rotate X
        double y3 = y2 * cxr - z2 * sxr;
        double z3 = y2 * sxr + z2 * cxr;
        verts[i].x = x2;
        verts[i].y = y3;
        verts[i].z = z3;
    }

    // Edges (12 edges of a cube)
    int edges[12][2] = {
        {0,1},{1,3},{3,2},{2,0}, // front face
        {4,5},{5,7},{7,6},{6,4}, // back face
        {0,4},{1,5},{3,7},{2,6}  // connecting
    };

    uint32_t color = g_schemes[g_app.color_scheme].glow_purple;
    uint32_t color2 = g_schemes[g_app.color_scheme].glow_cyan;

    // Draw edges
    for (int e = 0; e < 12; e++) {
        int a = edges[e][0], b = edges[e][1];
        int sx1, sy1, sx2, sy2;
        project_3d(verts[a], &sx1, &sy1, cx, cy, size);
        project_3d(verts[b], &sx2, &sy2, cx, cy, size);

        // Glow line
        uint32_t ec = (e < 8) ? color : color2;
        uint8_t ga = 40;
        uint32_t gc = (ec & 0x00FFFFFF) | ((uint32_t)ga << 24);
        draw_line(host, sx1 - 1, sy1, sx2 - 1, sy2, gc);
        draw_line(host, sx1 + 1, sy1, sx2 + 1, sy2, gc);
        draw_line(host, sx1, sy1 - 1, sx2, sy2 - 1, gc);
        draw_line(host, sx1, sy1 + 1, sx2, sy2 + 1, gc);
        draw_line(host, sx1, sy1, sx2, sy2, ec);
    }

    // Glow dots at vertices
    for (int i = 0; i < 8; i++) {
        int sx, sy;
        project_3d(verts[i], &sx, &sy, cx, cy, size);
        uint32_t vc = (i < 4) ? color : color2;
        draw_filled_circle(host, sx, sy, 2, vc);
    }
}

// ============================================================================
//  SINE WAVE (SIGNAL)
// ============================================================================

static void update_sine_wave(double dt) {
    g_app.wave_trail_y[g_app.wave_trail_head] =
        sin(g_app.time * 2.0) * 0.4 + sin(g_app.time * 3.7 + 1.0) * 0.2 +
        sin(g_app.time * 0.7 + 2.0) * 0.15;
    g_app.wave_trail_x[g_app.wave_trail_head] = 1.0;
    g_app.wave_trail_head = (g_app.wave_trail_head + 1) % WAVE_TRAIL_LEN;
}

static void draw_sine_wave(KainWin32UiHost* host, int px, int py, int pw, int ph) {
    int mid_y = py + ph / 2;
    int wave_h = ph / 2 - 20;
    uint32_t color = g_schemes[g_app.color_scheme].glow_cyan;

    // Draw trail with decaying opacity
    for (int i = 1; i < WAVE_TRAIL_LEN; i++) {
        int idx = (g_app.wave_trail_head - i + WAVE_TRAIL_LEN) % WAVE_TRAIL_LEN;
        int prev_idx = (idx - 1 + WAVE_TRAIL_LEN) % WAVE_TRAIL_LEN;

        double t = (double)i / (double)WAVE_TRAIL_LEN;
        double tx = px + pw * (1.0 - t);
        double ty = mid_y + g_app.wave_trail_y[idx] * wave_h;
        double px2 = px + pw * (1.0 - (double)(i - 1) / (double)WAVE_TRAIL_LEN);
        double py2 = mid_y + g_app.wave_trail_y[prev_idx] * wave_h;

        uint8_t alpha = (uint8_t)(180 * (1.0 - t));
        // Glow: thicker and fainter behind
        uint32_t c = (color & 0x00FFFFFF) | ((uint32_t)(alpha) << 24);
        uint32_t c_glow = (color & 0x00FFFFFF) | ((uint32_t)(alpha / 3) << 24);

        draw_line(host, (int)px2, (int)py2, (int)tx, (int)ty, c_glow);
        draw_line(host, (int)px2 + 1, (int)py2, (int)tx + 1, (int)ty, c_glow);
        draw_line(host, (int)px2, (int)py2 + 1, (int)tx, (int)ty + 1, c_glow);
        draw_line(host, (int)px2, (int)py2, (int)tx, (int)ty, c);
    }

    // Center line
    uint32_t dim = (color & 0x00FFFFFF) | (0x30 << 24);
    draw_line(host, px + 10, mid_y, px + pw - 10, mid_y, dim);
}

// ============================================================================
//  MATRIX TEXT RAIN (TERMINAL)
// ============================================================================

static void init_text_rain(void) {
    for (int i = 0; i < RAIN_DROPS; i++) {
        g_app.rain_x[i] = rand() % 18;  // column
        g_app.rain_y[i] = (double)(rand() % 40) * -1.0;
        g_app.rain_speed[i] = 0.3 + (double)(rand() % 100) / 200.0;
        g_app.rain_len[i] = 3 + rand() % 12;
        for (int j = 0; j < RAIN_CHARS; j++) {
            g_app.rain_text[i][j] = (char)(0x20 + rand() % 95);
        }
        g_app.rain_text[i][RAIN_CHARS] = '\0';
    }
}

static void update_text_rain(double dt) {
    int cols = 18;
    for (int i = 0; i < RAIN_DROPS; i++) {
        g_app.rain_y[i] += g_app.rain_speed[i] * dt * 30.0;
        // Shift characters
        if (g_app.rain_y[i] > 0 && rand() % 10 == 0) {
            memmove(g_app.rain_text[i], g_app.rain_text[i] + 1, RAIN_CHARS - 1);
            g_app.rain_text[i][RAIN_CHARS - 1] = (char)(0x20 + rand() % 95);
        }
        // Reset when off screen
        if (g_app.rain_y[i] > (double)g_app.rain_len[i] + 5.0) {
            g_app.rain_x[i] = rand() % cols;
            g_app.rain_y[i] = -(double)(rand() % 10);
            g_app.rain_speed[i] = 0.3 + (double)(rand() % 100) / 200.0;
            g_app.rain_len[i] = 3 + rand() % 12;
        }
    }
}

static void draw_text_rain(KainWin32UiHost* host, KainUiWidgetContext* ctx,
                            int px, int py, int pw, int ph) {
    if (g_app.font_mono <= 0) return;
    int cols = 18;
    int cell_w = pw / cols;
    int cell_h = 16;

    for (int i = 0; i < RAIN_DROPS; i++) {
        int cx = px + g_app.rain_x[i] * cell_w + cell_w / 4;
        int cy = py + (int)g_app.rain_y[i] * cell_h;

        // Draw each character
        for (int j = 0; j < g_app.rain_len[i] && j < RAIN_CHARS; j++) {
            int ch_y = cy - j * cell_h;
            if (ch_y < py - cell_h || ch_y > py + ph) continue;

            uint32_t color;
            if (j == 0) {
                // Leading character is bright
                color = 0xFFFFFFFF;
            } else {
                // Trail fades
                uint8_t alpha = (uint8_t)(255 - 220 * j / g_app.rain_len[i]);
                uint32_t scheme_color = g_schemes[g_app.color_scheme].glow_cyan;
                color = (scheme_color & 0x00FFFFFF) | ((uint32_t)alpha << 24);
            }

            char ch[2] = {g_app.rain_text[i][j % strlen(g_app.rain_text[i])], '\0'};
            if (ch[0] < 0x20) ch[0] = 'A' + (ch[0] & 0x0F);

            // Glow for leading char
            if (j == 0) {
                ui_widget_draw_text_ex(ctx, cx - 1, ch_y, ch, 0x4000FF00, 12, g_app.font_mono);
                ui_widget_draw_text_ex(ctx, cx + 1, ch_y, ch, 0x4000FF00, 12, g_app.font_mono);
                ui_widget_draw_text_ex(ctx, cx, ch_y - 1, ch, 0x4000FF00, 12, g_app.font_mono);
            }

            ui_widget_draw_text_ex(ctx, cx, ch_y, ch, color, 12, g_app.font_mono);
        }
    }
}

// ============================================================================
//  DIGITAL CLOCK
// ============================================================================

static void draw_digital_clock(KainWin32UiHost* host, KainUiWidgetContext* ctx,
                                int px, int py, int pw, int ph) {
    if (g_app.font_clock <= 0) return;

    // Get current local time
    SYSTEMTIME st;
    GetLocalTime(&st);
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
             st.wHour, st.wMinute, st.wSecond);

    // Center text
    int tw = ui_widget_text_width(ctx, time_str);
    if (tw <= 0) return;
    // But we need text_width with a specific font... use the font_mono or measure width
    // Actually use the clock font
    int tx = px + (pw - tw) / 2;
    int ty = py + (ph - 24) / 2;

    uint32_t color = g_schemes[g_app.color_scheme].glow_pink;

    // Strong glow behind clock digits
    for (int g = 8; g > 0; g -= 2) {
        uint8_t ga = (uint8_t)(15 - g * 1);
        uint32_t gc = (color & 0x00FFFFFF) | ((uint32_t)ga << 24);
        ui_widget_draw_text_ex(ctx, tx - g, ty, time_str, gc, 48, g_app.font_clock);
        ui_widget_draw_text_ex(ctx, tx + g, ty, time_str, gc, 48, g_app.font_clock);
        ui_widget_draw_text_ex(ctx, tx, ty - g, time_str, gc, 48, g_app.font_clock);
        ui_widget_draw_text_ex(ctx, tx, ty + g, time_str, gc, 48, g_app.font_clock);
    }

    // Clock text
    ui_widget_draw_text_ex(ctx, tx, ty, time_str, color, 48, g_app.font_clock);

    // Date below
    char date_str[32];
    snprintf(date_str, sizeof(date_str), "%02d/%02d/%04d",
             st.wMonth, st.wDay, st.wYear);
    tw = ui_widget_text_width(ctx, date_str);
    tx = px + (pw - tw) / 2;
    ui_widget_draw_text_ex(ctx, tx, py + ph - 20, date_str,
                           g_schemes[g_app.color_scheme].text_dim, 14, g_app.font_body);
}

// ============================================================================
//  BOUNCING CASSETTE ICONS
// ============================================================================

static void init_bouncers(void) {
    for (int i = 0; i < MAX_BOUNCERS; i++) {
        g_app.bx[i] = 200.0 + (double)(rand() % 600);
        g_app.by[i] = 100.0 + (double)(rand() % 300);
        double angle = (double)(rand() % 628) / 100.0;
        double speed = 1.0 + (double)(rand() % 100) / 50.0;
        g_app.bvx[i] = cos(angle) * speed;
        g_app.bvy[i] = sin(angle) * speed;
    }
    g_app.dragging_bouncer = -1;
}

static void update_bouncers(double dt, int w, int h, KainWin32UiHost* host) {
    int cassette_w = 60, cassette_h = 40;

    for (int i = 0; i < MAX_BOUNCERS; i++) {
        if (g_app.dragging_bouncer == i) continue;

        g_app.bx[i] += g_app.bvx[i] * dt * 60.0;
        g_app.by[i] += g_app.bvy[i] * dt * 60.0;

        // Gravity
        g_app.bvy[i] += 0.05 * dt * 60.0;

        // Bounce off edges
        if (g_app.bx[i] < 0) {
            g_app.bx[i] = 0; g_app.bvx[i] = -g_app.bvx[i] * 0.9;
            // Trail sparkle
            for (int s = 0; s < 5; s++)
                write_px(host, rand() % 10, rand() % 10,
                         g_schemes[g_app.color_scheme].glow_pink);
        }
        if (g_app.bx[i] > w - cassette_w) {
            g_app.bx[i] = (double)(w - cassette_w);
            g_app.bvx[i] = -g_app.bvx[i] * 0.9;
        }
        if (g_app.by[i] < 0) {
            g_app.by[i] = 0; g_app.bvy[i] = -g_app.bvy[i] * 0.9;
        }
        if (g_app.by[i] > h - cassette_h - 40) { // above control bar
            g_app.by[i] = (double)(h - cassette_h - 40);
            g_app.bvy[i] = -g_app.bvy[i] * 0.85;
            g_app.bvx[i] *= 0.98;
        }

        // Damping
        g_app.bvx[i] *= 0.999;
    }
}

static void draw_cassette(KainWin32UiHost* host, int x, int y, uint32_t color) {
    int cw = 50, ch = 34;

    // Glow
    uint32_t gc = (color & 0x00FFFFFF) | (0x30 << 24);
    fill_rect(host, x - 2, y - 2, cw + 4, ch + 4, gc);
    fill_rect(host, x - 1, y - 1, cw + 2, ch + 2, gc);

    // Cassette body
    fill_rect(host, x, y, cw, ch, 0xCC222244);
    draw_rect_border(host, x, y, cw, ch, color);

    // Tape reels (two circles)
    draw_circle(host, x + 14, y + ch / 2, 6, color);
    draw_circle(host, x + cw - 14, y + ch / 2, 6, color);
    // Filled reel centers
    draw_filled_circle(host, x + 14, y + ch / 2, 2, color);
    draw_filled_circle(host, x + cw - 14, y + ch / 2, 2, color);

    // Window between reels
    int win_x = x + 14 + 6 + 2;
    int win_w = cw - 14 - 6 - 2 - (14 + 6 + 2);
    if (win_w > 0) {
        fill_rect(host, win_x, y + ch / 2 - 3, win_w, 6, 0x44000000);
        draw_rect_border(host, win_x, y + ch / 2 - 3, win_w, 6, color);
    }

    // Label at top
    fill_rect(host, x + 8, y + 2, cw - 16, 5, 0xFF222244);
}

// ============================================================================
//  CONTROL BAR
// ============================================================================

static void draw_controls(KainUiWidgetContext* ctx, int w, int h) {
    int bar_y = h - 36;
    int bar_h = 36;

    // Background
    fill_rect(ctx->host, 0, bar_y, w, bar_h, 0xCC0A0A1A);
    fill_rect(ctx->host, 0, bar_y, w, 2, g_schemes[g_app.color_scheme].grid1);

    // Status text (left side)
    char status[128];
    snprintf(status, sizeof(status), "RETRO WAVE 2084  |  %s  |  %s",
             g_app.status_text, g_app.fps_str);

    if (g_app.font_body > 0) {
        ui_widget_draw_text_ex(ctx, 12, bar_y + 8, status,
                               g_schemes[g_app.color_scheme].text_dim, 14,
                               g_app.font_body);
    }

    // Controls (right side)
    int cx = w - 500;

    // Speed slider
    ui_widget_draw_text_ex(ctx, cx, bar_y + 8, "SPD", 0xFF8888C0, 12, g_app.font_label);
    g_app.grid_speed = g_app.control_slider_val * 4.0 + 0.5;
    cx += 35;

    // Draw custom slider
    int sl_w = 120, sl_h = 14;
    int sl_x = cx, sl_y = bar_y + 10;
    fill_rect(ctx->host, sl_x, sl_y, sl_w, sl_h, 0xFF1A1A30);
    int fill_w = (int)(g_app.control_slider_val * (double)sl_w);
    fill_rect(ctx->host, sl_x, sl_y, fill_w, sl_h, g_schemes[g_app.color_scheme].accent);
    draw_rect_border(ctx->host, sl_x, sl_y, sl_w, sl_h, 0xFF3A3A5C);

    // Slider interaction
    int mx = (int)ctx->mouse_x, my = (int)ctx->mouse_y;
    int mouse_down = ctx->mouse_down;

    if (mouse_down && mx >= sl_x && mx < sl_x + sl_w && my >= sl_y && my < sl_y + sl_h) {
        g_app.control_slider_val = (double)(mx - sl_x) / (double)sl_w;
        if (g_app.control_slider_val < 0.0) g_app.control_slider_val = 0.0;
        if (g_app.control_slider_val > 1.0) g_app.control_slider_val = 1.0;
    }

    cx += sl_w + 15;

    // Cycle colors button
    int btn_x = cx, btn_y = bar_y + 5, btn_w = 110, btn_h = 26;
    int btn_hover = (mx >= btn_x && mx < btn_x + btn_w && my >= btn_y && my < btn_y + btn_h);
    int btn_pressed = (btn_hover && mouse_down);

    uint32_t btn_color = btn_pressed ? 0xFF505080 : (btn_hover ? 0xFF404068 : 0xFF303050);
    fill_rect(ctx->host, btn_x, btn_y, btn_w, btn_h, btn_color);
    draw_rect_border(ctx->host, btn_x, btn_y, btn_w, btn_h, g_schemes[g_app.color_scheme].accent);

    const char* scheme_names[] = {"WAVE", "MATRIX", "OCEAN"};
    ui_widget_draw_text_ex(ctx, btn_x + 8, btn_y + 6, scheme_names[g_app.color_scheme],
                           0xFFE8E8F0, 12, g_app.font_label);

    // Check for click on button
    if (!mouse_down && g_app.prev_color_btn) {
        if (btn_hover) {
            g_app.color_scheme = (g_app.color_scheme + 1) % 3;
        }
    }
    g_app.prev_color_btn = (btn_hover && mouse_down) ? 1 : 0;

    cx += btn_w + 10;

    // Grid toggle
    int tg_x = cx, tg_y = bar_y + 5, tg_w = 80, tg_h = 26;
    int tg_hover = (mx >= tg_x && mx < tg_x + tg_w && my >= tg_y && my < tg_y + tg_h);

    uint32_t tg_color = g_app.grid_enabled ? 0xFF303050 : 0xFF1A1A30;
    fill_rect(ctx->host, tg_x, tg_y, tg_w, tg_h, tg_color);
    draw_rect_border(ctx->host, tg_x, tg_y, tg_w, tg_h, g_schemes[g_app.color_scheme].accent);

    ui_widget_draw_text_ex(ctx, tg_x + 10, tg_y + 6, g_app.grid_enabled ? "GRID:ON" : "GRID:OFF",
                           0xFFE8E8F0, 12, g_app.font_label);

    if (!mouse_down && g_app.prev_grid_toggle_val) {
        if (tg_hover) {
            g_app.grid_enabled = !g_app.grid_enabled;
        }
    }
    g_app.prev_grid_toggle_val = (tg_hover && mouse_down) ? 1 : 0;

    cx += tg_w + 10;

    // Custom status text display
    char status_text_display[40];
    snprintf(status_text_display, sizeof(status_text_display), "> %s", g_app.status_text);
    if (g_app.font_mono > 0) {
        ui_widget_draw_text_ex(ctx, cx, bar_y + 8, status_text_display,
                               0xFF00FFAA, 12, g_app.font_mono);
    }
}

// ============================================================================
//  GLITCH EFFECT
// ============================================================================

static void apply_glitch(KainWin32UiHost* host, int w, int h) {
    if (!g_app.glitch_active) return;

    int stride = host->fb_stride / 4;
    uint32_t* fb = (uint32_t*)host->framebuffer;
    if (!fb) return;

    int y = g_app.glitch_y;
    int gh = g_app.glitch_h;
    if (y < 0 || y + gh >= h) return;

    // Shift a horizontal band by random offset
    int shift = (rand() % 40) - 20;
    // Copy the band shifted
    for (int r = y; r < y + gh && r < h; r++) {
        // Shift pixels
        uint32_t temp[2000];
        if (shift > 0) {
            for (int c = 0; c < w - shift; c++)
                temp[c] = fb[r * stride + c];
            for (int c = 0; c < w - shift; c++) {
                int src = c;
                int dst = c + shift;
                if (dst >= 0 && dst < w)
                    fb[r * stride + dst] = temp[src];
            }
        } else if (shift < 0) {
            int abs_shift = -shift;
            for (int c = abs_shift; c < w; c++)
                temp[c] = fb[r * stride + c];
            for (int c = abs_shift; c < w; c++) {
                int dst = c + shift;
                if (dst >= 0 && dst < w)
                    fb[r * stride + dst] = temp[c];
            }
        }
        // Add noise strip
        if (rand() % 3 == 0) {
            for (int c = 0; c < w; c += 2) {
                if (c + r + rand() % 5 < w)
                    fb[r * stride + c + (rand() % 5)] = 0xFFFFFFFF;
            }
        }
    }

    g_app.glitch_active = 0;
}

static void update_glitch(double dt) {
    g_app.glitch_timer += (int)(dt * 60.0);
    // Every ~5 seconds, trigger a glitch for 1 frame
    if (g_app.glitch_timer > 300) {
        g_app.glitch_timer = 0;
        if (rand() % 3 == 0) {
            g_app.glitch_active = 1;
            g_app.glitch_y = rand() % (720 - 60);
            g_app.glitch_h = 10 + rand() % 50;
        }
    }
}

// ============================================================================
//  PANEL FRAME (glowing transparent panel)
// ============================================================================

static void draw_panel_frame(KainWin32UiHost* host, int x, int y, int w, int h,
                              const char* title, uint32_t border_color,
                              KainUiWidgetContext* ctx) {
    // Glow behind panel
    uint32_t glow_c = (border_color & 0x00FFFFFF) | (0x18 << 24);
    for (int g = 6; g > 0; g -= 2) {
        fill_rect(host, x - g, y - g, w + g * 2, h + g * 2, glow_c);
    }

    // Panel background (very dark, semi-transparent)
    fill_rect(host, x, y, w, h, 0x66080814);

    // Border
    draw_rect_border(host, x, y, w, h, border_color);
    // Double border effect (thin inner line)
    draw_rect_border(host, x + 1, y + 1, w - 2, h - 2,
                     (border_color & 0x00FFFFFF) | (0x50 << 24));

    // Corner accents
    int corner_len = 12;
    uint32_t corner_color = border_color;
    // Top-left
    draw_line(host, x + 2, y + corner_len, x + 2, y + 2, corner_color);
    draw_line(host, x + 2, y + 2, x + corner_len, y + 2, corner_color);
    // Top-right
    draw_line(host, x + w - 3, y + corner_len, x + w - 3, y + 2, corner_color);
    draw_line(host, x + w - corner_len, y + 2, x + w - 3, y + 2, corner_color);
    // Bottom-left
    draw_line(host, x + 2, y + h - 3, x + 2, y + h - corner_len, corner_color);
    draw_line(host, x + 2, y + h - 3, x + corner_len, y + h - 3, corner_color);
    // Bottom-right
    draw_line(host, x + w - 3, y + h - 3, x + w - 3, y + h - corner_len, corner_color);
    draw_line(host, x + w - corner_len, y + h - 3, x + w - 3, y + h - 3, corner_color);

    // Title
    if (title && title[0] && ctx && g_app.font_heading > 0) {
        ui_widget_draw_text_ex(ctx, x + 10, y + 6, title,
                               border_color, 14, g_app.font_heading);
    }
}

// ============================================================================
//  FONT LOADING
// ============================================================================

static void load_fonts(KainUiWidgetContext* ctx) {
    // Load 6+ different fonts for the retro wave panels
    g_app.font_body    = ui_widget_load_font(ctx, "C:/Windows/Fonts/segoeui.ttf", 14.0);
    g_app.font_heading = ui_widget_load_font(ctx, "C:/Windows/Fonts/impact.ttf", 16.0);
    g_app.font_mono    = ui_widget_load_font(ctx, "C:/Windows/Fonts/consola.ttf", 14.0);
    g_app.font_clock   = ui_widget_load_font(ctx, "C:/Windows/Fonts/arialbd.ttf", 48.0);
    g_app.font_fancy   = ui_widget_load_font(ctx, "C:/Windows/Fonts/Gabriola.ttf", 14.0);
    g_app.font_label   = ui_widget_load_font(ctx, "C:/Windows/Fonts/verdana.ttf", 12.0);

    // Fallback: if any font failed to load, try alternatives
    if (g_app.font_body <= 0)
        g_app.font_body = ui_widget_load_font(ctx, "C:/Windows/Fonts/arial.ttf", 14.0);
    if (g_app.font_heading <= 0)
        g_app.font_heading = ui_widget_load_font(ctx, "C:/Windows/Fonts/arialbd.ttf", 16.0);
    if (g_app.font_mono <= 0)
        g_app.font_mono = ui_widget_load_font(ctx, "C:/Windows/Fonts/cour.ttf", 14.0);
    if (g_app.font_clock <= 0)
        g_app.font_clock = ui_widget_load_font(ctx, "C:/Windows/Fonts/impact.ttf", 48.0);
    if (g_app.font_label <= 0)
        g_app.font_label = ui_widget_load_font(ctx, "C:/Windows/Fonts/arial.ttf", 12.0);

    // Set one font as the widget default
    if (g_app.font_body > 0)
        ctx->default_font = 0; // First font loaded = default

    printf("  Fonts loaded:\n");
    printf("    body:    %lld\n", (long long)g_app.font_body);
    printf("    heading: %lld\n", (long long)g_app.font_heading);
    printf("    mono:    %lld\n", (long long)g_app.font_mono);
    printf("    clock:   %lld\n", (long long)g_app.font_clock);
    printf("    fancy:   %lld\n", (long long)g_app.font_fancy);
    printf("    label:   %lld\n", (long long)g_app.font_label);
}

// ============================================================================
//  MAIN
// ============================================================================

int main(void) {
    // ── DPI scaling ───────────────────────────────────────────────────
    // Query monitor DPI and scale window so it's the right physical size
    // on high-DPI displays (4K+). Without this, the window is tiny.
    SetProcessDPIAware();
    HDC dpi_dc = GetDC(NULL);
    float dpi_scale = (float)GetDeviceCaps(dpi_dc, LOGPIXELSX) / 96.0f;
    ReleaseDC(NULL, dpi_dc);
    if (dpi_scale < 1.0f) dpi_scale = 1.0f;

    int win_w = (int)(SCREEN_W * dpi_scale + 0.5f);
    int win_h = (int)(SCREEN_H * dpi_scale + 0.5f);
    printf("[DPI] Scale: %.2f, Window: %dx%d (logical %dx%d)\n",
           dpi_scale, win_w, win_h, SCREEN_W, SCREEN_H);

    printf("╔═══════════════════════════════════════════════╗\n");
    printf("║     RETRO WAVE 2084 — Kain Native UI Demo     ║\n");
    printf("╚═══════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Build: " __DATE__ " " __TIME__ "\n\n");

    // Seed RNG
    srand((unsigned int)time(NULL));

    // Init color schemes
    init_color_schemes();

    // Init app state
    memset(&g_app, 0, sizeof(g_app));
    g_app.running = 1;
    g_app.grid_enabled = 1;
    g_app.anim_enabled = 1;
    g_app.grid_speed = 2.0;
    g_app.control_slider_val = 0.35;
    strcpy(g_app.status_text, "SESSION_ACTIVE");
    g_app.fps_str[0] = '\0';

    // Init rain
    init_text_rain();
    init_bouncers();

    // Create Kain UI session
    printf("[1/5] Creating UI session...\n");
    abi_ui_reset();
    g_app.session_id = abi_ui_session_create("RetroWave2084", win_w, win_h);
    if (g_app.session_id <= 0) {
        fprintf(stderr, "FAIL: session_create\n");
        return 1;
    }

    abi_ui_window_open(g_app.session_id, "RETRO WAVE 2084 — Kain Native UI Demo", win_w, win_h);
    if (abi_ui_host_attach(g_app.session_id, "winit") != 0) {
        fprintf(stderr, "FAIL: host_attach\n");
        return 1;
    }

    // Get host state
    KainNativeUiSession* ks = abi_ui_find_session(g_app.session_id);
    if (!ks || !ks->host_state) {
        fprintf(stderr, "FAIL: no host state\n");
        return 1;
    }
    g_host = (KainWin32UiHost*)ks->host_state;
    printf("  Window: %dx%d  hwnd=%p  fb=%p\n",
           g_host->width, g_host->height, (void*)g_host->hwnd, (void*)g_host->framebuffer);

    // Set the window title (abi_ui_window_open copies to session struct but
    // doesn't call SetWindowTextA)
    SetWindowTextA(g_host->hwnd, "RETRO WAVE 2084 — Kain Native UI Demo");

    // Subclass window proc — chain into original for non-WM_PAINT messages
    g_orig_wndproc = (WNDPROC)SetWindowLongPtrA(g_host->hwnd, GWLP_WNDPROC,
                                                  (LONG_PTR)retrowave_wndproc);
    // Store host in window user data for WM_PAINT access
    SetWindowLongPtrA(g_host->hwnd, GWLP_USERDATA, (LONG_PTR)g_host);

    // Create widget context
    printf("[2/5] Creating widget context...\n");
    KainUiWidgetContext* ctx = ui_widget_create(g_app.session_id);
    if (!ctx) {
        fprintf(stderr, "FAIL: widget_create\n");
        return 1;
    }

    // Override widget colors with retro wave scheme
    ctx->color_bg       = 0xFF0A0A1A;
    ctx->color_surface  = 0x1A1A0030;
    ctx->color_header   = 0xFF0D0D20;
    ctx->color_accent   = 0xFFFF00AA;
    ctx->color_text     = 0xFFF0E0FF;
    ctx->color_text_dim = 0xFF8888C0;

    // Load fonts
    printf("[3/5] Loading fonts...\n");
    load_fonts(ctx);
    if (g_app.font_body <= 0) {
        fprintf(stderr, "WARNING: No fonts loaded — text will not render\n");
    }

    // Set up node tree
    printf("[4/5] Creating node tree...\n");
    int64_t root = abi_ui_node_create(g_app.session_id, "root");
    abi_ui_node_set_rect(g_app.session_id, root, 0, 0, win_w, win_h);

    int64_t bg = abi_ui_node_create(g_app.session_id, "bg");
    abi_ui_node_set_parent(g_app.session_id, bg, root);
    abi_ui_node_set_rect(g_app.session_id, bg, 0, 0, win_w, win_h);
    abi_ui_node_set_style_string(g_app.session_id, bg, "fill_color", "#0A0A1A");

    printf("[5/5] Entering main loop...\n");
    printf("\n");
    printf("========================================================\n");
    printf("  Controls:\n");
    printf("    G        — Toggle grid animation\n");
    printf("    C        — Cycle color schemes\n");
    printf("    SPACE    — Toggle all animation\n");
    printf("    ESC      — Exit\n");
    printf("    Click-drag cassette icons to throw them around!\n");
    printf("========================================================\n\n");

    // ── Main loop ────────────────────────────���────────────────────────
    MSG msg;
    LARGE_INTEGER freq, prev_time, curr_time;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev_time);

    while (g_app.running) {
        // Message pump
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { g_app.running = 0; break; }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!g_app.running) break;

        // Delta time
        QueryPerformanceCounter(&curr_time);
        double dt = (double)(curr_time.QuadPart - prev_time.QuadPart) / (double)freq.QuadPart;
        if (dt > 0.05) dt = 0.05; // clamp to 50ms max
        prev_time = curr_time;

        // Update time
        if (g_app.anim_enabled) {
            g_app.time += dt;
            g_app.grid_offset += dt * g_app.grid_speed;
        }
        g_app.frame++;

        // FPS tracking
        g_app.fps_count++;
        g_app.fps_timer += dt;
        if (g_app.fps_timer >= 1.0) {
            g_app.fps = (double)g_app.fps_count / g_app.fps_timer;
            snprintf(g_app.fps_str, sizeof(g_app.fps_str), "FPS: %.0f", g_app.fps);
            g_app.fps_count = 0;
            g_app.fps_timer = 0.0;
        }

        // Update systems
        if (g_app.anim_enabled) {
            update_equalizer(dt);
            update_cube(dt);
            update_sine_wave(dt);
            update_text_rain(dt);
            update_bouncers(dt, g_host->width, g_host->height, g_host);
            update_glitch(dt);
        }

        // Begin frame
        abi_ui_begin_frame(g_app.session_id, dt * 1000.0);
        ui_widget_begin_frame(ctx);

        // ── RENDER ────────────────────────────────────────────────────
        ColorScheme* cs = &g_schemes[g_app.color_scheme];

        if (g_host->framebuffer) {
            uint32_t* fb = (uint32_t*)g_host->framebuffer;
            int stride = g_host->fb_stride / 4;
            int w = g_host->width, h = g_host->height;

            // Clear framebuffer
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    fb[y * stride + x] = cs->bg;

            // 1. Perspective grid
            if (g_app.grid_enabled) {
                draw_perspective_grid(g_host, w, h, g_app.grid_offset, cs);
            }

            // 2. Neon sun
            draw_neon_sun(g_host, w, h, cs);

            // 3. Floating glowing panels
            // ── Panel 1: SYS.LINK (top-left, equalizer) ──────────────
            draw_panel_frame(g_host, 20, 20, 280, 200, "SYS.LINK",
                            cs->glow_pink, ctx);
            if (g_app.anim_enabled) update_equalizer(dt);
            draw_equalizer(g_host, 30, 30, 260, 180);

            // ── Panel 2: DATA.CORE (top-right, cube) ─────────────────
            draw_panel_frame(g_host, 980, 20, 280, 220, "DATA.CORE",
                            cs->glow_purple, ctx);
            if (g_app.anim_enabled) update_cube(dt);
            draw_wireframe_cube(g_host, 1120, 120, 80);

            // ── Panel 3: SIGNAL (center-left, sine wave) ─────────────
            draw_panel_frame(g_host, 20, 470, 500, 210, "SIGNAL",
                            cs->glow_cyan, ctx);
            if (g_app.anim_enabled) update_sine_wave(dt);
            draw_sine_wave(g_host, 30, 480, 480, 190);

            // ── Panel 4: TERMINAL (right, text rain) ────────────────
            draw_panel_frame(g_host, 940, 420, 320, 260, "TERMINAL",
                            cs->glow_cyan, ctx);
            if (g_app.anim_enabled) update_text_rain(dt);
            draw_text_rain(g_host, ctx, 950, 430, 300, 240);

            // ── Panel 5: CLOCK (top-center) ──────────────────────────
            draw_panel_frame(g_host, 440, 20, 400, 100, "CLOCK",
                            cs->glow_pink, ctx);
            draw_digital_clock(g_host, ctx, 450, 30, 380, 80);

            // 4. Bouncers (cassette tapes)
            for (int i = 0; i < MAX_BOUNCERS; i++) {
                uint32_t bcolor = (i == 0) ? cs->glow_pink : cs->glow_cyan;
                draw_cassette(g_host, (int)g_app.bx[i], (int)g_app.by[i], bcolor);
            }

            // 5. Glitch effect
            apply_glitch(g_host, w, h);

            // 6. Controls bar (bottom)
            draw_controls(ctx, w, h);

            // 7. Screen shake reset
            g_app.shake_x = 0;
            g_app.shake_y = 0;
        }

        // End frame
        ui_widget_end_frame(ctx);
        abi_ui_end_frame(g_app.session_id);

        // Do NOT call abi_ui_host_present() — it calls win32_host_render_framebuffer()
        // which overwrites our custom pixel work with node tree rendering!
        // Instead, trigger WM_PAINT directly via InvalidateRect.
        InvalidateRect(g_host->hwnd, NULL, FALSE);

        // Cap at ~60 FPS
        Sleep(16);
    }

    // ── Cleanup ───────────────────────────────────────────────────────
    printf("\nShutdown after %d frames (%.1f seconds).\n",
           g_app.frame, g_app.time);
    ui_widget_destroy(ctx);
    abi_ui_session_destroy(g_app.session_id);
    printf("Done.\n");
    return 0;
}

// ============================================================================
//  WINDOW PROCEDURE
// ============================================================================

static LRESULT CALLBACK retrowave_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CLOSE:
            if (g_host) g_host->running = 0;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            KainWin32UiHost* host = (KainWin32UiHost*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
            if (host && host->hdc_buffer) {
                BitBlt(hdc, 0, 0, host->width, host->height,
                       host->hdc_buffer, 0, 0, SRCCOPY);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN: {
            switch (wp) {
                case VK_ESCAPE:
                    PostQuitMessage(0);
                    return 0;
                case 'G':
                    g_app.grid_enabled = !g_app.grid_enabled;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                case 'C':
                    g_app.color_scheme = (g_app.color_scheme + 1) % 3;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                case VK_SPACE:
                    g_app.anim_enabled = !g_app.anim_enabled;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            int mx = (int)(short)LOWORD(lp);
            int my = (int)(short)HIWORD(lp);
            for (int i = 0; i < MAX_BOUNCERS; i++) {
                if (mx >= g_app.bx[i] && mx < g_app.bx[i] + 50 &&
                    my >= g_app.by[i] && my < g_app.by[i] + 34) {
                    g_app.dragging_bouncer = i;
                    g_app.drag_off_x = mx - g_app.bx[i];
                    g_app.drag_off_y = my - g_app.by[i];
                    g_app.bvx[i] = 0; g_app.bvy[i] = 0;
                    return 0;
                }
            }
            break;
        }
        case WM_MOUSEMOVE: {
            if (g_app.dragging_bouncer >= 0) {
                int mx = (int)(short)LOWORD(lp);
                int my = (int)(short)HIWORD(lp);
                g_app.bx[g_app.dragging_bouncer] = mx - g_app.drag_off_x;
                g_app.by[g_app.dragging_bouncer] = my - g_app.drag_off_y;
                return 0;
            }
            break;
        }
        case WM_LBUTTONUP: {
            if (g_app.dragging_bouncer >= 0) {
                g_app.bvx[g_app.dragging_bouncer] = (double)(rand() % 200 - 100) / 100.0;
                g_app.bvy[g_app.dragging_bouncer] = (double)(rand() % 200 - 100) / 100.0;
                g_app.dragging_bouncer = -1;
                return 0;
            }
            break;
        }
    }
    // Chain everything else to the original wndproc
    return CallWindowProcA(g_orig_wndproc, hwnd, msg, wp, lp);
}
