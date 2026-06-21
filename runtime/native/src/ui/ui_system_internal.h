#ifndef ABI_UI_SYSTEM_INTERNAL_H
#define ABI_UI_SYSTEM_INTERNAL_H

#include "base.h"
#include "ui_system.h"
#include "../../include/component_surface.h"

#include <stddef.h>
#include <stdint.h>

typedef enum KainNativeUiStyleValueKind {
    ABI_UI_STYLE_I64 = 1,
    ABI_UI_STYLE_F64 = 2,
    ABI_UI_STYLE_STRING = 3,
} KainNativeUiStyleValueKind;

enum KainNativeUiNodeFlags {
    ABI_UI_NODE_HIDDEN = 1 << 0,
    ABI_UI_NODE_FOCUSABLE = 1 << 1,
    ABI_UI_NODE_INTERACTIVE = 1 << 2,
    ABI_UI_NODE_DISABLED = 1 << 3,
    ABI_UI_NODE_HOVERED = 1 << 4,
    ABI_UI_NODE_PRESSED = 1 << 5,
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
    char kind[ABI_UI_MAX_KEY];
    char text[ABI_UI_MAX_TEXT];
    char stable_key[ABI_UI_MAX_KEY];
    char accessibility_role[ABI_UI_MAX_KEY];
    char accessibility_label[ABI_UI_MAX_TEXT];
} KainNativeUiNode;

typedef struct KainNativeUiStyleRecord {
    int in_use;
    int64_t node_id;
    KainNativeUiStyleValueKind value_kind;
    int64_t i64_value;
    double f64_value;
    char key[ABI_UI_MAX_KEY];
    char string_value[ABI_UI_MAX_TEXT];
} KainNativeUiStyleRecord;

typedef struct KainNativeUiStateRecord {
    int in_use;
    int64_t node_id;
    KainNativeUiStyleValueKind value_kind;
    int64_t i64_value;
    double f64_value;
    char key[ABI_UI_MAX_KEY];
    char string_value[ABI_UI_MAX_TEXT];
} KainNativeUiStateRecord;

typedef struct KainNativeUiEvent {
    char kind[ABI_UI_MAX_KEY];
    int64_t target_node_id;
    int64_t key_code;
    double x;
    double y;
    char text[ABI_UI_MAX_TEXT];
} KainNativeUiEvent;

typedef struct KainNativeUiDrawCommand {
    char kind[ABI_UI_MAX_KEY];
    int64_t node_id;
    double x;
    double y;
    double width;
    double height;
    int64_t resource_id;
    int64_t font_resource_id;
    char text[ABI_UI_MAX_TEXT];
    char style_key[ABI_UI_MAX_KEY];
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
    char resource_type[ABI_UI_MAX_KEY];
    char key[ABI_UI_MAX_KEY];
    char aux[ABI_UI_MAX_TEXT];
} KainNativeUiResource;

typedef struct KainNativeUiMenu {
    int in_use;
    int64_t id;
    int64_t item_count;
    int64_t open;
    double x;
    double y;
    char key[ABI_UI_MAX_KEY];
} KainNativeUiMenu;

typedef struct KainNativeUiMenuItem {
    int in_use;
    int64_t id;
    int64_t menu_id;
    int64_t command_id;
    char key[ABI_UI_MAX_KEY];
    char label[ABI_UI_MAX_TEXT];
} KainNativeUiMenuItem;

typedef struct KainNativeUiDialog {
    int in_use;
    int64_t id;
    int64_t result;
    int64_t response_ready;
    char kind[ABI_UI_MAX_KEY];
    char title[ABI_UI_MAX_TEXT];
    char message[ABI_UI_MAX_TEXT];
    char response_text[ABI_UI_MAX_TEXT];
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
    char app_name[ABI_UI_MAX_KEY];
    char window_title[ABI_UI_MAX_TEXT];
    char host_backend[ABI_UI_MAX_KEY];
    char clipboard_text[ABI_UI_MAX_TEXT];
    char ime_text[ABI_UI_MAX_TEXT];
    char drag_payload[ABI_UI_MAX_TEXT];
    char hot_reload_key[ABI_UI_MAX_KEY];
    char dialog_response_text[ABI_UI_MAX_TEXT];
    KainNativeUiNode nodes[ABI_UI_MAX_NODES];
    KainNativeUiStyleRecord styles[ABI_UI_MAX_STYLES];
    KainNativeUiStateRecord state[ABI_UI_MAX_STATE];
    KainNativeUiDrawCommand draw_commands[ABI_UI_MAX_DRAW_COMMANDS];
    KainNativeUiEvent events[ABI_UI_MAX_EVENTS];
    KainNativeUiResource resources[ABI_UI_MAX_RESOURCES];
    KainNativeUiMenu menus[ABI_UI_MAX_MENUS];
    KainNativeUiMenuItem menu_items[ABI_UI_MAX_MENU_ITEMS];
    KainNativeUiDialog dialogs[ABI_UI_MAX_DIALOGS];
    uint32_t node_index[ABI_UI_NODE_INDEX_CAPACITY];
    uint32_t stable_key_index[ABI_UI_STABLE_KEY_INDEX_CAPACITY];
    uint32_t style_index[ABI_UI_STYLE_INDEX_CAPACITY];
    uint32_t state_index[ABI_UI_STATE_INDEX_CAPACITY];
    uint32_t resource_index[ABI_UI_RESOURCE_INDEX_CAPACITY];
    uint32_t menu_index[ABI_UI_MENU_INDEX_CAPACITY];
    uint32_t dialog_index[ABI_UI_DIALOG_INDEX_CAPACITY];
    uint64_t node_occupancy_bits[ABI_UI_NODE_OCCUPANCY_WORD_COUNT];
    uint64_t style_occupancy_bits[ABI_UI_STYLE_OCCUPANCY_WORD_COUNT];
    uint64_t state_occupancy_bits[ABI_UI_STATE_OCCUPANCY_WORD_COUNT];
    uint64_t resource_occupancy_bits[ABI_UI_RESOURCE_OCCUPANCY_WORD_COUNT];
    uint64_t menu_occupancy_bits[ABI_UI_MENU_OCCUPANCY_WORD_COUNT];
    uint64_t menu_item_occupancy_bits[ABI_UI_MENU_ITEM_OCCUPANCY_WORD_COUNT];
    uint64_t dialog_occupancy_bits[ABI_UI_DIALOG_OCCUPANCY_WORD_COUNT];
    KainNativeUiEvent active_event;
    int64_t node_count;
    int64_t style_count;
    int64_t state_count;
    int64_t draw_command_count;
    int64_t event_head;
    int64_t event_tail;
    int64_t event_count;
    void* host_state;
    const struct KainComponentSurface* component_surface;
    int64_t                           component_session_id;
} KainNativeUiSession;

#endif /* ABI_UI_SYSTEM_INTERNAL_H */
