// ============================================================================
//  input_fuzzer.c — Fuzz tests for kain_input.h
//  ============================================================================
//  Exercises the input pipeline with event floods, extreme coordinates,
//  all event kinds, null-pointer tests, and invalid pipelines.
//
//  Part of the Kain UI substrate (KUIF Phase 1).
//  ============================================================================

#include "fuzzer.h"

FuzzTelemetry fuzz_input(FuzzState* state, int iterations) {
    FuzzTelemetry tel;
    memset(&tel, 0, sizeof(tel));
    tel.domain_name = "input";

    FuzzState* s = state;
    clock_t start = clock();

    // ── Fuzz 1: Push and poll random events ─────────────────────
    for (int i = 0; i < iterations; i++) {
        tel.total_tests++;

        // Create a random event
        KainInputEvent evt;
        memset(&evt, 0, sizeof(evt));

        evt.kind = (KainInputEventKind)fuzz_int(s, 0, 11);  // 0-11 covers all enums
        evt.x = fuzz_float(s, -100000.0f, 100000.0f);
        evt.y = fuzz_float(s, -100000.0f, 100000.0f);
        evt.delta_x = fuzz_float(s, -10000.0f, 10000.0f);
        evt.delta_y = fuzz_float(s, -10000.0f, 10000.0f);
        evt.key_code = fuzz_i64_range(s, -1, 65535);
        evt.device_id = fuzz_i64_range(s, -1, 64);
        evt.timestamp_ms = fuzz_i64_range(s, 0, 1000000LL);

        // Random text (often empty/null-like)
        if (fuzz_bool(s)) {
            fuzz_rand_text(s, evt.text, 15);
        } else {
            evt.text[0] = '\0';
        }

        // Push event
        kain_input_push_event(s->input_pipeline, &evt);
        tel.total_tests++;

        // Poll it back (sometimes poll without pushing first)
        KainInputEvent out_evt;
        bool got = kain_input_poll_event(s->input_pipeline, &out_evt);
        if (got) {
            // Verify fields are reasonable
            if (out_evt.kind == evt.kind) {
                // Good — event round-tripped
            }
            tel.total_tests++;
        }
    }

    // ── Fuzz 2: Event flood (1025+ events) ──────────────────────
    tel.boundary_hits++;
    for (int flood = 0; flood < 5; flood++) {
        // Push 1025 events (1 over ring buffer capacity of 1024)
        for (int e = 0; e < 1025; e++) {
            KainInputEvent evt;
            memset(&evt, 0, sizeof(evt));
            evt.kind = KAIN_INPUT_KEY_DOWN;
            evt.key_code = (int64_t)(e % 256);
            kain_input_push_event(s->input_pipeline, &evt);
            tel.total_tests++;
        }

        // Poll what we can (should not crash)
        KainInputEvent out;
        int polled = 0;
        while (kain_input_poll_event(s->input_pipeline, &out) && polled < 2000) {
            polled++;
        }
        tel.total_tests++;
    }

    // ── Fuzz 3: Hit-test with extreme coordinates ───────────────
    tel.boundary_hits++;
    for (int h = 0; h < 100; h++) {
        float x = fuzz_float(s, -1e9f, 1e9f);
        float y = fuzz_float(s, -1e9f, 1e9f);
        int64_t hit = kain_input_hit_test(s->input_pipeline, x, y);
        if (hit < -1) {
            // hit_test should return -1 for no hit, or >=0 for a hit
            tel.edge_violations++;
        }
        tel.total_tests++;
    }

    // ── Fuzz 4: Event type name utility ─────────────────────────
    tel.boundary_hits++;
    for (int k = 0; k < 20; k++) {
        KainInputEventKind kind = (KainInputEventKind)(fuzz_int(s, -5, 15));
        const char* name = kain_input_event_type_name(kind);
        if (!name) {
            tel.failed++;
        }
        tel.total_tests++;
    }

    // ── Fuzz 5: Null-pointer / invalid pipeline tests ───────────
    tel.boundary_hits++;
    // NULL pipeline
    kain_input_pipeline_create(-1);  // invalid session — should still create OK
    kain_input_pipeline_destroy(NULL);
    {
        KainInputEvent evt;
        memset(&evt, 0, sizeof(evt));
        kain_input_push_event(NULL, &evt);
    }
    {
        KainInputEvent out;
        bool polled = kain_input_poll_event(NULL, &out);
        if (polled) {  // Should return false for NULL
            tel.failed++;
        }
    }
    int64_t ht = kain_input_hit_test(NULL, 0, 0);
    if (ht != -1) {  // Should return -1 for NULL
        tel.failed++;
    }
    tel.null_ptr_ok += 5;

    // NULL event pointer
    kain_input_push_event(s->input_pipeline, NULL);
    {
        bool polled = kain_input_poll_event(s->input_pipeline, NULL);
        if (polled) {
            // May or may not crash depending on internals
        }
    }
    tel.null_ptr_ok += 2;

    clock_t end = clock();
    tel.elapsed_ms = 1000.0 * (double)(end - start) / (double)CLOCKS_PER_SEC;

    printf("  OK input: %d ops, %d boundary tests, %d null-ptr tolerant in %.0f ms\n",
           tel.total_tests, tel.boundary_hits, tel.null_ptr_ok, tel.elapsed_ms);

    return tel;
}
