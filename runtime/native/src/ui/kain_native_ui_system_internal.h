#ifndef KAIN_NATIVE_UI_SYSTEM_INTERNAL_H
#define KAIN_NATIVE_UI_SYSTEM_INTERNAL_H

#include "kain_runtime_base.h"
#include "kain_native_ui_system.h"

#include <stddef.h>
#include <stdint.h>

typedef enum KainNativeUiStyleValueKind {
    KAIN_NATIVE_UI_STYLE_I64 = 1,
    KAIN_NATIVE_UI_STYLE_F64 = 2,
    KAIN_NATIVE_UI_STYLE_STRING = 3,
} KainNativeUiStyleValueKind;

enum KainNativeUiNodeFlags {
    KAIN_NATIVE_UI_NODE_HIDDEN = 1 << 0,
    KAIN_NATIVE_UI_NODE_FOCUSABLE = 1 << 1,
    KAIN_NATIVE_UI_NODE_INTERACTIVE = 1 << 2,
    KAIN_NATIVE_UI_NODE_DISABLED = 1 << 3,
    KAIN_NATIVE_UI_NODE_HOVERED = 1 << 4,
    KAIN_NATIVE_UI_NODE_PRESSED = 1 << 5,
};

typedef struct KainNativeUiNode {
    int in_use;
    int64_t id;
    int64_t parent_id;
    int64_t child_count;
    int64_t flags;
    int64_t dirty_reason;
    uint64_t revision;
    double x;
    double y;
    double width;
    double height;
    char kind[KAIN_NATIVE_UI_MAX_KEY];
    char text[KAIN_NATIVE_UI_MAX_TEXT];
    char stable_key[KAIN_NATIVE_UI_MAX_KEY];
    char accessibility_role[KAIN_NATIVE_UI_MAX_KEY];
    char accessibility_label[KAIN_NATIVE_UI_MAX_TEXT];
} KainNativeUiNode;

typedef struct KainNativeUiStyleRecord {
    int in_use;
    int64_t node_id;
    KainNativeUiStyleValueKind value_kind;
    int64_t i64_value;
    double f64_value;
    char key[KAIN_NATIVE_UI_MAX_KEY];
    char string_value[KAIN_NATIVE_UI_MAX_TEXT];
} KainNativeUiStyleRecord;

typedef struct KainNativeUiEvent {
    char kind[KAIN_NATIVE_UI_MAX_KEY];
    int64_t target_node_id;
    int64_t key_code;
    double x;
    double y;
    char text[KAIN_NATIVE_UI_MAX_TEXT];
} KainNativeUiEvent;

typedef struct KainNativeUiDrawCommand {
    char kind[KAIN_NATIVE_UI_MAX_KEY];
    int64_t node_id;
    double x;
    double y;
    double width;
    double height;
    int64_t resource_id;
    int64_t font_resource_id;
    char text[KAIN_NATIVE_UI_MAX_TEXT];
    char style_key[KAIN_NATIVE_UI_MAX_KEY];
} KainNativeUiDrawCommand;

typedef struct KainNativeUiResource {
    int in_use;
    int64_t id;
    int64_t width;
    int64_t height;
    int64_t byte_length;
    double scalar_value;
    uint8_t* bytes;
    uint64_t bytes_revision;
    char resource_type[KAIN_NATIVE_UI_MAX_KEY];
    char key[KAIN_NATIVE_UI_MAX_KEY];
    char aux[KAIN_NATIVE_UI_MAX_TEXT];
} KainNativeUiResource;

typedef struct KainNativeUiMenu {
    int in_use;
    int64_t id;
    int64_t item_count;
    int64_t open;
    double x;
    double y;
    char key[KAIN_NATIVE_UI_MAX_KEY];
} KainNativeUiMenu;

typedef struct KainNativeUiMenuItem {
    int in_use;
    int64_t id;
    int64_t menu_id;
    int64_t command_id;
    char key[KAIN_NATIVE_UI_MAX_KEY];
    char label[KAIN_NATIVE_UI_MAX_TEXT];
} KainNativeUiMenuItem;

typedef struct KainNativeUiDialog {
    int in_use;
    int64_t id;
    int64_t result;
    int64_t response_ready;
    char kind[KAIN_NATIVE_UI_MAX_KEY];
    char title[KAIN_NATIVE_UI_MAX_TEXT];
    char message[KAIN_NATIVE_UI_MAX_TEXT];
    char response_text[KAIN_NATIVE_UI_MAX_TEXT];
} KainNativeUiDialog;

typedef struct KainNativeUiSession {
    int in_use;
    int64_t id;
    int64_t width;
    int64_t height;
    int64_t open;
    int64_t frame_index;
    int64_t last_presented_frame;
    int64_t focused_node_id;
    int64_t dirty_count;
    int64_t next_node_id;
    int64_t next_resource_id;
    int64_t next_menu_id;
    int64_t next_menu_item_id;
    int64_t next_dialog_id;
    int64_t host_attached;
    int64_t host_pump_count;
    int64_t host_should_close;
    int64_t host_presented_draw_count;
    int64_t host_frame_hash;
    int64_t resource_count;
    int64_t menu_count;
    int64_t menu_item_count;
    int64_t dialog_count;
    int64_t active_menu_id;
    int64_t active_dialog_id;
    int64_t ime_active_node_id;
    int64_t drag_active_node_id;
    int64_t drag_drop_target_id;
    int64_t hot_reload_generation;
    int64_t dialog_response_ready;
    int64_t dialog_response_result;
    double drag_x;
    double drag_y;
    double last_delta_ms;
    char app_name[KAIN_NATIVE_UI_MAX_KEY];
    char window_title[KAIN_NATIVE_UI_MAX_TEXT];
    char host_backend[KAIN_NATIVE_UI_MAX_KEY];
    char clipboard_text[KAIN_NATIVE_UI_MAX_TEXT];
    char ime_text[KAIN_NATIVE_UI_MAX_TEXT];
    char drag_payload[KAIN_NATIVE_UI_MAX_TEXT];
    char hot_reload_key[KAIN_NATIVE_UI_MAX_KEY];
    char dialog_response_text[KAIN_NATIVE_UI_MAX_TEXT];
    KainNativeUiNode nodes[KAIN_NATIVE_UI_MAX_NODES];
    KainNativeUiStyleRecord styles[KAIN_NATIVE_UI_MAX_STYLES];
    KainNativeUiDrawCommand draw_commands[KAIN_NATIVE_UI_MAX_DRAW_COMMANDS];
    KainNativeUiEvent events[KAIN_NATIVE_UI_MAX_EVENTS];
    KainNativeUiResource resources[KAIN_NATIVE_UI_MAX_RESOURCES];
    KainNativeUiMenu menus[KAIN_NATIVE_UI_MAX_MENUS];
    KainNativeUiMenuItem menu_items[KAIN_NATIVE_UI_MAX_MENU_ITEMS];
    KainNativeUiDialog dialogs[KAIN_NATIVE_UI_MAX_DIALOGS];
    KainNativeUiEvent active_event;
    int64_t node_count;
    int64_t style_count;
    int64_t draw_command_count;
    int64_t event_head;
    int64_t event_tail;
    int64_t event_count;
    void* host_state;
} KainNativeUiSession;

#endif /* KAIN_NATIVE_UI_SYSTEM_INTERNAL_H */
