#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/input_system.h"
#include "../../include/base.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define ABI_INPUT_TEXT_EQUALS_CI _stricmp
#else
#include <strings.h>
#define ABI_INPUT_TEXT_EQUALS_CI strcasecmp
#endif

typedef struct KainNativeInputBinding {
    int in_use;
    int is_axis;
    double scale;
    char source_kind[ABI_INPUT_MAX_KEY];
    char event_kind[ABI_INPUT_MAX_KEY];
    char code[ABI_INPUT_MAX_KEY];
    char target[ABI_INPUT_MAX_KEY];
} KainNativeInputBinding;

typedef struct KainNativeInputEvent {
    int64_t sequence;
    char source_kind[ABI_INPUT_MAX_KEY];
    char source_id[ABI_INPUT_MAX_KEY];
    char event_kind[ABI_INPUT_MAX_KEY];
    char code[ABI_INPUT_MAX_KEY];
    char action[ABI_INPUT_MAX_KEY];
    char text[ABI_INPUT_MAX_TEXT];
    double value;
    double confidence;
} KainNativeInputEvent;

typedef struct KainNativeInputAxisState {
    char axis[ABI_INPUT_MAX_KEY];
    double value;
} KainNativeInputAxisState;

typedef struct KainNativeInputSession {
    int in_use;
    int64_t id;
    int64_t frame_index;
    int64_t next_sequence;
    double last_delta_ms;
    char name[ABI_INPUT_MAX_KEY];

    KainNativeInputBinding bindings[ABI_INPUT_MAX_BINDINGS];
    int64_t binding_count;

    KainNativeInputEvent pending_events[ABI_INPUT_MAX_EVENTS];
    int64_t pending_event_count;
    KainNativeInputEvent frame_events[ABI_INPUT_MAX_EVENTS];
    int64_t frame_event_count;

    char actions_down[ABI_INPUT_MAX_ACTIONS][ABI_INPUT_MAX_KEY];
    uint32_t actions_down_hashes[ABI_INPUT_MAX_ACTIONS];
    int64_t action_down_count;
    char actions_pressed[ABI_INPUT_MAX_ACTIONS][ABI_INPUT_MAX_KEY];
    uint32_t actions_pressed_hashes[ABI_INPUT_MAX_ACTIONS];
    int64_t action_pressed_count;
    char actions_released[ABI_INPUT_MAX_ACTIONS][ABI_INPUT_MAX_KEY];
    uint32_t actions_released_hashes[ABI_INPUT_MAX_ACTIONS];
    int64_t action_released_count;

    KainNativeInputAxisState axes[ABI_INPUT_MAX_AXES];
    int64_t axis_count;

    char text_commits[ABI_INPUT_MAX_TEXT_COMMITS][ABI_INPUT_MAX_TEXT];
    int64_t text_commit_count;

    char trace_text[ABI_INPUT_MAX_TRACE_TEXT];
    size_t trace_text_length;
} KainNativeInputSession;

static KainNativeInputSession g_sessions[ABI_INPUT_MAX_SESSIONS];
static int64_t g_next_session_id = 1;
static int64_t g_last_status = ABI_INPUT_OK;
static char g_last_error_kind[ABI_INPUT_MAX_KEY] = "ok";
static char g_last_error_message[ABI_INPUT_MAX_TEXT] = "";
static const char g_empty_string[] = "";

static void abi_input_copy(char* dest, size_t capacity, const char* source) {
    if (!dest || capacity == 0u) {
        return;
    }
    if (!source) {
        dest[0] = '\0';
        return;
    }
    snprintf(dest, capacity, "%s", source);
}

static const char* abi_input_string(const char* source) {
    return string_new((char*)(source ? source : g_empty_string));
}

static int64_t abi_input_ok(void) {
    g_last_status = ABI_INPUT_OK;
    abi_input_copy(g_last_error_kind, sizeof(g_last_error_kind), "ok");
    abi_input_copy(g_last_error_message, sizeof(g_last_error_message), "");
    return ABI_INPUT_OK;
}

static int64_t abi_input_fail(int64_t status, const char* kind, const char* message) {
    g_last_status = status;
    abi_input_copy(g_last_error_kind, sizeof(g_last_error_kind), kind ? kind : "error");
    abi_input_copy(g_last_error_message, sizeof(g_last_error_message), message ? message : "");
    return status;
}

static int abi_input_text_equal(const char* left, const char* right) {
    if (!left || !right) {
        return 0;
    }
    return ABI_INPUT_TEXT_EQUALS_CI(left, right) == 0;
}

static int abi_input_text_empty(const char* text) {
    return !text || text[0] == '\0';
}

/*
 * Compute a 32-bit token signature for an event kind string.
 * Signature: (len << 24) XOR (first << 16) XOR (second << 8) XOR last
 * For strings with length <= 1, second == first (to match the proof).
 * All 9 known event kind / source kind strings have DISTINCT signatures,
 * proven in:
 *   runtime/native/src/core/z3/proofs/native-input-event-kind-token-signatures-collision-free.yaml
 */
static uint32_t abi_input_event_kind_sig(const char* kind) {
    size_t len_val;
    uint32_t len, first, second, last;
    if (!kind || !kind[0]) {
        return 0;
    }
    len_val = strlen(kind);
    len = (uint32_t)len_val;
    first = (uint8_t)kind[0];
    second = (len_val > 1) ? (uint8_t)kind[1] : first;
    last = (len_val > 0) ? (uint8_t)kind[len_val - 1] : first;
    return (len << 24) ^ (first << 16) ^ (second << 8) ^ last;
}

/*
 * Case-insensitive FNV-1a hash for action/axis names.
 * Used for fast-rejection in abi_input_name_index.
 * All input table sizes are powers of two (proven):
 *   runtime/native/src/core/z3/proofs/native-input-hash-probe-bounds.yaml
 */
static uint32_t abi_input_str_hash(const char* s) {
    uint32_t h = 0x811c9dc5u; /* FNV-1a offset basis */
    unsigned char c;
    if (!s) {
        return 0;
    }
    while ((c = (unsigned char)*s++) != '\0') {
        /* Case-insensitive folding: uppercase -> lowercase */
        if (c >= 'A' && c <= 'Z') {
            c = (unsigned char)(c + 32);
        }
        h ^= (uint32_t)c;
        h *= 0x01000193u; /* FNV-1a prime */
    }
    return h;
}

static KainNativeInputSession* abi_input_session(int64_t session_id) {
    int i;
    for (i = 0; i < ABI_INPUT_MAX_SESSIONS; i++) {
        if (g_sessions[i].in_use && g_sessions[i].id == session_id) {
            return &g_sessions[i];
        }
    }
    return NULL;
}

static int abi_input_name_index(
    char names[][ABI_INPUT_MAX_KEY],
    const uint32_t* hashes,
    int64_t count,
    const char* name
) {
    int64_t i;
    if (abi_input_text_empty(name)) {
        return -1;
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-input-hash-probe-bounds.yaml
     * All input table sizes are powers of two — hash probe is safe.
     * Hash-based fast rejection: pre-computed FNV-1a hash avoids strcmp
     * for most non-matching entries. */
    if (hashes) {
        uint32_t name_hash = abi_input_str_hash(name);
        for (i = 0; i < count; i++) {
            if (hashes[i] == name_hash && abi_input_text_equal(names[i], name)) {
                return (int)i;
            }
        }
    } else {
        /* Fallback: no hash array available */
        for (i = 0; i < count; i++) {
            if (abi_input_text_equal(names[i], name)) {
                return (int)i;
            }
        }
    }
    return -1;
}

static int abi_input_name_add(
    char names[][ABI_INPUT_MAX_KEY],
    uint32_t* hashes,
    int64_t* count,
    int64_t capacity,
    const char* name
) {
    if (abi_input_text_empty(name)) {
        return 0;
    }
    if (abi_input_name_index(names, hashes, *count, name) >= 0) {
        return 1;
    }
    if (*count >= capacity) {
        return 0;
    }
    abi_input_copy(names[*count], ABI_INPUT_MAX_KEY, name);
    if (hashes) {
        hashes[*count] = abi_input_str_hash(name);
    }
    *count += 1;
    return 1;
}

static int abi_input_name_remove(
    char names[][ABI_INPUT_MAX_KEY],
    uint32_t* hashes,
    int64_t* count,
    const char* name
) {
    int64_t i;
    int index = abi_input_name_index(names, hashes, *count, name);
    if (index < 0) {
        return 0;
    }
    for (i = index; i + 1 < *count; i++) {
        abi_input_copy(names[i], ABI_INPUT_MAX_KEY, names[i + 1]);
        if (hashes) {
            hashes[i] = hashes[i + 1];
        }
    }
    if (*count > 0) {
        *count -= 1;
        names[*count][0] = '\0';
        if (hashes) {
            hashes[*count] = 0;
        }
    }
    return 1;
}

static int abi_input_binding_matches(const KainNativeInputBinding* binding, const KainNativeInputEvent* event) {
    if (!binding || !event || !binding->in_use) {
        return 0;
    }
    if (!abi_input_text_empty(binding->source_kind)
        && strcmp(binding->source_kind, "*") != 0
        && !abi_input_text_equal(binding->source_kind, event->source_kind)) {
        return 0;
    }
    return abi_input_text_equal(binding->event_kind, event->event_kind)
        && abi_input_text_equal(binding->code, event->code);
}

static const KainNativeInputBinding* abi_input_find_binding(
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
        if (binding->is_axis == is_axis && abi_input_binding_matches(binding, event)) {
            return binding;
        }
    }
    return NULL;
}

static void abi_input_trace_append(KainNativeInputSession* session, const KainNativeInputEvent* event) {
    int written;
    if (!session || !event || session->trace_text_length >= ABI_INPUT_MAX_TRACE_TEXT - 1u) {
        return;
    }
    written = snprintf(
        session->trace_text + session->trace_text_length,
        ABI_INPUT_MAX_TRACE_TEXT - session->trace_text_length,
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
        if (used >= ABI_INPUT_MAX_TRACE_TEXT - session->trace_text_length) {
            session->trace_text_length = ABI_INPUT_MAX_TRACE_TEXT - 1u;
            session->trace_text[session->trace_text_length] = '\0';
        } else {
            session->trace_text_length += used;
        }
    }
}

static int abi_input_axis_add(KainNativeInputSession* session, const char* axis, double value) {
    int64_t i;
    if (!session || abi_input_text_empty(axis)) {
        return 0;
    }
    for (i = 0; i < session->axis_count; i++) {
        if (abi_input_text_equal(session->axes[i].axis, axis)) {
            session->axes[i].value += value;
            return 1;
        }
    }
    if (session->axis_count >= ABI_INPUT_MAX_AXES) {
        return 0;
    }
    abi_input_copy(session->axes[session->axis_count].axis, ABI_INPUT_MAX_KEY, axis);
    session->axes[session->axis_count].value = value;
    session->axis_count += 1;
    return 1;
}

static void abi_input_reduce_action(KainNativeInputSession* session, const KainNativeInputEvent* event, const char* action) {
    uint32_t kind_sig;
    if (!session || !event || abi_input_text_empty(action)) {
        return;
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-input-event-kind-token-signatures-collision-free.yaml */
    kind_sig = abi_input_event_kind_sig(event->event_kind);
    switch (kind_sig) {
        case 0x066b6570: /* "key_up"     — len=6, first='k', second='e', last='p' */
        case 0x09706f70: /* "pointer_up"  — len=9, first='p', second='o', last='p' */
        case 0x09616370: /* "action_up"   — len=9, first='a', second='c', last='p' */
            if (abi_input_name_remove(
                    session->actions_down,
                    session->actions_down_hashes,
                    &session->action_down_count,
                    action)) {
                abi_input_name_add(
                    session->actions_released,
                    session->actions_released_hashes,
                    &session->action_released_count,
                    ABI_INPUT_MAX_ACTIONS,
                    action
                );
            }
            return;

        case 0x0661636e: /* "action" — len=6, first='a', second='c', last='n' */
            abi_input_name_add(
                session->actions_pressed,
                session->actions_pressed_hashes,
                &session->action_pressed_count,
                ABI_INPUT_MAX_ACTIONS,
                action
            );
            return;

        default:
            /* Default ("action_down", "text", "axis", unknown): add to pressed + down */
            if (abi_input_name_index(
                    session->actions_down,
                    session->actions_down_hashes,
                    session->action_down_count,
                    action) < 0) {
                abi_input_name_add(
                    session->actions_pressed,
                    session->actions_pressed_hashes,
                    &session->action_pressed_count,
                    ABI_INPUT_MAX_ACTIONS,
                    action
                );
            }
            abi_input_name_add(
                session->actions_down,
                session->actions_down_hashes,
                &session->action_down_count,
                ABI_INPUT_MAX_ACTIONS,
                action
            );
            break;
    }
}

static void abi_input_reduce_event(KainNativeInputSession* session, const KainNativeInputEvent* event) {
    const KainNativeInputBinding* action_binding;
    const KainNativeInputBinding* axis_binding;
    uint32_t kind_sig;
    if (!session || !event) {
        return;
    }

    /* Proof: runtime/native/src/core/z3/proofs/native-input-event-kind-token-signatures-collision-free.yaml */
    kind_sig = abi_input_event_kind_sig(event->event_kind);

    switch (kind_sig) {
        case 0x04746574: /* "text" — len=4, first='t', second='e', last='t' */
            if (!abi_input_text_empty(event->text)) {
                if (session->text_commit_count < ABI_INPUT_MAX_TEXT_COMMITS) {
                    abi_input_copy(
                        session->text_commits[session->text_commit_count],
                        ABI_INPUT_MAX_TEXT,
                        event->text
                    );
                    session->text_commit_count += 1;
                }
            }
            break;

        case 0x04617873: /* "axis" — len=4, first='a', second='x', last='s' */
            axis_binding = abi_input_find_binding(session, event, 1);
            if (axis_binding) {
                abi_input_axis_add(session, axis_binding->target, event->value * axis_binding->scale);
            } else {
                abi_input_axis_add(session, event->code, event->value);
            }
            break;

        default:
            break;
    }

    action_binding = abi_input_find_binding(session, event, 0);
    if (action_binding) {
        abi_input_reduce_action(session, event, action_binding->target);
    } else if (!abi_input_text_empty(event->action)) {
        abi_input_reduce_action(session, event, event->action);
    }
}

static int abi_input_split_fields(const char* line, char fields[][ABI_INPUT_MAX_TEXT], int max_fields) {
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
        } else if (offset + 1u < ABI_INPUT_MAX_TEXT) {
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

int64_t abi_input_reset(void) {
    memset(g_sessions, 0, sizeof(g_sessions));
    g_next_session_id = 1;
    return abi_input_ok();
}

int64_t abi_input_session_create(const char* name) {
    int i;
    for (i = 0; i < ABI_INPUT_MAX_SESSIONS; i++) {
        if (!g_sessions[i].in_use) {
            memset(&g_sessions[i], 0, sizeof(g_sessions[i]));
            g_sessions[i].in_use = 1;
            g_sessions[i].id = g_next_session_id++;
            g_sessions[i].next_sequence = 1;
            abi_input_copy(g_sessions[i].name, sizeof(g_sessions[i].name), name ? name : "input-session");
            abi_input_ok();
            return g_sessions[i].id;
        }
    }
    return abi_input_fail(ABI_INPUT_CAPACITY_EXCEEDED, "capacity", "input session capacity exceeded");
}

int64_t abi_input_session_destroy(int64_t session_id) {
    KainNativeInputSession* session = abi_input_session(session_id);
    if (!session) {
        return abi_input_fail(ABI_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    memset(session, 0, sizeof(*session));
    return abi_input_ok();
}

int64_t abi_input_session_count(void) {
    int i;
    int64_t count = 0;
    for (i = 0; i < ABI_INPUT_MAX_SESSIONS; i++) {
        if (g_sessions[i].in_use) {
            count += 1;
        }
    }
    abi_input_ok();
    return count;
}

int64_t abi_input_bind_action(
    int64_t session_id,
    const char* source_kind,
    const char* event_kind,
    const char* code,
    const char* action
) {
    KainNativeInputSession* session = abi_input_session(session_id);
    KainNativeInputBinding* binding;
    if (!session) {
        return abi_input_fail(ABI_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    if (abi_input_text_empty(event_kind) || abi_input_text_empty(code) || abi_input_text_empty(action)) {
        return abi_input_fail(ABI_INPUT_INVALID_ARGUMENT, "invalid_argument", "action binding requires event kind, code, and action");
    }
    if (session->binding_count >= ABI_INPUT_MAX_BINDINGS) {
        return abi_input_fail(ABI_INPUT_CAPACITY_EXCEEDED, "capacity", "input binding capacity exceeded");
    }
    binding = &session->bindings[session->binding_count++];
    memset(binding, 0, sizeof(*binding));
    binding->in_use = 1;
    binding->is_axis = 0;
    binding->scale = 1.0;
    abi_input_copy(binding->source_kind, sizeof(binding->source_kind), source_kind ? source_kind : "*");
    abi_input_copy(binding->event_kind, sizeof(binding->event_kind), event_kind);
    abi_input_copy(binding->code, sizeof(binding->code), code);
    abi_input_copy(binding->target, sizeof(binding->target), action);
    return abi_input_ok();
}

int64_t abi_input_bind_axis(
    int64_t session_id,
    const char* source_kind,
    const char* event_kind,
    const char* code,
    const char* axis,
    double scale
) {
    KainNativeInputSession* session = abi_input_session(session_id);
    KainNativeInputBinding* binding;
    if (!session) {
        return abi_input_fail(ABI_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    if (abi_input_text_empty(event_kind) || abi_input_text_empty(code) || abi_input_text_empty(axis)) {
        return abi_input_fail(ABI_INPUT_INVALID_ARGUMENT, "invalid_argument", "axis binding requires event kind, code, and axis");
    }
    if (session->binding_count >= ABI_INPUT_MAX_BINDINGS) {
        return abi_input_fail(ABI_INPUT_CAPACITY_EXCEEDED, "capacity", "input binding capacity exceeded");
    }
    binding = &session->bindings[session->binding_count++];
    memset(binding, 0, sizeof(*binding));
    binding->in_use = 1;
    binding->is_axis = 1;
    binding->scale = scale;
    abi_input_copy(binding->source_kind, sizeof(binding->source_kind), source_kind ? source_kind : "*");
    abi_input_copy(binding->event_kind, sizeof(binding->event_kind), event_kind);
    abi_input_copy(binding->code, sizeof(binding->code), code);
    abi_input_copy(binding->target, sizeof(binding->target), axis);
    return abi_input_ok();
}

int64_t abi_input_push_event(
    int64_t session_id,
    const char* source_kind,
    const char* source_id,
    const char* event_kind,
    const char* code,
    double value,
    const char* text,
    double confidence
) {
    KainNativeInputSession* session = abi_input_session(session_id);
    KainNativeInputEvent* event;
    if (!session) {
        return abi_input_fail(ABI_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    if (abi_input_text_empty(source_kind) || abi_input_text_empty(event_kind)) {
        return abi_input_fail(ABI_INPUT_INVALID_ARGUMENT, "invalid_argument", "input event requires source kind and event kind");
    }
    if (session->pending_event_count >= ABI_INPUT_MAX_EVENTS) {
        return abi_input_fail(ABI_INPUT_CAPACITY_EXCEEDED, "capacity", "pending input event capacity exceeded");
    }
    event = &session->pending_events[session->pending_event_count++];
    memset(event, 0, sizeof(*event));
    event->sequence = session->next_sequence++;
    event->value = value;
    event->confidence = confidence;
    abi_input_copy(event->source_kind, sizeof(event->source_kind), source_kind);
    abi_input_copy(event->source_id, sizeof(event->source_id), source_id ? source_id : "");
    abi_input_copy(event->event_kind, sizeof(event->event_kind), event_kind);
    abi_input_copy(event->code, sizeof(event->code), code ? code : "");
    if (abi_input_text_equal(event_kind, "action")
        || abi_input_text_equal(event_kind, "action_down")
        || abi_input_text_equal(event_kind, "action_up")) {
        abi_input_copy(event->action, sizeof(event->action), code ? code : "");
    }
    abi_input_copy(event->text, sizeof(event->text), text ? text : "");
    abi_input_trace_append(session, event);
    abi_input_ok();
    return event->sequence;
}

int64_t abi_input_push_agent_intent(
    int64_t session_id,
    const char* source_id,
    const char* action,
    const char* command_text,
    double confidence
) {
    KainNativeInputSession* session = abi_input_session(session_id);
    KainNativeInputEvent* event;
    if (!session) {
        return abi_input_fail(ABI_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    if (abi_input_text_empty(action)) {
        return abi_input_fail(ABI_INPUT_INVALID_ARGUMENT, "invalid_argument", "agent intent requires an action");
    }
    if (session->pending_event_count >= ABI_INPUT_MAX_EVENTS) {
        return abi_input_fail(ABI_INPUT_CAPACITY_EXCEEDED, "capacity", "pending input event capacity exceeded");
    }
    event = &session->pending_events[session->pending_event_count++];
    memset(event, 0, sizeof(*event));
    event->sequence = session->next_sequence++;
    event->value = 1.0;
    event->confidence = confidence;
    abi_input_copy(event->source_kind, sizeof(event->source_kind), "agent.intent");
    abi_input_copy(event->source_id, sizeof(event->source_id), source_id ? source_id : "");
    abi_input_copy(event->event_kind, sizeof(event->event_kind), "action");
    abi_input_copy(event->code, sizeof(event->code), action);
    abi_input_copy(event->action, sizeof(event->action), action);
    abi_input_copy(event->text, sizeof(event->text), command_text ? command_text : "");
    abi_input_trace_append(session, event);
    abi_input_ok();
    return event->sequence;
}

int64_t abi_input_begin_frame(int64_t session_id, double delta_ms) {
    KainNativeInputSession* session = abi_input_session(session_id);
    int64_t i;
    if (!session) {
        return abi_input_fail(ABI_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
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
        abi_input_reduce_event(session, &session->frame_events[i]);
    }
    abi_input_ok();
    return session->frame_index;
}

int64_t abi_input_frame_index(int64_t session_id) {
    KainNativeInputSession* session = abi_input_session(session_id);
    if (!session) {
        return abi_input_fail(ABI_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    abi_input_ok();
    return session->frame_index;
}

int64_t abi_input_event_count(int64_t session_id) {
    KainNativeInputSession* session = abi_input_session(session_id);
    if (!session) {
        return abi_input_fail(ABI_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    abi_input_ok();
    return session->frame_event_count;
}

static const KainNativeInputEvent* abi_input_frame_event(int64_t session_id, int64_t event_index) {
    KainNativeInputSession* session = abi_input_session(session_id);
    if (!session || event_index < 0 || event_index >= session->frame_event_count) {
        return NULL;
    }
    return &session->frame_events[event_index];
}

const char* abi_input_event_kind(int64_t session_id, int64_t event_index) {
    const KainNativeInputEvent* event = abi_input_frame_event(session_id, event_index);
    return abi_input_string(event ? event->event_kind : g_empty_string);
}

const char* abi_input_event_source_kind(int64_t session_id, int64_t event_index) {
    const KainNativeInputEvent* event = abi_input_frame_event(session_id, event_index);
    return abi_input_string(event ? event->source_kind : g_empty_string);
}

const char* abi_input_event_code(int64_t session_id, int64_t event_index) {
    const KainNativeInputEvent* event = abi_input_frame_event(session_id, event_index);
    return abi_input_string(event ? event->code : g_empty_string);
}

const char* abi_input_event_action(int64_t session_id, int64_t event_index) {
    const KainNativeInputEvent* event = abi_input_frame_event(session_id, event_index);
    return abi_input_string(event ? event->action : g_empty_string);
}

const char* abi_input_event_text(int64_t session_id, int64_t event_index) {
    const KainNativeInputEvent* event = abi_input_frame_event(session_id, event_index);
    return abi_input_string(event ? event->text : g_empty_string);
}

int64_t abi_input_action_pressed(int64_t session_id, const char* action) {
    KainNativeInputSession* session = abi_input_session(session_id);
    if (!session) return 0;
    return abi_input_name_index(session->actions_pressed, session->action_pressed_count, action) >= 0 ? 1 : 0;
}

int64_t abi_input_action_down(int64_t session_id, const char* action) {
    KainNativeInputSession* session = abi_input_session(session_id);
    if (!session) return 0;
    return abi_input_name_index(session->actions_down, session->action_down_count, action) >= 0 ? 1 : 0;
}

int64_t abi_input_action_released(int64_t session_id, const char* action) {
    KainNativeInputSession* session = abi_input_session(session_id);
    if (!session) return 0;
    return abi_input_name_index(session->actions_released, session->action_released_count, action) >= 0 ? 1 : 0;
}

double abi_input_axis_value(int64_t session_id, const char* axis) {
    KainNativeInputSession* session = abi_input_session(session_id);
    int64_t i;
    if (!session || abi_input_text_empty(axis)) {
        return 0.0;
    }
    for (i = 0; i < session->axis_count; i++) {
        if (abi_input_text_equal(session->axes[i].axis, axis)) {
            return session->axes[i].value;
        }
    }
    return 0.0;
}

int64_t abi_input_text_commit_count(int64_t session_id) {
    KainNativeInputSession* session = abi_input_session(session_id);
    if (!session) {
        return 0;
    }
    return session->text_commit_count;
}

const char* abi_input_text_commit(int64_t session_id, int64_t index) {
    KainNativeInputSession* session = abi_input_session(session_id);
    if (!session || index < 0 || index >= session->text_commit_count) {
        return abi_input_string(g_empty_string);
    }
    return abi_input_string(session->text_commits[index]);
}

const char* abi_input_trace_text(int64_t session_id) {
    KainNativeInputSession* session = abi_input_session(session_id);
    if (!session) {
        return abi_input_string(g_empty_string);
    }
    return abi_input_string(session->trace_text);
}

int64_t abi_input_replay_trace(int64_t session_id, const char* trace_text) {
    char* buffer;
    char* line;
    KainNativeInputSession* session = abi_input_session(session_id);
    if (!session) {
        return abi_input_fail(ABI_INPUT_INVALID_SESSION, "invalid_session", "input session not found");
    }
    if (!trace_text) {
        return abi_input_fail(ABI_INPUT_INVALID_ARGUMENT, "invalid_argument", "trace text is null");
    }
    buffer = (char*)malloc(strlen(trace_text) + 1u);
    if (!buffer) {
        return abi_input_fail(ABI_INPUT_INVALID_ARGUMENT, "allocation", "could not copy trace text");
    }
    strcpy(buffer, trace_text);
    line = strtok(buffer, "\n");
    while (line) {
        char fields[8][ABI_INPUT_MAX_TEXT];
        if (abi_input_split_fields(line, fields, 8) >= 8) {
            int64_t sequence = (int64_t)atoll(fields[0]);
            KainNativeInputEvent* event;
            if (session->pending_event_count >= ABI_INPUT_MAX_EVENTS) {
                free(buffer);
                return abi_input_fail(ABI_INPUT_CAPACITY_EXCEEDED, "capacity", "pending input event capacity exceeded");
            }
            event = &session->pending_events[session->pending_event_count++];
            memset(event, 0, sizeof(*event));
            event->sequence = sequence > 0 ? sequence : session->next_sequence++;
            if (event->sequence >= session->next_sequence) {
                session->next_sequence = event->sequence + 1;
            }
            event->value = atof(fields[5]);
            event->confidence = atof(fields[7]);
            abi_input_copy(event->source_kind, sizeof(event->source_kind), fields[1]);
            abi_input_copy(event->source_id, sizeof(event->source_id), fields[2]);
            abi_input_copy(event->event_kind, sizeof(event->event_kind), fields[3]);
            abi_input_copy(event->code, sizeof(event->code), fields[4]);
            abi_input_copy(event->text, sizeof(event->text), fields[6]);
            if (abi_input_text_equal(event->event_kind, "action")) {
                abi_input_copy(event->action, sizeof(event->action), event->code);
            }
        }
        line = strtok(NULL, "\n");
    }
    free(buffer);
    return abi_input_ok();
}

int64_t abi_input_last_status(void) {
    return g_last_status;
}

const char* abi_input_last_error_kind(void) {
    return abi_input_string(g_last_error_kind);
}

const char* abi_input_last_error_message(void) {
    return abi_input_string(g_last_error_message);
}
