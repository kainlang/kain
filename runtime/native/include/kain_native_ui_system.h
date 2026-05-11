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
#define KAIN_NATIVE_UI_MAX_RESOURCES 2048
#define KAIN_NATIVE_UI_MAX_MENUS 256
#define KAIN_NATIVE_UI_MAX_MENU_ITEMS 2048
#define KAIN_NATIVE_UI_MAX_DIALOGS 128
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

int64_t kain_native_ui_host_attach(int64_t session_id, const char* backend_id);
int64_t kain_native_ui_host_pump(int64_t session_id);
int64_t kain_native_ui_host_present(int64_t session_id);
int64_t kain_native_ui_host_presented_draw_count(int64_t session_id);
int64_t kain_native_ui_host_frame_hash(int64_t session_id);
int64_t kain_native_ui_host_should_close(int64_t session_id);
const char* kain_native_ui_host_backend(int64_t session_id);

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
int64_t kain_native_ui_node_set_stable_key(int64_t session_id, int64_t node_id, const char* stable_key);
const char* kain_native_ui_node_stable_key(int64_t session_id, int64_t node_id);
int64_t kain_native_ui_node_find_by_stable_key(int64_t session_id, const char* stable_key);

int64_t kain_native_ui_accessibility_set_role(int64_t session_id, int64_t node_id, const char* role);
int64_t kain_native_ui_accessibility_set_label(int64_t session_id, int64_t node_id, const char* label);
const char* kain_native_ui_accessibility_role(int64_t session_id, int64_t node_id);
const char* kain_native_ui_accessibility_label(int64_t session_id, int64_t node_id);

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
int64_t kain_native_ui_draw_command_resource(int64_t session_id, int64_t command_index);
double kain_native_ui_draw_command_x(int64_t session_id, int64_t command_index);
double kain_native_ui_draw_command_y(int64_t session_id, int64_t command_index);
double kain_native_ui_draw_command_width(int64_t session_id, int64_t command_index);
double kain_native_ui_draw_command_height(int64_t session_id, int64_t command_index);
const char* kain_native_ui_draw_command_text(int64_t session_id, int64_t command_index);
const char* kain_native_ui_draw_command_style(int64_t session_id, int64_t command_index);

int64_t kain_native_ui_resource_create(int64_t session_id, const char* resource_type, const char* key, int64_t width, int64_t height, int64_t byte_length);
int64_t kain_native_ui_font_create(int64_t session_id, const char* key, const char* family, double size);
int64_t kain_native_ui_texture_create(int64_t session_id, const char* key, int64_t width, int64_t height, const char* format, int64_t byte_length);
int64_t kain_native_ui_canvas_create(int64_t session_id, const char* key, int64_t width, int64_t height);
int64_t kain_native_ui_shader_create(int64_t session_id, const char* key, const char* stage, int64_t byte_length);
int64_t kain_native_ui_resource_count(int64_t session_id);
int64_t kain_native_ui_resource_exists(int64_t session_id, int64_t resource_id);
const char* kain_native_ui_resource_type(int64_t session_id, int64_t resource_id);
const char* kain_native_ui_resource_key(int64_t session_id, int64_t resource_id);
int64_t kain_native_ui_resource_width(int64_t session_id, int64_t resource_id);
int64_t kain_native_ui_resource_height(int64_t session_id, int64_t resource_id);
int64_t kain_native_ui_resource_byte_length(int64_t session_id, int64_t resource_id);
double kain_native_ui_text_measure_width(int64_t session_id, int64_t font_resource_id, const char* text);
double kain_native_ui_text_measure_height(int64_t session_id, int64_t font_resource_id, const char* text);
int64_t kain_native_ui_draw_resource(int64_t session_id, int64_t node_id, int64_t resource_id, double x, double y, double width, double height, const char* style_key);

int64_t kain_native_ui_clipboard_set_text(int64_t session_id, const char* text);
const char* kain_native_ui_clipboard_text(int64_t session_id);
int64_t kain_native_ui_ime_begin(int64_t session_id, int64_t node_id);
int64_t kain_native_ui_ime_commit_text(int64_t session_id, const char* text);
int64_t kain_native_ui_ime_end(int64_t session_id);
int64_t kain_native_ui_ime_active_node(int64_t session_id);
const char* kain_native_ui_ime_text(int64_t session_id);

int64_t kain_native_ui_drag_begin(int64_t session_id, int64_t node_id, const char* payload, double x, double y);
int64_t kain_native_ui_drag_update(int64_t session_id, double x, double y, int64_t drop_target_node_id);
int64_t kain_native_ui_drag_drop(int64_t session_id, int64_t drop_target_node_id);
int64_t kain_native_ui_drag_active_node(int64_t session_id);
int64_t kain_native_ui_drag_drop_target(int64_t session_id);
double kain_native_ui_drag_x(int64_t session_id);
double kain_native_ui_drag_y(int64_t session_id);
const char* kain_native_ui_drag_payload(int64_t session_id);

int64_t kain_native_ui_menu_create(int64_t session_id, const char* key);
int64_t kain_native_ui_menu_add_item(int64_t session_id, int64_t menu_id, const char* key, const char* label, int64_t command_id);
int64_t kain_native_ui_menu_open(int64_t session_id, int64_t menu_id, double x, double y);
int64_t kain_native_ui_menu_active(int64_t session_id);
int64_t kain_native_ui_menu_item_count(int64_t session_id, int64_t menu_id);
const char* kain_native_ui_menu_item_label(int64_t session_id, int64_t menu_id, int64_t item_index);
int64_t kain_native_ui_menu_item_command(int64_t session_id, int64_t menu_id, int64_t item_index);

int64_t kain_native_ui_dialog_request(int64_t session_id, const char* kind, const char* title, const char* message);
int64_t kain_native_ui_dialog_active(int64_t session_id);
const char* kain_native_ui_dialog_kind(int64_t session_id, int64_t dialog_id);
const char* kain_native_ui_dialog_title(int64_t session_id, int64_t dialog_id);
const char* kain_native_ui_dialog_message(int64_t session_id, int64_t dialog_id);
int64_t kain_native_ui_dialog_respond(int64_t session_id, int64_t dialog_id, int64_t result, const char* response_text);
int64_t kain_native_ui_dialog_poll_response(int64_t session_id);
const char* kain_native_ui_dialog_response_text(int64_t session_id);

int64_t kain_native_ui_hot_reload_begin(int64_t session_id, const char* revision_key);
int64_t kain_native_ui_hot_reload_commit(int64_t session_id);
int64_t kain_native_ui_hot_reload_generation(int64_t session_id);
const char* kain_native_ui_hot_reload_key(int64_t session_id);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_NATIVE_UI_SYSTEM_H */
