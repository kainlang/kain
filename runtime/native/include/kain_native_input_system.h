#ifndef KAIN_NATIVE_INPUT_SYSTEM_H
#define KAIN_NATIVE_INPUT_SYSTEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAIN_NATIVE_INPUT_MAX_SESSIONS 16
#define KAIN_NATIVE_INPUT_MAX_BINDINGS 512
#define KAIN_NATIVE_INPUT_MAX_EVENTS 1024
#define KAIN_NATIVE_INPUT_MAX_ACTIONS 256
#define KAIN_NATIVE_INPUT_MAX_AXES 128
#define KAIN_NATIVE_INPUT_MAX_TEXT_COMMITS 256
#define KAIN_NATIVE_INPUT_MAX_TEXT 256
#define KAIN_NATIVE_INPUT_MAX_KEY 96
#define KAIN_NATIVE_INPUT_MAX_TRACE_TEXT 65536

typedef enum KainNativeInputStatus {
    KAIN_NATIVE_INPUT_OK = 0,
    KAIN_NATIVE_INPUT_INVALID_SESSION = -1,
    KAIN_NATIVE_INPUT_CAPACITY_EXCEEDED = -2,
    KAIN_NATIVE_INPUT_INVALID_ARGUMENT = -3,
} KainNativeInputStatus;

int64_t kain_native_input_reset(void);
int64_t kain_native_input_session_create(const char* name);
int64_t kain_native_input_session_destroy(int64_t session_id);
int64_t kain_native_input_session_count(void);

int64_t kain_native_input_bind_action(
    int64_t session_id,
    const char* source_kind,
    const char* event_kind,
    const char* code,
    const char* action
);
int64_t kain_native_input_bind_axis(
    int64_t session_id,
    const char* source_kind,
    const char* event_kind,
    const char* code,
    const char* axis,
    double scale
);

int64_t kain_native_input_push_event(
    int64_t session_id,
    const char* source_kind,
    const char* source_id,
    const char* event_kind,
    const char* code,
    double value,
    const char* text,
    double confidence
);
int64_t kain_native_input_push_agent_intent(
    int64_t session_id,
    const char* source_id,
    const char* action,
    const char* command_text,
    double confidence
);

int64_t kain_native_input_begin_frame(int64_t session_id, double delta_ms);
int64_t kain_native_input_frame_index(int64_t session_id);

int64_t kain_native_input_event_count(int64_t session_id);
const char* kain_native_input_event_kind(int64_t session_id, int64_t event_index);
const char* kain_native_input_event_source_kind(int64_t session_id, int64_t event_index);
const char* kain_native_input_event_code(int64_t session_id, int64_t event_index);
const char* kain_native_input_event_action(int64_t session_id, int64_t event_index);
const char* kain_native_input_event_text(int64_t session_id, int64_t event_index);

int64_t kain_native_input_action_pressed(int64_t session_id, const char* action);
int64_t kain_native_input_action_down(int64_t session_id, const char* action);
int64_t kain_native_input_action_released(int64_t session_id, const char* action);
double kain_native_input_axis_value(int64_t session_id, const char* axis);

int64_t kain_native_input_text_commit_count(int64_t session_id);
const char* kain_native_input_text_commit(int64_t session_id, int64_t index);

const char* kain_native_input_trace_text(int64_t session_id);
int64_t kain_native_input_replay_trace(int64_t session_id, const char* trace_text);

int64_t kain_native_input_last_status(void);
const char* kain_native_input_last_error_kind(void);
const char* kain_native_input_last_error_message(void);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_NATIVE_INPUT_SYSTEM_H */
