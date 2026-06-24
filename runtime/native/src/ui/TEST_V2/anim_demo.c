// ============================================================================
//  anim_demo.c — Animated Particle System Demo
//  ============================================================================
//  Demonstrates:
//    - 100-particle system with velocity-based animation
//    - Bouncing off window edges
//    - Color cycling (hue shift over time)
//    - Fade-in/out effects using opacity
//    - Frame-by-frame position updates
//    - Real-time performance metrics
//  ============================================================================
//  Build:
//    clang -std=c11 -g -O0 anim_demo.c ../TEST/stubs.c ^
//      ../ui_system.c ../ui_host_adapter.c ../ui_renderer.c ../ui_layout.c ../ui_color.c ^
//      ../../core/input_system.c ^
//      -I../../../include -I.. -I../../core ^
//      -luser32 -lgdi32 -lopengl32 -o anim_demo.exe
//  ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ui_system.h"
#include "ui_system_internal.h"
#include "ui_host_adapter.h"
#include "../../include/ui_renderer.h"
#include "../../include/ui_layout.h"
#include "../../include/ui_color.h"

// ── Stubs ──────────────────────────────────────────────────────────────
char* string_new(char* src);
double kain_clampd(double value, double min_value, double max_value);

// ── KainWin32UiHost ────────────────────────────────────────────────────
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

// ── Particle system ────────────────────────────────────────────────────
#define MAX_PARTICLES 100

typedef struct {
    double x, y;
    double vx, vy;
    double hue;        // 0-360
    double size;
    double life;       // 0.0 - 1.0 (1.0 = full opacity)
    double speed;      // base speed multiplier
} Particle;

static Particle g_particles[MAX_PARTICLES];
static int g_particle_count = 0;
static double g_time = 0.0;

// HSV to RGB conversion
static void hsv_to_rgb(double h, double s, double v, int* r, int* g, int* b) {
    if (s < 0.001) { *r = *g = *b = (int)(v * 255); return; }
    h = fmod(h, 360.0); if (h < 0) h += 360.0;
    int hi = (int)(h / 60.0) % 6;
    double f = h / 60.0 - hi;
    double p = v * (1.0 - s);
    double q = v * (1.0 - f * s);
    double t = v * (1.0 - (1.0 - f) * s);
    switch (hi) {
        case 0: *r=(int)(v*255); *g=(int)(t*255); *b=(int)(p*255); break;
        case 1: *r=(int)(q*255); *g=(int)(v*255); *b=(int)(p*255); break;
        case 2: *r=(int)(p*255); *g=(int)(v*255); *b=(int)(t*255); break;
        case 3: *r=(int)(p*255); *g=(int)(q*255); *b=(int)(v*255); break;
        case 4: *r=(int)(t*255); *g=(int)(p*255); *b=(int)(v*255); break;
        case 5: *r=(int)(v*255); *g=(int)(p*255); *b=(int)(q*255); break;
    }
}

static void particle_init(Particle* p, int w, int h) {
    p->x = (double)(rand() % w);
    p->y = (double)(rand() % h);
    double angle = (double)(rand() % 360) * 3.14159 / 180.0;
    double speed = 1.0 + (double)(rand() % 100) / 50.0;
    p->vx = cos(angle) * speed;
    p->vy = sin(angle) * speed;
    p->hue = (double)(rand() % 360);
    p->size = 4.0 + (double)(rand() % 12);
    p->life = 0.5 + (double)(rand() % 50) / 100.0;
    p->speed = speed;
}

static void particle_system_init(int w, int h) {
    srand(42);
    g_particle_count = MAX_PARTICLES;
    for (int i = 0; i < g_particle_count; i++) {
        particle_init(&g_particles[i], w, h);
    }
}

static void particle_system_update(int w, int h, double dt) {
    g_time += dt;

    for (int i = 0; i < g_particle_count; i++) {
        Particle* p = &g_particles[i];

        // Update position
        p->x += p->vx * dt * 60.0;
        p->y += p->vy * dt * 60.0;

        // Gentle gravity and wind
        p->vy += 0.05 * dt * 60.0;
        p->vx += sin(g_time * 0.001 + i) * 0.02 * dt * 60.0;

        // Damping
        p->vx *= 0.998;
        p->vy *= 0.998;

        // Bounce off edges with energy loss
        if (p->x < 0) { p->x = 0; p->vx = -p->vx * 0.9; }
        if (p->x >= w) { p->x = (double)(w - 1); p->vx = -p->vx * 0.9; }
        if (p->y < 0) { p->y = 0; p->vy = -p->vy * 0.9; }
        if (p->y >= h) {
            p->y = (double)(h - 1);
            p->vy = -p->vy * 0.85;
            // Friction on ground
            p->vx *= 0.95;
        }

        // Color cycling: shift hue over time
        p->hue += 0.5 * dt * 60.0;
        if (p->hue >= 360.0) p->hue -= 360.0;

        // Pulse size
        p->size = 5.0 + 8.0 * (0.5 + 0.5 * sin(g_time * 0.003 + (double)i * 0.1));

        // Life fade - particles at the bottom fade
        double norm_y = p->y / (double)h;
        p->life = 0.6 + 0.4 * (1.0 - norm_y * norm_y);

        // If particle escapes or gets stuck, respawn
        if (fabs(p->vx) < 0.01 && fabs(p->vy) < 0.01) {
            particle_init(p, w, h);
        }
    }
}

static void particle_system_render(uint32_t* fb, int w, int h, int stride) {
    for (int i = 0; i < g_particle_count; i++) {
        Particle* p = &g_particles[i];
        int px = (int)p->x;
        int py = (int)p->y;
        int sz = (int)p->size;
        if (sz < 2) sz = 2;

        // Color from hue (full saturation, varying value)
        int r, g, b;
        double brightness = 0.6 + 0.4 * p->life;
        hsv_to_rgb(p->hue, 0.9, brightness, &r, &g, &b);
        uint32_t color = ((uint32_t)(int)(p->life * 255.0) << 24) |
                         ((uint32_t)(b & 0xFF) << 16) |
                         ((uint32_t)(g & 0xFF) << 8) |
                         (uint32_t)(r & 0xFF);

        // Draw particle as a filled circle (simple approximation)
        int half = sz / 2;
        int cx = px - half;
        int cy = py - half;
        int r2 = half * half;

        for (int row = cy; row < cy + sz && row < h; row++) {
            if (row < 0) continue;
            for (int col = cx; col < cx + sz && col < w; col++) {
                if (col < 0) continue;
                int dx = col - px;
                int dy = row - py;
                if (dx * dx + dy * dy <= r2) {
                    // Alpha blend
                    uint32_t bg = fb[row * stride + col];
                    uint32_t src_alpha = (color >> 24) & 0xFF;
                    if (src_alpha == 255) {
                        fb[row * stride + col] = color;
                    } else if (src_alpha > 0) {
                        uint32_t dst_alpha = 255 - src_alpha;
                        uint8_t or = ((color >> 16) & 0xFF) * src_alpha / 255 + ((bg >> 16) & 0xFF) * dst_alpha / 255;
                        uint8_t og = ((color >> 8) & 0xFF) * src_alpha / 255 + ((bg >> 8) & 0xFF) * dst_alpha / 255;
                        uint8_t ob = (color & 0xFF) * src_alpha / 255 + (bg & 0xFF) * dst_alpha / 255;
                        fb[row * stride + col] = 0xFF000000 | ((uint32_t)or << 16) | ((uint32_t)og << 8) | ob;
                    }
                }
            }
        }
    }
}

// ── Pixel helpers for UI chrome ────────────────────────────────────────
static void fill_rect(uint32_t* fb, int stride, int x, int y, int w, int h, uint32_t color) {
    for (int r = y; r < y + h && r < 2000; r++)
        for (int c = x; c < x + w && c < 2000; c++)
            if (r >= 0 && c >= 0) fb[r * stride + c] = color;
}

// ── Paint UI overlay ───────────────────────────────────────────────────
static void paint_overlay(uint32_t* fb, int w, int h, int stride, HDC gdi_dc, int64_t frame) {
    // Header bar
    fill_rect(fb, stride, 0, 0, w, 44, 0xFF1E1E32);
    fill_rect(fb, stride, 0, 42, w, 2, 0xFF21D4A1);

    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0xE8, 0xE8, 0xF0));
        SetBkMode(gdi_dc, TRANSPARENT);
        SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
        TextOutA(gdi_dc, 14, 6, "Particle System Demo", 20);
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));

        char info[128];
        snprintf(info, sizeof(info), "%d particles  |  Frame %lld  |  Kain Native UI  |  Esc to exit",
                 g_particle_count, (long long)frame);
        TextOutA(gdi_dc, 14, 24, info, (int)strlen(info));
    }

    // Status bar
    fill_rect(fb, stride, 0, h - 24, w, 24, 0xFF1E1E32);
    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        TextOutA(gdi_dc, 10, h - 18, "Particles bounce, pulse, and cycle colors  |  Close window to exit", 68);
    }
}

// ── Window subclass ────────────────────────────────────────────────────
static WNDPROC g_orig_wndproc = NULL;

static LRESULT CALLBACK anim_window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (hdc) {
            KainWin32UiHost* host = (KainWin32UiHost*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
            if (host && host->hdc_buffer) {
                BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
                       ps.rcPaint.right - ps.rcPaint.left,
                       ps.rcPaint.bottom - ps.rcPaint.top,
                       host->hdc_buffer, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        PostQuitMessage(0);
        return 0;
    }
    return CallWindowProcA(g_orig_wndproc, hwnd, msg, wp, lp);
}

// ── Main ───────────────────────────────────────────────────────────────
int main(void) {
    int win_w = 960, win_h = 600;

    printf("=== Particle System Demo — Kain Native UI ===\n");
    printf("Build: " __DATE__ " " __TIME__ "\n\n");

    // Init Kain session
    abi_ui_reset();
    int64_t session = abi_ui_session_create("AnimDemo", win_w, win_h);
    if (session <= 0) { fprintf(stderr, "FAIL: session_create\n"); return 1; }

    abi_ui_window_open(session, "Particle System Demo — Kain Native UI", win_w, win_h);
    if (abi_ui_host_attach(session, "winit") != 0) {
        fprintf(stderr, "FAIL: host_attach\n"); return 1;
    }

    KainNativeUiSession* ks = abi_ui_find_session(session);
    if (!ks || !ks->host_state) { fprintf(stderr, "FAIL: no host state\n"); return 1; }
    KainWin32UiHost* host = (KainWin32UiHost*)ks->host_state;

    // Subclass window
    g_orig_wndproc = (WNDPROC)SetWindowLongPtrA(host->hwnd, GWLP_WNDPROC,
                                                  (LONG_PTR)anim_window_proc);

    printf("Window: %dx%d  hwnd=%p  fb=%p\n",
           host->width, host->height, (void*)host->hwnd, (void*)host->framebuffer);

    // Init particle system at actual window size
    particle_system_init(host->width, host->height);

    // Build minimal Kain node tree
    int64_t root = abi_ui_node_create(session, "root");
    abi_ui_node_set_rect(session, root, 0, 0, win_w, win_h);

    int64_t bg = abi_ui_node_create(session, "bg");
    abi_ui_node_set_parent(session, bg, root);
    abi_ui_node_set_rect(session, bg, 0, 0, win_w, win_h);
    abi_ui_node_set_style_string(session, bg, "fill_color", "#1A1A24");

    printf("\nFrame loop running. %d particles animating.\n", MAX_PARTICLES);
    printf("========================================================\n");

    int64_t frame = 0;
    MSG msg;

    while (1) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { host->running = 0; break; }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!host->running) break;

        abi_ui_begin_frame(session, 16.67);
        abi_ui_end_frame(session);

        // Update particles (60fps physics step)
        particle_system_update(host->width, host->height, 1.0);

        // Render
        if (host->framebuffer) {
            // Paint particles
            particle_system_render((uint32_t*)host->framebuffer,
                                   host->width, host->height, host->fb_stride / 4);

            // Paint UI overlay (header, status bar)
            paint_overlay((uint32_t*)host->framebuffer,
                         host->width, host->height, host->fb_stride / 4,
                         host->hdc_buffer, frame);

            InvalidateRect(host->hwnd, NULL, FALSE);
        }

        frame++;
        if (frame % 60 == 0) {
            printf("Frame %lld | Particles: %d | Time: %.1fs\n",
                   (long long)frame, g_particle_count, g_time / 1000.0);
        }

        Sleep(16);
    }

    printf("\nShutdown after %lld frames (%.1f seconds simulated).\n",
           (long long)frame, g_time / 1000.0);
    abi_ui_session_destroy(session);
    printf("Done.\n");
    return 0;
}
