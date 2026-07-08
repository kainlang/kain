// ============================================================================
//  font_fuzzer.c — Fuzz tests for kain_font.h
//  ============================================================================
//  Exercises TTF loading from corrupt data, glyph access with extreme
//  codepoints, text measurement with edge inputs, and all NULL paths.
//
//  Part of the Kain UI substrate (KUIF Phase 1).
//  ============================================================================

#include "fuzzer.h"

// ── Corrupt TTF data generators ───────────────────────────────────

// Generate N bytes of pseudo-random data for corrupt TTF testing
static void fill_random_bytes(FuzzState* s, uint8_t* buf, int len) {
    for (int i = 0; i < len; i++) {
        buf[i] = (uint8_t)fuzz_rand(s);
    }
}

// A minimal valid TTF header that stb_truetype might accept
// (not fully valid — tests partial parsing)
static const uint8_t g_minimal_ttf_header[] = {
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // sfVersion + numTables
    0x00, 0x00, 0x00, 0x00,                           // searchRange, entrySelector, rangeShift
    // Minimal 'cmap' table header
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// ── Font fuzzer ───────────────────────────────────────────────────

FuzzTelemetry fuzz_font(FuzzState* state, int iterations) {
    FuzzTelemetry tel;
    memset(&tel, 0, sizeof(tel));
    tel.domain_name = "font";

    FuzzState* s = state;
    clock_t start = clock();

    // ── Fuzz 1: Load with corrupt TTF data ──────────────────────
    for (int i = 0; i < iterations; i++) {
        tel.total_tests++;

        // Generate various TTF-like data
        int data_len;
        uint8_t* data = NULL;
        bool use_random = (i % 5) < 4;  // 80% random, 20% minimal header

        if (use_random) {
            data_len = fuzz_int(s, 0, 4096);
            if (data_len > 0) {
                data = (uint8_t*)malloc((size_t)data_len);
                if (data) fill_random_bytes(s, data, data_len);
            }
        } else {
            data_len = (int)sizeof(g_minimal_ttf_header);
            data = (uint8_t*)malloc((size_t)data_len);
            if (data) memcpy(data, g_minimal_ttf_header, (size_t)data_len);
        }

        if (!data && data_len > 0) {
            tel.failed++;
            continue;
        }

        // Try loading
        float size = fuzz_float(s, -100.0f, 1024.0f);
        int64_t session_id = fuzz_i64_range(s, 0, 16);
        int64_t font_id = kain_font_load(session_id, data, (int64_t)data_len, size);

        // The load should fail gracefully (return 0) for corrupt data
        // If it returns > 0, that means stb_truetype parsed a valid font
        // from random bytes, which is extremely unlikely but not a crash.
        // The important thing is it didn't crash.
        if (font_id < 0) {
            tel.failed++;
        }

        free(data);
        tel.passed++;
    }

    // ── Fuzz 2: Font measurement with edge inputs ──────────────
    tel.boundary_hits++;
    for (int m = 0; m < 1000; m++) {
        tel.total_tests++;

        int64_t sess = fuzz_i64_range(s, 0, 16);
        int64_t font_id = fuzz_i64_range(s, -1, 100);

        // Random text for measurement
        char text[128];
        fuzz_rand_text(s, text, 128);

        // Measure width
        float width = kain_font_measure_text(sess, font_id, text);
        if (width < 0.0f || isnan(width) || isinf(width)) {
            tel.edge_violations++;
        }
        tel.total_tests++;
        tel.passed++;

        // Measure height
        float height = kain_font_line_height(sess, font_id);
        if (height < 0.0f || isnan(height) || isinf(height)) {
            tel.edge_violations++;
        }
        tel.total_tests++;
        tel.passed++;

        // Get metrics
        KainFontMetrics metrics = kain_font_get_metrics(sess, font_id);
        (void)metrics;
        tel.total_tests++;
        tel.passed++;
    }

    // ── Fuzz 3: Glyph access with extreme codepoints ───────────
    tel.boundary_hits++;
    for (int g = 0; g < 1000; g++) {
        tel.total_tests++;

        int64_t sess = fuzz_i64_range(s, 0, 16);
        int64_t font_id = fuzz_i64_range(s, -1, 100);
        int codepoint = (int)fuzz_i64_range(s, -1000, 0x110000);

        void* glyph = kain_font_get_glyph(sess, font_id, codepoint);
        if (glyph) {
            kain_font_release_glyph(glyph);
        }
        tel.total_tests++;
        tel.passed++;
    }

    // ── Fuzz 4: Load with extreme parameters ───────────────────
    tel.boundary_hits++;
    uint8_t dummy_data[256];
    memset(dummy_data, 0x42, sizeof(dummy_data));

    // Null data, non-zero length
    int64_t fid1 = kain_font_load(1, NULL, 100, 16.0f);
    if (fid1 != 0) { tel.edge_violations++; }
    tel.null_ptr_ok++;

    // Valid data, zero length
    int64_t fid2 = kain_font_load(1, dummy_data, 0, 16.0f);
    if (fid2 != 0) { tel.edge_violations++; }

    // Negative TTF length
    int64_t fid3 = kain_font_load(1, dummy_data, -1, 16.0f);
    if (fid3 != 0) { tel.edge_violations++; }

    // Huge TTF length (over 64MB limit)
    int64_t fid4 = kain_font_load(1, dummy_data, 67108865LL, 16.0f);
    if (fid4 != 0) { tel.edge_violations++; }

    tel.total_tests += 4;

    // ── Fuzz 5: Load path with edge paths ─────────────────────
    tel.boundary_hits++;
    int64_t fid5 = kain_font_load_path(1, NULL, 16.0f);
    if (fid5 != 0) { tel.edge_violations++; }
    tel.null_ptr_ok++;

    int64_t fid6 = kain_font_load_path(1, "", 16.0f);
    if (fid6 != 0) { tel.edge_violations++; }

    int64_t fid7 = kain_font_load_path(1, "C:/nonexistent_font_file.ttf.test", 16.0f);
    if (fid7 != 0) { tel.edge_violations++; }

    int64_t fid8 = kain_font_load_path(1, "/dev/null", 16.0f);
    if (fid8 != 0) { tel.edge_violations++; }

    tel.total_tests += 4;

    // ── Fuzz 6: Load default (tests platform probing) ─────────
    tel.boundary_hits++;
    int64_t default_font = kain_font_load_default(1, 16.0f);
    if (default_font > 0) {
        // Platform has a default font — test measurement with it
        float w = kain_font_measure_text(1, default_font, "Hello World! 123 ABC");
        if (w <= 0.0f) { /* empty measurement — could be valid if no glyph data */ }
        tel.total_tests++;
        tel.passed++;
    }

    // Load default with edge session
    int64_t df2 = kain_font_load_default(9999, 16.0f);  // invalid session
    (void)df2;
    int64_t df3 = kain_font_load_default(-1, -16.0f);   // invalid session + negative size
    (void)df3;
    tel.total_tests += 2;

    // ── Fuzz 7: NULL text measurement ──────────────────────────
    tel.boundary_hits++;
    float mw = kain_font_measure_text(1, 1, NULL);
    if (mw != 0.0f) { tel.edge_violations++; }
    tel.null_ptr_ok++;

    float mw2 = kain_font_measure_text(1, 1, "");
    if (mw2 != 0.0f) { tel.edge_violations++; }

    float lh = kain_font_line_height(1, 1);
    if (lh < 0.0f) { tel.edge_violations++; }
    tel.total_tests += 3;

    // ── Fuzz 8: Release NULL glyph ───────────────────────────
    kain_font_release_glyph(NULL);
    tel.null_ptr_ok++;
    tel.total_tests++;

    clock_t end = clock();
    tel.elapsed_ms = 1000.0 * (double)(end - start) / (double)CLOCKS_PER_SEC;

    printf("  OK font: %d ops, %d boundary tests, %d null-ptr tolerant in %.0f ms\n",
           tel.total_tests, tel.boundary_hits, tel.null_ptr_ok, tel.elapsed_ms);

    return tel;
}
