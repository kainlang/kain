#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/kain_native_input_system.h"
#include "../../include/kain_runtime_base.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define KAIN_NATIVE_INPUT_TEXT_EQUALS_CI _stricmp
#else
#include <strings.h>
#define KAIN_NATIVE_INPUT_TEXT_EQUALS_CI strcasecmp
#endif

typedef struct KainNativeInputBinding {
    int in_use;
    int is_axis;
    double scale;
    char source_kind[KAIN_NATIVE_INPUT_MAX_KEY];
    char event_kind[KAIN_NATIVE_INPUT_MAX_KEY];
    char code[KAIN_NATIVE_INPUT_MAX_KEY];
    char target[KAIN_NATIVE_INPUT_MAX_KEY];
} KainNativeInputBinding;

typedef struct KainNativeInputEvent {
    int64_t sequence;
    char source_kind[KAIN_NATIVE_INPUT_MAX_KEY];
    char source_id[KAIN_NATIVE_INPUT_MAX_KEY];
    char event_kind[KAIN_NATIVE_INPUT_MAX_KEY];
    char code[KAIN_NATIVE_INPUT_MAX_KEY];
    char action[KAIN_NATIVE_INPUT_MAX_KEY];
    char text[KAIN_NATIVE_INPUT_MAX_TEXT];
    double value;
    double confidence;
} KainNativeInputEvent;

typedef struct KainNativeInputAxisState {
    char axis[KAIN_NATIVE_INPUT_MAX_KEY];
    double value;
} KainNativeInputAxisState;

typedef struct KainNativeInputSession {
    int in_use;
    int64_t id;
    int64_t frame_index;
    int64_t next_sequence;
    double last_delta_ms;
    char name[KAIN_NATIVE_INPUT_MAX_KEY];

    KainNativeInputBinding bindings[KAIN_NATIVE_INPUT_MAX_BINDINGS];
    int64_t binding_count;

    KainNativeInputEvent pending_events[KAIN_NATIVE_INPUT_MAX_EVENTS];
    int64_t pending_event_count;
    KainNativeInputEvent frame_events[KAIN_NATIVE_INPUT_MAX_EVENTS];
    int64_t frame_event_count;

    char actions_down[KAIN_NATIVE_INPUT_MAX_ACTIONS][KAIN_NATIVE_INPUT_MAX_KEY];
    int64_t action_down_count;
    char actions_pressed[KAIN_NATIVE_INPUT_MAX_ACTIONS][KAIN_NATIVE_INPUT_MAX_KEY];
    int64_t action_pressed_count;
    char actions_released[KAIN_NATIVE_INPUT_MAX_ACTIONS][KAIN_NATIVE_INPUT_MAX_KEY];
    int64_t action_released_count;

    KainNativeInputAxisState axes[KAIN_NATIVE_INPUT_MAX_AXES];
    int64_t axis_count;

    char text_commits[KAIN_NATIVE_INPUT_MAX_TEXT_COMMITS][KAIN_NATIVE_INPUT_MAX_TEXT];
    int64_t text_commit_count;

    char trace_text[KAIN_NATIVE_INPUT_MAX_TRACE_TEXT];
    size_t trace_text_length;
} KainNativeInputSession;

static KainNativeInputSession g_sessions[KAIN_NATIVE_INPUT_MAX_SESSIONS];
static int64_t g_next_session_id = 1;
static int64_t g_last_status = KAIN_NATIVE_INPUT_OK;
static char g_last_error_kind[KAIN_NATIVE_INPUT_MAX_KEY] = "ok";
static char g_last_error_message[KAIN_NATIVE_INPUT_MAX_TEXT] = "";
static const char g_empty_string[] = "";

static void kain_native_input_copy(char* dest, size_t capacity, const char* source) {
    if (!dest || capacity == 0u) {
        return;
    }
    if (!source) {
        dest[0] = '\0';
        return;
    }
    snprintf(dest, capacity, "%s", source);
}

static const char* kain_native_input_string(const char* source) {
    return string_new((char*)(source ? source : g_empty_string));
}

static int64_t kain_native_input_ok(void) {
    g_last_status = KAIN_NATIVE_INPUT_OK;
    kain_native_input_copy(g_last_error_kind, sizeof(g_last_error_kind), "ok");
    kain_native_input_copy(g_last_error_message, sizeof(g_last_error_message), "");
    return KAIN_NATIVE_INPUT_OK;
}

static int64_t kain_native_input_fail(int64_t status, const char* kind, const char* message) {
    g_last_status = status;
    kain_native_input_copy(g_last_error_kind, sizeof(g_last_error_kind), kind ? kind : "error");
    kain_native_input_copy(g_last_error_message, sizeof(g_last_error_message), message ? message : "");
    return status;
}

static int kain_native_input_text_equal(const char* left, const char* right) {
    if (!left || !right) {
        return 0;
    }
    return KAIN_NATIVE_INPUT_TEXT_EQUALS_CI(left, right) == 0;
}

static int kain_native_input_text_empty(const char* text) {
    return !text || text[0] == '\0';
}

static KainNativeInputSession* kain_native_input_session(int64_t session_id) {
    int i;
    for (i = 0; i < KAIN_NATIVE_INPUT_MAX_SESSIONS; i++) {
        if (g_sessions[i].in_use && g_sessions[i].id == session_id) {
            return &g_sessions[i];
        }
    }
    return NULL;
}

static int kain_native_input_name_index(char names[][KAIN_NATIVE_INPUT_MAX_KEY], int64_t count, const char* name) {
    int64_t i;
    if (kain_native_input_text_empty(name)) {
        return -1;
    }
    for (i = 0; i < count; i++) {
        if (kain_native_input_text_equal(names[i], name)) {
            return (int)i;
        }
    }
    return -1;
}

static int kain_native_input_name_add(char names[][KAIN_NATIVE_INPUT_MAX_KEY], int64_t* count, int64_t capacity, const char* name) {
    if (kain_native_input_text_empty(name)) {
        return 0;
    }
    if (kain_native_input_name_index(names, *count, name) >= 0) {
        return 1;
    }
    if (*count >= capacity) {
        return 0;
    }
    kain_native_input_copy(names[*count], KAIN_NATIVE_INPUT_MAX_KEY, name);
    *count += 1;
    return 1;
}

static int kain_native_input_name_remove(char names[][KAIN_NATIVE_INPUT_MAX_KEY], int64_t* count, const char* name) {
    int index = kain_native_input_name_index(names, *count, name);
    int64_t i;
    if (index < 0) {
        return 0;
    }
    for (i = index; i + 1 < *count; i++) {
        kain_native_input_copy(names[i], KAIN_NATIVE_INPUT_MAX_KEY, names[i + 1]);
    }
    if (*count > 0) {
        *count -= 1;
        names[*count][0] = '\0';
    }
    return 1;
}

static int kain_native_input_binding_matches(const KainNativeInputBinding* binding, const KainNativeInputEvent* event) {
    if (!binding || !event || !binding->in_use) {
        return 0;
    }
    if (!kain_native_input_text_empty(binding->source_kind)
        && strcmp(binding->source_kind, "*") != 0
        && !kain_native_input_text_equal(binding->source_kind, event->source_kind)) {
        return 0;
    }
    return kain_native_input_text_equal(binding->event_kind, event->event_kind)
        && kain_native_input_text_equal(binding->code, event->code);
}

static const KainNativeInputBinding* kain_native_input_find_binding(
    const KainNativeInputSession* session,
    const KainNativeInputEvent* event,
    int is_axis
) {
    int64_t i;
    if (!session || !event) {
        return NULL;
    }
    for (i = 0; i < session->binding_count; i++) {
        const KainNativeInputBinding* binding = &session->bindings[i];
        if (binding->is_axis == is_axis && kain_native_input_binding_matches(binding, event)) {
            return binding;
        }
    }
    return NULL;
}

static void kain_native_input_trace_append(KainNativeInputSession* session, const KainNativeInputEvent* event) {
    int written;
    if (!session || !event || session->trace_text_length >= KAIN_NATIVE_INPUT_MAX_TRACE_TEXT - 1u) {
        return;
    }
    written = snprintf(
        session->trace_text + session->trace_text_length,
        KAIN_NATIVE_INPUT_MAX_TRACE_TEXT - session->trace_text_length,
        "%lld|%s|%s|%s|%s|%.17g|%s|%.17g\n",
        (long long)event->sequence,
        event->source_kind,
        event->source_id,
        event->event_kind,
        event->code,
        event->value,
        event->text,
        event->confidence
    );
    if (written > 0) {
        size_t used = (size_t)written;
        if (used >= KAIN_NATIVE_INPUT_MAX_TRACE_TEXT - session->trace_text_length) {
            session->trace_text_length = KAIN_NATIVE_INPUT_MAX_TRACE_TEXT - 1u;
            session->trace_text[session->trace_text_length] = '\0';
        } else {
            session->trace_text_length += used;
        }
    }
}

static int kain_native_input_axis_add(KainNativeInputSession* session, const char* axis, double value) {
    int64_t i;
    if (!session || kain_native_input_text_empty(axis)) {
        return 0;
    }
    for (i = 0; i < session->axis_count; i++) {
        if (kain_native_input_text_equal(session->axes[i].axis, axis)) {
            session->axes[i].value += value;
            return 1;
        }
    }
    if (session->axis_count >= KAIN_NATIVE_INPUT_MAX_AXES) {
        return 0;
    }
    kain_native_input_copy(session->axes[session->axis_count].axis, KAIN_NATIVE_INPUT_MAX_KEY, axis);
    session->axes[session->axis_count].value = value;
    session->axis_count += 1;
    return 1;
}

static void kain_native_input_reduce_action(KainNativeInputSession* session, const KainNativeInputEvent* event, const char* action) {
    if (!session || !event || kain_native_input_text_empty(action)) {
        return;
    }
    if (kain_native_input_text_equal(event->event_kind, "key_up")
        || kain_native_input_text_equal(event->event_kind, "pointer_up")
        || kain_native_input_text_equal(event->event_kind, "action_up")) {
        if (kain_native_input_name_remove(session->actions_down, &session->action_down_count, action)) {
            kain_native_input_name_add(
                session->actions_released,
                &session->action_released_count,
                KAIN_NATIVE_INPUT_MAX_ACTIONS,
                action
            );
        }
        return;
    }

    if (kain_native_input_text_equal(event->event_kind, "action")) {
        kain_native_input_name_add(
            session->actions_pressed,
            &session->action_pressed_count,
            KAIN_NATIVE_INPUT_MAX_ACTIONS,
            action
        );
        return;
    }

    if (kain_native_input_name_index(session->actions_down, session->action_down_count, action) < 0) {
        kain_native_input_name_add(
            session->actions_pressed,
            &session->action_pressed_count,
            KAIN_NATIVE_INPUT_MAX_ACTIONS,
            action
        );
    }
    kain_native_input_name_add(session->actions_down, &session->action_down_count, KAIN_NATIVE_INPUT_MAX_ACTIONS, action);
}

static void kain_native_input_reduce_event(KainNativeInputSession* session, const KainNativeInputEvent* event) {
    const KainNativeInputBinding* action_binding;
    const KainNativeInputBinding* axis_binding;
    if (!session || !event) {
        return;
    }

    if (kain_native_input_text_equal(event->event_kind, "text") && !kain_native_input_text_empty(event->text)) {
        if (session->text_commit_count < KAIN_NATIVE_INPUT_MAX_TEXT_COMMITS) {
            kain_native_input_copy(
                session->text_commits[session->text_commit_count],
                KAIN_NATIVE_INPUT_MAX_TEXT,
                event->text
            );
            session->text_commit_count += 1;
        }
    }

    if (kain_native_input_text_equal(event->event_kind, "axis")) {
        axis_binding = kain_native_input_find_binding(session, event, 1);
        if (axis_binding) {
            kain_native_input_axis_add(session, axis_binding->target, event->value * axis_binding->scale);
        } else {
            kain_native_input_axis_add(session, event->code, event->value);
        }
    }

    action_binding = kain_native_input_find_binding(session, event, 0);
    if (action_binding) {
        kain_native_input_reduce_action(session, event, action_binding->target);
    } else if (!kain_native_input_text_empty(event->action)) {
        kain_native_input_reduce_action(session, event, event->action);
    }
}

static int kain_native_input_split_fields(const char* line, char fields[][KAIN_NATIVE_INPUT_MAX_TEXT], int max_fields) {
    int field = 0;
    size_t offset = 0;
    const char* cursor = line;
    if (!line || max_fields <= 0) {
        return 0;
    }
    fields[0][0] = '\0';
    while (*cursor && field < max_fields) {
        if (*cursor == '|') {
            fields[field][offset] = '\0';
            field += 1;
            offset = 0;
            if (field < max_fields) {
                fields[field][0] = '\0';
            }
        } else if (offset + 1u < KAIN_NATIVE_INPUT_MAX_TEXT) {
            fields[field][offset++] = *cursor;
        }
        cursor++;
    }
    if (field < max_fields) {
        fields[field][offset] = '\0';
        field += 1;
    }
    return field;
}

int64_t kain_native_input_reset(void) {
    memset(g_sessions, 0, sizeof(g_sessions));
    g_next_session_id = 1;
    return kain_native_input_ok();
}

int64_t kain_native_input_session_create(const char* name) {
    int i;
    for (i = 0; i < KAIN_NATIVE_INPUT_MAX_SESSIONS; i++) {
        if (!g_sessions[i].in_use) {
            memset(&g_sessions[i], 0, sizeof(g_sessions[i]));
            g_sessions[i].in_use = 1;
            g_sessions[i].id = g_next_session_id++;
            g_sessions[i].next_sequence = 1;
            kain_native_input_copy(g_sessions[i].name, sizeof(g_sessions[i].name), name ? name : "input-session");
            kain_native_input_ok();
            return g_sessions[i].id;
        }
    }
    return kain_native_input_fail(KAIN_NATIVE_INPUT_CAPACITY_EXCEEDED, "capacity", "input session capacity exceeded");
}

int64_t kain_native_input_session_destroy(int64_t session_id) {
    KainNativeInputSession* session = kain_native_input_session(session_id);
    if (!session) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    memset(session, 0, sizeof(*session));
    return kain_native_input_ok();
}

int64_t kain_native_input_session_count(void) {
    int i;
    int64_t count = 0;
    for (i = 0; i < KAIN_NATIVE_INPUT_MAX_SESSIONS; i++) {
        if (g_sessions[i].in_use) {
            count += 1;
        }
    }
    kain_native_input_ok();
    return count;
}

int64_t kain_native_input_bind_action(
    int64_t session_id,
    const char* source_kind,
    const char* event_kind,
    const char* code,
    const char* action
) {
    KainNativeInputSession* session = kain_native_input_session(session_id);
    KainNativeInputBinding* binding;
    if (!session) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    if (kain_native_input_text_empty(event_kind) || kain_native_input_text_empty(code) || kain_native_input_text_empty(action)) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_INVALID_ARGUMENT, "invalid_argument", "action binding requires event kind, code, and action");
    }
    if (session->binding_count >= KAIN_NATIVE_INPUT_MAX_BINDINGS) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_CAPACITY_EXCEEDED, "capacity", "input binding capacity exceeded");
    }
    binding = &session->bindings[session->binding_count++];
    memset(binding, 0, sizeof(*binding));
    binding->in_use = 1;
    binding->is_axis = 0;
    binding->scale = 1.0;
    kain_native_input_copy(binding->source_kind, sizeof(binding->source_kind), source_kind ? source_kind : "*");
    kain_native_input_copy(binding->event_kind, sizeof(binding->event_kind), event_kind);
    kain_native_input_copy(binding->code, sizeof(binding->code), code);
    kain_native_input_copy(binding->target, sizeof(binding->target), action);
    return kain_native_input_ok();
}

int64_t kain_native_input_bind_axis(
    int64_t session_id,
    const char* source_kind,
    const char* event_kind,
    const char* code,
    const char* axis,
    double scale
) {
    KainNativeInputSession* session = kain_native_input_session(session_id);
    KainNativeInputBinding* binding;
    if (!session) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    if (kain_native_input_text_empty(event_kind) || kain_native_input_text_empty(code) || kain_native_input_text_empty(axis)) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_INVALID_ARGUMENT, "invalid_argument", "axis binding requires event kind, code, and axis");
    }
    if (session->binding_count >= KAIN_NATIVE_INPUT_MAX_BINDINGS) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_CAPACITY_EXCEEDED, "capacity", "input binding capacity exceeded");
    }
    binding = &session->bindings[session->binding_count++];
    memset(binding, 0, sizeof(*binding));
    binding->in_use = 1;
    binding->is_axis = 1;
    binding->scale = scale;
    kain_native_input_copy(binding->source_kind, sizeof(binding->source_kind), source_kind ? source_kind : "*");
    kain_native_input_copy(binding->event_kind, sizeof(binding->event_kind), event_kind);
    kain_native_input_copy(binding->code, sizeof(binding->code), code);
    kain_native_input_copy(binding->target, sizeof(binding->target), axis);
    return kain_native_input_ok();
}

int64_t kain_native_input_push_event(
    int64_t session_id,
    const char* source_kind,
    const char* source_id,
    const char* event_kind,
    const char* code,
    double value,
    const char* text,
    double confidence
) {
    KainNativeInputSession* session = kain_native_input_session(session_id);
    KainNativeInputEvent* event;
    if (!session) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    if (kain_native_input_text_empty(source_kind) || kain_native_input_text_empty(event_kind)) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_INVALID_ARGUMENT, "invalid_argument", "input event requires source kind and event kind");
    }
    if (session->pending_event_count >= KAIN_NATIVE_INPUT_MAX_EVENTS) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_CAPACITY_EXCEEDED, "capacity", "pending input event capacity exceeded");
    }
    event = &session->pending_events[session->pending_event_count++];
    memset(event, 0, sizeof(*event));
    event->sequence = session->next_sequence++;
    event->value = value;
    event->confidence = confidence;
    kain_native_input_copy(event->source_kind, sizeof(event->source_kind), source_kind);
    kain_native_input_copy(event->source_id, sizeof(event->source_id), source_id ? source_id : "");
    kain_native_input_copy(event->event_kind, sizeof(event->event_kind), event_kind);
    kain_native_input_copy(event->code, sizeof(event->code), code ? code : "");
    if (kain_native_input_text_equal(event_kind, "action")
        || kain_native_input_text_equal(event_kind, "action_down")
        || kain_native_input_text_equal(event_kind, "action_up")) {
        kain_native_input_copy(event->action, sizeof(event->action), code ? code : "");
    }
    kain_native_input_copy(event->text, sizeof(event->text), text ? text : "");
    kain_native_input_trace_append(session, event);
    kain_native_input_ok();
    return event->sequence;
}

int64_t kain_native_input_push_agent_intent(
    int64_t session_id,
    const char* source_id,
    const char* action,
    const char* command_text,
    double confidence
) {
    KainNativeInputSession* session = kain_native_input_session(session_id);
    KainNativeInputEvent* event;
    if (!session) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    if (kain_native_input_text_empty(action)) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_INVALID_ARGUMENT, "invalid_argument", "agent intent requires an action");
    }
    if (session->pending_event_count >= KAIN_NATIVE_INPUT_MAX_EVENTS) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_CAPACITY_EXCEEDED, "capacity", "pending input event capacity exceeded");
    }
    event = &session->pending_events[session->pending_event_count++];
    memset(event, 0, sizeof(*event));
    event->sequence = session->next_sequence++;
    event->value = 1.0;
    event->confidence = confidence;
    kain_native_input_copy(event->source_kind, sizeof(event->source_kind), "agent.intent");
    kain_native_input_copy(event->source_id, sizeof(event->source_id), source_id ? source_id : "");
    kain_native_input_copy(event->event_kind, sizeof(event->event_kind), "action");
    kain_native_input_copy(event->code, sizeof(event->code), action);
    kain_native_input_copy(event->action, sizeof(event->action), action);
    kain_native_input_copy(event->text, sizeof(event->text), command_text ? command_text : "");
    kain_native_input_trace_append(session, event);
    kain_native_input_ok();
    return event->sequence;
}

int64_t kain_native_input_begin_frame(int64_t session_id, double delta_ms) {
    KainNativeInputSession* session = kain_native_input_session(session_id);
    int64_t i;
    if (!session) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    session->frame_index += 1;
    session->last_delta_ms = delta_ms;
    session->frame_event_count = session->pending_event_count;
    if (session->frame_event_count > 0) {
        memcpy(session->frame_events, session->pending_events, sizeof(KainNativeInputEvent) * (size_t)session->frame_event_count);
    }
    session->pending_event_count = 0;
    session->action_pressed_count = 0;
    session->action_released_count = 0;
    session->axis_count = 0;
    session->text_commit_count = 0;

    for (i = 0; i < session->frame_event_count; i++) {
        kain_native_input_reduce_event(session, &session->frame_events[i]);
    }
    kain_native_input_ok();
    return session->frame_index;
}

int64_t kain_native_input_frame_index(int64_t session_id) {
    KainNativeInputSession* session = kain_native_input_session(session_id);
    if (!session) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    kain_native_input_ok();
    return session->frame_index;
}

int64_t kain_native_input_event_count(int64_t session_id) {
    KainNativeInputSession* session = kain_native_input_session(session_id);
    if (!session) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    kain_native_input_ok();
    return session->frame_event_count;
}

static const KainNativeInputEvent* kain_native_input_frame_event(int64_t session_id, int64_t event_index) {
    KainNativeInputSession* session = kain_native_input_session(session_id);
    if (!session || event_index < 0 || event_index >= session->frame_event_count) {
        return NULL;
    }
    return &session->frame_events[event_index];
}

const char* kain_native_input_event_kind(int64_t session_id, int64_t event_index) {
    const KainNativeInputEvent* event = kain_native_input_frame_event(session_id, event_index);
    return kain_native_input_string(event ? event->event_kind : g_empty_string);
}

const char* kain_native_input_event_source_kind(int64_t session_id, int64_t event_index) {
    const KainNativeInputEvent* event = kain_native_input_frame_event(session_id, event_index);
    return kain_native_input_string(event ? event->source_kind : g_empty_string);
}

const char* kain_native_input_event_code(int64_t session_id, int64_t event_index) {
    const KainNativeInputEvent* event = kain_native_input_frame_event(session_id, event_index);
    return kain_native_input_string(event ? event->code : g_empty_string);
}

const char* kain_native_input_event_action(int64_t session_id, int64_t event_index) {
    const KainNativeInputEvent* event = kain_native_input_frame_event(session_id, event_index);
    return kain_native_input_string(event ? event->action : g_empty_string);
}

const char* kain_native_input_event_text(int64_t session_id, int64_t event_index) {
    const KainNativeInputEvent* event = kain_native_input_frame_event(session_id, event_index);
    return kain_native_input_string(event ? event->text : g_empty_string);
}

int64_t kain_native_input_action_pressed(int64_t session_id, const char* action) {
    KainNativeInputSession* session = kain_native_input_session(session_id);
    if (!session) return 0;
    return kain_native_input_name_index(session->actions_pressed, session->action_pressed_count, action) >= 0 ? 1 : 0;
}

int64_t kain_native_input_action_down(int64_t session_id, const char* action) {
    KainNativeInputSession* session = kain_native_input_session(session_id);
    if (!session) return 0;
    return kain_native_input_name_index(session->actions_down, session->action_down_count, action) >= 0 ? 1 : 0;
}

int64_t kain_native_input_action_released(int64_t session_id, const char* action) {
    KainNativeInputSession* session = kain_native_input_session(session_id);
    if (!session) return 0;
    return kain_native_input_name_index(session->actions_released, session->action_released_count, action) >= 0 ? 1 : 0;
}

double kain_native_input_axis_value(int64_t session_id, const char* axis) {
    KainNativeInputSession* session = kain_native_input_session(session_id);
    int64_t i;
    if (!session || kain_native_input_text_empty(axis)) {
        return 0.0;
    }
    for (i = 0; i < session->axis_count; i++) {
        if (kain_native_input_text_equal(session->axes[i].axis, axis)) {
            return session->axes[i].value;
        }
    }
    return 0.0;
}

int64_t kain_native_input_text_commit_count(int64_t session_id) {
    KainNativeInputSession* session = kain_native_input_session(session_id);
    if (!session) {
        return 0;
    }
    return session->text_commit_count;
}

const char* kain_native_input_text_commit(int64_t session_id, int64_t index) {
    KainNativeInputSession* session = kain_native_input_session(session_id);
    if (!session || index < 0 || index >= session->text_commit_count) {
        return kain_native_input_string(g_empty_string);
    }
    return kain_native_input_string(session->text_commits[index]);
}

const char* kain_native_input_trace_text(int64_t session_id) {
    KainNativeInputSession* session = kain_native_input_session(session_id);
    if (!session) {
        return kain_native_input_string(g_empty_string);
    }
    return kain_native_input_string(session->trace_text);
}

int64_t kain_native_input_replay_trace(int64_t session_id, const char* trace_text) {
    char* buffer;
    char* line;
    KainNativeInputSession* session = kain_native_input_session(session_id);
    if (!session) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    if (!trace_text) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_INVALID_ARGUMENT, "invalid_argument", "trace text is null");
    }
    buffer = (char*)malloc(strlen(trace_text) + 1u);
    if (!buffer) {
        return kain_native_input_fail(KAIN_NATIVE_INPUT_INVALID_ARGUMENT, "allocation", "could not copy trace text");
    }
    strcpy(buffer, trace_text);
    line = strtok(buffer, "\n");
    while (line) {
        char fields[8][KAIN_NATIVE_INPUT_MAX_TEXT];
        if (kain_native_input_split_fields(line, fields, 8) >= 8) {
            int64_t sequence = (int64_t)atoll(fields[0]);
            KainNativeInputEvent* event;
            if (session->pending_event_count >= KAIN_NATIVE_INPUT_MAX_EVENTS) {
                free(buffer);
                return kain_native_input_fail(KAIN_NATIVE_INPUT_CAPACITY_EXCEEDED, "capacity", "pending input event capacity exceeded");
            }
            event = &session->pending_events[session->pending_event_count++];
            memset(event, 0, sizeof(*event));
            event->sequence = sequence > 0 ? sequence : session->next_sequence++;
            if (event->sequence >= session->next_sequence) {
                session->next_sequence = event->sequence + 1;
            }
            event->value = atof(fields[5]);
            event->confidence = atof(fields[7]);
            kain_native_input_copy(event->source_kind, sizeof(event->source_kind), fields[1]);
            kain_native_input_copy(event->source_id, sizeof(event->source_id), fields[2]);
            kain_native_input_copy(event->event_kind, sizeof(event->event_kind), fields[3]);
            kain_native_input_copy(event->code, sizeof(event->code), fields[4]);
            kain_native_input_copy(event->text, sizeof(event->text), fields[6]);
            if (kain_native_input_text_equal(event->event_kind, "action")) {
                kain_native_input_copy(event->action, sizeof(event->action), event->code);
            }
        }
        line = strtok(NULL, "\n");
    }
    free(buffer);
    return kain_native_input_ok();
}

int64_t kain_native_input_last_status(void) {
    return g_last_status;
}

const char* kain_native_input_last_error_kind(void) {
    return kain_native_input_string(g_last_error_kind);
}

const char* kain_native_input_last_error_message(void) {
    return kain_native_input_string(g_last_error_message);
}
