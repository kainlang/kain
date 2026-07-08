// ============================================================================
//  dpi_scaling_verification.c — DPI + Zoom Math Verification
//
//  Proves: kt_set_native_scale, kt_set_zoom, kt_scale_factor_x/y,
//          kt_native_scale_x/y, kt_round_to_pixel_*. 
//  Backend: null (headless, pure math verification).
//
//  Compile:
//    gcc -std=c11 -I ../../include -I .. tree.c box_math.c damage.c
//        draw_pixels.c arena.c hash_table.c color.c attr_table.c
//        kaintana_runtime_stubs.c ../../src/core/arena.c ../../src/core/version.c
//        ../../src/core/component_surface.c ../../src/core/handle.c
//        ../../src/core/input_system.c
//        examples_v2/dpi_scaling_verification.c -o dpi_scaling.exe
//
//  Run:
//    ./dpi_scaling.exe
// ============================================================================

#include "kaintana.h"
#include "backends/null/host_null.c"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  \x1b[1;31mFAIL\x1b[0m: %s\n", msg); failures++; } \
    else { printf("  \x1b[1;32mPASS\x1b[0m: %s\n", msg); } \
} while(0)

#define CLOSE_ENOUGH(a, b, eps) (fabsf((a) - (b)) < (eps))

int main(void) {
    printf("\n\x1b[1;36m=== Kaintana DPI Scaling Verification ===\x1b[0m\n\n");

    kt_init();
    kt_Session* s = kt_make("dpi_test", 1920, 1080);
    if (!s) { printf("FATAL: kt_make NULL\n"); return 1; }
    kt_backend_register(s, "null", &kaintana_null_backend);
    kt_backend_select(s, "null");

    // ── Test 1: Default scale (1.0x) ──────────────────────────────
    printf("\x1b[1;33mTest 1: Default 1.0x scale\x1b[0m\n");
    CHECK(CLOSE_ENOUGH(kt_scale_factor_x(s), 1.0f, 0.001f), "scale_factor_x default = 1.0");
    CHECK(CLOSE_ENOUGH(kt_scale_factor_y(s), 1.0f, 0.001f), "scale_factor_y default = 1.0");
    CHECK(CLOSE_ENOUGH(kt_native_scale_x(s), 1.0f, 0.001f), "native_scale_x default = 1.0");
    CHECK(CLOSE_ENOUGH(kt_native_scale_y(s), 1.0f, 0.001f), "native_scale_y default = 1.0");

    // ── Test 2: 150% DPI (Windows 150% scaling = 1.5x) ───────────
    printf("\n\x1b[1;33mTest 2: 150%% DPI (1.5x)\x1b[0m\n");
    kt_set_native_scale(s, 1.5f, 1.5f);
    CHECK(CLOSE_ENOUGH(kt_native_scale_x(s), 1.5f, 0.001f), "native_scale_x = 1.5");
    CHECK(CLOSE_ENOUGH(kt_native_scale_y(s), 1.5f, 0.001f), "native_scale_y = 1.5");
    CHECK(CLOSE_ENOUGH(kt_scale_factor_x(s), 1.5f, 0.001f), "effective x = 1.5 (no zoom)");
    CHECK(CLOSE_ENOUGH(kt_scale_factor_y(s), 1.5f, 0.001f), "effective y = 1.5 (no zoom)");

    // ── Test 3: 150% DPI + 2.0x user zoom = 3.0x effective ───────
    printf("\n\x1b[1;33mTest 3: 150%% DPI + 2.0x zoom = 3.0x effective\x1b[0m\n");
    kt_set_zoom(s, 2.0f);
    CHECK(CLOSE_ENOUGH(kt_native_scale_x(s), 1.5f, 0.001f), "native_scale_x stays 1.5");
    CHECK(CLOSE_ENOUGH(kt_scale_factor_x(s), 3.0f, 0.001f), "effective x = 1.5 * 2.0 = 3.0");
    CHECK(CLOSE_ENOUGH(kt_scale_factor_y(s), 3.0f, 0.001f), "effective y = 3.0");

    // ── Test 4: Zoom clamping to [0.2, 5.0] ──────────────────────
    printf("\n\x1b[1;33mTest 4: Zoom clamping [0.2, 5.0]\x1b[0m\n");
    kt_set_zoom(s, 0.05f);  // below min
    CHECK(CLOSE_ENOUGH(kt_scale_factor_x(s), 1.5f * 0.2f, 0.01f), "zoom clamped to 0.2 min");
    kt_set_zoom(s, 10.0f);  // above max
    CHECK(CLOSE_ENOUGH(kt_scale_factor_x(s), 1.5f * 5.0f, 0.01f), "zoom clamped to 5.0 max");
    kt_set_zoom(s, 1.0f);   // reset

    // ── Test 5: Native scale clamping to [0.1, 10.0] ─────────────
    printf("\n\x1b[1;33mTest 5: Native scale clamping [0.1, 10.0]\x1b[0m\n");
    kt_set_native_scale(s, 0.01f, 0.01f);
    CHECK(CLOSE_ENOUGH(kt_native_scale_x(s), 0.1f, 0.001f), "native_scale clamped to 0.1 min");
    kt_set_native_scale(s, 50.0f, 50.0f);
    CHECK(CLOSE_ENOUGH(kt_native_scale_x(s), 10.0f, 0.001f), "native_scale clamped to 10.0 max");
    kt_set_native_scale(s, 1.0f, 1.0f);  // reset

    // ── Test 6: Pixel snap math ───────────────────────────────────
    printf("\n\x1b[1;33mTest 6: Pixel-snap helpers\x1b[0m\n");
    float snap_x = kt_round_to_pixel_x(10.3f, 2.0f);
    // roundf(10.3 * 2.0) / 2.0 = roundf(20.6) / 2.0 = 21.0 / 2.0 = 10.5
    CHECK(CLOSE_ENOUGH(snap_x, 10.5f, 0.01f), "round_to_pixel_x(10.3, 2.0) = 10.5");
    float snap_y = kt_round_to_pixel_y(5.7f, 1.5f);
    // roundf(5.7 * 1.5) / 1.5 = roundf(8.55) / 1.5 = 9.0 / 1.5 = 6.0
    CHECK(CLOSE_ENOUGH(snap_y, 6.0f, 0.01f), "round_to_pixel_y(5.7, 1.5) = 6.0");

    // ── Test 7: DPI change triggers layout invalidation ───────────
    printf("\n\x1b[1;33mTest 7: DPI change → layout invalidation\x1b[0m\n");
    kt_set_native_scale(s, 2.0f, 2.0f);
    kt_begin(s, 16.0f);
    int root = kt_row(s, 0, "box", "dpi_box");
    kt_width(s, root, 100);
    kt_height(s, root, 50);
    kt_fill(s, root, "#FF4444");
    kt_end_row(s);
    kt_end(s);
    int cmds = kt_cmd_count(s);
    CHECK(cmds > 0, "DPI change didn't prevent command generation");
    printf("  Commands generated at 2x DPI: %d\n", cmds);

    kt_free(s);

    // ── Summary ───────────────────────────────────────────────────
    printf("\n\x1b[1;36m=== Results: %d failures ===\x1b[0m\n", failures);
    return failures > 0 ? 1 : 0;
}
