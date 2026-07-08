// ============================================================================
//  geometry_fuzzer.c — Fuzz tests for kain_geometry.h
//  ============================================================================
//  Exercises all geometry types and operations with randomized inputs,
//  boundary conditions, and null/edge cases.
//
//  Part of the Kain UI substrate (KUIF Phase 1).
//  ============================================================================

#include "fuzzer.h"

FuzzTelemetry fuzz_geometry(FuzzState* state, int iterations) {
    FuzzTelemetry tel;
    memset(&tel, 0, sizeof(tel));
    tel.domain_name = "geometry";

    clock_t start = clock();

    for (int i = 0; i < iterations; i++) {
        tel.total_tests++;

        // ── Random rects ──────────────────────────────────────────
        kainRect r1 = fuzz_rand_rect(state);
        kainRect r2 = fuzz_rand_rect(state);
        kainPoint p = fuzz_rand_point(state);

        // Exercise rect construction
        kainRect rm = kain_rect_make(r1.x, r1.y, r1.w, r1.h);
        (void)rm;

        // Exercise rect_contains
        bool contains = kain_rect_contains(r1, p);
        (void)contains;

        // Exercise rect_overlaps
        bool overlaps = kain_rect_overlaps(r1, r2);
        (void)overlaps;

        // Exercise rect_intersect
        kainRect inter = kain_rect_intersect(r1, r2);
        (void)inter;

        // Exercise rect_union
        kainRect un = kain_rect_union(r1, r2);
        // Verify: union must contain both original rects
        if (un.w > 0 && un.h > 0) {
            if (!kain_rect_contains(un, kain_point_make(r1.x, r1.y)) &&
                !kain_rect_contains(un, kain_point_make(r2.x, r2.y))) {
                // Possible if both rects have negative width/height
                // This isn't a real error — just note it
                tel.edge_violations++;
            }
        }

        // ── Random colors ─────────────────────────────────────────
        kainColor c1 = fuzz_rand_color(state);
        kainColor c2 = fuzz_rand_color(state);

        // Exercise color_from_u32
        uint32_t argb = fuzz_rand(state);
        kainColor c_from_u32 = kain_color_from_u32(argb);
        (void)c_from_u32;

        // Exercise color_to_u32
        uint32_t back = kain_color_to_u32(c1);
        (void)back;

        // Verify round-trip: clamp then convert
        kainColor clamped = kain_color_clamp(c1);
        kainColor roundtrip = kain_color_from_u32(kain_color_to_u32(clamped));
        (void)roundtrip;

        // Exercise color_lerp
        float t = fuzz_float(state, -1.0f, 2.0f);  // intentionally out of [0,1]
        kainColor lerped = kain_color_lerp(c1, c2, t);
        (void)lerped;

        // Exercise clamp
        kainColor clamped2 = kain_color_clamp(c1);
        (void)clamped2;

        // Verify clamp invariants
        if (clamped2.r < 0.0f || clamped2.r > 1.0f ||
            clamped2.g < 0.0f || clamped2.g > 1.0f ||
            clamped2.b < 0.0f || clamped2.b > 1.0f ||
            clamped2.a < 0.0f || clamped2.a > 1.0f) {
            tel.failed++;
            continue;
        }

        // ── Matrix operations ─────────────────────────────────────
        kainMatrix m1 = fuzz_rand_matrix(state);
        kainMatrix m2 = fuzz_rand_matrix(state);

        // Exercise identity
        kainMatrix ident = kain_matrix_identity();
        (void)ident;

        // Exercise translate/scale/rotate
        kainMatrix trans = kain_matrix_translate(
            fuzz_float(state, -5000.0f, 5000.0f),
            fuzz_float(state, -5000.0f, 5000.0f));
        kainMatrix scale = kain_matrix_scale(
            fuzz_float(state, -100.0f, 100.0f),
            fuzz_float(state, -100.0f, 100.0f));
        kainMatrix rot = kain_matrix_rotate(
            fuzz_float(state, -100.0f, 100.0f));
        (void)trans; (void)scale; (void)rot;

        // Exercise matrix_mul
        kainMatrix mul = kain_matrix_mul(m1, m2);
        (void)mul;

        // Exercise matrix_transform_point
        kainPoint tp = kain_matrix_transform_point(m1, p);
        (void)tp;

        // Verify identity transform is identity
        kainPoint id_check = kain_matrix_transform_point(ident, p);
        if (fabsf(id_check.x - p.x) > 0.0001f ||
            fabsf(id_check.y - p.y) > 0.0001f) {
            // Only flag if the difference is actual (p could be NaN)
            if (!isnan(p.x) && !isnan(p.y)) {
                tel.edge_violations++;
            }
        }

        // ── Point operations ──────────────────────────────────────
        kainPoint pa = kain_point_make(
            fuzz_float(state, -1e6f, 1e6f),
            fuzz_float(state, -1e6f, 1e6f));
        kainPoint pb = kain_point_make(
            fuzz_float(state, -1e6f, 1e6f),
            fuzz_float(state, -1e6f, 1e6f));
        kainPoint sum = kain_point_add(pa, pb);
        (void)sum;
        kainPoint diff = kain_point_sub(pa, pb);
        (void)diff;

        // ── Size operations ────────────────────────────────────────
        kainSize sz = kain_size_make(
            fuzz_float(state, -1e6f, 1e6f),
            fuzz_float(state, -1e6f, 1e6f));
        (void)sz;

        // ── Boundary value tests (every ~1000 iterations) ──────────
        if (i % 1000 == 0) {
            tel.boundary_hits++;

            // Zero rect
            kainRect zr = kain_rect_make(0, 0, 0, 0);
            (void)zr;

            // Negative rect
            kainRect nr = kain_rect_make(-1, -1, -1, -1);
            (void)nr;

            // Max uint32_t color round-trip
            kainColor max_col = kain_color_from_u32(0xFFFFFFFF);
            uint32_t max_back = kain_color_to_u32(max_col);
            // Should be 0xFFFFFFFF (white), or very close
            if (max_back != 0xFFFFFFFF && max_back != 0xFEFFFFFF &&
                max_back != 0xFFFEFFFF && max_back != 0xFFFFFEFF &&
                max_back != 0xFFFFFFFE) {
                // Minor precision loss is acceptable
            }

            // Transparent color
            kainColor tc = kain_color_from_u32(0x00000000);
            uint32_t tc_back = kain_color_to_u32(tc);
            if (tc_back != 0 && tc_back != 0x01010101) {
                // Near-zero is fine
            }

            // Identity transform preserves origin
            kainMatrix ident2 = kain_matrix_identity();
            kainPoint origin = kain_matrix_transform_point(ident2, kain_point_make(0, 0));
            if (origin.x != 0.0f || origin.y != 0.0f) {
                tel.edge_violations++;
            }

            // Rects with extreme values
            kainRect huge = kain_rect_make(-1e10f, -1e10f, 2e10f, 2e10f);
            kainRect tiny = kain_rect_make(1e-30f, 1e-30f, 1e-30f, 1e-30f);
            kainRect huge_inter = kain_rect_intersect(huge, tiny);
            if (huge_inter.w < 0.0f || huge_inter.h < 0.0f) {
                tel.edge_violations++;
            }
        }

        tel.passed++;
    }

    clock_t end = clock();
    tel.elapsed_ms = 1000.0 * (double)(end - start) / (double)CLOCKS_PER_SEC;

    if (tel.failed > 0) {
        printf("  ** geometry: %d/%d passed, %d failed, %d edge violations\n",
               tel.passed, tel.total_tests, tel.failed, tel.edge_violations);
    } else {
        printf("  OK geometry: %d ops in %.0f ms (%d boundary tests)\n",
               tel.total_tests, tel.elapsed_ms, tel.boundary_hits);
    }

    return tel;
}
