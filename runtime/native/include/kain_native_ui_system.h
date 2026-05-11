#ifndef KAIN_NATIVE_UI_SYSTEM_H
#define KAIN_NATIVE_UI_SYSTEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAIN_NATIVE_UI_MAX_SESSIONS 16
#define KAIN_NATIVE_UI_MAX_NODES 4096
#define KAIN_NATIVE_UI_MAX_STYLES 8192
#define KAIN_NATIVE_UI_MAX_DRAW_COMMANDS 8192
#define KAIN_NATIVE_UI_MAX_EVENTS 1024
#define KAIN_NATIVE_UI_MAX_TEXT 256
#define KAIN_NATIVE_UI_MAX_KEY 96

typedef enum KainNativeUiStatus {
    KAIN_NATIVE_UI_OK = 0,
    KAIN_NATIVE_UI_INVALID_SESSION = -1,
    KAIN_NATIVE_UI_INVALID_NODE = -2,
    KAIN_NATIVE_UI_CAPACITY_EXCEEDED = -3,
    KAIN_NATIVE_UI_INVALID_ARGUMENT = -4,
} KainNativeUiStatus;

int64_t kain_native_ui_reset(void);

int64_t kain_native_ui_session_create(const char* app_name, int64_t width, int64_t height);
int64_t kain_native_ui_session_destroy(int64_t session_id);
int64_t kain_native_ui_session_count(void);
int64_t kain_native_ui_window_open(int64_t session_id, const char* title, int64_t width, int64_t height);
int64_t kain_native_ui_window_close(int64_t session_id);

int64_t kain_native_ui_begin_frame(int64_t session_id, double delta_ms);
int64_t kain_native_ui_end_frame(int64_t session_id);
int64_t kain_native_ui_present(int64_t session_id);
int64_t kain_native_ui_frame_index(int64_t session_id);
int64_t kain_native_ui_last_presented_frame(int64_t session_id);

int64_t kain_native_ui_node_create(int64_t session_id, const char* kind);
int64_t kain_native_ui_node_destroy(int64_t session_id, int64_t node_id);
int64_t kain_native_ui_node_count(int64_t session_id);
int64_t kain_native_ui_node_exists(int64_t session_id, int64_t node_id);
int64_t kain_native_ui_node_set_parent(int64_t session_id, int64_t node_id, int64_t parent_id);
int64_t kain_native_ui_node_parent(int64_t session_id, int64_t node_id);
int64_t kain_native_ui_node_child_count(int64_t session_id, int64_t node_id);

int64_t kain_native_ui_node_set_rect(
    int64_t session_id,
    int64_t node_id,
    double x,
    double y,
    double width,
    double height
);
double kain_native_ui_node_x(int64_t session_id, int64_t node_id);
double kain_native_ui_node_y(int64_t session_id, int64_t node_id);
double kain_native_ui_node_width(int64_t session_id, int64_t node_id);
double kain_native_ui_node_height(int64_t session_id, int64_t node_id);

int64_t kain_native_ui_node_set_text(int64_t session_id, int64_t node_id, const char* text);
const char* kain_native_ui_node_text(int64_t session_id, int64_t node_id);
const char* kain_native_ui_node_kind(int64_t session_id, int64_t node_id);

int64_t kain_native_ui_node_set_flag(int64_t session_id, int64_t node_id, const char* flag, int64_t enabled);
int64_t kain_native_ui_node_has_flag(int64_t session_id, int64_t node_id, const char* flag);
int64_t kain_native_ui_node_set_style_i64(int64_t session_id, int64_t node_id, const char* key, int64_t value);
int64_t kain_native_ui_node_set_style_f64(int64_t session_id, int64_t node_id, const char* key, double value);
int64_t kain_native_ui_node_set_style_string(int64_t session_id, int64_t node_id, const char* key, const char* value);
int64_t kain_native_ui_node_style_i64(int64_t session_id, int64_t node_id, const char* key, int64_t fallback);
double kain_native_ui_node_style_f64(int64_t session_id, int64_t node_id, const char* key, double fallback);
const char* kain_native_ui_node_style_string(int64_t session_id, int64_t node_id, const char* key, const char* fallback);

int64_t kain_native_ui_focus(int64_t session_id, int64_t node_id);
int64_t kain_native_ui_focused_node(int64_t session_id);
int64_t kain_native_ui_hit_test(int64_t session_id, double x, double y);
int64_t kain_native_ui_mark_dirty(int64_t session_id, int64_t node_id, int64_t reason);
int64_t kain_native_ui_dirty_count(int64_t session_id);

int64_t kain_native_ui_push_event(
    int64_t session_id,
    const char* kind,
    int64_t target_node_id,
    double x,
    double y,
    int64_t key_code,
    const char* text
);
int64_t kain_native_ui_poll_event(int64_t session_id);
const char* kain_native_ui_event_kind(int64_t session_id);
int64_t kain_native_ui_event_target(int64_t session_id);
double kain_native_ui_event_x(int64_t session_id);
double kain_native_ui_event_y(int64_t session_id);
int64_t kain_native_ui_event_key_code(int64_t session_id);
const char* kain_native_ui_event_text(int64_t session_id);

int64_t kain_native_ui_draw_rect(
    int64_t session_id,
    int64_t node_id,
    double x,
    double y,
    double width,
    double height,
    const char* style_key
);
int64_t kain_native_ui_draw_text(
    int64_t session_id,
    int64_t node_id,
    double x,
    double y,
    const char* text,
    const char* style_key
);
int64_t kain_native_ui_draw_command_count(int64_t session_id);
const char* kain_native_ui_draw_command_kind(int64_t session_id, int64_t command_index);
int64_t kain_native_ui_draw_command_node(int64_t session_id, int64_t command_index);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_NATIVE_UI_SYSTEM_H */
