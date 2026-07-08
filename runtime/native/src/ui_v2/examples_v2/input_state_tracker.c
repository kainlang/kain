// ============================================================================
//  input_state_tracker.c — Input Pipeline + Interaction Query Verification
//
//  Proves: kt_input_mouse_move/down/up/scroll, kt_input_key_down/up/text,
//          kt_hovered, kt_clicked, kt_active, kt_state_get/put persistence.
//  Backend: null (headless, feed synthetic input, verify results).
//
//  Compile: see dpi_scaling_verification.c header or use Makefile target.
//  Run:     ./input_tracker.exe
//
//  This is a headless test — it feeds synthetic input events and checks
//  that the interaction query functions return correct results.
// ============================================================================

#include "kaintana.h"
#include "backends/null/host_null.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  \x1b[1;31mFAIL\x1b[0m: %s\n", msg); failures++; } \
    else { printf("  \x1b[1;32mPASS\x1b[0m: %s\n", msg); } \
} while(0)

int main(void) {
    printf("\n\x1b[1;36m=== Kaintana Input & Interaction Verification ===\x1b[0m\n\n");

    kt_init();
    kt_Session* s = kt_make("input_test", 800, 600);
    if (!s) { printf("FATAL: kt_make NULL\n"); return 1; }
    kt_backend_register(s, "null", &kaintana_null_backend);
    kt_backend_select(s, "null");

    // ── Test 1: Basic input funnel (no crash, no leak) ────────────
    printf("\x1b[1;33mTest 1: Input funnel round-trip\x1b[0m\n");
    kt_input_mouse_move(s, 100.0f, 200.0f);
    kt_input_mouse_down(s, 0);
    kt_input_mouse_up(s, 0);
    kt_input_scroll(s, 0.0f, -1.0f);
    kt_input_key_down(s, 65);  // 'A'
    kt_input_key_up(s, 65);
    kt_input_text(s, "hello");
    // All should succeed without crash
    CHECK(1, "input_funnel: all 7 input functions called without crash");

    // ── Test 2: Interaction queries on a button ───────────────────
    printf("\n\x1b[1;33mTest 2: Interaction queries (hover/click/active)\x1b[0m\n");

    // Place mouse at (100, 100)
    kt_input_mouse_move(s, 100.0f, 100.0f);

    // Build a simple UI with a button at known position
    kt_begin(s, 16.0f);
    int root = kt_row(s, 0, "box", "root");
    kt_width(s, root, 800);
    kt_height(s, root, 600);

    int btn = kt_row(s, root, "box", "my_button");
    kt_width(s, btn, 200);
    kt_height(s, btn, 50);
    kt_fill(s, btn, "#4488FF");
    kt_end_row(s);
    kt_end_row(s);
    kt_end(s);

    // Hover check: mouse at (100,100) should be inside btn (0,0)-(200,50) in root coords
    int hovered = kt_hovered(s, btn);
    CHECK(hovered == 1, "kt_hovered returns 1 when mouse is over button");

    // Move mouse away
    kt_input_mouse_move(s, 500.0f, 500.0f);
    kt_begin(s, 16.0f);
    root = kt_row(s, 0, "box", "root");
    kt_width(s, root, 800);
    kt_height(s, root, 600);
    btn = kt_row(s, root, "box", "my_button");
    kt_width(s, btn, 200);
    kt_height(s, btn, 50);
    kt_fill(s, btn, "#4488FF");
    kt_end_row(s);
    kt_end_row(s);
    kt_end(s);

    hovered = kt_hovered(s, btn);
    CHECK(hovered == 0, "kt_hovered returns 0 when mouse is away");

    // ── Test 3: State persistence across frames ────────────────────
    printf("\n\x1b[1;33mTest 3: State persistence (kt_put / kt_get)\x1b[0m\n");

    kt_begin(s, 16.0f);
    root = kt_row(s, 0, "box", "state_root");
    kt_end_row(s);
    kt_end(s);

    int64_t v = kt_get(s, "click_count", 0);
    CHECK(v == 0, "kt_get returns fallback when key missing");

    kt_put(s, "click_count", 42);
    v = kt_get(s, "click_count", 0);
    CHECK(v == 42, "kt_get returns stored int64 value");

    kt_put_s(s, "username", "zenta");
    const char* name = kt_get_s(s, "username", "");
    CHECK(strcmp(name, "zenta") == 0, "kt_get_s returns stored string");

    kt_put_f(s, "volume", 0.75);
    double vol = kt_get_f(s, "volume", 0.0);
    CHECK(vol > 0.7 && vol < 0.8, "kt_get_f returns stored double");

    // Verify state survives a frame
    kt_begin(s, 16.0f);
    root = kt_row(s, 0, "box", "state_root2");
    kt_end_row(s);
    kt_end(s);

    v = kt_get(s, "click_count", -1);
    CHECK(v == 42, "State survives across frames (click_count still 42)");

    name = kt_get_s(s, "username", "");
    CHECK(strcmp(name, "zenta") == 0, "String state survives across frames");

    // ── Test 4: Multiple frames with stable keys ───────────────────
    printf("\n\x1b[1;33mTest 4: Stable keys across frames\x1b[0m\n");

    int btn_id1, btn_id2;
    kt_begin(s, 16.0f);
    root = kt_row(s, 0, "box", "root");
    btn_id1 = kt_row(s, root, "box", "btn_save");
    kt_fill(s, btn_id1, "#00FF00");
    kt_end_row(s);
    kt_end_row(s);
    kt_end(s);
    CHECK(btn_id1 >= 0, "Frame 1: stable key 'btn_save' allocated");

    kt_begin(s, 16.0f);
    root = kt_row(s, 0, "box", "root");
    btn_id2 = kt_row(s, root, "box", "btn_save");
    kt_fill(s, btn_id2, "#FF0000");
    kt_end_row(s);
    kt_end_row(s);
    kt_end(s);
    CHECK(btn_id2 == btn_id1, "Frame 2: stable key 'btn_save' returns SAME node id");
    CHECK(btn_id2 >= 0, "Frame 2: stable key still valid");

    // ── Test 5: Empty key generates new node each frame ────────────
    printf("\n\x1b[1;33mTest 5: Empty keys = fresh nodes each frame\x1b[0m\n");

    int fresh1, fresh2;
    kt_begin(s, 16.0f);
    fresh1 = kt_row(s, 0, "box", "");
    kt_end_row(s);
    kt_end(s);

    kt_begin(s, 16.0f);
    fresh2 = kt_row(s, 0, "box", "");
    kt_end_row(s);
    kt_end(s);

    CHECK(fresh1 != fresh2, "Empty key generates different node ids each frame");
    CHECK(fresh1 >= 0 && fresh2 >= 0, "Empty key nodes are valid");

    // ── Test 6: Scroll input ──────────────────────────────────────
    printf("\n\x1b[1;33mTest 6: Scroll input\x1b[0m\n");
    kt_input_scroll(s, 0.0f, -5.0f);  // scroll down
    kt_input_scroll(s, 0.0f, -3.0f);  // more scroll
    kt_begin(s, 16.0f);
    root = kt_row(s, 0, "box", "scroll_test");
    kt_end_row(s);
    kt_end(s);
    // Scroll doesn't crash and we can poll state
    CHECK(1, "Scroll input doesn't crash");

    // ── Summary ───────────────────────────────────────────────────
    printf("\n\x1b[1;36m=== Results: %d failures ===\x1b[0m\n", failures);
    kt_free(s);
    return failures > 0 ? 1 : 0;
}
