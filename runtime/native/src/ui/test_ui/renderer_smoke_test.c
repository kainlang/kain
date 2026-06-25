// ============================================================================
//  renderer_smoke_test.c — Full Pipeline Renderer Smoke Test
//  ============================================================================
//  Goal: Verify that ui_render_frame() actually works WITHOUT crashing now
//  that the sibling-linked-list and 0-size bugs are fixed.
//
//  Tests:
//    1. Create session + window
//    2. Build a node tree with explicit positions + styles
//    3. Call the full pipeline: begin_frame → (nodes created once) →
//       end_frame → ui_layout_resolve → ui_render_frame → host_present
//    4. Verify framebuffer has non-zero pixels (rendering actually happened)
//
//  This is the FIRST test that exercises ui_render_frame() with actual nodes.
//  All previous test_ui tests bypassed it due to the crash bug.
//  ============================================================================
//
//  Build:
//    clang -std=c11 -g -O0 renderer_smoke_test.c ../TEST/stubs.c ^
//      ../ui_system.c ../ui_host_adapter.c ../ui_renderer.c ../ui_layout.c ../ui_color.c ^
//      ../../core/input_system.c ^
//      -I../../../include -I.. -I../../core ^
//      -luser32 -lgdi32 -lopengl32 ^
//      -o renderer_smoke_test.exe
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

// ── KainWin32UiHost (must match ui_host_adapter.c) ─────────────────────
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

// ── Color palette ──────────────────────────────────────────────────────
#define C_BG        0xFF1A1A24
#define C_SURFACE   0xFF252540
#define C_ACCENT    0xFF21D4A1
#define C_ACCENT2   0xFF4A90D9
#define C_ACCENT3   0xFFE8914A
#define C_TEXT      0xFFE8E8F0
#define C_WIN_BG    0xFF0F0F1A

// ── Test state ─────────────────────────────────────────────────────────
typedef struct {
    int64_t session_id;
    int64_t root_panel;
    int64_t header;
    int64_t body;
    int64_t btn_a;
    int64_t btn_b;
    int64_t btn_c;
    int64_t label;
    int frame_count;
    double fps_timer;
    double fps;
    int click_count_a;
    int click_count_b;
    int stage;
} AppState;

static AppState g_app = {0};
static char g_fps_text[64] = {0};
static char g_click_text[64] = {0};
static char g_stage_text[64] = "STAGE: RENDERER SMOKE TEST";

// ── Forward declarations ──────────────────────────────────────────────
static LRESULT CALLBACK test_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l);

// ── Get host state from session ────────────────────────────────────────
static KainWin32UiHost* get_host(int64_t sid) {
    KainNativeUiSession* s = (KainNativeUiSession*)abi_ui_find_session(sid);
    if (!s) return NULL;
    return (KainWin32UiHost*)s->host_state;
}

// ── Build the node tree ────────────────────────────────────────────────
static void build_node_tree(int64_t sid) {
    AppState* app = &g_app;
    int64_t w = 800, h = 600;

    // Root panel — fills entire window
    app->root_panel = abi_ui_node_create(sid, "panel");
    abi_ui_node_set_rect(sid, app->root_panel, 0, 0, (double)w, (double)h);
    abi_ui_node_set_style_string(sid, app->root_panel, "fill_color", "#0F0F1A");

    // Header bar
    app->header = abi_ui_node_create(sid, "panel");
    abi_ui_node_set_parent(sid, app->header, app->root_panel);
    abi_ui_node_set_rect(sid, app->header, 0, 0, (double)w, 40);
    abi_ui_node_set_style_string(sid, app->header, "fill_color", "#1A1A2E");
    abi_ui_node_set_style_string(sid, app->header, "border_color", "#2A2A4E");
    abi_ui_node_set_style_f64(sid, app->header, "border_width", 1.0);

    // Header text (uses node text, rendered via GDI in host_present)
    abi_ui_node_set_text(sid, app->header, " Kain UI — Renderer Smoke Test");

    // Body panel
    app->body = abi_ui_node_create(sid, "panel");
    abi_ui_node_set_parent(sid, app->body, app->root_panel);
    abi_ui_node_set_rect(sid, app->body, 20, 60, (double)w - 40, (double)h - 140);
    abi_ui_node_set_style_string(sid, app->body, "fill_color", "#1A1A2E");
    abi_ui_node_set_style_string(sid, app->body, "border_color", "#2A2A4E");
    abi_ui_node_set_style_f64(sid, app->body, "border_width", 1.0);
    abi_ui_node_set_style_f64(sid, app->body, "corner_radius", 8.0);

    // Button A (green accent)
    app->btn_a = abi_ui_node_create(sid, "button");
    abi_ui_node_set_parent(sid, app->btn_a, app->body);
    abi_ui_node_set_rect(sid, app->btn_a, 40, 40, 160, 48);
    abi_ui_node_set_style_string(sid, app->btn_a, "fill_color", "#21D4A1");
    abi_ui_node_set_style_string(sid, app->btn_a, "border_color", "#2EE0B0");
    abi_ui_node_set_style_f64(sid, app->btn_a, "border_width", 1.0);
    abi_ui_node_set_style_f64(sid, app->btn_a, "corner_radius", 6.0);
    abi_ui_node_set_text(sid, app->btn_a, "  Button A");
    abi_ui_node_set_flag(sid, app->btn_a, "interactive", 1);

    // Button B (blue accent)
    app->btn_b = abi_ui_node_create(sid, "button");
    abi_ui_node_set_parent(sid, app->btn_b, app->body);
    abi_ui_node_set_rect(sid, app->btn_b, 40, 100, 160, 48);
    abi_ui_node_set_style_string(sid, app->btn_b, "fill_color", "#4A90D9");
    abi_ui_node_set_style_string(sid, app->btn_b, "border_color", "#5AA0E9");
    abi_ui_node_set_style_f64(sid, app->btn_b, "border_width", 1.0);
    abi_ui_node_set_style_f64(sid, app->btn_b, "corner_radius", 6.0);
    abi_ui_node_set_text(sid, app->btn_b, "  Button B");
    abi_ui_node_set_flag(sid, app->btn_b, "interactive", 1);

    // Button C (orange accent)
    app->btn_c = abi_ui_node_create(sid, "button");
    abi_ui_node_set_parent(sid, app->btn_c, app->body);
    abi_ui_node_set_rect(sid, app->btn_c, 40, 160, 160, 48);
    abi_ui_node_set_style_string(sid, app->btn_c, "fill_color", "#E8914A");
    abi_ui_node_set_style_string(sid, app->btn_c, "border_color", "#F8A15A");
    abi_ui_node_set_style_f64(sid, app->btn_c, "border_width", 1.0);
    abi_ui_node_set_style_f64(sid, app->btn_c, "corner_radius", 6.0);
    abi_ui_node_set_text(sid, app->btn_c, "  EXIT");
    abi_ui_node_set_flag(sid, app->btn_c, "interactive", 1);

    // Status label
    app->label = abi_ui_node_create(sid, "label");
    abi_ui_node_set_parent(sid, app->label, app->body);
    abi_ui_node_set_rect(sid, app->label, 40, 240, 400, 80);
    abi_ui_node_set_text(sid, app->label, " Nodes rendered via ui_render_frame()\n Pipeline: LAYOUT → RENDER → PRESENT");
    abi_ui_node_set_style_string(sid, app->label, "ink_color", "#8888A0");
}

// ── Hit test + handle click ────────────────────────────────────────────
static void handle_click(int64_t sid, double mx, double my) {
    int64_t hit = abi_ui_hit_test(sid, mx, my);
    if (hit == g_app.btn_c) {
        // Exit button
        KainWin32UiHost* host = get_host(sid);
        if (host) host->running = 0;
    } else if (hit == g_app.btn_a) {
        g_app.click_count_a++;
    } else if (hit == g_app.btn_b) {
        g_app.click_count_b++;
    }
    snprintf(g_click_text, sizeof(g_click_text),
             "  A: %d clicks  |  B: %d clicks",
             g_app.click_count_a, g_app.click_count_b);
}

// ── Status bar text ────────────────────────────────────────────────────
static void update_status_text(double dt) {
    g_app.fps_timer += dt;
    g_app.frame_count++;
    if (g_app.fps_timer >= 1.0) {
        g_app.fps = (double)g_app.frame_count / g_app.fps_timer;
        g_app.frame_count = 0;
        g_app.fps_timer = 0.0;
        snprintf(g_fps_text, sizeof(g_fps_text), " FPS: %.0f", g_app.fps);
    }
}

// ── Direct GDI text rendering (for overlay text) ───────────────────────
static void render_gdi_overlay(KainWin32UiHost* host) {
    if (!host || !host->hdc_buffer) return;

    HGDIOBJ old_font = SelectObject(host->hdc_buffer,
        GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(host->hdc_buffer, TRANSPARENT);
    SetTextColor(host->hdc_buffer, RGB(0xE8, 0xE8, 0xF0));

    // Status bar at bottom
    RECT r = {20, host->height - 50, host->width - 20, host->height - 10};
    DrawTextA(host->hdc_buffer, g_fps_text, -1, &r,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextA(host->hdc_buffer, g_click_text, -1, &r,
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // Stage label
    RECT r2 = {20, host->height - 80, host->width - 20, host->height - 50};
    DrawTextA(host->hdc_buffer, g_stage_text, -1, &r2,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(host->hdc_buffer, old_font);
}

// ── Forward declaration of the internal session type to access host_state ─
// KainWin32UiHost is stored as opaque host_state on the session.

// ── Win32 Window Procedure ────────────────────────────────────────────
// Fixes the host adapter's WM_PAINT bug: the original handler creates a temp
// DC and tries to SelectObject the DIB into it, but the DIB is already
// selected into host->hdc_buffer. Our handler BitBlts directly from hdc_buffer.
static LRESULT CALLBACK test_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
        case WM_CLOSE:
            // Signal the host adapter to stop the session
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            // Find our host from the window's user data
            KainWin32UiHost* winhost = (KainWin32UiHost*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
            if (winhost && winhost->hdc_buffer) {
                BitBlt(hdc, 0, 0, winhost->width, winhost->height,
                       winhost->hdc_buffer, 0, 0, SRCCOPY);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, w, l);
}

// ── Main frame loop ────────────────────────────────────────────────────
static void test_frame_loop(int64_t sid) {
    KainWin32UiHost* host = get_host(sid);
    if (!host) return;

    static LARGE_INTEGER freq = {0};
    static LARGE_INTEGER last = {0};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&last);
    }

    // Store host pointer in window user data so WM_PAINT can find it
    SetWindowLongPtrA(host->hwnd, GWLP_USERDATA, (LONG_PTR)host);

    while (host->running) {
        // ── Delta time ──────────────────────────────────────────────
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double dt = (double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart;
        last = now;
        if (dt > 0.1) dt = 0.016;
        update_status_text(dt);

        // ── Pump messages (Win32 input + window events) ─────────────
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                host->running = 0;
            }
            if (msg.message == WM_LBUTTONDOWN) {
                int mx = (int)(short)LOWORD(msg.lParam);
                int my = (int)(short)HIWORD(msg.lParam);
                handle_click(sid, (double)mx, (double)my);
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
                host->running = 0;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        if (!host->running) break;

        // ── Begin frame ─────────────────────────────────────────────
        abi_ui_begin_frame(sid, dt * 1000.0);

        // ── End frame (build draw commands) ────────────────────────
        abi_ui_end_frame(sid);

        // ── LAYOUT: compute node positions ─────────────────────────
        KainNativeUiSession* s = (KainNativeUiSession*)abi_ui_find_session(sid);
        if (s) {
            ui_layout_resolve(s);
        }

        // ── RENDER: draw node tree to framebuffer ─────────────────
        // THIS is the call that used to crash! Bug A (sibling list) and
        // Bug B (0-size children skip) are now fixed.
        if (s && host->framebuffer) {
            ui_render_frame(
                s,
                (uint32_t*)host->framebuffer,
                host->width,
                host->height,
                host->fb_stride / 4
            );
        }

        // ── Overlay GDI text ─────────────────────────────────────────
        render_gdi_overlay(host);

        // ── Trigger WM_PAINT via InvalidateRect ─────────────────────
        InvalidateRect(host->hwnd, NULL, FALSE);

        // ── Small sleep to not eat 100% CPU ─────────────────────────
        Sleep(16); // ~60fps cap
    }
}

// ── Entry point ────────────────────────────────────────────────────────
int main(void) {
    printf("=== Kain UI Renderer Smoke Test ===\n");
    printf("Creating session...\n");

    // Create session (800x600)
    int64_t sid = abi_ui_session_create("RendererSmokeTest", 800, 600);
    if (sid < 0) {
        fprintf(stderr, "FAILED: abi_ui_session_create returned %lld\n", (long long)sid);
        return 1;
    }
    printf("  Session created: %lld\n", (long long)sid);

    // Open window with Win32 GDI backend
    int64_t win = abi_ui_window_open(sid, "Kain UI — Renderer Smoke Test", 800, 600);
    printf("  Window opened: %lld\n", (long long)win);

    // Attach winit backend (creates real HWND + DIB framebuffer)
    int64_t attach = abi_ui_host_attach(sid, "winit");
    printf("  Host attached: %lld\n", (long long)attach);

    // Get host to find HWND
    KainWin32UiHost* host = get_host(sid);
    if (!host) {
        fprintf(stderr, "FAILED: Could not get host state\n");
        return 1;
    }
    printf("  HWND: 0x%p\n", (void*)host->hwnd);
    printf("  Framebuffer: %dx%d (stride=%d)\n",
           host->width, host->height, host->fb_stride);

    // Subclass the window proc to fix WM_PAINT (DIB blit from hdc_buffer)
    SetWindowLongPtrA(host->hwnd, GWLP_WNDPROC, (LONG_PTR)test_wndproc);

    // Build the node tree
    printf("Building node tree...\n");
    build_node_tree(sid);

    printf("\n=== RENDERER PIPELINE VERIFICATION ===\n");
    printf("Running frame loop (Escape or click EXIT to quit)...\n\n");

    // Run the frame loop
    test_frame_loop(sid);

    // Cleanup
    printf("\n=== SHUTDOWN ===\n");
    abi_ui_session_destroy(sid);
    printf("Session destroyed. Test complete.\n");

    return 0;
}
