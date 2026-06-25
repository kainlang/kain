// ============================================================================
//  font_inferno.c — "FONT INFERNO" Typography Overload Demo
//  ============================================================================
//  A visually overwhelming typographic showcase that loads and displays
//  EVERY available .ttf font from C:/Windows/Fonts/ with insane animations.
//
//  Features:
//    - Scans & loads ALL .ttf files from C:/Windows/Fonts/ (300+ fonts)
//    - 3-column independent font carousel (each column cycles separately)
//    - Per-font display: pangram, alphabet, special chars in the font itself
//    - Floating Unicode character particles with rainbow cycling & trails
//    - Glow effects on every text element (draw twice at +1px with half alpha)
//    - Font size rage display animating 12→72
//    - Comparison mode (4-quadrant layout, press C)
//    - Fullscreen single-font preview (press F)
//    - Interactive: Space=pause, L/R=step, U/D=size, C=compare, F=fullscreen
//    - HUD: font name, file path, count "Font 47/312", FPS, size indicator
//    - Glitch effects every ~8 seconds
//
//  Build:
//    cd X:\runtime\native\src\ui\test_ui_v2
//    build.bat inferno          — build font_inferno.exe
//    build.bat inferno run      — build + run
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
#include "../../include/ui_font.h"

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

// ── Global DPI scale for pixel helpers ───────────────────────────
static float g_dpi = 1.0f;

// ============================================================================
//  CONSTANTS
// ============================================================================

#define SCREEN_W           1280
#define SCREEN_H           720
#define MAX_FONTS          512
#define MAX_PARTICLES      80
#define GLITCH_INTERVAL    480  // frames between glitches (~8s at 60fps)
#define FONT_SWITCH_MS     2000 // ms per font display
#define COLUMN_COUNT       3
#define PARTICLE_CHARS     "★♦♥♠•●○◎☆✦✧❖◆▶▲▼◄◈◇◊○●◐◑◒◓☀☁☂★☆✪✫✬✭✮✯❂✢✣✤✥❋❊✱✲✳✴✵✶✷✸✹✺✻✼✽✾✿❀❁"

// ============================================================================
//  FONT ENTRY
// ============================================================================

typedef struct {
    int64_t font_id;
    char name[64];
    char filepath[260];
    int loaded;
} FontEntry;

// ============================================================================
//  PARTICLE
// ============================================================================

typedef struct {
    double x, y;
    double vx, vy;
    char text[8];
    double hue;       // 0-360
    double phase;
    double size;      // 0.5-2.0 scale
    int lifetime;     // frames remaining, 0 = dead
    int max_lifetime;
} Particle;

// ============================================================================
//  APPLICATION STATE
// ============================================================================

typedef struct {
    int running;
    int64_t session_id;

    // Font array
    FontEntry fonts[MAX_FONTS];
    int font_count;

    // 3-column carousel state
    int col_font_idx[COLUMN_COUNT]; // which font each column is showing
    double col_timer[COLUMN_COUNT]; // ms timer for each column

    // Display modes
    int paused;
    int compare_mode;     // C toggle — 4 quadrants
    int fullscreen_mode;  // F toggle — single font fills screen
    int locked_quadrants[4]; // 1-4 lock in compare mode

    // Size rage
    int size_rage_val;    // current displayed size (12-72)
    int size_rage_dir;    // 1 = up, -1 = down

    // Timing
    double total_time_ms;
    int frame;
    double fps;
    char fps_str[32];

    // Particles
    Particle particles[MAX_PARTICLES];

    // Glitch
    int glitch_timer;
    int glitch_active;
    int glitch_y;
    int glitch_h;

    // Font switch speed multiplier
    double speed_mult;

    // HUD toggle
    int show_hud;

    // Screen shake
    double shake_x, shake_y;

} AppState;

static AppState g_app;
static KainWin32UiHost* g_host = NULL;
static double g_dpi_scale = 1.0;
static WNDPROC g_orig_wndproc = NULL;

// ============================================================================
//  PIXEL HELPERS
// ============================================================================

static uint32_t* get_fb(int* out_stride) {
    if (!g_host || !g_host->framebuffer) return NULL;
    *out_stride = g_host->fb_stride / 4;
    return (uint32_t*)g_host->framebuffer;
}

static void blend_px(uint32_t* dst, uint32_t src) {
    uint8_t sa = (src >> 24) & 0xFF;
    if (sa == 0) return;
    if (sa == 255) { *dst = src; return; }
    uint8_t sr = (src >> 16) & 0xFF;
    uint8_t sg = (src >> 8) & 0xFF;
    uint8_t sb = src & 0xFF;
    uint8_t da = 255 - sa;
    *dst = 0xFF000000
         | (uint32_t)(((uint16_t)sr * sa + ((*dst >> 16) & 0xFF) * da) / 255) << 16
         | (uint32_t)(((uint16_t)sg * sa + ((*dst >> 8) & 0xFF) * da) / 255) << 8
         | (uint32_t)(((uint16_t)sb * sa + (*dst & 0xFF) * da) / 255);
}

static void blend_px_safe(int x, int y, uint32_t color) {
    if (!g_host || !g_host->framebuffer) return;
    int w = g_host->width, h = g_host->height;
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    int stride = g_host->fb_stride / 4;
    blend_px(&((uint32_t*)g_host->framebuffer)[y * stride + x], color);
}

static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!g_host || !g_host->framebuffer || w <= 0 || h <= 0) return;
    int fb_w = g_host->width, fb_h = g_host->height;
    int stride = g_host->fb_stride / 4;
    uint32_t* fb = (uint32_t*)g_host->framebuffer;
    for (int r = y; r < y + h && r < fb_h; r++) {
        if (r < 0) continue;
        for (int c = x; c < x + w && c < fb_w; c++) {
            if (c < 0) continue;
            blend_px(&fb[r * stride + c], color);
        }
    }
}

static void draw_rect_border(int x, int y, int w, int h, uint32_t color) {
    fill_rect(x, y, w, 1, color);
    fill_rect(x, y + h - 1, w, 1, color);
    fill_rect(x, y, 1, h, color);
    fill_rect(x + w - 1, y, 1, h, color);
}

static void draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        blend_px_safe(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

// ── Draw filled circle ────────────────────────────────────────────────
static void draw_filled_circle(int cx, int cy, int r, uint32_t color) {
    if (!g_host) return;
    for (int y = -r; y <= r; y++) {
        int row = cy + y;
        if (row < 0 || row >= g_host->height) continue;
        int hw = (int)(sqrt((double)(r * r - y * y)) + 0.5);
        for (int x = -hw; x <= hw; x++) {
            blend_px_safe(cx + x, row, color);
        }
    }
}

// ── HSL to RGB conversion for rainbow colors ──────────────────────────
static uint32_t hsl_to_rgb(double h, double s, double l, double a) {
    double r, g, b;
    h = fmod(h, 360.0);
    if (h < 0) h += 360.0;
    s = kain_clampd(s, 0.0, 1.0);
    l = kain_clampd(l, 0.0, 1.0);
    double c = (1.0 - fabs(2.0 * l - 1.0)) * s;
    double hp = h / 60.0;
    double x = c * (1.0 - fabs(fmod(hp, 2.0) - 1.0));
    if (hp < 1.0)       { r = c; g = x; b = 0.0; }
    else if (hp < 2.0)  { r = x; g = c; b = 0.0; }
    else if (hp < 3.0)  { r = 0.0; g = c; b = x; }
    else if (hp < 4.0)  { r = 0.0; g = x; b = c; }
    else if (hp < 5.0)  { r = x; g = 0.0; b = c; }
    else                { r = c; g = 0.0; b = x; }
    double m = l - c / 2.0;
    uint8_t alpha = (uint8_t)kain_clampd(a * 255.0, 0.0, 255.0);
    uint8_t rr = (uint8_t)((r + m) * 255.0);
    uint8_t gg = (uint8_t)((g + m) * 255.0);
    uint8_t bb = (uint8_t)((b + m) * 255.0);
    return ((uint32_t)alpha << 24) | ((uint32_t)rr << 16) | ((uint32_t)gg << 8) | bb;
}

// ============================================================================
//  FONT LOADING (bypasses widget's 8-font limit, uses resources directly)
// ============================================================================

static int64_t load_font_direct(int64_t session_id, const char* path, double size) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > 16 * 1024 * 1024) { fclose(f); return 0; }
    uint8_t* data = (uint8_t*)malloc((size_t)len);
    if (!data) { fclose(f); return 0; }
    size_t nread = fread(data, 1, (size_t)len, f);
    fclose(f);
    if (nread != (size_t)len) { free(data); return 0; }

    // Extract family name from path
    const char* fname = strrchr(path, '/');
    if (!fname) fname = strrchr(path, '\\');
    if (!fname) fname = path;
    else fname++;

    char family[64];
    strncpy(family, fname, sizeof(family) - 1);
    family[sizeof(family) - 1] = '\0';
    // Remove .ttf extension
    char* dot = strstr(family, ".ttf");
    if (!dot) dot = strstr(family, ".TTF");
    if (dot) *dot = '\0';

    int64_t font_id = abi_ui_font_load_ttf(session_id, family, family, size, data, (int64_t)len);
    free(data);
    return font_id;
}

static void scan_and_load_fonts(int64_t session_id) {
    printf("[FONT] Scanning C:/Windows/Fonts/*.ttf...\n");

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA("C:\\Windows\\Fonts\\*.ttf", &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        printf("[FONT] ERROR: No .ttf files found in C:/Windows/Fonts/\n");
        return;
    }

    DWORD last_progress = GetTickCount();
    do {
        if (g_app.font_count >= MAX_FONTS) {
            printf("  [FONT] Reached max fonts (%d), stopping scan.\n", MAX_FONTS);
            fflush(stdout);
            break;
        }

        char path[MAX_PATH];
        snprintf(path, sizeof(path), "C:/Windows/Fonts/%s", ffd.cFileName);

        int64_t fid = load_font_direct(session_id, path, 36.0 * g_dpi_scale);
        if (fid > 0) {
            FontEntry* fi = &g_app.fonts[g_app.font_count];
            fi->font_id = fid;
            strncpy(fi->filepath, path, sizeof(fi->filepath) - 1);
            strncpy(fi->name, ffd.cFileName, sizeof(fi->name) - 1);
            char* dot = strstr(fi->name, ".ttf");
            if (!dot) dot = strstr(fi->name, ".TTF");
            if (dot) *dot = '\0';
            fi->loaded = 1;
            g_app.font_count++;

            DWORD now = GetTickCount();
            if (now - last_progress >= 2000) {
                printf("  [FONT] Loaded %d fonts...\n", g_app.font_count);
                fflush(stdout);
                last_progress = now;
            }
        }
        // Skip failures silently — .ttc collections, corrupted, etc.
    } while (FindNextFileA(hFind, &ffd) != 0);

    FindClose(hFind);

    printf("[FONT] === TOTAL: %d fonts loaded ===\n", g_app.font_count);
    fflush(stdout);
    if (g_app.font_count == 0) {
        printf("[FONT] WARNING: No fonts loaded! Will try fallback paths.\n");
        // Load a couple known-good fonts manually
        const char* fallbacks[] = {
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/consola.ttf",
            "C:/Windows/Fonts/segoeui.ttf",
            NULL
        };
        for (int i = 0; fallbacks[i]; i++) {
            int64_t fid = load_font_direct(session_id, fallbacks[i], 36.0 * g_dpi_scale);
            if (fid > 0 && g_app.font_count < MAX_FONTS) {
                FontEntry* fi = &g_app.fonts[g_app.font_count++];
                fi->font_id = fid;
                strncpy(fi->filepath, fallbacks[i], sizeof(fi->filepath) - 1);
                const char* fn = strrchr(fallbacks[i], '/');
                fn = fn ? fn + 1 : fallbacks[i];
                strncpy(fi->name, fn, sizeof(fi->name) - 1);
                char* dot = strstr(fi->name, ".ttf");
                if (dot) *dot = '\0';
                fi->loaded = 1;
            }
        }
        printf("[FONT] Fallback loaded %d fonts\n", g_app.font_count);
    }
}

// ============================================================================
//  PARTICLE SYSTEM
// ============================================================================

static const char* g_particle_chars[] = {
    "★", "♦", "♥", "♠", "•", "●", "○", "◎", "☆", "✦", "✧", "❖",
    "◆", "▶", "▲", "▼", "◄", "◈", "◇", "☀", "☁", "☂", "✪", "✫",
    "✬", "✭", "✮", "✯", "❂", "✢", "✣", "✤", "✥", "❋", "❊", "✱",
    "✲", "✳", "✴", "✵", "✶", "✷", "✸", "✹", "✺", "✻", "✼", "✽",
    "✾", "✿", "❀", "❁", "⚡", "☯", "⚙", "☰", "☷", "☶", "☵", "☴",
    "☳", "☲", "☱", "♲", "♳", "♴", "♵", "♶", "♷", "♸", "♹", "♺",
    NULL
};

static void spawn_particle(void) {
    // Find dead particle slot
    int slot = -1;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (g_app.particles[i].lifetime <= 0) { slot = i; break; }
    }
    if (slot < 0) return;

    Particle* p = &g_app.particles[slot];
    p->x = (double)(rand() % (g_host ? g_host->width : 1280));
    p->y = (double)(g_host ? g_host->height : 720) + 20.0;

    // Pick random particle char
    int char_idx = rand() % 64; // first 64 chars from the list
    const char* ch = g_particle_chars[char_idx % 64];
    if (!ch) ch = "★";
    strncpy(p->text, ch, sizeof(p->text) - 1);
    p->text[sizeof(p->text) - 1] = '\0';

    p->vx = (double)(rand() % 200 - 100) * 0.02;
    p->vy = -(double)(rand() % 100 + 30) * 0.03;
    p->hue = (double)(rand() % 360);
    p->phase = (double)(rand() % 628) / 100.0;
    p->size = 0.5 + (double)(rand() % 100) / 100.0;
    p->max_lifetime = 120 + rand() % 180; // 2-5 seconds
    p->lifetime = p->max_lifetime;
}

static void update_particles(double dt_ms) {
    double dt = dt_ms / 1000.0;

    // Spawn new particles occasionally
    if (rand() % 3 == 0 && !g_app.paused) {
        spawn_particle();
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &g_app.particles[i];
        if (p->lifetime <= 0) continue;

        if (!g_app.paused) {
            p->x += p->vx * dt * 60.0;
            p->y += p->vy * dt * 60.0;
            p->vy += 0.02 * dt * 60.0; // gravity
            p->hue += dt * 60.0; // rainbow cycling
            if (p->hue > 360.0) p->hue -= 360.0;
            p->lifetime--;
        }

        // Off-screen reset
        if (p->y > (g_host ? g_host->height : 720) + 50 ||
            p->x < -50 || p->x > (g_host ? g_host->width : 1280) + 50) {
            p->lifetime = 0;
        }
    }
}

static void draw_particles(KainUiWidgetContext* ctx) {
    if (!ctx) return;
    // Use first column's font for particles, with bounds safety
    int particle_font_idx = (g_app.font_count > 0 && g_app.col_font_idx[0] >= 0 &&
                             g_app.col_font_idx[0] < g_app.font_count)
                            ? g_app.col_font_idx[0] : 0;
    int64_t particle_fid = (g_app.font_count > 0 && g_app.fonts[particle_font_idx].loaded)
                           ? g_app.fonts[particle_font_idx].font_id : 0;
    if (particle_fid <= 0) return;

    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle* p = &g_app.particles[i];
        if (p->lifetime <= 0) continue;

        // Fade based on lifetime
        float life_ratio = (float)p->lifetime / (float)p->max_lifetime;
        int alpha = (int)(80 + 175 * life_ratio);
        if (alpha < 10) alpha = 10;

        // Rainbow color
        uint32_t color = hsl_to_rgb(p->hue, 0.9, 0.6, (double)alpha / 255.0);

        // Draw glow first
        uint32_t glow = (color & 0x00FFFFFF) | (((uint32_t)(alpha / 3)) << 24);
        int px = (int)p->x;
        int py = (int)p->y;

        // Glow pass
        ui_widget_draw_text_ex(ctx, px - 1, py - 1, p->text, glow, 0, particle_fid);
        ui_widget_draw_text_ex(ctx, px + 1, py - 1, p->text, glow, 0, particle_fid);
        ui_widget_draw_text_ex(ctx, px - 1, py + 1, p->text, glow, 0, particle_fid);
        ui_widget_draw_text_ex(ctx, px + 1, py + 1, p->text, glow, 0, particle_fid);

        // Main text
        ui_widget_draw_text_ex(ctx, px, py, p->text, color, 0, particle_fid);
    }
}

// ============================================================================
//  GLOW TEXT HELPER
// ============================================================================

static void draw_glow_text(KainUiWidgetContext* ctx, int x, int y,
                            const char* text, uint32_t color, int64_t font_id)
{
    if (!text || !text[0]) return;
    uint32_t glow = (color & 0x00FFFFFF) | (0x50 << 24);
    ui_widget_draw_text_ex(ctx, x - 1, y, text, glow, 0, font_id);
    ui_widget_draw_text_ex(ctx, x + 1, y, text, glow, 0, font_id);
    ui_widget_draw_text_ex(ctx, x, y - 1, text, glow, 0, font_id);
    ui_widget_draw_text_ex(ctx, x, y + 1, text, glow, 0, font_id);
    ui_widget_draw_text_ex(ctx, x, y, text, color, 0, font_id);
}

// Draw centered text with glow
static void draw_glow_text_centered(KainUiWidgetContext* ctx, int cx, int y,
                                     const char* text, uint32_t color, int64_t font_id)
{
    if (!text || !text[0]) return;
    double tw = abi_ui_text_measure_width(g_app.session_id, font_id, text);
    int x = cx - (int)(tw / 2.0 + 0.5);
    draw_glow_text(ctx, x, y, text, color, font_id);
}

// ============================================================================
//  FONT DISPLAY
// ============================================================================

static void draw_font_preview(KainUiWidgetContext* ctx, int x, int y, int w, int h,
                               int font_idx, uint32_t accent_color, const char* label)
{
    if (font_idx < 0 || font_idx >= g_app.font_count) return;
    if (!g_app.fonts[font_idx].loaded) return;

    int64_t fid = g_app.fonts[font_idx].font_id;
    const char* name = g_app.fonts[font_idx].name;

    int margin = 10;
    int cx = x + margin;
    int cy = y + margin;
    int cw = w - margin * 2;

    // ── Column background ─────────────────────────────────────────────
    fill_rect(x, y, w, h, 0x22000000);
    draw_rect_border(x, y, w, h, accent_color);

    // ── Font name (in the font itself, top) ───────────────────────────
    char header[96];
    if (label) {
        snprintf(header, sizeof(header), "%s: %s", label, name);
    } else {
        strncpy(header, name, sizeof(header) - 1);
    }
    draw_glow_text(ctx, cx, cy + 2, header, accent_color, fid);

    // ── Pangram ───────────────────────────────────────────────────────
    const char* pangram = "The quick brown fox jumps over the lazy dog 0123456789";
    uint32_t text_color = 0xFFE0E0F0;
    draw_glow_text(ctx, cx, cy + 32, pangram, text_color, fid);

    // ── Alphabet ──────────────────────────────────────────────────────
    const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    draw_glow_text(ctx, cx, cy + 58, alphabet, 0xFFAADDFF, fid);

    const char* alphabet_lower = "abcdefghijklmnopqrstuvwxyz";
    draw_glow_text(ctx, cx, cy + 78, alphabet_lower, 0xFF88AACC, fid);

    // ── Special characters ────────────────────────────────────────────
    const char* special = "!@#$%^&*()[]{}<>?/+=_-:;,.~";
    draw_glow_text(ctx, cx, cy + 98, special, 0xFFFFAA88, fid);

    // ── Numerals ──────────────────────────────────────────────────────
    const char* numerals = "0123456789";
    draw_glow_text(ctx, cx, cy + 118, numerals, 0xFF88FFAA, fid);

    // ── Size rage animation indicator ─────────────────────────────────
    char size_str[32];
    snprintf(size_str, sizeof(size_str), "SIZE: %dpx [pulsing]", g_app.size_rage_val);
    uint32_t size_color = hsl_to_rgb(g_app.total_time_ms * 0.05, 0.9, 0.6, 1.0);
    draw_glow_text(ctx, cx, cy + 142, size_str, size_color, fid);

    // ── Full alphabet showcase (smaller, all on one line) ─────────────
    // Only if there's enough vertical room
    if (h > 200) {
        // We'll render a few more lines showing the font's character
        draw_glow_text(ctx, cx, cy + 168, "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz", 0xFFCCCCDD, fid);
    }
}

// ============================================================================
//  GLITCH EFFECT
// ============================================================================

static void apply_glitch(void) {
    if (!g_app.glitch_active || !g_host || !g_host->framebuffer) return;

    int stride = g_host->fb_stride / 4;
    uint32_t* fb = (uint32_t*)g_host->framebuffer;
    int w = g_host->width, h = g_host->height;
    int y = g_app.glitch_y;
    int gh = g_app.glitch_h;
    if (y < 0 || y + gh >= h || gh <= 0) return;

    int shift = (rand() % 40) - 20;
    if (shift == 0) shift = 12;

    for (int r = y; r < y + gh && r < h; r++) {
        if (shift > 0) {
            for (int c = w - 1; c >= shift; c--) {
                fb[r * stride + c] = fb[r * stride + (c - shift)];
            }
        } else {
            int abs_shift = -shift;
            for (int c = 0; c < w - abs_shift; c++) {
                fb[r * stride + c] = fb[r * stride + (c + abs_shift)];
            }
        }
        // Noise strip
        if (rand() % 4 == 0) {
            for (int c = 0; c < w; c += 3) {
                fb[r * stride + (c + rand() % 5)] = (rand() % 2) ? 0xFFFFFFFF : 0xFF000000;
            }
        }
    }

    g_app.glitch_active = 0;
}

// ============================================================================
//  HUD
// ============================================================================

static void draw_hud(KainUiWidgetContext* ctx) {
    if (!g_app.show_hud) return;
    if (g_app.font_count == 0) return;

    int fb_w = g_host ? g_host->width : SCREEN_W;
    int fb_h = g_host ? g_host->height : SCREEN_H;

    // Top bar background
    fill_rect(0, 0, fb_w, 28, 0xBB080810);
    fill_rect(0, 28, fb_w, 1, 0xFF44AAFF);

    // Current font info (left)
    FontEntry* cur = NULL;
    if (g_app.col_font_idx[0] >= 0 && g_app.col_font_idx[0] < g_app.font_count) {
        cur = &g_app.fonts[g_app.col_font_idx[0]];
    }
    if (cur && cur->loaded) {
        char info[256];
        if (g_app.compare_mode) {
            snprintf(info, sizeof(info), "FONT INFERNO  |  COMPARE MODE  |  %s", cur->name);
        } else if (g_app.fullscreen_mode) {
            snprintf(info, sizeof(info), "FONT INFERNO  |  FULLSCREEN  |  %s", cur->name);
        } else {
            snprintf(info, sizeof(info), "FONT INFERNO  |  %s  |  3-COLUMN CAROUSEL", cur->name);
        }
        ui_widget_draw_text(ctx, 10, 6, info, 0xFF88CCFF, 14);
    }

    // Font count (right)
    char count_str[64];
    snprintf(count_str, sizeof(count_str), "Font %d / %d",
             g_app.col_font_idx[0] + 1, g_app.font_count);
    double cw = abi_ui_text_measure_width(g_app.session_id,
        ctx->font_count > 0 ? ctx->fonts[ctx->default_font < 0 ? 0 : ctx->default_font].font_id : 0,
        count_str);
    ui_widget_draw_text(ctx, fb_w - (int)cw - 100, 6, count_str, 0xFF88CCFF, 14);

    // FPS (far right)
    ui_widget_draw_text(ctx, fb_w - 70, 6, g_app.fps_str, 0xFF00FF88, 14);

    // Mode indicators (bottom left)
    char mode_str[128] = "";
    if (g_app.paused)    strcat(mode_str, " [PAUSED]");
    if (g_app.compare_mode) strcat(mode_str, " [COMPARE]");
    if (g_app.fullscreen_mode) strcat(mode_str, " [FULLSCREEN]");
    if (mode_str[0]) {
        ui_widget_draw_text(ctx, 10, fb_h - 20, mode_str, 0xFFFF8844, 14);
    }

    // Size indicator (bottom right)
    char sz_str[32];
    snprintf(sz_str, sizeof(sz_str), "SIZE: %dpx", g_app.size_rage_val);
    double sw = abi_ui_text_measure_width(g_app.session_id,
        ctx->font_count > 0 ? ctx->fonts[ctx->default_font < 0 ? 0 : ctx->default_font].font_id : 0,
        sz_str);
    ui_widget_draw_text(ctx, fb_w - (int)sw - 10, fb_h - 20, sz_str, 0xFF88FFAA, 14);

    // Speed indicator
    char spd_str[32];
    snprintf(spd_str, sizeof(spd_str), "SPD: %.1fx", g_app.speed_mult);
    ui_widget_draw_text(ctx, 10, fb_h - 40, spd_str, 0xFFAAAAAA, 12);

    // Controls hint (bottom center, dim)
    const char* controls = "SPC=pause  L/R=step  U/D=size  C=compare  F=full  H=hud  1-4=lock  ESC=exit";
    double ctrl_w = abi_ui_text_measure_width(g_app.session_id,
        ctx->font_count > 0 ? ctx->fonts[ctx->default_font < 0 ? 0 : ctx->default_font].font_id : 0,
        controls);
    int ctrl_x = (fb_w - (int)ctrl_w) / 2;
    if (ctrl_x < 0) ctrl_x = 0;
    ui_widget_draw_text(ctx, ctrl_x, fb_h - 38, controls, 0xFF555566, 11);
}

// ============================================================================
//  WINDOW PROCEDURE
// ============================================================================

static LRESULT CALLBACK font_inferno_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
        case WM_DESTROY:
        case WM_CLOSE:
            g_app.running = 0;
            return 0;

        case WM_KEYDOWN: {
            switch (w) {
                case VK_ESCAPE: g_app.running = 0; break;

                case VK_SPACE:
                    g_app.paused = !g_app.paused;
                    break;

                case VK_LEFT:
                    if (!g_app.paused) break;
                    for (int i = 0; i < COLUMN_COUNT; i++) {
                        g_app.col_font_idx[i]--;
                        if (g_app.col_font_idx[i] < 0)
                            g_app.col_font_idx[i] = g_app.font_count - 1;
                    }
                    break;

                case VK_RIGHT:
                    if (!g_app.paused) break;
                    for (int i = 0; i < COLUMN_COUNT; i++) {
                        g_app.col_font_idx[i]++;
                        if (g_app.col_font_idx[i] >= g_app.font_count)
                            g_app.col_font_idx[i] = 0;
                    }
                    break;

                case VK_UP:
                    g_app.size_rage_val += 4;
                    if (g_app.size_rage_val > 72) g_app.size_rage_val = 72;
                    break;

                case VK_DOWN:
                    g_app.size_rage_val -= 4;
                    if (g_app.size_rage_val < 12) g_app.size_rage_val = 12;
                    break;

                case 'C': case 'c':
                    g_app.compare_mode = !g_app.compare_mode;
                    g_app.fullscreen_mode = 0;
                    break;

                case 'F': case 'f':
                    g_app.fullscreen_mode = !g_app.fullscreen_mode;
                    g_app.compare_mode = 0;
                    break;

                case 'H': case 'h':
                    g_app.show_hud = !g_app.show_hud;
                    break;

                case '1': if (g_app.compare_mode) g_app.locked_quadrants[0] = !g_app.locked_quadrants[0]; break;
                case '2': if (g_app.compare_mode) g_app.locked_quadrants[1] = !g_app.locked_quadrants[1]; break;
                case '3': if (g_app.compare_mode) g_app.locked_quadrants[2] = !g_app.locked_quadrants[2]; break;
                case '4': if (g_app.compare_mode) g_app.locked_quadrants[3] = !g_app.locked_quadrants[3]; break;
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (g_host && g_host->framebuffer) {
                BitBlt(hdc, 0, 0, g_host->width, g_host->height,
                       g_host->hdc_buffer, 0, 0, SRCCOPY);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
    }

    if (g_orig_wndproc)
        return CallWindowProcA(g_orig_wndproc, hwnd, msg, w, l);
    return DefWindowProcA(hwnd, msg, w, l);
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

    int win_w = (int)(SCREEN_W * dpi_scale + 0.5f);
    int win_h = (int)(SCREEN_H * dpi_scale + 0.5f);
    printf("[DPI] Scale: %.2f, Window: %dx%d (logical %dx%d)\n",
           dpi_scale, win_w, win_h, SCREEN_W, SCREEN_H);

    // ── Banner ────────────────────────────────────────────────────────
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║              FONT INFERNO — Typography Overload          ║\n");
    printf("║       Loading EVERY .ttf from C:/Windows/Fonts/...       ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Build: " __DATE__ " " __TIME__ "\n\n");

    // Seed RNG
    srand((unsigned int)time(NULL));

    // ── Init app state ────────────────────────────────────────────────
    memset(&g_app, 0, sizeof(g_app));
    g_app.running = 1;
    g_app.size_rage_val = (int)(36 * g_dpi_scale + 0.5);
    g_app.size_rage_dir = 1;
    g_app.speed_mult = 1.0;
    g_app.show_hud = 1;
    g_app.font_count = 0;
    strcpy(g_app.fps_str, "FPS: --");

    for (int i = 0; i < COLUMN_COUNT; i++) {
        g_app.col_font_idx[i] = i;
        g_app.col_timer[i] = 0.0;
    }

    // ── Create Kain UI session ────────────────────────────────────────
    printf("[1/6] Creating UI session...\n");
    abi_ui_reset();
    g_app.session_id = abi_ui_session_create("FontInferno", win_w, win_h);
    if (g_app.session_id <= 0) {
        fprintf(stderr, "FAIL: session_create\n");
        return 1;
    }

    abi_ui_window_open(g_app.session_id, "FONT INFERNO — Typography Overload", win_w, win_h);
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

    SetWindowTextA(g_host->hwnd, "FONT INFERNO — Typography Overload");

    // Subclass window proc
    g_orig_wndproc = (WNDPROC)SetWindowLongPtrA(g_host->hwnd, GWLP_WNDPROC,
                                                  (LONG_PTR)font_inferno_wndproc);
    SetWindowLongPtrA(g_host->hwnd, GWLP_USERDATA, (LONG_PTR)g_host);

    // ── Create widget context ─────────────────────────────────────────
    printf("[2/6] Creating widget context...\n");
    KainUiWidgetContext* ctx = ui_widget_create(g_app.session_id);
    if (!ctx) {
        fprintf(stderr, "FAIL: widget_create\n");
        return 1;
    }

    // Load at least one default font for the widget system (HUD text)
    printf("[3/6] Loading widget default font...\n");
    // Note: ui_widget_load_font now scales internally by ctx->dpi_scale,
    // so pass logical size (14.0) without pre-scaling.
    int64_t default_fid = ui_widget_load_font(ctx, "C:/Windows/Fonts/consola.ttf", 14.0);
    if (default_fid <= 0) {
        default_fid = ui_widget_load_font(ctx, "C:/Windows/Fonts/arial.ttf", 14.0);
    }
    if (default_fid <= 0) {
        default_fid = ui_widget_load_font(ctx, "C:/Windows/Fonts/segoeui.ttf", 14.0);
    }
    printf("  Default font ID: %lld\n", (long long)default_fid);
    fflush(stdout);

    // ── Scan and load ALL fonts ───────────────────────────────────────
    printf("[4/6] Scanning & loading C:/Windows/Fonts/*.ttf...\n");
    fflush(stdout);
    DWORD scan_start = GetTickCount();
    scan_and_load_fonts(g_app.session_id);
    DWORD scan_end = GetTickCount();
    printf("  Scan took %.2f seconds\n", (double)(scan_end - scan_start) / 1000.0);
    printf("  Total: %d fonts loaded\n", g_app.font_count);
    fflush(stdout);

    if (g_app.font_count == 0) {
        fprintf(stderr, "FATAL: No fonts could be loaded. Exiting.\n");
        return 1;
    }

    // Initialize column font indices with spreading
    for (int i = 0; i < COLUMN_COUNT; i++) {
        g_app.col_font_idx[i] = (i * (g_app.font_count / 3)) % g_app.font_count;
        g_app.col_timer[i] = (double)i * (FONT_SWITCH_MS / (double)COLUMN_COUNT);
    }

    // Set up node tree
    printf("[5/6] Creating node tree...\n");
    int64_t root = abi_ui_node_create(g_app.session_id, "root");
    abi_ui_node_set_rect(g_app.session_id, root, 0, 0, win_w, win_h);
    int64_t bg = abi_ui_node_create(g_app.session_id, "bg");
    abi_ui_node_set_parent(g_app.session_id, bg, root);
    abi_ui_node_set_rect(g_app.session_id, bg, 0, 0, win_w, win_h);
    abi_ui_node_set_style_string(g_app.session_id, bg, "fill_color", "#08081A");

    // ── Controls banner ───────────────────────────────────────────────
    printf("[6/6] Entering main loop...\n");
    printf("\n");
    printf("============================================================\n");
    printf("  Controls:\n");
    printf("    SPACE    — Pause/unpause carousel\n");
    printf("    L/R      — Step through fonts (while paused)\n");
    printf("    U/D      — Change base font size display\n");
    printf("    C        — Toggle 4-quadrant comparison mode\n");
    printf("    F        — Toggle fullscreen font preview\n");
    printf("    1-4      — Lock individual quadrants (compare mode)\n");
    printf("    H        — Toggle HUD\n");
    printf("    ESC      — Exit\n");
    printf("============================================================\n\n");

    // ── Timing ────────────────────────────────────────────────────────
    LARGE_INTEGER perf_freq;
    QueryPerformanceFrequency(&perf_freq);
    LARGE_INTEGER last_time;
    QueryPerformanceCounter(&last_time);

    DWORD last_frame_log = GetTickCount();
    int frame_count = 0;

    // ── Main loop ─────────────────────────────────────────────────────
    while (g_app.running) {
        // ── Message pump ──────────────────────────────────────────────
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!g_app.running) break;

        // ── Timing ────────────────────────────────────────────────────
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double dt_ms = (double)(now.QuadPart - last_time.QuadPart) / (double)perf_freq.QuadPart * 1000.0;
        last_time = now;

        // Clamp dt to avoid spiral of death
        if (dt_ms > 100.0) dt_ms = 16.666;

        // ── FPS calculation ───────────────────────────────────────────
        frame_count++;
        g_app.frame++;
        DWORD now_ms = GetTickCount();
        if (now_ms - last_frame_log >= 1000) {
            g_app.fps = (double)frame_count * 1000.0 / (double)(now_ms - last_frame_log);
            snprintf(g_app.fps_str, sizeof(g_app.fps_str), "FPS: %.0f", g_app.fps);
            frame_count = 0;
            last_frame_log = now_ms;
        }

        if (!g_app.paused) {
            g_app.total_time_ms += dt_ms;

            // ── Update font carousel timers (3 independent columns) ────
            double interval = FONT_SWITCH_MS / g_app.speed_mult;
            for (int i = 0; i < COLUMN_COUNT; i++) {
                g_app.col_timer[i] += dt_ms;
                if (g_app.col_timer[i] >= interval) {
                    g_app.col_timer[i] -= interval;
                    // Only advance if not in compare mode with locked quadrants
                    if (!g_app.compare_mode || !g_app.locked_quadrants[i]) {
                        g_app.col_font_idx[i]++;
                        if (g_app.col_font_idx[i] >= g_app.font_count)
                            g_app.col_font_idx[i] = 0;
                    }
                }
            }

            // ── Update size rage ──────────────────────────────────────
            g_app.size_rage_val += g_app.size_rage_dir;
            if (g_app.size_rage_val >= (int)(72 * g_dpi_scale + 0.5)) { g_app.size_rage_val = (int)(72 * g_dpi_scale + 0.5); g_app.size_rage_dir = -1; }
            if (g_app.size_rage_val <= (int)(12 * g_dpi_scale + 0.5)) { g_app.size_rage_val = (int)(12 * g_dpi_scale + 0.5); g_app.size_rage_dir = 1; }

            // ── Update glitch ─────────────────────────────────────────
            g_app.glitch_timer++;
            if (g_app.glitch_timer >= GLITCH_INTERVAL) {
                g_app.glitch_timer = 0;
                if (rand() % 3 == 0) {
                    g_app.glitch_active = 1;
                    g_app.glitch_y = rand() % (g_host ? g_host->height - (int)(60 * g_dpi_scale + 0.5) : (int)(660 * g_dpi_scale + 0.5));
                    g_app.glitch_h = (int)(10 * g_dpi_scale + 0.5) + rand() % (int)(40 * g_dpi_scale + 0.5);
                }
            }
        }

        // ── Update particles (animates even during pause) ─────────────
        // Particles drift regardless of pause state for ambient effect
        update_particles(dt_ms);

        // ── Begin frame ───────────────────────────────────────────────
        abi_ui_begin_frame(g_app.session_id, dt_ms);

        // ── Clear framebuffer with dark gradient ──────────────────────
        if (g_host && g_host->framebuffer) {
            int stride = g_host->fb_stride / 4;
            uint32_t* fb = (uint32_t*)g_host->framebuffer;
            int fb_w = g_host->width, fb_h = g_host->height;

            // Dark gradient background
            for (int y = 0; y < fb_h; y++) {
                double t = (double)y / (double)fb_h;
                uint8_t r = (uint8_t)(8 + t * 15);
                uint8_t g = (uint8_t)(8 + t * 10);
                uint8_t b = (uint8_t)(26 + t * 20);
                uint32_t bg_color = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
                for (int x = 0; x < fb_w; x++) {
                    fb[y * stride + x] = bg_color;
                }
            }

            // ── Draw floating particles ──────────────────────────────
            draw_particles(ctx);

            // ── Draw font display based on mode ──────────────────────
            if (g_app.compare_mode) {
                // ── 4-quadrant comparison mode ────────────────────────
                int half_w = fb_w / 2;
                int half_h = fb_h / 2;
                int quad_fonts[4];
                for (int q = 0; q < 4; q++) {
                    if (g_app.locked_quadrants[q]) {
                        quad_fonts[q] = g_app.col_font_idx[q % COLUMN_COUNT];
                    } else {
                        quad_fonts[q] = (g_app.col_font_idx[0] + q * (g_app.font_count / 4)) % g_app.font_count;
                    }
                }

                const char* labels[] = {"QL", "QR", "BL", "BR"};
                uint32_t colors[] = {0xFFFF4488, 0xFF44FF88, 0xFF4488FF, 0xFFFFAA44};

                for (int q = 0; q < 4; q++) {
                    int qx = (q % 2) * half_w;
                    int qy = (q / 2) * half_h;
                    int qw = half_w - 2;
                    int qh = half_h - 2;

                    draw_font_preview(ctx, qx + 1, qy + 1, qw, qh,
                                      quad_fonts[q], colors[q], labels[q]);

                    // Lock indicator
                    if (g_app.locked_quadrants[q]) {
                        char lock_str[8];
                        snprintf(lock_str, sizeof(lock_str), "LOCKED");
                        ui_widget_draw_text(ctx, qx + qw - (int)(55 * g_dpi_scale + 0.5), qy + (int)(3 * g_dpi_scale + 0.5), lock_str, 0xFFFFFF44, (int)(11 * g_dpi_scale + 0.5));
                    }

                    // Quadrant number hint
                    char qnum[4];
                    snprintf(qnum, sizeof(qnum), "%d", q + 1);
                    ui_widget_draw_text(ctx, qx + (int)(4 * g_dpi_scale + 0.5), qy + qh - (int)(16 * g_dpi_scale + 0.5), qnum, 0x44FFFFFF, (int)(11 * g_dpi_scale + 0.5));
                }

            } else if (g_app.fullscreen_mode) {
                // ── Fullscreen single font preview ────────────────────
                int font_idx = g_app.col_font_idx[0];
                if (font_idx >= 0 && font_idx < g_app.font_count && g_app.fonts[font_idx].loaded) {
                    // Border glow
                    uint32_t border = hsl_to_rgb(g_app.total_time_ms * 0.03, 0.8, 0.5, 1.0);
                    draw_rect_border((int)(2 * g_dpi_scale + 0.5), (int)(30 * g_dpi_scale + 0.5), fb_w - (int)(4 * g_dpi_scale + 0.5), fb_h - (int)(32 * g_dpi_scale + 0.5), border);

                    int64_t fid = g_app.fonts[font_idx].font_id;
                    const char* name = g_app.fonts[font_idx].name;

                    // Gigantic font name (centered, upper third)
                    draw_glow_text_centered(ctx, fb_w / 2, (int)(50 * g_dpi_scale + 0.5), name, 0xFFFFFFFF, fid);

                    // Pangram (centered, middle)
                    const char* pangram = "The quick brown fox jumps over the lazy dog";
                    draw_glow_text_centered(ctx, fb_w / 2, (int)(100 * g_dpi_scale + 0.5), pangram, 0xFFE0E0F0, fid);

                    // Full alphabet
                    draw_glow_text_centered(ctx, fb_w / 2, (int)(140 * g_dpi_scale + 0.5),
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 0xFFAADDFF, fid);
                    draw_glow_text_centered(ctx, fb_w / 2, (int)(170 * g_dpi_scale + 0.5),
                        "abcdefghijklmnopqrstuvwxyz", 0xFF88AACC, fid);

                    // Digits & special
                    draw_glow_text_centered(ctx, fb_w / 2, (int)(200 * g_dpi_scale + 0.5),
                        "0123456789  !@#$%^&*()  []{}<>?/+=_-:;,.~",
                        0xFFFFAA88, fid);

                    // Size info
                    char sz_info[64];
                    snprintf(sz_info, sizeof(sz_info), "Size: 36px  |  Font %d / %d",
                             font_idx + 1, g_app.font_count);
                    draw_glow_text_centered(ctx, fb_w / 2, (int)(240 * g_dpi_scale + 0.5), sz_info,
                        hsl_to_rgb(g_app.total_time_ms * 0.05, 0.9, 0.6, 1.0), fid);

                    // Second pangram
                    draw_glow_text_centered(ctx, fb_w / 2, (int)(280 * g_dpi_scale + 0.5),
                        "How quickly daft jumping zebras vex!  0123456789",
                        0xFFCCCCDD, fid);

                    // Extra showcase if room
                    if (fb_h > 400) {
                        draw_glow_text_centered(ctx, fb_w / 2, (int)(330 * g_dpi_scale + 0.5),
                            "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz",
                            0xFFBBBBCC, fid);
                        draw_glow_text_centered(ctx, fb_w / 2, (int)(370 * g_dpi_scale + 0.5),
                            "!@#$%^&*()_+-=[]{}|;':\",./<>?`~ ¡™£¢∞§¶•ªº–≠",
                            0xFF99AACC, fid);
                    }
                }

            } else {
                // ── NORMAL: 3-column carousel ─────────────────────────
                int col_w = fb_w / COLUMN_COUNT;
                int col_start_y = (int)(32 * g_dpi_scale + 0.5);
                int col_h = fb_h - col_start_y - 2;

                // Column separator lines
                for (int c = 1; c < COLUMN_COUNT; c++) {
                    int sx = c * col_w;
                    for (int sy = col_start_y; sy < fb_h; sy++) {
                        if (sy >= 0 && sy < fb_h && sx >= 0 && sx < fb_w)
                            fb[sy * stride + sx] = ui_color_blend(0x30FFFFFF, fb[sy * stride + sx]);
                    }
                }

                // Top accent line (always valid since x ranges 0..fb_w-1)
                if (col_start_y - 1 >= 0 && col_start_y - 1 < fb_h) {
                    for (int x = 0; x < fb_w; x++) {
                        fb[(col_start_y - 1) * stride + x] = 0xFF44AAFF;
                    }
                }

                uint32_t col_colors[] = {0xFFFF4488, 0xFF44FF88, 0xFF4488FF};

                for (int c = 0; c < COLUMN_COUNT; c++) {
                    int cx = c * col_w;
                    int font_idx = g_app.col_font_idx[c];
                    char label[8];
                    snprintf(label, sizeof(label), "COL %d", c + 1);

                    draw_font_preview(ctx, cx + 1, col_start_y + 1, col_w - 2, col_h - 2,
                                      font_idx, col_colors[c], label);
                }
            }

            // ── Draw HUD ──────────────────────────────────────────────
            draw_hud(ctx);

            // ── Glitch effect ─────────────────────────────────────────
            apply_glitch();

            // ── Screen shake visual indicator (subtle border wobble) ──
            if (g_app.size_rage_val == 12 || g_app.size_rage_val == 72) {
                // Flash effect at extremes
                uint32_t flash = (g_app.size_rage_val == 72) ? 0x15FFFFFF : 0x150000FF;
                draw_rect_border(0, 0, fb_w, fb_h, flash);
            }
        }

        // ── End frame ─────────────────────────────────────────────────
        abi_ui_end_frame(g_app.session_id);

        // ── Trigger repaint ───────────────────────────────────────────
        if (g_host && g_host->hwnd) {
            InvalidateRect(g_host->hwnd, NULL, FALSE);
        }

        // ── Sleep to prevent CPU burn ─────────────────────────────────
        Sleep(1);
    }

    // ── Cleanup ───────────────────────────────────────────────────────
    printf("\n[SHUTDOWN] Exiting Font Inferno...\n");
    ui_widget_destroy(ctx);
    abi_ui_session_destroy(g_app.session_id);
    printf("[DONE] Goodbye!\n");
    return 0;
}
