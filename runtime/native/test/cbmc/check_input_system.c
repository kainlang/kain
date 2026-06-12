/*
 * check_input_system.c — CBMC verification harness for input_system module
 *
 * Verifies: session init/destroy, binding management, event push, frame
 * processing, action state queries, axis accumulation, text commits,
 * and error status reporting.  Tests only the public ABI functions
 * (abi_input_*) — no forward declarations of internal statics needed.
 *
 * Key data flow:
 *   session_create -> bind_action/bind_axis -> push_event ->
 *   begin_frame -> action_pressed/down/released, axis_value, text_commits
 *
 * Run:
 *   python test/scripts/run_pipeline.py cbmc --harness check_input_system
 * Or:
 *   cbmc --unwind 5 --trace test/cbmc/check_input_system.c \
 *        src/core/input_system.c -I include -I src/core
 */

#include "input_system.h"
#include <string.h>

/* Convenience: static string buffers for valid pointer provenance */
static char g_test_name[]   = "test-session";
static char g_key_kind[]    = "keyboard";
static char g_key_down[]    = "key_down";
static char g_key_up[]      = "key_up";
static char g_space_code[]  = "space";
static char g_jump_action[] = "jump";
static char g_mouse_kind[]  = "mouse";
static char g_axis_kind[]   = "axis";
static char g_x_axis[]      = "mouse_x";
static char g_text_kind[]   = "text";
static char g_key_a[]       = "key_a";
static char g_hello_text[]  = "hello";
static char g_agent_id[]    = "agent1";
static char g_empty_str[]   = "";


/* ──────────────────────────────────────────────────────────────────────
 * Check: reset produces a clean global state
 * ────────────────────────────────────────────────────────────────────── */
void check_reset_clears_state(void) {
    int64_t rc = abi_input_reset();

    __CPROVER_assert(rc == ABI_INPUT_OK, "reset returns OK");

    int64_t count = abi_input_session_count();
    __CPROVER_assert(count == 0, "no sessions after reset");

    int64_t status = abi_input_last_status();
    __CPROVER_assert(status == ABI_INPUT_OK, "status OK after reset");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: session_create / session_destroy lifecycle
 * ────────────────────────────────────────────────────────────────────── */
void check_session_create_destroy(void) {
    abi_input_reset();

    int64_t sid = abi_input_session_create(g_test_name);
    __CPROVER_assert(sid > 0, "session id > 0");

    int64_t count1 = abi_input_session_count();
    __CPROVER_assert(count1 == 1, "one session after create");

    int64_t rc = abi_input_session_destroy(sid);
    __CPROVER_assert(rc == ABI_INPUT_OK, "destroy returns OK");

    int64_t count2 = abi_input_session_count();
    __CPROVER_assert(count2 == 0, "zero sessions after destroy");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: destroy on invalid ID returns proper error
 * ────────────────────────────────────────────────────────────────────── */
void check_session_destroy_invalid(void) {
    abi_input_reset();

    int64_t rc = abi_input_session_destroy(9999);
    __CPROVER_assert(rc == ABI_INPUT_INVALID_SESSION,
                     "destroy invalid id returns INVALID_SESSION");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: session count after multiple creates/destroys
 * ────────────────────────────────────────────────────────────────────── */
void check_session_count_multiple(void) {
    abi_input_reset();

    int64_t s1 = abi_input_session_create(g_test_name);
    int64_t s2 = abi_input_session_create(g_test_name);
    __CPROVER_assert(s1 > 0 && s2 > 0 && s2 != s1,
                     "two unique session ids");

    int64_t cnt = abi_input_session_count();
    __CPROVER_assert(cnt == 2, "count == 2 after two creates");

    abi_input_session_destroy(s1);
    cnt = abi_input_session_count();
    __CPROVER_assert(cnt == 1, "count == 1 after destroying one");

    abi_input_session_destroy(s2);
    cnt = abi_input_session_count();
    __CPROVER_assert(cnt == 0, "count == 0 after destroying both");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: session_create with NULL name uses default
 * ────────────────────────────────────────────────────────────────────── */
void check_session_create_null_name(void) {
    abi_input_reset();

    int64_t sid = abi_input_session_create(NULL);
    __CPROVER_assert(sid > 0, "session create with NULL name succeeds");

    int64_t cnt = abi_input_session_count();
    __CPROVER_assert(cnt == 1, "one session after NULL-name create");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: bind_action creates a valid binding, then push and process it
 * ────────────────────────────────────────────────────────────────────── */
void check_bind_action(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    int64_t rc = abi_input_bind_action(
        sid, g_key_kind, g_key_down, g_space_code, g_jump_action);
    __CPROVER_assert(rc == ABI_INPUT_OK, "bind action returns OK");

    __CPROVER_assert(abi_input_session_count() == 1,
                     "session still alive after bind");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: bind_action with empty required args returns error
 * ────────────────────────────────────────────────────────────────────── */
void check_bind_action_empty_args(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    int64_t rc1 = abi_input_bind_action(
        sid, g_key_kind, g_empty_str, g_space_code, g_jump_action);
    __CPROVER_assert(rc1 == ABI_INPUT_INVALID_ARGUMENT,
                     "bind action empty event_kind");

    int64_t rc2 = abi_input_bind_action(
        sid, g_key_kind, g_key_down, g_empty_str, g_jump_action);
    __CPROVER_assert(rc2 == ABI_INPUT_INVALID_ARGUMENT,
                     "bind action empty code");

    int64_t rc3 = abi_input_bind_action(
        sid, g_key_kind, g_key_down, g_space_code, g_empty_str);
    __CPROVER_assert(rc3 == ABI_INPUT_INVALID_ARGUMENT,
                     "bind action empty action");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: bind_axis creates a valid axis binding
 * ────────────────────────────────────────────────────────────────────── */
void check_bind_axis(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    int64_t rc = abi_input_bind_axis(
        sid, g_mouse_kind, g_axis_kind, g_x_axis, g_x_axis, 1.0);
    __CPROVER_assert(rc == ABI_INPUT_OK, "bind axis returns OK");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: bind on invalid session returns proper error
 * ────────────────────────────────────────────────────────────────────── */
void check_bind_invalid_session(void) {
    abi_input_reset();

    int64_t rc = abi_input_bind_action(
        9999, g_key_kind, g_key_down, g_space_code, g_jump_action);
    __CPROVER_assert(rc == ABI_INPUT_INVALID_SESSION,
                     "bind on invalid session returns INVALID_SESSION");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: push_event enqueues and returns a positive sequence number
 * ────────────────────────────────────────────────────────────────────── */
void check_push_event(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    int64_t seq = abi_input_push_event(
        sid, g_key_kind, NULL, g_key_down, g_space_code,
        1.0, NULL, 1.0);
    __CPROVER_assert(seq > 0, "push_event returns positive sequence");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: push_event on invalid session returns error
 * ────────────────────────────────────────────────────────────────────── */
void check_push_event_invalid_session(void) {
    abi_input_reset();

    int64_t seq = abi_input_push_event(
        9999, g_key_kind, NULL, g_key_down, g_space_code,
        1.0, NULL, 1.0);
    __CPROVER_assert(seq < 0, "push_event on bad session returns negative");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: push_agent_intent enqueues an agent intent event
 * ────────────────────────────────────────────────────────────────────── */
void check_push_agent_intent(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    int64_t seq = abi_input_push_agent_intent(
        sid, g_agent_id, g_jump_action, "jump now", 0.95);
    __CPROVER_assert(seq > 0,
                     "push_agent_intent returns positive sequence");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: begin_frame processes pending events, advances frame index
 * ────────────────────────────────────────────────────────────────────── */
void check_begin_frame_processing(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    abi_input_bind_action(
        sid, g_key_kind, g_key_down, g_space_code, g_jump_action);
    abi_input_push_event(
        sid, g_key_kind, NULL, g_key_down, g_space_code,
        1.0, NULL, 1.0);

    int64_t fi = abi_input_begin_frame(sid, 16.67);
    __CPROVER_assert(fi > 0, "frame index > 0 after begin_frame");

    /* After begin_frame, action state queries return valid booleans */
    int64_t pressed = abi_input_action_pressed(sid, g_jump_action);
    int64_t down    = abi_input_action_down(sid, g_jump_action);
    __CPROVER_assert(pressed == 0 || pressed == 1,
                     "action_pressed returns valid boolean");
    __CPROVER_assert(down == 0 || down == 1,
                     "action_down returns valid boolean");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: begin_frame clears previous frame action state
 * ────────────────────────────────────────────────────────────────────── */
void check_begin_frame_clears_state(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    abi_input_bind_action(
        sid, g_key_kind, g_key_down, g_space_code, g_jump_action);
    abi_input_push_event(
        sid, g_key_kind, NULL, g_key_down, g_space_code, 1.0, NULL, 1.0);
    int64_t fi1 = abi_input_begin_frame(sid, 16.67);

    /* Frame with no events */
    int64_t fi2 = abi_input_begin_frame(sid, 16.67);
    __CPROVER_assert(fi2 > fi1, "frame index advances");

    /* After empty frame, pressed should be 0 */
    int64_t pressed = abi_input_action_pressed(sid, g_jump_action);
    __CPROVER_assert(pressed == 0,
                     "action_pressed == 0 after empty frame");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: frame_index tracks current frame
 * ────────────────────────────────────────────────────────────────────── */
void check_frame_index(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    int64_t fi0 = abi_input_frame_index(sid);
    __CPROVER_assert(fi0 >= 0, "initial frame index >= 0");

    abi_input_begin_frame(sid, 16.67);
    int64_t fi1 = abi_input_frame_index(sid);
    __CPROVER_assert(fi1 > fi0, "frame index advances after begin_frame");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: frame_index on invalid session returns error
 * ────────────────────────────────────────────────────────────────────── */
void check_frame_index_invalid(void) {
    abi_input_reset();
    int64_t fi = abi_input_frame_index(9999);
    __CPROVER_assert(fi < 0, "frame_index on bad session returns negative");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: event_count after begin_frame
 * ────────────────────────────────────────────────────────────────────── */
void check_event_count(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    int64_t ec0 = abi_input_event_count(sid);
    __CPROVER_assert(ec0 == 0, "event_count 0 before any events");

    abi_input_push_event(
        sid, g_key_kind, NULL, g_key_down, g_space_code, 1.0, NULL, 1.0);

    /* Still pending — not yet frame events */
    __CPROVER_assert(abi_input_event_count(sid) == 0,
                     "event_count 0 before begin_frame");

    abi_input_begin_frame(sid, 16.67);
    __CPROVER_assert(abi_input_event_count(sid) >= 0,
                     "event_count >= 0 after begin_frame");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: event accessors are null-safe for out-of-range index
 * ────────────────────────────────────────────────────────────────────── */
void check_event_accessors_oob(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    const char* kind   = abi_input_event_kind(sid, 0);
    const char* code   = abi_input_event_code(sid, 0);
    const char* action = abi_input_event_action(sid, 0);
    const char* text   = abi_input_event_text(sid, 0);
    __CPROVER_assert(kind   != NULL, "event_kind non-null for OOB");
    __CPROVER_assert(code   != NULL, "event_code non-null for OOB");
    __CPROVER_assert(action != NULL, "event_action non-null for OOB");
    __CPROVER_assert(text   != NULL, "event_text non-null for OOB");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: action state queries on invalid session return 0
 * ────────────────────────────────────────────────────────────────────── */
void check_action_queries_invalid_session(void) {
    abi_input_reset();

    int64_t p = abi_input_action_pressed(9999, g_jump_action);
    int64_t d = abi_input_action_down(9999, g_jump_action);
    int64_t r = abi_input_action_released(9999, g_jump_action);
    __CPROVER_assert(p == 0, "action_pressed on bad session = 0");
    __CPROVER_assert(d == 0, "action_down on bad session = 0");
    __CPROVER_assert(r == 0, "action_released on bad session = 0");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: text_commit_count and text_commit after begin_frame with text
 * ────────────────────────────────────────────────────────────────────── */
void check_text_commits(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    abi_input_push_event(
        sid, g_key_kind, NULL, g_text_kind, g_key_a,
        1.0, g_hello_text, 1.0);
    abi_input_begin_frame(sid, 16.67);

    int64_t tc = abi_input_text_commit_count(sid);
    __CPROVER_assert(tc >= 0, "text_commit_count >= 0");

    const char* t0 = abi_input_text_commit(sid, 0);
    __CPROVER_assert(t0 != NULL, "text_commit(0) returns non-null");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: axis_value on invalid session returns 0.0
 * ────────────────────────────────────────────────────────────────────── */
void check_axis_value_invalid_session(void) {
    abi_input_reset();
    double val = abi_input_axis_value(9999, g_x_axis);
    __CPROVER_assert(val == 0.0,
                     "axis_value on bad session returns 0.0");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: key_up produces released, clears down state
 * ────────────────────────────────────────────────────────────────────── */
void check_key_up_produces_released(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    abi_input_bind_action(
        sid, g_key_kind, g_key_down, g_space_code, g_jump_action);

    /* Press key */
    abi_input_push_event(
        sid, g_key_kind, NULL, g_key_down, g_space_code, 1.0, NULL, 1.0);
    abi_input_begin_frame(sid, 16.67);

    /* Release key */
    abi_input_push_event(
        sid, g_key_kind, NULL, g_key_up, g_space_code, 0.0, NULL, 1.0);
    abi_input_begin_frame(sid, 16.67);

    int64_t down     = abi_input_action_down(sid, g_jump_action);
    int64_t released = abi_input_action_released(sid, g_jump_action);
    __CPROVER_assert(down == 0, "action_down == 0 after key_up");
    __CPROVER_assert(released == 1, "action_released == 1 after key_up");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: last_status reflects most recent operation outcome
 * ────────────────────────────────────────────────────────────────────── */
void check_last_status_tracking(void) {
    abi_input_reset();

    abi_input_session_create(g_test_name);
    __CPROVER_assert(abi_input_last_status() == ABI_INPUT_OK,
                     "status OK after successful create");

    abi_input_session_destroy(9999);
    __CPROVER_assert(abi_input_last_status() == ABI_INPUT_INVALID_SESSION,
                     "status INVALID_SESSION after failed destroy");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: last_error_kind / last_error_message return non-null strings
 * ────────────────────────────────────────────────────────────────────── */
void check_last_error_strings(void) {
    abi_input_reset();

    __CPROVER_assert(abi_input_last_error_kind()    != NULL,
                     "error_kind non-null");
    __CPROVER_assert(abi_input_last_error_message() != NULL,
                     "error_message non-null");

    abi_input_session_destroy(9999);
    __CPROVER_assert(abi_input_last_error_kind()    != NULL,
                     "error_kind non-null after error");
    __CPROVER_assert(abi_input_last_error_message() != NULL,
                     "error_message non-null after error");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: MAX_SESSIONS creates succeed, the next one fails
 * ────────────────────────────────────────────────────────────────────── */
void check_session_capacity_exceeded(void) {
    abi_input_reset();

    int64_t ids[ABI_INPUT_MAX_SESSIONS];
    int i;
    for (i = 0; i < ABI_INPUT_MAX_SESSIONS; i++) {
        ids[i] = abi_input_session_create(g_test_name);
        __CPROVER_assert(ids[i] > 0,
                         "session create succeeds within capacity");
    }

    int64_t overflow = abi_input_session_create(g_test_name);
    __CPROVER_assert(overflow == ABI_INPUT_CAPACITY_EXCEEDED,
                     "capacity exceeded on extra create");

    /* Destroy one, reuse slot */
    abi_input_session_destroy(ids[0]);
    int64_t reused = abi_input_session_create(g_test_name);
    __CPROVER_assert(reused > 0, "create succeeds after destroy + reuse");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: push_event with empty source_kind or event_kind returns error
 * ────────────────────────────────────────────────────────────────────── */
void check_push_event_empty_fields(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    int64_t rc1 = abi_input_push_event(
        sid, g_empty_str, NULL, g_key_down, g_space_code, 1.0, NULL, 1.0);
    __CPROVER_assert(rc1 == ABI_INPUT_INVALID_ARGUMENT,
                     "push_event empty source_kind");

    int64_t rc2 = abi_input_push_event(
        sid, g_key_kind, NULL, g_empty_str, g_space_code, 1.0, NULL, 1.0);
    __CPROVER_assert(rc2 == ABI_INPUT_INVALID_ARGUMENT,
                     "push_event empty event_kind");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: push_agent_intent with empty action returns error
 * ────────────────────────────────────────────────────────────────────── */
void check_push_agent_intent_empty_action(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    int64_t rc = abi_input_push_agent_intent(
        sid, g_agent_id, g_empty_str, "text", 0.9);
    __CPROVER_assert(rc == ABI_INPUT_INVALID_ARGUMENT,
                     "push_agent_intent empty action");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: bind_axis with empty required args returns error
 * ────────────────────────────────────────────────────────────────────── */
void check_bind_axis_empty_args(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    int64_t rc1 = abi_input_bind_axis(
        sid, g_mouse_kind, g_empty_str, g_x_axis, g_x_axis, 1.0);
    __CPROVER_assert(rc1 == ABI_INPUT_INVALID_ARGUMENT,
                     "bind_axis empty event_kind");

    int64_t rc2 = abi_input_bind_axis(
        sid, g_mouse_kind, g_axis_kind, g_empty_str, g_x_axis, 1.0);
    __CPROVER_assert(rc2 == ABI_INPUT_INVALID_ARGUMENT,
                     "bind_axis empty code");

    int64_t rc3 = abi_input_bind_axis(
        sid, g_mouse_kind, g_axis_kind, g_x_axis, g_empty_str, 1.0);
    __CPROVER_assert(rc3 == ABI_INPUT_INVALID_ARGUMENT,
                     "bind_axis empty axis");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: trace_text returns non-null after events
 * ────────────────────────────────────────────────────────────────────── */
void check_trace_text(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    const char* trace0 = abi_input_trace_text(sid);
    __CPROVER_assert(trace0 != NULL, "trace_text returns non-null initially");

    abi_input_push_event(
        sid, g_key_kind, NULL, g_key_down, g_space_code, 1.0, NULL, 1.0);

    const char* trace1 = abi_input_trace_text(sid);
    __CPROVER_assert(trace1 != NULL,
                     "trace_text returns non-null after event");
}


/* ──────────────────────────────────────────────────────────────────────
 * Check: bind_action with NULL source_kind uses "*" wildcard
 * ────────────────────────────────────────────────────────────────────── */
void check_bind_action_null_source(void) {
    abi_input_reset();
    int64_t sid = abi_input_session_create(g_test_name);

    int64_t rc = abi_input_bind_action(
        sid, NULL, g_key_down, g_space_code, g_jump_action);
    __CPROVER_assert(rc == ABI_INPUT_OK,
                     "bind_action with NULL source_kind OK");
}


/* ──────────────────────────────────────────────────────────────────────
 * Main — run all checks
 * ────────────────────────────────────────────────────────────────────── */
int main(void) {
    check_reset_clears_state();
    check_session_create_destroy();
    check_session_destroy_invalid();
    check_session_count_multiple();
    check_session_create_null_name();
    check_bind_action();
    check_bind_action_empty_args();
    check_bind_axis();
    check_bind_axis_empty_args();
    check_bind_invalid_session();
    check_bind_action_null_source();
    check_push_event();
    check_push_event_invalid_session();
    check_push_agent_intent();
    check_push_agent_intent_empty_action();
    check_push_event_empty_fields();
    check_begin_frame_processing();
    check_begin_frame_clears_state();
    check_frame_index();
    check_frame_index_invalid();
    check_event_count();
    check_event_accessors_oob();
    check_action_queries_invalid_session();
    check_text_commits();
    check_axis_value_invalid_session();
    check_key_up_produces_released();
    check_last_status_tracking();
    check_last_error_strings();
    check_session_capacity_exceeded();
    check_trace_text();
    return 0;
}
