#include "kain_native_input_system.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int expect_int(const char* label, long long actual, long long expected) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %lld, got %lld\n", label, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_text(const char* label, const char* actual, const char* expected) {
    if (strcmp(actual ? actual : "", expected) != 0) {
        fprintf(stderr, "%s: expected '%s', got '%s'\n", label, expected, actual ? actual : "");
        return 1;
    }
    return 0;
}

static int expect_float(const char* label, double actual, double expected) {
    if (fabs(actual - expected) > 0.00001) {
        fprintf(stderr, "%s: expected %.4f, got %.4f\n", label, expected, actual);
        return 1;
    }
    return 0;
}

int main(void) {
    int64_t session;
    const char* trace;

    if (expect_int("reset", kain_native_input_reset(), 0)) return 1;
    session = kain_native_input_session_create("native-input-proof");
    if (session <= 0) return 2;
    if (expect_int("session count", kain_native_input_session_count(), 1)) return 3;

    if (expect_int("bind key down", kain_native_input_bind_action(session, "human.keyboard", "key_down", "Enter", "confirm"), 0)) return 4;
    if (expect_int("bind key up", kain_native_input_bind_action(session, "human.keyboard", "key_up", "Enter", "confirm"), 0)) return 5;
    if (expect_int("bind text", kain_native_input_bind_action(session, "cli.stdin", "text", "launch", "confirm"), 0)) return 6;
    if (expect_int("bind axis", kain_native_input_bind_axis(session, "human.pointer", "axis", "look_x", "viewport.look_x", 0.5), 0)) return 7;

    if (kain_native_input_push_event(session, "human.keyboard", "keyboard.primary", "key_down", "Enter", 1.0, "", 1.0) <= 0) return 8;
    if (expect_int("frame 1", kain_native_input_begin_frame(session, 16.0), 1)) return 9;
    if (expect_int("confirm pressed", kain_native_input_action_pressed(session, "confirm"), 1)) return 10;
    if (expect_int("confirm down", kain_native_input_action_down(session, "confirm"), 1)) return 11;

    if (kain_native_input_push_event(session, "human.keyboard", "keyboard.primary", "key_up", "Enter", 0.0, "", 1.0) <= 0) return 12;
    if (expect_int("frame 2", kain_native_input_begin_frame(session, 16.0), 2)) return 13;
    if (expect_int("confirm released", kain_native_input_action_released(session, "confirm"), 1)) return 14;
    if (expect_int("confirm no longer down", kain_native_input_action_down(session, "confirm"), 0)) return 15;

    if (kain_native_input_push_event(session, "human.pointer", "mouse.primary", "axis", "look_x", 4.0, "", 1.0) <= 0) return 16;
    if (kain_native_input_push_event(session, "cli.stdin", "stdin", "text", "launch", 1.0, "launch", 1.0) <= 0) return 17;
    if (expect_int("frame 3", kain_native_input_begin_frame(session, 16.0), 3)) return 18;
    if (expect_float("axis", kain_native_input_axis_value(session, "viewport.look_x"), 2.0)) return 19;
    if (expect_int("text commits", kain_native_input_text_commit_count(session), 1)) return 20;
    if (expect_text("text commit", kain_native_input_text_commit(session, 0), "launch")) return 21;
    if (expect_int("text action", kain_native_input_action_pressed(session, "confirm"), 1)) return 22;

    if (kain_native_input_push_agent_intent(session, "codex", "confirm", "activate focused command", 0.95) <= 0) return 23;
    if (expect_int("frame 4", kain_native_input_begin_frame(session, 16.0), 4)) return 24;
    if (expect_int("agent action", kain_native_input_action_pressed(session, "confirm"), 1)) return 25;
    if (expect_text("agent source", kain_native_input_event_source_kind(session, 0), "agent.intent")) return 26;
    if (expect_text("agent text", kain_native_input_event_text(session, 0), "activate focused command")) return 27;

    trace = kain_native_input_trace_text(session);
    if (!trace || !strstr(trace, "agent.intent")) return 28;

    if (expect_int("destroy", kain_native_input_session_destroy(session), 0)) return 29;
    return 0;
}
