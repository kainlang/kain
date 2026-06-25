#ifndef ABI_UI_SYSTEM_H
#define ABI_UI_SYSTEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ABI_UI_MAX_SESSIONS 16
#define ABI_UI_MAX_NODES 4096
#define ABI_UI_MAX_STYLES 8192
#define ABI_UI_MAX_STATE 8192
#define ABI_UI_MAX_DRAW_COMMANDS 8192
#define ABI_UI_MAX_EVENTS 1024
#define ABI_UI_MAX_RESOURCES 2048
#define ABI_UI_MAX_MENUS 256
#define ABI_UI_MAX_MENU_ITEMS 2048
#define ABI_UI_MAX_DIALOGS 128
#define ABI_UI_MAX_TEXT 256
#define ABI_UI_MAX_KEY 96

#if (ABI_UI_MAX_NODES & (ABI_UI_MAX_NODES - 1)) != 0
#error "ABI_UI_MAX_NODES must be a power of two"
#endif
#if (ABI_UI_MAX_STYLES & (ABI_UI_MAX_STYLES - 1)) != 0
#error "ABI_UI_MAX_STYLES must be a power of two"
#endif
#if (ABI_UI_MAX_STATE & (ABI_UI_MAX_STATE - 1)) != 0
#error "ABI_UI_MAX_STATE must be a power of two"
#endif
#if (ABI_UI_MAX_RESOURCES & (ABI_UI_MAX_RESOURCES - 1)) != 0
#error "ABI_UI_MAX_RESOURCES must be a power of two"
#endif
#if (ABI_UI_MAX_MENUS & (ABI_UI_MAX_MENUS - 1)) != 0
#error "ABI_UI_MAX_MENUS must be a power of two"
#endif
#if (ABI_UI_MAX_MENU_ITEMS & (ABI_UI_MAX_MENU_ITEMS - 1)) != 0
#error "ABI_UI_MAX_MENU_ITEMS must be a power of two"
#endif
#if (ABI_UI_MAX_DIALOGS & (ABI_UI_MAX_DIALOGS - 1)) != 0
#error "ABI_UI_MAX_DIALOGS must be a power of two"
#endif

#define ABI_UI_NODE_INDEX_CAPACITY ABI_UI_MAX_NODES
#define ABI_UI_NODE_INDEX_MASK (ABI_UI_NODE_INDEX_CAPACITY - 1u)
#define ABI_UI_STABLE_KEY_INDEX_CAPACITY ABI_UI_MAX_NODES
#define ABI_UI_STABLE_KEY_INDEX_MASK (ABI_UI_STABLE_KEY_INDEX_CAPACITY - 1u)
#define ABI_UI_STYLE_INDEX_CAPACITY ABI_UI_MAX_STYLES
#define ABI_UI_STYLE_INDEX_MASK (ABI_UI_STYLE_INDEX_CAPACITY - 1u)
#define ABI_UI_STATE_INDEX_CAPACITY ABI_UI_MAX_STATE
#define ABI_UI_STATE_INDEX_MASK (ABI_UI_STATE_INDEX_CAPACITY - 1u)
#define ABI_UI_RESOURCE_INDEX_CAPACITY ABI_UI_MAX_RESOURCES
#define ABI_UI_RESOURCE_INDEX_MASK (ABI_UI_RESOURCE_INDEX_CAPACITY - 1u)
#define ABI_UI_MENU_INDEX_CAPACITY ABI_UI_MAX_MENUS
#define ABI_UI_MENU_INDEX_MASK (ABI_UI_MENU_INDEX_CAPACITY - 1u)
#define ABI_UI_DIALOG_INDEX_CAPACITY ABI_UI_MAX_DIALOGS
#define ABI_UI_DIALOG_INDEX_MASK (ABI_UI_DIALOG_INDEX_CAPACITY - 1u)

#define ABI_UI_NODE_OCCUPANCY_WORD_COUNT (ABI_UI_MAX_NODES / 64u)
#define ABI_UI_STYLE_OCCUPANCY_WORD_COUNT (ABI_UI_MAX_STYLES / 64u)
#define ABI_UI_STATE_OCCUPANCY_WORD_COUNT (ABI_UI_MAX_STATE / 64u)
#define ABI_UI_RESOURCE_OCCUPANCY_WORD_COUNT (ABI_UI_MAX_RESOURCES / 64u)
#define ABI_UI_MENU_OCCUPANCY_WORD_COUNT (ABI_UI_MAX_MENUS / 64u)
#define ABI_UI_MENU_ITEM_OCCUPANCY_WORD_COUNT (ABI_UI_MAX_MENU_ITEMS / 64u)
#define ABI_UI_DIALOG_OCCUPANCY_WORD_COUNT (ABI_UI_MAX_DIALOGS / 64u)

typedef enum KainNativeUiStatus {
    ABI_UI_OK = 0,
    ABI_UI_INVALID_SESSION = -1,
    ABI_UI_INVALID_NODE = -2,
    ABI_UI_CAPACITY_EXCEEDED = -3,
    ABI_UI_INVALID_ARGUMENT = -4,
} KainNativeUiStatus;

int64_t abi_ui_reset(void);

int64_t abi_ui_session_create(const char* app_name, int64_t width, int64_t height);
int64_t abi_ui_session_destroy(int64_t session_id);
int64_t abi_ui_session_count(void);
int64_t abi_ui_window_open(int64_t session_id, const char* title, int64_t width, int64_t height);
int64_t abi_ui_window_close(int64_t session_id);

int64_t abi_ui_begin_frame(int64_t session_id, double delta_ms);
int64_t abi_ui_end_frame(int64_t session_id);
int64_t abi_ui_present(int64_t session_id);
int64_t abi_ui_frame_index(int64_t session_id);
int64_t abi_ui_last_presented_frame(int64_t session_id);

int64_t abi_ui_host_attach(int64_t session_id, const char* backend_id);
int64_t abi_ui_host_pump(int64_t session_id);
int64_t abi_ui_host_present(int64_t session_id);
int64_t abi_ui_host_presented_draw_count(int64_t session_id);
int64_t abi_ui_host_frame_hash(int64_t session_id);
int64_t abi_ui_host_should_close(int64_t session_id);
const char* abi_ui_host_backend(int64_t session_id);

int64_t abi_ui_node_create(int64_t session_id, const char* kind);
int64_t abi_ui_node_destroy(int64_t session_id, int64_t node_id);
int64_t abi_ui_node_count(int64_t session_id);
int64_t abi_ui_node_exists(int64_t session_id, int64_t node_id);
int64_t abi_ui_node_set_parent(int64_t session_id, int64_t node_id, int64_t parent_id);
int64_t abi_ui_node_parent(int64_t session_id, int64_t node_id);
int64_t abi_ui_node_child_count(int64_t session_id, int64_t node_id);

int64_t abi_ui_node_set_rect(
    int64_t session_id,
    int64_t node_id,
    double x,
    double y,
    double width,
    double height
);
double abi_ui_node_x(int64_t session_id, int64_t node_id);
double abi_ui_node_y(int64_t session_id, int64_t node_id);
double abi_ui_node_width(int64_t session_id, int64_t node_id);
double abi_ui_node_height(int64_t session_id, int64_t node_id);

int64_t abi_ui_node_set_text(int64_t session_id, int64_t node_id, const char* text);
const char* abi_ui_node_text(int64_t session_id, int64_t node_id);
const char* abi_ui_node_kind(int64_t session_id, int64_t node_id);
int64_t abi_ui_node_set_stable_key(int64_t session_id, int64_t node_id, const char* stable_key);
const char* abi_ui_node_stable_key(int64_t session_id, int64_t node_id);
int64_t abi_ui_node_find_by_stable_key(int64_t session_id, const char* stable_key);

int64_t abi_ui_accessibility_set_role(int64_t session_id, int64_t node_id, const char* role);
int64_t abi_ui_accessibility_set_label(int64_t session_id, int64_t node_id, const char* label);
const char* abi_ui_accessibility_role(int64_t session_id, int64_t node_id);
const char* abi_ui_accessibility_label(int64_t session_id, int64_t node_id);

int64_t abi_ui_node_set_flag(int64_t session_id, int64_t node_id, const char* flag, int64_t enabled);
int64_t abi_ui_node_has_flag(int64_t session_id, int64_t node_id, const char* flag);
int64_t abi_ui_node_set_style_i64(int64_t session_id, int64_t node_id, const char* key, int64_t value);
int64_t abi_ui_node_set_style_f64(int64_t session_id, int64_t node_id, const char* key, double value);
int64_t abi_ui_node_set_style_string(int64_t session_id, int64_t node_id, const char* key, const char* value);
int64_t abi_ui_node_style_i64(int64_t session_id, int64_t node_id, const char* key, int64_t fallback);
double abi_ui_node_style_f64(int64_t session_id, int64_t node_id, const char* key, double fallback);
const char* abi_ui_node_style_string(int64_t session_id, int64_t node_id, const char* key, const char* fallback);
int64_t abi_ui_node_set_state_i64(int64_t session_id, int64_t node_id, const char* key, int64_t value);
int64_t abi_ui_node_set_state_f64(int64_t session_id, int64_t node_id, const char* key, double value);
int64_t abi_ui_node_set_state_string(int64_t session_id, int64_t node_id, const char* key, const char* value);
int64_t abi_ui_node_state_i64(int64_t session_id, int64_t node_id, const char* key, int64_t fallback);
double abi_ui_node_state_f64(int64_t session_id, int64_t node_id, const char* key, double fallback);
const char* abi_ui_node_state_string(int64_t session_id, int64_t node_id, const char* key, const char* fallback);
int64_t abi_ui_state_count(int64_t session_id);

int64_t abi_ui_focus(int64_t session_id, int64_t node_id);
int64_t abi_ui_focused_node(int64_t session_id);
int64_t abi_ui_hit_test(int64_t session_id, double x, double y);
int64_t abi_ui_mark_dirty(int64_t session_id, int64_t node_id, int64_t reason);
int64_t abi_ui_dirty_count(int64_t session_id);

int64_t abi_ui_push_event(
    int64_t session_id,
    const char* kind,
    int64_t target_node_id,
    double x,
    double y,
    int64_t key_code,
    const char* text
);
int64_t abi_ui_poll_event(int64_t session_id);
const char* abi_ui_event_kind(int64_t session_id);
int64_t abi_ui_event_target(int64_t session_id);
double abi_ui_event_x(int64_t session_id);
double abi_ui_event_y(int64_t session_id);
int64_t abi_ui_event_key_code(int64_t session_id);
const char* abi_ui_event_text(int64_t session_id);

int64_t abi_ui_draw_rect(
    int64_t session_id,
    int64_t node_id,
    double x,
    double y,
    double width,
    double height,
    const char* style_key
);
int64_t abi_ui_draw_text(
    int64_t session_id,
    int64_t node_id,
    int64_t font_resource_id,
    double x,
    double y,
    const char* text,
    const char* style_key
);
int64_t abi_ui_draw_command_count(int64_t session_id);
const char* abi_ui_draw_command_kind(int64_t session_id, int64_t command_index);
int64_t abi_ui_draw_command_node(int64_t session_id, int64_t command_index);
int64_t abi_ui_draw_command_resource(int64_t session_id, int64_t command_index);
double abi_ui_draw_command_x(int64_t session_id, int64_t command_index);
double abi_ui_draw_command_y(int64_t session_id, int64_t command_index);
double abi_ui_draw_command_width(int64_t session_id, int64_t command_index);
double abi_ui_draw_command_height(int64_t session_id, int64_t command_index);
const char* abi_ui_draw_command_text(int64_t session_id, int64_t command_index);
const char* abi_ui_draw_command_style(int64_t session_id, int64_t command_index);
int64_t abi_ui_draw_command_font(int64_t session_id, int64_t command_index);

int64_t abi_ui_resource_create(int64_t session_id, const char* resource_type, const char* key, int64_t width, int64_t height, int64_t byte_length);
int64_t abi_ui_font_create(int64_t session_id, const char* key, const char* family, double size);
int64_t abi_ui_texture_create(int64_t session_id, const char* key, int64_t width, int64_t height, const char* format, int64_t byte_length);
int64_t abi_ui_canvas_create(int64_t session_id, const char* key, int64_t width, int64_t height);
int64_t abi_ui_shader_create(int64_t session_id, const char* key, const char* stage, int64_t byte_length);
int64_t abi_ui_resource_set_bytes(int64_t session_id, int64_t resource_id, const uint8_t* bytes, int64_t byte_length);
int64_t abi_ui_resource_set_bytes_hex(int64_t session_id, int64_t resource_id, const char* bytes_hex);
int64_t abi_ui_resource_count(int64_t session_id);
int64_t abi_ui_resource_exists(int64_t session_id, int64_t resource_id);
const char* abi_ui_resource_type(int64_t session_id, int64_t resource_id);
const char* abi_ui_resource_key(int64_t session_id, int64_t resource_id);
int64_t abi_ui_resource_width(int64_t session_id, int64_t resource_id);
int64_t abi_ui_resource_height(int64_t session_id, int64_t resource_id);
int64_t abi_ui_resource_byte_length(int64_t session_id, int64_t resource_id);
double abi_ui_text_measure_width(int64_t session_id, int64_t font_resource_id, const char* text);
double abi_ui_text_measure_height(int64_t session_id, int64_t font_resource_id, const char* text);
int64_t abi_ui_font_ascent(int64_t session_id, int64_t font_resource_id);
int64_t abi_ui_font_descent(int64_t session_id, int64_t font_resource_id);
int64_t abi_ui_font_line_gap(int64_t session_id, int64_t font_resource_id);
int64_t abi_ui_draw_resource(int64_t session_id, int64_t node_id, int64_t resource_id, double x, double y, double width, double height, const char* style_key);

int64_t abi_ui_clipboard_set_text(int64_t session_id, const char* text);
const char* abi_ui_clipboard_text(int64_t session_id);
int64_t abi_ui_ime_begin(int64_t session_id, int64_t node_id);
int64_t abi_ui_ime_commit_text(int64_t session_id, const char* text);
int64_t abi_ui_ime_end(int64_t session_id);
int64_t abi_ui_ime_active_node(int64_t session_id);
const char* abi_ui_ime_text(int64_t session_id);

int64_t abi_ui_drag_begin(int64_t session_id, int64_t node_id, const char* payload, double x, double y);
int64_t abi_ui_drag_update(int64_t session_id, double x, double y, int64_t drop_target_node_id);
int64_t abi_ui_drag_drop(int64_t session_id, int64_t drop_target_node_id);
int64_t abi_ui_drag_active_node(int64_t session_id);
int64_t abi_ui_drag_drop_target(int64_t session_id);
double abi_ui_drag_x(int64_t session_id);
double abi_ui_drag_y(int64_t session_id);
const char* abi_ui_drag_payload(int64_t session_id);

int64_t abi_ui_menu_create(int64_t session_id, const char* key);
int64_t abi_ui_menu_add_item(int64_t session_id, int64_t menu_id, const char* key, const char* label, int64_t command_id);
int64_t abi_ui_menu_open(int64_t session_id, int64_t menu_id, double x, double y);
int64_t abi_ui_menu_active(int64_t session_id);
int64_t abi_ui_menu_item_count(int64_t session_id, int64_t menu_id);
const char* abi_ui_menu_item_label(int64_t session_id, int64_t menu_id, int64_t item_index);
int64_t abi_ui_menu_item_command(int64_t session_id, int64_t menu_id, int64_t item_index);

int64_t abi_ui_dialog_request(int64_t session_id, const char* kind, const char* title, const char* message);
int64_t abi_ui_dialog_active(int64_t session_id);
const char* abi_ui_dialog_kind(int64_t session_id, int64_t dialog_id);
const char* abi_ui_dialog_title(int64_t session_id, int64_t dialog_id);
const char* abi_ui_dialog_message(int64_t session_id, int64_t dialog_id);
int64_t abi_ui_dialog_respond(int64_t session_id, int64_t dialog_id, int64_t result, const char* response_text);
int64_t abi_ui_dialog_poll_response(int64_t session_id);
const char* abi_ui_dialog_response_text(int64_t session_id);

int64_t abi_ui_hot_reload_begin(int64_t session_id, const char* revision_key);
int64_t abi_ui_hot_reload_commit(int64_t session_id);
int64_t abi_ui_hot_reload_generation(int64_t session_id);
const char* abi_ui_hot_reload_key(int64_t session_id);

// ── Widget Library ABI (from widgets/ui_widget.h) ─────────────────────
int64_t abi_ui_widget_create(int64_t session_id);
void    abi_ui_widget_destroy(int64_t ctx_ptr);
void    abi_ui_widget_begin_frame(int64_t ctx_ptr);
void    abi_ui_widget_end_frame(int64_t ctx_ptr);
int64_t abi_ui_widget_load_font(int64_t ctx_ptr, const char* filepath, double size);
int64_t abi_ui_widget_load_default_font(int64_t ctx_ptr, double size);
int64_t abi_ui_widget_button(int64_t ctx_ptr, const char* label);
int64_t abi_ui_widget_label(int64_t ctx_ptr, const char* text);
int64_t abi_ui_widget_checkbox(int64_t ctx_ptr, const char* label, int64_t current_value);
int64_t abi_ui_widget_slider(int64_t ctx_ptr, double current_value, double lo, double hi);
int64_t abi_ui_widget_textbox_poll(int64_t ctx_ptr, int64_t buf_ptr, int64_t buf_size);
int64_t abi_ui_widget_panel_begin(int64_t ctx_ptr, const char* title, double x, double y, double w, double h);
void    abi_ui_widget_panel_end(int64_t ctx_ptr);
int64_t abi_ui_widget_progress(int64_t ctx_ptr, const char* label, double value, double max_val);
int64_t abi_ui_widget_window(int64_t ctx_ptr, const char* title, double x, double y, double w, double h, int64_t open);
int64_t abi_ui_widget_layout_row(int64_t ctx_ptr, int64_t count, const int64_t* widths);
int64_t abi_ui_widget_layout_column(int64_t ctx_ptr, int64_t count, const int64_t* heights);
int64_t abi_ui_widget_layout_set_next(int64_t ctx_ptr, int64_t w, int64_t h);
const char* abi_ui_widget_textbox(int64_t ctx_ptr, const char* text, int64_t max_len);
int64_t abi_ui_widget_layout_begin(int64_t ctx_ptr, int64_t count, int64_t layout_type);
int64_t abi_ui_widget_layout_set_size(int64_t ctx_ptr, int64_t index, int64_t size);

// ── Font ABI (load / glyph access) ──────────────────────────────────
// Also declared in ui_font.h. Forward-declare KainUiGlyph here so
// ui_system.h is self-contained for code that only includes this header.
struct KainUiGlyph;
typedef struct KainUiGlyph KainUiGlyph;

int64_t abi_ui_font_load_ttf(
    int64_t session_id,
    const char* key,
    const char* family,
    double size,
    const uint8_t* ttf_data,
    int64_t ttf_len
);
KainUiGlyph* abi_ui_font_get_glyph(int64_t session_id, int64_t font_id, int codepoint);
void abi_ui_font_release_glyph(KainUiGlyph* glyph);

// ── Framebuffer accessors (DIB direct pixel access) ──────────────────
int64_t abi_ui_framebuffer_ptr(int64_t session_id);
int64_t abi_ui_framebuffer_width(int64_t session_id);
int64_t abi_ui_framebuffer_height(int64_t session_id);
int64_t abi_ui_framebuffer_stride(int64_t session_id);

// ── Window invalidation ───────────────────────────────────────────────
int64_t abi_ui_invalidate_window(int64_t session_id);

// ── Glyph Accessor ABI (struct field accessors for KainUiGlyph) ────────
int64_t abi_ui_glyph_bitmap_ptr(int64_t glyph_ptr);
int64_t abi_ui_glyph_width(int64_t glyph_ptr);
int64_t abi_ui_glyph_height(int64_t glyph_ptr);
int64_t abi_ui_glyph_x_offset(int64_t glyph_ptr);
int64_t abi_ui_glyph_y_offset(int64_t glyph_ptr);
int64_t abi_ui_glyph_advance(int64_t glyph_ptr);
void    abi_ui_glyph_release(int64_t glyph_ptr);

#ifdef __cplusplus
}
#endif

#endif /* ABI_UI_SYSTEM_H */
