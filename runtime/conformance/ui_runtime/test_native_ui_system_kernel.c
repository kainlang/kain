#include "../../native/include/ui_system.h"

#include <stdio.h>
#include <string.h>

static int g_failed = 0;

static int test_fail(const char* message) {
    fprintf(stderr, "[FAIL] %s\n", message);
    fflush(stderr);
    g_failed = 1;
    return 0;
}

static int test_true(int condition, const char* message) {
    if (!condition) {
        return test_fail(message);
    }
    return 1;
}

int main(void) {
    int64_t session;
    int64_t root;
    int64_t command;
    int64_t viewport;
    int64_t font;
    int64_t draw_count;
    int64_t hit;

    if (!test_true(abi_ui_reset() == ABI_UI_OK, "reset should succeed")) {
        return 1;
    }

    session = abi_ui_session_create("raw-native-ui-test", 1280, 720);
    if (!test_true(session > 0, "session handle should be positive")) {
        return 1;
    }
    if (!test_true(abi_ui_window_open(session, "Raw Native UI", 1280, 720) == ABI_UI_OK, "window open should succeed")) {
        return 1;
    }

    root = abi_ui_node_create(session, "user.system.root");
    command = abi_ui_node_create(session, "user.made.command");
    viewport = abi_ui_node_create(session, "user.made.viewport-without-catalog");
    if (!test_true(root > 0 && command > 0 && viewport > 0, "arbitrary authored node kinds should be accepted")) {
        return 1;
    }
    if (!test_true(abi_ui_node_count(session) == 3, "node count should track authored nodes")) {
        return 1;
    }

    if (!test_true(abi_ui_node_set_parent(session, command, root) == ABI_UI_OK, "command parent should attach")) {
        return 1;
    }
    if (!test_true(abi_ui_node_set_parent(session, viewport, root) == ABI_UI_OK, "viewport parent should attach")) {
        return 1;
    }
    if (!test_true(abi_ui_node_child_count(session, root) == 2, "root should report two children")) {
        return 1;
    }

    abi_ui_node_set_rect(session, root, 0.0, 0.0, 1280.0, 720.0);
    abi_ui_node_set_rect(session, command, 24.0, 24.0, 220.0, 48.0);
    abi_ui_node_set_rect(session, viewport, 24.0, 96.0, 900.0, 540.0);
    abi_ui_node_set_text(session, command, "Launch");
    font = abi_ui_font_create(session, "font.body", "Inter", 14.0);
    abi_ui_node_set_style_string(session, command, "fill", "#21d4a1");
    abi_ui_node_set_style_i64(session, command, "layer", 7);
    abi_ui_node_set_style_f64(session, viewport, "density", 1.5);

    if (!test_true(strcmp(abi_ui_node_kind(session, command), "user.made.command") == 0, "node kind should stay authored text")) {
        return 1;
    }
    if (!test_true(strcmp(abi_ui_node_text(session, command), "Launch") == 0, "node text should round trip")) {
        return 1;
    }
    if (!test_true(strcmp(abi_ui_node_style_string(session, command, "fill", "none"), "#21d4a1") == 0, "string style should round trip")) {
        return 1;
    }
    if (!test_true(abi_ui_node_style_i64(session, command, "layer", 0) == 7, "i64 style should round trip")) {
        return 1;
    }
    if (!test_true(abi_ui_node_style_f64(session, viewport, "density", 0.0) > 1.4, "f64 style should round trip")) {
        return 1;
    }
    if (!test_true(abi_ui_node_set_state_i64(session, command, "counter", 3) == ABI_UI_OK, "i64 state should write")) {
        return 1;
    }
    if (!test_true(abi_ui_node_set_state_f64(session, command, "spin", 1.25) == ABI_UI_OK, "f64 state should write")) {
        return 1;
    }
    if (!test_true(abi_ui_node_set_state_string(session, command, "shape.kind", "tetra.surface") == ABI_UI_OK, "string state should write")) {
        return 1;
    }
    if (!test_true(abi_ui_node_state_i64(session, command, "counter", 0) == 3, "i64 state should round trip")) {
        return 1;
    }
    if (!test_true(abi_ui_node_state_f64(session, command, "spin", 0.0) > 1.2, "f64 state should round trip")) {
        return 1;
    }
    if (!test_true(strcmp(abi_ui_node_state_string(session, command, "shape.kind", "rect"), "tetra.surface") == 0, "string state should round trip")) {
        return 1;
    }
    if (!test_true(abi_ui_node_state_i64(session, command, "missing", 42) == 42, "missing state should return fallback")) {
        return 1;
    }
    if (!test_true(abi_ui_state_count(session) == 3, "state count should track generic authored cells")) {
        return 1;
    }

    hit = abi_ui_hit_test(session, 32.0, 32.0);
    if (!test_true(hit == command, "hit test should return topmost authored rect")) {
        return 1;
    }
    if (!test_true(abi_ui_focus(session, command) == ABI_UI_OK, "focus should accept authored command node")) {
        return 1;
    }
    if (!test_true(abi_ui_focused_node(session) == command, "focused node should round trip")) {
        return 1;
    }
    abi_ui_node_set_flag(session, command, "hovered", 1);
    abi_ui_node_set_flag(session, command, "pressed", 1);
    if (!test_true(abi_ui_node_has_flag(session, command, "hovered") == 1, "hovered flag should round trip")) {
        return 1;
    }
    if (!test_true(abi_ui_node_has_flag(session, command, "pressed") == 1, "pressed flag should round trip")) {
        return 1;
    }

    abi_ui_begin_frame(session, 16.0);
    abi_ui_draw_rect(session, command, 24.0, 24.0, 220.0, 48.0, "fill");
    abi_ui_draw_text(session, command, font, 36.0, 52.0, "Launch", "label");
    draw_count = abi_ui_draw_command_count(session);
    if (!test_true(draw_count == 2, "draw command buffer should collect authored commands")) {
        return 1;
    }
    if (!test_true(strcmp(abi_ui_draw_command_kind(session, 0), "rect") == 0, "first draw command should be rect")) {
        return 1;
    }
    if (!test_true(abi_ui_draw_command_node(session, 1) == command, "second draw command should target command node")) {
        return 1;
    }
    if (!test_true(abi_ui_draw_command_font(session, 1) == font, "text draw command should keep authored font handle")) {
        return 1;
    }

    abi_ui_push_event(session, "pointer.down", command, 32.0, 32.0, 0, "primary");
    if (!test_true(abi_ui_poll_event(session) == 1, "event queue should poll one event")) {
        return 1;
    }
    if (!test_true(strcmp(abi_ui_event_kind(session), "pointer.down") == 0, "event kind should round trip")) {
        return 1;
    }
    if (!test_true(abi_ui_event_target(session) == command, "event target should round trip")) {
        return 1;
    }

    if (!test_true(abi_ui_present(session) == 1, "present should publish the first frame")) {
        return 1;
    }
    if (!test_true(abi_ui_last_presented_frame(session) == 1, "presented frame should round trip")) {
        return 1;
    }

    printf("[PASS] native ui raw kernel smoke\n");
    return g_failed ? 1 : 0;
}
