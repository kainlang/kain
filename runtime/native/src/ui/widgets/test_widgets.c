// ============================================================================
//  test_widgets.c — Kain Native UI Widget Library Demonstration
//  ============================================================================
//  Comprehensive test showing all 8 widgets from the widget library
//  in a real Win32 window with interactive controls.
//
//  Build:
//    clang -std=c11 -g -O0 test_widgets.c ui_widget.c ^
//      ../ui_system.c ../ui_host_adapter.c ../ui_renderer.c ^
//      ../ui_layout.c ../ui_color.c ^
//      ../../core/input_system.c ^
//      -I../../../include -I.. -I../../core ^
//      -luser32 -lgdi32 -lopengl32 -o test_widgets.exe
//
//  Run:
//    test_widgets.exe
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ui_widget.h"
#include "ui_system.h"          /* from -I../../../include */
#include "ui_system_internal.h"  /* from -I.. */

// ── Win32 Host struct (must match ui_host_adapter.c exactly) ───────────
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

// ── Extern stubs from core.c ──────────────────────────────────────────
extern double kain_clampd(double value, double min_value, double max_value);

// ── Application state ─────────────────────────────────────────────────
static struct {
    int64_t session;
    KainWin32UiHost* host;
    KainUiWidgetContext* ctx;

    // Widget state
    int click_count;
    int feature_enabled;
    double volume;
    char text_buf[64];
    int dark_mode;
    double progress;
    int auto_save;
    double speed;

    // Window state
    double win_x, win_y;
    int win_open;

    // Stats
    int64_t frame_count;
    double fps;
    double fps_timer;
} g_app;

// ── Font showcase state (loaded in main, rendered in render_demo) ──
#define MAX_SHOWCASE_FONTS 16
typedef struct { const char* path; const char* name; double size; } FontEntry;
static FontEntry g_font_entries[MAX_SHOWCASE_FONTS];
static int64_t g_font_ids[MAX_SHOWCASE_FONTS];
static int g_font_count = 0;

static const char* g_font_colors[MAX_SHOWCASE_FONTS] = {
    "#E8E8F0", "#21D4A1", "#4A90D9", "#E8914A", "#E84A5F",
    "#A78BFA", "#34D399", "#F472B6", "#60A5FA", "#FBBF24",
    "#6EE7B7", "#FCA5A5", "#818CF8", "#86EFAC", "#FDBA74",
};

// ── Clear the entire framebuffer to a solid color ─────────────────────
static void clear_framebuffer(uint32_t* fb, int w, int h, int stride, uint32_t color)
{
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            fb[r * stride + c] = color;
        }
    }
}

// ── Fill a single solid rect (fast path for background elements) ──────
static void fill_rect(uint32_t* fb, int stride, int fb_w, int fb_h,
                      int x, int y, int w, int h, uint32_t color)
{
    ui_widget_fill_rect(fb, stride, fb_w, fb_h, x, y, w, h, color);
}

// ============================================================================
//  DEMO RENDER
// ============================================================================

static void render_demo(void)
{
    KainUiWidgetContext* ctx = g_app.ctx;
    KainWin32UiHost* host = g_app.host;
    uint32_t* fb = (uint32_t*)host->framebuffer;
    int stride = host->fb_stride / 4;
    int fb_w = host->width;
    int fb_h = host->height;

    // ── 1. Clear to dark background ────────────────────────────────
    clear_framebuffer(fb, fb_w, fb_h, stride, UI_COLOR_BG);

    // ── 2. Draw a header bar ───────────────────────────────────────
    fill_rect(fb, stride, fb_w, fb_h, 0, 0, fb_w, 50, UI_COLOR_HEADER);
    fill_rect(fb, stride, fb_w, fb_h, 0, 49, fb_w, 2, UI_COLOR_ACCENT);

    // Header title (stb_truetype via widget ABI)
    ui_widget_draw_text(ctx, 14, 16, "Kain Native UI — Widget Library Demo",
                        UI_COLOR_TEXT, 18);

    // FPS display in top-right
    char fps_str[64];
    snprintf(fps_str, sizeof(fps_str), "FPS: %.0f  |  Frame: %lld",
             g_app.fps, (long long)g_app.frame_count);
    ui_widget_draw_text(ctx, fb_w - 260, 16, fps_str,
                        UI_COLOR_TEXT_DIM, 14);

    // ── 3. Left panel: Controls ────────────────────────────────────
    // Draw panel background manually (ui_panel creates nodes + draws)
    ui_panel(ctx, "Controls", 10, 55, 380, 420);
    {
        // Info label showing click count
        char info[64];
        snprintf(info, sizeof(info), "Button clicks: %d", g_app.click_count);
        ui_label(ctx, info);

        // Button
        if (ui_button(ctx, "Click Me!")) {
            g_app.click_count++;
            printf("[EVENT] Button clicked! Count: %d\n", g_app.click_count);
        }

        // Checkbox
        if (ui_checkbox(ctx, "Enable Feature", &g_app.feature_enabled)) {
            printf("[EVENT] Feature %s\n", g_app.feature_enabled ? "enabled" : "disabled");
        }

        // Slider
        ui_label(ctx, "Volume:");
        if (ui_slider(ctx, &g_app.volume, 0.0, 100.0)) {
            printf("[EVENT] Volume: %.1f\n", g_app.volume);
        }

        // Textbox
        ui_label(ctx, "Text Input:");
        if (ui_textbox(ctx, g_app.text_buf, (int)sizeof(g_app.text_buf))) {
            printf("[EVENT] Text: '%s'\n", g_app.text_buf);
        }

        // Another checkbox
        if (ui_checkbox(ctx, "Dark Mode", &g_app.dark_mode)) {
            printf("[EVENT] Dark Mode %s\n", g_app.dark_mode ? "on" : "off");
        }

        // Progress bar
        ui_progress(ctx, "Progress", g_app.progress, 100.0);
    }
    ui_panel_end(ctx);

    // ── 4. FONT SHOWCASE PANEL ────────────────────────────────────
    // Draw each loaded font rendering its name and a sample string
    int fs_x = 410, fs_y = 55, fs_w = fb_w - 430, fs_h = fb_h - 80;
    fill_rect(fb, stride, fb_w, fb_h, fs_x, fs_y, fs_w, fs_h, UI_COLOR_SURFACE);
    fill_rect(fb, stride, fb_w, fb_h, fs_x, fs_y, fs_w, 1, UI_COLOR_ACCENT);

    // Font panel header
    char fs_header[64];
    snprintf(fs_header, sizeof(fs_header), "Font Showcase (%d loaded)", g_font_count);
    ui_widget_draw_text(ctx, fs_x + 12, fs_y + 8, fs_header, UI_COLOR_TEXT, 16);

    uint32_t colors[] = {
        0xFFE8E8F0, 0xFF21D4A1, 0xFF4A90D9, 0xFFE8914A, 0xFFE84A5F,
        0xFFA78BFA, 0xFF34D399, 0xFFF472B6, 0xFF60A5FA, 0xFFFBBF24,
        0xFF6EE7B7, 0xFFFCA5A5, 0xFF818CF8, 0xFF86EFAC, 0xFFFDBA74,
    };

    int fy = fs_y + 38;
    int line_h = 58;
    for (int i = 0; i < g_font_count && i < 15; i++) {
        int row_y = fy + i * line_h;
        int box_x = fs_x + 14;

        // Background stripe (alternating)
        if (i % 2 == 0) {
            fill_rect(fb, stride, fb_w, fb_h, box_x, row_y - 2, fs_w - 28, line_h, 0xFF1A1A30);
        }

        // Small colored indicator dot
        uint32_t dot_col = colors[i % 15];
        fill_rect(fb, stride, fb_w, fb_h, box_x, row_y + 6, 8, 8, dot_col);

        // Font name in the font's own style (medium size)
        ui_widget_draw_text_ex(ctx, box_x + 18, row_y,
                               g_font_entries[i].name,
                               colors[i % 15],
                               g_font_entries[i].size,
                               g_font_ids[i]);

        // Sample sentence in the font (smaller, lighter)
        const char* samples[] = {
            "The quick brown fox jumps over the lazy dog.",
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789",
            "The quick brown fox jumps over the lazy dog.",
        };
        ui_widget_draw_text_ex(ctx, box_x + 18, row_y + 26,
                               samples[i % 3],
                               0xFF8888A0,
                               g_font_entries[i].size * 0.75,
                               g_font_ids[i]);
    }

    // Font count label at bottom of showcase
    char fs_footer[64];
    snprintf(fs_footer, sizeof(fs_footer),
             "%d fonts loaded from C:\\Windows\\Fonts\\", g_font_count);
    ui_widget_draw_text(ctx, fs_x + 12, fy + g_font_count * line_h + 10,
                        fs_footer, 0xFF666680, 12);

    // ── 5. Draggable window ────────────────────────────────────────
    if (g_app.win_open) {
        int was_open = g_app.win_open;
        ui_window(ctx, "Stats", &g_app.win_x, &g_app.win_y, 300, 160, &g_app.win_open);

        if (was_open && !g_app.win_open) {
            printf("[EVENT] Stats window closed\n");
        }

        if (g_app.win_open) {
            // Content inside window
            char line1[64], line2[64], line3[64];
            snprintf(line1, sizeof(line1), "Frame: %lld", (long long)g_app.frame_count);
            snprintf(line2, sizeof(line2), "Mouse: %.0f, %.0f",
                     g_app.ctx->mouse_x, g_app.ctx->mouse_y);
            snprintf(line3, sizeof(line3), "FPS: %.0f", g_app.fps);
            ui_label(ctx, line1);
            ui_label(ctx, line2);
            ui_label(ctx, line3);

            if (ui_button(ctx, "Log Stats")) {
                printf("[STATS] Frame=%lld  Volume=%.1f  Text='%s'  Progress=%.0f%%\n",
                       (long long)g_app.frame_count, g_app.volume,
                       g_app.text_buf[0] ? g_app.text_buf : "(empty)",
                       g_app.progress);
            }

            ui_panel_end(ctx);  // Close window container
        }
    } else {
        // Draw a "Show Window" button if window is closed
        if (ui_button(ctx, "Show Window")) {
            g_app.win_open = 1;
            g_app.win_x = 400;
            g_app.win_y = 60;
            printf("[EVENT] Stats window opened\n");
        }
    }

    // ── 6. Status bar at the bottom ────────────────────────────────
    int sb_y = fb_h - 24;
    fill_rect(fb, stride, fb_w, fb_h, 0, sb_y, fb_w, 24, UI_COLOR_HEADER);
    fill_rect(fb, stride, fb_w, fb_h, 0, sb_y, fb_w, 1, UI_COLOR_BORDER);

    {
        char status[128];
        snprintf(status, sizeof(status),
                 "Kain Widget Demo  |  Esc=Exit  |  Clicks: %d  |  Volume: %.0f  |  %s",
                 g_app.click_count, g_app.volume,
                 g_app.feature_enabled ? "FEATURE ON" : "feature off");
        ui_widget_draw_text(ctx, 10, sb_y + 4, status,
                            UI_COLOR_TEXT_DIM, 14);
    }
}

// ============================================================================
//  MAIN
// ============================================================================

int main(void)
{
    int win_w = 1280, win_h = 720;

    printf("=== Kain Native UI — Widget Library Demo ===\n");
    printf("Build: " __DATE__ " " __TIME__ "\n\n");

    // ── Initialize UI system ───────────────────────────────────────
    abi_ui_reset();

    int64_t session = abi_ui_session_create("WidgetDemo", win_w, win_h);
    if (session <= 0) {
        fprintf(stderr, "FAIL: abi_ui_session_create\n");
        return 1;
    }
    printf("[UI] Session: %lld\n", (long long)session);

    // Open window and attach Win32 host
    abi_ui_window_open(session, "Kain Native UI — Widget Library Demo", win_w, win_h);
    if (abi_ui_host_attach(session, "winit") != 0) {
        fprintf(stderr, "FAIL: abi_ui_host_attach\n");
        return 1;
    }
    printf("[UI] Backend: %s\n", abi_ui_host_backend(session));

    // Get host pointer
    KainNativeUiSession* ks = abi_ui_find_session(session);
    if (!ks || !ks->host_state) {
        fprintf(stderr, "FAIL: no host state\n");
        return 1;
    }
    KainWin32UiHost* host = (KainWin32UiHost*)ks->host_state;
    printf("[UI] Window: %p  %dx%d  fb=%p\n",
           (void*)host->hwnd, host->width, host->height,
           (void*)host->framebuffer);

    // ── Create widget context ──────────────────────────────────────
    KainUiWidgetContext* ctx = ui_widget_create(session);
    if (!ctx) {
        fprintf(stderr, "FAIL: ui_widget_create\n");
        return 1;
    }
    printf("[WIDGET] Context created\n");

    // ── FONT SHOWCASE: Load 15 fonts and render each one ──────────
    FontEntry fonts_to_load[] = {
        {"C:/Windows/Fonts/arial.ttf",          "Arial",             20.0},
        {"C:/Windows/Fonts/arialbd.ttf",        "Arial Bold",        20.0},
        {"C:/Windows/Fonts/ariali.ttf",         "Arial Italic",      20.0},
        {"C:/Windows/Fonts/calibri.ttf",        "Calibri",           20.0},
        {"C:/Windows/Fonts/Candara.ttf",        "Candara",           20.0},
        {"C:/Windows/Fonts/CascadiaCode.ttf",   "Cascadia Code",     18.0},
        {"C:/Windows/Fonts/CascadiaMono.ttf",   "Cascadia Mono",     18.0},
        {"C:/Windows/Fonts/comic.ttf",          "Comic Sans",        20.0},
        {"C:/Windows/Fonts/consola.ttf",        "Consolas",          18.0},
        {"C:/Windows/Fonts/Gabriola.ttf",       "Gabriola",          26.0},
        {"C:/Windows/Fonts/Inkfree.ttf",        "Ink Free",          22.0},
        {"C:/Windows/Fonts/bahnschrift.ttf",    "Bahnschrift",       20.0},
        {"C:/Windows/Fonts/cambriab.ttf",       "Cambria Bold",      20.0},
        {"C:/Windows/Fonts/ariblk.ttf",         "Arial Black",       20.0},
        {"C:/Windows/Fonts/JetBrainsMonoNLNerdFont-Regular.ttf", "JetBrains Mono", 18.0},
        {NULL, NULL, 0}
    };

    g_font_count = 0;
    for (int i = 0; fonts_to_load[i].path && g_font_count < MAX_SHOWCASE_FONTS; i++) {
        int64_t fid = ui_widget_load_font(ctx, fonts_to_load[i].path, fonts_to_load[i].size);
        if (fid > 0) {
            g_font_entries[g_font_count] = fonts_to_load[i];
            g_font_ids[g_font_count] = fid;
            g_font_count++;
            printf("[FONT %d] %s (id=%lld, %.0fpx)\n",
                   g_font_count, fonts_to_load[i].name,
                   (long long)fid, fonts_to_load[i].size);
        } else {
            printf("[FONT FAIL] %s — %s\n", fonts_to_load[i].name, fonts_to_load[i].path);
        }
    }
    printf("[FONT] Loaded %d/%zu fonts\n", g_font_count,
           sizeof(fonts_to_load)/sizeof(fonts_to_load[0]) - 1);

    // ── Initialize app state ───────────────────────────────────────
    g_app.session = session;
    g_app.host = host;
    g_app.ctx = ctx;
    g_app.text_buf[0] = '\0';
    g_app.volume = 50.0;
    g_app.speed = 1.0;
    g_app.feature_enabled = 1;
    g_app.win_open = 1;
    g_app.win_x = 400;
    g_app.win_y = 60;
    g_app.frame_count = 0;

    // ── Frame Loop ─────────────────────────────────────────────────
    printf("\nFrame loop running. Close window or press Esc to exit.\n");
    printf("========================================================\n");

    int running = 1;
    MSG msg;
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    while (running) {
        // ── Message pump ──────────────────────────────────────────
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = 0;
                host->running = 0;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!host->running) running = 0;
        if (!running) break;

        // Check Escape key
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            printf("Escape pressed, exiting.\n");
            break;
        }

        // ── Begin UI frame + widgets ──────────────────────────────
        abi_ui_begin_frame(session, 16.67);
        ui_widget_begin_frame(ctx);

        // Update simulated progress
        g_app.progress += 0.5 * g_app.speed;
        if (g_app.progress > 100.0) {
            g_app.progress = 0.0;
        }

        // ── Render all widgets ────────────────────────────────────
        render_demo();

        // ── End frame ─────────────────────────────────────────────
        ui_widget_end_frame(ctx);
        abi_ui_end_frame(session);

        // ── Update window ────────────────────────────────────────
        InvalidateRect(host->hwnd, NULL, FALSE);

        // ── FPS counter ──────────────────────────────────────────
        g_app.frame_count++;
        QueryPerformanceCounter(&end);
        double elapsed = (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)freq.QuadPart;
        if (elapsed >= 1000.0) {
            g_app.fps = (double)g_app.frame_count * 1000.0 / elapsed;
            g_app.frame_count = 0;
            QueryPerformanceCounter(&start);
        }

        // ── Rate limit ~60fps ────────────────────────────────────
        Sleep(16);
    }

    // ── Cleanup ───────────────────────────────────────────────────
    printf("\nShutting down...\n");
    ui_widget_destroy(ctx);
    abi_ui_session_destroy(session);
    printf("Done.\n");

    return 0;
}
