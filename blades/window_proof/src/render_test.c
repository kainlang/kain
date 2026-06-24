// ============================================================================
// render_test.c — Minimal C test that exactly mimics the Kain frame loop
// ============================================================================
// Build:
//   LIB="..." clang -std=c11 -g -O0 render_test.c \
//     ../runtime/native/src/ui/ui_system.c \
//     ../runtime/native/src/ui/ui_host_adapter.c \
//     ../runtime/native/src/ui/ui_renderer.c \
//     ../runtime/native/src/ui/ui_layout.c \
//     ../runtime/native/src/ui/ui_color.c \
//     ../runtime/native/src/ui/stubs.c \
//     -I../runtime/native/include \
//     -I../runtime/native/src/ui \
//     -I../runtime/native/src/core \
//     -luser32 -lgdi32 -o render_test.exe
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_system.h"
#include "ui_system_internal.h"
#include "ui_host_adapter.h"
#include "../../include/ui_renderer.h"
#include "../../include/ui_layout.h"
#include "../../include/ui_color.h"

char* string_new(char* src);
double kain_clampd(double value, double min_value, double max_value);

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

// ── Main ────────────────────────────────────────────────────────────────
int main(void) {
    printf("=== Render Test ===\n");

    abi_ui_reset();
    int64_t session = abi_ui_session_create("rendertest", 640, 480);
    if (session <= 0) { printf("FAIL: session_create\n"); return 1; }

    abi_ui_window_open(session, "Render Test", 640, 480);
    if (abi_ui_host_attach(session, "winit") != 0) {
        printf("FAIL: host_attach\n"); return 1;
    }
    printf("Backend: %s\n", abi_ui_host_backend(session));

    KainNativeUiSession* ks = abi_ui_find_session(session);
    if (!ks || !ks->host_state) { printf("FAIL: no host_state\n"); return 1; }
    KainWin32UiHost* host = (KainWin32UiHost*)ks->host_state;
    printf("Host: hwnd=%p fb=%p %dx%d stride=%d\n",
           (void*)host->hwnd, (void*)host->framebuffer,
           host->width, host->height, host->fb_stride);

    // Create nodes + styles (same as Kain code)
    int64_t root = abi_ui_node_create(session, "root");
    abi_ui_node_set_rect(session, root, 0, 0, 640, 480);
    abi_ui_node_set_style_string(session, root, "fill_color", "#1A1A24");

    int64_t box1 = abi_ui_node_create(session, "box");
    abi_ui_node_set_parent(session, box1, root);
    abi_ui_node_set_rect(session, box1, 50, 50, 200, 150);
    abi_ui_node_set_style_string(session, box1, "fill_color", "#21D4A1");

    printf("Nodes: root=%lld box1=%lld\n", (long long)root, (long long)box1);

    // ── Frame pipeline (exactly as Kain does it) ──────────────────
    
    // 1. begin_frame
    int64_t bf = abi_ui_begin_frame(session, 16.0);
    printf("begin_frame=%lld\n", (long long)bf);

    // 2. draw_rect
    int64_t d1 = abi_ui_draw_rect(session, root, 0, 0, 640, 480, "fill_color");
    printf("draw1=%lld\n", (long long)d1);
    int64_t d2 = abi_ui_draw_rect(session, box1, 50, 50, 200, 150, "fill_color");
    printf("draw2=%lld\n", (long long)d2);

    printf("draw_commands=%lld\n", (long long)ks->draw_command_count);
    
    // 3. end_frame
    int64_t ef = abi_ui_end_frame(session);
    printf("end_frame=%lld\n", (long long)ef);
    
    // 4. present
    int64_t pr = abi_ui_present(session);
    printf("present=%lld\n", (long long)pr);
    
    // 5. host_present
    printf("Calling host_present...\n");
    int64_t hp = abi_ui_host_present(session);
    printf("host_present=%lld\n", (long long)hp);

    printf("Rendered. Waiting 3 seconds...\n");
    Sleep(3000);

    abi_ui_session_destroy(session);
    printf("Done.\n");
    return 0;
}
