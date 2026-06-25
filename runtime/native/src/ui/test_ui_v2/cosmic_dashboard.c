// ============================================================================
//  cosmic_dashboard.c — COSMIC DASHBOARD UI Demo for Kain Native UI
//  ============================================================================
//  Style: Dark space theme, animated nebula background, floating glass-morphism
//  panels, real-time telemetry data. A NASA/JPL mission control dashboard
//  designed by a sci-fi artist.
//
//  Features:
//    - 300+ particle starfield with parallax depth and drift
//    - Shifting nebula gradient background (deep blues, purples, magentas)
//    - 6 floating glass panels with colored borders
//    - 8 different fonts loaded from C:/Windows/Fonts/
//    - Live FPS counter (monospace top-right)
//    - Animated scanline sweep every 3 seconds
//    - Interactive: click panels to reorder (z-order), toggle nebula,
//      drag panels by title bars, slider for particle speed
//    - Keyboard: Space=pause, 1-6=toggle panels, Escape=exit
//
//  Build:
//    clang -std=c11 -g -O0 cosmic_dashboard.c ../widgets/stubs.c ^
//      ../widgets/ui_widget.c ../ui_system.c ../ui_host_adapter.c ^
//      ../ui_renderer.c ../ui_layout.c ../ui_color.c ^
//      -I../../../include -I.. -I../widgets -I../../core ^
//      -luser32 -lgdi32 -lopengl32 -o cosmic_dashboard.exe
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
#include "ui_widget.h"
#include "ui_renderer.h"
#include "ui_layout.h"

// ── Stubs ──────────────────────────────────────────────────────────────
char* string_new(char* src);
double kain_clampd(double value, double min_value, double max_value);

// ── Win32 Host struct (must match ui_host_adapter.c) ───────────────────
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

// ── Color palette — deep space ─────────────────────────────────────────
#define C_SPACE_0      0xFF0A0A14
#define C_SPACE_1      0xFF0F0F1E
#define C_SPACE_2      0xFF14142A
#define C_GLASS_BG     0xAA15152A
#define C_GLASS_BG2    0x99121224
#define C_GLASS_BORDER 0x44FFFFFF
#define C_GLASS_SHINE  0x08FFFFFF
#define C_ACCENT_CYAN  0xFF00E5FF
#define C_ACCENT_BLUE  0xFF2979FF
#define C_ACCENT_PURP  0xFF7C4DFF
#define C_ACCENT_PINK  0xFFFF4081
#define C_ACCENT_GREEN 0xFF00E676
#define C_ACCENT_AMBER 0xFFFFAB00
#define C_ACCENT_ORANGE 0xFFFF6D00
#define C_TEXT_PRIMARY 0xFFE0E0F0
#define C_TEXT_SECOND  0xFF9090B0
#define C_TEXT_DIM     0xFF505070
#define C_FPS_GREEN    0xFF00FF88
#define C_SCANLINE     0x60FFFFFF

// ── Panel accent colors ───────────────────────────────────────────────
static const uint32_t g_panel_accents[] = {
    C_ACCENT_CYAN,   // Panel 0: System Status
    C_ACCENT_BLUE,   // Panel 1: Stellar Data
    C_ACCENT_PURP,   // Panel 2: Signal Telemetry
    C_ACCENT_PINK,   // Panel 3: Command Console
    C_ACCENT_GREEN,  // Panel 4: Navigation
    C_ACCENT_AMBER,  // Panel 5: Particle Count
};

// ── Dashboard dimensions (set at runtime) ──────────────────────────────
static int g_win_w = 1280;
static int g_win_h = 720;
static int g_header_h = 48;
static int g_margin = 14;
static int g_title_bar_h = 28;

// ── Global state ───────────────────────────────────────────────────────
static int64_t g_session_id = -1;
static KainWin32UiHost* g_host = NULL;

// ── Particle system ────────────────────────────────────────────────────
#define MAX_PARTICLES 350
typedef struct {
    float x, y, z;        // Position (z for parallax: 1.0=far, 3.0=near)
    float vx, vy;         // Drift velocity per second
    float brightness;     // 0.0 – 1.0
    float size;           // 0.5 – 3.0 pixels
    float phase;          // Random phase for twinkling (0 – 2π)
} Particle;

static Particle g_particles[MAX_PARTICLES];
static int g_particle_count = 300;
static double g_particle_speed = 1.0;    // Multiplier (controlled by slider)
static int g_particles_initialized = 0;

// ── Nebula state ───────────────────────────────────────────────────────
static double g_nebula_time = 0.0;
static int g_nebula_enabled = 1;

// ── Panel content types ────────────────────────────────────────────────
typedef struct PanelContent {
    // System Status (panel 0)
    double cpu_usage;
    double mem_usage;
    double uptime_seconds;
    
    // Stellar Data (panel 1)
    float stars_chart[60];       // x,y positions for star chart
    
    // Signal Telemetry (panel 2)
    float waveform_buffer[400];  // Ring buffer for scrolling wave
    
    // Command Console (panel 3)
    char log_lines[16][72];      // Ring buffer of log lines
    int log_count;
    
    // Navigation (panel 4)
    double compass_angle;        // Slowly rotating
    
    // Particle Count (panel 5)
    double ring_angle;           // Rotation phase
} PanelContent;

// ── Panel system ───────────────────────────────────────────────────────
#define MAX_PANELS 6
typedef struct {
    int visible;
    int z_order;           // Higher = on top
    double x, y, w, h;     // Position and size
    double drag_offset_x, drag_offset_y;
    int dragging;           // 1 while being dragged
    int hovered_close;      // 1 if close button is hovered
    double anim_time;       // Per-panel animation phase
    char title[32];
    uint32_t accent_color;
    struct PanelContent content;
} DashboardPanel;

static DashboardPanel g_panels[MAX_PANELS];
static int g_focused_panel = -1;     // Panel being interacted with

// ── Scanline effect ────────────────────────────────────────────────────
static double g_scanline_timer = 0.0;
static double g_scanline_y = -100.0; // Off-screen
static int g_scanline_active = 0;

// ── Animation / timing ─────────────────────────────────────────────────
static double g_total_time = 0.0;
static double g_fps = 60.0;
static int g_frame_count = 0;
static double g_fps_timer = 0.0;
static int g_animation_paused = 0;

// ── Font table ─────────────────────────────────────────────────────────
#define MAX_FONTS 12
typedef struct {
    char name[48];
    double size;
    int64_t font_id;
    int loaded;
} FontEntry;

static FontEntry g_fonts[MAX_FONTS];
static int g_font_count = 0;

// ── Cursor ─────────────────────────────────────────────────────────────
static int g_cursor_visible = 1;

// ── Math utilities ─────────────────────────────────────────────────────
static float frand(void) {
    return (float)rand() / (float)RAND_MAX;
}

static float frand_range(float lo, float hi) {
    return lo + frand() * (hi - lo);
}

static double clamp(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// ── Helper: get font_id by name ────────────────────────────────────────
static int64_t find_font(const char* name) {
    for (int i = 0; i < g_font_count; i++) {
        if (g_fonts[i].loaded && strcmp(g_fonts[i].name, name) == 0) {
            return g_fonts[i].font_id;
        }
    }
    return 0;
}

// ── Load all fonts ─────────────────────────────────────────────────────
static void load_all_fonts(KainUiWidgetContext* wctx) {
    // Note: ui_widget_load_font scales internally by ctx->dpi_scale,
    // so pass logical sizes without pre-scaling.
    // Font 0: Arial 14 — general panel text
    int64_t fid = ui_widget_load_font(wctx, "C:/Windows/Fonts/arial.ttf", 14.0);
    snprintf(g_fonts[g_font_count].name, sizeof(g_fonts[0].name), "arial14");
    g_fonts[g_font_count].font_id = fid;
    g_fonts[g_font_count].loaded = (fid > 0);
    g_fonts[g_font_count].size = 14.0;
    g_font_count++;

    // Font 1: Consolas 11 — monospace data, FPS counter
    fid = ui_widget_load_font(wctx, "C:/Windows/Fonts/consola.ttf", 11.0);
    snprintf(g_fonts[g_font_count].name, sizeof(g_fonts[0].name), "consola11");
    g_fonts[g_font_count].font_id = fid;
    g_fonts[g_font_count].loaded = (fid > 0);
    g_fonts[g_font_count].size = 11.0;
    g_font_count++;

    // Font 2: Consolas 14 — telemetry data
    fid = ui_widget_load_font(wctx, "C:/Windows/Fonts/consola.ttf", 14.0);
    snprintf(g_fonts[g_font_count].name, sizeof(g_fonts[0].name), "consola14");
    g_fonts[g_font_count].font_id = fid;
    g_fonts[g_font_count].loaded = (fid > 0);
    g_fonts[g_font_count].size = 14.0;
    g_font_count++;

    // Font 3: Impact 18 — panel header titles
    fid = ui_widget_load_font(wctx, "C:/Windows/Fonts/impact.ttf", 18.0);
    snprintf(g_fonts[g_font_count].name, sizeof(g_fonts[0].name), "impact18");
    g_fonts[g_font_count].font_id = fid;
    g_fonts[g_font_count].loaded = (fid > 0);
    g_fonts[g_font_count].size = 18.0;
    g_font_count++;

    // Font 4: Georgia 12 — elegant second text
    fid = ui_widget_load_font(wctx, "C:/Windows/Fonts/georgia.ttf", 12.0);
    snprintf(g_fonts[g_font_count].name, sizeof(g_fonts[0].name), "georgia12");
    g_fonts[g_font_count].font_id = fid;
    g_fonts[g_font_count].loaded = (fid > 0);
    g_fonts[g_font_count].size = 12.0;
    g_font_count++;

    // Font 5: Verdana 10 — compact data labels
    fid = ui_widget_load_font(wctx, "C:/Windows/Fonts/verdana.ttf", 10.0);
    snprintf(g_fonts[g_font_count].name, sizeof(g_fonts[0].name), "verdana10");
    g_fonts[g_font_count].font_id = fid;
    g_fonts[g_font_count].loaded = (fid > 0);
    g_fonts[g_font_count].size = 10.0;
    g_font_count++;

    // Font 6: Tahoma 14 — UI element text
    fid = ui_widget_load_font(wctx, "C:/Windows/Fonts/tahoma.ttf", 14.0);
    snprintf(g_fonts[g_font_count].name, sizeof(g_fonts[0].name), "tahoma14");
    g_fonts[g_font_count].font_id = fid;
    g_fonts[g_font_count].loaded = (fid > 0);
    g_fonts[g_font_count].size = 14.0;
    g_font_count++;

    // Font 7: CascadiaMono 16 — large data readouts
    fid = ui_widget_load_font(wctx, "C:/Windows/Fonts/CascadiaMono.ttf", 16.0);
    snprintf(g_fonts[g_font_count].name, sizeof(g_fonts[0].name), "cascadia16");
    g_fonts[g_font_count].font_id = fid;
    g_fonts[g_font_count].loaded = (fid > 0);
    g_fonts[g_font_count].size = 16.0;
    g_font_count++;

    // Font 8: CascadiaMono 22 — very large values
    fid = ui_widget_load_font(wctx, "C:/Windows/Fonts/CascadiaMono.ttf", 22.0);
    snprintf(g_fonts[g_font_count].name, sizeof(g_fonts[0].name), "cascadia22");
    g_fonts[g_font_count].font_id = fid;
    g_fonts[g_font_count].loaded = (fid > 0);
    g_fonts[g_font_count].size = 22.0;
    g_font_count++;

    // Font 9: Arial 10 — small labels
    fid = ui_widget_load_font(wctx, "C:/Windows/Fonts/arial.ttf", 10.0);
    snprintf(g_fonts[g_font_count].name, sizeof(g_fonts[0].name), "arial10");
    g_fonts[g_font_count].font_id = fid;
    g_fonts[g_font_count].loaded = (fid > 0);
    g_fonts[g_font_count].size = 10.0;
    g_font_count++;

    // Font 10: Impact 24 — HUD-style big text
    fid = ui_widget_load_font(wctx, "C:/Windows/Fonts/impact.ttf", 24.0);
    snprintf(g_fonts[g_font_count].name, sizeof(g_fonts[0].name), "impact24");
    g_fonts[g_font_count].font_id = fid;
    g_fonts[g_font_count].loaded = (fid > 0);
    g_fonts[g_font_count].size = 24.0;
    g_font_count++;

    // Set default font to consola14 for widget text rendering
    if (find_font("consola14") > 0) {
        for (int i = 0; i < wctx->font_count; i++) {
            if (wctx->fonts[i].font_id == find_font("consola14")) {
                wctx->default_font = i;
                break;
            }
        }
    }

    printf("[FONTS] Loaded %d/%d fonts\n", g_font_count, MAX_FONTS);
}

// ── Draw text with a specific named font ──────────────────────────────
static void draw_text_font(KainUiWidgetContext* wctx, int x, int y,
                            const char* text, uint32_t color, const char* font_name)
{
    int64_t fid = find_font(font_name);
    if (fid > 0) {
        ui_widget_draw_text_ex(wctx, x, y, text, color, 0, fid);
    } else {
        ui_widget_draw_text(wctx, x, y, text, color, 14);
    }
}

// ── Measure text width with a named font ──────────────────────────────
static int text_width_font(KainUiWidgetContext* wctx, const char* text, const char* font_name)
{
    if (!text || !text[0]) return 0;
    int64_t fid = find_font(font_name);
    if (fid > 0) {
        int tw = (int)(abi_ui_text_measure_width(g_session_id, fid, text) + 0.5);
        return tw;
    }
    return (int)strlen(text) * 8;
}

// ============================================================================
//   PARTICLE SYSTEM
// ============================================================================

static void init_particles(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        g_particles[i].x = frand() * 2000.0f - 100.0f;
        g_particles[i].y = frand() * 1200.0f - 100.0f;
        g_particles[i].z = 1.0f + frand() * 2.5f;      // 1.0–3.5 parallax depth
        g_particles[i].vx = frand_range(-8.0f, 8.0f);
        g_particles[i].vy = frand_range(-4.0f, 4.0f);
        g_particles[i].brightness = 0.1f + frand() * 0.9f;
        g_particles[i].size = 0.5f + frand() * 2.5f;
        g_particles[i].phase = frand() * 6.2832f;
    }
    g_particles_initialized = 1;
}

static void update_particles(double dt) {
    if (g_animation_paused) return;
    double speed = g_particle_speed * dt;

    for (int i = 0; i < g_particle_count; i++) {
        Particle* p = &g_particles[i];
        // Far particles (z=1) move slow, near (z=3) move fast — parallax
        float parallax = 1.0f / p->z;
        
        p->x += p->vx * (float)speed * parallax;
        p->y += p->vy * (float)speed * parallax;

        // Wrap around screen edges with some margin
        if (p->x > g_win_w + 50) p->x = -50.0f;
        if (p->x < -50.0f) p->x = (float)(g_win_w + 50);
        if (p->y > g_win_h + 50) p->y = -50.0f;
        if (p->y < -50.0f) p->y = (float)(g_win_h + 50);
    }
}

static void draw_particles(uint32_t* fb, int stride, int fb_w, int fb_h) {
    if (!fb) return;

    for (int i = 0; i < g_particle_count; i++) {
        Particle* p = &g_particles[i];
        
        // Twinkle: brightness oscillates with sine
        float twinkle = 0.6f + 0.4f * sinf((float)g_total_time * 2.0f + p->phase);
        float b = p->brightness * twinkle;
        
        // Z-based depth darkening: far = dimmer
        float depth_fade = 1.0f - (p->z - 1.0f) * 0.15f;
        if (depth_fade < 0.3f) depth_fade = 0.3f;
        
        int alpha = (int)(b * depth_fade * 255.0f);
        if (alpha < 8) alpha = 8;
        if (alpha > 220) alpha = 220;
        
        int px = (int)p->x;
        int py = (int)p->y;
        int sz = (int)(p->size + 0.5f);
        
        // Star color: warm (yellowish) to cool (blue-white) based on brightness
        uint8_t r = (uint8_t)(200 + (uint8_t)(b * 55.0f));
        uint8_t g = (uint8_t)(200 + (uint8_t)(b * 55.0f));
        uint8_t bv = (uint8_t)(220 + (uint8_t)(b * 35.0f));
        uint32_t star_color = ((uint32_t)alpha << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bv;
        
        // Draw star as a small square or cross
        if (sz <= 1) {
            if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                fb[py * stride + px] = ui_color_blend(star_color, fb[py * stride + px]);
        } else if (sz == 2) {
            for (int dy = 0; dy < 2; dy++) {
                for (int dx = 0; dx < 2; dx++) {
                    int sx = px + dx, sy = py + dy;
                    if (sx >= 0 && sx < fb_w && sy >= 0 && sy < fb_h)
                        fb[sy * stride + sx] = ui_color_blend(star_color, fb[sy * stride + sx]);
                }
            }
        } else {
            // Larger star: draw a cross-sparkle shape
            uint32_t core = star_color;
            uint32_t glow = ((uint32_t)(alpha / 3) << 24) | 0x00FFFFFF;
            
            // Center pixel
            if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                fb[py * stride + px] = ui_color_blend(core, fb[py * stride + px]);
            
            // Cross arms
            int arms[] = {-1,0, 1,0, 0,-1, 0,1, -1,-1, 1,-1, -1,1, 1,1};
            for (int a = 0; a < 8; a++) {
                int sx = px + arms[a*2], sy = py + arms[a*2+1];
                if (sx >= 0 && sx < fb_w && sy >= 0 && sy < fb_h)
                    fb[sy * stride + sx] = ui_color_blend(glow, fb[sy * stride + sx]);
            }
        }
    }
}

// ============================================================================
//   NEBULA GRADIENT
// ============================================================================

static void draw_nebula(uint32_t* fb, int stride, int fb_w, int fb_h, double time) {
    if (!fb || !g_nebula_enabled) return;

    // Shifting nebula with multiple sine-wave color bands
    // Cycle through deep blues, purples, and magentas
    for (int y = 0; y < fb_h; y++) {
        float ny = (float)y / (float)fb_h;
        for (int x = 0; x < fb_w; x++) {
            float nx = (float)x / (float)fb_w;
            
            // Multiple octaves of noise-like sine waves
            float v1 = sinf(nx * 3.0f + ny * 2.5f + (float)time * 0.15f) * 0.5f + 0.5f;
            float v2 = sinf(nx * 5.5f - ny * 4.0f + (float)time * 0.10f) * 0.5f + 0.5f;
            float v3 = sinf((nx + ny) * 2.0f + (float)time * 0.08f) * 0.5f + 0.5f;
            float v4 = sinf(nx * 7.0f + ny * 3.0f - (float)time * 0.12f) * 0.5f + 0.5f;
            
            // Blend nebula colors: deep blue, purple, magenta, indigo
            float r = v1 * 8.0f + v3 * 15.0f;
            float g = v2 * 4.0f + v4 * 6.0f + v1 * 10.0f;
            float b = v1 * 20.0f + v2 * 18.0f + v4 * 12.0f;
            
            // Central glow (brighter near center)
            float cx = nx - 0.5f, cy = ny - 0.5f;
            float center_dist = sqrtf(cx*cx + cy*cy) * 1.4f;
            float center_fade = 1.0f - center_dist;
            if (center_fade < 0.0f) center_fade = 0.0f;
            r += center_fade * 12.0f;
            g += center_fade * 8.0f;
            b += center_fade * 20.0f;
            
            // Clamp
            if (r > 30.0f) r = 30.0f;
            if (g > 25.0f) g = 25.0f;
            if (b > 40.0f) b = 40.0f;
            
            // Pack as semi-transparent overlay (alpha ~0.15)
            uint8_t ar = (uint8_t)(r * 2.0f);
            uint8_t ag = (uint8_t)(g * 2.0f);
            uint8_t ab = (uint8_t)(b * 2.5f);
            uint32_t nebula_color = (0x25 << 24) | (ar << 16) | (ag << 8) | ab;
            
            fb[y * stride + x] = ui_color_blend(nebula_color, fb[y * stride + x]);
        }
    }
}

// ============================================================================
//   PANEL SYSTEM
// ============================================================================

static void init_panels(void) {
    const char* titles[] = {
        "S Y S T E M   S T A T U S",
        "S T E L L A R   D A T A",
        "S I G N A L   T E L E M E T R Y",
        "C O M M A N D   C O N S O L E",
        "N A V I G A T I O N",
        "P A R T I C L E   F L U X"
    };

    for (int i = 0; i < MAX_PANELS; i++) {
        g_panels[i].visible = 1;
        g_panels[i].z_order = i;
        g_panels[i].x = 0;
        g_panels[i].y = 0;
        g_panels[i].w = 0;
        g_panels[i].h = 0;
        g_panels[i].dragging = 0;
        g_panels[i].hovered_close = 0;
        g_panels[i].anim_time = frand() * 6.2832;
        g_panels[i].accent_color = g_panel_accents[i];
        strncpy(g_panels[i].title, titles[i], sizeof(g_panels[i].title) - 1);
        g_panels[i].title[sizeof(g_panels[i].title) - 1] = '\0';
    }
    
    // Content initialization
    g_panels[0].content.cpu_usage = 45.0;
    g_panels[0].content.mem_usage = 62.0;
    g_panels[0].content.uptime_seconds = 0.0;
    
    // Stellar chart: pre-place random dots
    for (int i = 0; i < 60; i += 2) {
        g_panels[1].content.stars_chart[i] = frand() * 300.0f;
        g_panels[1].content.stars_chart[i+1] = frand() * 160.0f;
    }
    
    // Waveform: initialize to sine
    for (int i = 0; i < 400; i++) {
        g_panels[2].content.waveform_buffer[i] = sinf(i * 0.05f);
    }
    
    // Console log: initial messages
    g_panels[3].content.log_count = 0;
    snprintf(g_panels[3].content.log_lines[g_panels[3].content.log_count % 16],
             sizeof(g_panels[3].content.log_lines[0]),
             "[SYS] COSMIC DASHBOARD v1.0 ONLINE");
    g_panels[3].content.log_count++;
    snprintf(g_panels[3].content.log_lines[g_panels[3].content.log_count % 16],
             sizeof(g_panels[3].content.log_lines[0]),
             "[SYS] All telemetry channels nominal");
    g_panels[3].content.log_count++;
    snprintf(g_panels[3].content.log_lines[g_panels[3].content.log_count % 16],
             sizeof(g_panels[3].content.log_lines[0]),
             "[NAV] Orbital insertion complete");
    g_panels[3].content.log_count++;
    snprintf(g_panels[3].content.log_lines[g_panels[3].content.log_count % 16],
             sizeof(g_panels[3].content.log_lines[0]),
             "[SIG] Carrier lock acquired on channel 7");
    g_panels[3].content.log_count++;
    snprintf(g_panels[3].content.log_lines[g_panels[3].content.log_count % 16],
             sizeof(g_panels[3].content.log_lines[0]),
             "[ENV] Radiation levels: nominal");
    g_panels[3].content.log_count++;
    
    g_panels[4].content.compass_angle = 0.0;
    g_panels[4].content.ring_angle = 0.0;
    g_panels[5].content.ring_angle = 0.0;
}

static void layout_panels(void) {
    int margin = (int)(g_margin * g_dpi + 0.5f);
    int title_h = (int)(g_header_h * g_dpi + 0.5f);
    int tb_h = (int)(g_title_bar_h * g_dpi + 0.5f);
    int ww = g_win_w;
    int wh = g_win_h;
    
    // 3-column, 2-row grid layout
    int cols = 3;
    int rows = 2;
    int gap_x = margin;
    int gap_y = margin;
    int usable_w = ww - margin * (cols + 1);
    int usable_h = wh - title_h - margin * (rows + 1);
    int panel_w = usable_w / cols;
    int panel_h = usable_h / rows;

    for (int i = 0; i < MAX_PANELS; i++) {
        int col = i % cols;
        int row = i / cols;
        int px = margin + col * (panel_w + gap_x);
        int py = title_h + margin + row * (panel_h + gap_y);
        
        // If panel hasn't been manually repositioned, use grid layout
        if (!g_panels[i].dragging) {
            g_panels[i].x = (double)px;
            g_panels[i].y = (double)py;
            g_panels[i].w = (double)panel_w;
            g_panels[i].h = (double)panel_h;
        }
    }
}

// ── Draw glass panel background ───────────────────────────────────────
static void draw_panel_bg(uint32_t* fb, int stride, int fb_w, int fb_h,
                           DashboardPanel* p)
{
    int ix = (int)p->x, iy = (int)p->y, iw = (int)p->w, ih = (int)p->h;
    int r = (int)(8 * g_dpi + 0.5f); // corner radius
    if (ix + iw < 0 || iy + ih < 0 || ix > fb_w || iy > fb_h) return;
    
    // Glass background (dark semi-transparent)
    for (int row = iy; row < iy + ih && row < fb_h; row++) {
        if (row < 0) continue;
        for (int col = ix; col < ix + iw && col < fb_w; col++) {
            if (col < 0) continue;
            
            // Corner radius check
            int inside = 1;
            if (col < ix + r && row < iy + r) {
                int dx = (ix + r) - col - 1;
                int dy = (iy + r) - row - 1;
                inside = (dx >= 0 && dy >= 0 && dx*dx + dy*dy <= r*r);
            } else if (col >= ix + iw - r && row < iy + r) {
                int dx = col - (ix + iw - r) + 1;
                int dy = (iy + r) - row - 1;
                inside = (dx >= 0 && dy >= 0 && dx*dx + dy*dy <= r*r);
            } else if (col < ix + r && row >= iy + ih - r) {
                int dx = (ix + r) - col - 1;
                int dy = row - (iy + ih - r) + 1;
                inside = (dx >= 0 && dy >= 0 && dx*dx + dy*dy <= r*r);
            } else if (col >= ix + iw - r && row >= iy + ih - r) {
                int dx = col - (ix + iw - r) + 1;
                int dy = row - (iy + ih - r) + 1;
                inside = (dx >= 0 && dy >= 0 && dx*dx + dy*dy <= r*r);
            }
            
            if (!inside) continue;
            
            uint32_t dst = fb[row * stride + col];
            
            // Glass base: dark transluscent
            uint32_t glass = C_GLASS_BG;
            
            // Subtle shine gradient at top
            float top_fade = 1.0f - (float)(row - iy) / (float)ih;
            if (top_fade > 0.0f) {
                int shine = (int)(top_fade * 12.0f);
                uint8_t sr = (uint8_t)((glass >> 16) & 0xFF) + shine;
                uint8_t sg = (uint8_t)((glass >> 8) & 0xFF) + shine;
                uint8_t sb = (uint8_t)(glass & 0xFF) + shine + 5;
                if (sr > 255) sr = 255; if (sg > 255) sg = 255; if (sb > 255) sb = 255;
                glass = (glass & 0xFF000000) | (sr << 16) | (sg << 8) | sb;
            }
            
            fb[row * stride + col] = ui_color_blend(glass, dst);
        }
    }
    
    // Accent top border (colored line at the very top)
    uint32_t accent = g_panel_accents[0]; // default
    for (int pi = 0; pi < MAX_PANELS; pi++) {
        if (&g_panels[pi] == p) { accent = g_panel_accents[pi]; break; }
    }
    for (int col = ix + r; col < ix + iw - r && col < fb_w; col++) {
        if (col >= 0 && iy >= 0 && iy < fb_h)
            fb[iy * stride + col] = accent;
    }
    
    // Subtle border
    uint32_t border = C_GLASS_BORDER;
    // Top (already drawn, but draw thin line below accent)
    for (int col = ix; col < ix + iw && col < fb_w; col++) {
        if (col >= 0 && iy + 1 >= 0 && iy + 1 < fb_h)
            fb[(iy + 1) * stride + col] = ui_color_blend(0x10FFFFFF, fb[(iy + 1) * stride + col]);
    }
    
    // Title bar separator
    int tby = iy + g_title_bar_h;
    for (int col = ix + 2; col < ix + iw - 2 && col < fb_w; col++) {
        if (col >= 0 && tby >= 0 && tby < fb_h)
            fb[tby * stride + col] = ui_color_blend(accent, fb[tby * stride + col]);
    }
}

// ── Draw panel title bar ──────────────────────────────────────────────
static void draw_panel_title(KainUiWidgetContext* wctx, int px, int py, int pw,
                              const char* title, uint32_t accent, int hover_close)
{
    int tby = py + 4;
    
    // Title text with impact font
    draw_text_font(wctx, px + 10, tby, title, C_TEXT_PRIMARY, "impact18");
    
    // Close button (small X in top-right)
    int close_x = px + pw - 24;
    int close_y = py + 4;
    int close_s = 18;
    
    if (hover_close) {
        ui_widget_fill_rect((uint32_t*)g_host->framebuffer, g_host->fb_stride / 4,
                           g_host->width, g_host->height,
                           close_x, close_y, close_s, close_s, 0x44FF4060);
    }
    
    // Draw X
    uint32_t* fb = (uint32_t*)g_host->framebuffer;
    int stride = g_host->fb_stride / 4;
    int fb_w = g_host->width, fb_h = g_host->height;
    uint32_t x_color = hover_close ? 0xFFFFFFFF : 0x88FFFFFF;
    for (int i = 0; i < 8; i++) {
        int x1 = close_x + 5 + i, y1 = close_y + 5 + i;
        int x2 = close_x + 5 + i, y2 = close_y + 13 - i;
        if (x1 >= 0 && x1 < fb_w && y1 >= 0 && y1 < fb_h)
            fb[y1 * stride + x1] = ui_color_blend(x_color, fb[y1 * stride + x1]);
        if (x2 >= 0 && x2 < fb_w && y2 >= 0 && y2 < fb_h)
            fb[y2 * stride + x2] = ui_color_blend(x_color, fb[y2 * stride + x2]);
    }
}

// ============================================================================
//   PANEL CONTENT RENDERING
// ============================================================================

// ── Panel 0: System Status ─────────────────────────────────────────────
static void draw_panel_system_status(KainUiWidgetContext* wctx, DashboardPanel* p) {
    int cx = (int)p->x + 12;
    int cy = (int)p->y + (int)g_title_bar_h + 10;
    int cw = (int)p->w - 24;
    int ch = (int)p->h - (int)g_title_bar_h - 18;
    
    // Clock display
    time_t rawtime;
    struct tm* timeinfo;
    char timebuf[64];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", timeinfo);
    
    draw_text_font(wctx, cx, cy, timebuf, C_ACCENT_CYAN, "cascadia22");
    draw_text_font(wctx, cx + 135, cy + 6, "UTC", C_TEXT_DIM, "verdana10");
    
    // Uptime
    g_panels[0].content.uptime_seconds += 0.016;
    int up_h = (int)(g_panels[0].content.uptime_seconds) / 3600;
    int up_m = ((int)(g_panels[0].content.uptime_seconds) % 3600) / 60;
    int up_s = (int)(g_panels[0].content.uptime_seconds) % 60;
    char uptime[32];
    snprintf(uptime, sizeof(uptime), "UPTIME  %02d:%02d:%02d", up_h, up_m, up_s);
    draw_text_font(wctx, cx, cy + 36, uptime, C_TEXT_SECOND, "consola11");
    
    // CPU Gauge
    if (!g_animation_paused) {
        g_panels[0].content.cpu_usage += (frand() - 0.5) * 3.0;
        if (g_panels[0].content.cpu_usage < 10) g_panels[0].content.cpu_usage = 10;
        if (g_panels[0].content.cpu_usage > 95) g_panels[0].content.cpu_usage = 95;
    }
    
    int gauge_y = cy + 58;
    int gauge_w = cw;
    int gauge_h = 14;
    
    // CPU label
    draw_text_font(wctx, cx, gauge_y, "CPU", C_TEXT_DIM, "arial10");
    
    // CPU bar background
    ui_widget_fill_rounded_rect((uint32_t*)g_host->framebuffer, g_host->fb_stride / 4,
                                g_host->width, g_host->height,
                                cx, gauge_y + 14, gauge_w, gauge_h, 0x44151530, 4);
    
    // CPU bar fill
    int cpu_w = (int)(g_panels[0].content.cpu_usage / 100.0 * gauge_w);
    if (cpu_w > 2) {
        uint32_t cpu_color = g_panels[0].content.cpu_usage > 80 ? C_ACCENT_PINK :
                             g_panels[0].content.cpu_usage > 50 ? C_ACCENT_AMBER : C_ACCENT_GREEN;
        ui_widget_fill_rounded_rect((uint32_t*)g_host->framebuffer, g_host->fb_stride / 4,
                                    g_host->width, g_host->height,
                                    cx, gauge_y + 14, cpu_w, gauge_h, cpu_color, 4);
    }
    
    // CPU percentage text
    char cpu_str[16];
    snprintf(cpu_str, sizeof(cpu_str), "%.0f%%", g_panels[0].content.cpu_usage);
    draw_text_font(wctx, cx + gauge_w - 45, gauge_y, cpu_str, C_TEXT_PRIMARY, "consola14");
    
    // MEM gauge
    int mem_y = gauge_y + 34;
    if (!g_animation_paused) {
        g_panels[0].content.mem_usage += (frand() - 0.5) * 1.5;
        if (g_panels[0].content.mem_usage < 30) g_panels[0].content.mem_usage = 30;
        if (g_panels[0].content.mem_usage > 92) g_panels[0].content.mem_usage = 92;
    }
    
    draw_text_font(wctx, cx, mem_y, "MEM", C_TEXT_DIM, "arial10");
    
    ui_widget_fill_rounded_rect((uint32_t*)g_host->framebuffer, g_host->fb_stride / 4,
                                g_host->width, g_host->height,
                                cx, mem_y + 14, gauge_w, gauge_h, 0x44151530, 4);
    
    int mem_w = (int)(g_panels[0].content.mem_usage / 100.0 * gauge_w);
    if (mem_w > 2) {
        ui_widget_fill_rounded_rect((uint32_t*)g_host->framebuffer, g_host->fb_stride / 4,
                                    g_host->width, g_host->height,
                                    cx, mem_y + 14, mem_w, gauge_h, C_ACCENT_BLUE, 4);
    }
    
    char mem_str[16];
    snprintf(mem_str, sizeof(mem_str), "%.0f%%", g_panels[0].content.mem_usage);
    draw_text_font(wctx, cx + gauge_w - 45, mem_y, mem_str, C_TEXT_PRIMARY, "consola14");
    
    // Network activity indicator (small pulsing dots)
    int dot_y = mem_y + 36;
    draw_text_font(wctx, cx, dot_y, "NET:", C_TEXT_DIM, "arial10");
    for (int i = 0; i < 5; i++) {
        double pulse = 0.3 + 0.7 * (0.5 + 0.5 * sin(g_total_time * 2.5 + i * 1.2));
        int alpha = (int)(pulse * 255);
        uint32_t dot_color = (alpha << 24) | (0x00 << 16) | (0xE5 << 8) | 0xFF;
        int dx = cx + 35 + i * 14;
        ui_widget_fill_rounded_rect((uint32_t*)g_host->framebuffer, g_host->fb_stride / 4,
                                    g_host->width, g_host->height,
                                    dx, dot_y + 2, 8, 8, dot_color, 4);
    }
}

// ── Panel 1: Stellar Data ─────────────────────────────────────────────
static void draw_panel_stellar(KainUiWidgetContext* wctx, DashboardPanel* p) {
    int cx = (int)p->x + 12;
    int cy = (int)p->y + (int)g_title_bar_h + 10;
    int cw = (int)p->w - 24;
    int ch = (int)p->h - (int)g_title_bar_h - 18;
    int chart_w = cw - 20;
    int chart_h = ch - 10;
    
    uint32_t* fb = (uint32_t*)g_host->framebuffer;
    int stride = g_host->fb_stride / 4;
    int fb_w = g_host->width, fb_h = g_host->height;
    
    // Constellation chart background
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h,
                                 cx, cy, chart_w, chart_h, 0x220A0A20, 6);
    
    int chart_cx = cx + 10;
    int chart_cy = cy + 10;
    int chart_w2 = chart_w - 20;
    int chart_h2 = chart_h - 20;
    
    // Animate star positions (slow drift)
    int num_stars = 25;
    
    // Draw connections between nearby stars (constellation lines)
    float star_x[30], star_y[30];
    for (int i = 0; i < num_stars; i++) {
        float base_x = g_panels[1].content.stars_chart[i * 2 % 60];
        float base_y = g_panels[1].content.stars_chart[(i * 2 + 1) % 60];
        // Slow drift
        float drift_x = sinf((float)g_total_time * 0.1f + i * 0.7f) * 3.0f;
        float drift_y = cosf((float)g_total_time * 0.08f + i * 1.1f) * 3.0f;
        star_x[i] = chart_cx + (base_x / 300.0f) * chart_w2 + drift_x;
        star_y[i] = chart_cy + (base_y / 160.0f) * chart_h2 + drift_y;
    }
    
    // Draw lines between stars that are close
    for (int i = 0; i < num_stars; i++) {
        for (int j = i + 1; j < num_stars; j++) {
            float dx = star_x[i] - star_x[j];
            float dy = star_y[i] - star_y[j];
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < 50.0f && dist > 5.0f) {
                int alpha = (int)((1.0f - dist / 50.0f) * 40 + 10);
                if (alpha > 60) alpha = 60;
                uint32_t line_color = (alpha << 24) | 0x0088CCFF;
                // Simple line drawing
                int steps = (int)dist;
                if (steps < 1) steps = 1;
                for (int s = 0; s <= steps; s++) {
                    int lx = (int)(star_x[i] + (star_x[j] - star_x[i]) * s / steps);
                    int ly = (int)(star_y[i] + (star_y[j] - star_y[i]) * s / steps);
                    if (lx >= 0 && lx < fb_w && ly >= 0 && ly < fb_h)
                        fb[ly * stride + lx] = ui_color_blend(line_color, fb[ly * stride + lx]);
                }
            }
        }
    }
    
    // Draw stars
    for (int i = 0; i < num_stars; i++) {
        float b = 0.4f + 0.6f * (0.5f + 0.5f * sinf((float)g_total_time * 0.5f + i * 1.7f));
        int alpha = (int)(b * 200);
        int sx = (int)star_x[i], sy = (int)star_y[i];
        uint32_t sc = (alpha << 24) | 0x00DDFFFF;
        if (sx >= 0 && sx < fb_w && sy >= 0 && sy < fb_h)
            fb[sy * stride + sx] = ui_color_blend(sc, fb[sy * stride + sx]);
        // Small glow
        if (b > 0.6f) {
            uint32_t glow = ((alpha / 2) << 24) | 0x004488FF;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int gx = sx + dx, gy = sy + dy;
                    if (gx >= 0 && gx < fb_w && gy >= 0 && gy < fb_h)
                        fb[gy * stride + gx] = ui_color_blend(glow, fb[gy * stride + gx]);
                }
        }
    }
    
    // Labels
    draw_text_font(wctx, cx + 6, cy + 6, "SECTOR 7-G", C_TEXT_DIM, "consola11");
    draw_text_font(wctx, cx + chart_w - 90, cy + 6, "LIVE", C_ACCENT_GREEN, "consola11");
}

// ── Panel 2: Signal Telemetry ─────────────────────────────────────────
static void draw_panel_signal(KainUiWidgetContext* wctx, DashboardPanel* p) {
    int cx = (int)p->x + 12;
    int cy = (int)p->y + (int)g_title_bar_h + 10;
    int cw = (int)p->w - 24;
    int ch = (int)p->h - (int)g_title_bar_h - 18;
    
    uint32_t* fb = (uint32_t*)g_host->framebuffer;
    int stride = g_host->fb_stride / 4;
    int fb_w = g_host->width, fb_h = g_host->height;
    
    int wave_y = cy + 20;
    int wave_h = ch - 50;
    int wave_w = cw - 10;
    int wave_x = cx + 5;
    
    if (wave_h < 20) wave_h = 20;
    
    // Waveform background
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h,
                                 wave_x, wave_y, wave_w, wave_h, 0x220A0A20, 4);
    
    // Grid lines
    for (int g = 0; g < 4; g++) {
        int gy = wave_y + wave_h * g / 4;
        for (int gx = wave_x; gx < wave_x + wave_w; gx++) {
            if (gx >= 0 && gx < fb_w && gy >= 0 && gy < fb_h)
                fb[gy * stride + gx] = ui_color_blend(0x10152040, fb[gy * stride + gx]);
        }
    }
    
    // Update waveform: shift buffer and add new sample
    if (!g_animation_paused) {
        for (int i = 0; i < 399; i++)
            g_panels[2].content.waveform_buffer[i] = g_panels[2].content.waveform_buffer[i + 1];
        float new_sample = sinf((float)g_total_time * 3.0f) * 0.5f
                         + sinf((float)g_total_time * 7.5f) * 0.25f
                         + sinf((float)g_total_time * 1.2f) * 0.15f
                         + (frand() - 0.5f) * 0.15f;
        if (new_sample < -1.0f) new_sample = -1.0f;
        if (new_sample > 1.0f) new_sample = 1.0f;
        g_panels[2].content.waveform_buffer[399] = new_sample;
    }
    
    // Draw waveform
    int mid_y = wave_y + wave_h / 2;
    float amp = (float)(wave_h / 2 - 4);
    
    // Draw pulse ring at front
    uint32_t wave_color = C_ACCENT_PURP;
    for (int i = 1; i < wave_w && i < 400; i++) {
        int idx = (400 - wave_w + i);
        if (idx < 0) idx = 0;
        if (idx >= 400) idx = 399;
        float val = g_panels[2].content.waveform_buffer[idx];
        int py1 = mid_y + (int)(val * amp);
        int prev_idx = idx - 1;
        if (prev_idx < 0) prev_idx = 0;
        float prev_val = g_panels[2].content.waveform_buffer[prev_idx];
        int py0 = mid_y + (int)(prev_val * amp);
        
        // Draw vertical line segment
        int y0 = py0 < py1 ? py0 : py1;
        int y1 = py0 < py1 ? py1 : py0;
        int sx = wave_x + i;
        
        for (int sy = y0; sy <= y1; sy++) {
            if (sx >= 0 && sx < fb_w && sy >= 0 && sy < fb_h)
                fb[sy * stride + sx] = ui_color_blend(wave_color, fb[sy * stride + sx]);
        }
    }
    
    // Channel labels
    draw_text_font(wctx, cx + 4, cy, "CH-7", C_ACCENT_PURP, "consola11");
    draw_text_font(wctx, cx + 65, cy, "+4.2dBm", C_TEXT_SECOND, "consola11");
    
    // Bottom stats
    int stats_y = wave_y + wave_h + 4;
    draw_text_font(wctx, cx + 4, stats_y, "FREQ: 1420.406 MHz", C_TEXT_DIM, "consola11");
    draw_text_font(wctx, cx + cw - 140, stats_y, "SNR: 34.7 dB", C_ACCENT_GREEN, "consola11");
}

// ── Panel 3: Command Console ──────────────────────────────────────────
static void draw_panel_console(KainUiWidgetContext* wctx, DashboardPanel* p) {
    int cx = (int)p->x + 12;
    int cy = (int)p->y + (int)g_title_bar_h + 10;
    int cw = (int)p->w - 24;
    int ch = (int)p->h - (int)g_title_bar_h - 18;
    
    uint32_t* fb = (uint32_t*)g_host->framebuffer;
    int stride = g_host->fb_stride / 4;
    int fb_w = g_host->width, fb_h = g_host->height;
    
    // Console background (darker, terminal-style)
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h,
                                 cx, cy, cw, ch, 0xCC080812, 4);
    
    // Border glow
    for (int b = 0; b < cw; b++) {
        if (cx + b >= 0 && cx + b < fb_w && cy + ch - 1 >= 0 && cy + ch - 1 < fb_h)
            fb[(cy + ch - 1) * stride + (cx + b)] = ui_color_blend(0x102970FF, fb[(cy + ch - 1) * stride + (cx + b)]);
    }
    
    // Add new log lines periodically
    if (!g_animation_paused && g_frame_count % 120 == 0) {
        PanelContent* pc = &g_panels[3].content;
        const char* msgs[] = {
            "[SYS] Telemetry heartbeat OK",
            "[SIG] Downlink signal strength: -72.4 dBm",
            "[NAV] Course correction burn: 0.02%",
            "[ENV] Solar wind flux: nominal",
            "[SYS] Memory page flush complete",
            "[GPS] Orbital position lock: 98.7%",
            "[CMD] Scheduled diagnostic: pending",
            "[SIG] Noise floor: -104.2 dBm",
            "[NAV] Star tracker calibration nominal",
            "[SYS] Clock synchronization delta: 0.3ms"
        };
        int msg_idx = rand() % 10;
        snprintf(pc->log_lines[pc->log_count % 16],
                 sizeof(pc->log_lines[0]), "%s", msgs[msg_idx]);
        pc->log_count++;
    }
    
    // Draw log lines (newest at bottom)
    int line_h = 14;
    int max_lines = ch / line_h;
    if (max_lines > 16) max_lines = 16;
    if (max_lines < 1) max_lines = 1;
    
    int log_start = cy + ch - max_lines * line_h;
    PanelContent* pc = &g_panels[3].content;
    
    for (int i = 0; i < max_lines && i < 16; i++) {
        int line_idx = (pc->log_count - max_lines + i);
        if (line_idx < 0) line_idx = 0;
        int buf_idx = line_idx % 16;
        
        char* line = pc->log_lines[buf_idx];
        if (line[0]) {
            // Color-code by prefix
            uint32_t text_color = C_TEXT_SECOND;
            if (strncmp(line, "[SIG]", 5) == 0)
                text_color = C_ACCENT_PURP;
            else if (strncmp(line, "[NAV]", 5) == 0)
                text_color = C_ACCENT_GREEN;
            else if (strncmp(line, "[SYS]", 5) == 0)
                text_color = C_ACCENT_CYAN;
            else if (strncmp(line, "[ENV]", 5) == 0)
                text_color = C_ACCENT_AMBER;
            else if (strncmp(line, "[CMD]", 5) == 0)
                text_color = C_ACCENT_ORANGE;
            
            draw_text_font(wctx, cx + 6, log_start + i * line_h, line, text_color, "consola11");
        }
    }
    
    // Prompt line at bottom
    draw_text_font(wctx, cx + 6, cy + ch - line_h - 2, "> _", C_ACCENT_GREEN, "consola14");
}

// ── Panel 4: Navigation (Compass) ─────────────────────────────────────
static void draw_panel_nav(KainUiWidgetContext* wctx, DashboardPanel* p) {
    int cx = (int)p->x + 12;
    int cy = (int)p->y + (int)g_title_bar_h + 10;
    int cw = (int)p->w - 24;
    int ch = (int)p->h - (int)g_title_bar_h - 18;
    
    uint32_t* fb = (uint32_t*)g_host->framebuffer;
    int stride = g_host->fb_stride / 4;
    int fb_w = g_host->width, fb_h = g_host->height;
    
    // Update compass angle
    if (!g_animation_paused)
        g_panels[4].content.compass_angle += 0.02;
    
    double angle = g_panels[4].content.compass_angle;
    int compass_cx = cx + cw / 2;
    int compass_cy = cy + 10 + (ch - 20) / 2;
    int compass_r = (cw < ch ? cw : ch) / 2 - 15;
    if (compass_r < 20) compass_r = 20;
    
    // Outer ring
    for (int r_outer = compass_r - 2; r_outer <= compass_r + 2; r_outer++) {
        for (int a = 0; a < 360; a += 2) {
            double rad = a * 3.14159 / 180.0;
            int rx = compass_cx + (int)(r_outer * cos(rad));
            int ry = compass_cy + (int)(r_outer * sin(rad));
            if (rx >= 0 && rx < fb_w && ry >= 0 && ry < fb_h)
                fb[ry * stride + rx] = ui_color_blend(0x3080C0FF, fb[ry * stride + rx]);
        }
    }
    
    // Tick marks (every 30 degrees)
    for (int a = 0; a < 360; a += 30) {
        double rad = a * 3.14159 / 180.0;
        int tick_len = (a % 90 == 0) ? 8 : 4;
        int r1 = compass_r - 5;
        int r2 = compass_r - 5 - tick_len;
        int mark_bright = (a % 90 == 0) ? 80 : 40;
        uint32_t mark_col = (mark_bright << 24) | 0x0088DDFF;
        
        for (int t = 0; t < 3; t++) {
            int r = r1 - t * (r1 - r2) / 3;
            int mx = compass_cx + (int)(r * cos(rad));
            int my = compass_cy + (int)(r * sin(rad));
            if (mx >= 0 && mx < fb_w && my >= 0 && my < fb_h)
                fb[my * stride + mx] = ui_color_blend(mark_col, fb[my * stride + mx]);
        }
    }
    
    // Direction labels
    draw_text_font(wctx, compass_cx - 5, compass_cy - compass_r - 18, "N", C_ACCENT_PINK, "impact18");
    draw_text_font(wctx, compass_cx + compass_r + 4, compass_cy - 7, "E", C_TEXT_DIM, "impact14");
    draw_text_font(wctx, compass_cx - 7, compass_cy + compass_r + 2, "S", C_TEXT_DIM, "impact14");
    draw_text_font(wctx, compass_cx - compass_r - 16, compass_cy - 7, "W", C_TEXT_DIM, "impact14");
    
    // Animated needle
    double needle_angle = angle * 0.5; // Slow rotation
    // Primary needle (north)
    int nx = compass_cx + (int)(compass_r * 0.7 * sin(needle_angle));
    int ny = compass_cy - (int)(compass_r * 0.7 * cos(needle_angle));
    
    // Draw needle line
    int steps = compass_r;
    for (int s = 0; s <= steps; s++) {
        int lx = compass_cx + (int)((nx - compass_cx) * s / steps);
        int ly = compass_cy + (int)((ny - compass_cy) * s / steps);
        if (lx >= 0 && lx < fb_w && ly >= 0 && ly < fb_h)
            fb[ly * stride + lx] = ui_color_blend(0xC0FF4080, fb[ly * stride + lx]);
    }
    
    // Secondary needle (south, opposite direction)
    int sx = compass_cx - (int)(compass_r * 0.4 * sin(needle_angle));
    int sy = compass_cy + (int)(compass_r * 0.4 * cos(needle_angle));
    for (int s = 0; s <= (int)(compass_r * 0.4); s++) {
        int lx = compass_cx + (int)((sx - compass_cx) * s / (int)(compass_r * 0.4));
        int ly = compass_cy + (int)((sy - compass_cy) * s / (int)(compass_r * 0.4));
        if (lx >= 0 && lx < fb_w && ly >= 0 && ly < fb_h)
            fb[ly * stride + lx] = ui_color_blend(0x602979FF, fb[ly * stride + lx]);
    }
    
    // Center dot
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h,
                                 compass_cx - 3, compass_cy - 3, 6, 6,
                                 C_ACCENT_CYAN, 3);
    
    // Heading text
    char heading[24];
    double deg = fmod(angle * 180.0 / 3.14159, 360.0);
    if (deg < 0) deg += 360.0;
    snprintf(heading, sizeof(heading), "HDG: %.0f°", deg);
    draw_text_font(wctx, cx + 4, cy + ch - 16, heading, C_TEXT_SECOND, "consola11");
    
    // GPS status
    draw_text_font(wctx, cx + cw - 80, cy + ch - 16, "GPS LOCKED", C_ACCENT_GREEN, "consola11");
}

// ── Panel 5: Particle Flux (Rotating Ring) ────────────────────────────
static void draw_particle_flux(KainUiWidgetContext* wctx, DashboardPanel* p) {
    int cx = (int)p->x + 12;
    int cy = (int)p->y + (int)g_title_bar_h + 10;
    int cw = (int)p->w - 24;
    int ch = (int)p->h - (int)g_title_bar_h - 18;
    
    uint32_t* fb = (uint32_t*)g_host->framebuffer;
    int stride = g_host->fb_stride / 4;
    int fb_w = g_host->width, fb_h = g_host->height;
    
    if (!g_animation_paused)
        g_panels[5].content.ring_angle += 0.02;
    
    double ring_angle = g_panels[5].content.ring_angle;
    int ring_cx = cx + cw / 2;
    int ring_cy = cy + 10 + (ch - 20) / 2;
    int ring_r = (cw < ch ? cw : ch) / 2 - 15;
    if (ring_r < 20) ring_r = 20;
    
    // Draw ring path (faint ellipse)
    for (int a = 0; a < 360; a += 1) {
        double rad = a * 3.14159 / 180.0;
        // Elliptical perspective
        double ex = ring_cx + ring_r * cos(rad);
        double ey = ring_cy + ring_r * 0.4 * sin(rad);
        // Scale factor for "3D" look - front is brighter
        float z_depth = cos(rad) * 0.5f + 0.5f;
        int alpha = (int)(15 + z_depth * 25);
        uint32_t col = (alpha << 24) | 0x0088DDFF;
        // Draw small dot at each ring position
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int px = (int)ex + dx, py = (int)ey + dy;
                if (px >= 0 && px < fb_w && py >= 0 && py < fb_h)
                    fb[py * stride + px] = ui_color_blend(col, fb[py * stride + px]);
            }
    }
    
    // Draw particles orbiting the ring
    int num_orbiting = 12;
    for (int i = 0; i < num_orbiting; i++) {
        double particle_angle = ring_angle + i * 6.2832 / num_orbiting;
        double rad = particle_angle;
        double ex = ring_cx + ring_r * cos(rad);
        double ey = ring_cy + ring_r * 0.4 * sin(rad);
        
        float z_depth = cos(rad) * 0.5f + 0.5f;
        int psize = (int)(1 + z_depth * 3);
        uint32_t pcolor;
        if (i % 3 == 0) pcolor = C_ACCENT_CYAN;
        else if (i % 3 == 1) pcolor = C_ACCENT_PURP;
        else pcolor = C_ACCENT_PINK;
        
        // Apply alpha based on depth
        int palpha = (int)(100 + z_depth * 155);
        pcolor = (palpha << 24) | (pcolor & 0x00FFFFFF);
        
        ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h,
                                     (int)ex - psize/2, (int)ey - psize/2,
                                     psize, psize, pcolor, psize/2 + 1);
    }
    
    // Count text
    char count_str[32];
    snprintf(count_str, sizeof(count_str), "FLUX: %d P/s", g_particle_count * 3);
    draw_text_font(wctx, cx + 4, cy + ch - 16, count_str, C_TEXT_SECOND, "consola11");
    
    // Speed control label
    draw_text_font(wctx, cx + cw - 120, cy + ch - 16, "SPEED", C_TEXT_DIM, "consola11");
}

// ============================================================================
//   SCANLINE EFFECT
// ============================================================================

static void update_scanline(double dt) {
    if (g_animation_paused) return;
    
    g_scanline_timer += dt;
    if (g_scanline_timer >= 3.0 && !g_scanline_active) {
        g_scanline_active = 1;
        g_scanline_y = -50.0;
        g_scanline_timer = 0.0;
    }
    
    if (g_scanline_active) {
        g_scanline_y += dt * 300.0; // 300 px/s sweep
        if (g_scanline_y > g_win_h + 50) {
            g_scanline_active = 0;
            g_scanline_y = -100.0;
        }
    }
}

static void draw_scanline(uint32_t* fb, int stride, int fb_w, int fb_h) {
    if (!g_scanline_active) return;
    
    int sy = (int)g_scanline_y;
    int scan_h = 3;
    
    for (int row = sy; row < sy + scan_h && row < fb_h; row++) {
        if (row < 0) continue;
        // Scanline fades at edges
        float fade = 1.0f;
        if (row < sy + 1) fade = 0.3f;
        if (row > sy + scan_h - 2) fade = 0.3f;
        int alpha = (int)(fade * 150);
        uint32_t scan_color = (alpha << 24) | 0x00FFFFFF;
        
        for (int col = 0; col < fb_w && col < fb_w; col++) {
            if (col >= 0)
                fb[row * stride + col] = ui_color_blend(scan_color, fb[row * stride + col]);
        }
    }
}

// ============================================================================
//   MAIN RENDER FUNCTION
// ============================================================================

static void render_frame(KainUiWidgetContext* wctx, double dt) {
    if (!g_host || !g_host->framebuffer) return;
    
    uint32_t* fb = (uint32_t*)g_host->framebuffer;
    int stride = g_host->fb_stride / 4;
    int fb_w = g_host->width;
    int fb_h = g_host->height;
    
    // ── 1. Clear to deep space ──────────────────────────────────────
    for (int y = 0; y < fb_h; y++) {
        for (int x = 0; x < fb_w; x++) {
            fb[y * stride + x] = C_SPACE_0;
        }
    }
    
    // ── 2. Draw nebula gradient (behind particles) ──────────────────
    draw_nebula(fb, stride, fb_w, fb_h, g_total_time);
    
    // ── 3. Draw particle starfield ──────────────────────────────────
    update_particles(dt);
    draw_particles(fb, stride, fb_w, fb_h);
    
    // ── 4. Draw header bar ─────────────────────────────────────────
    // Dark header with accent underline
    for (int y = 0; y < g_header_h && y < fb_h; y++) {
        float hf = (float)y / (float)g_header_h;
        uint32_t hcol;
        if (y < 2) {
            hcol = 0xFF1A1A30;
        } else {
            int h_alpha = (int)(40 + (1.0f - hf) * 30);
            hcol = (h_alpha << 24) | 0x00080A20;
        }
        for (int x = 0; x < fb_w; x++) {
            fb[y * stride + x] = ui_color_blend(hcol, fb[y * stride + x]);
        }
    }
    
    // Header accent line
    for (int x = 0; x < fb_w; x++) {
        float hx = (float)x / (float)fb_w;
        uint32_t accent = 0;
        if (hx < 0.33f) accent = C_ACCENT_CYAN;
        else if (hx < 0.66f) accent = C_ACCENT_PURP;
        else accent = C_ACCENT_PINK;
        if (g_header_h - 2 >= 0 && g_header_h - 2 < fb_h && x >= 0 && x < fb_w)
            fb[(g_header_h - 2) * stride + x] = accent;
    }
    
    // Header title
    draw_text_font(wctx, 16, 10, "COSMIC DASHBOARD  |  MISSION CONTROL", C_TEXT_PRIMARY, "impact18");
    
    // Header subtitle
    // Heartbeat indicator — pulses with animation state
    int hb_size = g_animation_paused ? 4 : 6;
    double hb_pulse = 0.3 + 0.7 * (0.5 + 0.5 * sin(g_total_time * 6.0));
    uint32_t hb_color = g_animation_paused ? 0xFF505050 : 
        ((uint32_t)(int)(hb_pulse * 255) << 24) | 0x0000FF88;
    ui_widget_fill_rounded_rect(fb, stride, fb_w, fb_h,
                                 6, 4, hb_size, hb_size, hb_color, 3);
    
    draw_text_font(wctx, 16, 28, "KAIN NATIVE UI  ·  REAL-TIME TELEMETRY", C_TEXT_DIM, "consola11");
    
    // FPS counter top-right
    char fps_str[32];
    snprintf(fps_str, sizeof(fps_str), "FPS: %.0f", g_fps);
    int fps_w = text_width_font(wctx, fps_str, "consola14");
    draw_text_font(wctx, fb_w - fps_w - 16, 10, fps_str, C_FPS_GREEN, "consola14");
    
    char frame_str[32];
    snprintf(frame_str, sizeof(frame_str), "FRM: %lld", (long long)g_frame_count);
    int frame_w = text_width_font(wctx, frame_str, "consola11");
    draw_text_font(wctx, fb_w - frame_w - 16, 28, frame_str, C_TEXT_DIM, "consola11");
    
    // Nebula toggle button
    char neb_str[16];
    snprintf(neb_str, sizeof(neb_str), "NEBULA: %s", g_nebula_enabled ? "ON" : "OFF");
    uint32_t neb_color = g_nebula_enabled ? C_ACCENT_GREEN : C_TEXT_DIM;
    int nx = fb_w - 190;
    draw_text_font(wctx, nx, 28, neb_str, neb_color, "consola11");
    
    // ── 5. Update and draw panels (sorted by z-order) ───────────────
    // Sort panels by z_order
    int sorted[MAX_PANELS];
    for (int i = 0; i < MAX_PANELS; i++) sorted[i] = i;
    // Simple bubble sort by z_order (ascending)
    for (int i = 0; i < MAX_PANELS; i++) {
        for (int j = i + 1; j < MAX_PANELS; j++) {
            if (g_panels[sorted[i]].z_order > g_panels[sorted[j]].z_order) {
                int tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }
    
    // Update panel animations
    for (int i = 0; i < MAX_PANELS; i++) {
        if (!g_animation_paused)
            g_panels[i].anim_time += dt * 0.5;
    }
    
    // Draw panels in z-order
    for (int si = 0; si < MAX_PANELS; si++) {
        int pi = sorted[si];
        if (!g_panels[pi].visible) continue;
        
        DashboardPanel* panel = &g_panels[pi];
        
        // Draw glass background
        draw_panel_bg(fb, stride, fb_w, fb_h, panel);
        
        // Draw title bar
        draw_panel_title(wctx, (int)panel->x, (int)panel->y, (int)panel->w,
                         panel->title, panel->accent_color, panel->hovered_close);
        
        // Draw panel content
        switch (pi) {
            case 0: draw_panel_system_status(wctx, panel); break;
            case 1: draw_panel_stellar(wctx, panel); break;
            case 2: draw_panel_signal(wctx, panel); break;
            case 3: draw_panel_console(wctx, panel); break;
            case 4: draw_panel_nav(wctx, panel); break;
            case 5: draw_particle_flux(wctx, panel); break;
        }
    }
    
    // ── 6. Scanline effect ───────────────────────────────────────
    update_scanline(dt);
    draw_scanline(fb, stride, fb_w, fb_h);
    
    // ── 7. Controls hint at bottom ──────────────────────────────
    if (g_frame_count < 300) {
        char hint[128];
        snprintf(hint, sizeof(hint),
                 "SPACE=pause  |  1-6=toggle panels  |  ESC=exit  |  Click title bars to drag panels");
        int hint_w = text_width_font(wctx, hint, "consola11");
        draw_text_font(wctx, (fb_w - hint_w) / 2, fb_h - 22, hint, 0x60404060, "consola11");
    }
    
    // ── Update neon panel layout (if window resized) ────────────
    static int last_w = 0, last_h = 0;
    if (last_w != g_host->width || last_h != g_host->height) {
        g_win_w = g_host->width;
        g_win_h = g_host->height;
        layout_panels();
        last_w = g_host->width;
        last_h = g_host->height;
    }
}

// ============================================================================
//   EVENT HANDLING
// ============================================================================

static int point_in_rect(double px, double py, double rx, double ry, double rw, double rh) {
    return (px >= rx && px < rx + rw && py >= ry && py < ry + rh);
}

static void handle_mouse_down(double mx, double my) {
    // Check if click is on a panel's close button (top-right X)
    for (int i = MAX_PANELS - 1; i >= 0; i--) { // reverse order = topmost first
        if (!g_panels[i].visible) continue;
        int close_x = (int)g_panels[i].x + (int)g_panels[i].w - 24;
        int close_y = (int)g_panels[i].y + 4;
        if (point_in_rect(mx, my, (double)close_x, (double)close_y, 18, 18)) {
            g_panels[i].visible = 0;
            return;
        }
    }
    
    // Check if click is on a panel's title bar (start drag, raise to top)
    for (int i = MAX_PANELS - 1; i >= 0; i--) {
        if (!g_panels[i].visible) continue;
        DashboardPanel* p = &g_panels[i];
        
        // Title bar click region
        if (point_in_rect(mx, my, p->x, p->y, p->w, (double)g_title_bar_h)) {
            // Raise to top: give this panel the highest z_order
            int max_z = 0;
            for (int j = 0; j < MAX_PANELS; j++)
                if (g_panels[j].z_order > max_z) max_z = g_panels[j].z_order;
            p->z_order = max_z + 1;
            
            // Start dragging
            p->dragging = 1;
            p->drag_offset_x = mx - p->x;
            p->drag_offset_y = my - p->y;
            g_focused_panel = i;
            return;
        }
    }
}

static void handle_mouse_move(double mx, double my) {
    // Handle active drag
    if (g_focused_panel >= 0 && g_focused_panel < MAX_PANELS) {
        DashboardPanel* p = &g_panels[g_focused_panel];
        if (p->dragging) {
            double new_x = mx - p->drag_offset_x;
            double new_y = my - p->drag_offset_y;
            
            // Clamp to screen bounds
            if (new_x < -p->w + 50) new_x = -p->w + 50;
            if (new_y < -20) new_y = -20;
            if (new_x > g_host->width - 50) new_x = g_host->width - 50;
            if (new_y > g_host->height - 30) new_y = g_host->height - 30;
            
            p->x = new_x;
            p->y = new_y;
        }
    }
    
    // Update hover states on close buttons
    for (int i = 0; i < MAX_PANELS; i++) {
        if (!g_panels[i].visible) continue;
        int close_x = (int)g_panels[i].x + (int)g_panels[i].w - 24;
        int close_y = (int)g_panels[i].y + 4;
        g_panels[i].hovered_close = point_in_rect(mx, my, (double)close_x, (double)close_y, 18, 18);
    }
    
    // Nebula toggle button hover
    // (handled in nebula toggle click)
}

static void handle_mouse_up(double mx, double my) {
    // Stop dragging all panels
    for (int i = 0; i < MAX_PANELS; i++) {
        g_panels[i].dragging = 0;
    }
    g_focused_panel = -1;
    
    // Check nebula toggle click
    int nx = g_host->width - 190;
    if (point_in_rect(mx, my, (double)nx, 28.0, 160.0, 14.0)) {
        g_nebula_enabled = !g_nebula_enabled;
    }
}

static void handle_keyboard(int vk) {
    switch (vk) {
        case VK_ESCAPE:
            g_host->running = 0;
            break;
        case VK_SPACE:
            g_animation_paused = !g_animation_paused;
            break;
        case '1': g_panels[0].visible = !g_panels[0].visible; break;
        case '2': g_panels[1].visible = !g_panels[1].visible; break;
        case '3': g_panels[2].visible = !g_panels[2].visible; break;
        case '4': g_panels[3].visible = !g_panels[3].visible; break;
        case '5': g_panels[4].visible = !g_panels[4].visible; break;
        case '6': g_panels[5].visible = !g_panels[5].visible; break;
    }
}

// ============================================================================
//   WIN32 WINDOW PROCEDURE
// ============================================================================

static WNDPROC g_orig_wndproc = NULL;

static LRESULT CALLBACK cosmic_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
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
        case WM_LBUTTONDOWN: {
            int mx = (int)(short)LOWORD(lp);
            int my = (int)(short)HIWORD(lp);
            handle_mouse_down((double)mx, (double)my);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: {
            int mx = (int)(short)LOWORD(lp);
            int my = (int)(short)HIWORD(lp);
            handle_mouse_up((double)mx, (double)my);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE: {
            int mx = (int)(short)LOWORD(lp);
            int my = (int)(short)HIWORD(lp);
            handle_mouse_move((double)mx, (double)my);
            return 0;
        }
        case WM_KEYDOWN: {
            handle_keyboard((int)wp);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_DPICHANGED: {
            RECT* rect = (RECT*)lp;
            SetWindowPos(hwnd, NULL, rect->left, rect->top,
                         rect->right - rect->left,
                         rect->bottom - rect->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_SIZE: {
            if (g_host && wp != SIZE_MINIMIZED) {
                g_win_w = LOWORD(lp);
                g_win_h = HIWORD(lp);
                layout_panels();
                // Note: framebuffer still at old size — causes drift.
                // Full fix needs DIB recreation in host adapter.
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
    }
    return CallWindowProcA(g_orig_wndproc, hwnd, msg, wp, lp);
}

// ============================================================================
//   MAIN
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
    g_dpi = dpi_scale;
    printf("[DPI] Scale: %.2f (display DPI: %ld\n", dpi_scale,
           (long)(dpi_scale * 96.0f + 0.5f));

    // Scale window dimensions
    g_win_w = (int)(1280 * dpi_scale + 0.5f);
    g_win_h = (int)(720 * dpi_scale + 0.5f);
    printf("[DPI] Window: %dx%d (logical 1280x720)\n", g_win_w, g_win_h);

    printf("=== COSMIC DASHBOARD — Kain Native UI  ===\n");
    printf("Build: " __DATE__ " " __TIME__ "\n\n");
    
    srand((unsigned int)time(NULL));
    
    // ── Create UI session ───────────────────────────────────────────
    g_session_id = abi_ui_session_create("CosmicDashboard", g_win_w, g_win_h);
    if (g_session_id <= 0) {
        fprintf(stderr, "FAILED: abi_ui_session_create returned %lld\n", (long long)g_session_id);
        return 1;
    }
    printf("[UI] Session created: %lld\n", (long long)g_session_id);
    
    // Open window
    int64_t win = abi_ui_window_open(g_session_id, "COSMIC DASHBOARD — Mission Control", g_win_w, g_win_h);
    printf("[UI] Window opened: %lld\n", (long long)win);
    
    // Attach winit backend (Win32 GDI)
    int64_t attach = abi_ui_host_attach(g_session_id, "winit");
    printf("[UI] Host attached: %lld\n", (long long)attach);
    
    // Get host
    KainNativeUiSession* s = abi_ui_find_session(g_session_id);
    if (!s || !s->host_state) {
        fprintf(stderr, "FAILED: No host state\n");
        return 1;
    }
    g_host = (KainWin32UiHost*)s->host_state;
    printf("[UI] Host: %dx%d  stride=%d  fb=%p  hwnd=%p\n",
           g_host->width, g_host->height, g_host->fb_stride,
           (void*)g_host->framebuffer, (void*)g_host->hwnd);
    
    g_win_w = g_host->width;
    g_win_h = g_host->height;
    printf("[UI] Actual framebuffer: %dx%d\n", g_win_w, g_win_h);
    
    // Subclass window proc
    g_orig_wndproc = (WNDPROC)SetWindowLongPtrA(g_host->hwnd, GWLP_WNDPROC,
                                                  (LONG_PTR)cosmic_wndproc);
    
    // Store host in window user data
    SetWindowLongPtrA(g_host->hwnd, GWLP_USERDATA, (LONG_PTR)g_host);
    
    // Set the window title (the host adapter hardcodes "Kain UI")
    SetWindowTextA(g_host->hwnd, "COSMIC DASHBOARD — Mission Control");
    
    // ── Create widget context ───────────────────────────────────────
    KainUiWidgetContext* wctx = ui_widget_create(g_session_id);
    if (!wctx) {
        fprintf(stderr, "FAILED: ui_widget_create\n");
        return 1;
    }
    printf("[WIDGET] Context created\n");
    
    // ── Load all fonts ──────────────────────────────────────────────
    load_all_fonts(wctx);
    printf("[FONTS] Font loading complete\n");
    
    // ── Initialize particles ────────────────────────────────────────
    init_particles();
    printf("[PARTICLES] %d particles initialized\n", g_particle_count);
    
    // ── Initialize panels ───────────────────────────────────────────
    init_panels();
    layout_panels();
    printf("[PANELS] %d dashboard panels initialized\n", MAX_PANELS);
    
    // ── Frame loop ──────────────────────────────────────────────────
    LARGE_INTEGER freq, last_time;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last_time);
    
    printf("\n=== COSMIC DASHBOARD RUNNING ===\n");
    printf("  Space = toggle animation pause\n");
    printf("  1-6   = toggle panel visibility\n");
    printf("  Esc   = exit\n");
    printf("================================\n\n");
    
    MSG msg;
    int64_t frame = 0;
    
    while (1) {
        // ── Delta time ──────────────────────────────────────────
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double dt = (double)(now.QuadPart - last_time.QuadPart) / (double)freq.QuadPart;
        last_time = now;
        if (dt > 0.1) dt = 0.016; // Cap at 100ms (first frame after lag)
        
        g_total_time += dt;
        
        // ── FPS tracking ────────────────────────────────────────
        g_frame_count++;
        g_fps_timer += dt;
        if (g_fps_timer >= 1.0) {
            g_fps = (double)g_frame_count / g_fps_timer;
            g_frame_count = 0;
            g_fps_timer = 0.0;
        }
        
        // ── Pump messages ───────────────────────────────────────
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_host->running = 0;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!g_host || !g_host->running) break;
        
        // ── UI frame lifecycle ──────────────────────────────────
        abi_ui_begin_frame(g_session_id, dt * 1000.0);
        abi_ui_end_frame(g_session_id);
        
        // ── Render cosmic dashboard ─────────────────────────────
        render_frame(wctx, dt);
        
        // ── Present to screen (UpdateWindow = synchronous paint) ──
        InvalidateRect(g_host->hwnd, NULL, FALSE);
        UpdateWindow(g_host->hwnd);
        
        // ── Throttle to ~60fps ─────────────────────────────────
        Sleep(16);
        frame++;
        
        // Periodic status
        if (frame % 300 == 0) {
            printf("[FRAME %lld] FPS: %.0f | Particles: %d | Panels: %d/%d\n",
                   (long long)frame, g_fps, g_particle_count,
                   g_panels[0].visible + g_panels[1].visible + g_panels[2].visible +
                   g_panels[3].visible + g_panels[4].visible + g_panels[5].visible,
                   MAX_PANELS);
        }
    }
    
    // ── Cleanup ─────────────────────────────────────────────────────
    printf("\n=== SHUTDOWN ===\n");
    printf("Total frames: %lld\n", (long long)frame);
    
    ui_widget_destroy(wctx);
    abi_ui_session_destroy(g_session_id);
    
    printf("Session destroyed. Goodbye, Commander.\n");
    return 0;
}
