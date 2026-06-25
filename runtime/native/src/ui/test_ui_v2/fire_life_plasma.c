// ============================================================================
//  fire_life_plasma.c — "FIRE + LIFE + PLASMA" Triple-Threat Demo
//  ============================================================================
//  Three classic algorithmic effects running simultaneously, each in its own
//  panel, with loaded fonts for labels and 6 cycling color palettes.
//
//  Features:
//    Panel 1: DOOM FIRE  — classic Doom engine fire at 320x200
//    Panel 2: GAME OF LIFE — Conway's Game of Life at 100x100
//    Panel 3: PLASMA     — animated plasma at 400x400
//    6 color palettes cycling every 30s across all panels
//    6+ loaded fonts for title, labels, status, legend
//    Mouse interaction: hover fire = blow on fire, click life = toggle cell
//    Keyboard: Space=pause, F=reset fire, G=rand life, P=cycle palette,
//              1/2/3=toggle panels, Esc=exit
//    FPS counter, total pixels, frame time in header/status bars
//
//  Build:
//    cd X:\runtime\native\src\ui\test_ui_v2
//    build.bat fire_life_plasma
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

#include "ui_system.h"
#include "ui_system_internal.h"
#include "ui_font.h"
#include "ui_color.h"
#include "ui_renderer.h"
#include "ui_widget.h"

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
    float dpi_scale;
} KainWin32UiHost;

// ============================================================================
//  CONSTANTS
// ============================================================================

#define WINDOW_W         1400
#define WINDOW_H         800
#define HEADER_H         48
#define STATUS_H         32
#define PANEL_GAP        12
#define PANEL_TOP        56

#define FIRE_W           320
#define FIRE_H           200
#define LIFE_W           100
#define LIFE_H           100
#define PLASMA_W         400
#define PLASMA_H         400
#define CELL_SIZE        3                     // Life cell size in pixels
#define MAX_PALETTES     6
#define PALETTE_SIZE     256

// ── DPI scaling ──────────────────────────────────────────────────────
static double g_dpi_scale = 1.0;

// ── Panel positions (computed at runtime from WINDOW_W/PANEL_TOP/etc) ─
static int g_fire_x, g_fire_y, g_life_y, g_plasma_y;
static int g_pw;           // panel width
static int g_life_x;
static int g_plasma_x;
static int g_fire_ox, g_fire_oy;               // fire buffer offset inside panel
static int g_life_ox, g_life_oy;               // life offset inside panel
static int g_plasma_ox, g_plasma_oy;           // plasma offset inside panel

static void compute_layout(void) {
    double ds = g_dpi_scale;
    int aw = (int)(WINDOW_W * ds + 0.5);
    int gap = (int)(PANEL_GAP * ds + 0.5);
    g_pw = (aw - gap * 4) / 3;
    int ph = (int)(WINDOW_H * ds + 0.5) - (int)(PANEL_TOP * ds + 0.5) - (int)(STATUS_H * ds + 0.5) - gap;
    int label_h = (int)(28 * ds + 0.5);

    g_fire_x   = gap;
    g_life_x   = gap * 2 + g_pw;
    g_plasma_x = gap * 3 + g_pw * 2;
    g_fire_y = g_life_y = g_plasma_y = (int)(PANEL_TOP * ds + 0.5);

    // Center each effect in its panel
    g_fire_ox   = g_fire_x   + (g_pw - FIRE_W) / 2;
    g_fire_oy   = g_fire_y   + label_h + (ph - label_h - FIRE_H) / 2;
    g_life_ox   = g_life_x   + (g_pw - LIFE_W * CELL_SIZE) / 2;
    g_life_oy   = g_life_y   + label_h + (ph - label_h - LIFE_H * CELL_SIZE) / 2;
    g_plasma_ox = g_plasma_x + (g_pw - PLASMA_W) / 2;
    g_plasma_oy = g_plasma_y + label_h + (ph - label_h - PLASMA_H) / 2;
}

// ============================================================================
//  COLOR PALETTE SYSTEM — 6 palettes × 256 entries
// ============================================================================

static uint32_t g_palettes[MAX_PALETTES][PALETTE_SIZE];
static int g_current_palette = 0;          // 0-5
static double g_palette_switch_timer = 0.0;
static const double PALETTE_SWITCH_INTERVAL = 30.0;  // seconds

// Key color stops for each palette (position 0.0–1.0, color 0xAARRGGBB)
typedef struct { float pos; uint32_t color; } ColorStop;
#define STOP(pos, color) { pos, color }

static void build_palette(uint32_t* pal, const ColorStop* stops, int nstops) {
    for (int i = 0; i < PALETTE_SIZE; i++) {
        float t = (float)i / (float)(PALETTE_SIZE - 1);
        // Find two stops to interpolate between
        int si = 0;
        for (int s = 0; s < nstops - 1; s++) {
            if (t >= stops[s].pos) si = s;
        }
        if (si >= nstops - 1) si = nstops - 2;
        float lo = stops[si].pos, hi = stops[si + 1].pos;
        float frac = (hi > lo) ? (t - lo) / (hi - lo) : 0.0f;
        if (frac < 0.0f) frac = 0.0f;
        if (frac > 1.0f) frac = 1.0f;

        uint32_t c0 = stops[si].color, c1 = stops[si + 1].color;
        uint8_t r = (uint8_t)(((c0 >> 16) & 0xFF) * (1.0f - frac) + ((c1 >> 16) & 0xFF) * frac);
        uint8_t g = (uint8_t)(((c0 >> 8) & 0xFF)  * (1.0f - frac) + ((c1 >> 8) & 0xFF)  * frac);
        uint8_t b = (uint8_t)((c0 & 0xFF)          * (1.0f - frac) + (c1 & 0xFF)          * frac);
        pal[i] = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
}

static void init_palettes(void) {
    // 0: CLASSIC — Doom fire (dark → red → orange → yellow → white)
    ColorStop classic[] = {
        STOP(0.00f, 0xFF000000), STOP(0.10f, 0xFF1A0000), STOP(0.20f, 0xFF440000),
        STOP(0.30f, 0xFF880000), STOP(0.40f, 0xFFCC2200), STOP(0.50f, 0xFFFF4400),
        STOP(0.60f, 0xFFFF6600), STOP(0.70f, 0xFFFFAA00), STOP(0.80f, 0xFFFFCC00),
        STOP(0.90f, 0xFFFFEE44), STOP(1.00f, 0xFFFFFFFF)
    };
    build_palette(g_palettes[0], classic, 11);

    // 1: INFERNO — black → purple → red → orange → yellow
    ColorStop inferno[] = {
        STOP(0.00f, 0xFF000000), STOP(0.10f, 0xFF0C0020), STOP(0.20f, 0xFF2A0055),
        STOP(0.30f, 0xFF5A0055), STOP(0.40f, 0xFF8A0044), STOP(0.50f, 0xFFCC0033),
        STOP(0.60f, 0xFFFF4422), STOP(0.70f, 0xFFFF7722), STOP(0.80f, 0xFFFFAA22),
        STOP(0.90f, 0xFFFFCC44), STOP(1.00f, 0xFFFFEE66)
    };
    build_palette(g_palettes[1], inferno, 11);

    // 2: OCEAN — dark blue → cyan → teal → white
    ColorStop ocean[] = {
        STOP(0.00f, 0xFF000000), STOP(0.10f, 0xFF001020), STOP(0.20f, 0xFF002266),
        STOP(0.30f, 0xFF003388), STOP(0.40f, 0xFF0055AA), STOP(0.50f, 0xFF0088CC),
        STOP(0.60f, 0xFF00AADD), STOP(0.70f, 0xFF00CCEE), STOP(0.80f, 0xFF44DDFF),
        STOP(0.90f, 0xFF99EEFF), STOP(1.00f, 0xFFFFFFFF)
    };
    build_palette(g_palettes[2], ocean, 11);

    // 3: NEON — purple → pink → cyan → white
    ColorStop neon[] = {
        STOP(0.00f, 0xFF000000), STOP(0.10f, 0xFF220033), STOP(0.20f, 0xFF550088),
        STOP(0.30f, 0xFF8800AA), STOP(0.40f, 0xFFBB00CC), STOP(0.50f, 0xFFFF00AA),
        STOP(0.60f, 0xFFFF44CC), STOP(0.70f, 0xFFFF88DD), STOP(0.80f, 0xFF00FFDD),
        STOP(0.90f, 0xFF88FFEE), STOP(1.00f, 0xFFFFFFFF)
    };
    build_palette(g_palettes[3], neon, 11);

    // 4: MATRIX — black → dark green → bright green → white
    ColorStop matrix[] = {
        STOP(0.00f, 0xFF000000), STOP(0.10f, 0xFF001100), STOP(0.20f, 0xFF002200),
        STOP(0.30f, 0xFF004400), STOP(0.40f, 0xFF006600), STOP(0.50f, 0xFF009900),
        STOP(0.60f, 0xFF00CC00), STOP(0.70f, 0xFF00FF00), STOP(0.80f, 0xFF44FF44),
        STOP(0.90f, 0xFF88FF88), STOP(1.00f, 0xFFFFFFFF)
    };
    build_palette(g_palettes[4], matrix, 11);

    // 5: AURORA — dark → purple → green → cyan → white
    ColorStop aurora[] = {
        STOP(0.00f, 0xFF000000), STOP(0.10f, 0xFF0A0020), STOP(0.20f, 0xFF220055),
        STOP(0.30f, 0xFF335522), STOP(0.40f, 0xFF007744), STOP(0.50f, 0xFF00AA55),
        STOP(0.60f, 0xFF00CC77), STOP(0.70f, 0xFF44DDAA), STOP(0.80f, 0xFF88EECC),
        STOP(0.90f, 0xFFCCFFEE), STOP(1.00f, 0xFFFFFFFF)
    };
    build_palette(g_palettes[5], aurora, 11);
}

static uint32_t palette_color(int pidx, int heat) {
    if (heat < 0) heat = 0;
    if (heat >= PALETTE_SIZE) heat = PALETTE_SIZE - 1;
    return g_palettes[pidx][heat];
}

static const char* palette_names[MAX_PALETTES] = {
    "CLASSIC", "INFERNO", "OCEAN", "NEON", "MATRIX", "AURORA"
};

// ============================================================================
//  DOOM FIRE — 320×200 buffer, classic spread + cool algorithm
// ============================================================================

static uint8_t g_fire_buf[FIRE_H][FIRE_W];
static int g_fire_gen = 0;

static void fire_reset(void) {
    memset(g_fire_buf, 0, sizeof(g_fire_buf));
    g_fire_gen = 0;
}

static void fire_update(void) {
    // Ignite bottom row randomly
    for (int x = 0; x < FIRE_W; x++) {
        if ((rand() % 4) == 0)
            g_fire_buf[FIRE_H - 1][x] = 255;
        else
            g_fire_buf[FIRE_H - 1][x] = (uint8_t)(200 + rand() % 56);
    }

    // Propagate upward with cooling
    for (int y = 0; y < FIRE_H - 1; y++) {
        for (int x = 0; x < FIRE_W; x++) {
            int xm1 = (x > 0) ? x - 1 : 0;
            int xp1 = (x < FIRE_W - 1) ? x + 1 : FIRE_W - 1;
            int xm2 = (x > 1) ? x - 2 : 0;

            int sum = (int)g_fire_buf[y + 1][x]
                    + (int)g_fire_buf[y + 1][xm1]
                    + (int)g_fire_buf[y + 1][xp1]
                    + (int)g_fire_buf[y + 1][xm2] * 2;
            int val = sum / 5;

            // Cooling: subtract random amount
            int cool = rand() & 3;
            val -= cool;
            if (val < 0) val = 0;
            if (val > 255) val = 255;

            g_fire_buf[y][x] = (uint8_t)val;
        }
    }
    g_fire_gen++;
}

static void fire_add_heat(int mx, int my, int radius, uint8_t heat) {
    // Convert screen coords to fire buffer coords
    int fx = mx - g_fire_ox;
    int fy = my - g_fire_oy;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int sx = fx + dx, sy = fy + dy;
            if (sx >= 0 && sx < FIRE_W && sy >= 0 && sy < FIRE_H) {
                float dist = sqrtf((float)(dx*dx + dy*dy));
                if (dist <= (float)radius) {
                    float falloff = 1.0f - dist / (float)(radius + 1);
                    uint8_t h = (uint8_t)((int)heat * falloff);
                    if (h > g_fire_buf[sy][sx])
                        g_fire_buf[sy][sx] = h;
                }
            }
        }
    }
}

// ============================================================================
//  GAME OF LIFE — 100×100, B3/S23, age tracking
// ============================================================================

static uint8_t g_life_buf[LIFE_H][LIFE_W];       // 0=dead, 1=alive
static int g_life_age[LIFE_H][LIFE_W];            // generation age (0=dead, 1+)
static int g_life_gen = 0;
static int g_life_alive = 0;

static void life_randomize(void) {
    for (int y = 0; y < LIFE_H; y++) {
        for (int x = 0; x < LIFE_W; x++) {
            g_life_buf[y][x] = (rand() % 100 < 25) ? 1 : 0;
            g_life_age[y][x] = g_life_buf[y][x] ? 1 : 0;
        }
    }
    g_life_gen = 0;
    // Count alive
    g_life_alive = 0;
    for (int y = 0; y < LIFE_H; y++)
        for (int x = 0; x < LIFE_W; x++)
            if (g_life_buf[y][x]) g_life_alive++;
}

static void life_toggle(int mx, int my) {
    int lx = (mx - g_life_ox) / CELL_SIZE;
    int ly = (my - g_life_oy) / CELL_SIZE;
    if (lx >= 0 && lx < LIFE_W && ly >= 0 && ly < LIFE_H) {
        g_life_buf[ly][lx] = !g_life_buf[ly][lx];
        g_life_age[ly][lx] = g_life_buf[ly][lx] ? 1 : 0;
    }
}

static int count_neighbors(int y, int x) {
    int n = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int ny = y + dy, nx = x + dx;
            if (ny >= 0 && ny < LIFE_H && nx >= 0 && nx < LIFE_W)
                n += g_life_buf[ny][nx];
        }
    }
    return n;
}

static void life_update(void) {
    uint8_t next[LIFE_H][LIFE_W] = {0};
    int next_age[LIFE_H][LIFE_W] = {0};

    g_life_alive = 0;
    for (int y = 0; y < LIFE_H; y++) {
        for (int x = 0; x < LIFE_W; x++) {
            int n = count_neighbors(y, x);
            int alive = g_life_buf[y][x];

            if (alive) {
                if (n == 2 || n == 3) {
                    next[y][x] = 1;
                    next_age[y][x] = g_life_age[y][x] + 1;
                    if (next_age[y][x] > 255) next_age[y][x] = 255;
                    g_life_alive++;
                }
            } else {
                if (n == 3) {
                    next[y][x] = 1;
                    next_age[y][x] = 1;
                    g_life_alive++;
                }
            }
        }
    }
    memcpy(g_life_buf, next, sizeof(g_life_buf));
    memcpy(g_life_age, next_age, sizeof(g_life_age));
    g_life_gen++;
}

// ============================================================================
//  PLASMA — animated 400×400 classic plasma
// ============================================================================

static double g_plasma_time = 0.0;
static int g_plasma_palette = 0;             // palette index for plasma (can be independent)
static double g_plasma_freq1 = 0.02;
static double g_plasma_freq2 = 0.03;
static double g_plasma_freq3 = 0.04;
static double g_plasma_freq4 = 0.06;

static void plasma_cycle_palette(void) {
    g_plasma_palette = (g_plasma_palette + 1) % MAX_PALETTES;
}

static void plasma_update(double dt) {
    g_plasma_time += dt * 0.7;
}

// Bilinearly interpolated pixel from a 4-sample plasma
static int plasma_sample(int x, int y, double time) {
    double v1 = sin(x * g_plasma_freq1 + time);
    double v2 = sin(y * g_plasma_freq2 + time * 1.3);
    double v3 = sin((x + y) * g_plasma_freq3 + time * 0.7);
    double v4 = sin(sqrt((double)(x * x + y * y)) * g_plasma_freq4 + time * 1.1);
    double v = (v1 + v2 + v3 + v4) / 4.0;          // -1.0 to 1.0
    v = (v + 1.0) * 0.5;                            // 0.0 to 1.0
    if (v < 0.0) v = 0.0;
    if (v > 1.0) v = 1.0;
    return (int)(v * (PALETTE_SIZE - 1) + 0.5);
}

// ============================================================================
//  APPLICATION STATE
// ============================================================================

typedef struct {
    int running;
    int paused;
    int64_t session_id;
    KainWin32UiHost* host;
    KainUiWidgetContext* ctx;

    // Font IDs
    int64_t font_title;      // Impact 28 — demo title
    int64_t font_label;      // Verdana 14 — panel labels
    int64_t font_mono;       // Consolas 12 — status, FPS
    int64_t font_heading;    // Arial Bold 16 — panel headers
    int64_t font_body;       // Segoe UI 13 — parameter text
    int64_t font_legend;     // Tahoma 11 — key legend
    int64_t font_fancy;      // Gabriola 16 — decorative

    // Panel visibility
    int show_fire;
    int show_life;
    int show_plasma;

    // Performance
    double fps;
    double fps_timer;
    int fps_count;
    double total_time;
    int64_t frame;
    double frame_time_ms;
    char fps_str[32];
    char status_str[128];

    // Mouse state
    int mouse_x, mouse_y;
    int mouse_down;
    int prev_mouse_down;

    // Fire mouse interaction
    int fire_hovered;

    // Life interaction
    int life_click_handled;
} AppState;

static AppState g_app;
static KainWin32UiHost* g_host = NULL;      // alias for wndproc
static WNDPROC g_orig_wndproc = NULL;

// ============================================================================
//  PIXEL HELPERS
// ============================================================================

static void write_px(uint32_t* fb, int stride, int x, int y, int fb_w, int fb_h, uint32_t color) {
    if (x < 0 || x >= fb_w || y < 0 || y >= fb_h) return;
    fb[y * stride + x] = color;
}

// Safe alpha blend at a pixel position
static void blend_px(uint32_t* fb, int stride, int x, int y, int fb_w, int fb_h, uint32_t color) {
    if (x < 0 || x >= fb_w || y < 0 || y >= fb_h) return;
    uint32_t* dst = &fb[y * stride + x];
    uint8_t sa = (color >> 24) & 0xFF;
    if (sa == 0) return;
    if (sa == 255) { *dst = color; return; }
    uint8_t sr = (color >> 16) & 0xFF, sg = (color >> 8) & 0xFF, sb = color & 0xFF;
    uint8_t da = 255 - sa;
    uint8_t dr = ((uint16_t)sr * sa + ((*dst >> 16) & 0xFF) * da) / 255;
    uint8_t dg = ((uint16_t)sg * sa + ((*dst >> 8) & 0xFF) * da) / 255;
    uint8_t db = ((uint16_t)sb * sa + (*dst & 0xFF) * da) / 255;
    *dst = 0xFF000000 | ((uint32_t)dr << 16) | ((uint32_t)dg << 8) | db;
}

// Fill rect with bounds checking
static void fill_rect(uint32_t* fb, int stride, int x, int y, int w, int h,
                       int fb_w, int fb_h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    for (int r = y; r < y + h && r < fb_h; r++) {
        if (r < 0) continue;
        for (int c = x; c < x + w && c < fb_w; c++) {
            if (c < 0) continue;
            blend_px(fb, stride, c, r, fb_w, fb_h, color);
        }
    }
}

// Draw rect border (1px)
static void rect_border(uint32_t* fb, int stride, int x, int y, int w, int h,
                         int fb_w, int fb_h, uint32_t color) {
    fill_rect(fb, stride, x, y, w, 1, fb_w, fb_h, color);
    fill_rect(fb, stride, x, y + h - 1, w, 1, fb_w, fb_h, color);
    fill_rect(fb, stride, x, y, 1, h, fb_w, fb_h, color);
    fill_rect(fb, stride, x + w - 1, y, 1, h, fb_w, fb_h, color);
}

// ============================================================================
//  FONT LOADING
// ============================================================================

static void load_fonts(void) {
    KainUiWidgetContext* ctx = g_app.ctx;
    // Note: ui_widget_load_font scales internally by ctx->dpi_scale,
    // so pass logical sizes without pre-scaling.
    g_app.font_title   = ui_widget_load_font(ctx, "C:/Windows/Fonts/impact.ttf", 26.0);
    g_app.font_heading = ui_widget_load_font(ctx, "C:/Windows/Fonts/arialbd.ttf", 15.0);
    g_app.font_label   = ui_widget_load_font(ctx, "C:/Windows/Fonts/verdana.ttf", 13.0);
    g_app.font_mono    = ui_widget_load_font(ctx, "C:/Windows/Fonts/consola.ttf", 12.0);
    g_app.font_body    = ui_widget_load_font(ctx, "C:/Windows/Fonts/segoeui.ttf", 13.0);
    g_app.font_legend  = ui_widget_load_font(ctx, "C:/Windows/Fonts/tahoma.ttf", 11.0);
    g_app.font_fancy   = ui_widget_load_font(ctx, "C:/Windows/Fonts/Gabriola.ttf", 16.0);

    // Fallbacks
    if (g_app.font_title <= 0)
        g_app.font_title = ui_widget_load_font(ctx, "C:/Windows/Fonts/arialbd.ttf", 26.0);
    if (g_app.font_body <= 0)
        g_app.font_body = ui_widget_load_font(ctx, "C:/Windows/Fonts/arial.ttf", 13.0);
    if (g_app.font_heading <= 0)
        g_app.font_heading = g_app.font_title;
    if (g_app.font_label <= 0)
        g_app.font_label = ui_widget_load_font(ctx, "C:/Windows/Fonts/arial.ttf", 13.0);
    if (g_app.font_mono <= 0)
        g_app.font_mono = ui_widget_load_font(ctx, "C:/Windows/Fonts/cour.ttf", 12.0);
    if (g_app.font_legend <= 0)
        g_app.font_legend = g_app.font_mono;

    printf("  Fonts: title=%lld heading=%lld label=%lld mono=%lld body=%lld legend=%lld fancy=%lld\n",
           (long long)g_app.font_title, (long long)g_app.font_heading,
           (long long)g_app.font_label, (long long)g_app.font_mono,
           (long long)g_app.font_body, (long long)g_app.font_legend,
           (long long)g_app.font_fancy);
}

// ── Draw text helpers ─────────────────────────────────────────────────
static void draw_text(int x, int y, const char* text, uint32_t color, int64_t font_id) {
    if (font_id > 0 && text && text[0])
        ui_widget_draw_text_ex(g_app.ctx, x, y, text, color, 0, font_id);
}

static int text_width(const char* text, int64_t font_id) {
    if (!text || !text[0] || font_id <= 0) return 0;
    int tw = (int)(abi_ui_text_measure_width(g_app.session_id, font_id, text) + 0.5);
    return tw > 0 ? tw : (int)strlen(text) * 8;
}

// ============================================================================
//  PANEL DRAWING
// ============================================================================

static void draw_panel_bg(uint32_t* fb, int stride, int fb_w, int fb_h,
                           int x, int y, int w, int h, uint32_t accent)
{
    // Panel background (dark semi-transparent)
    fill_rect(fb, stride, x, y, w, h, fb_w, fb_h, 0xCC0A0A18);

    // Glow behind panel
    uint32_t glow_c = (accent & 0x00FFFFFF) | (0x12 << 24);
    for (int g = 4; g > 0; g -= 2)
        fill_rect(fb, stride, x - g, y - g, w + g * 2, h + g * 2, fb_w, fb_h, glow_c);

    // Accent top bar (colored line)
    fill_rect(fb, stride, x, y, w, 2, fb_w, fb_h, accent);

    // Border
    rect_border(fb, stride, x, y, w, h, fb_w, fb_h, (accent & 0x00FFFFFF) | (0x60 << 24));
    rect_border(fb, stride, x + 1, y + 1, w - 2, h - 2, fb_w, fb_h,
                (accent & 0x00FFFFFF) | (0x25 << 24));

    // Corner accents
    uint32_t ca = accent;
    for (int c = 0; c < 8; c++) {
        write_px(fb, stride, x + 2 + c, y + 2, fb_w, fb_h, ca);
        write_px(fb, stride, x + w - 3 - c, y + 2, fb_w, fb_h, ca);
        write_px(fb, stride, x + 2 + c, y + h - 3, fb_w, fb_h, ca);
        write_px(fb, stride, x + w - 3 - c, y + h - 3, fb_w, fb_h, ca);
        write_px(fb, stride, x + 2, y + 2 + c, fb_w, fb_h, ca);
        write_px(fb, stride, x + w - 3, y + 2 + c, fb_w, fb_h, ca);
        write_px(fb, stride, x + 2, y + h - 3 - c, fb_w, fb_h, ca);
        write_px(fb, stride, x + w - 3, y + h - 3 - c, fb_w, fb_h, ca);
    }
}

// ============================================================================
//  RENDER FRAME
// ============================================================================

static void render_frame(double dt) {
    if (!g_host || !g_host->framebuffer) return;
    double ds = g_dpi_scale;
    uint32_t* fb = (uint32_t*)g_host->framebuffer;
    int stride = g_host->fb_stride / 4;
    int fb_w = g_host->width;
    int fb_h = g_host->height;

    uint32_t bg = 0xFF08080E;
    int pal_idx = g_current_palette;

    int header_h = (int)(HEADER_H * ds + 0.5);
    int status_h = (int)(STATUS_H * ds + 0.5);
    int gap = (int)(PANEL_GAP * ds + 0.5);
    int label_h = (int)(28 * ds + 0.5);

    // --- 1. Clear framebuffer to dark background ----------------------
    for (int y = 0; y < fb_h; y++)
        for (int x = 0; x < fb_w; x++)
            fb[y * stride + x] = bg;

    // --- 2. Draw header bar ------------------------------------------
    {
        // Dark gradient header
        for (int y = 0; y < header_h && y < fb_h; y++) {
            float t = (float)y / (float)header_h;
            uint32_t hcol = (uint32_t)((40 + (1.0f - t) * 20)) << 24;
            hcol |= 0x00080A18;
            for (int x = 0; x < fb_w; x++)
                fb[y * stride + x] = hcol;
        }
        // Accent underline
        uint32_t accent_bar = palette_color(pal_idx, 200);
        for (int x = 0; x < fb_w; x++)
            fb[(header_h - 2) * stride + x] = accent_bar;

        // Title
        int tx = (int)(14 * ds + 0.5);
        draw_text(tx, (int)(11 * ds + 0.5), "FIRE + LIFE + PLASMA", 0xFFE8E8F0, g_app.font_title);

        // Palette name badge
        char pal_badge[32];
        snprintf(pal_badge, sizeof(pal_badge), "[ %s ]", palette_names[pal_idx]);
        int pbw = text_width(pal_badge, g_app.font_mono);
        draw_text(fb_w - pbw - (int)(160 * ds + 0.5), (int)(15 * ds + 0.5), pal_badge, accent_bar, g_app.font_mono);

        // FPS in header right
        draw_text(fb_w - text_width(g_app.fps_str, g_app.font_heading) - (int)(14 * ds + 0.5),
                  (int)(13 * ds + 0.5), g_app.fps_str, 0xFF00FF88, g_app.font_heading);
    }

    // --- 3. Draw panels ----------------------------------------------
    uint32_t fire_accent  = palette_color(pal_idx, 200);
    uint32_t life_accent  = palette_color(pal_idx, 170);
    uint32_t plasma_accent = palette_color(pal_idx, 180);

    int panel_ph = fb_h - (int)(PANEL_TOP * ds + 0.5) - status_h - gap;
    int panel_pt = (int)(PANEL_TOP * ds + 0.5);

    // --- Panel 1: DOOM FIRE ------------------------------------------
    if (g_app.show_fire) {
        draw_panel_bg(fb, stride, fb_w, fb_h,
                       g_fire_x, g_fire_y, g_pw, panel_ph, fire_accent);
        char fire_label[64];
        snprintf(fire_label, sizeof(fire_label), "DOOM FIRE â  Gen %d", g_fire_gen);
        draw_text(g_fire_x + (int)(10 * ds + 0.5), g_fire_y + (int)(6 * ds + 0.5), fire_label, fire_accent, g_app.font_heading);

        for (int fy = 0; fy < FIRE_H; fy++) {
            for (int fx = 0; fx < FIRE_W; fx++) {
                int px = g_fire_ox + fx;
                int py = g_fire_oy + fy;
                if (px >= g_fire_x && px < g_fire_x + g_pw && py >= g_fire_y && py < g_fire_y + panel_ph) {
                    int heat = g_fire_buf[fy][fx];
                    fb[py * stride + px] = palette_color(pal_idx, heat);
                }
            }
        }

        rect_border(fb, stride, g_fire_ox - 1, g_fire_oy - 1,
                     FIRE_W + 2, FIRE_H + 2, fb_w, fb_h,
                     (fire_accent & 0x00FFFFFF) | (0x30 << 24));
    }

    // --- Panel 2: GAME OF LIFE ---------------------------------------
    if (g_app.show_life) {
        draw_panel_bg(fb, stride, fb_w, fb_h,
                       g_life_x, g_life_y, g_pw, panel_ph, life_accent);
        char life_label[64];
        snprintf(life_label, sizeof(life_label), "GAME OF LIFE â  Gen %d  Alive %d",
                 g_life_gen, g_life_alive);
        draw_text(g_life_x + (int)(10 * ds + 0.5), g_life_y + (int)(6 * ds + 0.5), life_label, life_accent, g_app.font_heading);

        int cell = CELL_SIZE;
        uint32_t dead_bg  = 0xFF0A0A14;
        uint32_t grid_line = (life_accent & 0x00FFFFFF) | (0x08 << 24);

        for (int ly = 0; ly < LIFE_H; ly++) {
            for (int lx = 0; lx < LIFE_W; lx++) {
                int px = g_life_ox + lx * cell;
                int py = g_life_oy + ly * cell;
                if (g_life_buf[ly][lx]) {
                    int age = g_life_age[ly][lx];
                    uint32_t alive_color;
                    if (age < 3) alive_color = 0xFF88FF44;
                    else if (age < 10) alive_color = 0xFF44DD22;
                    else if (age < 30) alive_color = 0xFF22AA11;
                    else if (age < 80) alive_color = 0xFF117710;
                    else alive_color = 0xFF0A4408;
                    fill_rect(fb, stride, px, py, cell, cell, fb_w, fb_h, alive_color);
                } else {
                    fill_rect(fb, stride, px, py, cell, cell, fb_w, fb_h, dead_bg);
                }
                if (lx < LIFE_W - 1)
                    write_px(fb, stride, px + cell - 1, py, fb_w, fb_h, grid_line);
                if (ly < LIFE_H - 1)
                    for (int gx = 0; gx < cell; gx++)
                        write_px(fb, stride, px + gx, py + cell - 1, fb_w, fb_h, grid_line);
            }
        }
    }

    // --- Panel 3: PLASMA ---------------------------------------------
    if (g_app.show_plasma) {
        draw_panel_bg(fb, stride, fb_w, fb_h,
                       g_plasma_x, g_plasma_y, g_pw, panel_ph, plasma_accent);
        char plasma_label[64];
        int pp = g_plasma_palette;
        snprintf(plasma_label, sizeof(plasma_label), "PLASMA â  %s",
                 palette_names[pp]);
        draw_text(g_plasma_x + (int)(10 * ds + 0.5), g_plasma_y + (int)(6 * ds + 0.5), plasma_label, plasma_accent, g_app.font_heading);

        double t = g_plasma_time;
        for (int py = 0; py < PLASMA_H; py++) {
            for (int px = 0; px < PLASMA_W; px++) {
                int sx = g_plasma_ox + px;
                int sy = g_plasma_oy + py;
                if (sx >= g_plasma_x && sx < g_plasma_x + g_pw &&
                    sy >= g_plasma_y && sy < g_plasma_y + panel_ph) {
                    int v = plasma_sample(px, py, t);
                    fb[sy * stride + sx] = palette_color(pp, v);
                }
            }
        }
    }

    // --- 4. Status bar -----------------------------------------------
    {
        int sbar_y = fb_h - status_h;
        for (int y = sbar_y; y < fb_h && y < fb_h; y++) {
            float t = (float)(y - sbar_y) / (float)status_h;
            uint32_t scol = (uint32_t)((40 + t * 15)) << 24;
            scol |= 0x00080A14;
            for (int x = 0; x < fb_w; x++)
                fb[y * stride + x] = scol;
        }
        for (int x = 0; x < fb_w; x++)
            fb[sbar_y * stride + x] = (palette_color(pal_idx, 200) & 0x00FFFFFF) | (0x55 << 24);

        int total_px = FIRE_W * FIRE_H + LIFE_W * LIFE_H + PLASMA_W * PLASMA_H;
        snprintf(g_app.status_str, sizeof(g_app.status_str),
                 "FPS: %.0f  |  Pixels: %d  |  Frame: %.1fms  |  Gen: %d  |  Space=pause  F=fire  G=life  P=palette  1/2/3=toggle  Esc=exit",
                 g_app.fps, total_px, g_app.frame_time_ms * 1000.0, g_fire_gen);
        draw_text((int)(12 * ds + 0.5), sbar_y + (int)(8 * ds + 0.5), g_app.status_str, 0xFF808090, g_app.font_legend);
    }

    // --- 5. Legend overlay (top-right, fades after 5 sec) ------------
    if (g_app.total_time < 6.0) {
        const char* legend =
            "Space = Pause All\n"
            "F = Reset Fire\n"
            "G = Randomize Life\n"
            "P = Cycle Plasma\n"
            "1/2/3 = Toggle Panels\n"
            "Esc = Exit\n"
            "\n"
            "Mouse over fire = Blow\n"
            "Click life = Toggle cell";
        int lx = fb_w - (int)(220 * ds + 0.5);
        int ly = header_h + (int)(10 * ds + 0.5);
        uint8_t fade = (uint8_t)(255 * (1.0 - g_app.total_time / 6.0));
        uint32_t legend_bg = ((uint32_t)(fade / 2) << 24) | 0x00080A18;
        fill_rect(fb, stride, lx, ly, (int)(200 * ds + 0.5), (int)(150 * ds + 0.5), fb_w, fb_h, legend_bg);
        draw_text(lx + (int)(8 * ds + 0.5), ly + (int)(8 * ds + 0.5), legend, ((uint32_t)fade << 24) | 0x00A0A0C0, g_app.font_legend);
    }
}
// ============================================================================
//  WINDOW PROCEDURE
// ============================================================================

static LRESULT CALLBACK flp_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
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
                    if (g_host) g_host->running = 0;
                    PostQuitMessage(0);
                    return 0;
                case VK_SPACE:
                    g_app.paused = !g_app.paused;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                case 'F':
                    if (!(GetKeyState(VK_SHIFT) & 0x8000)) {  // lowercase F
                        fire_reset();
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                    return 0;
                case 'G':
                    life_randomize();
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                case 'P':
                    plasma_cycle_palette();
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                case '1':
                    g_app.show_fire = !g_app.show_fire;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                case '2':
                    g_app.show_life = !g_app.show_life;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                case '3':
                    g_app.show_plasma = !g_app.show_plasma;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
            }
            break;
        }
        case WM_MOUSEMOVE: {
            int mx = (int)(short)LOWORD(lp);
            int my = (int)(short)HIWORD(lp);
            g_app.mouse_x = mx;
            g_app.mouse_y = my;

            // Check if mouse is over fire panel → blow on fire
            if (g_app.show_fire && !g_app.paused) {
                int fx = mx - g_fire_ox;
                int fy = my - g_fire_oy;
                if (fx >= 0 && fx < FIRE_W && fy >= 0 && fy < FIRE_H) {
                    g_app.fire_hovered = 1;
                    fire_add_heat(mx, my, 4, 220);
                } else {
                    g_app.fire_hovered = 0;
                }
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int mx = (int)(short)LOWORD(lp);
            int my = (int)(short)HIWORD(lp);
            g_app.mouse_down = 1;
            g_app.mouse_x = mx;
            g_app.mouse_y = my;

            // Check if click is on Game of Life panel → toggle cell
            if (g_app.show_life) {
                int lx = mx - g_life_ox;
                int ly = my - g_life_oy;
                if (lx >= 0 && lx < LIFE_W * CELL_SIZE &&
                    ly >= 0 && ly < LIFE_H * CELL_SIZE) {
                    life_toggle(mx, my);
                    g_app.life_click_handled = 1;
                }
            }
            // Also blow on fire on click (strong burst)
            if (g_app.show_fire && !g_app.paused) {
                int fx = mx - g_fire_ox;
                int fy = my - g_fire_oy;
                if (fx >= 0 && fx < FIRE_W && fy >= 0 && fy < FIRE_H) {
                    fire_add_heat(mx, my, 8, 255);
                }
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: {
            g_app.mouse_down = 0;
            return 0;
        }
        case WM_CHAR: {
            // Handle lowercase f (from WM_KEYDOWN, upper case comes through WM_CHAR for shift-insensitive)
            if (wp == 'f' || wp == 'F') {
                fire_reset();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;
        }
        case WM_DPICHANGED: {
            RECT* rect = (RECT*)lp;
            SetWindowPos(hwnd, NULL, rect->left, rect->top,
                         rect->right - rect->left,
                         rect->bottom - rect->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
    }
    return CallWindowProcA(g_orig_wndproc, hwnd, msg, wp, lp);
}

// ============================================================================
//  MAIN
// ============================================================================

int main(void) {
    // ── DPI scaling ───────────────────────────────────────────────────
    SetProcessDPIAware();
    HDC dpi_dc = GetDC(NULL);
    float dpi_scale = (float)GetDeviceCaps(dpi_dc, LOGPIXELSX) / 96.0f;
    ReleaseDC(NULL, dpi_dc);
    if (dpi_scale < 1.0f) dpi_scale = 1.0f;
    g_dpi_scale = dpi_scale;

    int win_w = (int)(WINDOW_W * dpi_scale + 0.5f);
    int win_h = (int)(WINDOW_H * dpi_scale + 0.5f);
    printf("[DPI] Scale: %.2f, Window: %dx%d (logical %dx%d)\n",
           dpi_scale, win_w, win_h, WINDOW_W, WINDOW_H);

    printf("╔═══════════════════════════════════════════════╗\n");
    printf("║   FIRE + LIFE + PLASMA — Kain Native UI      ║\n");
    printf("╚═══════════════════════════════════════════════╝\n");
    printf("Build: " __DATE__ " " __TIME__ "\n\n");

    srand((unsigned int)time(NULL));

    // Init app state
    memset(&g_app, 0, sizeof(g_app));
    g_app.running = 1;
    g_app.show_fire = 1;
    g_app.show_life = 1;
    g_app.show_plasma = 1;
    g_app.fps_str[0] = '\0';
    g_app.status_str[0] = '\0';

    // Init palettes
    init_palettes();
    printf("[PALETTES] %d color palettes initialized\n", MAX_PALETTES);

    // Init fire
    fire_reset();
    printf("[FIRE] %dx%d buffer\n", FIRE_W, FIRE_H);

    // Init life
    life_randomize();
    printf("[LIFE] %dx%d grid, %d initial alive\n", LIFE_W, LIFE_H, g_life_alive);

    // Compute layout
    compute_layout();

    // ── Create UI session ─────────────────────────────────────────────
    printf("[1/5] Creating UI session...\n");
    abi_ui_reset();
    g_app.session_id = abi_ui_session_create("FireLifePlasma", win_w, win_h);
    if (g_app.session_id <= 0) {
        fprintf(stderr, "FAIL: session_create\n");
        return 1;
    }

    abi_ui_window_open(g_app.session_id, "FIRE + LIFE + PLASMA — Kain Native UI Demo", win_w, win_h);
    if (abi_ui_host_attach(g_app.session_id, "winit") != 0) {
        fprintf(stderr, "FAIL: host_attach\n");
        return 1;
    }

    KainNativeUiSession* ks = abi_ui_find_session(g_app.session_id);
    if (!ks || !ks->host_state) {
        fprintf(stderr, "FAIL: no host state\n");
        return 1;
    }
    g_host = (KainWin32UiHost*)ks->host_state;
    g_app.host = g_host;
    printf("  Window: %dx%d  hwnd=%p  fb=%p\n",
           g_host->width, g_host->height, (void*)g_host->hwnd, (void*)g_host->framebuffer);

    SetWindowTextA(g_host->hwnd, "FIRE + LIFE + PLASMA — Kain Native UI Demo");

    // Recompute layout with actual framebuffer dimensions
    // (The host adapter may have created a different size due to DPI)
    int actual_w = g_host->width;
    int actual_h = g_host->height;

    // Subclass window proc
    g_orig_wndproc = (WNDPROC)SetWindowLongPtrA(g_host->hwnd, GWLP_WNDPROC,
                                                  (LONG_PTR)flp_wndproc);
    SetWindowLongPtrA(g_host->hwnd, GWLP_USERDATA, (LONG_PTR)g_host);

    // ── Create widget context ─────────────────────────────────────────
    printf("[2/5] Creating widget context...\n");
    g_app.ctx = ui_widget_create(g_app.session_id);
    if (!g_app.ctx) {
        fprintf(stderr, "FAIL: widget_create\n");
        return 1;
    }

    // Load fonts
    printf("[3/5] Loading fonts...\n");
    load_fonts();
    if (g_app.font_title <= 0) {
        fprintf(stderr, "WARNING: No fonts loaded — text will not render\n");
    }

    // ── Node tree (minimal, for UI system) ────────────────────────────
    printf("[4/5] Creating node tree...\n");
    int64_t root = abi_ui_node_create(g_app.session_id, "root");
    abi_ui_node_set_rect(g_app.session_id, root, 0, 0, actual_w, actual_h);

    int64_t bg = abi_ui_node_create(g_app.session_id, "bg");
    abi_ui_node_set_parent(g_app.session_id, bg, root);
    abi_ui_node_set_rect(g_app.session_id, bg, 0, 0, actual_w, actual_h);

    printf("[5/5] Entering main loop...\n\n");

    printf("========================================================\n");
    printf("  Controls:\n");
    printf("    Space  — Pause/Resume all\n");
    printf("    F      — Reset fire\n");
    printf("    G      — Randomize Game of Life\n");
    printf("    P      — Cycle plasma palette\n");
    printf("    1/2/3  — Toggle fire/life/plasma panels\n");
    printf("    Esc    — Exit\n");
    printf("    Mouse over fire = blow on it\n");
    printf("    Click life grid = toggle cell\n");
    printf("========================================================\n\n");

    // ── Main loop ─────────────────────────────────────────────────────
    LARGE_INTEGER freq, prev_time, curr_time;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev_time);

    MSG msg;

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
        if (dt > 0.05) dt = 0.05;   // clamp to 50ms max (first frame, etc.)
        prev_time = curr_time;
        g_app.frame_time_ms = dt;

        // Update total time
        if (!g_app.paused) {
            g_app.total_time += dt;
        }

        // FPS tracking
        g_app.fps_count++;
        g_app.fps_timer += dt;
        if (g_app.fps_timer >= 1.0) {
            g_app.fps = (double)g_app.fps_count / g_app.fps_timer;
            snprintf(g_app.fps_str, sizeof(g_app.fps_str), "FPS: %.0f", g_app.fps);
            g_app.fps_count = 0;
            g_app.fps_timer = 0.0;
        }

        // Palette auto-cycle every 30 seconds
        if (!g_app.paused) {
            g_palette_switch_timer += dt;
            if (g_palette_switch_timer >= PALETTE_SWITCH_INTERVAL) {
                g_palette_switch_timer = 0.0;
                g_current_palette = (g_current_palette + 1) % MAX_PALETTES;
            }
        }

        // ── Update effects (all simultaneously when not paused) ──────
        if (!g_app.paused) {
            fire_update();
            life_update();
            plasma_update(dt);
        }

        // ── UI frame lifecycle ────────────────────────────────────────
        abi_ui_begin_frame(g_app.session_id, dt * 1000.0);
        ui_widget_begin_frame(g_app.ctx);
        abi_ui_end_frame(g_app.session_id);

        // ── Render ────────────────────────────────────────────────────
        render_frame(dt);

        // ── End UI frame ──────────────────────────────────────────────
        ui_widget_end_frame(g_app.ctx);

        // ── Present to screen ─────────────────────────────────────────
        InvalidateRect(g_host->hwnd, NULL, FALSE);
        UpdateWindow(g_host->hwnd);

        // ── Throttle to ~120fps cap ─────────────────────────────────
        double elapsed = (double)(curr_time.QuadPart - prev_time.QuadPart) / (double)freq.QuadPart;
        double target_dt = 1.0 / 120.0;
        if (elapsed < target_dt) {
            int sleep_ms = (int)((target_dt - elapsed) * 1000.0);
            if (sleep_ms > 0) Sleep(sleep_ms);
        }

        g_app.frame++;

        // Periodic status
        if (g_app.frame % 600 == 0) {
            printf("[FRAME %lld] FPS: %.0f | Fire gen: %d | Life gen: %d | Alive: %d | Palette: %s\n",
                   (long long)g_app.frame, g_app.fps, g_fire_gen, g_life_gen,
                   g_life_alive, palette_names[g_current_palette]);
        }
    }

    // ── Cleanup ───────────────────────────────────────────────────────
    printf("\n=== SHUTDOWN ===\n");
    printf("Total frames: %lld (%.1f seconds)\n",
           (long long)g_app.frame, g_app.total_time);

    ui_widget_destroy(g_app.ctx);
    abi_ui_session_destroy(g_app.session_id);
    printf("Done.\n");
    return 0;
}
