// ============================================================================
//  font_test.c — stb_truetype Font Integration Test
//  ============================================================================
//  Verifies the full font system pipeline:
//    1. Create a session
//    2. Load a TTF font from C:/Windows/Fonts/segoeui.ttf
//    3. Create a node with text and style
//    4. Measure text width/height with real metrics
//    5. Render text glyphs into a framebuffer
//    6. Read back and verify glyph pixels exist
//
//  Build:
//    clang -std=c11 -g -O0 font_test.c stubs.c ^
//      ../ui_system.c ../ui_renderer.c ../ui_color.c ../ui_layout.c ^
//      ../../core/input_system.c ../../core/component_surface.c ^
//      -I../../../include -I.. -I../../core -I../../../extras/_stb-truetype ^
//      -luser32 -lgdi32 -lopengl32 ^
//      <msvc lib paths> ^
//      -o font_test.exe
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_system.h"
#include "ui_system_internal.h"
#include "ui_host_adapter.h"
#include "../../include/ui_renderer.h"
#include "../../include/ui_color.h"
#include "../../include/ui_font.h"

// Stub dependencies
char* string_new(char* src);
double kain_clampd(double value, double min_value, double max_value);

// ── Test helpers ────────────────────────────────────────────────────────

static int test_count = 0;
static int pass_count = 0;

#define TEST(cond, msg) do { \
    test_count++; \
    if (!(cond)) { \
        printf("  FAIL [%d] %s\n", test_count, msg); \
    } else { \
        printf("  PASS [%d] %s\n", test_count, msg); \
        pass_count++; \
    } \
} while(0)

// ── Load a TTF file from disk ──────────────────────────────────────────
static uint8_t* load_ttf_file(const char* path, int64_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("  INFO: cannot open '%s'\n", path);
        *out_len = 0;
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) {
        fclose(f);
        *out_len = 0;
        return NULL;
    }
    uint8_t* data = (uint8_t*)malloc((size_t)len);
    if (!data) {
        fclose(f);
        *out_len = 0;
        return NULL;
    }
    size_t read = fread(data, 1, (size_t)len, f);
    fclose(f);
    if ((long)read != len) {
        free(data);
        *out_len = 0;
        return NULL;
    }
    *out_len = (int64_t)len;
    return data;
}

// ── Count non-zero pixels in a framebuffer region ──────────────────────
static int count_nonzero_pixels(uint32_t* fb, int stride, int x, int y, int w, int h) {
    int count = 0;
    int row, col;
    for (row = y; row < y + h && row >= 0; row++) {
        for (col = x; col < x + w && col >= 0; col++) {
            if (fb[row * stride + col] != 0xFF1A1A24) {  /* not background */
                count++;
            }
        }
    }
    return count;
}

// ═════════════════════════════════════════════════════════════════════
//  MAIN
// ═════════════════════════════════════════════════════════════════════

int main(void) {
    int64_t session_id;
    int64_t font_id;
    KainNativeUiSession* session;
    uint8_t* ttf_data = NULL;
    int64_t ttf_len = 0;
    const char* font_paths[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/times.ttf",
    };
    int i;
    int found_font = 0;

    printf("=== Kain UI Font Integration Test ===\n\n");

    // ── 1. Session creation ──────────────────────────────────────────
    printf("[01] Session lifecycle:\n");
    session_id = abi_ui_session_create("font_test", 640, 480);
    TEST(session_id > 0, "session created");
    session = abi_ui_find_session(session_id);
    TEST(session != NULL, "session found by ID");

    // ── 2. Find and load a TTF font ──────────────────────────────────
    printf("\n[02] Font loading:\n");
    for (i = 0; i < 3; i++) {
        ttf_data = load_ttf_file(font_paths[i], &ttf_len);
        if (ttf_data && ttf_len > 0) {
            printf("  Loaded: %s (%lld bytes)\n", font_paths[i], (long long)ttf_len);
            found_font = 1;
            break;
        }
    }

    if (!found_font) {
        printf("  WARN: No system font found — creating dummy test\n");
        // Create minimal test with dummy font
    }

    font_id = abi_ui_font_create(session_id, "test-font", "Segoe UI", 18.0);
    TEST(font_id > 0, "font resource created");

    if (found_font) {
        int64_t status = abi_ui_resource_set_bytes(session_id, font_id, ttf_data, ttf_len);
        TEST(status >= 0, "TTF data loaded into font resource");

        // Verify font data initialized
        const char* rtype = abi_ui_resource_type(session_id, font_id);
        int64_t rlen = abi_ui_resource_byte_length(session_id, font_id);
        TEST(rtype && rtype[0] != 0, "font resource has type string");
        TEST(rlen == ttf_len, "font resource byte_length = TTF data size");
        /* Internal font_data verified via glyph API */
    } else {
        printf("  SKIP: font data not loaded, skipping glyph tests\n");
    }

    // ── 3. Text measurement (real vs fallback) ───────────────────────
    printf("\n[03] Text measurement:\n");
    {
        double w1 = abi_ui_text_measure_width(session_id, font_id, "Hello World");
        double h1 = abi_ui_text_measure_height(session_id, font_id, "Hello World");
        TEST(w1 > 0.0, "measure_width > 0");
        TEST(h1 > 0.0, "measure_height > 0");
        printf("      Measure: width=%.1f, height=%.1f\n", w1, h1);

        if (found_font) {
            /* With real font, width should be different from heuristic */
            double heuristic = strlen("Hello World") * 18.0 * 0.56;
            printf("      Real=%.1f, Heuristic=%.1f\n", w1, heuristic);
        }
    }

    // ── 4. Glyph cache hit/miss ──────────────────────────────────────
    printf("\n[04] Glyph cache:\n");
    if (found_font) {
        KainUiGlyph* gA = abi_ui_font_get_glyph(session_id, font_id, 'A');
        TEST(gA != NULL, "glyph 'A' found");
        if (gA) {
            TEST(gA->width > 0 && gA->height > 0, "glyph 'A' has bitmap dimensions");
            TEST(gA->bitmap != NULL, "glyph 'A' has bitmap data");
            printf("      Glyph 'A': %dx%d, advance=%d, offset=(%d,%d)\n",
                   gA->width, gA->height, gA->advance, gA->x_offset, gA->y_offset);

            /* Second lookup should be cache hit */
            KainUiGlyph* gA2 = abi_ui_font_get_glyph(session_id, font_id, 'A');
            TEST(gA2 != NULL && gA2->bitmap == gA->bitmap, "glyph 'A' cache hit (same bitmap ptr)");

            abi_ui_font_release_glyph(gA2);
            abi_ui_font_release_glyph(gA);
        }

        /* Test multiple glyphs */
        KainUiGlyph* glyphs[5];
        const char* test_chars = "Hello";
        int ci;
        for (ci = 0; ci < 5; ci++) {
            glyphs[ci] = abi_ui_font_get_glyph(session_id, font_id, test_chars[ci]);
        }
        for (ci = 0; ci < 5; ci++) {
            if (glyphs[ci]) {
                printf("      Glyph '%c': %dx%d advance=%d\n",
                       test_chars[ci], glyphs[ci]->width, glyphs[ci]->height,
                       glyphs[ci]->advance);
            }
        }
        for (ci = 0; ci < 5; ci++) {
            if (glyphs[ci]) abi_ui_font_release_glyph(glyphs[ci]);
        }

        /* Test unicode (e.g. euro sign) */
        KainUiGlyph* gEuro = abi_ui_font_get_glyph(session_id, font_id, 0x20AC);
        TEST(gEuro != NULL, "glyph Euro sign (U+20AC) found");
        if (gEuro) abi_ui_font_release_glyph(gEuro);

        /* Test missing glyph */
        KainUiGlyph* gMissing = abi_ui_font_get_glyph(session_id, font_id, 0xFFFFFF);
        /* This may or may not be NULL depending on font coverage */
        printf("      Glyph U+FFFFFF: %s\n", gMissing ? "found (fallback font?)" : "NULL (ok)");
        if (gMissing) abi_ui_font_release_glyph(gMissing);
    } else {
        printf("  SKIP: no font data loaded\n");
    }

    // ── 5. Font vertical metrics ─────────────────────────────────────
    printf("\n[05] Font vertical metrics:\n");
    if (found_font) {
        int ascent = 0, descent = 0, line_gap = 0;
        int r = kain_ui_font_get_vmetrics(session_id, font_id, &ascent, &descent, &line_gap);
        TEST(r == 0, "vmetrics returned 0");
        if (r == 0) {
            TEST(ascent > 0, "ascent > 0");
            TEST(descent < 0, "descent < 0");
            printf("      ascent=%d descent=%d line_gap=%d  (line_height=%d)\n",
                   ascent, descent, line_gap, ascent - descent + line_gap);
        }
    } else {
        printf("  SKIP: no font data loaded\n");
    }

    // ── 6. Frame rendering with text ─────────────────────────────────
    printf("\n[06] Render text to framebuffer:\n");
    {
        int fb_w = 320;
        int fb_h = 60;
        int fb_stride = fb_w;
        uint32_t* fb = (uint32_t*)calloc((size_t)(fb_w * fb_h), sizeof(uint32_t));

        /* Create a node with text */
        int64_t node_id = abi_ui_node_create(session_id, "text");
        TEST(node_id > 0, "node created");

        abi_ui_node_set_rect(session_id, node_id, 10, 10, 300, 40);
        abi_ui_node_set_text(session_id, node_id, found_font ? "Hello Kain!" : "No font loaded");
        abi_ui_node_set_style_string(session_id, node_id, "ink_color", "#FF21D4A1");

        if (found_font) {
            abi_ui_node_set_style_i64(session_id, node_id, "font", font_id);
        }

        /* Add a fill so node is visible */
        abi_ui_node_set_style_string(session_id, node_id, "fill_color", "#252540");

        /* Render */
        abi_ui_begin_frame(session_id, 16.0);
        int64_t drawn = ui_render_frame(session, fb, fb_w, fb_h, fb_stride);
        TEST(drawn == fb_w * fb_h, "render_frame returned correct pixel count");

        /* Check that text pixels exist */
        int text_pixels = count_nonzero_pixels(fb, fb_stride, 10, 10, 300, 40);
        TEST(text_pixels > 0, "text rendered non-zero pixels in node region");
        printf("      Non-background pixels in node region: %d\n", text_pixels);

        /* Check that background fill rendered */
        int bg_pixels = count_nonzero_pixels(fb, fb_stride, 0, 0, fb_w, fb_h);
        TEST(bg_pixels > fb_w * fb_h / 2, "background fill covers >50%% of framebuffer");

        abi_ui_end_frame(session_id);
        free(fb);
    }

    // ── 7. Draw command text rendering ───────────────────────────────
    printf("\n[07] Draw command text:\n");
    if (found_font) {
        int fb_w = 200;
        int fb_h = 40;
        int fb_stride = fb_w;
        uint32_t* fb = (uint32_t*)calloc((size_t)(fb_w * fb_h), sizeof(uint32_t));

        /* Create a node for draw command context */
        int64_t cmd_node = abi_ui_node_create(session_id, "dummy");
        abi_ui_node_set_rect(session_id, cmd_node, 0, 0, 200, 40);

        /* Push a text draw command */
        int64_t dc = abi_ui_draw_text(session_id, cmd_node, font_id, 10, 25,
                                       "DrawCmd!", "ink_color");
        TEST(dc > 0, "draw_text command created");

        /* Set the style key color */
        abi_ui_node_set_style_string(session_id, cmd_node, "ink_color", "#4A90D9");

        /* Render */
        abi_ui_begin_frame(session_id, 16.0);
        ui_render_frame(session, fb, fb_w, fb_h, fb_stride);
        int text_pixels = count_nonzero_pixels(fb, fb_stride, 8, 8, 184, 24);
        TEST(text_pixels > 0, "draw command text produced glyph pixels");
        printf("      Draw command text pixels: %d\n", text_pixels);
        abi_ui_end_frame(session_id);
        free(fb);
    } else {
        printf("  SKIP: no font data loaded\n");
    }

    // ── 8. Font resource management ──────────────────────────────────
    printf("\n[08] Resource management:\n");
    {
        int64_t count_before = abi_ui_resource_count(session_id);
        int64_t f2 = abi_ui_font_create(session_id, "second-font", "Arial", 24.0);
        TEST(f2 > 0, "second font resource created");
        int64_t count_after = abi_ui_resource_count(session_id);
        TEST(count_after > count_before, "resource count increased");

        /* Cleanup second font */
        abi_ui_node_destroy(session_id, f2); /* f2 is not a node id, but this will fail silently */
        /* Font resources are cleaned up via session destroy */
    }

    // ── 9. Measure text with UTF-8 ───────────────────────────────────
    printf("\n[09] UTF-8 measurement:\n");
    if (found_font) {
        /* Test ASCII text */
        double ascii_w = abi_ui_text_measure_width(session_id, font_id, "Hello");
        TEST(ascii_w > 0.0, "ASCII text measurable");

        /* Test with newline (should account for line break) */
        double multi_w = abi_ui_text_measure_width(session_id, font_id, "Line1\nLine2");
        TEST(multi_w > 0.0, "multi-line text measurable");

        printf("      'Hello' width: %.1f\n", ascii_w);
        printf("      'Line1\\nLine2' width: %.1f\n", multi_w);
    } else {
        printf("  SKIP: no font data loaded\n");
    }

    // ── Cleanup ──────────────────────────────────────────────────────
    printf("\n[10] Cleanup:\n");
    if (ttf_data) free(ttf_data);
    int64_t dc = abi_ui_session_destroy(session_id);
    TEST(dc == ABI_UI_OK, "session destroyed");
    TEST(abi_ui_find_session(session_id) == NULL, "session freed after destroy");

    // ── Summary ──────────────────────────────────────────────────────
    printf("\n═══════════════════════════════════════════\n");
    printf("  Results: %d / %d passed\n", pass_count, test_count);
    printf("═══════════════════════════════════════════\n");

    return (pass_count == test_count) ? 0 : 1;
}
